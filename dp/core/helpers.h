#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <math.h>
#include <time.h>

static inline long get_ms(void)
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    ms = round(spec.tv_nsec / 1.0e6);

    if (ms > 999) {
        ms = 0;
    }

    return ms;
}