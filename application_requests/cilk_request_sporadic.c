#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../runtime/rts-config.h"

int program;
int input;
int control_uid;
int second_level_uid;

int main(int argc, char* argv[]) {
    int fd;
    char str[BUF_SIZE];
    if (argc!=5) {
    	printf("Abort: the input should be \"program<int> input<int> control_uid<int> second_level_uid<int>\"\n");
    	return -1;   
    }

    program = atoi(argv[1]);
    input = atoi(argv[2]);
    control_uid = atoi(argv[3]);
    second_level_uid = atoi(argv[4]);
    if (control_uid!=0) {
        printf("Contract Error: control_uid of sporadic program must be 0!\n");
        exit(0);
    }

    char program_fifo_path[BUF_SIZE];
    sprintf(program_fifo_path, "%s_%d_%d", PROGRAM_FIFO_NAME_BASE, control_uid, second_level_uid);
    unlink(program_fifo_path);
    mkfifo(program_fifo_path, S_IFIFO|0666);

    sprintf(str, "%d %d %d %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %f", program, input, control_uid, second_level_uid, 0, 0, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 0.0);
    fd = open(SERVER_FIFO_NAME, O_WRONLY);
    write(fd, str, strlen(str)+1);
    close(fd);

    int ret_fscanf;
    FILE *fp;
    fp = fopen(program_fifo_path, "r");
    if (fp == NULL) {
        printf("ERROR: fopen failed!\n");
        exit(-1);
    }
    int result;
    float run_time;
    unsigned long long nothing1;
    unsigned long long nothing2;
    ret_fscanf = fscanf(fp, "%d %f %llu %llu", &result, &run_time, &nothing1, &nothing2);
    if (ret_fscanf == -EOF) {
        printf("End of fifo\n");
    }
    if (ret_fscanf < 0) {
        printf("fscanf failed!");
        exit(-1);
    }
    fclose(fp);
    printf("result: %d\ntime: %f\n", result, run_time);
    unlink(program_fifo_path);
   	return 0; 
}