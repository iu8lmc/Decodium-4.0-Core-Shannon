#include <QByteArray>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

namespace
{

constexpr int kSampleRate {12000};
constexpr int kSeconds {15};
constexpr int kNmax {kSampleRate * kSeconds};
constexpr int kLeadIn {6000};
constexpr int kBitsPerSample {16};

[[noreturn]] void fail (char const* message)
{
  throw std::runtime_error {message};
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

void write_wav (char const* path, std::vector<qint16> const& samples)
{
  QByteArray const blob = make_wav_blob (samples);
  std::ofstream out {path, std::ios::binary};
  if (!out)
    {
      fail ("unable to open output wav");
    }
  out.write (blob.constData (), blob.size ());
}

void add_wave (std::vector<float>& data, QVector<float> const& wave, float gain)
{
  int const count = std::min<int> (wave.size (), static_cast<int> (data.size ()) - kLeadIn);
  for (int i = 0; i < count; ++i)
    {
      data[static_cast<size_t> (kLeadIn + i)] += gain * wave.at (i);
    }
}

void add_rtty (std::vector<float>& data, float sig)
{
  double const kTwopi = 8.0 * std::atan (1.0);
  float const dt = 1.0f / float (kSampleRate);
  float phi = 0.0f;
  float dphi = 0.0f;
  int j0 = -1;
  std::mt19937 rng {0xdec01du};
  std::uniform_real_distribution<float> dist {0.0f, 1.0f};

  for (int i = kLeadIn; i < kNmax - kLeadIn; ++i)
    {
      int const j = qRound (float (i + 1) * dt / 0.022f);
      if (j != j0)
        {
          float f0 = 1415.0f;
          if (dist (rng) > 0.5f)
            {
              f0 = 1585.0f;
            }
          dphi = kTwopi * f0 * dt;
          j0 = j;
        }
      phi += dphi;
      if (phi > kTwopi)
        {
          phi -= float (kTwopi);
        }
      data[static_cast<size_t> (i)] += sig * std::sin (phi);
    }
}

std::vector<qint16> quantize (std::vector<float> const& data)
{
  float datmax = 0.0f;
  for (float value : data)
    {
      datmax = std::max (datmax, std::fabs (value));
    }
  if (datmax <= 0.0f)
    {
      datmax = 1.0f;
    }

  std::vector<qint16> out (data.size ());
  for (size_t i = 0; i < data.size (); ++i)
    {
      float const scaled = 32767.0f * data[i] / datmax;
      long sample = std::lround (scaled);
      if (sample < -32767l)
        {
          sample = -32767l;
        }
      else if (sample > 32767l)
        {
          sample = 32767l;
        }
      out[i] = static_cast<qint16> (sample);
    }
  return out;
}

}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 2)
        {
          std::cout << "Usage: rtty_spec <snr>\n";
          return 0;
        }

      float const snrdb = QByteArray {argv[1]}.toFloat ();
      float const sig = std::pow (10.0f, 0.05f * snrdb);

      std::vector<float> data (kNmax, 0.0f);
      std::mt19937 rng {0x5eed1234u};
      std::normal_distribution<float> noise {0.0f, 1.0f};
      for (float& sample : data)
        {
          sample = noise (rng);
        }

      add_rtty (data, sig);

      QString const message = QStringLiteral ("WB9XYZ KA2ABC FN42");

      auto const ft8Encoded = decodium::txmsg::encodeFt8 (message);
      if (!ft8Encoded.ok || ft8Encoded.tones.size () != 79)
        {
          fail ("failed to encode FT8 test message");
        }
      add_wave (data, decodium::txwave::generateFt8Wave (ft8Encoded.tones.constData (), 79, 1920,
                                                         99.0f, 12000.0f, 3500.0f), sig);
      add_wave (data, decodium::txwave::generateFt8Wave (ft8Encoded.tones.constData (), 79, 1920,
                                                         2.0f, 12000.0f, 4000.0f), sig);

      auto const ft4Encoded = decodium::txmsg::encodeFt4 (message);
      if (!ft4Encoded.ok || ft4Encoded.tones.size () != 103)
        {
          fail ("failed to encode FT4 test message");
        }
      add_wave (data, decodium::txwave::generateFt4Wave (ft4Encoded.tones.constData (), 103, 576,
                                                         12000.0f, 4500.0f), sig);

      write_wav ("000000_000001.wav", quantize (data));
      return 0;
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
