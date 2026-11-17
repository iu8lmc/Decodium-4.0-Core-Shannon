#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <complex>
#include <limits>
#include <memory>
#include <vector>

#include <fftw3.h>

#include "Detector/FftCompat.hpp"
#include "Detector/FtxFastMath.hpp"

namespace
{

using Complex = std::complex<float>;

constexpr float kTiny = 1.0e-30f;

template <typename T>
T clamp_value (T value, T lo, T hi)
{
  return std::max (lo, std::min (hi, value));
}

struct ComplexFft32
{
  ComplexFft32 ()
    : in (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * 32))),
      out (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * 32))),
      plan (nullptr)
  {
    if (in && out)
      {
        plan = decodium::fft_compat::plan_dft_1d (32, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~ComplexFft32 ()
  {
    if (plan)
      {
        decodium::fft_compat::destroy_plan (plan);
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

  fftwf_complex* in;
  fftwf_complex* out;
  fftwf_plan plan;
};

ComplexFft32& fft32 ()
{
  // FFTW teardown can race macOS/OpenMP TLS finalization during application
  // shutdown. Keep this tiny per-thread workspace alive until process exit
  // instead of destroying its FFTW plan from a late thread_local destructor.
  static thread_local auto* instance = new ComplexFft32;
  return *instance;
}

inline float abs2 (Complex const& value)
{
  return std::norm (value);
}

inline float abs1 (Complex const& value)
{
  return std::abs (value);
}

inline float& bm_at (float* bitmetrics, int rows, int row, int col)
{
  return bitmetrics[row + rows * col];
}

float ft2_legacy_equalized_beta (std::array<std::array<float, 4>, 103> const& pwr_eq,
                                 float* noise_var_eq_out = nullptr)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};

  int noise_sum_eq = 0;
  int nnoise_eq = 0;
  for (int k = 0; k < 4; ++k)
    {
      for (int itone = 0; itone < 4; ++itone)
        {
          if (itone != icos4a[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
          if (itone != icos4b[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k + 33)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
          if (itone != icos4c[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k + 66)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
          if (itone != icos4d[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k + 99)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
        }
    }

  int noise_var_eq = 0;
  if (nnoise_eq > 0)
    {
      noise_var_eq = static_cast<int> (static_cast<float> (noise_sum_eq)
                                       / std::max (1.0f, static_cast<float> (nnoise_eq)));
    }
  if (static_cast<float> (noise_var_eq) < 1.0e-10f)
    {
      noise_var_eq = static_cast<int> (1.0e-10f);
    }
  if (noise_var_eq_out)
    {
      *noise_var_eq_out = static_cast<float> (noise_var_eq);
    }

  float beta_eq = std::numeric_limits<float>::infinity ();
  if (noise_var_eq != 0)
    {
      beta_eq = 0.5f / static_cast<float> (noise_var_eq);
    }
  return clamp_value (beta_eq, 0.01f, 50.0f);
}

float ft2_legacy_base_noise_var (std::array<std::array<float, 4>, 103> const& pwr)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};

  int noise_sum = 0;
  int nnoise = 0;
  for (int k = 0; k < 4; ++k)
    {
      for (int itone = 0; itone < 4; ++itone)
        {
          if (itone != icos4a[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
          if (itone != icos4b[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k + 33)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
          if (itone != icos4c[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k + 66)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
          if (itone != icos4d[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k + 99)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
        }
    }

  float noise_var = 0.0f;
  if (nnoise > 0)
    {
      noise_var = static_cast<float> (noise_sum) / std::max (1.0f, static_cast<float> (nnoise));
    }
  return std::max (noise_var, 1.0e-10f);
}

void normalizebmet_cpp (float* data, int n)
{
  float sum = 0.0f;
  float sum2 = 0.0f;
  for (int i = 0; i < n; ++i)
    {
      sum += data[i];
      sum2 += data[i] * data[i];
    }

  float const mean = sum / static_cast<float> (n);
  float const mean2 = sum2 / static_cast<float> (n);
  float const var = mean2 - mean * mean;
  float sigma = 0.0f;
  if (var > 0.0f)
    {
      sigma = std::sqrt (var);
    }
  else
    {
      sigma = std::sqrt (std::max (mean2, 0.0f));
    }
  if (sigma <= 0.0f)
    {
      return;
    }

  for (int i = 0; i < n; ++i)
    {
      data[i] /= sigma;
    }
}

// DECODIUM_FT8_STORICO_ENERGIA=1: somma di energie nello storico CQ (vedi
// save_cq_signal_history in FtxFt8Stage4.cpp). Letto una volta sola.
inline bool ft8_history_energy_mode ()
{
  static bool const v = [] {
    // Acceso di default dalla 1.0.625. DECODIUM_FT8_STORICO_ENERGIA=0 lo spegne.
    char const* raw = std::getenv ("DECODIUM_FT8_STORICO_ENERGIA");
    return !raw || raw[0] != '0';
  }();
  return v;
}

void normalizebmet_rms_cpp (float* data, int n)
{
  float sum2 = 0.0f;
  for (int i = 0; i < n; ++i)
    {
      sum2 += data[i] * data[i];
    }

  float const mean2 = sum2 / static_cast<float> (n);
  float const sigma = std::sqrt (std::max (mean2, 0.0f));
  if (sigma <= 0.0f)
    {
      return;
    }

  for (int i = 0; i < n; ++i)
    {
      data[i] /= sigma;
    }
}

void finalize_ft2_bitmetric_columns (float* bitmetrics, int rows)
{
  bm_at (bitmetrics, rows, 204, 1) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 1) = bm_at (bitmetrics, rows, 205, 0);
  for (int i = 200; i < 204; ++i)
    {
      bm_at (bitmetrics, rows, i, 2) = bm_at (bitmetrics, rows, i, 1);
    }
  bm_at (bitmetrics, rows, 204, 2) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 2) = bm_at (bitmetrics, rows, 205, 0);

  normalizebmet_cpp (bitmetrics + rows * 1, rows);
  normalizebmet_cpp (bitmetrics + rows * 2, rows);
}

std::array<std::array<bool, 8>, 256> const& one8_table ()
{
  static std::array<std::array<bool, 8>, 256> table = [] {
    std::array<std::array<bool, 8>, 256> value {};
    for (int i = 0; i < 256; ++i)
      {
        for (int j = 0; j < 8; ++j)
          {
            value[static_cast<size_t> (i)][static_cast<size_t> (j)] = (i & (1 << j)) != 0;
          }
      }
    return value;
  }();
  return table;
}

std::array<std::array<bool, 9>, 512> const& one9_table ()
{
  static std::array<std::array<bool, 9>, 512> table = [] {
    std::array<std::array<bool, 9>, 512> value {};
    for (int i = 0; i < 512; ++i)
      {
        for (int j = 0; j < 9; ++j)
          {
            value[static_cast<size_t> (i)][static_cast<size_t> (j)] = (i & (1 << j)) != 0;
          }
      }
    return value;
  }();
  return table;
}

void fft_symbol_4tones (Complex const* input, std::array<Complex, 4>& tones,
                        std::array<float, 4>& mags, std::array<float, 4>* power = nullptr)
{
  auto& fft = fft32 ();
  if (!fft.valid ())
    {
      tones.fill (Complex {});
      mags.fill (0.0f);
      if (power)
        {
          power->fill (0.0f);
        }
      return;
    }

  for (int i = 0; i < 32; ++i)
    {
      fft.in[i][0] = input[i].real ();
      fft.in[i][1] = input[i].imag ();
    }
  fftwf_execute (fft.plan);

  for (int i = 0; i < 4; ++i)
    {
      Complex const value {fft.out[i][0], fft.out[i][1]};
      tones[static_cast<size_t> (i)] = value;
      mags[static_cast<size_t> (i)] = abs1 (value);
      if (power)
        {
          (*power)[static_cast<size_t> (i)] = abs2 (value);
        }
    }
}

void dft_symbol_4tones (Complex const* input, std::array<Complex, 4>& tones,
                        std::array<float, 4>& mags, std::array<float, 4>* power = nullptr)
{
  constexpr int NSS = 32;
  constexpr float kTwoPi = 6.28318530717958647692f;

  tones.fill (Complex {});
  mags.fill (0.0f);
  if (power)
    {
      power->fill (0.0f);
    }

  static auto const phasors = [] {
    std::array<std::array<Complex, 4>, NSS> table {};
    for (int n = 0; n < NSS; ++n)
      for (int bin = 0; bin < 4; ++bin)
        {
          float const angle = -kTwoPi * static_cast<float> (bin * n) / static_cast<float> (NSS);
          table[static_cast<size_t> (n)][static_cast<size_t> (bin)] = Complex {std::cos (angle), std::sin (angle)};
        }
    return table;
  } ();
  for (int n = 0; n < NSS; ++n)
    {
      Complex const sample = input[n];
      for (int bin = 0; bin < 4; ++bin)
        {
          tones[static_cast<size_t> (bin)] += sample * phasors[static_cast<size_t> (n)][static_cast<size_t> (bin)];
        }
    }

  for (int bin = 0; bin < 4; ++bin)
    {
      Complex const value = tones[static_cast<size_t> (bin)];
      mags[static_cast<size_t> (bin)] = abs1 (value);
      if (power)
        {
          (*power)[static_cast<size_t> (bin)] = abs2 (value);
        }
    }
}

void fft_symbol_8tones_ft8 (Complex const* input, std::array<Complex, 8>& tones,
                            std::array<float, 8>& mags)
{
  auto& fft = fft32 ();
  if (!fft.valid ())
    {
      tones.fill (Complex {});
      mags.fill (0.0f);
      return;
    }

  for (int i = 0; i < 32; ++i)
    {
      fft.in[i][0] = input[i].real ();
      fft.in[i][1] = input[i].imag ();
    }
  fftwf_execute (fft.plan);

  for (int i = 0; i < 8; ++i)
    {
      Complex const value {fft.out[i][0], fft.out[i][1]};
      tones[static_cast<size_t> (i)] = value;
      mags[static_cast<size_t> (i)] = abs1 (value);
    }
}

float ft8_sync_average (Complex const* cd0, int np2, int ibest,
                        std::array<int, 7> const& icos7)
{
  constexpr int NN = 79;
  constexpr int NSS = 32;

  float total = 0.0f;
  int count = 0;
  for (int symbol = 0; symbol < NN; ++symbol)
    {
      int sync_index = -1;
      if (symbol < 7)
        {
          sync_index = symbol;
        }
      else if (symbol >= 36 && symbol < 43)
        {
          sync_index = symbol - 36;
        }
      else if (symbol >= 72)
        {
          sync_index = symbol - 72;
        }
      if (sync_index < 0)
        {
          continue;
        }

      int const i1 = ibest + symbol * NSS;
      if (i1 < 0 || i1 + NSS - 1 > np2 - 1)
        {
          continue;
        }

      std::array<Complex, NSS> csymb {};
      for (int i = 0; i < NSS; ++i)
        {
          csymb[static_cast<size_t> (i)] = cd0[i1 + i];
        }

      std::array<Complex, 8> tones {};
      std::array<float, 8> mags {};
      fft_symbol_8tones_ft8 (csymb.data (), tones, mags);

      int const sync_tone = icos7[static_cast<size_t> (sync_index)];
      float const sync_level = mags[static_cast<size_t> (sync_tone)];
      float total_level = 0.0f;
      for (float value : mags)
        {
          total_level += value;
        }
      float const noise_level = (total_level - sync_level) / 7.0f;
      if (noise_level > 1.0e-16f)
        {
          total += sync_level / noise_level;
          ++count;
        }
    }

  return count > 0 ? total / static_cast<float> (count) : 99.0f;
}

void condition_ft8_weak_symbol_edges (std::array<Complex, 32>& csymb)
{
  csymb[0] *= 1.9f;
  csymb[31] *= 1.9f;

  float const first = abs1 (csymb[0]);
  float const last = abs1 (csymb[31]);
  if (last <= 1.0e-16f)
    {
      return;
    }

  float const scr = std::sqrt (first) / std::sqrt (last);
  if (scr > 1.0f)
    {
      csymb[31] *= scr;
    }
  else if (scr > 1.0e-16f)
    {
      csymb[0] /= scr;
    }
}

bool check_ft4_sync (std::array<std::array<float, 4>, 103> const& s4, int minimum)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};
  int is1 = 0;
  int is2 = 0;
  int is3 = 0;
  int is4 = 0;

  for (int k = 0; k < 4; ++k)
    {
      auto argmax = [] (std::array<float, 4> const& row) {
        return static_cast<int> (std::max_element (row.begin (), row.end ()) - row.begin ());
      };
      if (icos4a[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k)])) ++is1;
      if (icos4b[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k + 33)])) ++is2;
      if (icos4c[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k + 66)])) ++is3;
      if (icos4d[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k + 99)])) ++is4;
    }

  return (is1 + is2 + is3 + is4) >= minimum;
}

void ft2_channel_est_cpp (Complex const* cd, Complex* cd_eq, float* ch_snr)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};
  constexpr int NN = 103;
  constexpr int NSS = 32;

  std::array<int, 16> sync_pos {};
  for (int j = 0; j < 4; ++j)
    {
      sync_pos[static_cast<size_t> (j)] = j + 1;
      sync_pos[static_cast<size_t> (j + 4)] = j + 34;
      sync_pos[static_cast<size_t> (j + 8)] = j + 67;
      sync_pos[static_cast<size_t> (j + 12)] = j + 100;
    }

  std::array<Complex, 16> h_sync {};
  std::array<Complex, NN> h_est {};
  std::array<float, NN> h_mag {};
  float sum_noise = 0.0f;
  float ncount = 0.0f;

  for (int j = 0; j < 16; ++j)
    {
      int const k = sync_pos[static_cast<size_t> (j)];
      int const idx = (k - 1) * NSS;
      std::array<Complex, 4> cs_rx {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + idx, cs_rx, mags, &power);

      int itone = 0;
      if (j < 4) itone = icos4a[static_cast<size_t> (j)];
      else if (j < 8) itone = icos4b[static_cast<size_t> (j - 4)];
      else if (j < 12) itone = icos4c[static_cast<size_t> (j - 8)];
      else itone = icos4d[static_cast<size_t> (j - 12)];

      h_sync[static_cast<size_t> (j)] = cs_rx[static_cast<size_t> (itone)];
      for (int m = 0; m < 4; ++m)
        {
          if (m != itone)
            {
              sum_noise += power[static_cast<size_t> (m)];
              ncount += 1.0f;
            }
        }
    }

  float noise_var = ncount > 0.0f ? sum_noise / ncount : 1.0e-10f;
  noise_var = std::max (noise_var, 1.0e-10f);

  for (int j = 0; j < 16; ++j)
    {
      h_est[static_cast<size_t> (sync_pos[static_cast<size_t> (j)] - 1)] = h_sync[static_cast<size_t> (j)];
    }

  h_est[0] = h_sync[0];
  h_est[1] = 0.5f * (h_sync[0] + h_sync[1]);
  h_est[2] = 0.5f * (h_sync[1] + h_sync[2]);
  h_est[3] = 0.5f * (h_sync[2] + h_sync[3]);

  for (int k = 5; k <= 33; ++k)
    {
      float w = static_cast<float> (k - 3) / static_cast<float> (35 - 3);
      w = clamp_value (w, 0.0f, 1.0f);
      h_est[static_cast<size_t> (k - 1)] = (1.0f - w) * 0.5f * (h_sync[2] + h_sync[3])
        + w * 0.5f * (h_sync[4] + h_sync[5]);
    }

  h_est[33] = 0.5f * (h_sync[4] + h_sync[5]);
  h_est[34] = 0.5f * (h_sync[5] + h_sync[6]);
  h_est[35] = 0.5f * (h_sync[6] + h_sync[7]);
  h_est[36] = h_sync[7];

  for (int k = 38; k <= 66; ++k)
    {
      float w = static_cast<float> (k - 36) / static_cast<float> (68 - 36);
      w = clamp_value (w, 0.0f, 1.0f);
      h_est[static_cast<size_t> (k - 1)] = (1.0f - w) * 0.5f * (h_sync[6] + h_sync[7])
        + w * 0.5f * (h_sync[8] + h_sync[9]);
    }

  h_est[66] = 0.5f * (h_sync[8] + h_sync[9]);
  h_est[67] = 0.5f * (h_sync[9] + h_sync[10]);
  h_est[68] = 0.5f * (h_sync[10] + h_sync[11]);
  h_est[69] = h_sync[11];

  for (int k = 71; k <= 99; ++k)
    {
      float w = static_cast<float> (k - 69) / static_cast<float> (101 - 69);
      w = clamp_value (w, 0.0f, 1.0f);
      h_est[static_cast<size_t> (k - 1)] = (1.0f - w) * 0.5f * (h_sync[10] + h_sync[11])
        + w * 0.5f * (h_sync[12] + h_sync[13]);
    }

  h_est[99] = 0.5f * (h_sync[12] + h_sync[13]);
  h_est[100] = 0.5f * (h_sync[13] + h_sync[14]);
  h_est[101] = 0.5f * (h_sync[14] + h_sync[15]);
  h_est[102] = h_sync[15];

  for (int k = 0; k < NN; ++k)
    {
      h_mag[static_cast<size_t> (k)] = abs2 (h_est[static_cast<size_t> (k)]);
    }

  for (int k = 0; k < NN; ++k)
    {
      int const idx = k * NSS;
      float const den = h_mag[static_cast<size_t> (k)] + noise_var;
      if (den > 1.0e-20f)
        {
          Complex const coeff = std::conj (h_est[static_cast<size_t> (k)]) / den;
          for (int i = 0; i < NSS; ++i)
            {
              cd_eq[idx + i] = cd[idx + i] * coeff;
            }
        }
      else
        {
          std::copy (cd + idx, cd + idx + NSS, cd_eq + idx);
        }

      ch_snr[k] = noise_var > 1.0e-20f ? h_mag[static_cast<size_t> (k)] / noise_var : 100.0f;
    }
}

// Somme complesse di tutte le combinazioni di toni di un gruppo di nsym simboli
// consecutivi. tone (k, v) restituisce il tono v, gia' in ordine Gray, del
// simbolo k-esimo del gruppo. Nell'indice di una combinazione il primo simbolo
// occupa le cifre piu' significative e la somma procede dal primo simbolo
// all'ultimo, come nelle espressioni scritte per esteso che questa funzione
// sostituisce: i valori ottenuti sono gli stessi, bit per bit.
template <typename ToneAt>
inline int ftx_build_group_sums (int tones, int nsym, ToneAt const& tone, Complex* sums)
{
  int count = tones;
  for (int value = 0; value < tones; ++value)
    {
      sums[value] = tone (0, value);
    }
  for (int symbol = 1; symbol < nsym; ++symbol)
    {
      std::array<Complex, 8> next {};
      for (int value = 0; value < tones; ++value)
        {
          next[static_cast<size_t> (value)] = tone (symbol, value);
        }
      // Espansione sul posto, dall'ultima combinazione alla prima.
      for (int i = count - 1; i >= 0; --i)
        {
          Complex const base = sums[i];
          for (int value = tones - 1; value >= 0; --value)
            {
              sums[i * tones + value] = base + next[static_cast<size_t> (value)];
            }
        }
      count *= tones;
    }
  return count;
}

// Massimi per bit di un gruppo di nsym simboli: FT8 (8 toni, fino a 3 simboli)
// e FT4 (4 toni, fino a 4 simboli).
//
// La metrica di un bit e' max(|somma|) sulle combinazioni col bit a 1 meno lo
// stesso massimo col bit a 0. La forma diretta calcolava il modulo (hypotf) di
// tutte le combinazioni e poi le riscorreva una volta per bit: in FT8 da sola
// era oltre meta' del tempo di una decodifica a profondita' 3.
//
// Qui il confronto avviene sui moduli al quadrato, che ordinano le combinazioni
// allo stesso modo, e il modulo vero viene calcolato solo per le vincitrici.
// I massimi per bit si ricavano dai massimi marginali per cifra (una cifra =
// un simbolo), cosi' le combinazioni si scorrono una volta sola.
// Il valore restituito e' lo stesso abs1() della combinazione vincente che
// calcolava la forma diretta.
//
// I quadrati sono in doppia precisione di proposito. In singola, due
// combinazioni con moduli vicini entro l'ultimo bit potevano scambiarsi
// l'ordine rispetto ad abs1() e il massimo usciva diverso di un bit (misurato:
// un valore su decine di milioni). In doppia i prodotti sono esatti e
// l'ordine coincide con quello dei moduli.
//
// max1[p] / max0[p]: modulo massimo fra le combinazioni col bit p dell'indice
// rispettivamente a 1 e a 0, per p = 0 .. (bit per simbolo) * nsym - 1.
//
// L'area di lavoro appartiene al chiamante, che la crea una volta per tutti i
// gruppi: 8^3 combinazioni in FT8, 4^4 in FT4.
struct FtxMaximaWorkspace
{
  std::array<Complex, 512> sums {};
  std::array<double, 512> power {};
};

template <size_t Tones, size_t Symbols>
inline void ftx_group_bit_maxima (std::array<std::array<Complex, Tones>, Symbols> const& cs,
                                  std::array<int, Tones> const& graymap,
                                  int nsym, int ks, float* max1, float* max0,
                                  FtxMaximaWorkspace& work)
{
  static_assert (Tones == 4 || Tones == 8, "four or eight tones per symbol");
  constexpr int tones = static_cast<int> (Tones);
  constexpr int digit_bits = Tones == 8 ? 3 : 2;

  std::array<Complex, 512>& sums = work.sums;
  std::array<double, 512>& power = work.power;
  int const nt = ftx_build_group_sums (
      tones, nsym,
      [&cs, &graymap, ks] (int symbol, int value) {
        return cs[static_cast<size_t> (ks - 1 + symbol)]
                 [static_cast<size_t> (graymap[static_cast<size_t> (value)])];
      },
      sums.data ());

  for (int i = 0; i < nt; ++i)
    {
      double const re = sums[static_cast<size_t> (i)].real ();
      double const im = sums[static_cast<size_t> (i)].imag ();
      power[static_cast<size_t> (i)] = re * re + im * im;
    }

  // Massimo marginale per ogni valore di ogni cifra dell'indice.
  std::array<std::array<double, 8>, 4> best_power {};
  std::array<std::array<int, 8>, 4> best_index {};
  for (int digit = 0; digit < nsym; ++digit)
    {
      int const shift = digit_bits * digit;
      for (int value = 0; value < tones; ++value)
        {
          best_power[static_cast<size_t> (digit)][static_cast<size_t> (value)] = -1.0;
          best_index[static_cast<size_t> (digit)][static_cast<size_t> (value)] = value << shift;
        }
    }
  for (int i = 0; i < nt; ++i)
    {
      double const value = power[static_cast<size_t> (i)];
      for (int digit = 0; digit < nsym; ++digit)
        {
          size_t const slot = static_cast<size_t> ((i >> (digit_bits * digit)) & (tones - 1));
          if (value > best_power[static_cast<size_t> (digit)][slot])
            {
              best_power[static_cast<size_t> (digit)][slot] = value;
              best_index[static_cast<size_t> (digit)][slot] = i;
            }
        }
    }

  for (int bit = 0; bit < digit_bits * nsym; ++bit)
    {
      size_t const digit = static_cast<size_t> (bit / digit_bits);
      int const mask = 1 << (bit % digit_bits);
      double power1 = -1.0;
      double power0 = -1.0;
      int index1 = -1;
      int index0 = -1;
      for (int value = 0; value < tones; ++value)
        {
          double const candidate = best_power[digit][static_cast<size_t> (value)];
          if ((value & mask) != 0)
            {
              if (index1 < 0 || candidate > power1)
                {
                  power1 = candidate;
                  index1 = best_index[digit][static_cast<size_t> (value)];
                }
            }
          else if (index0 < 0 || candidate > power0)
            {
              power0 = candidate;
              index0 = best_index[digit][static_cast<size_t> (value)];
            }
        }
      max1[bit] = abs1 (sums[static_cast<size_t> (index1)]);
      max0[bit] = abs1 (sums[static_cast<size_t> (index0)]);
    }
}

// Massimo di n valori. Quattro accumulatori indipendenti invece di una sola
// catena: il massimo e' una selezione esatta, quindi l'ordine non cambia il
// risultato.
inline float ftx_max_of (float const* values, int n)
{
  float a = -1.0e30f;
  float b = -1.0e30f;
  float c = -1.0e30f;
  float d = -1.0e30f;
  int i = 0;
  for (; i + 4 <= n; i += 4)
    {
      a = std::max (a, values[i]);
      b = std::max (b, values[i + 1]);
      c = std::max (c, values[i + 2]);
      d = std::max (d, values[i + 3]);
    }
  for (; i < n; ++i)
    {
      a = std::max (a, values[i]);
    }
  return std::max (std::max (a, b), std::max (c, d));
}

// Copia in dst, in ordine crescente di indice, gli elementi di src il cui
// indice ha il bit indicato a 1 (set) o a 0. nt e' una potenza di due.
inline void ftx_gather_half (float const* src, int nt, int bit, bool set, float* dst)
{
  int const run = 1 << bit;
  for (int base = set ? run : 0; base < nt; base += 2 * run)
    {
      // Ciclo esplicito: per i tratti corti una chiamata a memmove costa piu'
      // della copia.
      for (int j = 0; j < run; ++j)
        {
          dst[j] = src[base + j];
        }
      dst += run;
    }
}

// Area di lavoro di ftx_group_bit_lse. Appartiene al chiamante, che la crea
// una volta per tutti i gruppi: azzerarla a ogni gruppo costerebbe piu' degli
// esponenziali risparmiati.
struct FtxLseWorkspace
{
  std::array<float, 256> value {};
  std::array<float, 256> folded {};
  std::array<float, 256> global_terms {};
  std::array<float, 128> half_values {};
  // Una corsia per ogni meta' di ogni bit: gli addendi in ordine di indice.
  std::array<std::array<float, 128>, 16> lanes {};
};

// Metriche di bit FT2 di un gruppo: per ogni bit dell'indice di combinazione
//
//   (max1 + log sum exp (v - max1)) - (max0 + log sum exp (v - max0))
//
// dove le somme corrono sulle combinazioni col bit a 1 e a 0 e max1/max0 sono i
// rispettivi massimi. values[i] e' il valore gia' scalato della combinazione i;
// nt = 2^nbits, al piu' 256. metric[p] e' la metrica del bit p dell'indice.
//
// La forma diretta ricalcolava un esponenziale per ogni combinazione e per
// ogni bit, e sommava una combinazione alla volta. Qui:
//  - per ogni bit una delle due meta' contiene il massimo globale, quindi i
//    suoi termini exp (v - max) sono gli stessi per tutti i bit e si calcolano
//    una volta sola;
//  - gli esponenziali dell'altra meta' si calcolano su un vettore contiguo,
//    vettorizzabile dove il compilatore lo consente;
//  - le sedici somme (due meta' per otto bit) avanzano insieme invece che una
//    dopo l'altra.
// Ogni addendo e' lo stesso valore di prima e ogni somma procede nello stesso
// ordine di indice: i risultati coincidono bit per bit con la forma diretta.
inline void ftx_group_bit_lse (float const* values, int nt, int nbits, float* metric,
                               FtxLseWorkspace& work)
{
  std::array<float, 256>& value = work.value;
  std::array<float, 256>& folded = work.folded;
  std::copy_n (values, nt, value.begin ());
  std::copy_n (values, nt, folded.begin ());

  // Massimi delle due meta' per ogni bit: si toglie ogni volta il bit piu' alto
  // sostituendo ogni coppia col suo massimo.
  std::array<float, 8> max1 {};
  std::array<float, 8> max0 {};
  int width = nt;
  for (int bit = nbits - 1; bit >= 0; --bit)
    {
      int const half = width / 2;
      max0[static_cast<size_t> (bit)] = ftx_max_of (folded.data (), half);
      max1[static_cast<size_t> (bit)] = ftx_max_of (folded.data () + half, half);
      for (int j = 0; j < half; ++j)
        {
          folded[static_cast<size_t> (j)] =
              std::max (folded[static_cast<size_t> (j)], folded[static_cast<size_t> (half + j)]);
        }
      width = half;
    }

  // Termini riferiti al massimo globale, comuni a tutti i bit.
  float const global = std::max (max1[0], max0[0]);
  float* const global_terms = work.global_terms.data ();
  for (int i = 0; i < nt; ++i)
    {
      global_terms[i] = decodium::ftx_expf (value[static_cast<size_t> (i)] - global);
    }

  int const half = nt / 2;
  float* const half_values = work.half_values.data ();
  for (int bit = 0; bit < nbits; ++bit)
    {
      for (int set = 0; set <= 1; ++set)
        {
          float const maximum = set != 0 ? max1[static_cast<size_t> (bit)]
                                         : max0[static_cast<size_t> (bit)];
          float* const lane = work.lanes[static_cast<size_t> (2 * bit + set)].data ();
          if (maximum == global)
            {
              ftx_gather_half (global_terms, nt, bit, set != 0, lane);
            }
          else
            {
              ftx_gather_half (value.data (), nt, bit, set != 0, half_values);
              for (int j = 0; j < half; ++j)
                {
                  lane[j] = decodium::ftx_expf (half_values[j] - maximum);
                }
            }
        }
    }

  // Tutte le somme insieme, ciascuna nel proprio ordine di indice. Le corsie
  // oltre 2 * nbits non sono usate: sommarle costa meno che distinguerle.
  std::array<float, 16> lse {};
  for (int j = 0; j < half; ++j)
    {
      for (size_t lane = 0; lane < 16; ++lane)
        {
          lse[lane] += work.lanes[lane][static_cast<size_t> (j)];
        }
    }

  for (int bit = 0; bit < nbits; ++bit)
    {
      float const maxp1 = max1[static_cast<size_t> (bit)];
      float const maxp0 = max0[static_cast<size_t> (bit)];
      float const lse1 = lse[static_cast<size_t> (2 * bit + 1)];
      float const lse0 = lse[static_cast<size_t> (2 * bit)];
      metric[bit] = (maxp1 + std::log (std::max (lse1, kTiny))) -
                    (maxp0 + std::log (std::max (lse0, kTiny)));
    }
}

void run_ft4_bitmetrics (Complex const* cd, float* bitmetrics, int* badsync)
{
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  constexpr int NN = 103;
  constexpr int NSS = 32;
  constexpr int rows = 2 * NN;

  std::fill (bitmetrics, bitmetrics + rows * 3, 0.0f);
  *badsync = 0;

  std::array<std::array<Complex, 4>, NN> cs {};
  std::array<std::array<float, 4>, NN> s4 {};

  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      fft_symbol_4tones (cd + k * NSS, tones, mags);
      cs[static_cast<size_t> (k)] = tones;
      s4[static_cast<size_t> (k)] = mags;
    }

  if (!check_ft4_sync (s4, 8))
    {
      *badsync = 1;
      return;
    }

  FtxMaximaWorkspace maxima_work;
  for (int nseq = 1; nseq <= 3; ++nseq)
    {
      int const nsym = (nseq == 1 ? 1 : (nseq == 2 ? 2 : 4));
      int const ibmax = (nsym == 1 ? 1 : (nsym == 2 ? 3 : 7));

      for (int ks = 1; ks <= NN - nsym + 1; ks += nsym)
        {
          // Massimi indicizzati per bit dell'indice di combinazione.
          std::array<float, 8> group_max1 {};
          std::array<float, 8> group_max0 {};
          ftx_group_bit_maxima (cs, graymap, nsym, ks,
                                group_max1.data (), group_max0.data (), maxima_work);

          int const ipt = 1 + (ks - 1) * 2;
          for (int ib = 0; ib <= ibmax; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }
              bm_at (bitmetrics, rows, ipt + ib - 1, nseq - 1) =
                  group_max1[static_cast<size_t> (ibmax - ib)]
                  - group_max0[static_cast<size_t> (ibmax - ib)];
            }
        }
    }

  bm_at (bitmetrics, rows, 204, 1) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 1) = bm_at (bitmetrics, rows, 205, 0);
  for (int i = 200; i < 204; ++i)
    {
      bm_at (bitmetrics, rows, i, 2) = bm_at (bitmetrics, rows, i, 1);
    }
  bm_at (bitmetrics, rows, 204, 2) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 2) = bm_at (bitmetrics, rows, 205, 0);

  normalizebmet_cpp (bitmetrics + rows * 0, rows);
  normalizebmet_cpp (bitmetrics + rows * 1, rows);
  normalizebmet_cpp (bitmetrics + rows * 2, rows);
}

void run_ft2_bitmetrics_impl (Complex const* cd, float* bitmetrics, int* badsync,
                              bool allow_equalized_branch,
                              float* bitmetrics_base_out,
                              float* bmet_eq_raw_out,
                              float* bmet_eq_out,
                              Complex* cd_eq_out,
                              float* ch_snr_out,
                              int* use_cheq_out,
                              float* snr_min_out,
                              float* snr_max_out,
                              float* snr_mean_out,
                              float* fading_depth_out,
                              float* noise_var_out,
                              float* noise_var_eq_out)
{
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  auto const& one = one8_table ();
  constexpr int NN = 103;
  constexpr int NSS = 32;
  constexpr int rows = 2 * NN;

  std::fill (bitmetrics, bitmetrics + rows * 3, 0.0f);
  *badsync = 0;
  if (bitmetrics_base_out)
    {
      std::fill (bitmetrics_base_out, bitmetrics_base_out + rows * 3, 0.0f);
    }
  if (bmet_eq_raw_out)
    {
      std::fill (bmet_eq_raw_out, bmet_eq_raw_out + rows, 0.0f);
    }
  if (bmet_eq_out)
    {
      std::fill (bmet_eq_out, bmet_eq_out + rows, 0.0f);
    }
  if (cd_eq_out)
    {
      std::fill_n (cd_eq_out, NN * NSS, Complex {});
    }
  if (ch_snr_out)
    {
      std::fill (ch_snr_out, ch_snr_out + NN, 0.0f);
    }
  if (use_cheq_out) *use_cheq_out = 0;
  if (snr_min_out) *snr_min_out = 0.0f;
  if (snr_max_out) *snr_max_out = 0.0f;
  if (snr_mean_out) *snr_mean_out = 0.0f;
  if (fading_depth_out) *fading_depth_out = 0.0f;
  if (noise_var_out) *noise_var_out = 0.0f;
  if (noise_var_eq_out) *noise_var_eq_out = 0.0f;

  std::array<std::array<Complex, 4>, NN> cs {};
  std::array<std::array<float, 4>, NN> s4 {};
  std::array<std::array<float, 4>, NN> pwr {};

  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + k * NSS, tones, mags, &power);
      cs[static_cast<size_t> (k)] = tones;
      s4[static_cast<size_t> (k)] = mags;
      pwr[static_cast<size_t> (k)] = power;
    }

  float noise_var = ft2_legacy_base_noise_var (pwr);
  if (noise_var_out)
    {
      *noise_var_out = noise_var;
    }
  float beta = clamp_value (0.5f / noise_var, 0.01f, 50.0f);

  if (!check_ft4_sync (s4, 3))
    {
      *badsync = 1;
      return;
    }

  std::array<float, rows> bmet_eq {};
  FtxLseWorkspace lse_work;
  for (int nseq = 1; nseq <= 3; ++nseq)
    {
      int const nsym = (nseq == 1 ? 1 : (nseq == 2 ? 2 : 4));
      int const nt = 1 << (2 * nsym);
      int const ibmax = (nsym == 1 ? 1 : (nsym == 2 ? 3 : 7));
      float const beta_eff = beta / static_cast<float> (nsym);
      std::array<Complex, 256> sums {};
      std::array<float, 256> sp {};

      for (int ks = 1; ks <= NN - nsym + 1; ks += nsym)
        {
          ftx_build_group_sums (
              4, nsym,
              [&cs, ks] (int symbol, int value) {
                return cs[static_cast<size_t> (ks - 1 + symbol)]
                         [static_cast<size_t> (graymap[static_cast<size_t> (value)])];
              },
              sums.data ());
          for (int i = 0; i < nt; ++i)
            {
              float const power = abs2 (sums[static_cast<size_t> (i)]);
              sp[static_cast<size_t> (i)] = beta_eff * power;
            }

          std::array<float, 8> group_metric {};
          ftx_group_bit_lse (sp.data (), nt, ibmax + 1, group_metric.data (), lse_work);

          int const ipt = 1 + (ks - 1) * 2;
          for (int ib = 0; ib <= ibmax; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }
              bm_at (bitmetrics, rows, ipt + ib - 1, nseq - 1) =
                  group_metric[static_cast<size_t> (ibmax - ib)];
            }
        }
    }

  std::array<float, rows * 3> bitmetrics_base {};
  std::copy_n (bitmetrics, rows * 3, bitmetrics_base.data ());
  normalizebmet_cpp (bitmetrics_base.data () + rows * 0, rows);
  finalize_ft2_bitmetric_columns (bitmetrics_base.data (), rows);
  if (bitmetrics_base_out)
    {
      std::copy_n (bitmetrics_base.data (), rows * 3, bitmetrics_base_out);
    }

  std::vector<Complex> cd_eq (NN * NSS);
  std::array<float, NN> ch_snr {};
  ft2_channel_est_cpp (cd, cd_eq.data (), ch_snr.data ());
  if (cd_eq_out)
    {
      std::copy_n (cd_eq.data (), NN * NSS, cd_eq_out);
    }
  if (ch_snr_out)
    {
      std::copy_n (ch_snr.data (), NN, ch_snr_out);
    }

  float snr_min = ch_snr[0];
  float snr_max = ch_snr[0];
  float snr_mean = 0.0f;
  for (float value : ch_snr)
    {
      snr_min = std::min (snr_min, value);
      snr_max = std::max (snr_max, value);
      snr_mean += value;
    }
  snr_mean /= static_cast<float> (NN);
  float fading_depth = snr_min > 1.0e-10f ? 10.0f * std::log10 (snr_max / snr_min) : 30.0f;
  if (snr_min_out) *snr_min_out = snr_min;
  if (snr_max_out) *snr_max_out = snr_max;
  if (snr_mean_out) *snr_mean_out = snr_mean;
  if (fading_depth_out) *fading_depth_out = fading_depth;
  // The legacy FT2 demapper relies on implicit Fortran typing in the
  // equalized-noise path. Keep that behaviour explicit here so the C++ port
  // matches the historical decoder output, including weak-signal edge cases.
  bool const use_cheq = allow_equalized_branch && fading_depth > 2.0f;
  if (use_cheq_out)
    {
      *use_cheq_out = use_cheq ? 1 : 0;
    }

  if (use_cheq)
    {
      std::array<std::array<Complex, 4>, NN> cs_eq {};
      std::array<std::array<float, 4>, NN> pwr_eq {};
      for (int k = 0; k < NN; ++k)
        {
          std::array<Complex, 4> tones {};
          std::array<float, 4> mags {};
          std::array<float, 4> power {};
          dft_symbol_4tones (cd_eq.data () + k * NSS, tones, mags, &power);
          cs_eq[static_cast<size_t> (k)] = tones;
          pwr_eq[static_cast<size_t> (k)] = power;
        }

      float beta_eq = ft2_legacy_equalized_beta (pwr_eq, noise_var_eq_out);

      for (int ks = 1; ks <= NN; ++ks)
        {
          std::array<float, 4> sp_eq {};
          for (int i = 0; i < 4; ++i)
            {
              sp_eq[static_cast<size_t> (i)] = pwr_eq[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i)])];
            }
          int const ipt = 1 + (ks - 1) * 2;
          float snr_weight = 1.0f;
          if (snr_mean > 1.0e-10f)
            {
              snr_weight = std::sqrt (ch_snr[static_cast<size_t> (ks - 1)] / snr_mean);
              snr_weight = clamp_value (snr_weight, 0.1f, 3.0f);
            }
          for (int ib = 0; ib <= 1; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }
              float maxp1 = -1.0e30f;
              float maxp0 = -1.0e30f;
              for (int i = 0; i < 4; ++i)
                {
                  float const pval = beta_eq * sp_eq[static_cast<size_t> (i)];
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (1 - ib)];
                  if (set) maxp1 = std::max (maxp1, pval);
                  else maxp0 = std::max (maxp0, pval);
                }
              float lse1 = 0.0f;
              float lse0 = 0.0f;
              for (int i = 0; i < 4; ++i)
                {
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (1 - ib)];
                  float const pval = beta_eq * sp_eq[static_cast<size_t> (i)];
                  if (set) lse1 += decodium::ftx_expf (pval - maxp1);
                  else lse0 += decodium::ftx_expf (pval - maxp0);
                }
              bmet_eq[static_cast<size_t> (ipt + ib - 1)] =
                ((maxp1 + std::log (std::max (lse1, kTiny))) -
                 (maxp0 + std::log (std::max (lse0, kTiny)))) * snr_weight;
            }
        }

      if (bmet_eq_raw_out)
        {
          std::copy_n (bmet_eq.data (), rows, bmet_eq_raw_out);
        }
      normalizebmet_cpp (bmet_eq.data (), rows);
      if (bmet_eq_out)
        {
          std::copy_n (bmet_eq.data (), rows, bmet_eq_out);
        }
      float blend = clamp_value ((fading_depth - 2.0f) / 10.0f, 0.0f, 0.90f);
      normalizebmet_cpp (bitmetrics + rows * 0, rows);
      for (int i = 0; i < rows; ++i)
        {
          bm_at (bitmetrics, rows, i, 0) =
            (1.0f - blend) * bm_at (bitmetrics, rows, i, 0) + blend * bmet_eq[static_cast<size_t> (i)];
        }
      normalizebmet_cpp (bitmetrics + rows * 0, rows);
    }
  else
    {
      normalizebmet_cpp (bitmetrics + rows * 0, rows);
    }

  finalize_ft2_bitmetric_columns (bitmetrics, rows);
}

void run_ft2_bitmetrics (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft2_bitmetrics_impl (cd, bitmetrics, badsync,
                           true,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

void run_ft8_bitmetrics (Complex const* cd0, int np2, int ibest, int imetric,
                         float scale, bool weak_deep,
                         bool equalize_tone_power,
                         Complex const* history_cs,
                         Complex* current_cs_out,
                         float* s8_out, int* nsync_out,
                         float* llra, float* llrb, float* llrc,
                         float* llrd, float* llre)
{
  constexpr int NN = 79;
  constexpr int NSS = 32;
  static std::array<int, 7> const icos7 {{3, 1, 4, 0, 6, 5, 2}};
  static std::array<int, 8> const graymap {{0, 1, 3, 2, 5, 6, 4, 7}};
  auto const& one = one9_table ();

  std::array<std::array<Complex, 8>, NN> cs {};
  std::array<std::array<Complex, 8>, NN> cs_reverse {};
  std::array<float, 512> s2 {};
  std::array<float, 174> bmeta {};
  std::array<float, 174> bmetb {};
  std::array<float, 174> bmetc {};
  std::array<float, 174> bmetd {};
  std::array<float, 174> bmete {};

  bool const reverse_metric = (imetric == 3);
  bool const need_reverse_tones = reverse_metric || imetric == 4;
  int const metric_shape = reverse_metric ? 1 : (imetric == 4 ? 2 : imetric);
  float const sync_average = ft8_sync_average (cd0, np2, ibest, icos7);
  bool const condition_weak_edges = sync_average < 2.5f;

  for (int k = 0; k < NN; ++k)
    {
      int const i1 = ibest + k * NSS;
      std::array<Complex, 32> csymb {};
      if (i1 >= 0 && i1 + 31 <= np2 - 1)
        {
          for (int i = 0; i < NSS; ++i)
            {
              csymb[static_cast<size_t> (i)] = cd0[i1 + i];
            }
        }
      if (condition_weak_edges)
        {
          condition_ft8_weak_symbol_edges (csymb);
        }

      std::array<Complex, 8> forward_tones {};
      std::array<float, 8> forward_mags {};
      fft_symbol_8tones_ft8 (csymb.data (), forward_tones, forward_mags);

      // I toni del simbolo rovesciato servono solo alle metriche 3 e 4.
      std::array<Complex, 8> reverse_tones {};
      std::array<float, 8> reverse_mags {};
      if (need_reverse_tones)
        {
          std::array<Complex, 32> csymb_reverse {};
          for (int i = 0; i < NSS; ++i)
            {
              csymb_reverse[static_cast<size_t> (i)] =
                  std::conj (csymb[static_cast<size_t> (NSS - 1 - i)]);
            }
          fft_symbol_8tones_ft8 (csymb_reverse.data (), reverse_tones, reverse_mags);
        }

      auto const& tones = reverse_metric ? reverse_tones : forward_tones;
      auto const& mags = reverse_metric ? reverse_mags : forward_mags;
      for (int tone = 0; tone < 8; ++tone)
        {
          cs[static_cast<size_t> (k)][static_cast<size_t> (tone)] = tones[static_cast<size_t> (tone)] / 1.0e3f;
          cs_reverse[static_cast<size_t> (k)][static_cast<size_t> (tone)] =
              reverse_tones[static_cast<size_t> (tone)] / 1.0e3f;
          s8_out[tone + 8 * k] = mags[static_cast<size_t> (tone)];
        }
    }

  if (equalize_tone_power)
    {
      std::array<float, 8> tone_power {};
      for (int tone = 0; tone < 8; ++tone)
        {
          for (int symbol = 0; symbol < 7; ++symbol)
            {
              tone_power[static_cast<size_t> (tone)] += s8_out[tone + 8 * symbol];
            }
          for (int symbol = 17; symbol < NN; ++symbol)
            {
              tone_power[static_cast<size_t> (tone)] += s8_out[tone + 8 * symbol];
            }
        }
      int quiet_tone = 0;
      for (int tone = 1; tone < 8; ++tone)
        {
          if (tone_power[static_cast<size_t> (tone)] < tone_power[static_cast<size_t> (quiet_tone)])
            {
              quiet_tone = tone;
            }
        }
      float const quiet_power = tone_power[static_cast<size_t> (quiet_tone)];
      if (quiet_power > 1.0e-12f)
        {
          for (int tone = 0; tone < 8; ++tone)
            {
              if (tone == quiet_tone)
                {
                  continue;
                }
              float const ratio = tone_power[static_cast<size_t> (tone)] / quiet_power;
              if (ratio <= 1.5f)
                {
                  continue;
                }
              float const complex_scale = 1.0f / std::sqrt (ratio);
              for (int symbol = 0; symbol < NN; ++symbol)
                {
                  s8_out[tone + 8 * symbol] /= ratio;
                  cs[static_cast<size_t> (symbol)][static_cast<size_t> (tone)] *= complex_scale;
                  cs_reverse[static_cast<size_t> (symbol)][static_cast<size_t> (tone)] *= complex_scale;
                }
            }
        }
    }

  if (current_cs_out)
    {
      for (int symbol = 0; symbol < NN; ++symbol)
        {
          for (int tone = 0; tone < 8; ++tone)
            {
              current_cs_out[symbol * 8 + tone] =
                  cs[static_cast<size_t> (symbol)][static_cast<size_t> (tone)];
            }
        }
    }

  int is1 = 0;
  int is2 = 0;
  int is3 = 0;
  for (int k = 0; k < 7; ++k)
    {
      auto argmax_symbol = [s8_out](int symbol) {
        int best = 0;
        float best_value = s8_out[8 * symbol];
        for (int tone = 1; tone < 8; ++tone)
          {
            float const value = s8_out[tone + 8 * symbol];
            if (value > best_value)
              {
                best = tone;
                best_value = value;
              }
          }
        return best;
      };

      if (icos7[static_cast<size_t> (k)] == argmax_symbol (k)) ++is1;
      if (icos7[static_cast<size_t> (k)] == argmax_symbol (k + 36)) ++is2;
      if (icos7[static_cast<size_t> (k)] == argmax_symbol (k + 72)) ++is3;
    }
  *nsync_out = is1 + is2 + is3;

  float srr = 99.0f;
  if (weak_deep)
    {
      float synclev = 0.0f;
      float sumlev = 0.0f;
      for (int k = 0; k < 7; ++k)
        {
          int const symbol = k + 36;
          synclev += s8_out[icos7[static_cast<size_t> (k)] + 8 * symbol];
          for (int tone = 0; tone < 8; ++tone)
            {
              sumlev += s8_out[tone + 8 * symbol];
            }
        }
      float snoiselev = (sumlev - synclev) / 7.0f;
      if (snoiselev < 0.1f)
        {
          snoiselev = 1.0f;
        }
      srr = synclev / snoiselev;
    }
  bool const use_weak_transform = weak_deep && srr < 2.5f;
  FtxMaximaWorkspace maxima_work;

  for (int nsym = 1; nsym <= 3; ++nsym)
    {
      int const nt = 1 << (3 * nsym);
      for (int ihalf = 1; ihalf <= 2; ++ihalf)
        {
          for (int k = 1; k <= 29; k += nsym)
            {
              int const ks = (ihalf == 1) ? (k + 7) : (k + 43);
              int const i32 = 1 + (k - 1) * 3 + (ihalf - 1) * 87;
              int ibmax = 2;
              if (nsym == 2) ibmax = 5;
              if (nsym == 3) ibmax = 8;

              // Massimi indicizzati per bit dell'indice di combinazione.
              std::array<float, 9> group_max1 {};
              std::array<float, 9> group_max0 {};
              if (!history_cs)
                {
                  ftx_group_bit_maxima (cs, graymap, nsym, ks,
                                        group_max1.data (), group_max0.data (),
                                        maxima_work);
                  // Le forme di metrica sono crescenti nel modulo: applicarle
                  // al solo massimo equivale ad applicarle a ogni combinazione.
                  auto const shape = [&] (float value) {
                    if (use_weak_transform)
                      {
                        if (metric_shape == 1)
                          {
                            if (srr > 2.3f)
                              {
                                value *= value;
                              }
                            else if (value < 5.77f)
                              {
                                float const value2 = value * value;
                                value = 1.0f + 8.0f * value2 - 0.12f * value2 * value2;
                              }
                            else
                              {
                                value = (value + 5.82f) * (value + 5.82f);
                              }
                          }
                        else
                          {
                            value = std::pow (0.5f * value, 3.0f);
                          }
                      }
                    else if (metric_shape == 2)
                      {
                        value *= value;
                      }
                    return value;
                  };
                  for (int bit = 0; bit <= ibmax; ++bit)
                    {
                      group_max1[static_cast<size_t> (bit)] = shape (group_max1[static_cast<size_t> (bit)]);
                      group_max0[static_cast<size_t> (bit)] = shape (group_max0[static_cast<size_t> (bit)]);
                    }
                }
              else
                {
                  for (int i = 0; i < nt; ++i)
                    {
                      int const i1 = i / 64;
                      int const i2 = (i & 63) / 8;
                      int const i3 = i & 7;
                      float value = 0.0f;
                      Complex current {};
                      Complex history {};
                      if (nsym == 1)
                        {
                          int const tone = graymap[static_cast<size_t> (i3)];
                          current = cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (tone)];
                          history = history_cs[(ks - 1) * 8 + tone];
                        }
                      else if (nsym == 2)
                        {
                          int const tone1 = graymap[static_cast<size_t> (i2)];
                          int const tone2 = graymap[static_cast<size_t> (i3)];
                          current =
                              cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (tone1)] +
                              cs[static_cast<size_t> (ks)][static_cast<size_t> (tone2)];
                          history = history_cs[(ks - 1) * 8 + tone1]
                                    + history_cs[ks * 8 + tone2];
                        }
                      else
                        {
                          int const tone1 = graymap[static_cast<size_t> (i1)];
                          int const tone2 = graymap[static_cast<size_t> (i2)];
                          int const tone3 = graymap[static_cast<size_t> (i3)];
                          current =
                              cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (tone1)] +
                              cs[static_cast<size_t> (ks)][static_cast<size_t> (tone2)] +
                              cs[static_cast<size_t> (ks + 1)][static_cast<size_t> (tone3)];
                          history = history_cs[(ks - 1) * 8 + tone1]
                                    + history_cs[ks * 8 + tone2]
                                    + history_cs[(ks + 1) * 8 + tone3];
                        }
                      // Con DECODIUM_FT8_STORICO_ENERGIA=1 lo storico porta la
                      // radice della somma delle energie passate, e qui si sommano
                      // le ENERGIE (|S|^2), non i moduli: e' il combinatore quadratico
                      // classico, misurato in FT2 entro 0,05 dB dalla verosimiglianza
                      // esatta. Il risultato resta in unita' di modulo per le forme
                      // di metrica a valle.
                      value = ft8_history_energy_mode ()
                                  ? std::sqrt (abs2 (current) + abs2 (history))
                                  : abs1 (current) + abs1 (history);
                      if (use_weak_transform)
                        {
                          value = std::pow (0.5f * value, 3.0f);
                        }
                      s2[static_cast<size_t> (i)] = value;
                    }
                  for (int bit = 0; bit <= ibmax; ++bit)
                    {
                      float max1 = -1.0e30f;
                      float max0 = -1.0e30f;
                      for (int i = 0; i < nt; ++i)
                        {
                          float const value = s2[static_cast<size_t> (i)];
                          if (one[static_cast<size_t> (i)][static_cast<size_t> (bit)]) max1 = std::max (max1, value);
                          else max0 = std::max (max0, value);
                        }
                      group_max1[static_cast<size_t> (bit)] = max1;
                      group_max0[static_cast<size_t> (bit)] = max0;
                    }
                }

              for (int ib = 0; ib <= ibmax; ++ib)
                {
                  if (i32 + ib > 174)
                    {
                      continue;
                    }

                  float const max1 = group_max1[static_cast<size_t> (ibmax - ib)];
                  float const max0 = group_max0[static_cast<size_t> (ibmax - ib)];
                  float const bm = max1 - max0;
                  int const out_index = i32 + ib - 1;

                  if (nsym == 1)
                    {
                      bmeta[static_cast<size_t> (out_index)] = bm;
                      float const den = std::max (max1, max0);
                      bmetd[static_cast<size_t> (out_index)] = den > 0.0f ? (bm / den) : 0.0f;
                    }
                  else if (nsym == 2)
                    {
                      bmetb[static_cast<size_t> (out_index)] = bm;
                    }
                  else
                    {
                      bmetc[static_cast<size_t> (out_index)] = bm;
                    }
                }
            }
        }
    }

  for (int i = 0; i < 174; ++i)
    {
      std::array<float, 3> const temp {{
          bmeta[static_cast<size_t> (i)],
          bmetb[static_cast<size_t> (i)],
          bmetc[static_cast<size_t> (i)]
        }};
      int best = 0;
      float best_abs = std::abs (temp[0]);
      for (int j = 1; j < 3; ++j)
        {
          float const current_abs = std::abs (temp[static_cast<size_t> (j)]);
          if (current_abs > best_abs)
            {
              best = j;
              best_abs = current_abs;
            }
        }
      bmete[static_cast<size_t> (i)] = temp[static_cast<size_t> (best)];
    }

  std::array<float, 174> bmet_reverse {};
  bool const use_reverse_extra = (imetric == 4);
  if (use_reverse_extra)
    {
      std::array<float, 174> rbmeta {};
      std::array<float, 174> rbmetb {};
      std::array<float, 174> rbmetc {};
      for (int nsym = 1; nsym <= 3; ++nsym)
        {
          for (int ihalf = 1; ihalf <= 2; ++ihalf)
            {
              for (int k = 1; k <= 29; k += nsym)
                {
                  int const ks = (ihalf == 1) ? (k + 7) : (k + 43);
                  int const i32 = 1 + (k - 1) * 3 + (ihalf - 1) * 87;
                  int ibmax = 2;
                  if (nsym == 2) ibmax = 5;
                  if (nsym == 3) ibmax = 8;
                  std::array<float, 9> group_max1 {};
                  std::array<float, 9> group_max0 {};
                  ftx_group_bit_maxima (cs_reverse, graymap, nsym, ks,
                                        group_max1.data (), group_max0.data (),
                                        maxima_work);
                  if (use_weak_transform)
                    {
                      for (int bit = 0; bit <= ibmax; ++bit)
                        {
                          group_max1[static_cast<size_t> (bit)] =
                              std::pow (0.5f * group_max1[static_cast<size_t> (bit)], 3.0f);
                          group_max0[static_cast<size_t> (bit)] =
                              std::pow (0.5f * group_max0[static_cast<size_t> (bit)], 3.0f);
                        }
                    }
                  for (int ib = 0; ib <= ibmax; ++ib)
                    {
                      if (i32 + ib > 174)
                        {
                          continue;
                        }
                      float const max1 = group_max1[static_cast<size_t> (ibmax - ib)];
                      float const max0 = group_max0[static_cast<size_t> (ibmax - ib)];
                      float const bm = max1 - max0;
                      int const out_index = i32 + ib - 1;
                      if (nsym == 1) rbmeta[static_cast<size_t> (out_index)] = bm;
                      else if (nsym == 2) rbmetb[static_cast<size_t> (out_index)] = bm;
                      else rbmetc[static_cast<size_t> (out_index)] = bm;
                    }
                }
            }
        }

      for (int i = 0; i < 174; ++i)
        {
          std::array<float, 3> const temp {{
              rbmeta[static_cast<size_t> (i)],
              rbmetb[static_cast<size_t> (i)],
              rbmetc[static_cast<size_t> (i)]
            }};
          int best = 0;
          float best_abs = std::abs (temp[0]);
          for (int j = 1; j < 3; ++j)
            {
              float const current_abs = std::abs (temp[static_cast<size_t> (j)]);
              if (current_abs > best_abs)
                {
                  best = j;
                  best_abs = current_abs;
                }
            }
          bmet_reverse[static_cast<size_t> (i)] = temp[static_cast<size_t> (best)];
        }
    }

  normalizebmet_rms_cpp (bmeta.data (), 174);
  normalizebmet_rms_cpp (bmetb.data (), 174);
  normalizebmet_rms_cpp (bmetc.data (), 174);
  normalizebmet_rms_cpp (bmetd.data (), 174);
  normalizebmet_rms_cpp (bmete.data (), 174);
  if (use_reverse_extra)
    {
      normalizebmet_rms_cpp (bmet_reverse.data (), 174);
    }

  for (int i = 0; i < 174; ++i)
    {
      llra[i] = scale * bmeta[static_cast<size_t> (i)];
      llrb[i] = scale * bmetb[static_cast<size_t> (i)];
      llrc[i] = scale * bmetc[static_cast<size_t> (i)];
      llrd[i] = scale * bmetd[static_cast<size_t> (i)];
      llre[i] = scale * (use_reverse_extra
                         ? bmet_reverse[static_cast<size_t> (i)]
                         : bmete[static_cast<size_t> (i)]);
    }
}

}

extern "C" void ftx_ft4_bitmetrics_c (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft4_bitmetrics (cd, bitmetrics, badsync);
}

extern "C" void ftx_ft4_bitmetrics_ref_c (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft4_bitmetrics (cd, bitmetrics, badsync);
}

extern "C" void ftx_ft2_bitmetrics_c (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft2_bitmetrics (cd, bitmetrics, badsync);
}

// Moduli dei 4 toni per ciascuno dei 103 simboli della finestra: mags[k*4+j].
// Serve alla conferma a livello di tono del messaggio atteso (Stage7, tipo 8):
// con la sequenza di toni nota, la statistica e' la somma non coerente del
// modulo sul tono atteso meno la media degli altri tre, sui simboli dati.
extern "C" void ftx_ft2_symbol_mags_c (Complex const* cd, float* mags)
{
  constexpr int NN = 103;
  constexpr int NSS = 32;
  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> m {};
      fft_symbol_4tones (cd + k * NSS, tones, m);
      for (int j = 0; j < 4; ++j)
        {
          mags[k * 4 + j] = m[static_cast<size_t> (j)];
        }
    }
}

// Accumulo fra slot (Stage7, DECODIUM_FT2_ACCUMULO). Spettri di simbolo e
// fattore di rumore di UNA finestra, da sommare a livello di IPOTESI con
// quelli di altri slot della stessa stazione. cs_out[k*4+j] = FFT del simbolo
// k sul tono j (103 x 4); beta_out = 0,5 / varianza di rumore, lo stesso beta
// che il demodulatore usa per scalare le energie.
// Bit-metrics COERENTI per FT8: la fase del canale stimata dai 21 simboli
// Costas (tre gruppi da 7 ai simboli k, k+36, k+72, toni {3,1,4,0,6,5,2}).
//
// Stessa idea della versione FT2 qui sotto, con le differenze del modo: 8
// ipotesi di tono invece di 4, max-log invece di log-sum-exp, tre blocchi di
// combinazione coerente (1, 2 e 3 simboli) invece di 1, 2 e 4, e la
// normalizzazione in RMS sui 174 bit invece che in sigma sulle 206 righe.
//
// Con la fase nota l'ipotesi si pesa sulla PROIEZIONE invece che sul modulo, e
// la proiezione puo' essere negativa: e' quella l'informazione in piu', perche'
// un tono in controfase e' meno probabile di uno a energia nulla.
//
// Tetto teorico su FT8: 1,27 dB (terne 3,96 contro 2,69 del limite coerente),
// piu' alto dell'1,1 di FT2. Come in FT2 va usata come attempt IN PIU', mai al
// posto delle altre: da sola peggiora a segnale forte.
//
// cs: [79][8] spettri per simbolo, gia' catturati da
// ftx_ft8_bitmetrics_capture_c. Uscite: cinque insiemi da 174, gia' scalati.
extern "C" void ftx_ft8_bitmetrics_coherent_c (Complex const* cs, float scale,
                                               float* llra, float* llrb, float* llrc,
                                               float* llrd, float* llre)
{
  static std::array<int, 8> const graymap {{0, 1, 3, 2, 5, 6, 4, 7}};
  static int const icos7[7] = {3, 1, 4, 0, 6, 5, 2};
  static int const gruppo[3] = {0, 36, 72};
  auto const& one = one9_table ();

  // Ancore di fase sui tre gruppi Costas.
  std::array<Complex, 3> ancora {};
  std::array<float, 3> centro {};
  for (int g = 0; g < 3; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 7; ++k)
        somma += cs[static_cast<size_t> ((gruppo[g] + k) * 8 + icos7[k])];
      float const mag = std::abs (somma);
      ancora[static_cast<size_t> (g)] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[static_cast<size_t> (g)] = static_cast<float> (gruppo[g]) + 3.0f;
    }
  auto rotazione = [&] (int simbolo) {
    float const x = static_cast<float> (simbolo);
    Complex rif;
    if (x <= centro[0]) rif = ancora[0];
    else if (x >= centro[2]) rif = ancora[2];
    else
      {
        int const g = (x > centro[1]) ? 1 : 0;
        float const t = (x - centro[static_cast<size_t> (g)])
                        / (centro[static_cast<size_t> (g + 1)] - centro[static_cast<size_t> (g)]);
        rif = ancora[static_cast<size_t> (g)] * (1.0f - t) + ancora[static_cast<size_t> (g + 1)] * t;
      }
    float const mag = std::abs (rif);
    return mag > 0.0f ? std::conj (rif / mag) : Complex {1.0f, 0.0f};
  };

  std::array<float, 174> bmeta {}, bmetb {}, bmetc {};
  for (int nsym = 1; nsym <= 3; ++nsym)
    {
      int const nt = 1 << (3 * nsym);
      for (int ihalf = 1; ihalf <= 2; ++ihalf)
        {
          for (int k = 1; k <= 29; k += nsym)
            {
              int const ks = (ihalf == 1) ? (k + 7) : (k + 43);
              Complex const rot = rotazione (ks - 1);
              std::array<float, 512> sp {};
              for (int i = 0; i < nt; ++i)
                {
                  int const i1 = (i >> 6) & 7, i2 = (i >> 3) & 7, i3 = i & 7;
                  Complex somma {};
                  if (nsym == 1)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  else if (nsym == 2)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i3)])];
                  else
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i1)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> ((ks + 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  sp[static_cast<size_t> (i)] = std::real (somma * rot);
                }

              int const i32 = 1 + (k - 1) * 3 + (ihalf - 1) * 87;
              int const ibmax = (nsym == 1 ? 2 : (nsym == 2 ? 5 : 8));
              for (int ib = 0; ib <= ibmax; ++ib)
                {
                  if (i32 + ib > 174) continue;
                  float max1 = -1.0e30f, max0 = -1.0e30f;
                  for (int i = 0; i < nt; ++i)
                    {
                      float const v = sp[static_cast<size_t> (i)];
                      if (one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)]) max1 = std::max (max1, v);
                      else max0 = std::max (max0, v);
                    }
                  int const out_index = i32 + ib - 1;
                  float const bm = max1 - max0;
                  if (nsym == 1) bmeta[static_cast<size_t> (out_index)] = bm;
                  else if (nsym == 2) bmetb[static_cast<size_t> (out_index)] = bm;
                  else bmetc[static_cast<size_t> (out_index)] = bm;
                }
            }
        }
    }

  normalizebmet_rms_cpp (bmeta.data (), 174);
  normalizebmet_rms_cpp (bmetb.data (), 174);
  normalizebmet_rms_cpp (bmetc.data (), 174);

  for (int i = 0; i < 174; ++i)
    {
      float const a = bmeta[static_cast<size_t> (i)];
      float const b = bmetb[static_cast<size_t> (i)];
      float const c = bmetc[static_cast<size_t> (i)];
      float primo = a, secondo = b;
      if (std::abs (b) > std::abs (primo)) { secondo = primo; primo = b; }
      else if (std::abs (b) > std::abs (secondo)) { secondo = b; }
      if (std::abs (c) > std::abs (primo)) { secondo = primo; primo = c; }
      else if (std::abs (c) > std::abs (secondo)) { secondo = c; }
      llra[i] = scale * a;
      llrb[i] = scale * b;
      llrc[i] = scale * c;
      llrd[i] = scale * primo;
      llre[i] = scale * secondo;
    }
}

// Bit-metrics FT8 con informazione a priori sugli ALTRI bit del gruppo
// (demodulazione iterativa, BICM-ID).
//
// PERCHE'. Il demodulatore scompone ogni gruppo di simboli in bit trattati come
// indipendenti; non lo sono, perche' condividono le stesse ipotesi di tono. Il
// calcolo dell'informazione mutua (lab/tools/fsk_gmi.py) misura quanto costa
// quella scomposizione: 0,93 dB su un simbolo, 1,45 su coppie, 1,63 su terne --
// e cresce proprio dove la produzione combina piu' simboli. Il max-log invece
// non c'entra (0,06-0,13 dB dalla metrica esatta): ottimizzare la formula
// sarebbe tempo buttato.
//
// COME. `la` porta l'estrinseca del decodificatore
// (superldpc_extrinsic174_91_c): per ogni ipotesi si somma alla metrica il
// contributo a priori di TUTTI i bit del gruppo TRANNE quello che si sta
// calcolando. Escluderlo e' cio' che rende l'uscita estrinseca, cioe' notizia
// nuova per il decodificatore e non l'eco della sua stessa opinione.
//
// LA SCALA CONTA. La metrica e' un modulo |cs|, l'a priori un LLR: sommarli
// direttamente vorrebbe dire pesare l'a priori a caso. Il fattore di
// conversione e' l'RMS della metrica grezza diviso `scale`, perche' l'uscita
// normalizzata vale scale*(max1-max0)/rms. Si ricava qui da una prima passata
// di max-log senza a priori, cosi' non dipende da cosa passa il chiamante.
// Gli |cs| delle ipotesi si calcolano UNA volta sola e si riusano nelle due
// passate: la parte cara sono quelli, non il max-log.
//
// I CINQUE INSIEMI sono quelli di run_ft8_bitmetrics e non altri: a, b, c sono
// le metriche a 1, 2 e 3 simboli; d e' quella a un simbolo divisa per il
// proprio livello max(max1,max0); e e' quella col modulo maggiore fra le tre.
// (Attenzione: NON e' la coppia "primo e secondo" della versione coerente.
// Con `la` nullo questa funzione riproduce la produzione bit per bit, e il
// banco lo verifica su tutti e cinque -- e' l'unico modo di sapere che
// l'innesto e' inerte da spento.)
//
// Come la passata coerente va usata come tentativo IN PIU', mai al posto degli
// altri. Misurato sul banco (tests/ft8_bicm_genie.cpp, solo il ramo a un
// simbolo): +21,3% di decodifiche, ~0,3 dB, zero falsi su 2236 cornici di
// rumore e sulle 960 prove con segnale. Il banco pero' confronta con la sola
// llra, mentre qui la produzione ha gia' cinque passate: il guadagno
// incrementale vero va misurato sulla catena, non dedotto da quel numero --
// e' l'errore che aveva gonfiato la passata coerente da +23% a +80%.
// Rapporto: lab/misure/20260910_bicm_id_ft8.md
//
// cs: [79][8] spettri per simbolo, gia' catturati da
// ftx_ft8_bitmetrics_capture_c (stesso ingresso della versione coerente).
// la: [174] convenzione Decodium (positivo = bit 1); nullo = nessuna.
// Uscite: cinque insiemi da 174, gia' scalati.
extern "C" void ftx_ft8_bitmetrics_bicm_c (Complex const* cs, float scale, float const* la,
                                           float* llra, float* llrb, float* llrc,
                                           float* llrd, float* llre)
{
  static std::array<int, 8> const graymap {{0, 1, 3, 2, 5, 6, 4, 7}};
  auto const& one = one9_table ();

  struct Gruppo
  {
    int nsym {0};
    int i32 {0};
    int ibmax {0};
    std::size_t off {0};
  };

  // Passata comune: i moduli delle ipotesi, una volta sola per ogni gruppo.
  std::vector<float> ipotesi;
  ipotesi.reserve (12700);                 // 58*8 + 30*64 + 20*512
  std::vector<Gruppo> gruppi;
  gruppi.reserve (110);
  for (int nsym = 1; nsym <= 3; ++nsym)
    {
      int const nt = 1 << (3 * nsym);
      int const ibmax = (nsym == 1 ? 2 : (nsym == 2 ? 5 : 8));
      for (int ihalf = 1; ihalf <= 2; ++ihalf)
        {
          for (int k = 1; k <= 29; k += nsym)
            {
              int const ks = (ihalf == 1) ? (k + 7) : (k + 43);
              std::size_t const off = ipotesi.size ();
              for (int i = 0; i < nt; ++i)
                {
                  int const i1 = i / 64, i2 = (i & 63) / 8, i3 = i & 7;
                  Complex somma {};
                  if (nsym == 1)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  else if (nsym == 2)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i3)])];
                  else
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i1)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> ((ks + 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  ipotesi.push_back (abs1 (somma));
                }
              gruppi.push_back (Gruppo {nsym, 1 + (k - 1) * 3 + (ihalf - 1) * 87, ibmax, off});
            }
        }
    }

  // max-log di un gruppo. Con `peso` non nullo ogni ipotesi porta in piu' il
  // contributo a priori degli altri bit del gruppo, gia' in unita' di metrica.
  // `dst_d` riceve la forma divisa per il livello, che esiste solo a un simbolo.
  auto max_log = [&] (Gruppo const& g, float const* peso, float const* proprio,
                      std::array<float, 174>& dst, std::array<float, 174>* dst_d) {
    int const nt = 1 << (3 * g.nsym);
    for (int ib = 0; ib <= g.ibmax; ++ib)
      {
        if (g.i32 + ib > 174) continue;
        float max1 = -1.0e30f, max0 = -1.0e30f;
        for (int i = 0; i < nt; ++i)
          {
            float v = ipotesi[g.off + static_cast<std::size_t> (i)];
            bool const uno = one[static_cast<size_t> (i)][static_cast<size_t> (g.ibmax - ib)];
            if (peso)
              {
                // il proprio bit non entra mai nel proprio a priori
                v += peso[i] - (uno ? proprio[ib] : 0.0f);
              }
            if (uno) max1 = std::max (max1, v);
            else max0 = std::max (max0, v);
          }
        std::size_t const out_index = static_cast<size_t> (g.i32 + ib - 1);
        float const bm = max1 - max0;
        dst[out_index] = bm;
        if (dst_d)
          {
            float const den = std::max (max1, max0);
            (*dst_d)[out_index] = den > 0.0f ? (bm / den) : 0.0f;
          }
      }
  };

  // Passata 1: senza a priori. Serve a due cose -- le metriche di partenza e,
  // dal loro RMS, il fattore che porta un LLR nelle unita' della metrica.
  std::array<float, 174> bmeta {}, bmetb {}, bmetc {}, bmetd {}, bmete {};
  for (Gruppo const& g : gruppi)
    max_log (g, nullptr, nullptr,
             g.nsym == 1 ? bmeta : (g.nsym == 2 ? bmetb : bmetc),
             g.nsym == 1 ? &bmetd : nullptr);

  if (la)
    {
      auto rms_di = [] (std::array<float, 174> const& v) {
        double s = 0.0;
        for (int i = 0; i < 174; ++i) s += static_cast<double> (v[static_cast<size_t> (i)]) * v[static_cast<size_t> (i)];
        double const r = std::sqrt (s / 174.0);
        return r > 0.0 ? static_cast<float> (r) : 1.0f;
      };
      float const conv[3] = {rms_di (bmeta) / scale, rms_di (bmetb) / scale, rms_di (bmetc) / scale};

      // Passata 2: le stesse ipotesi, ora pesate con l'estrinseca.
      std::array<float, 512> peso {};
      std::array<float, 9> proprio {};
      for (Gruppo const& g : gruppi)
        {
          int const nt = 1 << (3 * g.nsym);
          float const cv = conv[static_cast<size_t> (g.nsym - 1)];
          for (int ib = 0; ib <= g.ibmax; ++ib)
            {
              int const c = g.i32 + ib - 1;
              proprio[static_cast<size_t> (ib)] = (c < 174) ? cv * la[c] : 0.0f;
            }
          for (int i = 0; i < nt; ++i)
            {
              float somma = 0.0f;
              for (int ib = 0; ib <= g.ibmax; ++ib)
                if (one[static_cast<size_t> (i)][static_cast<size_t> (g.ibmax - ib)])
                  somma += proprio[static_cast<size_t> (ib)];
              peso[static_cast<size_t> (i)] = somma;
            }
          max_log (g, peso.data (), proprio.data (),
                   g.nsym == 1 ? bmeta : (g.nsym == 2 ? bmetb : bmetc),
                   g.nsym == 1 ? &bmetd : nullptr);
        }
    }

  // e = quella col modulo maggiore fra le tre, come in run_ft8_bitmetrics.
  for (int i = 0; i < 174; ++i)
    {
      std::array<float, 3> const temp {{bmeta[static_cast<size_t> (i)],
                                        bmetb[static_cast<size_t> (i)],
                                        bmetc[static_cast<size_t> (i)]}};
      int best = 0;
      float best_abs = std::abs (temp[0]);
      for (int j = 1; j < 3; ++j)
        {
          float const current_abs = std::abs (temp[static_cast<size_t> (j)]);
          if (current_abs > best_abs) { best = j; best_abs = current_abs; }
        }
      bmete[static_cast<size_t> (i)] = temp[static_cast<size_t> (best)];
    }

  normalizebmet_rms_cpp (bmeta.data (), 174);
  normalizebmet_rms_cpp (bmetb.data (), 174);
  normalizebmet_rms_cpp (bmetc.data (), 174);
  normalizebmet_rms_cpp (bmetd.data (), 174);
  normalizebmet_rms_cpp (bmete.data (), 174);

  for (int i = 0; i < 174; ++i)
    {
      llra[i] = scale * bmeta[static_cast<size_t> (i)];
      llrb[i] = scale * bmetb[static_cast<size_t> (i)];
      llrc[i] = scale * bmetc[static_cast<size_t> (i)];
      llrd[i] = scale * bmetd[static_cast<size_t> (i)];
      llre[i] = scale * bmete[static_cast<size_t> (i)];
    }
}

// Bit-metrics COERENTI: la fase del canale stimata dai simboli di
// sincronismo, invece di essere ignorata.
//
// PERCHE'. Il demodulatore di FT2 e' non coerente: usa l'energia dell'ipotesi
// di tono, perche' la fase del canale non si conosce. Ma la fase non e'
// l'informazione da decodificare, e' un parametro di disturbo -- e i parametri
// di disturbo si stimano. FT2 trasmette 16 simboli di sincronismo con toni
// NOTI (i quattro gruppi Costas ai simboli 0-3, 33-36, 66-69, 99-102): su
// quelli la fase si misura direttamente.
//
// COME. Un'ancora per gruppo (somma coerente dei suoi quattro simboli al tono
// giusto, per alzare il rapporto segnale-rumore della stima), poi
// interpolazione fra un'ancora e l'altra. L'interpolazione si fa sui vettori
// complessi unitari e non sugli angoli, cosi' non serve gestire i salti di
// 2 pi greco. Con la fase nota l'informazione sta nella parte reale e la
// quadratura e' solo rumore, quindi l'ipotesi si pesa sulla sua proiezione.
//
// COSA ASPETTARSI. Da sola questa metrica PEGGIORA a rapporto segnale-rumore
// alto, dove il non coerente va gia' bene e l'errore di stima butta via
// segnale. Va usata come passata IN PIU' accanto alle cinque cieche, mai al
// loro posto: misurato +23% di decodifiche e +6% di fantasmi contro le cinque
// passate di produzione (lab/misure/20260909_genio_ft8_e_fase.md).
//
// Uscita: 206 righe normalizzate, stesso formato di un piano di
// ftx_ft2_bitmetrics_c, da mappare con gli indici di build_llr_sets.
extern "C" void ftx_ft2_bitmetrics_coherent_c (Complex const* cd, float* rows_out)
{
  constexpr int NN = 103;
  constexpr int NSS = 32;
  constexpr int rows = 2 * NN;
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  static int const icos[4][4] = {{0,1,3,2},{1,0,2,3},{2,3,1,0},{3,2,0,1}};
  static int const gruppo[4] = {0, 33, 66, 99};

  std::array<std::array<Complex, 4>, NN> cs {};
  std::array<std::array<float, 4>, NN> pwr {};
  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + k * NSS, tones, mags, &power);
      cs[static_cast<size_t> (k)] = tones;
      pwr[static_cast<size_t> (k)] = power;
    }
  float const noise_var = ft2_legacy_base_noise_var (pwr);
  float const beta = clamp_value (0.5f / noise_var, 0.01f, 50.0f);

  // Ancore di fase sui quattro gruppi Costas.
  std::array<Complex, 4> ancora {};
  std::array<float, 4> centro {};
  for (int g = 0; g < 4; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 4; ++k)
        {
          somma += cs[static_cast<size_t> (gruppo[g] + k)][static_cast<size_t> (icos[g][k])];
        }
      float const mag = std::abs (somma);
      ancora[static_cast<size_t> (g)] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[static_cast<size_t> (g)] = static_cast<float> (gruppo[g]) + 1.5f;
    }

  std::array<float, rows> m {};
  for (int r = 0; r < rows; ++r)
    {
      int const s = r / 2;
      int const mio = 1 - (r % 2);

      float const x = static_cast<float> (s);
      Complex rif;
      if (x <= centro[0]) rif = ancora[0];
      else if (x >= centro[3]) rif = ancora[3];
      else
        {
          int g = 0;
          while (g < 3 && x > centro[static_cast<size_t> (g + 1)]) ++g;
          float const t = (x - centro[static_cast<size_t> (g)])
                          / (centro[static_cast<size_t> (g + 1)] - centro[static_cast<size_t> (g)]);
          rif = ancora[static_cast<size_t> (g)] * (1.0f - t)
                + ancora[static_cast<size_t> (g + 1)] * t;
        }
      float const rmag = std::abs (rif);
      Complex const rot = rmag > 0.0f ? std::conj (rif / rmag) : Complex {1.0f, 0.0f};

      float w[4];
      for (int i = 0; i < 4; ++i)
        {
          float const proj = std::real (cs[static_cast<size_t> (s)][static_cast<size_t> (graymap[static_cast<size_t> (i)])] * rot);
          w[i] = beta * proj * std::fabs (proj);
        }
      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) max1 = std::max (max1, w[i]);
          else max0 = std::max (max0, w[i]);
        }
      float lse1 = 0.0f, lse0 = 0.0f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) lse1 += decodium::ftx_expf (w[i] - max1);
          else lse0 += decodium::ftx_expf (w[i] - max0);
        }
      m[static_cast<size_t> (r)] = (max1 + std::log (std::max (lse1, kTiny)))
                                   - (max0 + std::log (std::max (lse0, kTiny)));
    }

  normalizebmet_cpp (m.data (), rows);
  std::copy_n (m.data (), rows, rows_out);
}

extern "C" void ftx_ft2_symbol_spectra_c (Complex const* cd, Complex* cs_out, float* beta_out)
{
  constexpr int NN = 103;
  constexpr int NSS = 32;
  std::array<std::array<float, 4>, NN> pwr {};
  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + k * NSS, tones, mags, &power);
      for (int j = 0; j < 4; ++j)
        {
          cs_out[k * 4 + j] = tones[static_cast<size_t> (j)];
        }
      pwr[static_cast<size_t> (k)] = power;
    }
  float const noise_var = ft2_legacy_base_noise_var (pwr);
  *beta_out = clamp_value (0.5f / noise_var, 0.01f, 50.0f);
}

// Bit-metrics dalla SOMMA di piu' slot. Stesse ipotesi a 1, 2 e 4 simboli del
// demodulatore (run_ft2_bitmetrics_impl, ramo non equalizzato), ma l'energia
// di ogni ipotesi e' sommata sugli slot, ciascuno col proprio beta: la somma
// coerente resta DENTRO lo slot (fase continua fra simboli vicini), fra slot
// diversi si sommano le energie (fase indipendente). E' la combinazione
// quadratica classica; il Monte Carlo (lab/tools/slot_accumulo.py) la da'
// entro 0,05 dB dalla verosimiglianza esatta. Con nslot = 1 riproduce il
// ramo base. cs = [nslot][103*4], beta = [nslot].
extern "C" void ftx_ft2_bitmetrics_accum_c (Complex const* cs, float const* beta, int nslot,
                                            float* bitmetrics)
{
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  constexpr int NN = 103;
  constexpr int rows = 2 * NN;
  std::fill (bitmetrics, bitmetrics + rows * 3, 0.0f);
  if (!cs || !beta || nslot <= 0)
    {
      return;
    }

  FtxLseWorkspace lse_work;
  for (int nseq = 1; nseq <= 3; ++nseq)
    {
      int const nsym = (nseq == 1 ? 1 : (nseq == 2 ? 2 : 4));
      int const nt = 1 << (2 * nsym);
      int const ibmax = (nsym == 1 ? 1 : (nsym == 2 ? 3 : 7));
      std::array<Complex, 256> sums {};
      std::array<float, 256> sp {};
      for (int ks = 1; ks <= NN - nsym + 1; ks += nsym)
        {
          std::fill_n (sp.begin (), nt, 0.0f);
          for (int s = 0; s < nslot; ++s)
            {
              Complex const* c = cs + static_cast<size_t> (s) * NN * 4;
              ftx_build_group_sums (
                  4, nsym,
                  [c, ks] (int symbol, int value) {
                    return c[(ks - 1 + symbol) * 4 + graymap[static_cast<size_t> (value)]];
                  },
                  sums.data ());
              for (int i = 0; i < nt; ++i)
                {
                  sp[static_cast<size_t> (i)] +=
                      (beta[s] / static_cast<float> (nsym)) * abs2 (sums[static_cast<size_t> (i)]);
                }
            }

          std::array<float, 8> group_metric {};
          ftx_group_bit_lse (sp.data (), nt, ibmax + 1, group_metric.data (), lse_work);

          int const ipt = 1 + (ks - 1) * 2;
          for (int ib = 0; ib <= ibmax; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }
              bm_at (bitmetrics, rows, ipt + ib - 1, nseq - 1) =
                  group_metric[static_cast<size_t> (ibmax - ib)];
            }
        }
    }

  normalizebmet_cpp (bitmetrics + rows * 0, rows);
  finalize_ft2_bitmetric_columns (bitmetrics, rows);
}

extern "C" void ftx_ft2_channel_est_c (Complex const* cd, Complex* cd_eq, float* ch_snr)
{
  if (!cd || !cd_eq || !ch_snr)
    {
      return;
    }

  ft2_channel_est_cpp (cd, cd_eq, ch_snr);
}

extern "C" void ftx_ft2_bitmetrics_diag_c (Complex const* cd,
                                           float* bitmetrics_final,
                                           float* bitmetrics_base,
                                           float* bmet_eq_raw,
                                           float* bmet_eq,
                                           Complex* cd_eq,
                                           float* ch_snr,
                                           int* badsync,
                                           int* use_cheq,
                                           float* snr_min,
                                           float* snr_max,
                                           float* snr_mean,
                                           float* fading_depth,
                                           float* noise_var,
                                           float* noise_var_eq)
{
  run_ft2_bitmetrics_impl (cd, bitmetrics_final, badsync,
                           true,
                           bitmetrics_base, bmet_eq_raw, bmet_eq, cd_eq, ch_snr, use_cheq,
                           snr_min, snr_max, snr_mean, fading_depth, noise_var, noise_var_eq);
}

// Phase-tracking bit metrics for FT8: forward-backward (BCJR) over a
// quantised carrier-phase x residual-frequency trellis.
//
// FT8 is continuous-phase 8-GFSK with h = 1: in the symbol-spaced spectra
// cs[79][8] the transmitted tone of every symbol carries the SAME carrier
// phase, up to the slowly varying channel phase and a residual frequency
// error. The blind metrics throw that away (|sum| over 1..3 symbols); the
// Costas-anchored coherent metrics interpolate the phase between three
// anchors only. Here the phase is a hidden Markov state:
//
//   phase[k+1] = phase[k] + 2*pi*df[k]*Tsym + w,   w ~ N(0, sigma^2)
//   df[k+1]    = df[k] or a neighbouring frequency state (prob. freq_walk)
//
// with NP phase states and NF frequency states kDfHz apart, i.e. the residual
// frequency error left by the 0.5 Hz sync grid (and a slow drift) is tracked
// inside the trellis instead of being searched beforehand. The 21 Costas
// symbols are pilots (tone known), data symbols have a uniform tone prior.
// Amplitude and noise variance are estimated from the data: noise from the
// 147 off-tone bins of the Costas symbols, amplitude from the Costas tones,
// then optionally re-estimated from the posteriors (em_rounds).
//
// Like the coherent and BICM metrics this is an EXTRA attempt, never a
// replacement of the blind ones.
//
// cs: [79][8] symbol spectra from ftx_ft8_bitmetrics_capture_c (not tone
// equalised). llr: [174], Decodium convention (positive = bit 1), RMS
// normalised and multiplied by `scale` like the other FT8 metrics.
// info (optional, 4 floats): RMS of the unnormalised LLRs, mean residual
// frequency (Hz), estimated Es/N0 (linear), log-likelihood of the frame.
// min_esn0: the attempt is skipped (return 0) when the pilot estimate of
// Es/N0 is below this value; most sync candidates are noise and stop here.
// Returns 1 on success, 0 when skipped or no usable estimate exists (llr zeroed).
extern "C" int ftx_ft8_bitmetrics_trellis_c (Complex const* cs, float scale,
                                             float sigma_deg, float freq_walk,
                                             int em_rounds, float min_esn0,
                                             float* llr, float* info)
{
  constexpr int NN = 79;
  constexpr int NP = 16;
  constexpr int NF = 15;
  constexpr int kMaxTaps = 24;
  constexpr float kDfHz = 0.08f;
  constexpr float kTsym = 0.16f;
  constexpr float kTwoPi = 6.28318530717958647692f;
  static int const icos7[7] = {3, 1, 4, 0, 6, 5, 2};
  static int const graymap[8] = {0, 1, 3, 2, 5, 6, 4, 7};
  static std::array<float, NP> const cos_p = [] {
    std::array<float, NP> v {};
    for (int p = 0; p < NP; ++p) v[static_cast<size_t> (p)] = std::cos (kTwoPi * static_cast<float> (p) / NP);
    return v;
  }();
  static std::array<float, NP> const sin_p = [] {
    std::array<float, NP> v {};
    for (int p = 0; p < NP; ++p) v[static_cast<size_t> (p)] = std::sin (kTwoPi * static_cast<float> (p) / NP);
    return v;
  }();

  std::fill_n (llr, 174, 0.0f);
  if (info) std::fill_n (info, 4, 0.0f);

  std::array<int, NN> pilot;
  pilot.fill (-1);
  for (int k = 0; k < 7; ++k)
    {
      pilot[static_cast<size_t> (k)] = icos7[k];
      pilot[static_cast<size_t> (k + 36)] = icos7[k];
      pilot[static_cast<size_t> (k + 72)] = icos7[k];
    }

  // First estimates from the pilots alone.
  double noise_sum = 0.0;
  double pilot_sum = 0.0;
  double total_sum = 0.0;
  for (int k = 0; k < NN; ++k)
    {
      for (int t = 0; t < 8; ++t)
        {
          double const pw = std::norm (cs[k * 8 + t]);
          total_sum += pw;
          if (pilot[static_cast<size_t> (k)] < 0) continue;
          if (t == pilot[static_cast<size_t> (k)]) pilot_sum += pw;
          else noise_sum += pw;
        }
    }
  float sigma2 = static_cast<float> (noise_sum / 147.0);
  if (!(sigma2 > 0.0f) || !std::isfinite (sigma2))
    {
      return 0;
    }
  float amp2 = static_cast<float> (pilot_sum / 21.0) - sigma2;
  if (amp2 < min_esn0 * sigma2)
    {
      return 0;           // cheap gate: the pilots show no usable signal
    }

  // Transition kernels: for each frequency state the probability mass of
  // N(advance, sigma^2) falling in each phase bin.
  float const sigma_bins = std::max (sigma_deg, 1.0f) * NP / 360.0f;
  std::array<std::array<float, kMaxTaps>, NF> taps {};
  std::array<int, NF> tap_lo {};
  std::array<int, NF> tap_n {};
  for (int f = 0; f < NF; ++f)
    {
      float const adv = static_cast<float> (f - NF / 2) * kDfHz * kTsym * NP;   // bins per symbol
      int const lo = static_cast<int> (std::floor (adv - 2.5f * sigma_bins));
      int const hi = static_cast<int> (std::ceil (adv + 2.5f * sigma_bins));
      int const n = std::min (hi - lo + 1, kMaxTaps);
      float sum = 0.0f;
      for (int j = 0; j < n; ++j)
        {
          float const a = (static_cast<float> (lo + j) - 0.5f - adv) / (sigma_bins * 1.41421356f);
          float const b = (static_cast<float> (lo + j) + 0.5f - adv) / (sigma_bins * 1.41421356f);
          float const w = 0.5f * (std::erf (b) - std::erf (a));
          taps[static_cast<size_t> (f)][static_cast<size_t> (j)] = w;
          sum += w;
        }
      for (int j = 0; j < n; ++j) taps[static_cast<size_t> (f)][static_cast<size_t> (j)] /= sum;
      tap_lo[static_cast<size_t> (f)] = lo;
      tap_n[static_cast<size_t> (f)] = n;
    }
  float const eps = std::max (0.0f, std::min (freq_walk, 0.3f));
  auto stay = [eps] (int f) {
    return 1.0f - eps * ((f > 0 ? 1.0f : 0.0f) + (f < NF - 1 ? 1.0f : 0.0f));
  };

  static thread_local std::vector<float> e_store;
  static thread_local std::vector<float> lk_store;
  static thread_local std::vector<float> alpha_store;
  e_store.resize (static_cast<size_t> (NN) * NP * 8);
  lk_store.resize (static_cast<size_t> (NN) * NP);
  alpha_store.resize (static_cast<size_t> (NN) * NF * NP);
  float* const e = e_store.data ();
  float* const lk = lk_store.data ();
  float* const alpha = alpha_store.data ();

  std::array<float, 174> raw {};
  double loglik = 0.0;
  double freq_mean = 0.0;
  for (int round = 0; round <= std::max (0, em_rounds); ++round)
    {
      // Es/N0 kept inside a sane window: below -6 dB there is nothing to
      // track, above 16 dB the blind metrics do not need help.
      amp2 = std::max (0.25f * sigma2, std::min (amp2, 40.0f * sigma2));
      float const amp = std::sqrt (amp2);
      float const c = 2.0f * amp / sigma2;

      loglik = 0.0;
      for (int k = 0; k < NN; ++k)
        {
          int const pt = pilot[static_cast<size_t> (k)];
          float ref = 0.0f;
          if (pt >= 0) ref = std::abs (cs[k * 8 + pt]);
          else
            for (int t = 0; t < 8; ++t) ref = std::max (ref, std::abs (cs[k * 8 + t]));
          float const offset = c * ref;
          loglik += offset;
          float* const ek = e + static_cast<size_t> (k) * NP * 8;
          float* const lkk = lk + static_cast<size_t> (k) * NP;
          std::fill_n (lkk, NP, 1.0e-30f);
          for (int t = 0; t < 8; ++t)
            {
              float const yr = c * cs[k * 8 + t].real ();
              float const yi = c * cs[k * 8 + t].imag ();
              float* const et = ek + t * NP;
              for (int p = 0; p < NP; ++p)
                {
                  float const x = yr * cos_p[static_cast<size_t> (p)] + yi * sin_p[static_cast<size_t> (p)] - offset;
                  et[p] = decodium::ftx_expf (std::min (x, 80.0f));
                }
              if (pt < 0)
                for (int p = 0; p < NP; ++p) lkk[p] += 0.125f * et[p];
              else if (t == pt)
                for (int p = 0; p < NP; ++p) lkk[p] += et[p];
            }
        }

      // Forward pass.
      std::array<std::array<float, NP>, NF> conv {};
      std::array<float, 3 * NP> ext {};
      auto convolve = [&] (float const* in, int f, bool forward, float* out) {
        for (int p = 0; p < NP; ++p)
          {
            ext[static_cast<size_t> (p)] = in[p];
            ext[static_cast<size_t> (p + NP)] = in[p];
            ext[static_cast<size_t> (p + 2 * NP)] = in[p];
          }
        int const lo = tap_lo[static_cast<size_t> (f)];
        int const n = tap_n[static_cast<size_t> (f)];
        float const* w = taps[static_cast<size_t> (f)].data ();
        // forward: out[p] = sum_j w[j] * in[p - (lo + j)]
        // backward: out[p] = sum_j w[j] * in[p + (lo + j)]
        std::fill_n (out, NP, 0.0f);
        for (int j = 0; j < n; ++j)
          {
            float const* const src = ext.data () + (forward ? NP - lo - j : NP + lo + j);
            float const wj = w[j];
            for (int p = 0; p < NP; ++p) out[p] += wj * src[p];
          }
      };
      for (int i = 0; i < NF * NP; ++i) alpha[i] = 1.0f / static_cast<float> (NF * NP);
      bool ok = true;
      for (int k = 0; k + 1 < NN && ok; ++k)
        {
          float const* const ak = alpha + static_cast<size_t> (k) * NF * NP;
          float* const an = alpha + static_cast<size_t> (k + 1) * NF * NP;
          float const* const lkk = lk + static_cast<size_t> (k) * NP;
          std::array<float, NP> g {};
          for (int f = 0; f < NF; ++f)
            {
              for (int p = 0; p < NP; ++p) g[static_cast<size_t> (p)] = ak[f * NP + p] * lkk[p];
              convolve (g.data (), f, true, conv[static_cast<size_t> (f)].data ());
            }
          float sum = 0.0f;
          for (int f = 0; f < NF; ++f)
            {
              float const s0 = stay (f);
              for (int p = 0; p < NP; ++p)
                {
                  float v = s0 * conv[static_cast<size_t> (f)][static_cast<size_t> (p)];
                  if (f > 0) v += eps * conv[static_cast<size_t> (f - 1)][static_cast<size_t> (p)];
                  if (f < NF - 1) v += eps * conv[static_cast<size_t> (f + 1)][static_cast<size_t> (p)];
                  an[f * NP + p] = v;
                  sum += v;
                }
            }
          if (!(sum > 0.0f) || !std::isfinite (sum)) { ok = false; break; }
          float const inv = 1.0f / sum;
          for (int i = 0; i < NF * NP; ++i) an[i] *= inv;
          loglik += std::log (sum);
        }
      if (!ok)
        {
          return 0;
        }

      // Backward pass, posteriors and bit metrics on the way.
      std::array<std::array<float, NP>, NF> beta {};
      std::array<std::array<float, NP>, NF> hmix {};
      for (auto& row : beta) row.fill (1.0f);
      double em_num = 0.0;
      freq_mean = 0.0;
      for (int k = NN - 1; k >= 0; --k)
        {
          float const* const ak = alpha + static_cast<size_t> (k) * NF * NP;
          float const* const ek = e + static_cast<size_t> (k) * NP * 8;
          float const* const lkk = lk + static_cast<size_t> (k) * NP;
          int const pt = pilot[static_cast<size_t> (k)];
          std::array<float, NP> wp {};
          float fnum = 0.0f;
          float fden = 0.0f;
          for (int f = 0; f < NF; ++f)
            {
              float fs = 0.0f;
              for (int p = 0; p < NP; ++p)
                {
                  float const v = ak[f * NP + p] * beta[static_cast<size_t> (f)][static_cast<size_t> (p)];
                  wp[static_cast<size_t> (p)] += v;
                  fs += v * lkk[p];
                }
              fnum += fs * static_cast<float> (f - NF / 2);
              fden += fs;
            }
          if (!(fden > 0.0f) || !std::isfinite (fden))
            {
              return 0;
            }
          if (k == NN - 1) loglik += std::log (fden);
          freq_mean += static_cast<double> (kDfHz * fnum / fden) / NN;

          std::array<float, 8> app {};
          float app_sum = 0.0f;
          float proj = 0.0f;
          for (int t = 0; t < 8; ++t)
            {
              if (pt >= 0 && t != pt) continue;
              float const yr = cs[k * 8 + t].real ();
              float const yi = cs[k * 8 + t].imag ();
              float const* const et = ek + t * NP;
              float v = 0.0f;
              float pr = 0.0f;
              for (int p = 0; p < NP; ++p)
                {
                  float const w = wp[static_cast<size_t> (p)] * et[p];
                  v += w;
                  pr += w * (yr * cos_p[static_cast<size_t> (p)] + yi * sin_p[static_cast<size_t> (p)]);
                }
              app[static_cast<size_t> (t)] = v;
              app_sum += v;
              proj += pr;
            }
          if (app_sum > 0.0f) em_num += static_cast<double> (proj / app_sum);

          if (pt < 0)
            {
              int const data_index = k < 36 ? k - 7 : k - 14;       // 0..57
              for (int ib = 0; ib < 3; ++ib)
                {
                  float p1 = 1.0e-30f;
                  float p0 = 1.0e-30f;
                  for (int i = 0; i < 8; ++i)
                    {
                      float const v = app[static_cast<size_t> (graymap[i])];
                      if ((i >> (2 - ib)) & 1) p1 += v;
                      else p0 += v;
                    }
                  raw[static_cast<size_t> (3 * data_index + ib)] = std::log (p1) - std::log (p0);
                }
            }

          if (k == 0) break;
          for (int f = 0; f < NF; ++f)
            for (int p = 0; p < NP; ++p)
              hmix[static_cast<size_t> (f)][static_cast<size_t> (p)] =
                  lkk[p] * beta[static_cast<size_t> (f)][static_cast<size_t> (p)];
          float sum = 0.0f;
          for (int f = 0; f < NF; ++f)
            {
              float const s0 = stay (f);
              std::array<float, NP> mixed {};
              for (int p = 0; p < NP; ++p)
                {
                  float v = s0 * hmix[static_cast<size_t> (f)][static_cast<size_t> (p)];
                  if (f > 0) v += eps * hmix[static_cast<size_t> (f - 1)][static_cast<size_t> (p)];
                  if (f < NF - 1) v += eps * hmix[static_cast<size_t> (f + 1)][static_cast<size_t> (p)];
                  mixed[static_cast<size_t> (p)] = v;
                }
              convolve (mixed.data (), f, false, beta[static_cast<size_t> (f)].data ());
              for (int p = 0; p < NP; ++p) sum += beta[static_cast<size_t> (f)][static_cast<size_t> (p)];
            }
          if (!(sum > 0.0f) || !std::isfinite (sum))
            {
              return 0;
            }
          float const inv = static_cast<float> (NF * NP) / sum;
          for (auto& row : beta)
            for (float& v : row) v *= inv;
        }

      if (round < em_rounds)
        {
          // Re-estimate amplitude and noise from the posteriors.
          float const amp_new = static_cast<float> (em_num / NN);
          amp2 = amp_new > 0.0f ? amp_new * amp_new : 0.0f;
          sigma2 = std::max (static_cast<float> ((total_sum - NN * static_cast<double> (amp2)) / (NN * 8.0)),
                             0.25f * sigma2);
        }
    }

  double sum2 = 0.0;
  for (float v : raw) sum2 += static_cast<double> (v) * v;
  float const rms = static_cast<float> (std::sqrt (sum2 / 174.0));
  if (!(rms > 0.0f) || !std::isfinite (rms))
    {
      return 0;
    }
  for (int i = 0; i < 174; ++i) llr[i] = scale * raw[static_cast<size_t> (i)] / rms;
  if (info)
    {
      info[0] = rms;
      info[1] = static_cast<float> (freq_mean);
      info[2] = amp2 / sigma2;
      info[3] = static_cast<float> (loglik);
    }
  return 1;
}

extern "C" void ftx_ft8_bitmetrics_c (Complex const* cd0, int np2, int ibest, int imetric,
                                      float* s8_out, int* nsync_out,
                                      float* llra, float* llrb, float* llrc,
                                      float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, 3.2f, false, false,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_scaled_c (Complex const* cd0, int np2, int ibest, int imetric,
                                             float scale,
                                             float* s8_out, int* nsync_out,
                                             float* llra, float* llrb, float* llrc,
                                             float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, false, false,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_deep_c (Complex const* cd0, int np2, int ibest, int imetric,
                                            float scale,
                                            float* s8_out, int* nsync_out,
                                            float* llra, float* llrb, float* llrc,
                                            float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, true, false,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_equalized_c (Complex const* cd0, int np2, int ibest, int imetric,
                                                float scale,
                                                float* s8_out, int* nsync_out,
                                                float* llra, float* llrb, float* llrc,
                                                float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, false, true,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_deep_equalized_c (Complex const* cd0, int np2, int ibest,
                                                     int imetric, float scale,
                                                     float* s8_out, int* nsync_out,
                                                     float* llra, float* llrb,
                                                     float* llrc, float* llrd,
                                                     float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, true, true,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_capture_c (Complex const* cd0, int np2, int ibest,
                                               int imetric, float scale,
                                               int weak_deep, int equalize_tone_power,
                                               Complex const* history_cs,
                                               Complex* current_cs_out,
                                               float* s8_out, int* nsync_out,
                                               float* llra, float* llrb,
                                               float* llrc, float* llrd,
                                               float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale,
                      weak_deep != 0, equalize_tone_power != 0,
                      history_cs, current_cs_out,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_a8_score_c (Complex const* cd, int nzz, float tbest, int const* itone_best,
                                    float* plog, int* nhard, float* sigobig)
{
  if (!cd || !itone_best || !plog || !nhard || !sigobig)
    {
      return;
    }

  constexpr int NN = 79;
  constexpr int NSS = 32;

  float plog_value = 0.0f;
  int nhard_value = 0;
  int nsum = 0;
  float sum_sync = 0.0f;
  float sum_sig = 0.0f;
  float sum_big = 0.0f;
  int const offset = static_cast<int> (std::lround ((tbest + 0.5f) / 0.005f));

  for (int k = 0; k < NN; ++k)
    {
      int const i0 = NSS * k + offset;
      std::array<Complex, NSS> csymb {};
      for (int i = 0; i < NSS; ++i)
        {
          int const index = i0 + i;
          if (index >= 0 && index < nzz)
            {
              csymb[static_cast<size_t> (i)] = cd[index];
            }
        }

      std::array<Complex, 8> tones {};
      std::array<float, 8> mags {};
      fft_symbol_8tones_ft8 (csymb.data (), tones, mags);

      std::array<float, 8> power {};
      float sum = 0.0f;
      for (int tone = 0; tone < 8; ++tone)
        {
          power[static_cast<size_t> (tone)] = mags[static_cast<size_t> (tone)] * mags[static_cast<size_t> (tone)];
          sum += power[static_cast<size_t> (tone)];
        }

      int const target_tone = std::max (0, std::min (itone_best[k], 7));
      if (sum > 0.0f)
        {
          float const p = power[static_cast<size_t> (target_tone)] / sum;
          plog_value += std::log (std::max (p, kTiny));
          ++nsum;
        }

      int const ipk = static_cast<int> (std::max_element (power.begin (), power.end ()) - power.begin ());
      if (ipk != target_tone)
        {
          ++nhard_value;
        }

      if (k <= 6 || (k >= 36 && k <= 42) || k >= 72)
        {
          sum_sync += power[static_cast<size_t> (target_tone)];
        }
      else
        {
          sum_sig += power[static_cast<size_t> (target_tone)];
        }
      sum_big += power[static_cast<size_t> (ipk)];
    }

  if (nsum < NN)
    {
      plog_value += static_cast<float> (NN - nsum) * std::log (0.125f);
    }

  *plog = plog_value;
  *nhard = nhard_value;
  *sigobig = sum_big > 0.0f ? (sum_sync + sum_sig) / sum_big : 0.0f;
}
