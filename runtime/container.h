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

#include "elastic.h"
#include "platform-scheduler.h"
#include "server-support.h"
#include "macro-bench-support.h"
#include "micro-bench-support.h"
extern void container_set_by_request(platform_program * p, platform_program_request * pr);

void platform_activate_container(platform_program * new_p);
void platform_deactivate_container(platform_program * p);

void container_do_elastic_adaption(platform_program * p);
void container_set_init_run(platform_program * p);
void container_print_result(__cilkrts_worker * w);
void container_setup_to_run(platform_program * p, platform_program_request* pr);
void container_block(__cilkrts_worker * w);
platform_program * get_container(platform_global_state *G, int uid);
platform_program * get_container_by_control_uid(platform_global_state * G, int control_uid);

void platform_unset_container_cpu_map(struct platform_global_state * G);
void platform_set_container_cpu_map(platform_global_state * G);

void print_all_last_exit_cpu_id(platform_global_state * G);