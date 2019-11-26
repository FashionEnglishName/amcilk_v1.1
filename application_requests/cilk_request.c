#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

int program;
int input;

int main(int argc, char* argv[]) {
    int fd;
    char str[BUF_SIZE];
    if (argc!=3) {
    	printf("Abort: the input should be \"program<int> input<int>\"\n");
    	return -1;   
    }

    program = atoi(argv[1]);
    input = atoi(argv[2]);

    sprintf(str, "%d %d %d %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %f", 
        program, input, 0, 0, 0, 1,
        (unsigned long long)1, (unsigned long long)0, (unsigned long long)0, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 
        (unsigned long long)0, (unsigned long long)0, 0.0);
    fd = open(SERVER_FIFO_NAME, O_WRONLY);
    write(fd, str, strlen(str)+1);
    close(fd);
   	return 0; 
}