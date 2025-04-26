#define _USE_MATH_DEFINES
#include <math.h>
#include "process.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void initialize_process_state(Process *p) {
    p->remaining_time = p->burst_time;
    p->start_time = -1;
    p->finish_time = -1;
    p->waiting_time = 0;
    p->turnaround_time = 0;
    p->state = STATE_NEW;
    p->current_priority = p->priority;
    p->time_in_ready_queue = 0;
    p->io_completion_time = -1;
    p->current_queue = -1;
    p->time_slice_remaining = 0;
}

// --- Distribuição exponencial ---
double rand_exponential(double lambda) {
    if (lambda <= 0) return 1.0;
    double u;
    do {
        u = (double)rand() / (RAND_MAX + 1.0);
    } while (u == 0.0 || u == 1.0);
    return -log(u) / lambda;
}

// --- Distribuição normal (Box-Muller) ---
double rand_normal(double mean, double stddev) {
    if (stddev < 0) stddev = 0;
    double u1, u2;
    do {
        u1 = (double)rand() / (RAND_MAX + 1.0);
    } while (u1 == 0.0);
    u2 = (double)rand() / (RAND_MAX + 1.0);
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return z * stddev + mean;
}

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
        list[i].priority = 1 + (i % 5);
        list[i].deadline = list[i].arrival_time + 10 + (rand() % 5);
        list[i].period = 0;
        list[i].io_burst_duration = (rand() % 2 == 0) ? (2 + rand() % 4) : 0;
        initialize_process_state(&list[i]);
    }
    return list;
}

Process* generate_random_processes(int count, double lambda_arrival, double p1, double p2, int burst_dist_type, int prio_type,
                                   double io_chance, int min_io_duration, int max_io_duration) {
    if (count <= 0) return NULL;
    Process* list = malloc(sizeof(Process) * count);
     if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória em generate_random_processes\n");
        return NULL;
    }
    double current_time = 0.0;

    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;

        current_time += rand_exponential(lambda_arrival);
        list[i].arrival_time = (int)round(current_time);
        if (list[i].arrival_time < 0) list[i].arrival_time = 0;

        double burst_val;
        if (burst_dist_type == 1) {
            double lambda_burst = p1;
             if (lambda_burst <= 0) lambda_burst = 0.1;
            burst_val = rand_exponential(lambda_burst);
        } else {
            double mean = p1;
            double stddev = p2;
            burst_val = rand_normal(mean, stddev);
        }
        list[i].burst_time = (int)round(burst_val);
        if (list[i].burst_time <= 0) list[i].burst_time = 1;

        if (prio_type == 1) {
             list[i].priority = 1 + rand() % 5;
        } else {
            int dice = rand() % 100;
            if (dice < 40) list[i].priority = 1;
            else if (dice < 70) list[i].priority = 2;
            else if (dice < 90) list[i].priority = 3;
            else if (dice < 97) list[i].priority = 4;
            else list[i].priority = 5;
        }

        double avg_burst = (burst_dist_type == 1 && p1 > 0) ? (1.0 / p1) : ((burst_dist_type == 0) ? p1 : 5.0);
        int slack = (int)round(avg_burst * 1.5) + (rand() % ((int)avg_burst + 1));
        list[i].deadline = list[i].arrival_time + list[i].burst_time + slack;
         if (list[i].deadline <= list[i].arrival_time) {
             list[i].deadline = list[i].arrival_time + list[i].burst_time + 1;
         }

        list[i].period = 0;

        list[i].io_burst_duration = 0;
        if (((double)rand() / RAND_MAX) < io_chance) {
            if (max_io_duration > min_io_duration) {
                 list[i].io_burst_duration = min_io_duration + rand() % (max_io_duration - min_io_duration + 1);
            } else if (max_io_duration >= 0) {
                list[i].io_burst_duration = min_io_duration;
            }
             if (list[i].io_burst_duration <= 0) list[i].io_burst_duration = 1;
        }

        initialize_process_state(&list[i]);
    }
    return list;
}


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
        if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#' || buffer[0] == ' ') continue;
        int id, arr, bur, pri, dead, per, io_dur=0;
        if (sscanf(buffer, "%d %d %d %d %d %d %d", &id, &arr, &bur, &pri, &dead, &per, &io_dur) >= 6) {
             count++;
        } else {
            fprintf(stderr, "Aviso: Linha mal formatada ignorada no ficheiro: %s", buffer);
        }
    }

    if (count == 0) {
        fprintf(stderr, "Erro: Nenhum processo válido encontrado no ficheiro '%s'.\n", filename);
        fclose(file);
        *count_ptr = 0;
        return NULL;
    }

    Process* list = malloc(sizeof(Process) * count);
    if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória para %d processos do ficheiro.\n", count);
        fclose(file);
        *count_ptr = 0;
        return NULL;
    }

    rewind(file);
    int current_process_index = 0;
    while (fgets(buffer, sizeof(buffer), file) && current_process_index < count) {
         if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#' || buffer[0] == ' ') continue;
        int id, arr, bur, pri, dead, per, io_dur = 0;
        int fields_read = sscanf(buffer, "%d %d %d %d %d %d %d", &id, &arr, &bur, &pri, &dead, &per, &io_dur);

        if (fields_read >= 6) {
            list[current_process_index].id = id;
            list[current_process_index].arrival_time = arr;
            list[current_process_index].burst_time = (bur > 0) ? bur : 1;
            list[current_process_index].priority = pri;
            list[current_process_index].deadline = dead;
            list[current_process_index].period = per;
            list[current_process_index].io_burst_duration = (fields_read == 7 && io_dur > 0) ? io_dur : 0;

            initialize_process_state(&list[current_process_index]);
            current_process_index++;
        }
    }
    fclose(file);
    *count_ptr = count;
    printf("Lidos %d processos do ficheiro '%s'.\n", count, filename);
    return list;
}