#include "Detector/JT9NarrowDecoder.hpp"
#include "Detector/JT9FastDecoder.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Modulator/LegacyJtEncoder.hpp"

#include "commons.h"

#include <QCoreApplication>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
constexpr int kSampleRate = RX_SAMPLE_RATE;
constexpr int kPeriodSeconds = 60;
constexpr int kAudioSamples = kSampleRate * kPeriodSeconds;
constexpr int kSamplesPerSymbol = 6912;
constexpr float kToneSpacing = 12000.0f / static_cast<float> (kSamplesPerSymbol);
constexpr float kFrequency = 1500.0f;
constexpr int kSignalStart = kSampleRate;

QVector<short> make_fixture (QString const& message)
{
  QVector<short> audio (kAudioSamples, 0);
  auto const encoded = decodium::txmsg::encodeJt9 (message);
  if (!encoded.ok || encoded.tones.size () < 85)
    {
      return {};
    }
  auto const wave = decodium::txwave::generateToneWave (
      encoded.tones.constData (), 85, kSamplesPerSymbol, static_cast<float> (kSampleRate),
      kToneSpacing, kFrequency);
  int const available = std::min (wave.size (), audio.size () - kSignalStart);
  for (int i = 0; i < available; ++i)
    {
      audio[kSignalStart + i] = static_cast<short> (
          std::lround (24000.0f * wave.at (i)));
    }
  return audio;
}

bool run_case (QString const& message)
{
  auto const audio = make_fixture (message);
  if (audio.isEmpty ())
    {
      std::fprintf (stderr, "JT9 narrow fixture generation failed\n");
      return false;
    }

  decodium::legacyjt::DecodeRequest request;
  request.serial = 1;
  request.mode = QStringLiteral ("JT9");
  request.audio = audio;
  request.npts8 = audio.size () / 8;
  request.nzhsym = 184;
  request.nutc = 1234;
  request.nfqso = static_cast<int> (kFrequency);
  request.ntol = 100;
  request.ndepth = 3;
  request.nfa = 0;
  request.nfb = 5000;
  request.newdat = 1;
  request.nagain = 0;

  decodium::jt9narrow::CorrState state;
  auto const rows = decodium::jt9narrow::decode_async_jt9_narrow (request, &state);

  for (auto const& row : rows)
    {
      std::fprintf (stderr, "JT9 narrow row: %s\n", row.toLatin1 ().constData ());
    }
  if (rows.isEmpty () || !rows.front ().contains (message))
    {
      std::fprintf (stderr, "JT9 narrow decoder produced no matching row for '%s'\n",
                    message.toLatin1 ().constData ());
      return false;
    }
  return true;
}

bool run_ideal_fano_case (QString const& message)
{
  auto const encoded = decodium::txmsg::encodeJt9 (message);
  if (!encoded.ok)
    {
      return false;
    }

  std::array<unsigned char, 206> encodedBits {};
  std::array<unsigned char, 206> interleavedBits {};
  std::array<std::int8_t, 207> scrambledSoft {};
  std::array<std::int8_t, 207> soft {};
  std::array<signed char, 13> tailBytes {};
  auto const packed = decodium::legacy_jt::detail::packmsg (
      decodium::legacy_jt::detail::fixed_ascii (message, 22));
  decodium::legacy_jt::detail::entail (packed.dat, tailBytes);
  decodium::legacy_jt::detail::encode232 (tailBytes, encodedBits);
  decodium::legacy_jt::detail::interleave9 (encodedBits, interleavedBits);
  for (int i = 0; i < 206; ++i)
    {
      scrambledSoft[static_cast<std::size_t> (i)] =
          interleavedBits[static_cast<std::size_t> (i)] ? 127 : -127;
    }
  auto const order = decodium::legacy_jt::detail::interleave9_order ();
  for (int i = 0; i < 206; ++i)
    {
      // inverse of interleave9_deinterleave: ib[i] = ia[j0[i]].
      soft[static_cast<std::size_t> (i)] =
          scrambledSoft[static_cast<std::size_t> (order[static_cast<std::size_t> (i)])];
    }
  soft[206] = 0;

  int nlim = 0;
  QByteArray const decoded = decodium::jt9fast::decode_soft_symbols (soft, 30000, &nlim);
  std::fprintf (stderr, "JT9 ideal Fano decoded='%s' nlim=%d\n",
                decoded.constData (), nlim);
  return decoded == decodium::legacy_jt::detail::fixed_ascii (message, 22);
}
}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  QString const message = QStringLiteral ("CQ TESTA JN70");
  if (!run_ideal_fano_case (message))
    {
      std::fprintf (stderr, "JT9 ideal Fano regression failed\n");
      return 1;
    }
  bool const ok = run_case (message);
  if (ok)
    {
      std::printf ("JT9 narrow decode compare passed\n");
      return 0;
    }
  return 1;
}
