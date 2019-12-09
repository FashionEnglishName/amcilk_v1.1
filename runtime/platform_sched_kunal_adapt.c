#include "platform_sched_kunal_adapt.h"
#include "sched_stats.h"

int get_cpu_cycle_status(platform_program * p) {
	if (p->begin_cpu_cycle_ts!=0) {
		p->total_cycles = 0;
		p->total_stealing_cycles = 0;
		p->total_work_cycles = 0;
		int i = 0;
		for (i=0; i<p->G->nproc; i++) {
			p->total_stealing_cycles += p->g->workers[i]->l->stealing_cpu_cycles;
		}
		p->total_cycles = rdtsc() - p->begin_cpu_cycle_ts;
		p->total_work_cycles = p->total_cycles - p->total_stealing_cycles;
		printf("total: %llu, steal: %llu (%f), non-steal: %llu (%f)", 
			p->total_cycles, 
			p->total_stealing_cycles, 
			p->total_stealing_cycles*(1.0)/p->total_cycles,
			p->total_work_cycles,
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
    	get_cpu_cycle_status(tmp_p);
    	tmp_p = tmp_p->next;
    }
}