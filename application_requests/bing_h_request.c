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

#define SERVER_FIFO_NAME "/project/adams/home/zhe.wang/cilk_fifo" //"/home/zhe.wang/cilk_fifo" //"/project/adams/home/zhe.wang/cilk_fifo"
#define PROGRAM_FIFO_NAME_BASE "/project/adams/home/zhe.wang/cilk_fifo_p" //"/home/zhe.wang/cilk_fifo_p" //"/project/adams/home/zhe.wang/cilk_fifo_p"
#define BUF_SIZE 10240

//#define NumRequests 1000000
#define NumRequests 500
//#define NumRequests 500
//#define NumRequests 200
//#define NumRequests 100
//#define NumRequests 10
#define ResultAccuracy 1000000.0
#define RequestWeightMultiplierFirstGroup 200
#define MeanRequestWeigth 3500*4.78
#define POWER_NINE 1000000000

int scale = 2;
//int scale = 1;
//double scale = 0.5;
int id[NumRequests];
struct timespec release[NumRequests];
struct timespec start[NumRequests];
struct timespec finish[NumRequests];
int num_segment[NumRequests];
int done[NumRequests];
int work[NumRequests];
//struct timespec segment_length;

int num_strands = 128;
//int num_strands = 64;
double randarray[NumRequests];
int logarray[NumRequests];
        double PoissonArray[NumRequests];
        double PoissonTail;
        int PoissonArrayPosition;
struct timespec m_timeStartServer, lasttime;
int m_sentRequests;
double exeratio = 0.1;
const char * newLogFileName = "results.txt";

double interArrivalTime = 0.83;
int i = 0;
int program;
int input;


struct __cilk_job{
    int job_id;
    void * data;
    int job_size;

};

void get_time(struct timespec* ts ){
    clock_gettime( CLOCK_MONOTONIC, ts);
}

void sleep_for_ts(struct timespec sleep_time){
  //Otherwise, nanosleep
    //struct timespec zero = { 0, 0 };
    while( nanosleep(&sleep_time,&sleep_time) != 0 )
    {
        if (sleep_time.tv_nsec <= 0 && sleep_time.tv_sec <= 0) 
            break;
    }

    return;
}

int job(int program, int input) {
    int fd;
    char str[BUF_SIZE];
    /*struct timespec interval;
    interval.tv_sec = 0;
    interval.tv_nsec = 10000;
    nanosleep(&interval, NULL);*/
    usleep(5000); //according to test, 5000 guarantee no miss requests
    sprintf(str, "%d %d %d %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %f", program, input, 0, 0, 0, 1, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 
        (unsigned long long)0, (unsigned long long)0, (unsigned long long)0, 0.0);
    fd = open(SERVER_FIFO_NAME, O_WRONLY);
    write(fd, str, strlen(str)+1);
    close(fd);
    return 0; 
}

int main(int argc, char* argv[])
{
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
    struct timespec response;
    struct timespec response2;
    struct timespec curTime;
    struct timespec elapsed;
    struct timespec sleepinter;

    srand48(27);
    struct __cilk_job j;

    //int fd;
    char str[BUF_SIZE];
    if (argc!=3) {
        printf("Abort: the input should be \"program<int> input<int>\"\n");
        return -1;   
    }
    //j.func = foo;
    j.job_id = -1;
       PoissonTail = 0;
        PoissonArrayPosition = 0;
        for (int i = 0; i<NumRequests; i++)
        {
                //double X = ((double)rand() / (double)RAND_MAX);
                double X = drand48();
                //we don't need overflow
                if (X == 1) X -= 0.0001;
                else if (X == 0) X += 0.0001;
                //-1 is lambda rate parameter of the exponential distribution
                //PoissonArray[i] = (log(1 - X) / (-1))  * RequestWeightMultiplierFirstGroup*MeanRequestWeigth;
                //PoissonArray[i] = (log(1 - X) / (-1))  * 1000.0 * interArrivalTime;
                PoissonArray[i] = (log(1 - X) / (-1))  * interArrivalTime;
//lognormal mili second
//PoissonArray[i] = double(logarray[NumRequests-i-1])/2981*interArrivalTime;
        }
        for (int i = 0; i<NumRequests; i++)
        {
                //double y = ((double)rand() / (double)RAND_MAX);
                double y = drand48();
                //we don't need overflow
                if (y == 1)     y -= 0.0001;
                else if (y == 0) y += 0.0001;
                randarray[i] = (log(1 - y) / (-1)) * exeratio;
        }

        get_time(&m_timeStartServer);
        lasttime = m_timeStartServer;
        //printf("lasttime is %ld\n", lasttime.tv_sec + lasttime.tv_nsec * POWER_NINE);
         sleepinter.tv_sec= ((int)(interArrivalTime/1000/10)); //do exame here
         sleepinter.tv_nsec = ((int)((interArrivalTime/1000/10-(int)(interArrivalTime/1000/10))*1000000000));

	int i = m_sentRequests = 0;
	//for (int i = 0; i < NumRequests; i++)
	while (m_sentRequests < NumRequests)
	{
        //printf("NumRequest is %d\n", NumRequests);
        //printf("sleep %d %d\n", sleepinter.tv_sec, sleepinter.tv_nsec);
        sleep_for_ts(sleepinter);
                        
        get_time(&curTime);
        //printf("curTime is %ld\n", curTime.tv_sec + curTime.tv_nsec * POWER_NINE);
        //ts_diff(lasttime, curTime, elapsed);
        if(lasttime.tv_nsec + (unsigned long long)lasttime.tv_sec * POWER_NINE > curTime.tv_nsec + (unsigned long long) curTime.tv_sec * POWER_NINE){
        elapsed.tv_nsec = lasttime.tv_nsec - curTime.tv_nsec;
        elapsed.tv_sec = lasttime.tv_sec - curTime.tv_sec;
      } else {
        elapsed.tv_nsec = curTime.tv_nsec - lasttime.tv_nsec;
        elapsed.tv_sec = curTime.tv_sec - lasttime.tv_sec;
      }

      if( elapsed.tv_nsec < 0 ){            //If we have a negative nanoseconds
        elapsed.tv_nsec += nanosec_in_sec; //value then we carry over from the
        elapsed.tv_sec -= 1;               //seconds part of the struct timespec
      }
        double elapsedMS = elapsed.tv_sec*1000.0+(elapsed.tv_nsec)/1000000.0;
        double t = PoissonTail + elapsedMS;
        lasttime = curTime;

        while (m_sentRequests < NumRequests)
        {
            if (PoissonArray[PoissonArrayPosition]<t)
            {
    //std::cout << PoissonArray[PoissonArrayPosition] <<" "<<t<<" "<<elapsedMS<<" "<<elapsed<<" "<<curTime<<std::endl;
                t -= PoissonArray[PoissonArrayPosition];
                PoissonArray[PoissonArrayPosition] = -1;
                PoissonArrayPosition++;
                double X = drand48();
                //we don't need overflow
                if (X == 1) X -= 0.0001;
                else if (X == 0) X += 0.0001;
                //-1 is lambda rate parameter of the exponential distribution
                double m_weight = (log(1 - X) / (-1))  * RequestWeightMultiplierFirstGroup*MeanRequestWeigth;
        		int weight = (int) m_weight;

                //Bing workload 2 -- new

                
			    int localEnd = 10000;
			    if(weight < 10) localEnd = 0;
			    else if(weight%1000 < 367) localEnd /= 2;
			    else if(weight%1000 < 551) localEnd *= 1;
			    else if(weight%1000 < 631) localEnd *= 1.5;
			    else if(weight%1000 < 691) localEnd *= 2;
			    else if(weight%1000 < 734) localEnd *= 2.5;
			    else if(weight%1000 < 767) localEnd *= 3;
			    else if(weight%1000 < 794) localEnd *= 3.5;
			    else if(weight%1000 < 817) localEnd *= 4;
			    else if(weight%1000 < 837) localEnd *= 4.5;
			    else if(weight%1000 < 854) localEnd *= 5;
			    else if(weight%1000 < 867) localEnd *= 5.5;
			    else if(weight%1000 < 877) localEnd *= 6;
			    else if(weight%1000 < 891) localEnd *= 6.75;
			    else if(weight%1000 < 901) localEnd *= 7.75;
			    else if(weight%1000 < 927) localEnd *= 10;
			    else if(weight%1000 < 949) localEnd *= 14;
			    else if(weight%1000 < 959) localEnd *= 17.5;
			    else if(weight%1000 < 969) localEnd *= 19;
			    else if(weight%1000 < 984) localEnd *= 19.5;
			    else                       localEnd *= 20;
                
        		int mywork = localEnd*scale; //microsec? 9750
        		int totaliter = mywork/1; //630
        		num_segment[i] = totaliter/num_strands; //10
        		work[i] = mywork;	

        		j.job_id++;
        		get_time(&release[i]);
        		id[i] = j.job_id;
            //printf("xxxxxx0xxxxxx data = %ld %ld.\n", release[i].tv_sec,release[i].tv_nsec);
        		j.data = (void *)&id[i];
        		//j.job_size = num_segment[i];
        		j.job_size = 10;//num_segment[i]/100;
                //printf("num_segment is %d, m_sentRequests: %d, NumRequests: %d\n", num_segment[j.job_id], m_sentRequests, NumRequests);
                job(program, num_segment[j.job_id]);
        		m_sentRequests++;
        		i = m_sentRequests;
            }       
            else
            {
                //printf("break! m_sentRequests: %d, NumRequests: %d\n", m_sentRequests, NumRequests);
                PoissonTail = t;
                break;
            }       
        }
    }
    return 0;
    //close(fd);
    //unsigned int microseconds = 10000000;
    unsigned int microseconds = 100000;

    for (int i = 0; i<NumRequests; i++) {
    	while (done[i] == 0)
    		usleep(microseconds);
    }

    double maxlatency = 0;
    double totallatency = 0;
    double parallelism = 0;

    for (int i = 0; i<NumRequests; i++) {
    	//ts_diff(release[i], finish[i], response);
        if( release[i].tv_nsec + (unsigned long long)release[i].tv_sec * POWER_NINE > finish[i].tv_nsec + (unsigned long long)finish[i].tv_sec * POWER_NINE ){
            response.tv_nsec = release[i].tv_nsec - finish[i].tv_nsec;
            response.tv_sec = release[i].tv_sec - finish[i].tv_sec;
          } else {
            response.tv_nsec = finish[i].tv_nsec - release[i].tv_nsec;
            response.tv_sec = finish[i].tv_sec - release[i].tv_sec;
          }

          if( response.tv_nsec < 0 ){            //If we have a negative nanoseconds
            response.tv_nsec += nanosec_in_sec; //value then we carry over from the
            response.tv_sec -= 1;               //seconds part of the struct timespec
          }
    	double tmptime = response.tv_sec+response.tv_nsec/1000000000.0;
    	
    	//ts_diff(start[i], finish[i], response2);
          if( start[i].tv_nsec + (unsigned long long)start[i].tv_sec * POWER_NINE > finish[i].tv_nsec + (unsigned long long)finish[i].tv_sec * POWER_NINE ){
            response2.tv_nsec = start[i].tv_nsec - finish[i].tv_nsec;
            response2.tv_sec = start[i].tv_sec - finish[i].tv_sec;
          } else {
            response2.tv_nsec = finish[i].tv_nsec - start[i].tv_nsec;
            response2.tv_sec = finish[i].tv_sec - start[i].tv_sec;
          }

          if( response2.tv_nsec < 0 ){            //If we have a negative nanoseconds
            response2.tv_nsec += nanosec_in_sec; //value then we carry over from the
            response2.tv_sec -= 1;               //seconds part of the struct timespec
          }
    	double tmptime2 = response2.tv_sec+response2.tv_nsec/1000000000.0;
    	if (tmptime > maxlatency) maxlatency=tmptime;
    	totallatency += tmptime;
    	parallelism += ((double)work[i]/1000000.0/tmptime2);
    	//fprintf(stderr,"%lf\n",tmptime);
    }
    FILE *fp;
    fp=fopen("result.txt","wa+");
    fprintf(fp,"~~~~~~~~~~~~end~~~~~~~~~~~~ avg %lf max %lf par %lf \n", totallatency/((double) NumRequests), maxlatency, parallelism/((double) NumRequests));//同输出printf一样，以格式方式输出到文本中
    fclose(fp);
    //printf("~~~~~~~~~~~~end~~~~~~~~~~~~ avg %lf max %lf par %lf \n", totallatency/((double) NumRequests), maxlatency, parallelism/((double) NumRequests));
    return 0;
}