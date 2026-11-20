#include "JttyWave.hpp"

#include "JttyTbcc.hpp"

#include <cmath>

namespace decodium
{
namespace jtty
{

namespace
{

float gfsk_pulse (float b, float t)
{
  float const pi = 4.0f * std::atan (1.0f);
  float const c = pi * std::sqrt (2.0f / std::log (2.0f));
  return 0.5f * (std::erf (c * b * (t + 0.5f)) - std::erf (c * b * (t - 0.5f)));
}

float fmodulo (float a, float p)
{
  return a - std::floor (a / p) * p;
}

// La parte comune: la frequenza istantanea levigata, con un simbolo fittizio
// in testa e uno in coda uguali al primo e all'ultimo.
std::vector<float> smoothed_dphi (int const* tones, int nsym, int nsps, float bt,
                                  float fsample, float f0)
{
  float const twopi = 8.0f * std::atan (1.0f);
  float const dt = 1.0f / fsample;
  float const hmod = 1.0f;
  std::vector<float> pulse (static_cast<std::size_t> (3 * nsps));
  for (int i = 1; i <= 3 * nsps; ++i)
    {
      float const tt = (static_cast<float> (i) - 1.5f * static_cast<float> (nsps)) / static_cast<float> (nsps);
      pulse[static_cast<std::size_t> (i - 1)] = gfsk_pulse (bt, tt);
    }
  std::vector<float> dphi (static_cast<std::size_t> ((nsym + 2) * nsps), 0.0f);
  float const dphi_peak = twopi * hmod / static_cast<float> (nsps);
  for (int j = 1; j <= nsym; ++j)
    {
      int const ib = (j - 1) * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        dphi[static_cast<std::size_t> (ib + i)] += dphi_peak * pulse[static_cast<std::size_t> (i)]
            * static_cast<float> (tones[j - 1]);
    }
  for (int i = 0; i < 2 * nsps; ++i)
    dphi[static_cast<std::size_t> (i)] += dphi_peak * static_cast<float> (tones[0])
        * pulse[static_cast<std::size_t> (nsps + i)];
  for (int i = 0; i < 2 * nsps; ++i)
    dphi[static_cast<std::size_t> (nsym * nsps + i)] += dphi_peak * static_cast<float> (tones[nsym - 1])
        * pulse[static_cast<std::size_t> (i)];
  for (auto& d : dphi) d = d + twopi * f0 * dt;
  return dphi;
}

}

std::vector<int> tones_for_frames (std::vector<Frame> const& frames)
{
  std::vector<int> tones;
  tones.reserve (frames.size () * kFrameSymbols);
  for (Frame const f : frames)
    {
      int payload[kPayloadBits];
      frame_to_payload (f, payload);
      int data[kInformationBits];
      tbcc_encode (payload, data);
      tones.insert (tones.end (), kSync13, kSync13 + kSyncSymbols);
      tones.insert (tones.end (), data, data + kInformationBits);
    }
  return tones;
}

std::vector<float> generate_wave (std::vector<int> const& tones, int nsps, float bt,
                                  float fsample, float f0)
{
  int const nsym = static_cast<int> (tones.size ());
  if (nsym <= 0 || nsps <= 0) return {};
  int const nwave = nsym * nsps;
  float const twopi = 8.0f * std::atan (1.0f);
  std::vector<float> const dphi = smoothed_dphi (tones.data (), nsym, nsps, bt, fsample, f0);
  std::vector<float> wave (static_cast<std::size_t> (nwave), 0.0f);
  float phi = 0.0f;
  int k = 0;
  for (int j = nsps; j <= nsps + nwave - 1; ++j)
    {
      wave[static_cast<std::size_t> (k++)] = std::sin (phi);
      phi = fmodulo (phi + dphi[static_cast<std::size_t> (j)], twopi);
    }
  int const nramp = static_cast<int> (std::lround (static_cast<float> (nsps) / 8.0f));
  for (int i = 0; i < nramp; ++i)
    wave[static_cast<std::size_t> (i)] *= (1.0f - std::cos (twopi * static_cast<float> (i) / (2.0f * nramp))) / 2.0f;
  int const k1 = nsym * nsps - nramp;
  for (int i = 0; i < nramp; ++i)
    wave[static_cast<std::size_t> (k1 + i)] *= (1.0f + std::cos (twopi * static_cast<float> (i) / (2.0f * nramp))) / 2.0f;
  return wave;
}

std::vector<std::complex<float>> generate_complex_wave (int const* tones, int nsym, int nsps,
                                                        float bt, float fsample, float f0)
{
  if (nsym <= 0 || nsps <= 0) return {};
  constexpr int NTAB = 65536;
  static std::vector<std::complex<float>> const ctab = [] {
    std::vector<std::complex<float>> t (NTAB);
    float const tp = 8.0f * std::atan (1.0f);
    for (int i = 0; i < NTAB; ++i)
      {
        float const phi = static_cast<float> (i) * tp / static_cast<float> (NTAB);
        t[static_cast<std::size_t> (i)] = {std::cos (phi), std::sin (phi)};
      }
    return t;
  } ();
  int const nwave = nsym * nsps;
  float const twopi = 8.0f * std::atan (1.0f);
  std::vector<float> const dphi = smoothed_dphi (tones, nsym, nsps, bt, fsample, f0);
  std::vector<std::complex<float>> cwave (static_cast<std::size_t> (nwave));
  float phi = 0.0f;
  int k = 0;
  for (int j = nsps; j <= nsps + nwave - 1; ++j)
    {
      int const i = std::min (NTAB - 1, static_cast<int> (phi * static_cast<float> (NTAB) / twopi));
      cwave[static_cast<std::size_t> (k++)] = ctab[static_cast<std::size_t> (i)];
      phi = fmodulo (phi + dphi[static_cast<std::size_t> (j)], twopi);
    }
  int const nramp = static_cast<int> (std::lround (static_cast<float> (nsps) / 8.0f));
  for (int i = 0; i < nramp; ++i)
    cwave[static_cast<std::size_t> (i)] *= (1.0f - std::cos (twopi * static_cast<float> (i) / (2.0f * nramp))) / 2.0f;
  int const k1 = nsym * nsps - nramp;
  for (int i = 0; i < nramp; ++i)
    cwave[static_cast<std::size_t> (k1 + i)] *= (1.0f + std::cos (twopi * static_cast<float> (i) / (2.0f * nramp))) / 2.0f;
  return cwave;
}

}
}
