#pragma once
#include <linux/spinlock.h>
#include <linux/types.h>

#ifdef SIZE_BUF_IO
#define BUFFER_SIZE_IO SIZE_BUF_IO
#else
#define BUFFER_SIZE_IO 4096
#endif

typedef struct
{
    union
    {
        struct
        {
            int size;
            int **a;
            int **b;
            int **result;
        } matrix_multiplication;
    } args;
} op_cpu_args_t;

typedef struct
{
    char *filename;
    union
    {
        struct
        {
            int len_to_read;
        } read;
        struct
        {
            unsigned char *to_write;
            int len_to_write;
            int iterations;
        } write;
        struct
        {
            char *str_to_find;
        } word_counting;
    } args;
} op_disk_args_t;

typedef struct
{
    struct socket *sock;
    spinlock_t *lock; // Optional, for thread safety
    union
    {
        struct
        {
            void *payload;
            int size_payload;
            int iterations;
        } send;
        struct
        {
            char *ip;
            int port;
            void *payload;
            int size_payload;
            int iterations;
        } conn_send;
    } args;
} op_network_args_t;

int op_cpu_matrix_multiplication_init(op_cpu_args_t *args);
void op_cpu_matrix_multiplication_free(op_cpu_args_t *args);
void op_cpu_matrix_multiplication(op_cpu_args_t *args);

int op_disk_word_counting(op_disk_args_t *args);
ssize_t op_disk_read(op_disk_args_t *args);
ssize_t op_disk_write(op_disk_args_t *args);

int op_network_send(op_network_args_t *args);
int op_network_conn_send(op_network_args_t *args);
