/* azdist.c — C replacement for lib/azdist.f90
 *
 * subroutine azdist(MyGrid, HisGrid, utch, nAz, nEl, nDmiles, nDkm, nHotAz, nHotABetter)
 *   Compute azimuth, elevation, distance, and propagation path info between grids.
 *   MyGrid, HisGrid: Fortran character(len=*) — space-padded, 4 or 6 chars.
 *     The function fills missing sub-square chars (positions 4-5) with 'm' in-place.
 *   utch: UTC hours (real*8 / double).
 *   Cache: result is reused if grids and utch are within 10 minutes.
 *
 * Fortran ABI: two hidden size_t lengths appended after all explicit args.
 */
#include <math.h>
#include <string.h>
#include <stddef.h>

extern void grid2deg_(char *grid0, float *dlong, float *dlat, size_t grid0_hidden_len);
extern void geodist_ (const float *Eplat, const float *Eplon,
                      const float *Stlat, const float *Stlon,
                      float *Az, float *Baz, float *Dist);

static const float eltab[22] = {
    18.f, 15.f, 13.f, 11.f,  9.f,  8.f,  7.f,  6.f,
     5.3f, 4.7f,  4.f,  3.3f, 2.7f,  2.f,  1.5f, 1.f,
     0.8f, 0.6f,  0.4f, 0.2f, 0.0f, 0.0f
};
static const float daztab[22] = {
    21.f, 18.f, 16.f, 15.f, 14.f, 13.f, 12.f, 11.f,
    10.7f,10.3f,10.f, 10.f, 10.f, 10.f, 10.f, 10.f,
    10.f,  9.f,  9.f,  9.f,  8.f,  8.f
};

void azdist_(char *MyGrid, char *HisGrid, const double *utch,
             int *nAz, int *nEl, int *nDmiles, int *nDkm,
             int *nHotAz, int *nHotABetter,
             size_t MyGrid_len, size_t HisGrid_len)
{
    /* cache keys (saved pre-fill values) */
    static char   mygrid0[6]  = "      ";
    static char   hisgrid0[6] = "      ";
    static double utch0        = -999.0;
    /* cached computed results */
    static float  Az = 0.f, El = 0.f, Dmiles = 0.f, Dkm = 0.f;
    static float  HotA = 0.f, HotB = 0.f;
    static int    HotABetter = 1;

    size_t mgl = MyGrid_len  < 6 ? MyGrid_len  : 6;
    size_t hgl = HisGrid_len < 6 ? HisGrid_len : 6;

    /* Trivial same-grid case */
    if (mgl == hgl && memcmp(MyGrid, HisGrid, mgl) == 0) {
        *nAz = 0; *nEl = 0; *nDmiles = 0; *nDkm = 0;
        *nHotAz = 0; *nHotABetter = 1;
        return;
    }

    /* Cache hit: same grids and within 10 minutes of UTC */
    if (memcmp(MyGrid,  mygrid0,  6) == 0 &&
        memcmp(HisGrid, hisgrid0, 6) == 0 &&
        fabs(*utch - utch0) < 0.1666667)
        goto output;

    /* Save cache keys (pre-fill values) */
    utch0 = *utch;
    memcpy(mygrid0,  MyGrid,  6);
    memcpy(hisgrid0, HisGrid, 6);

    /* Fill missing sub-square with 'm' (modifies caller's buffer, Fortran ABI) */
    if (mgl >= 5 && MyGrid[4]  == ' ') MyGrid[4]  = 'm';
    if (mgl >= 6 && MyGrid[5]  == ' ') MyGrid[5]  = 'm';
    if (hgl >= 5 && HisGrid[4] == ' ') HisGrid[4] = 'm';
    if (hgl >= 6 && HisGrid[5] == ' ') HisGrid[5] = 'm';

    /* Same-grid after sub-square fill */
    if (mgl == hgl && memcmp(MyGrid, HisGrid, mgl) == 0) {
        Az = 0.f; Dmiles = 0.f; Dkm = 0.f; El = 0.f;
        HotA = 0.f; HotB = 0.f; HotABetter = 1;
        goto output;
    }

    {
        float dlong1 = 0.f, dlat1 = 0.f;
        float dlong2 = 0.f, dlat2 = 0.f;
        grid2deg_(MyGrid,  &dlong1, &dlat1, mgl);
        grid2deg_(HisGrid, &dlong2, &dlat2, hgl);

        const float eps = 1.e-6f;
        Az = 0.f; Dmiles = 0.f; Dkm = 0.f; El = 0.f;
        HotA = 0.f; HotB = 0.f; HotABetter = 1;

        if (fabsf(dlat1 - dlat2) < eps && fabsf(dlong1 - dlong2) < eps)
            goto output;

        float difflong = fmodf(dlong1 - dlong2 + 720.0f, 360.0f);
        if (fabsf(dlat1 + dlat2) < eps && fabsf(difflong - 180.0f) < eps) {
            Dkm = 20400.f;
            goto output;
        }

        float Baz;
        geodist_(&dlat1, &dlong1, &dlat2, &dlong2, &Az, &Baz, &Dkm);

        int ndkm = (int)(Dkm / 100.f);
        int j = ndkm - 4;
        if (j < 1)  j = 1;
        if (j > 21) j = 21;

        /* u always computed (matches C++ geo_distance reference) */
        float u = (Dkm - 100.0f * (float)ndkm) / 100.0f;

        if (Dkm < 500.0f) {
            El = 18.0f;
        } else {
            El = (1.0f - u) * eltab[j - 1] + u * eltab[j];
        }

        /* j is 1-based in Fortran → j-1 and j in C (0-based) */
        float daz = daztab[j - 1] + u * (daztab[j] - daztab[j - 1]);
        Dmiles = Dkm / 1.609344f;

        float utchours = (float)*utch;
        float tmid = fmodf(utchours - 0.5f*(dlong1 + dlong2)/15.0f + 48.0f, 24.0f);

        int IamEast = 0;
        if (dlong1 < dlong2) IamEast = 1;
        if (dlong1 == dlong2 && dlat1 > dlat2) IamEast = 0;

        float azEast = IamEast ? Az : Baz;

        if ((azEast >= 45.0f && azEast < 135.0f) ||
            (azEast >= 225.0f && azEast < 315.0f)) {
            HotABetter = 1;
            if (fabsf(tmid - 6.0f) < 6.0f)         HotABetter = 0;
            if ((dlat1 + dlat2) / 2.0f < 0.0f)     HotABetter = !HotABetter;
        } else {
            HotABetter = 0;
            if (fabsf(tmid - 12.0f) < 6.0f)         HotABetter = 1;
        }

        if (IamEast) { HotA = Az - daz; HotB = Az + daz; }
        else         { HotA = Az + daz; HotB = Az - daz; }

        if (HotA < 0.0f)   HotA += 360.0f;
        if (HotA > 360.0f) HotA -= 360.0f;
        if (HotB < 0.0f)   HotB += 360.0f;
        if (HotB > 360.0f) HotB -= 360.0f;
    }

output:
    *nAz         = (int)roundf(Az);
    *nEl         = (int)roundf(El);
    *nDmiles     = (int)roundf(Dmiles);
    *nDkm        = (int)roundf(Dkm);
    *nHotAz      = (int)roundf(HotABetter ? HotA : HotB);
    *nHotABetter = HotABetter;
}
