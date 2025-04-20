#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

// Algoritmos
void schedule_fcfs(Process *list, int count);
void schedule_rr(Process *list, int count, int quantum);
void schedule_priority(Process *list, int count, int preemptive);
void schedule_sjf(Process *list, int count);
void schedule_edf(Process *list, int count);
void schedule_rm(Process *list, int count);
void schedule_edf_preemptive(Process *list, int count);
void schedule_rm_preemptive(Process *list, int count);

#endif