#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

constexpr int kFt8NMax = 15 * 12000;
constexpr int kFt8NFrame = 1920 * 79;
constexpr int kFt8NFilt = 6000;
constexpr float kFt8SampleRate = 12000.0f;
constexpr int kFt2NMax = 45000;
constexpr int kFt2Ntone = 103;
constexpr int kFt2Nsps = 288;
constexpr int kFt2NFrame = (kFt2Ntone + 2) * kFt2Nsps;
constexpr int kFt2NFilt = 700;
constexpr float kFt2SampleRate = 12000.0f;
constexpr int kFt4NMax = 21 * 3456;
constexpr int kFt4Ntone = 103;
constexpr int kFt4Nsps = 576;
constexpr int kFt4NFrame = (kFt4Ntone + 2) * kFt4Nsps;
constexpr int kFt4NFilt = 1400;
constexpr float kFt4SampleRate = 12000.0f;
constexpr float kBt = 2.0f;
constexpr int kNtone = 79;
constexpr int kNsps = 1920;
constexpr int kNTab = 65536;
constexpr float kPi = 3.14159265358979323846f;
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

struct Ft8ComplexFft
{
  Ft8ComplexFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kFt8NMax))),
      forward (nullptr),
      inverse (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kFt8NMax, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
        inverse = fftwf_plan_dft_1d (kFt8NMax, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
      }
  }

  ~Ft8ComplexFft ()
  {
    if (forward)
      {
        fftwf_destroy_plan (forward);
      }
    if (inverse)
      {
        fftwf_destroy_plan (inverse);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward && inverse;
  }

  Complex* values ()
  {
    return reinterpret_cast<Complex*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
  fftwf_plan inverse;
};

struct Ft8RealFft
{
  Ft8RealFft ()
    : in (static_cast<float*> (fftwf_malloc (sizeof (float) * kFt8NMax))),
      out (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * (kFt8NMax / 2 + 1)))),
      plan (nullptr)
  {
    if (in && out)
      {
        plan = fftwf_plan_dft_r2c_1d (kFt8NMax, in, out, FFTW_ESTIMATE);
      }
  }

  ~Ft8RealFft ()
  {
    if (plan)
      {
        fftwf_destroy_plan (plan);
      }
    if (out)
      {
        fftwf_free (out);
      }
    if (in)
      {
        fftwf_free (in);
      }
  }

  bool valid () const
  {
    return in && out && plan;
  }

  float* in;
  fftwf_complex* out;
  fftwf_plan plan;
};

struct Ft2ComplexFft
{
  Ft2ComplexFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kFt2NMax))),
      forward (nullptr),
      inverse (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kFt2NMax, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
        inverse = fftwf_plan_dft_1d (kFt2NMax, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
      }
  }

  ~Ft2ComplexFft ()
  {
    if (forward)
      {
        fftwf_destroy_plan (forward);
      }
    if (inverse)
      {
        fftwf_destroy_plan (inverse);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward && inverse;
  }

  Complex* values ()
  {
    return reinterpret_cast<Complex*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
  fftwf_plan inverse;
};

struct Ft4ComplexFft
{
  Ft4ComplexFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kFt4NMax))),
      forward (nullptr),
      inverse (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kFt4NMax, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
        inverse = fftwf_plan_dft_1d (kFt4NMax, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
      }
  }

  ~Ft4ComplexFft ()
  {
    if (forward)
      {
        fftwf_destroy_plan (forward);
      }
    if (inverse)
      {
        fftwf_destroy_plan (inverse);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward && inverse;
  }

  Complex* values ()
  {
    return reinterpret_cast<Complex*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
  fftwf_plan inverse;
};

Ft8ComplexFft& complex_fft ()
{
  thread_local Ft8ComplexFft fft;
  return fft;
}

Ft8RealFft& real_fft ()
{
  thread_local Ft8RealFft fft;
  return fft;
}

Ft2ComplexFft& ft2_complex_fft ()
{
  thread_local Ft2ComplexFft fft;
  return fft;
}

Ft4ComplexFft& ft4_complex_fft ()
{
  thread_local Ft4ComplexFft fft;
  return fft;
}

struct Ft8SubtractWorkspace
{
  std::vector<Complex> cref = std::vector<Complex> (static_cast<size_t> (kFt8NFrame));
  std::vector<float> dd = std::vector<float> (static_cast<size_t> (kFt8NMax));
};

Ft8SubtractWorkspace& workspace ()
{
  thread_local Ft8SubtractWorkspace state;
  return state;
}

struct Ft2SubtractWorkspace
{
  std::vector<Complex> cref = std::vector<Complex> (static_cast<size_t> (kFt2NFrame));
  std::vector<float> dd = std::vector<float> (static_cast<size_t> (kFt2NMax));
};

Ft2SubtractWorkspace& ft2_workspace ()
{
  thread_local Ft2SubtractWorkspace state;
  return state;
}

struct Ft4SubtractWorkspace
{
  std::vector<Complex> cref = std::vector<Complex> (static_cast<size_t> (kFt4NFrame));
  std::vector<float> dd = std::vector<float> (static_cast<size_t> (kFt4NMax));
};

Ft4SubtractWorkspace& ft4_workspace ()
{
  thread_local Ft4SubtractWorkspace state;
  return state;
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

struct Ft8SubtractFilter
{
  std::array<Complex, kFt8NMax> spectrum {};
  std::array<float, kFt8NFilt / 2 + 1> endcorrection {};
  bool ready {false};
};

Ft8SubtractFilter& subtract_filter ()
{
  thread_local Ft8SubtractFilter filter;
  if (!filter.ready)
    {
      auto& fft = complex_fft ();
      if (!fft.valid ())
        {
          return filter;
        }

      std::array<float, kFt8NFilt + 1> window {};
      float sumw = 0.0f;
      for (int j = -kFt8NFilt / 2; j <= kFt8NFilt / 2; ++j)
        {
          float const value = std::pow (std::cos (kPi * static_cast<float> (j)
                                                  / static_cast<float> (kFt8NFilt)), 2.0f);
          window[static_cast<size_t> (j + kFt8NFilt / 2)] = value;
          sumw += value;
        }

      Complex* cw = fft.values ();
      std::fill (cw, cw + kFt8NMax, Complex {});
      for (int i = 0; i <= kFt8NFilt; ++i)
        {
          cw[i] = Complex (window[static_cast<size_t> (i)] / sumw, 0.0f);
        }
      std::rotate (cw, cw + (kFt8NFilt / 2 + 1), cw + kFt8NMax);
      fftwf_execute (fft.forward);

      float const fac = 1.0f / static_cast<float> (kFt8NMax);
      for (int i = 0; i < kFt8NMax; ++i)
        {
          filter.spectrum[static_cast<size_t> (i)] = cw[i] * fac;
        }

      std::array<float, kFt8NFilt + 2> suffix {};
      suffix[kFt8NFilt + 1] = 0.0f;
      for (int i = kFt8NFilt; i >= 0; --i)
        {
          suffix[static_cast<size_t> (i)] = suffix[static_cast<size_t> (i + 1)] + window[static_cast<size_t> (i)];
        }
      for (int j = 0; j <= kFt8NFilt / 2; ++j)
        {
          float const tail = suffix[static_cast<size_t> (kFt8NFilt / 2 + j)];
          filter.endcorrection[static_cast<size_t> (j)] = 1.0f / (1.0f - tail / sumw);
        }

      filter.ready = true;
    }
  return filter;
}

struct Ft2SubtractFilter
{
  std::array<Complex, kFt2NMax> spectrum {};
  bool ready {false};
};

Ft2SubtractFilter& ft2_subtract_filter ()
{
  thread_local Ft2SubtractFilter filter;
  if (!filter.ready)
    {
      auto& fft = ft2_complex_fft ();
      if (!fft.valid ())
        {
          return filter;
        }

      std::array<float, kFt2NFilt + 1> window {};
      float sumw = 0.0f;
      for (int j = -kFt2NFilt / 2; j <= kFt2NFilt / 2; ++j)
        {
          float const value = std::pow (std::cos (kPi * static_cast<float> (j)
                                                  / static_cast<float> (kFt2NFilt)), 2.0f);
          window[static_cast<size_t> (j + kFt2NFilt / 2)] = value;
          sumw += value;
        }

      Complex* cw = fft.values ();
      std::fill (cw, cw + kFt2NMax, Complex {});
      for (int i = 0; i <= kFt2NFilt; ++i)
        {
          cw[i] = Complex (window[static_cast<size_t> (i)] / sumw, 0.0f);
        }
      std::rotate (cw, cw + (kFt2NFilt / 2 + 1), cw + kFt2NMax);
      fftwf_execute (fft.forward);

      float const fac = 1.0f / static_cast<float> (kFt2NMax);
      for (int i = 0; i < kFt2NMax; ++i)
        {
          filter.spectrum[static_cast<size_t> (i)] = cw[i] * fac;
        }

      filter.ready = true;
    }
  return filter;
}

struct Ft4SubtractFilter
{
  std::array<Complex, kFt4NMax> spectrum {};
  bool ready {false};
};

Ft4SubtractFilter& ft4_subtract_filter ()
{
  thread_local Ft4SubtractFilter filter;
  if (!filter.ready)
    {
      auto& fft = ft4_complex_fft ();
      if (!fft.valid ())
        {
          return filter;
        }

      std::array<float, kFt4NFilt + 1> window {};
      float sumw = 0.0f;
      for (int j = -kFt4NFilt / 2; j <= kFt4NFilt / 2; ++j)
        {
          float const value = std::pow (std::cos (kPi * static_cast<float> (j)
                                                  / static_cast<float> (kFt4NFilt)), 2.0f);
          window[static_cast<size_t> (j + kFt4NFilt / 2)] = value;
          sumw += value;
        }

      Complex* cw = fft.values ();
      std::fill (cw, cw + kFt4NMax, Complex {});
      for (int i = 0; i <= kFt4NFilt; ++i)
        {
          cw[i] = Complex (window[static_cast<size_t> (i)] / sumw, 0.0f);
        }
      std::rotate (cw, cw + (kFt4NFilt / 2 + 1), cw + kFt4NMax);
      fftwf_execute (fft.forward);

      float const fac = 1.0f / static_cast<float> (kFt4NMax);
      for (int i = 0; i < kFt4NMax; ++i)
        {
          filter.spectrum[static_cast<size_t> (i)] = cw[i] * fac;
        }

      filter.ready = true;
    }
  return filter;
}

void generate_ft8_reference (int const* itone, float f0, Complex* out)
{
  if (!itone || !out || !std::isfinite (f0))
    {
      return;
    }

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
      int const tone = std::max (0, std::min (7, itone[j]));
      for (int i = 0; i < 3 * kNsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
            * static_cast<float> (tone);
        }
    }

  int const first_tone = std::max (0, std::min (7, itone[0]));
  int const last_tone = std::max (0, std::min (7, itone[kNtone - 1]));
  for (int i = 0; i < 2 * kNsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * static_cast<float> (first_tone)
        * pulse[static_cast<size_t> (kNsps + i)];
      dphi[static_cast<size_t> (kNtone * kNsps + i)] += dphi_peak
        * static_cast<float> (last_tone) * pulse[static_cast<size_t> (i)];
    }

  float const carrier_step = kTwoPi * f0 / kFt8SampleRate;
  auto const& table = ctab ();
  float phi = 0.0f;
  for (int j = kNsps, k = 0; j < kNsps + kFt8NFrame; ++j, ++k)
    {
      int const index = std::max (0, std::min (kNTab - 1,
                                              static_cast<int> (phi * static_cast<float> (kNTab) / kTwoPi)));
      out[k] = table[static_cast<size_t> (index)];
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)] + carrier_step);
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

void generate_ft2_reference (int const* itone, float f0, Complex* out)
{
  if (!itone || !out || !std::isfinite (f0))
    {
      return;
    }

  static std::array<float, 3 * kFt2Nsps> const pulse = [] {
    std::array<float, 3 * kFt2Nsps> value {};
    for (int i = 0; i < 3 * kFt2Nsps; ++i)
      {
        float const tt = (static_cast<float> (i + 1) - 1.5f * static_cast<float> (kFt2Nsps))
          / static_cast<float> (kFt2Nsps);
        value[static_cast<size_t> (i)] = gfsk_pulse (1.0f, tt);
      }
    return value;
  }();

  std::array<float, kFt2NFrame> dphi {};
  float const dphi_peak = kTwoPi / static_cast<float> (kFt2Nsps);
  for (int j = 0; j < kFt2Ntone; ++j)
    {
      int const ib = j * kFt2Nsps;
      for (int i = 0; i < 3 * kFt2Nsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
            * static_cast<float> (itone[j]);
        }
    }

  float const carrier_step = kTwoPi * f0 / kFt2SampleRate;
  float phi = 0.0f;
  for (int j = 0; j < kFt2NFrame; ++j)
    {
      out[j] = Complex (std::cos (phi), std::sin (phi));
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)] + carrier_step);
    }

  for (int i = 0; i < kFt2Nsps; ++i)
    {
      float const env = 0.5f * (1.0f - std::cos (kTwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (kFt2Nsps))));
      out[i] *= env;
    }

  int const tail_start = (kFt2Ntone + 1) * kFt2Nsps;
  for (int i = 0; i < kFt2Nsps; ++i)
    {
      float const env = 0.5f * (1.0f + std::cos (kTwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (kFt2Nsps))));
      out[tail_start + i] *= env;
    }
}

void generate_ft4_reference (int const* itone, float f0, Complex* out)
{
  if (!itone || !out || !std::isfinite (f0))
    {
      return;
    }

  static std::array<float, 3 * kFt4Nsps> const pulse = [] {
    std::array<float, 3 * kFt4Nsps> value {};
    for (int i = 0; i < 3 * kFt4Nsps; ++i)
      {
        float const tt = (static_cast<float> (i + 1) - 1.5f * static_cast<float> (kFt4Nsps))
          / static_cast<float> (kFt4Nsps);
        value[static_cast<size_t> (i)] = gfsk_pulse (1.0f, tt);
      }
    return value;
  }();

  std::array<float, kFt4NFrame> dphi {};
  float const dphi_peak = kTwoPi / static_cast<float> (kFt4Nsps);
  for (int j = 0; j < kFt4Ntone; ++j)
    {
      int const ib = j * kFt4Nsps;
      for (int i = 0; i < 3 * kFt4Nsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
            * static_cast<float> (itone[j]);
        }
    }

  float const carrier_step = kTwoPi * f0 / kFt4SampleRate;
  float phi = 0.0f;
  for (int j = 0; j < kFt4NFrame; ++j)
    {
      out[j] = Complex (std::cos (phi), std::sin (phi));
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)] + carrier_step);
    }

  for (int i = 0; i < kFt4Nsps; ++i)
    {
      float const env = 0.5f * (1.0f - std::cos (kTwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (kFt4Nsps))));
      out[i] *= env;
    }

  int const tail_start = (kFt4Ntone + 1) * kFt4Nsps;
  for (int i = 0; i < kFt4Nsps; ++i)
    {
      float const env = 0.5f * (1.0f + std::cos (kTwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (kFt4Nsps))));
      out[tail_start + i] *= env;
    }
}

float evaluate_subtract_metric (float const* source,
                                Complex const* cref,
                                int nstart,
                                float f0,
                                bool refine_metric,
                                Complex const* cw,
                                float const* endcorrection,
                                int nfilt,
                                float* destination)
{
  auto& cfft = complex_fft ();
  auto& rfft = real_fft ();
  if (!cfft.valid () || !rfft.valid ())
    {
      return 0.0f;
    }

  Complex* cfilt = cfft.values ();
  std::copy (source, source + kFt8NMax, destination);

  for (int i = 0; i < kFt8NFrame; ++i)
    {
      int const sample_index = nstart - 1 + i;
      if (sample_index >= 0 && sample_index < kFt8NMax)
        {
          cfilt[i] = Complex (source[sample_index], 0.0f) * std::conj (cref[i]);
        }
      else
        {
          cfilt[i] = Complex {};
        }
    }
  std::fill (cfilt + kFt8NFrame, cfilt + kFt8NMax, Complex {});

  fftwf_execute (cfft.forward);
  for (int i = 0; i < kFt8NMax; ++i)
    {
      cfilt[i] *= cw[i];
    }
  fftwf_execute (cfft.inverse);

  for (int i = 0; i <= nfilt / 2; ++i)
    {
      cfilt[i] *= endcorrection[i];
      cfilt[kFt8NFrame - 1 - i] *= endcorrection[i];
    }

  std::fill (rfft.in, rfft.in + kFt8NMax, 0.0f);
  for (int i = 0; i < kFt8NFrame; ++i)
    {
      int const sample_index = nstart - 1 + i;
      if (sample_index >= 0 && sample_index < kFt8NMax)
        {
          Complex const z = cfilt[i] * cref[i];
          destination[sample_index] -= 2.0f * z.real ();
          rfft.in[i] = destination[sample_index];
        }
    }

  if (!refine_metric)
    {
      return 0.0f;
    }

  fftwf_execute (rfft.plan);
  float const df = kFt8SampleRate / static_cast<float> (kFt8NMax);
  int ia = static_cast<int> ((f0 - 1.5f * 6.25f) / df);
  int ib = static_cast<int> ((f0 + 8.5f * 6.25f) / df);
  ia = std::max (0, std::min (ia, kFt8NMax / 2));
  ib = std::max (0, std::min (ib, kFt8NMax / 2));
  if (ib < ia)
    {
      return 0.0f;
    }

  float sqq = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const re = rfft.out[i][0];
      float const im = rfft.out[i][1];
      sqq += re * re + im * im;
    }
  return sqq;
}

}

extern "C" void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt)
{
  if (!dd0 || !itone || !std::isfinite (f0) || !std::isfinite (dt))
    {
      return;
    }

  auto& filter = subtract_filter ();
  if (!filter.ready)
    {
      return;
    }

  auto& state = workspace ();
  if (state.cref.size () != static_cast<size_t> (kFt8NFrame)
      || state.dd.size () != static_cast<size_t> (kFt8NMax))
    {
      return;
    }
  generate_ft8_reference (itone, f0, state.cref.data ());

  auto do_subtract = [&] (int idt, bool refine_metric, float* destination) {
    int const nstart = static_cast<int> (dt * kFt8SampleRate) + 1 + idt;
    return evaluate_subtract_metric (dd0, state.cref.data (), nstart, f0, refine_metric,
                                     filter.spectrum.data (), filter.endcorrection.data (),
                                     kFt8NFilt, destination);
  };

  if (lrefinedt != 0)
    {
      float sqa = do_subtract (-90, true, state.dd.data ());
      float sqb = do_subtract (90, true, state.dd.data ());
      float sq0 = do_subtract (0, true, state.dd.data ());
      float const b = sqb - sqa;
      float const c = sqb + sqa - 2.0f * sq0;
      if (std::abs (c) < 1.0e-30f)
        {
          return;
        }
      float const dx = -b / (2.0f * c);
      if (std::abs (dx) > 1.0f)
        {
          return;
        }
      int const i2 = static_cast<int> (std::lround (90.0f * dx));
      do_subtract (i2, false, state.dd.data ());
      std::copy (state.dd.begin (), state.dd.end (), dd0);
      return;
    }

  do_subtract (0, false, state.dd.data ());
  std::copy (state.dd.begin (), state.dd.end (), dd0);
}

extern "C" void ftx_gen_ft8_refsig_c (int const* itone, float f0, fftwf_complex* cref_out)
{
  if (!itone || !cref_out)
    {
      return;
    }

  generate_ft8_reference (itone, f0, reinterpret_cast<Complex*> (cref_out));
}

extern "C" void genft8refsig_ (int* itone, fftwf_complex* cref_out, float* f0)
{
  if (!f0)
    {
      return;
    }
  ftx_gen_ft8_refsig_c (itone, *f0, cref_out);
}

extern "C" void subtractft8_ (float* dd0, int* itone, float* f0, float* dt, int* lrefinedt)
{
  if (!f0 || !dt)
    {
      return;
    }

  int const refine = (lrefinedt && *lrefinedt != 0) ? 1 : 0;
  ftx_subtract_ft8_c (dd0, itone, *f0, *dt, refine);
}

extern "C" void ftx_subtract_ft8var_c (float* dd8, int const* itone, float f0, float dt,
                                       int nfilt, fftwf_complex const* cw,
                                       float const* endcorrection)
{
  if (!dd8 || !itone || !cw || !endcorrection || nfilt <= 0)
    {
      return;
    }

  auto& state = workspace ();
  if (state.cref.size () != static_cast<size_t> (kFt8NFrame))
    {
      return;
    }
  generate_ft8_reference (itone, f0, state.cref.data ());
  int const nstart = static_cast<int> (dt * kFt8SampleRate) + 1;
  evaluate_subtract_metric (dd8, state.cref.data (), nstart, f0, false,
                            reinterpret_cast<Complex const*> (cw), endcorrection,
                            nfilt, dd8);
}

extern "C" void ftx_subtract_ft2_c (float* dd0, int const* itone, float f0, float dt)
{
  if (!dd0 || !itone || !std::isfinite (f0) || !std::isfinite (dt))
    {
      return;
    }

  auto& filter = ft2_subtract_filter ();
  if (!filter.ready)
    {
      return;
    }

  auto& state = ft2_workspace ();
  if (state.cref.size () != static_cast<size_t> (kFt2NFrame)
      || state.dd.size () != static_cast<size_t> (kFt2NMax))
    {
      return;
    }

  auto& fft = ft2_complex_fft ();
  if (!fft.valid ())
    {
      return;
    }

  generate_ft2_reference (itone, f0, state.cref.data ());
  std::copy (dd0, dd0 + kFt2NMax, state.dd.begin ());

  Complex* cfilt = fft.values ();
  std::fill (cfilt, cfilt + kFt2NMax, Complex {});

  int const nstart = static_cast<int> (dt * kFt2SampleRate) + 1 - kFt2Nsps;
  for (int i = 0; i < kFt2NFrame; ++i)
    {
      int const sample_index = nstart - 1 + i;
      if (sample_index >= 0 && sample_index < kFt2NMax)
        {
          cfilt[i] = Complex (dd0[sample_index], 0.0f) * std::conj (state.cref[static_cast<size_t> (i)]);
        }
    }

  fftwf_execute (fft.forward);
  for (int i = 0; i < kFt2NMax; ++i)
    {
      cfilt[i] *= filter.spectrum[static_cast<size_t> (i)];
    }
  fftwf_execute (fft.inverse);

  for (int i = 0; i < kFt2NFrame; ++i)
    {
      int const sample_index = nstart - 1 + i;
      if (sample_index >= 0 && sample_index < kFt2NMax)
        {
          Complex const z = cfilt[i] * state.cref[static_cast<size_t> (i)];
          state.dd[static_cast<size_t> (sample_index)] -= 2.0f * z.real ();
        }
    }

  std::copy (state.dd.begin (), state.dd.end (), dd0);
}

extern "C" void ftx_subtract_ft4_c (float* dd0, int const* itone, float f0, float dt)
{
  if (!dd0 || !itone || !std::isfinite (f0) || !std::isfinite (dt))
    {
      return;
    }

  auto& filter = ft4_subtract_filter ();
  if (!filter.ready)
    {
      return;
    }

  auto& state = ft4_workspace ();
  if (state.cref.size () != static_cast<size_t> (kFt4NFrame)
      || state.dd.size () != static_cast<size_t> (kFt4NMax))
    {
      return;
    }

  auto& fft = ft4_complex_fft ();
  if (!fft.valid ())
    {
      return;
    }

  generate_ft4_reference (itone, f0, state.cref.data ());
  std::copy (dd0, dd0 + kFt4NMax, state.dd.begin ());

  Complex* cfilt = fft.values ();
  std::fill (cfilt, cfilt + kFt4NMax, Complex {});

  int const nstart = static_cast<int> (dt * kFt4SampleRate) + 1 - kFt4Nsps;
  for (int i = 0; i < kFt4NFrame; ++i)
    {
      int const sample_index = nstart - 1 + i;
      if (sample_index >= 0 && sample_index < kFt4NMax)
        {
          cfilt[i] = Complex (dd0[sample_index], 0.0f) * std::conj (state.cref[static_cast<size_t> (i)]);
        }
    }

  fftwf_execute (fft.forward);
  for (int i = 0; i < kFt4NMax; ++i)
    {
      cfilt[i] *= filter.spectrum[static_cast<size_t> (i)];
    }
  fftwf_execute (fft.inverse);

  for (int i = 0; i < kFt4NFrame; ++i)
    {
      int const sample_index = nstart - 1 + i;
      if (sample_index >= 0 && sample_index < kFt4NMax)
        {
          Complex const z = cfilt[i] * state.cref[static_cast<size_t> (i)];
          state.dd[static_cast<size_t> (sample_index)] -= 2.0f * z.real ();
        }
    }

  std::copy (state.dd.begin (), state.dd.end (), dd0);
}

extern "C" void subtractft2_ (float* dd0, int* itone, float* f0, float* dt)
{
  if (!f0 || !dt)
    {
      return;
    }

  ftx_subtract_ft2_c (dd0, itone, *f0, *dt);
}

extern "C" void subtractft4_ (float* dd0, int* itone, float* f0, float* dt)
{
  if (!f0 || !dt)
    {
      return;
    }

  ftx_subtract_ft4_c (dd0, itone, *f0, *dt);
}
