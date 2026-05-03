// -*- Mode: C++ -*-
#include "Detector/FT8DecodeWorker.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>

#include <QMutexLocker>

#include "Logger.hpp"
#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"
extern "C"
{
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_cancel_c (int cancel);
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
}

namespace
{
  constexpr int kFt8SampleCount {180000};
  constexpr int kFt8MaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};
  constexpr int kFt8StableDspStage {4};

  void set_ft8_stage4_cancel (bool cancel)
  {
    ftx_ft8_stage4_set_cancel_c (cancel ? 1 : 0);
  }

  long long ft8_stage4_deadline_ms_from_now (int maxDecodeMs)
  {
    if (maxDecodeMs <= 0)
      {
        return 0;
      }
    using namespace std::chrono;
    auto const now = steady_clock::now ().time_since_epoch ();
    return duration_cast<milliseconds> (now).count () + maxDecodeMs;
  }

  QString format_decode_utc (int nutc)
  {
    if (nutc <= 0)
      {
        return QString {};
      }
    return QString::number (nutc).rightJustified (6, QLatin1Char {'0'});
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

  QString build_row (QString const& utcPrefix, int snr, float dt, float freq,
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
    QString row = QStringLiteral ("%1%2%3 ~  %4 %5")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
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
    rows.reserve (qBound (0, nout, kFt8MaxLines));
    for (int i = 0; i < qBound (0, nout, kFt8MaxLines); ++i)
      {
        rows << build_row (utcPrefix, snrs[i], dts[i], freqs[i], naps[i], quals[i],
                           decodeds, i);
      }
    return rows;
  }

  void log_ft8_dsp_rollout_once ()
  {
    static std::once_flag once;
    std::call_once (once, [] {
      LOG_INFO ("FT8 DSP rollout active: stage=" << kFt8StableDspStage
                << " downsample=cpp subtract=cpp saved_subtractions=cpp async_core=cpp"
                << " (runtime baseline)");
    });
  }
}

namespace decodium
{
namespace ft8
{

FT8DecodeWorker::FT8DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FT8DecodeWorker::cancelCurrentDecode ()
{
  set_ft8_stage4_cancel (true);
}

void FT8DecodeWorker::beginShutdown ()
{
  m_shuttingDown.store (true, std::memory_order_relaxed);
  set_ft8_stage4_cancel (true);
}

void FT8DecodeWorker::decode (DecodeRequest const& request)
{
  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  set_ft8_stage4_cancel (false);
  log_ft8_dsp_rollout_once ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  ftx_ft8_stage4_set_deadline_ms_c (ft8_stage4_deadline_ms_from_now (request.maxDecodeMs));
  ftx_ft8_stage4_set_supplemental_c (request.supplemental ? 1 : 0);
  // Turbo Feedback: estende belief-propagation a 50 iter (default 30).
  ftx_ft8_stage4_set_ldpc_max_iter_c (request.turboFeedbackEnabled ? 50 : 30);
  bool const wantOsd = (request.supplemental && request.ndepth >= 3) || request.neuralSyncEnabled;
  if (request.supplemental && request.ndepth >= 4)
    {
      ftx_ft8_stage4_set_ldpc_osd_c (3, 4);
    }
  else if (wantOsd)
    {
      ftx_ft8_stage4_set_ldpc_osd_c (3, 3);
    }
  else
    {
      ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
    }

  short int iwave[kFt8SampleCount] {};
  int const copyCount = std::min (static_cast<int>(request.audio.size ()), static_cast<int>(kFt8SampleCount));
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, iwave);
    }

  int snrs[kFt8MaxLines] {};
  float dts[kFt8MaxLines] {};
  float freqs[kFt8MaxLines] {};
  int naps[kFt8MaxLines] {};
  float quals[kFt8MaxLines] {};
  signed char bits77[kFt8MaxLines * kBitsPerMessage] {};
  char decodeds[kFt8MaxLines * kDecodedChars] {};

  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int nfqso = qBound (0, request.nfqso, 5000);
  int nftx = qBound (0, request.nftx, 5000);
  int nutc = qMax (0, request.nutc);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int nzhsym = qBound (41, request.nzhsym, 50);
  int ndepth = qBound (1, request.ndepth, 4);
  float emedelay = request.emedelay;
  int ncontest = qBound (0, request.ncontest, 16);
  int nagain = request.nagain ? 1 : 0;
  int lft8apon = request.lft8apon ? 1 : 0;
  int ltry_a8 = (nzhsym == 41 || request.lmultift8 != 0) ? 1 : 0;
  int lapcqonly = request.lapcqonly ? 1 : 0;
  int napwid = qBound (0, request.napwid, 200);
  int ldiskdat = request.ldiskdat ? 1 : 0;
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  auto hisgrid = to_fortran_field (request.hisgrid, 6);

  ftx_ft8_async_decode_stage4_c (iwave, &nqsoprogress, &nfqso, &nftx, &nutc, &nfa, &nfb,
                                 &nzhsym, &ndepth, &emedelay, &ncontest, &nagain,
                                 &lft8apon, &ltry_a8, &lapcqonly, &napwid, mycall.constData (),
                                 hiscall.constData (), hisgrid.constData (), &ldiskdat,
                                 nullptr,
                                 &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                                 &bits77[0], &decodeds[0], &nout);
  ftx_ft8_stage4_set_deadline_ms_c (0);
  ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
  ftx_ft8_stage4_set_supplemental_c (0);
  ftx_ft8_stage4_set_ldpc_max_iter_c (30);

  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  QString const utcPrefix = format_decode_utc (request.nutc);
  QStringList rows = build_rows (utcPrefix, nout, snrs, dts, freqs, naps, quals, decodeds);
  if (request.neuralSyncEnabled)
    {
      // Stage4 OSD non espone counter; proxy: rapporto decodi vs cap di 5.
      double const score = wantOsd && !rows.isEmpty ()
          ? std::min (1.0, rows.size () / 5.0)
          : 0.0;
      Q_EMIT neuralSyncHit (score);
    }
  if (request.turboFeedbackEnabled)
    {
      // Counter LDPC iter non esposto via callback; proxy: 50 (cap turbo) se decodi presenti, 0 altrimenti.
      // Triggera LED Turbo Feedback (soglia >8 in DecodiumBridge::notifyTurboIterations).
      Q_EMIT turboIterations (rows.isEmpty () ? 0 : 50);
    }
  Q_EMIT decodeReady (request.serial, rows);
}

void FT8DecodeWorker::resetDecoderState ()
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  ftx_ft8_stage4_reset_c ();
}

}
}
