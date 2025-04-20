#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

// Assinaturas atualizadas para incluir max_simulation_time (-1 indica sem limite)
void schedule_fcfs(Process *list, int count, int max_simulation_time);
void schedule_rr(Process *list, int count, int quantum, int max_simulation_time);
void schedule_priority(Process *list, int count, int preemptive, int max_simulation_time);
void schedule_sjf(Process *list, int count, int max_simulation_time);
void schedule_edf_preemptive(Process *list, int count, int max_simulation_time);
void schedule_rm_preemptive(Process *list, int count, int max_simulation_time);
void schedule_mlq(Process *list, int count, int max_simulation_time);

// Funções simplistas originais
void schedule_edf(Process *list, int count, int max_simulation_time);
void schedule_rm(Process *list, int count, int max_simulation_time);

#endif