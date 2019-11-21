#include "platform_sched_CFS.h"

void platform_scheduler_CFS(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) { //before new program running or after a program finishes
    int cpu_running_count = G->nproc - 2; //core 0 is running for the scheduler so it does not counted.
    int average_program_core_count = 0;
    int remaining_core_count = 0;
    int i, j;
    if (run_type==NEW_PROGRAM || run_type==EXIT_PROGRAM) {
        if (G->nprogram_running!=0) {
            average_program_core_count = (int)(cpu_running_count/(G->nprogram_running)); //no next program.
            remaining_core_count = G->nproc - average_program_core_count*G->nprogram_running;
        } else {
            printf("[PLATFORM ERROR0]: G->nprogram_running is 0 when exit, no need do platform_scheduler_CFS\n");
        }
    } else {
        printf("[PLATFORM ERROR1]: wrong input of run_type in platform_scheduler_CFS\n");
    }
    printf("average_program_core_count is %d\n", average_program_core_count);
    //printf("[PLATFORM]: running core count is %d, average core count is %d, remaining core count is %d\n", cpu_running_count, average_program_core_count, remaining_core_count);
    //assign cores for previous programs
    platform_program * p = G->program_head->next;
    j = 0;
    while(p!=NULL) {
        if (p->done_one==0) {
            p->cpu_mask[0] = 0; //core 0 is occupied by main thread platform scheduler
            p->cpu_mask[1] = 0; //core 1 is occupied by task generator
            for (i=2; i<G->nproc; i++) {
                if (i<=((j+1)*average_program_core_count)-1+2 && i>=(j*average_program_core_count)+2) {
                    p->cpu_mask[i] = 1;
                } else {
                    p->cpu_mask[i] = 0;
                }
            }
            j++;
        }
        p = p->next;
        //printf("[PLATFORM]: a program runs on cores from %d to %d\n", (j*average_program_core_count)+1, ((j+1)*average_program_core_count)-1+1);
    }
    //assign cores for the new program
    if (run_type==NEW_PROGRAM) {
        if (G->new_program->control_uid==0||G->new_program->run_times==1) {
            G->new_program->cpu_mask[0] = 0; //core 0 is occupied by main thread platform scheduler
            G->new_program->cpu_mask[1] = 0; //core 1 is occupied by task generator
            for (i=2; i<G->nproc; i++) {
                if (i>=(G->nprogram_running*average_program_core_count)+2 && i<=((G->nprogram_running+1)*average_program_core_count)-1+2) {
                    G->new_program->cpu_mask[i] = 1;
                } else {
                    G->new_program->cpu_mask[i] = 0;
                }
            }
            //printf("[PLATFORM]: a program runs on cores from %d to %d\n", (G->nprogram_running*average_program_core_count)+1, ((G->nprogram_running+1)*average_program_core_count)-1+1);
        }
    } else if (run_type==EXIT_PROGRAM) {
        //do nothing
    } else {
        printf("[PLATFORM ERROR2]: wrong input of run_type in platform_scheduler_CFS\n");
    }
}
