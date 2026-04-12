/* plotsave.c — C replacement for lib/plotsave.f90
 *
 * subroutine plotsave(swide, nw, nh, irow)
 *   Manage a scrolling 2-D float buffer used by the waterfall display.
 *   irow == -99 : free/reset the buffer
 *   irow  <  0  : push swide as a new row (older rows shift down)
 *   irow  >= 0  : copy stored row irow back into swide
 *
 * Buffer layout: sw[j * nw + i]  ≡  Fortran sw(i, j), 0-based, column-major.
 */
#include <stdlib.h>
#include <string.h>

static float *sw   = NULL;
static int    nw0  = -1;
static int    nh0  = -1;

void plotsave_(float *swide, const int *nw, const int *nh, const int *irow)
{
    int w   = *nw;
    int h   = *nh;
    int row = *irow;

    if (row == -99) {
        free(sw); sw = NULL; nw0 = -1; nh0 = -1;
        return;
    }

    if (w != nw0 || h != nh0 || !sw) {
        free(sw);
        sw  = (float *)calloc((size_t)(w * h), sizeof(float));
        nw0 = w;
        nh0 = h;
    }

    if (row < 0) {
        /* push: shift rows 0..h-2 → 1..h-1, then fill row 0 */
        for (int j = h - 1; j >= 1; j--)
            memcpy(sw + j * w, sw + (j - 1) * w, (size_t)w * sizeof(float));
        memcpy(sw, swide, (size_t)w * sizeof(float));
    } else {
        /* read: return stored row */
        memcpy(swide, sw + row * w, (size_t)w * sizeof(float));
    }
}
