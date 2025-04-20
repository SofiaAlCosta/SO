#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

// Preservando o valor original da macro para RM
#ifdef AGING_THRESHOLD
#define original_aging_threshold AGING_THRESHOLD
#endif


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
        return p1->arrival_time - p2->arrival_time; // Desempate por chegada
    }
    return p1->current_priority - p2->current_priority; // Menor valor numérico = maior prioridade
}

int compare_deadline(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    if (p1->deadline == p2->deadline) {
        return p1->arrival_time - p2->arrival_time; // Desempate por chegada
    }
    return p1->deadline - p2->deadline;
}

// --- Funções Auxiliares ---
int find_min_arrival_time(Process *list, int count) {
    int min_arrival = INT_MAX;
    int found = 0;
    for (int i = 0; i < count; i++) {
        // Considera apenas processos não terminados E que ainda não chegaram (NEW)
        if (list[i].state == STATE_NEW && list[i].arrival_time >= 0 && list[i].arrival_time < min_arrival) {
            min_arrival = list[i].arrival_time;
            found = 1;
        }
    }
    // Retorna 0 se nenhum processo encontrado OU se o primeiro chega em 0
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
                // Desempate por tempo de chegada (FIFO entre prioridades iguais)
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
            if (processes[i].deadline < min_deadline) {
                min_deadline = processes[i].deadline;
                best_idx = i;
            } else if (processes[i].deadline == min_deadline) {
                 // Desempate por tempo de chegada
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
                // Desempate por tempo de chegada
                 if (best_idx == -1 || processes[i].arrival_time < processes[best_idx].arrival_time) {
                    best_idx = i;
                }
            }
        }
    }
    return best_idx; // Retorna -1 se nenhum pronto
}

// Função Aging (Aplicada apenas se preemptivo e enable_aging=1)
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
             // Importante resetar para evitar aging logo ao voltar de I/O ou ao ser preemptido
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
            // Reatribuição de fila MLQ/MLFQ poderia ocorrer aqui se necessário
            moved_count++;
        }
    }
    return moved_count; // Retorna quantos processos terminaram I/O
}


int check_new_arrivals(Process *list, int count, int current_time) {
     int arrived_count = 0;
    for (int i = 0; i < count; i++) {
        // Importante checar se já chegou e está NEW
        if (list[i].state == STATE_NEW && list[i].arrival_time <= current_time) {
             printf("        Arrival: P%d at time %d\n", list[i].id, current_time);
            list[i].state = STATE_READY;
            list[i].time_in_ready_queue = 0; // Reset aging counter
             // Atribuição inicial de fila MLQ/MLFQ poderia ocorrer aqui
             if (list[i].current_queue == -1) { // Atribui fila se ainda não tiver
                 if (list[i].priority <= 2) list[i].current_queue = 0;
                 else if (list[i].priority <= 4) list[i].current_queue = 1;
                 else list[i].current_queue = 2;
             }
            arrived_count++;
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
    printf("ID | Chegada | Burst | Prio | Dead | IO Dur | Start | Finish | Turnar | Wait | D.Met?| Estado Final (R:Tempo Restante)\n");
    printf("----------------------------------------------------------------------------------------------------------------------\n");

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
            // do tempo em READY seria melhor, mas esta é a definição comum.
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
             // Se não terminou, marca como incompleto e mostra estado atual
             if (list[i].state == STATE_RUNNING) status_str = "Executando";
             else if (list[i].state == STATE_READY) status_str = "Pronto";
             else if (list[i].state == STATE_BLOCKED) status_str = "Bloqueado";
             else if (list[i].state == STATE_NEW) status_str = "Pendente";
             else status_str = "Desconhecido"; // Should not happen

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
    printf("----------------------------------------------------------------------------------------------------------------------\n");

    float avg_waiting = (completed_count > 0) ? (float)total_waiting / completed_count : 0;
    float avg_turnaround = (completed_count > 0) ? (float)total_turnaround / completed_count : 0;
    // CPU Busy time é o tempo total menos o tempo ocioso.
    // Note: O tempo de context switch *está incluído* no tempo de simulação total,
    // mas *não* é contado como tempo ocioso nem como tempo de execução de processo.
    // A utilização pode ser calculada como (TempoTotal - TempoOcioso) / TempoTotal.
    float cpu_busy_time = (float)(final_time - total_idle_time);
    float cpu_utilization = (final_time > 0) ? (cpu_busy_time / final_time) * 100.0f : 0;
    if (cpu_utilization > 100.0f) cpu_utilization = 100.0f; // Sanity check
    if (cpu_utilization < 0.0f) cpu_utilization = 0.0f;   // Sanity check
    float throughput = (final_time > 0) ? (float)completed_count / final_time : 0;

    printf("\n--- Métricas Globais ---\n");
    printf("Tempo Final da Simulação:      %d\n", final_time);
    printf("Tempo Ocioso da CPU:           %d\n", total_idle_time);
    printf("Tempo Ocupado da CPU (estim.):  %.0f\n", cpu_busy_time);
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

// --- Funções de Log Gantt ---

void init_gantt_log(GanttLog *log) {
    log->segments = NULL;
    log->count = 0;
    log->capacity = 0;
}

void add_gantt_segment(GanttLog *log, int pid, int start, int end) {
    // Ignora segmentos inválidos (sem duração ou início após fim)
    if (start >= end) {
        //fprintf(stderr,"Warning: Ignorando segmento Gantt inválido (PID %d, %d -> %d)\n", pid, start, end);
        return;
    }
    // Aumenta capacidade se necessário
    if (log->count >= log->capacity) {
        log->capacity = (log->capacity == 0) ? 16 : log->capacity * 2;
        GanttSegment *new_segments = realloc(log->segments, log->capacity * sizeof(GanttSegment));
        if (!new_segments) {
            perror("Erro ao realocar memória para Gantt log");
            // Decide como tratar: abortar? ignorar segmento?
            // Por agora, vamos ignorar o segmento
            return;
        }
        log->segments = new_segments;
    }
    // Adiciona o segmento
    log->segments[log->count].process_id = pid;
    log->segments[log->count].start_time = start;
    log->segments[log->count].end_time = end;
    log->count++;
}

void free_gantt_log(GanttLog *log) {
    if (log && log->segments) {
        free(log->segments);
        log->segments = NULL;
        log->count = 0;
        log->capacity = 0;
    }
}

void write_gantt_data(const char* filename, GanttLog *log) {
    if (!log || log->count == 0) {
        printf("Nenhum dado de Gantt para escrever.\n");
        return;
    }
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Erro ao abrir ficheiro de dados do Gantt");
        return;
    }
    fprintf(f, "PID,Start,End\n");
    for (int i = 0; i < log->count; i++) {
        fprintf(f, "%d,%d,%d\n", log->segments[i].process_id,
                log->segments[i].start_time, log->segments[i].end_time);
    }
    fclose(f);
    printf("Dados do Gráfico de Gantt escritos em '%s'\n", filename);
}

// ---------------------- FCFS (First-Come, First-Served) ----------------------
void schedule_fcfs(Process *list, int count, int max_simulation_time, GanttLog *log) {
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
    // Ordena por tempo de chegada para fácil seleção FCFS dos prontos
    qsort(local_list, count, sizeof(Process), compare_arrival);

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1; // Para rastrear CS
    int segment_start_time = 0; // Início do segmento Gantt atual
    int segment_pid = -1;      // PID do processo no segmento atual (-1 = idle)

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) {
        total_idle_time = current_time;
        add_gantt_segment(log, -1, 0, current_time); // Log inicial idle
        segment_start_time = current_time;
    }

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {

        // Chamadas com (void) para evitar warnings, pois o retorno não é usado aqui
        (void)check_new_arrivals(local_list, count, current_time);
        (void)check_io_completions(local_list, count, current_time);

        // Se a CPU está livre, procura o próximo processo PRONTO (por ordem de chegada)
        if (current_running_idx == -1) {
             int next_ready_idx = -1;
             // Encontra o primeiro processo PRONTO na lista ordenada por chegada
             for (int i = 0; i < count; i++){
                 if (local_list[i].state == STATE_READY) {
                     next_ready_idx = i;
                     break; // FCFS pega o primeiro pronto
                 }
             }

            // Se encontrou um processo pronto para executar
            if (next_ready_idx != -1) {
                // --- Fim do segmento anterior (Idle) ---
                if (segment_pid == -1 && current_time > segment_start_time) {
                    add_gantt_segment(log, -1, segment_start_time, current_time);
                }
                segment_start_time = current_time; // Potencial início do CS ou execução

                current_running_idx = next_ready_idx;
                Process *p = &local_list[current_running_idx];

                // Aplica custo de Context Switch se houver troca
                if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id && last_process_id != -1) { // Evita CS do Idle inicial
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);

                     // Adiciona segmento para o Context Switch (opcional, aqui apenas avança tempo)
                     // add_gantt_segment(log, -2, current_time, current_time + CONTEXT_SWITCH_COST); // ID -2 para CS

                     current_time += CONTEXT_SWITCH_COST;
                     total_context_switches++;
                     segment_start_time = current_time; // Segmento do processo começa DEPOIS do CS

                     // Re-checa chegadas/IO durante CS
                     (void)check_new_arrivals(local_list, count, current_time);
                     (void)check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                }

                // Inicia execução do processo
                p->state = STATE_RUNNING;
                if (p->start_time == -1) p->start_time = current_time;
                last_process_id = p->id;
                segment_pid = p->id; // Define o PID do segmento atual
                // segment_start_time já foi definido antes do CS ou quando saiu do idle

                printf("%-5d | P%d inicia execução (Burst Total: %d, Restante: %d)\n", current_time, p->id, p->burst_time, p->remaining_time);

                 int time_to_execute = p->remaining_time;
                 int execution_end_time = current_time + time_to_execute;
                 int time_limit_reached = 0;

                 // Verifica limite de simulação
                 if (max_simulation_time != -1 && execution_end_time > max_simulation_time) {
                     execution_end_time = max_simulation_time;
                     time_to_execute = max_simulation_time - current_time;
                     time_limit_reached = 1;
                     printf("        Execução de P%d limitada a %d unidades pelo T Max\n", p->id, time_to_execute);
                 }

                 // Se não há tempo para executar devido ao limite
                 if (time_to_execute <= 0 && time_limit_reached) {
                      // Log do segmento (mesmo que duração 0 se CS ocorreu no limite)
                      add_gantt_segment(log, p->id, segment_start_time, current_time);
                      segment_pid = -1; // Volta a idle (ou termina simulação)
                      segment_start_time = current_time;

                      p->state = STATE_READY; // Volta a pronto se interrompido
                      current_running_idx = -1;
                      break; // Sai do loop principal
                 }

                 // Simula a execução passo a passo para checar eventos
                 int exec_step = 0;
                 while(exec_step < time_to_execute) {
                      current_time++;
                      exec_step++;
                      p->remaining_time--;
                      (void)check_new_arrivals(local_list, count, current_time);
                      (void)check_io_completions(local_list, count, current_time);
                      // Em FCFS, não há preempção nem I/O no meio
                 }

                 // --- Fim do segmento de execução ---
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                 segment_start_time = current_time; // Próximo segmento começa aqui
                 segment_pid = -1; // CPU fica livre (ou vai para I/O)

                 // Verifica o que aconteceu no fim da execução
                 if (p->remaining_time == 0) {
                     printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                     // Verifica se vai para I/O
                     if (p->io_burst_duration > 0) {
                         printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                         p->state = STATE_BLOCKED;
                         p->io_completion_time = current_time + p->io_burst_duration;
                     } else {
                         p->state = STATE_TERMINATED;
                         p->finish_time = current_time;
                         completed_count++;
                     }
                     current_running_idx = -1; // Libera a CPU
                 } else if (time_limit_reached) {
                      printf("%-5d | Simulação INTERROMPIDA (T Max) enquanto P%d executava.\n", current_time, p->id);
                      p->state = STATE_READY; // Processo volta para pronto
                      current_running_idx = -1;
                      // O break do loop principal ocorrerá na próxima iteração
                 } else {
                       // Situação inesperada em FCFS (não deveria parar no meio sem ser TMax)
                       fprintf(stderr, "Erro lógico em FCFS: P%d parou inesperadamente.\n", p->id);
                       p->state = STATE_TERMINATED; // Força terminação
                       p->finish_time = current_time;
                       completed_count++;
                       current_running_idx = -1;
                 }

            } else { // Nenhum processo pronto, CPU fica Ociosa
                 // --- Garante que segmento anterior (se processo) foi logado ---
                 // (Já tratado ao final da execução do processo)

                // Calcula até quando ficar ocioso
                int next_event_time = INT_MAX;
                // Verifica próxima chegada
                for (int i = 0; i < count; i++) {
                    if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) {
                         next_event_time = local_list[i].arrival_time;
                    }
                    if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) {
                         next_event_time = local_list[i].io_completion_time;
                    }
                }

                int idle_until;
                // Define fim do idle: próximo evento ou limite de simulação
                 if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                      idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                      if (idle_until <= current_time) { // Já atingiu o tempo ou não há mais eventos
                           break; // Termina simulação
                      }
                       printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
                 } else {
                      idle_until = next_event_time;
                      printf("%-5d | CPU Ociosa até t=%d (Próximo evento)\n", current_time, idle_until);
                 }

                 // Avança o tempo e contabiliza idle
                 if (idle_until > current_time) {
                    // Log do segmento Idle
                    if (segment_pid != -1) { // Se antes estava um processo, loga ele (redundante?)
                       // add_gantt_segment(...); // Deveria ter sido logado ao parar
                    }
                    if(current_time > segment_start_time && segment_pid == -1) {
                        // Evita logar idle sobre idle se já era idle
                    } else {
                         add_gantt_segment(log, -1, current_time, idle_until);
                         segment_start_time = idle_until; // Novo segmento começa após o idle
                    }
                    segment_pid = -1; // Garante que estamos em idle

                    total_idle_time += (idle_until - current_time);
                    current_time = idle_until;
                 }
            }
        } else {
             // Se current_running_idx != -1, a lógica de execução/término já tratou o tempo.
             // Este else não deveria ser alcançado em FCFS, pois ele só libera a CPU no final.
             fprintf(stderr, "Erro: Estado inesperado em FCFS (CPU ocupada sem processamento).\n");
             current_running_idx = -1; // Tenta recuperar
        }
    } // Fim do while

    // --- Log do último segmento (se houver) ---
    if (current_time > segment_start_time) {
       add_gantt_segment(log, segment_pid, segment_start_time, current_time);
    }


    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}


// ---------------------- Round Robin (RR) ----------------------
void schedule_rr(Process *list, int count, int quantum, int max_simulation_time, GanttLog *log) {
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
    int last_ready_checked_idx = -1; // Para busca RR na fila de prontos

    int segment_start_time = 0;
    int segment_pid = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) {
        total_idle_time = current_time;
        add_gantt_segment(log, -1, 0, current_time);
        segment_start_time = current_time;
    }

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {

        // Chamadas com (void) para evitar warnings
        (void)check_new_arrivals(local_list, count, current_time);
        (void)check_io_completions(local_list, count, current_time);

        // --- Seleção do Próximo Processo (se CPU livre) ---
        if (current_running_idx == -1) {
             int next_ready_idx = -1;
             int search_start_idx = (last_ready_checked_idx + 1);
             // Procura circularmente a partir do último verificado
             for (int i = 0; i < count; ++i) {
                 int check_idx = (search_start_idx + i) % count;
                 if (local_list[check_idx].state == STATE_READY) {
                     next_ready_idx = check_idx;
                     last_ready_checked_idx = check_idx; // Atualiza último encontrado
                     break;
                 }
             }

            // Se encontrou um processo pronto
            if (next_ready_idx != -1) {
                 // --- Fim do segmento anterior (Idle) ---
                 if (segment_pid == -1 && current_time > segment_start_time) {
                     add_gantt_segment(log, -1, segment_start_time, current_time);
                 }
                 segment_start_time = current_time;

                 current_running_idx = next_ready_idx;
                 Process *p = &local_list[current_running_idx];

                 // Context Switch
                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id && last_process_id != -1) {
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);

                     current_time += CONTEXT_SWITCH_COST;
                     total_context_switches++;
                     segment_start_time = current_time; // Início após CS

                     // Recheck durante CS
                     (void)check_new_arrivals(local_list, count, current_time);
                     (void)check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                     // Importante: Após CS, o mesmo processo ainda deve ser executado (não re-seleciona em RR aqui)
                 }

                 // Inicia execução
                 p->state = STATE_RUNNING;
                 p->time_slice_remaining = quantum;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id;
                 segment_pid = p->id;
                 // segment_start_time já está correto

                 printf("%-5d | P%d inicia/continua execução (Quantum: %d, Restante: %d)\n", current_time, p->id, quantum, p->remaining_time);
            }
        } // Fim da seleção (if current_running_idx == -1)


        // --- Execução do Processo Atual (se houver) ---
        if (current_running_idx != -1) {
            Process *p = &local_list[current_running_idx];

            // Verifica se pode executar neste instante
            if (max_simulation_time != -1 && current_time >= max_simulation_time) {
                // Simulação termina antes de executar
                add_gantt_segment(log, p->id, segment_start_time, current_time); // Loga o que correu até agora
                segment_pid = -1; // Assume que parou
                segment_start_time = current_time;

                p->state = STATE_READY; // Volta a pronto
                current_running_idx = -1;
                break; // Sai do loop
            }

            // Executa uma unidade de tempo
            current_time++;
            p->remaining_time--;
            p->time_slice_remaining--;
            printf("        P%d executa (R:%d, Q:%d)\n", p->id, p->remaining_time, p->time_slice_remaining);

            // Recheck arrivals/completions during execution tick
             (void)check_new_arrivals(local_list, count, current_time);
             (void)check_io_completions(local_list, count, current_time);


            // --- Verifica Fim da Execução ou Quantum ---
            int process_stopped = 0;
            if (p->remaining_time == 0) { // Processo terminou
                printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                add_gantt_segment(log, p->id, segment_start_time, current_time); // Loga segmento terminado
                 if (p->io_burst_duration > 0) {
                     printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                     p->state = STATE_BLOCKED;
                     p->io_completion_time = current_time + p->io_burst_duration;
                 } else {
                     p->state = STATE_TERMINATED;
                     p->finish_time = current_time;
                     completed_count++;
                 }
                process_stopped = 1;
            } else if (p->time_slice_remaining == 0) { // Quantum esgotou
                printf("%-5d | P%d fim do quantum, volta para READY\n", current_time, p->id);
                add_gantt_segment(log, p->id, segment_start_time, current_time); // Loga segmento do quantum
                // Verifica se vai para I/O no fim do quantum (comum em algumas variações)
                 if (p->io_burst_duration > 0 && (rand() % 3 == 0) ) { // Ex: 1/3 de chance de I/O no fim do quantum
                    printf("        P%d iniciando I/O (%d unidades) no fim do quantum\n", p->id, p->io_burst_duration);
                    p->state = STATE_BLOCKED;
                    p->io_completion_time = current_time + p->io_burst_duration;
                } else {
                    p->state = STATE_READY;
                    p->time_in_ready_queue = 0; // Reseta contador de aging ao voltar pra ready
                }
                process_stopped = 1;
            }
            // Poderia adicionar I/O no meio da execução aqui também, similar a preemptivo

            // Se o processo parou (terminou ou quantum)
            if (process_stopped) {
                current_running_idx = -1;
                segment_pid = -1; // CPU fica livre para próxima decisão
                segment_start_time = current_time; // Próximo segmento (idle ou processo) começa agora
            }
            // Se não parou, continua no próximo ciclo do while (o segmento Gantt continua)

        } else { // --- CPU OCIOSA ---
             // --- Garante que segmento anterior (se processo) foi logado ---
             // (Já tratado quando o processo para)

            // Calcula até quando ficar ocioso
            int next_event_time = INT_MAX;
            int has_ready_process = 0;
            for (int i = 0; i < count; i++) {
                 if(local_list[i].state == STATE_READY) has_ready_process = 1;
                 if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                 if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
            }

            // Se já há processo pronto, não fica ocioso, loop continua
            if(has_ready_process) continue;

             int idle_until;
             if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                   if (idle_until <= current_time) { break; }
                   printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
             } else {
                  idle_until = next_event_time;
                   printf("%-5d | CPU Ociosa até t=%d (Próximo evento)\n", current_time, idle_until);
             }

             if (idle_until > current_time) {
                 if (segment_pid != -1) { /* Log do processo anterior já feito */ }
                 // Log Idle segment
                 add_gantt_segment(log, -1, current_time, idle_until);
                 segment_start_time = idle_until;
                 segment_pid = -1;

                 total_idle_time += (idle_until - current_time);
                 current_time = idle_until;
             }
        } // Fim do if(current_running_idx != -1) / else (ocioso)

    } // Fim do while

    // --- Log do último segmento ---
    if (current_time > segment_start_time) {
       add_gantt_segment(log, segment_pid, segment_start_time, current_time);
    }

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}


// ------------------ Priority Scheduling ------------------
void schedule_priority(Process *list, int count, int preemptive, int enable_aging, int max_simulation_time, GanttLog *log) {
    printf("\n--- Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
    if (enable_aging && preemptive) printf("    (Aging Habilitado: Threshold=%d, Interval=%d)\n", AGING_THRESHOLD, AGING_INTERVAL);
    else if (preemptive) printf("    (Aging Desabilitado)\n");
    else printf("    (Aging N/A para Non-Preemptive)\n"); // Adicionado para clareza
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

    int segment_start_time = 0;
    int segment_pid = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) {
        total_idle_time = current_time;
        add_gantt_segment(log, -1, 0, current_time);
        segment_start_time = current_time;
    }
    last_aging_check = current_time;

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {

        // Chamadas com (void) para evitar warnings
        (void)check_new_arrivals(local_list, count, current_time);
        (void)check_io_completions(local_list, count, current_time);
        int applied_aging = 0;

        // Aplicar Aging periodicamente (apenas se preemptivo E habilitado)
        if (preemptive && enable_aging && (current_time >= last_aging_check + AGING_INTERVAL)) {
             apply_aging(local_list, count, current_time);
             last_aging_check = current_time;
             applied_aging = 1;
        }

        // Encontra processo pronto de maior prioridade (menor número)
        int highest_prio_idx = find_highest_priority_process_ready(local_list, count, current_time);
        int preemption_occurred = 0;

        // --- Decisão de Escalonamento ---
        if (highest_prio_idx != -1) { // Há processos prontos
            Process *next_p = &local_list[highest_prio_idx];

            if (current_running_idx == -1) { // CPU estava livre
                 // --- Fim do segmento anterior (Idle) ---
                 if (segment_pid == -1 && current_time > segment_start_time) {
                    add_gantt_segment(log, -1, segment_start_time, current_time);
                 }
                 segment_start_time = current_time;

                 current_running_idx = highest_prio_idx; Process *p = next_p;

                 // Context Switch (se necessário)
                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id && last_process_id != -1) {
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);
                     current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                     // Recheck events during CS
                     (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                     if (preemptive && enable_aging && (current_time >= last_aging_check + AGING_INTERVAL)){ apply_aging(local_list, count, current_time); last_aging_check = current_time; }
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                     // Re-escolher após CS? Não em prioridade, mantém o escolhido.
                 }

                 // Inicia execução
                 p->state = STATE_RUNNING; p->time_in_ready_queue = 0; // Reseta contador aging ao rodar
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id; segment_pid = p->id;
                 printf("%-5d | P%d (Prio: %d) inicia execução (R: %d)\n", current_time, p->id, p->current_priority, p->remaining_time);

            } else { // CPU Ocupada, verificar preempção
                 Process *running_p = &local_list[current_running_idx];
                 // Preempção ocorre se: modo preemptivo E (novo processo mais prioritário OU aging mudou prioridades e o 'novo' agora é mais prioritário)
                 if (preemptive && highest_prio_idx != current_running_idx &&
                     (next_p->current_priority < running_p->current_priority || (applied_aging && next_p->current_priority < running_p->current_priority)) )
                  {
                      preemption_occurred = 1;
                      printf("%-5d | PREEMPÇÃO: P%d (Prio: %d) preempta P%d (Prio: %d)\n",
                             current_time, next_p->id, next_p->current_priority, running_p->id, running_p->current_priority);

                      // --- Fim do segmento do processo preemptido ---
                      add_gantt_segment(log, running_p->id, segment_start_time, current_time);
                      segment_start_time = current_time; // Início do CS ou novo processo

                      // Trata processo preemptido (volta pra READY ou vai pra I/O)
                      if (running_p->io_burst_duration > 0 && (rand() % 5 == 0) ) { // Chance de ir pra I/O ao ser preemptido
                          printf("        P%d preemptido iniciando I/O (%d unidades)\n", running_p->id, running_p->io_burst_duration);
                          running_p->state = STATE_BLOCKED; running_p->io_completion_time = current_time + running_p->io_burst_duration;
                      } else {
                          running_p->state = STATE_READY; running_p->time_in_ready_queue = 0; // Reseta aging ao voltar pra ready
                      }

                      // Troca para o novo processo
                      current_running_idx = highest_prio_idx; Process *p = next_p;

                      // Context Switch
                      if (CONTEXT_SWITCH_COST > 0) {
                          printf("%-5d | Context Switch (P%d to P%d) - Custo: %d\n", current_time, running_p->id, p->id, CONTEXT_SWITCH_COST);
                          current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                          // Recheck events
                          (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                          if (preemptive && enable_aging && (current_time >= last_aging_check + AGING_INTERVAL)){ apply_aging(local_list, count, current_time); last_aging_check = current_time; }
                          if (max_simulation_time != -1 && current_time >= max_simulation_time) { running_p->state = STATE_READY; current_running_idx = -1; break; }
                          // Re-escolher após CS? Em prioridade, sim, pois outro pode ter chegado/mudado prio
                          highest_prio_idx = find_highest_priority_process_ready(local_list, count, current_time);
                          if (highest_prio_idx == -1) { current_running_idx = -1; segment_pid = -1; goto check_idle_prio; } // Virou idle
                          current_running_idx = highest_prio_idx; p = &local_list[current_running_idx]; // Pega o realmente mais prioritário agora
                      }

                      // Inicia execução do novo processo
                      p->state = STATE_RUNNING; p->time_in_ready_queue = 0;
                      if (p->start_time == -1) p->start_time = current_time;
                      last_process_id = p->id; segment_pid = p->id;
                      printf("%-5d | P%d (Prio: %d) inicia execução PREEMPTIVA (R: %d)\n", current_time, p->id, p->current_priority, p->remaining_time);

                 }
                 // Se não houve preempção, processo atual continua
            }
        } else { // Nenhum processo pronto
            // Se a CPU estava ocupada, o processo terminou ou bloqueou no passo anterior
            if (current_running_idx != -1) {
                 // O log do segmento desse processo deve ter sido feito ao parar
                 last_process_id = local_list[current_running_idx].id; // Guarda ID do último que rodou
                 current_running_idx = -1;
                 segment_pid = -1; // Marca CPU como livre
                 segment_start_time = current_time; // Idle começa agora
            }
            // Se já estava livre (segment_pid == -1), não faz nada aqui, vai pro check_idle
        }

        // --- Executa 1 unidade de tempo se CPU ocupada ---
        if (current_running_idx != -1 && !preemption_occurred) {
             Process *p = &local_list[current_running_idx];

             if (max_simulation_time != -1 && current_time >= max_simulation_time) {
                 add_gantt_segment(log, p->id, segment_start_time, current_time); // Loga o que correu
                 p->state = STATE_READY; current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
                 break;
             }

             // Executa 1 tick
             current_time++; p->remaining_time--;
             printf("        P%d executa (R:%d)\n", p->id, p->remaining_time);

              // Recheck arrivals/completions during execution tick
             (void)check_new_arrivals(local_list, count, current_time);
             (void)check_io_completions(local_list, count, current_time);


             // --- Verifica se terminou ou se bloqueia (em preemptivo) ---
             int process_stopped = 0;
             if (p->remaining_time == 0) { // Terminou
                 printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                 if (p->io_burst_duration > 0) {
                     printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                     p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                 } else {
                     p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                 }
                 process_stopped = 1;
             } else if (preemptive) { // Apenas em preemptivo pode bloquear no meio
                 // Chance de I/O (modelo simplificado)
                 if (p->io_burst_duration > 0 && p->burst_time > 1 && (rand() % (p->burst_time * 2) < 1) ) { // Chance menor de I/O no meio
                       printf("%-5d | P%d iniciando I/O (%d unidades) durante execução\n", current_time, p->id, p->io_burst_duration);
                       add_gantt_segment(log, p->id, segment_start_time, current_time);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                       process_stopped = 1;
                 }
             }

             if(process_stopped) {
                last_process_id = p->id; // Guarda quem parou
                current_running_idx = -1;
                segment_pid = -1; // CPU livre
                segment_start_time = current_time; // Próximo segmento começa aqui
             }
             // Se não parou, o segmento continua

        } else if (current_running_idx == -1) { // --- CPU OCIOSA ---
        check_idle_prio:
             // --- Garante que segmento anterior (se processo) foi logado ---
             // (Já tratado quando o processo para)

             int next_event_time = INT_MAX;
             int has_ready_process = 0;
             for (int i = 0; i < count; i++) {
                 if(local_list[i].state == STATE_READY) has_ready_process = 1;
                 if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                 if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
             }

             if(has_ready_process) continue; // Se ficou pronto, não fica idle

              int idle_until;
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                  if (idle_until <= current_time) { break; }
                  printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
              } else {
                   idle_until = next_event_time;
                   printf("%-5d | CPU Ociosa até t=%d (Próximo evento)\n", current_time, idle_until);
              }

              if (idle_until > current_time) {
                   if (segment_pid != -1) { /* Log anterior já feito */ }
                   add_gantt_segment(log, -1, current_time, idle_until);
                   segment_start_time = idle_until;
                   segment_pid = -1;
                   total_idle_time += (idle_until - current_time);
                   current_time = idle_until;
              }
        } // Fim do if (execução) / else (idle)

    } // Fim do while

    // --- Log do último segmento ---
    if (current_time > segment_start_time) {
       add_gantt_segment(log, segment_pid, segment_start_time, current_time);
    }

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}

// ---------------------- SJF (Non-Preemptive) ----------------------
void schedule_sjf(Process *list, int count, int max_simulation_time, GanttLog *log) {
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
    // Não precisa ordenar a lista principal para SJF NP

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;

    int segment_start_time = 0;
    int segment_pid = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) {
        total_idle_time = current_time;
        add_gantt_segment(log, -1, 0, current_time);
        segment_start_time = current_time;
    }

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

     while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
         (void)check_new_arrivals(local_list, count, current_time);
         (void)check_io_completions(local_list, count, current_time);

         // Se CPU livre, escolhe o PRONTO com menor burst_time
         if (current_running_idx == -1) {
             int shortest_idx = find_shortest_job_process_ready(local_list, count, current_time);

             if (shortest_idx != -1) { // Encontrou um processo pronto
                 // --- Fim do segmento Idle ---
                 if (segment_pid == -1 && current_time > segment_start_time) {
                    add_gantt_segment(log, -1, segment_start_time, current_time);
                 }
                 segment_start_time = current_time;

                 current_running_idx = shortest_idx;
                 Process *p = &local_list[current_running_idx];

                 // Context Switch
                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id && last_process_id != -1) {
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);
                     current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                     // Recheck events
                     (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                 }

                 // Inicia Execução
                 p->state = STATE_RUNNING;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id; segment_pid = p->id;
                 printf("%-5d | P%d (Burst: %d) inicia execução (R: %d)\n", current_time, p->id, p->burst_time, p->remaining_time);

                 // --- Simula toda a execução (NP) ---
                 int time_to_execute = p->remaining_time;
                 int execution_end_time = current_time + time_to_execute;
                 int time_limit_reached = 0;

                 if (max_simulation_time != -1 && execution_end_time > max_simulation_time) {
                     execution_end_time = max_simulation_time;
                     time_to_execute = max_simulation_time - current_time;
                     time_limit_reached = 1;
                     printf("        Execução de P%d limitada a %d unidades pelo T Max\n", p->id, time_to_execute);
                 }

                 if (time_to_execute <= 0 && time_limit_reached) {
                     add_gantt_segment(log, p->id, segment_start_time, current_time);
                     segment_pid = -1; segment_start_time = current_time;
                     p->state = STATE_READY; current_running_idx = -1;
                     break;
                 }

                 // Avança o tempo, checando eventos no caminho
                 int exec_step = 0;
                 while(exec_step < time_to_execute) {
                      current_time++; exec_step++; p->remaining_time--;
                      (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                 }

                 // --- Fim do Segmento ---
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                 segment_start_time = current_time; segment_pid = -1;

                 // Verifica resultado
                 if (p->remaining_time == 0) {
                     printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                      if (p->io_burst_duration > 0) {
                           printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                           p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                      } else {
                           p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                      }
                 } else if (time_limit_reached) {
                      printf("%-5d | Simulação INTERROMPIDA (T Max) enquanto P%d executava.\n", current_time, p->id);
                      p->state = STATE_READY;
                 } else {
                     fprintf(stderr, "Erro lógico em SJF P%d\n", p->id);
                     p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                 }
                 current_running_idx = -1; // Libera CPU

             } else { // Nenhum pronto, vai para idle
                 goto check_idle_sjf;
             }
         }
         // Se CPU ocupada (current_running_idx != -1), não faz nada em SJF NP até terminar

         if (current_running_idx == -1) { // Só entra aqui se CPU ficou livre ou estava livre e não achou pronto
         check_idle_sjf:
             // --- Log segmento anterior (processo) já feito ---
             int next_event_time = INT_MAX;
             int has_ready_process = 0;
             for (int i = 0; i < count; i++) {
                 if(local_list[i].state == STATE_READY) has_ready_process = 1;
                 if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                 if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
             }

             if(has_ready_process) continue; // Se ficou pronto, tenta escalonar

             int idle_until;
             if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                 idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                 if (idle_until <= current_time) { break; }
                 printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
             } else {
                 idle_until = next_event_time;
                 printf("%-5d | CPU Ociosa até t=%d (Próximo evento)\n", current_time, idle_until);
             }

             if (idle_until > current_time) {
                 if (segment_pid != -1) { /* Log anterior já feito */ }
                 add_gantt_segment(log, -1, current_time, idle_until);
                 segment_start_time = idle_until; segment_pid = -1;
                 total_idle_time += (idle_until - current_time);
                 current_time = idle_until;
             }
         }
     } // Fim do while

    // --- Log do último segmento ---
    if (current_time > segment_start_time) {
       add_gantt_segment(log, segment_pid, segment_start_time, current_time);
    }

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}


// ---------------------- EDF (Preemptive) ----------------------
// Implementação similar a Priority Preemptive, mas critério é Deadline
void schedule_edf_preemptive(Process *list, int count, int max_simulation_time, GanttLog *log) {
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
    // Não precisa ordenar lista principal

    int current_time = 0;
    int completed_count = 0;
    int total_idle_time = 0;
    int total_context_switches = 0;
    int current_running_idx = -1;
    int last_process_id = -1;

    int segment_start_time = 0;
    int segment_pid = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) {
        total_idle_time = current_time;
        add_gantt_segment(log, -1, 0, current_time);
        segment_start_time = current_time;
    }

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

     while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        // Chamadas com (void) para evitar warnings
        (void)check_new_arrivals(local_list, count, current_time);
        (void)check_io_completions(local_list, count, current_time);

        // Encontra processo pronto com menor deadline
        int earliest_deadline_idx = find_earliest_deadline_process_ready(local_list, count, current_time);
        int preemption_occurred = 0;

        // --- Decisão de Escalonamento ---
        if (earliest_deadline_idx != -1) { // Há processos prontos
            Process *next_p = &local_list[earliest_deadline_idx];

            if (current_running_idx == -1) { // CPU estava livre
                 // --- Fim do segmento Idle ---
                 if (segment_pid == -1 && current_time > segment_start_time) {
                     add_gantt_segment(log, -1, segment_start_time, current_time);
                 }
                 segment_start_time = current_time;

                 current_running_idx = earliest_deadline_idx; Process *p = next_p;

                 // Context Switch
                 if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id && last_process_id != -1) {
                      char from_str[10];
                      if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                      printf("%-5d | Context Switch (%s to P%d) - Custo: %d\n", current_time, from_str, p->id, CONTEXT_SWITCH_COST);
                      current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                      // Recheck events
                      (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                      if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                      // Re-escolher após CS? Sim, pois outro com deadline menor pode ter chegado
                      earliest_deadline_idx = find_earliest_deadline_process_ready(local_list, count, current_time);
                      if(earliest_deadline_idx == -1) { current_running_idx = -1; segment_pid = -1; goto check_idle_edf;}
                      current_running_idx = earliest_deadline_idx; p = &local_list[current_running_idx];
                 }

                 // Inicia execução
                 p->state = STATE_RUNNING;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id; segment_pid = p->id;
                 printf("%-5d | P%d (Deadl: %d) inicia execução (R: %d)\n", current_time, p->id, p->deadline, p->remaining_time);

            } else { // CPU Ocupada, verificar preempção por deadline
                 Process *running_p = &local_list[current_running_idx];
                 // Preempção se novo processo tem deadline estritamente menor, ou igual com chegada antes
                 if (earliest_deadline_idx != current_running_idx &&
                     (next_p->deadline < running_p->deadline || (next_p->deadline == running_p->deadline && next_p->arrival_time < running_p->arrival_time)))
                 {
                     preemption_occurred = 1;
                     printf("%-5d | PREEMPÇÃO EDF: P%d (D:%d) preempta P%d (D:%d)\n",
                             current_time, next_p->id, next_p->deadline, running_p->id, running_p->deadline);

                     // --- Fim do segmento preemptido ---
                     add_gantt_segment(log, running_p->id, segment_start_time, current_time);
                     segment_start_time = current_time;

                     // Trata processo preemptido
                      if (running_p->io_burst_duration > 0 && (rand() % 5 == 0)) {
                          printf("        P%d preemptido iniciando I/O (%d unidades)\n", running_p->id, running_p->io_burst_duration);
                          running_p->state = STATE_BLOCKED; running_p->io_completion_time = current_time + running_p->io_burst_duration;
                      } else { running_p->state = STATE_READY; }

                      // Troca para o novo processo
                      current_running_idx = earliest_deadline_idx; Process *p = next_p;

                      // Context Switch
                      if (CONTEXT_SWITCH_COST > 0) {
                         printf("%-5d | Context Switch (P%d to P%d) - Custo: %d\n", current_time, running_p->id, p->id, CONTEXT_SWITCH_COST);
                         current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                         // Recheck events
                         (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                         if (max_simulation_time != -1 && current_time >= max_simulation_time) { running_p->state = STATE_READY; current_running_idx = -1; break; }
                         // Re-escolher após CS
                         earliest_deadline_idx = find_earliest_deadline_process_ready(local_list, count, current_time);
                         if (earliest_deadline_idx == -1) { current_running_idx = -1; segment_pid = -1; goto check_idle_edf; }
                         current_running_idx = earliest_deadline_idx; p = &local_list[current_running_idx];
                      }

                      // Inicia execução do novo
                      p->state = STATE_RUNNING;
                      if (p->start_time == -1) p->start_time = current_time;
                      last_process_id = p->id; segment_pid = p->id;
                      printf("%-5d | P%d (Deadl: %d) inicia execução PREEMPTIVA (R: %d)\n", current_time, p->id, p->deadline, p->remaining_time);
                 }
                 // Se não houve preempção, continua
            }
        } else { // Nenhum processo pronto
             if (current_running_idx != -1) {
                 // Processo que estava rodando terminou ou bloqueou no passo anterior
                 last_process_id = local_list[current_running_idx].id;
                 current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
             }
        }

        // --- Executa 1 unidade de tempo se CPU ocupada ---
        if (current_running_idx != -1 && !preemption_occurred) {
             Process *p = &local_list[current_running_idx];
             if (max_simulation_time != -1 && current_time >= max_simulation_time) {
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                 p->state = STATE_READY; current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
                 break;
             }

             current_time++; p->remaining_time--;
             printf("        P%d executa (R:%d)\n", p->id, p->remaining_time);

             // Recheck arrivals/completions during execution tick
             (void)check_new_arrivals(local_list, count, current_time);
             (void)check_io_completions(local_list, count, current_time);

             // --- Verifica se terminou ou bloqueia ---
             int process_stopped = 0;
             if (p->remaining_time == 0) {
                 printf("%-5d | P%d TERMINOU\n", current_time, p->id);
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                  if (p->io_burst_duration > 0) {
                       printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                  } else {
                       p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++;
                  }
                 process_stopped = 1;
             } else {
                  // Chance de I/O no meio
                  if (p->io_burst_duration > 0 && p->burst_time > 1 && (rand() % (p->burst_time * 2) < 1)) {
                       printf("%-5d | P%d iniciando I/O (%d unidades) durante execução\n", current_time, p->id, p->io_burst_duration);
                       add_gantt_segment(log, p->id, segment_start_time, current_time);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                       process_stopped = 1;
                 }
             }

             if(process_stopped) {
                last_process_id = p->id;
                current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
             }
        } else if (current_running_idx == -1) { // --- CPU OCIOSA ---
        check_idle_edf:
             // --- Log segmento anterior (processo) já feito ---
              int next_event_time = INT_MAX;
              int has_ready_process = 0;
              for(int i=0; i<count; i++) {
                  if(local_list[i].state == STATE_READY) has_ready_process = 1;
                  if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                  if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
              }

              if(has_ready_process) continue;

              int idle_until;
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                  if(idle_until <= current_time) { break; }
                  printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
              } else {
                  idle_until = next_event_time;
                  printf("%-5d | CPU Ociosa até t=%d (Próximo evento)\n", current_time, idle_until);
              }

              if(idle_until > current_time) {
                  if (segment_pid != -1) { /* Log anterior já feito */ }
                  add_gantt_segment(log, -1, current_time, idle_until);
                  segment_start_time = idle_until; segment_pid = -1;
                  total_idle_time += (idle_until - current_time);
                  current_time = idle_until;
              }
        } // Fim if (execução) / else (idle)
     } // Fim do while

    // --- Log do último segmento ---
    if (current_time > segment_start_time) {
       add_gantt_segment(log, segment_pid, segment_start_time, current_time);
    }

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}

// ---------------------- RM (Preemptive - baseado em Prioridade) ----------------------
void schedule_rm_preemptive(Process *list, int count, int max_simulation_time, GanttLog *log) {
    printf("\n--- RM (Rate Monotonic - Preemptive, baseado em Prioridade Estática) ---\n");
    printf("    (Assume que 'priority' reflete a prioridade RM: 1=max, menor período=maior prio)\n");
    printf("    (Aging Desabilitado por padrão para RM)\n");

     // Chama a função de prioridade preemptiva SEM aging
     schedule_priority(list, count, 1 /* preemptive */, 0 /* enable_aging */, max_simulation_time, log);

     // Restaura valor original do aging threshold se foi modificado
     // (Embora neste código não estejamos usando #undef/#define dentro da função)
     // #ifdef original_aging_threshold
     // #define AGING_THRESHOLD original_aging_threshold
     // #endif
}


// ---------------------- MLQ (Multilevel Queue) ----------------------
// Filas: Q0 (Prio 1-2, RR q=B), Q1 (Prio 3-4, RR q=2B), Q2 (Prio 5+, FCFS)
void schedule_mlq(Process *list, int count, int base_quantum, int max_simulation_time, GanttLog *log) {
     printf("\n--- MLQ (Multilevel Queue) ---\n");
     printf("    Q0 (Prio 1,2): RR (q=%d)\n", base_quantum);
     printf("    Q1 (Prio 3,4): RR (q=%d)\n", base_quantum * 2);
     printf("    Q2 (Prio 5+):  FCFS\n");
     printf("    (Preempção entre filas: Q0 > Q1 > Q2)\n");
     if (max_simulation_time != -1) printf("Tempo Máximo de Simulação: %d\n", max_simulation_time);
     printf("Custo Troca de Contexto: %d\n", CONTEXT_SWITCH_COST);
     if (count <= 0 || base_quantum <=0) return;

    Process *local_list = malloc(count * sizeof(Process));
    if (!local_list) { return; }
    for(int i=0; i<count; i++) {
        local_list[i] = list[i];
        initialize_process_state(&local_list[i]);
        // Atribui fila inicial baseada na prioridade ORIGINAL
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
    // Índices para busca RR dentro das filas Q0 e Q1
    int last_checked_q0 = -1;
    int last_checked_q1 = -1;

    int segment_start_time = 0;
    int segment_pid = -1;

    current_time = find_min_arrival_time(local_list, count);
    if (current_time > 0) {
        total_idle_time = current_time;
        add_gantt_segment(log, -1, 0, current_time);
        segment_start_time = current_time;
    }

    printf("\nTempo | Evento\n");
    printf("------------------------------------------\n");

    while (completed_count < count && (max_simulation_time == -1 || current_time < max_simulation_time)) {
        // Chamadas com (void) para evitar warnings
        (void)check_new_arrivals(local_list, count, current_time);
        (void)check_io_completions(local_list, count, current_time);

        // --- Seleciona o próximo processo ---
        // Procura primeiro em Q0, depois Q1, depois Q2
        int candidate_idx = -1;
        int candidate_queue = -1;
        int current_quantum = 0;

        // Procura em Q0 (RR)
        int search_start_q0 = (last_checked_q0 + 1);
        for (int i = 0; i < count; ++i) {
            int idx = (search_start_q0 + i) % count;
            if (local_list[idx].state == STATE_READY && local_list[idx].current_queue == 0) {
                candidate_idx = idx; candidate_queue = 0; current_quantum = base_quantum; last_checked_q0 = idx;
                goto process_selected_mlq; // Achou em Q0, vai executar
            }
        }
        // Se não achou em Q0, procura em Q1 (RR)
         int search_start_q1 = (last_checked_q1 + 1);
         for (int i = 0; i < count; ++i) {
            int idx = (search_start_q1 + i) % count;
             if (local_list[idx].state == STATE_READY && local_list[idx].current_queue == 1) {
                candidate_idx = idx; candidate_queue = 1; current_quantum = base_quantum * 2; last_checked_q1 = idx;
                goto process_selected_mlq; // Achou em Q1
            }
        }
        // Se não achou em Q1, procura em Q2 (FCFS)
         int fcfs_idx = -1; int min_arrival_q2 = INT_MAX;
         for (int i = 0; i < count; ++i) {
              if (local_list[i].state == STATE_READY && local_list[i].current_queue == 2) {
                  // FCFS dentro da Q2
                  if (local_list[i].arrival_time < min_arrival_q2) {
                       min_arrival_q2 = local_list[i].arrival_time;
                       fcfs_idx = i;
                  }
              }
         }
         if (fcfs_idx != -1) {
             candidate_idx = fcfs_idx; candidate_queue = 2;
             // Quantum para FCFS é o tempo restante
             current_quantum = local_list[candidate_idx].remaining_time;
             goto process_selected_mlq; // Achou em Q2
         }

    process_selected_mlq:; // Label para onde saltar após achar um candidato

        int preemption_occurred = 0;

        // --- Decide se executa/preempta ---
        if (candidate_idx != -1) { // Encontrou algum processo pronto
             Process *next_p = &local_list[candidate_idx];

             if (current_running_idx == -1) { // CPU Livre
                 // --- Fim do segmento Idle ---
                 if (segment_pid == -1 && current_time > segment_start_time) {
                     add_gantt_segment(log, -1, segment_start_time, current_time);
                 }
                 segment_start_time = current_time;

                 current_running_idx = candidate_idx; Process *p = next_p;

                 // Context Switch
                  if (CONTEXT_SWITCH_COST > 0 && last_process_id != p->id && last_process_id != -1) {
                     char from_str[10];
                     if (last_process_id == -1) strcpy(from_str, "Idle"); else snprintf(from_str, sizeof(from_str), "P%d", last_process_id);
                     printf("%-5d | Context Switch (%s to P%d [Q%d]) - Custo: %d\n", current_time, from_str, p->id, candidate_queue, CONTEXT_SWITCH_COST);
                     current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                     // Recheck events
                     (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                     if (max_simulation_time != -1 && current_time >= max_simulation_time) break;
                     // Recalcula candidato após CS? Sim, pode ter chegado um de fila superior
                     continue; // Recomeça o loop while para re-selecionar o melhor candidato AGORA
                  }

                 // Inicia execução
                 p->state = STATE_RUNNING; p->time_slice_remaining = current_quantum;
                 if (p->start_time == -1) p->start_time = current_time;
                 last_process_id = p->id; segment_pid = p->id;
                 printf("%-5d | P%d [Q%d] inicia execução (Qtm: %d, R: %d)\n", current_time, p->id, candidate_queue, current_quantum, p->remaining_time);

             } else { // CPU Ocupada, verifica preempção entre filas
                  Process *running_p = &local_list[current_running_idx];
                  // Preempção se o candidato é de uma fila SUPERIOR (menor índice) à do processo atual
                  if (candidate_queue < running_p->current_queue) {
                       preemption_occurred = 1;
                       printf("%-5d | PREEMPÇÃO MLQ: P%d [Q%d] preempta P%d [Q%d]\n",
                             current_time, next_p->id, candidate_queue, running_p->id, running_p->current_queue);

                       // --- Fim do segmento preemptido ---
                       add_gantt_segment(log, running_p->id, segment_start_time, current_time);
                       segment_start_time = current_time;

                       // Trata processo preemptido
                       if (running_p->io_burst_duration > 0 && (rand() % 5 == 0)) {
                           printf("        P%d preemptido iniciando I/O (%d unidades)\n", running_p->id, running_p->io_burst_duration);
                           running_p->state = STATE_BLOCKED; running_p->io_completion_time = current_time + running_p->io_burst_duration;
                       } else { running_p->state = STATE_READY; }

                       // Troca para o novo
                       current_running_idx = candidate_idx; Process *p = next_p;

                       // Context Switch
                       if (CONTEXT_SWITCH_COST > 0) {
                           printf("%-5d | Context Switch (P%d to P%d [Q%d]) - Custo: %d\n", current_time, running_p->id, p->id, candidate_queue, CONTEXT_SWITCH_COST);
                           current_time += CONTEXT_SWITCH_COST; total_context_switches++; segment_start_time = current_time;
                           // Recheck events
                           (void)check_new_arrivals(local_list, count, current_time); (void)check_io_completions(local_list, count, current_time);
                           if (max_simulation_time != -1 && current_time >= max_simulation_time) { running_p->state = STATE_READY; current_running_idx = -1; break; }
                           // Recalcula candidato após CS
                           continue; // Recomeça o loop while
                       }

                       // Inicia execução do novo
                       p->state = STATE_RUNNING; p->time_slice_remaining = current_quantum;
                       if (p->start_time == -1) p->start_time = current_time;
                       last_process_id = p->id; segment_pid = p->id;
                       printf("%-5d | P%d [Q%d] inicia PREEMPTIVA (Qtm: %d, R: %d)\n", current_time, p->id, candidate_queue, current_quantum, p->remaining_time);
                  }
                  // Se não houve preempção, continua executando o atual
             }
        } else { // Nenhum processo pronto em nenhuma fila
             if (current_running_idx != -1) {
                 // Processo rodando terminou ou bloqueou no passo anterior
                 last_process_id = local_list[current_running_idx].id;
                 current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
             }
        }

        // --- Executa 1 unidade de tempo se CPU ocupada ---
        if (current_running_idx != -1 && !preemption_occurred) {
            Process *p = &local_list[current_running_idx];
            if (max_simulation_time != -1 && current_time >= max_simulation_time) {
                add_gantt_segment(log, p->id, segment_start_time, current_time);
                p->state = STATE_READY; current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
                break;
            }

            current_time++; p->remaining_time--;
            // Decrementa quantum apenas se não for FCFS (Q2)
            if (p->current_queue < 2) {
                p->time_slice_remaining--;
            }
            printf("        P%d [Q%d] executa (R:%d, Q:%d)\n", p->id, p->current_queue, p->remaining_time, p->time_slice_remaining);

             // Recheck arrivals/completions during execution tick
             (void)check_new_arrivals(local_list, count, current_time);
             (void)check_io_completions(local_list, count, current_time);


            // --- Verifica se terminou ou fim do quantum (para Q0/Q1) ou I/O ---
            int process_stopped = 0;
            if (p->remaining_time == 0) { // Terminou
                 printf("%-5d | P%d [Q%d] TERMINOU\n", current_time, p->id, p->current_queue);
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                  if (p->io_burst_duration > 0) {
                       printf("        P%d iniciando I/O (%d unidades) apos termino\n", p->id, p->io_burst_duration);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                  } else { p->state = STATE_TERMINATED; p->finish_time = current_time; completed_count++; }
                 process_stopped = 1;
            } else if (p->current_queue < 2 && p->time_slice_remaining == 0) { // Fim quantum Q0/Q1
                 printf("%-5d | P%d [Q%d] fim do quantum, volta para READY\n", current_time, p->id, p->current_queue);
                 add_gantt_segment(log, p->id, segment_start_time, current_time);
                 // Chance de I/O no fim do quantum
                 if (p->io_burst_duration > 0 && (rand() % 3 == 0)) {
                      printf("        P%d iniciando I/O (%d unidades) no fim do quantum\n", p->id, p->io_burst_duration);
                      p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                 } else { p->state = STATE_READY; }
                 process_stopped = 1;
            } else { // Pode ir para I/O no meio (chance baixa)
                 if (p->io_burst_duration > 0 && p->burst_time > 1 && (rand() % (p->burst_time * 3) < 1)) {
                       printf("%-5d | P%d [Q%d] iniciando I/O (%d unidades) durante execução\n", current_time, p->id, p->current_queue, p->io_burst_duration);
                       add_gantt_segment(log, p->id, segment_start_time, current_time);
                       p->state = STATE_BLOCKED; p->io_completion_time = current_time + p->io_burst_duration;
                       process_stopped = 1;
                 }
            }

            if(process_stopped) {
               last_process_id = p->id;
               current_running_idx = -1; segment_pid = -1; segment_start_time = current_time;
            }

        } else if (current_running_idx == -1) { // --- CPU OCIOSA ---
             // --- Log segmento anterior (processo) já feito ---
             int next_event_time = INT_MAX;
             int has_ready_process = 0;
              for(int i=0; i<count; i++) {
                  if(local_list[i].state == STATE_READY) has_ready_process = 1;
                  if (local_list[i].state == STATE_NEW && local_list[i].arrival_time < next_event_time) next_event_time = local_list[i].arrival_time;
                  if (local_list[i].state == STATE_BLOCKED && local_list[i].io_completion_time < next_event_time) next_event_time = local_list[i].io_completion_time;
              }

              if(has_ready_process) continue;

              int idle_until;
              if (next_event_time == INT_MAX || (max_simulation_time != -1 && next_event_time >= max_simulation_time)) {
                  idle_until = (max_simulation_time != -1) ? max_simulation_time : current_time;
                  if(idle_until <= current_time) { break; }
                  printf("%-5d | CPU Ociosa até %d (Fim da Simulação)\n", current_time, idle_until);
              } else {
                  idle_until = next_event_time;
                  printf("%-5d | CPU Ociosa até t=%d (Próximo evento)\n", current_time, idle_until);
              }

              if(idle_until > current_time) {
                  if (segment_pid != -1) { /* Log anterior já feito */ }
                  add_gantt_segment(log, -1, current_time, idle_until);
                  segment_start_time = idle_until; segment_pid = -1;
                  total_idle_time += (idle_until - current_time);
                  current_time = idle_until;
              }
        } // Fim if (execução) / else (idle)
    } // Fim do while

    // --- Log do último segmento ---
    if (current_time > segment_start_time) {
       add_gantt_segment(log, segment_pid, segment_start_time, current_time);
    }

    printf("------------------------------------------\n");
    calculate_final_metrics(local_list, count, current_time, total_idle_time, total_context_switches);
    free(local_list);
}