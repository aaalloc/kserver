#include "task.h"
#include "ksocket_handler.h"
#include <linux/module.h>

void w_cpu(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);
    // TODO: make generic call function here
    int res = op_cpu_matrix_multiplication_init(&c_task->t.args.cpu_args);
    if (unlikely(res < 0))
    {
        pr_err("%s: Failed to w_cpu: %d\n", THIS_MODULE->name, res);
        return;
    }

    op_cpu_matrix_multiplication(&c_task->t.args.cpu_args);
    op_cpu_matrix_multiplication_free(&c_task->t.args.cpu_args);
    // pr_info("%s: CPU operation finished\n", THIS_MODULE->name);

    // pr_info("%s: PID of w_cpu: %d\n", THIS_MODULE->name, get_current()->pid);
    for (int i = 0; i < c_task->total_next_workqueue; i++)
    {
        struct next_workqueue *next_wq = &c_task->next_works[i];
        INIT_WORK(&next_wq->cw->work, next_wq->func);
        queue_work(next_wq->wq, &next_wq->cw->work);
    }
}

void w_net(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);
    // TODO: make generic call function here
    int res = op_network_send(&c_task->t.args.net_args);
    if (unlikely(res < 0))
    {
        pr_err("%s: Failed to w_net: %d\n", THIS_MODULE->name, res);
        return;
    }

    // pr_info("%s: PID of w_net: %d\n", THIS_MODULE->name, get_current()->pid);
    // pr_info("%s: network done\n", THIS_MODULE->name);
    for (int i = 0; i < c_task->total_next_workqueue; i++)
    {
        struct next_workqueue *next_wq = &c_task->next_works[i];
        INIT_WORK(&next_wq->cw->work, next_wq->func);
        queue_work(next_wq->wq, &next_wq->cw->work);
    }
}

void w_conn_net(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);
    // TODO: make generic call function here
    int res = op_network_conn_send(&c_task->t.args.net_args);
    if (unlikely(res < 0))
    {
        pr_err("%s: Failed to w_conn_net: %d\n", THIS_MODULE->name, res);
        return;
    }

    // pr_info("%s: PID of w_conn_net: %d\n", THIS_MODULE->name, get_current()->pid);
    // pr_info("%s: network done\n", THIS_MODULE->name);
    for (int i = 0; i < c_task->total_next_workqueue; i++)
    {
        struct next_workqueue *next_wq = &c_task->next_works[i];
        INIT_WORK(&next_wq->cw->work, next_wq->func);
        queue_work(next_wq->wq, &next_wq->cw->work);
    }
}

void w_disk(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);
    // TODO: make generic call function here
    int ret = op_disk_write(&c_task->t.args.disk_args);
    if (unlikely(ret < 0))
    {
        pr_err("%s: Failed to w_disk: %d\n", THIS_MODULE->name, ret);
        return;
    }

    // pr_info("%s: PID of w_disk: %d\n", THIS_MODULE->name, get_current()->pid);
    // pr_info("%s: Disk operation finished, read: %d\n", THIS_MODULE->name, ret);
    for (int i = 0; i < c_task->total_next_workqueue; i++)
    {
        struct next_workqueue *next_wq = &c_task->next_works[i];
        INIT_WORK(&next_wq->cw->work, next_wq->func);
        queue_work(next_wq->wq, &next_wq->cw->work);
    }
}

void free_client_work_list(void)
{
    struct client_work *cw, *tmp;
    int counter = 0;
    list_for_each_entry_safe(cw, tmp, &lclients_works, list)
    {
        list_del(&cw->list);
        kfree(cw);
        counter++;
    }
    pr_info("%s: Freed %d client works\n", THIS_MODULE->name, counter);
}