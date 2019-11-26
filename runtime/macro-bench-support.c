#include "macro-bench-support.h"
#include "rts-config.h"

void program_set_requested_time_ns(platform_program_request * pr) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    pr->requested_time = nanos;
}

void program_set_pick_container_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->pick_container_time = nanos;
}

void program_set_picked_container_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->picked_container_time = nanos;
}

void program_set_activate_container_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->activate_container_time = nanos;
}

void program_set_core_assignment_time_when_run_ns(platform_program * p) {
    if (p!=NULL) {
        struct timespec temp;
        unsigned long long nanos;
        clock_gettime(CLOCK_MONOTONIC , &temp);
        nanos = temp.tv_nsec;
        nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
        p->core_assignment_time_when_run = nanos;
    }
}

void program_set_platform_preemption_time_ns(platform_program * p) {
    if (p!=NULL) {
        struct timespec temp;
        unsigned long long nanos;
        clock_gettime(CLOCK_MONOTONIC , &temp);
        nanos = temp.tv_nsec;
        nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
        p->platform_preemption_time = nanos;
    }
}

void program_set_begin_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->begin_time = nanos;
}

void program_set_end_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->end_time = nanos;
}

void program_set_pause_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->pause_time = nanos;
}

void program_set_resume_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->resume_time = nanos;
}

void program_set_begin_exit_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->begin_exit_time = nanos;
}

void program_set_sleeped_all_other_workers_time_ns(platform_program * p) {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    p->sleeped_all_other_workers_time = nanos;
}

unsigned long long program_get_overhead_picking_container_ns(platform_program * p) {
    return p->picked_container_time - p->pick_container_time;
}

unsigned long long program_get_overhead_activating_container_ns(platform_program * p) {
    return p->activate_container_time - p->picked_container_time;
}

unsigned long long program_get_overhead_core_assignment_ns(platform_program * p) {
    return p->core_assignment_time_when_run - p->activate_container_time;
}

unsigned long long program_get_overhead_preemption_ns(platform_program * p) {
    return p->platform_preemption_time - p->core_assignment_time_when_run;
}

unsigned long long program_get_run_time_ns(platform_program * p) {
    return p->end_time - p->begin_time;
}

unsigned long long program_get_response_latency_ns(platform_program * p) {
    return p->begin_time - p->requested_time;
}

unsigned long long program_get_flow_time_ns(platform_program * p) {
    return p->end_time - p->requested_time;
}

unsigned long long program_get_waiting_all_other_sleep_ns(platform_program * p) {
    return p->sleeped_all_other_workers_time - p->begin_exit_time;
}

//print result
void program_print_result(platform_program * p) {
    p->G->macro_test_acc_pick_time = p->G->macro_test_acc_pick_time + program_get_overhead_picking_container_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_activate_time = p->G->macro_test_acc_activate_time + program_get_overhead_activating_container_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_core_assign_time = p->G->macro_test_acc_core_assign_time + program_get_overhead_core_assignment_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_preempt_time = p->G->macro_test_acc_preempt_time + program_get_overhead_preemption_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_waiting_all_other_sleep = p->G->macro_test_acc_waiting_all_other_sleep + program_get_waiting_all_other_sleep_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_run_time = p->G->macro_test_acc_run_time + program_get_run_time_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_latency = p->G->macro_test_acc_latency + program_get_response_latency_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_flow_time = p->G->macro_test_acc_flow_time + program_get_flow_time_ns(p)/1000/1000/1000.0;
    printf("\t(p %d, input: %d)num: %llu, buff_global: %d, buff_self: %d, latency: %f, run: %f, flow: %f, pick: %f, activate: %f, core_assign: %f, preempt %f\n", 
            p->control_uid, p->input, p->G->macro_test_num_programs_executed,
            get_count_program_request_buffer(p->G, 0), 
            get_count_program_request_buffer(p->G, p->control_uid), 
            program_get_response_latency_ns(p)/1000/1000/1000.0, 
            program_get_run_time_ns(p)/1000/1000/1000.0, 
            program_get_flow_time_ns(p)/1000/1000/1000.0, 
            program_get_overhead_picking_container_ns(p)/1000/1000/1000.0, 
            program_get_overhead_activating_container_ns(p)/1000/1000/1000.0, 
            program_get_overhead_core_assignment_ns(p)/1000/1000/1000.0, 
            program_get_overhead_preemption_ns(p)/1000/1000/1000.0);
}
/*
void program_print_result_acc(platform_program * p) {
    if (p->run_times<=1) {
        return;
    }
    p->G->macro_test_acc_pick_time = p->G->macro_test_acc_pick_time + program_get_overhead_picking_container_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_activate_time = p->G->macro_test_acc_activate_time + program_get_overhead_activating_container_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_core_assign_time = p->G->macro_test_acc_core_assign_time + program_get_overhead_core_assignment_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_preempt_time = p->G->macro_test_acc_preempt_time + program_get_overhead_preemption_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_waiting_all_other_sleep = p->G->macro_test_acc_waiting_all_other_sleep + program_get_waiting_all_other_sleep_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_run_time = p->G->macro_test_acc_run_time + program_get_run_time_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_latency = p->G->macro_test_acc_latency + program_get_response_latency_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_flow_time = p->G->macro_test_acc_flow_time + program_get_flow_time_ns(p)/1000/1000/1000.0;
    printf("[FINISH CONTAINER %d]: (input: %d)num: %llu, buff: %d, latency: %f, run: %f, flow: %f, || pick: %f, activate: %f, core_assign: %f, preempt: %f, all_sleep: %f, run: %f, latency: %f, flow: %f\n", 
            p->control_uid, p->input, p->G->macro_test_num_programs_executed,
            get_count_program_request_buffer(p->G, 0), 
            program_get_response_latency_ns(p)/1000/1000/1000.0, 
            program_get_run_time_ns(p)/1000/1000/1000.0, 
            program_get_flow_time_ns(p)/1000/1000/1000.0, 
            p->G->macro_test_acc_pick_time, 
            p->G->macro_test_acc_activate_time, 
            p->G->macro_test_acc_core_assign_time, 
            p->G->macro_test_acc_preempt_time,
            p->G->macro_test_acc_waiting_all_other_sleep,
            p->G->macro_test_acc_run_time,
            p->G->macro_test_acc_latency,
            p->G->macro_test_acc_flow_time);
}*/
void program_print_result_acc(platform_program * p) {
    if (p->run_times<=0) {
        return;
    }
    p->G->macro_test_acc_pick_time = p->G->macro_test_acc_pick_time + program_get_overhead_picking_container_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_activate_time = p->G->macro_test_acc_activate_time + program_get_overhead_activating_container_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_core_assign_time = p->G->macro_test_acc_core_assign_time + program_get_overhead_core_assignment_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_preempt_time = p->G->macro_test_acc_preempt_time + program_get_overhead_preemption_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_waiting_all_other_sleep = p->G->macro_test_acc_waiting_all_other_sleep + program_get_waiting_all_other_sleep_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_run_time = p->G->macro_test_acc_run_time + program_get_run_time_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_latency = p->G->macro_test_acc_latency + program_get_response_latency_ns(p)/1000/1000/1000.0;
    p->G->macro_test_acc_flow_time = p->G->macro_test_acc_flow_time + program_get_flow_time_ns(p)/1000/1000/1000.0;
    int buff_count = get_count_program_request_buffer(p->G, 0);
    printf("[FINISH CONTAINER %d]: (input: %d)num: %llu, sched revision: %llu, stop container: %llu, buff: %d, run: %f, latency: %f, flow: %f, aver flow: %f, run_container: %d\n", 
            p->control_uid, p->input, p->G->macro_test_num_programs_executed, p->G->macro_test_num_scheduling_revision, p->G->macro_test_num_stop_container,
            buff_count, 
            p->G->macro_test_acc_run_time,
            p->G->macro_test_acc_latency,
            p->G->macro_test_acc_flow_time,
            p->G->macro_test_acc_flow_time/p->G->macro_test_num_programs_executed,
            p->G->nprogram_running);
    if (buff_count>OVERLOADED_THRESHOLD) {
        printf("ERROR: system overheaded\n");
        abort();
    }
}