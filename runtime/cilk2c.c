#include <stdio.h>

#include "debug.h"
#include "cilk2c.h"
#include "internal.h"
#include "fiber.h"
#include "membar.h"
#include "cilk-scheduler.h"
#include "macro-bench-support.h"

int cilkg_nproc = 0;

// ================================================================
// This file comtains the compiler ABI, which corresponds to 
// conceptually what the compiler generates to implement Cilk code.
// They are included here in part as documentation, and in part
// allow one to write and run "hand-compiled" Cilk code.
// ================================================================

// inlined by the compiler
void __cilkrts_enter_frame(__cilkrts_stack_frame *sf) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_enter_frame) frame %p\n", w->self, sf);

    sf->flags = CILK_FRAME_VERSION;
    sf->call_parent = w->current_stack_frame; 
    sf->worker = w;
    w->current_stack_frame = sf;
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);
}

// inlined by the compiler; this implementation is only used in container-main.c
void __cilkrts_enter_frame_fast(__cilkrts_stack_frame * sf) {
    __cilkrts_worker * w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_enter_frame_fast) frame %p\n", w->self, sf);

    sf->flags = CILK_FRAME_VERSION;
    sf->call_parent = w->current_stack_frame; 
    sf->worker = w;
    w->current_stack_frame = sf;
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);
}

// inlined by the compiler; this implementation is only used in container-main.c
void __cilkrts_detach(__cilkrts_stack_frame * sf) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_detach) frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    //CILK_ASSERT(w, w->current_stack_frame == sf);

    struct __cilkrts_stack_frame *parent = sf->call_parent;
    struct __cilkrts_stack_frame *volatile *tail = w->tail;
    CILK_ASSERT(w, (tail+1) < w->ltq_limit);

    // store parent at *tail, and then increment tail
    *tail++ = parent;
    sf->flags |= CILK_FRAME_DETACHED;
    Cilk_membar_StoreStore();
    w->tail = tail;
}

// inlined by the compiler; this implementation is only used in container-main.c
void __cilkrts_save_fp_ctrl_state(__cilkrts_stack_frame *sf) {
    sysdep_save_fp_ctrl_state(sf);
}

void __cilkrts_sync(__cilkrts_stack_frame *sf) {

    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    __cilkrts_alert(ALERT_SYNC, "[%d]: (__cilkrts_sync) syncing frame %p\n", w->self, sf);

    //CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, sf == w->current_stack_frame);

    if( Cilk_sync(w, sf) == SYNC_READY ) {
        __cilkrts_alert(ALERT_SYNC, 
            "[%d]: (__cilkrts_sync) synced frame %p!\n", w->self, sf);
        // The Cilk_sync restores the original rsp stored in sf->ctx
        // if this frame is ready to sync.
        sysdep_longjmp_to_sf(sf);
    } else {
        __cilkrts_alert(ALERT_SYNC, 
            "[%d]: (__cilkrts_sync) waiting to sync frame %p!\n", w->self, sf);
        longjmp_to_runtime(w);                        
    }
}

// inlined by the compiler; this implementation is only used in container-main.c
void __cilkrts_pop_frame(__cilkrts_stack_frame * sf) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_pop_frame) frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    //CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    w->current_stack_frame = sf->call_parent;
    sf->call_parent = 0;
}

void __cilkrts_leave_frame(__cilkrts_stack_frame * sf) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, 
        "[%d]: (__cilkrts_leave_frame) leaving frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    //CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    // WHEN_CILK_DEBUG(sf->magic = ~CILK_STACKFRAME_MAGIC);
    if (w->g->program->done_one==1) {
                //printf("enter __cilkrts_leave_frame1 %d, e: %p, t: %p\n", w->self, w->exc, w->tail);
            }

    if(sf->flags & CILK_FRAME_DETACHED) { // if this frame is detached
        __cilkrts_stack_frame *volatile *t = w->tail;
        --t;
        w->tail = t;
        __sync_fetch_and_add(&sf->flags, ~CILK_FRAME_DETACHED); 
        // Cilk_membar_StoreLoad(); // the sync_fetch_and_add replaced mfence
                // which is slightly more efficient.  Note that this
                // optimiation is applicable *ONLY* on i386 and x86_64

        //Zhe: for triggering elastic, two entries to Cilk_exception_handler: 1. e>t, as original; 2. worker is set to sleep.
        /*if (w->g->elastic_core->test_thief==w->self) {
            printf("CILK_FRAME_DETACHED, t:%p exc:%p, steal attemps:%d\n", t, w->exc, w->g->elastic_core->test_num_try_steal);
        }*/
        if (w->g->program->done_one==1) {
                //printf("enter __cilkrts_leave_frame2 %d, e: %p, t: %p\n", w->self, w->exc, t);
            }

        if(__builtin_expect(w->exc>t,0)) {
            // this may not return if last work item has been stolen
            if (w->g->program->done_one==1) {
                //printf("enter __cilkrts_leave_frame3 before Cilk_exception_handler %d\n", w->self);
            }
            Cilk_exception_handler(); 
            w = __cilkrts_get_tls_worker();
            printf("p:%d, w:%d returns __cilkrts_leave_frame\n", w->g->program->control_uid, w->self);
        }
        // CILK_ASSERT(w, *(w->tail) == w->current_stack_frame);
    } else {
        // A detached frame would never need to call Cilk_set_return, which 
        // performs the return protocol of a full frame back to its parent
        // when the full frame is called (not spawned).  A spawned full 
        // frame returning is done via a different protocol, which is 
        // triggered in Cilk_exception_handler. 
        if(sf->flags & CILK_FRAME_STOLEN) { // if this frame has a full frame
            __cilkrts_alert(ALERT_RETURN, 
                "[%d]: (__cilkrts_leave_frame) parent is call_parent!\n", w->self);
            // leaving a full frame; need to get the full frame of its call
            // parent back onto the deque
            Cilk_set_return(w);
            CILK_ASSERT(w, w->current_stack_frame->flags & CILK_FRAME_VERSION);
        }
    }
}

int __cilkrts_get_nworkers(void) {
  return cilkg_nproc;
}

//Zhe: trigger exception for elastic operations: add or decrease a core
void __cilkrts_elastic_trigger_sleep(__cilkrts_stack_frame * sf, int cpu_id) {
    elastic_worker_request_cpu_to_sleep(sf->worker, cpu_id);
}

void __cilkrts_elastic_trigger_activate(__cilkrts_stack_frame * sf, int cpu_id) {
    elastic_worker_request_cpu_to_recover(sf->worker, cpu_id);
}

void __cilkrts_set_begin_time_ns() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    program_set_begin_time_ns(w->g->program);
}

void __cilkrts_set_end_time_ns() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    program_set_end_time_ns(w->g->program);
}

unsigned long long __cilkrts_get_run_time_ns() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    return program_get_run_time_ns(w->g->program);
}

void __cilkrts_macro_add_program_run_num() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    w->g->program->G->macro_test_num_programs_executed++;
}

unsigned long long __cilkrts_macro_get_program_run_num() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    return w->g->program->G->macro_test_num_programs_executed;
}

int __cilkrts_get_control_uid() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    return w->g->program->control_uid;
}

int __cilkrts_get_input() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    return w->g->program->input;
}

int __cilkrts_get_request_buffer_size(int uid) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    return get_count_program_request_buffer(w->g->program->G, uid);
}

void __cilkrts_print_cpu_mask() {
    __cilkrts_worker *w = __cilkrts_get_tls_worker(); //Chen
    int i = 0;
    printf("\t");
    for (i=0; i<w->g->program->G->nproc; i++) {
        printf("%d", w->g->program->cpu_mask[i]);
    }
    printf("\n");
}