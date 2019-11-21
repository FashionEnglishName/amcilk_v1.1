#ifndef RT_GOMP_TIMESPEC_FUNCTIONS_H
#define RT_GOMP_TIMESPEC_FUNCTIONS_H

#include <time.h>

const long int nanosec_in_sec = 1000000000;
const long int millisec_in_sec = 1000;
const long int nanosec_in_millisec = 1000000;



void ts_diff (struct timespec ts1, struct timespec ts2, struct timespec result);
void sleep_until_ts (struct timespec end_time);
void sleep_for_ts (struct timespec sleep_time);
void busy_work(struct timespec length);




#endif /* RT_GOMP_TIMESPEC_FUNCTIONS_H */