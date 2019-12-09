#include "platform_sched_kunal_adapt.h"
#include "sched_stats.h"

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

int get_cpu_cycle_status(platform_program * p) {
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
			p->total_work_cycles*(1.0)/p->total_cycles);*/
		printf("(p%d, job finish:%d, input:%d, cpu num:%d): non-steal: %f\n", 
			p->control_uid,
			p->job_finish,
			p->input,
			p->num_cpu,
			p->total_work_cycles*(1.0)/p->total_cycles);

		return 0;
	} else {
		printf("kunal_adaptive_scheduler: get_cpu_cycle_status failed (not start running)\n");
		return -1;
	}
}

void kunal_adaptive_scheduler(platform_global_state * G) {
    //TODO
    printf("do kunal_adaptive_scheduler\n");
    platform_program * tmp_p = G->program_head->next;
    while(tmp_p!=NULL) {
    	pthread_spin_lock(&(tmp_p->cpu_cycle_status_lock));
    	get_cpu_cycle_status(tmp_p);
    	reset_cpu_cycle_status(tmp_p);
    	pthread_spin_unlock(&(tmp_p->cpu_cycle_status_lock));
    	tmp_p = tmp_p->next;
    }
}