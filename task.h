#pragma once
#include "ksocket_handler.h"
#include "operations.h"
#include <asm/atomic.h>
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

struct client_work;
struct work_watchdog;
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
    struct work_watchdog *watchdog;
    size_t total_next_workqueue;
    // TODO: Actually, here we should use a struct list_head, but for simplicity sake now it is more duable to use an
    // array
    struct next_workqueue next_works[MAX_PARALLEL_TASKS];
};

struct work_watchdog
{
    struct work_struct work;
    struct workqueue_struct *wq;
    atomic_t works_left;
    wait_queue_head_t wait_any_work_done;
    // TODO: arg need to be generic
    struct ksocket_handler arg;
    void (*func)(void *);
    // void *func;
    struct list_head list;
};

static struct list_head lclients_works = LIST_HEAD_INIT(lclients_works);
static struct list_head lwork_watchdog = LIST_HEAD_INIT(lwork_watchdog);

void free_client_work_list(void);
void free_work_watchdog_list(void);

void ww_loop(struct work_struct *work);

void w_cpu(struct work_struct *work);
void w_net(struct work_struct *work);
void w_disk(struct work_struct *work);
struct client_work *create_task(struct task t, enum task_type type, int total_next_workqueue,
                                struct next_workqueue next_works[]);