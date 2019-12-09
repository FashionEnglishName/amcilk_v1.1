#ifndef _CILK_INTERNAL_H
#define _CILK_INTERNAL_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

// Includes
#include "debug.h"
#include "fiber.h"
#include "internal-malloc.h"
#include "jmpbuf.h"
#include "mutex.h"
#include "rts-config.h"
#include "sched_stats.h"
#include "closure.h"

#define NOBODY -1

#if CILK_STATS
#define WHEN_CILK_STATS(ex) ex
#else
#define WHEN_CILK_STATS(ex)
#endif

// Forward declaration
typedef struct __cilkrts_worker __cilkrts_worker;
typedef struct __cilkrts_stack_frame __cilkrts_stack_frame;

typedef struct Elastic_core Elastic_core;
typedef struct platform_program platform_program;
typedef struct platform_global_state platform_global_state;
typedef struct platform_program_request platform_program_request;

//===============================================
// Cilk stack frame related defs
//===============================================

/**
 * Every spawning function has a frame descriptor.  A spawning function
 * is a function that spawns or detaches.  Only spawning functions
 * are visible to the Cilk runtime.
 *
 * NOTE: if you are using the Tapir compiler, you should not change
 * these fields; ok to change for hand-compiled code.
 * See Tapir compiler ABI: 
 * https://github.com/wsmoses/Tapir-LLVM/blob/cilkr/lib/Transforms/Tapir/CilkRABI.cpp
 */
struct __cilkrts_stack_frame {
    // Flags is a bitfield with values defined below. Client code
    // initializes flags to 0 before the first Cilk operation.
    uint32_t flags;

    // call_parent points to the __cilkrts_stack_frame of the closest
    // ancestor spawning function, including spawn helpers, of this frame.
    // It forms a linked list ending at the first stolen frame.
    __cilkrts_stack_frame *  call_parent;

    // The client copies the worker from TLS here when initializing
    // the structure.  The runtime ensures that the field always points
    // to the __cilkrts_worker which currently "owns" the frame.
    __cilkrts_worker * worker;

    // Before every spawn and nontrivial sync the client function
    // saves its continuation here.
    jmpbuf ctx;

    /**
     * Architecture-specific floating point state.  
     * mxcsr and fpcsr should be set when setjmp is called in client code.  
     *
     * They are for linux / x86_64 platforms only.  Note that the Win64
     * jmpbuf for the Intel64 architecture already contains this information
     * so there is no need to use these fields on that OS/architecture.
     */
    uint32_t mxcsr;
    uint16_t fpcsr;

    //preemption
    jmpbuf prempt_ctx;
    uint32_t prempt_mxcsr;
    uint16_t prempt_fpcsr;

    //switching
    jmpbuf switch_ctx;
    uint32_t switch_mxcsr;
    uint16_t switch_fpcsr;


    /**
     * reserved is not used at this time.  Client code should initialize it
     * to 0 before the first Cilk operation
     */
    uint16_t reserved;      // ANGE: leave it to make it 8-byte aligned.
    uint32_t magic;
};

//===========================================================
// Value defines for the flags field in cilkrts_stack_frame
//===========================================================

/* CILK_FRAME_STOLEN is set if the frame has ever been stolen. */
#define CILK_FRAME_STOLEN 0x01

/* CILK_FRAME_UNSYNCHED is set if the frame has been stolen and
   is has not yet executed _Cilk_sync. It is technically a misnomer in that a
   frame can have this flag set even if all children have returned. */
#define CILK_FRAME_UNSYNCHED 0x02

/* Is this frame detached (spawned)? If so the runtime needs
   to undo-detach in the slow path epilogue. */
#define CILK_FRAME_DETACHED 0x04

/* CILK_FRAME_EXCEPTION_PROBED is set if the frame has been probed in the
   exception handler first pass */
#define CILK_FRAME_EXCEPTION_PROBED 0x08

/* Is this frame receiving an exception after sync? */
#define CILK_FRAME_EXCEPTING 0x10

/* Is this the last (oldest) Cilk frame? */
#define CILK_FRAME_LAST 0x80

/* Is this frame in the epilogue, or more generally after the last
   sync when it can no longer do any Cilk operations? */
#define CILK_FRAME_EXITING 0x0100

#define CILK_FRAME_VERSION (__CILKRTS_ABI_VERSION << 24)

//===========================================================
// Helper functions for the flags field in cilkrts_stack_frame
//===========================================================

/* A frame is set to be stolen as long as it has a corresponding Closure */
static inline void __cilkrts_set_stolen(__cilkrts_stack_frame *sf) {
    sf->flags |= CILK_FRAME_STOLEN;
}

/* A frame is set to be unsynced only if it has parallel subcomputation
 * underneathe, i.e., only if it has spawned children executing on a different
 * worker 
 */
static inline void __cilkrts_set_unsynced(__cilkrts_stack_frame *sf) {
    sf->flags |= CILK_FRAME_UNSYNCHED;
}

static inline void __cilkrts_set_synced(__cilkrts_stack_frame *sf) {
    sf->flags &= ~CILK_FRAME_UNSYNCHED;
}

/* Returns nonzero if the frame is not synched. */
static inline int __cilkrts_unsynced(__cilkrts_stack_frame *sf) {
    return (sf->flags & CILK_FRAME_UNSYNCHED);
}

/* Returns nonzero if the frame has been stolen. */
static inline int __cilkrts_stolen(__cilkrts_stack_frame *sf) {
    return (sf->flags & CILK_FRAME_STOLEN);
}

/* Returns nonzero if the frame is synched. */
static inline int __cilkrts_synced(__cilkrts_stack_frame *sf) {
    return( (sf->flags & CILK_FRAME_UNSYNCHED) == 0 );
}

/* Returns nonzero if the frame has never been stolen. */
static inline int __cilkrts_not_stolen(__cilkrts_stack_frame *sf) {
    return( (sf->flags & CILK_FRAME_STOLEN) == 0);
}

//===============================================
// Worker related definition 
//===============================================

// Forward declaration
typedef struct global_state global_state;
typedef struct local_state local_state;
typedef struct __cilkrts_stack_frame **CilkShadowStack;


//AMCilk D/S
enum PLATFORM_SCHEDULER_TYPE {
    NEW_PROGRAM=1, EXIT_PROGRAM, TIMED
};

enum ELASTIC_STATE {
    ACTIVE=1, 
    SLEEP_REQUESTED, TO_SLEEP, 
    SLEEPING_ADAPTING_DEQUE, SLEEPING_ACTIVE_DEQUE, SLEEPING_MUGGING_DEQUE, SLEEPING_INACTIVE_DEQUE, 
    ACTIVATE_REQUESTED, ACTIVATING, EXIT_SWITCHING0, EXIT_SWITCHING1, EXIT_SWITCHING2, DO_MUGGING};

struct Elastic_core {
    volatile int * cpu_state_group;
    volatile int ptr_sleeping_inactive_deque; //beginning of inactive
    volatile int ptr_sleeping_active_deque; //beginning of sleeping
    cilk_mutex lock;
    int * sleep_requests;
    int * activate_requests;

    int test_thief;
    int test_num_give_up_steal_running;
    int test_num_give_up_steal_returning;
    int test_num_try_steal;
    int test_num_thief_returning_cl_null;
    int test_num_steal_success;
    int test_num_try_lock_failed;
    int test_num_bug;
    int test_num_null_cl;
    int test_num_back_to_user_code;
    int test_num_bottom_returning;
    int test_num_back_to_runtime;
    int test_num_xtract_returning_cl;
};

struct platform_program {
    struct platform_global_state * G;

    struct platform_program_request * request_buffer_head;
    pthread_spinlock_t request_buffer_lock;

    int invariant_running_worker_id;
    volatile int pickable;
    volatile int hint_stop_container;
    volatile int job_finish;
    volatile int last_do_exit_worker_id;
    volatile int is_switching;
    volatile int run_request_before_block;
    volatile int running_job;

    struct platform_program * next;
    struct platform_program * prev;
    struct global_state * g;
    int self;
    int run_for_init_container;
    int input;
    int control_uid;
    int second_level_uid;
    int mute;

    int periodic;
    unsigned long long next_period_feedback_s;
    unsigned long long next_period_feedback_us;
    unsigned long long max_period_s;
    unsigned long long max_period_us;
    unsigned long long min_period_s;
    unsigned long long min_period_us;
    unsigned long long C_s;
    unsigned long long C_us;
    unsigned long long L_s;
    unsigned long long L_us;
    float elastic_coefficient;

    float utilization_max;
    float utilization_min;
    int num_core_max;
    int num_core_min;
    int m_for_task_compress_par2;
    unsigned long long t_s_for_task_compress_par2;
    unsigned long long t_us_for_task_compress_par2;
    unsigned long long * t_s_dict_for_task_compress_par2;
    unsigned long long * t_us_dict_for_task_compress_par2;
    float z_for_task_compress_par2;
    int flag_for_task_compress_par2;

    int* cpu_mask;
    int* try_cpu_mask;
    volatile int num_cpu;
    volatile int try_num_cpu;
    volatile unsigned long long begin_cpu_cycle_ts;
    volatile unsigned long long total_cycles;
    volatile unsigned long long total_stealing_cycles;
    volatile unsigned long long total_work_cycles;
    
    int tmp_num_cpu;
    volatile int done_one;
    int run_times;
    unsigned long long requested_time;
    unsigned long long pick_container_time;
    unsigned long long picked_container_time;
    unsigned long long activate_container_time;
    unsigned long long core_assignment_time_when_run;
    unsigned long long platform_preemption_time;
    unsigned long long begin_time;
    unsigned long long end_time;
    unsigned long long resume_time;
    unsigned long long pause_time;

    unsigned long long begin_exit_time;
    unsigned long long sleeped_all_other_workers_time;
};

struct platform_global_state {
    struct platform_program * program_head;
    struct platform_program * new_program;
    struct platform_program * stop_program;
    struct platform_program * deprecated_program_head;
    struct platform_program * deprecated_program_tail;
    struct platform_program * program_container_pool[CONTAINER_COUNT];
    struct platform_program_request * request_buffer_head;
    pthread_spinlock_t request_buffer_lock;

    int enable_timed_scheduling;
    int timed_interval_sec;
    int timed_interval_usec;
    struct itimerval timed_scheduling_itv;
    struct itimerval timed_scheduling_oldtv;

    int a_container_activated_no_preemption;
    int nproc;
    int* cpu_container_map;
    volatile int nprogram_running;
    unsigned int rand_next;
    pthread_mutex_t lock;
    //pthread_spinlock_t lock;

    int giveup_core_containers[CONTAINER_COUNT];
    int receive_core_containers[CONTAINER_COUNT];

    int new_input;
    int new_program_id;

    int platform_argc;
    char **platform_argv;
    int* most_recent_stop_program_cpumask;
    unsigned long long* micro_time_test;
    int micro_time_worker_activate_focus;
    int micro_time_worker_sleep_focus;
    int micro_test_to_run_input;
    unsigned long long macro_test_num_programs_executed;

    float macro_test_acc_pick_time;
    float macro_test_acc_activate_time;
    float macro_test_acc_core_assign_time;
    float macro_test_acc_preempt_time;
    float macro_test_acc_waiting_all_other_sleep;
    float macro_test_acc_run_time;
    float macro_test_acc_latency;
    float macro_test_acc_flow_time;

    unsigned long long macro_test_job1_come_time;
    unsigned long long macro_test_job2_come_time;
    unsigned long long macro_test_job3_come_time;
    unsigned long long macro_test_scheduling_begin_1;
    unsigned long long macro_test_scheduling_finish_1;
    unsigned long long macro_test_scheduling_begin_2;
    unsigned long long macro_test_scheduling_finish_2;
    unsigned long long macro_test_num_scheduling_revision;
    unsigned long long macro_test_num_stop_container;
};

struct platform_program_request {
    struct platform_global_state * G;
    struct platform_program_request * next;
    struct platform_program_request * prev;
    int program_id;
    int input;
    int control_uid;
    int second_level_uid;
    int periodic;
    int mute;
    unsigned long long max_period_s;
    unsigned long long max_period_us;
    unsigned long long min_period_s;
    unsigned long long min_period_us;
    unsigned long long C_s;
    unsigned long long C_us;
    unsigned long long L_s;
    unsigned long long L_us;
    float elastic_coefficient;
    unsigned long long requested_time;
};

// Actual declaration
struct rts_options {
    int nproc;
    int deqdepth;
    int64_t stacksize;
    int fiber_pool_cap;
};

#define DEFAULT_OPTIONS \
{                                                             \
    DEFAULT_NPROC,          /* num of workers to create */    \
    DEFAULT_DEQ_DEPTH,      /* num of entries in deque */     \
    DEFAULT_STACK_SIZE,     /* stack size to use for fiber */ \
    DEFAULT_FIBER_POOL_CAP, /* alloc_batch_size */            \
}

// Actual declaration
struct global_state {
    /* globally-visible options (read-only after init) */
    struct rts_options options;

    /*
     * this string is printed when an assertion fails.  If we just inline
     * it, apparently gcc generates many copies of the string.
     */
    const char *assertion_failed_msg;
    const char *stack_overflow_msg;

    /* dynamically-allocated array of deques, one per processor */
    struct ReadyDeque **deques;
    struct __cilkrts_worker **workers;
    pthread_t * threads;
    struct Elastic_core *elastic_core;
    struct Closure *invoke_main;

    struct cilk_fiber_pool fiber_pool __attribute__((aligned(64)));
    struct global_im_pool im_pool __attribute__((aligned(64)));
    struct cilk_im_desc im_desc __attribute__((aligned(64)));
    cilk_mutex im_lock;  // lock for accessing global im_desc

    WHEN_SCHED_STATS(struct global_sched_stats stats);

    volatile int invoke_main_initialized;
    volatile int start;
    volatile int done;
    cilk_mutex print_lock; // global lock for printing messages 

    int cilk_main_argc;
    char **cilk_main_args;

    int cilk_main_return;
    int cilk_main_exit;

    //Zhe: platform
    struct platform_program * program;
};

// Actual declaration
struct local_state {
    __cilkrts_stack_frame **shadow_stack;

    int provably_good_steal;
    unsigned int rand_next;

    jmpbuf rts_ctx;
    struct cilk_fiber_pool fiber_pool;
    struct cilk_im_desc im_desc;
    struct cilk_fiber * fiber_to_free;
    WHEN_SCHED_STATS(struct sched_stats stats);

    //Zhe: elastic scheduling
    volatile enum ELASTIC_STATE elastic_s;
    volatile int elastic_pos_in_cpu_state_group;
    int to_sleep_when_idle_signal;
    pthread_mutex_t elastic_lock;
    pthread_cond_t elastic_cond;
    int block_pipe_fd[2];

    //Zhe: platform
    int run_at_beginning;
    int is_in_runtime;

    //Zhe: stealing overhead measurement
    unsigned long long stealing_cpu_cycles;
};

/**
 * NOTE: if you are using the Tapir compiler, you should not change
 * these fields; ok to change for hand-compiled code.
 * See Tapir compiler ABI: 
 * https://github.com/wsmoses/Tapir-LLVM/blob/cilkr/lib/Transforms/Tapir/CilkRABI.cpp
 **/
struct __cilkrts_worker {
    // T and H pointers in the THE protocol
    __cilkrts_stack_frame * volatile * volatile tail;
    __cilkrts_stack_frame * volatile * volatile head;
    __cilkrts_stack_frame * volatile * volatile exc;

    // Limit of the Lazy Task Queue, to detect queue overflow
    __cilkrts_stack_frame * volatile *ltq_limit;

    // Worker id.
    int32_t self;

    // Global state of the runtime system, opaque to the client.
    global_state * g;

    // Additional per-worker state hidden from the client.
    local_state * l;

    // A slot that points to the currently executing Cilk frame.
    __cilkrts_stack_frame * current_stack_frame;
};

#endif // _CILK_INTERNAL_H
