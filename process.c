// process.c
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

// Distribuição normal (Box-Muller)
double rand_normal(double mean, double stddev) {
    // Garantir stddev não negativo
    if (stddev < 0) stddev = 0;
    double u1 = (double)rand() / (RAND_MAX + 1.0);
    double u2 = (double)rand() / (RAND_MAX + 1.0);
    double z = sqrt(-2.0 * log(u1)) * cos(2 * M_PI * u2);
    return z * stddev + mean;
}

// Geração de processos estáticos (inalterada)
Process* generate_static_processes(int count) {
    // ... (código inalterado) ...
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
        if (list[i].burst_time <=0) list[i].burst_time = 1; // Garantir burst positivo
        list[i].remaining_time = list[i].burst_time;
        list[i].priority = 1 + (i % 5);
        list[i].deadline = list[i].arrival_time + 10 + (rand() % 5); // Deadline um pouco variado
        list[i].period = 0;
    }
    return list;
}

// Geração de processos aleatórios com distribuições probabilísticas (com opções)
Process* generate_random_processes(int count, double lambda_arrival, double p1, double p2, int burst_dist_type, int prio_type) {
    if (count <= 0) return NULL;
    Process* list = malloc(sizeof(Process) * count);
     if (!list) {
        fprintf(stderr, "Erro: Falha ao alocar memória em generate_random_processes\n");
        return NULL;
    }
    double current_time = 0.0;

    for (int i = 0; i < count; i++) {
        list[i].id = i + 1;

        // Tempo de Chegada (Exponencial)
        current_time += rand_exponential(lambda_arrival);
        list[i].arrival_time = (int)round(current_time);
        if (list[i].arrival_time < 0) list[i].arrival_time = 0;

        // Tempo de Burst (Normal ou Exponencial)
        double burst_val;
        if (burst_dist_type == 1) { // Exponencial para Burst
            double lambda_burst = p1; // p1 é lambda_burst neste caso
             if (lambda_burst <= 0) lambda_burst = 0.1; // Evitar lambda inválido
            burst_val = rand_exponential(lambda_burst);
             // Média da exponencial é 1/lambda. Ajustar se necessário.
        } else { // Normal (padrão)
            double mean = p1;
            double stddev = p2;
            burst_val = rand_normal(mean, stddev);
        }
        list[i].burst_time = (int)round(burst_val);
        if (list[i].burst_time <= 0) list[i].burst_time = 1; // Garante burst mínimo de 1
        list[i].remaining_time = list[i].burst_time;


        // Prioridade (Ponderada ou Uniforme)
        if (prio_type == 1) { // Uniforme
             list[i].priority = 1 + rand() % 5; // Exemplo: 1 a 5 uniforme
        } else { // Ponderada (padrão)
            int dice = rand() % 100;
            if (dice < 40) list[i].priority = 1;
            else if (dice < 70) list[i].priority = 2;
            else if (dice < 90) list[i].priority = 3;
            else if (dice < 97) list[i].priority = 4;
            else list[i].priority = 5;
        }

        // Deadline (Exemplo simples)
        double avg_burst = (burst_dist_type == 1) ? (1.0 / p1) : p1; // Média do burst
        list[i].deadline = list[i].arrival_time + (int)round(2.0 * avg_burst) + (rand() % (int)(avg_burst+1)) - (int)(avg_burst/2.0);
         if (list[i].deadline < list[i].arrival_time + list[i].burst_time) {
             list[i].deadline = list[i].arrival_time + list[i].burst_time + 1;
         }

        list[i].period = 0;
    }
    return list;
}


// Leitura de processos de um ficheiro (inalterada)
Process* read_processes_from_file(const char* filename, int* count_ptr) {
    // ... (código inalterado) ...
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir ficheiro de processos");
        *count_ptr = 0;
        return NULL;
    }
    int count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#') continue;
        int id, arr, bur, pri, dead, per;
        if (sscanf(buffer, "%d %d %d %d %d %d", &id, &arr, &bur, &pri, &dead, &per) == 6) count++;
        else fprintf(stderr, "Aviso: Linha mal formatada ignorada no ficheiro: %s", buffer);
    }
    if (count == 0) { /* ... erro ... */ fclose(file); *count_ptr = 0; return NULL; }
    Process* list = malloc(sizeof(Process) * count);
    if (!list) { /* ... erro malloc ... */ fclose(file); *count_ptr = 0; return NULL; }
    rewind(file);
    int current_process_index = 0;
    while (fgets(buffer, sizeof(buffer), file) && current_process_index < count) {
         if (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '#') continue;
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