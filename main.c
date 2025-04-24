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
typedef struct _client
{
    struct list_head list;             // For linked list
    struct socket *sock;               // Socket for communication
    struct work_struct client_context; // representing the client context
} client;

struct socket *listen_sock;

LIST_HEAD(clients);

static void client_work(struct work_struct *work) {}

static int __init custom_init(void)
{
    pr_info(KERN_INFO "Server started.\n");
    // wq_clients = create_workqueue("wq_clients");
    // if (!wq_clients)
    // {
    //     pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
    //     return -ENOMEM;
    // }

    int res = open_lsocket(&listen_sock, 12345);
    if (res < 0)
    {
        pr_err("%s: Failed to open socket: %d\n", THIS_MODULE->name, res);
        return res;
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

static void __exit custom_exit(void)
{
    //
    pr_info("%s: Server stopped.\n", THIS_MODULE->name);
    close_lsocket(listen_sock);
}

module_init(custom_init);
module_exit(custom_exit);