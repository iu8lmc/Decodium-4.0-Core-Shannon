#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "Modulator/FtxMessageEncoder.hpp"

extern "C" float gran_ ();

namespace
{
constexpr int kSampleRate = 12000;
constexpr int kBitsPerSample = 16;
constexpr int kNsps = 6;
constexpr double kTwoPi = 6.28318530717958647692;

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

std::string trim_right (QByteArray value)
{
  while (!value.isEmpty () && (value.back () == ' ' || value.back () == '\0'))
    {
      value.chop (1);
    }
  return value.toStdString ();
}

std::vector<float> make_pings (int tr_period, int npts, float width, float sig)
{
  std::vector<float> pings (static_cast<size_t> (npts), 0.0f);
  std::vector<float> t0 (static_cast<size_t> (std::max (tr_period, 1)), 0.0f);
  for (int i = 1; i < tr_period; ++i)
    {
      t0[static_cast<size_t> (i)] = static_cast<float> (i);
    }

  float const dt = 1.0f / float (kSampleRate);
  for (int i = 1; i <= npts; ++i)
    {
      int const iping = std::min (std::max (1, i / kSampleRate), tr_period - 1);
      double const t = (double (i) * dt - t0[static_cast<size_t> (iping)]) / width;
      float fac = 0.0f;
      if (t >= 0.0 && t <= 10.0)
        {
          fac = static_cast<float> (2.718 * t * std::exp (-t));
        }
      pings[static_cast<size_t> (i - 1)] = fac * sig;
    }
  return pings;
}

void print_usage ()
{
  std::cout << "Usage:   msk144sim       message      TRp freq width snr nfiles\n"
            << "Example: msk144sim \"K1ABC W9XYZ EN37\"  15 1500  0.12   2    1\n"
            << "         msk144sim \"K1ABC W9XYZ EN37\"  30 1500  2.5   15    1\n";
}
}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 7)
        {
          print_usage ();
          return 0;
        }

      QString const message = QString::fromLocal8Bit (argv[1]).toUpper ().simplified ();
      int const tr_period = QString::fromLocal8Bit (argv[2]).toInt ();
      float const freq = QString::fromLocal8Bit (argv[3]).toFloat ();
      float const width = QString::fromLocal8Bit (argv[4]).toFloat ();
      float const snrdb = QString::fromLocal8Bit (argv[5]).toFloat ();
      int const nfiles = std::max (1, QString::fromLocal8Bit (argv[6]).toInt ());

      int const npts = tr_period * kSampleRate;
      if (tr_period <= 1 || npts <= 0)
        {
          fail (QStringLiteral ("invalid TR period"));
        }

      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (message);
      if (!encoded.ok || encoded.tones.isEmpty ())
        {
          fail (QStringLiteral ("illegal MSK144 message"));
        }

      int nsym = 144;
      if (encoded.tones.size () > 40 && encoded.tones.at (40) < 0)
        {
          nsym = 40;
        }
      if (nsym <= 0)
        {
          fail (QStringLiteral ("illegal MSK144 symbol count"));
        }

      std::cout << "Requested message: " << message.toStdString () << '\n'
                << "Message sent     : " << trim_right (encoded.msgsent) << '\n'
                << "Tones:\n";
      for (int i = 0; i < nsym; ++i)
        {
          std::cout << encoded.tones.at (i);
          if ((i + 1) % 72 == 0)
            {
              std::cout << '\n';
            }
        }
      if (nsym % 72 != 0)
        {
          std::cout << '\n';
        }

      std::vector<float> waveform (static_cast<size_t> (npts), 0.0f);
      double const baud = 2000.0;
      double const dphi0 = kTwoPi * (double (freq) - 0.25 * baud) / double (kSampleRate);
      double const dphi1 = kTwoPi * (double (freq) + 0.25 * baud) / double (kSampleRate);
      double phi = 0.0;
      int const nreps = npts / (nsym * kNsps);
      int k = 0;
      for (int rep = 0; rep < nreps; ++rep)
        {
          for (int i = 0; i < nsym; ++i)
            {
              double const dphi = encoded.tones.at (i) == 0 ? dphi0 : dphi1;
              for (int j = 0; j < kNsps && k < npts; ++j)
                {
                  waveform[static_cast<size_t> (k++)] = static_cast<float> (std::cos (phi));
                  phi = std::fmod (phi + dphi, kTwoPi);
                  if (phi < 0.0)
                    {
                      phi += kTwoPi;
                    }
                }
            }
        }

      float const sig = std::sqrt (2.0f) * std::pow (10.0f, 0.05f * snrdb);
      std::vector<float> const pings = make_pings (tr_period, npts, width, sig);
      float const noise_scale = std::sqrt (6000.0f / 2500.0f);

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::vector<qint16> pcm (static_cast<size_t> (npts), 0);
          for (int i = 0; i < npts; ++i)
            {
              float const sample = pings[static_cast<size_t> (i)] * waveform[static_cast<size_t> (i)]
                                   + noise_scale * gran_ ();
              pcm[static_cast<size_t> (i)] = static_cast<qint16> (qBound (-32767, qRound (30.0f * sample), 32767));
            }

          QString const file_name = QStringLiteral ("000000_%1.wav").arg (ifile, 6, 10, QLatin1Char ('0'));
          QFile file {file_name};
          if (!file.open (QIODevice::WriteOnly))
            {
              fail (QStringLiteral ("unable to write %1").arg (file_name));
            }
          QByteArray const blob = make_wav_blob (pcm);
          if (file.write (blob) != blob.size ())
            {
              fail (QStringLiteral ("failed to write %1").arg (file_name));
            }
        }

      return 0;
    }
  catch (std::exception const& ex)
    {
      std::cerr << ex.what () << '\n';
      return 1;
    }
}
