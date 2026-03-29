#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

constexpr int kNtone = 79;
constexpr int kNsps = 1920;
constexpr int kNWave = 151680;
constexpr int kNTab = 65536;
constexpr float kBt = 2.0f;
constexpr float kTwoPi = 6.28318530717958647692f;

float gfsk_pulse (float bt, float t)
{
  float const c = 0.5f * kTwoPi * std::sqrt (2.0f / std::log (2.0f));
  return 0.5f * (std::erff (c * bt * (t + 0.5f)) - std::erff (c * bt * (t - 0.5f)));
}

float wrap_phase (float phase)
{
  phase = std::fmod (phase, kTwoPi);
  if (phase < 0.0f)
    {
      phase += kTwoPi;
    }
  return phase;
}

std::array<Complex, kNTab> const& ctab ()
{
  static std::array<Complex, kNTab> table = [] {
    std::array<Complex, kNTab> value {};
    for (int i = 0; i < kNTab; ++i)
      {
        float const phase = static_cast<float> (i) * kTwoPi / static_cast<float> (kNTab);
        value[static_cast<size_t> (i)] = Complex (std::cos (phase), std::sin (phase));
      }
    return value;
  }();
  return table;
}

void generate_ft8_complex_reference (int const* itone, Complex* out)
{
  std::array<float, 3 * kNsps> pulse {};
  std::vector<float> dphi (static_cast<size_t> ((kNtone + 2) * kNsps), 0.0f);
  for (int i = 0; i < 3 * kNsps; ++i)
    {
      float const tt = (static_cast<float> (i + 1) - 1.5f * static_cast<float> (kNsps))
        / static_cast<float> (kNsps);
      pulse[static_cast<size_t> (i)] = gfsk_pulse (kBt, tt);
    }

  float const dphi_peak = kTwoPi / static_cast<float> (kNsps);
  for (int j = 0; j < kNtone; ++j)
    {
      int const ib = j * kNsps;
      for (int i = 0; i < 3 * kNsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
            * static_cast<float> (itone[j]);
        }
    }

  for (int i = 0; i < 2 * kNsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * static_cast<float> (itone[0])
        * pulse[static_cast<size_t> (kNsps + i)];
      dphi[static_cast<size_t> (kNtone * kNsps + i)] += dphi_peak
        * static_cast<float> (itone[kNtone - 1]) * pulse[static_cast<size_t> (i)];
    }

  auto const& table = ctab ();
  float phi = 0.0f;
  for (int j = kNsps, k = 0; j < kNsps + kNWave; ++j, ++k)
    {
      int const index = std::max (0, std::min (kNTab - 1,
                                              static_cast<int> (phi * static_cast<float> (kNTab) / kTwoPi)));
      out[k] = table[static_cast<size_t> (index)];
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)]);
    }

  int const nramp = static_cast<int> (std::lround (static_cast<float> (kNsps) / 8.0f));
  for (int i = 0; i < nramp; ++i)
    {
      float const env = 0.5f * (1.0f - std::cos (kTwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (nramp))));
      out[i] *= env;
    }
  int const tail_start = kNtone * kNsps - nramp;
  for (int i = 0; i < nramp; ++i)
    {
      float const env = 0.5f * (1.0f + std::cos (kTwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (nramp))));
      out[tail_start + i] *= env;
    }
}

template <int Rows>
void fill_sparse_sync (int const* itone, fftwf_complex* out)
{
  if (!itone || !out)
    {
      return;
    }

  std::array<Complex, kNWave> wave {};
  generate_ft8_complex_reference (itone, wave.data ());

  int m = 13440; // Fortran m=13441 with 1-based indexing
  for (int row = 0; row < Rows; ++row)
    {
      for (int col = 0; col < 32; ++col)
        {
          Complex const value = wave[static_cast<size_t> (m)];
          out[row + Rows * col][0] = value.real ();
          out[row + Rows * col][1] = value.imag ();
          m += 60;
        }
    }
}

}

extern "C" void ftx_build_ft8var_synce_c (int const* itone, fftwf_complex* csynce)
{
  fill_sparse_sync<19> (itone, csynce);
}

extern "C" void ftx_build_ft8var_csyncsd_c (int const* itone, fftwf_complex* csyncsd)
{
  fill_sparse_sync<19> (itone, csyncsd);
}

extern "C" void ftx_build_ft8var_csyncsdcq_c (int const* itone, fftwf_complex* csyncsdcq)
{
  if (!itone || !csyncsdcq)
    {
      return;
    }

  std::array<Complex, kNWave> wave {};
  generate_ft8_complex_reference (itone, wave.data ());

  int m = 13440; // Fortran m=13441 with 1-based indexing
  for (int row = 0; row < 58; ++row)
    {
      if (row == 29)
        {
          m += 13440;
        }
      for (int col = 0; col < 32; ++col)
        {
          Complex const value = wave[static_cast<size_t> (m)];
          csyncsdcq[row + 58 * col][0] = value.real ();
          csyncsdcq[row + 58 * col][1] = value.imag ();
          m += 60;
        }
    }
}
