#include "container.h"
#include "membar.h"

void platform_activate_container(platform_program * new_p) {
    if (new_p->periodic!=1 || new_p->run_times==1) {
        platform_program * p = new_p->G->program_head;
        while (p->next!=NULL) {
            p = p->next;
        }
        p->next = new_p;
        new_p->prev = p;
        new_p->G->nprogram_running++;
    }
}

void platform_deactivate_container(platform_program * p) {
    //for non periodic scheduling, remove p from program_list
    if (p->periodic==0) {
        p->prev->next = p->next;
        if (p->next!=NULL) {
            p->next->prev = p->prev;
        }
        p->next = NULL;
        p->prev = NULL;
        p->G->nprogram_running--;
    }
}

platform_program * get_container(platform_global_state *G, int uid) {
    if (uid!=0) { //if know uid, just return it
        platform_program * np = get_container_by_control_uid(G, uid);
        if (__sync_bool_compare_and_swap(&(np->pickable), 1, 0)) {
            program_set_pick_container_time_ns(np);
            program_set_picked_container_time_ns(np);
            return np;
        } else {
            //printf("ERROR! get_container control_uid is not pickable\n");
            return NULL;
        }
    } else { //if unspec, find a empty one
        int i = 0;
        for (i=0; i<CONTAINER_COUNT; i++) {
            platform_program * np =  G->program_container_pool[i];
            if (__sync_bool_compare_and_swap(&(np->pickable), 1, 0)) {
                program_set_pick_container_time_ns(np);
                //printf("[PICK %d]: pick container\n", np->control_uid);
                int j = 0; //make sure elastic state consistency
                for (j=0; j<np->G->nproc; j++) {
                    while(np->g->workers[j]->l->elastic_s!=SLEEPING_INACTIVE_DEQUE) {
                        //usleep(TIME_MAKE_SURE_STOP_WORKER_SLEEP);
                    }
                }
                program_set_picked_container_time_ns(np);
                return np;
            }
        }
        return NULL;
    }
}

platform_program * get_container_by_control_uid(struct platform_global_state * G, int control_uid) {
    if (control_uid>0 && control_uid<=CONTAINER_COUNT) {
        return G->program_container_pool[control_uid-1];
    } else {
        printf("[ERROR]: get_container_by_control_uid, control_uid %d error!\n", control_uid);
        return NULL;
    }
}

void platform_unset_container_cpu_map(struct platform_global_state * G) {
    platform_program * tmp_p = G->program_head->next;
    int i = 0;
    while(tmp_p!=NULL) {
        for (i=0; i<G->nproc; i++) {
            G->cpu_container_map[i] = -1;
        }
        tmp_p = tmp_p->next;
    }
}

void platform_set_container_cpu_map(struct platform_global_state * G) {
    platform_unset_container_cpu_map(G);
    platform_program * tmp_p = G->program_head->next;
    int i = 0;
    while(tmp_p!=NULL) {
        for (i=0; i<G->nproc; i++) {
            if (tmp_p->cpu_mask[i]==1) {
                G->cpu_container_map[i] = tmp_p->control_uid;
            }
        }
        tmp_p = tmp_p->next;
    }
}

void container_set_init_run(platform_program * p) {
    int i;
    p->g->workers[0]->l->run_at_beginning = 0; //cpu 0 is ocuppied by main thread scheduler
    p->g->workers[1]->l->run_at_beginning = 0; //cpu 1 is ocuppied by task generator
    for (i=2; i<p->g->options.nproc; i++) {
        p->g->workers[i]->l->run_at_beginning = 1;//p->cpu_mask[i];
    }
}

void container_setup_to_run(platform_program * p, platform_program_request* pr) {
    //set container
    container_set_by_request(p, pr);
    platform_deinit_program_request(pr);
    p->G->new_program = p;
    p->run_times++; //must before platform_activate_container
    p->done_one = 0;
}

void print_all_last_exit_cpu_id(platform_global_state * G) {
    int i = 0;
    printf("\tprint_all_last_exit_cpu_id: ");
    for (i=0; i<CONTAINER_COUNT; i++) {
        printf("%d invariant: %d", G->program_container_pool[i]->last_do_exit_worker_id, G->program_container_pool[i]->invariant_running_worker_id);
    }
    printf("\n");
}

void container_block(__cilkrts_worker * w) {
    w->g->program->hint_stop_container = 1;
    Cilk_fence();

    //wait until all other workers are sleeping
    program_set_begin_exit_time_ns(w->g->program);
    int i = 2;
    for (i=2; i<w->g->options.nproc; i++) {
        if (i!=w->self) {
            platform_guarantee_sleep_inactive_deque_worker(w->g->program, i);
        }
    }
    if (w->g->program->G->nprogram_running!=0) {
        platform_preemption(w->g->program->G, w->g->program, EXIT_PROGRAM);
    }
    program_set_sleeped_all_other_workers_time_ns(w->g->program);

    //sleep section
    if (__sync_bool_compare_and_swap(&(w->l->elastic_s), ACTIVE, SLEEPING_INACTIVE_DEQUE)) {
        elastic_core_lock(w);
        w->g->elastic_core->ptr_sleeping_inactive_deque--;
        elastic_do_exchange_state_group(w, w->g->workers[w->g->elastic_core->cpu_state_group[w->g->elastic_core->ptr_sleeping_inactive_deque]]);
        elastic_core_unlock(w); //Zhe: Change
        //exit scheduling
        if (__sync_bool_compare_and_swap(&(w->g->program->pickable), 0, 1)) {
            printf("[STOP CONTAINER %d] (pickable: %d): invariant %d sleep! estate is %d\n", w->g->program->control_uid, w->g->program->pickable, w->self, w->l->elastic_s);
            w->g->program->G->macro_test_num_stop_container++;
            pthread_mutex_unlock(&(w->g->program->G->lock));
            //pthread_spin_unlock(&(w->g->program->G->lock));
            //printf("[G LOCK]: %d RELEASE the G_lock\n", w->g->program->control_uid);
            elastic_do_cond_sleep(w);

            //activated
            w = __cilkrts_get_tls_worker();
            pthread_mutex_lock(&(w->g->program->G->lock));
            //pthread_spin_lock(&(w->g->program->G->lock));
            //printf("[G LOCK]: %d GET the G_lock\n", w->g->program->control_uid);
        } else {
            printf("[BUG %d] when stop, pickable state is error!\n", w->g->program->control_uid);
            abort();
        }
    } else {
        printf("BUG: someone put invariant %d (e state %d) to sleep\n", w->self, w->l->elastic_s);
        abort();
    }
    //

    //new run preparation
    w = __cilkrts_get_tls_worker();
    if (__sync_bool_compare_and_swap(&(w->l->elastic_s), ACTIVATE_REQUESTED, ACTIVATING)) {
        elastic_core_lock(w);
        elastic_do_exchange_state_group(w, w->g->workers[w->g->elastic_core->cpu_state_group[w->g->elastic_core->ptr_sleeping_inactive_deque]]);
        w->g->elastic_core->ptr_sleeping_inactive_deque++;
        elastic_core_unlock(w);
        if (__sync_bool_compare_and_swap(&(w->l->elastic_s), ACTIVATING, ACTIVE)) {
            usleep(10000);
            w->g->program->hint_stop_container = 0;
            //printf("Activated!\n");
        }
    } else {
        printf("[BUG %d] inv sleeping failed when block!\n", w->g->program->control_uid);
        abort();
    }
    //
}

void container_do_elastic_adaption(platform_program * p) {
    unsigned long long t0, t1, t2, t3, t4;
    //Here, the last time of container_do_elastic_adaption in Cilk runtime system per program should be finished
    t0 = micro_get_clock();
    elastic_core_lock_g(p->g);
    int active_worker_id_begin = p->g->elastic_core->ptr_sleeping_active_deque + 1;
    int active_worker_id_end = p->g->elastic_core->ptr_sleeping_inactive_deque - 1;
    elastic_core_unlock_g(p->g);
    //printf("\t%d container_do_elastic_adaption, get ptr done\n", p->control_uid);
    if (p->G->new_program!=NULL) {
        //printf("\t[container_do_elastic_adaption] new program is %d\n", p->G->new_program->control_uid);
    }
    //print_cpu_mask(p);
    print_cpu_state_group(p);
    t1 = micro_get_clock();
    int i;
    if (p->cpu_mask[0]==1) {
        printf("[PLATFORM ERROR]: a program is set to run on core 0, abort it\n");
        abort();
    }
    if (p->cpu_mask[1]==1) {
        printf("[PLATFORM ERROR]: a program is set to run on core 1, abort it\n");
        abort();
    }
    //printf("\t%d %d container_do_elastic_adaption, get ptr done\n", p->control_uid, p->input);
    for (i=2; i<p->g->options.nproc; i++) {
        if (p->g->workers[i]->l->elastic_pos_in_cpu_state_group>=active_worker_id_begin && p->g->workers[i]->l->elastic_pos_in_cpu_state_group<=active_worker_id_end && p->cpu_mask[i]==0 && p->g->workers[i]->l->elastic_s==ACTIVE) { //currently, cpu i is active and need to be inactive
            platform_try_sleep_worker(p, i);
        }
    }
    //printf("\t%d(%d) scheduling request sleep first attempts done\n", p->control_uid, p->input);
    t2 = micro_get_clock();
    for (i=2; i<p->g->options.nproc; i++) {
        if (p->g->workers[i]->l->elastic_pos_in_cpu_state_group>=active_worker_id_begin && p->g->workers[i]->l->elastic_pos_in_cpu_state_group<=active_worker_id_end && p->cpu_mask[i]==0) { //currently, cpu i is active and need to be inactive
            platform_guarantee_sleep_worker(p, i);
        }
    }
    t3 = micro_get_clock();
    //printf("\t%d(%d) scheduling check sleeping cores done\n", p->control_uid, p->input);
    for (i=2; i<p->g->options.nproc; i++) {
        if (p->g->workers[i]->l->elastic_pos_in_cpu_state_group<active_worker_id_begin && p->cpu_mask[i]==1) { //currently, cpu i is inactive and need to be active
            platform_guarantee_cancel_worker_sleep(p, i);
        } else if (p->g->workers[i]->l->elastic_pos_in_cpu_state_group>active_worker_id_end && p->cpu_mask[i]==1) { //currently, cpu i is inactive and need to be active
            platform_guarantee_activate_worker(p, i);
        }
    }
    t4 = micro_get_clock();
    //printf("\t%d %d scheduling activating cores done\n", p->control_uid, p->input);
    print_cpu_state_group(p);
    //printf("%f, %f, %f, %f\n", (t1-t0)/1000/1000/1000.0, (t2-t1)/1000/1000/1000.0, (t3-t2)/1000/1000/1000.0, (t4-t3)/1000/1000/1000.0);
}