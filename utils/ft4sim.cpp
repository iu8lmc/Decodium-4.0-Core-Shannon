#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtEndian>

#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <fftw3.h>

#include "Modulator/FtxMessageEncoder.hpp"
#include "lib/init_random_seed.h"

extern "C"
{
float gran_ ();
void watterson_ (fftwf_complex* c, int* npts, int* nsig, float* fs, float* delay, float* fspread);
void gen_ft4wave_ (int* itone, int* nsym, int* nsps, float* fsample, float* f0,
                   std::complex<float>* cwave, float* wave, int* icmplx, int* nwave);
}

namespace
{
constexpr int kNsym = 103;
constexpr int kNsps = 576;
constexpr int kNmax = 21 * 3456;
constexpr int kNz = kNsym * kNsps;
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

template <typename T>
std::vector<T> circular_shift (std::vector<T> const& input, int shift)
{
  std::vector<T> output (input.size ());
  if (input.empty ())
    {
      return output;
    }

  int const size = static_cast<int> (input.size ());
  int const wrapped = ((shift % size) + size) % size;
  for (int i = 0; i < size; ++i)
    {
      output[static_cast<size_t> ((i + wrapped) % size)] = input[static_cast<size_t> (i)];
    }
  return output;
}

QString fixed_37 (QByteArray const& msgsent)
{
  QByteArray clipped = msgsent.left (37);
  if (clipped.size () < 37)
    {
      clipped.append (QByteArray (37 - clipped.size (), ' '));
    }
  return QString::fromLatin1 (clipped);
}

void print_message_bits (QByteArray const& bits, int i3)
{
  std::cout << '\n';
  if (i3 == 1)
    {
      std::cout << "         mycall                         hiscall                    hisgrid\n";
      for (int i = 0; i < bits.size (); ++i)
        {
          std::cout << (bits.at (i) != 0 ? '1' : '0');
          if (i == 27 || i == 29 || i == 57 || i == 59 || i == 60 || i == 75)
            {
              std::cout << ' ';
            }
        }
      std::cout << '\n';
    }
  else
    {
      std::cout << "Message bits:\n";
      for (char bit : bits)
        {
          std::cout << (bit != 0 ? '1' : '0');
        }
      std::cout << '\n';
    }
}

void print_tones (QVector<int> const& tones)
{
  std::cout << "\nChannel symbols:\n";
  for (int tone : tones)
    {
      std::cout << tone;
    }
  std::cout << "\n\n";
}

}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 8)
        {
          std::cout << "Usage:    ft4sim \"message\"               f0   DT fdop del nfiles snr\n"
                    << "Examples: ft4sim \"CQ W9XYZ EN37\"        1500 0.0  0.1 1.0   10   -15\n"
                    << "          ft4sim \"K1ABC W9XYZ R 539 WI\" 1500 0.0  0.1 1.0   10   -15\n";
          return 0;
        }

      QString const message = QString::fromLocal8Bit (argv[1]);
      float const f0 = QString::fromLocal8Bit (argv[2]).toFloat ();
      float const xdt = QString::fromLocal8Bit (argv[3]).toFloat ();
      float const fspread = QString::fromLocal8Bit (argv[4]).toFloat ();
      float const delay = QString::fromLocal8Bit (argv[5]).toFloat ();
      int const nfiles = std::abs (QString::fromLocal8Bit (argv[6]).toInt ());
      float const snrdb = QString::fromLocal8Bit (argv[7]).toFloat ();
      if (nfiles <= 0)
        {
          fail (QStringLiteral ("nfiles must be positive"));
        }

      auto const encoded = decodium::txmsg::encodeFt4 (message);
      if (!encoded.ok || encoded.msgbits.size () != 77 || encoded.tones.size () != kNsym)
        {
          fail (QStringLiteral ("failed to encode FT4 message \"%1\"").arg (message));
        }

      float const fs = static_cast<float> (kSampleRate);
      float const dt = 1.0f / fs;
      float const txt = float ((kNsym + 2) * kNsps) / fs;
      float const bandwidth_ratio = 2500.0f / (fs / 2.0f);
      float sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * snrdb);
      if (snrdb > 90.0f)
        {
          sig = 1.0f;
        }

      std::array<int, kNsym> itone {};
      for (int i = 0; i < kNsym; ++i)
        {
          itone[static_cast<size_t> (i)] = encoded.tones.at (i);
        }

      std::vector<std::complex<float>> c0 (kNmax, std::complex<float> {});
      std::vector<float> wave_unused (kNmax, 0.0f);
      int nsym = kNsym;
      int nsps = kNsps;
      int icmplx = 1;
      int nwave = kNmax;
      float f0_copy = f0;
      float fsample = fs;
      gen_ft4wave_ (itone.data (), &nsym, &nsps, &fsample, &f0_copy,
                    c0.data (), wave_unused.data (), &icmplx, &nwave);

      int const kshift = static_cast<int> (std::lround ((xdt + 0.5f) / dt)) - kNsps;
      c0 = circular_shift (c0, -kshift);
      if (kshift > 0)
        {
          std::fill (c0.begin (), c0.begin () + std::min (kshift, kNmax), std::complex<float> {});
        }
      else if (kshift < 0)
        {
          int const tail = std::max (0, kNmax + kshift);
          std::fill (c0.begin () + tail, c0.end (), std::complex<float> {});
        }

      QString const msgsent37 = fixed_37 (encoded.msgsent);
      std::cout << "\nMessage: " << std::left << std::setw (37) << msgsent37.toStdString ()
                << "   i3.n3: " << encoded.i3 << '.' << encoded.n3 << '\n';
      std::cout << std::fixed << std::setprecision (3)
                << "f0:" << std::setw (9) << f0
                << "   DT:" << std::setw (6) << std::setprecision (2) << xdt
                << "   TxT:" << std::setw (6) << std::setprecision (1) << txt
                << "   SNR:" << std::setw (6) << snrdb << '\n';

      print_message_bits (encoded.msgbits, encoded.i3);
      print_tones (encoded.tones);

      init_random_seed ();

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::vector<std::complex<float>> c = c0;
          if (fspread != 0.0f || delay != 0.0f)
            {
              int npts = kNmax;
              int nsig = kNz;
              float delay_copy = delay;
              float fspread_copy = fspread;
              watterson_ (reinterpret_cast<fftwf_complex*> (c.data ()), &npts, &nsig,
                          &fsample, &delay_copy, &fspread_copy);
            }

          std::vector<float> wave (kNmax, 0.0f);
          for (int i = 0; i < kNmax; ++i)
            {
              wave[static_cast<size_t> (i)] = sig * c[static_cast<size_t> (i)].real ();
            }

          if (snrdb < 90.0f)
            {
              for (float& sample : wave)
                {
                  sample += gran_ ();
                }
            }

          if (snrdb < 90.0f)
            {
              for (float& sample : wave)
                {
                  sample *= 100.0f;
                }
            }
          else
            {
              float peak = 0.0f;
              for (float sample : wave)
                {
                  peak = std::max (peak, std::abs (sample));
                }
              if (peak > 0.0f)
                {
                  float const scale = 32766.9f / peak;
                  for (float& sample : wave)
                    {
                      sample *= scale;
                    }
                }
            }

          bool clipped = false;
          std::vector<qint16> pcm (kNmax, 0);
          for (int i = 0; i < kNmax; ++i)
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

          QString const fname = QStringLiteral ("000000_%1.wav").arg (ifile, 6, 10, QLatin1Char ('0'));
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

          std::cout << std::setw (4) << ifile
                    << std::setw (7) << std::fixed << std::setprecision (2) << xdt
                    << std::setw (8) << std::fixed << std::setprecision (2) << f0
                    << std::setw (7) << std::fixed << std::setprecision (1) << snrdb
                    << "  " << fname.toStdString () << '\n';
        }
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }

  return 0;
}
