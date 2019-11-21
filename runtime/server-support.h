#ifndef _SERVER_H
#define _SERVER_H

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>
#include "internal.h"
#include "elastic.h"
#include "container.h"
#include "platform-scheduler.h"
#include "macro-bench-support.h"

void * main_thread_new_program_receiver(void * arg);
void * main_thread_thread_container_trigger(void * arg);
void * main_thread_timed_scheduling(void * arg);
void init_timed_sleep_lock_and_cond();
void set_timer(struct itimerval * itv, struct itimerval * oldtv, int sec, int usec);
void pipe_sig_handler(int s);
void timed_scheduling_signal_handler(int n);
void platform_response_to_client(platform_program * p);
void container_plugin_enable_run_cycle(__cilkrts_worker * w);

struct platform_program_request * platform_init_program_request_head(struct platform_global_state * G);
struct platform_program_request * platform_init_program_request(struct platform_global_state * G, 
    int program_id, int input, int control_uid, int second_level_uid, int periodic, int mute, 
    unsigned long long max_period_s, unsigned long long max_period_us, unsigned long long min_period_s, unsigned long long min_period_us, 
    unsigned long long C_s, unsigned long long C_us, unsigned long long L_s, unsigned long long L_us, float elastic_coefficient);
void platform_append_program_request(platform_program_request * new_pr);
struct platform_program_request * platform_peek_first_request(struct platform_global_state * G, int control_uid);
struct platform_program_request * platform_pop_first_request(struct platform_global_state * G, int control_uid);
int platform_program_request_buffer_is_empty(struct platform_global_state * G, int control_uid);
void platform_deinit_program_request(platform_program_request * pr);
int get_count_program_request_buffer(struct platform_global_state * G, int control_uid);

#endif