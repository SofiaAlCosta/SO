#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "process.h"
#include "scheduler.h"

void print_usage() {
    printf("Uso: ./probsched [opções]\n");
    printf("Opções:\n");
    printf("  -h, --help          Mostrar esta ajuda\n");
    printf("  -a <algoritmo>      Algoritmo a usar (fcfs, sjf, rr, prio-np, prio-p, edf, rm) (padrão: fcfs)\n");
    printf("  -n <numero>         Número de processos a gerar (padrão: 5)\n");
    printf("  -q <quantum>        Time quantum para Round Robin (padrão: 4)\n");
    printf("  -s <semente>        Semente para gerador aleatório (padrão: baseado no tempo)\n");
    printf("  --gen <modo>        Modo de geração: 'static' ou 'random' (padrão: random)\n");
}

int main(int argc, char *argv[]) {

    // --- Valores Padrão ---
    char algorithm[20] = "fcfs";
    int num_processes = 5;
    int quantum = 4;
    int seed = time(NULL);
    int use_fixed_seed = 0;
    char generation_mode[10] = "random";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "-a") == 0) {
            if (i + 1 < argc) {
                strncpy(algorithm, argv[++i], sizeof(algorithm) - 1);
                algorithm[sizeof(algorithm) - 1] = '\0';
            } else {
                fprintf(stderr, "Erro: Flag -a requer um argumento (nome do algoritmo).\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 < argc) {
                num_processes = atoi(argv[++i]);
                if (num_processes <= 0) {
                    fprintf(stderr, "Erro: Número de processos inválido.\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Erro: Flag -n requer um argumento (número).\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-q") == 0) {
            if (i + 1 < argc) {
                quantum = atoi(argv[++i]);
                 if (quantum <= 0) {
                    fprintf(stderr, "Erro: Quantum inválido.\n");
                    return 1;
                }
            } else {
                 fprintf(stderr, "Erro: Flag -q requer um argumento (número).\n");
                 return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
             if (i + 1 < argc) {
                seed = atoi(argv[++i]);
                use_fixed_seed = 1;
            } else {
                 fprintf(stderr, "Erro: Flag -s requer um argumento (número da semente).\n");
                 return 1;
            }
        } else if (strcmp(argv[i], "--gen") == 0) {
             if (i + 1 < argc) {
                strncpy(generation_mode, argv[++i], sizeof(generation_mode) - 1);
                generation_mode[sizeof(generation_mode) - 1] = '\0';
                if (strcmp(generation_mode, "static") != 0 && strcmp(generation_mode, "random") != 0) {
                     fprintf(stderr, "Erro: Modo de geração inválido ('static' ou 'random').\n");
                     return 1;
                }
            } else {
                 fprintf(stderr, "Erro: Flag --gen requer um argumento ('static' ou 'random').\n");
                 return 1;
            }
        } else {
            fprintf(stderr, "Erro: Opção desconhecida '%s'\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    // --- Inicialização e Geração ---
    printf("--- Simulador ProbSched ---\n");
    printf("Configuração: Algoritmo=%s, Processos=%d, Quantum(RR)=%d, Geração=%s, Semente=%s%d\n",
           algorithm, num_processes, quantum, generation_mode,
           use_fixed_seed ? "(fixa) " : "(tempo) ", seed);

    srand(seed);

    Process* process_list = NULL;
    if (strcmp(generation_mode, "static") == 0) {
        printf("A gerar %d processos estáticos...\n", num_processes);
        process_list = generate_static_processes(num_processes);
    } else {
         printf("A gerar %d processos aleatórios...\n", num_processes);
         process_list = generate_random_processes(num_processes, 10, 8);
    }

     if (process_list == NULL) {
        fprintf(stderr, "Erro ao alocar memória para a lista de processos.\n");
        return 1;
    }

    printf("\n--- Lista de Processos Gerada ---\n");
    printf("ID | Chegada | Burst | Prioridade | Deadline\n");
    printf("--------------------------------------------\n");
    for (int i = 0; i < num_processes; i++) {
        printf("P%-2d| %-7d | %-5d | %-10d | %-8d\n",
               process_list[i].id,
               process_list[i].arrival_time,
               process_list[i].burst_time,
               process_list[i].priority,
               process_list[i].deadline);
        process_list[i].remaining_time = process_list[i].burst_time;
    }
     printf("--------------------------------------------\n");

    // --- Seleção e Execução do Algoritmo ---
    if (strcmp(algorithm, "fcfs") == 0) {
        schedule_fcfs(process_list, num_processes);
    } else if (strcmp(algorithm, "sjf") == 0) {
        schedule_sjf(process_list, num_processes);
    } else if (strcmp(algorithm, "rr") == 0) {
        schedule_rr(process_list, num_processes, quantum);
    } else if (strcmp(algorithm, "prio-np") == 0) {
        schedule_priority(process_list, num_processes, 0); // 0 = Non-Preemptive
    } else if (strcmp(algorithm, "prio-p") == 0) {
        schedule_priority(process_list, num_processes, 1); // 1 = Preemptive
    } else if (strcmp(algorithm, "edf") == 0) {
        schedule_edf_preemptive(process_list, num_processes);
    } else if (strcmp(algorithm, "rm") == 0) {
        schedule_rm_preemptive(process_list, num_processes);
    } else {
        fprintf(stderr, "Erro: Algoritmo '%s' desconhecido ou não implementado.\n", algorithm);
        free(process_list);
        return 1;
    }

    free(process_list);
    printf("\n--- Simulação Concluída ---\n");

    return 0;
}