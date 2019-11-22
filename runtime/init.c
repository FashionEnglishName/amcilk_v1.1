#define _GNU_SOURCE
#include <sched.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include "debug.h"
#include "fiber.h"
#include "readydeque.h"
#include "sched_stats.h"
#include "cilk-scheduler.h"
#include "container.h"
#include "elastic.h"

extern void cleanup_invoke_main(Closure *invoke_main);
extern int parse_command_line(struct rts_options *options, int *argc, char *argv[]);

extern int cilkg_nproc;

static platform_program *container_init_head(platform_global_state *G);

/* Linux only */
static int linux_get_num_proc() {
    const char *envstr = getenv("CILK_NWORKERS");
    if (envstr)
        return strtol(envstr, NULL, 0);
    return get_nprocs();
}

platform_global_state * platform_global_state_init(int argc, char* argv[]) {
    platform_global_state * G = (platform_global_state *) malloc(sizeof(platform_global_state));
    G->program_head = container_init_head(G);
    G->request_buffer_head = platform_init_program_request_head(G);
    G->new_program = NULL;
    G->stop_program = NULL;
    G->new_program_id = -1;
    G->new_input = 0;
    G->deprecated_program_head = container_init_head(G);
    G->deprecated_program_tail = G->deprecated_program_head;
    pthread_spin_init(&(G->request_buffer_lock), PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&(G->lock), NULL);
    //pthread_spin_init(&(G->lock), PTHREAD_PROCESS_PRIVATE);
    G->nproc = -1;
    int available_cores = 0;
    cpu_set_t process_mask;
    //get the mask from the parent thread (master thread)
    int err = pthread_getaffinity_np (pthread_self(), sizeof(process_mask), &process_mask);
    if (0 == err) {
        int j;
        //Get the number of available cores (copied from os-unix.c)
        available_cores = 0;
        for (j = 0; j < CPU_SETSIZE; j++){
            if (CPU_ISSET(j, &process_mask)){
                available_cores++;
            }
        }
    }
    const char *envstr = getenv("CILK_NWORKERS");
    G->nproc = (envstr ? linux_get_num_proc() : available_cores);
    G->platform_argc = argc;
    G->platform_argv = argv;

    int i=0;
    G->most_recent_stop_program_cpumask = (int*) malloc(sizeof(int)*G->nproc);
    for (i=0; i<G->nproc; i++) {
        G->most_recent_stop_program_cpumask[i] = 0;
    }

    G->cpu_container_map = (int*) malloc(sizeof(int)*G->nproc);
    for (i=0; i<G->nproc; i++) {
        G->cpu_container_map[i] = -1;
    }

    for (i=0; i<G->nproc; i++) {
        G->giveup_core_containers[i] = -1;
        G->receive_core_containers[i] = -1;
    }

    G->nprogram_running = 0;
    G->enable_timed_scheduling = 0;
    G->timed_interval_sec = 0;
    G->timed_interval_usec = 0;

    G->micro_time_test = (unsigned long long*) malloc(sizeof(unsigned long long)*100);
    G->micro_time_worker_activate_focus = 3;
    G->micro_time_worker_sleep_focus = 3;
    G->micro_test_to_run_input = -1;
    platform_rts_srand(G, 7*162347);
    G->macro_test_num_programs_executed = 0;
    G->macro_test_acc_pick_time = 0;
    G->macro_test_acc_activate_time = 0;
    G->macro_test_acc_core_assign_time = 0;
    G->macro_test_acc_preempt_time = 0;
    G->macro_test_acc_waiting_all_other_sleep = 0;
    G->macro_test_acc_run_time = 0;
    G->macro_test_acc_latency = 0;
    G->macro_test_acc_flow_time = 0;
    return G;
}

static void global_state_init(platform_program * p, int argc, char* argv[]) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (global_state_init) Initializing global state.\n");
    global_state * g = (global_state *) malloc(sizeof(global_state));

    if(parse_command_line(&g->options, &argc, argv)) {
        // user invoked --help; quit
        free(g);
        exit(0);
    }
    
    if(g->options.nproc == 0) {
        // use the number of cores online right now 
        int available_cores = 0;
        cpu_set_t process_mask;
            //get the mask from the parent thread (master thread)
        int err = pthread_getaffinity_np (pthread_self(), sizeof(process_mask), &process_mask);
        if (0 == err)
        {
            int j;
            //Get the number of available cores (copied from os-unix.c)
            available_cores = 0;
            for (j = 0; j < CPU_SETSIZE; j++){
                if (CPU_ISSET(j, &process_mask)){
                    available_cores++;
                }
            }
        }
        const char *envstr = getenv("CILK_NWORKERS");
        g->options.nproc = (envstr ? linux_get_num_proc() : available_cores);

    }
    cilkg_nproc = g->options.nproc;

    int active_size = g->options.nproc;
    g->invoke_main_initialized = 0;
    g->start = 0;
    g->done = 0;
    cilk_mutex_init(&(g->print_lock));

    g->workers = (__cilkrts_worker **) malloc(active_size * sizeof(__cilkrts_worker *));
    g->deques = (ReadyDeque **) malloc(active_size * sizeof(ReadyDeque *));
    g->threads = (pthread_t *) malloc(active_size * sizeof(pthread_t));
    g->elastic_core = (Elastic_core *) malloc(sizeof(Elastic_core));

    cilk_internal_malloc_global_init(g); // initialize internal malloc first
    cilk_fiber_pool_global_init(g);
    cilk_global_sched_stats_init(&(g->stats));

    g->cilk_main_argc = argc;
    g->cilk_main_args = argv;

    p->g = g;
    g->program = p;
}

void init_timed_scheduling(platform_global_state *G, int enbale, int interval_sec, int interval_usec) {
    G->enable_timed_scheduling = enbale;
    G->timed_interval_sec = interval_sec;
    G->timed_interval_usec = interval_usec;
}

platform_program *container_init(platform_global_state *G, int control_uid, int second_level_uid) {
    int i=0;
    platform_program * p = (platform_program *) malloc(sizeof(platform_program));
    p->request_buffer_head = platform_init_program_request_head(G);
    pthread_spin_init(&(p->request_buffer_lock), PTHREAD_PROCESS_PRIVATE);
    //managemenet
    p->G = G;
    p->g = NULL;
    p->next = NULL;
    p->prev = NULL;
    p->control_uid = control_uid;
    p->is_switching = 0;
    for(int i=0; i < JMPBUF_SIZE; i++) { p->container_exit_handling_ctx[i] = NULL; };
    p->invariant_running_worker_id = p->control_uid + 1; //workers: 2~N, containers: 1~N
    p->jump_to_exit_handler = 0;
    p->second_level_uid = second_level_uid;
    p->run_times = 0;
    p->requested_time = 0;
    p->begin_time = 0;
    p->end_time = 0;
    p->pause_time = 0;
    p->resume_time = 0;
    p->next_period_feedback_s = 1;
    p->next_period_feedback_us = 1;
    pthread_spin_init(&(p->stop_lock), PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&(p->run_lock), PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&(p->exit_lock), PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&(p->trigger_lock), PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&(p->scheduling_lock), PTHREAD_PROCESS_PRIVATE);
    p->cpu_mask = (int*) malloc(sizeof(int)*G->nproc);
    for (i=0; i<G->nproc; i++) {
        p->cpu_mask[i] = 0;
    }
    p->try_cpu_mask = (int*) malloc(sizeof(int)*G->nproc);
    for (i=0; i<G->nproc; i++) {
        p->try_cpu_mask[i] = 0;
    }
    p->num_cpu = 0;
    p->try_num_cpu = 0;

    p->t_s_dict_for_task_compress_par2 = (unsigned long long*) malloc(sizeof(unsigned long long)*(p->G->nproc));
    p->t_us_dict_for_task_compress_par2 = (unsigned long long*) malloc(sizeof(unsigned long long)*(p->G->nproc));
    return p;
}

void container_set_by_init(platform_program * p, int program_id, int input, int periodic, int mute, unsigned long long max_period_s, unsigned long long max_period_us, unsigned long long min_period_s, unsigned long long min_period_us, unsigned long long C_s, unsigned long long C_us, unsigned long long L_s, unsigned long long L_us, float elastic_coefficient) {
    int i = 0;
    //managemenet
    p->run_for_init_container = 1;
    p->self = program_id;
    p->input = input;
    p->mute = mute;
    p->periodic = periodic;
    p->done_one = 0;
    p->pickable = 0;
    p->hint_stop_container = 0;
    p->flag_enter_exit_routine = 0;
    p->job_finish = 0;
    p->last_exit_worker_entered_runtime = 0;
    p->last_do_exit_worker_id = -1;
    p->is_switching = 0;

    //periodic
    p->max_period_s = max_period_s;
    p->max_period_us = max_period_us;
    p->min_period_s = min_period_s;
    p->min_period_us = min_period_us;
    //real time
    p->C_s = C_s;
    p->C_us = C_us;
    p->L_s = L_s;
    p->L_us = L_us;
    p->elastic_coefficient = elastic_coefficient;
    if (C_s!=0 || C_us!=0) {
        //real time
        p->utilization_max = 1.0*(p->C_s*1000*1000.0 + p->C_us)/(p->min_period_s*1000*1000.0 + p->min_period_us);
        p->utilization_min = 1.0*(p->C_s*1000*1000.0 + p->C_us)/(p->max_period_s*1000*1000.0 + p->max_period_us); 
        printf("\tthis new program u min is %f, u max is %f\n", p->utilization_min, p->utilization_max);
        //federate scheduling
        p->num_core_max = ceil(1.0*(C_s*1000*1000.0+C_us-L_s*1000*1000.0-L_us)/(min_period_s*1000*1000.0+min_period_us-L_s*1000*1000.0-L_us)); //m = ceil((C-L)/(T-L))
        p->num_core_min = ceil(1.0*(C_s*1000*1000.0+C_us-L_s*1000*1000.0-L_us)/(max_period_s*1000*1000.0+max_period_us-L_s*1000*1000.0-L_us));
        printf("\tthis new program need %d ~ %d cores\n", p->num_core_min, p->num_core_max);
        //algorithm spec
        p->m_for_task_compress_par2 = p->num_core_min;//
        p->t_s_for_task_compress_par2 = 0;//
        p->t_us_for_task_compress_par2 = 0;//
        p->z_for_task_compress_par2 = 0;//
        p->flag_for_task_compress_par2 = 1;
        for (i=0; i<p->G->nproc; i++) {
            if (i<(p->num_core_min-1) || i>(p->num_core_max-1)) {
                p->t_s_dict_for_task_compress_par2[i] = 0;
                p->t_us_dict_for_task_compress_par2[i] = 0;
            } else {
                double tmp = (p->C_s*1000*1000.0+p->C_us - p->L_s*1000*1000.0-p->L_us)*1.0/(i+1) + p->L_s*1000*1000.0 + p->L_us;
                p->t_s_dict_for_task_compress_par2[i] = floor(tmp/1000/1000);
                p->t_us_dict_for_task_compress_par2[i] = tmp - p->t_s_dict_for_task_compress_par2[i]*1000*1000;
            }
            //printf("!!!!!in program %d, %d cores have period %llu %llu\n", p->control_uid, i+1, p->t_s_dict_for_task_compress_par2[i], p->t_us_dict_for_task_compress_par2[i]);
        }
    } else {
        p->utilization_max = 0;
        p->utilization_min = 0;
        p->num_core_max = 0;
        p->num_core_min = 0;
        p->m_for_task_compress_par2 = 0;
        p->t_s_for_task_compress_par2 = 0;
        p->t_us_for_task_compress_par2 = 0;
        p->z_for_task_compress_par2 = 0;
    }
}

void container_set_by_request(platform_program * p, platform_program_request * pr) {
    int i = 0;
    //managemenet
    p->run_for_init_container = 0;
    p->self = pr->program_id;
    p->input = pr->input;
    p->mute = pr->mute;
    p->periodic = pr->periodic;
    p->done_one = 0;
    p->pickable = 0;
    p->hint_stop_container = 0;
    p->job_finish = 0;
    p->last_exit_worker_entered_runtime = 0;
    p->last_do_exit_worker_id = -1;
    
    //periodic
    p->max_period_s = pr->max_period_s;
    p->max_period_us = pr->max_period_us;
    p->min_period_s = pr->min_period_s;
    p->min_period_us = pr->min_period_us;
    //real time
    p->C_s = pr->C_s;
    p->C_us = pr->C_us;
    p->L_s = pr->L_s;
    p->L_us = pr->L_us;
    p->elastic_coefficient = pr->elastic_coefficient;
    if (pr->C_s!=0 || pr->C_us!=0) {
        //real time
        p->utilization_max = 1.0*(p->C_s*1000*1000.0 + p->C_us)/(p->min_period_s*1000*1000.0 + p->min_period_us);
        p->utilization_min = 1.0*(p->C_s*1000*1000.0 + p->C_us)/(p->max_period_s*1000*1000.0 + p->max_period_us); 
        printf("\tthis new program u min is %f, u max is %f\n", p->utilization_min, p->utilization_max);
        //federate scheduling
        p->num_core_max = ceil(1.0*(p->C_s*1000*1000.0+p->C_us-p->L_s*1000*1000.0-p->L_us)/(p->min_period_s*1000*1000.0+p->min_period_us-p->L_s*1000*1000.0-p->L_us)); //m = ceil((C-L)/(T-L))
        p->num_core_min = ceil(1.0*(p->C_s*1000*1000.0+p->C_us-p->L_s*1000*1000.0-p->L_us)/(p->max_period_s*1000*1000.0+p->max_period_us-p->L_s*1000*1000.0-p->L_us));
        printf("\tthis new program need %d ~ %d cores\n", p->num_core_min, p->num_core_max);
        //algorithm spec
        p->m_for_task_compress_par2 = p->num_core_min;//
        p->t_s_for_task_compress_par2 = 0;//
        p->t_us_for_task_compress_par2 = 0;//
        p->z_for_task_compress_par2 = 0;//
        p->flag_for_task_compress_par2 = 1;
        for (i=0; i<p->G->nproc; i++) {
            if (i<(p->num_core_min-1) || i>(p->num_core_max-1)) {
                p->t_s_dict_for_task_compress_par2[i] = (unsigned long long) 0;
                p->t_us_dict_for_task_compress_par2[i] = (unsigned long long) 0;
            } else {
                double tmp = (p->C_s*1000*1000.0+p->C_us - p->L_s*1000*1000.0-p->L_us)*1.0/(i+1) + p->L_s*1000*1000.0 + p->L_us;
                p->t_s_dict_for_task_compress_par2[i] = (unsigned long long) floor(tmp/1000/1000);
                p->t_us_dict_for_task_compress_par2[i] = (unsigned long long) (tmp - p->t_s_dict_for_task_compress_par2[i]*1000*1000);
            }
            //printf("!!!!!in program %d, %d cores have period %llu %llu\n", p->control_uid, i+1, p->t_s_dict_for_task_compress_par2[i], p->t_us_dict_for_task_compress_par2[i]);
        }
    } else {
        p->utilization_max = 0;
        p->utilization_min = 0;
        p->num_core_max = 0;
        p->num_core_min = 0;
        p->m_for_task_compress_par2 = 0;
        p->t_s_for_task_compress_par2 = 0;
        p->t_us_for_task_compress_par2 = 0;
        p->z_for_task_compress_par2 = 0;
    }
    p->requested_time = pr->requested_time;
}

static platform_program *container_init_head(platform_global_state *G) {
    platform_program * p = (platform_program *) malloc(sizeof(platform_program));
    p->G = G;
    p->g = NULL;
    p->self = -1;
    p->next = NULL;
    p->prev = NULL;
    p->input = -1;
    p->cpu_mask = NULL;
    p->done_one = 0;
    return p;
}

static local_state *worker_local_init(global_state *g, int worker_id) {
    local_state * l = (local_state *) malloc(sizeof(local_state));
    l->shadow_stack = (__cilkrts_stack_frame **) 
        malloc(g->options.deqdepth * sizeof(struct __cilkrts_stack_frame *));
    for(int i=0; i < JMPBUF_SIZE; i++) { l->rts_ctx[i] = NULL; }
    l->fiber_to_free = NULL;
    l->provably_good_steal = 0;
    l->to_sleep_when_idle_signal = 0;
    cilk_sched_stats_init(&(l->stats));

    l->elastic_s = ACTIVE;
    l->elastic_pos_in_cpu_state_group = worker_id;
    l->run_at_beginning = 0;
    l->is_in_runtime = 0;
    pthread_mutex_init(&(l->elastic_lock), NULL);
    pthread_cond_init(&(l->elastic_cond), NULL);

    return l;
}

void deques_init(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (deques_init) Initializing deques.\n");
    for (int i = 0; i < p->g->options.nproc; i++) {
        ReadyDeque* deque = (ReadyDeque*) malloc (sizeof(ReadyDeque));
        p->g->deques[i] = deque;
        deque->top = NULL;
        deque->bottom = NULL;
        WHEN_CILK_DEBUG(deque->mutex_owner = NOBODY);
        cilk_mutex_init(&(deque->mutex));
    }
}

void  elastic_core_init(platform_program * p) {
    int i=0;
    //fprintf(stderr, "test: elastic_core_init\n");
    p->g->elastic_core->cpu_state_group = (int*) malloc(sizeof(int)*p->g->options.nproc);
    for (i=0; i<p->g->options.nproc; i++) {
        p->g->elastic_core->cpu_state_group[i] = i;
    }
    p->g->elastic_core->ptr_sleeping_inactive_deque = p->g->options.nproc;
    p->g->elastic_core->ptr_sleeping_active_deque = -1;
    cilk_mutex_init(&(p->g->elastic_core->lock));

    p->g->elastic_core->sleep_requests = (int*) malloc(sizeof(int)*p->g->options.nproc);
    p->g->elastic_core->activate_requests = (int*) malloc(sizeof(int)*p->g->options.nproc);
    for (i=0; i<p->g->options.nproc; i++) {
        p->g->elastic_core->sleep_requests[i] = 0;
        p->g->elastic_core->activate_requests[i] = 0;
    }
    p->g->elastic_core->test_thief = -1;
    p->g->elastic_core->test_num_give_up_steal_running = 0;
    p->g->elastic_core->test_num_give_up_steal_returning = 0;
    p->g->elastic_core->test_num_try_steal = 0;
    p->g->elastic_core->test_num_thief_returning_cl_null = 0;
    p->g->elastic_core->test_num_steal_success = 0;
    p->g->elastic_core->test_num_try_lock_failed = 0;
    p->g->elastic_core->test_num_bug = 0;
    p->g->elastic_core->test_num_null_cl = 0;
    p->g->elastic_core->test_num_back_to_user_code = 0;
    p->g->elastic_core->test_num_bottom_returning = 0;
    p->g->elastic_core->test_num_back_to_runtime = 0;
    p->g->elastic_core->test_num_xtract_returning_cl = 0;
}

static void workers_init(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (workers_init) Initializing workers.\n");
    for (int i = 0; i < p->g->options.nproc; i++) {
        __cilkrts_alert(ALERT_BOOT, "[M]: (workers_init) Initializing worker %d.\n", i);
        __cilkrts_worker * w = (__cilkrts_worker *) malloc(sizeof(__cilkrts_worker));
        w->self = i;
        w->g = p->g;
        w->l = worker_local_init(p->g, w->self);

        w->ltq_limit = w->l->shadow_stack + p->g->options.deqdepth;
        p->g->workers[i] = w;
        w->tail = w->l->shadow_stack + 1;
        w->head = w->l->shadow_stack + 1;
        w->exc = w->head;
        w->current_stack_frame = NULL;
        elastic_init_block_pipe(w);

        // initialize internal malloc first
        cilk_internal_malloc_per_worker_init(w);
        cilk_fiber_pool_per_worker_init(w); //long time
    }
}

int get_run_core_id(platform_program * p) {
    int i = 0;
    for (i=0; i<p->G->nproc; i++) {
        if (p->cpu_mask[i]==1) {
            return i;
        }
    }
    printf("[BUG]: no running core at beginning! set the running core as core 1\n");
    return 1;
}

static void* scheduler_thread_proc(void * arg) {
    __cilkrts_worker * w = (__cilkrts_worker *)arg;
    __cilkrts_alert(ALERT_BOOT, "[%d]: (scheduler_thread_proc)\n", w->self);
    __cilkrts_set_tls_worker(w);
    long long idle = 0;
    while(!w->g->start) {
        usleep(1);
        idle++;
    }

    if(w->self == get_run_core_id(w->g->program)&&w->g->done==0) { //all program runs on a running core at beginning as invoke_main
        worker_scheduler(w, w->g->invoke_main);
    } else {
        //Zhe, set init running cores, right after run_program returns
        if (!w->g->done && !elastic_safe(w)) { //at very beginning
            if (w->l->run_at_beginning==0) {
                //do sleep
                if (__sync_bool_compare_and_swap(&(w->l->elastic_s), ACTIVE, SLEEPING_INACTIVE_DEQUE)) { 
                    elastic_core_lock(w);
                    w->g->elastic_core->ptr_sleeping_inactive_deque--;
                    elastic_do_exchange_state_group(w, w->g->workers[w->g->elastic_core->cpu_state_group[w->g->elastic_core->ptr_sleeping_inactive_deque]]);
                    elastic_core_unlock(w);
                    //printf("TEST[%d]: goto SLEEPING_INACTIVE_DEQUE state, E:%p, current_stack_frame:%p, state:%d, head:%p, tail:%p\n", w->self, w->exc, w->current_stack_frame, w->l->elastic_s, w->head, w->tail);
                    elastic_do_cond_sleep(w);

                    //activated
                    w = __cilkrts_get_tls_worker();
                    if (__sync_bool_compare_and_swap(&(w->l->elastic_s), ACTIVATE_REQUESTED, ACTIVATING)) {
                        //printf("TEST[%d]: goto ACTIVATING state, current_stack_frame:%p\n", w->self, w->current_stack_frame);
                        elastic_core_lock(w);
                        elastic_do_exchange_state_group(w, w->g->workers[w->g->elastic_core->cpu_state_group[w->g->elastic_core->ptr_sleeping_inactive_deque]]);
                        w->g->elastic_core->ptr_sleeping_inactive_deque++;
                        elastic_core_unlock(w);
                        
                        if (__sync_bool_compare_and_swap(&(w->l->elastic_s), ACTIVATING, ACTIVE)) {
                            //printf("Activated!\n");
                        }
                    }
                }
            }
        }
        worker_scheduler(w, NULL);
    }

    return 0;
}

static void threads_init(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (threads_init) Setting up threads.\n");
    for (int i = 0; i < p->g->options.nproc; i++) {
        int status = pthread_create(&p->g->threads[i], //long time
                NULL,
                scheduler_thread_proc,
                p->g->workers[i]);
        if (status != 0) 
            __cilkrts_bug("Cilk: thread creation (%d) failed: %d\n", i, status);
        //Affinity setting, from cilkplus-rts
        cpu_set_t process_mask;
        //Get the mask from the parent thread (master thread)
        int err = pthread_getaffinity_np (pthread_self(), sizeof(process_mask), &process_mask);
        if (0 == err) //long time
        {
            int j;
            //Get the number of available cores (copied from os-unix.c)
            int available_cores = 0;
            for (j = 0; j < CPU_SETSIZE; j++){
               if (CPU_ISSET(j, &process_mask)){
                   available_cores++;
               }
            }

            //Bind the worker to a core according worker id
            int workermaskid = i % available_cores;
            for (j = 0; j < CPU_SETSIZE; j++)
            {
                if (CPU_ISSET(j, &process_mask))
                {
                    if (workermaskid == 0){
                    // Bind the worker to the assigned cores
                        cpu_set_t mask;
                        CPU_ZERO(&mask);
                        CPU_SET(j, &mask);
                        int ret_val = pthread_setaffinity_np(p->g->threads[i], sizeof(mask), &mask);
                        if (ret_val != 0)
                        {
                            __cilkrts_bug("ERROR: Could not set CPU affinity");
                        }
                        break;
                    }
                    else{
                        workermaskid--;
                    }
                }
            }
        }
        else{
            __cilkrts_bug("Cannot get affinity mask by pthread_getaffinity_np");
        }
    }
    usleep(10);
}

void __cilkrts_init(platform_program * p, int argc, char* argv[]) {
    //fprintf(stderr, "__cilkrts_init\n");
    __cilkrts_alert(ALERT_BOOT, "[M]: (__cilkrts_init)\n");
    global_state_init(p, argc, argv);
    __cilkrts_init_tls_variables();
    workers_init(p);//long time
    container_set_init_run(p);
    deques_init(p);
    threads_init(p);//long time
    elastic_core_init(p);
}

static void global_state_terminate(platform_program * p) {
    cilk_fiber_pool_global_terminate(p->g);
    cilk_internal_malloc_global_terminate(p->g);
    cilk_sched_stats_print(p->g);
}

void platform_global_state_deinit(platform_global_state * G) {
    platform_program * p = G->program_head;
    platform_program * tmp;
    while(p!=NULL) {
        tmp = p->next;
        free(p);
        p = tmp;
    }
    p = G->deprecated_program_head;
    tmp = NULL;
    while(p!=NULL) {
        tmp = p->next;
        free(p);
        p = tmp;
    }
    free(G);
}

static void global_state_deinit(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (global_state_deinit) Clean up global state.\n");
    cleanup_invoke_main(p->g->invoke_main);
    cilk_fiber_pool_global_destroy(p->g);
    cilk_internal_malloc_global_destroy(p->g); // internal malloc last
    cilk_mutex_destroy(&(p->g->print_lock));
    free(p->g->workers);
    free(p->g->deques);
    free(p->g->threads);
    free(p->g);
}

static void deques_deinit(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (deques_deinit) Clean up deques.\n");
    for (int i = 0; i < p->g->options.nproc; i++) {
        CILK_ASSERT_G(p->g->deques[i]->mutex_owner == NOBODY);
        cilk_mutex_destroy(&(p->g->deques[i]->mutex));
    }
}

static void workers_terminate(platform_program * p) {
    for(int i = 0; i < p->g->options.nproc; i++) {
        __cilkrts_worker *w = p->g->workers[i];
        cilk_fiber_pool_per_worker_terminate(w);
        cilk_internal_malloc_per_worker_terminate(w); // internal malloc last
    }
}

static void workers_deinit(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (workers_deinit) Clean up workers.\n");
    for(int i = 0; i < p->g->options.nproc; i++) {
        __cilkrts_worker *w = p->g->workers[i];
        CILK_ASSERT(w, w->l->fiber_to_free == NULL);

        cilk_fiber_pool_per_worker_destroy(w);
        cilk_internal_malloc_per_worker_destroy(w); // internal malloc last
        free(w->l->shadow_stack);
        free(w->l);
        free(w);
        p->g->workers[i] = NULL;
    }
}

void __cilkrts_cleanup(platform_program * p) {
    workers_terminate(p);
    // global_state_terminate collects and prints out stats, and thus
    // should occur *BEFORE* worker_deinit, because worker_deinit  
    // deinitializes worker-related data structures which may 
    // include stats that we care about.
    // Note: the fiber pools uses the internal-malloc, and fibers in fiber 
    // pools are not freed until workers_deinit.  Thus the stats included on 
    // internal-malloc that does not include all the free fibers.  
    global_state_terminate(p); 

    workers_deinit(p);
    deques_deinit(p);
    global_state_deinit(p);
}
