#include "eat_time.h"
#include <linux/ktime.h>
#include <linux/slab.h>

void clock_eat_time(unsigned long nanoseconds)
{
    // This function will block the execution for a specified number of nanoseconds.
    // It is not intended to do anything useful, just to consume time.
    ktime_t start = ktime_get();
    ktime_t end = ktime_add_ns(start, nanoseconds);

    while (ktime_before(ktime_get(), end))
        ; // Busy-wait until the specified time has passed
    ktime_t elapsed = ktime_sub(ktime_get(), start);
    // Debug purpose: Print the start and end times
    pr_info("Clock eat time: elapsed = %lld ns", ktime_to_ns(elapsed));
}

int **create_matrix(int size)
{
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

void free_matrix(int **m, int size)
{
    if (!m)
        return;

    for (int i = 0; i < size; i++)
        if (m[i])
            kfree(m[i]);
    kfree(m);
}

#define MATRIX_SIZE 1000
int _a[MATRIX_SIZE][MATRIX_SIZE] = {0};      // Example size, can be adjusted
int _b[MATRIX_SIZE][MATRIX_SIZE] = {0};      // Example size, can be adjusted
int _result[MATRIX_SIZE][MATRIX_SIZE] = {0}; // Example size, can be adjusted

void perform_matrix_operations(void)
{
    for (int i = 0; i < MATRIX_SIZE; i++)
        for (int j = 0; j < MATRIX_SIZE; j++)
            _result[i][j] = _a[i][j] + _b[i][j]; // Example operation: matrix addition
}

struct matrix_eat_time_param
{
    int size_matrix;
    int repeat_operations;
};

// NOTE: LUT based on empircal measurements done on grid5000/grenoble with troll nodes
struct matrix_eat_time_param MATRIX_LUT[] = {
    [1] = {.size_matrix = 1, .repeat_operations = 1000000}, [2] = {.size_matrix = 2, .repeat_operations = 1000000},
    //.....
};

void matrix_eat_time(int nanoseconds)
{
    struct matrix_eat_time_param param = MATRIX_LUT[1]; // Default to 1x1 matrix
    for (int i = 0; i < param.repeat_operations; i++)
        perform_matrix_operations();
}