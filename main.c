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

LIST_HEAD(clients);

static int open_lsocket(struct socket **result, int port)
{
    struct socket *sock;
    struct sockaddr_in addr;
    int error;
    int opt = 1;
    sockptr_t kopt = {.kernel = (char *)&opt, .is_kernel = 1};

    // IPv4, TCP
    // TODO: fix lsp with right include
    error = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (error < 0)
    {
        pr_err("%s: socket_create failed: %d\n", THIS_MODULE->name, error);
        return error;
    }

    error = sock->ops->setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, kopt, sizeof(opt));
    if (error < 0)
    {
        pr_err("%s: kernel_setsockopt failed: %d\n", THIS_MODULE->name, error);
        goto err_setsockopt;
    }

    error = sock->ops->setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, kopt, sizeof(opt));
    if (error < 0)
    {
        pr_err("%s: kernel_setsockopt failed: %d\n", THIS_MODULE->name, error);
        goto err_setsockopt;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    pr_info("%s: binding to port %d\n", THIS_MODULE->name, port);
    *result = sock;
    return 0;
err_bind:
    kernel_sock_shutdown(sock, SHUT_RDWR);
err_setsockopt:
    sock_release(sock);
    return error;
}

static void client_work(struct work_struct *work) {}

static int __init custom_init(void)
{
    pr_info(KERN_INFO "Server started.\n");
    wq_clients = create_workqueue("wq_clients");
    if (!wq_clients)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    for (int i;; i++)
    {
        int res = kernel_accept(NULL, NULL, 0);

        client *cl = kmalloc(sizeof(client), GFP_KERNEL);
        if (!cl)
        {
            pr_err("%s: Failed to allocate memory for client\n", THIS_MODULE->name);
            return -ENOMEM;
        }
        cl->sock = NULL;                      // Initialize the socket to NULL
        INIT_WORK(&cl->client_context, NULL); // Initialize the work structure
        list_add_tail(&cl->list, &clients);   // Add to the list of clients
        // Add the work to the workqueue
        if (queue_work(wq_clients, &cl->client_context))
        {
            pr_info("%s: Work queued successfully\n", THIS_MODULE->name);
        }
        else
        {
            pr_err("%s: Failed to queue work\n", THIS_MODULE->name);
            kfree(cl);
            return -ENOMEM;
        }
    }

    return 0;
}

static void __exit custom_exit(void)
{
    //
    pr_info("%s: Server stopped.\n", THIS_MODULE->name);
}

module_init(custom_init);
module_exit(custom_exit);