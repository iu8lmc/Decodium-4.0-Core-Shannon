#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtEndian>

#include <algorithm>
#include <array>
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

extern "C"
{
float gran_ ();
float rran_ ();
void watterson_ (fftwf_complex* c, int* npts, int* nsig, float* fs, float* delay, float* fspread);
void four2a_ (std::complex<float>* a, int* nfft, int* ndim, int* isign, int* iform);
void lorentzian_fading_ (std::complex<float>* c, int* npts, float* fs, float* fspread);
void gen_fst4wave_ (int* itone, int* nsym, int* nsps, int* nwave, float* fsample,
                    int* hmod, float* f0, int* icmplx, std::complex<float>* cwave, float* wave);
}

namespace
{
constexpr int kFst4Symbols = 160;
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

int nsps_for_period (int nsec)
{
  switch (nsec)
    {
    case 15: return 720;
    case 30: return 1680;
    case 60: return 3888;
    case 120: return 8200;
    case 300: return 21504;
    case 900: return 66560;
    case 1800: return 134400;
    default: return 0;
    }
}

std::string trim_right (QByteArray value)
{
  while (!value.isEmpty () && (value.back () == ' ' || value.back () == '\0'))
    {
      value.chop (1);
    }
  return value.toStdString ();
}

void print_usage ()
{
  std::cout << "Usage:    fst4sim \"message\"        TRsec f0   DT  fdop del nfiles snr W\n"
            << "Examples: fst4sim \"K1JT K9AN EN50\"  60  1500 0.0   0.1 1.0   10   -15 F\n"
            << "W (T or F) argument is hint to encoder to use WSPR message when there is ambiguity\n";
}

void print_message_bits (decodium::txmsg::EncodedMessage const& encoded)
{
  std::cout << '\n';
  if (encoded.i3 == 1)
    {
      std::cout << "         mycall                         hiscall                    hisgrid\n";
      for (int i = 0; i < 77 && i < encoded.msgbits.size (); ++i)
        {
          std::cout << (encoded.msgbits.at (i) != 0 ? '1' : '0');
          if (i == 27 || i == 29 || i == 57 || i == 59 || i == 60 || i == 75)
            {
              std::cout << ' ';
            }
        }
      std::cout << '\n';
      return;
    }

  if (encoded.i3 == 0 && encoded.n3 == 6)
    {
      std::cout << "50-bit message                                     CRC24\n";
      for (int i = 0; i < 50 && i < encoded.msgbits.size (); ++i)
        {
          std::cout << (encoded.msgbits.at (i) != 0 ? '1' : '0');
        }
      std::cout << ' ';
      for (int i = 50; i < 74 && i < encoded.msgbits.size (); ++i)
        {
          std::cout << (encoded.msgbits.at (i) != 0 ? '1' : '0');
        }
      std::cout << "\n01234567890123456789012345678901234567890123456789\n";
      return;
    }

  std::cout << "Message bits:\n";
  for (int i = 0; i < encoded.msgbits.size (); ++i)
    {
      std::cout << (encoded.msgbits.at (i) != 0 ? '1' : '0');
      if (i == 76 && encoded.msgbits.size () > 77)
        {
          std::cout << ' ';
        }
    }
  std::cout << '\n';
}

void print_tones (QVector<int> const& tones)
{
  std::cout << "\nChannel symbols:\n";
  for (int i = 0; i < tones.size (); ++i)
    {
      std::cout << tones.at (i);
      if ((i + 1) % 80 == 0)
        {
          std::cout << '\n';
        }
    }
  if (tones.size () % 80 != 0)
    {
      std::cout << '\n';
    }
  std::cout << '\n';
}

extern "C" void lorentzian_fading_ (std::complex<float>* c, int* npts, float* fs, float* fspread)
{
  if (!c || !npts || !fs || !fspread || *npts <= 0 || *fspread <= 0.0f)
    {
      return;
    }

  int const total = *npts;
  float const twopi = 8.0f * std::atan (1.0f);
  float const df = *fs / total;
  int const nh = total / 2;
  std::vector<std::complex<float>> cspread (static_cast<size_t> (total), std::complex<float> {});
  cspread[0] = std::complex<float> {1.0f, 0.0f};
  if (nh < total)
    {
      cspread[static_cast<size_t> (nh)] = std::complex<float> {};
    }

  constexpr float b = 6.0f;
  for (int i = 1; i <= nh; ++i)
    {
      float const f = i * df;
      float const x = b * f / *fspread;
      float amplitude = 0.0f;
      if (x < 3.0f)
        {
          amplitude = std::sqrt (1.111f / (1.0f + x * x) - 0.1f);
          float const phi1 = twopi * rran_ ();
          cspread[static_cast<size_t> (i)] =
              amplitude * std::complex<float> {std::cos (phi1), std::sin (phi1)};
        }
      if (i < total)
        {
          std::complex<float> z {};
          if (x < 3.0f)
            {
              float const phi2 = twopi * rran_ ();
              z = amplitude * std::complex<float> {std::cos (phi2), std::sin (phi2)};
            }
          cspread[static_cast<size_t> (total - i)] = z;
        }
    }

  int nfft = total;
  int ndim = 1;
  int isign = 1;
  int iform = 1;
  four2a_ (cspread.data (), &nfft, &ndim, &isign, &iform);

  float power = 0.0f;
  for (std::complex<float> const& value : cspread)
    {
      power += std::norm (value);
    }
  float const average_power = power / total;
  if (!(average_power > 0.0f))
    {
      return;
    }

  float const factor = std::sqrt (1.0f / average_power);
  for (int i = 0; i < total; ++i)
    {
      c[i] *= factor * cspread[static_cast<size_t> (i)];
    }
}
}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 10)
        {
          print_usage ();
          return 0;
        }

      QString const message = QString::fromLocal8Bit (argv[1]);
      int const nsec = QString::fromLocal8Bit (argv[2]).toInt ();
      float const f00 = QString::fromLocal8Bit (argv[3]).toFloat ();
      float const xdt = QString::fromLocal8Bit (argv[4]).toFloat ();
      float const fspread = QString::fromLocal8Bit (argv[5]).toFloat ();
      float const delay = QString::fromLocal8Bit (argv[6]).toFloat ();
      int const nfiles = std::abs (QString::fromLocal8Bit (argv[7]).toInt ());
      float const snrdb = QString::fromLocal8Bit (argv[8]).toFloat ();
      QString const wspr_arg = QString::fromLocal8Bit (argv[9]).trimmed ().toUpper ();
      bool const wspr_hint = (wspr_arg == QStringLiteral ("T")
                              || wspr_arg == QStringLiteral ("1")
                              || wspr_arg == QStringLiteral ("TRUE"));

      if (nfiles <= 0)
        {
          fail (QStringLiteral ("nfiles must be positive"));
        }

      int const nsps = nsps_for_period (nsec);
      if (nsps == 0)
        {
          fail (QStringLiteral ("invalid TR sequence length"));
        }

      decodium::txmsg::EncodedMessage const encoded =
          decodium::txmsg::encodeFst4WithHint (message, wspr_hint, false);
      if (!encoded.ok || encoded.tones.size () != kFst4Symbols || encoded.msgbits.size () != 101)
        {
          fail (QStringLiteral ("failed to encode FST4/FST4W message \"%1\"").arg (message));
        }

      float const fs = static_cast<float> (kSampleRate);
      float const dt = 1.0f / fs;
      int const nmax = nsec * kSampleRate;
      int const nz = nsps * kFst4Symbols;
      int const nwave = std::max (nmax, (kFst4Symbols + 2) * nsps);
      float const txt = float (nz) * dt;
      float const baud = fs / float (nsps);
      float const bandwidth_ratio = 2500.0f / (fs / 2.0f);
      float sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * snrdb);
      if (snrdb > 90.0f)
        {
          sig = 1.0f;
        }

      std::vector<int> itone (kFst4Symbols, 0);
      for (int i = 0; i < kFst4Symbols; ++i)
        {
          itone[static_cast<size_t> (i)] = encoded.tones.at (i);
        }

      std::vector<std::complex<float>> c0 (static_cast<size_t> (nwave), std::complex<float> {});
      std::vector<float> wave_unused (static_cast<size_t> (nwave), 0.0f);
      int nsym = kFst4Symbols;
      int nsps_copy = nsps;
      int hmod = 1;
      int nwave_copy = nwave;
      float fsample = fs;
      float f0 = f00 + 1.5f * hmod * baud;
      int icmplx = 1;
      gen_fst4wave_ (itone.data (), &nsym, &nsps_copy, &nwave_copy, &fsample, &hmod, &f0,
                     &icmplx, c0.data (), wave_unused.data ());

      int kshift = static_cast<int> (std::lround ((xdt + 1.0f) / dt));
      if (nsec == 15)
        {
          kshift = static_cast<int> (std::lround ((xdt + 0.5f) / dt));
        }
      c0 = circular_shift (c0, -kshift);
      if (kshift > 0)
        {
          std::fill (c0.begin (), c0.begin () + std::min (kshift, nwave), std::complex<float> {});
        }
      else if (kshift < 0)
        {
          int const tail = std::max (0, nmax + kshift);
          std::fill (c0.begin () + tail, c0.begin () + nmax, std::complex<float> {});
        }

      std::cout << "\nMessage: " << std::left << std::setw (37) << trim_right (encoded.msgsent)
                << "W:" << std::boolalpha << wspr_hint
                << " iwspr:" << ((encoded.i3 == 0 && encoded.n3 == 6) ? 1 : 0) << '\n';
      std::cout << std::fixed << std::setprecision (3)
                << "f0:" << std::setw (9) << f00
                << "   DT:" << std::setw (6) << std::setprecision (2) << xdt
                << "   TxT:" << std::setw (6) << std::setprecision (1) << txt
                << "   SNR:" << std::setw (6) << snrdb << '\n';

      print_message_bits (encoded);
      print_tones (encoded.tones);

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::vector<std::complex<float>> c = c0;
          if (fspread > 0.0f || delay != 0.0f)
            {
              int npts = nwave;
              int nsig = nz;
              float delay_copy = delay;
              float fspread_copy = fspread;
              watterson_ (reinterpret_cast<fftwf_complex*> (c.data ()), &npts, &nsig,
                          &fsample, &delay_copy, &fspread_copy);
            }
          else if (fspread < 0.0f)
            {
              int npts = nwave;
              float spread = -fspread;
              lorentzian_fading_ (c.data (), &npts, &fsample, &spread);
            }

          std::vector<float> wave (static_cast<size_t> (nwave), 0.0f);
          for (int i = 0; i < nwave; ++i)
            {
              wave[static_cast<size_t> (i)] = sig * c[static_cast<size_t> (i)].imag ();
            }
          if (snrdb < 90.0f)
            {
              for (int i = 0; i < nmax; ++i)
                {
                  wave[static_cast<size_t> (i)] += gran_ ();
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
          std::vector<qint16> pcm (static_cast<size_t> (nmax), 0);
          for (int i = 0; i < nmax; ++i)
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

          QString const fname = nmax / kSampleRate <= 30
              ? QStringLiteral ("000000_%1.wav").arg (ifile, 6, 10, QLatin1Char ('0'))
              : QStringLiteral ("000000_%1.wav").arg (ifile, 4, 10, QLatin1Char ('0'));
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
                    << std::setw (8) << std::fixed << std::setprecision (2) << f00
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
