#include "platform-scheduler.h"

void platform_scheduler_task_compress_par2(platform_global_state * G, enum PLATFORM_SCHEDULER_TYPE run_type) {
    if (run_type==NEW_PROGRAM) {
        /*if (G->new_program->run_times!=1) { //only do schedule for the first run
            return;
        }*/
        //do calculation
        platform_program * p = G->program_head->next;
        int M = G->nproc - 2; //can not assign core 0 and core 1 to any program
        int N = 0;
        while(p!=NULL) {
            M = M - p->num_core_min;
            printf("\tM %d, num_core_min %d\n", M, p->num_core_min);
            p->flag_for_task_compress_par2 = 1;
            N++; //num of previous programs
            p = p->next;
        }
        N++; //num of previous programs + new one
        if (M<0) {
            int i = 0;
            for (i=0; i<G->nproc; i++) {
                G->new_program->cpu_mask[i] = 0;
            }
            printf("[Unschedulable]: make the new one sleeping! M: %d, G->new_program->num_core_min: %d\n", M, G->new_program->num_core_min);
        } else if (M!=0) {
            //init
            p = G->program_head->next;
            while(p!=NULL) {
                p->m_for_task_compress_par2 = p->num_core_min;
                p->t_s_for_task_compress_par2 = 0;
                p->t_us_for_task_compress_par2 = 0;
                p->z_for_task_compress_par2 = 0;
                p = p->next;
            }
            //go to elastic optimization on p->m_for_task_compress_par2
            p = G->program_head->next;
            while(p!=NULL) {
                int idx = p->m_for_task_compress_par2 - 1;
                float x = (1.0/p->elastic_coefficient) * 
                    (p->utilization_max- (1.0*(p->C_s*1000*1000.0+p->C_us)/(p->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p->t_us_dict_for_task_compress_par2[idx]))) * 
                    (p->utilization_max- (1.0*(p->C_s*1000*1000.0+p->C_us)/(p->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p->t_us_dict_for_task_compress_par2[idx])));
                p->m_for_task_compress_par2++;
                idx = p->m_for_task_compress_par2 - 1;
                float y = (1.0/p->elastic_coefficient) * 
                    (p->utilization_max- (1.0*(p->C_s*1000*1000.0+p->C_us)/(p->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p->t_us_dict_for_task_compress_par2[idx]))) * 
                    (p->utilization_max- (1.0*(p->C_s*1000*1000.0+p->C_us)/(p->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p->t_us_dict_for_task_compress_par2[idx])));
                p->z_for_task_compress_par2 = x - y;
                p->m_for_task_compress_par2--;
                p = p->next;
            }
            int heap_flag = N;
            platform_program * p_max_z;
            while(M>0 && heap_flag>0) {
                float max_z = -1;
                //find p of max z among previous programs and the new one
                p = G->program_head->next;
                while(p!=NULL) {
                    //printf("control_uid %d, z_for_task_compress_par2 %f > max_z: %f, flag_for_task_compress_par2 %d==1\n", p->control_uid, p->z_for_task_compress_par2, max_z, p->flag_for_task_compress_par2);
                    if (p->z_for_task_compress_par2>max_z && p->flag_for_task_compress_par2==1) {
                        max_z = p->z_for_task_compress_par2;
                        p_max_z = p;
                    }
                    p = p->next;
                }
                if (p_max_z!=NULL) {
                    //printf("!!!!!!p %d has z %f\n", p_max_z->control_uid, max_z);
                    //printf("!!!!!!p %d has z %f, current z max is %f\n", G->new_program->control_uid, G->new_program->z_for_task_compress_par2, max_z);
                }
                //
                //permanently assign the core 
                p_max_z->m_for_task_compress_par2++;
                M--;
                //printf("!!!permanently assign a core to program %d, then it has core %d\n", p_max_z->control_uid, p_max_z->m_for_task_compress_par2);
                p_max_z->t_s_for_task_compress_par2 = p_max_z->t_s_dict_for_task_compress_par2[p_max_z->m_for_task_compress_par2];
                p_max_z->t_us_for_task_compress_par2 = p_max_z->t_us_dict_for_task_compress_par2[p_max_z->m_for_task_compress_par2];
                //
                if (M>0 && p_max_z->m_for_task_compress_par2 < p_max_z->num_core_max) {
                    int idx = p_max_z->m_for_task_compress_par2 - 1;
                    float x = (1.0/p_max_z->elastic_coefficient) * 
                        (p_max_z->utilization_max- (1.0*(p_max_z->C_s*1000*1000.0+p_max_z->C_us)/(p_max_z->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p_max_z->t_us_dict_for_task_compress_par2[idx]))) * 
                        (p_max_z->utilization_max- (1.0*(p_max_z->C_s*1000*1000.0+p_max_z->C_us)/(p_max_z->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p_max_z->t_us_dict_for_task_compress_par2[idx])));
                    p_max_z->m_for_task_compress_par2++;
                    idx = p_max_z->m_for_task_compress_par2 - 1;
                    float y = (1.0/p_max_z->elastic_coefficient) * 
                        (p_max_z->utilization_max- (1.0*(p_max_z->C_s*1000*1000.0+p_max_z->C_us)/(p_max_z->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p_max_z->t_us_dict_for_task_compress_par2[idx]))) * 
                        (p_max_z->utilization_max- (1.0*(p_max_z->C_s*1000*1000.0+p_max_z->C_us)/(p_max_z->t_s_dict_for_task_compress_par2[idx]*1000*1000.0+p_max_z->t_us_dict_for_task_compress_par2[idx])));
                    p_max_z->z_for_task_compress_par2 = x - y;
                    //printf("m is %d, z is %f, x is %f, y is %f\n", p_max_z->m_for_task_compress_par2, p_max_z->z_for_task_compress_par2, x, y);
                    p_max_z->m_for_task_compress_par2--;
                } else {
                    heap_flag--;
                    p_max_z->flag_for_task_compress_par2 = 0; //do not repeatedly pick the same one
                }
            }
            p = G->program_head->next;
            while(p!=NULL) {
                int idx = p->m_for_task_compress_par2 - 1;
                p->next_period_feedback_s = p->t_s_dict_for_task_compress_par2[idx];
                p->next_period_feedback_us = p->t_us_dict_for_task_compress_par2[idx];
                printf("\tprogram uid: %d, elastic_coefficient: %f, assigned num cores: %d, period: %llu %llu\n", p->control_uid, p->elastic_coefficient, p->m_for_task_compress_par2, p->t_s_dict_for_task_compress_par2[idx], p->t_us_dict_for_task_compress_par2[idx]);
                p = p->next;
            }
        } else { //M==0
            //it is optimized and do nothing, directly using p->m_for_task_compress_par2
        }
        //set cpu_mask for previous program and the new one
        p = G->program_head->next;
        int starting_core = 2;
        int i = 0;
        while(p!=NULL) {
            p->cpu_mask[0] = 0; //do not run program on core 0
            p->cpu_mask[1] = 0; //do not run program on core 0
            //printf("NUM OF CORE TO RUN: %d, starting_core is %d\n", p->m_for_task_compress_par2, starting_core);
            for (i=2; i<G->nproc; i++) {
                if (i>=starting_core && i<starting_core+p->m_for_task_compress_par2) {
                    p->cpu_mask[i] = 1;
                } else {
                    p->cpu_mask[i] = 0;
                }
            }
            print_cpu_mask(p);
            starting_core = starting_core+p->m_for_task_compress_par2;
            p = p->next;
        }

    } else {
        //do nothing
        //printf("do nothing\n");
    }
    //update next_period_feedback_s
}