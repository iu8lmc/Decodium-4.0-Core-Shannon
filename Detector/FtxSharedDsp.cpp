// -*- Mode: C++ -*-
#include "Detector/FftCompat.hpp"

#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
  static_assert (sizeof (std::complex<float>) == sizeof (fftwf_complex),
                 "std::complex<float> must match fftwf_complex layout");

  struct FftPlanKey
  {
    int nfft {0};
    int isign {0};
    int iform {0};
    std::uintptr_t address {0};

    bool operator== (FftPlanKey const& other) const noexcept
    {
      return nfft == other.nfft
          && isign == other.isign
          && iform == other.iform
          && address == other.address;
    }
  };

  struct FftPlanKeyHash
  {
    std::size_t operator() (FftPlanKey const& key) const noexcept
    {
      std::size_t seed = static_cast<std::size_t> (key.address);
      seed ^= static_cast<std::size_t> (key.nfft) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
      seed ^= static_cast<std::size_t> (key.isign) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
      seed ^= static_cast<std::size_t> (key.iform) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  class FftPlanRegistry
  {
  public:
    fftwf_plan find_or_create (std::complex<float>* data, int nfft, int isign, int iform)
    {
      FftPlanKey key;
      key.nfft = nfft;
      key.isign = isign;
      key.iform = iform;
      key.address = reinterpret_cast<std::uintptr_t> (data);
      std::lock_guard<std::mutex> lock {mutex_};
      auto const found = plans_.find (key);
      if (found != plans_.end ())
        {
          return found->second;
        }

      fftwf_plan const plan = create_plan (data, nfft, isign, iform);
      if (plan)
        {
          plans_.emplace (key, plan);
        }
      return plan;
    }

    void clear ()
    {
      std::lock_guard<std::mutex> lock {mutex_};
      for (auto const& entry : plans_)
        {
          if (entry.second)
            {
              fftwf_destroy_plan (entry.second);
            }
        }
      plans_.clear ();
    }

  private:
    static fftwf_plan create_plan (std::complex<float>* data, int nfft, int isign, int iform)
    {
      if (!data || nfft <= 0)
        {
          return nullptr;
        }

      auto* complex_data = reinterpret_cast<fftwf_complex*> (data);
      auto* real_data = reinterpret_cast<float*> (data);
      constexpr unsigned flags = FFTW_ESTIMATE;

      if (isign == -1 && iform == 1)
        {
          return fftwf_plan_dft_1d (nfft, complex_data, complex_data, FFTW_FORWARD, flags);
        }
      if (isign == 1 && iform == 1)
        {
          return fftwf_plan_dft_1d (nfft, complex_data, complex_data, FFTW_BACKWARD, flags);
        }
      if (isign == -1 && iform == 0)
        {
          return fftwf_plan_dft_r2c_1d (nfft, real_data, complex_data, flags);
        }
      if (isign == 1 && iform == -1)
        {
          return fftwf_plan_dft_c2r_1d (nfft, complex_data, real_data, flags);
        }

      std::fprintf (stderr, "Unsupported legacy FFT request: nfft=%d isign=%d iform=%d\n",
                    nfft, isign, iform);
      return nullptr;
    }

    std::mutex mutex_;
    std::unordered_map<FftPlanKey, fftwf_plan, FftPlanKeyHash> plans_;
  };

  FftPlanRegistry& fft_plan_registry ()
  {
    static auto* registry = new FftPlanRegistry;
    return *registry;
  }

  float percentile_value (float const* values, int count, int pct)
  {
    if (!values || count <= 0 || pct < 0 || pct > 100)
      {
        return 1.0f;
      }

    std::vector<float> sorted (values, values + count);
    std::sort (sorted.begin (), sorted.end ());
    int index = static_cast<int> (std::lround (static_cast<double> (count) * 0.01 * pct));
    index = std::max (1, std::min (count, index));
    return sorted[static_cast<std::size_t> (index - 1)];
  }

  bool solve_linear_system (std::vector<double> matrix, std::vector<double> rhs,
                            std::vector<double>* solution)
  {
    int const n = static_cast<int> (rhs.size ());
    if (!solution || static_cast<int> (matrix.size ()) != n * n)
      {
        return false;
      }

    constexpr double epsilon = 1.0e-18;
    for (int column = 0; column < n; ++column)
      {
        int pivot = column;
        double pivot_abs = std::fabs (matrix[static_cast<std::size_t> (column * n + column)]);
        for (int row = column + 1; row < n; ++row)
          {
            double const value = std::fabs (matrix[static_cast<std::size_t> (row * n + column)]);
            if (value > pivot_abs)
              {
                pivot_abs = value;
                pivot = row;
              }
          }
        if (pivot_abs <= epsilon)
          {
            return false;
          }

        if (pivot != column)
          {
            for (int k = column; k < n; ++k)
              {
                std::swap (matrix[static_cast<std::size_t> (column * n + k)],
                           matrix[static_cast<std::size_t> (pivot * n + k)]);
              }
            std::swap (rhs[static_cast<std::size_t> (column)],
                       rhs[static_cast<std::size_t> (pivot)]);
          }

        double const diagonal = matrix[static_cast<std::size_t> (column * n + column)];
        for (int row = column + 1; row < n; ++row)
          {
            double const factor = matrix[static_cast<std::size_t> (row * n + column)] / diagonal;
            if (factor == 0.0)
              {
                continue;
              }
            matrix[static_cast<std::size_t> (row * n + column)] = 0.0;
            for (int k = column + 1; k < n; ++k)
              {
                matrix[static_cast<std::size_t> (row * n + k)] -=
                    factor * matrix[static_cast<std::size_t> (column * n + k)];
              }
            rhs[static_cast<std::size_t> (row)] -= factor * rhs[static_cast<std::size_t> (column)];
          }
      }

    solution->assign (static_cast<std::size_t> (n), 0.0);
    for (int row = n - 1; row >= 0; --row)
      {
        double sum = rhs[static_cast<std::size_t> (row)];
        for (int column = row + 1; column < n; ++column)
          {
            sum -= matrix[static_cast<std::size_t> (row * n + column)]
                * (*solution)[static_cast<std::size_t> (column)];
          }
        double const diagonal = matrix[static_cast<std::size_t> (row * n + row)];
        if (std::fabs (diagonal) <= epsilon)
          {
            return false;
          }
        (*solution)[static_cast<std::size_t> (row)] = sum / diagonal;
      }
    return true;
  }
}

namespace decodium
{
namespace fft_compat
{

void legacy_fft_execute (std::complex<float>* buffer, int nfft, int isign, int iform)
{
  if (!buffer || nfft == 0)
    {
      return;
    }

  fftwf_plan const plan = fft_plan_registry ().find_or_create (buffer, nfft, isign, iform);
  if (plan)
    {
      fftwf_execute (plan);
    }
}

void legacy_fft_cleanup ()
{
  fft_plan_registry ().clear ();
}

}
}

extern "C"
{
  void wsjt_fft_compat_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform, int)
  {
    (void) ndim;
    if (!nfft)
      {
        return;
      }
    if (*nfft < 0)
      {
        decodium::fft_compat::legacy_fft_cleanup ();
        return;
      }
    if (!a || !isign || !iform || *nfft == 0)
      {
        return;
      }

    decodium::fft_compat::legacy_fft_execute (a, *nfft, *isign, *iform);
  }

  void blanker_ (short iwave[], int* nz, int* ndropmax, int* npct, std::complex<float> c_bigfft[])
  {
    if (!iwave || !nz || !ndropmax || !npct || !c_bigfft || *nz <= 0 || *ndropmax <= 0)
      {
        return;
      }

    int const sample_count = *nz;
    std::vector<int> hist (32769, 0);
    float const fblank = 0.01f * *npct;
    int const target = static_cast<int> (std::lround (sample_count * fblank / *ndropmax));

    for (int i = 0; i < sample_count; ++i)
      {
        if (iwave[i] == std::numeric_limits<short>::min ())
          {
            iwave[i] = static_cast<short> (-32767);
          }
        int const magnitude = std::abs (static_cast<int> (iwave[i]));
        hist[static_cast<std::size_t> (magnitude)] += 1;
      }

    int cumulative = 0;
    int threshold = 0;
    for (int i = 32768; i >= 0; --i)
      {
        cumulative += hist[static_cast<std::size_t> (i)];
        if (cumulative >= target)
          {
            threshold = i;
            break;
          }
      }

    std::fill_n (c_bigfft, static_cast<std::size_t> (sample_count / 2 + 1), std::complex<float> {});

    int ndrop = 0;
    float xx = 0.0f;
    for (int i = 0; i < sample_count; ++i)
      {
        int sample = iwave[i];
        if (ndrop > 0)
          {
            sample = 0;
            --ndrop;
          }

        if (std::abs (sample) > threshold)
          {
            sample = 0;
            ndrop = *ndropmax;
          }

        if ((i & 1) == 0)
          {
            xx = static_cast<float> (sample);
          }
        else
          {
            int const j = i / 2;
            c_bigfft[static_cast<std::size_t> (j)] =
                std::complex<float> {xx, static_cast<float> (sample)};
          }
      }
  }

  void pctile_ (float x[], int* npts, int* npct, float* xpct)
  {
    if (!xpct || !npts || !npct)
      {
        return;
      }
    *xpct = percentile_value (x, *npts, *npct);
  }

  void polyfit_ (double x[], double y[], double sigmay[], int* npts, int* nterms, int* mode,
                 double a[], double* chisqr)
  {
    if (!x || !y || !sigmay || !npts || !nterms || !mode || !a || !chisqr
        || *npts <= 0 || *nterms <= 0)
      {
        return;
      }

    int const point_count = *npts;
    int const term_count = *nterms;
    int const nmax = 2 * term_count - 1;
    std::vector<double> sumx (static_cast<std::size_t> (nmax), 0.0);
    std::vector<double> sumy (static_cast<std::size_t> (term_count), 0.0);
    double chisq = 0.0;

    for (int i = 0; i < point_count; ++i)
      {
        double const xi = x[i];
        double const yi = y[i];
        double weight = 1.0;
        if (*mode < 0)
          {
            weight = 1.0 / std::fabs (yi);
          }
        else if (*mode > 0)
          {
            weight = 1.0 / (sigmay[i] * sigmay[i]);
          }

        double xterm = weight;
        for (int n = 0; n < nmax; ++n)
          {
            sumx[static_cast<std::size_t> (n)] += xterm;
            xterm *= xi;
          }

        double yterm = weight * yi;
        for (int n = 0; n < term_count; ++n)
          {
            sumy[static_cast<std::size_t> (n)] += yterm;
            yterm *= xi;
          }

        chisq += weight * yi * yi;
      }

    std::vector<double> matrix (static_cast<std::size_t> (term_count * term_count), 0.0);
    for (int row = 0; row < term_count; ++row)
      {
        for (int column = 0; column < term_count; ++column)
          {
            matrix[static_cast<std::size_t> (row * term_count + column)] =
                sumx[static_cast<std::size_t> (row + column)];
          }
        a[row] = 0.0;
      }

    std::vector<double> coeffs;
    if (!solve_linear_system (matrix, sumy, &coeffs))
      {
        *chisqr = 0.0;
        return;
      }

    for (int i = 0; i < term_count; ++i)
      {
        a[i] = coeffs[static_cast<std::size_t> (i)];
      }

    for (int row = 0; row < term_count; ++row)
      {
        chisq -= 2.0 * a[row] * sumy[static_cast<std::size_t> (row)];
        for (int column = 0; column < term_count; ++column)
          {
            chisq += a[row] * a[column] * sumx[static_cast<std::size_t> (row + column)];
          }
      }

    int const free = point_count - term_count;
    *chisqr = free > 0 ? chisq / free : 0.0;
  }
}
