#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "process.h"
#include "scheduler.h"

void print_usage() {
    printf("Uso: ./probsched [opções]\n");
    printf("Opções:\n");
    printf("  -h, --help           Mostrar esta ajuda\n");
    printf("  -a <algoritmo>       Algoritmo (fcfs,sjf,rr,prio-np,prio-p,edf,rm,mlq) (padrão: fcfs)\n");
    printf("                       (edf/rm/mlq/prio-p são preemptivos)\n");
    printf("                       (prio-p inclui Aging por padrão)\n");
    printf("  -n <numero>          Número de processos a gerar (random/static) (padrão: 10)\n");
    printf("  -f <filename>        Ler processos de um ficheiro (ignora -n, --gen, dist params)\n");
    printf("                       Formato: ID Chegada Burst Prio Dead Period [IODuration]\n");
    printf("  -t <max_time>        Tempo máximo de simulação (-1 para sem limite) (padrão: 100)\n");
    printf("  -q <quantum>         Time quantum base para Round Robin e MLQ (padrão: 4)\n");
    printf("  -s <semente>         Semente para gerador aleatório (padrão: baseado no tempo)\n");
    printf("  --gen <modo>         Modo de geração se -f não for usado: 'static' ou 'random' (padrão: random)\n");
    printf("  --burst-dist <dist>  Distribuição para burst time: 'normal' ou 'exp' (padrão: normal)\n");
    printf("  --prio-gen <tipo>    Geração de prioridade: 'weighted' ou 'uniform' (padrão: weighted)\n");
    printf("  --lambda <valor>     Lambda para chegada Exp E para burst Exp (padrão: 0.2 / 0.1)\n");
    printf("  --mean <valor>       Média para burst time Normal (padrão: 10.0)\n");
    printf("  --stddev <valor>     Desvio padrão para burst time Normal (padrão: 3.0)\n");
    printf("  --io-chance <prob>   Probabilidade (0.0 a 1.0) de um processo ter I/O (padrão: 0.3)\n");
    printf("  --io-dur <min> <max> Duração min/max para I/O bursts (padrão: 3 8)\n");
}

void print_process_list(Process* list, int count) {
    if (!list || count <= 0) return;
    printf("\n--- Lista de Processos (%d) ---\n", count);
    printf("ID | Chegada | Burst | Prio | Dead | Period | IO Dur\n");
    printf("--------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("P%-2d| %-7d | %-5d | %-4d | %-4d | %-6d | %d\n",
               list[i].id, list[i].arrival_time, list[i].burst_time,
               list[i].priority, list[i].deadline, list[i].period, list[i].io_burst_duration);
    }
    printf("--------------------------------------------------\n");
}


int main(int argc, char *argv[]) {

    char algorithm[20] = "fcfs";
    int num_processes = 10;
    int quantum = 4;
    int seed = time(NULL);
    int use_fixed_seed = 0;
    char generation_mode[10] = "random";
    char input_filename[256] = "";
    int max_simulation_time = 100;
    char burst_dist_str[10] = "normal";
    int burst_dist_type = 0;
    char prio_gen_str[10] = "weighted";
    int prio_type = 0;
    double lambda_arrival = 0.2;
    double lambda_burst = 0.1;
    double mean_norm = 10.0;
    double stddev_norm = 3.0;
    double io_chance = 0.3;
    int min_io_duration = 3;
    int max_io_duration = 8;


    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { print_usage(); return 0; }
        else if (strcmp(argv[i], "-a") == 0) { if (++i < argc) strncpy(algorithm, argv[i], sizeof(algorithm)-1); else { fprintf(stderr, "Erro: Faltando argumento para -a\n"); return 1;} }
        else if (strcmp(argv[i], "-n") == 0) { if (++i < argc) num_processes = atoi(argv[i]); if(num_processes<=0) { fprintf(stderr,"Aviso: Numero de processos inválido '%s', usando N=1.\n", argv[i]); num_processes=1;} else {} }
        else if (strcmp(argv[i], "-f") == 0) { if (++i < argc) strncpy(input_filename, argv[i], sizeof(input_filename)-1); else { fprintf(stderr, "Erro: Faltando argumento para -f\n"); return 1;} }
        else if (strcmp(argv[i], "-t") == 0) { if (++i < argc) max_simulation_time = atoi(argv[i]); else { fprintf(stderr, "Erro: Faltando argumento para -t\n"); return 1;} }
        else if (strcmp(argv[i], "-q") == 0) {
            if (++i < argc) {
                quantum = atoi(argv[i]);
                if (quantum <= 0) {
                    fprintf(stderr, "Aviso: Quantum inválido '%s', usando quantum=1.\n", argv[i]);
                    quantum = 1;
                }
            } else {
                fprintf(stderr, "Erro: Faltando argumento para -q\n");
                return 1;
            }
        } 
        else if (strcmp(argv[i], "-s") == 0) { if (++i < argc) { seed = atoi(argv[i]); use_fixed_seed = 1; } else { fprintf(stderr, "Erro: Faltando argumento para -s\n"); return 1;} }
        else if (strcmp(argv[i], "--gen") == 0) { if (++i < argc) strncpy(generation_mode, argv[i], sizeof(generation_mode)-1); else { fprintf(stderr, "Erro: Faltando argumento para --gen\n"); return 1;} }
        else if (strcmp(argv[i], "--burst-dist") == 0) {
             if (++i < argc) { strncpy(burst_dist_str, argv[i], sizeof(burst_dist_str)-1);
                 if(strcmp(burst_dist_str, "exp")==0) burst_dist_type = 1; else burst_dist_type = 0;
             } else { fprintf(stderr, "Erro: Faltando argumento para --burst-dist\n"); return 1;}
        }
        else if (strcmp(argv[i], "--prio-gen") == 0) {
            if (++i < argc) { strncpy(prio_gen_str, argv[i], sizeof(prio_gen_str)-1);
                 if(strcmp(prio_gen_str, "uniform")==0) prio_type = 1; else prio_type = 0;
            } else { fprintf(stderr, "Erro: Faltando argumento para --prio-gen\n"); return 1;}
        }
        else if (strcmp(argv[i], "--lambda") == 0) { if (++i < argc) { lambda_arrival = atof(argv[i]); lambda_burst = atof(argv[i]);} else { fprintf(stderr, "Erro: Faltando argumento para --lambda\n"); return 1;} }
        else if (strcmp(argv[i], "--mean") == 0) { if (++i < argc) mean_norm = atof(argv[i]); else { fprintf(stderr, "Erro: Faltando argumento para --mean\n"); return 1;} }
        else if (strcmp(argv[i], "--stddev") == 0) { if (++i < argc) stddev_norm = atof(argv[i]); if(stddev_norm<0) stddev_norm=0; else { fprintf(stderr, "Erro: Faltando argumento para --stddev\n"); return 1;} }
        else if (strcmp(argv[i], "--io-chance") == 0) {
             if (++i < argc) { io_chance = atof(argv[i]); if(io_chance<0.0 || io_chance>1.0) io_chance=0.0; } else { fprintf(stderr, "Erro: Faltando argumento para --io-chance\n"); return 1; }
        } else if (strcmp(argv[i], "--io-dur") == 0) {
             if (i + 2 < argc) {
                 min_io_duration = atoi(argv[++i]);
                 max_io_duration = atoi(argv[++i]);
                 if (min_io_duration < 0) min_io_duration = 0;
                 if (max_io_duration < min_io_duration) max_io_duration = min_io_duration;
             } else { fprintf(stderr, "Erro: Flag --io-dur requer min e max.\n"); return 1; }
        }
        else { fprintf(stderr, "Erro: Opção desconhecida '%s'\n", argv[i]); print_usage(); return 1; }
    }


    printf("--- Simulador ProbSched ---\n");
    printf("Config: Algo=%s, N=%d, Q=%d, TMax=%d, Seed=%s%d\n",
           algorithm, num_processes, quantum, max_simulation_time,
           use_fixed_seed ? "(fixa) " : "(tempo) ", seed);
     printf("        Gen=%s", (strlen(input_filename)>0 ? "file" : generation_mode));
     if (strlen(input_filename) == 0 && strcmp(generation_mode,"random")==0) {
          printf(", Burst=%s(M=%.1f,SD=%.1f / L=%.2f), Prio=%s, L_Arr=%.2f\n",
                burst_dist_str, mean_norm, stddev_norm, lambda_burst, prio_gen_str, lambda_arrival);
          printf("        IO: Chance=%.2f, Dur=%d-%d\n", io_chance, min_io_duration, max_io_duration);
     } else if (strlen(input_filename) > 0) {
         printf(" ('%s')\n", input_filename);
     } else {
         printf("\n");
     }


    srand(seed);
    Process* process_list = NULL;
    int actual_process_count = 0;

    if (strlen(input_filename) > 0) {
        process_list = read_processes_from_file(input_filename, &actual_process_count);
        if (!process_list) return 1;
        num_processes = actual_process_count;
    } else {
        actual_process_count = num_processes;
        if (strcmp(generation_mode, "static") == 0) {
             process_list = generate_static_processes(actual_process_count);
        } else {
             double p1_burst = (burst_dist_type == 1) ? lambda_burst : mean_norm;
             double p2_burst = (burst_dist_type == 1) ? 0.0 : stddev_norm;
             process_list = generate_random_processes(actual_process_count, lambda_arrival, p1_burst, p2_burst, burst_dist_type, prio_type, io_chance, min_io_duration, max_io_duration);
        }
         if (!process_list) {
             fprintf(stderr, "Erro: Falha ao gerar lista de processos.\n");
             return 1;
         }
    }

    if (actual_process_count == 0) {
        fprintf(stderr, "Erro: Nenhum processo para simular.\n");
        return 1;
    }

    print_process_list(process_list, actual_process_count);


    printf("\nA executar algoritmo: %s\n", algorithm);
    if (strcmp(algorithm, "fcfs") == 0) {
        schedule_fcfs(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "sjf") == 0) {
        schedule_sjf(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "rr") == 0) {
        schedule_rr(process_list, actual_process_count, quantum, max_simulation_time);
    } else if (strcmp(algorithm, "prio-np") == 0) {
        schedule_priority(process_list, actual_process_count, 0, 0, max_simulation_time);
    } else if (strcmp(algorithm, "prio-p") == 0) {
        schedule_priority(process_list, actual_process_count, 1, 1, max_simulation_time);
    } else if (strcmp(algorithm, "edf") == 0) {
        schedule_edf_preemptive(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "rm") == 0) {
        schedule_rm_preemptive(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "mlq") == 0) {
         schedule_mlq(process_list, actual_process_count, quantum, max_simulation_time);
    } else {
        fprintf(stderr, "Erro: Algoritmo '%s' desconhecido.\n", algorithm);
        free(process_list);
        return 1;
    }


    free(process_list);
    printf("\n--- Simulação Concluída ---\n");
    return 0;
}