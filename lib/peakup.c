/* peakup.c — C replacement for lib/peakup.f90
 *
 * subroutine peakup(ym, y0, yp, dx)
 *   Parabolic interpolation: find sub-sample peak offset dx
 *   given three samples: y-minus (ym), y-center (y0), y-plus (yp).
 */

void peakup_(const float *ym, const float *y0, const float *yp, float *dx)
{
    float b = *yp - *ym;
    float c = *yp + *ym - 2.0f * *y0;
    *dx = (c != 0.0f) ? -b / (2.0f * c) : 0.0f;
}
