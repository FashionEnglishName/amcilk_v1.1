#include "platform_sched_kunal_adapt.h"
#include "sched_stats.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void reset_cpu_cycle_status(platform_program * p) {
	if (p->job_init_finish==1) {
		int i = 0;
		p->total_cycles = 0;
	    p->total_stealing_cycles = 0;
	    p->total_work_cycles = 0;
	    for (i=0; i<p->G->nproc; i++) {
	        p->g->workers[i]->l->stealing_cpu_cycles = 0;
	    }
	    p->begin_cpu_cycle_ts = rdtsc(); //get time stamp
	} else {
		printf("kunal_adaptive_scheduler: reset_cpu_cycle_status failed (not start running)\n");
	}
}

int analyze_cpu_cycle_status(platform_program * p) {
	if (p->job_init_finish==1) {
		p->total_cycles = 0;
		p->total_stealing_cycles = 0;
		p->total_work_cycles = 0;
		int i = 0;
		for (i=0; i<p->G->nproc; i++) {
			p->total_stealing_cycles += p->g->workers[i]->l->stealing_cpu_cycles;
		}
		p->total_cycles = (rdtsc() - p->begin_cpu_cycle_ts) * p->G->nproc;
		p->total_work_cycles = p->total_cycles - p->total_stealing_cycles;
		/*printf("(p%d, job finish:%d): total: %llu, steal: %llu (%f), non-steal: %llu (%f)\n", 
			p->control_uid,
			p->job_finish,
			p->total_cycles, 
			p->total_stealing_cycles, 
			p->total_stealing_cycles*(1.0)/p->total_cycles,
			p->total_work_cycles,
			non_steal_utilization);*/

		float non_steal_utilization = p->total_work_cycles*(1.0)/p->total_cycles;
		int efficient = 0;
		int satisfied = 0;
		if (non_steal_utilization < KUNAL_ADAPTIVE_FEEDBACK_INEFFICIENT_THRESHOLD) {
			efficient = 0;
		} else {
			efficient = 1;
		}
		if (p->desired_num_cpu == p->try_num_cpu) {
			satisfied = 1;
		} else if (p->desired_num_cpu > p->try_num_cpu) {
			satisfied = 0;
		} else {
			printf("BUG, %d, try_num_cpu (%d) > desired_num_cpu (%d)\n", p->control_uid, p->try_num_cpu, p->desired_num_cpu);
			abort();
		}
		if (efficient==0) {
			p->desired_num_cpu = MIN(MAX(floor(p->desired_num_cpu/KUNAL_ADAPTIVE_FEEDBACK_RESPONSIVENESS_PARAMETER), 1), p->G->nproc-2);
		} else if (satisfied==1) {
			p->desired_num_cpu = MIN(MAX(ceil(p->desired_num_cpu*KUNAL_ADAPTIVE_FEEDBACK_RESPONSIVENESS_PARAMETER), 1), p->G->nproc-2);
		} else {
			p->desired_num_cpu = p->desired_num_cpu;
		}

		if (p->desired_num_cpu<=0) {
			printf("BUG, desired_num_cpu <= 0\n");
			abort();
		}

		/*printf("(p%d, job finish:%d, input:%d, cpu num:%d): non-steal: %f, efficient: %d, satisfied: %d, desired: %d\n", 
			p->control_uid,
			p->job_finish,
			p->input,
			p->try_num_cpu,
			non_steal_utilization,
			efficient,
			satisfied,
			p->desired_num_cpu);*/
		printf("(p%d, job finish:%d, input:%d, cpu num:%d): total: %llu, steal: %llu (%f), non-steal: %llu (%f), efficient: %d, satisfied: %d, desired: %d\n", 
			p->control_uid,
			p->job_finish,
			p->input,
			p->try_num_cpu,
			p->total_cycles, 
			p->total_stealing_cycles, 
			p->total_stealing_cycles*(1.0)/p->total_cycles,
			p->total_work_cycles,
			non_steal_utilization,
			efficient,
			satisfied,
			p->desired_num_cpu);
		return 0;
	} else {
		printf("kunal_adaptive_scheduler: analyze_cpu_cycle_status failed (not start running)\n");
		return -1;
	}
}

void kunal_adaptive_scheduler(platform_global_state * G) {
    //printf("do kunal_adaptive_scheduler\n");
    platform_program * tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
    	pthread_spin_lock(&(tmp_p->cpu_cycle_status_lock));
    	analyze_cpu_cycle_status(tmp_p);
    	reset_cpu_cycle_status(tmp_p);
    	pthread_spin_unlock(&(tmp_p->cpu_cycle_status_lock));
    	tmp_p = tmp_p->next;
    }
}