#include "operations.h"
#include <linux/slab.h>
#include <linux/tcp.h>

#include <asm/uaccess.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

op_cpu_matrix_multiplication_args_t *op_cpu_matrix_multiplication_init(int size)
{
    op_cpu_matrix_multiplication_args_t *args = kmalloc(sizeof(op_cpu_matrix_multiplication_args_t), GFP_KERNEL);
    if (!args)
        return NULL;
    args->size = size;
    args->a = kzalloc(size * sizeof(int *), GFP_KERNEL);
    if (!args->a)
    {
        kfree(args);
        return NULL;
    }
    args->b = kzalloc(size * sizeof(int *), GFP_KERNEL);
    if (!args->b)
    {
        kfree(args->a);
        kfree(args);
        return NULL;
    }
    args->result = kzalloc(size * sizeof(int *), GFP_KERNEL);
    if (!args->result)
    {
        kfree(args->b);
        kfree(args->a);
        kfree(args);
        return NULL;
    }

    for (int i = 0; i < size; i++)
    {
        args->a[i] = kzalloc(size * sizeof(int), GFP_KERNEL);
        if (!args->a[i])
        {
            for (int j = 0; j < i; j++)
                kfree(args->a[j]);
            kfree(args->result);
            kfree(args->b);
            kfree(args->a);
            kfree(args);
            return NULL;
        }
        args->b[i] = kzalloc(size * sizeof(int), GFP_KERNEL);
        if (!args->b[i])
        {
            for (int j = 0; j <= i; j++)
                kfree(args->a[j]);
            kfree(args->result);
            kfree(args->b);
            kfree(args->a);
            kfree(args);
            return NULL;
        }
        args->result[i] = kzalloc(size * sizeof(int), GFP_KERNEL);
        if (!args->result[i])
        {
            for (int j = 0; j <= i; j++)
                kfree(args->a[j]);
            for (int j = 0; j < i; j++)
                kfree(args->b[j]);
            kfree(args->result);
            kfree(args->b);
            kfree(args->a);
            kfree(args);
            return NULL;
        }
    }

    return args;
}
void op_cpu_matrix_multiplication_free(op_cpu_matrix_multiplication_args_t *args)
{
    if (args)
    {
        for (int i = 0; i < args->size; i++)
        {
            kfree(args->a[i]);
            kfree(args->b[i]);
            kfree(args->result[i]);
        }
        kfree(args->result);
        kfree(args->b);
        kfree(args->a);
        kfree(args);
    }
}
void *op_cpu_matrix_multiplication(void *args)
{
    op_cpu_matrix_multiplication_args_t *op_args = (op_cpu_matrix_multiplication_args_t *)args;
    int size = op_args->size;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            op_args->result[i][j] = 0;
            for (int k = 0; k < size; k++)
                op_args->result[i][j] += op_args->a[i][k] * op_args->b[k][j];
        }
    }
    return NULL;
}

void read_file(char *filename)
{
    struct file *file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(file));
        return;
    }

    char buf[512];
    while (1)
    {
        ssize_t ret = kernel_read(file, buf, sizeof(buf), &file->f_pos);
        if (ret < 0)
        {
            pr_err("Failed to read file: %ld\n", ret);
            break;
        }
        if (ret == 0)
            break; // EOF
        pr_info("Read %zd bytes: %.*s\n", ret, (int)ret, buf);
    }

    if (file)
        filp_close(file, NULL);
}

void *op_disk_word_counting(void *args)
{
    op_disk_word_counting_args_t *op_args = (op_disk_word_counting_args_t *)args;
    char *filename = op_args->filename;
    char *str_to_find = op_args->str_to_find;

    struct file *file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(file));
        return NULL;
    }

    unsigned char *buf = kzalloc(4096, GFP_KERNEL);
    if (!buf)
    {
        filp_close(file, NULL);
        return NULL;
    }

    int count = 0;
    while (1)
    {
        ssize_t ret = kernel_read(file, buf, sizeof(buf), &file->f_pos);
        if (ret < 0)
        {
            pr_err("Failed to read file: %ld\n", ret);
            break;
        }
        if (ret == 0)
            break; // EOF

        // Count occurrences of str_to_find in buf
        char *p = buf;
        while ((p = strstr(p, str_to_find)) != NULL)
        {
            count++;
            p += strlen(str_to_find);
        }
    }

    if (file)
        filp_close(file, NULL);
    return (void *)(long)count;
}
void *op_network_send(void *args)
{
    op_network_send_args_t *op_args = (op_network_send_args_t *)args;
    int size_payload = op_args->size_payload;
    struct socket *sock = op_args->sock;
    int iterations = op_args->iterations;

    char *buf = kmalloc(size_payload, GFP_KERNEL);
    if (!buf)
        return NULL;
    memset(buf, 'A', size_payload);

    for (int i = 0; i < iterations; i++)
    {
        struct msghdr msg = {0};
        struct kvec vec;
        int ret;
        vec.iov_base = buf;
        vec.iov_len = size_payload;

        ret = kernel_sendmsg(sock, &msg, &vec, 1, size_payload);
        if (ret < 0)
        {
            pr_err("Failed to send message: %d\n", ret);
            goto clean;
        }
    }
clean:
    kfree(buf);
    return NULL;
}