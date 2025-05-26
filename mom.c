#include "mom.h"
#include "ksocket_handler.h"
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
struct workqueue_struct *mom_work_watchdog;

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

    mom_work_watchdog = alloc_workqueue("mom_work_watchdog", WQ_UNBOUND, 0);
    if (!mom_work_watchdog)
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

    struct work_watchdog *ww = kmalloc(sizeof(struct work_watchdog), GFP_KERNEL);
    if (!ww)
    {
        pr_err("%s: Failed to allocate memory for work_watchdog\n", THIS_MODULE->name);
        kfree(cw_net_3);
        kfree(cw_cpu_2);
        kfree(cw_disk_2);
        kfree(cw_cpu_1);
        return -ENOMEM;
    }
    ww->wq = mom_work_watchdog;
    INIT_WORK(&ww->work, ww_call);

    list_add(&cw_cpu_1->list, &lclients_works);
    list_add(&cw_cpu_2->list, &lclients_works);
    list_add(&cw_disk_2->list, &lclients_works);
    list_add(&cw_net_3->list, &lclients_works);

    ww->works_left = (atomic_t){.counter = 4};

    // ww->arg = (struct ksocket_handler){.sock = s, .buf = (unsigned char[]){0x022}, .len = 1};
    // ww->func = (void (*)(void *))ksocket_write;
    ww->sock = s;

    init_waitqueue_head(&ww->wait_any_work_done);

    *cw_net_3 = (struct client_work){
        .watchdog = ww,
        .t =
            {
                .sock = s,
                .args.net_args = {.sock = s, .args.send = {.size_payload = 10, .iterations = 1}},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };
    // INIT_WORK(&cw_net_3->work, w_net);

    *cw_cpu_2 = (struct client_work){
        .watchdog = ww,
        .t =
            {
                .sock = s,
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = 1,
        .next_works = {{.wq = mom_third_step, .cw = cw_net_3, .func = w_net}},
    };
    // INIT_WORK(&cw_cpu_2->work, w_cpu);

    *cw_disk_2 = (struct client_work){
        .watchdog = ww,
        .t =
            {
                .sock = s,
                .args.disk_args = {.filename = "/tmp/mom_disk_write.txt",
                                   // TODO: for write, I think random value is better
                                   .args.write = {.to_write = "hello_world_something",
                                                  .len_to_write = 22,
                                                  .iterations = 10000}},
            },
        .total_next_workqueue = 0,
        .next_works = {},
    };
    // INIT_WORK(&cw_disk_2->work, w_disk);

    *cw_cpu_1 = (struct client_work){
        .watchdog = ww,
        .t =
            {
                .sock = s,
                .args.cpu_args.args =
                    {
                        .matrix_multiplication = {.size = 1000, .a = NULL, .b = NULL, .result = NULL},
                    },
            },
        .total_next_workqueue = 2,
        .next_works = {{.wq = mom_second_step_cpu, .cw = cw_cpu_2, .func = w_cpu},
                       {.wq = mom_second_step_disk, .cw = cw_disk_2, .func = w_disk}},
    };
    INIT_WORK(&cw_cpu_1->work, w_cpu);

    queue_work(mom_first_step, &cw_cpu_1->work);
    return 0;
}

void mom_publish_free_wq(void)
{
    if (mom_first_step)
    {
        flush_workqueue(mom_first_step);
        destroy_workqueue(mom_first_step);
    }

    if (mom_second_step_cpu)
    {
        flush_workqueue(mom_second_step_cpu);
        destroy_workqueue(mom_second_step_cpu);
    }

    if (mom_second_step_disk)
    {
        flush_workqueue(mom_second_step_disk);
        destroy_workqueue(mom_second_step_disk);
    }

    if (mom_third_step)
    {
        flush_workqueue(mom_third_step);
        destroy_workqueue(mom_third_step);
    }

    if (mom_work_watchdog)
    {
        flush_workqueue(mom_work_watchdog);
        destroy_workqueue(mom_work_watchdog);
    }
}