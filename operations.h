#pragma once

typedef struct
{
    int size;
    int **a;
    int **b;
    int **result;
} op_cpu_matrix_multiplication_args_t;

typedef struct
{
    char *filename;
    char *str_to_find;
} op_disk_word_counting_args_t;

typedef struct
{
    struct socket *sock;
    int size_payload;
    int iterations;
} op_network_send_args_t;

op_cpu_matrix_multiplication_args_t *op_cpu_matrix_multiplication_init(int size);
void op_cpu_matrix_multiplication_free(op_cpu_matrix_multiplication_args_t *args);
void *op_cpu_matrix_multiplication(void *args);

void read_file(char *filename);

void *op_disk_word_counting(void *args);
void *op_network_send(void *args);
