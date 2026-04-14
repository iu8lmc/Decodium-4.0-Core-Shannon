#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtEndian>

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
int ftx_gen_ft8wave_complex_c (int const* itone, int nsym, int nsps, float bt,
                               float fsample, float f0, float* real_out,
                               float* imag_out, int nwave);
float gran_ ();
void watterson_ (fftwf_complex* c, int* npts, int* nsig, float* fs, float* delay, float* fspread);
}

namespace
{
constexpr int kNsym = 79;
constexpr int kNsps = 1920;
constexpr int kNwave = kNsym * kNsps;
constexpr int kNmax = 15 * 12000;
constexpr int kSampleRate = 12000;
constexpr int kBitsPerSample = 16;
constexpr int kDataStart = 6000;
constexpr int kDataStop = 157692;

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error (message.toStdString ());
}

double db (double x)
{
  if (x > 1.259e-10)
    {
      return 10.0 * std::log10 (x);
    }
  return -99.0;
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

void apply_watterson_profile (QString const& profile, float* fspread, float* delay, bool* matched)
{
  *matched = true;
  if (profile == QStringLiteral ("LQ"))
    {
      *fspread = 0.5f;
      *delay = 0.5f;
    }
  else if (profile == QStringLiteral ("LM"))
    {
      *fspread = 1.5f;
      *delay = 2.0f;
    }
  else if (profile == QStringLiteral ("LD"))
    {
      *fspread = 10.0f;
      *delay = 6.0f;
    }
  else if (profile == QStringLiteral ("MQ"))
    {
      *fspread = 0.1f;
      *delay = 0.5f;
    }
  else if (profile == QStringLiteral ("MM"))
    {
      *fspread = 0.5f;
      *delay = 1.0f;
    }
  else if (profile == QStringLiteral ("MD"))
    {
      *fspread = 1.0f;
      *delay = 2.0f;
    }
  else if (profile == QStringLiteral ("HQ"))
    {
      *fspread = 0.5f;
      *delay = 1.0f;
    }
  else if (profile == QStringLiteral ("HM"))
    {
      *fspread = 10.0f;
      *delay = 3.0f;
    }
  else if (profile == QStringLiteral ("HD"))
    {
      *fspread = 30.0f;
      *delay = 7.0f;
    }
  else
    {
      *matched = false;
    }
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
          std::cout << "Usage:    ft8sim \"message\"                 f0     DT fdop del nfiles snr\n"
                    << "Examples: ft8sim \"K1ABC W9XYZ EN37\"       1500.0 0.0  0.5 1.0   10   -18\n"
                    << "          ft8sim \"K1ABC W9XYZ EN37\"       1500.0 0.0  MM  1.0   10   -18\n"
                    << "          ft8sim \"WA9XYZ/R KA1ABC/R FN42\" 1500.0 0.0  0.1 1.0   10   -18\n"
                    << "          ft8sim \"K1ABC RR73; W9XYZ <KH1/KH7Z> -11\" 300 0 0 0 25 1 -10\n"
                    << "          ft8sim \"<G4ABC/P> <PA9XYZ> R 570007 JO22DB\" 1500 0 0 0 1 -10\n";
          return 0;
        }

      QString const message = QString::fromLocal8Bit (argv[1]);
      float f0 = QString::fromLocal8Bit (argv[2]).toFloat ();
      float const xdt = QString::fromLocal8Bit (argv[3]).toFloat ();
      QString const profile_or_spread = QString::fromLocal8Bit (argv[4]).trimmed ().toUpper ();
      float delay = 0.0f;
      float fspread = 0.0f;
      bool matched_profile = false;
      apply_watterson_profile (profile_or_spread, &fspread, &delay, &matched_profile);
      if (!matched_profile)
        {
          fspread = profile_or_spread.toFloat ();
          delay = QString::fromLocal8Bit (argv[5]).toFloat ();
        }

      int nfiles = std::abs (QString::fromLocal8Bit (argv[6]).toInt ());
      float const snrdb = QString::fromLocal8Bit (argv[7]).toFloat ();

      if (nfiles <= 0)
        {
          fail (QStringLiteral ("nfiles must be positive"));
        }

      decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt8 (message);
      if (!encoded.ok || encoded.msgbits.size () != 77 || encoded.tones.size () != kNsym)
        {
          fail (QStringLiteral ("failed to encode FT8 message \"%1\"").arg (message));
        }

      if (f0 < 100.0f)
        {
          f0 = 1500.0f;
        }

      float const fs = static_cast<float> (kSampleRate);
      float const dt = 1.0f / fs;
      float const txt = static_cast<float> (kNwave) / fs;
      float const baud = 1.0f / (static_cast<float> (kNsps) * dt);
      float const bw = 8.0f * baud;
      float const bandwidth_ratio = 2500.0f / (fs / 2.0f);
      float sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * snrdb);
      if (snrdb > 90.0f)
        {
          sig = 1.0f;
        }

      std::vector<float> cwave_real (kNwave, 0.0f);
      std::vector<float> cwave_imag (kNwave, 0.0f);
      if (!ftx_gen_ft8wave_complex_c (encoded.tones.constData (), kNsym, kNsps, 2.0f, fs, f0,
                                      cwave_real.data (), cwave_imag.data (), kNwave))
        {
          fail (QStringLiteral ("failed to synthesize FT8 waveform"));
        }

      std::vector<std::complex<float>> cwave (kNwave);
      for (int i = 0; i < kNwave; ++i)
        {
          cwave[static_cast<size_t> (i)] = {cwave_real[static_cast<size_t> (i)],
                                            cwave_imag[static_cast<size_t> (i)]};
        }

      QString const msgsent37 = fixed_37 (encoded.msgsent);

      std::cout << '\n';
      std::cout << "Decoded message: " << std::left << std::setw (37) << msgsent37.toStdString ()
                << "   i3.n3: " << encoded.i3 << '.' << encoded.n3 << '\n';
      std::cout << std::fixed << std::setprecision (3)
                << "f0:" << std::setw (9) << f0
                << "   DT:" << std::setw (6) << std::setprecision (2) << xdt
                << "   TxT:" << std::setw (6) << std::setprecision (1) << txt
                << "   SNR:" << std::setw (6) << snrdb
                << "  BW:" << std::setw (4) << bw << '\n';
      std::cout << std::fixed << std::setprecision (3)
                << "Fspread:" << std::setw (7) << fspread
                << " Hz   Delay:" << std::setw (7) << delay << " ms\n";

      print_message_bits (encoded.msgbits, encoded.i3);
      print_tones (encoded.tones);

      init_random_seed ();

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::vector<std::complex<float>> c0 (kNmax, {0.0f, 0.0f});
          std::copy (cwave.begin (), cwave.end (), c0.begin ());

          int const shift = -static_cast<int> (std::lround ((xdt + 0.5f) / dt));
          c0 = circular_shift (c0, shift);

          if (fspread != 0.0f || delay != 0.0f)
            {
              int npts = kNmax;
              int nsig = kNwave;
              float delay_copy = delay;
              float fspread_copy = fspread;
              watterson_ (reinterpret_cast<fftwf_complex*> (c0.data ()), &npts, &nsig,
                          const_cast<float*> (&fs), &delay_copy, &fspread_copy);
            }

          std::vector<float> wave (kNmax, 0.0f);
          for (int i = 0; i < kNmax; ++i)
            {
              wave[static_cast<size_t> (i)] = sig * c0[static_cast<size_t> (i)].imag ();
            }

          double psig = 0.0;
          double pnoise = 0.0;
          if (snrdb < 90.0f)
            {
              for (int i = 0; i < kNmax; ++i)
                {
                  float const xnoise = gran_ ();
                  if (i >= kDataStart && i < kDataStop)
                    {
                      float const sample = wave[static_cast<size_t> (i)];
                      psig += static_cast<double> (sample) * static_cast<double> (sample);
                      pnoise += static_cast<double> (xnoise) * static_cast<double> (xnoise);
                    }
                  wave[static_cast<size_t> (i)] += xnoise;
                }
            }

          pnoise *= bandwidth_ratio;
          double const snr_2500 = db (pnoise > 0.0 ? psig / pnoise : 0.0);

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
                    << "  " << std::left << std::setw (17) << fname.toStdString ()
                    << std::right << std::setw (8) << std::fixed << std::setprecision (2) << snr_2500
                    << '\n';
        }
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }

  return 0;
}
