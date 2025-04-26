#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

#define CONTEXT_SWITCH_COST 1
#define AGING_THRESHOLD 20
#define AGING_INTERVAL 10

void schedule_fcfs(Process *list, int count, int max_simulation_time);
void schedule_rr(Process *list, int count, int quantum, int max_simulation_time);
void schedule_priority(Process *list, int count, int preemptive, int enable_aging, int max_simulation_time);
void schedule_sjf(Process *list, int count, int max_simulation_time);
void schedule_edf_preemptive(Process *list, int count, int max_simulation_time);
void schedule_rm_preemptive(Process *list, int count, int max_simulation_time);
void schedule_mlq(Process *list, int count, int base_quantum, int max_simulation_time);

int find_min_arrival_time(Process *list, int count);
void initialize_process_state(Process *p);

#endif