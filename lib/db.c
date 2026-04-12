/* db.c — C replacement for lib/db.f90
 *
 * real function db(x)
 *   Returns 10*log10(x), clipped to -99 dB floor.
 */
#include <math.h>

float db_(const float *x)
{
    return (*x > 1.259e-10f) ? 10.0f * log10f(*x) : -99.0f;
}
