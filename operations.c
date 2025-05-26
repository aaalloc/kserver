#include "operations.h"
#include "ksocket_handler.h"
#include <linux/slab.h>
#include <linux/tcp.h>

#include <asm/uaccess.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

int op_cpu_matrix_multiplication_init(op_cpu_args_t *args)
{
    int size = args->args.matrix_multiplication.size;
    args->args.matrix_multiplication.a = kzalloc(size * sizeof(int *), GFP_KERNEL);
    if (!args->args.matrix_multiplication.a)
    {
        kfree(args);
        return -1;
    }
    args->args.matrix_multiplication.b = kzalloc(size * sizeof(int *), GFP_KERNEL);
    if (!args->args.matrix_multiplication.b)
    {
        kfree(args->args.matrix_multiplication.a);
        kfree(args);
        return -1;
    }
    args->args.matrix_multiplication.result = kzalloc(size * sizeof(int *), GFP_KERNEL);
    if (!args->args.matrix_multiplication.result)
    {
        kfree(args->args.matrix_multiplication.b);
        kfree(args->args.matrix_multiplication.a);
        kfree(args);
        return -1;
    }

    for (int i = 0; i < size; i++)
    {
        args->args.matrix_multiplication.a[i] = kzalloc(size * sizeof(int), GFP_KERNEL);
        if (!args->args.matrix_multiplication.a[i])
        {
            for (int j = 0; j < i; j++)
                kfree(args->args.matrix_multiplication.a[j]);
            kfree(args->args.matrix_multiplication.a);
            kfree(args->args.matrix_multiplication.b);
            kfree(args->args.matrix_multiplication.result);
            kfree(args);
            return -1;
        }

        args->args.matrix_multiplication.b[i] = kzalloc(size * sizeof(int), GFP_KERNEL);
        if (!args->args.matrix_multiplication.b[i])
        {
            for (int j = 0; j <= i; j++)
                kfree(args->args.matrix_multiplication.a[j]);
            for (int j = 0; j < i; j++)
                kfree(args->args.matrix_multiplication.b[j]);
            kfree(args->args.matrix_multiplication.a);
            kfree(args->args.matrix_multiplication.b);
            kfree(args->args.matrix_multiplication.result);
            kfree(args);
            return -1;
        }

        args->args.matrix_multiplication.result[i] = kzalloc(size * sizeof(int), GFP_KERNEL);
        if (!args->args.matrix_multiplication.result[i])
        {
            for (int j = 0; j <= i; j++)
                kfree(args->args.matrix_multiplication.a[j]);
            for (int j = 0; j <= i; j++)
                kfree(args->args.matrix_multiplication.b[j]);
            for (int j = 0; j < i; j++)
                kfree(args->args.matrix_multiplication.result[j]);
            kfree(args->args.matrix_multiplication.a);
            kfree(args->args.matrix_multiplication.b);
            kfree(args->args.matrix_multiplication.result);
            kfree(args);
            return -1;
        }
    }

    return 1;
}
void op_cpu_matrix_multiplication_free(op_cpu_args_t *args)
{
    for (int i = 0; i < args->args.matrix_multiplication.size; i++)
    {
        kfree(args->args.matrix_multiplication.a[i]);
        kfree(args->args.matrix_multiplication.b[i]);
        kfree(args->args.matrix_multiplication.result[i]);
    }
    kfree(args->args.matrix_multiplication.result);
    kfree(args->args.matrix_multiplication.b);
    kfree(args->args.matrix_multiplication.a);
}

void op_cpu_matrix_multiplication(op_cpu_args_t *args)
{
    int size = args->args.matrix_multiplication.size;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            args->args.matrix_multiplication.result[i][j] = 0;
            for (int k = 0; k < size; k++)
                args->args.matrix_multiplication.result[i][j] +=
                    args->args.matrix_multiplication.a[i][k] * args->args.matrix_multiplication.b[k][j];
        }
    }
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

int op_disk_word_counting(op_disk_args_t *args)
{
    char *filename = args->filename;
    char *str_to_find = args->args.word_counting.str_to_find;

    struct file *file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(file));
        return -1;
    }

    unsigned char *buf = kzalloc(4096, GFP_KERNEL);
    if (!buf)
    {
        pr_err("Failed to allocate memory for buffer\n");
        filp_close(file, NULL);
        return -ENOMEM;
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
    return count;
}

#define BUFFER_SIZE 4096
ssize_t op_disk_read(op_disk_args_t *args)
{
    char *filename = args->filename;
    int len_to_read = args->args.read.len_to_read;

    struct file *file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(file));
        return -1;
    }

    unsigned char *buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buf)
    {
        pr_err("Failed to allocate memory for buffer\n");
        filp_close(file, NULL);
        return -ENOMEM;
    }

    int total_read = 0;
    ssize_t ret_read;
    while (total_read < len_to_read)
    {
        ret_read = kernel_read(file, buf, BUFFER_SIZE, &file->f_pos);
        if (ret_read < 0)
        {
            pr_err("Failed to read file: %ld\n", ret_read);
            kfree(buf);
            filp_close(file, NULL);
            return ret_read;
        }
        if (ret_read == 0)
            break; // EOF

        total_read += ret_read;
    }

    kfree(buf);
    filp_close(file, NULL);
    return total_read;
}

ssize_t op_disk_write(op_disk_args_t *args)
{
    char *filename = args->filename;
    unsigned char *to_write = args->args.write.to_write;
    int len_to_write = args->args.write.len_to_write;
    int iterations = args->args.write.iterations;

    struct file *file = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(file))
    {
        pr_err("Failed to open file: %ld\n", PTR_ERR(file));
        return -1;
    }

    ssize_t ret;
    ssize_t len_written = 0;
    for (int i = 0; i < iterations; i++)
    {
        ret = kernel_write(file, to_write, len_to_write, &file->f_pos);
        if (ret < 0)
        {
            pr_err("Failed to write to file: %zd\n", ret);
            filp_close(file, NULL);
            return ret;
        }
        len_written += ret;
    }

    filp_close(file, NULL);
    return len_written;
}

int op_network_send(op_network_args_t *args)
{
    int size_payload = args->args.send.size_payload;
    struct socket *sock = args->sock;
    int iterations = args->args.send.iterations;

    uint16_t *buf = kmalloc(size_payload, GFP_KERNEL);
    if (!buf)
        return -1;
    memset(buf, 'A', size_payload);

    for (int i = 0; i < iterations; i++)
    {
        if (ksocket_write((struct ksocket_handler){
                .sock = sock,
                .buf = buf,
                .len = size_payload,
            }) < 0)
            goto clean;
    }

clean:
    kfree(buf);
    return 0;
}