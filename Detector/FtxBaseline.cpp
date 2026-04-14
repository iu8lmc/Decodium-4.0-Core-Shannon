#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{
double percentile_cutoff (std::vector<double> values, int percent)
{
  if (values.empty ())
    {
      return 0.0;
    }

  std::sort (values.begin (), values.end ());
  int index = static_cast<int> (std::lround (values.size () * 0.01 * percent));
  index = std::max (static_cast<int>(1), static_cast<int>(std::min (index, static_cast<int> (values.size ()))));
  return values[static_cast<size_t> (index - 1)];
}

bool fit_quartic (std::vector<double> const& x, std::vector<double> const& y, std::array<double, 5>& coeffs)
{
  if (x.size () != y.size () || x.size () < coeffs.size ())
    {
      coeffs.fill (0.0);
      return false;
    }

  std::array<double, 9> sumx {};
  std::array<double, 5> sumy {};
  for (size_t i = 0; i < x.size (); ++i)
    {
      double term = 1.0;
      for (double& sum : sumx)
        {
          sum += term;
          term *= x[i];
        }

      double yterm = y[i];
      for (double& sum : sumy)
        {
          sum += yterm;
          yterm *= x[i];
        }
    }

  std::array<std::array<double, 6>, 5> aug {};
  for (int row = 0; row < 5; ++row)
    {
      for (int col = 0; col < 5; ++col)
        {
          aug[row][col] = sumx[static_cast<size_t> (row + col)];
        }
      aug[row][5] = sumy[static_cast<size_t> (row)];
    }

  for (int pivot = 0; pivot < 5; ++pivot)
    {
      int best = pivot;
      for (int row = pivot + 1; row < 5; ++row)
        {
          if (std::fabs (aug[row][pivot]) > std::fabs (aug[best][pivot]))
            {
              best = row;
            }
        }

      if (std::fabs (aug[best][pivot]) < 1e-12)
        {
          coeffs.fill (0.0);
          return false;
        }

      if (best != pivot)
        {
          std::swap (aug[best], aug[pivot]);
        }

      double const scale = aug[pivot][pivot];
      for (int col = pivot; col < 6; ++col)
        {
          aug[pivot][col] /= scale;
        }

      for (int row = 0; row < 5; ++row)
        {
          if (row == pivot)
            {
              continue;
            }
          double const factor = aug[row][pivot];
          if (factor == 0.0)
            {
              continue;
            }
          for (int col = pivot; col < 6; ++col)
            {
              aug[row][col] -= factor * aug[pivot][col];
            }
        }
    }

  for (int i = 0; i < 5; ++i)
    {
      coeffs[static_cast<size_t> (i)] = aug[i][5];
    }
  return true;
}
}

extern "C" void ftx_baseline_fit_c (float const* spectrum, int size, int ia, int ib,
                                    float offset_db, int linear_output, float* baseline)
{
  if (!spectrum || !baseline || size <= 0)
    {
      return;
    }

  std::fill (baseline, baseline + size, 0.0f);

  ia = std::max (1, ia);
  ib = std::min (size, ib);
  if (ib < ia)
    {
      return;
    }

  constexpr int nseg = 10;
  constexpr int npct = 10;
  int const segment_len = std::max (1, (ib - ia + 1) / nseg);
  int const i0 = (ib - ia + 1) / 2;

  std::vector<double> x;
  std::vector<double> y;
  x.reserve (1000);
  y.reserve (1000);

  for (int seg = 0; seg < nseg; ++seg)
    {
      int const ja = ia + seg * segment_len;
      if (ja > ib)
        {
          break;
        }
      int const jb = std::min (ib, ja + segment_len - 1);

      std::vector<double> segment;
      segment.reserve (static_cast<size_t> (jb - ja + 1));
      for (int index = ja; index <= jb; ++index)
        {
          double const value = std::max (static_cast<double> (spectrum[index - 1]), 1e-30);
          segment.push_back (10.0 * std::log10 (value));
        }

      double const cutoff = percentile_cutoff (segment, npct);
      for (int index = ja; index <= jb; ++index)
        {
          double const value = std::max (static_cast<double> (spectrum[index - 1]), 1e-30);
          double const db = 10.0 * std::log10 (value);
          if (db <= cutoff && x.size () < 1000)
            {
              x.push_back (static_cast<double> (index - i0));
              y.push_back (db);
            }
        }
    }

  std::array<double, 5> coeffs {};
  if (!fit_quartic (x, y, coeffs))
    {
      return;
    }

  for (int index = ia; index <= ib; ++index)
    {
      double const t = static_cast<double> (index - i0);
      double const db = coeffs[0]
                      + t * (coeffs[1]
                      + t * (coeffs[2]
                      + t * (coeffs[3]
                      + t * coeffs[4])))
                      + static_cast<double> (offset_db);
      baseline[index - 1] = linear_output != 0
          ? static_cast<float> (std::pow (10.0, db / 10.0))
          : static_cast<float> (db);
    }
}
