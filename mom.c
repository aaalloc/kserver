#include "mom.h"
#include <linux/module.h>
#include <linux/workqueue.h>

// ┌────┐
// │READ│
// └┬───┘
// ┌▽───────┐
// │(N)_CPU │
// └┬──────┬┘
// ┌▽────┐┌▽─────┐
// │CPU  ││DISK  │
// └┬────┘└──────┘
// ┌▽─────────────┐
// │(NCLIENTS)_NET│
// └──────────────┘
// READ: done in client_handler() (step 0)
// N_CPU: CPU task that will be repeated N times (step 1)
// CPU & DISK: CPU & DISK tasks executed in parallel (step 2)
// NCLIENTS_NET: network tasks that will be executed in parallel (step 4)

struct workqueue_struct *mom_first_step;
struct workqueue_struct *mom_second_step_cpu;
struct workqueue_struct *mom_second_step_disk;
struct workqueue_struct *mom_third_step;

void mom_publish_first_step(void)
{
    mom_first_step = alloc_workqueue("mom_first_step", WQ_UNBOUND, 0);
    if (!mom_first_step)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return;
    }
}

// Start will be (N)_CPU
void mom_publish_start(struct socket *s)
{
    struct task t = {.sock = s, .args.cpu_args = {.size = 1000, .a = NULL, .b = NULL, .result = NULL}};

    struct client_work *cw = create_task(t, TASK_CPU, 2,
                                         (struct next_workqueue[]){{.wq = mom_second_step_cpu, .func = w_cpu},
                                                                   {.wq = mom_second_step_disk, .func = w_disk}});
    if (unlikely(!cw))
    {
        pr_err("%s: Failed to create client work\n", THIS_MODULE->name);
        return;
    }
    queue_work(mom_first_step, &cw->work);
}