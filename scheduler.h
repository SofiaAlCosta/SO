#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

// --- Parâmetros Configuráveis (Exemplos) ---
#define CONTEXT_SWITCH_COST 1
#define AGING_THRESHOLD 20 // Tempo em READY para envelhecer
#define AGING_INTERVAL 10 // Intervalo de tempo para verificar aging

// --- NOVO: Estados do Processo (movido para process.h) ---

// --- NOVO: Estruturas para Gráfico de Gantt ---
typedef struct {
    int process_id; // -1 para IDLE
    int start_time;
    int end_time;
} GanttSegment;

typedef struct {
    GanttSegment *segments;
    int count;
    int capacity;
} GanttLog;


// --- Funções de Escalonamento (Assinaturas Atualizadas) ---
void schedule_fcfs(Process *list, int count, int max_simulation_time, GanttLog *log);
void schedule_rr(Process *list, int count, int quantum, int max_simulation_time, GanttLog *log);
void schedule_priority(Process *list, int count, int preemptive, int enable_aging, int max_simulation_time, GanttLog *log);
void schedule_sjf(Process *list, int count, int max_simulation_time, GanttLog *log);
void schedule_edf_preemptive(Process *list, int count, int max_simulation_time, GanttLog *log);
void schedule_rm_preemptive(Process *list, int count, int max_simulation_time, GanttLog *log);
void schedule_mlq(Process *list, int count, int base_quantum, int max_simulation_time, GanttLog *log);

// --- Funções de Log Gantt ---
void write_gantt_data(const char* filename, GanttLog *log);
void init_gantt_log(GanttLog *log);
void add_gantt_segment(GanttLog *log, int pid, int start, int end);
void free_gantt_log(GanttLog *log);

// --- Funções Auxiliares (declaradas se necessário, mas definidas em scheduler.c) ---
int find_min_arrival_time(Process *list, int count);
void initialize_process_state(Process *p); // Já está em process.h/c

#endif // SCHEDULER_H