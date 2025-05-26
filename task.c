#include "task.h"
#include "ksocket_handler.h"
#include <linux/module.h>
void ww_call(struct work_struct *work)
{
    struct work_watchdog *ww = container_of(work, struct work_watchdog, work);
    ksocket_write((struct ksocket_handler){
        .sock = ww->sock,
        .buf = (uint16_t[]){0x1337},
        .len = 1,
    });
    // pr_info("%s: Work watchdog finished\n", THIS_MODULE->name);
}

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

    if (atomic_sub_and_test(1, &c_task->watchdog->works_left))
        queue_work(c_task->watchdog->wq, &c_task->watchdog->work);

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

    if (atomic_sub_and_test(1, &c_task->watchdog->works_left))
        queue_work(c_task->watchdog->wq, &c_task->watchdog->work);
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

    if (atomic_sub_and_test(1, &c_task->watchdog->works_left))
        queue_work(c_task->watchdog->wq, &c_task->watchdog->work);

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
    list_for_each_entry_safe(cw, tmp, &lclients_works, list)
    {
        list_del(&cw->list);
        kfree(cw);
    }
}

void free_work_watchdog_list(void)
{
    struct work_watchdog *ww, *tmp;
    list_for_each_entry_safe(ww, tmp, &lwork_watchdog, list)
    {
        list_del(&ww->list);
        kfree(ww);
    }
}