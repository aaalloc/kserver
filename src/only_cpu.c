#include <linux/module.h>
#include <linux/workqueue.h>

#include "only_cpu.h"
#include "task.h"

struct workqueue_struct *only_cpu_wq;

int only_cpu_init(void)
{
    only_cpu_wq = alloc_workqueue("only_cpu_wq", 0, 0);
    if (unlikely(!only_cpu_wq))
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    return 0;
}

int only_cpu_start(void)
{
    if (unlikely(!only_cpu_wq))
    {
        pr_err("%s: Workqueue not initialized\n", THIS_MODULE->name);
        return -EINVAL;
    }

    struct client_work *cw = kzalloc(sizeof(struct client_work), GFP_KERNEL);
    if (unlikely(!cw))
    {
        pr_err("%s: Failed to allocate memory for client_work\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    *cw = (struct client_work){
        .t =
            {
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = 0,
    };
    INIT_WORK(&cw->work, w_cpu);

    spin_lock(&lclients_works_lock);
    list_add(&cw->list, &lclients_works);
    spin_unlock(&lclients_works_lock);

    queue_work(only_cpu_wq, &cw->work);
    return 0;
}

void only_cpu_free(void)
{
    if (only_cpu_wq)
    {
        flush_workqueue(only_cpu_wq);
        destroy_workqueue(only_cpu_wq);
    }

    free_client_work_list();
}
