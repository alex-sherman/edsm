#ifndef TIMING_H
#define TIMING_H
#include <stdint.h>

#define USECS_PER_SEC   1000000
#define NSECS_PER_USEC  1000

int64_t get_elapsed_us(struct timeval *start);
int get_monotonic_time(struct timeval *dst);

#endif