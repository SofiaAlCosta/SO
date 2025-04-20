// scheduler.c
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h> // Para INT_MAX
#include <string.h> // Para memset, memcpy

// --- Funções de Comparação (CORRIGIDAS) ---
int compare_arrival(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->arrival_time - p2->arrival_time;
}

int compare_priority(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->current_priority == p2->current_priority) {
        return p1->arrival_time - p2->arrival_time;
    }
    return p1->current_priority - p2->current_priority;
}

int compare_deadline(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->deadline == p2->deadline) {
        return p1->arrival_time - p2->arrival_time;
    }
    return p1->deadline - p2->deadline;
}



// --- Funções Auxiliares (sem alterações aqui) ---
int find_min_arrival_time(Process *list, int count) {
    int min_arrival = INT_MAX;
    int found = 0;
    for (int i = 0; i < count; i++) {
        // Considera apenas processos não terminados
        if (list[i].state != STATE_TERMINATED && list[i].arrival_time >= 0 && list[i].arrival_time < min_arrival) {
            min_arrival = list[i].arrival_time;
            found = 1;
        }
    }
    // Retorna 0 se nenhum processo encontrado ou se o primeiro chega em 0
    // Retorna o menor tempo de chegada positivo caso contrário
    return found ? min_arrival : 0;
}


int find_highest_priority_process_ready(Process *processes, int count, int current_time) {
    int best_idx = -1;
    int min_priority = INT_MAX; // Menor valor numérico é maior prioridade
    for (int i = 0; i < count; i++) {
        if (processes[i].state == STATE_READY && processes[i].arrival_time <= current_time) {
             if (processes[i].current_priority < min_priority) {
                min_priority = processes[i].current_priority;
                best_idx = i;
            } else if (processes[i].current_priority == min_priority) {
                if (best_idx == -1 || processes[i].arrival_time < processes[best_idx].arrival_time) {
                    best_idx = i;
                }
            }
        }
    }
    return best_idx; // Retorna -1 se nenhum processo pronto for encontrado
}

int find_earliest_deadline_process_ready(Process *processes, int count, int current_time) {
    int best_idx = -1;
    int min_deadline = INT_MAX;
    for (int i = 0; i < count; i++) {
         if (processes[i].state == STATE_READY && processes[i].arrival_time <= current_time) {
            // Considera apenas deadlines positivos, ou trata 0/negativo como não-realtime?
            // Assumindo que deadline é sempre relevante se o estado for READY.
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
    return best_idx; // Retorna -1 se nenhum pronto
}

int find_shortest_job_process_ready(Process *processes, int count, int current_time) {
    int best_idx = -1;
    int min_burst = INT_MAX;
    for (int i = 0; i < count; i++) {
        if (processes[i].state == STATE_READY && processes[i].arrival_time <= current_time) {
            // SJF usa o burst time ORIGINAL para decisão (em não-preemptivo)
            if (processes[i].burst_time < min_burst) {
                min_burst = processes[i].burst_time;
                best_idx = i;
            } else if (processes[i].burst_time == min_burst) {
                 if (best_idx == -1 || processes[i].arrival_time < processes[best_idx].arrival_time) {
                    best_idx = i;
                }
            }
        }
    }
    return best_idx; // Retorna -1 se nenhum pronto
}

void apply_aging(Process *list, int count, int current_time) {
    for (int i = 0; i < count; i++) {
        if (list[i].state == STATE_READY && list[i].current_priority > 1) { // Só aplica a prontos e não ao de prioridade máxima (1)
            list[i].time_in_ready_queue++;
            if (list[i].time_in_ready_queue >= AGING_THRESHOLD) {
                 printf("        Aging: P%d (Prio %d -> %d) at time %d\n",
                        list[i].id, list[i].current_priority, list[i].current_priority - 1, current_time);
                list[i].current_priority--;
                list[i].time_in_ready_queue = 0; // Reseta contador após aging
            }
        } else {
             // Reseta contador se não estiver pronto (ou já tem prio max)
             // Importante resetar para evitar aging logo ao voltar de I/O
             list[i].time_in_ready_queue = 0;
        }
    }
}


int check_io_completions(Process *list, int count, int current_time) {
    int moved_count = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].state == STATE_BLOCKED && current_time >= list[i].io_completion_time) {
            printf("        I/O Complete: P%d at time %d\n", list[i].id, current_time);
            list[i].state = STATE_READY;
            list[i].io_completion_time = -1;
            list[i].time_in_ready_queue = 0; // Reset aging counter upon becoming ready
            moved_count++;
            // Poderia reatribuir fila em MLQ aqui se necessário
        }
    }
    return moved_count; // Retorna quantos processos terminaram I/O
}


int check_new_arrivals(Process *list, int count, int current_time) {
     int arrived_count = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].state == STATE_NEW && list[i].arrival_time <= current_time) {
             printf("        Arrival: P%d at time %d\n", list[i].id, current_time);
            list[i].state = STATE_READY;
            list[i].time_in_ready_queue = 0; // Reset aging counter
            arrived_count++;
             // Poderia atribuir fila em MLQ aqui se necessário
        }
    }
    return arrived_count; // Retorna quantos processos chegaram
}

void calculate_final_metrics(Process *list, int count, int final_time, int total_idle_time, int total_context_switches) {
    long long total_waiting = 0;
    long long total_turnaround = 0;
    int completed_count = 0;
    int deadline_misses = 0;

    printf("\n--- Resultados Finais ---\n");
    printf("ID | Chegada | Burst | Prio | Dead | IO Dur | Start | Finish | Turnar | Wait | D.Met?| Estado Final\n");
    printf("---------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        const char* status_str;
        int missed = 0;
        char start_str[6], finish_str[7], wait_str[5], turn_str[7];

        if (list[i].state == STATE_TERMINATED) {
            status_str = "Completo";
            completed_count++;
            list[i].turnaround_time = list[i].finish_time - list[i].arrival_time;
            // Waiting time é o turnaround menos o tempo que realmente executou (burst original)
            // e menos o tempo que passou bloqueado em I/O (se aplicável e rastreado)
            // Simplificação: Usamos o turnaround - burst original. Uma contagem mais precisa
            // do tempo em READY seria melhor.
            list[i].waiting_time = list[i].turnaround_time - list[i].burst_time;
            if (list[i].waiting_time < 0) list[i].waiting_time = 0; // Não pode ser negativo

            total_turnaround += list[i].turnaround_time;
            total_waiting += list[i].waiting_time;

            if (list[i].deadline > 0 && list[i].finish_time > list[i].deadline) {
                deadline_misses++;
                missed = 1;
            }
            snprintf(start_str, 6, "%d", list[i].start_time);
            snprintf(finish_str, 7, "%d", list[i].finish_time);
            snprintf(wait_str, 5, "%d", list[i].waiting_time);
            snprintf(turn_str, 7, "%d", list[i].turnaround_time);

        } else {
             // Se não terminou, calcula métricas parciais se desejado, ou marca como incompleto
             if (list[i].state == STATE_RUNNING) status_str = "Executando";
             else if (list[i].state == STATE_READY) status_str = "Pronto";
             else if (list[i].state == STATE_BLOCKED) status_str = "Bloqueado";
             else status_str = "Pendente";

             if (list[i].start_time != -1) snprintf(start_str, 6, "%d", list[i].start_time); else strcpy(start_str, "-----");
             strcpy(finish_str, "------");
             strcpy(wait_str, "----");
             strcpy(turn_str, "------");
        }

        printf("P%-2d| %-7d | %-5d | %-4d | %-4d | %-6d | %-5s | %-6s | %-6s | %-4s | %-5s | %s (R:%d)\n",
               list[i].id, list[i].arrival_time, list[i].burst_time, list[i].priority, list[i].deadline,
               list[i].io_burst_duration,
               start_str, finish_str, turn_str, wait_str,
               (list[i].state == STATE_TERMINATED) ? (missed ? "NAO" : "Sim") : "-----",
               status_str, list[i].remaining_time);
    }
    printf("---------------------------------------------------------------------------------------------------\n");

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    float cpu_busy_time = (float)(final_time - total_idle_time);
    // A utilização pode incluir o tempo de context switch como 'busy'
    float cpu_utilization = (final_time > 0) ? (cpu_busy_time / final_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
     if (cpu_utilization < 0.0f) cpu_utilization = 0.0f; // Sanity check
    float throughput = (final_time > 0) ? (float)completed_count / final_time : 0;

    printf("\n--- Métricas Globais ---\n");
    printf("Tempo Final da Simulação:      %d\n", final_time);
    printf("Tempo Ocioso da CPU:           %d\n", total_idle_time);
    printf("Tempo Ocupado da CPU (estim):  %.0f\n", cpu_busy_time);
    printf("Número de Trocas de Contexto:  %d\n", total_context_switches);
    printf("Custo Total Trocas Contexto:   %d\n", total_context_switches * CONTEXT_SWITCH_COST);
    printf("--------------------------------------------------\n");
    printf("Processos Completos:           %d de %d\n", completed_count, count);
    printf("Média Tempo Espera (completos):    %.2f\n", avg_waiting);
    printf("Média Tempo Turnaround (completos):%.2f\n", avg_turnaround);
    printf("Utilização da CPU:             %.2f %%\n", cpu_utilization);
    printf("Throughput (completos/tempo):  %.4f processos/unidade de tempo\n", throughput);
    printf("Deadlines Perdidos (completos):%d\n", deadline_misses);
    printf("--------------------------------------------------\n");
}

// ---------------------- FCFS (First-Come, First-Served) ----------------------
void schedule_fcfs(Process *list, int count, int max_simulation_time) {
    printf("\n--- FCFS (First-Come, First-Served) ---\n");
    if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
    printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);

    if (count <= 0) { return; }

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { fprintf(stderr, "Erro malloc FCFS\n"); return; }
    for (int i = 0; i < count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
    }
    qsort(local_list, count, sizeof(Process), compare_arrival);

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) total_idle_time = current_time;

    // int process_idx = 0; // REMOVIDO - Não estava sendo usado

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {

        check_new_arrivals(local_list, count, current_time);
        check_io_completions(local_list, count, current_time);

        if (current_running_idx == -1) {
             int next_ready_idx = -1;
             for (int i = 0; i < count; i++){
                 if (local_list[i].state == STATE_READY) {
                     next_ready_idx = i;
                     break;
                 }
             }

            if (next_ready_idx != -1) {
                current_running_idx = next_ready_idx;
                Process *p = &local_list[current_running_idx];

                if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id) {
                     // CORRIGIDO Printf Context Switch
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);

                     current_time += CONTEXT_SWITCH_COST;
                     total_context_switches++;
                     check_new_arrivals(local_list, count, current_time);
                     check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                }
                //else if (CONTEXT_SWITCH_COST > 0 && last_process_id == -1) { ... } // Redundante com a lógica acima

                p->state = STATE_RUNNING;
                if (p->start_time == -1) p->start_time = current_time;
                last_process_id = p->id;
                printf("%-5d | P%d inicia execução (Burst Total: %d, Restante: %d)\n", current_time, p->id, p->burst_time, p->remaining_time);

                 int time_to_execute = p->remaining_time;
                 int time_limit_reached = 0;

                 if (max_simulation_time != -1 && current_time + time_to_execute > max_simulation_time) {
                     time_to_execute = max_simulation_time - current_time;
                     time_limit_reached = 1;
                     printf("        Execução de P%d limitada a %d unidades pelo T Max\n", p->id, time_to_execute);
                 }

                 if (time_to_execute <= 0 && time_limit_reached) {
                      p->state = STATE_READY;
                      current_running_idx = -1;
                      break;
                 }

                 // int execution_start_time = current_time; // REMOVIDO - Não usado
                 for (int t = 0; t < time_to_execute; ++t) {
                      current_time++;
                      p->remaining_time--;
                      check_new_arrivals(local_list, count, current_time);
                      check_io_completions(local_list, count, current_time);
                 }

                 if (p->remaining_time == 0) {
                     printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                     if (p->io_burst_duration > 0) {
                         printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                         p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                     } else {
                         p->state = STATE_TERMINATED;
                         p->finish_time = current_time;
                         completed_count++;
                     }
                     current_running_idx = -1;
                 } else if (time_limit_reached) {
                      printf("%-5d | Simulação INTERROMPIDA (T Max) enquanto P%d executava.\n", current_time, p->id);
                      p->state = STATE_READY;
                      current_running_idx = -1;
                      break;
                 } else {
                       fprintf(stderr, "Erro lógico em FCFS P%d\n", p->id);
                       p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++; current_running_idx = -1;
                 }

            } else { // Nenhum processo pronto, CPU Ociosa
                int next_event_time = INT_MAX;
                for (int i = 0; i < count; i++) {
                    if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                    if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
                }
                 if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                      int idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                       if (idle_until > current_time) {
                          printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
                          total_idle_time += (idle_until - current_time);
                          current_time = idle_until;
                       }
                      break;
                 } else {
                      if (next_event_time > current_time) {
                         printf("%-5d | CPU Ociosa até t=%d\n", current_time, next_event_time);
                         total_idle_time += (next_event_time - current_time);
                         current_time = next_event_time;
                      }
                 }
            }
        }
        // Se current_running_idx != -1, a lógica de execução/término já tratou.
    } // Fim do while

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}


// ---------------------- Round Robin (RR) ----------------------
void schedule_rr(Process *list, int count, int quantum, int max_simulation_time) {
     printf("\n--- Round Robin (q = %d) ---\n", quantum);
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
     printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);
     if (count <= 0 || quantum <=0) { return; }

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
    }

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;
    int last_ready_checked_idx = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) total_idle_time = current_time;

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {

        check_new_arrivals(local_list, count, current_time);
        check_io_completions(local_list, count, current_time);

        int process_executed_or_found = 0;

        if (current_running_idx != -1) {
            process_executed_or_found = 1;
        } else {
             int next_ready_idx = -1;
             int search_start_idx = (last_ready_checked_idx + 1); // Não precisa %count aqui
             for (int i = 0; i < count; ++i) {
                 int check_idx = (search_start_idx + i) % count; // Aplica o módulo aqui
                 if (local_list[check_idx].state == STATE_READY) {
                     next_ready_idx = check_idx;
                     last_ready_checked_idx = check_idx;
                     break;
                 }
             }

            if (next_ready_idx != -1) {
                current_running_idx = next_ready_idx;
                Process *p = &local_list[current_running_idx];
                process_executed_or_found = 1;

                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id) {
                     // CORRIGIDO Printf Context Switch
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);

                     current_time += CONTEXT_SWITCH_COST;
                     total_context_switches++;
                     check_new_arrivals(local_list, count, current_time);
                     check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                }

                p->state = STATE_RUNNING;
                p->time_slice_remaining = quantum;
                if (p->start_time == -1) p->start_time = current_time;
                last_process_id = p->id;
                printf("%-5d | P%d inicia/continua execução (Quantum: %d, Restante: %d)\n", current_time, p->id, quantum, p->remaining_time);
            }
        }

        if (current_running_idx != -1) {
            Process *p = &local_list[current_running_idx];

            if (max_simulation_time != -1 && current_time >= max_simulation_time) {
                p->state = STATE_READY;
                current_running_idx = -1;
                break;
            }

            current_time++;
            p->remaining_time--;
            p->time_slice_remaining--;
            printf("        P%d executa (R:%d, Q:%d)\n", p->id, p->remaining_time, p->time_slice_remaining);

            if (p->remaining_time == 0) {
                printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                 if (p->io_burst_duration > 0) {
                     printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                     p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                 } else {
                     p->state = STATE_TERMINATED;
                     p->finish_time = current_time;
                     completed_count++;
                 }
                current_running_idx = -1;
            } else if (p->time_slice_remaining == 0) {
                printf("%-5d | P%d fim do quantum, volta para READY\n", current_time, p->id);
                if (p->io_burst_duration > 0) {
                    printf("        P%d iniciando I/O (%d unidades)\n", p->id, p->io_burst_duration);
                    p->state = STATE_BLOCKED;
                    p->io_completion_time = current_time + p->io_burst_duration;
                } else {
                    p->state = STATE_READY;
                    p->time_in_ready_queue = 0;
                }
                current_running_idx = -1;
            }
        }

        if (!process_executed_or_found && current_running_idx == -1) {
             int next_event_time = INT_MAX;
             for (int i = 0; i < count; i++) {
                 if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                 if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
             }
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  int idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                   if (idle_until > current_time) {
                       printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
                       total_idle_time += (idle_until - current_time);
                       current_time = idle_until;
                   }
                  break;
              } else {
                   if (next_event_time > current_time) {
                       printf("%-5d | CPU Ociosa até t=%d\n", current_time, next_event_time);
                       total_idle_time += (next_event_time - current_time);
                       current_time = next_event_time;
                   }
              }
        }
    } // Fim do while

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}


// ------------------ Priority Scheduling ------------------
// MODIFICADO: Aceita enable_aging
void schedule_priority(Process *list, int count, int preemptive, int enable_aging, int max_simulation_time) {
    printf("\n--- Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
    if (enable_aging && preemptive) printf("    (Aging Habilitado: Threshold=%d, Interval=%d)\n", AGING_THRESHOLD, AGING_INTERVAL);
    else if (preemptive) printf("    (Aging Desabilitado)\n");
    if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
    printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);
    if (count <= 0) { return; }

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
        local_list[i].current_priority = list[i].priority; // Garante que começa com a prioridade original
    }

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;
    int last_aging_check = 0;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) total_idle_time = current_time;
    last_aging_check = current_time;

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {

        check_new_arrivals(local_list, count, current_time);
        check_io_completions(local_list, count, current_time);

        // Aplicar Aging periodicamente (apenas se preemptivo E habilitado)
        if (preemptive && enable_aging && (current_time - last_aging_check >= AGING_INTERVAL)) {
             apply_aging(local_list, count, current_time);
             last_aging_check = current_time;
        }

        int highest_prio_idx = find_highest_priority_process_ready(local_list, count, current_time);

        if (highest_prio_idx != -1) {
            Process *next_p = &local_list[highest_prio_idx];

            if (current_running_idx == -1) { // CPU Livre
                current_running_idx = highest_prio_idx;
                Process *p = next_p;
                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id) {
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);
                     current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                     check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                     // Re-checar aging após CS
                     if (preemptive && enable_aging && (current_time - last_aging_check >= AGING_INTERVAL)){ apply_aging(local_list, count, current_time); last_aging_check = current_time; }
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                     // Re-escolher após CS
                     highest_prio_idx = find_highest_priority_process_ready(local_list, count, current_time);
                     if (highest_prio_idx == -1) { current_running_idx = -1; goto check_idle_prio; }
                     current_running_idx = highest_prio_idx; p = &local_list[current_running_idx];
                 }
                p->state = STATE_RUNNING; p->time_in_ready_queue = 0;
                if (p->start_time == -1) p->start_time = current_time;
                last_process_id = p->id;
                printf("%-5d | P%d (Prio: %d) inicia/continua execução (R: %d)\n", current_time, p->id, p->current_priority, p->remaining_time);

            } else { // CPU Ocupada
                 Process *running_p = &local_list[current_running_idx];
                 // Preempção só ocorre se for preemptivo e houver um processo de MAIOR prioridade (menor número)
                 if (preemptive && next_p->current_priority < running_p->current_priority) {
                      printf("%-5d | PREEMPÇÃO: P%d (Prio: %d) preempta P%d (Prio: %d)\n",
                             current_time, next_p->id, next_p->current_priority, running_p->id, running_p->current_priority);
                       if (running_p->io_burst_duration > 0 && running_p->remaining_time > 0) {
                           printf("        P%d preemptido iniciando I/O (%d unidades)\n", running_p->id, running_p->io_burst_duration);
                           running_p->state = STATE_BLOCKED; running_p->io_completion_time = current_time + running_p->io_burst_duration;
                       } else { running_p->state = STATE_READY; running_p->time_in_ready_queue = 0; }

                      current_running_idx = highest_prio_idx; Process *p = next_p;
                      if (CONTEXT_SWITCH_COST > 0) {
                          printf("%-5d | Context Switch (P%d to P%d) - Custo: %d\n", current_time, running_p->id, p->id, CONTEXT_SWITCH_COST);
                          current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                          check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                          if (preemptive && enable_aging && (current_time - last_aging_check >= AGING_INTERVAL)){ apply_aging(local_list, count, current_time); last_aging_check = current_time; }
                          if (max_simulation_time != -1 && current_time >= max_simulation_time) { running_p->state = STATE_READY; current_running_idx = -1; break; }
                          // Re-escolher após CS
                          highest_prio_idx = find_highest_priority_process_ready(local_list, count, current_time);
                          if (highest_prio_idx == -1) { current_running_idx = -1; goto check_idle_prio; }
                          current_running_idx = highest_prio_idx; p = &local_list[current_running_idx];
                      }
                      p->state = STATE_RUNNING; p->time_in_ready_queue = 0;
                      if (p->start_time == -1) p->start_time = current_time;
                      last_process_id = p->id;
                      printf("%-5d | P%d (Prio: %d) inicia execução PREEMPTIVA (R: %d)\n", current_time, p->id, p->current_priority, p->remaining_time);
                 }
                 // Se não houve preempção, processo atual continua
            }
        } else { // Nenhum pronto
            current_running_idx = -1;
        }

        // Executa 1 unidade se CPU ocupada
        if (current_running_idx != -1) {
             Process *p = &local_list[current_running_idx];
             if (max_simulation_time != -1 && current_time >= max_simulation_time) { p->state = STATE_READY; current_running_idx = -1; break; }

             current_time++; p->remaining_time--;
             printf("        P%d executa (R:%d)\n", p->id, p->remaining_time);

             if (p->remaining_time == 0) { // Terminou
                 printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                 if (p->io_burst_duration > 0) {
                     printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                     p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                 } else {
                     p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                 }
                 current_running_idx = -1;
             } else if (preemptive) { // Só verifica I/O no meio se for preemptivo
                  // Chance de I/O (modelo simplificado)
                 if (p->io_burst_duration > 0 && p->burst_time > 0 && (rand() % p->burst_time < 1) ) { // Evita divisão por zero se burst_time=0
                       printf("%-5d | P%d iniciando I/O (%d unidades) durante execução\n", current_time, p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                       current_running_idx = -1;
                 }
                 // Se não fez I/O, continua
             }
             // Em NP, nunca faz I/O no meio nem é preemptido, então continua até fim
        } else { // CPU OCIOSA
        check_idle_prio:
             int next_event_time = INT_MAX;
              for (int i = 0; i < count; i++) {
                 if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                 if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
             }
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  int idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                  if (idle_until > current_time) { printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until); total_idle_time += (idle_until - current_time); current_time = idle_until; }
                  break;
              } else {
                   if (next_event_time > current_time) { printf("%-5d | CPU Ociosa até t=%d\n", current_time, next_event_time); total_idle_time += (next_event_time - current_time); current_time = next_event_time; }
              }
        }
    } // Fim do while

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}

// ---------------------- SJF (Non-Preemptive) ----------------------
void schedule_sjf(Process *list, int count, int max_simulation_time) {
    printf("\n--- SJF (Shortest Job First - Non-Preemptive) ---\n");
    if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
    printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);
    if (count <= 0) { return; }

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
    }

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) total_idle_time = current_time;

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

     while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
         check_new_arrivals(local_list, count, current_time);
         check_io_completions(local_list, count, current_time);

         if (current_running_idx == -1) {
             int shortest_idx = find_shortest_job_process_ready(local_list, count, current_time);

             if (shortest_idx != -1) {
                 current_running_idx = shortest_idx;
                 Process *p = &local_list[current_running_idx];

                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id) {
                      // CORRIGIDO Printf Context Switch
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);

                     current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                     check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                 }

                 p->state = STATE_RUNNING;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id;
                 printf("%-5d | P%d (Burst: %d) inicia execução (R: %d)\n", current_time, p->id, p->burst_time, p->remaining_time);

                 int time_to_execute = p->remaining_time;
                 int time_limit_reached = 0;

                 if (max_simulation_time != -1 && current_time + time_to_execute > max_simulation_time) {
                     time_to_execute = max_simulation_time - current_time; time_limit_reached = 1;
                     printf("        Execução de P%d limitada a %d unidades pelo T Max\n", p->id, time_to_execute);
                 }

                 if (time_to_execute <= 0 && time_limit_reached) { p->state = STATE_READY; current_running_idx = -1; break; }

                 // int execution_start_time = current_time; // REMOVIDO - Não usado
                 for (int t = 0; t < time_to_execute; ++t) {
                      current_time++; p->remaining_time--;
                      check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                 }

                 if (p->remaining_time == 0) {
                     printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                      if (p->io_burst_duration > 0) {
                           printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                           p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                      } else {
                           p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                      }
                     current_running_idx = -1;
                 } else if (time_limit_reached) {
                      printf("%-5d | Simulação INTERROMPIDA (T Max) enquanto P%d executava.\n", current_time, p->id);
                      p->state = STATE_READY; current_running_idx = -1; break;
                 } else {
                     fprintf(stderr, "Erro lógico em SJF P%d\n", p->id);
                     p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++; current_running_idx = -1;
                 }
             } else {
                 goto check_idle_sjf; // Nenhum pronto
             }
         }
         // Se CPU ocupada, não faz nada em NP

         if (current_running_idx == -1) {
         check_idle_sjf:
             int next_event_time = INT_MAX;
             for (int i = 0; i < count; i++) {
                 if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                 if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
             }
             if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                 int idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                 if (idle_until > current_time) { printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until); total_idle_time += (idle_until - current_time); current_time = idle_until; }
                 break;
             } else {
                 if (next_event_time > current_time) { printf("%-5d | CPU Ociosa até t=%d\n", current_time, next_event_time); total_idle_time += (next_event_time - current_time); current_time = next_event_time; }
             }
         }
     } // Fim do while

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}


// ---------------------- EDF (Preemptive) ----------------------
void schedule_edf_preemptive(Process *list, int count, int max_simulation_time) {
    printf("\n--- EDF (Earliest Deadline First - Preemptive) ---\n");
    if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
    printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);
    if (count <= 0) { return; }

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
    }

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) total_idle_time = current_time;

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

     while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        check_new_arrivals(local_list, count, current_time);
        check_io_completions(local_list, count, current_time);

        int earliest_deadline_idx = find_earliest_deadline_process_ready(local_list, count, current_time);

        if (earliest_deadline_idx != -1) {
            Process *next_p = &local_list[earliest_deadline_idx];

            if (current_running_idx == -1) {
                 current_running_idx = earliest_deadline_idx;
                 Process *p = next_p;
                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id) {
                      // CORRIGIDO Printf Context Switch
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);

                     current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                     check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                     earliest_deadline_idx = find_earliest_deadline_process_ready(local_list, count, current_time);
                     if (earliest_deadline_idx == -1) { current_running_idx = -1; goto check_idle_edf; }
                     current_running_idx = earliest_deadline_idx; p = &local_list[current_running_idx];
                 }
                 p->state = STATE_RUNNING;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id;
                 printf("%-5d | P%d (Deadl: %d) inicia/continua execução (R: %d)\n", current_time, p->id, p->deadline, p->remaining_time);

            } else {
                 Process *running_p = &local_list[current_running_idx];
                 if (next_p->deadline < running_p->deadline || (next_p->deadline == running_p->deadline && next_p->arrival_time < running_p->arrival_time)) {
                     printf("%-5d | PREEMPÇÃO EDF: P%d (D:%d) preempta P%d (D:%d)\n",
                             current_time, next_p->id, next_p->deadline, running_p->id, running_p->deadline);
                      if (running_p->io_burst_duration > 0 && running_p->remaining_time > 0) {
                          printf("        P%d preemptido iniciando I/O (%d unidades)\n", running_p->id, running_p->io_burst_duration);
                          running_p->state = STATE_BLOCKED; running_p->io_completion_time = current_time + running_p->io_burst_duration;
                      } else { running_p->state = STATE_READY; }

                      current_running_idx = earliest_deadline_idx;
                      Process *p = next_p;
                      if (CONTEXT_SWITCH_COST > 0) {
                         // CORRIGIDO Printf Context Switch
                         printf("%-5d | Context Switch (P%d to P%d) - Custo: %d\n", current_time, running_p->id, p->id, CONTEXT_SWITCH_COST);

                         current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                         check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                         if (max_simulation_time != -1 && current_time >= max_simulation_time) { running_p->state = STATE_READY; current_running_idx = -1; break; }
                         earliest_deadline_idx = find_earliest_deadline_process_ready(local_list, count, current_time);
                         if (earliest_deadline_idx == -1) { current_running_idx = -1; goto check_idle_edf; }
                         current_running_idx = earliest_deadline_idx; p = &local_list[current_running_idx];
                      }
                      p->state = STATE_RUNNING;
                      if (p->start_time == -1) p->start_time = current_time;
                      last_process_id = p->id;
                      printf("%-5d | P%d (Deadl: %d) inicia execução PREEMPTIVA (R: %d)\n", current_time, p->id, p->deadline, p->remaining_time);
                 }
            }
        } else {
             current_running_idx = -1;
        }

        if (current_running_idx != -1) {
             Process *p = &local_list[current_running_idx];
             if (max_simulation_time != -1 && current_time >= max_simulation_time) { p->state = STATE_READY; current_running_idx = -1; break; }

             current_time++; p->remaining_time--;
             printf("        P%d executa (R:%d)\n", p->id, p->remaining_time);

             if (p->remaining_time == 0) {
                 printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                  if (p->io_burst_duration > 0) {
                       printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                  } else {
                       p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                  }
                 current_running_idx = -1;
             } else {
                  if (p->io_burst_duration > 0 && (rand() % p->burst_time < 1)) {
                       printf("%-5d | P%d iniciando I/O (%d unidades) durante execução\n", current_time, p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                       current_running_idx = -1;
                 }
             }
        } else {
        check_idle_edf:
              int next_event_time = INT_MAX;
              for(int i=0; i<count; i++) {
                  if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                  if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
              }
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  int idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                  if(idle_until > current_time) { printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until); total_idle_time += (idle_until - current_time); current_time = idle_until; }
                  break;
              } else {
                  if(next_event_time > current_time) { printf("%-5d | CPU Ociosa até t=%d\n", current_time, next_event_time); total_idle_time += (next_event_time - current_time); current_time = next_event_time; }
              }
        }
     } // Fim do while

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}

// ---------------------- RM (Preemptive - baseado em Prioridade) ----------------------
void schedule_rm_preemptive(Process *list, int count, int max_simulation_time) {
    printf("\n--- RM (Rate Monotonic - Preemptive, baseado em Prioridade Estática) ---\n");
    printf("    (Considera campo 'priority' como prioridade RM: 1=max)\n");
    printf("    (Aging Desabilitado por padrão para RM)\n");

     schedule_priority(list, count, 1 /* preemptive */, 0 /* enable_aging */, max_simulation_time);

     // Restaura valor original do aging
     #undef AGING_THRESHOLD
     #define AGING_THRESHOLD original_aging_threshold
}


// ---------------------- MLQ (Multilevel Queue) ----------------------
void schedule_mlq(Process *list, int count, int base_quantum, int max_simulation_time) {
     printf("\n--- MLQ (Multilevel Queue) ---\n");
     printf("    Q0 (Prio 1,2): RR (q=%d)\n", base_quantum);
     printf("    Q1 (Prio 3,4): RR (q=%d)\n", base_quantum * 2);
     printf("    Q2 (Prio 5):   FCFS\n");
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
     printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);
     if (count <= 0 || base_quantum <=0) return;

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
        if (list[i].priority <= 2) local_list[i].current_queue = 0;
        else if (list[i].priority <= 4) local_list[i].current_queue = 1;
        else local_list[i].current_queue = 2;
    }

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;
    int last_checked_q0 = -1;
    int last_checked_q1 = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) total_idle_time = current_time;

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        check_new_arrivals(local_list, count, current_time);
        check_io_completions(local_list, count, current_time);

        int candidate_idx = -1;
        int candidate_queue = -1;
        int current_quantum = 0;

        int search_start_q0 = (last_checked_q0 + 1);
        for (int i = 0; i < count; ++i) {
            int idx = (search_start_q0 + i) % count;
            if (local_list[idx].state == STATE_READY && local_list[idx].current_queue == 0) {
                candidate_idx = idx; candidate_queue = 0; current_quantum = base_quantum; last_checked_q0 = idx; goto process_selected_mlq;
            }
        }
        int search_start_q1 = (last_checked_q1 + 1);
         for (int i = 0; i < count; ++i) {
            int idx = (search_start_q1 + i) % count;
             if (local_list[idx].state == STATE_READY && local_list[idx].current_queue == 1) {
                candidate_idx = idx; candidate_queue = 1; current_quantum = base_quantum * 2; last_checked_q1 = idx; goto process_selected_mlq;
            }
        }
         int fcfs_idx = -1; int min_arrival = INT_MAX;
         for (int i = 0; i < count; ++i) {
              if (local_list[i].state == STATE_READY && local_list[i].current_queue == 2) {
                  if (local_list[i].arrival_time < min_arrival) { min_arrival = local_list[i].arrival_time; fcfs_idx = i; }
              }
         }
         if (fcfs_idx != -1) { candidate_idx = fcfs_idx; candidate_queue = 2; current_quantum = local_list[candidate_idx].remaining_time; goto process_selected_mlq; }

    process_selected_mlq:;

        if (candidate_idx != -1) {
             Process *next_p = &local_list[candidate_idx];
             if (current_running_idx == -1) {
                 current_running_idx = candidate_idx; Process *p = next_p;
                  if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id) {
                      // CORRIGIDO Printf Context Switch
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d [Q%d]) - Custo: %d\n", current_time, from_str, p->id, candidate_queue, CONTEXT_SWITCH_COST);

                     current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                     check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                     continue; // Recomeça loop para re-selecionar
                  }
                 p->state = STATE_RUNNING; p->time_slice_remaining = current_quantum;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id;
                 printf("%-5d | P%d [Q%d] inicia/continua (Qtm: %d, R: %d)\n", current_time, p->id, candidate_queue, current_quantum, p->remaining_time);

             } else {
                  Process *running_p = &local_list[current_running_idx];
                  if (candidate_queue < running_p->current_queue) {
                       printf("%-5d | PREEMPÇÃO MLQ: P%d [Q%d] preempta P%d [Q%d]\n",
                             current_time, next_p->id, candidate_queue, running_p->id, running_p->current_queue);
                       if (running_p->io_burst_duration > 0 && running_p->remaining_time > 0) {
                           printf("        P%d preemptido iniciando I/O (%d unidades)\n", running_p->id, running_p->io_burst_duration);
                           running_p->state = STATE_BLOCKED; running_p->io_completion_time = current_time + running_p->io_burst_duration;
                       } else { running_p->state = STATE_READY; }

                       current_running_idx = candidate_idx; Process *p = next_p;
                       if (CONTEXT_SWITCH_COST > 0) {
                            // CORRIGIDO Printf Context Switch
                           printf("%-5d | Context Switch (P%d to P%d [Q%d]) - Custo: %d\n", current_time, running_p->id, p->id, candidate_queue, CONTEXT_SWITCH_COST);

                           current_time += CONTEXT_SWITCH_COST; total_context_switches++;
                           check_new_arrivals(local_list, count, current_time); check_io_completions(local_list, count, current_time);
                           if (max_simulation_time != -1 && current_time >= max_simulation_time) { running_p->state = STATE_READY; current_running_idx = -1; break; }
                            continue; // Recomeça loop para re-selecionar
                       }
                       p->state = STATE_RUNNING; p->time_slice_remaining = current_quantum;
                       if (p->start_time == -1) p->start_time = current_time;
                       last_process_id = p->id;
                       printf("%-5d | P%d [Q%d] inicia PREEMPTIVA (Qtm: %d, R: %d)\n", current_time, p->id, candidate_queue, current_quantum, p->remaining_time);
                  }
             }
        } else {
             current_running_idx = -1;
        }

        if (current_running_idx != -1) {
            Process *p = &local_list[current_running_idx];
            if (max_simulation_time != -1 && current_time >= max_simulation_time) { p->state = STATE_READY; current_running_idx = -1; break; }

            current_time++; p->remaining_time--; p->time_slice_remaining--;
            printf("        P%d [Q%d] executa (R:%d, Q:%d)\n", p->id, p->current_queue, p->remaining_time, p->time_slice_remaining);

            if (p->remaining_time == 0) {
                 printf("%-5d | P%d [Q%d] TERMINOU\n", current_time, p->id, p->current_queue);
                  if (p->io_burst_duration > 0) {
                       printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                  } else { p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++; }
                 current_running_idx = -1;
            } else if (p->time_slice_remaining == 0 && p->current_queue < 2) { // Fim quantum Q0/Q1
                 printf("%-5d | P%d [Q%d] fim do quantum, volta para READY\n", current_time, p->id, p->current_queue);
                 if (p->io_burst_duration > 0) {
                      printf("        P%d iniciando I/O (%d unidades)\n", p->id, p->io_burst_duration);
                      p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                 } else { p->state = STATE_READY; }
                 current_running_idx = -1;
            } else { // FCFS ou RR no meio do quantum
                 if (p->io_burst_duration > 0 && (rand() % p->burst_time < 1)) {
                       printf("%-5d | P%d iniciando I/O (%d unidades) durante execução\n", current_time, p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                       current_running_idx = -1;
                 }
            }
        } else { // CPU OCIOSA
             int next_event_time = INT_MAX;
              for(int i=0; i<count; i++) {
                  if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                  if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
              }
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  int idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                  if(idle_until > current_time) { printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until); total_idle_time += (idle_until - current_time); current_time = idle_until; }
                  break;
              } else {
                  if(next_event_time > current_time) { printf("%-5d | CPU Ociosa até t=%d\n", current_time, next_event_time); total_idle_time += (next_event_time - current_time); current_time = next_event_time; }
              }
        }
    } // Fim do while

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}