#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>

// ------------------------- Utilitários -------------------------

int compare_arrival(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->arrival_time - p2->arrival_time;
}

int compare_priority(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->priority - p2->priority;
}

int compare_deadline(const void *a, const void *b) {
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->deadline - p2->deadline;
}

//Tempo de início da simulaçãoo
int find_min_arrival_time(Process *list, int count) {
    if (count <= 0) return 0;
    int min_arrival = list[0].arrival_time;
    for (int i = 1; i < count; i++) {
        if (list[i].arrival_time < min_arrival) {
            min_arrival = list[i].arrival_time;
        }
    }
    return min_arrival;
}
// ---------------------- FCFS ----------------------

void schedule_fcfs(Process *list, int count) {
    int current_time = 0;
    int total_waiting = 0, total_turnaround = 0;

    qsort(list, count, sizeof(Process), compare_arrival);

    printf("\n--- FCFS ---\n");
    for (int i = 0; i < count; i++) {
        if (current_time < list[i].arrival_time)
            current_time = list[i].arrival_time;

        int start = current_time;
        int finish = start + list[i].burst_time;
        int turnaround = finish - list[i].arrival_time;
        int waiting = start - list[i].arrival_time;

        total_turnaround += turnaround;
        total_waiting += waiting;
        current_time = finish;

        printf("P%d Start=%d Finish=%d Wait=%d Turnaround=%d\n", list[i].id, start, finish, waiting, turnaround);
    }

    printf("Média Espera: %.2f | Média Turnaround: %.2f\n",
           (float)total_waiting / count, (float)total_turnaround / count);
}

// ---------------------- Round Robin ----------------------

void schedule_rr(Process *list, int count, int quantum) {
    printf("\n--- Round Robin (q = %d) ---\n", quantum);
    int current_time = 0, completed = 0;
    int *remaining = malloc(count * sizeof(int));
    int *arrival = malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        remaining[i] = list[i].burst_time;
        arrival[i] = list[i].arrival_time;
    }

    while (completed < count) {
        int done = 1;
        for (int i = 0; i < count; i++) {
            if (remaining[i] > 0 && arrival[i] <= current_time) {
                done = 0;
                int exec = (remaining[i] > quantum) ? quantum : remaining[i];
                printf("Tempo %d -> P%d executa %d unidades\n", current_time, list[i].id, exec);
                current_time += exec;
                remaining[i] -= exec;
                if (remaining[i] == 0) {
                    completed++;
                    int turnaround = current_time - list[i].arrival_time;
                    int waiting = turnaround - list[i].burst_time;
                    printf("P%d terminou. Waiting=%d Turnaround=%d\n", list[i].id, waiting, turnaround);
                }
            }
        }
        if (done) current_time++;  // Se nenhum processo estava pronto
    }

    free(remaining);
    free(arrival);
}

// ------------------ Priority Scheduling ------------------

void schedule_priority(Process *list, int count, int preemptive) {
    printf("\n--- Priority Scheduling (%s) ---\n", preemptive ? "Preemptive" : "Non-Preemptive");
    int current_time = 0, completed = 0;
    int *remaining = malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) remaining[i] = list[i].burst_time;

    while (completed < count) {
        int idx = -1;
        int min_priority = 1e9;

        for (int i = 0; i < count; i++) {
            if (list[i].arrival_time <= current_time && remaining[i] > 0) {
                if (list[i].priority < min_priority) {
                    min_priority = list[i].priority;
                    idx = i;
                }
            }
        }

        if (idx != -1) {
            int exec_time = preemptive ? 1 : remaining[idx];
            printf("Tempo %d -> P%d executa %d unidades\n", current_time, list[idx].id, exec_time);
            remaining[idx] -= exec_time;
            current_time += exec_time;
            if (remaining[idx] == 0) {
                completed++;
                int turnaround = current_time - list[idx].arrival_time;
                int waiting = turnaround - list[idx].burst_time;
                printf("P%d terminou. Waiting=%d Turnaround=%d\n", list[idx].id, waiting, turnaround);
            }
        } else {
            current_time++;
        }
    }

    free(remaining);
}

// ------------------------ EDF ------------------------

void schedule_edf(Process *list, int count) {
    printf("\n--- EDF (Earliest Deadline First) ---\n");
    qsort(list, count, sizeof(Process), compare_deadline);
    schedule_fcfs(list, count);  // EDF sem preempção = FCFS ordenado por deadline
}

// ------------------ Rate Monotonic ------------------

void schedule_rm(Process *list, int count) {
    printf("\n--- Rate Monotonic (Simples) ---\n");
    qsort(list, count, sizeof(Process), compare_priority); // RM assume menor período = maior prioridade
    schedule_fcfs(list, count);
}

// ---------------------- SJF (Sem preempção) ----------------------

void schedule_sjf(Process *list, int count) {
    printf("\n--- SJF (Shortest Job First - Non-Preemptive) ---\n");

    if (count <= 0) return;

    int current_time = 0;
    int completed = 0;
    int total_waiting = 0, total_turnaround = 0;
    int total_burst = 0;
    int last_finish_time = 0;
    int cpu_idle_time = 0;
    int initial_start_time = -1;

    int *finished = calloc(count, sizeof(int));
    for(int i=0; i<count; i++) {
        list[i].remaining_time = list[i].burst_time;
        total_burst += list[i].burst_time;
    }


    while (completed < count) {
        int shortest_idx = -1;
        int min_burst = INT_MAX;

        for (int i = 0; i < count; i++) {
            if (!finished[i] && list[i].arrival_time <= current_time) {
                if (list[i].burst_time < min_burst) {
                    min_burst = list[i].burst_time;
                    shortest_idx = i;
                }
                else if (list[i].burst_time == min_burst) {
                    if (shortest_idx == -1 || list[i].arrival_time < list[shortest_idx].arrival_time) {
                         shortest_idx = i;
                    }
                }
            }
        }

        if (shortest_idx != -1) {
            Process *p = &list[shortest_idx];

            if (p->arrival_time > current_time) {
                 cpu_idle_time += (p->arrival_time - current_time);
                 current_time = p->arrival_time;
            }

            if(initial_start_time == -1) initial_start_time = current_time;

            int start = current_time;
            int finish = start + p->burst_time;
            int turnaround = finish - p->arrival_time;
            int waiting = start - p->arrival_time;

            total_turnaround += turnaround;
            total_waiting += waiting;
            current_time = finish;
            last_finish_time = current_time;

            finished[shortest_idx] = 1;
            completed++;

            printf("P%d Start=%d Finish=%d Wait=%d Turnaround=%d (Burst=%d)\n",
                   p->id, start, finish, waiting, turnaround, p->burst_time);

        } else {
            int next_arrival = INT_MAX;
            for(int i=0; i<count; i++) {
                if (!finished[i] && list[i].arrival_time < next_arrival) {
                    next_arrival = list[i].arrival_time;
                }
            }
             if (next_arrival == INT_MAX) break;

            if (next_arrival > current_time) {
                cpu_idle_time += (next_arrival - current_time);
                current_time = next_arrival;
            } else {
                 current_time++;
                 cpu_idle_time++;
            }
        }
    }

    free(finished);

     printf("--- Métricas SJF ---\n");
     printf("Média Espera: %.2f | Média Turnaround: %.2f\n",
           (float)total_waiting / count, (float)total_turnaround / count);
}