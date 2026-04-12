/* genwave.c — C replacement for lib/genwave.f90
 *
 * subroutine genwave(itone,nsym,nsps,nwave,fsample,tonespacing,f0,icmplx,cwave,wave)
 *   Generate an FSK audio waveform or complex baseband signal.
 *   tonespacing is real*8 (double) — ABI difference from the float args.
 *   icmplx == 1 → fill cwave (complex); else → fill wave (real, sin only).
 *
 * Fortran COMPLEX == C float _Complex (re,im layout).
 */
#include <complex.h>
#include <math.h>
#include <string.h>

void genwave_(const int    *itone,
              const int    *nsym,
              const int    *nsps,
              const int    *nwave,
              const float  *fsample,
              const double *tonespacing,
              const float  *f0,
              const int    *icmplx,
              float _Complex *cwave,
              float          *wave)
{
    static const double twopi = 6.283185307179586;

    int nw = *nwave;
    if (*icmplx <= 0) memset(wave,  0, (size_t)nw * sizeof(float));
    if (*icmplx == 1) memset(cwave, 0, (size_t)nw * sizeof(float _Complex));

    double dt  = 1.0 / (double)*fsample;
    double phi = 0.0;
    int k = 0;

    for (int j = 0; j < *nsym; j++) {
        double freq = (double)*f0 + itone[j] * (*tonespacing);
        double dphi = twopi * freq * dt;
        for (int i = 0; i < *nsps; i++) {
            if (*icmplx == 1)
                cwave[k] = (float)cos(phi) + (float)sin(phi) * I;
            else
                wave[k] = (float)sin(phi);
            k++;
            phi += dphi;
            if (phi > twopi) phi -= twopi;
        }
    }
}
