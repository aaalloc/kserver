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

static struct workqueue_struct *wq_clients;
static struct task_struct *kserver_thread;
typedef struct _client
{
    struct list_head list;             // For linked list
    struct socket *sock;               // Socket for communication
    struct work_struct client_context; // representing the client context
} client;

struct socket *listen_sock;

LIST_HEAD(clients);

static void client_work(struct work_struct *work)
{
    client *cl = container_of(work, client, client_context);

    pr_info("%s: client_work AAA started\n", THIS_MODULE->name);

    kernel_sock_shutdown(cl->sock, SHUT_RDWR);
}

/*
 * This function creates a client structure, happend
 * it to clients list and return it work_struct
 */
static struct work_struct *create_client(struct socket *sock)
{
    client *cl = kmalloc(sizeof(client), GFP_KERNEL);
    if (!cl)
    {
        pr_err("%s: Failed to allocate memory for client\n", THIS_MODULE->name);
        return NULL;
    }

    cl->sock = sock;
    INIT_WORK(&cl->client_context, client_work);
    list_add_tail(&cl->list, &clients);

    return &cl->client_context;
}

static int kserver_daemon(void *data)
{
    (void)data;
    struct socket *sock;
    struct work_struct *work;

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
        if ((work = create_client(sock)) == NULL)
        {
            pr_err("%s: Failed to create client work\n", THIS_MODULE->name);
            kernel_sock_shutdown(sock, SHUT_RDWR);
            sock_release(sock);
            continue;
        }
        pr_info("%s: kernel_accept succeeded\n", THIS_MODULE->name);

        queue_work(wq_clients, work);
    }

    pr_info("%s: kserver_daemon stopped\n", THIS_MODULE->name);

    client *cl, *tmp;
    list_for_each_entry_safe(cl, tmp, &clients, list)
    {
        list_del(&cl->list);
        kernel_sock_shutdown(cl->sock, SHUT_RDWR);
        sock_release(cl->sock);
        kfree(cl);
    }
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
    wq_clients = create_workqueue("wq_clients");
    if (!wq_clients)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
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

    // for (int i;; i++)
    // {
    //     int res = kernel_accept(NULL, NULL, 0);

    //     client *cl = kmalloc(sizeof(client), GFP_KERNEL);
    //     if (!cl)
    //     {
    //         pr_err("%s: Failed to allocate memory for client\n", THIS_MODULE->name);
    //         return -ENOMEM;
    //     }
    //     cl->sock = NULL;                      // Initialize the socket to NULL
    //     INIT_WORK(&cl->client_context, NULL); // Initialize the work structure
    //     list_add_tail(&cl->list, &clients);   // Add to the list of clients
    //     // Add the work to the workqueue
    //     if (queue_work(wq_clients, &cl->client_context))
    //     {
    //         pr_info("%s: Work queued successfully\n", THIS_MODULE->name);
    //     }
    //     else
    //     {
    //         pr_err("%s: Failed to queue work\n", THIS_MODULE->name);
    //         kfree(cl);
    //         return -ENOMEM;
    //     }
    // }
    return 0;
}

static void __exit kserver_exit(void)
{
    pr_info("%s: Server stopped.\n", THIS_MODULE->name);

    if (kserver_thread)
        kthread_stop(kserver_thread);

    close_lsocket(listen_sock);

    if (wq_clients)
        destroy_workqueue(wq_clients);

    pr_info("%s: bye bye\n", THIS_MODULE->name);
}

module_init(kserver_init);
module_exit(kserver_exit);