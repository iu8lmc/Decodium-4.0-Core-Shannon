/* degrade_snr.c — C replacement for lib/degrade_snr.f90
 *
 * subroutine degrade_snr(d2, npts, db, bw)
 *   Add white Gaussian noise to degrade S/N by db dB within bandwidth bw Hz.
 *   d2(npts): int16 samples at 12 kHz.
 */
#include <math.h>

extern float gran_(void);   /* lib/gran.c */

void degrade_snr_(short *d2, const int *npts, const float *db, const float *bw)
{
    int n = *npts;
    float p0 = 0.0f;
    for (int i = 0; i < n; i++) {
        float x = (float)d2[i];
        p0 += x * x;
    }
    p0 /= (float)n;
    if (*bw > 0.0f) p0 *= 6000.0f / *bw;

    float s   = sqrtf(p0 * (powf(10.0f, 0.1f * *db) - 1.0f));
    float fac = sqrtf(p0 / (p0 + s * s));

    for (int i = 0; i < n; i++) {
        float val = fac * ((float)d2[i] + s * gran_());
        /* nint equivalent: round to nearest, clamp to int16 range */
        int v = (int)(val >= 0.0f ? val + 0.5f : val - 0.5f);
        if (v >  32767) v =  32767;
        if (v < -32768) v = -32768;
        d2[i] = (short)v;
    }
}
