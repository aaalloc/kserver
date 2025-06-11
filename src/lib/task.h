#pragma once
#include "ksocket_handler.h"
#include "operations.h"
#include <asm/atomic.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#define MAX_PARALLEL_TASKS 10
struct task
{
    union
    {
        op_cpu_args_t cpu_args;
        op_disk_args_t disk_args;
        op_network_args_t net_args;
    } args;
    struct work_struct work;
    struct list_head list;
};

enum task_type
{
    TASK_CPU,
    TASK_DISK,
    TASK_NET,
};

struct client_work;
struct next_workqueue
{
    struct workqueue_struct *wq;
    struct client_work *cw;
    void (*func)(struct work_struct *);
};

struct client_work
{
    struct list_head list;
    struct work_struct work;
    struct task t;
    size_t total_next_workqueue;
    // TODO: Actually, here we should use a struct list_head, but for simplicity sake now it is more duable to use an
    // array
    struct next_workqueue next_works[MAX_PARALLEL_TASKS];
};

extern spinlock_t lclients_works_lock;
extern struct list_head lclients_works;
// void free_client_work_list(void);

void w_cpu(struct work_struct *work);
void w_net(struct work_struct *work);
void w_conn_net(struct work_struct *work);
void w_disk(struct work_struct *work);
