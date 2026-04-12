/* chkss2.c — C replacement for lib/chkss2.f90
 *
 * subroutine chkss2(ss2, freq, drift, schk)
 *   Compute sync-quality metric from ss2(0:8, 85) by averaging the
 *   normalised power at JT9 sync positions (ii array from jt9sync.f90).
 *
 * Fortran layout: ss2(row, col) → C flat index (col-1)*9 + row
 * Sync positions ii(1..16) are 1-based column indices.
 */

static const int chkss2_ii[16] = {
    1, 2, 5, 10, 16, 23, 33, 35, 51, 52, 55, 60, 66, 73, 83, 85
};

void chkss2_(const float *ss2, const float *freq, const float *drift,
             float *schk)
{
    (void)freq; (void)drift;   /* used only to suppress Fortran compiler warning */

    float ave = 0.0f;
    for (int k = 0; k < 9 * 85; k++) ave += ss2[k];
    ave /= (9.0f * 85.0f);

    float s1 = 0.0f;
    for (int i = 0; i < 16; i++) {
        int j = chkss2_ii[i];
        if (j <= 85)
            s1 += ss2[(j - 1) * 9 + 0] / ave - 1.0f;
    }
    *schk = s1 / 16.0f;
}
