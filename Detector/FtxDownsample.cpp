#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

constexpr int kFt8NMax = 15 * 12000;
constexpr int kFt8Nsps = 1920;
constexpr int kFt8Nfft1 = 192000;
constexpr int kFt8Nfft2 = 3200;
constexpr int kFt8Half1 = kFt8Nfft1 / 2;
constexpr int kFt8VarLow = -800;
constexpr int kFt8VarHigh = 4000;
constexpr int kFt8VarOffset = -kFt8VarLow;
constexpr int kFt8VarSize = kFt8VarHigh - kFt8VarLow + 1;
float const kFt8VarFac = 0.01f / std::sqrt (61440.0f);
constexpr int kFt2NDown = 9;
constexpr int kFt2NMax = 45000;
constexpr int kFt2Nsps = 288;
constexpr int kFt2Nfft1 = kFt2NMax;
constexpr int kFt2Nfft2 = kFt2NMax / kFt2NDown;
constexpr int kFt2Half1 = kFt2Nfft1 / 2;
constexpr int kFt4NDown = 18;
constexpr int kFt4NMax = 21 * 3456;
constexpr int kFt4Nsps = 576;
constexpr int kFt4Nfft1 = kFt4NMax;
constexpr int kFt4Nfft2 = kFt4NMax / kFt4NDown;
constexpr int kFt4Half1 = kFt4Nfft1 / 2;
constexpr int kFiltNfft = 180000;
constexpr int kFiltNh = kFiltNfft / 2;
constexpr float kInputSampleRate = 12000.0f;
constexpr float kPi = 3.14159265358979323846f;

struct Ft8DownsampleWorkspace
{
  float* long_time {nullptr};
  fftwf_complex* long_freq {nullptr};
  fftwf_complex* short_time {nullptr};
  std::array<float, 101> taper {};
  std::array<Complex, kFt8Nfft2> rotate_scratch {};
  fftwf_plan long_forward {nullptr};
  fftwf_plan short_inverse {nullptr};
  bool taper_ready {false};
  bool long_valid {false};
  bool ready {false};

  Ft8DownsampleWorkspace ()
  {
    long_time = fftwf_alloc_real (kFt8Nfft1 + 2);
    long_freq = fftwf_alloc_complex (kFt8Half1 + 1);
    short_time = fftwf_alloc_complex (kFt8Nfft2);
    if (!long_time || !long_freq || !short_time)
      {
        return;
      }

    std::memset (long_time, 0, static_cast<size_t> (kFt8Nfft1 + 2) * sizeof (float));
    std::memset (long_freq, 0, static_cast<size_t> (kFt8Half1 + 1) * sizeof (fftwf_complex));
    std::memset (short_time, 0, static_cast<size_t> (kFt8Nfft2) * sizeof (fftwf_complex));

    long_forward = fftwf_plan_dft_r2c_1d (kFt8Nfft1, long_time, long_freq, FFTW_ESTIMATE);
    short_inverse = fftwf_plan_dft_1d (kFt8Nfft2, short_time, short_time, FFTW_BACKWARD, FFTW_ESTIMATE);
    ready = long_forward && short_inverse;
  }

  ~Ft8DownsampleWorkspace ()
  {
    if (long_forward)
      {
        fftwf_destroy_plan (long_forward);
      }
    if (short_inverse)
      {
        fftwf_destroy_plan (short_inverse);
      }
    if (short_time)
      {
        fftwf_free (short_time);
      }
    if (long_freq)
      {
        fftwf_free (long_freq);
      }
    if (long_time)
      {
        fftwf_free (long_time);
      }
  }
};

struct Ft2DownsampleWorkspace
{
  float* long_time {nullptr};
  fftwf_complex* long_freq {nullptr};
  fftwf_complex* short_time {nullptr};
  std::array<float, kFt2Nfft2> window {};
  fftwf_plan long_forward {nullptr};
  fftwf_plan short_inverse {nullptr};
  bool window_ready {false};
  bool long_valid {false};
  bool ready {false};

  Ft2DownsampleWorkspace ()
  {
    long_time = fftwf_alloc_real (kFt2Nfft1);
    long_freq = fftwf_alloc_complex (kFt2Half1 + 1);
    short_time = fftwf_alloc_complex (kFt2Nfft2);
    if (!long_time || !long_freq || !short_time)
      {
        return;
      }

    std::memset (long_time, 0, static_cast<size_t> (kFt2Nfft1) * sizeof (float));
    std::memset (long_freq, 0, static_cast<size_t> (kFt2Half1 + 1) * sizeof (fftwf_complex));
    std::memset (short_time, 0, static_cast<size_t> (kFt2Nfft2) * sizeof (fftwf_complex));

    long_forward = fftwf_plan_dft_r2c_1d (kFt2Nfft1, long_time, long_freq, FFTW_ESTIMATE);
    short_inverse = fftwf_plan_dft_1d (kFt2Nfft2, short_time, short_time, FFTW_BACKWARD, FFTW_ESTIMATE);
    ready = long_forward && short_inverse;
  }

  ~Ft2DownsampleWorkspace ()
  {
    if (long_forward)
      {
        fftwf_destroy_plan (long_forward);
      }
    if (short_inverse)
      {
        fftwf_destroy_plan (short_inverse);
      }
    if (short_time)
      {
        fftwf_free (short_time);
      }
    if (long_freq)
      {
        fftwf_free (long_freq);
      }
    if (long_time)
      {
        fftwf_free (long_time);
      }
  }
};

struct Ft4DownsampleWorkspace
{
  float* long_time {nullptr};
  fftwf_complex* long_freq {nullptr};
  fftwf_complex* short_time {nullptr};
  std::array<float, kFt4Nfft2> window {};
  fftwf_plan long_forward {nullptr};
  fftwf_plan short_inverse {nullptr};
  bool window_ready {false};
  bool long_valid {false};
  bool ready {false};

  Ft4DownsampleWorkspace ()
  {
    long_time = fftwf_alloc_real (kFt4Nfft1);
    long_freq = fftwf_alloc_complex (kFt4Half1 + 1);
    short_time = fftwf_alloc_complex (kFt4Nfft2);
    if (!long_time || !long_freq || !short_time)
      {
        return;
      }

    std::memset (long_time, 0, static_cast<size_t> (kFt4Nfft1) * sizeof (float));
    std::memset (long_freq, 0, static_cast<size_t> (kFt4Half1 + 1) * sizeof (fftwf_complex));
    std::memset (short_time, 0, static_cast<size_t> (kFt4Nfft2) * sizeof (fftwf_complex));

    long_forward = fftwf_plan_dft_r2c_1d (kFt4Nfft1, long_time, long_freq, FFTW_ESTIMATE);
    short_inverse = fftwf_plan_dft_1d (kFt4Nfft2, short_time, short_time, FFTW_BACKWARD, FFTW_ESTIMATE);
    ready = long_forward && short_inverse;
  }

  ~Ft4DownsampleWorkspace ()
  {
    if (long_forward)
      {
        fftwf_destroy_plan (long_forward);
      }
    if (short_inverse)
      {
        fftwf_destroy_plan (short_inverse);
      }
    if (short_time)
      {
        fftwf_free (short_time);
      }
    if (long_freq)
      {
        fftwf_free (long_freq);
      }
    if (long_time)
      {
        fftwf_free (long_time);
      }
  }
};

struct Ft8VarDownsampleWorkspace
{
  float* long_time {nullptr};
  fftwf_complex* long_freq {nullptr};
  fftwf_complex* short_time {nullptr};
  std::array<float, 55> taper {};
  std::array<Complex, kFt8Nfft2> rotate_scratch {};
  fftwf_plan long_forward {nullptr};
  fftwf_plan short_inverse {nullptr};
  bool taper_ready {false};
  bool long_valid {false};
  bool ready {false};

  Ft8VarDownsampleWorkspace ()
  {
    long_time = fftwf_alloc_real (kFt8Nfft1 + 2);
    long_freq = fftwf_alloc_complex (kFt8Half1 + 1);
    short_time = fftwf_alloc_complex (kFt8Nfft2);
    if (!long_time || !long_freq || !short_time)
      {
        return;
      }

    std::memset (long_time, 0, static_cast<size_t> (kFt8Nfft1 + 2) * sizeof (float));
    std::memset (long_freq, 0, static_cast<size_t> (kFt8Half1 + 1) * sizeof (fftwf_complex));
    std::memset (short_time, 0, static_cast<size_t> (kFt8Nfft2) * sizeof (fftwf_complex));

    long_forward = fftwf_plan_dft_r2c_1d (kFt8Nfft1, long_time, long_freq, FFTW_ESTIMATE);
    short_inverse = fftwf_plan_dft_1d (kFt8Nfft2, short_time, short_time, FFTW_BACKWARD, FFTW_ESTIMATE);
    ready = long_forward && short_inverse;
  }

  ~Ft8VarDownsampleWorkspace ()
  {
    if (long_forward)
      {
        fftwf_destroy_plan (long_forward);
      }
    if (short_inverse)
      {
        fftwf_destroy_plan (short_inverse);
      }
    if (short_time)
      {
        fftwf_free (short_time);
      }
    if (long_freq)
      {
        fftwf_free (long_freq);
      }
    if (long_time)
      {
        fftwf_free (long_time);
      }
  }
};

Ft8DownsampleWorkspace& downsample_workspace ()
{
  thread_local Ft8DownsampleWorkspace workspace;
  return workspace;
}

Ft2DownsampleWorkspace& ft2_downsample_workspace ()
{
  thread_local Ft2DownsampleWorkspace workspace;
  return workspace;
}

Ft4DownsampleWorkspace& ft4_downsample_workspace ()
{
  thread_local Ft4DownsampleWorkspace workspace;
  return workspace;
}

Ft8VarDownsampleWorkspace& ft8var_downsample_workspace ()
{
  thread_local Ft8VarDownsampleWorkspace workspace;
  return workspace;
}

inline Complex load_complex (fftwf_complex const& value)
{
  return {value[0], value[1]};
}

inline void store_complex (fftwf_complex& dest, Complex const& value)
{
  dest[0] = value.real ();
  dest[1] = value.imag ();
}

void ensure_taper (Ft8DownsampleWorkspace& ws)
{
  if (ws.taper_ready)
    {
      return;
    }

  for (int i = 0; i <= 100; ++i)
    {
      ws.taper[static_cast<size_t> (i)] = 0.5f * (1.0f + std::cos (static_cast<float> (i) * kPi / 100.0f));
    }
  ws.taper_ready = true;
}

void compute_long_fft (Ft8DownsampleWorkspace& ws, float const* dd)
{
  if (!ws.ready)
    {
      ws.long_valid = false;
      return;
    }
  if (!dd)
    {
      std::memset (ws.long_time, 0, static_cast<size_t> (kFt8Nfft1 + 2) * sizeof (float));
      ws.long_valid = false;
      return;
    }
  std::copy (dd, dd + kFt8NMax, ws.long_time);
  std::fill (ws.long_time + kFt8NMax, ws.long_time + kFt8Nfft1 + 2, 0.0f);
  fftwf_execute (ws.long_forward);
  ws.long_valid = true;
}

void ensure_ft2_window (Ft2DownsampleWorkspace& ws)
{
  if (ws.window_ready)
    {
      return;
    }

  float const df = kInputSampleRate / static_cast<float> (kFt2Nfft1);
  float const baud = kInputSampleRate / static_cast<float> (kFt2Nsps);
  int const iwt = static_cast<int> (0.5f * baud / df);
  int const iwf = static_cast<int> (4.0f * baud / df);
  int const iws = static_cast<int> (baud / df);

  ws.window.fill (0.0f);
  for (int i = 0; i < iwt; ++i)
    {
      ws.window[static_cast<size_t> (i)] =
          0.5f * (1.0f + std::cos (kPi * static_cast<float> (iwt - 1 - i) /
                                   static_cast<float> (iwt)));
    }
  for (int i = 0; i < iwf; ++i)
    {
      ws.window[static_cast<size_t> (iwt + i)] = 1.0f;
    }
  for (int i = 0; i < iwt; ++i)
    {
      ws.window[static_cast<size_t> (iwt + iwf + i)] =
          0.5f * (1.0f + std::cos (kPi * static_cast<float> (i) /
                                   static_cast<float> (iwt)));
    }
  std::rotate (ws.window.begin (),
               ws.window.begin () + (iws % kFt2Nfft2),
               ws.window.end ());
  ws.window_ready = true;
}

void ensure_ft4_window (Ft4DownsampleWorkspace& ws)
{
  if (ws.window_ready)
    {
      return;
    }

  float const df = kInputSampleRate / static_cast<float> (kFt4Nfft1);
  float const baud = kInputSampleRate / static_cast<float> (kFt4Nsps);
  int const iwt = static_cast<int> (0.5f * baud / df);
  int const iwf = static_cast<int> (4.0f * baud / df);
  int const iws = static_cast<int> (baud / df);

  ws.window.fill (0.0f);
  for (int i = 0; i < iwt; ++i)
    {
      ws.window[static_cast<size_t> (i)] =
          0.5f * (1.0f + std::cos (kPi * static_cast<float> (iwt - 1 - i) /
                                   static_cast<float> (iwt)));
    }
  for (int i = 0; i < iwf; ++i)
    {
      ws.window[static_cast<size_t> (iwt + i)] = 1.0f;
    }
  for (int i = 0; i < iwt; ++i)
    {
      ws.window[static_cast<size_t> (iwt + iwf + i)] =
          0.5f * (1.0f + std::cos (kPi * static_cast<float> (i) /
                                   static_cast<float> (iwt)));
    }
  std::rotate (ws.window.begin (),
               ws.window.begin () + (iws % kFt4Nfft2),
               ws.window.end ());
  ws.window_ready = true;
}

void compute_ft2_long_fft (Ft2DownsampleWorkspace& ws, float const* dd)
{
  if (!ws.ready)
    {
      ws.long_valid = false;
      return;
    }
  if (!dd)
    {
      std::memset (ws.long_time, 0, static_cast<size_t> (kFt2Nfft1) * sizeof (float));
      ws.long_valid = false;
      return;
    }

  std::copy (dd, dd + kFt2NMax, ws.long_time);
  fftwf_execute (ws.long_forward);
  ws.long_valid = true;
}

void compute_ft4_long_fft (Ft4DownsampleWorkspace& ws, float const* dd)
{
  if (!ws.ready)
    {
      ws.long_valid = false;
      return;
    }
  if (!dd)
    {
      std::memset (ws.long_time, 0, static_cast<size_t> (kFt4Nfft1) * sizeof (float));
      ws.long_valid = false;
      return;
    }

  std::copy (dd, dd + kFt4NMax, ws.long_time);
  fftwf_execute (ws.long_forward);
  ws.long_valid = true;
}

void ensure_ft8var_taper (Ft8VarDownsampleWorkspace& ws)
{
  if (ws.taper_ready)
    {
      return;
    }

  for (int i = 0; i <= 54; ++i)
    {
      ws.taper[static_cast<size_t> (i)] = 0.5f * (1.0f + std::cos (static_cast<float> (i) * kPi / 55.0f));
    }
  ws.taper_ready = true;
}

void compute_ft8var_long_fft (Ft8VarDownsampleWorkspace& ws, float const* dd)
{
  if (!ws.ready)
    {
      ws.long_valid = false;
      return;
    }
  if (!dd)
    {
      std::memset (ws.long_time, 0, static_cast<size_t> (kFt8Nfft1 + 2) * sizeof (float));
      ws.long_valid = false;
      return;
    }

  std::copy (dd, dd + kFt8NMax, ws.long_time);
  std::fill (ws.long_time + kFt8NMax, ws.long_time + kFt8Nfft1 + 2, 0.0f);
  fftwf_execute (ws.long_forward);
  ws.long_valid = true;
}

void ft8_downsample_impl (float const* dd, bool* newdat, float f0, Complex* c1)
{
  auto& ws = downsample_workspace ();
  ensure_taper (ws);

  if (!c1 || !ws.ready)
    {
      return;
    }

  std::fill (c1, c1 + kFt8Nfft2, Complex {});

  if (!std::isfinite (f0))
    {
      return;
    }

  bool const need_fft = !ws.long_valid || (newdat && *newdat);
  if (need_fft)
    {
      compute_long_fft (ws, dd);
      if (!ws.long_valid)
        {
          return;
        }
      if (newdat)
        {
          *newdat = false;
        }
    }

  float const df = kInputSampleRate / static_cast<float> (kFt8Nfft1);
  float const baud = kInputSampleRate / static_cast<float> (kFt8Nsps);
  int const i0 = static_cast<int> (std::lround (f0 / df));
  float const ft = f0 + 8.5f * baud;
  int const it = std::min (static_cast<int> (std::lround (ft / df)), kFt8Half1);
  float const fb = f0 - 1.5f * baud;
  int const ib = std::max (1, static_cast<int> (std::lround (fb / df)));

  if (it < ib)
    {
      return;
    }

  for (int i = 0; i < kFt8Nfft2; ++i)
    {
      ws.short_time[i][0] = 0.0f;
      ws.short_time[i][1] = 0.0f;
    }
  int k = 0;
  for (int i = ib; i <= it && k < kFt8Nfft2; ++i)
    {
      ws.short_time[k][0] = ws.long_freq[i][0];
      ws.short_time[k][1] = ws.long_freq[i][1];
      ++k;
    }

  if (k <= 0)
    {
      return;
    }

  for (int i = 0; i <= 100 && i < k; ++i)
    {
      ws.short_time[i][0] *= ws.taper[static_cast<size_t> (100 - i)];
      ws.short_time[i][1] *= ws.taper[static_cast<size_t> (100 - i)];
    }
  for (int i = 0; i <= 100 && i < k; ++i)
    {
      int const idx = k - 1 - i;
      if (idx >= 0)
        {
          ws.short_time[idx][0] *= ws.taper[static_cast<size_t> (i)];
          ws.short_time[idx][1] *= ws.taper[static_cast<size_t> (i)];
        }
    }

  if (kFt8Nfft2 > 0)
    {
      int shift = i0 - ib;
      shift %= kFt8Nfft2;
      if (shift < 0)
        {
          shift += kFt8Nfft2;
        }
      if (shift != 0)
        {
          for (int i = 0; i < kFt8Nfft2; ++i)
            {
              ws.rotate_scratch[static_cast<size_t> (i)] = load_complex (ws.short_time[(i + shift) % kFt8Nfft2]);
            }
          for (int i = 0; i < kFt8Nfft2; ++i)
            {
              store_complex (ws.short_time[i], ws.rotate_scratch[static_cast<size_t> (i)]);
            }
        }
    }

  fftwf_execute (ws.short_inverse);

  float const fac = 1.0f / std::sqrt (static_cast<float> (kFt8Nfft1) * static_cast<float> (kFt8Nfft2));
  for (int i = 0; i < kFt8Nfft2; ++i)
    {
      c1[i] = load_complex (ws.short_time[i]) * fac;
    }
}

void ft2_downsample_impl (float const* dd, bool* newdata, float f0, Complex* c)
{
  auto& ws = ft2_downsample_workspace ();
  ensure_ft2_window (ws);

  if (!c || !ws.ready)
    {
      return;
    }

  std::fill (c, c + kFt2Nfft2, Complex {});

  if (!std::isfinite (f0))
    {
      return;
    }

  bool const need_fft = !ws.long_valid || (newdata && *newdata);
  if (need_fft)
    {
      compute_ft2_long_fft (ws, dd);
      if (!ws.long_valid)
        {
          return;
        }
      if (newdata)
        {
          *newdata = false;
        }
    }

  float const df = kInputSampleRate / static_cast<float> (kFt2Nfft1);
  int const i0 = static_cast<int> (std::lround (f0 / df));

  for (int i = 0; i < kFt2Nfft2; ++i)
    {
      ws.short_time[i][0] = 0.0f;
      ws.short_time[i][1] = 0.0f;
    }

  if (i0 >= 0 && i0 <= kFt2Half1)
    {
      ws.short_time[0][0] = ws.long_freq[i0][0];
      ws.short_time[0][1] = ws.long_freq[i0][1];
    }

  for (int i = 1; i <= kFt2Nfft2 / 2; ++i)
    {
      if (i0 + i >= 0 && i0 + i <= kFt2Half1)
        {
          ws.short_time[i][0] = ws.long_freq[i0 + i][0];
          ws.short_time[i][1] = ws.long_freq[i0 + i][1];
        }
      if (i0 - i >= 0 && i0 - i <= kFt2Half1)
        {
          ws.short_time[kFt2Nfft2 - i][0] = ws.long_freq[i0 - i][0];
          ws.short_time[kFt2Nfft2 - i][1] = ws.long_freq[i0 - i][1];
        }
    }

  float const scale = 1.0f / static_cast<float> (kFt2Nfft2);
  for (int i = 0; i < kFt2Nfft2; ++i)
    {
      float const w = ws.window[static_cast<size_t> (i)] * scale;
      ws.short_time[i][0] *= w;
      ws.short_time[i][1] *= w;
    }

  fftwf_execute (ws.short_inverse);
  for (int i = 0; i < kFt2Nfft2; ++i)
    {
      c[i] = load_complex (ws.short_time[i]);
    }
}

void ft4_downsample_impl (float const* dd, bool* newdata, float f0, Complex* c)
{
  auto& ws = ft4_downsample_workspace ();
  ensure_ft4_window (ws);

  if (!c || !ws.ready)
    {
      return;
    }

  std::fill (c, c + kFt4Nfft2, Complex {});

  if (!std::isfinite (f0))
    {
      return;
    }

  bool const need_fft = !ws.long_valid || (newdata && *newdata);
  if (need_fft)
    {
      compute_ft4_long_fft (ws, dd);
      if (!ws.long_valid)
        {
          return;
        }
      if (newdata)
        {
          *newdata = false;
        }
    }

  float const df = kInputSampleRate / static_cast<float> (kFt4Nfft1);
  int const i0 = static_cast<int> (std::lround (f0 / df));

  for (int i = 0; i < kFt4Nfft2; ++i)
    {
      ws.short_time[i][0] = 0.0f;
      ws.short_time[i][1] = 0.0f;
    }

  if (i0 >= 0 && i0 <= kFt4Half1)
    {
      ws.short_time[0][0] = ws.long_freq[i0][0];
      ws.short_time[0][1] = ws.long_freq[i0][1];
    }

  for (int i = 1; i <= kFt4Nfft2 / 2; ++i)
    {
      if (i0 + i >= 0 && i0 + i <= kFt4Half1)
        {
          ws.short_time[i][0] = ws.long_freq[i0 + i][0];
          ws.short_time[i][1] = ws.long_freq[i0 + i][1];
        }
      if (i0 - i >= 0 && i0 - i <= kFt4Half1)
        {
          ws.short_time[kFt4Nfft2 - i][0] = ws.long_freq[i0 - i][0];
          ws.short_time[kFt4Nfft2 - i][1] = ws.long_freq[i0 - i][1];
        }
    }

  float const scale = 1.0f / static_cast<float> (kFt4Nfft2);
  for (int i = 0; i < kFt4Nfft2; ++i)
    {
      float const w = ws.window[static_cast<size_t> (i)] * scale;
      ws.short_time[i][0] *= w;
      ws.short_time[i][1] *= w;
    }

  fftwf_execute (ws.short_inverse);
  for (int i = 0; i < kFt4Nfft2; ++i)
    {
      c[i] = load_complex (ws.short_time[i]);
    }
}

void zero_ft8var_output (Complex* out)
{
  if (!out)
    {
      return;
    }
  std::fill (out, out + kFt8VarSize, Complex {});
}

void ft8var_downsample_impl (float const* dd, bool* newdat, float f0, int nqso,
                             Complex* c0, Complex* c2, Complex* c3, bool lhighsens,
                             bool* lsubtracted, int* npos, float const* freqsub)
{
  auto& ws = ft8var_downsample_workspace ();
  ensure_ft8var_taper (ws);

  zero_ft8var_output (c0);
  zero_ft8var_output (c2);
  zero_ft8var_output (c3);

  if (!c0 || !ws.ready || !std::isfinite (f0))
    {
      return;
    }

  bool redo_fft = false;
  if (lsubtracted && *lsubtracted && npos && freqsub)
    {
      for (int i = 0; i < *npos; ++i)
        {
          if (std::fabs (f0 - freqsub[i]) < 50.0f)
            {
              redo_fft = true;
              *lsubtracted = false;
              break;
            }
        }
    }

  bool const need_fft = !ws.long_valid || (newdat && *newdat) || redo_fft;
  if (need_fft)
    {
      compute_ft8var_long_fft (ws, dd);
      if (!ws.long_valid)
        {
          return;
        }
      if (newdat)
        {
          *newdat = false;
        }
      if (npos)
        {
          *npos = 0;
        }
    }

  float const df = 0.0625f;
  int const i0 = static_cast<int> (std::lround (f0 / df));
  int const it = std::min (static_cast<int> (std::lround ((f0 + 55.75f) / df)), kFt8Half1);
  int const ib = std::max (1, static_cast<int> (std::lround ((f0 - 5.75f) / df)));
  if (it < ib)
    {
      return;
    }

  for (int i = 0; i < kFt8Nfft2; ++i)
    {
      ws.short_time[i][0] = 0.0f;
      ws.short_time[i][1] = 0.0f;
    }

  int const k = it - ib;
  for (int i = 0; i <= k && i < kFt8Nfft2; ++i)
    {
      ws.short_time[i][0] = ws.long_freq[ib + i][0];
      ws.short_time[i][1] = ws.long_freq[ib + i][1];
    }
  if (k < 0)
    {
      return;
    }

  for (int i = 0; i <= 54 && i <= k; ++i)
    {
      float const leading = ws.taper[static_cast<size_t> (54 - i)];
      ws.short_time[i][0] *= leading;
      ws.short_time[i][1] *= leading;
    }
  for (int i = 0; i <= 54 && i <= k; ++i)
    {
      int const idx = k - 54 + i;
      if (idx >= 0 && idx < kFt8Nfft2)
        {
          float const trailing = ws.taper[static_cast<size_t> (i)];
          ws.short_time[idx][0] *= trailing;
          ws.short_time[idx][1] *= trailing;
        }
    }

  int shift = i0 - ib;
  shift %= kFt8Nfft2;
  if (shift < 0)
    {
      shift += kFt8Nfft2;
    }
  if (shift != 0)
    {
      for (int i = 0; i < kFt8Nfft2; ++i)
        {
          ws.rotate_scratch[static_cast<size_t> (i)] = load_complex (ws.short_time[(i + shift) % kFt8Nfft2]);
        }
      for (int i = 0; i < kFt8Nfft2; ++i)
        {
          store_complex (ws.short_time[i], ws.rotate_scratch[static_cast<size_t> (i)]);
        }
    }

  if (lhighsens)
    {
      ws.short_time[0][0] *= 1.93f; ws.short_time[0][1] *= 1.93f;
      ws.short_time[799][0] *= 1.7f; ws.short_time[799][1] *= 1.7f;
      ws.short_time[800][0] *= 1.7f; ws.short_time[800][1] *= 1.7f;
      ws.short_time[3199][0] *= 1.93f; ws.short_time[3199][1] *= 1.93f;
    }
  else
    {
      ws.short_time[45][0] *= 1.49f; ws.short_time[45][1] *= 1.49f;
      ws.short_time[54][0] *= 1.49f; ws.short_time[54][1] *= 1.49f;
      ws.short_time[3145][0] *= 1.49f; ws.short_time[3145][1] *= 1.49f;
      ws.short_time[3154][0] *= 1.49f; ws.short_time[3154][1] *= 1.49f;
    }

  fftwf_execute (ws.short_inverse);

  std::array<Complex, kFt8Nfft2> base {};
  for (int i = 0; i < kFt8Nfft2; ++i)
    {
      base[static_cast<size_t> (i)] = load_complex (ws.short_time[i]) * kFt8VarFac;
      c0[kFt8VarOffset + i] = base[static_cast<size_t> (i)];
    }

  if (nqso > 1 && c2)
    {
      for (int i = 0; i < kFt8Nfft2 - 1; ++i)
        {
          c2[kFt8VarOffset + i] = 0.5f * (base[static_cast<size_t> (i)] + base[static_cast<size_t> (i + 1)]);
        }
      c2[kFt8VarOffset + kFt8Nfft2 - 1] = base[static_cast<size_t> (kFt8Nfft2 - 1)];
    }

  if (nqso == 3 && c3)
    {
      c3[kFt8VarOffset] = base[0];
      for (int i = 1; i < kFt8Nfft2; ++i)
        {
          c3[kFt8VarOffset + i] = 0.5f * (base[static_cast<size_t> (i - 1)] + base[static_cast<size_t> (i)]);
        }
    }
}

void ft8_filter_impl (float f0, int nslots, float width, float* wave)
{
  thread_local std::vector<float> x (static_cast<size_t> (kFiltNfft), 0.0f);
  thread_local std::vector<Complex> cx (static_cast<size_t> (kFiltNh + 1));
  thread_local fftwf_plan forward = fftwf_plan_dft_r2c_1d (kFiltNfft,
                                                           x.data (),
                                                           reinterpret_cast<fftwf_complex*> (cx.data ()),
                                                           FFTW_ESTIMATE);
  thread_local fftwf_plan inverse = fftwf_plan_dft_c2r_1d (kFiltNfft,
                                                           reinterpret_cast<fftwf_complex*> (cx.data ()),
                                                           x.data (),
                                                           FFTW_ESTIMATE);

  std::copy (wave, wave + kFiltNfft, x.begin ());
  fftwf_execute (forward);

  float const df = kInputSampleRate / static_cast<float> (kFiltNfft);
  float const fa = f0 - 0.5f * 6.25f;
  float const fb = f0 + 7.5f * 6.25f + static_cast<float> (nslots - 1) * 60.0f;
  int const ia2 = static_cast<int> (std::lround (fa / df));
  int const ib1 = static_cast<int> (std::lround (fb / df));
  int const ia1 = static_cast<int> (std::lround (static_cast<float> (ia2) - width / df));
  int const ib2 = static_cast<int> (std::lround (static_cast<float> (ib1) + width / df));

  int const lo_keep = std::max (0, ia1);
  int const lo_taper_end = std::min (ia2, kFiltNh);
  int const hi_taper_start = std::max (ib1, 0);
  int const hi_keep = std::min (ib2, kFiltNh);

  for (int i = lo_keep; i <= lo_taper_end; ++i)
    {
      float const fil = 0.5f * (1.0f + std::cos (kPi * df * static_cast<float> (i - ia2) / width));
      cx[static_cast<size_t> (i)] *= fil;
    }
  for (int i = hi_taper_start; i <= hi_keep; ++i)
    {
      float const fil = 0.5f * (1.0f + std::cos (kPi * df * static_cast<float> (i - ib1) / width));
      cx[static_cast<size_t> (i)] *= fil;
    }

  for (int i = 0; i < lo_keep && i <= kFiltNh; ++i)
    {
      cx[static_cast<size_t> (i)] = Complex {};
    }
  for (int i = std::max (hi_keep + 1, 0); i <= kFiltNh; ++i)
    {
      cx[static_cast<size_t> (i)] = Complex {};
    }

  fftwf_execute (inverse);
  float const scale = 1.0f / static_cast<float> (kFiltNfft);
  for (int i = 0; i < kFiltNfft; ++i)
    {
      wave[i] = x[static_cast<size_t> (i)] * scale;
    }
}

}

extern "C" void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1)
{
  if (!c1)
    {
      return;
    }
  bool need_fft = newdat && (*newdat != 0);
  ft8_downsample_impl (dd, &need_fft, f0, reinterpret_cast<Complex*> (c1));
  if (newdat)
    {
      *newdat = need_fft ? 1 : 0;
    }
}

extern "C" void ft8_downsample_ (float* dd, int* newdat, float* f0, fftwf_complex* c1)
{
  if (!f0)
    {
      return;
    }

  int newdat_local = newdat ? ((*newdat != 0) ? 1 : 0) : 0;
  int const had_newdat = newdat_local;
  ftx_ft8_downsample_c (dd, &newdat_local, *f0, c1);
  if (newdat && had_newdat != 0)
    {
      *newdat = newdat_local != 0 ? 1 : 0;
    }
}

extern "C" void ftx_ft8var_downsample_c (float const* dd, int* newdat, float const* f0,
                                          int const* nqso, fftwf_complex* c0, fftwf_complex* c2,
                                          fftwf_complex* c3, int const* lhighsens,
                                          int* lsubtracted, int* npos, float const* freqsub)
{
  if (!f0)
    {
      return;
    }

  bool need_fft = newdat && (*newdat != 0);
  bool subtracted = lsubtracted && (*lsubtracted != 0);
  int local_npos = npos ? *npos : 0;
  ft8var_downsample_impl (dd, &need_fft, *f0, nqso ? *nqso : 1,
                          reinterpret_cast<Complex*> (c0),
                          reinterpret_cast<Complex*> (c2),
                          reinterpret_cast<Complex*> (c3),
                          lhighsens && (*lhighsens != 0),
                          &subtracted, &local_npos, freqsub);
  if (newdat)
    {
      *newdat = need_fft ? 1 : 0;
    }
  if (lsubtracted)
    {
      *lsubtracted = subtracted ? 1 : 0;
    }
  if (npos)
    {
      *npos = local_npos;
    }
}

extern "C" void ftx_ft2_downsample_c (float const* dd, int* newdata, float f0, fftwf_complex* c)
{
  if (!c)
    {
      return;
    }
  bool need_fft = newdata && (*newdata != 0);
  ft2_downsample_impl (dd, &need_fft, f0, reinterpret_cast<Complex*> (c));
  if (newdata)
    {
      *newdata = need_fft ? 1 : 0;
    }
}

extern "C" void ftx_ft4_downsample_c (float const* dd, int* newdata, float f0, fftwf_complex* c)
{
  if (!c)
    {
      return;
    }
  bool need_fft = newdata && (*newdata != 0);
  ft4_downsample_impl (dd, &need_fft, f0, reinterpret_cast<Complex*> (c));
  if (newdata)
    {
      *newdata = need_fft ? 1 : 0;
    }
}

extern "C" void ft4_downsample_ (float* dd, int* newdata, float* f0, fftwf_complex* c)
{
  if (!f0)
    {
      return;
    }

  int newdata_local = newdata ? ((*newdata != 0) ? 1 : 0) : 0;
  int const had_newdata = newdata_local;
  ftx_ft4_downsample_c (dd, &newdata_local, *f0, c);
  if (newdata && had_newdata != 0)
    {
      *newdata = newdata_local != 0 ? 1 : 0;
    }
}

extern "C" void ftx_filt8_c (float const* f0, int const* nslots, float const* width, float* wave)
{
  ft8_filter_impl (f0 ? *f0 : 0.0f, nslots ? *nslots : 0, width ? *width : 0.0f, wave);
}

extern "C" void filt8_ (float* f0, int* nslots, float* width, float* wave)
{
  ftx_filt8_c (f0, nslots, width, wave);
}
