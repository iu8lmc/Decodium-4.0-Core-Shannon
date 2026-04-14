#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "lib/init_random_seed.h"

extern "C" float gran_ ();

namespace
{
constexpr int kNsym = 103;
constexpr int kNsps = 576;
constexpr int kNzz = 72576;
constexpr int kSampleRate = 12000;
constexpr int kBitsPerSample = 16;

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error (message.toStdString ());
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
  put_u16 (blob, 20, 1u);
  put_u16 (blob, 22, 1u);
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

struct SignalSpec
{
  int snr {-17};
  float xdt {0.0f};
  int freq {0};
  QString message;
};

}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 3)
        {
          std::cout << "Usage:    ft4sim_mult nsigs nfiles\n"
                    << "Example:  ft4sim_mult  20     8\n";
          return 0;
        }

      int const nsigs = QString::fromLocal8Bit (argv[1]).toInt ();
      int const nfiles = QString::fromLocal8Bit (argv[2]).toInt ();
      if (nsigs <= 0 || nfiles <= 0)
        {
          fail (QStringLiteral ("nsigs and nfiles must be positive"));
        }

      std::ifstream input {"lib/ft4/messages.txt"};
      if (!input)
        {
          input.open ("messages.txt");
        }
      if (!input)
        {
          fail (QStringLiteral ("Cannot open file \"messages.txt\""));
        }

      float const fs = static_cast<float> (kSampleRate);
      float const dt = 1.0f / fs;
      float const bandwidth_ratio = 2500.0f / (fs / 2.0f);
      init_random_seed ();
      std::mt19937 rng {std::random_device {} ()};
      std::uniform_real_distribution<float> uniform {-0.5f, 0.5f};

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::string header;
          bool found = false;
          while (std::getline (input, header))
            {
              if (header.rfind ("File", 0) == 0)
                {
                  int file_id = std::atoi (header.substr (4).c_str ());
                  if (file_id == ifile)
                    {
                      found = true;
                      break;
                    }
                }
            }
          if (!found)
            {
              break;
            }

          std::vector<float> wave (kNzz, 0.0f);
          QString const fname = QStringLiteral ("000000_%1.wav").arg (ifile, 6, 10, QLatin1Char ('0'));

          for (int isig = 1; isig <= nsigs; ++isig)
            {
              std::streampos const record_start = input.tellg ();
              std::string line;
              if (!std::getline (input, line))
                {
                  break;
                }
              if (line.rfind ("File", 0) == 0)
                {
                  input.seekg (record_start);
                  break;
                }

              SignalSpec spec;
              if (line.size () >= 43)
                {
                  spec.snr = std::atoi (line.substr (34, 3).c_str ());
                  spec.xdt = uniform (rng);
                  spec.freq = std::atoi (line.substr (42, 5).c_str ());
                  spec.message = QString::fromLatin1 (line.c_str () + 48).leftJustified (37, QLatin1Char (' '), true);
                }
              else
                {
                  continue;
                }

              if (spec.snr < -17)
                {
                  spec.snr = -17;
                }

              auto const encoded = decodium::txmsg::encodeFt4 (spec.message);
              if (!encoded.ok || encoded.tones.size () != kNsym)
                {
                  continue;
                }

              float const f0 = float (spec.freq) * 960.0f / 576.0f;
              QVector<float> const wave0 = decodium::txwave::generateFt4Wave (encoded.tones.constData (),
                                                                              kNsym, kNsps,
                                                                              fs, f0);

              std::vector<float> tmp (kNzz, 0.0f);
              int const k0 = std::max (0, int (std::lround ((spec.xdt + 0.5f) / dt)));
              int const copy = std::min (int (wave0.size ()), kNzz - k0);
              for (int i = 0; i < copy; ++i)
                {
                  tmp[static_cast<size_t> (k0 + i)] = wave0.at (i);
                }

              float const sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * float (spec.snr));
              for (int i = 0; i < kNzz; ++i)
                {
                  wave[static_cast<size_t> (i)] += sig * tmp[static_cast<size_t> (i)];
                }

              std::cout << fname.toStdString ()
                        << std::setw (4) << isig
                        << std::setw (5) << spec.snr
                        << std::setw (5) << std::fixed << std::setprecision (1) << spec.xdt
                        << std::setw (6) << int (std::lround (f0))
                        << "  " << spec.message.trimmed ().toStdString () << '\n';
            }

          for (float& sample : wave)
            {
              sample += gran_ ();
              sample *= 30.0f;
            }

          bool clipped = false;
          std::vector<qint16> pcm (kNzz, 0);
          for (int i = 0; i < kNzz; ++i)
            {
              float const sample = wave[static_cast<size_t> (i)];
              clipped = clipped || std::abs (sample) > 32767.0f;
              int value = static_cast<int> (std::lround (sample));
              value = std::max (-32768, std::min (32767, value));
              pcm[static_cast<size_t> (i)] = static_cast<qint16> (value);
            }
          if (clipped)
            {
              std::cerr << "Warning - data will be clipped.\n";
            }

          QFile file {QFileInfo {fname}.absoluteFilePath ()};
          if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
            {
              fail (QStringLiteral ("cannot open output WAV \"%1\": %2").arg (fname, file.errorString ()));
            }
          QByteArray const blob = make_wav_blob (pcm);
          if (file.write (blob) != blob.size ())
            {
              fail (QStringLiteral ("failed to write WAV \"%1\"").arg (fname));
            }
          file.close ();
          std::cout << '\n';
        }
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }

  return 0;
}
