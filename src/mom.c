#include "mom.h"
#include "ksocket_handler.h"
#include <linux/module.h>
#include <linux/workqueue.h>

// ┌────┐
// │READ│
// └┬───┘
// ┌▽──────────────────────────┐
// │(N)_CPU                    │
// └┬─────────────────────────┬┘
// ┌▽───────────────────────┐┌▽───┐
// │CPU                     ││DISK│
// └┬──────────────────────┬┘└────┘
// ┌▽────────────────────┐┌▽───────────────────────────┐
// │(NCLIENTS)_NET_NOTIFY││(NCLIENTS)_NET_PUBACK_CLIENT│
// └─────────────────────┘└────────────────────────────┘
// READ: done in client_handler() (step 0)
// N_CPU: CPU task that will be repeated N times (step 1)
// CPU & DISK: CPU & DISK tasks executed in parallel (step 2)
// (NCLIENTS)_NET_NOTIFY: network tasks that will be executed in parallel for notifying subscriber on topic (step 3)
// (NCLIENTS)_NET_PUBACK_CLIENT: network task that will be executed in parallel send PUBACK to client to "pub" on topic
// (step 3)

struct workqueue_struct *mom_first_step;
struct workqueue_struct *mom_second_step_cpu;
struct workqueue_struct *mom_second_step_disk;
struct workqueue_struct *mom_third_step_net_notify_sub;
struct workqueue_struct *mom_third_step_net_ack;

typedef struct _listen_addr
{
    char ip[16]; // IPv4 address string
    int port;
    struct socket *sock;
} listen_addr;

#define MAX_LISTEN_SOCKETS 10

static listen_addr listen_sockets[MAX_LISTEN_SOCKETS];
static int num_connect_sockets = 0;

static int parse_address(const char *addr_str, listen_addr *addr)
{
    char *colon_pos;
    int ip_len;

    if (unlikely(!addr_str || !addr))
    {
        pr_err("%s: Invalid address or address structure is NULL\n", THIS_MODULE->name);
        return -EINVAL;
    }

    colon_pos = strchr(addr_str, ':');
    if (unlikely(!colon_pos))
    {
        pr_err("%s: Invalid address format: %s (expected IP:PORT)\n", THIS_MODULE->name, addr_str);
        return -EINVAL;
    }

    ip_len = colon_pos - addr_str;
    if (unlikely(ip_len >= sizeof(addr->ip)))
    {
        pr_err("%s: IP address too long: %s\n", THIS_MODULE->name, addr_str);
        return -EINVAL;
    }

    strncpy(addr->ip, addr_str, ip_len);
    addr->ip[ip_len] = '\0';

    if (unlikely(kstrtoint(colon_pos + 1, 10, &addr->port) < 0))
    {
        pr_err("%s: Invalid port number: %s\n", THIS_MODULE->name, colon_pos + 1);
        return -EINVAL;
    }

    if (unlikely(addr->port < 0 || addr->port > 65535))
    {
        pr_err("%s: Port number out of range: %d\n", THIS_MODULE->name, addr->port);
        return -EINVAL;
    }

    return 0;
}

// Function to parse the comma-separated list of addresses
// WARNING: Update num_connect_sockets global variable
// listen_sockets global variable will be update also
static int parse_listen_addresses(const char *addresses_str)
{
    char *addresses_copy, *token, *ptr;
    int count = 0;

    if (unlikely(!addresses_str))
    {
        pr_err("%s: No addresses provided for parsing\n", THIS_MODULE->name);
        return -EINVAL;
    }

    addresses_copy = kstrdup(addresses_str, GFP_KERNEL);
    if (unlikely(!addresses_copy))
    {
        pr_err("%s: Failed to allocate memory for addresses copy\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    ptr = addresses_copy;
    while ((token = strsep(&ptr, ",")) && count < MAX_LISTEN_SOCKETS)
    {
        // Remove leading/trailing whitespace
        while (*token == ' ' || *token == '\t')
            token++;

        if (parse_address(token, &listen_sockets[count]) == 0)
        {
            pr_info("%s: Parsed address %d: %s:%d\n", THIS_MODULE->name, count, listen_sockets[count].ip,
                    listen_sockets[count].port);
            count++;
        }
    }

    kfree(addresses_copy);
    num_connect_sockets = count;

    if (unlikely(count == 0))
    {
        pr_err("%s: No valid addresses parsed\n", THIS_MODULE->name);
        return -EINVAL;
    }

    pr_info("%s: Parsed %d listen addresses\n", THIS_MODULE->name, count);
    return 0;
}

int mom_publish_init(char *addresses_str)
{
    // addresses_str represent the client addresses when a mom publish
    // is done, it will send a publish to all of them
    int ret = parse_listen_addresses(addresses_str);
    if (unlikely(ret < 0))
    {
        pr_err("%s: Failed to parse listen addresses: %d\n", THIS_MODULE->name, ret);
        return ret;
    }

    mom_first_step = alloc_workqueue("mom_first_step", 0, 0);
    if (unlikely(!mom_first_step))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_second_step_cpu = alloc_workqueue("mom_second_step_cpu", 0, 0);
    if (unlikely(!mom_second_step_cpu))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_second_step_disk = alloc_workqueue("mom_second_step_disk", 0, 0);
    if (unlikely(!mom_second_step_disk))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_third_step_net_notify_sub = alloc_workqueue("mom_third_step_net_notify_sub", 0, 0);
    if (unlikely(!mom_third_step_net_notify_sub))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_third_step_net_ack = alloc_workqueue("mom_third_step_net_ack", 0, 0);
    if (unlikely(!mom_third_step_net_ack))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    return 0;
}

// Start will be (N)_CPU
int mom_publish_start(struct socket *s, spinlock_t *sp, char *ack_flag_msg, int ack_flag_msg_len)
{
    struct client_work *cw_net_3_ack = kzalloc(sizeof(struct client_work), GFP_KERNEL);
    if (unlikely(!cw_net_3_ack))
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_2\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    struct client_work *cw_cpu_2 = kzalloc(sizeof(struct client_work), GFP_KERNEL);
    if (unlikely(!cw_cpu_2))
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_2\n", THIS_MODULE->name);
        kfree(cw_net_3_ack);
        return -ENOMEM;
    }

    struct client_work *cw_disk_2 = kzalloc(sizeof(struct client_work), GFP_KERNEL);
    if (unlikely(!cw_disk_2))
    {
        pr_err("%s: Failed to allocate memory for cw_disk_2\n", THIS_MODULE->name);
        kfree(cw_net_3_ack);
        kfree(cw_cpu_2);
        return -ENOMEM;
    }

    struct client_work *cw_cpu_1 = kzalloc(sizeof(struct client_work), GFP_KERNEL);
    if (unlikely(!cw_cpu_1))
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_1\n", THIS_MODULE->name);
        kfree(cw_net_3_ack);
        kfree(cw_cpu_2);
        kfree(cw_disk_2);
        return -ENOMEM;
    }

    *cw_net_3_ack = (struct client_work){
        .t =
            {
                .args.net_args = {.sock = s,
                                  .lock = sp,
                                  .args.send = {.payload = ack_flag_msg,
                                                .size_payload = ack_flag_msg_len,
                                                .iterations = 1}},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };

    *cw_cpu_2 = (struct client_work){
        .t =
            {
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 100, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = num_connect_sockets,
    };

    struct list_head tmp_cw_net_3_notify = LIST_HEAD_INIT(tmp_cw_net_3_notify);
    for (int i = 0; i < num_connect_sockets; i++)
    {
        struct client_work *cw_net_3_notify = kzalloc(sizeof(struct client_work), GFP_KERNEL);
        if (unlikely(!cw_net_3_notify))
        {
            pr_err("%s: Failed to allocate memory for cw_net_3_notify\n", THIS_MODULE->name);
            kfree(cw_net_3_ack);
            kfree(cw_cpu_2);
            kfree(cw_disk_2);
            kfree(cw_cpu_1);
            return -ENOMEM;
        }

        *cw_net_3_notify = (struct client_work){
            .t =
                {
                    .args.net_args = {.sock = NULL,
                                      .lock = NULL,
                                      .args.conn_send = {.ip = listen_sockets[i].ip,
                                                         .port = listen_sockets[i].port,
                                                         .payload = "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                                                         .size_payload = 26,
                                                         .iterations = 1}},
                },
            .total_next_workqueue = 0,
            .next_works = {},
        };
        cw_cpu_2->next_works[i] = (struct next_workqueue){
            .wq = mom_third_step_net_notify_sub,
            .cw = cw_net_3_notify,
            .func = w_conn_net,
        };
        list_add(&cw_net_3_notify->list, &tmp_cw_net_3_notify);
    }

    // INIT_WORK(&cw_cpu_2->work, w_cpu);

    *cw_disk_2 = (struct client_work){
        .t =
            {
                .args.disk_args = {.filename = "/tmp/mom_disk_write.txt",
                                   .args.write = {.to_write = "hello_world_something",
                                                  .len_to_write = 22,
                                                  .iterations = 1}},
            },
        .total_next_workqueue = 1,
        .next_works = {{
            .wq = mom_third_step_net_ack,
            .cw = cw_net_3_ack,
            .func = w_net,
        }},
    };
    // INIT_WORK(&cw_disk_2->work, w_disk);

    *cw_cpu_1 = (struct client_work){
        .t =
            {
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 100, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = 2,
        .next_works = {{.wq = mom_second_step_cpu, .cw = cw_cpu_2, .func = w_cpu},
                       {.wq = mom_second_step_disk, .cw = cw_disk_2, .func = w_disk}},
    };
    INIT_WORK(&cw_cpu_1->work, w_cpu);

    spin_lock(&lclients_works_lock);
    list_add(&cw_cpu_1->list, &lclients_works);
    list_add(&cw_cpu_2->list, &lclients_works);
    list_add(&cw_disk_2->list, &lclients_works);
    list_add(&cw_net_3_ack->list, &lclients_works);

    struct client_work *cw_net_3_notify, *tmp;
    list_for_each_entry_safe(cw_net_3_notify, tmp, &tmp_cw_net_3_notify, list)
    {
        list_del(&cw_net_3_notify->list);
        list_add(&cw_net_3_notify->list, &lclients_works);
    }
    spin_unlock(&lclients_works_lock);

    queue_work(mom_first_step, &cw_cpu_1->work);
    return 0;
}

void mom_publish_free_wq(void)
{
    if (mom_first_step)
    {
        flush_workqueue(mom_first_step);
        destroy_workqueue(mom_first_step);
    }

    if (mom_second_step_cpu)
    {
        flush_workqueue(mom_second_step_cpu);
        destroy_workqueue(mom_second_step_cpu);
    }

    if (mom_second_step_disk)
    {
        flush_workqueue(mom_second_step_disk);
        destroy_workqueue(mom_second_step_disk);
    }

    if (mom_third_step_net_notify_sub)
    {
        flush_workqueue(mom_third_step_net_notify_sub);
        destroy_workqueue(mom_third_step_net_notify_sub);
    }

    if (mom_third_step_net_ack)
    {
        flush_workqueue(mom_third_step_net_ack);
        destroy_workqueue(mom_third_step_net_ack);
    }
}

void mom_publish_free(void)
{
    // Free all listen sockets
    // for (int i = 0; i < num_connect_sockets; i++)
    // {
    //     if (listen_sockets[i].sock)
    //         close_lsocket(listen_sockets[i].sock);
    // }

    mom_publish_free_wq();
    free_client_work_list();
}
