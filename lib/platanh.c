/* platanh.c — C replacement for lib/platanh.f90
 *
 * subroutine platanh(x, y)
 *   Piecewise-linear approximation of atanh(x), used in LDPC belief
 *   propagation decoders (bpdecode40, bpdecode128_90, etc.).
 *   Same algorithm already present as platanh_msk144() in MSK144DecodeWorker.cpp.
 */

void platanh_(const float *x, float *y)
{
    float xv = *x;
    float z   = (xv < 0.0f) ? -xv : xv;
    float sgn = (xv < 0.0f) ? -1.0f : 1.0f;

    if      (z <= 0.664f)  *y = xv / 0.83f;
    else if (z <= 0.9217f) *y = sgn * (z - 0.4064f) / 0.322f;
    else if (z <= 0.9951f) *y = sgn * (z - 0.8378f) / 0.0524f;
    else if (z <= 0.9998f) *y = sgn * (z - 0.9914f) / 0.0012f;
    else                   *y = sgn * 7.0f;
}
