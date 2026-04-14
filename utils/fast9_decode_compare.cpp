#include <QByteArray>
#include <QCoreApplication>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "Detector/JT9FastDecoder.hpp"
#include "Detector/JT9FastDecodeWorker.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "wsjtx_config.h"

extern "C"
{
  void fast_decode_ (short id2[], int narg[], double* trperiod,
                     char msg[], char mycall[], char hiscall[],
                     fortran_charlen_t, fortran_charlen_t, fortran_charlen_t);
}

namespace
{

constexpr int kSampleRate = 12000;
constexpr int kAudioCount = 30 * kSampleRate;
constexpr int kMaxLines = 100;
constexpr int kLineChars = 80;

QByteArray to_fortran_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

QStringList parse_fortran_rows (char const* buffer)
{
  QStringList rows;
  for (int i = 0; i < kMaxLines; ++i)
    {
      char const* line = buffer + i * kLineChars;
      if (line[0] == '\0')
        {
          break;
        }
      QByteArray rowBytes {line, kLineChars};
      while (!rowBytes.isEmpty ())
        {
          char const c = rowBytes.back ();
          if (c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
              rowBytes.chop (1);
              continue;
            }
          break;
        }
      if (!rowBytes.isEmpty ())
        {
          rows << QString::fromLatin1 (rowBytes);
        }
    }
  return rows;
}

QVector<short> synthesize_fast9_audio (QString const& message, int nsubmode, float base_freq, int* signal_samples)
{
  auto const encoded = decodium::txmsg::encodeJt9 (message, false);
  QVector<short> audio (kAudioCount, 0);
  if (!encoded.ok)
    {
      return audio;
    }

  int const nsps = 60 << (7 - nsubmode);
  float const tone_spacing = 12000.0f / nsps;
  float phase = 0.0f;
  int const start = 12000;
  int pos = start;
  for (int tone : encoded.tones)
    {
      float const freq = base_freq + tone * tone_spacing;
      float const delta = float (2.0 * M_PI) * freq / kSampleRate;
      for (int i = 0; i < nsps && pos < audio.size (); ++i, ++pos)
        {
          float const sample = 12000.0f * std::sin (phase);
          audio[pos] = static_cast<short> (std::lround (sample));
          phase += delta;
          if (phase > float (2.0 * M_PI))
            {
              phase = std::fmod (phase, float (2.0 * M_PI));
            }
        }
    }

  if (signal_samples)
    {
      *signal_samples = pos;
    }
  return audio;
}

bool compare_case (QString const& message, int nsubmode, int rxfreq, int ftol)
{
  int kdone = 0;
  auto const audio = synthesize_fast9_audio (message, nsubmode, float (rxfreq), &kdone);

  decodium::jt9fast::DecodeRequest request;
  request.audio = audio;
  request.nutc = 123456;
  request.kdone = kdone;
  request.nsubmode = nsubmode;
  request.newdat = 1;
  request.minsync = -10;
  request.npick = 0;
  request.t0_ms = 0;
  request.t1_ms = 0;
  request.maxlines = 50;
  request.rxfreq = rxfreq;
  request.ftol = ftol;
  request.aggressive = 0;
  request.trperiod = 30.0;
  request.mycall = QByteArrayLiteral ("K1ABC");
  request.hiscall = QByteArrayLiteral ("W9XYZ");

  int narg[15] {};
  narg[0] = request.nutc;
  narg[1] = request.kdone;
  narg[2] = request.nsubmode;
  narg[3] = request.newdat;
  narg[4] = request.minsync;
  narg[5] = request.npick;
  narg[6] = request.t0_ms;
  narg[7] = request.t1_ms;
  narg[8] = request.maxlines;
  narg[9] = 102;
  narg[10] = request.rxfreq;
  narg[11] = request.ftol;
  narg[12] = 0;
  narg[13] = -1;
  narg[14] = request.aggressive;

  std::array<char, kMaxLines * kLineChars> outlines {};
  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  double trperiod = request.trperiod;
  fast_decode_ (const_cast<short*> (audio.constData ()), narg, &trperiod,
                outlines.data (), mycall.data (), hiscall.data (),
                static_cast<fortran_charlen_t> (outlines.size ()),
                static_cast<fortran_charlen_t> (12),
                static_cast<fortran_charlen_t> (12));

  QStringList const fortran_rows = parse_fortran_rows (outlines.data ());
  decodium::jt9fast::FastDecodeState state;
  QStringList const cpp_rows = decodium::jt9fast::decode_fast9 (request, &state);

  if (fortran_rows.isEmpty ())
    {
      std::fprintf (stderr, "fortran fast9 produced no rows for '%s' submode=%d\n",
                    message.toLatin1 ().constData (), nsubmode);
      return false;
    }

  if (fortran_rows != cpp_rows)
    {
      std::fprintf (stderr, "fast9 row mismatch for '%s' submode=%d\n",
                    message.toLatin1 ().constData (), nsubmode);
      std::fprintf (stderr, "fortran:\n");
      for (auto const& row : fortran_rows)
        {
          std::fprintf (stderr, "  %s\n", row.toLatin1 ().constData ());
        }
      std::fprintf (stderr, "cxx:\n");
      for (auto const& row : cpp_rows)
        {
          std::fprintf (stderr, "  %s\n", row.toLatin1 ().constData ());
        }
      return false;
    }

  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  bool ok = true;
  ok = compare_case (QStringLiteral ("CQ WB9XYZ EN34"), 5, 1500, 100) && ok;
  if (!ok)
    {
      return 1;
    }

  std::printf ("Fast9 decode compare passed for synthesized JT9F signal\n");
  return 0;
}
