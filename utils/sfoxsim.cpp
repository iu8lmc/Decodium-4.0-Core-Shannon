#include <QByteArray>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#include <fftw3.h>

#include "Modulator/FtxWaveformGenerator.hpp"
#include "commons.h"

extern "C" {
float gran_ ();
void watterson_ (fftwf_complex* c, int* npts, int* nsig, float* fs, float* delay, float* fspread);
void sfox_gen_gfsk_ (int* idat, float* f0, int* isync, int* itone, std::complex<float>* cdat);
}

namespace
{
constexpr int kSampleRate = 12000;
constexpr int kWaveSamples = 15 * 12000;
constexpr int kBitsPerSample = 16;
constexpr int kSuperFoxSymbols = 151;
constexpr float kBandwidthRatio = 2500.0f / float (kSampleRate);

std::array<int, 24> const kSyncPositions {{
  1, 2, 4, 7, 11, 16, 22, 29, 37, 39, 42, 43,
  45, 48, 52, 57, 63, 70, 78, 80, 83, 84, 86, 89
}};

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

void apply_watterson_profile (QString const& profile, float* fspread, float* delay)
{
  *fspread = 0.0f;
  *delay = 0.0f;
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

QString normalize_otp_key (QString const& raw)
{
  QString trimmed = raw.trimmed ();
  if (trimmed.startsWith (QStringLiteral ("OTP:"), Qt::CaseInsensitive))
    {
      return trimmed;
    }
  QString digits;
  for (QChar const ch : trimmed)
    {
      if (ch.isDigit ())
        {
          digits.append (ch);
        }
    }
  if (digits.size () < 6)
    {
      digits = digits.rightJustified (6, QLatin1Char {'0'});
    }
  else
    {
      digits = digits.right (6);
    }
  return QStringLiteral ("OTP:%1").arg (digits);
}

void clear_fox_state ()
{
  std::fill_n (foxcom_.wave, static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])), 0.0f);
  std::fill_n (&foxcom_.cmsg[0][0], static_cast<int> (sizeof (foxcom_.cmsg)), ' ');
  std::fill_n (foxcom_.textMsg, static_cast<int> (sizeof (foxcom_.textMsg)), ' ');
  std::fill_n (&foxcom3_.cmsg2[0][0], static_cast<int> (sizeof (foxcom3_.cmsg2)), ' ');
  std::fill_n (foxcom3_.itone3, 151, 0);
  foxcom_.nslots = 0;
  foxcom_.nfreq = 750;
  foxcom_.bMoreCQs = false;
  foxcom_.bSendMsg = false;
}

QString format_report (int report)
{
  return QStringLiteral ("%1").arg (report, 3, 10, QLatin1Char {' '}).replace (QLatin1Char {' '}, QLatin1Char {'+'});
}

void populate_messages (QString const& foxcall, int nh1, int nh2, bool more_cqs, bool send_msg)
{
  clear_fox_state ();

  struct Pair
  {
    char const* first;
    char const* second;
    int report;
  };

  std::array<Pair, 5> const pairs {{
      {"W0AAA", "W5FFF", -18},
      {"W1BBB", "W6GGG", -15},
      {"W2CCC", "W7HHH", -12},
      {"W3DDD", "W8III", -9},
      {"W4EEE", "W9JJJ", -6},
  }};

  QStringList lines;
  lines.reserve (5);
  for (int i = 0; i < 5; ++i)
    {
      if (i >= nh1 && i >= nh2)
        {
          continue;
        }
      QString line;
      QString const rpt = format_report (pairs[static_cast<size_t> (i)].report);
      if (i < nh1 && i < nh2)
        {
          line = QStringLiteral ("%1 RR73; %2 <%3> %4")
                     .arg (QString::fromLatin1 (pairs[static_cast<size_t> (i)].first),
                           QString::fromLatin1 (pairs[static_cast<size_t> (i)].second),
                           foxcall, rpt);
        }
      else if (i >= nh1)
        {
          line = QStringLiteral ("%1 <%2> %3")
                     .arg (QString::fromLatin1 (pairs[static_cast<size_t> (i)].second),
                           foxcall, rpt);
        }
      else
        {
          line = QStringLiteral ("%1 %2 RR73")
                     .arg (QString::fromLatin1 (pairs[static_cast<size_t> (i)].first), foxcall);
        }
      lines << line;
    }

  if (lines.isEmpty () && more_cqs)
    {
      lines << QStringLiteral ("CQ %1 FN20").arg (foxcall);
    }

  foxcom_.nslots = std::min (lines.size (), 5);
  foxcom_.nfreq = 750;
  foxcom_.bMoreCQs = more_cqs;
  foxcom_.bSendMsg = send_msg;
  for (int i = 0; i < foxcom_.nslots; ++i)
    {
      QByteArray const fixed = lines.at (i).leftJustified (40, QLatin1Char {' '}, true).left (40).toLatin1 ();
      std::copy_n (fixed.constData (), 40, foxcom_.cmsg[i]);
    }

  QByteArray free_text = QByteArray {"0123456789ABCDEFGHIJKLMNOP"};
  QFile text_file {QStringLiteral ("text_msg.txt")};
  if (send_msg && text_file.open (QIODevice::ReadOnly | QIODevice::Text))
    {
      free_text = text_file.readLine ().trimmed ();
      if (free_text.isEmpty ())
        {
          free_text = QByteArray {"0123456789ABCDEFGHIJKLMNOP"};
        }
    }
  free_text = free_text.leftJustified (26, ' ', true).left (26);
  std::copy_n (free_text.constData (), 26, foxcom_.textMsg);
}

std::array<int, 127> extract_superfox_data_symbols ()
{
  std::array<int, 127> data {};
  int data_index = 0;
  for (int symbol = 1; symbol <= kSuperFoxSymbols; ++symbol)
    {
      if (std::find (kSyncPositions.begin (), kSyncPositions.end (), symbol) != kSyncPositions.end ())
        {
          continue;
        }
      if (data_index >= 127)
        {
          break;
        }
      data[static_cast<size_t> (data_index++)] = std::max (0, foxcom3_.itone3[symbol - 1] - 1);
    }
  return data;
}

void print_usage ()
{
  std::cout << "Usage:   sfoxsim   f0  DT Chan FoxC H1 H2 CQ FT nfiles snr\n"
            << "Example: sfoxsim  750 0.0  MM  K1JT  5  1  0  0   10   -15\n"
            << "         f0=0 to dither f0 and DT\n"
            << "         Chan Channel type AW LQ LM LD MQ MM MD HQ HM HD\n";
}
}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 11)
        {
          print_usage ();
          return 0;
        }

      float const f0 = QString::fromLocal8Bit (argv[1]).toFloat ();
      float const xdt = QString::fromLocal8Bit (argv[2]).toFloat ();
      QString const channel = QString::fromLocal8Bit (argv[3]).trimmed ().toUpper ();
      QString const foxcall = QString::fromLocal8Bit (argv[4]).trimmed ().toUpper ();
      int const nh1 = std::max (0, QString::fromLocal8Bit (argv[5]).toInt ());
      int const nh2 = std::max (0, QString::fromLocal8Bit (argv[6]).toInt ());
      bool const more_cqs = QString::fromLocal8Bit (argv[7]).toInt () != 0;
      bool const send_msg = QString::fromLocal8Bit (argv[8]).toInt () != 0;
      int const nfiles = std::max (1, QString::fromLocal8Bit (argv[9]).toInt ());
      float const snr = QString::fromLocal8Bit (argv[10]).toFloat ();

      populate_messages (foxcall, nh1, nh2, more_cqs, send_msg);
      if (!decodium::txwave::generateSuperFoxTx (normalize_otp_key (QStringLiteral ("0000000000"))))
        {
          fail (QStringLiteral ("failed to assemble native SuperFox message"));
        }

      std::array<int, 127> const data_symbols = extract_superfox_data_symbols ();
      float fspread = 0.0f;
      float delay = 0.0f;
      float fsample = float (kSampleRate);
      apply_watterson_profile (channel, &fspread, &delay);

      std::mt19937 rng {1u};
      std::uniform_real_distribution<float> uniform01 {0.0f, 1.0f};

      std::cout << "sfoxsim: f0=  " << std::fixed << std::setprecision (1) << f0
                << "  dt= " << std::setprecision (2) << xdt
                << "   Channel: " << channel.toStdString ()
                << "   snr: " << std::setprecision (1) << snr << " dB\n";

      float const sig = std::sqrt (2.0f * kBandwidthRatio) * std::pow (10.0f, 0.05f * snr);
      float const sigr = std::sqrt (2.0f) * sig;

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          float f1 = f0;
          float dt1 = xdt;
          if (f0 == 0.0f)
            {
              f1 = 750.0f + 20.0f * (uniform01 (rng) - 0.5f);
              dt1 = uniform01 (rng) - 0.5f;
            }

          std::array<int, 151> tones {};
          std::vector<std::complex<float>> cdat (kWaveSamples, std::complex<float> {});
          sfox_gen_gfsk_ (const_cast<int*> (data_symbols.data ()), &f1,
                          const_cast<int*> (kSyncPositions.data ()), tones.data (), cdat.data ());

          int const shift = -static_cast<int> (std::lround ((0.5f + dt1) * float (kSampleRate)));
          std::vector<std::complex<float>> crcvd = circular_shift (cdat, shift);
          if (fspread != 0.0f || delay != 0.0f)
            {
              int npts = kWaveSamples;
              int nsig = kWaveSamples;
              float delay_copy = delay;
              float fspread_copy = fspread;
              watterson_ (reinterpret_cast<fftwf_complex*> (crcvd.data ()), &npts, &nsig,
                          &fsample, &delay_copy, &fspread_copy);
            }

          std::vector<qint16> pcm (kWaveSamples, 0);
          for (int i = 0; i < kWaveSamples; ++i)
            {
              float sample = std::imag (crcvd[static_cast<size_t> (i)]);
              sample *= sigr;
              if (snr < 90.0f)
                {
                  sample += gran_ ();
                  sample *= 100.0f;
                }
              else
                {
                  sample *= 32767.0f;
                }
              pcm[static_cast<size_t> (i)] = static_cast<qint16> (qBound (-32767, qRound (sample), 32767));
            }

          int const seconds = (ifile - 1) * 30;
          int const hour = seconds / 3600;
          int const minute = (seconds - hour * 3600) / 60;
          int const second = seconds % 60;
          QString const file_name = QStringLiteral ("000000_%1%2%3.wav")
                                        .arg (hour, 2, 10, QLatin1Char {'0'})
                                        .arg (minute, 2, 10, QLatin1Char {'0'})
                                        .arg (second, 2, 10, QLatin1Char {'0'});
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
