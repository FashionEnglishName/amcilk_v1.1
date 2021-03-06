#ifndef _CONFIG_H 
#define _CONFIG_H 

#define __CILKRTS_VERSION 0x0
#define __CILKRTS_ABI_VERSION 0x1

#define CILK_DEBUG 1
#define CILK_STATS 0

#define CILK_CACHE_LINE 64
#define CILK_CACHE_LINE_PAD  char __dummy[CILK_CACHE_LINE]

#define PROC_SPEED_IN_GHZ 2.2
#define PAGE_SIZE 4096
#define MIN_NUM_PAGES_PER_STACK 4 
#define DEFAULT_STACKSIZE 0x100000 // 1 MBytes

#define DEFAULT_NPROC 0             // 0 for # of cores available
#define DEFAULT_DEQ_DEPTH 1024
#define DEFAULT_STACK_SIZE 0x100000 // 1 MBytes
#define DEFAULT_FIBER_POOL_CAP  128 // initial per-worker fiber pool capacity 


//C/S arch
#define SERVER_FIFO_NAME "/home/zhe.wang/cilk_fifo" //"/home/zhe.wang/cilk_fifo" //"/project/adams/home/zhe.wang/cilk_fifo"
#define PROGRAM_FIFO_NAME_BASE "/home/zhe.wang/cilk_fifo_p" //"/home/zhe.wang/cilk_fifo_p" //"/project/adams/home/zhe.wang/cilk_fifo_p"
#define BUF_SIZE 10240
//server end spec
#define CONTAINER_COUNT 14
#define TIME_MAKE_SURE_TO_SLEEP 200//100 //important! depends of the size of a single frame
#define TIME_MAKE_SURE_TO_ACTIVATE 200//100 //important! depends of implementation of block/activate mechanism
#define TIME_EXIT_CTX_SWITCH 200 //important! can not be too small for the efficiency
#define TIME_REQUEST_RECEIVE_INTERVAL 40000//25000//Bing: (12500 best! for disable the feedback) 50000, 45000, 75301; Finance:
#define TIME_CONTAINER_TRIGGER_INTERVAL TIME_REQUEST_RECEIVE_INTERVAL //important! when workload is high
#define GUARANTEE_PREMPT_WAITING_TIME 10000
#define MAX_ACTIVE_CONTAIN_THRESHOLD CONTAINER_COUNT
#define OVERLOADED_THRESHOLD 10*CONTAINER_COUNT //2*
#define MINI_ACTIVE_CONTAIN_THRESHOLD CONTAINER_COUNT

#define KUNAL_ADAPTIVE_FEEDBACK_PERIOD_S 0
#define KUNAL_ADAPTIVE_FEEDBACK_PERIOD_US TIME_REQUEST_RECEIVE_INTERVAL/10
#define KUNAL_ADAPTIVE_FEEDBACK_INEFFICIENT_THRESHOLD 0.95 //0.8~0.95
#define KUNAL_ADAPTIVE_FEEDBACK_RESPONSIVENESS_PARAMETER 2 //1.2~2.0
#define KUNAL_ADAPTIVE_FEEDBACK_ENABLE 0 //0 or 1


/*
//C/S arch
#define SERVER_FIFO_NAME "/project/adams/home/zhe.wang/cilk_fifo" //"/home/zhe.wang/cilk_fifo" //"/project/adams/home/zhe.wang/cilk_fifo"
#define PROGRAM_FIFO_NAME_BASE "/project/adams/home/zhe.wang/cilk_fifo_p" //"/home/zhe.wang/cilk_fifo_p" //"/project/adams/home/zhe.wang/cilk_fifo_p"
#define BUF_SIZE 10240
//server end spec
#define CONTAINER_COUNT 30
#define TIME_MAKE_SURE_TO_SLEEP 100//100 //important! depends of the size of a single frame
#define TIME_MAKE_SURE_TO_ACTIVATE 100//100 //important! depends of implementation of block/activate mechanism
#define TIME_EXIT_CTX_SWITCH 100 //important! can not be too small for the efficiency
#define TIME_REQUEST_RECEIVE_INTERVAL 50000//Bing: 50000, 45000, 75301; Finance:
#define TIME_CONTAINER_TRIGGER_INTERVAL TIME_REQUEST_RECEIVE_INTERVAL //important! when workload is high
#define GUARANTEE_PREMPT_WAITING_TIME 10000
#define CONTAINER_COUNT_THRESHOLD CONTAINER_COUNT
#define OVERLOADED_THRESHOLD 2*CONTAINER_COUNT
#define MINI_ACTIVE_CONTAIN_THRESHOLD CONTAINER_COUNT

#define KUNAL_ADAPTIVE_FEEDBACK_PERIOD_S 0
#define KUNAL_ADAPTIVE_FEEDBACK_PERIOD_US TIME_REQUEST_RECEIVE_INTERVAL/10
*/
#endif // _CONFIG_H 
