/* entail.c — C replacement for lib/entail.f90
 *
 * subroutine entail(dgen, data0)
 *   Pack 12 × 6-bit values from dgen(13) into 9 signed bytes in data0(13),
 *   MSB first, then zero data0(10..13).
 */
#include <stdint.h>

void entail_(const int *dgen, int8_t *data0)
{
    int i4 = 0, k = 0, m = 0;

    for (int i = 0; i < 12; i++) {
        int n = dgen[i];
        for (int j = 1; j <= 6; j++) {
            k++;
            i4 = ((i4 << 1) | ((n >> (6 - j)) & 1)) & 0xFF;
            if (k == 8) {
                data0[m++] = (int8_t)(i4 > 127 ? i4 - 256 : i4);
                k = 0;
            }
        }
    }
    /* zero Fortran elements 10..13 (0-based 9..12) */
    for (int mm = 9; mm < 13; mm++)
        data0[mm] = 0;
}
