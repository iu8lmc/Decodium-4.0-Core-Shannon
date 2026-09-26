// tests/ft2link_bench.cpp
//
// P1 iu8lmc — Banco soglia-dB FT2-Link (task 1 della fase P1 del piano).
//
// Misura, per ciascun profilo (NARROW, W500, W2300 FAST, W2300 ROBUST), il
// rapporto segnale/rumore al quale il decoder raggiunge il 50% di successo,
// con rumore bianco gaussiano (AWGN) calibrato in banda di riferimento
// 2500 Hz (convenzione ham/WSJT-X: i numeri sono confrontabili con FT8/FT4).
//
// Metodo per ogni punto SNR:
//   frame noto -> generateXFrameWaveform -> inserito con offset casuale in un
//   buffer con silenzio prima/dopo -> AWGN calibrato sull'energia media del
//   burst -> decodeXFrameWaveformWithMetrics -> successo se il frame decodificato
//   combacia (type/sequence/payload). Sweep SNR discendente a passi di 2 dB,
//   N prove per punto, stop quando il successo crolla; soglia 50% interpolata.
//
// C++17 puro, linka solo ft2link_core (nessuna dipendenza Qt).
// Uso: ft2link_bench [trials-per-punto=20] [seed=12345]

#include "lib/ft2link/FT2LinkFrame.hpp"
#include "lib/ft2link/FT2LinkWaveform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace
{

using decodium::ft2link::Frame;
using decodium::ft2link::FrameType;
using decodium::ft2link::Profile;
using decodium::ft2link::W2300RateMode;

constexpr double kSampleRate = 12000.0;
constexpr double kNoiseRefBandwidthHz = 2500.0;

Frame makeTestFrame (Profile profile, std::size_t payloadBytes)
{
  Frame frame;
  frame.type = FrameType::Data;
  frame.profile = profile;
  frame.flags = decodium::ft2link::FlagEndOfMessage;
  frame.sessionId = 0x1234;
  frame.sequence = 7;
  frame.payload.resize (payloadBytes);
  for (std::size_t i = 0; i < payloadBytes; ++i)
    {
      frame.payload[i] = static_cast<std::uint8_t> ((i * 37 + 11) & 0xff);
    }
  return frame;
}

bool sameFrame (Frame const& a, Frame const& b)
{
  return a.type == b.type
      && a.sessionId == b.sessionId
      && a.sequence == b.sequence
      && a.payload == b.payload;
}

double meanPower (std::vector<float> const& wave)
{
  if (wave.empty ())
    {
      return 0.0;
    }
  double sum = 0.0;
  for (float sample : wave)
    {
      sum += static_cast<double> (sample) * static_cast<double> (sample);
    }
  return sum / static_cast<double> (wave.size ());
}

// Costruisce il buffer di prova: silenzio + burst + silenzio, poi AWGN con
// sigma calibrato perche' SNR (in banda 2500 Hz) rispetto alla potenza media
// del burst sia snrDb.
std::vector<float> buildTrialBuffer (std::vector<float> const& wave,
                                     double signalPower,
                                     double snrDb,
                                     std::mt19937& rng)
{
  std::uniform_int_distribution<std::size_t> leadDist (
      static_cast<std::size_t> (0.05 * kSampleRate),
      static_cast<std::size_t> (0.30 * kSampleRate));
  std::size_t const lead = leadDist (rng);
  std::size_t const tail = static_cast<std::size_t> (0.10 * kSampleRate);

  std::vector<float> buffer (lead + wave.size () + tail, 0.0f);
  std::copy (wave.begin (), wave.end (), buffer.begin () + static_cast<long> (lead));

  // sigma^2 totale del rumore bianco (banda 0..6000 Hz) tale che la porzione
  // in 2500 Hz dia l'SNR richiesto: sigma2_2500 = sigma2 * 2500/6000.
  double const snrLinear = std::pow (10.0, snrDb / 10.0);
  double const sigma2 = signalPower * (kSampleRate / 2.0)
      / (kNoiseRefBandwidthHz * snrLinear);
  double const sigma = std::sqrt (sigma2);
  std::normal_distribution<float> noise (0.0f, static_cast<float> (sigma));
  for (float& sample : buffer)
    {
      sample += noise (rng);
    }
  return buffer;
}

struct ProfileBench
{
  std::string name;
  Profile profile;
  std::size_t payloadBytes;
  // genera la waveform del frame di prova
  std::vector<float> (*generate) (Frame const&, std::string*);
  // tenta la decodifica dal buffer rumoroso
  bool (*decode) (std::vector<float> const&, Frame*, std::string*);
};

std::vector<float> genNarrow (Frame const& frame, std::string* error)
{
  return decodium::ft2link::generateNarrowFrameWaveform (
      frame, decodium::ft2link::NarrowWaveformConfig {}, error);
}
bool decNarrow (std::vector<float> const& wave, Frame* frame, std::string* error)
{
  decodium::ft2link::NarrowDecodeMetrics metrics;
  return decodium::ft2link::decodeNarrowFrameWaveformWithMetrics (
      wave, frame, &metrics, decodium::ft2link::NarrowWaveformConfig {}, error);
}

std::vector<float> genW500 (Frame const& frame, std::string* error)
{
  return decodium::ft2link::generateW500FrameWaveform (
      frame, decodium::ft2link::W500WaveformConfig {}, error);
}
bool decW500 (std::vector<float> const& wave, Frame* frame, std::string* error)
{
  decodium::ft2link::W500DecodeMetrics metrics;
  return decodium::ft2link::decodeW500FrameWaveformWithMetrics (
      wave, frame, &metrics, decodium::ft2link::W500WaveformConfig {}, error);
}

std::vector<float> genW2300Fast (Frame const& frame, std::string* error)
{
  decodium::ft2link::W2300WaveformConfig config;
  config.rateMode = W2300RateMode::Fast;
  return decodium::ft2link::generateW2300FrameWaveform (frame, config, error);
}
std::vector<float> genW2300Robust (Frame const& frame, std::string* error)
{
  decodium::ft2link::W2300WaveformConfig config;
  config.rateMode = W2300RateMode::Robust;
  return decodium::ft2link::generateW2300FrameWaveform (frame, config, error);
}
bool decW2300 (std::vector<float> const& wave, Frame* frame, std::string* error)
{
  decodium::ft2link::W2300DecodeMetrics metrics;
  return decodium::ft2link::decodeW2300FrameWaveformWithMetrics (
      wave, frame, &metrics, decodium::ft2link::W2300WaveformConfig {}, error);
}

}  // namespace

int main (int argc, char** argv)
{
  int const trials = argc > 1 ? std::max (4, std::atoi (argv[1])) : 20;
  unsigned const baseSeed = argc > 2
      ? static_cast<unsigned> (std::strtoul (argv[2], nullptr, 10))
      : 12345u;

  ProfileBench const benches[] = {
    {"NARROW",       Profile::Narrow,   24u, &genNarrow,      &decNarrow},
    {"W500",         Profile::Wide500,  48u, &genW500,        &decW500},
    {"W2300 FAST",   Profile::Wide2300, 200u, &genW2300Fast,   &decW2300},
    {"W2300 ROBUST", Profile::Wide2300, 200u, &genW2300Robust, &decW2300},
  };

  std::printf ("# FT2-Link banco soglia-dB (AWGN, SNR in banda 2500 Hz)\n");
  std::printf ("# trials/punto=%d seed=%u\n", trials, baseSeed);
  std::printf ("profilo,snr_db,successi,prove,rate\n");

  for (ProfileBench const& bench : benches)
    {
      Frame const frame = makeTestFrame (bench.profile, bench.payloadBytes);
      std::string error;
      std::vector<float> const wave = bench.generate (frame, &error);
      if (wave.empty ())
        {
          std::printf ("%s,GENERATE-FAILED,%s\n", bench.name.c_str (),
                       error.c_str ());
          continue;
        }
      double const signalPower = meanPower (wave);
      double const durationS =
          static_cast<double> (wave.size ()) / kSampleRate;
      std::fprintf (stderr, "[%s] burst %.2fs, payload %zuB, Psig %.4g\n",
                    bench.name.c_str (), durationS, bench.payloadBytes,
                    signalPower);

      // Pre-check di sanita' del banco: a +60 dB il rumore e' trascurabile;
      // se il decoder fallisce QUI il problema e' il decoder/harness, non l'SNR.
      {
        std::mt19937 rng (baseSeed + 999983u);
        std::vector<float> const clean =
            buildTrialBuffer (wave, signalPower, 60.0, rng);
        Frame decoded;
        std::string decodeError;
        bool const ok = bench.decode (clean, &decoded, &decodeError)
                        && sameFrame (frame, decoded);
        std::printf ("%s,60,%d,1,%.2f%s\n", bench.name.c_str (),
                     ok ? 1 : 0, ok ? 1.0 : 0.0,
                     ok ? "" : "  ## SANITY FAIL (+60 dB)");
      }

      double previousSnr = 999.0;
      double previousRate = -1.0;
      double threshold50 = 999.0;
      int weakPoints = 0;
      bool everSucceeded = false;

      for (double snrDb = 24.0; snrDb >= -24.0; snrDb -= 2.0)
        {
          int successes = 0;
          for (int trial = 0; trial < trials; ++trial)
            {
              std::mt19937 rng (baseSeed
                                + static_cast<unsigned> (snrDb * 100.0)
                                + static_cast<unsigned> (trial) * 7919u
                                + static_cast<unsigned> (
                                    bench.payloadBytes) * 104729u);
              std::vector<float> const buffer =
                  buildTrialBuffer (wave, signalPower, snrDb, rng);
              Frame decoded;
              std::string decodeError;
              if (bench.decode (buffer, &decoded, &decodeError)
                  && sameFrame (frame, decoded))
                {
                  ++successes;
                }
            }
          double const rate = static_cast<double> (successes)
                              / static_cast<double> (trials);
          std::printf ("%s,%.0f,%d,%d,%.2f\n", bench.name.c_str (), snrDb,
                       successes, trials, rate);
          std::fflush (stdout);
          if (rate > 0.0)
            {
              everSucceeded = true;
            }

          if (previousRate >= 0.5 && previousSnr < 500.0
              && rate < 0.5 && threshold50 > 500.0)
            {
              // interpolazione lineare del punto 50% tra i due SNR
              double const span = previousRate - rate;
              double const frac = span > 0.0 ? (previousRate - 0.5) / span : 0.5;
              threshold50 = previousSnr + (snrDb - previousSnr) * frac;
            }
          previousSnr = snrDb;
          previousRate = rate;

          if (rate < 0.10)
            {
              ++weakPoints;
              if (weakPoints >= 2 && everSucceeded)
                {
                  break;  // il decoder e' morto: inutile scendere ancora
                }
            }
          else
            {
              weakPoints = 0;
            }
        }

      if (threshold50 < 500.0)
        {
          std::printf ("## %s: soglia 50%% = %.1f dB (2500 Hz)\n",
                       bench.name.c_str (), threshold50);
        }
      else if (everSucceeded)
        {
          std::printf ("## %s: sempre sopra 50%% fino a -24 dB\n",
                       bench.name.c_str ());
        }
      else
        {
          std::printf ("## %s: MAI decodificato (verificare pipeline)\n",
                       bench.name.c_str ());
        }
      std::fflush (stdout);
    }

  return 0;
}
