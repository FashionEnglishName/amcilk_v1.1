#ifndef TIMING_COUNT
#define TIMING_COUNT 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"
#include "ktiming.h"
 

// int * count;

/* 
 * nqueen  4 = 2 
 * nqueen  5 = 10 
 * nqueen  6 = 4 
 * nqueen  7 = 40 
 * nqueen  8 = 92 
 * nqueen  9 = 352 
 * nqueen 10 = 724
 * nqueen 11 = 2680 
 * nqueen 12 = 14200 
 * nqueen 13 = 73712 
 * nqueen 14 = 365596 
 * nqueen 15 = 2279184 
 */

/*
 * <a> contains array of <n> queen positions.  Returns 1
 * if none of the queens conflict, and returns 0 otherwise.
 */
static int ok (int n, char *a) {

    int i, j;
    char p, q;

    for (i = 0; i < n; i++) {
        p = a[i];
        for (j = i + 1; j < n; j++) {
            q = a[j];
            if (q == p || q == p - (j - i) || q == p + (j - i))
                return 0;
        }
    }

    return 1;
}

static void __attribute__ ((noinline))
nqueens_spawn_helper(int *count, int n, int j, char *a); 

static int nqueens(int n, int j, char *a) {

    char *b;
    int i;
    int *count;
    int solNum = 0;

    if (n == j) {
        return 1;
    }

    count = (int *) alloca(n * sizeof(int));
    (void) memset(count, 0, n * sizeof (int));

    alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    if (sf.worker->g->program->done_one==1) {
      //printf("enter nqueens\n");
    }

    for (i = 0; i < n; i++) {

        /***
         * Strictly speaking, this (alloca after spawn) is frowned 
         * up on, but in this case, this is ok, because b returned by 
         * alloca is only used in this iteration; later spawns don't 
         * need to be able to access copies of b from previous iterations 
         ***/
        b = (char *) alloca((j + 1) * sizeof (char));
        memcpy(b, a, j * sizeof (char));
        b[j] = i;

        if(ok (j + 1, b)) {

            /* count[i] = cilk_spawn nqueens(n, j + 1, b); */
            __cilkrts_save_fp_ctrl_state(&sf);
            if(!__builtin_setjmp(sf.ctx)) {
                nqueens_spawn_helper(&(count[i]), n, j+1, b);
            }
        }
    }

    /* cilk_sync */
    if(sf.flags & CILK_FRAME_UNSYNCHED) {
      __cilkrts_save_fp_ctrl_state(&sf);
      if(!__builtin_setjmp(sf.ctx)) {
        __cilkrts_sync(&sf);
      }
    }

    for(i = 0; i < n; i++) {
        solNum += count[i];
    }
      /*__cilkrts_elastic_trigger_activate(&sf, 10);
      __cilkrts_elastic_trigger_sleep(&sf, 3); //Zhe: test for elastic
      __cilkrts_elastic_trigger_activate(&sf, 3);
      __cilkrts_elastic_trigger_sleep(&sf, 5); //Zhe: test for elastic
      __cilkrts_elastic_trigger_activate(&sf, 5);
      __cilkrts_elastic_trigger_sleep(&sf, 11);  */
      /*__cilkrts_elastic_trigger_sleep(&sf, 12); 
      __cilkrts_elastic_trigger_sleep(&sf, 13); 
      __cilkrts_elastic_trigger_sleep(&sf, 14); 
      __cilkrts_elastic_trigger_sleep(&sf, 15); 
      __cilkrts_elastic_trigger_sleep(&sf, 5); 
      __cilkrts_elastic_trigger_sleep(&sf, 6); 
      __cilkrts_elastic_trigger_sleep(&sf, 7); 
      __cilkrts_elastic_trigger_sleep(&sf, 8);*/ 
      //__cilkrts_elastic_trigger_sleep(&sf, 9);
    
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    if (solNum>1000 && sf.worker->g->program->input==14) {
      //printf("result %d\n", solNum);
    }

    return solNum;
}

static void __attribute__ ((noinline)) 
nqueens_spawn_helper(int *count, int n, int j, char *a) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);

    if (sf.worker->g->program->done_one==1) {
      //printf("enter nqueens_spawn_helper\n");
    }

    __cilkrts_detach(&sf);

    *count = nqueens(n, j, a);

    __cilkrts_pop_frame(&sf);
    if (*count>1000 && sf.worker->g->program->input==14) {
      //printf("spawn %d\n", *count);
    }

    if (sf.worker->g->program->done_one==1) {
      //printf("enter nqueens_spawn_helper before __cilkrts_leave_frame %d, e: %p, t:%p\n", sf.worker->self, sf.worker->exc, sf.worker->tail);
    }

    __cilkrts_leave_frame(&sf); 

    if (sf.worker->g->program->done_one==1) {
      //printf("enter nqueens_spawn_helper after __cilkrts_leave_frame\n");
    }
}

int nqueens_cilk_main(int n) { 
  //printf("run nqueen: %d\n", n);

  char *a;
  int res;

  a = (char *) alloca (n * sizeof (char));
  res = 0;

#if TIMING_COUNT
  //clockmark_t begin, end;
  unsigned long long elapsed[TIMING_COUNT];

  for(int i=0; i < TIMING_COUNT; i++) {
      __cilkrts_set_begin_time_ns();
      res = nqueens(n, 0, a);
      __cilkrts_set_end_time_ns();
      elapsed[i] = __cilkrts_get_run_time_ns();
  }
  print_runtime(elapsed, TIMING_COUNT);
#else
  res = nqueens(n, 0, a);
#endif

  if (res == 0) {
      printf("No solution found.\n");
  } else {
      printf("\tTotal number of solutions : %d\n", res);
  }

  return res;
}
