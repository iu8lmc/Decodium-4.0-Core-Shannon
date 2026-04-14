#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QtEndian>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

namespace
{
  constexpr int kSampleRate {12000};
  constexpr int kFrameSamples {45000};
  constexpr int kNsps {288};
  constexpr float kDefaultFrequency {1000.0f};
  constexpr float kDefaultGain {0.85f};
  constexpr float kDefaultOffsetMs {600.0f};
  constexpr quint16 kPcmFormat {1};
  constexpr quint16 kMonoChannels {1};
  constexpr quint16 kBitsPerSample {16};

  [[noreturn]] void fail (QString const& message)
  {
    throw std::runtime_error {message.toStdString ()};
  }

  float parse_float_option (QCommandLineParser const& parser, QCommandLineOption const& option,
                            QString const& name)
  {
    bool ok = false;
    float const value = parser.value (option).toFloat (&ok);
    if (!ok)
      {
        fail (QStringLiteral ("invalid --%1 value").arg (name));
      }
    return value;
  }

  int parse_int_option (QCommandLineParser const& parser, QCommandLineOption const& option,
                        QString const& name)
  {
    bool ok = false;
    int const value = parser.value (option).toInt (&ok);
    if (!ok)
      {
        fail (QStringLiteral ("invalid --%1 value").arg (name));
      }
    return value;
  }

  void put_u16 (QByteArray& data, int offset, quint16 value)
  {
    qToLittleEndian<quint16> (value, reinterpret_cast<uchar*> (data.data () + offset));
  }

  void put_u32 (QByteArray& data, int offset, quint32 value)
  {
    qToLittleEndian<quint32> (value, reinterpret_cast<uchar*> (data.data () + offset));
  }

  QByteArray make_wav_blob (std::vector<qint16> const& samples)
  {
    int const data_size = static_cast<int> (samples.size () * sizeof (qint16));
    QByteArray blob (44 + data_size, '\0');

    std::copy_n ("RIFF", 4, blob.data ());
    put_u32 (blob, 4, static_cast<quint32> (blob.size () - 8));
    std::copy_n ("WAVE", 4, blob.data () + 8);
    std::copy_n ("fmt ", 4, blob.data () + 12);
    put_u32 (blob, 16, 16u);
    put_u16 (blob, 20, kPcmFormat);
    put_u16 (blob, 22, kMonoChannels);
    put_u32 (blob, 24, static_cast<quint32> (kSampleRate));
    put_u32 (blob, 28, static_cast<quint32> (kSampleRate * sizeof (qint16)));
    put_u16 (blob, 32, static_cast<quint16> (sizeof (qint16)));
    put_u16 (blob, 34, kBitsPerSample);
    std::copy_n ("data", 4, blob.data () + 36);
    put_u32 (blob, 40, static_cast<quint32> (data_size));

    char* output = blob.data () + 44;
    for (size_t i = 0; i < samples.size (); ++i)
      {
        qToLittleEndian<qint16> (samples[i],
                                 reinterpret_cast<uchar*> (output + static_cast<int> (i * sizeof (qint16))));
      }

    return blob;
  }

  float compute_signal_rms (std::vector<float> const& frame)
  {
    double sum_sq = 0.0;
    int count = 0;
    for (float sample : frame)
      {
        if (sample == 0.0f)
          {
            continue;
          }
        sum_sq += static_cast<double> (sample) * static_cast<double> (sample);
        ++count;
      }
    if (count == 0)
      {
        return 0.0f;
      }
    return static_cast<float> (std::sqrt (sum_sq / static_cast<double> (count)));
  }

  void add_awgn (std::vector<float>& frame, float snr_db, int seed)
  {
    float const signal_rms = compute_signal_rms (frame);
    if (signal_rms <= 0.0f)
      {
        return;
      }

    double const sigma = static_cast<double> (signal_rms) / std::pow (10.0, static_cast<double> (snr_db) / 20.0);
    std::mt19937 rng {static_cast<std::mt19937::result_type> (seed)};
    std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
    for (float& sample : frame)
      {
        sample += noise (rng);
      }
  }
}

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QCoreApplication::setApplicationName (QStringLiteral ("ft2_make_test_wav"));

  try
    {
      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Generate a synthetic 12000 Hz mono PCM FT2 WAV file using the C++ TX stack."));
      parser.addHelpOption ();

      QCommandLineOption message_option (
          QStringList {QStringLiteral ("message")},
          QStringLiteral ("FT2 message to encode."),
          QStringLiteral ("text"),
          QStringLiteral ("CQ TEST K1ABC FN42"));
      QCommandLineOption freq_option (
          QStringList {QStringLiteral ("freq")},
          QStringLiteral ("Carrier frequency in Hz inside the 0-3000 audio passband."),
          QStringLiteral ("hz"),
          QString::number (kDefaultFrequency, 'f', 1));
      QCommandLineOption gain_option (
          QStringList {QStringLiteral ("gain")},
          QStringLiteral ("Waveform gain in the range 0..1 before PCM conversion."),
          QStringLiteral ("value"),
          QString::number (kDefaultGain, 'f', 2));
      QCommandLineOption offset_option (
          QStringList {QStringLiteral ("offset-ms")},
          QStringLiteral ("Silence to prepend before the FT2 burst, in milliseconds."),
          QStringLiteral ("ms"),
          QString::number (kDefaultOffsetMs, 'f', 1));
      QCommandLineOption snr_option (
          QStringList {QStringLiteral ("snr-db")},
          QStringLiteral ("Optional AWGN SNR in dB relative to signal RMS. Omit for a clean file."),
          QStringLiteral ("db"));
      QCommandLineOption seed_option (
          QStringList {QStringLiteral ("seed")},
          QStringLiteral ("PRNG seed used when --snr-db is provided."),
          QStringLiteral ("n"),
          QStringLiteral ("12345"));

      parser.addOption (message_option);
      parser.addOption (freq_option);
      parser.addOption (gain_option);
      parser.addOption (offset_option);
      parser.addOption (snr_option);
      parser.addOption (seed_option);
      parser.addPositionalArgument (
          QStringLiteral ("output"),
          QStringLiteral ("Path to the generated 12000 Hz mono 16-bit FT2 WAV file."));

      parser.process (app);

      QStringList const positional = parser.positionalArguments ();
      if (positional.size () != 1)
        {
          fail (QStringLiteral ("expected exactly one positional output path"));
        }

      QString const output_path = QFileInfo {positional.front ()}.absoluteFilePath ();
      QString const message = parser.value (message_option);
      float const frequency = parse_float_option (parser, freq_option, QStringLiteral ("freq"));
      float const gain = parse_float_option (parser, gain_option, QStringLiteral ("gain"));
      float const offset_ms = parse_float_option (parser, offset_option, QStringLiteral ("offset-ms"));
      int const seed = parse_int_option (parser, seed_option, QStringLiteral ("seed"));

      if (frequency < 0.0f || frequency > 3000.0f)
        {
          fail (QStringLiteral ("--freq must be in the range 0..3000 Hz"));
        }
      if (gain <= 0.0f || gain > 1.0f)
        {
          fail (QStringLiteral ("--gain must be in the range (0, 1]"));
        }
      if (offset_ms < 0.0f)
        {
          fail (QStringLiteral ("--offset-ms must be non-negative"));
        }

      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt2 (message);
      if (!encoded.ok || encoded.tones.isEmpty ())
        {
          fail (QStringLiteral ("failed to encode FT2 message \"%1\"").arg (message));
        }

      QVector<float> const wave = decodium::txwave::generateFt2Wave (
          encoded.tones.constData (), encoded.tones.size (), kNsps, static_cast<float> (kSampleRate),
          frequency);
      if (wave.isEmpty ())
        {
          fail (QStringLiteral ("failed to generate FT2 waveform"));
        }

      int const offset_samples =
          static_cast<int> (std::lround (static_cast<double> (offset_ms) * kSampleRate / 1000.0));
      if (offset_samples < 0 || offset_samples + wave.size () > kFrameSamples)
        {
          fail (QStringLiteral ("waveform does not fit in a %1-sample FT2 frame with the requested offset")
                    .arg (kFrameSamples));
        }

      std::vector<float> frame (static_cast<size_t> (kFrameSamples), 0.0f);
      for (int i = 0; i < wave.size (); ++i)
        {
          frame[static_cast<size_t> (offset_samples + i)] = gain * wave[i];
        }

      if (parser.isSet (snr_option))
        {
          float const snr_db = parse_float_option (parser, snr_option, QStringLiteral ("snr-db"));
          add_awgn (frame, snr_db, seed);
        }

      std::vector<qint16> pcm (static_cast<size_t> (kFrameSamples), 0);
      for (int i = 0; i < kFrameSamples; ++i)
        {
          float const clipped = std::max (-1.0f, std::min (1.0f, frame[static_cast<size_t> (i)]));
          pcm[static_cast<size_t> (i)] =
              static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
        }

      QFile file {output_path};
      if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
        {
          fail (QStringLiteral ("cannot open \"%1\" for writing: %2")
                    .arg (output_path, file.errorString ()));
        }

      QByteArray const wav = make_wav_blob (pcm);
      if (file.write (wav) != wav.size ())
        {
          fail (QStringLiteral ("failed to write WAV file \"%1\": %2")
                    .arg (output_path, file.errorString ()));
        }
      file.close ();

      std::cout << "output: " << output_path.toStdString () << '\n';
      std::cout << "message: " << encoded.msgsent.trimmed ().toStdString () << '\n';
      std::cout << "tones: " << encoded.tones.size () << '\n';
      std::cout << "samples: " << kFrameSamples << '\n';
      std::cout << "freq_hz: " << frequency << '\n';
      std::cout << "offset_ms: " << offset_ms << '\n';
      if (parser.isSet (snr_option))
        {
          std::cout << "snr_db: " << parser.value (snr_option).toStdString () << '\n';
          std::cout << "seed: " << seed << '\n';
        }
      return 0;
    }
  catch (std::exception const& error)
    {
      std::cerr << "ft2_make_test_wav: " << error.what () << '\n';
      return 1;
    }
}
