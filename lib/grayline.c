/* grayline.c — C replacement for lib/grayline.f90
 *
 * subroutine grayline(nyear, month, nday, uth, mygrid, nduration, isun)
 *   Determine whether the station is in sunrise grayline (0), day (1),
 *   sunset grayline (2), or night (3) at the given UTC and grid locator.
 *   Delegates to grid2deg_ and sun_ which remain in wsjt_fort.
 *
 * Fortran ABI: character*6 mygrid → hidden length appended last.
 */
#include <stddef.h>

/* grid2deg(grid0, dlong, dlat): Fortran still in wsjt_fort */
extern void grid2deg_(char *grid0, float *dlong, float *dlat, size_t grid0_len);

/* sun(nyear, month, nday, uth, lon, lat, rasun, decsun, lst, azsun, elsun, mjd, day) */
extern void sun_(const int *nyear, const int *month, const int *nday,
                 const float *uth, const float *lon, const float *lat,
                 float *rasun, float *decsun, float *lst,
                 float *azsun, float *elsun, float *mjd, float *day);

void grayline_(const int *nyear, const int *month, const int *nday,
               const float *uth, char *mygrid, const int *nduration, int *isun,
               size_t mygrid_hidden_len)
{
    float elon, lat;
    grid2deg_(mygrid, &elon, &lat, mygrid_hidden_len);
    float lon = -elon;

    float uth0 = *uth - 0.5f * (float)*nduration / 60.0f;
    float uth1 = *uth + 0.5f * (float)*nduration / 60.0f;

    float rasun, decsun, lst, azsun, elsun0, mjd, day;
    sun_(nyear, month, nday, &uth0, &lon, &lat,
         &rasun, &decsun, &lst, &azsun, &elsun0, &mjd, &day);

    float elsun1;
    sun_(nyear, month, nday, &uth1, &lon, &lat,
         &rasun, &decsun, &lst, &azsun, &elsun1, &mjd, &day);

    static const float elchk = -0.8333f;
    if      (elsun0 < elchk && elsun1 >= elchk) *isun = 0;
    else if (elsun0 > elchk && elsun1 <= elchk) *isun = 2;
    else if (elsun1 > elchk)                    *isun = 1;
    else                                         *isun = 3;
}
