/* flat1.c — C replacement for lib/flat1.f90
 *
 * subroutine flat1(savg, iz, nsmo, syellow)
 *   Compute a running median baseline from savg(iz) using pctile_,
 *   then normalise: syellow = savg / (baseline + 0.001 * peak).
 *
 * pctile_ is provided by Detector/PctileCompat.cpp (extern "C").
 */
extern void pctile_(float *x, int *npts, int *npct, float *xpct);

void flat1_(float *savg, const int *iz, const int *nsmo, float *syellow)
{
    float x[8192];          /* local baseline array, matches Fortran */

    int n    = *iz;
    int sm   = *nsmo;
    int ia   = sm / 2 + 1; /* Fortran 1-based first valid position */
    int ib   = n - sm / 2 - 1;
    int nh   = 10;          /* nstep/2, nstep = 20 */
    int nstep = 20;
    int p50  = 50;

    for (int i = ia; i <= ib; i += nstep) {
        /* pctile(savg(i-sm/2), sm, 50, x(i)) — 1-based indexing → 0-based */
        pctile_(&savg[i - sm/2 - 1], &sm, &p50, &x[i - 1]);
        /* x(i-nh : i+nh-1) = x(i) */
        for (int k = i - nh; k <= i + nh - 1; k++)
            x[k - 1] = x[i - 1];
    }
    /* x(1:ia-1) = x(ia) */
    for (int k = 1; k <= ia - 1; k++)
        x[k - 1] = x[ia - 1];
    /* x(ib+1:iz) = x(ib) */
    for (int k = ib + 1; k <= n; k++)
        x[k - 1] = x[ib - 1];

    /* x0 = 0.001 * maxval(x(iz/10 : 9*iz/10)) */
    int lo = n / 10, hi = 9 * n / 10;
    float xmax = x[lo - 1];
    for (int k = lo; k <= hi; k++)
        if (x[k - 1] > xmax) xmax = x[k - 1];
    float x0 = 0.001f * xmax;

    for (int k = 0; k < n; k++)
        syellow[k] = savg[k] / (x[k] + x0);
}
