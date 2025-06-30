#pragma once
#define MATRIX_SIZE 1000

struct matrix_eat_time_param
{
    int size_matrix;
    int repeat_operations;
};

// some function that will eat a precise amount of time doing something not intersting

int **create_matrix(int size);
void free_matrix(int **m, int size);
void matrix_eat_time(struct matrix_eat_time_param param);

void clock_eat_time(int ns);
