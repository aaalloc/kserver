#include "mom.h"
#include "ksocket_handler.h"
#include <linux/module.h>
#include <linux/workqueue.h>

// ┌────┐
// │READ│
// └┬───┘
// ┌▽───────┐
// │(N)_CPU │
// └┬──────┬┘
// ┌▽────┐┌▽─────┐
// │CPU  ││DISK  │
// └┬────┘└──────┘
// ┌▽─────────────┐
// │(NCLIENTS)_NET│
// └──────────────┘
// READ: done in client_handler() (step 0)
// N_CPU: CPU task that will be repeated N times (step 1)
// CPU & DISK: CPU & DISK tasks executed in parallel (step 2)
// NCLIENTS_NET: network tasks that will be executed in parallel (step 4)

struct workqueue_struct *mom_first_step;
struct workqueue_struct *mom_second_step_cpu;
struct workqueue_struct *mom_second_step_disk;
struct workqueue_struct *mom_third_step_net_notify_sub;
struct workqueue_struct *mom_third_step_net_ack;

#define MAX_LISTEN_SOCKETS 10
typedef struct _listen_addr
{
    char ip[16]; // IPv4 address string
    int port;
    struct socket *sock;
} listen_addr;

static listen_addr listen_sockets[MAX_LISTEN_SOCKETS];
struct client_work cw_nets[MAX_LISTEN_SOCKETS];
static int num_listen_sockets = 0;

static int parse_address(const char *addr_str, listen_addr *addr)
{
    char *colon_pos;
    int ip_len;

    if (!addr_str || !addr)
    {
        return -EINVAL;
    }

    colon_pos = strchr(addr_str, ':');
    if (!colon_pos)
    {
        pr_err("%s: Invalid address format: %s (expected IP:PORT)\n", THIS_MODULE->name, addr_str);
        return -EINVAL;
    }

    ip_len = colon_pos - addr_str;
    if (ip_len >= sizeof(addr->ip))
    {
        pr_err("%s: IP address too long: %s\n", THIS_MODULE->name, addr_str);
        return -EINVAL;
    }

    strncpy(addr->ip, addr_str, ip_len);
    addr->ip[ip_len] = '\0';

    if (kstrtoint(colon_pos + 1, 10, &addr->port) < 0)
    {
        pr_err("%s: Invalid port number: %s\n", THIS_MODULE->name, colon_pos + 1);
        return -EINVAL;
    }

    if (addr->port <= 0 || addr->port > 65535)
    {
        pr_err("%s: Port number out of range: %d\n", THIS_MODULE->name, addr->port);
        return -EINVAL;
    }

    return 0;
}

// Function to parse the comma-separated list of addresses
static int parse_listen_addresses(const char *addresses_str)
{
    char *addresses_copy, *token, *ptr;
    int count = 0;

    if (!addresses_str)
    {
        return -EINVAL;
    }

    addresses_copy = kstrdup(addresses_str, GFP_KERNEL);
    if (!addresses_copy)
    {
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
    num_listen_sockets = count;

    if (count == 0)
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
    if (ret < 0)
    {
        pr_err("%s: Failed to parse listen addresses: %d\n", THIS_MODULE->name, ret);
        return ret;
    }
    for (int i = 0; i < num_listen_sockets; i++)
    {
        struct socket *sock;
        ret = connect_lsocket_addr(&sock, listen_sockets[i].ip, listen_sockets[i].port);
        if (ret < 0)
        {
            pr_err("%s: Failed to open socket for %s:%d: %d\n", THIS_MODULE->name, listen_sockets[i].ip,
                   listen_sockets[i].port, ret);
            return ret;
        }
        listen_sockets[i].sock = sock;
        cw_nets[i] = (struct client_work){
            .t =
                {
                    .sock = sock,
                    .args.net_args = {.sock = sock,
                                      .args.send = {.payload = "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                                                    .size_payload = 26,
                                                    .iterations = 1}},
                },
            .total_next_workqueue = 0,
            .next_works = {},
        };
    }

    mom_first_step = alloc_workqueue("mom_first_step", WQ_UNBOUND, 0);
    if (!mom_first_step)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_second_step_cpu = alloc_workqueue("mom_second_step_cpu", WQ_UNBOUND, 0);
    if (!mom_second_step_cpu)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_second_step_disk = alloc_workqueue("mom_second_step_disk", WQ_UNBOUND, 0);
    if (!mom_second_step_disk)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_third_step_net_notify_sub = alloc_workqueue("mom_third_step_net_notify_sub", WQ_UNBOUND, 0);
    if (!mom_third_step_net_notify_sub)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_third_step_net_ack = alloc_workqueue("mom_third_step_net_ack", WQ_UNBOUND, 0);
    if (!mom_third_step_net_ack)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    return 0;
}

// Start will be (N)_CPU
int mom_publish_start(struct socket *s, char *ack_flag_msg, int ack_flag_msg_len)
{
    struct client_work *cw_net_3_ack = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_net_3_ack)
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_2\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    struct client_work *cw_cpu_2 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_cpu_2)
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_2\n", THIS_MODULE->name);
        kfree(cw_net_3_ack);
        return -ENOMEM;
    }

    struct client_work *cw_disk_2 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_disk_2)
    {
        pr_err("%s: Failed to allocate memory for cw_disk_2\n", THIS_MODULE->name);
        kfree(cw_net_3_ack);
        kfree(cw_cpu_2);
        return -ENOMEM;
    }

    struct client_work *cw_cpu_1 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_cpu_1)
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_1\n", THIS_MODULE->name);
        kfree(cw_net_3_ack);
        kfree(cw_cpu_2);
        kfree(cw_disk_2);
        return -ENOMEM;
    }

    list_add(&cw_cpu_1->list, &lclients_works);
    list_add(&cw_cpu_2->list, &lclients_works);
    list_add(&cw_disk_2->list, &lclients_works);
    list_add(&cw_net_3_ack->list, &lclients_works);

    *cw_cpu_2 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = num_listen_sockets,
    };
    for (int i = 0; i < num_listen_sockets; i++)
    {
        cw_cpu_2->next_works[i].wq = mom_third_step_net_notify_sub;
        cw_cpu_2->next_works[i].cw = &cw_nets[i];
        cw_cpu_2->next_works[i].func = w_net;
        // no need to add to the list, it is allocated on the stack not the heap
    }
    *cw_net_3_ack = (struct client_work){
        .t =
            {
                .sock = s,
                .args.net_args = {.sock = s,
                                  .args.send = {.payload = ack_flag_msg,
                                                .size_payload = ack_flag_msg_len,
                                                .iterations = 1}},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };
    // for having the notify sub and ack executed in parallel
    cw_cpu_2->next_works[cw_cpu_2->total_next_workqueue].wq = mom_third_step_net_ack;
    cw_cpu_2->next_works[cw_cpu_2->total_next_workqueue].cw = cw_net_3_ack;
    cw_cpu_2->next_works[cw_cpu_2->total_next_workqueue].func = w_net;
    cw_cpu_2->total_next_workqueue++;

    // INIT_WORK(&cw_cpu_2->work, w_cpu);

    *cw_disk_2 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.disk_args = {.filename = "/tmp/mom_disk_write.txt",
                                   // TODO: for write, I think random value is better
                                   .args.write = {.to_write = "hello_world_something",
                                                  .len_to_write = 22,
                                                  .iterations = 100000}},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };
    // INIT_WORK(&cw_disk_2->work, w_disk);

    *cw_cpu_1 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = 2,
        .next_works = {{.wq = mom_second_step_cpu, .cw = cw_cpu_2, .func = w_cpu},
                       {.wq = mom_second_step_disk, .cw = cw_disk_2, .func = w_disk}},
    };
    INIT_WORK(&cw_cpu_1->work, w_cpu);

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
    for (int i = 0; i < num_listen_sockets; i++)
    {
        if (listen_sockets[i].sock)
            close_lsocket(listen_sockets[i].sock);
    }

    mom_publish_free_wq();
}
