#include "elastic.h"
#include "membar.h"
#include "jmpbuf.h"

void elastic_core_lock(__cilkrts_worker *w) {
    cilk_mutex_lock(&(w->g->elastic_core->lock));
}

void elastic_core_unlock(__cilkrts_worker *w) {
    cilk_mutex_unlock(&(w->g->elastic_core->lock));
}

void elastic_core_lock_g(global_state *g) {
    cilk_mutex_lock(&(g->elastic_core->lock));
}

void elastic_core_unlock_g(global_state *g) {
    cilk_mutex_unlock(&(g->elastic_core->lock));
}

void elastic_do_exchange_state_group(__cilkrts_worker *worker_a, __cilkrts_worker *worker_b) {
    worker_a->g->elastic_core->cpu_state_group[worker_a->l->elastic_pos_in_cpu_state_group] = worker_b->self;
    worker_a->g->elastic_core->cpu_state_group[worker_b->l->elastic_pos_in_cpu_state_group] = worker_a->self;
    int tmp_pos;
    tmp_pos = worker_a->l->elastic_pos_in_cpu_state_group;
    worker_a->l->elastic_pos_in_cpu_state_group = worker_b->l->elastic_pos_in_cpu_state_group;
    worker_b->l->elastic_pos_in_cpu_state_group = tmp_pos;
}

void elastic_activate_all_sleeping_workers(__cilkrts_worker *w) {
	int i;
    //printf("TEST[%d]: elastic_activate_all_sleeping_workers\n", w->self);
    for (i=0; i<w->g->options.nproc; i++) {
        w->g->workers[i]->l->elastic_s = ACTIVE;
        elastic_do_cond_activate_for_exit(w->g->workers[i]);
    }
}

void program_activate_all_sleeping_workers(platform_program * p) {
    int i;
    //printf("elastic_activate_all_sleeping_workers\n");
    for (i=1; i<p->g->options.nproc; i++) { //do not activate on core 0
        p->g->workers[i]->l->elastic_s = ACTIVE;
        elastic_do_cond_activate_for_exit(p->g->workers[i]);
    }
}

void elastic_do_cond_sleep_pthread_mutex(__cilkrts_worker *w) {
    pthread_mutex_lock(&(w->l->elastic_lock));
    pthread_cond_wait(&(w->l->elastic_cond), &(w->l->elastic_lock));
    pthread_mutex_unlock(&(w->l->elastic_lock));
}

void elastic_do_cond_activate_pthread_mutex(__cilkrts_worker *w) {
    int re;
    pthread_mutex_lock(&(w->l->elastic_lock));
    re = pthread_cond_signal(&(w->l->elastic_cond));
    pthread_mutex_unlock(&(w->l->elastic_lock));
    if (re!=0) {
        printf("activate error %d\n", re);
    }
    
}

void elastic_init_block_pipe(__cilkrts_worker *w) {
    if (pipe(w->l->block_pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
}

void elastic_do_cond_sleep_poll_pipe(__cilkrts_worker *w) {
    int ret;
    struct pollfd pfd[1];
    if (pipe(w->l->block_pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pfd[0].fd = w->l->block_pipe_fd[0];
    pfd[0].events = POLL_IN;
    pfd[0].revents = 0;
    ret = poll(pfd, 1, -1);
    char tmp;
    read(w->l->block_pipe_fd[0], &tmp, 1);
    close(w->l->block_pipe_fd[0]);
    close(w->l->block_pipe_fd[1]);
    if (w->l->elastic_s!=ACTIVATE_REQUESTED) {
        printf("[ERROR]: failed to sleep!\n");
        abort();
    }
}

void elastic_do_cond_activate_poll_pipe(__cilkrts_worker *w) {
    char c = 0;
    write(w->l->block_pipe_fd[1], &c, 1);
}

void elastic_do_cond_sleep(__cilkrts_worker *w) {
    //elastic_do_cond_sleep_poll_pipe(w);
    elastic_do_cond_sleep_pthread_mutex(w);
    if (w->l->elastic_s!=ACTIVATE_REQUESTED) {
        printf("[ERROR]: failed to sleep!\n");
        abort();
    }
}

void elastic_do_cond_activate(__cilkrts_worker *w) {
    //elastic_do_cond_activate_poll_pipe(w);
    elastic_do_cond_activate_pthread_mutex(w);
}

void elastic_do_cond_activate_for_exit(__cilkrts_worker *w) {
    elastic_do_cond_activate(w);
}

bool elastic_safe(__cilkrts_worker *w) {
    return w->l->rts_ctx[0]!=NULL && w->l->rts_ctx[1]!=NULL && w->l->rts_ctx[2]!=NULL;
}

void print_elastic_safe(platform_program* p) {
    int i = 0;
    printf("\t");
    for (i=1; i<p->G->nproc; i++) {
        printf("%d", elastic_safe(p->g->workers[i]));
    }
    printf("\n");
}

void elastic_mugging(__cilkrts_worker *w, int victim){
    ReadyDeque * tmp = w->g->deques[w->self];
    w->g->deques[w->self] = w->g->deques[victim];
    w->g->deques[victim] = tmp;
  
    __cilkrts_stack_frame * volatile *tail_tmp = w->tail;
    w->tail = w->g->workers[victim]->tail;
    w->g->workers[victim]->tail = tail_tmp;

    __cilkrts_stack_frame * volatile * head_tmp = w->head;
    w->head = w->g->workers[victim]->head;
    w->g->workers[victim]->head = head_tmp;

    __cilkrts_stack_frame * volatile * exc_tmp = w->exc;
    w->exc = w->g->workers[victim]->exc;
    w->g->workers[victim]->exc = exc_tmp;

    __cilkrts_stack_frame ** shadow_tmp = w->l->shadow_stack;
    w->l->shadow_stack = w->g->workers[victim]->l->shadow_stack;
    w->g->workers[victim]->l->shadow_stack = shadow_tmp;

    int tmp_provably_good_steal = w->l->provably_good_steal;
    w->l->provably_good_steal = w->g->workers[victim]->l->provably_good_steal;
    w->g->workers[victim]->l->provably_good_steal = tmp_provably_good_steal;

    __cilkrts_stack_frame * tmp_current_stack_frame = w->g->workers[victim]->current_stack_frame;
    w->g->workers[victim]->current_stack_frame = w->current_stack_frame;
    w->current_stack_frame = tmp_current_stack_frame;

    w->current_stack_frame->worker = w;
    w->g->workers[victim]->current_stack_frame->worker = w->g->workers[victim];
    //printf("TEST[%d]: elastic_mugging of worker[%d] finished, %p\n", w->self, victim, w->current_stack_frame);
}

void elastic_worker_request_cpu_to_sleep(__cilkrts_worker *w, int cpu_id) {
    if (w->g->elastic_core->sleep_requests[cpu_id]==0) {
        if (elastic_safe(w->g->workers[cpu_id])) {
            if (__sync_bool_compare_and_swap(&(w->g->workers[cpu_id]->l->elastic_s), ACTIVE, SLEEP_REQUESTED)) {
                if (__sync_bool_compare_and_swap(&(w->g->elastic_core->sleep_requests[cpu_id]), 0, 1)) {
                    printf("Request sleep:\n");
                    w->g->workers[cpu_id]->exc = w->g->workers[cpu_id]->tail + DEFAULT_DEQ_DEPTH; //invoke exception handler
                    printf("TEST[%d]: request worker[%d] goto SLEEP_REQUESTED state, exc:%p\n", w->self, cpu_id, w->g->workers[cpu_id]->exc);
                }
            }
        }
    }
}

void elastic_worker_request_cpu_to_recover(__cilkrts_worker *w, int cpu_id) {
    if (w->g->elastic_core->activate_requests[cpu_id]==0) {
        if (elastic_safe(w->g->workers[cpu_id])) {
            if (__sync_bool_compare_and_swap(&(w->g->workers[cpu_id]->l->elastic_s), SLEEPING_INACTIVE_DEQUE, ACTIVATE_REQUESTED)) {
                if (__sync_bool_compare_and_swap(&(w->g->elastic_core->activate_requests[cpu_id]), 0, 1)) {
                    printf("TEST[%d]: request worker[%d] goto ACTIVATE_REQUESTED state, current_stack_frame:%p\n", w->self, cpu_id, w->g->workers[cpu_id]->current_stack_frame);
                    elastic_do_cond_activate(w->g->workers[cpu_id]);  
                }    
            }
        }
    }
}

void assert_num_ancestor(int assert_spawn_count, int assert_call_count, int assert_call_frame_count) {
    __cilkrts_worker * w = __cilkrts_get_tls_worker();
    Closure *cl = deque_peek_bottom(w, w->self);
    int count_spawn = -1;
    int count_call = -1;
    while(cl!=NULL) {
        cl = cl->spawn_parent;
        count_spawn++;
    }
    cl = deque_peek_bottom(w, w->self);
    while(cl!=NULL) {
        cl = cl->call_parent;
        count_call++;
    }

    int count_call_frame = -1;
    Closure * cl_top = deque_peek_top(w, w->self);
    if (cl_top!=NULL) {
        __cilkrts_stack_frame * cl_frame = cl_top->frame;
        while(cl_frame!=NULL) {
            cl_frame = cl_frame->call_parent;
            count_call_frame++;
        }
    }

    printf("assert_num_ancestor_bottom: spawn parent: %d, call parent: %d, call frame parent: %d\n", count_spawn, count_call, count_call_frame);
    if (count_spawn!=assert_spawn_count || count_call!=assert_call_count || count_call_frame!=assert_call_frame_count) {
        printf("ERROR: num of ancestor is wrong! (spawn:%d, call:%d), should be (%d, %d)\n", count_spawn, count_call, assert_spawn_count, assert_call_count);
        abort();
    }
}


//Choice 1
/*inline int elastic_get_worker_id_sleeping_active_deque(__cilkrts_worker *w) {
    if (w->g->workers[w->g->elastic_core->cpu_state_group[0]]->l->elastic_s==SLEEPING_ACTIVE_DEQUE) { //TODO, Lockfree implementation
        //printf("!!!!elastic_get_worker_id_sleeping_active_deque\n");
        return w->g->elastic_core->cpu_state_group[0];
    } else {
        return -1;
    }
    return -1;
}*/

//Choice 2, best
inline int elastic_get_worker_id_sleeping_active_deque(__cilkrts_worker *w) {
    if (w->g->elastic_core->ptr_sleeping_active_deque>=0) {
        w->l->rand_next = w->l->rand_next * 1103515245 + 12345;
        int id = w->l->rand_next >> 16;
        return w->g->elastic_core->cpu_state_group[id%((w->g->elastic_core->ptr_sleeping_active_deque)+1)];
    } else {
        return -1;
    }
}
//correct standard
/*inline int elastic_get_worker_id_sleeping_active_deque(__cilkrts_worker *w) {
    for (int i=0; i<w->g->program->G->nproc; i++) {
        if (w->g->workers[i]->l->elastic_s==SLEEPING_ACTIVE_DEQUE) {
            return i;
        }
    }
    return -1;
}*/

/*inline int elastic_get_worker_id_sleeping_active_deque(__cilkrts_worker *w) {
    int i = 0;
    while(1) {
        if (w->g->workers[w->g->elastic_core->cpu_state_group[i]]->l->elastic_s==SLEEPING_ACTIVE_DEQUE) { //TODO, Lockfree implementation
            return w->g->elastic_core->cpu_state_group[i];
        } else if (w->g->workers[w->g->elastic_core->cpu_state_group[i]]->l->elastic_s==SLEEPING_MUGGING_DEQUE) {
            i++;
        } else {
            break;
        }
    }
    return -1;
}*/

void print_worker_deque(__cilkrts_worker *w) {
    deque_lock_self(w);
    printf("%p, %p, cl: %p\n", w->tail, w->head, deque_peek_bottom(w, w->self));
    deque_unlock_self(w);
}

void platform_try_sleep_worker(platform_program * p, int cpu_id) {
    if (elastic_safe(p->g->workers[cpu_id])) {
        Cilk_fence();
        if (__sync_bool_compare_and_swap(&(p->g->workers[cpu_id]->l->elastic_s), ACTIVE, SLEEP_REQUESTED)) {
            p->g->workers[cpu_id]->exc = p->g->workers[cpu_id]->tail + DEFAULT_DEQ_DEPTH; //invoke exception handler
        } else if (p->g->workers[cpu_id]->l->elastic_s!=SLEEP_REQUESTED && 
            p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING0 && 
            p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING1 && 
            p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING2 && 
            p->g->workers[cpu_id]->l->elastic_s!=DO_MUGGING) {
            printf("ERROR! set sleep immediately failed in worker %d in e state %d (p:%d, last:%d)\n", 
                cpu_id, p->g->workers[cpu_id]->l->elastic_s, p->control_uid, p->last_do_exit_worker_id);
            abort();
        }
        Cilk_fence();
    } else {
        printf("ERROR! %d, SLEEP FAILED, elastic unsafe! worker is %d\n", p->control_uid, cpu_id);
        abort();
    }
}

void platform_guarantee_sleep_worker(platform_program * p, int cpu_id) {
    int count = 0;
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    if (elastic_safe(p->g->workers[cpu_id])) {
        Cilk_fence();
        while((p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_ACTIVE_DEQUE && 
            p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_INACTIVE_DEQUE && 
            p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_ADAPTING_DEQUE && 
            p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_MUGGING_DEQUE)) {
            if (__sync_bool_compare_and_swap(&(p->g->workers[cpu_id]->l->elastic_s), ACTIVE, SLEEP_REQUESTED)) {
                //printf("\t (p:%d w:%d) guarantee %d worker %d is in ACTIVE state, set as SLEEP_REQUESTED\n", 
                //    w->g->program->control_uid, w->self, p->control_uid, cpu_id);
            }
            //usleep(TIME_MAKE_SURE_TO_SLEEP);
            p->g->workers[cpu_id]->exc = p->g->workers[cpu_id]->tail + DEFAULT_DEQ_DEPTH; //invoke exception handler
            usleep(TIME_MAKE_SURE_TO_SLEEP);
            if (p->is_switching==0 && p->g->workers[cpu_id]->l->elastic_s!=DO_MUGGING) {
                count++;
            }
            if (count>GUARANTEE_PREMPT_WAITING_TIME) {
                printf("ERROR! (p:%d w:%d) guarantee sleep failed in (p:%d w:%d last:%d job_finish:%d hint_stop_container:%d) in e state %d\n", 
                    w->g->program->control_uid, w->self, p->control_uid, cpu_id, p->last_do_exit_worker_id, p->job_finish, p->hint_stop_container, 
                    p->g->workers[cpu_id]->l->elastic_s);
                abort();
            }
            Cilk_fence();
        }
    } else {
        printf("ERROR! %d, SLEEP CHECK FAILED, elastic unsafe! worker is %d\n", p->control_uid, cpu_id);
        abort();
    }
}

void platform_guarantee_sleep_inactive_deque_worker(platform_program * p, int cpu_id) {
    int count = 0;
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    if (elastic_safe(p->g->workers[cpu_id])) {
        Cilk_fence();
        while(p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_INACTIVE_DEQUE) {
            //__sync_bool_compare_and_swap(&(p->g->workers[cpu_id]->l->elastic_s), ACTIVE, SLEEP_REQUESTED);
            //usleep(TIME_MAKE_SURE_TO_SLEEP);
            p->g->workers[cpu_id]->exc = p->g->workers[cpu_id]->tail + DEFAULT_DEQ_DEPTH; //invoke exception handler
            usleep(TIME_MAKE_SURE_TO_SLEEP);
            if (p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_MUGGING_DEQUE && p->g->workers[cpu_id]->l->elastic_s!=SLEEPING_ADAPTING_DEQUE) {
                count++;
            }
            if (count>GUARANTEE_PREMPT_WAITING_TIME) {
                printf("ERROR! (p:%d w:%d) guarantee sleep_inactive_deque failed in (p:%d w:%d last:%d job_finish:%d hint_stop_container:%d) in e state %d)\n", 
                    w->g->program->control_uid, w->self, p->control_uid, cpu_id, p->last_do_exit_worker_id, p->job_finish, p->hint_stop_container, 
                    p->g->workers[cpu_id]->l->elastic_s);
                abort();
            }
            Cilk_fence();
        }
    } else {
        printf("ERROR! %d, SLEEP GUARANTEE FAILED, elastic unsafe! worker is %d\n", p->control_uid, cpu_id);
        abort();
    }
}

void platform_guarantee_activate_worker(platform_program * p, int cpu_id) {
    int count = 0;
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    if (elastic_safe(p->g->workers[cpu_id])) {
        Cilk_fence();
        while(p->g->workers[cpu_id]->l->elastic_s!=ACTIVE) {
            if (__sync_bool_compare_and_swap(&(p->g->workers[cpu_id]->l->elastic_s), SLEEPING_INACTIVE_DEQUE, ACTIVATE_REQUESTED)) {
                usleep(TIME_MAKE_SURE_TO_ACTIVATE); //must before the activate for the efficiency of activation signal delivery
                elastic_do_cond_activate(p->g->workers[cpu_id]);
            } else if (p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING0 && 
                p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING1 && 
                p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING2 && 
                p->g->workers[cpu_id]->l->elastic_s!=DO_MUGGING  && 
                p->g->workers[cpu_id]->l->elastic_s!=ACTIVATE_REQUESTED && 
                p->g->workers[cpu_id]->l->elastic_s!=ACTIVATING &&
                p->g->workers[cpu_id]->l->elastic_s!=ACTIVE) {
                printf("ERROR! (p:%d w:%d) guarantee activate a (p:%d) worker %d in e state %d, not in SLEEPING_INACTIVE_DEQUE state\n", 
                    w->g->program->control_uid, w->self, p->control_uid, cpu_id, p->g->workers[cpu_id]->l->elastic_s);
                abort();
            }
            count++;
            usleep(TIME_MAKE_SURE_TO_ACTIVATE);
            if (count>GUARANTEE_PREMPT_WAITING_TIME) {
                printf("ERROR! (p:%d w:%d) guarantee activate failed in (p:%d w:%d last:%d job_finish:%d hint_stop_container:%d) in e state %d\n", 
                    w->g->program->control_uid, w->self, p->control_uid, cpu_id, p->last_do_exit_worker_id, p->job_finish, p->hint_stop_container, 
                    p->g->workers[cpu_id]->l->elastic_s);
                abort();
            }
            Cilk_fence();
        }
    } else {
        printf("ERROR! %d, ACTIVATE FAILED, elastic unsafe! worker is %d\n", p->control_uid, cpu_id);
        abort();
    }
}

void platform_try_activate_worker(platform_program * p, int cpu_id) {
    if (elastic_safe(p->g->workers[cpu_id])) {
        Cilk_fence();
        if (__sync_bool_compare_and_swap(&(p->g->workers[cpu_id]->l->elastic_s), SLEEPING_INACTIVE_DEQUE, ACTIVATE_REQUESTED)) {
            elastic_do_cond_activate(p->g->workers[cpu_id]);
        } else {
            printf("ERROR! try activate failed in worker %d in e state %d (p:%d, last:%d)\n", 
                cpu_id, p->g->workers[cpu_id]->l->elastic_s, p->control_uid, p->last_do_exit_worker_id);
            abort();
        }
        Cilk_fence();
    } else {
        printf("ERROR! %d, TRY ACTIVATE FAILED, elastic unsafe! worker is %d\n", p->control_uid, cpu_id);
        abort();
    }
}

void platform_guarantee_cancel_worker_sleep(platform_program * p, int cpu_id) {
    int count = 0;
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    if (elastic_safe(p->g->workers[cpu_id])) {
        Cilk_fence();
        while(p->g->workers[cpu_id]->l->elastic_s!=ACTIVE) {
            if (__sync_bool_compare_and_swap(&(p->g->workers[cpu_id]->l->elastic_s), SLEEPING_ACTIVE_DEQUE, ACTIVATE_REQUESTED)) {
                usleep(TIME_MAKE_SURE_TO_ACTIVATE); //must before the activate for the efficiency of activation signal delivery
                elastic_do_cond_activate(p->g->workers[cpu_id]);
            } else if (p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING0 && 
                p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING1 && 
                p->g->workers[cpu_id]->l->elastic_s!=EXIT_SWITCHING2 && 
                p->g->workers[cpu_id]->l->elastic_s!=ACTIVATE_REQUESTED && 
                p->g->workers[cpu_id]->l->elastic_s!=ACTIVATING &&
                p->g->workers[cpu_id]->l->elastic_s!=ACTIVE) {
                printf("ERROR! (p:%d w:%d) guarantee canel sleep failed in worker %d in e state %d (p:%d, last:%d)\n", 
                    w->g->program->control_uid, w->self, cpu_id, p->g->workers[cpu_id]->l->elastic_s, p->control_uid, p->last_do_exit_worker_id);
                abort();
            }
            count++;
            usleep(TIME_MAKE_SURE_TO_ACTIVATE);
            if (count>GUARANTEE_PREMPT_WAITING_TIME) {
                printf("ERROR! (p:%d w:%d) guarantee cancel sleep failed in (p:%d w:%d last:%d job_finish:%d hint_stop_container:%d) in e state %d\n", 
                    w->g->program->control_uid, w->self, p->control_uid, cpu_id, p->last_do_exit_worker_id, p->job_finish, p->hint_stop_container, 
                    p->g->workers[cpu_id]->l->elastic_s);
                abort();
            }
            Cilk_fence();
        }
    } else {
        printf("ERROR! %d, CANCEL SLEEP FAILED, elastic unsafe! worker is %d\n", p->control_uid, cpu_id);
        abort();
    }
}

void platform_rts_srand(platform_global_state * G, unsigned int seed) {
    G->rand_next = seed;
}

void print_cpu_state_group(platform_program * p) {
    int i=0;
    printf("\t(p %d)", p->control_uid);
    for (i=0; i<p->g->options.nproc; i++) {
        printf("%d ", p->g->workers[p->g->elastic_core->cpu_state_group[i]]->l->elastic_s);
    }
    printf("\n");
    //printf("\t(ptr_sleeping_inactive_deque %d)", p->g->elastic_core->ptr_sleeping_inactive_deque);
    //printf("(ptr_sleeping_active_deque %d)\n", p->g->elastic_core->ptr_sleeping_active_deque);
}

void print_cpu_mask(platform_program * p) {
    int i=0;
    printf("\t(p %d)\t", p->control_uid);
    for (i=0; i<p->g->options.nproc; i++) {
        printf("%d", p->cpu_mask[i]);
    }
    printf("\n");
}

void print_try_cpu_mask(platform_program * p) {
    int i=0;
    printf("\t(p %d)", p->control_uid);
    for (i=0; i<p->g->options.nproc; i++) {
        printf("%d", p->try_cpu_mask[i]);
    }
    printf("\n");
}