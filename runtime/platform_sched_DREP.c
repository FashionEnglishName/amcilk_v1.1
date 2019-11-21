#include "platform_sched_DREP.h"

unsigned int platform_scheduler_rand(platform_global_state * G) {
    G->rand_next = G->rand_next * 1103515245 + 12345;
    return (G->rand_next >> 16);
}

platform_program * rand_pick_unfinished_job(platform_global_state * G) {
    int num;
    int i = 0;
    num = platform_scheduler_rand(G)%(G->nprogram_running);
    //printf("[platform_scheduler_DREP]: finish, the number is %d\n", num);
    platform_program * p = G->program_head->next;
    while(p!=NULL) {
        //if (p->done_one==0) {
            if (i==num) {
                return p;
            }
            i++;
        //}
        p = p->next;
    }
    printf("[PLATFORM ERROR4]: BUG: in rand_pick_unfinished_job, nprogram_running is incorrect!\n");
    abort();
    return NULL;
}

void platform_scheduler_DREP(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) { //To add lock
    int i;
    //unsigned long long begin, end;
    //begin = micro_get_clock();
    if (run_type==NEW_PROGRAM) {
        for(i=0; i<G->nproc; i++) {
             G->new_program->try_cpu_mask[i] = 0;
        }
        G->new_program->try_cpu_mask[0] = 0; //core0 is used for scheduling and not available
        G->new_program->try_cpu_mask[1] = 0; //core1 is used for task generator and not available
        if (G->nprogram_running!=0) { //if running programs exist
            for (i=2; i<G->nproc; i++) {
                int num = platform_scheduler_rand(G)%(G->nprogram_running) + 1;  //the new program has not registered yet
                //printf("[platform_scheduler_DREP]: run, the number is %d\n", num);
                if (num==1) {
                    G->new_program->try_cpu_mask[i] = 1; //make core i active for the new program
                    //printf("[platform_scheduler_DREP]: pick core %d and assign it to the new program\n", i);
                    platform_program * p = G->program_head->next;
                    while(p!=NULL) {
                        if (p->control_uid!=G->new_program->control_uid) {
                            p->try_cpu_mask[i] = 0; //make core i sleep for all previous programs
                        }
                        p = p->next;
                    }
                } else {
                    G->new_program->try_cpu_mask[i] = 0; //make core i sleep for the new program
                }
            }
        } else { //if no running program exists, run on all cores except for core0 and core1
            for (i=2; i<G->nproc; i++) {
                G->new_program->try_cpu_mask[i] = 1;
            }
        }
    } else if (run_type==EXIT_PROGRAM) {
        //printf("\texit program! %d\n", G->stop_program->control_uid);
        //print_try_cpu_mask(G->stop_program);
        for (i=2; i<G->nproc; i++) {
            if (G->stop_program->try_cpu_mask[i]==1) { //core i was used in the finished program
                platform_program * p = rand_pick_unfinished_job(G);
                if (p!=NULL) {

                    p->try_cpu_mask[i] = 1;
                    //printf("\t[platform_scheduler_DREP]: give core %d to %d after exit\n", i, p->control_uid);
                } else {
                    printf("[ERROR]: FAILED1 TO GIVE CORES\n");
                    abort();
                }
            }
            //G->program_container_pool[G->most_recent_stop_container-1]->try_cpu_mask[i] = 0;
        }
    } else {
        printf("[PLATFORM ERROR3]: wrong input of run_type in platform_scheduler_DREP\n");
        abort();
    }

    platform_program * tmptmpp = G->program_head->next;
    int cpu_count = 0;
    while(tmptmpp!=NULL) {
        //print_try_cpu_mask(tmptmpp);
        for(i=0; i<G->nproc; i++) {
            if (tmptmpp->try_cpu_mask[i]==1) {
                cpu_count++;
            }
        }
        tmptmpp = tmptmpp->next;
    }
    /*printf("\tDREP assigned cpu count: %d, run type: %d\n", cpu_count, run_type);
    if (G->new_program!=NULL) {
        printf("\tnew p: %d\n", G->new_program->control_uid);
    }*/
    //end = micro_get_clock();
    //printf("DREP: %f\n", (end-begin)/1000/1000/1000.0);
}