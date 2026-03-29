#include "FtxWaveformGenerator.hpp"

#include "FtxMessageEncoder.hpp"
#include "commons.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>

#include <fftw3.h>

#include "wsjtx_config.h"

extern "C" {
struct FoxCom3
{
  int nslots2;
  char cmsg2[5][40];
  int itone3[151];
};

extern FoxCom3 foxcom3_;
}

namespace
{
constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kFt2FoxNsym = 103;
constexpr int kFt2FoxNsps = 4 * 288;
constexpr int kFt2FoxNwave = (kFt2FoxNsym + 2) * kFt2FoxNsps;
constexpr int kFt2FoxNfft = 614400;
constexpr int kFt8FoxNsym = 79;
constexpr int kFt8FoxNsps = 4 * 1920;
constexpr int kFt8FoxNwave = kFt8FoxNsym * kFt8FoxNsps;
constexpr int kFt8FoxNfft = 614400;

int clamp_int (int value, int low, int high)
{
  if (value < low)
    {
      return low;
    }
  if (value > high)
    {
      return high;
    }
  return value;
}

double gfsk_pulse (double bt, double t)
{
  double const c = 0.5 * kTwoPi * std::sqrt (2.0 / std::log (2.0));
  return 0.5 * (std::erf (c * bt * (t + 0.5)) - std::erf (c * bt * (t - 0.5)));
}

double wrap_phase (double phase)
{
  phase = std::fmod (phase, kTwoPi);
  if (phase < 0.0)
    {
      phase += kTwoPi;
    }
  return phase;
}

QVector<float> generate_ft2_ft4_wave (int const* itone, int nsym, int nsps, float fsample, float f0)
{
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f)
    {
      return {};
    }

  QVector<double> pulse (3 * nsps);
  for (int i = 0; i < pulse.size (); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[i] = gfsk_pulse (1.0, tt);
    }

  QVector<double> dphi ((nsym + 2) * nsps, 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[ib + i] += dphi_peak * pulse[i] * double (itone[j]);
        }
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  QVector<float> wave ((nsym + 2) * nsps, 0.0f);
  double phi = 0.0;
  for (int i = 0; i < wave.size (); ++i)
    {
      wave[i] = std::sin (phi);
      phi = wrap_phase (phi + dphi[i] + carrier_step);
    }

  for (int i = 0; i < nsps; ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
      wave[i] = float (wave[i] * env);
    }

  int const tail_start = (nsym + 1) * nsps;
  for (int i = 0; i < nsps; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
      wave[tail_start + i] = float (wave[tail_start + i] * env);
    }

  return wave;
}

void generate_ft2_ft4_complex_wave (int const* itone, int nsym, int nsps, float fsample, float f0,
                                    std::complex<float>* cwave, int nwave)
{
  if (!cwave || nwave <= 0)
    {
      return;
    }

  std::fill_n (cwave, nwave, std::complex<float> {0.0f, 0.0f});
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f)
    {
      return;
    }

  QVector<double> pulse (3 * nsps);
  for (int i = 0; i < pulse.size (); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[i] = gfsk_pulse (1.0, tt);
    }

  QVector<double> dphi ((nsym + 2) * nsps, 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[ib + i] += dphi_peak * pulse[i] * double (itone[j]);
        }
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  double phi = 0.0;
  int const samples = std::min (nwave, (nsym + 2) * nsps);
  for (int i = 0; i < samples; ++i)
    {
      cwave[i] = std::complex<float> {float (std::cos (phi)), float (std::sin (phi))};
      phi = wrap_phase (phi + dphi[i] + carrier_step);
    }

  for (int i = 0; i < std::min (nsps, samples); ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
      cwave[i] *= float (env);
    }

  int const tail_start = (nsym + 1) * nsps;
  if (tail_start < samples)
    {
      for (int i = 0; i < nsps && tail_start + i < samples; ++i)
        {
          double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
          cwave[tail_start + i] *= float (env);
        }
    }
}

QVector<float> generate_ft8_wave (int const* itone, int nsym, int nsps, float bt, float fsample, float f0)
{
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f)
    {
      return {};
    }

  QVector<double> pulse (3 * nsps);
  for (int i = 0; i < pulse.size (); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[i] = gfsk_pulse (double (bt), tt);
    }

  QVector<double> dphi ((nsym + 2) * nsps, 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[ib + i] += dphi_peak * pulse[i] * double (itone[j]);
        }
    }

  for (int i = 0; i < 2 * nsps; ++i)
    {
      dphi[i] += dphi_peak * double (itone[0]) * pulse[nsps + i];
      dphi[nsym * nsps + i] += dphi_peak * double (itone[nsym - 1]) * pulse[i];
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  QVector<float> wave (nsym * nsps, 0.0f);
  double phi = 0.0;
  for (int i = 0; i < wave.size (); ++i)
    {
      wave[i] = std::sin (phi);
      phi = wrap_phase (phi + dphi[nsps + i] + carrier_step);
    }

  int const nramp = std::max (1, int (std::lround (double (nsps) / 8.0)));
  for (int i = 0; i < nramp; ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      wave[i] = float (wave[i] * env);
    }

  int const tail_start = nsym * nsps - nramp;
  for (int i = 0; i < nramp; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      wave[tail_start + i] = float (wave[tail_start + i] * env);
    }

  return wave;
}

void generate_q65_complex_wave (int const* itone, int nsym, double fsample, double f0,
                                double dfgen, std::complex<float>* cwave, int nwave)
{
  if (!itone || !cwave || nsym <= 0 || fsample <= 0.0 || nwave <= 0)
    {
      return;
    }

  double const dt = 1.0 / fsample;
  double const tsym = 7200.0 / 12000.0;
  double phi = 0.0;
  double dphi = kTwoPi * dt * f0;
  double t = 0.0;
  int current_symbol = 0;
  for (int i = 0; i < nwave; ++i)
    {
      t += dt;
      int symbol = static_cast<int> (t / tsym + 1.0);
      symbol = clamp_int (symbol, 1, nsym);
      if (symbol != current_symbol)
        {
          dphi = kTwoPi * dt * (f0 + double (itone[symbol - 1]) * dfgen);
          current_symbol = symbol;
        }
      phi = wrap_phase (phi + dphi);
      cwave[i] = std::complex<float> {float (std::cos (phi)), float (-std::sin (phi))};
    }
}

extern "C" {
struct FoxCom2
{
  int itone2[kFt8FoxNsym];
  signed char msgbits2[77];
};

FoxCom2 foxcom2_;

void genq65_ (char* msg0, int* ichk, char* msgsent, int* itone, int* i3, int* n3,
              fortran_charlen_t len1, fortran_charlen_t len2);
}

void normalize_wave (QVector<float>& wave)
{
  float peak = 0.0f;
  for (float sample : wave)
    {
      peak = std::max (peak, std::abs (sample));
    }
  if (peak <= 0.0f)
    {
      return;
    }
  for (float& sample : wave)
    {
      sample /= peak;
    }
}

void fox_bandlimit_inplace (QVector<float>& wave, int nslots, int nfreq, float width)
{
  if (wave.isEmpty () || nslots <= 0 || width <= 0.0f)
    {
      return;
    }

  std::vector<float> time_domain (static_cast<size_t> (kFt8FoxNfft), 0.0f);
  int const copy_samples = std::min (wave.size (), kFt8FoxNwave);
  for (int i = 0; i < copy_samples; ++i)
    {
      time_domain[static_cast<size_t> (i)] = wave.at (i);
    }

  int const nh = kFt8FoxNfft / 2;
  std::vector<std::array<float, 2>> spectrum (static_cast<size_t> (nh + 1));
  auto* fft_spectrum = reinterpret_cast<fftwf_complex*> (spectrum.data ());
  fftwf_plan forward = fftwf_plan_dft_r2c_1d (kFt8FoxNfft, time_domain.data (), fft_spectrum, FFTW_ESTIMATE);
  fftwf_plan inverse = fftwf_plan_dft_c2r_1d (kFt8FoxNfft, fft_spectrum, time_domain.data (), FFTW_ESTIMATE);
  if (!forward || !inverse)
    {
      if (forward) fftwf_destroy_plan (forward);
      if (inverse) fftwf_destroy_plan (inverse);
      return;
    }

  fftwf_execute (forward);

  float const df = 48000.0f / static_cast<float> (kFt8FoxNfft);
  float const fa = static_cast<float> (nfreq) - 0.5f * 6.25f;
  float const fb = static_cast<float> (nfreq) + 7.5f * 6.25f + static_cast<float> (nslots - 1) * 60.0f;
  int const ia2 = clamp_int (static_cast<int> (std::lround (fa / df)), 0, nh);
  int const ib1 = clamp_int (static_cast<int> (std::lround (fb / df)), 0, nh);
  int const ia1 = clamp_int (static_cast<int> (std::lround ((fa - width) / df)), 0, ia2);
  int const ib2 = clamp_int (static_cast<int> (std::lround ((fb + width) / df)), ib1, nh);

  float const pi = static_cast<float> (std::acos (-1.0));
  for (int i = ia1; i <= ia2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ia2) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = ib1; i <= ib2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ib1) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = 0; i < ia1; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }
  for (int i = ib2 + 1; i <= nh; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }

  fftwf_execute (inverse);
  fftwf_destroy_plan (forward);
  fftwf_destroy_plan (inverse);

  float const scale = 1.0f / static_cast<float> (kFt8FoxNfft);
  for (int i = 0; i < copy_samples; ++i)
    {
      wave[i] = time_domain[static_cast<size_t> (i)] * scale;
    }
}

void ft2_fox_bandlimit_inplace (QVector<float>& wave, int nslots, int nfreq, float width)
{
  if (wave.isEmpty () || nslots <= 0 || width <= 0.0f)
    {
      return;
    }

  std::vector<float> time_domain (static_cast<size_t> (kFt2FoxNfft), 0.0f);
  int const copy_samples = std::min (wave.size (), kFt2FoxNwave);
  for (int i = 0; i < copy_samples; ++i)
    {
      time_domain[static_cast<size_t> (i)] = wave.at (i);
    }

  int const nh = kFt2FoxNfft / 2;
  std::vector<std::array<float, 2>> spectrum (static_cast<size_t> (nh + 1));
  auto* fft_spectrum = reinterpret_cast<fftwf_complex*> (spectrum.data ());
  fftwf_plan forward = fftwf_plan_dft_r2c_1d (kFt2FoxNfft, time_domain.data (), fft_spectrum, FFTW_ESTIMATE);
  fftwf_plan inverse = fftwf_plan_dft_c2r_1d (kFt2FoxNfft, fft_spectrum, time_domain.data (), FFTW_ESTIMATE);
  if (!forward || !inverse)
    {
      if (forward) fftwf_destroy_plan (forward);
      if (inverse) fftwf_destroy_plan (inverse);
      return;
    }

  fftwf_execute (forward);

  float const df = 48000.0f / static_cast<float> (kFt2FoxNfft);
  float const baud = 48000.0f / static_cast<float> (kFt2FoxNsps);
  float const fa = static_cast<float> (nfreq) - 0.5f * baud;
  float const fb = static_cast<float> (nfreq) + 3.5f * baud + static_cast<float> (nslots - 1) * 500.0f;
  int const ia2 = clamp_int (static_cast<int> (std::lround (fa / df)), 0, nh);
  int const ib1 = clamp_int (static_cast<int> (std::lround (fb / df)), 0, nh);
  int const ia1 = clamp_int (static_cast<int> (std::lround ((fa - width) / df)), 0, ia2);
  int const ib2 = clamp_int (static_cast<int> (std::lround ((fb + width) / df)), ib1, nh);

  float const pi = static_cast<float> (std::acos (-1.0));
  for (int i = ia1; i <= ia2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ia2) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = ib1; i <= ib2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ib1) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = 0; i < ia1; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }
  for (int i = ib2 + 1; i <= nh; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }

  fftwf_execute (inverse);
  fftwf_destroy_plan (forward);
  fftwf_destroy_plan (inverse);

  float const scale = 1.0f / static_cast<float> (kFt2FoxNfft);
  for (int i = 0; i < copy_samples; ++i)
    {
      wave[i] = time_domain[static_cast<size_t> (i)] * scale;
    }
}
}

namespace decodium
{
namespace txwave
{

QVector<float> generateFt2Wave (int const* itone, int nsym, int nsps, float fsample, float f0)
{
  return generate_ft2_ft4_wave (itone, nsym, nsps, fsample, f0);
}

QVector<float> generateFt4Wave (int const* itone, int nsym, int nsps, float fsample, float f0)
{
  return generate_ft2_ft4_wave (itone, nsym, nsps, fsample, f0);
}

QVector<float> generateFt8Wave (int const* itone, int nsym, int nsps, float bt, float fsample, float f0)
{
  return generate_ft8_wave (itone, nsym, nsps, bt, fsample, f0);
}

}
}

extern "C" int ftx_gen_ft8wave_complex_c (int const* itone, int nsym, int nsps, float bt,
                                           float fsample, float f0, float* real_out,
                                           float* imag_out, int nwave)
{
  constexpr int kNTab = 65536;
  if (!itone || !real_out || !imag_out || nsym <= 0 || nsps <= 0 || nwave <= 0 || fsample <= 0.0f)
    {
      return 0;
    }

  std::vector<double> pulse (static_cast<size_t> (3 * nsps), 0.0);
  for (int i = 0; i < 3 * nsps; ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[static_cast<size_t> (i)] = gfsk_pulse (double (bt), tt);
    }

  std::vector<double> dphi (static_cast<size_t> ((nsym + 2) * nsps), 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
            * double (itone[j]);
        }
    }

  for (int i = 0; i < 2 * nsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * double (itone[0])
        * pulse[static_cast<size_t> (nsps + i)];
      dphi[static_cast<size_t> (nsym * nsps + i)] += dphi_peak * double (itone[nsym - 1])
        * pulse[static_cast<size_t> (i)];
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  double phi = 0.0;
  int const samples = std::min (nwave, nsym * nsps);
  for (int i = 0; i < samples; ++i)
    {
      int index = static_cast<int> (phi * double (kNTab) / kTwoPi);
      if (index < 0)
        {
          index = 0;
        }
      else if (index >= kNTab)
        {
          index = kNTab - 1;
        }
      double const phi_quantized = double (index) * kTwoPi / double (kNTab);
      real_out[i] = float (std::cos (phi_quantized));
      imag_out[i] = float (std::sin (phi_quantized));
      phi = wrap_phase (phi + dphi[static_cast<size_t> (nsps + i)] + carrier_step);
    }
  for (int i = samples; i < nwave; ++i)
    {
      real_out[i] = 0.0f;
      imag_out[i] = 0.0f;
    }

  int const nramp = std::max (1, int (std::lround (double (nsps) / 8.0)));
  for (int i = 0; i < std::min (nramp, samples); ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      real_out[i] = float (real_out[i] * env);
      imag_out[i] = float (imag_out[i] * env);
    }

  int const tail_start = std::max (0, samples - nramp);
  for (int i = 0; i < nramp && tail_start + i < samples; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      real_out[tail_start + i] = float (real_out[tail_start + i] * env);
      imag_out[tail_start + i] = float (imag_out[tail_start + i] * env);
    }

  return 1;
}

extern "C" void foxgen_ (bool* bSuperFox, char const* /*fname*/, fortran_charlen_t /*len*/)
{
  if (bSuperFox && *bSuperFox)
    {
      foxcom3_.nslots2 = foxcom_.nslots;
      std::memset (foxcom3_.cmsg2, ' ', sizeof (foxcom3_.cmsg2));
      for (int slot = 0; slot < std::min (foxcom_.nslots, 5); ++slot)
        {
          std::memcpy (foxcom3_.cmsg2[slot], foxcom_.cmsg[slot], 40);
        }
      std::fill_n (foxcom3_.itone3, 151, 0);
      return;
    }

  QVector<float> wave (kFt8FoxNwave, 0.0f);
  std::fill_n (foxcom2_.itone2, kFt8FoxNsym, 0);
  std::fill_n (foxcom2_.msgbits2, 77, static_cast<signed char> (0));

  int const slots = clamp_int (foxcom_.nslots, 0, 5);
  for (int slot = 0; slot < slots; ++slot)
    {
      QString const message = QString::fromLatin1 (foxcom_.cmsg[slot], 40).left (37);
      auto const encoded = decodium::txmsg::encodeFt8 (message);
      if (!encoded.ok || encoded.tones.size () < kFt8FoxNsym)
        {
          continue;
        }

      float const f0 = static_cast<float> (foxcom_.nfreq + slot * 60);
      QVector<float> const slot_wave = decodium::txwave::generateFt8Wave (encoded.tones.constData (),
                                                                          kFt8FoxNsym,
                                                                          kFt8FoxNsps,
                                                                          2.0f,
                                                                          48000.0f,
                                                                          f0);
      int const samples = std::min (wave.size (), slot_wave.size ());
      for (int i = 0; i < samples; ++i)
        {
          wave[i] += slot_wave.at (i);
        }

      if (slot == slots - 1)
        {
          for (int i = 0; i < kFt8FoxNsym; ++i)
            {
              foxcom2_.itone2[i] = encoded.tones.at (i);
            }
          for (int i = 0; i < 77 && i < encoded.msgbits.size (); ++i)
            {
              foxcom2_.msgbits2[i] = static_cast<signed char> (encoded.msgbits.at (i) != 0);
            }
        }
    }

  normalize_wave (wave);
  fox_bandlimit_inplace (wave, slots, foxcom_.nfreq, 50.0f);
  normalize_wave (wave);

  int const max_samples = static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0]));
  int const copy_samples = std::min (max_samples, wave.size ());
  std::copy_n (wave.constBegin (), copy_samples, foxcom_.wave);
  if (copy_samples < max_samples)
    {
      std::fill (foxcom_.wave + copy_samples, foxcom_.wave + max_samples, 0.0f);
    }
}

extern "C" void foxgenft2_ ()
{
  QVector<float> wave (kFt2FoxNwave, 0.0f);

  int const slots = clamp_int (foxcom_.nslots, 0, 5);
  for (int slot = 0; slot < slots; ++slot)
    {
      QString const message = QString::fromLatin1 (foxcom_.cmsg[slot], 40).left (37);
      auto const encoded = decodium::txmsg::encodeFt2 (message);
      if (!encoded.ok || encoded.tones.size () < kFt2FoxNsym)
        {
          continue;
        }

      float const f0 = static_cast<float> (foxcom_.nfreq + slot * 500);
      QVector<float> const slot_wave = decodium::txwave::generateFt2Wave (encoded.tones.constData (),
                                                                          kFt2FoxNsym,
                                                                          kFt2FoxNsps,
                                                                          48000.0f,
                                                                          f0);
      int const samples = std::min (wave.size (), slot_wave.size ());
      for (int i = 0; i < samples; ++i)
        {
          wave[i] += slot_wave.at (i);
        }
    }

  normalize_wave (wave);
  ft2_fox_bandlimit_inplace (wave, slots, foxcom_.nfreq, 50.0f);
  normalize_wave (wave);

  int const max_samples = static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0]));
  int const copy_samples = std::min (max_samples, wave.size ());
  std::copy_n (wave.constBegin (), copy_samples, foxcom_.wave);
  if (copy_samples < max_samples)
    {
      std::fill (foxcom_.wave + copy_samples, foxcom_.wave + max_samples, 0.0f);
    }
}

extern "C" void gen_ft8wave_ (int* itone, int* nsym, int* nsps, float* bt, float* fsample,
                               float* f0, std::complex<float>* cwave, float* wave,
                               int* icmplx, int* nwave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (icmplx && *icmplx != 0)
    {
      if (cwave)
        {
          std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
        }
      if (!itone || !nsym || !nsps || !bt || !fsample || !f0 || !cwave)
        {
          return;
        }

      std::vector<float> real_out (static_cast<size_t> (output_samples), 0.0f);
      std::vector<float> imag_out (static_cast<size_t> (output_samples), 0.0f);
      if (!ftx_gen_ft8wave_complex_c (itone, symbol_count, samples_per_symbol, *bt, *fsample, *f0,
                                      real_out.data (), imag_out.data (), output_samples))
        {
          return;
        }
      for (int i = 0; i < output_samples; ++i)
        {
          cwave[i] = std::complex<float> {real_out[static_cast<size_t> (i)],
                                          imag_out[static_cast<size_t> (i)]};
        }
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (!itone || !nsym || !nsps || !bt || !fsample || !f0 || !wave)
    {
      return;
    }

  QVector<float> const generated = decodium::txwave::generateFt8Wave (itone, symbol_count,
                                                                      samples_per_symbol, *bt,
                                                                      *fsample, *f0);
  int const samples = std::min (output_samples, generated.size ());
  for (int i = 0; i < samples; ++i)
    {
      wave[i] = generated.at (i);
    }
}

extern "C" void gen_ft4wave_ (int* itone, int* nsym, int* nsps, float* fsample, float* f0,
                               std::complex<float>* cwave, float* wave,
                               int* icmplx, int* nwave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (icmplx && *icmplx != 0)
    {
      if (cwave)
        {
          std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
        }
      if (!itone || !nsym || !nsps || !fsample || !f0 || !cwave)
        {
          return;
        }

      generate_ft2_ft4_complex_wave (itone, symbol_count, samples_per_symbol, *fsample, *f0,
                                     cwave, output_samples);
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (!itone || !nsym || !nsps || !fsample || !f0 || !wave)
    {
      return;
    }

  QVector<float> const generated = decodium::txwave::generateFt4Wave (itone, symbol_count,
                                                                      samples_per_symbol,
                                                                      *fsample, *f0);
  int const samples = std::min (output_samples, generated.size ());
  for (int i = 0; i < samples; ++i)
    {
      wave[i] = generated.at (i);
    }
}

extern "C" void gen_ft2wave_ (int* itone, int* nsym, int* nsps, float* fsample, float* f0,
                               std::complex<float>* cwave, float* wave,
                               int* icmplx, int* nwave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (icmplx && *icmplx != 0)
    {
      if (cwave)
        {
          std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
        }
      if (!itone || !nsym || !nsps || !fsample || !f0 || !cwave)
        {
          return;
        }

      generate_ft2_ft4_complex_wave (itone, symbol_count, samples_per_symbol, *fsample, *f0,
                                     cwave, output_samples);
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (!itone || !nsym || !nsps || !fsample || !f0 || !wave)
    {
      return;
    }

  QVector<float> const generated = decodium::txwave::generateFt2Wave (itone, symbol_count,
                                                                      samples_per_symbol,
                                                                      *fsample, *f0);
  int const samples = std::min (output_samples, generated.size ());
  for (int i = 0; i < samples; ++i)
    {
      wave[i] = generated.at (i);
    }
}

extern "C" void gen_q65_wave_ (char* msg, int* ntxFreq, int* mode64,
                               char* msgsent, short iwave[], int* nwave,
                               int len1, int len2)
{
  if (nwave) *nwave = 0;
  if (!msg || !ntxFreq || !mode64 || !msgsent || !iwave || !nwave)
    {
      return;
    }

  std::array<char, 37> msg37 {};
  std::array<char, 37> msgsent37 {};
  std::array<int, 85> itone {};
  std::fill (msg37.begin (), msg37.end (), ' ');
  std::fill (msgsent37.begin (), msgsent37.end (), ' ');
  std::copy_n (msg, std::min (len1, 37), msg37.data ());

  int ichk = 0;
  int i3 = -1;
  int n3 = -1;
  genq65_ (msg37.data (), &ichk, msgsent37.data (), itone.data (), &i3, &n3,
           static_cast<fortran_charlen_t> (37), static_cast<fortran_charlen_t> (37));

  std::fill_n (msgsent, len2, ' ');
  std::copy_n (msgsent37.data (), std::min (len2, 37), msgsent);

  double const fsample = 11025.0;
  double const dt = 1.0 / fsample;
  double const tsym = 7200.0 / 12000.0;
  double const f0 = double (*ntxFreq);
  int const ndf = 1 << std::max (0, *mode64 - 1);
  double const dfgen = double (ndf) * 12000.0 / 7200.0;
  int const samples = static_cast<int> (85.0 * 7200.0 * fsample / 12000.0);
  *nwave = 2 * samples;

  double phi = 0.0;
  double dphi = kTwoPi * dt * f0;
  double t = 0.0;
  int current_symbol = 0;
  for (int i = 0; i < samples; ++i)
    {
      t += dt;
      int symbol = clamp_int (static_cast<int> (t / tsym + 1.0), 1, 85);
      if (symbol != current_symbol)
        {
          dphi = kTwoPi * dt * (f0 + double (itone[static_cast<size_t> (symbol - 1)]) * dfgen);
          current_symbol = symbol;
        }
      phi = wrap_phase (phi + dphi);
      iwave[2 * i] = static_cast<short> (std::lround (32767.0 * std::cos (phi)));
      iwave[2 * i + 1] = static_cast<short> (std::lround (32767.0 * std::sin (phi)));
    }
}

extern "C" void gen_q65_cwave_ (char* msg, int* ntxFreq, int* ntone_spacing,
                                char* msgsent, std::complex<float>* cwave, int* nwave,
                                fortran_charlen_t len1, fortran_charlen_t len2)
{
  if (nwave) *nwave = 0;
  if (!msg || !ntxFreq || !ntone_spacing || !msgsent || !cwave || !nwave)
    {
      return;
    }

  std::array<char, 37> msg37 {};
  std::array<char, 37> msgsent37 {};
  std::array<int, 85> itone {};
  std::fill (msg37.begin (), msg37.end (), ' ');
  std::fill (msgsent37.begin (), msgsent37.end (), ' ');
  std::copy_n (msg, std::min (static_cast<int> (len1), 37), msg37.data ());

  int ichk = 0;
  int i3 = -1;
  int n3 = -1;
  genq65_ (msg37.data (), &ichk, msgsent37.data (), itone.data (), &i3, &n3,
           static_cast<fortran_charlen_t> (37), static_cast<fortran_charlen_t> (37));

  std::fill_n (msgsent, static_cast<size_t> (len2), ' ');
  std::copy_n (msgsent37.data (), std::min (static_cast<int> (len2), 37), msgsent);

  int const samples = static_cast<int> (85.0 * 7200.0 * 96000.0 / 12000.0);
  *nwave = samples;
  std::fill_n (cwave, static_cast<size_t> (samples), std::complex<float> {0.0f, 0.0f});
  generate_q65_complex_wave (itone.data (), 85, 96000.0, double (*ntxFreq),
                             double (*ntone_spacing) * 12000.0 / 7200.0, cwave, samples);
}
