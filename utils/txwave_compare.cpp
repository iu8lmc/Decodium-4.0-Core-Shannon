#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include "Modulator/FtxWaveformGenerator.hpp"
#include "wsjtx_config.h"

extern "C" {
void genwave_ (int itone[], int* nsym, int* nsps, int* nwave, float* fsample,
               double* toneSpacing, float* f0, int* icmplx, float xjunk[], float wave[]);
void gen_echocall_ (char* basecall, int itone[], fortran_charlen_t);
void morse_ (char* msg, int* icw, int* ncw, fortran_charlen_t);
}

namespace
{

QByteArray fixed_6 (QString const& text)
{
  QByteArray latin = text.left (6).toLatin1 ();
  if (latin.size () < 6)
    {
      latin.append (QByteArray (6 - latin.size (), ' '));
    }
  return latin;
}

bool compare_echo_call (QString const& callsign)
{
  QByteArray input = fixed_6 (callsign);
  std::array<int, 6> fortran_tones {};
  gen_echocall_ (input.data (), fortran_tones.data (), static_cast<fortran_charlen_t> (input.size ()));

  auto const cpp_tones = decodium::txwave::encodeEchoCallTones (QString::fromLatin1 (input.constData (), 6));
  for (int i = 0; i < 6; ++i)
    {
      if (fortran_tones[static_cast<size_t> (i)] != cpp_tones[static_cast<size_t> (i)])
        {
          std::fprintf (stderr, "Echo-call tone mismatch for '%s' at %d: fortran=%d cxx=%d\n",
                        input.constData (), i, fortran_tones[static_cast<size_t> (i)],
                        cpp_tones[static_cast<size_t> (i)]);
          return false;
        }
    }
  return true;
}

bool compare_morse_bits (QString const& message)
{
  QByteArray input = message.toLatin1 ();
  std::array<int, 250> fortran_bits {};
  int fortran_count = 0;
  morse_ (input.data (), fortran_bits.data (), &fortran_count, static_cast<fortran_charlen_t> (input.size ()));

  int cpp_count = 0;
  auto const cpp_bits = decodium::txwave::encodeMorseBits (message, &cpp_count);
  if (fortran_count != cpp_count)
    {
      std::fprintf (stderr, "Morse bit-count mismatch for '%s': fortran=%d cxx=%d\n",
                    input.constData (), fortran_count, cpp_count);
      return false;
    }

  for (int i = 0; i < static_cast<int> (cpp_bits.size ()); ++i)
    {
      if (fortran_bits[static_cast<size_t> (i)] != cpp_bits[static_cast<size_t> (i)])
        {
          std::fprintf (stderr, "Morse bit mismatch for '%s' at %d: fortran=%d cxx=%d\n",
                        input.constData (), i, fortran_bits[static_cast<size_t> (i)],
                        cpp_bits[static_cast<size_t> (i)]);
          return false;
        }
    }

  return true;
}

bool compare_tone_wave (std::array<int, 8> const& tones, int nsps, int nwave,
                        float fsample, double tone_spacing, float f0)
{
  int nsym = static_cast<int> (tones.size ());
  int icmplx = 0;
  std::vector<int> tone_copy (tones.begin (), tones.end ());
  std::vector<float> fortran_wave (static_cast<size_t> (nwave), 0.0f);
  std::vector<float> fortran_dummy (static_cast<size_t> (nwave), 0.0f);
  genwave_ (tone_copy.data (), &nsym, &nsps, &nwave, &fsample, &tone_spacing, &f0,
            &icmplx, fortran_dummy.data (), fortran_wave.data ());

  QVector<float> const cpp_wave = decodium::txwave::generateToneWave (tones.data (), nsym, nsps, fsample,
                                                                      static_cast<float> (tone_spacing), f0);
  int const active_samples = nsym * nsps;
  if (cpp_wave.size () != active_samples)
    {
      std::fprintf (stderr, "Tone-wave sample count mismatch: expected %d got %d\n",
                    active_samples, cpp_wave.size ());
      return false;
    }

  double max_diff = 0.0;
  for (int i = 0; i < active_samples; ++i)
    {
      max_diff = std::max (max_diff, std::fabs (static_cast<double> (fortran_wave[static_cast<size_t> (i)])
                                                - static_cast<double> (cpp_wave.at (i))));
    }
  if (max_diff > 5e-5)
    {
      std::fprintf (stderr, "Tone-wave max diff too large: %.9f\n", max_diff);
      return false;
    }

  for (int i = active_samples; i < nwave; ++i)
    {
      if (std::fabs (static_cast<double> (fortran_wave[static_cast<size_t> (i)])) > 1e-6)
        {
          std::fprintf (stderr, "Tone-wave trailing sample not zero at %d: %.9f\n",
                        i, static_cast<double> (fortran_wave[static_cast<size_t> (i)]));
          return false;
        }
    }

  return true;
}

bool validate_cw_wave (QString const& message, int ifreq)
{
  QVector<float> const cpp_wave = decodium::txwave::generateCwWave (message, ifreq);
  if (cpp_wave.size () != 98304)
    {
      std::fprintf (stderr, "CW-wave sample count mismatch for '%s': expected 98304 got %d\n",
                    message.toLatin1 ().constData (), cpp_wave.size ());
      std::fflush (stderr);
      return false;
    }

  double max_abs = 0.0;
  for (float const sample : cpp_wave)
    {
      max_abs = std::max (max_abs, std::fabs (static_cast<double> (sample)));
    }
  if (max_abs < 0.1 || max_abs > 1.0001)
    {
      std::fprintf (stderr, "CW-wave amplitude out of range for '%s': %.9f\n",
                    message.toLatin1 ().constData (), max_abs);
      std::fflush (stderr);
      return false;
    }

  int const tail_start = 98304 - 3 * 96 - 1;
  for (int i = tail_start; i < cpp_wave.size (); ++i)
    {
      if (std::fabs (static_cast<double> (cpp_wave.at (i))) > 1e-6)
        {
          std::fprintf (stderr, "CW-wave tail not silent for '%s' at %d: %.9f\n",
                        message.toLatin1 ().constData (), i, static_cast<double> (cpp_wave.at (i)));
          std::fflush (stderr);
          return false;
        }
    }

  return true;
}

}  // namespace

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  QStringList const kEchoCalls {
    QStringLiteral ("K1ABC"),
    QStringLiteral ("EA8/K1"),
    QStringLiteral ("9H1SR"),
    QStringLiteral ("CQ"),
    QStringLiteral ("AB1C/"),
  };

  for (QString const& call : kEchoCalls)
    {
      if (!compare_echo_call (call))
        {
          return 1;
        }
    }

  QStringList const kMorseMessages {
    QStringLiteral ("K1ABC"),
    QStringLiteral ("9H1SR"),
    QStringLiteral ("CQ TEST"),
    QStringLiteral ("EA8/K1ABC"),
    QStringLiteral ("FT2"),
  };
  for (QString const& message : kMorseMessages)
    {
      if (!compare_morse_bits (message))
        {
          return 1;
        }
    }

  if (!compare_tone_wave ({0, 1, 2, 3, 4, 5, 6, 7}, 4096, 8 * 4096, 48000.0f, 25.0, 1500.0f))
    {
      return 1;
    }
  if (!compare_tone_wave ({63, 12, 24, 18, 7, 31, 42, 3}, 7200, (8 + 2) * 7200, 48000.0f, 48000.0 / 7200.0, 1325.0f))
    {
      return 1;
    }

  QStringList const kCwMessages {
    QStringLiteral ("CQ"),
    QStringLiteral ("TEST"),
    QStringLiteral ("K1ABC"),
    QStringLiteral ("CQ K1ABC"),
    QStringLiteral ("K1ABC/9H1"),
  };

  for (QString const& message : kCwMessages)
    {
      if (!validate_cw_wave (message, 1500))
        {
          return 1;
        }
    }

  std::printf ("TX wave compare passed for %d echo calls, %d morse messages and %d CW messages\n",
               kEchoCalls.size (), kMorseMessages.size (), kCwMessages.size ());
  return 0;
}
