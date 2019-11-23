#include "server-support.h"

pthread_mutex_t timed_scheduling_sleep_lock;
pthread_cond_t timed_scheduling_sleep_cond;

//Sub service functions
//plaform request receiver functions
void * main_thread_new_program_receiver(void * arg) {
    platform_global_state * G = (platform_global_state *)arg;

    //server start
    while(1) {
        int program_id, input, control_uid, second_level_uid, periodic, mute;
        unsigned long long max_period_s, max_period_us, min_period_s, min_period_us, C_s, C_us, L_s, L_us;
        float elastic_coefficient;

        int ret_fscanf;
        //Get request
        FILE *fp;
        //printf("waiting for FIFO input...\n");
        fp = fopen(SERVER_FIFO_NAME, "r");
        if (fp == NULL) {
            printf("ERROR: fopen failed! Reason: %s\n", strerror(errno));
            exit(-1);
        }
        ret_fscanf = fscanf(fp, "%d %d %d %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %f", &program_id, &input, &control_uid, &second_level_uid, &periodic, &mute, &max_period_s, &max_period_us, &min_period_s, &min_period_us, &C_s, &C_us, &L_s, &L_us, &elastic_coefficient);
        if (ret_fscanf == EOF) {
            //printf("End of fifo\n");
            continue;
        }
        if (ret_fscanf < -1) {
            printf("fscanf failed! Reason: %s", strerror(errno));
            exit(-1);
        }
        fclose(fp);
        //printf("[NEW REQUEST:] run program %d with input %d\n", program_id, input);
        platform_program_request * new_request = platform_init_program_request(G, program_id, input, control_uid, second_level_uid, periodic, mute, max_period_s, max_period_us, min_period_s, min_period_us, C_s, C_us, L_s, L_us, elastic_coefficient);
        program_set_requested_time_ns(new_request);
        platform_append_program_request(new_request);
        //printf("buffer size: %d\n", get_count_program_request_buffer(G));

        usleep(TIME_REQUEST_RECEIVE_INTERVAL);
    }
    return 0;
}

//plaform trigger idle container if necessary
void * main_thread_thread_container_trigger(void * arg) {
    platform_global_state * G = (platform_global_state *)arg;
    platform_program_request * old_pr = NULL;
    platform_program_request * pr = NULL;
    int i = 0;
    int control_uid = 0;
    while(1) {
        //see local buffer
        for (i=0; i<CONTAINER_COUNT; i++) {
            control_uid = i + 1;
            pr = platform_peek_first_request(G, control_uid);
            if (pr!=NULL) {
                platform_program* p = get_container(G, pr->control_uid);
                if (p!=NULL) {
                    platform_guarantee_activate_worker(p, p->invariant_running_worker_id);
                }
                usleep(TIME_CONTAINER_TRIGGER_INTERVAL);
            }
        }

        //see global buffer
        pr = platform_peek_first_request(G, 0);
        if (pr==old_pr && pr!=NULL) {
            platform_program* p = get_container(G, pr->control_uid);
            if (p!=NULL) {
                platform_guarantee_activate_worker(p, p->invariant_running_worker_id);
            }
            usleep(TIME_CONTAINER_TRIGGER_INTERVAL);
        }
        old_pr = pr;
    }
    return 0;
}

//support timed scheduling
void init_timed_sleep_lock_and_cond() {
    pthread_mutex_init(&(timed_scheduling_sleep_lock), NULL);
    pthread_cond_init(&(timed_scheduling_sleep_cond), NULL);
}

void* main_thread_timed_scheduling(void * arg) {
    platform_global_state * G = (platform_global_state *)arg;
    while (G->enable_timed_scheduling==1) {
        pthread_mutex_lock(&(G->lock));
        //pthread_spin_lock(&(G->lock));
        platform_scheduling(G, NULL, TIMED);
        platform_preemption(G, NULL, TIMED);
        pthread_mutex_unlock(&(G->lock));
        //pthread_spin_unlock(&(G->lock));
        //Do sleep
        pthread_mutex_lock(&(timed_scheduling_sleep_lock));
        pthread_cond_wait(&(timed_scheduling_sleep_cond), &(timed_scheduling_sleep_lock));
        pthread_mutex_unlock(&(timed_scheduling_sleep_lock));
    }
    return 0;
}

void set_timer(struct itimerval * itv, struct itimerval * oldtv, int sec, int usec) {
    itv->it_interval.tv_sec = sec;
    itv->it_interval.tv_usec = usec;
    itv->it_value.tv_sec = sec;
    itv->it_value.tv_usec = usec;
    setitimer(ITIMER_REAL, itv, oldtv);
}

void timed_scheduling_signal_handler(int n) {
    pthread_mutex_lock(&(timed_scheduling_sleep_lock));
    pthread_cond_signal(&(timed_scheduling_sleep_cond));
    pthread_mutex_unlock(&(timed_scheduling_sleep_lock));
}

//handle pip error
void pipe_sig_handler(int s) {
    //do nothing when pipe broken
}

//response to client
void platform_response_to_client(platform_program * p) {
    char program_fifo_path[BUF_SIZE];
    char str[BUF_SIZE];
    int fd;
    sprintf(program_fifo_path, "%s_%d_%d", PROGRAM_FIFO_NAME_BASE, p->control_uid, p->second_level_uid);
    sprintf(str, "%d %f %llu %llu", p->g->cilk_main_return, (float)((p->end_time - p->resume_time + p->pause_time - p->begin_time)/1000000000.0), p->next_period_feedback_s, p->next_period_feedback_us);
    fd = open(program_fifo_path, O_WRONLY);
    write(fd, str, strlen(str)+1);
    close(fd);
}

//request buffer
platform_program_request * platform_init_program_request_head(struct platform_global_state * G) {
    platform_program_request * pr = (platform_program_request *) malloc(sizeof(platform_program_request));
    pr->G = G;
    pr->next = NULL;
    pr->prev = NULL;
    pr->program_id = 0;
    pr->input = 0;
    pr->periodic = 0;
    pr->mute = 0;
    pr->max_period_s = 0;
    pr->max_period_us = 0;
    pr->min_period_s = 0;
    pr->min_period_us = 0;
    pr->C_s = 0;
    pr->C_us = 0;
    pr->L_s = 0;
    pr->L_us = 0;
    pr->elastic_coefficient = 0;
    pr->requested_time = 0;
    return pr;
}

platform_program_request * platform_init_program_request(struct platform_global_state * G, 
    int program_id, int input, int control_uid, int second_level_uid, int periodic, int mute, 
    unsigned long long max_period_s, unsigned long long max_period_us, unsigned long long min_period_s, unsigned long long min_period_us, 
    unsigned long long C_s, unsigned long long C_us, unsigned long long L_s, unsigned long long L_us, float elastic_coefficient) {

    platform_program_request * pr = (platform_program_request *) malloc(sizeof(platform_program_request));
    pr->G = G;
    pr->next = NULL;
    pr->prev = NULL;
    pr->program_id = program_id;
    pr->input = input;
    pr->control_uid = control_uid;
    pr->second_level_uid = second_level_uid;
    pr->periodic = periodic;
    pr->mute = mute;
    pr->max_period_s = max_period_s;
    pr->max_period_us = max_period_us;
    pr->min_period_s = min_period_s;
    pr->min_period_us = min_period_us;
    pr->C_s = C_s;
    pr->C_us = C_us;
    pr->L_s = L_s;
    pr->L_us = L_us;
    pr->elastic_coefficient = elastic_coefficient;
    pr->requested_time = 0;
    return pr;
}

void platform_append_program_request(platform_program_request * new_pr) {
    if (new_pr->control_uid==0) {
        pthread_spin_lock(&(new_pr->G->request_buffer_lock));
        platform_program_request * pr = new_pr->G->request_buffer_head;
        while(pr->next!=NULL) {
            pr = pr->next;
        }
        pr->next = new_pr;
        new_pr->prev = pr;
        pthread_spin_unlock(&(new_pr->G->request_buffer_lock));
    } else {
        platform_program * p = get_container_by_control_uid(new_pr->G, new_pr->control_uid);
        pthread_spin_lock(&(p->request_buffer_lock));
        platform_program_request * pr = p->request_buffer_head;
        while(pr->next!=NULL) {
            pr = pr->next;
        }
        pr->next = new_pr;
        new_pr->prev = pr;
        pthread_spin_unlock(&(p->request_buffer_lock));
    }
}

struct platform_program_request * platform_pop_first_request(struct platform_global_state * G, int control_uid) {
    if (control_uid==0) {
        pthread_spin_lock(&(G->request_buffer_lock));
        platform_program_request * pr_head = G->request_buffer_head;
        if (pr_head->next==NULL) {
            pthread_spin_unlock(&(G->request_buffer_lock));
            return NULL;
        } else {
            platform_program_request * result = pr_head->next;
            pr_head->next = result->next;
            if (result->next!=NULL) {
                result->next->prev = pr_head;
            }
            pthread_spin_unlock(&(G->request_buffer_lock));
            return result; //if buffer is empty, return NULL
        }
    } else {
        platform_program * p = get_container_by_control_uid(G, control_uid);
        pthread_spin_lock(&(p->request_buffer_lock));
        platform_program_request * pr_head = p->request_buffer_head;
        if (pr_head->next==NULL) {
            pthread_spin_unlock(&(p->request_buffer_lock));
            return NULL;
        } else {
            platform_program_request * result = pr_head->next;
            pr_head->next = result->next;
            if (result->next!=NULL) {
                result->next->prev = pr_head;
            }
            pthread_spin_unlock(&(p->request_buffer_lock));
            return result; //if buffer is empty, return NULL
        }
    }
}

struct platform_program_request * platform_peek_first_request(struct platform_global_state * G, int control_uid) {
    if (control_uid==0) {
        return G->request_buffer_head->next;
    } else {
        platform_program * p = get_container_by_control_uid(G, control_uid);
        return p->request_buffer_head->next;
    }
}

int platform_program_request_buffer_is_empty(struct platform_global_state * G, int control_uid) {
    if (control_uid==0) {
        pthread_spin_lock(&(G->request_buffer_lock));
        platform_program_request * pr_head = G->request_buffer_head;
        if (pr_head->next==NULL) {
            pthread_spin_unlock(&(G->request_buffer_lock));
            return 1;
        } else {
            pthread_spin_unlock(&(G->request_buffer_lock));
            return 0;
        }
    } else {
        platform_program * p = get_container_by_control_uid(G, control_uid);
        pthread_spin_lock(&(p->request_buffer_lock));
        platform_program_request * pr_head = p->request_buffer_head;
        if (pr_head->next==NULL) {
            pthread_spin_unlock(&(p->request_buffer_lock));
            return 1;
        } else {
            pthread_spin_unlock(&(p->request_buffer_lock));
            return 0;
        }
    }
}

void platform_deinit_program_request(platform_program_request * pr) {
    free(pr);
}

int get_count_program_request_buffer(struct platform_global_state * G, int control_uid) {
    if (control_uid==0) {
        pthread_spin_lock(&(G->request_buffer_lock));
        platform_program_request * pr = G->request_buffer_head->next;
        int i = 0;
        while(pr!=NULL) {
            i++;
            pr = pr->next;
        }
        pthread_spin_unlock(&(G->request_buffer_lock));
        return i;
    } else {
        platform_program * p = get_container_by_control_uid(G, control_uid);
        pthread_spin_lock(&(p->request_buffer_lock));
        platform_program_request * pr = p->request_buffer_head->next;
        int i = 0;
        while(pr!=NULL) {
            i++;
            pr = pr->next;
        }
        pthread_spin_unlock(&(p->request_buffer_lock));
        return i;
    }
}

void container_plugin_enable_run_cycle(__cilkrts_worker * w) {
    //printf("[G LOCK]: %d TO GET the G_lock\n", w->g->program->control_uid);
    pthread_mutex_lock(&(w->g->program->G->lock));
    //pthread_spin_lock(&(w->g->program->G->lock));
    //printf("[G LOCK]: %d GET the G_lock\n", w->g->program->control_uid);
    w->g->program->done_one = 1;
    w->g->program->G->most_recent_stop_program_cpumask = w->g->program->cpu_mask;
    w->g->program->G->stop_program = w->g->program;

    //print_num_ancestor();

    //exit scheduling
    platform_deactivate_container(w->g->program);//1122
    if (w->g->program->G->nprogram_running!=0) {
        platform_scheduling(w->g->program->G, w->g->program, EXIT_PROGRAM);
    }

    platform_program_request * first_request;
new_point:
    first_request = platform_pop_first_request(w->g->program->G, w->g->program->control_uid);
    if (first_request==NULL) {
        first_request = platform_pop_first_request(w->g->program->G, 0);
        if (first_request!=NULL) {
            printf("[NEW CONTAINER %d] input: %d\n", w->g->program->control_uid, w->g->program->input);
            platform_activate_container(w->g->program);//1122
            container_setup_to_run(w->g->program, first_request);
            program_set_activate_container_time_ns(w->g->program);
            platform_scheduling(w->g->program->G, w->g->program, NEW_PROGRAM);
            program_set_core_assignment_time_when_run_ns(w->g->program);
            platform_preemption(w->g->program->G, w->g->program, NEW_PROGRAM);
        } else {
            //printf("[BLOCK CONTAINER %d] no request, enter block\n", w->g->program->control_uid);
            container_block(w);
            goto new_point;
        }
    } else {
        container_setup_to_run(w->g->program, first_request);
        program_set_activate_container_time_ns(w->g->program);
        platform_scheduling(w->g->program->G, w->g->program, NEW_PROGRAM);
        program_set_core_assignment_time_when_run_ns(w->g->program);
        platform_preemption(w->g->program->G, w->g->program, NEW_PROGRAM);
    }
    //pthread_spin_unlock(&(w->g->program->G->lock));
    pthread_mutex_unlock(&(w->g->program->G->lock));
    //printf("[G LOCK]: %d RELEASE the G_lock\n", w->g->program->control_uid);
}