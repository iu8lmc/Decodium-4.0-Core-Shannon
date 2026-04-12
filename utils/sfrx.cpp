#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

extern "C" int ftx_superfox_decode_lines_c (short const* iwave,
                                            int nfqso,
                                            int ntol,
                                            char* lines_out,
                                            int line_stride,
                                            int max_lines,
                                            int* nsnr_out,
                                            float* freq_out,
                                            float* dt_out);

namespace
{
constexpr int kSuperFoxSampleCount {15 * 12000};
constexpr int kSuperFoxLineStride {64};
constexpr int kSuperFoxMaxLines {16};

struct WavFixture
{
  int sampleRate {0};
  std::vector<short> samples;
};

WavFixture read_pcm16_mono_wav (QString const& fileName)
{
  QFile file {fileName};
  if (!file.open (QIODevice::ReadOnly))
    {
      return {};
    }

  QByteArray const blob = file.readAll ();
  if (blob.size () < 12 || blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
    {
      return {};
    }

  bool have_fmt = false;
  bool have_data = false;
  quint16 audio_format = 0;
  quint16 channels = 0;
  quint32 sample_rate = 0;
  quint16 bits_per_sample = 0;
  QByteArray data_chunk;

  int pos = 12;
  while (pos + 8 <= blob.size ())
    {
      QByteArray const chunk_id = blob.mid (pos, 4);
      quint32 const chunk_size = qFromLittleEndian<quint32> (
          reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
      pos += 8;
      if (pos + static_cast<int> (chunk_size) > blob.size ())
        {
          return {};
        }

      if (chunk_id == "fmt ")
        {
          if (chunk_size < 16)
            {
              return {};
            }
          auto const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
          audio_format = qFromLittleEndian<quint16> (fmt);
          channels = qFromLittleEndian<quint16> (fmt + 2);
          sample_rate = qFromLittleEndian<quint32> (fmt + 4);
          bits_per_sample = qFromLittleEndian<quint16> (fmt + 14);
          have_fmt = true;
        }
      else if (chunk_id == "data")
        {
          data_chunk = blob.mid (pos, static_cast<int> (chunk_size));
          have_data = true;
        }

      pos += static_cast<int> ((chunk_size + 1u) & ~1u);
    }

  if (!have_fmt || !have_data || audio_format != 1u || channels != 1u || bits_per_sample != 16u)
    {
      return {};
    }

  int const sample_count = data_chunk.size () / 2;
  std::vector<short> samples (static_cast<size_t> (sample_count));
  auto const* raw = reinterpret_cast<uchar const*> (data_chunk.constData ());
  for (int i = 0; i < sample_count; ++i)
    {
      samples[static_cast<size_t> (i)] = static_cast<short> (qFromLittleEndian<qint16> (raw + 2 * i));
    }

  WavFixture fixture;
  fixture.sampleRate = static_cast<int> (sample_rate);
  fixture.samples = std::move (samples);
  return fixture;
}

QString extract_hhmmss (QString const& fileName)
{
  QRegularExpression const re {QStringLiteral (R"((?:^|_)(\d{6})(?:\.[Ww][Aa][Vv])$)")};
  QRegularExpressionMatch const match = re.match (QFileInfo {fileName}.fileName ());
  if (match.hasMatch ())
    {
      return match.captured (1);
    }
  return QStringLiteral ("000000");
}

void print_usage ()
{
  std::cout << "Usage:    sfrx fsync ftol infile [...]\n"
            << "          sfrx 775 10 240811_102400.wav\n"
            << "Reads one or more .wav files and calls the native SuperFox decoder on each.\n";
}
}

int main (int argc, char* argv[])
{
  if (argc < 4)
    {
      print_usage ();
      return 0;
    }

  bool ok_fsync = false;
  bool ok_ftol = false;
  int const nfqso = QString::fromLocal8Bit (argv[1]).toInt (&ok_fsync);
  int const ntol = QString::fromLocal8Bit (argv[2]).toInt (&ok_ftol);
  if (!ok_fsync || !ok_ftol)
    {
      print_usage ();
      return 1;
    }

  for (int index = 3; index < argc; ++index)
    {
      QString const wav_path = QFileInfo {QString::fromLocal8Bit (argv[index])}.absoluteFilePath ();
      WavFixture const wav = read_pcm16_mono_wav (wav_path);
      if (wav.sampleRate != 12000 || wav.samples.empty ())
        {
          std::cerr << "Unable to read 12 kHz mono PCM WAV: " << wav_path.toStdString () << '\n';
          continue;
        }

      std::vector<short> padded (kSuperFoxSampleCount, 0);
      int const copy = std::min<int> (static_cast<int> (wav.samples.size ()), kSuperFoxSampleCount);
      std::copy_n (wav.samples.begin (), copy, padded.begin ());

      std::array<char, kSuperFoxLineStride * kSuperFoxMaxLines> lines {};
      std::fill (lines.begin (), lines.end (), ' ');
      int nsnr = 0;
      float freq = 0.0f;
      float dt = 0.0f;
      int const count = ftx_superfox_decode_lines_c (padded.data (), nfqso, ntol,
                                                     lines.data (), kSuperFoxLineStride, kSuperFoxMaxLines,
                                                     &nsnr, &freq, &dt);
      if (count <= 0)
        {
          continue;
        }

      QString const hhmmss = extract_hhmmss (wav_path);
      for (int line_index = 0; line_index < std::min (count, kSuperFoxMaxLines); ++line_index)
        {
          QByteArray raw {lines.data () + line_index * kSuperFoxLineStride, kSuperFoxLineStride};
          while (!raw.isEmpty () && (raw.back () == ' ' || raw.back () == '\0'))
            {
              raw.chop (1);
            }
          if (raw.isEmpty ())
            {
              continue;
            }

          std::cout << hhmmss.toStdString ()
                    << std::setw (4) << nsnr
                    << std::fixed << std::setprecision (1)
                    << std::setw (5) << dt
                    << std::setw (5) << static_cast<int> (std::lround (freq))
                    << " ~ " << raw.constData () << '\n';
        }
    }

  return 0;
}
