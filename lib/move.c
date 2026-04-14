/* move.c — C replacement for lib/move.f90
 *
 * subroutine move(x, y, n)
 *   real x(n), y(n)
 *   y = x  (element-wise copy)
 */
#include <string.h>

void move_(const float *x, float *y, const int *n)
{
    memcpy(y, x, (size_t)*n * sizeof(float));
}
