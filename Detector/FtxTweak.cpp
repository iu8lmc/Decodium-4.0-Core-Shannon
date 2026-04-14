#include <cmath>
#include <complex>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

constexpr float kTwoPi = 6.28318530717958647692f;

void apply_polynomial_tweak (Complex const* in, Complex* out, int npts, float fsample, float const* a)
{
  if (!in || !out || !a || npts <= 0 || fsample == 0.0f)
    {
      return;
    }

  Complex w (1.0f, 0.0f);
  float const x0 = 0.5f * static_cast<float> (npts + 1);
  float const s = 2.0f / static_cast<float> (npts);
  float const phase_scale = kTwoPi / fsample;

  for (int idx = 0; idx < npts; ++idx)
    {
      float const x = s * (static_cast<float> (idx + 1) - x0);
      float const x2 = x * x;
      float const p2 = 1.5f * x2 - 0.5f;
      float const p3 = 2.5f * x2 * x - 1.5f * x;
      float const p4 = 4.375f * x2 * x2 - 3.75f * x2 + 0.375f;
      float const dphi = (a[0] + x * a[1] + p2 * a[2] + p3 * a[3] + p4 * a[4]) * phase_scale;
      Complex const wstep (std::cos (dphi), std::sin (dphi));
      w *= wstep;
      out[idx] = w * in[idx];
    }
}

void apply_constant_tweak_segment (Complex const* in, Complex* out, int nbot, int npts, float fsample, float const* a)
{
  if (!in || !out || !a || fsample == 0.0f || npts < 0)
    {
      return;
    }

  int const offset = -nbot;
  if (offset < 0)
    {
      return;
    }

  Complex w (1.0f, 0.0f);
  float const dphi = a[0] * (kTwoPi / fsample);
  Complex const wstep (std::cos (dphi), std::sin (dphi));

  for (int logical = 0; logical <= npts; ++logical)
    {
      int const physical = offset + logical;
      w *= wstep;
      out[physical] = w * in[physical];
    }
}

}

extern "C" void ftx_twkfreq1_c (fftwf_complex const* ca, int const* npts, float const* fsample,
                                float const* a, fftwf_complex* cb)
{
  apply_polynomial_tweak (reinterpret_cast<Complex const*> (ca),
                          reinterpret_cast<Complex*> (cb),
                          npts ? *npts : 0,
                          fsample ? *fsample : 0.0f,
                          a);
}

extern "C" void twkfreq1_ (fftwf_complex* ca, int* npts, float* fsample,
                            float* a, fftwf_complex* cb)
{
  ftx_twkfreq1_c (ca, npts, fsample, a, cb);
}

extern "C" void ftx_twkfreq1var_c (fftwf_complex const* ca, int const* nbot, int const* npts,
                                   float const* fsample, float const* a, fftwf_complex* cb)
{
  apply_constant_tweak_segment (reinterpret_cast<Complex const*> (ca),
                                reinterpret_cast<Complex*> (cb),
                                nbot ? *nbot : 0,
                                npts ? *npts : -1,
                                fsample ? *fsample : 0.0f,
                                a);
}
