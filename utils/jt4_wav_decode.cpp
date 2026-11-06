#include "Detector/JT4Decoder.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QVector>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace
{
constexpr int kDecoderSampleRate = 12000;
constexpr int kDecoderSamples = 52 * kDecoderSampleRate;

struct WavData
{
  int sampleRate {};
  QVector<short> samples;
};

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
}

WavData readWav (QString const& path)
{
  QFile file {path};
  if (!file.open (QIODevice::ReadOnly))
    fail (QStringLiteral ("Cannot open WAV: %1").arg (file.errorString ()));

  QByteArray const blob = file.readAll ();
  if (blob.size () < 12 || blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
    fail (QStringLiteral ("Input is not a RIFF/WAVE file"));

  quint16 audioFormat = 0;
  quint16 channels = 0;
  quint16 bitsPerSample = 0;
  quint32 sampleRate = 0;
  QByteArray pcm;
  for (int pos = 12; pos + 8 <= blob.size (); )
    {
      QByteArray const id = blob.mid (pos, 4);
      quint32 const size = qFromLittleEndian<quint32> (
          reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
      pos += 8;
      if (pos + static_cast<int> (size) > blob.size ())
        fail (QStringLiteral ("Truncated WAV chunk %1").arg (QString::fromLatin1 (id)));
      if (id == "fmt " && size >= 16)
        {
          auto const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
          audioFormat = qFromLittleEndian<quint16> (fmt);
          channels = qFromLittleEndian<quint16> (fmt + 2);
          sampleRate = qFromLittleEndian<quint32> (fmt + 4);
          bitsPerSample = qFromLittleEndian<quint16> (fmt + 14);
        }
      else if (id == "data")
        {
          pcm = blob.mid (pos, static_cast<int> (size));
        }
      pos += static_cast<int> ((size + 1u) & ~1u);
    }

  if (audioFormat != 1 || channels != 1 || bitsPerSample != 16 || sampleRate == 0 || pcm.isEmpty ())
    fail (QStringLiteral ("WAV must be PCM mono 16-bit with a data chunk"));

  WavData result;
  result.sampleRate = static_cast<int> (sampleRate);
  result.samples.resize (pcm.size () / 2);
  auto const* raw = reinterpret_cast<uchar const*> (pcm.constData ());
  for (int i = 0; i < result.samples.size (); ++i)
    result.samples[i] = static_cast<short> (qFromLittleEndian<qint16> (raw + 2 * i));
  return result;
}

QVector<short> toDecoderRate (WavData const& wav)
{
  QVector<short> result (kDecoderSamples, 0);
  if (wav.samples.isEmpty ())
    return result;

  int const sourceSamples = static_cast<int> (wav.samples.size ());
  int const outputSamples = std::min (
      static_cast<int> (result.size ()),
      static_cast<int> (std::floor (static_cast<double> (sourceSamples)
                                    * kDecoderSampleRate / wav.sampleRate)));
  for (int i = 0; i < outputSamples; ++i)
    {
      double const source = static_cast<double> (i) * wav.sampleRate / kDecoderSampleRate;
      int const left = std::min (static_cast<int> (source), sourceSamples - 1);
      int const right = std::min (left + 1, sourceSamples - 1);
      double const fraction = source - left;
      result[i] = static_cast<short> (std::lround (
          (1.0 - fraction) * wav.samples[left] + fraction * wav.samples[right]));
    }
  return result;
}
}

int main (int argc, char** argv)
try
{
  QCoreApplication app {argc, argv};
  QCommandLineParser parser;
  parser.addHelpOption ();
  QCommandLineOption frequency {{"f", "frequency"}, "Audio frequency in Hz.", "HZ", "1500"};
  QCommandLineOption submode {{"b", "submode"}, "JT4 submode A through G.", "LETTER", "A"};
  QCommandLineOption tolerance {{"t", "tolerance"}, "Frequency tolerance in Hz.", "HZ", "50"};
  QCommandLineOption depth {{"d", "depth"}, "Decode depth.", "LEVEL", "3"};
  parser.addOption (frequency);
  parser.addOption (submode);
  parser.addOption (tolerance);
  parser.addOption (depth);
  parser.addPositionalArgument ("wav", "Input WAV file.");
  parser.process (app);
  if (parser.positionalArguments ().size () != 1)
    fail (QStringLiteral ("Specify exactly one WAV file"));

  WavData const wav = readWav (parser.positionalArguments ().front ());
  QVector<short> const audio = toDecoderRate (wav);
  QString const submodeValue = parser.value (submode).trimmed ().toUpper ();
  int const submodeIndex = submodeValue.isEmpty () ? 0 : qBound (0, submodeValue.at (0).unicode () - u'A', 6);

  decodium::legacyjt::DecodeRequest request;
  request.serial = 1;
  request.mode = QStringLiteral ("JT4");
  request.audio = audio;
  request.nutc = 175700;
  request.nfqso = parser.value (frequency).toInt ();
  request.ntol = parser.value (tolerance).toInt ();
  request.ndepth = parser.value (depth).toInt ();
  request.nsubmode = submodeIndex;
  request.newdat = 1;
  request.mycall = QByteArrayLiteral ("TESTA");
  request.hiscall = QByteArrayLiteral ("TESTB");
  request.hisgrid = QByteArrayLiteral ("JN70");
  request.tempDir = QDir::tempPath ().toLocal8Bit ();
  request.dataDir = QDir::currentPath ().toLocal8Bit ();

  std::printf ("JT4 WAV: input=%dHz samples=%d decoder=%dHz samples=%d submode=%c center=%dHz tolerance=%dHz depth=%d\n",
               wav.sampleRate, static_cast<int> (wav.samples.size ()),
               kDecoderSampleRate, static_cast<int> (audio.size ()),
               'A' + submodeIndex, request.nfqso, request.ntol, request.ndepth);
  decodium::jt4::AverageState state;
  QStringList const rows = decodium::jt4::decode_async_jt4 (request, &state);
  for (QString const& row : rows)
    std::puts (row.toLocal8Bit ().constData ());
  std::printf ("<DecodeFinished> rows=%d\n", static_cast<int> (rows.size ()));
  return rows.isEmpty () ? 2 : 0;
}
catch (std::exception const& error)
{
  std::fprintf (stderr, "%s\n", error.what ());
  return 1;
}
