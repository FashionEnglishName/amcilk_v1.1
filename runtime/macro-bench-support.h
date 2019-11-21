#ifndef _MACRO_BENCH_H
#define _MACRO_BENCH_H

#include <stdio.h> 

#include "internal.h"
#include "elastic.h"
#include "server-support.h"

void program_set_requested_time_ns(platform_program_request * pr);
void program_set_pick_container_time_ns(platform_program * p);
void program_set_picked_container_time_ns(platform_program * p);
void program_set_activate_container_time_ns(platform_program * p);
void program_set_core_assignment_time_when_run_ns(platform_program * p);
void program_set_platform_preemption_time_ns(platform_program * p);
void program_set_begin_time_ns(platform_program * p);
void program_set_end_time_ns(platform_program * p);
void program_set_pause_time_ns(platform_program * p);
void program_set_resume_time_ns(platform_program * p);
void program_set_begin_exit_time_ns(platform_program * p);
void program_set_sleeped_all_other_workers_time_ns(platform_program * p);

unsigned long long program_get_run_time_ns(platform_program * p);
 
void program_print_result(platform_program * p);
void program_print_result_acc(platform_program * p);
#endif