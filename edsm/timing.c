#include <time.h>
#include <sys/time.h>

#include "debug.h"
#include "timing.h"

int64_t get_elapsed_us(struct timeval *start)
{
    struct timeval stop;
    get_monotonic_time(&stop);

    int64_t output = (int64_t)(stop.tv_sec - start->tv_sec) * (int64_t)USECS_PER_SEC +
            (int64_t)(stop.tv_usec - start->tv_usec);
    if(output < 0)
        DEBUG_MSG("Elapsed time < 0 %d %ld - %d %ld = %ld", stop.tv_sec, stop.tv_usec, start->tv_sec, start->tv_usec, output);
    return output;
}

int get_monotonic_time(struct timeval *dst)
{
    struct timespec ts;
    int rtn = clock_gettime(CLOCK_MONOTONIC, &ts);
    dst->tv_sec = ts.tv_sec;
    dst->tv_usec = ts.tv_nsec / NSECS_PER_USEC;

    return rtn;
}