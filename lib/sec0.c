/* sec0.c — C replacement for lib/sec0.f90
 *
 * subroutine sec0(n, t)
 *   Simple execution timer.
 *   n=0: record start time; n!=0: return elapsed seconds in t.
 */
#include <time.h>

static long long sec0_count0_ns = 0;

void sec0_(const int *n, float *t)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long now_ns = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    if (*n == 0) {
        sec0_count0_ns = now_ns;
    } else {
        *t = (float)(now_ns - sec0_count0_ns) * 1.0e-9f;
    }
}
