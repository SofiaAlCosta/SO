#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include <stdio.h>

// --- Estados do Processo ---
typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_BLOCKED,
    STATE_TERMINATED
} ProcessState;


// --- Estrutura do Processo (PCB) ---
typedef struct {
    int id;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int deadline;
    int period;

    // --- Campos para Métricas e Estado ---
    int start_time;
    int finish_time;
    int waiting_time;
    int turnaround_time;

    // --- Campos para Funcionalidades Avançadas ---
    ProcessState state;
    int current_priority;
    int time_in_ready_queue;
    int io_burst_duration;
    int io_completion_time;
    int current_queue;
    int time_slice_remaining;

} Process;

// --- Funções de Geração/Leitura/Inicialização ---
Process* generate_static_processes(int count);

Process* generate_random_processes(int count, double lambda_arrival, double p1, double p2, int burst_dist_type, int prio_type,
                                   double io_chance, int min_io_duration, int max_io_duration);

Process* read_processes_from_file(const char* filename, int* count_ptr);
void initialize_process_state(Process *p);

#endif