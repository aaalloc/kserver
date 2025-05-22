#pragma once
#include "operations.h"
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

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

struct client_work
{
    struct list_head list;
    struct work_struct work;
    struct task t;
};

static struct list_head lclients_works = LIST_HEAD_INIT(lclients_works);

void free_client_work_list(void);
struct client_work *create_task(struct task t, enum task_type type);