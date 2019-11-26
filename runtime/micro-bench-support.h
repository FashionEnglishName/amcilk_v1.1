#include "internal.h"
#include "elastic.h"
#include "stdio.h"

void micro_test_get_clock(platform_global_state * G, int idx);
unsigned long long micro_get_diff_a_b(platform_global_state * G, int idx_a, int idx_b);
void micro_test_reset(platform_global_state * G, int idx);
unsigned long long micro_get_clock();
void elastic_all_worker_frame_num_test(__cilkrts_worker *w);