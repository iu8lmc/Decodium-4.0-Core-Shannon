/* graycode.c — C replacement for lib/graycode.f90 + lib/graycode65.f90
 *
 * subroutine graycode(ia, n, idir, ib)
 *   Apply/remove Gray code to array ia(n) -> ib(n).
 *
 * subroutine graycode65(dat, n, idir)
 *   Apply/remove Gray code in-place on dat(n).
 *
 * Both delegate to igray() already implemented in lib/igray.c.
 */

int igray_(const int *n, const int *idir);

void graycode_(int *ia, const int *n, const int *idir, int *ib)
{
    int idir_val = *idir;
    for (int i = 0; i < *n; i++)
        ib[i] = igray_(&ia[i], &idir_val);
}

void graycode65_(int *dat, const int *n, const int *idir)
{
    int idir_val = *idir;
    for (int i = 0; i < *n; i++)
        dat[i] = igray_(&dat[i], &idir_val);
}
