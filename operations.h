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

int op_cpu_matrix_multiplication_init(op_cpu_matrix_multiplication_args_t *args);
void op_cpu_matrix_multiplication_free(op_cpu_matrix_multiplication_args_t *args);
void op_cpu_matrix_multiplication(op_cpu_matrix_multiplication_args_t *args);

int op_disk_word_counting(op_disk_word_counting_args_t *args);
int op_network_send(op_network_send_args_t *args);
