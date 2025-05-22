#pragma once
#include "operations.h"
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define MAX_PARALLEL_TASKS 10
struct task
{
    union
    {
        op_cpu_matrix_multiplication_args_t cpu_args;
        op_disk_word_counting_args_t disk_args;
        op_network_send_args_t net_args;
    } args;
    struct work_struct work;
    struct list_head list;
    struct socket *sock;
};

enum task_type
{
    TASK_CPU,
    TASK_DISK,
    TASK_NET,
};

struct next_workqueue
{
    struct workqueue_struct *wq;
    struct work_struct *work;
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

static struct list_head lclients_works = LIST_HEAD_INIT(lclients_works);

void free_client_work_list(void);

void w_cpu(struct work_struct *work);
void w_net(struct work_struct *work);
void w_disk(struct work_struct *work);
struct client_work *create_task(struct task t, enum task_type type, int total_next_workqueue,
                                struct next_workqueue next_works[total_next_workqueue]);