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

int mom_publish_init(void)
{
    mom_first_step = alloc_workqueue("mom_first_step", WQ_UNBOUND, 0);
    if (!mom_first_step)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_second_step_cpu = alloc_workqueue("mom_second_step_cpu", WQ_UNBOUND, 0);
    if (!mom_second_step_cpu)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_second_step_disk = alloc_workqueue("mom_second_step_disk", WQ_UNBOUND, 0);
    if (!mom_second_step_disk)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    mom_third_step = alloc_workqueue("mom_third_step", WQ_UNBOUND, 0);
    if (!mom_third_step)
    {
        pr_err("%s: Failed to create workqueue\n", THIS_MODULE->name);
        return -ENOMEM;
    }

    return 0;
}

// Start will be (N)_CPU
int mom_publish_start(struct socket *s)
{
    struct client_work *cw_net_3 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_net_3)
    {
        pr_err("%s: Failed to allocate memory for cw_net_3\n", THIS_MODULE->name);
        return -ENOMEM;
    }
    struct client_work *cw_cpu_2 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_cpu_2)
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_2\n", THIS_MODULE->name);
        kfree(cw_net_3);
        return -ENOMEM;
    }

    struct client_work *cw_disk_2 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_disk_2)
    {
        pr_err("%s: Failed to allocate memory for cw_disk_2\n", THIS_MODULE->name);
        kfree(cw_net_3);
        kfree(cw_cpu_2);
        return -ENOMEM;
    }

    struct client_work *cw_cpu_1 = kmalloc(sizeof(struct client_work), GFP_KERNEL);
    if (!cw_cpu_1)
    {
        pr_err("%s: Failed to allocate memory for cw_cpu_1\n", THIS_MODULE->name);
        kfree(cw_net_3);
        kfree(cw_cpu_2);
        kfree(cw_disk_2);
        return -ENOMEM;
    }

    list_add(&cw_cpu_1->list, &lclients_works);
    list_add(&cw_cpu_2->list, &lclients_works);
    list_add(&cw_disk_2->list, &lclients_works);
    list_add(&cw_net_3->list, &lclients_works);

    *cw_net_3 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.net_args = {.sock = s, .size_payload = 10, .iterations = 1},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };
    // INIT_WORK(&cw_net_3->work, w_net);

    *cw_cpu_2 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.cpu_args = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
            },
        .total_next_workqueue = 1,
        .next_works = {{.wq = mom_third_step, .cw = cw_net_3, .func = w_net}},
    };
    // INIT_WORK(&cw_cpu_2->work, w_cpu);

    *cw_disk_2 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.disk_args = {.filename = "/etc/6764457a.txt", .str_to_find = "a"},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };
    // INIT_WORK(&cw_disk_2->work, w_disk);

    *cw_cpu_1 = (struct client_work){
        .t =
            {
                .sock = s,
                .args.cpu_args = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
            },
        .total_next_workqueue = 2,
        .next_works = {{.wq = mom_second_step_cpu, .cw = cw_cpu_2, .func = w_cpu},
                       {.wq = mom_second_step_disk, .cw = cw_disk_2, .func = w_disk}},
    };
    INIT_WORK(&cw_cpu_1->work, w_cpu);

    queue_work(mom_first_step, &cw_cpu_1->work);
    return 0;
}