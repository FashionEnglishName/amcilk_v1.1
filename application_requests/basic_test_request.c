#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sched.h>

#include "../runtime/cilk2c.h"
#include "../handcomp_test/ktiming.h"
#include "../handcomp_test/timespec_functions.h"
#include "../runtime/rts-config.h"

//#define NumRequests 1000000
//#define NumRequests 1000
//#define NumRequests 100
#define NumRequests 10

double interArrivalTime = 0.83;
int i = 0;
int program;
int input;


int request_program_run(int program, int input) {
    int fd;
    char str[BUF_SIZE];
    struct timespec interval;
    interval.tv_sec = 0;
    interval.tv_nsec = 10000;
    nanosleep(&interval, NULL);
    sprintf(str, "%d %d %d %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %f", program, input, 0, 0, 0, 1, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 0.0);
    fd = open(SERVER_FIFO_NAME, O_WRONLY);
    write(fd, str, strlen(str)+1);
    close(fd);
    return 0; 
}

int main(int argc, char* argv[]) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(1, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);
    //srand(27);
    if (argc!=3) {
        printf("Abort: the input should be \"program<int> input<int>\"\n");
        return -1;   
    }
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    program = atoi(argv[1]);
    input = atoi(argv[2]);

    int i = 0;
    request_program_run(program, input*100);
    for (i=0; i<NumRequests; i++) {
        request_program_run(program, input);
        sleep(1);
    }
    return 0;
}