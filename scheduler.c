// scheduler.c
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h> // Para INT_MAX
#include <math.h>   // Para M_PI (se necessário)
#include <string.h> // Para memset (opcional)

// ------------------------- Utilitários -------------------------

int compare_arrival(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->arrival_time - p2->arrival_time;
}

int compare_priority(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->priority == p2->priority) {
        return p1->arrival_time - p2->arrival_time;
    }
    return p1->priority - p2->priority;
}

int compare_deadline(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->deadline == p2->deadline) {
        return p1->arrival_time - p2->arrival_time;
    }
    return p1->deadline - p2->deadline;
}

int find_min_arrival_time(Process *list, int count) {
    if (count <= 0) return 0;
    int min_arrival = INT_MAX;
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].arrival_time >= 0 && list[i].arrival_time < min_arrival) {
            min_arrival = list[i].arrival_time;
            found = 1;
        }
    }
    return found ? min_arrival : 0;
}

// --- Funções de Seleção para EDF/RM Preemptivo ---
int find_earliest_deadline_process(Process *processes, int count, int *finished_flags, int current_time) {
    int best_idx = -1;
    int min_deadline = INT_MAX;
    for (int i = 0; i < count; i++) {
        if (!finished_flags[i] && processes[i].arrival_time <= current_time && processes[i].remaining_time > 0) {
            if (processes[i].deadline < min_deadline) {
                min_deadline = processes[i].deadline;
                best_idx = i;
            } else if (processes[i].deadline == min_deadline) {
                if (best_idx == -1 || processes[i].arrival_time < processes[best_idx].arrival_time) {
                    best_idx = i;
                }
            }
        }
    }
    return best_idx;
}

int find_highest_priority_process(Process *processes, int count, int *finished_flags, int current_time) {
    int best_idx = -1;
    int min_priority = INT_MAX;
    for (int i = 0; i < count; i++) {
        if (!finished_flags[i] && processes[i].arrival_time <= current_time && processes[i].remaining_time > 0) {
             if (processes[i].priority < min_priority) {
                min_priority = processes[i].priority;
                best_idx = i;
            } else if (processes[i].priority == min_priority) {
                if (best_idx == -1 || processes[i].arrival_time < processes[best_idx].arrival_time) {
                    best_idx = i;
                }
            }
        }
    }
    return best_idx;
}


// ---------------------- FCFS (com Limite de Tempo e Métricas) ----------------------
void schedule_fcfs(Process *list, int count, int max_simulation_time) {
    printf("\n--- FCFS (First-Come, First-Served) ---\n");
    if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);

    if (count <= 0) {
         printf("Nenhum processo para escalonar.\n");
         return;
    }

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { fprintf(stderr, "Falha ao alocar memória em FCFS\n"); return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
    }

    qsort(local_list, count, sizeof(Process), compare_arrival);

    int current_time = 0;
    int completed_count = 0;
    int total_waiting = 0, total_turnaround = 0;
    int total_burst_original = 0; // Soma dos bursts originais
    int deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_event_time = 0; // Tempo do último evento (finish ou interrupção)
    int first_event_time = find_min_arrival_time(local_list, count);
     if (first_event_time < 0) first_event_time = 0;


    current_time = first_event_time;
     // Verifica se o início já ultrapassa o limite
     if (max_simulation_time != -1 && current_time >= max_simulation_time) {
         printf("Tempo inicial %d >= Tempo Máximo %d. Nenhuma execução.\n", current_time, max_simulation_time);
         free(local_list);
         return;
     }
    if (current_time > 0) {
        cpu_idle_time = current_time;
    }
    last_event_time = current_time;


    printf("ID | Chegada | Burst | Pri | Dead | Start | Finish | Wait | Turnar | D.Met?| Status\n");
    printf("---------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < count && (max_simulation_time == -1 || current_time < max_simulation_time) ; i++) {
        Process *p = &local_list[i];
        total_burst_original += p->burst_time;
        const char* status = "Pendente";


        if (max_simulation_time != -1 && p->arrival_time >= max_simulation_time) {
             status = "Ignorado (T Chegada >= T Max)";
              printf("P%-2d| %-7d | %-5d | %-3d | %-4d | ----- | ------ | ---- | ------ | ----- | %s\n",
                   p->id, p->arrival_time, p->burst_time, p->priority, p->deadline, status);
             continue;
        }

        if (current_time < p->arrival_time) {
            int next_event_time = p->arrival_time;
            if (max_simulation_time != -1 && next_event_time >= max_simulation_time) {
                 cpu_idle_time += (max_simulation_time - current_time);
                 current_time = max_simulation_time;
                 last_event_time = current_time;
                 printf("Tempo limite %d atingido durante espera por P%d\n", max_simulation_time, p->id);
                 break;
            } else {
                 cpu_idle_time += (next_event_time - current_time);
                 current_time = next_event_time;
            }
        }

        int start = current_time;
        int finish = start + p->burst_time;
        int time_executed = 0;


        if (max_simulation_time != -1 && finish > max_simulation_time) {
            time_executed = max_simulation_time - start;
            if (time_executed < 0) time_executed = 0; // Segurança
            finish = max_simulation_time;
            p->remaining_time = p->burst_time - time_executed;
            status = "Interrompido";
            current_time = finish;
            last_event_time = current_time;
             printf("P%-2d| %-7d | %-5d | %-3d | %-4d | %-5d | %-6d | ---- | ------ | ----- | %s (R:%d)\n",
                   p->id, p->arrival_time, p->burst_time, p->priority, p->deadline, start, finish, status, p->remaining_time);
             break; // Sai do loop for, tempo acabou
        } else {
             time_executed = p->burst_time;
             p->remaining_time = 0;
             status = "Completo";
             completed_count++;

             int turnaround = finish - p->arrival_time;
             int waiting = start - p->arrival_time;
             if (waiting < 0) waiting = 0;
             if (turnaround < 0) turnaround = 0;

             total_turnaround += turnaround;
             total_waiting += waiting;
             current_time = finish;
             last_event_time = finish;

             int missed = 0;
             if (p->deadline > 0 && finish > p->deadline) {
                 deadline_misses++;
                 missed = 1;
             }
              printf("P%-2d| %-7d | %-5d | %-3d | %-4d | %-5d | %-6d | %-4d | %-6d | %-5s | %s\n",
                   p->id, p->arrival_time, p->burst_time, p->priority, p->deadline,
                   start, finish, waiting, turnaround, missed ? "NÃO" : "Sim", status);
        }
    }
    printf("---------------------------------------------------------------------------------------------\n");


    // --- Métricas Finais FCFS ---
    int actual_stop_time = last_event_time;
    float simulation_duration = (float)(actual_stop_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)actual_stop_time > 0 ? actual_stop_time : 1.0f;

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    // Tempo ocupado é o tempo total menos o idle.
    float cpu_busy_time = (float)(actual_stop_time - cpu_idle_time);
    if (cpu_busy_time < 0) cpu_busy_time = 0; // Segurança
    float cpu_utilization = (actual_stop_time > 0) ? (cpu_busy_time / actual_stop_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (actual_stop_time > 0) ? (float)completed_count / actual_stop_time : 0;

    printf("\n--- Métricas FCFS ---\n");
    printf("Processos Completos:         %d de %d\n", completed_count, count);
    printf("Tempo Final da Simulação:    %d\n", actual_stop_time);
    printf("Tempo Ocioso da CPU:         %d\n", cpu_idle_time);
    printf("Tempo Ocupado da CPU (estim):%.0f\n", cpu_busy_time);
    printf("--------------------------------------------------\n");
    printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
    printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
    printf("Utilização da CPU:                 %.2f %%\n", cpu_utilization);
    printf("Throughput (completos/tempo total):%.4f\n", throughput_total_time);
    printf("Deadlines Perdidos (completos):    %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(local_list);
}

// ---------------------- Round Robin (com Limite de Tempo) ----------------------
void schedule_rr(Process *list, int count, int quantum, int max_simulation_time) {
    printf("\n--- Round Robin (q = %d) ---\n", quantum);
    if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
     if (count <= 0 || quantum <=0) { /* ... (mensagem erro) */ return; }

    int current_time = 0, completed_count = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst_original = 0, deadline_misses = 0;
    int last_event_time = 0, cpu_idle_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
     if (first_event_time < 0) first_event_time = 0;

    int *remaining = malloc(count * sizeof(int));
    int *arrival = malloc(count * sizeof(int));
    int *finish_time = malloc(count * sizeof(int)); // Guarda tempo final se completou
    // Não precisamos de start_time ou has_started se calculamos espera no final
    if (!remaining || !arrival || !finish_time) { /* ... (erro malloc) */ /* ... (free) */ return; }

    for (int i = 0; i < count; i++) {
        remaining[i] = list[i].burst_time;
        arrival[i] = list[i].arrival_time;
        total_burst_original += list[i].burst_time;
        finish_time[i] = -1; // -1 indica não terminado
    }

    current_time = first_event_time;
    if (max_simulation_time != -1 && current_time >= max_simulation_time) { /* ... (erro tempo inicial)*/ /* free */ return; }
    if (current_time > 0) cpu_idle_time = current_time;
    last_event_time = current_time;

    printf("Tempo | Evento\n");
    printf("--------------------------------\n");

    // Loop principal com verificação de tempo
    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        int executed_in_cycle = 0;
        int process_interrupted = 0;

        for (int i = 0; i < count; i++) {
            // Se o tempo acabou dentro do loop for, sai dele
             if (max_simulation_time != -1 && current_time >= max_simulation_time) {
                 process_interrupted = 1; // Indica que parou por tempo
                 break;
             }

            if (remaining[i] > 0 && arrival[i] <= current_time) {
                int exec_time = (remaining[i] > quantum) ? quantum : remaining[i];
                int time_until_limit = (max_simulation_time != -1) ? max_simulation_time - current_time : exec_time + 1; // Tempo disponível

                // Verifica se a execução (mesmo parcial) ultrapassa o limite
                if (time_until_limit <= 0) {
                     process_interrupted = 1; break; // Já estamos no limite
                }

                // Ajusta o tempo de execução se necessário
                if (exec_time > time_until_limit) {
                    exec_time = time_until_limit;
                    process_interrupted = 1; // Será interrompido no final deste quantum
                    printf("%-5d | P%d executa por %d unidades (INTERROMPIDO T Limite)\n", current_time, list[i].id, exec_time);
                } else {
                    printf("%-5d | P%d executa por %d unidades (Resta: %d)\n", current_time, list[i].id, exec_time, remaining[i] - exec_time);
                }

                remaining[i] -= exec_time;
                current_time += exec_time;
                last_event_time = current_time;
                executed_in_cycle = 1;


                if (remaining[i] == 0) {
                    completed_count++;
                    finish_time[i] = current_time; // Guarda tempo de conclusão
                    int turnaround = finish_time[i] - list[i].arrival_time;
                    int waiting = turnaround - list[i].burst_time;
                    if (waiting < 0) waiting = 0;
                    total_turnaround += turnaround;
                    total_waiting += waiting;

                    printf("%-5d | P%d TERMINOU. Espera=%d, Turnaround=%d\n", current_time, list[i].id, waiting, turnaround);

                    if (list[i].deadline > 0 && finish_time[i] > list[i].deadline) {
                        deadline_misses++;
                        printf("%-5d | -> Deadline P%d perdido\n", current_time, list[i].id);
                    }
                }
                // Se foi interrompido pelo tempo limite, sai do loop for
                if (process_interrupted) break;
            }
        } // Fim do loop for(i...)

        // Se o tempo acabou, sai do loop while
         if (process_interrupted) break;

        // Se nenhum processo executou neste ciclo (e tempo não acabou)
        if (!executed_in_cycle && completed_count < count) {
            int next_event_time = INT_MAX;
             for(int i=0; i<count; i++) {
                 // Procura próxima chegada *depois* do tempo atual
                 if(remaining[i] > 0 && arrival[i] > current_time && arrival[i] < next_event_time) {
                    next_event_time = arrival[i];
                 }
             }

             if (next_event_time == INT_MAX) { // Não há mais chegadas e ninguém pronto
                printf("%-5d | CPU Ociosa (Fim da simulação?)\n", current_time);
                if(completed_count < count) printf("        AVISO: %d processos não completaram.\n", count-completed_count);
                break; // Sai do loop while
             } else {
                 // Verifica se avançar até a próxima chegada excede o limite
                 if (max_simulation_time != -1 && next_event_time >= max_simulation_time) {
                     cpu_idle_time += (max_simulation_time - current_time);
                     current_time = max_simulation_time;
                     last_event_time = current_time;
                      printf("%-5d | CPU Ociosa até T Limite %d\n", current_time - (max_simulation_time - current_time), max_simulation_time);
                     break; // Sai do loop while
                 } else {
                     printf("%-5d | CPU Ociosa até t=%d.\n", current_time, next_event_time);
                     cpu_idle_time += (next_event_time - current_time);
                     current_time = next_event_time;
                 }
             }
        }
    } // Fim do loop while
     printf("--------------------------------\n");

    // --- Métricas Finais RR ---
    int actual_stop_time = last_event_time;
    float simulation_duration = (float)(actual_stop_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)actual_stop_time > 0 ? actual_stop_time : 1.0f;

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    float cpu_busy_time = (float)(actual_stop_time - cpu_idle_time); // Estimativa
     if (cpu_busy_time < 0) cpu_busy_time = 0;
    float cpu_utilization = (actual_stop_time > 0) ? (cpu_busy_time / actual_stop_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (actual_stop_time > 0) ? (float)completed_count / actual_stop_time : 0;

    printf("\n--- Métricas Round Robin (q=%d) ---\n", quantum);
    printf("Processos Completos:         %d de %d\n", completed_count, count);
    printf("Tempo Final da Simulação:    %d\n", actual_stop_time);
    printf("Tempo Ocioso da CPU (estim): %d\n", cpu_idle_time);
    printf("Tempo Ocupado da CPU (estim):%.0f\n", cpu_busy_time);
    printf("--------------------------------------------------\n");
    printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
    printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
    printf("Utilização da CPU:                 %.2f %%\n", cpu_utilization);
    printf("Throughput (completos/tempo total):%.4f\n", throughput_total_time);
    printf("Deadlines Perdidos (completos):    %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(remaining);
    free(arrival);
    free(finish_time);
    // free(has_started); // Removido
}


// ------------------ Priority Scheduling (com Limite de Tempo) ------------------
void schedule_priority(Process *list, int count, int preemptive, int max_simulation_time) {
    printf("\n--- Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
     if (count <= 0) { /* ... */ return; }

    int current_time = 0, completed_count = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst_original = 0;
    int deadline_misses = 0, cpu_idle_time = 0, last_event_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
    if (first_event_time < 0) first_event_time = 0;

    int *remaining = malloc(count * sizeof(int));
    Process *local_list = malloc(count * sizeof(Process));
    int *finish_times = malloc(count * sizeof(int));
     if (!remaining || !local_list || !finish_times) { /* ... erro malloc, free ... */ return; }
    for (int i = 0; i < count; i++) {
        local_list[i] = list[i];
        remaining[i] = list[i].burst_time;
        local_list[i].remaining_time = list[i].burst_time;
        total_burst_original += list[i].burst_time;
        finish_times[i] = -1;
    }

    current_time = first_event_time;
    if (max_simulation_time != -1 && current_time >= max_simulation_time) { /* ... erro tempo inicial, free ...*/ return; }
    if (current_time > 0) cpu_idle_time = current_time;
    last_event_time = current_time;


    printf("Tempo | Evento\n");
    printf("--------------------------------\n");

    // Loop principal com verificação de tempo
    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        int idx = find_highest_priority_process(local_list, count, finish_times, current_time); // Usa finish_times como 'finished flag'
        int process_interrupted = 0;


        if (idx != -1) { // Processo encontrado
            int exec_time = 0;
             int time_until_limit = (max_simulation_time != -1) ? max_simulation_time - current_time : INT_MAX;
             if (time_until_limit <= 0) { process_interrupted = 1; break; } // Já no limite

            if (preemptive) {
                exec_time = 1; // Executa por 1 unidade
            } else {
                exec_time = remaining[idx]; // Tenta executar tudo
            }

             // Ajusta o tempo de execução se exceder o restante ou o limite de tempo
             if (exec_time > remaining[idx]) exec_time = remaining[idx];
             if (exec_time > time_until_limit) {
                 exec_time = time_until_limit;
                 process_interrupted = 1;
                  printf("%-5d | P%d (Pri: %d) executa por %d unidades (INTERROMPIDO T Limite)\n",
                    current_time, local_list[idx].id, local_list[idx].priority, exec_time);
             } else {
                  printf("%-5d | P%d (Pri: %d) executa por %d unidades (Resta: %d)\n",
                    current_time, local_list[idx].id, local_list[idx].priority, exec_time, remaining[idx] - exec_time);
             }


            remaining[idx] -= exec_time;
            current_time += exec_time;
            last_event_time = current_time;

            if (remaining[idx] == 0) {
                completed_count++;
                finish_times[idx] = current_time; // Marca como terminado
                int turnaround = finish_times[idx] - local_list[idx].arrival_time;
                int waiting = turnaround - local_list[idx].burst_time;
                 if (waiting < 0) waiting = 0;
                total_turnaround += turnaround;
                total_waiting += waiting;

                printf("%-5d | P%d TERMINOU. Espera=%d, Turnaround=%d\n", current_time, local_list[idx].id, waiting, turnaround);
                 if (local_list[idx].deadline > 0 && finish_times[idx] > local_list[idx].deadline) {
                    deadline_misses++;
                    printf("%-5d | -> Deadline P%d perdido\n", current_time, local_list[idx].id);
                 }
            }
             // Se foi interrompido pelo tempo, sai do loop while
             if (process_interrupted) break;

        } else { // Nenhum processo pronto
            // Lógica para avançar tempo ocioso (igual a RR)
            int next_arrival = INT_MAX;
             for(int i=0; i<count; i++) {
                 if(remaining[i] > 0 && local_list[i].arrival_time > current_time) {
                     if (local_list[i].arrival_time < next_arrival) {
                         next_arrival = local_list[i].arrival_time;
                     }
                 }
             }
             if (next_arrival == INT_MAX) {
                 printf("%-5d | CPU Ociosa (Fim da simulação?)\n", current_time);
                  if (completed_count < count) printf("        AVISO: %d processos não completaram.\n", count - completed_count);
                 break;
             } else {
                  if (max_simulation_time != -1 && next_arrival >= max_simulation_time) {
                     cpu_idle_time += (max_simulation_time - current_time);
                     current_time = max_simulation_time;
                     last_event_time = current_time;
                      printf("%-5d | CPU Ociosa até T Limite %d\n", current_time - (max_simulation_time - current_time), max_simulation_time);
                     break;
                 } else {
                      printf("%-5d | CPU Ociosa até t=%d.\n", current_time, next_arrival);
                      cpu_idle_time += (next_arrival - current_time);
                     current_time = next_arrival;
                 }
             }
        }
    } // Fim do while
     printf("--------------------------------\n");

     // --- Métricas Finais Priority ---
     // (Lógica idêntica a FCFS/RR para calcular métricas com completed_count e actual_stop_time)
    int actual_stop_time = last_event_time;
    float simulation_duration = (float)(actual_stop_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)actual_stop_time > 0 ? actual_stop_time : 1.0f;

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    float cpu_busy_time = (float)(actual_stop_time - cpu_idle_time); // Estimativa
     if (cpu_busy_time < 0) cpu_busy_time = 0;
    float cpu_utilization = (actual_stop_time > 0) ? (cpu_busy_time / actual_stop_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (actual_stop_time > 0) ? (float)completed_count / actual_stop_time : 0;

     printf("\n--- Métricas Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
     printf("Processos Completos:         %d de %d\n", completed_count, count);
     printf("Tempo Final da Simulação:    %d\n", actual_stop_time);
     printf("Tempo Ocioso da CPU (estim): %d\n", cpu_idle_time);
     printf("Tempo Ocupado da CPU (estim):%.0f\n", cpu_busy_time);
     printf("--------------------------------------------------\n");
     printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
     printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
     printf("Utilização da CPU:                 %.2f %%\n", cpu_utilization);
     printf("Throughput (completos/tempo total):%.4f\n", throughput_total_time);
     printf("Deadlines Perdidos (completos):    %d\n", deadline_misses);
     printf("--------------------------------------------------\n");

    free(remaining);
    free(local_list);
    free(finish_times);
}


// ---------------------- SJF (Non-Preemptive - com Limite de Tempo) ----------------------
void schedule_sjf(Process *list, int count, int max_simulation_time) {
    printf("\n--- SJF (Shortest Job First - Non-Preemptive) ---\n");
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
    if (count <= 0) { /* ... */ return; }

    int current_time = 0;
    int completed_count = 0;
    int total_waiting = 0, total_turnaround = 0;
    int total_burst_original = 0;
    int deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_event_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
    if (first_event_time < 0) first_event_time = 0;

    int *finished = calloc(count, sizeof(int)); // Usa 0 ou 1 para marcar concluído
    Process *local_list = malloc(count * sizeof(Process));
     if (!finished || !local_list) { /* ... erro malloc, free ...*/ return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
        total_burst_original += list[i].burst_time;
    }

    current_time = first_event_time;
    if (max_simulation_time != -1 && current_time >= max_simulation_time) { /* ... erro tempo inicial, free ...*/ return; }
    if (current_time > 0) cpu_idle_time = current_time;
    last_event_time = current_time;


    printf("ID | Chegada | Burst | Pri | Dead | Start | Finish | Wait | Turnar | D.Met?| Status\n");
    printf("---------------------------------------------------------------------------------------------\n");

    // Loop principal com verificação de tempo
    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        int shortest_idx = -1;
        int min_burst = INT_MAX;

        // Encontrar o processo pronto com menor burst (SJF)
        for (int i = 0; i < count; i++) {
            if (!finished[i] && local_list[i].arrival_time <= current_time) {
                 // Usa burst_time original para decisão em SJF não-preemptivo
                 if (local_list[i].burst_time < min_burst) {
                    min_burst = local_list[i].burst_time;
                    shortest_idx = i;
                } else if (local_list[i].burst_time == min_burst) {
                    if (shortest_idx == -1 || local_list[i].arrival_time < local_list[shortest_idx].arrival_time) {
                         shortest_idx = i;
                    }
                }
            }
        }

        if (shortest_idx != -1) { // Processo encontrado
            Process *p = &local_list[shortest_idx];
            int start = current_time;
            int finish = start + p->burst_time; // Executa tudo em não-preemptivo
            const char* status = "Pendente";
            int time_executed = 0;

             // Verifica se a execução excede o limite
             if (max_simulation_time != -1 && finish > max_simulation_time) {
                 time_executed = max_simulation_time - start;
                 if (time_executed < 0) time_executed = 0;
                 finish = max_simulation_time;
                 p->remaining_time = p->burst_time - time_executed; // Atualiza restante
                 status = "Interrompido";
                 current_time = finish;
                 last_event_time = current_time;
                 printf("P%-2d| %-7d | %-5d | %-3d | %-4d | %-5d | %-6d | ---- | ------ | ----- | %s (R:%d)\n",
                        p->id, p->arrival_time, p->burst_time, p->priority, p->deadline, start, finish, status, p->remaining_time);
                 break; // Tempo acabou
             } else {
                 // Execução completa
                 time_executed = p->burst_time;
                 p->remaining_time = 0;
                 status = "Completo";
                 finished[shortest_idx] = 1; // Marca como concluído
                 completed_count++;

                 int turnaround = finish - p->arrival_time;
                 int waiting = start - p->arrival_time;
                 if (waiting < 0) waiting = 0;
                 if (turnaround < 0) turnaround = 0;

                 total_turnaround += turnaround;
                 total_waiting += waiting;
                 current_time = finish;
                 last_event_time = finish;

                 int missed = 0;
                 if (p->deadline > 0 && finish > p->deadline) {
                     deadline_misses++;
                     missed = 1;
                 }
                   printf("P%-2d| %-7d | %-5d | %-3d | %-4d | %-5d | %-6d | %-4d | %-6d | %-5s | %s\n",
                       p->id, p->arrival_time, p->burst_time, p->priority, p->deadline,
                       start, finish, waiting, turnaround, missed ? "NÃO" : "Sim", status);
             }
        } else { // Nenhum processo pronto
             // Lógica para avançar tempo ocioso (igual a FCFS/Priority)
             int next_arrival = INT_MAX;
             for(int i=0; i<count; i++) {
                 if (!finished[i] && local_list[i].arrival_time > current_time) {
                      if (local_list[i].arrival_time < next_arrival) {
                         next_arrival = local_list[i].arrival_time;
                      }
                 }
             }
             if (next_arrival == INT_MAX) {
                  printf("%-5d | CPU Ociosa (Fim da simulação?)\n", current_time);
                   if (completed_count < count) printf("        AVISO: %d processos não completaram.\n", count - completed_count);
                  break;
             } else {
                   if (max_simulation_time != -1 && next_arrival >= max_simulation_time) {
                      cpu_idle_time += (max_simulation_time - current_time);
                      current_time = max_simulation_time;
                      last_event_time = current_time;
                       printf("%-5d | CPU Ociosa até T Limite %d\n", current_time - (max_simulation_time - current_time), max_simulation_time);
                      break;
                  } else {
                       printf("%-5d | CPU Ociosa até t=%d.\n", current_time, next_arrival);
                       cpu_idle_time += (next_arrival - current_time);
                      current_time = next_arrival;
                  }
             }
        }
    } // Fim do while
     printf("---------------------------------------------------------------------------------------------\n");
    free(finished);

     // --- Métricas Finais SJF ---
     // (Lógica idêntica a FCFS para calcular métricas com completed_count e actual_stop_time)
    int actual_stop_time = last_event_time;
    float simulation_duration = (float)(actual_stop_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)actual_stop_time > 0 ? actual_stop_time : 1.0f;

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    float cpu_busy_time = (float)(actual_stop_time - cpu_idle_time);
     if (cpu_busy_time < 0) cpu_busy_time = 0;
    float cpu_utilization = (actual_stop_time > 0) ? (cpu_busy_time / actual_stop_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (actual_stop_time > 0) ? (float)completed_count / actual_stop_time : 0;

    printf("\n--- Métricas SJF (Non-Preemptive) ---\n");
    printf("Processos Completos:         %d de %d\n", completed_count, count);
    printf("Tempo Final da Simulação:    %d\n", actual_stop_time);
    printf("Tempo Ocioso da CPU:         %d\n", cpu_idle_time);
    printf("Tempo Ocupado da CPU (estim):%.0f\n", cpu_busy_time);
    printf("--------------------------------------------------\n");
    printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
    printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
    printf("Utilização da CPU:                 %.2f %%\n", cpu_utilization);
    printf("Throughput (completos/tempo total):%.4f\n", throughput_total_time);
    printf("Deadlines Perdidos (completos):    %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

     free(local_list);
}

// ---------------------- EDF (Preemptive - com Limite de Tempo) ----------------------
void schedule_edf_preemptive(Process *list, int count, int max_simulation_time) {
    printf("\n--- EDF (Earliest Deadline First - Preemptive) ---\n");
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
    if (count <= 0) { /* ... */ return; }

    int current_time = 0;
    int completed_count = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst_original = 0, deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_event_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
     if (first_event_time < 0) first_event_time = 0;

    Process *local_list = malloc(count * sizeof(Process));
    int *finished = calloc(count, sizeof(int)); // 0=não, 1=sim
    int *start_exec_time = malloc(count * sizeof(int)); // Não usado no cálculo final aqui
    if (!local_list || !finished || !start_exec_time) { /* ... erro malloc, free ...*/ return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
        total_burst_original += list[i].burst_time;
        start_exec_time[i] = -1; // Não estritamente necessário agora
    }

    current_time = first_event_time;
     if (max_simulation_time != -1 && current_time >= max_simulation_time) { /* ... erro tempo inicial, free ...*/ return; }
    if (current_time > 0) cpu_idle_time = current_time;
    last_event_time = current_time;

    printf("Tempo | CPU  | Evento\n");
    printf("------------------------------------------\n");

    // Loop principal com verificação de tempo
    while(completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        int running_idx = find_earliest_deadline_process(local_list, count, finished, current_time);

        if (running_idx != -1) { // Processo encontrado
            Process *p = &local_list[running_idx];

            printf("%-5d | P%-3d | Executa (Deadline: %d, Resta: %d)\n", current_time, p->id, p->deadline, p->remaining_time - 1);
            p->remaining_time--;
            current_time++; // Avança 1 unidade
            last_event_time = current_time;

            if (p->remaining_time == 0) {
                finished[running_idx] = 1; // Marca como concluído
                completed_count++;
                int finish_time = current_time;
                int turnaround = finish_time - p->arrival_time;
                int waiting = turnaround - p->burst_time; // Usa burst original
                 if (waiting < 0) waiting = 0;

                total_turnaround += turnaround;
                total_waiting += waiting;

                printf("%-5d | P%-3d | TERMINOU. Espera=%d, Turnaround=%d\n", current_time, p->id, waiting, turnaround);

                if (p->deadline > 0 && finish_time > p->deadline) {
                    deadline_misses++;
                     printf("%-5d | P%-3d | !!! DEADLINE PERDIDO\n", current_time, p->id);
                }
            }
             // Preempção é tratada na próxima iteração pelo find_earliest_deadline_process
        } else { // Nenhum processo pronto
            // Lógica para avançar tempo ocioso (igual a FCFS/Priority/SJF)
             int next_arrival = INT_MAX;
             for(int i=0; i<count; i++) {
                 if (!finished[i] && local_list[i].arrival_time > current_time) {
                     if (local_list[i].arrival_time < next_arrival) {
                         next_arrival = local_list[i].arrival_time;
                     }
                 }
             }

             if (next_arrival == INT_MAX) {
                 printf("%-5d | ---- | Ociosa (Fim da simulação?)\n", current_time);
                 if (completed_count < count) {
                      printf("        | AVISO: %d processos não completaram.\n", count - completed_count);
                 }
                 break;
             } else {
                 if (max_simulation_time != -1 && next_arrival >= max_simulation_time) {
                     cpu_idle_time += (max_simulation_time - current_time);
                     current_time = max_simulation_time;
                     last_event_time = current_time;
                      printf("%-5d | ---- | Ociosa até T Limite %d\n", current_time - (max_simulation_time - current_time), max_simulation_time);
                     break;
                 } else {
                     printf("%-5d | ---- | Ociosa até t=%d\n", current_time, next_arrival);
                     cpu_idle_time += (next_arrival - current_time);
                     current_time = next_arrival;
                 }
             }
        }
    } // Fim do while
     printf("------------------------------------------\n");

    // --- Métricas Finais EDF Preemptivo ---
     // (Lógica idêntica a FCFS para calcular métricas com completed_count e actual_stop_time)
    int actual_stop_time = last_event_time;
    float simulation_duration = (float)(actual_stop_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)actual_stop_time > 0 ? actual_stop_time : 1.0f;

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    float cpu_busy_time = (float)(actual_stop_time - cpu_idle_time);
     if (cpu_busy_time < 0) cpu_busy_time = 0;
    float cpu_utilization = (actual_stop_time > 0) ? (cpu_busy_time / actual_stop_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (actual_stop_time > 0) ? (float)completed_count / actual_stop_time : 0;


    printf("\n--- Métricas EDF (Preemptive) ---\n");
    printf("Processos Completos:         %d de %d\n", completed_count, count);
    printf("Tempo Final da Simulação:    %d\n", actual_stop_time);
    printf("Tempo Ocioso da CPU (estim): %d\n", cpu_idle_time);
    printf("Tempo Ocupado da CPU (estim):%.0f\n", cpu_busy_time);
    printf("--------------------------------------------------\n");
    printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
    printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
    printf("Utilização da CPU:                 %.2f %%\n", cpu_utilization);
    printf("Throughput (completos/tempo total):%.4f\n", throughput_total_time);
    printf("Deadlines Perdidos (completos):    %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(local_list);
    free(finished);
    free(start_exec_time);
}


// ---------------------- RM (Preemptive - com Limite de Tempo) ----------------------
void schedule_rm_preemptive(Process *list, int count, int max_simulation_time) {
     printf("\n--- RM (Rate Monotonic - Preemptive, baseado em Prioridade) ---\n");
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
     if (count <= 0) { /* ... */ return; }

    // --- A LÓGICA INTERNA É IDÊNTICA A EDF PREEMPTIVO, ---
    // --- EXCETO QUE USA find_highest_priority_process ---
    // --- Reutilização de código seria ideal, mas para clareza, duplicamos ---

    int current_time = 0;
    int completed_count = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst_original = 0, deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_event_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
     if (first_event_time < 0) first_event_time = 0;

    Process *local_list = malloc(count * sizeof(Process));
    int *finished = calloc(count, sizeof(int));
    if (!local_list || !finished) { /* ... erro malloc, free ...*/ return; }
     for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
        total_burst_original += list[i].burst_time;
    }

    current_time = first_event_time;
     if (max_simulation_time != -1 && current_time >= max_simulation_time) { /* ... erro, free ...*/ return; }
    if (current_time > 0) cpu_idle_time = current_time;
    last_event_time = current_time;

    printf("Tempo | CPU  | Evento\n");
    printf("------------------------------------------\n");

     while(completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        // A única mudança é aqui: usa find_highest_priority_process
        int running_idx = find_highest_priority_process(local_list, count, finished, current_time);

        if (running_idx != -1) {
             Process *p = &local_list[running_idx];

            printf("%-5d | P%-3d | Executa (Prioridade: %d, Resta: %d)\n", current_time, p->id, p->priority, p->remaining_time - 1);
            p->remaining_time--;
            current_time++;
            last_event_time = current_time;

             if (p->remaining_time == 0) {
                finished[running_idx] = 1;
                completed_count++;
                int finish_time = current_time;
                int turnaround = finish_time - p->arrival_time;
                int waiting = turnaround - p->burst_time;
                 if (waiting < 0) waiting = 0;
                total_turnaround += turnaround;
                total_waiting += waiting;
                printf("%-5d | P%-3d | TERMINOU. Espera=%d, Turnaround=%d\n", current_time, p->id, waiting, turnaround);
                 if (p->deadline > 0 && finish_time > p->deadline) {
                    deadline_misses++;
                     printf("%-5d | P%-3d | !!! DEADLINE PERDIDO\n", current_time, p->id);
                }
            }
        } else { // Nenhum processo pronto
             // Lógica para avançar tempo ocioso (igual a EDF)
             int next_arrival = INT_MAX;
             for(int i=0; i<count; i++) {
                 if (!finished[i] && local_list[i].arrival_time > current_time) {
                     if (local_list[i].arrival_time < next_arrival) {
                         next_arrival = local_list[i].arrival_time;
                     }
                 }
             }
             if (next_arrival == INT_MAX) {
                 printf("%-5d | ---- | Ociosa (Fim da simulação?)\n", current_time);
                  if (completed_count < count) printf("        | AVISO: %d processos não completaram.\n", count - completed_count);
                 break;
             } else {
                  if (max_simulation_time != -1 && next_arrival >= max_simulation_time) {
                     cpu_idle_time += (max_simulation_time - current_time);
                     current_time = max_simulation_time;
                     last_event_time = current_time;
                      printf("%-5d | ---- | Ociosa até T Limite %d\n", current_time - (max_simulation_time - current_time), max_simulation_time);
                     break;
                  } else {
                      printf("%-5d | ---- | Ociosa até t=%d\n", current_time, next_arrival);
                      cpu_idle_time += (next_arrival - current_time);
                      current_time = next_arrival;
                  }
             }
        }
    }
     printf("------------------------------------------\n");

     // --- Métricas Finais RM Preemptivo ---
     // (Lógica idêntica a EDF Preemptivo)
     int actual_stop_time = last_event_time;
     float simulation_duration = (float)(actual_stop_time - first_event_time);
     if (simulation_duration <= 0) simulation_duration = (float)actual_stop_time > 0 ? actual_stop_time : 1.0f;

     float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
     float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
     float cpu_busy_time = (float)(actual_stop_time - cpu_idle_time);
      if (cpu_busy_time < 0) cpu_busy_time = 0;
     float cpu_utilization = (actual_stop_time > 0) ? (cpu_busy_time / actual_stop_time) * 100.0f : 0;
     if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
     float throughput_total_time = (actual_stop_time > 0) ? (float)completed_count / actual_stop_time : 0;

    printf("\n--- Métricas RM (Preemptive - baseado em Prioridade) ---\n");
    printf("Processos Completos:         %d de %d\n", completed_count, count);
    printf("Tempo Final da Simulação:    %d\n", actual_stop_time);
    printf("Tempo Ocioso da CPU (estim): %d\n", cpu_idle_time);
    printf("Tempo Ocupado da CPU (estim):%.0f\n", cpu_busy_time);
    printf("--------------------------------------------------\n");
    printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
    printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
    printf("Utilização da CPU:                 %.2f %%\n", cpu_utilization);
    printf("Throughput (completos/tempo total):%.4f\n", throughput_total_time);
    printf("Deadlines Perdidos (completos):    %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(local_list);
    free(finished);
    // free(start_exec_time); // Não alocado aqui
}


// ------------------------ EDF (Simplista - com Limite de Tempo) ------------------------
// Assinatura atualizada
void schedule_edf(Process *list, int count, int max_simulation_time) {
    printf("\n--- EDF (Implementação Simplista: Ordena por Deadline + FCFS) ---\n");
     if (count <= 0) { /* ... */ return; }
    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { /* ... */ return; }
     for(int i=0; i<count; i++) { /* ... (copia) ... */ }

    qsort(local_list, count, sizeof(Process), compare_deadline);
    printf("Executando FCFS na lista ordenada por deadline...\n");
    // Chama FCFS passando o limite de tempo
    schedule_fcfs(local_list, count, max_simulation_time);

    free(local_list);
}

// ------------------ Rate Monotonic (Simplista - com Limite de Tempo) ------------------
// Assinatura atualizada
void schedule_rm(Process *list, int count, int max_simulation_time) {
    printf("\n--- Rate Monotonic (Implementação Simplista: Chama Priority Scheduling NP) ---\n");
     if (count <= 0) { /* ... */ return; }
    printf("Assumindo que 'priority' reflete a taxa.\n");
    printf("Executando Priority Scheduling Não-Preemptivo...\n");
    // Chama Priority NP passando o limite de tempo
    schedule_priority(list, count, 0, max_simulation_time);
}