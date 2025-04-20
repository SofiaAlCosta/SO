#define _USE_MATH_DEFINES
#include <math.h>
#include "process.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Distribuição exponencial
double rand_exponential(double lambda) {
    if (lambda <= 0) return 1.0;
    double u = (double)rand() / (RAND_MAX + 1.0);
    return -log(1.0 - u) / lambda;
}

// Distribuição normal
double rand_normal(double mean, double stddev) {
    double u1 = (double)rand() / (RAND_MAX + 1.0);
    double u2 = (double)rand() / (RAND_MAX + 1.0);
    double z = sqrt(-2.0 * log(u1)) * cos(2 * M_PI * u2);
    return z * stddev + mean;
}

// Geração de processos estáticos
Process* generate_static_processes(int count) {
    if (count <= 0) return NULL;
    Process* list = malloc(sizeof(Process) * count);
    if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória em generate_static_processes\n");
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;
        list[i].arrival_time = i * 2;
        list[i].burst_time = 5 + (i % 3);
        if (list[i].burst_time <=0) list[i].burst_time = 1;
        list[i].remaining_time = list[i].burst_time;
        list[i].priority = 1 + (i % 5);
        list[i].deadline = list[i].arrival_time + 10 + (rand() % 5);
        list[i].period = 0;
    }
    return list;
}

Process* generate_random_processes(int count, double lambda, double mean, double stddev) {
    if (count <= 0) return NULL;
    Process* list = malloc(sizeof(Process) * count);
     if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória em generate_random_processes\n");
        return NULL;
    }
    double current_time = 0.0;

    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;

        // Tempo de Chegada
        current_time += rand_exponential(lambda);
        list[i].arrival_time = (int)round(current_time);
        if (list[i].arrival_time < 0) list[i].arrival_time = 0;

        // Tempo de Burst
        int burst = (int)round(rand_normal(mean, stddev));
        list[i].burst_time = burst > 0 ? burst : 1;
        list[i].remaining_time = list[i].burst_time;

        // Prioridade
        int dice = rand() % 100;
        if (dice < 40) {
            list[i].priority = 1;
        } else if (dice < 70) {
            list[i].priority = 2;
        } else if (dice < 90) {
            list[i].priority = 3;
        } else if (dice < 97) {
            list[i].priority = 4;
        } else {
            list[i].priority = 5;
        }

        list[i].deadline = list[i].arrival_time + (int)round(2.0 * mean) + (rand() % (int)(mean+1)) - (int)(mean/2.0);
         if (list[i].deadline < list[i].arrival_time + list[i].burst_time) {
             list[i].deadline = list[i].arrival_time + list[i].burst_time + 1;
         }


        list[i].period = 0;
    }

    // --- Leitura de Processos de Ficheiro ---
Process* read_processes_from_file(const char* filename, int* count_ptr) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir ficheiro de processos");
        *count_ptr = 0;
        return NULL;
    }

    int count = 0;
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#') {
            continue;
        }
        int id, arr, bur, pri, dead, per;
        if (sscanf(buffer, "%d %d %d %d %d %d", &id, &arr, &bur, &pri, &dead, &per) == 6) {
             count++;
        } else {
            fprintf(stderr, "Aviso: Linha mal formatada ignorada no ficheiro: %s", buffer);
        }
    }

    if (count == 0) {
        fprintf(stderr, "Erro: Nenhum processo válido encontrado no ficheiro '%s'\n", filename);
        fclose(file);
        *count_ptr = 0;
        return NULL;
    }

    Process* list = malloc(sizeof(Process) * count);
    if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória para ler processos do ficheiro\n");
        fclose(file);
        *count_ptr = 0;
        return NULL;
    }

    rewind(file);
    int current_process_index = 0;
    while (fgets(buffer, sizeof(buffer), file) && current_process_index < count) {
         if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#') {
            continue;
        }
        int id, arr, bur, pri, dead, per;
        if (sscanf(buffer, "%d %d %d %d %d %d", &id, &arr, &bur, &pri, &dead, &per) == 6) {
            list[current_process_index].id = id;
            list[current_process_index].arrival_time = arr;
            list[current_process_index].burst_time = (bur > 0) ? bur : 1;
            list[current_process_index].remaining_time = list[current_process_index].burst_time;
            list[current_process_index].priority = pri;
            list[current_process_index].deadline = dead;
            list[current_process_index].period = per;
            current_process_index++;
        }
    }

    fclose(file);
    *count_ptr = count;
    printf("Lidos %d processos do ficheiro '%s'.\n", count, filename);
    return list;
}

    return list;
}