#define _USE_MATH_DEFINES
#include "process.h"
#include <time.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Distribuição exponencial (para arrival time)
double rand_exponential(double lambda) {
    double u = (double) rand() / RAND_MAX;
    return -log(1 - u) / lambda;
}

// Distribuição normal (Box-Muller)
double rand_normal(double mean, double stddev) {
    double u1 = (double) rand() / RAND_MAX;
    double u2 = (double) rand() / RAND_MAX;
    double z = sqrt(-2.0 * log(u1)) * cos(2 * M_PI * u2);
    return z * stddev + mean;
}

// Geração de processos com valores fixos ou aleatórios simples
Process* generate_static_processes(int count) {
    Process* list = malloc(sizeof(Process) * count);
    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;
        list[i].arrival_time = i * 2;
        list[i].burst_time = 5 + (i % 3);
        list[i].remaining_time = list[i].burst_time;
        list[i].priority = 1 + (i % 5);
        list[i].deadline = list[i].arrival_time + 10;  // exemplo
        list[i].period = 0;
    }
    return list;
}

// Geração de processos com distribuições probabilísticas
Process* generate_random_processes(int count, int max_arrival, int max_burst) {
    Process* list = malloc(sizeof(Process) * count);
    double current_time = 0.0;

    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;

        current_time += rand_exponential(0.5);
        list[i].arrival_time = (int) current_time;

        int burst = (int) rand_normal(5.0, 2.0);
        list[i].burst_time = burst > 0 ? burst : 1;
        list[i].remaining_time = list[i].burst_time;

        list[i].priority = 1 + rand() % 5;
        list[i].deadline = list[i].arrival_time + 10 + rand() % 10;
        list[i].period = 0; // por agora não são periódicos
    }

    return list;
}