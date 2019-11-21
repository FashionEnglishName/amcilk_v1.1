#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
 
#define SERVER_FIFO_NAME "/home/zhe.wang/cilk_fifo" //"/project/adams/home/zhe.wang/cilk_fifo"
#define PROGRAM_FIFO_NAME_BASE "/home/zhe.wang/cilk_fifo_p" //"/project/adams/home/zhe.wang/cilk_fifo_p"

#define BUF_SIZE 10240

static int count = 0;

pthread_mutex_t lock;
pthread_cond_t cond;

unsigned long long period_min_s;
unsigned long long period_min_us;
unsigned long long period_max_s;
unsigned long long period_max_us;
unsigned long long period_s;
unsigned long long period_us;
unsigned long long C_s;
unsigned long long C_us;
unsigned long long L_s;
unsigned long long L_us;
float elastic_coefficient;

int life_span;
int program;
int input;
int control_uid;
int second_level_uid = 0;


struct itimerval itv;
struct itimerval oldtv;

unsigned long long last_time = 0;
unsigned long long current_time = 0;

unsigned long long get_time() {
    struct timespec temp;
    unsigned long long nanos;
    clock_gettime(CLOCK_MONOTONIC , &temp);
    nanos = temp.tv_nsec;
    nanos += ((unsigned long long)temp.tv_sec) * 1000 * 1000 * 1000;
    return nanos;
}

void set_timer(struct itimerval * itv, struct itimerval * oldtv, unsigned long long sec, unsigned long long usec) {
    itv->it_interval.tv_sec = 0;
    itv->it_interval.tv_usec = 0;
    itv->it_value.tv_sec = sec;
    itv->it_value.tv_usec = usec;
    setitimer(ITIMER_REAL, itv, oldtv);
}

void signal_handler(int m) {
    pthread_mutex_lock(&lock);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
    set_timer(&(itv), &(oldtv), period_s, period_us);
}

static void* timer_thread_func(void * arg) {
    int fd;
    char str[BUF_SIZE];
    int * life_span = (int *)arg;
    set_timer(&(itv), &(oldtv), period_s, period_us);
    count = 0;
    while (count < *life_span) {
        count++;
        printf("life: %d\n", *life_span-count);
        sprintf(str, "%d %d %d %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %f", program, input, control_uid, second_level_uid, 1, 0, 
            period_max_s, period_max_us, period_min_s, period_min_us, C_s, C_us, L_s, L_us, elastic_coefficient); //send program run request to server
        fd = open(SERVER_FIFO_NAME, O_WRONLY);
        write(fd, str, strlen(str)+1);
        close(fd);

        pthread_mutex_lock(&lock);
        pthread_cond_wait(&cond, &lock);
        pthread_mutex_unlock(&lock);

        last_time = current_time;
        current_time = get_time();
        printf("period: %fs\n", ((double)(current_time-last_time)) / 1000000000.0f);
    }
    exit(0);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc!=15) {
    	printf("Abort: the input should be \"program<int> input<int> control_uid<int> second_level_uid<int> life_span<int> period_max_s<int> period_max_us<int> period_min_s<int> period_min_us<int> C_s<int> C_us<int> L_s<int> L_us<int> elastic_coefficient<float>\"\n");
    	return -1;
    }
    program = atoi(argv[1]); //get config from input
    input = atoi(argv[2]);
    control_uid = atoi(argv[3]);
    if (control_uid==0) {
        printf("Contract Error: control_uid of periodic program must not be 0!\n");
        exit(0);
    }
    life_span = atoi(argv[5]);
    period_max_s = atoi(argv[6]);
    period_max_us = atoi(argv[7]);
    period_min_s = atoi(argv[8]);
    period_min_us = atoi(argv[9]);
    C_s = atoi(argv[10]);
    C_us = atoi(argv[11]);
    L_s = atoi(argv[12]);
    L_us = atoi(argv[13]);
    elastic_coefficient = atof(argv[14]);
    period_s = period_max_s; //at beginning, we use the max period
    period_us = period_max_us;
    printf("max_s %llu, max_us %llu\n", period_max_s, period_max_us);
    pthread_mutex_init(&(lock), NULL);
    pthread_cond_init(&(cond), NULL);
    //mkfifo(SERVER_FIFO_NAME, S_IFIFO|0666);
    char program_fifo_path[BUF_SIZE];
    sprintf(program_fifo_path, "%s_%d_%d", PROGRAM_FIFO_NAME_BASE, control_uid, second_level_uid);
    unlink(program_fifo_path);
    mkfifo(program_fifo_path, S_IFIFO|0666);

    signal(SIGALRM, signal_handler);
    current_time = get_time();

    pthread_t timer_thread;
    int status = pthread_create(&timer_thread, NULL, timer_thread_func, &life_span);
    if (status != 0) {
        printf("error: thread creation scheduling thread failed: %d\n", status);
    }
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    status = pthread_setaffinity_np(timer_thread, sizeof(mask), &mask);
    if (status != 0) {
        printf("error: Could not set CPU affinity");
    }
    /*status = pthread_join(timer_thread, NULL);
    if(status != 0) {
        printf("error: thread join failed\n");
    }*/

    while(1) {
        int ret_fscanf;
        //Get request
        FILE *fp;

        fp = fopen(program_fifo_path, "r");
        if (fp == NULL) {
            printf("ERROR: fopen failed!\n");
            exit(-1);
        }
        int result;
        float run_time;
        ret_fscanf = fscanf(fp, "%d %f %llu %llu", &result, &run_time, &period_s, &period_us); //get response from server end
        printf("next period: %llu %llu\n", period_s, period_us);
        if (ret_fscanf == EOF) {
            printf("End of fifo\n");
        }
        if (ret_fscanf < -1) {
            printf("ERROR: fopen failed (%d/%d)! Reason: %s\n", ret_fscanf, EOF, strerror(errno));
            exit(-1);
        }
        fclose(fp);
        printf("result: %d\ntime: %f\n", result, run_time);
    }
    //unlink(SERVER_FIFO_NAME);
    unlink(program_fifo_path);
   	return 0; 
}