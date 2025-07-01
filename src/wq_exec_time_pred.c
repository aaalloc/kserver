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

static int time = 2;
module_param(time, int, 0644);
MODULE_PARM_DESC(time, "Number of time to spend in function (in seconds)");

static int work_type = 0;
module_param(work_type, int, 0644);
MODULE_PARM_DESC(work_type, "0 for matrix_eat_time, 1 for clock_eat_time");

static int n_op_matrix = 1000;
module_param(n_op_matrix, int, 0644);
MODULE_PARM_DESC(n_op_matrix, "Number of operations for matrix_eat_time");

#define PATH_MEASUREMENT_START "/tmp/wq-exec-time-pred-%d-%s-%s_affinity-%s.start"
#define PATH_MEASUREMENT_END "/tmp/wq-exec-time-pred-%d-%s-%s_affinity-%s.end"

struct file *measurement_start_file = NULL;
struct file *measurement_end_file = NULL;

unsigned long long measurement_start = 0;
unsigned long long measurement_end = 0;

static DECLARE_COMPLETION(xp_done);
struct clock_work
{
    int time;
    struct work_struct work;
};

struct matrix_work
{
    struct matrix_eat_time_param param;
    struct work_struct work;
};

void clock_work_handler(struct work_struct *work)
{
    struct clock_work *id = container_of(work, struct clock_work, work);

#ifdef RDTSC_ENABLED
    measurement_start = rdtsc_serialize();
#endif
    clock_eat_time(id->time);
#ifdef RDTSC_ENABLED
    measurement_end = rdtsc_serialize();
#endif

    complete(&xp_done);
}

void matrix_work_handler(struct work_struct *work)
{
    struct matrix_work *id = container_of(work, struct matrix_work, work);

#ifdef RDTSC_ENABLED
    measurement_start = rdtsc_serialize();
#endif
    matrix_eat_time(id->param);
#ifdef RDTSC_ENABLED
    measurement_end = rdtsc_serialize();
#endif

    // signal that work is done
    complete(&xp_done);
}

void write_measurements_to_file(struct file *file, unsigned long long val)
{
    if (file)
    {
        char buf[64];
        int to_write = snprintf(buf, sizeof(buf), "%llu\n", val);
        kernel_write(file, buf, to_write, &file->f_pos);
    }
}

char path_start[512] = {0};
char path_end[512] = {0};

static int start_xp(void *data)
{
    char *bound_str = unbound_or_bounded ? "unbound" : "bounded";
    char *affinity_str = high_affinity ? "high" : "low";

    struct timespec64 ts;
    struct tm tm;
    char iso_timestamp[32];

    // Get current time in UTC
    ktime_get_real_ts64(&ts);
    time64_to_tm(ts.tv_sec, 0, &tm);

    // Format: YYYY-MM-DDTHH:MM:SS.MSZ
    snprintf(iso_timestamp, sizeof(iso_timestamp), "%04ld-%02d-%02dT%02d:%02d:%02d:%03luZ", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);

    snprintf(path_start, sizeof(path_start), PATH_MEASUREMENT_START, n_op_matrix, bound_str, affinity_str,
             iso_timestamp);
    snprintf(path_end, sizeof(path_end), PATH_MEASUREMENT_END, n_op_matrix, bound_str, affinity_str, iso_timestamp);

    measurement_start_file = filp_open(path_start, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (unlikely(IS_ERR(measurement_start_file)))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(measurement_start_file));
        return -1;
    }

    measurement_end_file = filp_open(path_end, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (unlikely(IS_ERR(measurement_end_file)))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(measurement_end_file));
        filp_close(measurement_start_file, NULL);
        return -1;
    }

    struct workqueue_struct *wq = alloc_workqueue(
        "wq_exec_time_pred", (unbound_or_bounded ? WQ_UNBOUND : 0) | (high_affinity ? WQ_HIGHPRI : 0), 0);

    if (unlikely(!wq))
    {
        pr_err("Failed to create workqueue\n");
        return -ENOMEM;
    }
    pr_info("%s: Workqueue created with %s affinity and %s bound\n", THIS_MODULE->name, high_affinity ? "high" : "low",
            unbound_or_bounded ? "unbound" : "bounded");

    switch (work_type)
    {
    case 0:                                                                   // matrix_eat_time
        struct matrix_work work_wrap = {.param = {.size_matrix = MATRIX_SIZE, // Example size, can be adjusted
                                                  .repeat_operations = n_op_matrix}};

        INIT_WORK(&work_wrap.work, matrix_work_handler);
        queue_work(wq, &work_wrap.work);
        break;
    case 1: // clock_eat_time
        struct clock_work work_wrap_clock = {
            .time = time * 1000000000, // Time in seconds to nanoseconds
        };
        INIT_WORK(&work_wrap_clock.work, clock_work_handler);
        queue_work(wq, &work_wrap_clock.work);
        break;
    default:
        pr_err("Invalid work type: %d\n", work_type);
        destroy_workqueue(wq);
        return -EINVAL;
    }
    wait_for_completion(&xp_done);
    pr_info("%s: Work completed\n", THIS_MODULE->name);
    destroy_workqueue(wq);
    return 0;
}

static int __init start(void) { return start_xp(NULL); }

static void __exit end(void)
{
    pr_info("%s: Exiting module\n", THIS_MODULE->name);
    write_measurements_to_file(measurement_start_file, measurement_start);
    write_measurements_to_file(measurement_end_file, measurement_end);
    if (measurement_start_file)
    {
        filp_close(measurement_start_file, NULL);
        measurement_start_file = NULL;
    }
    if (measurement_end_file)
    {
        filp_close(measurement_end_file, NULL);
        measurement_end_file = NULL;
    }

    pr_info("%s: Measurement done, available at\n%s\n %s\n", THIS_MODULE->name, path_start, path_end);
}

module_init(start);
module_exit(end);
