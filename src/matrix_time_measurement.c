// Goal: make a table of time measurements for matrix operations depending on the size of the matrix.
// with a precised step.
// Matrix 10x10: ... s
// Matrix 100x100: ... s
// Matrix 1000x1000: ... s
// ...

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("yanovskyy");
MODULE_LICENSE("GPL");

static int size_matrix = 10;
module_param(size_matrix, int, 0644);
MODULE_PARM_DESC(size_matrix, "size of the matrix to start with");

static int repeat_operations = 1;
module_param(repeat_operations, int, 0644);
MODULE_PARM_DESC(repeat_operations, "number of times to repeat matrix operations");

static int **create_matrix(int size)
{
    pr_info("Creating matrix of size %d\n", size);
    int **data = kmalloc(size * sizeof(int *), GFP_KERNEL);
    if (!data)
    {
        pr_err("Failed to allocate memory for matrix rows\n");
        return NULL;
    }

    for (int i = 0; i < size; i++)
    {
        data[i] = kmalloc(size * sizeof(int), GFP_KERNEL);
        if (!data[i])
        {
            pr_err("Failed to allocate memory for matrix row %d\n", i);
            for (int j = 0; j < i; j++)
                kfree(data[j]);
            kfree(data);
            return NULL;
        }
        // Initialize the matrix with some values, e.g., zero
        for (int j = 0; j < size; j++)
            data[i][j] = 0;
    }
    return data;
}

static void free_matrix(int **m, int size)
{
    if (!m)
        return;

    for (int i = 0; i < size; i++)
        if (m[i])
            kfree(m[i]);
    kfree(m);
}

static void perform_matrix_operations(int **a, int **b, int **result)
{
    for (int i = 0; i < size_matrix; i++)
        for (int j = 0; j < size_matrix; j++)
            result[i][j] = a[i][j] + b[i][j];
}

static int __init start(void)
{
    pr_info("%s: Initializing module with matrix size %d\n", THIS_MODULE->name, size_matrix);
    int **a = create_matrix(size_matrix);
    if (!a)
    {
        pr_err("Failed to allocate memory for matrix A\n");
        return -ENOMEM;
    }
    int **b = create_matrix(size_matrix);
    if (!b)
    {
        pr_err("Failed to allocate memory for matrix B\n");
        free_matrix(a, size_matrix);
        return -ENOMEM;
    }
    int **result = create_matrix(size_matrix);
    if (!result)
    {
        pr_err("Failed to allocate memory for result matrix\n");
        free_matrix(a, size_matrix);
        free_matrix(b, size_matrix);
        return -ENOMEM;
    }

    // if (!a)
    // {
    // pr_err("Failed to allocate memory for matrices\n");
    // return -ENOMEM;
    // }

    ktime_t start = ktime_get_real();
    for (int i = 0; i < repeat_operations; i++)
        perform_matrix_operations(a, b, result);
    ktime_t end = ktime_get_real();

    s64 elapsed_ns = ktime_to_ns(ktime_sub(end, start));
    s64 elapsed_ms = elapsed_ns / 1000000; // Convert nanoseconds to
    s64 elapsed_s = elapsed_ms / 1000;     // Convert milliseconds to seconds

    pr_info("%s: Matrix operations completed in %lld milliseconds (%lld nanoseconds)\n", THIS_MODULE->name, elapsed_ms,
            elapsed_ns);

    free_matrix(a, size_matrix);
    free_matrix(b, size_matrix);
    free_matrix(result, size_matrix);
    return 0;
}

static void __exit end(void)
{
    pr_info("%s: Exiting module\n", THIS_MODULE->name);
    // Here you would typically clean up resources, if any were allocated.
}

module_init(start);
module_exit(end);