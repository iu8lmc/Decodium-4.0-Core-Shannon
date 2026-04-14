/* chkhist.c — C replacement for lib/chkhist.f90
 *
 * subroutine chkhist(mrsym, nmax, ipk)
 *   Histogram mode finder over a fixed 63-element integer array.
 *   mrsym values are expected in 0..63.
 *   Returns nmax (count of mode) and ipk (mode value + 1).
 */

void chkhist_(const int *mrsym, int *nmax, int *ipk)
{
    int hist[64] = {0};

    for (int j = 0; j < 63; j++)
        hist[mrsym[j]]++;

    *nmax = 0;
    for (int i = 0; i < 64; i++) {
        if (hist[i] > *nmax) {
            *nmax = hist[i];
            *ipk  = i + 1;
        }
    }
}
