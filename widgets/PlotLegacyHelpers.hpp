#ifndef PLOT_LEGACY_HELPERS_HPP
#define PLOT_LEGACY_HELPERS_HPP

namespace decodium
{
namespace plot
{

inline void smooth121_inplace (float* x, int npts)
{
  if (!x || npts <= 2)
    {
      return;
    }

  float x0 = x[0];
  for (int i = 1; i < npts - 1; ++i)
    {
      float const x1 = x[i];
      x[i] = 0.5f * x[i] + 0.25f * (x0 + x[i + 1]);
      x0 = x1;
    }
}

void flat4_inplace (float* spectrum, int npts0, int nflatten);
void plotsave_row (float* swide, int nw, int nh, int irow);
void clear_saved_waterfall ();

}
}

#endif // PLOT_LEGACY_HELPERS_HPP
