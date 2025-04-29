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
    struct socket *sock;
} client_work;

typedef struct _client_work_task
{
    struct work_struct work;
    struct list_head list;
    void *arg;
} client_work_task;

struct socket *listen_sock;

LIST_HEAD(clients);
LIST_HEAD(clients_work);
LIST_HEAD(clients_work_tasks);

static inline client_work *create_client_work(struct socket *sock)
{
    client_work *cw = kmalloc(sizeof(client_work), GFP_KERNEL);
    if (!cw)
        return NULL;

    cw->sock = sock;
    return cw;
}

__attribute__((optimize("O0"))) static void infite_loop(void)
{
    for (;;)
        continue;
}

static void w_cpu(struct work_struct *work)
{
    client_work_task *cw_task = container_of(work, client_work_task, work);
    int size_mat = (intptr_t)cw_task->arg;
    op_cpu_matrix_multiplication_args_t *cpu_args = op_cpu_matrix_multiplication_init(size_mat);
    op_cpu_matrix_multiplication(cpu_args);
    op_cpu_matrix_multiplication_free(cpu_args);
    pr_info("%s: CPU operation finished\n", THIS_MODULE->name);
}

static void w_disk_net(struct work_struct *work)
{
    client_work_task *cw_task = container_of(work, client_work_task, work);

    op_disk_word_counting_args_t disk_args = {
        .filename = "/etc/6764457a.txt",
        .str_to_find = "a",
    };
    int cout = (intptr_t)op_disk_word_counting(&disk_args);
    pr_info("%s: Disk operation finished, count: %d\n", THIS_MODULE->name, cout);
    op_network_send_args_t net_args = {
        .sock = (struct socket *)cw_task->arg,
        .size_payload = 1000,
        .iterations = 1000,
    };
    op_network_send(&net_args);
    pr_info("%s: network done\n", THIS_MODULE->name);
}

static void client_worker(struct work_struct *work)
{
    client_work *cw = container_of(work, client_work, work);

    client_work_task *cw_task_cpu = kmalloc(sizeof(client_work_task), GFP_KERNEL);
    if (!cw_task_cpu)
    {
        pr_err("%s: Failed to allocate memory for client work task\n", THIS_MODULE->name);
        return;
    }

    client_work_task *cw_task_disk_net = kmalloc(sizeof(client_work_task), GFP_KERNEL);
    if (!cw_task_disk_net)
    {
        pr_err("%s: Failed to allocate memory for client work task\n", THIS_MODULE->name);
        return;
    }
    list_add_tail(&cw_task_cpu->list, &clients_work_tasks);
    list_add_tail(&cw_task_disk_net->list, &clients_work_tasks);

    // ┌──────┐
    // │DECODE│
    // └┬────┬┘
    // ┌▽──┐┌▽───┐
    // │CPU││DISK│
    // └───┘└┬───┘
    // ┌─────▽┐
    // │NET   │
    // └──────┘
    cw_task_cpu->arg = (void *)1000;
    cw_task_disk_net->arg = (void *)cw->sock;

    INIT_WORK(&cw_task_cpu->work, w_cpu);
    INIT_WORK(&cw_task_disk_net->work, w_disk_net);
    queue_work(kserver_wq_clients_work_tasks, &cw_task_cpu->work);
    queue_work(kserver_wq_clients_work_tasks, &cw_task_disk_net->work);

    // TODO: synchronize work, use another queue when on these tasks ends ?
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

        client_work *cw = create_client_work(cl->sock);
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
    kserver_wq_clients_work_tasks = alloc_workqueue("wq_clients_work", WQ_CPU_INTENSIVE | WQ_HIGHPRI, 0);
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
        kfree(cw);
    }
}

static void free_client_work_tasks_list(void)
{
    client_work_task *cw_task, *tmp;
    list_for_each_entry_safe(cw_task, tmp, &clients_work_tasks, list)
    {
        list_del(&cw_task->list);
        kfree(cw_task);
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
    free_client_work_tasks_list();
    pr_info("%s: bye bye\n", THIS_MODULE->name);
}

module_init(kserver_init);
module_exit(kserver_exit);