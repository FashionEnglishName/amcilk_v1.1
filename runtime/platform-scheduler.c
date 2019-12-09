#include "platform-scheduler.h"
#include "membar.h"

void platform_adjust_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) {
    //adjust try_cpu_mask: give up unnecessay cores
    platform_program * tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
        if (tmp_p->desired_num_cpu<tmp_p->try_num_cpu) {
            int tmp_diff_num_cpu = tmp_p->try_num_cpu - tmp_p->desired_num_cpu;
            for (i=0; i<tmp_p->G->nproc; i++) {
                if (tmp_p->try_cpu_mask[i]==1 && tmp_diff_num_cpu>0) {
                    tmp_p->try_cpu_mask[i] = 0;
                    tmp_diff_num_cpu--;
                }
            }
            tmp_p->try_num_cpu = tmp_p->desired_num_cpu;
        }
        tmp_p = tmp_p->next;
    }
    //see idle cores
    for (i=0; i<G->nproc; i++) {
        int flag = 0;
        tmp_p = G->program_head->next;
        while(tmp_p!=NULL) {
            if (tmp_p->try_cpu_mask[i]==1) {
                flag = 1;
            }
            tmp_p = tmp_p->next;
        }
        if (flag==1) {
            //cpu i is idle core
            printf("cpu %d is a idle core\n", i);
        }
    }
}

void platform_determine_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) {
    //printf("\tplatform_determine_scheduling\n");
    platform_program * tmp_p;
    platform_program * tmp_p2;
    int i = 0;
    int try_num_cpu = 0;
    int count = 0;

    //init and set try_num_cpu
    tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
        try_num_cpu = 0;
        for(i=2; i<G->nproc; i++) {
            if(tmp_p->try_cpu_mask[i]==1) {
                try_num_cpu++;
            }
        }
        tmp_p->try_num_cpu = try_num_cpu;
        tmp_p->tmp_num_cpu = 0; //unset tmp_num_cpu
        tmp_p = tmp_p->next;
    }

    //A scheduler may decide to put all core to sleep.
    //In this case, revise the try_cpu_mask to guarantee the invariant is not put to sleep
    tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
        if (tmp_p->try_num_cpu==0) {
            //printf("[WARNING]: p%d revise 0 core allocation\n", tmp_p->control_uid);
            tmp_p->G->macro_test_num_scheduling_revision++;
            tmp_p->try_cpu_mask[tmp_p->invariant_running_worker_id] = 1;
            tmp_p->try_num_cpu = 1;
            tmp_p2 = G->program_head->next;
            while(tmp_p2!=NULL) {
                if (tmp_p2->control_uid!=tmp_p->control_uid) {
                    if (tmp_p2->try_cpu_mask[tmp_p->invariant_running_worker_id]==1) {
                        tmp_p2->try_cpu_mask[tmp_p->invariant_running_worker_id] = 0;
                        tmp_p2->try_num_cpu--;
                    }
                }
                tmp_p2 = tmp_p2->next;
            }
        }
        tmp_p = tmp_p->next;
    }
    //now, try_num_cpu and try_cpu_mask is fixed, 
    //then set cpu_container_map based on try_num_cpu, 
    //and update num_cpu

    //unset cpu map
    for(i=0; i<G->nproc; i++) {
        G->cpu_container_map[i] = -1;
    }

    //reserve for invariant
    tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
        G->cpu_container_map[tmp_p->invariant_running_worker_id] = tmp_p->control_uid;
        //printf("\tp%d set invariant_running_worker_id %d\n", tmp_p->control_uid, tmp_p->invariant_running_worker_id);
        tmp_p->tmp_num_cpu++;
        tmp_p = tmp_p->next;
    }

    //set running container in cpu_container_map
    tmp_p = G->program_head->next;
    int count_running_container = 0;
    while(tmp_p!=NULL) {
        //printf("\tcurrent running p%d, new%d, old%d\n", tmp_p->control_uid, tmp_p->try_num_cpu, tmp_p->num_cpu);
        tmp_p->num_cpu = tmp_p->try_num_cpu;
        //max locality of core assignment
        for(i=2; i<G->nproc; i++) {
            if (tmp_p->tmp_num_cpu>=tmp_p->try_num_cpu) { //num_cpu may > try_num_cpu
                break;
            }
            if (tmp_p->cpu_mask[i]==1 && G->cpu_container_map[i]==-1) {
                G->cpu_container_map[i] = tmp_p->control_uid;
                tmp_p->tmp_num_cpu++;
            }
        }
        //num_cpu may < try_num_cpu
        for(i=2; i<G->nproc; i++) {
            if (tmp_p->tmp_num_cpu>=tmp_p->try_num_cpu) {
                break;
            }
            if (G->cpu_container_map[i]==-1) {
                G->cpu_container_map[i] = tmp_p->control_uid;
                tmp_p->tmp_num_cpu++;
            }
        }
        count_running_container++;

        tmp_p = tmp_p->next;
    }
    //cpu_contain_map is done, then finish allocation by setting cpu_mask

    //see container map
    printf("\tcontainer map: ");
    for(i=0; i<G->nproc; i++) {
        printf("%d ", G->cpu_container_map[i]);
    }
    printf("\n");

    if (count_running_container!=G->nprogram_running) {
        printf("ERROR: inconsistency between scheduler and nprogram_running.\n");
    }

    //set cpu_mask according to cpu_container_map
    //printf("\tset cpu_mask according to cpu_container_map\n");
    tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
        count = 0;
        for(i=2; i<tmp_p->g->options.nproc; i++) {
            if (G->cpu_container_map[i]==tmp_p->control_uid) {
                count++;
                tmp_p->cpu_mask[i] = 1;
            } else {
                tmp_p->cpu_mask[i] = 0;
            }
        }
        //print_cpu_mask(tmp_p);
        if (count==0) {
            printf("[ERROR]: a program has 0 allocated core\n");
            abort();
        }
        tmp_p = tmp_p->next;
    }
}

//interface
void platform_timed_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) {
    if (run_type==TIMED) {
        kunal_adaptive_scheduler(G);
    } else {
        printf("[PLATFORM ERROR5]: type should not be any state other than platform_timed_scheduling\n");
    }
}
void platform_new_job_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) {
    if (run_type==NEW_PROGRAM) {
        platform_scheduler_DREP(G, run_type);
    } else {
        printf("[PLATFORM ERROR5]: type should not be any state other than platform_new_job_scheduling\n");
    }
}
void platform_exit_job_scheduling(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) {
    if (run_type==EXIT_PROGRAM) {
        platform_scheduler_DREP(G, run_type);
    } else {
        printf("[PLATFORM ERROR5]: type should not be any state other than platform_exit_job_scheduling\n");
    }
}

//framework fixed
void platform_scheduling(platform_global_state * G, platform_program * p, enum PLATFORM_SCHEDULER_TYPE run_type) {
    //printf("[SCHEDULING %d]: calculate core assignment in run_type: %d\n", p->control_uid, run_type);
    if (run_type==NEW_PROGRAM) {
        platform_new_job_scheduling(G, run_type);
    } else if (run_type==EXIT_PROGRAM) {
        platform_exit_job_scheduling(G, run_type);
    } else if (run_type==TIMED) {
        platform_timed_scheduling(G, run_type);
    } else {
        printf("[PLATFORM ERROR5]: undefined RUN_TYPE\n");
        abort();
    }
}

void platform_preemption(platform_global_state * G, platform_program * p, enum PLATFORM_SCHEDULER_TYPE run_type) {
    //printf("[SCHEDULING %d]: do scheduling in run_type: %d\n", p->control_uid, run_type);
    platform_adjust_scheduling(G, run_type);
    platform_determine_scheduling(G, run_type);
    platform_program * tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
        if ((tmp_p->periodic==0 || tmp_p->done_one==0)) { //add this for periodic
            container_do_elastic_adaption(tmp_p);
        }
        tmp_p = tmp_p->next;
    }
    //printf("[SCHEDULING %d]: platform_elastic_scheduling done in run_type: %d\n", p->control_uid, run_type);
    if (run_type==NEW_PROGRAM) {
        program_set_platform_preemption_time_ns(p);
    }
}