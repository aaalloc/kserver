#pragma once

struct matrix_eat_time_param
{
    int size_matrix;
    int repeat_operations;
};

// NOTE: LUT based on empircal measurements done on grid5000/grenoble with troll nodes
static struct matrix_eat_time_param MATRIX_LUT[] = {
    // index in milliseconds
    [1000] = {.size_matrix = 1000, .repeat_operations = 1059}, [500] = {.size_matrix = 1000, .repeat_operations = 525},
    [100] = {.size_matrix = 1000, .repeat_operations = 105},   [50] = {.size_matrix = 1000, .repeat_operations = 50},
    [10] = {.size_matrix = 1000, .repeat_operations = 10},     [5] = {.size_matrix = 1000, .repeat_operations = 5},
    [1] = {.size_matrix = 1000, .repeat_operations = 1}};

// some function that will eat a precise amount of time doing something not intersting

int **create_matrix(int size);
void free_matrix(int **m, int size);
void matrix_eat_time(struct matrix_eat_time_param param);

void clock_eat_time(int ns);
