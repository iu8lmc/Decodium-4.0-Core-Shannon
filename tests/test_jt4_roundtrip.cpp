#include "Detector/JT4Decoder.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{

constexpr int kSampleRate = 12000;
constexpr int kDecodeSeconds = 52;
constexpr int kSignalStart = kSampleRate;
constexpr float kAudioFrequency = 1500.0f;

QVector<short> make_fixture (QString const& message, int submode)
{
  auto const encoded = decodium::txmsg::encodeJt4 (message);
  if (!encoded.ok || encoded.tones.size () != 206)
    return {};

  auto const wave = decodium::txwave::generateJt4Wave (
      encoded.tones.constData (), encoded.tones.size (),
      static_cast<float> (kSampleRate), kAudioFrequency, submode);
  QVector<short> audio (kDecodeSeconds * kSampleRate, 0);
  int const available = std::min (wave.size (), audio.size () - kSignalStart);
  for (int i = 0; i < available; ++i)
    audio[kSignalStart + i] = static_cast<short> (
        std::lround (24000.0f * wave.at (i)));
  return audio;
}

bool run_case (QString const& message, int submode)
{
  QVector<short> const audio = make_fixture (message, submode);
  if (audio.isEmpty ())
    {
      std::fprintf (stderr, "JT4 fixture generation failed\n");
      return false;
    }

  decodium::legacyjt::DecodeRequest request;
  request.serial = 1;
  request.mode = QStringLiteral ("JT4");
  request.audio = audio;
  request.nutc = 1234;
  request.nfqso = static_cast<int> (kAudioFrequency);
  request.ntol = 50;
  request.ndepth = 3;
  request.nsubmode = submode;
  request.minsync = 0;
  request.minw = 0;
  request.newdat = 1;
  request.mycall = QByteArrayLiteral ("TESTB");
  request.hiscall = QByteArrayLiteral ("TESTA");
  request.hisgrid = QByteArrayLiteral ("JN70");
  request.tempDir = QDir::tempPath ().toLocal8Bit ();
  request.dataDir = QDir::currentPath ().toLocal8Bit ();

  decodium::jt4::AverageState state;
  QStringList const rows = decodium::jt4::decode_async_jt4 (request, &state);
  for (QString const& row : rows)
    std::fprintf (stderr, "JT4%c row: %s\n", 'A' + submode,
                  row.toLatin1 ().constData ());

  QString const expected = QString::fromLatin1 (
      decodium::txmsg::encodeJt4 (message).msgsent).trimmed ();
  for (QString const& row : rows)
    if (row.contains (expected))
      return true;

  std::fprintf (stderr, "JT4%c decoder produced no matching row for '%s'\n",
                'A' + submode, expected.toLatin1 ().constData ());
  return false;
}

}  // namespace

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  QString const message = QStringLiteral ("CQ TESTA JN70");
  for (int submode = 0; submode <= 6; ++submode)
    {
      if (!run_case (message, submode))
        return 1;
    }
  std::printf ("JT4A-G waveform roundtrips passed\n");
  return 0;
}
