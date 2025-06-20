#include <linux/init.h>   // For __init and __exit macros
#include <linux/kernel.h> // For KERN_INFO, KERN_WARNING, and KERN_ERR
#include <linux/list.h>   // For linked list operations
#include <linux/module.h> // For module macros
#include <linux/slab.h>   // For memory allocation functions
#include <linux/thread_info.h>
#include <linux/types.h>         // For data type definitions
#include <linux/workqueue_api.h> // For deferred work

#include <linux/errno.h>
#include <linux/in.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/version.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("yanovskyy");
MODULE_LICENSE("GPL");

static int high_affinity = 0;
module_param(high_affinity, int, 0644);
MODULE_PARM_DESC(high_affinity, "0 for low affinity, 1 for high affinity");

static int unbound_or_bounded = 0;
module_param(unbound_or_bounded, int, 0644);
MODULE_PARM_DESC(unbound_or_bounded, "0 for bounded workqueue, 1 for unbound workqueue");

static int iteration = 1000;
module_param(iteration, int, 0644);
MODULE_PARM_DESC(iteration, "Number of iterations for work insertion");

#define PATH_MEASUREMENT_START "/tmp/wq-insert-exec-%d-%s-%s_affinity-%s.start"
#define PATH_MEASUREMENT_END "/tmp/wq-insert-exec-%d-%s-%s_affinity-%s.end"

struct file *measurement_start_file = NULL;
struct file *measurement_end_file = NULL;

// unsigned long long measurement_start_arr[ITERATION];
// unsigned long long measurement_end_arr[ITERATION];

unsigned long long *measurement_start_arr = NULL;
unsigned long long *measurement_end_arr = NULL;

atomic_t index_measurement_start = ATOMIC_INIT(0);
atomic_t index_measurement_end = ATOMIC_INIT(0);

struct work_struct *works = NULL;

void work_handler(struct work_struct *work)
{
    struct task_struct *task = current;
    // pr_info("Work handler executed by task: %s (PID: %d)\n", task->comm, task->pid);
}

void update_measurement_start(unsigned long long start_time)
{
    if (measurement_start_file)
    {
        int tmp_index = atomic_inc_return_relaxed(&index_measurement_start) - 1;
        measurement_start_arr[tmp_index] = start_time;
    }
}

void update_measurement_end(unsigned long long end_time)
{
    if (measurement_end_file)
    {
        int tmp_index = atomic_inc_return_relaxed(&index_measurement_end) - 1;
        measurement_end_arr[tmp_index] = end_time;
    }
}

void write_measurements_to_file(struct file *file, unsigned long long *arr, int count)
{
    if (file && arr && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            char buf[64];
            int to_write = snprintf(buf, sizeof(buf), "%llu\n", arr[i]);
            kernel_write(file, buf, to_write, &file->f_pos);
        }
    }
}

char path_start[512] = {0};
char path_end[512] = {0};
static int __init start(void)
{
    works = kmalloc_array(iteration, sizeof(struct work_struct), GFP_KERNEL);
    if (unlikely(!works))
    {
        pr_err("Failed to allocate memory for works\n");
        return -ENOMEM;
    }

    measurement_start_arr = kmalloc_array(iteration, sizeof(unsigned long long), GFP_KERNEL);
    if (unlikely(!measurement_start_arr))
    {
        pr_err("Failed to allocate memory for measurement_start_arr\n");
        kfree(works);
        return -ENOMEM;
    }

    measurement_end_arr = kmalloc_array(iteration, sizeof(unsigned long long), GFP_KERNEL);
    if (unlikely(!measurement_end_arr))
    {
        pr_err("Failed to allocate memory for measurement_end_arr\n");
        kfree(measurement_start_arr);
        kfree(works);
        return -ENOMEM;
    }

    char *bound_str = unbound_or_bounded ? "unbound" : "bounded";
    char *affinity_str = high_affinity ? "high" : "low";

    struct timespec64 ts;
    struct tm tm;
    char iso_timestamp[32];

    // Get current time in UTC
    ktime_get_real_ts64(&ts);
    time64_to_tm(ts.tv_sec, 0, &tm);

    // Format: YYYY-MM-DDTHH:MM:SSZ
    snprintf(iso_timestamp, sizeof(iso_timestamp), "%04ld-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1,
             tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    snprintf(path_start, sizeof(path_start), PATH_MEASUREMENT_START, iteration, bound_str, affinity_str, iso_timestamp);
    snprintf(path_end, sizeof(path_end), PATH_MEASUREMENT_END, iteration, bound_str, affinity_str, iso_timestamp);

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

    init_hook_measurement_workqueue_insert_exec(update_measurement_start, update_measurement_end);
    init_measurement_workqueue_id(work_handler);

    struct workqueue_struct *wq = alloc_workqueue(
        "wq_time_insert_exec", (unbound_or_bounded ? WQ_UNBOUND : 0) | (high_affinity ? WQ_HIGHPRI : 0), 0);

    if (unlikely(!wq))
    {
        pr_err("Failed to create workqueue\n");
        return -ENOMEM;
    }
    pr_info("%s: Workqueue created with %s affinity and %s bound\n", THIS_MODULE->name, high_affinity ? "high" : "low",
            unbound_or_bounded ? "unbound" : "bounded");

    pr_info("%s: Starting work insertion with %d iterations\n", THIS_MODULE->name, iteration);
    for (int i = 0; i < iteration; i++)
    {
        INIT_WORK(&works[i], work_handler);
        queue_work(wq, &works[i]);
    }

    return 0;
}

static void __exit end(void)
{
    pr_info("%s: Exiting module\n", THIS_MODULE->name);
    init_hook_measurement_workqueue_insert_exec(NULL, NULL);
    init_measurement_workqueue_id(NULL);

    write_measurements_to_file(measurement_start_file, measurement_start_arr, atomic_read(&index_measurement_start));
    write_measurements_to_file(measurement_end_file, measurement_end_arr, atomic_read(&index_measurement_end));

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

    if (works)
    {
        kfree(works);
        works = NULL;
    }

    if (measurement_start_arr)
    {
        kfree(measurement_start_arr);
        measurement_start_arr = NULL;
    }

    if (measurement_end_arr)
    {
        kfree(measurement_end_arr);
        measurement_end_arr = NULL;
    }

    pr_info("%s: Measurement done, available at\n%s\n %s\n", THIS_MODULE->name, path_start, path_end);
}

module_init(start);
module_exit(end);
