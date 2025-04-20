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
    printf("  -h, --help          Mostrar esta ajuda\n");
    printf("  -a <algoritmo>      Algoritmo (fcfs, sjf, rr, prio-np, prio-p, edf, rm) (padrão: fcfs)\n");
    printf("                      (Nota: edf/rm usam implementações preemptivas)\n");
    printf("  -n <numero>         Número de processos a gerar (random/static) (padrão: 5)\n");
    printf("  -f <filename>       Ler processos estáticos de um ficheiro (ignora -n e --gen)\n");
    printf("  -t <max_time>       Tempo máximo de simulação (-1 para sem limite) (padrão: -1)\n");
    printf("  -q <quantum>        Time quantum para Round Robin (padrão: 4)\n");
    printf("  -s <semente>        Semente para gerador aleatório (padrão: baseado no tempo)\n");
    printf("  --gen <modo>        Modo de geração se -f não for usado: 'static' ou 'random' (padrão: random)\n");
    printf("  --lambda <valor>    Parâmetro lambda para chegada Exponencial (padrão: 0.5)\n");
    printf("  --mean <valor>      Média para burst time Normal (padrão: 8.0)\n");
    printf("  --stddev <valor>    Desvio padrão para burst time Normal (padrão: 3.0)\n");
}

int main(int argc, char *argv[]) {

    // --- Valores Padrão ---
    char algorithm[20] = "fcfs";
    int num_processes = 5; // Padrão se não usar -f
    int quantum = 4;
    int seed = time(NULL);
    int use_fixed_seed = 0;
    char generation_mode[10] = "random";
    double lambda_exp = 0.5;
    double mean_norm = 8.0;
    double stddev_norm = 3.0;
    char input_filename[256] = ""; // String vazia indica não usar ficheiro
    int max_simulation_time = -1;  // Sem limite por padrão

    // --- Parsing dos Argumentos de Linha de Comando ---
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-a") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-n") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-f") == 0) { // Ler nome do ficheiro
             if (i + 1 < argc) { strncpy(input_filename, argv[++i], sizeof(input_filename) - 1); input_filename[sizeof(input_filename) - 1] = '\0'; }
             else { fprintf(stderr, "Erro: Flag -f requer argumento.\n"); return 1; }
        }
        else if (strcmp(argv[i], "-t") == 0) { // Ler tempo máximo
             if (i + 1 < argc) { max_simulation_time = atoi(argv[++i]); }
             else { fprintf(stderr, "Erro: Flag -t requer argumento.\n"); return 1; }
        }
        else if (strcmp(argv[i], "-q") == 0) { /* ... */ }
        else if (strcmp(argv[i], "-s") == 0) { /* ... */ }
        else if (strcmp(argv[i], "--gen") == 0) { /* ... */ }
        else if (strcmp(argv[i], "--lambda") == 0) { /* ... */ }
        else if (strcmp(argv[i], "--mean") == 0) { /* ... */ }
        else if (strcmp(argv[i], "--stddev") == 0) { /* ... */ }
        else { /* ... (erro opção desconhecida) */ }
    }

    // --- Inicialização e Geração ---
    printf("--- Simulador ProbSched ---\n");
    // ... (imprime configuração) ...
    if (max_simulation_time != -1) printf("              Tempo Máx Simulação: %d\n", max_simulation_time);

    srand(seed);

    Process* process_list = NULL;
    int actual_process_count = 0; // Usado se ler de ficheiro

    if (strlen(input_filename) > 0) { // Se um nome de ficheiro foi dado
        printf("A ler processos do ficheiro: %s\n", input_filename);
        process_list = read_processes_from_file(input_filename, &actual_process_count);
        if (!process_list) {
            fprintf(stderr, "Falha ao ler processos do ficheiro.\n");
            return 1;
        }
        num_processes = actual_process_count; // Atualiza o número de processos
    } else { // Gerar processos
        actual_process_count = num_processes; // Usa o valor de -n ou padrão
        if (strcmp(generation_mode, "static") == 0) {
             printf("A gerar %d processos estáticos...\n", actual_process_count);
             process_list = generate_static_processes(actual_process_count);
        } else { // random
             printf("A gerar %d processos aleatórios (L=%.2f, M=%.1f, SD=%.1f)...\n",
                    actual_process_count, lambda_exp, mean_norm, stddev_norm);
             process_list = generate_random_processes(actual_process_count, lambda_exp, mean_norm, stddev_norm);
        }
         if (process_list == NULL) {
             fprintf(stderr, "Erro ao gerar lista de processos.\n");
             return 1;
         }
    }


    printf("\n--- Lista de Processos (%d) ---\n", actual_process_count);
    // ... (imprime lista como antes) ...

    // --- Seleção e Execução do Algoritmo (Passando max_simulation_time) ---
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
    } else {
        // ... (erro algoritmo desconhecido) ...
    }

    // --- Limpeza ---
    free(process_list);
    printf("\n--- Simulação Concluída ---\n");

    return 0;
}