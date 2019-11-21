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