#define _GNU_SOURCE

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
#include "readydeque.h"
#include "cilk2c.h"
#include "fiber.h"
#include "membar.h"
#include "cilk-scheduler.h"
#include "platform-scheduler.h"
#include "server-support.h"
#include "container.h"

extern void platform_global_state_deinit(platform_global_state * G);
extern platform_global_state * platform_global_state_init(int argc, char* argv[]);
extern platform_program *container_init(platform_global_state *G, int control_uid, int second_level_uid);
extern void container_set_by_init(platform_program * p, int program_id, int input, int periodic, int mute, 
    unsigned long long max_period_s, unsigned long long max_period_us, unsigned long long min_period_s, 
    unsigned long long min_period_us, unsigned long long C_s, unsigned long long C_us, unsigned long long L_s, 
    unsigned long long L_us, float elastic_coefficient);
extern void run_program(platform_global_state * G, platform_program * p);
extern void exit_all_programs(platform_global_state * G);

extern void init_timed_scheduling(platform_global_state *G, int enbale, int interval_sec, int interval_usec);

#undef main
int main(int argc, char* argv[]) {
    unlink(SERVER_FIFO_NAME);
    printf("Init platform data structure...\n");
    platform_global_state * G = platform_global_state_init(argc, argv);
    init_timed_sleep_lock_and_cond();
    init_timed_scheduling(G, 0, 1, 0);
    signal(SIGPIPE, pipe_sig_handler);
    //

    printf("Init FIFO...\n");
    int ret_mkfifo;
    umask(0);
    ret_mkfifo = mkfifo(SERVER_FIFO_NAME, S_IFIFO | 0666);
    if (ret_mkfifo < 0) {
        printf("ERROR: mkfifo failed! Reason: %s\n", strerror(errno));
        exit(-1);
    }
    //

    printf("Init adaptive scheduler thread...\n");
    pthread_t scheduling_thread;
    int status = pthread_create(&scheduling_thread, NULL, main_thread_timed_scheduling, G);
    if (status != 0) {
        __cilkrts_bug("Cilk: thread creation scheduling thread failed: %d\n", status);
    }
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    status = pthread_setaffinity_np(scheduling_thread, sizeof(mask), &mask);
    if (status != 0) {
        __cilkrts_bug("ERROR: Could not set CPU affinity");
    }
    signal(SIGALRM, timed_scheduling_signal_handler);
    set_timer(&(G->timed_scheduling_itv), &(G->timed_scheduling_oldtv), G->timed_interval_sec, G->timed_interval_usec);
    //

    //create program containers
    int i = 0;
    for (i=0; i<CONTAINER_COUNT; i++) {
        printf("***INIT*** container control_uid is %d\n", i+1);
        platform_program * p = container_init(G, i+1, 0);
        container_set_by_init(p, 0, 13, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        G->program_container_pool[i] = p;
        int j = 2;
        p->cpu_mask[0] = 0;
        p->cpu_mask[1] = 0;
        p->try_cpu_mask[0] = 0;
        p->try_cpu_mask[1] = 0;
        for (j=2; j<p->G->nproc; j++) {
            p->try_cpu_mask[j] = 1;
            p->cpu_mask[j] = 1;
        }
        p->num_cpu = p->G->nproc-2;
        p->try_num_cpu = p->G->nproc-2;
        platform_activate_container(p);
        run_program(G, p);
        usleep(1000*500); //to guarantee the correctness of the jump buffer: should be non-null
    }
    for (i=0; i<CONTAINER_COUNT; i++) {
        while(G->program_container_pool[i]->pickable==0) {
            sleep(1);
        }
        G->program_container_pool[i]->run_times--;
    }

    //wait for all containers are init
    for (i=0; i<CONTAINER_COUNT; i++) {
        while(G->program_container_pool[i]->pickable==0) {
            usleep(1000*100);
        }
    }
    sleep(1);

    //container trigger, run on core 0
    /*CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    pthread_t thread_container_trigger;
    status = pthread_create(&thread_container_trigger, NULL, main_thread_thread_container_trigger, G);
    if (status != 0) {
        __cilkrts_bug("Cilk: thread creation receiver thread failed: %d\n", status);
    }
    status = pthread_setaffinity_np(thread_container_trigger, sizeof(mask), &mask);
    if (status != 0) {
        __cilkrts_bug("ERROR: Could not set CPU affinity");
    }
    //printf("\t%d new program and scheduling finishes, to relase big lock\n", p->control_uid);
    if(status != 0) {
        printf("Cilk runtime error: thread join receiver thread failed: %d\n", status);
    }*/

    //request receiver, run on core 1
    CPU_ZERO(&mask);
    CPU_SET(1, &mask);
    pthread_t thread_receiver;
    status = pthread_create(&thread_receiver, NULL, main_thread_new_program_receiver, G);
    if (status != 0) {
        __cilkrts_bug("Cilk: thread creation receiver thread failed: %d\n", status);
    }
    status = pthread_setaffinity_np(thread_receiver, sizeof(mask), &mask);
    if (status != 0) {
        __cilkrts_bug("ERROR: Could not set CPU affinity");
    }
    //printf("\t%d new program and scheduling finishes, to relase big lock\n", p->control_uid);
    if(status != 0) {
        printf("Cilk runtime error: thread join receiver thread failed: %d\n", status);
    }
    printf("***INIT DONE!***\n");

    status = pthread_join(thread_receiver, NULL);
    //printf("\t%d new program and scheduling finishes, to relase big lock\n", p->control_uid);
    if(status != 0) {
        printf("Cilk runtime error: thread join scheduling thread failed: %d\n", status);
    }
    //

    exit_all_programs(G);
    platform_global_state_deinit(G);
    unlink(SERVER_FIFO_NAME);
    sleep(1);
    return 0;
}