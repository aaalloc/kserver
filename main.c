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

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("yanovskyy");
MODULE_LICENSE("GPL");

#define BUF_SIZE 4096

static struct workqueue_struct *kserver_wq_clients_read;
static struct workqueue_struct *kserver_wq_clients_work;
static struct workqueue_struct *kserver_wq_clients_work_tasks;

static struct task_struct *kserver_thread;
typedef struct _client
{
    struct list_head list;             // For linked list
    struct socket *sock;               // Socket for communication
    struct work_struct client_context; // representing the client context
} client;

typedef struct _client_work
{
    struct work_struct work;
    // unsigned char *packet;
    // int packet_len;
    struct list_head list;
} client_work;

struct socket *listen_sock;

LIST_HEAD(clients);
LIST_HEAD(clients_work);

static inline client_work *create_client_work(unsigned char *packet, int packet_len)
{
    client_work *cw = kmalloc(sizeof(client_work), GFP_KERNEL);
    if (!cw)
        return NULL;

    // cw->packet = kzalloc(packet_len, GFP_KERNEL);
    // if (!cw->packet)
    // {
    //     kfree(cw);
    //     return NULL;
    // }
    // memcpy(cw->packet, packet, packet_len);
    // cw->packet_len = packet_len;

    return cw;
}

__attribute__((optimize("O0"))) static void infite_loop(void)
{
    for (;;)
        continue;
}

static void client_worker(struct work_struct *work)
{
    client_work *cw = container_of(work, client_work, work);
    pr_info("%s: Client work done\n", THIS_MODULE->name);
    // pr_info("%s: Packet : %s\n", THIS_MODULE->name, cw->packet);
}

static void client_handler(struct work_struct *work)
{
    client *cl = container_of(work, client, client_context);

    unsigned char *buf;
    buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!buf)
    {
        pr_err("%s: Failed to allocate memory for buffer\n", THIS_MODULE->name);
        goto clean;
    }

    // TODO: this is in a case where workqueue should be created with flag
    // WQ_UNBOUND, one workaround is put back self to the workqueue
    // after ksocket_read has been done
    for (;;)
    {
        int ret = ksocket_read(cl->sock, buf, BUF_SIZE);
        if (ret < 0)
        {
            pr_err("%s: ksocket_read failed: %d\n", THIS_MODULE->name, ret);
            goto clean;
        }
        if (ret == 0)
        {
            pr_info("%s: Connection closed by peer\n", THIS_MODULE->name);
            goto clean;
        }

        client_work *cw = create_client_work(buf, ret);
        if (!cw)
        {
            pr_err("%s: Failed to allocate memory for client work\n", THIS_MODULE->name);
            goto clean;
        }

        list_add_tail(&cw->list, &clients_work);
        INIT_WORK(&cw->work, client_worker);
        queue_work(kserver_wq_clients_work, &cw->work);
        pr_info("%s: Packet : %s\n", THIS_MODULE->name, buf);
    }

clean:
    kernel_sock_shutdown(cl->sock, SHUT_RDWR);
    kfree(buf);
}

/*
 * This function creates a client structure, appends
 * it to clients list and return a work_struct
 */
static inline struct work_struct *create_client(struct socket *sock)
{
    client *cl = kmalloc(sizeof(client), GFP_KERNEL);
    if (!cl)
    {
        pr_err("%s: Failed to allocate memory for client\n", THIS_MODULE->name);
        return NULL;
    }

    cl->sock = sock;
    INIT_WORK(&cl->client_context, client_handler);
    list_add_tail(&cl->list, &clients);

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
        if (error < 0)
        {
            pr_err("%s: kernel_accept failed: %d\n", THIS_MODULE->name, error);
            continue;
        }

        client_read_work = create_client(sock);
        if (!client_read_work)
        {
            pr_err("%s: Failed to create client work\n", THIS_MODULE->name);
            kernel_sock_shutdown(sock, SHUT_RDWR);
            sock_release(sock);
            continue;
        }

        queue_work(kserver_wq_clients_read, client_read_work);
    }

    pr_info("%s: kserver_daemon stopped\n", THIS_MODULE->name);
    return 0;
}

static int __init kserver_init(void)
{
    pr_info(KERN_INFO "Server started.\n");

    int res = open_lsocket(&listen_sock, 12345);
    if (res < 0)
    {
        pr_err("%s: Failed to open socket: %d\n", THIS_MODULE->name, res);
        return res;
    }

    // Note for the future: have a check on flags, espcially
    // WQ_HIGHPRI, WQ_CPU_INTENSIVE
    // https://www.kernel.org/doc/html/next/core-api/workqueue.html#flags
    kserver_wq_clients_read = create_workqueue("wq_clients");
    if (!kserver_wq_clients_read)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }
    kserver_wq_clients_work = alloc_workqueue("wq_clients_work", WQ_UNBOUND, 0);
    if (!kserver_wq_clients_work)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        destroy_workqueue(kserver_wq_clients_read);
        return -ENOMEM;
    }
    kserver_wq_clients_work_tasks = create_workqueue("wq_clients_work_tasks");
    if (!kserver_wq_clients_work_tasks)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        destroy_workqueue(kserver_wq_clients_read);
        destroy_workqueue(kserver_wq_clients_work);
        return -ENOMEM;
    }

    kserver_thread = kthread_run(kserver_daemon, NULL, THIS_MODULE->name);
    if (IS_ERR(kserver_thread))
    {
        pr_err("%s: Failed to create kernel thread\n", THIS_MODULE->name);
        res = close_lsocket(listen_sock);
        if (res < 0)
            pr_err("%s: Failed to close socket: %d\n", THIS_MODULE->name, res);

        return PTR_ERR(kserver_thread);
    }

    return 0;
}

static void free_client_list(void)
{
    client *cl, *tmp;
    list_for_each_entry_safe(cl, tmp, &clients, list)
    {
        list_del(&cl->list);
        kernel_sock_shutdown(cl->sock, SHUT_RDWR);
        sock_release(cl->sock);
        kfree(cl);
    }
}

static void free_client_work_list(void)
{
    client_work *cw, *tmp;
    list_for_each_entry_safe(cw, tmp, &clients_work, list)
    {
        list_del(&cw->list);
        // kfree(cw->packet);
        kfree(cw);
    }
}

static void __exit kserver_exit(void)
{
    pr_info("%s: Server stopped.\n", THIS_MODULE->name);

    if (kserver_thread)
        kthread_stop(kserver_thread);

    close_lsocket(listen_sock);

    if (kserver_wq_clients_read)
    {
        flush_workqueue(kserver_wq_clients_read);
        destroy_workqueue(kserver_wq_clients_read);
    }

    if (kserver_wq_clients_work)
    {
        flush_workqueue(kserver_wq_clients_work);
        destroy_workqueue(kserver_wq_clients_work);
    }

    if (kserver_wq_clients_work_tasks)
    {
        flush_workqueue(kserver_wq_clients_work_tasks);
        destroy_workqueue(kserver_wq_clients_work_tasks);
    }

    free_client_list();
    free_client_work_list();
    pr_info("%s: bye bye\n", THIS_MODULE->name);
}

module_init(kserver_init);
module_exit(kserver_exit);