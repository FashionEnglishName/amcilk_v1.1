#ifndef _ELASTIC_H
#define _ELASTIC_H


#include <sys/time.h>
#include <signal.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <errno.h>

#include "internal.h"
#include "rts-config.h"
#include "mutex.h"
#include "cilk-scheduler.h"
#include "readydeque.h"
#include "jmpbuf.h"

void elastic_worker_request_cpu_to_sleep(__cilkrts_worker *w, int cpu_id);
void elastic_worker_request_cpu_to_recover(__cilkrts_worker *w, int cpu_id);
void elastic_do_exchange_state_group(__cilkrts_worker *worker_a, __cilkrts_worker *worker_b);
void elastic_activate_all_sleeping_workers(__cilkrts_worker *w);
void program_activate_all_sleeping_workers(platform_program * p);
void elastic_do_cond_sleep(__cilkrts_worker *w);
void elastic_do_cond_activate(__cilkrts_worker *w);
void elastic_do_cond_activate_for_exit(__cilkrts_worker *w);
bool elastic_safe(__cilkrts_worker *w);
void elastic_mugging(__cilkrts_worker *w, int victim);
void elastic_core_lock(__cilkrts_worker *w);
void elastic_core_unlock(__cilkrts_worker *w);
void elastic_core_lock_g(global_state *g);
void elastic_core_unlock_g(global_state *g);
int elastic_get_worker_id_sleeping_active_deque(__cilkrts_worker *w);

void platform_rts_srand(platform_global_state * G, unsigned int seed);
void print_cpu_state_group(platform_program * p);
void print_elastic_safe(platform_program* p);
void print_cpu_mask(platform_program * p);
void print_try_cpu_mask(platform_program * p);

void print_worker_deque(__cilkrts_worker *w);
void assert_num_ancestor(int assert_spawn_count, int assert_call_count, int assert_call_frame_count);

void platform_try_sleep_worker(platform_program * p, int cpu_id);
void platform_guarantee_sleep_worker(platform_program * p, int cpu_id);
void platform_guarantee_sleep_inactive_deque_worker(platform_program * p, int cpu_id);
void platform_guarantee_activate_worker(platform_program * p, int cpu_id);
void platform_guarantee_cancel_worker_sleep(platform_program * p, int cpu_id);
void platform_try_activate_worker(platform_program * p, int cpu_id);

void elastic_init_block_pipe(__cilkrts_worker *w);

#endif