#include <linux/init.h>   // For __init and __exit macros
#include <linux/kernel.h> // For KERN_INFO, KERN_WARNING, and KERN_ERR
#include <linux/list.h>   // For linked list operations
#include <linux/module.h> // For module macros
#include <linux/slab.h>   // For memory allocation functions
#include <linux/thread_info.h>
#include <linux/types.h>         // For data type definitions
#include <linux/workqueue_api.h> // For deferred work

#include <linux/delay.h>
#include <linux/ktime.h>

#include <linux/errno.h>
#include <linux/in.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/version.h>

#include "eat_time.h" // For time-eating functions

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("yanovskyy");
MODULE_LICENSE("GPL");

static int high_affinity = 0;
module_param(high_affinity, int, 0644);
MODULE_PARM_DESC(high_affinity, "0 for low affinity, 1 for high affinity");

static int unbound_or_bounded = 0;
module_param(unbound_or_bounded, int, 0644);
MODULE_PARM_DESC(unbound_or_bounded, "0 for bounded workqueue, 1 for unbound workqueue");

static int nr_work_max = 1000;
module_param(nr_work_max, int, 0644);
MODULE_PARM_DESC(nr_work_max, "Maximum number of work items");

static int delay = 1000;
module_param(delay, int, 0644);
MODULE_PARM_DESC(delay, "Delay in milliseconds between work submissions");

struct matrix_work
{
    struct matrix_eat_time_param param;
    struct work_struct work;
};

struct workqueue_struct *wq = NULL;

void work_handler(struct work_struct *work)
{
    struct matrix_work *mw = container_of(work, struct matrix_work, work);
    matrix_eat_time(mw->param);
}
static int start_xp(void *data)
{
    // char *bound_str = unbound_or_bounded ? "unbound" : "bounded";
    // char *affinity_str = high_affinity ? "high" : "low";

    wq = alloc_workqueue("wq_new_worker", (unbound_or_bounded ? WQ_UNBOUND : 0) | (high_affinity ? WQ_HIGHPRI : 0), 0);

    if (unlikely(!wq))
    {
        pr_err("Failed to create workqueue\n");
        return -ENOMEM;
    }
    pr_info("%s: Workqueue created with %s affinity and %s bound\n", THIS_MODULE->name, high_affinity ? "high" : "low",
            unbound_or_bounded ? "unbound" : "bounded");

    for (int i = 0; i < nr_work_max; i++)
    {
        struct matrix_work work_wrap = {.param = {.size_matrix = MATRIX_SIZE, // Example size, can be adjusted
                                                  .repeat_operations = MATRIX_SIZE}};

        INIT_WORK(&work_wrap.work, work_handler);
        queue_work(wq, &work_wrap.work);

        msleep(delay);
    }

    pr_info("%s: Work inserted, check workers\n", THIS_MODULE->name);
    return 0;
}

static int __init start(void) { return start_xp(NULL); }

static void __exit end(void)
{
    destroy_workqueue(wq);
    pr_info("%s: Exiting module\n", THIS_MODULE->name);
}

module_init(start);
module_exit(end);
