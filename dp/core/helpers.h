#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include "benchmark.h"

#include <stdlib.h>
#include <math.h>
#include <time.h>

#if SCHEDULE_METHOD == METHOD_PI
#define PRE_PROTECTCALL { asm volatile ("cli" :::); }
#define POST_PROTECTCALL { asm volatile ("sti" :::); }
#else 
#define PRE_PROTECTCALL { }
#define POST_PROTECTCALL { }
#endif 

static inline uint64_t get_us()
{
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

static inline uint64_t get_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_nsec;
}

static inline void nsleep(uint64_t ns)
{
	int start = get_ns();
	while (get_ns() - start < ns){
		asm volatile("nop");
		asm volatile("nop");
	}
}

static inline double get_random_expo(double lambda){
    double u;
    u = rand() / (RAND_MAX + 1.0);
    return -log(1- u) / lambda;
}