/* slope.c — C replacement for lib/slope.f90
 *
 * subroutine slope(y, npts, xpk)
 *   Remove best-fit linear slope from y(npts), ignoring the peak region
 *   xpk +/- 4 bins during the fit, then normalise by RMS.
 *   x-coordinates are 1-based integers (matching the Fortran x=i loop).
 */
#include <math.h>

void slope_(float *y, const int *npts, const float *xpk)
{
    int   n  = *npts;
    float pk = *xpk;

    float sumw = 0.0f, sumx = 0.0f, sumy  = 0.0f;
    float sumx2 = 0.0f, sumxy = 0.0f;

    for (int i = 0; i < n; i++) {
        float xi = (float)(i + 1);   /* Fortran: x = i (1-based) */
        if (fabsf(xi - pk) > 4.0f) {
            sumw  += 1.0f;
            sumx  += xi;
            sumy  += y[i];
            sumx2 += xi * xi;
            sumxy += xi * y[i];
        }
    }

    float delta = sumw * sumx2 - sumx * sumx;
    float a = (sumx2 * sumy  - sumx  * sumxy) / delta;
    float b = (sumw  * sumxy - sumx  * sumy)  / delta;

    float sq = 0.0f;
    for (int i = 0; i < n; i++) {
        float xi = (float)(i + 1);
        y[i] -= (a + b * xi);
        if (fabsf(xi - pk) > 4.0f) sq += y[i] * y[i];
    }

    float rms = sqrtf(sq / (sumw - 4.0f));
    for (int i = 0; i < n; i++) y[i] /= rms;
}
