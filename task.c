#include "task.h"
#include <linux/module.h>
void w_cpu(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);
    int res = op_cpu_matrix_multiplication_init(&c_task->t.args.cpu_args);
    if (unlikely(res < 0))
    {
        pr_err("%s: Failed to w_cpu: %d\n", THIS_MODULE->name, res);
        return;
    }

    op_cpu_matrix_multiplication(&c_task->t.args.cpu_args);
    op_cpu_matrix_multiplication_free(&c_task->t.args.cpu_args);
    pr_info("%s: CPU operation finished\n", THIS_MODULE->name);
}

void w_net(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);
    // op_network_send_args_t net_args = {
    // .sock = (struct socket *)cw_task->arg,
    // .size_payload = 10,
    // .iterations = 1,
    // };
    int res = op_network_send(&c_task->t.args.net_args);
    if (unlikely(res < 0))
        pr_err("%s: Failed to w_net: %d\n", THIS_MODULE->name, res);

    // pr_info("%s: network done\n", THIS_MODULE->name);
}

void w_disk(struct work_struct *work)
{
    struct client_work *c_task = container_of(work, struct client_work, work);

    int cout = op_disk_word_counting(&c_task->t.args.disk_args);

    if (unlikely(cout < 0))
        pr_err("%s: Failed to w_disk: %d\n", THIS_MODULE->name, cout);

    pr_info("%s: Disk operation finished, count: %d\n", THIS_MODULE->name, cout);
}

struct client_work *create_task(struct task t, enum task_type type)
{
    struct client_work *cw_task_net = kmalloc(sizeof(cw_task_net), GFP_KERNEL);
    if (!cw_task_net)
    {
        pr_err("%s: Failed to allocate memory for client work task\n", THIS_MODULE->name);
        return NULL;
    }
    list_add_tail(&cw_task_net->list, &lclients_works);
    cw_task_net->t = t;
    switch (type)
    {
    case TASK_CPU:
        INIT_WORK(&cw_task_net->work, w_cpu);
        break;
    case TASK_DISK:
        INIT_WORK(&cw_task_net->work, w_disk);
        break;
    case TASK_NET:
        INIT_WORK(&cw_task_net->work, w_net);
        break;
    default:
        pr_err("%s: Unknown task type\n", THIS_MODULE->name);
        kfree(cw_task_net);
        return NULL;
    }
    return cw_task_net;
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