#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

void fft_inplace (std::vector<Complex>& values, int direction)
{
  if (values.empty ())
    {
      return;
    }

  fftwf_plan plan = fftwf_plan_dft_1d (static_cast<int> (values.size ()),
                                       reinterpret_cast<fftwf_complex*> (values.data ()),
                                       reinterpret_cast<fftwf_complex*> (values.data ()),
                                       direction, FFTW_ESTIMATE);
  if (!plan)
    {
      return;
    }

  fftwf_execute (plan);
  fftwf_destroy_plan (plan);
}

std::vector<Complex> circular_shift_right (Complex const* input, int size, int shift)
{
  std::vector<Complex> shifted (static_cast<size_t> (size), Complex {0.0f, 0.0f});
  if (!input || size <= 0)
    {
      return shifted;
    }

  int const wrapped_shift = ((shift % size) + size) % size;
  for (int i = 0; i < size; ++i)
    {
      int const target = (i + wrapped_shift) % size;
      shifted[static_cast<size_t> (target)] = input[i];
    }
  return shifted;
}

}

extern "C"
{
  float gran_ ();
}

extern "C" void watterson_ (fftwf_complex* c_in, int* npts_in, int* nsig_in, float* fs_in,
                             float* delay_in, float* fspread_in)
{
  if (!c_in || !npts_in || !nsig_in || !fs_in || !delay_in || !fspread_in)
    {
      return;
    }

  int const npts = *npts_in;
  int const nsig = *nsig_in;
  float const fs = *fs_in;
  float const delay = *delay_in;
  float const fspread = *fspread_in;
  if (npts <= 0 || nsig <= 0 || !std::isfinite (fs) || fs == 0.0f)
    {
      return;
    }

  auto* c = reinterpret_cast<Complex*> (c_in);
  std::vector<Complex> cs1 (static_cast<size_t> (npts), Complex {0.0f, 0.0f});
  std::vector<Complex> cs2 (static_cast<size_t> (npts), Complex {0.0f, 0.0f});

  int nonzero = 0;
  float const df = fs / static_cast<float> (npts);
  if (fspread > 0.0f)
    {
      for (int i = 0; i < npts; ++i)
        {
          cs1[static_cast<size_t> (i)] = 0.707f * Complex {gran_ (), gran_ ()};
          cs2[static_cast<size_t> (i)] = 0.707f * Complex {gran_ (), gran_ ()};
        }

      fft_inplace (cs1, FFTW_FORWARD);
      fft_inplace (cs2, FFTW_FORWARD);

      for (int i = 0; i < npts; ++i)
        {
          float f = static_cast<float> (i) * df;
          if (i > npts / 2)
            {
              f = static_cast<float> (i - npts) * df;
            }

          float const x = (f / fspread) * (f / fspread);
          float const a = x <= 50.0f ? std::exp (-x) : 0.0f;
          cs1[static_cast<size_t> (i)] *= a;
          cs2[static_cast<size_t> (i)] *= a;

          if (std::abs (f) < 10.0f && std::norm (cs1[static_cast<size_t> (i)]) > 0.0f)
            {
              ++nonzero;
            }
        }

      fft_inplace (cs1, FFTW_BACKWARD);
      fft_inplace (cs2, FFTW_BACKWARD);

      float const scale = 1.0f / static_cast<float> (npts);
      for (int i = 0; i < npts; ++i)
        {
          cs1[static_cast<size_t> (i)] *= scale;
          cs2[static_cast<size_t> (i)] *= scale;
        }
    }

  int const nshift = static_cast<int> (std::lround (0.001f * delay * fs));
  std::vector<Complex> c2 = delay > 0.0f
                              ? circular_shift_right (c, npts, nshift)
                              : std::vector<Complex> (static_cast<size_t> (npts), Complex {0.0f, 0.0f});

  float sq = 0.0f;
  for (int i = 0; i < npts; ++i)
    {
      if (nonzero > 1)
        {
          c[i] = 0.5f * (cs1[static_cast<size_t> (i)] * c[i] + cs2[static_cast<size_t> (i)] * c2[static_cast<size_t> (i)]);
        }
      else
        {
          c[i] = 0.5f * (c[i] + c2[static_cast<size_t> (i)]);
        }
      sq += std::norm (c[i]);
    }

  float const rms = std::sqrt (sq / static_cast<float> (nsig));
  if (rms <= 0.0f || !std::isfinite (rms))
    {
      return;
    }

  for (int i = 0; i < npts; ++i)
    {
      c[i] /= rms;
    }
}
