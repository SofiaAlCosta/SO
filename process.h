#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include <stdio.h>

// Estrutura do Processo (PCB)
typedef struct {
    int id;               // Identificador único do processo
    int arrival_time;     // Tempo de chegada
    int burst_time;       // Tempo total de execução necessário
    int remaining_time;   // Tempo restante de execução (para preempção)
    int priority;         // Prioridade (quanto menor, maior prioridade)
    int deadline;         // Deadline absoluto (para EDF)
    int period;           // Período (para Rate Monotonic)
} Process;

// Geração de processos estáticos (valores fixos aleatórios)
Process* generate_static_processes(int count);

// Geração de processos aleatórios com distribuições probabilísticas
Process* generate_random_processes(int count, int max_arrival, int max_burst);

#endif // PROCESS_H