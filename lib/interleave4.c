/* interleave4.c — C replacement for lib/interleave4.f90
 *
 * subroutine interleave4(id, ndir)
 *   Apply (ndir == 1) or invert (ndir != 1) a bit-reversal permutation
 *   in-place on 206 int8 symbols.  Same permutation table as interleave9.
 */
#include <stdint.h>
#include <string.h>

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

void interleave4_(int8_t *id, const int *ndir)
{
    if (!j0_inited) { build_j0(); j0_inited = 1; }

    int8_t itmp[206];
    if (*ndir == 1) {
        for (int i = 0; i <= 205; i++) itmp[j0[i]] = id[i];
    } else {
        for (int i = 0; i <= 205; i++) itmp[i] = id[j0[i]];
    }
    memcpy(id, itmp, 206);
}
