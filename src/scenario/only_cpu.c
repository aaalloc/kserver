#include <linux/module.h>
#include <linux/workqueue.h>

#include "only_cpu.h"
#include "task.h"

struct workqueue_struct *only_cpu_wq;

int only_cpu_init(void)
{
    only_cpu_wq = alloc_workqueue("only_cpu_wq", WQ_UNBOUND | WQ_SYSFS, 0);
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

    struct client_work cw = {
        .t =
            {
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 100, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = 0,
    };

    INIT_WORK(&cw.work, w_cpu);
    queue_work(only_cpu_wq, &cw.work);
    return 0;
}

void only_cpu_free(void)
{
    if (only_cpu_wq)
    {
        flush_workqueue(only_cpu_wq);
        destroy_workqueue(only_cpu_wq);
    }
}