#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "process.h"
#include "scheduler.h"

#define NUM_PROCESSES 5 // Define um número de processos para testar

int main() {
    //Gerador de numeros aleatorios, gerar lista de processos
    srand(time(NULL));
    printf("--- Simulador de Escalonamento ProbSched ---\n");
    printf("A gerar %d processos aleatórios...\n", NUM_PROCESSES);
    Process* process_list = generate_random_processes(NUM_PROCESSES, 10, 8); // APENAS EXEMPLO

    //Caso haja erro
    if (process_list == NULL) {
        fprintf(stderr, "Erro ao alocar memória para a lista de processos.\n");
        return 1;
    }

    //Lista de processos gerada (só pra confirmar)
    printf("\n--- Lista de Processos Gerada ---\n");
    for (int i = 0; i < NUM_PROCESSES; i++) {
        printf("P%d: Chegada=%d, Burst=%d, Prioridade=%d, Deadline=%d\n",
               process_list[i].id,
               process_list[i].arrival_time,
               process_list[i].burst_time,
               process_list[i].priority,
               process_list[i].deadline);
        process_list[i].remaining_time = process_list[i].burst_time;
    }

    schedule_fcfs(process_list, NUM_PROCESSES);

    //Libertar a memória
    free(process_list);
    printf("\n--- Simulação Concluída ---\n");

    return 0;
}