#include <stdio.h>
#include <stdlib.h>

#include "../runtime/cilk2c.h"
#include "ktiming.h"

#ifndef TIMING_COUNT 
#define TIMING_COUNT 1 
#endif

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
   
int fib(int n) {
    int x, y, _tmp;

    if(n < 2) {
        return n;
    }
    
    x = cilk_spawn fib(n - 1);
    y = fib(n - 2);
    cilk_sync;

    return x+y;
}
*/

extern unsigned long ZERO;

static void __attribute__ ((noinline)) fib_spawn_helper(int *x, int n); 

int fib(int n) {
    int x, y, _tmp;

    if(n < 2)
        return n;

    alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);
    /* x = spawn fib(n-1) */
    __cilkrts_save_fp_ctrl_state(&sf);
    if(!__builtin_setjmp(sf.ctx)) {
      fib_spawn_helper(&x, n-1);
    }
    //__cilkrts_elastic_trigger_sleep(&sf, 1); //Zhe: test for elastic
    //__cilkrts_elastic_trigger_activate(&sf, 1);
    /*__cilkrts_elastic_trigger_sleep(&sf, 3); //Zhe: test for elastic
    __cilkrts_elastic_trigger_activate(&sf, 3);
    __cilkrts_elastic_trigger_sleep(&sf, 5); //Zhe: test for elastic
    __cilkrts_elastic_trigger_activate(&sf, 5);*/

    y = fib(n - 2);

    /* cilk_sync */
    if(sf.flags & CILK_FRAME_UNSYNCHED) {
      __cilkrts_save_fp_ctrl_state(&sf);
      if(!__builtin_setjmp(sf.ctx)) {
        __cilkrts_sync(&sf);
      }
    }
    _tmp = x + y;

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    return _tmp;
}

static void __attribute__ ((noinline)) fib_spawn_helper(int *x, int n) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);
    __cilkrts_detach(&sf);
    *x = fib(n);
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf); 
}

int fib_cilk_main(int n) {
    int i;
    int res;
    unsigned long long running_time[TIMING_COUNT];
    //printf("run fib: %d\n", n);

    for(i = 0; i < TIMING_COUNT; i++) {
        __cilkrts_set_begin_time_ns();
        res = fib(n);
        __cilkrts_set_end_time_ns();
        running_time[i] = __cilkrts_get_run_time_ns();
    }
    //printf("Result: %d\n", res);
    print_runtime(running_time, TIMING_COUNT); 

    return res;
}
