#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include <stdio.h>

// Estrutura do Processo (PCB)
typedef struct {
    int id;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int deadline;
    int period;
} Process;

// Geração de processos estáticos
Process* generate_static_processes(int count);

// Geração de processos aleatórios com distribuições probabilísticas (nova assinatura)
Process* generate_random_processes(int count, double lambda, double p1, double p2, int burst_dist_type, int prio_type);

// Leitura de processos de um ficheiro
Process* read_processes_from_file(const char* filename, int* count);

#endif