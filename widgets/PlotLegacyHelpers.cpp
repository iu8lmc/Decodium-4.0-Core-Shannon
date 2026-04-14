#include "PlotLegacyHelpers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <vector>

extern "C"
{
  void pctile_ (float x[], int* npts, int* npct, float* xpct);
  void polyfit_ (double x[], double y[], double sigmay[], int* npts, int* nterms, int* mode,
                 double a[], double* chisqr);
}

namespace
{
struct PlotSaveState
{
  int width {-1};
  int height {-1};
  std::vector<float> rows;
};

PlotSaveState& plotsave_state ()
{
  static PlotSaveState state;
  return state;
}
}

namespace decodium
{
namespace plot
{

void flat4_inplace (float* spectrum, int npts0, int nflatten)
{
  if (!spectrum || npts0 <= 0)
    {
      return;
    }

  int const npts = std::min (6827, npts0);
  if (spectrum[0] > 1.0e29f)
    {
      return;
    }

  for (int i = 0; i < npts; ++i)
    {
      spectrum[i] = 10.0f * std::log10 (spectrum[i]);
    }

  if (nflatten <= 0)
    {
      return;
    }

  int nterms = 5;
  if (nflatten == 2)
    {
      nterms = 1;
    }

  constexpr int nseg = 10;
  int npct = 10;
  int const nlen = npts / nseg;
  int const i0 = npts / 2;

  std::array<double, 1000> x {};
  std::array<double, 1000> y {};
  int k = 0;

  for (int n = 1; n <= nseg; ++n)
    {
      int ib = n * nlen;
      int const ia = ib - nlen + 1;
      if (n == nseg)
        {
          ib = npts;
        }

      if (ia < 1 || ib < ia || ib > npts)
        {
          continue;
        }

      int count = ib - ia + 1;
      float base = 0.0f;
      pctile_ (spectrum + (ia - 1), &count, &npct, &base);
      for (int i = ia; i <= ib; ++i)
        {
          if (spectrum[i - 1] <= base)
            {
              if (k < static_cast<int> (x.size ()))
                {
                  x[static_cast<size_t> (k)] = static_cast<double> (i - i0);
                  y[static_cast<size_t> (k)] = static_cast<double> (spectrum[i - 1]);
                  ++k;
                }
            }
        }
    }

  int kz = k;
  int mode = 0;
  std::array<double, 5> coeffs {};
  double chisqr = 0.0;
  polyfit_ (x.data (), y.data (), y.data (), &kz, &nterms, &mode, coeffs.data (), &chisqr);

  for (int i = 1; i <= npts; ++i)
    {
      double const t = static_cast<double> (i - i0);
      double const yfit = coeffs[0]
                        + t * (coeffs[1]
                        + t * (coeffs[2]
                        + t * (coeffs[3]
                        + t * coeffs[4])));
      spectrum[i - 1] -= static_cast<float> (yfit);
    }
}

void plotsave_row (float* swide, int nw, int nh, int irow)
{
  auto& state = plotsave_state ();

  if (irow == -99)
    {
      state.rows.clear ();
      return;
    }

  if (nw <= 0 || nh <= 0)
    {
      return;
    }

  if (state.width != nw || state.height != nh
      || state.rows.size () != static_cast<size_t> (nw * nh))
    {
      state.width = nw;
      state.height = nh;
      state.rows.assign (static_cast<size_t> (nw * nh), 0.0f);
    }

  if (irow < 0)
    {
      if (!swide)
        {
          return;
        }
      for (int row = nh - 1; row >= 1; --row)
        {
          std::copy_n (state.rows.begin () + static_cast<ptrdiff_t> ((row - 1) * nw),
                       nw,
                       state.rows.begin () + static_cast<ptrdiff_t> (row * nw));
        }
      std::copy_n (swide, nw, state.rows.begin ());
      return;
    }

  if (!swide || irow >= nh)
    {
      return;
    }

  std::copy_n (state.rows.begin () + static_cast<ptrdiff_t> (irow * nw), nw, swide);
}

void clear_saved_waterfall ()
{
  auto& state = plotsave_state ();
  state.width = -1;
  state.height = -1;
  state.rows.clear ();
}

}
}
