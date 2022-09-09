#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "ci_lib.h"
#include "loop.h"

#define DEBUG

uint64_t accuracy_test [1024*1024*2] = {0}; 
uint64_t accuracy_iter = 0;

static inline uint64_t get_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_nsec;
}


void interrupt_handler(long ic)
{
    printf("handler: %lu\n", ic);
    // accuracy_test[accuracy_iter ++] = get_ns();
}


int main()
{
    // // Faster access to array - load cache
    // for (size_t i = 0; i < sizeof(accuracy_test) / sizeof(uint64_t) ; i++)
    // {
    //     accuracy_test[i] = 0;
    // }

    // For vdso
    for (size_t i = 0; i < 1000; i++)
    {
        get_ns();
    }

    register_ci(20, 20, interrupt_handler);

    uint64_t start = get_ns();
    for (size_t i = 0; i < 10; i++)
    {
        not_loop_nops();
    }
    uint64_t end = get_ns();


// #ifdef DEBUG
//     for (size_t i = 0; i < accuracy_iter - 1; i++)
//     {
//         printf("%lu, ", accuracy_test[i+1] - accuracy_test[i] );
//     }
//     printf("\n");
// #endif

    printf("time: %llu\n", end -start);
    return 0;
}