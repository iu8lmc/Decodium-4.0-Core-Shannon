/* interleave9.c — C replacement for lib/interleave9.f90
 *
 * subroutine interleave9(ia, ndir, ib)
 *   Apply (ndir > 0) or invert (ndir <= 0) a bit-reversal permutation on
 *   206 int8 symbols.  The permutation table j0[] is built from the
 *   8-bit reversals of 0..255 that are <= 205.
 *
 * Note: the original Fortran had a bug where `first=.false.` was commented
 *   out, causing the table to be recomputed on every call.  In C it is
 *   computed exactly once via a static flag.
 */
#include <stdint.h>

static int  j0[206];
static int  j0_inited = 0;

static void build_j0(void)
{
    int k = 0;
    for (int i = 0; i <= 255; i++) {
        int m = i, n = 0;
        for (int b = 0; b < 8; b++) { n = 2 * n + (m & 1); m >>= 1; }
        if (n <= 205) j0[k++] = n;
    }
}

void interleave9_(int8_t *ia, const int *ndir, int8_t *ib)
{
    if (!j0_inited) { build_j0(); j0_inited = 1; }

    if (*ndir > 0) {
        for (int i = 0; i <= 205; i++) ib[j0[i]] = ia[i];
    } else {
        for (int i = 0; i <= 205; i++) ib[i] = ia[j0[i]];
    }
}
