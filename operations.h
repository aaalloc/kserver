#pragma once
#include <linux/types.h>
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
    union
    {
        struct
        {
            void *payload;
            int size_payload;
            int iterations;
        } send;
    } args;
} op_network_args_t;

int op_cpu_matrix_multiplication_init(op_cpu_args_t *args);
void op_cpu_matrix_multiplication_free(op_cpu_args_t *args);
void op_cpu_matrix_multiplication(op_cpu_args_t *args);

int op_disk_word_counting(op_disk_args_t *args);
ssize_t op_disk_read(op_disk_args_t *args);
ssize_t op_disk_write(op_disk_args_t *args);

int op_network_send(op_network_args_t *args);
