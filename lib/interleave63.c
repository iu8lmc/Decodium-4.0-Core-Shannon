/* interleave63.c — C replacement for lib/interleave63.f90
 *
 * subroutine interleave63(d1, idir)
 *   Interleave (idir >= 0) or de-interleave (idir < 0) 63 integers
 *   treated as a 7×9 matrix (Fortran d1(0:6, 0:8)), by transposing to 9×7.
 *
 * Fortran column-major layout:
 *   d1(i, j) lives at flat offset j*7 + i    (shape 0:6 × 0:8)
 *   d2(j, i) lives at flat offset i*9 + j    (shape 0:8 × 0:6)
 * The move(d2,d1,63) call is a raw 63-int (=63-float) memcpy.
 */
#include <string.h>

void interleave63_(int *d1, const int *idir)
{
    int d2[63];

    if (*idir >= 0) {
        /* d2(j,i) = d1(i,j)  →  transpose d1 into d2, then copy back */
        for (int i = 0; i <= 6; i++)
            for (int j = 0; j <= 8; j++)
                d2[i * 9 + j] = d1[j * 7 + i];
        memcpy(d1, d2, 63 * sizeof(int));
    } else {
        /* copy d1 → d2, then d1(i,j) = d2(j,i)  →  transpose back */
        memcpy(d2, d1, 63 * sizeof(int));
        for (int i = 0; i <= 6; i++)
            for (int j = 0; j <= 8; j++)
                d1[j * 7 + i] = d2[i * 9 + j];
    }
}
