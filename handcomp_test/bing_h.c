#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "../runtime/cilk2c.h"
#include "ktiming.h"
#include "timespec_functions.h"

typedef unsigned long DWORD;
extern unsigned long ZERO;
struct timespec segment_length = {.tv_sec = 0, .tv_nsec = 10000}; //nsec = 1000
#define POWER_NINE 1000000000

static void __attribute__ ((noinline)) foo_spawn_helper(int low, int high);

static inline void
workload_for_1micros(int length) {
    volatile double temp = 0;
    volatile int long long i;
        for ( i = 0; i < 52*length; ++i )
        temp = sqrt((double)i*i);
}

void busy_work(struct timespec length)
{
    struct timespec curr_time;
    struct timespec target_time;
    //struct timespec twoms = { 0, 2000000 };
    //struct timespec twomicros = { 0, 1000 };
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &curr_time);
    target_time.tv_nsec = curr_time.tv_nsec + length.tv_nsec;
    target_time.tv_sec = curr_time.tv_sec + length.tv_sec;

    while(curr_time.tv_nsec + (unsigned long long)curr_time.tv_sec * POWER_NINE + 2000000 < target_time.tv_nsec + (unsigned long long)target_time.tv_sec * POWER_NINE)
    {
        workload_for_1micros(1000);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &curr_time);
    }
    while(curr_time.tv_nsec + (unsigned long long)curr_time.tv_sec * POWER_NINE + 1000 < (unsigned long long)target_time.tv_sec * POWER_NINE + target_time.tv_nsec)
    {
        workload_for_1micros(1);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &curr_time);
    }
    //printf("at busy work\n");
}

void ts_diff (struct timespec ts1, struct timespec ts2, struct timespec result){

  if( ts1.tv_nsec + (unsigned long long)ts1.tv_sec * POWER_NINE > ts2.tv_nsec + (unsigned long long)ts2.tv_sec * POWER_NINE){
    result.tv_nsec = ts1.tv_nsec - ts2.tv_nsec;
    result.tv_sec = ts1.tv_sec - ts2.tv_sec;
  } else {
    result.tv_nsec = ts2.tv_nsec - ts1.tv_nsec;
    result.tv_sec = ts2.tv_sec - ts1.tv_sec;
  }

  if( result.tv_nsec < 0 ){            //If we have a negative nanoseconds
    result.tv_nsec += nanosec_in_sec; //value then we carry over from the
    result.tv_sec -= 1;               //seconds part of the struct timespec
  }
}

void sleep_for_ts (struct timespec sleep_time){
  //Otherwise, nanosleep
    //struct timespec zero = { 0, 0 };
    while( nanosleep(&sleep_time,&sleep_time) != 0 )
    {
        if (sleep_time.tv_nsec <= 0 && sleep_time.tv_sec <= 0) 
            break;
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//#define NumRequests 1000000
#define NumRequests 1000
//#define NumRequests 10
#define ResultAccuracy 1000000.0
#define RequestWeightMultiplierFirstGroup 200
#define MeanRequestWeigth 3500*4.78
int scale = 2;
//int scale = 1;
//double scale = 0.5;
int id[NumRequests];
struct timespec release[NumRequests];
struct timespec start[NumRequests];
struct timespec finish[NumRequests];
int num_segment[NumRequests];
int done[NumRequests];
int work[NumRequests];
//struct timespec segment_length;

int num_strands = 64*20;//*50000;
//int num_strands = 64;
double randarray[NumRequests];
int logarray[NumRequests];
        double PoissonArray[NumRequests];
        double PoissonTail;
        int PoissonArrayPosition;
struct timespec m_timeStartServer, lasttime;
int m_sentRequests;
double exeratio = 0.1;
const char * newLogFileName = "results.txt";

double interArrivalTime = 0.83;
int i = 0;


void foo_spawn(int low, int high) {

        clockmark_t begin, end;
    //fprintf(stderr, "(job %d) release = %ld %ld.\n",jid, release[jid].tv_sec,release[jid].tv_nsec);
//    fprintf(stderr, "(job %d) work = %d. release = %ld %ld\n",
//      jid, num_segment[jid]*num_strands*15,release[jid].tv_sec,release[jid].tv_nsec);
        //printf("foo_spawn, low %d, high %d\n", low, high);
        //int count = high - low; //previous version
        int count = 0;

        if(high > low){
            count = high - low;
        }
        else{
            printf("ERROR: high is smaller than low\n.");
        }

        if (count <= 50){
            //printf("count is %d\n", high - low);
            //busy_work(segment_length);
            return;
        }

        int mid = low + count / 2;

        alloca(ZERO);
        __cilkrts_stack_frame sf;
        __cilkrts_enter_frame(&sf);
        __cilkrts_save_fp_ctrl_state(&sf);

        begin = ktiming_getmark();
        //printf("foo_spawn_1: %llu", begin);



        if(!__builtin_setjmp(sf.ctx)) {
            foo_spawn_helper(low, mid);
        }
        end = ktiming_getmark();
        //printf("running time: %f\n", ktiming_diff_usec(&begin, &end)/1000/1000/1000.0);

        //low = mid;
        begin = ktiming_getmark();
        foo_spawn(mid, high);
        end = ktiming_getmark();
        //printf("running time: %f\n", ktiming_diff_usec(&begin, &end)/1000/1000/1000.0);

        begin = ktiming_getmark();
        busy_work(segment_length);
        end = ktiming_getmark();
        //printf("running time: %f\n", ktiming_diff_usec(&begin, &end)/1000/1000/1000.0);

        if(sf.flags & CILK_FRAME_UNSYNCHED) {
            __cilkrts_save_fp_ctrl_state(&sf);
        if(!__builtin_setjmp(sf.ctx)) {
            __cilkrts_sync(&sf);
        }
    }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
    //printf("high %d, low %d\n", high, low);
            

//  fprintf(stderr,"(job %d) %d finish %ld %ld\n",jid,done[jid],finish[jid].tv_sec-release[jid].tv_sec,finish[jid].tv_nsec-release[jid].tv_nsec);
    return;
}

static void __attribute__ ((noinline)) foo_spawn_helper(int low, int high) {
    //printf("foo_spawn_helper, low is %d, high is %d\n", low, high);

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);
    __cilkrts_detach(&sf);
    foo_spawn(low, high);
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf); 
}

//section 1, like nqueen. Input n
int bing_h_cilk_main(int n) {
    //int jid = *((int *)data);
    //int jnumseg = num_segment[jid];//check table
    
    /*if (argc!=2) {
        printf("input error, should give num_segment as int\n");
        exit(-1);
    }*/
    int jnumseg = n;
    __cilkrts_set_begin_time_ns();
    //printf("begin time: %llu\n.", begin);
    int i;
    for (i = 0; i < jnumseg; ++i){
    
        foo_spawn(0, num_strands);
        busy_work(segment_length);
    }

    __cilkrts_set_end_time_ns();
    //printf("end time: %llu\n.", end);
    __cilkrts_macro_add_program_run_num();
    /*printf("\tprogram finish, (p %d, input: %d)num: %d, buff: %d, latency: %f, run: %f, flow: %f, pick: %f, activate: %f, core_assign: %f, preempt %f\n", 
        __cilkrts_get_control_uid(), __cilkrts_get_input(), __cilkrts_macro_get_program_run_num(),
        __cilkrts_get_request_buffer_size(), 
        __cilkrts_get_response_latency_time_ns()/1000/1000/1000.0, 
        __cilkrts_get_run_time_ns()/1000/1000/1000.0, 
        __cilkrts_get_flow_time_ns()/1000/1000/1000.0, 
        __cilkrts_get_overhead_picking_container_ns()/1000/1000/1000.0, 
        __cilkrts_get_overhead_activating_container_ns()/1000/1000/1000.0, 
        __cilkrts_get_overhead_core_assignment_ns()/1000/1000/1000.0, 
        __cilkrts_get_overhead_preemption_ns()/1000/1000/1000.0);*/
    //__cilkrts_print_cpu_mask();

    //done[jid] = 1;

    return 1;

}

