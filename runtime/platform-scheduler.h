#ifndef _PLAT_SCHED_H
#define _PLAT_SCHED_H

#include <stdio.h>

#include "elastic.h"
#include "container.h"
#include "macro-bench-support.h"

#include "platform_sched_CFS.h"
#include "platform_sched_DREP.h"
#include "platform_sched_kunal_adapt.h"
#include "platform_sched_task_compress_par2.h"

void platform_timed_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type);
void platform_new_job_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type);
void platform_exit_job_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type);
void platform_scheduling(platform_global_state * G, platform_program * p, enum PLATFORM_SCHEDULER_TYPE run_type);
void platform_preemption(platform_global_state * G, platform_program * p, enum PLATFORM_SCHEDULER_TYPE run_type);

#endif