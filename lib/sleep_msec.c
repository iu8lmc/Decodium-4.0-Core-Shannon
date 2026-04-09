/* sleep_msec.c — C replacement for lib/sleep_msec.f90
 *
 * subroutine sleep_msec(n)
 *   Sleep for n milliseconds.
 */
#include <unistd.h>

void sleep_msec_(const int *n)
{
    usleep((unsigned int)(*n) * 1000u);
}
