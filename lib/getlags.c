/* getlags.c — C replacement for lib/getlags.f90
 *
 * subroutine getlags(nsps8, lag0, lag1, lag2)
 *   Return lag parameters for a given samples-per-symbol count.
 */
#include <stdlib.h>

void getlags_(const int *nsps8, int *lag0, int *lag1, int *lag2)
{
    switch (*nsps8) {
        case 864:   *lag1 = 39;  *lag2 = 291; *lag0 = 123; break;
        case 1920:  *lag1 = 70;  *lag2 = 184; *lag0 = 108; break;
        case 5120:  *lag1 = 84;  *lag2 = 129; *lag0 = 99;  break;
        case 10368: *lag1 = 91;  *lag2 = 112; *lag0 = 98;  break;
        case 31500: *lag1 = 93;  *lag2 = 102; *lag0 = 96;  break;
        default:    abort();  /* matches Fortran: stop 'Error in getlags' */
    }
}
