#include <linux/init.h>   // For __init and __exit macros
#include <linux/kernel.h> // For KERN_INFO, KERN_WARNING, and KERN_ERR
#include <linux/list.h>   // For linked list operations
#include <linux/module.h> // For module macros
#include <linux/slab.h>   // For memory allocation functions
#include <linux/tcp.h>    // For socket
#include <linux/thread_info.h>
#include <linux/types.h>         // For data type definitions
#include <linux/workqueue_api.h> // For deferred work
#include <net/sock.h>

#include <linux/errno.h>
#include <linux/in.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/version.h>

#include "ksocket_handler.h"

#include "operations.h"
#include "scenario.h"
#include "task.h"

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("yanovskyy");
MODULE_LICENSE("GPL");

#define BUF_SIZE 4096
#define MAX_LISTEN_SOCKETS 10
#define MAX_ADDR_STR_LEN 256

// Module parameters
static char *listen_addresses = "0.0.0.0:12345";
module_param(listen_addresses, charp, 0644);
MODULE_PARM_DESC(listen_addresses,
                 "Comma-separated list of IP:port addresses to listen on (e.g., '192.168.1.1:8080,127.0.0.1:9090')");

static int kserver_port = 12345;
module_param(kserver_port, int, 0644);
MODULE_PARM_DESC(kserver_port, "Port number for the kernel server (default: 12345)");

static int scenario = -1;
module_param(scenario, int, 0644);
MODULE_PARM_DESC(scenario, "Scenario to run (0: CPU, 1: MOM)");

static struct workqueue_struct *kserver_clients_read;

static struct task_struct *kserver_thread;
typedef struct _client
{
    struct list_head list;             // For linked list
    struct socket *sock;               // Socket for communication
    struct work_struct client_context; // representing the client context
} client;

struct socket *listen_sock;

LIST_HEAD(lclients);
LIST_HEAD(lclients_works);
DEFINE_SPINLOCK(lclients_works_lock);

static void client_handler(struct work_struct *work)
{
    client *cl = container_of(work, client, client_context);

    void *buf;
    buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (unlikely(!buf))
    {
        pr_err("%s: Failed to allocate memory for buffer\n", THIS_MODULE->name);
        goto clean;
    }
    spinlock_t sp = __SPIN_LOCK_UNLOCKED(sp);

    uint32_t len_recv = 0;
    // TODO: this is in a case where workqueue should be created with flag
    // WQ_UNBOUND, one workaround is put back self to the workqueue
    // after ksocket_read has been done
    for (;;)
    {
        int ret = ksocket_read((struct ksocket_handler){
            .sock = cl->sock,
            .buf = &len_recv,
            .len = sizeof(len_recv),
        });
        if (ret < 0)
        {
            pr_err("%s: ksocket_read failed: %d\n", THIS_MODULE->name, ret);
            goto clean;
        }
        if (ret == 0)
        {
            // pr_info("%s: Connection closed by peer\n", THIS_MODULE->name);
            goto clean;
        }
        ret = ksocket_read((struct ksocket_handler){
            .sock = cl->sock,
            .buf = buf,
            .len = len_recv,
        });
        if (ret < 0)
        {
            pr_err("%s: ksocket_read failed: %d\n", THIS_MODULE->name, ret);
            goto clean;
        }
        if (ret == 0)
        {
            // pr_info("%s: Connection closed by peer\n", THIS_MODULE->name);
            goto clean;
        }

        int res = 0;
        switch (scenario)
        {
        case ONLY_CPU:
            res = only_cpu_start();
            break;
        case MOM_PUBLISH:
            res = mom_publish_start(cl->sock, &sp, MOM_PUBLISH_ACK_FLAG, MOM_PUBLISH_ACK_FLAG_LEN);
            break;
        default:
            pr_err("%s: Invalid scenario selected\n", THIS_MODULE->name);
            goto clean;
        }

        if (unlikely(res < 0))
        {
            pr_err("%s: Failed to start scenario: %d\n", THIS_MODULE->name, res);
            goto clean;
        }

        // pr_info("%s: Packet : %s\n", THIS_MODULE->name, buf);
    }

clean:
    kernel_sock_shutdown(cl->sock, SHUT_RDWR);
    if (buf)
        kfree(buf);
}

/*
 * This function creates a client structure, appends
 * it to clients list and return a work_struct
 */
static inline struct work_struct *create_client(struct socket *sock)
{
    client *cl = kmalloc(sizeof(client), GFP_KERNEL);
    if (unlikely(!cl))
    {
        pr_err("%s: Failed to allocate memory for client\n", THIS_MODULE->name);
        return NULL;
    }

    cl->sock = sock;
    INIT_WORK(&cl->client_context, client_handler);
    list_add_tail(&cl->list, &lclients);

    return &cl->client_context;
}

static int kserver_daemon(void *data)
{
    (void)data;
    struct socket *sock;
    struct work_struct *client_read_work;

    allow_signal(SIGKILL);
    allow_signal(SIGTERM);

    while (!kthread_should_stop())
    {
        int error = kernel_accept(listen_sock, &sock, 0);
        if (unlikely(error < 0))
        {
            pr_err("%s: kernel_accept failed: %d\n", THIS_MODULE->name, error);
            continue;
        }

        client_read_work = create_client(sock);
        if (unlikely(!client_read_work))
        {
            pr_err("%s: Failed to create client work\n", THIS_MODULE->name);
            kernel_sock_shutdown(sock, SHUT_RDWR);
            sock_release(sock);
            continue;
        }

        queue_work(kserver_clients_read, client_read_work);
    }

    pr_info("%s: kserver_daemon stopped\n", THIS_MODULE->name);
    return 0;
}

static int __init kserver_init(void)
{
    pr_info(KERN_INFO "Server started.\n");
    int res = 0;
    if (!is_scenario_valid(scenario))
    {
        pr_err("%s: Invalid scenario selected: %d\n", THIS_MODULE->name, scenario);
        return -EINVAL;
    }

    pr_info("%s: Running scenario: %s\n", THIS_MODULE->name, get_scenario_description(scenario));
    switch (scenario)
    {
    case ONLY_CPU:
        res = only_cpu_init();
        break;
    case MOM_PUBLISH:
        res = mom_publish_init(listen_addresses);
        break;
    default:
        pr_err("%s: Invalid scenario selected\n", THIS_MODULE->name);
        return -EINVAL;
    }

    if (unlikely(res < 0))
    {
        pr_err("%s: Failed to initialize MOM publish\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    res = open_lsocket(&listen_sock, kserver_port);
    if (unlikely(res < 0))
    {
        pr_err("%s: Failed to open socket: %d\n", THIS_MODULE->name, res);
        return res;
    }

    // Note for the future: have a check on flags, espcially
    // WQ_HIGHPRI, WQ_CPU_INTENSIVE
    // https://www.kernel.org/doc/html/next/core-api/workqueue.html#flags
    kserver_clients_read = alloc_workqueue("kserver_clients_read", 0, 0);
    if (unlikely(!kserver_clients_read))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    kserver_thread = kthread_run(kserver_daemon, NULL, THIS_MODULE->name);
    if (unlikely(IS_ERR(kserver_thread)))
    {
        pr_err("%s: Failed to create kernel thread\n", THIS_MODULE->name);
        res = close_lsocket(listen_sock);
        if (unlikely(res < 0))
            pr_err("%s: Failed to close socket: %d\n", THIS_MODULE->name, res);

        return PTR_ERR(kserver_thread);
    }
    return 0;
}

static void free_client_list(void)
{
    client *cl, *tmp;
    list_for_each_entry_safe(cl, tmp, &lclients, list)
    {
        list_del(&cl->list);
        kernel_sock_shutdown(cl->sock, SHUT_RDWR);
        sock_release(cl->sock);
        kfree(cl);
    }
}

static void __exit kserver_exit(void)
{
    pr_info("%s: Server stopped.\n", THIS_MODULE->name);

    if (kserver_thread)
        kthread_stop(kserver_thread);

    close_lsocket(listen_sock);

    if (kserver_clients_read)
    {
        flush_workqueue(kserver_clients_read);
        destroy_workqueue(kserver_clients_read);
    }

    switch (scenario)
    {
    case ONLY_CPU:
        only_cpu_free();
        break;
    case MOM_PUBLISH:
        mom_publish_free();
        break;
    default:
        pr_err("%s: oops you shouldn't be here\n", THIS_MODULE->name);
        break;
    }

    free_client_list();

    pr_info("%s: bye bye\n", THIS_MODULE->name);
}

module_init(kserver_init);
module_exit(kserver_exit);
