#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>

// ------------------------- Utilitários -------------------------

// Comparador para qsort: ordena por tempo de chegada
int compare_arrival(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->arrival_time - p2->arrival_time;
}

// Comparador para qsort: ordena por prioridade (menor primeiro)
int compare_priority(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->priority == p2->priority) {
        return p1->arrival_time - p2->arrival_time;
    }
    return p1->priority - p2->priority;
}

// Comparador para qsort: ordena por deadline (mais próximo primeiro)
int compare_deadline(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->deadline == p2->deadline) {
        return p1->arrival_time - p2->arrival_time;
    }
    return p1->deadline - p2->deadline;
}

// Função auxiliar para encontrar o tempo de chegada do primeiro processo
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


// ---------------------- FCFS (com Métricas Completas) ----------------------
void schedule_fcfs(Process *list, int count) {
    printf("\n--- FCFS (First-Come, First-Served) ---\n");
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
    int total_waiting = 0, total_turnaround = 0;
    int total_burst = 0;
    int deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_finish_time = 0;

    current_time = local_list[0].arrival_time;
    if (current_time < 0) current_time = 0;
    if (current_time > 0) {
        cpu_idle_time = current_time;
    }

    printf("ID | Chegada | Burst | Prioridade | Deadline | Start | Finish | Espera | Turnaround | Deadline Met?\n");
    printf("-----------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        Process *p = &local_list[i];
        total_burst += p->burst_time;

        if (current_time < p->arrival_time) {
            cpu_idle_time += (p->arrival_time - current_time);
            current_time = p->arrival_time;
        }

        int start = current_time;
        int finish = start + p->burst_time;
        int turnaround = finish - p->arrival_time;
        int waiting = start - p->arrival_time;

        if (waiting < 0) waiting = 0;
        if (turnaround < 0) turnaround = 0;

        total_turnaround += turnaround;
        total_waiting += waiting;
        current_time = finish;
        last_finish_time = finish;

        int missed = 0;
        if (p->deadline > 0 && finish > p->deadline) {
            deadline_misses++;
            missed = 1;
        }

        printf("P%-2d| %-7d | %-5d | %-10d | %-8d | %-5d | %-6d | %-6d | %-10d | %s\n",
               p->id, p->arrival_time, p->burst_time, p->priority, p->deadline,
               start, finish, waiting, turnaround, missed ? "NÃO" : "Sim");
    }
    printf("-----------------------------------------------------------------------------------------------------\n");

    int first_arrival_time = (count > 0) ? local_list[0].arrival_time : 0;
    if (first_arrival_time < 0) first_arrival_time = 0;

    float simulation_duration = (float)(last_finish_time - first_arrival_time);
    if (simulation_duration <= 0 && last_finish_time > 0) simulation_duration = (float)last_finish_time;
    if (simulation_duration <= 0) simulation_duration = 1.0f;

    float avg_waiting = (count > 0) ? (float)total_waiting / count : 0;
    float avg_turnaround = (count > 0) ? (float)total_turnaround / count : 0;
    float cpu_busy_time = (float)total_burst;
    float cpu_utilization = (last_finish_time > 0) ? (cpu_busy_time / last_finish_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (last_finish_time > 0) ? (float)count / last_finish_time : 0;

    printf("\n--- Métricas FCFS ---\n");
    printf("Tempo Total de Simulação (até último finish): %d\n", last_finish_time);
    printf("Tempo Ocioso da CPU:                       %d\n", cpu_idle_time);
    printf("Tempo Efetivo da CPU (Soma dos Bursts):    %d\n", total_burst);
    printf("--------------------------------------------------\n");
    printf("Média de Tempo de Espera:                  %.2f\n", avg_waiting);
    printf("Média de Tempo de Turnaround:              %.2f\n", avg_turnaround);
    printf("Utilização da CPU:                         %.2f %%\n", cpu_utilization);
    printf("Throughput (procs / tempo total):          %.4f\n", throughput_total_time);
    printf("Deadlines Perdidos:                        %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(local_list);
}

// ---------------------- Round Robin ----------------------
void schedule_rr(Process *list, int count, int quantum) {
    printf("\n--- Round Robin (q = %d) ---\n", quantum);
     if (count <= 0 || quantum <=0) {
         printf("Nenhum processo ou quantum inválido.\n");
         return;
     }

    int current_time = 0, completed = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst = 0, deadline_misses = 0;
    int last_finish_time = 0, cpu_idle_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
     if (first_event_time < 0) first_event_time = 0;
    current_time = first_event_time;
    if (current_time > 0) cpu_idle_time = current_time;

    int *remaining = malloc(count * sizeof(int));
    int *arrival = malloc(count * sizeof(int));
    int *start_time = malloc(count * sizeof(int));
    int *finish_time = malloc(count * sizeof(int));
    int *has_started = calloc(count, sizeof(int));

     if (!remaining || !arrival || !start_time || !finish_time || !has_started) {
        fprintf(stderr, "Falha ao alocar memória em RR\n");
        free(remaining); free(arrival); free(start_time); free(finish_time); free(has_started);
        return;
     }

    for (int i = 0; i < count; i++) {
        remaining[i] = list[i].burst_time;
        arrival[i] = list[i].arrival_time;
        total_burst += list[i].burst_time;
        start_time[i] = -1;
        finish_time[i] = -1;
    }

    printf("Tempo | Evento\n");
    printf("--------------------------------\n");

    while (completed < count) {
        int executed_in_cycle = 0;
        int ready_count = 0;

        for (int i = 0; i < count; i++) {
            if (remaining[i] > 0 && arrival[i] <= current_time) {
                 ready_count++;
                int exec_time = (remaining[i] > quantum) ? quantum : remaining[i];

                if(start_time[i] == -1) {
                    start_time[i] = current_time;
                }

                printf("%-5d | P%d executa por %d unidades (Restam: %d)\n", current_time, list[i].id, exec_time, remaining[i] - exec_time);

                current_time += exec_time;
                remaining[i] -= exec_time;
                executed_in_cycle = 1;

                if (remaining[i] == 0) {
                    completed++;
                    finish_time[i] = current_time;
                    last_finish_time = current_time;
                    int turnaround = finish_time[i] - list[i].arrival_time;
                    int waiting = turnaround - list[i].burst_time;
                    if (waiting < 0) waiting = 0;
                    total_turnaround += turnaround;
                    total_waiting += waiting;

                    printf("%-5d | P%d TERMINOU. Espera=%d, Turnaround=%d\n", current_time, list[i].id, waiting, turnaround);

                    if (list[i].deadline > 0 && finish_time[i] > list[i].deadline) {
                        deadline_misses++;
                        printf("%-5d | -> Deadline P%d perdido (Terminou: %d, Deadline: %d)\n", current_time, list[i].id, finish_time[i], list[i].deadline);
                    }
                }
            }
        }

        if (!executed_in_cycle && completed < count) {
            int next_event_time = INT_MAX;
             for(int i=0; i<count; i++) {
                if(remaining[i] > 0 && arrival[i] > current_time && arrival[i] < next_event_time) {
                    next_event_time = arrival[i];
                }
             }

             if (next_event_time == INT_MAX) {
                printf("%-5d | CPU Ociosa (nenhum processo pronto ou a chegar).\n", current_time);
                 int found_remaining = 0;
                 for(int k=0; k<count; k++) if(remaining[k]>0) found_remaining=1;
                 if(!found_remaining && completed == count) break; // Normal exit
                 if(!found_remaining && completed < count) {
                     printf("        AVISO: Nenhum restante mas nem todos completos!\n");
                     break;
                 }
                 current_time++;
                 cpu_idle_time++;
             } else {
                 printf("%-5d | CPU Ociosa até próxima chegada em t=%d.\n", current_time, next_event_time);
                 cpu_idle_time += (next_event_time - current_time);
                 current_time = next_event_time;
             }
        } else if (completed == count && !executed_in_cycle) {
              break;
        } else if (executed_in_cycle) {
        } else {
             printf("%-5d | Estado inesperado no loop RR.\n", current_time);
             break;
        }
    }
     printf("--------------------------------\n");

    if (last_finish_time < current_time) last_finish_time = current_time;

    float avg_waiting = (count > 0) ? (float)total_waiting / count : 0;
    float avg_turnaround = (count > 0) ? (float)total_turnaround / count : 0;
    float cpu_busy_time = (float)total_burst;
    float cpu_utilization = (last_finish_time > 0) ? (cpu_busy_time / last_finish_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (last_finish_time > 0) ? (float)count / last_finish_time : 0;

    printf("\n--- Métricas Round Robin (q=%d) ---\n", quantum);
    printf("Tempo Total de Simulação (até último finish): %d\n", last_finish_time);
    printf("Tempo Ocioso da CPU (aprox.):              %d\n", cpu_idle_time);
    printf("Tempo Efetivo da CPU (Soma dos Bursts):    %d\n", total_burst);
    printf("--------------------------------------------------\n");
    printf("Média de Tempo de Espera:                  %.2f\n", avg_waiting);
    printf("Média de Tempo de Turnaround:              %.2f\n", avg_turnaround);
    printf("Utilização da CPU:                         %.2f %%\n", cpu_utilization);
    printf("Throughput (procs / tempo total):          %.4f\n", throughput_total_time);
    printf("Deadlines Perdidos:                        %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(remaining);
    free(arrival);
    free(start_time);
    free(finish_time);
    free(has_started);
}

// ------------------ Priority Scheduling ------------------
// TODO: Adicionar cálculo e apresentação das métricas completas
void schedule_priority(Process *list, int count, int preemptive) {
    printf("\n--- Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
     if (count <= 0) {
         printf("Nenhum processo para escalonar.\n");
         return;
     }

    int current_time = 0, completed = 0;
    // Métricas a calcular
    int total_waiting = 0, total_turnaround = 0, total_burst = 0;
    int deadline_misses = 0, cpu_idle_time = 0, last_finish_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
    if (first_event_time < 0) first_event_time = 0;

    int *remaining = malloc(count * sizeof(int));
    Process *local_list = malloc(count * sizeof(Process));
    int *finish_times = malloc(count * sizeof(int)); // Para guardar tempos finais
     if (!remaining || !local_list || !finish_times) {
         fprintf(stderr, "Falha ao alocar memória em Priority\n");
         free(remaining); free(local_list); free(finish_times); return;
     }
    for (int i = 0; i < count; i++) {
        local_list[i] = list[i];
        remaining[i] = list[i].burst_time;
        local_list[i].remaining_time = list[i].burst_time;
        total_burst += list[i].burst_time;
        finish_times[i] = -1;
    }

    current_time = first_event_time;
    if (current_time > 0) cpu_idle_time = current_time;


    printf("Tempo | Evento\n");
    printf("--------------------------------\n");

    while (completed < count) {
        int idx = -1;
        int min_priority = INT_MAX;

        // Encontrar processo pronto com maior prioridade
        for (int i = 0; i < count; i++) {
            if (local_list[i].arrival_time <= current_time && remaining[i] > 0) {
                if (local_list[i].priority < min_priority) {
                    min_priority = local_list[i].priority;
                    idx = i;
                }
                 else if (local_list[i].priority == min_priority) {
                     if (idx == -1 || local_list[i].arrival_time < local_list[idx].arrival_time) {
                         idx = i;
                     }
                 }
            }
        }

        if (idx != -1) {
            // Processo selecionado
            int exec_time = 0;
            if (preemptive) {
                // Preemptivo: Executa por 1 unidade OU até terminar OU até chegar alguém mais prioritário
                // Simplificação: Executa por 1 unidade
                exec_time = 1;
            } else {
                // Não preemptivo: Executa até o fim
                exec_time = remaining[idx];
            }
             if (exec_time > remaining[idx]) exec_time = remaining[idx]; // Garante não exceder o restante

            printf("%-5d | P%d (Pri: %d) executa por %d unidades (Restam: %d)\n",
                    current_time, local_list[idx].id, local_list[idx].priority, exec_time, remaining[idx] - exec_time);

            remaining[idx] -= exec_time;
            current_time += exec_time;
            last_finish_time = current_time; // Atualiza tempo da última atividade

            if (remaining[idx] == 0) {
                completed++;
                finish_times[idx] = current_time;
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
        } else {
            // Nenhum processo pronto, avançar o tempo para a próxima chegada
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
                  if (completed < count) printf("        AVISO: %d processos não completaram.\n", count - completed);
                 break;
             } else {
                  printf("%-5d | CPU Ociosa até t=%d.\n", current_time, next_arrival);
                  cpu_idle_time += (next_arrival - current_time);
                 current_time = next_arrival;
             }
        }
    }
     printf("--------------------------------\n");

     // --- Cálculo e Impressão de Métricas (Priority) ---
    float simulation_duration = (float)(last_finish_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)last_finish_time > 0 ? last_finish_time : 1.0f;

    float avg_waiting = (count > 0) ? (float)total_waiting / count : 0;
    float avg_turnaround = (count > 0) ? (float)total_turnaround / count : 0;
    float cpu_busy_time = (float)total_burst;
    float cpu_utilization = (last_finish_time > 0) ? (cpu_busy_time / last_finish_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (last_finish_time > 0) ? (float)count / last_finish_time : 0;


     printf("\n--- Métricas Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
     printf("Tempo Total de Simulação (até último evento): %d\n", last_finish_time);
     printf("Tempo Ocioso da CPU (estimado):            %d\n", cpu_idle_time);
     printf("Tempo Efetivo da CPU (Soma dos Bursts):    %d\n", total_burst);
     printf("--------------------------------------------------\n");
     printf("Média de Tempo de Espera:                  %.2f\n", avg_waiting);
     printf("Média de Tempo de Turnaround:              %.2f\n", avg_turnaround);
     printf("Utilização da CPU:                         %.2f %%\n", cpu_utilization);
     printf("Throughput (procs / tempo total):          %.4f\n", throughput_total_time);
     printf("Deadlines Perdidos:                        %d\n", deadline_misses);
     printf("--------------------------------------------------\n");

    free(remaining);
    free(local_list);
    free(finish_times);
}

// ---------------------- SJF (Non-Preemptive - com Métricas Completas) ----------------------
void schedule_sjf(Process *list, int count) {
    printf("\n--- SJF (Shortest Job First - Non-Preemptive) ---\n");
    if (count <= 0) {
         printf("Nenhum processo para escalonar.\n");
         return;
    }

    int current_time = 0;
    int completed = 0;
    int total_waiting = 0, total_turnaround = 0;
    int total_burst = 0;
    int deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_finish_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
    if (first_event_time < 0) first_event_time = 0;

    int *finished = calloc(count, sizeof(int));
    Process *local_list = malloc(count * sizeof(Process));
     if (!finished || !local_list) {
         fprintf(stderr, "Falha ao alocar memória em SJF\n");
         free(finished); free(local_list); return;
     }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
        total_burst += list[i].burst_time;
    }

    current_time = first_event_time;
    if (current_time > 0) {
        cpu_idle_time = current_time;
    }

    printf("ID | Chegada | Burst | Prioridade | Deadline | Start | Finish | Espera | Turnaround | Deadline Met?\n");
    printf("-----------------------------------------------------------------------------------------------------\n");

    while (completed < count) {
        int shortest_idx = -1;
        int min_burst = INT_MAX;

        for (int i = 0; i < count; i++) {
            if (!finished[i] && local_list[i].arrival_time <= current_time) {
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

        if (shortest_idx != -1) {
            Process *p = &local_list[shortest_idx];
            int start = current_time;
            int finish = start + p->burst_time;
            int turnaround = finish - p->arrival_time;
            int waiting = start - p->arrival_time;

            if (waiting < 0) waiting = 0;
            if (turnaround < 0) turnaround = 0;

            total_turnaround += turnaround;
            total_waiting += waiting;
            current_time = finish;
            last_finish_time = current_time;

            finished[shortest_idx] = 1;
            completed++;

            int missed = 0;
            if (p->deadline > 0 && finish > p->deadline) {
                deadline_misses++;
                missed = 1;
            }

            printf("P%-2d| %-7d | %-5d | %-10d | %-8d | %-5d | %-6d | %-6d | %-10d | %s\n",
                   p->id, p->arrival_time, p->burst_time, p->priority, p->deadline,
                   start, finish, waiting, turnaround, missed ? "NÃO" : "Sim");

        } else {
            int next_arrival = INT_MAX;
            for(int i=0; i<count; i++) {
                if (!finished[i] && local_list[i].arrival_time > current_time) {
                     if (local_list[i].arrival_time < next_arrival) {
                        next_arrival = local_list[i].arrival_time;
                     }
                }
            }

            if (next_arrival == INT_MAX) {
                 int still_to_run = 0;
                 for(int i=0; i<count; i++) if (!finished[i]) still_to_run = 1;
                 if(still_to_run) printf("%-5d | AVISO/ERRO: CPU Ociosa, mas nem todos terminaram e não há chegadas futuras.\n", current_time);
                 break;
            }

            int idle_period = next_arrival - current_time;
            if (idle_period < 0) idle_period = 0;
            cpu_idle_time += idle_period;
            current_time = next_arrival;
        }
    }
     printf("-----------------------------------------------------------------------------------------------------\n");

    free(finished);

     float simulation_duration = (float)(last_finish_time - first_event_time);
     if (simulation_duration <= 0 && last_finish_time > 0) simulation_duration = (float)last_finish_time;
     if (simulation_duration <= 0) simulation_duration = 1.0f;

    float avg_waiting = (count > 0) ? (float)total_waiting / count : 0;
    float avg_turnaround = (count > 0) ? (float)total_turnaround / count : 0;
    float cpu_busy_time = (float)total_burst;
    float cpu_utilization = (last_finish_time > 0) ? (cpu_busy_time / last_finish_time) * 100.0f : 0;
     if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (last_finish_time > 0) ? (float)count / last_finish_time : 0;

    printf("\n--- Métricas SJF (Non-Preemptive) ---\n");
    printf("Tempo Total de Simulação (até último finish): %d\n", last_finish_time);
    printf("Tempo Ocioso da CPU:                       %d\n", cpu_idle_time);
    printf("Tempo Efetivo da CPU (Soma dos Bursts):    %d\n", total_burst);
    printf("--------------------------------------------------\n");
    printf("Média de Tempo de Espera:                  %.2f\n", avg_waiting);
    printf("Média de Tempo de Turnaround:              %.2f\n", avg_turnaround);
    printf("Utilização da CPU:                         %.2f %%\n", cpu_utilization);
    printf("Throughput (procs / tempo total):          %.4f\n", throughput_total_time);
    printf("Deadlines Perdidos:                        %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

     free(local_list);
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

// ---------------------- EDF (Preemptive) ----------------------
void schedule_edf_preemptive(Process *list, int count) {
    printf("\n--- EDF (Earliest Deadline First - Preemptive) ---\n");
    if (count <= 0) { printf("Nenhum processo.\n"); return; }

    int current_time = 0;
    int completed = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst = 0, deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_finish_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
     if (first_event_time < 0) first_event_time = 0;

    Process *local_list = malloc(count * sizeof(Process));
    int *finished = calloc(count, sizeof(int));
    int *start_exec_time = malloc(count * sizeof(int));
    if (!local_list || !finished || !start_exec_time) {
        fprintf(stderr, "Falha memória EDF\n"); free(local_list); free(finished); free(start_exec_time); return;
    }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
        total_burst += list[i].burst_time;
        start_exec_time[i] = -1;
    }

    current_time = first_event_time;
    if (current_time > 0) cpu_idle_time = current_time;

    printf("Tempo | CPU  | Evento\n");
    printf("------------------------------------------\n");

    while(completed < count) {
        int running_idx = find_earliest_deadline_process(local_list, count, finished, current_time);

        if (running_idx != -1) {
            Process *p = &local_list[running_idx];
            if (start_exec_time[running_idx] == -1) {
                 start_exec_time[running_idx] = current_time;
            }

            printf("%-5d | P%-3d | Executa (Deadline: %d, Resta: %d)\n", current_time, p->id, p->deadline, p->remaining_time - 1);
            p->remaining_time--;
            current_time++;
            last_finish_time = current_time;

            if (p->remaining_time == 0) {
                finished[running_idx] = 1;
                completed++;
                int finish_time = current_time;
                int turnaround = finish_time - p->arrival_time;
                int waiting = turnaround - p->burst_time;
                 if (waiting < 0) waiting = 0;

                total_turnaround += turnaround;
                total_waiting += waiting;

                printf("%-5d | P%-3d | TERMINOU. Espera=%d, Turnaround=%d\n", current_time, p->id, waiting, turnaround);

                if (p->deadline > 0 && finish_time > p->deadline) {
                    deadline_misses++;
                     printf("%-5d | P%-3d | !!! DEADLINE PERDIDO (Terminou: %d, Deadline: %d)\n", current_time, p->id, finish_time, p->deadline);
                }
            }
        } else {
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
                 if (completed < count) {
                      printf("        | AVISO: Simulação terminou mas %d processos não completaram.\n", count - completed);
                 }
                 break;
             } else {
                 printf("%-5d | ---- | Ociosa até t=%d\n", current_time, next_arrival);
                 cpu_idle_time += (next_arrival - current_time);
                 current_time = next_arrival;
             }
        }
    }
     printf("------------------------------------------\n");

    float simulation_duration = (float)(last_finish_time - first_event_time);
    if (simulation_duration <= 0) simulation_duration = (float)last_finish_time > 0 ? last_finish_time : 1.0f;

    float avg_waiting = (count > 0) ? (float)total_waiting / count : 0;
    float avg_turnaround = (count > 0) ? (float)total_turnaround / count : 0;
    float cpu_busy_time = (float)total_burst;
    float cpu_utilization = (last_finish_time > 0) ? (cpu_busy_time / last_finish_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (last_finish_time > 0) ? (float)count / last_finish_time : 0;

    printf("\n--- Métricas EDF (Preemptive) ---\n");
    printf("Tempo Total de Simulação (até último evento): %d\n", last_finish_time);
    printf("Tempo Ocioso da CPU (estimado):            %d\n", cpu_idle_time);
    printf("Tempo Efetivo da CPU (Soma dos Bursts):    %d\n", total_burst);
    printf("--------------------------------------------------\n");
    printf("Média de Tempo de Espera:                  %.2f\n", avg_waiting);
    printf("Média de Tempo de Turnaround:              %.2f\n", avg_turnaround);
    printf("Utilização da CPU:                         %.2f %%\n", cpu_utilization);
    printf("Throughput (procs / tempo total):          %.4f\n", throughput_total_time);
    printf("Deadlines Perdidos:                        %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(local_list);
    free(finished);
    free(start_exec_time);
}

// ---------------------- RM (Preemptive) ----------------------
void schedule_rm_preemptive(Process *list, int count) {
    printf("\n--- RM (Rate Monotonic - Preemptive, baseado em Prioridade) ---\n");
     if (count <= 0) { printf("Nenhum processo.\n"); return; }

    int current_time = 0;
    int completed = 0;
    int total_waiting = 0, total_turnaround = 0, total_burst = 0, deadline_misses = 0;
    int cpu_idle_time = 0;
    int last_finish_time = 0;
    int first_event_time = find_min_arrival_time(list, count);
    if (first_event_time < 0) first_event_time = 0;

    Process *local_list = malloc(count * sizeof(Process));
    int *finished = calloc(count, sizeof(int));
    int *start_exec_time = malloc(count * sizeof(int));
     if (!local_list || !finished || !start_exec_time) {
        fprintf(stderr, "Falha memória RM\n"); free(local_list); free(finished); free(start_exec_time); return;
    }
     for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
        total_burst += list[i].burst_time;
        start_exec_time[i] = -1;
    }

    current_time = first_event_time;
    if (current_time > 0) cpu_idle_time = current_time;

    printf("Tempo | CPU  | Evento\n");
    printf("------------------------------------------\n");

     while(completed < count) {
        int running_idx = find_highest_priority_process(local_list, count, finished, current_time);

        if (running_idx != -1) {
             Process *p = &local_list[running_idx];
             if (start_exec_time[running_idx] == -1) {
                 start_exec_time[running_idx] = current_time;
            }

            printf("%-5d | P%-3d | Executa (Prioridade: %d, Resta: %d)\n", current_time, p->id, p->priority, p->remaining_time - 1);
            p->remaining_time--;
            current_time++;
            last_finish_time = current_time;

             if (p->remaining_time == 0) {
                finished[running_idx] = 1;
                completed++;
                int finish_time = current_time;
                int turnaround = finish_time - p->arrival_time;
                int waiting = turnaround - p->burst_time;
                 if (waiting < 0) waiting = 0;

                total_turnaround += turnaround;
                total_waiting += waiting;

                printf("%-5d | P%-3d | TERMINOU. Espera=%d, Turnaround=%d\n", current_time, p->id, waiting, turnaround);

                 if (p->deadline > 0 && finish_time > p->deadline) {
                    deadline_misses++;
                     printf("%-5d | P%-3d | !!! DEADLINE PERDIDO (Terminou: %d, Deadline: %d)\n", current_time, p->id, finish_time, p->deadline);
                }
            }
        } else {
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
                  if (completed < count) {
                      printf("        | AVISO: Simulação terminou mas %d processos não completaram.\n", count - completed);
                 }
                 break;
             } else {
                 printf("%-5d | ---- | Ociosa até t=%d\n", current_time, next_arrival);
                 cpu_idle_time += (next_arrival - current_time);
                 current_time = next_arrival;
             }
        }
    }
     printf("------------------------------------------\n");

     float simulation_duration = (float)(last_finish_time - first_event_time);
     if (simulation_duration <= 0) simulation_duration = (float)last_finish_time > 0 ? last_finish_time : 1.0f;

    float avg_waiting = (count > 0) ? (float)total_waiting / count : 0;
    float avg_turnaround = (count > 0) ? (float)total_turnaround / count : 0;
    float cpu_busy_time = (float)total_burst;
    float cpu_utilization = (last_finish_time > 0) ? (cpu_busy_time / last_finish_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f;
    float throughput_total_time = (last_finish_time > 0) ? (float)count / last_finish_time : 0;

    printf("\n--- Métricas RM (Preemptive - baseado em Prioridade) ---\n");
    printf("Tempo Total de Simulação (até último evento): %d\n", last_finish_time);
    printf("Tempo Ocioso da CPU (estimado):            %d\n", cpu_idle_time);
    printf("Tempo Efetivo da CPU (Soma dos Bursts):    %d\n", total_burst);
    printf("--------------------------------------------------\n");
    printf("Média de Tempo de Espera:                  %.2f\n", avg_waiting);
    printf("Média de Tempo de Turnaround:              %.2f\n", avg_turnaround);
    printf("Utilização da CPU:                         %.2f %%\n", cpu_utilization);
    printf("Throughput (procs / tempo total):          %.4f\n", throughput_total_time);
    printf("Deadlines Perdidos:                        %d\n", deadline_misses);
    printf("--------------------------------------------------\n");

    free(local_list);
    free(finished);
    free(start_exec_time);
}

// ------------------------ EDF (Simplista - Não Preemptivo por default) ------------------------
void schedule_edf(Process *list, int count) {
    printf("\n--- EDF (Implementação Simplista: Ordena por Deadline + FCFS) ---\n");
     if (count <= 0) {
         printf("Nenhum processo para escalonar.\n");
         return;
     }
    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { fprintf(stderr, "Falha ao alocar memória em EDF\n"); return; }
     for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        local_list[i].remaining_time = list[i].burst_time;
     }

    qsort(local_list, count, sizeof(Process), compare_deadline);
    printf("Executando FCFS na lista ordenada por deadline...\n");
    schedule_fcfs(local_list, count);

    free(local_list);
}

// ------------------ Rate Monotonic (Simplista - Não Preemptivo por default) ------------------
void schedule_rm(Process *list, int count) {
    printf("\n--- Rate Monotonic (Implementação Simplista: Chama Priority Scheduling) ---\n");
     if (count <= 0) {
         printf("Nenhum processo para escalonar.\n");
         return;
     }
    printf("Assumindo que 'priority' reflete a taxa (menor valor = maior taxa/prioridade).\n");
    printf("Executando Priority Scheduling Não-Preemptivo...\n");
    schedule_priority(list, count, 0);
}