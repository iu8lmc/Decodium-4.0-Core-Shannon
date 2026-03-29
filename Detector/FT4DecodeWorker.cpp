// -*- Mode: C++ -*-
#include "Detector/FT4DecodeWorker.hpp"

#include <algorithm>
#include <mutex>

#include <QMutexLocker>

#include "Logger.hpp"
#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"
extern "C"
{
  void ftx_ft4_decode_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                         int* ndepth, int* lapcqonly, int* ncontest,
                         char const* mycall, char const* hiscall,
                         float syncs[], int snrs[], float dts[], float freqs[],
                         int naps[], float quals[], signed char bits77[],
                         char decodeds[], int* nout,
                         fortran_charlen_t, fortran_charlen_t,
                         fortran_charlen_t);
}

namespace
{
  constexpr int kFt4SampleCount {72576};
  constexpr int kFt4MaxLines {100};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};
  constexpr int kFt4StableDspStage {4};
  char constexpr kFt4DspStageEnv[] {"DECODIUM_FT4_CPP_DSP_STAGE"};

  QString format_decode_utc (int nutc)
  {
    if (nutc <= 0)
      {
        return QString {};
      }
    int const width = nutc > 9999 ? 6 : 4;
    return QString::number (nutc).rightJustified (width, QLatin1Char {'0'});
  }

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  QString decode_fallback (char const* decodeds, int index)
  {
    QByteArray fallback {decodeds + index * kDecodedChars, kDecodedChars};
    int end = fallback.size ();
    while (end > 0 && (fallback.at (end - 1) == ' ' || fallback.at (end - 1) == '\0'))
      {
        --end;
      }
    QString decoded = QString::fromLatin1 (fallback.constData (), end);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    return decoded.left (kDecodedChars);
  }

  QString build_row (QString const& utcPrefix, char marker, int snr, float dt, float freq,
                     int nap, float qual, char const* decodeds, int index)
  {
    QString decoded = decode_fallback (decodeds, index);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    decoded = decoded.left (kDecodedChars);
    if (nap != 0 && qual < 0.17f && decoded.size () >= kDecodedChars)
      {
        decoded[kDecodedChars - 1] = QLatin1Char {'?'};
      }
    QByteArray const decodedLatin = decoded.toLatin1 ();
    QByteArray annot = "  ";
    if (nap != 0)
      {
        annot = QStringLiteral ("a%1").arg (nap).leftJustified (2, QLatin1Char {' '}).toLatin1 ();
      }
    QString row = QStringLiteral ("%1%2%3 %4  %5 %6")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
        .arg (QChar::fromLatin1 (marker))
        .arg (QString::fromLatin1 (decodedLatin.constData (), decodedLatin.size ()))
        .arg (QString::fromLatin1 (annot.constData (), 2));
    if (!utcPrefix.isEmpty ())
      {
        row.prepend (utcPrefix);
      }
    return row;
  }

  QStringList build_rows (QString const& utcPrefix, int nout,
                          int const* snrs, float const* dts, float const* freqs,
                          int const* naps, float const* quals, char const* decodeds)
  {
    QStringList rows;
    rows.reserve (qBound (0, nout, kFt4MaxLines));
    for (int i = 0; i < qBound (0, nout, kFt4MaxLines); ++i)
      {
        rows << build_row (utcPrefix, '+', snrs[i], dts[i], freqs[i], naps[i], quals[i],
                           decodeds, i);
      }
    return rows;
  }

  int ft4_dsp_rollout_stage ()
  {
    QByteArray const raw = qgetenv (kFt4DspStageEnv);
    if (raw.isEmpty ())
      {
        return kFt4StableDspStage;
      }

    bool ok = false;
    int const parsed = raw.toInt (&ok);
    if (!ok)
      {
        return kFt4StableDspStage;
      }
    return std::max (0, std::min (4, parsed));
  }

  void log_ft4_dsp_rollout_once ()
  {
    static std::once_flag once;
    std::call_once (once, [] {
      QByteArray const raw = qgetenv (kFt4DspStageEnv);
      int const stage = ft4_dsp_rollout_stage ();
      LOG_INFO ("FT4 DSP rollout active: stage=" << stage
                << " getcandidates=" << (stage >= 1 ? "cpp" : "fortran")
                << " sync4d=" << ((stage == 2 || stage >= 4) ? "cpp" : "fortran")
                << " bitmetrics=" << (stage >= 4 ? "cpp" : "fortran")
                << (raw.isEmpty ()
                    ? " (default)"
                    : std::string {" via "} + kFt4DspStageEnv + "=" + raw.constData ()));
    });
  }
}

namespace decodium
{
namespace ft4
{

FT4DecodeWorker::FT4DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FT4DecodeWorker::decode (DecodeRequest const& request)
{
  log_ft4_dsp_rollout_once ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  short int iwave[kFt4SampleCount] {};
  int const copyCount = std::min (request.audio.size (), kFt4SampleCount);
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, iwave);
    }

  int snrs[kFt4MaxLines] {};
  float dts[kFt4MaxLines] {};
  float freqs[kFt4MaxLines] {};
  int naps[kFt4MaxLines] {};
  float quals[kFt4MaxLines] {};
  signed char bits77[kFt4MaxLines * kBitsPerMessage] {};
  char decodeds[kFt4MaxLines * kDecodedChars] {};
  float syncs[kFt4MaxLines] {};

  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int nfqso = qBound (0, request.nfqso, 5000);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int ndepth = qBound (1, request.ndepth, 4);
  int lapcqonly = 0;
  int ncontest = qBound (0, request.ncontest, 16);
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);

  ftx_ft4_decode_c (iwave, &nqsoprogress, &nfqso, &nfa, &nfb,
                    &ndepth, &lapcqonly, &ncontest, mycall.data (), hiscall.data (),
                    &syncs[0], &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                    &bits77[0], &decodeds[0], &nout,
                    static_cast<fortran_charlen_t> (12),
                    static_cast<fortran_charlen_t> (12),
                    static_cast<fortran_charlen_t> (kFt4MaxLines * kDecodedChars));

  LOG_DEBUG ("FT4 decode completed: stage=" << ft4_dsp_rollout_stage ()
             << " nout=" << nout);
  QString const utcPrefix = format_decode_utc (request.nutc);
  Q_EMIT decodeReady (request.serial, build_rows (utcPrefix, nout, snrs, dts, freqs, naps, quals,
                                                  decodeds));
}

}
}
