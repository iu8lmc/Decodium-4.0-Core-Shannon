/* smo.c — C replacement for lib/smo.f90
 *
 * subroutine smo(x, npts, y, nadd)
 *   Sliding-window sum of width nadd centred on each element.
 *   Boundary (half-width nh) elements are zeroed.
 *   Result is written back into x.
 */
#include <string.h>

void smo_(float *x, const int *npts, float *y, const int *nadd)
{
    int n  = *npts;
    int nh = *nadd / 2;

    /* Fortran: do i=1+nh, npts-nh  →  0-based: i = nh .. n-nh-1 */
    for (int i = nh; i < n - nh; i++) {
        float sum = 0.0f;
        for (int j = -nh; j <= nh; j++)
            sum += x[i + j];
        y[i] = sum;
    }

    memcpy(x, y, (size_t)n * sizeof(float));

    /* zero boundary regions */
    for (int i = 0; i < nh; i++)     x[i] = 0.0f;
    for (int i = n - nh; i < n; i++) x[i] = 0.0f;
}
