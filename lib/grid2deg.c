/* grid2deg.c — C replacement for lib/grid2deg.f90
 *
 * subroutine grid2deg(grid0, dlong, dlat)
 *   Convert a 6-character Maidenhead grid locator to West longitude and
 *   North latitude in degrees.
 *
 * Fortran ABI: character*(*) grid0 → hidden length appended after dlat.
 */
#include <stddef.h>

void grid2deg_(char *grid0, float *dlong, float *dlat, size_t grid0_hidden_len)
{
    /* copy into local 6-char buffer, space-padding if shorter */
    char grid[6];
    int len = (int)grid0_hidden_len;
    for (int i = 0; i < 6; i++)
        grid[i] = (i < len) ? grid0[i] : ' ';

    /* if subsquare letter (position 5, 0-based 4) is missing or out of range,
       default both subsquare chars to 'mm' (matches Fortran: grid(5:6)='mm') */
    int c5 = (unsigned char)grid[4];
    if (grid[4] == ' ' || c5 <= 64 || c5 >= 128) {
        grid[4] = 'm'; grid[5] = 'm';
    }

    /* normalise case: field letters (0,1) → uppercase, subsquare (4,5) → lowercase */
    if (grid[0] >= 'a' && grid[0] <= 'z') grid[0] -= ('a' - 'A');
    if (grid[1] >= 'a' && grid[1] <= 'z') grid[1] -= ('a' - 'A');
    if (grid[4] >= 'A' && grid[4] <= 'Z') grid[4] += ('a' - 'A');
    if (grid[5] >= 'A' && grid[5] <= 'Z') grid[5] += ('a' - 'A');

    /* longitude: West, degrees */
    int   nlong    = 180 - 20 * (grid[0] - 'A');
    int   n20d     = 2  *  (grid[2] - '0');
    float xminlong = 5.0f * ((grid[4] - 'a') + 0.5f);
    *dlong = (float)(nlong - n20d) - xminlong / 60.0f;

    /* latitude: North, degrees */
    int   nlat   = -90 + 10 * (grid[1] - 'A') + (grid[3] - '0');
    float xminlat = 2.5f * ((grid[5] - 'a') + 0.5f);
    *dlat = (float)nlat + xminlat / 60.0f;
}
