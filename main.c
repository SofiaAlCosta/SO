// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "process.h"
#include "scheduler.h"

// --- Funções Auxiliares ---
void print_usage() {
    printf("Uso: ./probsched [opções]\n");
    printf("Opções:\n");
    printf("  -h, --help           Mostrar esta ajuda\n");
    printf("  -a <algoritmo>       Algoritmo (fcfs,sjf,rr,prio-np,prio-p,edf,rm,mlq) (padrão: fcfs)\n"); // Add mlq
    printf("                       (Nota: edf/rm/mlq usam implementações preemptivas)\n");
    printf("  -n <numero>          Número de processos a gerar (random/static) (padrão: 5)\n");
    printf("  -f <filename>        Ler processos estáticos de um ficheiro (ignora -n, --gen, dist params)\n");
    printf("  -t <max_time>        Tempo máximo de simulação (-1 para sem limite) (padrão: -1)\n");
    printf("  -q <quantum>         Time quantum base para Round Robin (padrão: 4)\n"); // E usado em MLQ
    printf("  -s <semente>         Semente para gerador aleatório (padrão: baseado no tempo)\n");
    printf("  --gen <modo>         Modo de geração se -f não for usado: 'static' ou 'random' (padrão: random)\n");
    printf("  --burst-dist <dist>  Distribuição para burst time: 'normal' ou 'exp' (padrão: normal)\n");
    printf("  --prio-gen <tipo>    Geração de prioridade: 'weighted' ou 'uniform' (padrão: weighted)\n");
    printf("  --lambda <valor>     Lambda para chegada Exp OR para burst Exp (padrão: 0.5 / 0.1)\n");
    printf("  --mean <valor>       Média para burst time Normal (padrão: 8.0)\n");
    printf("  --stddev <valor>     Desvio padrão para burst time Normal (padrão: 3.0)\n");
}

int main(int argc, char *argv[]) {

    // --- Valores Padrão ---
    char algorithm[20] = "fcfs";
    int num_processes = 5;
    int quantum = 4; // Quantum base para RR e MLQ Q0
    int seed = time(NULL);
    int use_fixed_seed = 0;
    char generation_mode[10] = "random";
    char input_filename[256] = "";
    int max_simulation_time = -1;
    // Geração aleatória
    char burst_dist_str[10] = "normal";
    int burst_dist_type = 0; // 0=Normal, 1=Exponential
    char prio_gen_str[10] = "weighted";
    int prio_type = 0; // 0=Weighted, 1=Uniform
    double lambda_arrival = 0.5;
    double lambda_burst = 0.1; // Usado se burst_dist == exp
    double mean_norm = 8.0;
    double stddev_norm = 3.0;


    // --- Parsing dos Argumentos ---
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { print_usage(); return 0; }
        else if (strcmp(argv[i], "-a") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-n") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-f") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-t") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-q") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-s") == 0) { /* ... */ }
        else if (strcmp(argv[i], "--gen") == 0) { /* ... */ }
        // Parsing novas flags
        else if (strcmp(argv[i], "--burst-dist") == 0) {
            if (i + 1 < argc) { strncpy(burst_dist_str, argv[++i], sizeof(burst_dist_str) - 1); burst_dist_str[sizeof(burst_dist_str) - 1] = '\0'; }
            else { fprintf(stderr, "Erro: Flag --burst-dist requer argumento.\n"); return 1; }
            if (strcmp(burst_dist_str, "exp") == 0) burst_dist_type = 1;
            else if (strcmp(burst_dist_str, "normal") != 0) { fprintf(stderr, "Erro: Distribuição de burst inválida ('normal' ou 'exp').\n"); return 1;}
        }
         else if (strcmp(argv[i], "--prio-gen") == 0) {
            if (i + 1 < argc) { strncpy(prio_gen_str, argv[++i], sizeof(prio_gen_str) - 1); prio_gen_str[sizeof(prio_gen_str) - 1] = '\0'; }
            else { fprintf(stderr, "Erro: Flag --prio-gen requer argumento.\n"); return 1; }
             if (strcmp(prio_gen_str, "uniform") == 0) prio_type = 1;
            else if (strcmp(prio_gen_str, "weighted") != 0) { fprintf(stderr, "Erro: Geração de prioridade inválida ('weighted' ou 'uniform').\n"); return 1;}
        }
        else if (strcmp(argv[i], "--lambda") == 0) { // Usado para arrival E burst exp
            if (i + 1 < argc) {
                double val = atof(argv[++i]); if (val <= 0) { fprintf(stderr, "Erro: Lambda deve ser > 0.\n"); return 1; }
                lambda_arrival = val;
                lambda_burst = val; // Usa o mesmo lambda por simplicidade, poderia ter flags separadas
            } else { fprintf(stderr, "Erro: Flag --lambda requer argumento.\n"); return 1; }
        }
        else if (strcmp(argv[i], "--mean") == 0) { /* ... */ }
        else if (strcmp(argv[i], "--stddev") == 0) { /* ... */ }
        else { /* ... erro opção desconhecida */ }
    }

    // --- Inicialização e Geração ---
    printf("--- Simulador ProbSched ---\n");
    printf("Config: Algo=%s, N=%d, Q=%d, Gen=%s, Seed=%s%d, TMax=%d\n",
           algorithm, num_processes, quantum,
           (strlen(input_filename)>0 ? "file" : generation_mode),
           use_fixed_seed ? "(fixa) " : "(tempo) ", seed, max_simulation_time);
    if (strlen(input_filename) == 0 && strcmp(generation_mode,"random")==0) {
         printf("        Dist: Burst=%s(M=%.1f,SD=%.1f / L=%.2f), Prio=%s, L_Arr=%.2f\n",
               burst_dist_str, mean_norm, stddev_norm, lambda_burst, prio_gen_str, lambda_arrival);
    }

    srand(seed);
    Process* process_list = NULL;
    int actual_process_count = 0;

    if (strlen(input_filename) > 0) { // Ler do ficheiro
        process_list = read_processes_from_file(input_filename, &actual_process_count);
        if (!process_list) return 1;
        num_processes = actual_process_count;
    } else { // Gerar processos
        actual_process_count = num_processes;
        if (strcmp(generation_mode, "static") == 0) {
             process_list = generate_static_processes(actual_process_count);
        } else { // random
             // Define parâmetros p1, p2 baseado na distribuição de burst escolhida
             double p1 = (burst_dist_type == 1) ? lambda_burst : mean_norm;
             double p2 = (burst_dist_type == 1) ? 0 : stddev_norm; // p2 não usado por exp burst
             process_list = generate_random_processes(actual_process_count, lambda_arrival, p1, p2, burst_dist_type, prio_type);
        }
         if (process_list == NULL) return 1;
    }

    printf("\n--- Lista de Processos (%d) ---\n", actual_process_count);
    // ... (imprime lista) ...

    // --- Seleção e Execução ---
    printf("\nA executar algoritmo: %s\n", algorithm);
    if (strcmp(algorithm, "fcfs") == 0) {
        schedule_fcfs(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "sjf") == 0) {
        schedule_sjf(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "rr") == 0) {
        schedule_rr(process_list, actual_process_count, quantum, max_simulation_time);
    } else if (strcmp(algorithm, "prio-np") == 0) {
        schedule_priority(process_list, actual_process_count, 0, max_simulation_time);
    } else if (strcmp(algorithm, "prio-p") == 0) {
        schedule_priority(process_list, actual_process_count, 1, max_simulation_time);
    } else if (strcmp(algorithm, "edf") == 0) {
        schedule_edf_preemptive(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "rm") == 0) {
        schedule_rm_preemptive(process_list, actual_process_count, max_simulation_time);
    } else if (strcmp(algorithm, "mlq") == 0) { // <-- Adicionado MLQ
         schedule_mlq(process_list, actual_process_count, max_simulation_time);
    } else {
        fprintf(stderr, "Erro: Algoritmo '%s' desconhecido.\n", algorithm);
        free(process_list);
        return 1;
    }

    // --- Limpeza ---
    free(process_list);
    printf("\n--- Simulação Concluída ---\n");
    return 0;
}