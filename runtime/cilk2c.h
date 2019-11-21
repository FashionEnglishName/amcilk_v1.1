#ifndef _CILK2C_H
#define _CILK2C_H

#include <stdlib.h>
#include "internal.h"

// mainly used by container-main.c
extern unsigned long ZERO;

// These functoins are mostly inlined by the compiler, except for 
// __cilkrts_leave_frame.  However, their implementations are also
// provided in cilk2c.c.  The implementations in cilk2c.c are used 
// by container-main.c and can be used to "hand compile" cilk code.
void __cilkrts_enter_frame(__cilkrts_stack_frame *sf);
void __cilkrts_enter_frame_fast(__cilkrts_stack_frame * sf);
void __cilkrts_save_fp_ctrl_state(__cilkrts_stack_frame *sf);
void __cilkrts_detach(__cilkrts_stack_frame * self);
void __cilkrts_sync(__cilkrts_stack_frame *sf);
void __cilkrts_pop_frame(__cilkrts_stack_frame * sf);
void __cilkrts_leave_frame(__cilkrts_stack_frame * sf);
int __cilkrts_get_nworkers(void);
//platform
void __cilkrts_elastic_trigger_sleep(__cilkrts_stack_frame * sf, int cpu_id);
void __cilkrts_elastic_trigger_activate(__cilkrts_stack_frame * sf, int cpu_id);
void __cilkrts_elastic_do_recover(__cilkrts_stack_frame * sf);
void __cilkrts_set_begin_time_ns();
void __cilkrts_set_end_time_ns();
unsigned long long __cilkrts_get_run_time_ns();
void __cilkrts_macro_add_program_run_num();
unsigned long long __cilkrts_macro_get_program_run_num();
int __cilkrts_get_control_uid();
void __cilkrts_print_cpu_mask();
int __cilkrts_get_input();
int __cilkrts_get_request_buffer_size(int uid);

#endif
