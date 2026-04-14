/* averms.c — C replacement for lib/averms.f90
 *
 * subroutine averms(x, n, nskip, ave, rms)
 *   Compute mean and RMS of x(n), excluding nskip samples around the peak.
 *   If nskip < 0, include all samples.
 */
#include <math.h>
#include <stdlib.h>

void averms_(const float *x, const int *n, const int *nskip,
             float *ave, float *rms)
{
    int N  = *n;
    int sk = *nskip;

    /* find 1-based peak index (matches Fortran maxloc) */
    int ipk = 1;
    float xmax = x[0];
    for (int i = 1; i < N; i++) {
        if (x[i] > xmax) { xmax = x[i]; ipk = i + 1; }
    }

    int ns = 0;
    float s = 0.0f, sq = 0.0f;
    for (int i = 0; i < N; i++) {
        int fi = i + 1;           /* 1-based index, matching Fortran */
        if (sk < 0 || abs(fi - ipk) > sk) {
            s  += x[i];
            sq += x[i] * x[i];
            ns++;
        }
    }
    *ave = s / (float)ns;
    *rms = sqrtf(sq / (float)ns - (*ave) * (*ave));
}
