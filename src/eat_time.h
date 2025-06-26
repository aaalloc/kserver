#pragma once

// some function that will eat a precise amount of time doing something not intersting

int **create_matrix(int size);
void free_matrix(int **m, int size);
void matrix_eat_time(int nanoseconds);

void clock_eat_time(unsigned long nanoseconds);
