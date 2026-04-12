/* deg2grid.c — C replacement for lib/deg2grid.f90
 *
 * subroutine deg2grid(dlong0, dlat, grid)
 *   Convert West longitude + North latitude (degrees) to a 6-character
 *   Maidenhead grid locator written into the Fortran character*6 grid.
 *
 * Fortran ABI: character*6 grid → hidden length appended after dlat.
 */
#include <stddef.h>

void deg2grid_(const float *dlong0, const float *dlat,
               char *grid, size_t grid_hidden_len)
{
    (void)grid_hidden_len;
    float dlong = *dlong0;
    if (dlong < -180.0f) dlong += 360.0f;
    if (dlong >  180.0f) dlong -= 360.0f;

    /* longitude: working east from 180° in units of 5 minutes */
    int nlong = (int)(60.0f * (180.0f - dlong) / 5.0f);
    int n1 = nlong / 240;
    int n2 = (nlong - 240 * n1) / 24;
    int n3 =  nlong - 240 * n1 - 24 * n2;
    grid[0] = (char)('A' + n1);   /* field  letter  */
    grid[2] = (char)('0' + n2);   /* square digit   */
    grid[4] = (char)('a' + n3);   /* subsquare letter */

    /* latitude: working north from -90° in units of 2.5 minutes */
    int nlat = (int)(60.0f * (*dlat + 90.0f) / 2.5f);
    n1 = nlat / 240;
    n2 = (nlat - 240 * n1) / 24;
    n3 =  nlat - 240 * n1 - 24 * n2;
    grid[1] = (char)('A' + n1);
    grid[3] = (char)('0' + n2);
    grid[5] = (char)('a' + n3);
}
