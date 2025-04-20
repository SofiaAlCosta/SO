// process.c
#define _USE_MATH_DEFINES
#include <math.h>
#include "process.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // Para memset

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- NOVA: Função auxiliar para inicializar ---
void initialize_process_state(Process *p) {
    p->remaining_time = p->burst_time;
    p->start_time = -1;
    p->finish_time = -1;
    p->waiting_time = 0;
    p->turnaround_time = 0;
    p->state = STATE_NEW;
    p->current_priority = p->priority;
    p->time_in_ready_queue = 0;
    // io_burst_duration é definido na geração/leitura
    p->io_completion_time = -1;
    p->current_queue = -1;
    p->time_slice_remaining = 0;
}


// Distribuição exponencial
double rand_exponential(double lambda) {
    if (lambda <= 0) return 1.0; // Evitar divisão por zero ou log de 1
    double u;
    do {
        u = (double)rand() / (RAND_MAX + 1.0);
    } while (u == 0.0 || u == 1.0); // Evitar log(0) ou log(infinito)
    return -log(u) / lambda; // Usar u em vez de 1-u é equivalente
}

// Distribuição normal (Box-Muller)
double rand_normal(double mean, double stddev) {
    if (stddev < 0) stddev = 0;
    // Evitar u1=0 que causa log(0) = -infinito
    double u1, u2;
    do {
        u1 = (double)rand() / (RAND_MAX + 1.0);
    } while (u1 == 0.0);
    u2 = (double)rand() / (RAND_MAX + 1.0);
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
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
        list[i].priority = 1 + (i % 5);
        list[i].deadline = list[i].arrival_time + 10 + (rand() % 5);
        list[i].period = 0;
        list[i].io_burst_duration = (rand() % 2 == 0) ? (2 + rand() % 4) : 0; // Exemplo: 50% chance de I/O
        initialize_process_state(&list[i]); // Inicializa estado
    }
    return list;
}

// Geração de processos aleatórios (MODIFICADA para I/O)
Process* generate_random_processes(int count, double lambda_arrival, double p1, double p2, int burst_dist_type, int prio_type,
                                   double io_chance, int min_io_duration, int max_io_duration) { // Adicionados params I/O
    if (count <= 0) return NULL;
    Process* list = malloc(sizeof(Process) * count);
     if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória em generate_random_processes\n");
        return NULL;
    }
    double current_time = 0.0;

    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;

        // Tempo de Chegada (Exponencial para inter-chegada)
        current_time += rand_exponential(lambda_arrival);
        list[i].arrival_time = (int)round(current_time);
        if (list[i].arrival_time < 0) list[i].arrival_time = 0; // Garantir não negativo

        // Tempo de Burst (Normal ou Exponencial)
        double burst_val;
        if (burst_dist_type == 1) { // Exponencial para Burst
            double lambda_burst = p1;
             if (lambda_burst <= 0) lambda_burst = 0.1;
            burst_val = rand_exponential(lambda_burst);
        } else { // Normal (padrão)
            double mean = p1;
            double stddev = p2;
            burst_val = rand_normal(mean, stddev);
        }
        list[i].burst_time = (int)round(burst_val);
        if (list[i].burst_time <= 0) list[i].burst_time = 1; // Garante burst mínimo de 1

        // Prioridade (Ponderada ou Uniforme)
        if (prio_type == 1) { // Uniforme
             list[i].priority = 1 + rand() % 5; // Exemplo: 1 a 5 uniforme
        } else { // Ponderada (padrão)
            int dice = rand() % 100;
            if (dice < 40) list[i].priority = 1;       // 40%
            else if (dice < 70) list[i].priority = 2; // 30%
            else if (dice < 90) list[i].priority = 3; // 20%
            else if (dice < 97) list[i].priority = 4; // 7%
            else list[i].priority = 5;                // 3%
        }

        // Deadline (Exemplo simples melhorado)
        double avg_burst = (burst_dist_type == 1 && p1 > 0) ? (1.0 / p1) : ((burst_dist_type == 0) ? p1 : 5.0); // Média do burst estimada
        int slack = (int)round(avg_burst * 1.5) + (rand() % ((int)avg_burst + 1)); // Folga maior
        list[i].deadline = list[i].arrival_time + list[i].burst_time + slack;
         if (list[i].deadline <= list[i].arrival_time) { // Garantir deadline > chegada
             list[i].deadline = list[i].arrival_time + list[i].burst_time + 1;
         }

        list[i].period = 0; // Não usado ativamente aqui

        // --- NOVO: Geração de I/O ---
        list[i].io_burst_duration = 0;
        if (((double)rand() / RAND_MAX) < io_chance) { // Verifica a chance de ter I/O
            if (max_io_duration > min_io_duration) {
                 list[i].io_burst_duration = min_io_duration + rand() % (max_io_duration - min_io_duration + 1);
            } else if (max_io_duration >= 0) {
                list[i].io_burst_duration = min_io_duration;
            }
             if (list[i].io_burst_duration <= 0) list[i].io_burst_duration = 1; // Garante duração positiva se tiver I/O
        }
        //---------------------------

        initialize_process_state(&list[i]); // Inicializa os outros campos
    }
    return list;
}


// Leitura de processos de um ficheiro
Process* read_processes_from_file(const char* filename, int* count_ptr) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir ficheiro de processos");
        *count_ptr = 0;
        return NULL;
    }
    int count = 0;
    char buffer[256];
    // Contar linhas válidas primeiro
    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#' || buffer[0] == ' ') continue;
        int id, arr, bur, pri, dead, per, io_dur=0; // io_dur opcional
        // Tenta ler 7, depois 6 campos
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
        int id, arr, bur, pri, dead, per, io_dur = 0; // Default I/O duration to 0
        int fields_read = sscanf(buffer, "%d %d %d %d %d %d %d", &id, &arr, &bur, &pri, &dead, &per, &io_dur);

        if (fields_read >= 6) {
            list[current_process_index].id = id;
            list[current_process_index].arrival_time = arr;
            list[current_process_index].burst_time = (bur > 0) ? bur : 1; // Burst > 0
            list[current_process_index].priority = pri;
            list[current_process_index].deadline = dead;
            list[current_process_index].period = per;
            // Define io_burst_duration apenas se foi lido (7 campos) e for positivo
            list[current_process_index].io_burst_duration = (fields_read == 7 && io_dur > 0) ? io_dur : 0;

            initialize_process_state(&list[current_process_index]); // Inicializa outros campos
            current_process_index++;
        }
    }
    fclose(file);
    *count_ptr = count;
    printf("Lidos %d processos do ficheiro '%s'.\n", count, filename);
    return list;
}