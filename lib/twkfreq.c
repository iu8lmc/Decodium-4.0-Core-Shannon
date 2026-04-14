/* twkfreq.c — C replacement for lib/twkfreq.f90
 *
 * subroutine twkfreq(c3, c4, npts, fsample, a)
 *   Frequency-correct a complex signal c3 → c4 using a 3-parameter model:
 *     dphi = (a(1) + x*a(2) + p2*a(3)) * (2π/fsample)
 *   where x ∈ [-1,1] is normalised position and p2 is the Legendre P2.
 *
 * Fortran COMPLEX == C float _Complex (IEEE 754, (re,im) layout).
 */
#include <complex.h>
#include <math.h>

void twkfreq_(float _Complex *c3, float _Complex *c4,
              const int *npts, const float *fsample, const float *a)
{
    static const float twopi = 6.283185307f;
    int   n  = *npts;
    float fs = *fsample;

    float _Complex w = 1.0f + 0.0f * I;
    float x0 = 0.5f * (float)(n + 1);
    float s  = 2.0f / (float)n;

    for (int i = 1; i <= n; i++) {
        float x    = s * ((float)i - x0);
        float p2   = 1.5f * x * x - 0.5f;
        float dphi = (a[0] + x * a[1] + p2 * a[2]) * (twopi / fs);
        float _Complex wstep = cosf(dphi) + sinf(dphi) * I;
        w = w * wstep;
        c4[i - 1] = w * c3[i - 1];
    }
}
