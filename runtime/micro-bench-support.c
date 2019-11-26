#include "micro-bench-support.h"

void micro_test_get_clock(platform_global_state * G, int idx) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    G->micro_time_test[idx] = nanos;
}

unsigned long long micro_get_diff_a_b(platform_global_state * G, int idx_a, int idx_b) {
    return (unsigned long long)(G->micro_time_test[idx_a]-G->micro_time_test[idx_b]); 
}

void micro_test_reset(platform_global_state * G, int idx) {
    G->micro_time_test[idx] = 0;
}

unsigned long long micro_get_clock() {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    return nanos;
}

void elastic_all_worker_frame_num_test(__cilkrts_worker *w) {
    if (elastic_safe(w)) {
        if (w->g->elastic_core->test_thief!=-1){
            /*printf("%d:\t%d(%ld)\t(success %d/gu run %d/gu return %d/bottom return %d/lock failed %d/bug %d/null cl top%d)\t(steal attempts: %d)\t(jump user %d)\t(jump runtime %d)\t(xtract returning cl %d)\n",
                w->self,
                w->g->elastic_core->test_thief,
                w->g->workers[w->g->elastic_core->test_thief]->tail - w->g->workers[w->g->elastic_core->test_thief]->head+1, 
                w->g->elastic_core->test_num_steal_success,
                w->g->elastic_core->test_num_give_up_steal_running,
                w->g->elastic_core->test_num_give_up_steal_returning,
                w->g->elastic_core->test_num_bottom_returning,
                w->g->elastic_core->test_num_try_lock_failed,
                w->g->elastic_core->test_num_bug,
                w->g->elastic_core->test_num_null_cl,
                w->g->elastic_core->test_num_try_steal,
                w->g->elastic_core->test_num_back_to_user_code,
                w->g->elastic_core->test_num_back_to_runtime,
                w->g->elastic_core->test_num_xtract_returning_cl);*/

            printf("%d:\t%d(%ld)\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n", 
                w->self,
                w->g->elastic_core->test_thief,
                w->g->workers[w->g->elastic_core->test_thief]->tail - w->g->workers[w->g->elastic_core->test_thief]->head+1, 
                w->g->workers[0]->tail - w->g->workers[0]->head+1, 
                w->g->workers[1]->tail - w->g->workers[1]->head+1,
                w->g->workers[2]->tail - w->g->workers[2]->head+1, 
                w->g->workers[3]->tail - w->g->workers[3]->head+1, 
                w->g->workers[4]->tail - w->g->workers[4]->head+1, 
                w->g->workers[5]->tail - w->g->workers[5]->head+1, 
                w->g->workers[6]->tail - w->g->workers[6]->head+1, 
                w->g->workers[7]->tail - w->g->workers[7]->head+1, 
                w->g->workers[8]->tail - w->g->workers[8]->head+1, 
                w->g->workers[9]->tail - w->g->workers[9]->head+1, 
                w->g->workers[10]->tail - w->g->workers[10]->head+1, 
                w->g->workers[11]->tail - w->g->workers[11]->head+1, 
                w->g->workers[12]->tail - w->g->workers[12]->head+1, 
                w->g->workers[13]->tail - w->g->workers[13]->head+1, 
                w->g->workers[14]->tail - w->g->workers[14]->head+1, 
                w->g->workers[15]->tail - w->g->workers[15]->head+1, 
                w->g->workers[16]->tail - w->g->workers[16]->head+1, 
                w->g->workers[17]->tail - w->g->workers[17]->head+1, 
                w->g->workers[18]->tail - w->g->workers[18]->head+1, 
                w->g->workers[19]->tail - w->g->workers[19]->head+1, 
                w->g->workers[20]->tail - w->g->workers[20]->head+1, 
                w->g->workers[21]->tail - w->g->workers[21]->head+1, 
                w->g->workers[22]->tail - w->g->workers[22]->head+1, 
                w->g->workers[23]->tail - w->g->workers[23]->head+1);
        }
    }
}