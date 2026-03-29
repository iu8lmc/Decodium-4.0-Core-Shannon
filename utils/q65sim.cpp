#include "wsjtx_config.h"

#include <QByteArray>
#include <QString>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fftw3.h>

extern "C"
{
void q65_encode_message_ (char* msg0, char* msgsent, int* payload, int* codeword, int* itone,
                          fortran_charlen_t len1, fortran_charlen_t len2);
}

namespace
{
constexpr int kPayloadSymbols {13};
constexpr int kCodewordSymbols {63};
constexpr int kChannelSymbols {85};
constexpr int kMessageChars {37};
constexpr int kSampleRate {12000};
constexpr int kBitsPerSample {16};
constexpr int kMaxSignals {10};
double const kTwoPi = 8.0 * std::atan (1.0);

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
      qToLittleEndian<qint16> (
        samples[i],
        reinterpret_cast<uchar*> (output + static_cast<int> (i * sizeof (qint16))));
    }

  return blob;
}

void write_wav (QString const& path, std::vector<qint16> const& samples)
{
  QByteArray const blob = make_wav_blob (samples);
  std::ofstream out {path.toStdString ().c_str (), std::ios::binary};
  if (!out)
    {
      fail (QStringLiteral ("unable to open output file %1").arg (path));
    }
  out.write (blob.constData (), blob.size ());
}

float gran ()
{
  static float gset = 0.0f;
  static bool iset = false;
  if (iset)
    {
      iset = false;
      return gset;
    }

  float rsq = 0.0f;
  float v1 = 0.0f;
  float v2 = 0.0f;
  do
    {
      v1 = 2.0f * static_cast<float> (std::rand ()) / static_cast<float> (RAND_MAX) - 1.0f;
      v2 = 2.0f * static_cast<float> (std::rand ()) / static_cast<float> (RAND_MAX) - 1.0f;
      rsq = v1 * v1 + v2 * v2;
    }
  while (rsq >= 1.0f || rsq == 0.0f);

  float const fac = std::sqrt (-2.0f * std::log (rsq) / rsq);
  gset = v1 * fac;
  iset = true;
  return v2 * fac;
}

float rran ()
{
  return static_cast<float> (std::rand ()) / static_cast<float> (RAND_MAX);
}

QString trim_fixed (char const* data, int size)
{
  int used = size;
  while (used > 0 && (data[used - 1] == ' ' || data[used - 1] == '\0'))
    {
      --used;
    }
  return QString::fromLatin1 (data, used);
}

QString fixed_message (QString const& input)
{
  QString clipped = input.leftJustified (kMessageChars, QLatin1Char (' '), true);
  if (clipped.size () > kMessageChars)
    {
      clipped = clipped.left (kMessageChars);
    }
  return clipped;
}

bool is_single_tone_message (QString const& message)
{
  return message.trimmed ().startsWith (QStringLiteral ("ST"), Qt::CaseInsensitive);
}

struct EncodedQ65
{
  bool ok {false};
  QString msgsent;
  std::array<int, kPayloadSymbols> payload {};
  std::array<int, kCodewordSymbols> codeword {};
  std::array<int, kChannelSymbols> tones {};
};

EncodedQ65 encode_q65 (QString const& message)
{
  EncodedQ65 result;
  if (is_single_tone_message (message))
    {
      result.ok = true;
      result.msgsent = QStringLiteral ("ST");
      return result;
    }

  std::array<char, kMessageChars> msg {};
  std::array<char, kMessageChars> msgsent {};
  std::fill (msg.begin (), msg.end (), ' ');
  std::fill (msgsent.begin (), msgsent.end (), ' ');
  QByteArray latin = fixed_message (message).toLatin1 ();
  std::memcpy (msg.data (), latin.constData (), std::min (latin.size (), kMessageChars));

  q65_encode_message_ (msg.data (), msgsent.data (), result.payload.data (), result.codeword.data (),
                       result.tones.data (), static_cast<fortran_charlen_t> (kMessageChars),
                       static_cast<fortran_charlen_t> (kMessageChars));
  result.msgsent = trim_fixed (msgsent.data (), kMessageChars);
  result.ok = !result.msgsent.trimmed ().isEmpty ();
  return result;
}

void print_usage ()
{
  std::cout << "Usage:   q65sim         \"msg\"     A-F freq fDop DT  f1 Stp TRp Nsig Nfile SNR\n"
            << "Example: q65sim \"K1ABC W9XYZ EN37\" A  1500 0.0 0.0 0.0  1   60   1    1   -26\n"
            << "Example: q65sim \"ST\" A  1500 0.0 0.0 0.0  1   60   1    1   -26\n"
            << "         fDop = Doppler spread\n"
            << "         f1   = Drift or Doppler rate (Hz/min)\n"
            << "         Stp  = Step size (Hz)\n"
            << "         Stp  = 0 implies no Doppler tracking\n"
            << "         Nsig = number of generated signals, 1 - 10\n"
            << "         Creates filenames which increment to permit averaging in first period\n"
            << "         If msg = ST program produces a single tone at freq\n";
}

void print_rows (char const* label, int const* values, int count, int width, int wrap)
{
  std::cout << label << '\n';
  for (int i = 0; i < count; ++i)
    {
      if (i != 0 && i % wrap == 0)
        {
          std::cout << '\n';
        }
      std::cout << std::setw (width) << values[i];
    }
  std::cout << '\n';
}

QString mutate_message_for_index (QString const& base, int index)
{
  int first_space = base.indexOf (QLatin1Char (' '));
  if (first_space < 0)
    {
      return base;
    }
  int second_rel = base.mid (first_space + 1).indexOf (QLatin1Char (' '));
  if (second_rel < 1)
    {
      return base;
    }
  int i0 = first_space + second_rel - 1;
  if (i0 + 1 >= base.size ())
    {
      return base;
    }
  QChar repl1 = QChar::fromLatin1 (static_cast<char> ('A' + index));
  QChar repl2 = QChar::fromLatin1 (static_cast<char> ('A' + index));
  QString out = base;
  out[i0] = repl1;
  out[i0 + 1] = repl2;
  return out;
}

int nsps_for_period (int ntrperiod)
{
  switch (ntrperiod)
    {
    case 15: return 1800;
    case 30: return 3600;
    case 60: return 7200;
    case 120: return 16000;
    case 300: return 41472;
    default: return 0;
    }
}

QString file_name_for_index (int ifile, int ntrperiod)
{
  int const istart = ifile * ntrperiod * 2 - ntrperiod * 2;
  if (ntrperiod < 30)
    {
      int const imins = istart / 60;
      int const isecs = istart - 60 * imins;
      return QStringLiteral ("000000_%1%2.wav")
        .arg (imins, 4, 10, QLatin1Char ('0'))
        .arg (isecs, 2, 10, QLatin1Char ('0'));
    }
  return QStringLiteral ("000000_%1.wav").arg (istart / 60, 4, 10, QLatin1Char ('0'));
}

void apply_lorentz_fading (std::vector<std::complex<float>>& cdat, float fspread)
{
  if (fspread == 0.0f || cdat.empty ())
    {
      return;
    }

  int const nfft = static_cast<int> (cdat.size ());
  int const nh = nfft / 2;
  float const df = static_cast<float> (kSampleRate) / static_cast<float> (nfft);
  float const b = 6.0f;
  std::vector<std::complex<float>> cspread (static_cast<size_t> (nfft), std::complex<float> {});
  cspread[0] = std::complex<float> {1.0f, 0.0f};
  if (nh < nfft)
    {
      cspread[static_cast<size_t> (nh)] = std::complex<float> {0.0f, 0.0f};
    }

  for (int i = 1; i <= nh; ++i)
    {
      float const f = i * df;
      float const x = b * f / fspread;
      float a = 0.0f;
      std::complex<float> z1 {};
      std::complex<float> z2 {};
      if (x < 3.0f)
        {
          a = std::sqrt (1.111f / (1.0f + x * x) - 0.1f);
          float const phi1 = static_cast<float> (kTwoPi) * rran ();
          float const phi2 = static_cast<float> (kTwoPi) * rran ();
          z1 = std::complex<float> {a * std::cos (phi1), a * std::sin (phi1)};
          z2 = std::complex<float> {a * std::cos (phi2), a * std::sin (phi2)};
        }
      cspread[static_cast<size_t> (i)] = z1;
      if (i < nfft)
        {
          cspread[static_cast<size_t> (nfft - i)] = z2;
        }
    }

  fftwf_plan plan = fftwf_plan_dft_1d (
    nfft, reinterpret_cast<fftwf_complex*> (cspread.data ()),
    reinterpret_cast<fftwf_complex*> (cspread.data ()), FFTW_BACKWARD, FFTW_ESTIMATE);
  if (!plan)
    {
      fail (QStringLiteral ("unable to create FFTW plan for q65sim"));
    }
  fftwf_execute (plan);
  fftwf_destroy_plan (plan);

  double sum = 0.0;
  for (int i = 0; i < nfft; ++i)
    {
      sum += std::norm (cspread[static_cast<size_t> (i)]);
    }
  if (sum <= 0.0)
    {
      return;
    }
  float const fac = std::sqrt (static_cast<float> (nfft / sum));
  for (int i = 0; i < nfft; ++i)
    {
      cspread[static_cast<size_t> (i)] *= fac;
      cdat[static_cast<size_t> (i)] *= cspread[static_cast<size_t> (i)];
    }
}
}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 12)
        {
          print_usage ();
          return 0;
        }

      QString const message_in = QString::fromLocal8Bit (argv[1]);
      QString const submode_text = QString::fromLocal8Bit (argv[2]).trimmed ().toUpper ();
      if (submode_text.size () != 1 || submode_text.at (0) < QLatin1Char ('A')
          || submode_text.at (0) > QLatin1Char ('F'))
        {
          fail (QStringLiteral ("invalid Q65 submode"));
        }
      QChar const submode_char = submode_text.at (0);
      int const mode65 = 1 << (submode_char.toLatin1 () - 'A');
      double const f00 = QString::fromLocal8Bit (argv[3]).toDouble ();
      float const fspread = QString::fromLocal8Bit (argv[4]).toFloat ();
      float const xdt = QString::fromLocal8Bit (argv[5]).toFloat ();
      float const f1 = QString::fromLocal8Bit (argv[6]).toFloat ();
      int const nstp = QString::fromLocal8Bit (argv[7]).toInt ();
      int const ntrperiod = QString::fromLocal8Bit (argv[8]).toInt ();
      int const nsig = QString::fromLocal8Bit (argv[9]).toInt ();
      int const nfiles = QString::fromLocal8Bit (argv[10]).toInt ();
      float const snrdb = QString::fromLocal8Bit (argv[11]).toFloat ();

      int const nsps = nsps_for_period (ntrperiod);
      if (nsps == 0)
        {
          fail (QStringLiteral ("Invalid TR period"));
        }
      if (nsig < 1 || nsig > kMaxSignals)
        {
          fail (QStringLiteral ("Nsig must be between 1 and 10"));
        }
      if (nfiles < 1)
        {
          fail (QStringLiteral ("Nfile must be positive"));
        }

      int const npts = kSampleRate * ntrperiod;
      double const dt = 1.0 / static_cast<double> (kSampleRate);
      double const baud = static_cast<double> (kSampleRate) / static_cast<double> (nsps);
      float const bandwidth_ratio = 2500.0f / 6000.0f;
      float sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * snrdb);
      if (snrdb > 90.0f)
        {
          sig = 1.0f;
        }

      std::array<QString, kMaxSignals> messages;
      std::array<EncodedQ65, kMaxSignals> encoded;
      messages.fill (fixed_message (message_in));
      for (int i = 0; i < nsig; ++i)
        {
          if (nsig >= 2)
            {
              messages[static_cast<size_t> (i)] = fixed_message (mutate_message_for_index (message_in, i));
            }
          encoded[static_cast<size_t> (i)] = encode_q65 (messages[static_cast<size_t> (i)]);
          if (!encoded[static_cast<size_t> (i)].ok)
            {
              fail (QStringLiteral ("failed to encode Q65 message \"%1\"")
                      .arg (messages[static_cast<size_t> (i)].trimmed ()));
            }
        }

      if (nsig == 1 && !is_single_tone_message (messages[0]))
        {
          std::cout << "Generated message\n";
          std::cout << "6-bit:  ";
          for (int value : encoded[0].payload)
            {
              std::cout << std::setw (3) << value;
            }
          std::cout << "\nbinary: ";
          for (int value : encoded[0].payload)
            {
              for (int bit = 5; bit >= 0; --bit)
                {
                  std::cout << ((value >> bit) & 1);
                }
            }
          std::cout << "\n\n";
          print_rows ("Codeword:", encoded[0].codeword.data (), kCodewordSymbols, 3, 20);
          std::cout << '\n';
          print_rows ("Channel symbols:", encoded[0].tones.data (), kChannelSymbols, 3, 20);
        }

      std::cout << "File    TR   Freq Mode  S/N   Dop     DT   f1  Stp  Message\n"
                << std::string (70, '-') << '\n';

      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::vector<float> xnoise (static_cast<size_t> (npts), 0.0f);
          if (snrdb < 90.0f)
            {
              for (int i = 0; i < npts; ++i)
                {
                  xnoise[static_cast<size_t> (i)] = gran ();
                }
            }
          std::vector<std::complex<float>> cdat (static_cast<size_t> (npts), std::complex<float> {});

          double nfstep = 0.0;
          double nf1 = f00;
          if (nsig >= 2)
            {
              double const n = 65.0 * baud * mode65 / 100.0 + 0.9999;
              nfstep = 100.0 * n;
              nf1 = 1500.0 - nfstep * (nsig - 1) / 2.0;
            }

          QString display_message = encoded[0].msgsent;
          for (int isig = 0; isig < nsig; ++isig)
            {
              double f0 = f00;
              if (nsig >= 2)
                {
                  f0 = nf1 + isig * nfstep;
                }
              display_message = encoded[static_cast<size_t> (isig)].msgsent;

              double phi = 0.0;
              double dphi = 0.0;
              int k = static_cast<int> ((xdt + (ntrperiod >= 60 ? 1.0f : 0.5f)) * kSampleRate);
              int isym0 = -99;
              bool const single_tone = is_single_tone_message (messages[static_cast<size_t> (isig)]);

              for (int i = 1; i <= npts; ++i)
                {
                  int const isym = i / nsps + 1;
                  if (isym > kChannelSymbols)
                    {
                      break;
                    }
                  if (isym != isym0)
                    {
                      double freq_drift = f1 * i * dt / 60.0;
                      if (nstp != 0)
                        {
                          freq_drift -= nstp * std::nearbyint (freq_drift / nstp);
                        }
                      double freq = f0 + freq_drift;
                      if (!single_tone)
                        {
                          freq += encoded[static_cast<size_t> (isig)]
                                    .tones[static_cast<size_t> (isym - 1)] * baud * mode65;
                        }
                      dphi = kTwoPi * freq * dt;
                      isym0 = isym;
                    }
                  phi += dphi;
                  if (phi > kTwoPi)
                    {
                      phi -= kTwoPi;
                    }
                  k += 1;
                  if (k >= 1 && k <= npts)
                    {
                      cdat[static_cast<size_t> (k - 1)] += std::complex<float> {
                        sig * static_cast<float> (std::cos (phi)),
                        sig * static_cast<float> (std::sin (phi))};
                    }
                }
            }

          if (fspread != 0.0f)
            {
              apply_lorentz_fading (cdat, fspread);
            }

          std::vector<qint16> samples (static_cast<size_t> (npts), 0);
          float const scale = snrdb >= 90.0f ? 32767.0f : 100.0f;
          for (int i = 0; i < npts; ++i)
            {
              float const sample = cdat[static_cast<size_t> (i)].imag () + xnoise[static_cast<size_t> (i)];
              long value = std::lround (scale * sample);
              if (value < -32768l)
                {
                  value = -32768l;
                }
              else if (value > 32767l)
                {
                  value = 32767l;
                }
              samples[static_cast<size_t> (i)] = static_cast<qint16> (value);
            }

          QString const path = file_name_for_index (ifile, ntrperiod);
          write_wav (path, samples);

          std::cout << std::setw (4) << ifile
                    << std::setw (6) << ntrperiod
                    << std::fixed << std::setprecision (1) << std::setw (7) << f00
                    << "  " << submode_char.toLatin1 ()
                    << std::setw (7) << std::setprecision (1) << snrdb
                    << std::setw (7) << std::setprecision (2) << fspread
                    << std::setw (6) << std::setprecision (1) << xdt
                    << std::setw (6) << std::setprecision (1) << f1
                    << std::setw (4) << nstp << "  "
                    << display_message.toStdString () << '\n';
        }

      return 0;
    }
  catch (std::exception const& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
