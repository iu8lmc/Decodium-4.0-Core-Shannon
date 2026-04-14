// C++ replacement for the deleted lib/pctile.f90
// Provides the extern "C" pctile_ symbol so Fortran sources that still call
// pctile_ (sync4.f90, sync9.f90, sync9w.f90, downsam9.f90) can link.

#include <algorithm>
#include <cmath>
#include <vector>

extern "C" void pctile_ (float x[], int* npts, int* npct, float* xpct)
{
  int const n = *npts;
  int const p = *npct;
  if (n <= 0 || p < 0 || p > 100)
    {
      *xpct = 1.0f;
      return;
    }
  std::vector<float> tmp (x, x + n);
  std::sort (tmp.begin (), tmp.end ());
  int j = static_cast<int> (std::lround (n * 0.01 * p));
  if (j < 1)
    j = 1;
  if (j > n)
    j = n;
  *xpct = tmp[static_cast<std::size_t> (j - 1)];
}
