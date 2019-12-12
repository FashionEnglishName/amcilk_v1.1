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
#include "sched_stats.h"

extern unsigned long ZERO;

extern int cilk_main(int program_id, int input);

extern void __cilkrts_init(platform_program * p, int argc, char* argv[]);
extern void __cilkrts_cleanup(platform_program * p);

__attribute__((noreturn)) void invoke_main(); // forward decl

Closure * create_invoke_main(global_state *const g) {

    Closure *t;
    __cilkrts_stack_frame * sf;
    struct cilk_fiber *fiber;
    t = Closure_create_main();
    t->status = CLOSURE_READY;

    __cilkrts_alert(ALERT_BOOT, "[M]: (create_invoke_main) invoke_main = %p.\n", t);

    sf = malloc(sizeof(*sf));
    fiber = cilk_main_fiber_allocate();

    // it's important to set the following fields for the root closure, 
    // because we use the info to jump to the right stack position and start
    // executing user code.  For any other frames, these fields get setup 
    // in user code before a spawn and when it gets stolen the first time.
    void *new_rsp = (void *)sysdep_reset_jump_buffers_for_resume(fiber, sf);
    CILK_ASSERT_G(SP(sf) == new_rsp);
    FP(sf) = new_rsp;
    PC(sf) = (void *) invoke_main;

    sf->flags = CILK_FRAME_VERSION;
    __cilkrts_set_stolen(sf);
    // FIXME
    sf->flags |= CILK_FRAME_DETACHED;

    t->frame = sf;
    sf->worker = (__cilkrts_worker *) NOBODY;
    t->fiber = fiber;
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);

    __cilkrts_alert(ALERT_BOOT, "[M]: (create_invoke_main) invoke_main->fiber = %p.\n", fiber);

    return t;
}

void cleanup_invoke_main(Closure *invoke_main) {
    printf("cleanup_invoke_main???????\n");
    cilk_main_fiber_deallocate(invoke_main->fiber); 
    free(invoke_main->frame);
    Closure_destroy_main(invoke_main);
}

void spawn_cilk_main(platform_program *p, int *res) {
    __cilkrts_stack_frame *sf = alloca(sizeof(__cilkrts_stack_frame));
    __cilkrts_enter_frame_fast(sf);
    __cilkrts_detach(sf);
    p->running_job = 1;
    *res = cilk_main(p->self, p->input);
    __cilkrts_pop_frame(sf);
    __cilkrts_leave_frame(sf);
}

/*
 * ANGE: strictly speaking, we could just call cilk_main instead of spawn,
 * but spawning has a couple advantages: 
 * - it allow us to do tlmm_set_closure_stack_mapping in a natural way 
 for the invoke_main closure (otherwise would need to setup it specially).
 * - the sync point after spawn of cilk_main provides a natural point to
 *   resume if user ever calls Cilk_exit and abort the entire computation.
 */

__attribute__((noreturn)) void invoke_main() {
    __cilkrts_worker *w;
    __cilkrts_stack_frame *sf;

//run section begin
run_point:
    w = __cilkrts_get_tls_worker();
    sf = w->current_stack_frame;
    w->g->program->job_init_finish = 1;
    //printf("\t%d, %d, new run section begin!\n", w->g->program->control_uid, w->self);
    char * rsp;
    char * nsp;
    int _tmp;

    ASM_GET_SP(rsp);
    __cilkrts_alert(ALERT_BOOT, "[%d]: (invoke_main).\n", w->self);
    __cilkrts_alert(ALERT_BOOT, "[%d]: (invoke_main) rsp = %p.\n", w->self, rsp);

    alloca(ZERO);

    __cilkrts_save_fp_ctrl_state(sf);
    if(__builtin_setjmp(sf->ctx) == 0) {
        //printf("\tinvariant %d call spawn_cilk_main\n", w->self);
        spawn_cilk_main(w->g->program, &_tmp);
    } else {
        // ANGE: Important to reset using sf->worker;
        // otherwise w gets cached in a register 
        //w = sf->worker;
        //__cilkrts_alert(ALERT_BOOT, "[%d]: (invoke_main) corrected worker after spawn.\n", w->self);

        w = __cilkrts_get_tls_worker();
        if (w->self==w->g->program->invariant_running_worker_id) {
            //printf("[PLATFORM]: invariant %d enters to the point after spawn_cilk_main\n", w->self);
        }
    }

    ASM_GET_SP(nsp);
    __cilkrts_alert(ALERT_BOOT, "[%d]: (invoke_main) new rsp = %p.\n", w->self, nsp);

    CILK_ASSERT_G(w == __cilkrts_get_tls_worker());

    if (sf!=NULL) {
        if(__cilkrts_unsynced(sf)) {
            __cilkrts_save_fp_ctrl_state(sf);
            if(__builtin_setjmp(sf->ctx) == 0) {
                 __cilkrts_sync(sf);
            } else {
                // ANGE: Important to reset using sf->worker; 
                // otherwise w gets cached in a register
                //w = sf->worker;
                //__cilkrts_alert(ALERT_BOOT, "[%d]: (invoke_main) corrected worker after sync.\n", w->self);
                w = __cilkrts_get_tls_worker();
                w->g->program->running_job = 0;
                if (w->self==w->g->program->invariant_running_worker_id) {
                    //printf("[PLATFORM]: invariant %d enters to the point after sync\n", w->self);
                }
            }
        }
    } else {
        printf("[BUG]: sf is NULL, %d\n", w->g->program->control_uid);
        abort();
    }

    //a job is completed
    assert_num_ancestor(0, 0, 0);
    if (__sync_bool_compare_and_swap(&(w->g->program->job_finish), 0, -1)) {
        w->g->cilk_main_return = _tmp;
        w->g->program->last_do_exit_worker_id = w->self; //must update before set job_finish.
        Cilk_fence();
        if (__sync_bool_compare_and_swap(&(w->g->program->job_finish), -1, 1)) {
            if (w->self!=w->g->program->invariant_running_worker_id) {
                __cilkrts_save_fp_ctrl_state_for_switch(w->current_stack_frame);
                if(!__builtin_setjmp(w->current_stack_frame->switch_ctx)) {
                    if (__sync_bool_compare_and_swap(&(w->g->program->is_switching), 0, 1)) { //do switching
                        printf("[PLATFORM %d]: last worker %d jumps to runtime\n", w->g->program->control_uid, w->self);
                        longjmp_to_runtime(w);
                    } else {
                        printf("ERROR: is_switching is not 0 which is wrong at beginning\n");
                        abort();
                    }
                }
            }

            //only inv worker can reach here
            w = __cilkrts_get_tls_worker();
            printf("[PLATFORM %d]: invariant %d enters to exit handling\n", w->g->program->control_uid, w->self);
            if (w->self==w->g->program->invariant_running_worker_id) {
                //w->g->program->is_switching = 0;
                if (__sync_bool_compare_and_swap(&(w->g->program->is_switching), 1, 2) ||
                    __sync_bool_compare_and_swap(&(w->g->program->is_switching), 0, 0) ) {
                    pthread_mutex_lock(&(w->g->program->G->lock));
                    program_print_result_acc(w->g->program);
                    if (w->g->program->mute==0) {
                        platform_response_to_client(w->g->program);
                    }
                    pthread_mutex_unlock(&(w->g->program->G->lock));
                    container_plugin_enable_run_cycle(w);
                    goto run_point; //new cycle  
                } else {
                    printf("ERROR: is_switching error: not 1 nor 0\n");
                    abort();
                }
            } else {
                printf("ERROR: a non-inv worker jumps to exiting handling\n");
                abort();
            }
        } else {
            printf("ERROR: job_finish state is changed by others\n");
            abort();
        }
    }

    //meaningless, this line can not be reached
    longjmp_to_runtime(w);//just for correctness
}

static void program_main_thread_init(platform_program * p) {
    __cilkrts_alert(ALERT_BOOT, "[M]: (program_main_thread_init) Setting up main thread's closure.\n");

    p->g->invoke_main = create_invoke_main(p->g);
    // ANGE: the order here is important!
    Cilk_membar_StoreStore();
}

static void threads_join(platform_program * p) {
    for (int i = 0; i < p->g->options.nproc; i++) {
        int status = pthread_join(p->g->threads[i], NULL);
        if(status != 0)
            printf("Cilk runtime error: thread join (%d) failed: %d\n", i, status);
    }
    __cilkrts_alert(ALERT_BOOT, "[M]: (threads_join) All workers joined!\n");
}

static void __cilkrts_exit(platform_program * p) {
    __cilkrts_cleanup(p);
}

void run_program(platform_global_state * G, platform_program * p) {
    __cilkrts_init(p, G->platform_argc, G->platform_argv);
    //fprintf(stderr, "Cheetah: invoking user main with %d workers.\n", p->g->options.nproc);
    program_main_thread_init(p);
    p->g->start = 1;
}

void join_programs(platform_global_state * G) {
    platform_program * tmp = G->program_head->next;
    while(tmp!=NULL) {
        threads_join(tmp);
        tmp = tmp->next;
    }
}

int exit_program(platform_program * p) {
    int ret;
    ret = p->g->cilk_main_return;
    __cilkrts_exit(p);
    return ret;
}

void exit_all_programs(platform_global_state * G) {
    platform_program * tmp = G->program_head->next;
    while(tmp!=NULL) {
        exit_program(tmp);
        tmp = tmp->next;
    }
    tmp = G->deprecated_program_head->next;
    while(tmp!=NULL) {
        exit_program(tmp);
        tmp = tmp->next;
    }
}