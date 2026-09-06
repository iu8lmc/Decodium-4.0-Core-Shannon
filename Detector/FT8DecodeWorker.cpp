// -*- Mode: C++ -*-
#include "Detector/FT8DecodeWorker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QTimeZone>

#include "Logger.hpp"
#include "commons.h"
#include "Detector/DecodeMetricLogging.hpp"
#include "Detector/DecodeWorkerScheduling.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Detector/FtxApStorico.hpp"

extern "C"
{
  int ftx_ft8_ap_storico_tentativi_c ();
  int ftx_ft8_ap_msg_tentativi_c ();
  int ftx_ft8_ap_msg_successi_c ();
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_cancel_c (int cancel);
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_decode_options_c (int low_thresholds, int subpass,
                                            int cycles, int rx_freq_sensitivity,
                                            int candidate_thin);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  void ftx_ft8_stage4_set_superfox_options_c (int enabled, int ntol_hz);
  void ftx_ft8_stage4_set_force_fresh_slot_c (int force);
  void ftx_ft8_stage4_set_freqpart_c (int bins);
  void ftx_ft8_stage4_set_syncmin_scale_c (float scale);
  void ftx_ft8_stage4_set_decode_syncmin_c (int gate);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_stage4_seed_known_cq_c (char const* call, char const* grid,
                                       float freq, float dt, int nutc);
  void ftx_ft8_stage4_seed_known_cq_call_c (char const* call,
                                            float freq, float dt, int nutc);
  void ftx_ft8_stage4_seed_hash_call_c (char const* call);
  void ftx_ft8_stage4_apply_hash_seed_cache_c ();
  void ftx_ft8_a7_save_c (int jseq, float dt, float freq, char const* msg37);
  int ftx_ft8_freqpart_bins_used_c ();
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
  std::atomic_bool g_hashSeedShutdownRequested {false};

  constexpr int kFt8SampleCount {180000};
  constexpr int kFt8MaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};
  constexpr int kFt8StableDspStage {4};
  constexpr qint64 kHashSeedTailBytes {16 * 1024 * 1024};
  constexpr qint64 kHashSeedRefreshTailBytes {2 * 1024 * 1024};
  constexpr int kHashSeedMaxCalls {8192};
  constexpr qint64 kHashSeedInitialSoftBudgetMs {1500};
  constexpr qint64 kHashSeedRefreshIntervalMs {10000};
  constexpr int kHashSeedRefreshMaxFiles {10};
  constexpr qint64 kHashSeedRefreshSoftBudgetMs {350};
  constexpr int kHashSeedApplyBatchCalls {64};
  constexpr qint64 kHashSeedApplyBudgetMs {25};
  constexpr int kHashSeedRefreshApplyMaxCalls {128};
  constexpr int kExternalA7SeedLimit {96};
  constexpr int kExternalA7PairSeedLimit {48};
  constexpr int kExternalA7InsertLimit {104};
  constexpr int kHashSeedPriorityMonths {6};
  [[maybe_unused]] constexpr int kMaxDecodeThreads {24};

  // Il numero di thread OpenMP si imposta UNA VOLTA SOLA e non si cambia piu'.
  // Con libgomp, cambiarlo distrugge e ricrea il pool: i thread che muoiono
  // portano con se' i propri oggetti thread_local, i cui distruttori girano
  // dentro LdrShutdownThread quando heap e loader sono gia' in smontaggio.
  // Da li' nascevano la corruzione dello heap (0xc0000374), il segfault sui
  // contatori di riferimento e lo stallo sul loader lock che tenevano spenta
  // la fase profonda di FT8.
  //
  // Si fissa al valore piu' alto che il chiamante possa chiedere: le passate
  // che ne vogliono meno lo ottengono con la clausola num_threads sulla
  // singola regione parallela, che NON ridimensiona il pool.
  extern "C" void ftx_ft8_set_thread_budget_c (int n);

  void apply_decode_thread_limit (int threads)
  {
#ifdef _OPENMP
    static bool const fissato = [] {
      omp_set_dynamic (0);
      omp_set_num_threads (kMaxDecodeThreads);
      return true;
    } ();
    (void) fissato;
    // Il limite richiesto viaggia in una variabile che la regione parallela
    // legge con num_threads: rispetta il carico voluto senza ridimensionare.
    ftx_ft8_set_thread_budget_c (std::max (1, std::min (threads, kMaxDecodeThreads)));
#else
    (void) threads;
#endif
  }

  int active_decode_thread_limit ()
  {
#ifdef _OPENMP
    return omp_get_max_threads ();
#else
    return 1;
#endif
  }

  QString current_thread_id_hex ()
  {
    return QString::number (reinterpret_cast<quintptr> (QThread::currentThreadId ()), 16);
  }

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

  bool plausible_hash_seed_token (QString const& token)
  {
    if (token.size () < 3 || token.size () > 13)
      {
        return false;
      }
    static QSet<QString> const skipTokens {
      QStringLiteral ("CQ"), QStringLiteral ("DE"), QStringLiteral ("DX"),
      QStringLiteral ("QRZ"), QStringLiteral ("RRR"), QStringLiteral ("RR73"),
      QStringLiteral ("FT8"), QStringLiteral ("FT4"), QStringLiteral ("FT2"),
      QStringLiteral ("JT9"), QStringLiteral ("JT65"), QStringLiteral ("Q65"),
      QStringLiteral ("WSPR"), QStringLiteral ("FST4")
    };
    if (skipTokens.contains (token))
      {
        return false;
      }
    static QRegularExpression const reportRx {
      QStringLiteral (R"(^(?:R)?[+-]?\d{1,2}$)")
    };
    static QRegularExpression const modeSuffixRx {QStringLiteral (R"(^A\d+$)")};
    static QRegularExpression const gridRx {
      QStringLiteral (R"(^[A-R]{2}\d{2}(?:[A-X]{2})?$)")
    };
    if (reportRx.match (token).hasMatch ()
        || modeSuffixRx.match (token).hasMatch ()
        || gridRx.match (token).hasMatch ())
      {
        return false;
      }

    bool hasLetter = false;
    bool hasDigit = false;
    for (QChar const ch : token)
      {
        if (ch >= QLatin1Char ('A') && ch <= QLatin1Char ('Z'))
          {
            hasLetter = true;
          }
        else if (ch >= QLatin1Char ('0') && ch <= QLatin1Char ('9'))
          {
            hasDigit = true;
          }
        else if (ch != QLatin1Char ('/'))
          {
            return false;
          }
      }
    return hasLetter && hasDigit;
  }

  bool hash_seed_shutdown_requested ()
  {
    return g_hashSeedShutdownRequested.load (std::memory_order_acquire)
           || QCoreApplication::closingDown ();
  }

  struct HashSeedCallAccumulator
  {
    std::deque<QString> calls;

    QStringList to_list () const
    {
      QSet<QString> seen;
      QStringList result;
      result.reserve (std::min (static_cast<int> (calls.size ()), kHashSeedMaxCalls));
      for (auto it = calls.rbegin (); it != calls.rend ()
           && result.size () < kHashSeedMaxCalls; ++it)
        {
          if (!seen.contains (*it))
            {
              seen.insert (*it);
              result.append (*it);
            }
        }
      std::reverse (result.begin (), result.end ());
      return result;
    }
  };

  void remember_hash_seed_call (HashSeedCallAccumulator& accumulator, QString call)
  {
    call = call.trimmed ().toUpper ();
    if (call.startsWith (QLatin1Char ('<')) && call.endsWith (QLatin1Char ('>')))
      {
        call = call.mid (1, call.size () - 2);
      }
    if (!plausible_hash_seed_token (call))
      {
        return;
      }
    accumulator.calls.push_back (call);
    while (static_cast<int> (accumulator.calls.size ()) > kHashSeedMaxCalls * 4)
      {
        accumulator.calls.pop_front ();
      }
  }

  bool collect_hash_seed_calls_from_tail (QString const& path,
                                          HashSeedCallAccumulator& accumulator,
                                          qint64 tailBytes = kHashSeedTailBytes)
  {
    if (hash_seed_shutdown_requested ())
      {
        return false;
      }
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }
    QByteArray const data = file.readAll ().toUpper ();
    QString const text = QString::fromLatin1 (data.constData (), data.size ());
    static QRegularExpression const tokenRx {
      QStringLiteral (R"((?:<[A-Z0-9/]{3,13}>|[A-Z0-9/]{3,13}))")
    };
    auto it = tokenRx.globalMatch (text);
    int scannedTokens = 0;
    while (it.hasNext ())
      {
        if ((++scannedTokens & 0x3ff) == 0 && hash_seed_shutdown_requested ())
          {
            return false;
          }
        remember_hash_seed_call (accumulator, it.next ().captured (0));
      }
    return true;
  }

  struct KnownCqSeed
  {
    QString call;
    QString grid;
    float freq {0.0f};
    float dt {0.0f};
    int nutc {0};
    int hits {1};
  };

  struct KnownCqCallSeed
  {
    QString call;
    float freq {0.0f};
    float dt {0.0f};
    int nutc {0};
    int hits {1};
  };

  struct A7MessageSeed
  {
    QString message;
    float freq {0.0f};
    float dt {0.0f};
    int nutc {0};
    int hits {1};
    int priority {0};
  };

  struct HashSeedSnapshot
  {
    QStringList calls;
    int filesRead {0};
    QStringList sources;
    qint64 elapsedMs {0};
  };

  struct HashContextRefresh
  {
    QStringList calls;
    QHash<QString, KnownCqSeed> cqSeeds;
    QHash<QString, KnownCqCallSeed> cqCallSeeds;
    QHash<QString, A7MessageSeed> a7Seeds;
    int currentNutc {0};
    bool seedKnownCqReplay {false};
    bool seedExternalA7 {false};
    int filesRead {0};
    qint64 elapsedMs {0};
  };

  void remember_known_cq_seed (QHash<QString, KnownCqSeed>& seeds, QString call,
                               QString grid, float freq, float dt, int nutc)
  {
    call = call.trimmed ().toUpper ();
    grid = grid.trimmed ().toUpper ();
    if (!plausible_hash_seed_token (call)
        || !QRegularExpression {QStringLiteral (R"(^[A-R]{2}\d{2}$)")}.match (grid).hasMatch ()
        || freq <= 0.0f || nutc <= 0)
      {
        return;
      }
    QString const key = call + QLatin1Char ('|') + grid;
    auto it = seeds.find (key);
    if (it == seeds.end ())
      {
        seeds.insert (key, KnownCqSeed {call, grid, freq, dt, nutc, 1});
        return;
      }
    KnownCqSeed& seed = it.value ();
    seed.freq = freq;
    seed.dt = dt;
    seed.nutc = nutc;
    seed.hits = std::min (seed.hits + 1, 20);
  }

  void remember_known_cq_call_seed (QHash<QString, KnownCqCallSeed>& seeds,
                                    QString call, float freq, float dt, int nutc)
  {
    call = call.trimmed ().toUpper ();
    if (!plausible_hash_seed_token (call) || freq <= 0.0f || nutc <= 0)
      {
        return;
      }
    auto it = seeds.find (call);
    if (it == seeds.end ())
      {
        seeds.insert (call, KnownCqCallSeed {call, freq, dt, nutc, 1});
        return;
      }
    KnownCqCallSeed& seed = it.value ();
    seed.freq = freq;
    seed.dt = dt;
    seed.nutc = nutc;
    seed.hits = std::min (seed.hits + 1, 20);
  }

  int ft8_nutc_seconds (int nutc)
  {
    int const raw = std::abs (nutc);
    int const seconds = raw % 100;
    int const minutes = (raw / 100) % 100;
    int const hours = (raw / 10000) % 100;
    return hours * 3600 + minutes * 60 + seconds;
  }

  int ft8_nutc_delta_seconds (int newer, int older)
  {
    int delta = ft8_nutc_seconds (newer) - ft8_nutc_seconds (older);
    if (delta < -12 * 3600)
      {
        delta += 24 * 3600;
      }
    if (delta > 12 * 3600)
      {
        delta -= 24 * 3600;
      }
    return delta;
  }

  bool collect_known_cq_call_seeds_from_tail (QString const& path,
                                              QHash<QString, KnownCqCallSeed>& seeds,
                                              qint64 tailBytes,
                                              int currentNutc)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }

    static QRegularExpression const jtdxCqCallRx {
      QStringLiteral (R"(^(\d{8})_(\d{6})\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+~\s+CQ\s+([A-Z0-9/]{3,12})(?:\s+[\^*?#]+)?\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };
    static QRegularExpression const decodiumCqCallRx {
      QStringLiteral (R"(^(\d{6})_(\d{6})\s+\S+\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+CQ\s+([A-Z0-9/]{3,12})(?:\s+[\^*?#]+)?\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc ();
    constexpr qint64 kMaxAgeSeconds = 30 * 60 + 15;
    auto lineDate = [] (QString const& token) {
      if (token.size () == 8)
        {
          return QDate (token.left (4).toInt (), token.mid (4, 2).toInt (),
                        token.mid (6, 2).toInt ());
        }
      return QDate (2000 + token.left (2).toInt (), token.mid (2, 2).toInt (),
                    token.mid (4, 2).toInt ());
    };

    while (!file.atEnd ())
      {
        QString const line = QString::fromLatin1 (file.readLine ()).trimmed ().toUpper ();
        QRegularExpressionMatch match = jtdxCqCallRx.match (line);
        if (!match.hasMatch ())
          {
            match = decodiumCqCallRx.match (line);
          }
        if (!match.hasMatch ())
          {
            continue;
          }

        QDate const date = lineDate (match.captured (1));
        QTime const time = QTime::fromString (match.captured (2), QStringLiteral ("HHmmss"));
        QDateTime const when {date, time, QTimeZone::UTC};
        qint64 const age = when.isValid () ? when.secsTo (nowUtc) : -1;
        if (age < 0 || age > kMaxAgeSeconds)
          {
            continue;
          }

        bool okDt = false;
        bool okFreq = false;
        bool okNutc = false;
        float const dt = match.captured (3).toFloat (&okDt);
        float const freq = match.captured (4).toFloat (&okFreq);
        int const nutc = match.captured (2).toInt (&okNutc);
        if (!okFreq || !okNutc)
          {
            continue;
          }
        if (currentNutc > 0 && ft8_nutc_delta_seconds (currentNutc, nutc) <= 0)
          {
            continue;
          }
        remember_known_cq_call_seed (seeds, match.captured (5),
                                     freq, okDt ? dt : 0.0f, nutc);
      }
    return true;
  }

  bool collect_known_cq_seeds_from_tail (QString const& path,
                                         QHash<QString, KnownCqSeed>& seeds,
                                         qint64 tailBytes,
                                         int currentNutc)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }

    static QRegularExpression const jtdxCqRx {
      QStringLiteral (R"(^(\d{8})_(\d{6})\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+~\s+CQ\s+([A-Z0-9/]{3,12})\s+([A-R]{2}\d{2})\b)"),
      QRegularExpression::CaseInsensitiveOption
    };
    static QRegularExpression const decodiumCqRx {
      QStringLiteral (R"(^(\d{6})_(\d{6})\s+\S+\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+CQ\s+([A-Z0-9/]{3,12})\s+([A-R]{2}\d{2})\b)"),
      QRegularExpression::CaseInsensitiveOption
    };

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc ();
    constexpr qint64 kMaxAgeSeconds = 3 * 3600 + 15;
    auto lineDate = [] (QString const& token) {
      if (token.size () == 8)
        {
          return QDate (token.left (4).toInt (), token.mid (4, 2).toInt (),
                        token.mid (6, 2).toInt ());
        }
      return QDate (2000 + token.left (2).toInt (), token.mid (2, 2).toInt (),
                    token.mid (4, 2).toInt ());
    };

    while (!file.atEnd ())
      {
        QString const line = QString::fromLatin1 (file.readLine ()).trimmed ().toUpper ();
        QRegularExpressionMatch match = jtdxCqRx.match (line);
        if (!match.hasMatch ())
          {
            match = decodiumCqRx.match (line);
          }
        if (!match.hasMatch ())
          {
            continue;
          }

        QDate const date = lineDate (match.captured (1));
        QTime const time = QTime::fromString (match.captured (2), QStringLiteral ("HHmmss"));
        QDateTime const when {date, time, QTimeZone::UTC};
        qint64 const age = when.isValid () ? when.secsTo (nowUtc) : -1;
        if (age < 0 || age > kMaxAgeSeconds)
          {
            continue;
          }

        bool okDt = false;
        bool okFreq = false;
        bool okNutc = false;
        float const dt = match.captured (3).toFloat (&okDt);
        float const freq = match.captured (4).toFloat (&okFreq);
        int const nutc = match.captured (2).toInt (&okNutc);
        if (!okFreq || !okNutc)
          {
            continue;
          }
        if (currentNutc > 0 && ft8_nutc_delta_seconds (currentNutc, nutc) <= 0)
          {
            continue;
          }
        remember_known_cq_seed (seeds, match.captured (5), match.captured (6),
                                freq, okDt ? dt : 0.0f, nutc);
      }
    return true;
  }

  bool plausible_a7_message_token (QString const& token)
  {
    if (token == QStringLiteral ("CQ"))
      {
        return true;
      }
    if (token == QStringLiteral ("RRR") || token == QStringLiteral ("RR73")
        || token == QStringLiteral ("73"))
      {
        return true;
      }
    static QRegularExpression const reportRx {
      QStringLiteral (R"(^R?[+-]?\d{1,2}$)")
    };
    static QRegularExpression const gridRx {
      QStringLiteral (R"(^[A-R]{2}\d{2}$)")
    };
    if (reportRx.match (token).hasMatch () || gridRx.match (token).hasMatch ())
      {
        return true;
      }
    return plausible_hash_seed_token (token);
  }

  QString normalize_a7_logged_message (QString message)
  {
    message = message.trimmed ().toUpper ();
    message.remove (QRegularExpression {QStringLiteral (R"(\s+[\^*?#]+$)")});
    message = message.simplified ();
    if (message.isEmpty () || message.size () > kDecodedChars
        || message.contains (QLatin1Char ('<')) || message.contains (QLatin1Char ('/')))
      {
        return {};
      }

    QStringList const words = message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (words.size () < 2 || words.size () > 4)
      {
        return {};
      }
    if (words.first () == QStringLiteral ("CQ"))
      {
        if (words.size () > 3 || !plausible_hash_seed_token (words.value (1)))
          {
            return {};
          }
        if (words.size () == 3
            && !QRegularExpression {QStringLiteral (R"(^[A-R]{2}\d{2}$)")}.match (words.value (2)).hasMatch ())
          {
            return {};
          }
        return message;
      }
    if (!plausible_hash_seed_token (words.value (0))
        || !plausible_hash_seed_token (words.value (1)))
      {
        return {};
      }
    for (int index = 2; index < words.size (); ++index)
      {
        if (!plausible_a7_message_token (words.value (index)))
          {
            return {};
          }
      }
    return message;
  }

  int a7_message_seed_priority (QString const& message)
  {
    QStringList const words =
        message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (words.size () >= 3 && words.first () != QStringLiteral ("CQ"))
      {
        return 3;
      }
    if (words.size () >= 3 && words.first () == QStringLiteral ("CQ"))
      {
        return 2;
      }
    return 1;
  }

  void remember_a7_message_seed (QHash<QString, A7MessageSeed>& seeds,
                                 QString message, float freq, float dt, int nutc,
                                 int priority = 0)
  {
    message = normalize_a7_logged_message (message);
    if (message.isEmpty () || freq <= 0.0f || nutc <= 0)
      {
        return;
      }
    if (priority <= 0)
      {
        priority = a7_message_seed_priority (message);
      }
    auto it = seeds.find (message);
    if (it == seeds.end ())
      {
        seeds.insert (message, A7MessageSeed {message, freq, dt, nutc, 1, priority});
        return;
      }
    A7MessageSeed& seed = it.value ();
    seed.hits = std::min (seed.hits + 1, 16);
    seed.priority = std::max (seed.priority, priority);
    if (ft8_nutc_delta_seconds (nutc, seed.nutc) >= 0)
      {
        seed.freq = freq;
        seed.dt = dt;
        seed.nutc = nutc;
      }
  }

  bool a7_seed_is_directed_pair (A7MessageSeed const& seed)
  {
    QStringList const words =
        seed.message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    return seed.priority >= 4 && words.size () == 2;
  }

  void remember_a7_directed_pair_seeds (QHash<QString, A7MessageSeed>& seeds,
                                        QString const& normalizedMessage,
                                        float freq, float dt, int nutc)
  {
    QStringList const words =
        normalizedMessage.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (words.size () < 3 || words.first () == QStringLiteral ("CQ"))
      {
        return;
      }

    QString const call1 = words.value (0);
    QString const call2 = words.value (1);
    if (call1 == call2 || call1.contains (QLatin1Char ('/'))
        || call2.contains (QLatin1Char ('/'))
        || !plausible_hash_seed_token (call1)
        || !plausible_hash_seed_token (call2))
      {
        return;
      }

    remember_a7_message_seed (seeds, call1 + QLatin1Char (' ') + call2,
                              freq, dt, nutc, 4);
    remember_a7_message_seed (seeds, call2 + QLatin1Char (' ') + call1,
                              freq, dt, nutc, 4);
  }

  bool collect_a7_message_seeds_from_tail (QString const& path,
                                           QHash<QString, A7MessageSeed>& seeds,
                                           qint64 tailBytes,
                                           int currentNutc)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }

    static QRegularExpression const jtdxRx {
      QStringLiteral (R"(^(\d{8})_(\d{6})\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+~\s+(.+?)\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };
    static QRegularExpression const decodiumRx {
      QStringLiteral (R"(^(\d{6})_(\d{6})\s+\S+\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+(.+?)\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc ();
    constexpr qint64 kMaxWallAgeSeconds = 30 * 60 + 15;
    constexpr int kMaxDecodeAgeSeconds = 75;
    auto lineDate = [] (QString const& token) {
      if (token.size () == 8)
        {
          return QDate (token.left (4).toInt (), token.mid (4, 2).toInt (),
                        token.mid (6, 2).toInt ());
        }
      return QDate (2000 + token.left (2).toInt (), token.mid (2, 2).toInt (),
                    token.mid (4, 2).toInt ());
    };

    while (!file.atEnd ())
      {
        QString const line = QString::fromLatin1 (file.readLine ()).trimmed ().toUpper ();
        QRegularExpressionMatch match = jtdxRx.match (line);
        if (!match.hasMatch ())
          {
            match = decodiumRx.match (line);
          }
        if (!match.hasMatch ())
          {
            continue;
          }

        QDate const date = lineDate (match.captured (1));
        QTime const time = QTime::fromString (match.captured (2), QStringLiteral ("HHmmss"));
        QDateTime const when {date, time, QTimeZone::UTC};
        qint64 const wallAge = when.isValid () ? when.secsTo (nowUtc) : -1;
        if (wallAge < 0 || wallAge > kMaxWallAgeSeconds)
          {
            continue;
          }

        bool okDt = false;
        bool okFreq = false;
        bool okNutc = false;
        float const dt = match.captured (3).toFloat (&okDt);
        float const freq = match.captured (4).toFloat (&okFreq);
        int const nutc = match.captured (2).toInt (&okNutc);
        if (!okFreq || !okNutc)
          {
            continue;
          }
        if (currentNutc > 0)
          {
            int const decodeAge = ft8_nutc_delta_seconds (currentNutc, nutc);
            if (decodeAge <= 0 || decodeAge > kMaxDecodeAgeSeconds)
              {
                continue;
              }
          }
        QString const normalizedMessage = normalize_a7_logged_message (match.captured (5));
        if (!normalizedMessage.startsWith (QStringLiteral ("CQ ")))
          {
            remember_a7_message_seed (seeds, normalizedMessage,
                                      freq, okDt ? dt : 0.0f, nutc);
          }
        remember_a7_directed_pair_seeds (seeds, normalizedMessage,
                                         freq, okDt ? dt : 0.0f, nutc);
      }
    return true;
  }

  QStringList hash_seed_monthly_all_files ()
  {
    QStringList names;
    QDate const currentMonth = QDate::currentDate ();
    for (int i = kHashSeedPriorityMonths - 1; i >= 0; --i)
      {
        QDate const month = currentMonth.addMonths (-i);
        names.append (month.toString (QStringLiteral ("yyyyMM"))
                      + QStringLiteral ("_ALL.TXT"));
      }
    return names;
  }

  QString hash_seed_source_label (QString const& path)
  {
    QFileInfo const info {path};
    QFileInfo const dirInfo {info.absolutePath ()};
    return dirInfo.fileName () + QLatin1Char ('/')
           + info.fileName () + QStringLiteral (":")
           + QString::number (info.size ());
  }

  QStringList hash_seed_external_path_entries ()
  {
    static QStringList const entries = [] {
      QStringList result;
      QSet<QString> seen;
      auto addEntry = [&] (QString const& path) {
        QString const trimmed = path.trimmed ();
        if (trimmed.isEmpty ())
          {
            return;
          }
        QString const absolute = QFileInfo {trimmed}.absoluteFilePath ();
        QString const clean = QDir::cleanPath (absolute);
        if (!clean.isEmpty () && !seen.contains (clean))
          {
            seen.insert (clean);
            result.append (clean);
          }
      };

      for (char const* name : {"DECODIUM_HASH_SEED_PATHS", "DECODIUM_EXTERNAL_LOG_DIRS"})
        {
          QString const value = qEnvironmentVariable (name).trimmed ();
          if (value.isEmpty ())
            {
              continue;
            }
          for (QString const& entry : value.split (QDir::listSeparator (), Qt::SkipEmptyParts))
            {
              addEntry (entry);
            }
        }
      return result;
    } ();
    return entries;
  }

  bool hash_seed_path_is_external_override (QString const& path)
  {
    QString const cleanPath = QDir::cleanPath (QFileInfo {path}.absoluteFilePath ());
    if (cleanPath.isEmpty ())
      {
        return false;
      }
    for (QString const& entry : hash_seed_external_path_entries ())
      {
        QFileInfo const info {entry};
        QString const cleanEntry = QDir::cleanPath (info.absoluteFilePath ());
        if (cleanEntry.isEmpty ())
          {
            continue;
          }
        if (info.isDir ())
          {
            if (cleanPath == cleanEntry || cleanPath.startsWith (cleanEntry + QLatin1Char ('/')))
              {
                return true;
              }
          }
        else if (cleanPath == cleanEntry)
          {
            return true;
          }
      }
    return false;
  }

  QStringList local_hash_seed_paths ()
  {
    QStringList paths;
    QSet<QString> seen;
    auto addPath = [&paths, &seen] (QString const& path, bool priority = false) {
      QString const clean = QDir::cleanPath (path);
      if (clean.isEmpty ())
        {
          return;
        }
      if (seen.contains (clean))
        {
          if (priority)
            {
              paths.removeAll (clean);
              paths.append (clean);
            }
          return;
        }
      if (!clean.isEmpty ())
        {
          seen.insert (clean);
          paths.append (clean);
        }
    };

    auto addLogDir = [&addPath] (QString const& path, bool priority = false) {
      QDir const dir {path};
      if (!dir.exists ())
        {
          return;
        }
      static QStringList const filters {
        QStringLiteral ("CALL3.TXT"),
        QStringLiteral ("ALL.TXT"),
        QStringLiteral ("all.txt"),
        QStringLiteral ("*_ALL.TXT")
      };
      for (QString const& name : dir.entryList (filters, QDir::Files, QDir::Name))
        {
          addPath (dir.filePath (name), priority);
        }
    };

      auto addMonthlyAllTxt = [&addPath] (QString const& path, bool priority) {
        QDir const dir {path};
        if (!dir.exists ())
        {
          return;
        }
      for (QString const& name : hash_seed_monthly_all_files ())
        {
          addPath (dir.filePath (name), priority);
          }
      };

      auto addExternalEntry = [&] (QString const& path) {
        QFileInfo const info {path};
        if (info.isDir ())
          {
            QDir const dir {info.absoluteFilePath ()};
            addLogDir (dir.absolutePath (), true);
            addMonthlyAllTxt (dir.absolutePath (), true);
            for (QString const& child : {QStringLiteral ("WSJT-X"), QStringLiteral ("JTDX")})
              {
                QString const childPath = dir.filePath (child);
                addLogDir (childPath, true);
                addMonthlyAllTxt (childPath, true);
              }
            return;
          }
        addPath (path, true);
      };

      addLogDir (QStandardPaths::writableLocation (QStandardPaths::AppDataLocation));

    QStringList dataRoots;
    auto addDataRoot = [&dataRoots] (QString const& root) {
      QString const clean = QDir::cleanPath (root);
      if (!clean.isEmpty () && !dataRoots.contains (clean))
        {
          dataRoots.append (clean);
        }
    };
    addDataRoot (QStandardPaths::writableLocation (QStandardPaths::GenericDataLocation));
    addDataRoot (QDir {QDir::homePath ()}.filePath (QStringLiteral ("Library/Application Support")));

    for (QString const& root : dataRoots)
      {
        QDir const dir {root};
        addLogDir (dir.filePath (QStringLiteral ("IU8LMC/Decodium")));
        addLogDir (dir.filePath (QStringLiteral ("IU8LMC/Decodium/embedded-ft2")));
        addLogDir (dir.filePath (QStringLiteral ("WSJT-X")));
        addLogDir (dir.filePath (QStringLiteral ("JTDX")));
        addLogDir (dir.filePath (QStringLiteral ("IU8LMC/Decodium/embedded-ft2")), true);
          addMonthlyAllTxt (dir.filePath (QStringLiteral ("WSJT-X")), true);
          addMonthlyAllTxt (dir.filePath (QStringLiteral ("JTDX")), true);
        }
      for (QString const& path : hash_seed_external_path_entries ())
        {
          addExternalEntry (path);
        }
      return paths;
    }

  int hash_seed_path_priority (QString const& path)
  {
    QFileInfo const info {path};
    QString const cleanPath = QDir::cleanPath (path).toUpper ();
    QString const name = info.fileName ().toUpper ();
    int priority = 0;

      if (name == QStringLiteral ("CALL3.TXT"))
        {
          priority += 10;
      }
    if (name == QStringLiteral ("ALL.TXT")
        || name.endsWith (QStringLiteral ("_ALL.TXT")))
      {
        priority += 20;
      }

    if (cleanPath.contains (QStringLiteral ("/IU8LMC/DECODIUM/EMBEDDED-FT2/")))
      {
        priority += 20;
        if (name == QStringLiteral ("ALL.TXT"))
          {
            // Keep Decodium's live embedded ALL.TXT in the selected seed set.
            // It carries calls decoded during the current run, which lets later
            // type-2/hash messages resolve without waiting for another app log.
            priority += 80;
          }
      }
    else if (cleanPath.contains (QStringLiteral ("/IU8LMC/DECODIUM/")))
      {
        priority += 10;
        if (name == QStringLiteral ("ALL.TXT"))
          {
            priority += 70;
          }
      }
    if (cleanPath.contains (QStringLiteral ("/WSJT-X/")))
      {
        priority += 30;
      }
    if (cleanPath.contains (QStringLiteral ("/JTDX/")))
      {
          priority += 40;
        }
      if (hash_seed_path_is_external_override (path))
        {
          priority += 100;
        }

      QStringList const monthly = hash_seed_monthly_all_files ();
    if (!monthly.isEmpty () && name == monthly.constLast ().toUpper ())
      {
        priority += 50;
      }
    else
      {
        for (QString const& monthlyName : monthly)
          {
            if (name == monthlyName.toUpper ())
              {
                priority += 30;
                break;
              }
          }
      }

    return priority;
  }

  bool hash_seed_path_is_required_under_budget (QString const& path)
  {
    QFileInfo const info {path};
    QString const cleanPath = QDir::cleanPath (path).toUpper ();
    QString const name = info.fileName ().toUpper ();

    if (cleanPath.contains (QStringLiteral ("/IU8LMC/DECODIUM/EMBEDDED-FT2/"))
        && name == QStringLiteral ("ALL.TXT"))
      {
        return true;
      }

    QString const currentMonthly =
      QDate::currentDate ().toString (QStringLiteral ("yyyyMM")).toUpper ()
      + QStringLiteral ("_ALL.TXT");
    return name == currentMonthly
           && (cleanPath.contains (QStringLiteral ("/JTDX/"))
               || cleanPath.contains (QStringLiteral ("/WSJT-X/")));
  }

  bool hash_seed_remaining_required_under_budget (QStringList const& paths,
                                                  int startIndex)
  {
    for (int i = startIndex; i < paths.size (); ++i)
      {
        if (hash_seed_path_is_required_under_budget (paths.at (i)))
          {
            return true;
          }
      }
    return false;
  }

  QStringList local_hash_seed_priority_paths (int maxFiles = 0)
  {
    struct Candidate
    {
      QString path;
      int priority {};
      QDateTime modified;
    };

    QList<Candidate> candidates;
    for (QString const& path : local_hash_seed_paths ())
      {
        QFileInfo const info {path};
        if (!info.exists () || !info.isFile ())
          {
            continue;
          }
        candidates.append ({path, hash_seed_path_priority (path), info.lastModified ()});
      }

    std::sort (candidates.begin (), candidates.end (), [] (Candidate const& lhs,
                                                            Candidate const& rhs) {
      if (lhs.priority != rhs.priority)
        {
          return lhs.priority > rhs.priority;
        }
      if (lhs.modified != rhs.modified)
        {
          return lhs.modified > rhs.modified;
        }
      return lhs.path < rhs.path;
    });

    if (maxFiles > 0 && candidates.size () > maxFiles)
      {
        candidates.erase (candidates.begin () + maxFiles, candidates.end ());
      }

    // HashSeedCallAccumulator keeps the most recently processed calls when it
    // trims. Process the selected set from lower to higher priority so current
    // monthly JTDX/WSJT-X logs win over older or generic sources.
    std::sort (candidates.begin (), candidates.end (), [] (Candidate const& lhs,
                                                            Candidate const& rhs) {
      if (lhs.priority != rhs.priority)
        {
          return lhs.priority < rhs.priority;
        }
      if (lhs.modified != rhs.modified)
        {
          return lhs.modified < rhs.modified;
        }
      return lhs.path < rhs.path;
    });

    QStringList paths;
    paths.reserve (candidates.size ());
    for (Candidate const& candidate : candidates)
      {
        paths.append (candidate.path);
      }
    return paths;
  }

  bool external_known_cq_seed_enabled ();
  bool external_known_cq_source_allowed (QString const& path);
  bool external_a7_seed_enabled ();

  HashSeedSnapshot collect_initial_hash_seed_snapshot ()
  {
    QElapsedTimer timer;
    timer.start ();
    HashSeedSnapshot snapshot;
    HashSeedCallAccumulator accumulator;
    QStringList const seedPaths =
      local_hash_seed_priority_paths (kHashSeedRefreshMaxFiles);
    for (int i = 0; i < seedPaths.size (); ++i)
      {
        QString const& path = seedPaths.at (i);
        if (hash_seed_shutdown_requested ())
          {
            break;
          }
        if (timer.elapsed () >= kHashSeedInitialSoftBudgetMs
            && !hash_seed_path_is_required_under_budget (path))
          {
            if (hash_seed_remaining_required_under_budget (seedPaths, i + 1))
              {
                continue;
              }
            break;
          }
        if (collect_hash_seed_calls_from_tail (path, accumulator))
          {
            ++snapshot.filesRead;
            snapshot.sources.append (hash_seed_source_label (path));
          }
        if (timer.elapsed () >= kHashSeedInitialSoftBudgetMs
            && !hash_seed_remaining_required_under_budget (seedPaths, i + 1))
          {
            break;
          }
      }
    snapshot.calls = accumulator.to_list ();
    snapshot.elapsedMs = timer.elapsed ();
    return snapshot;
  }

  HashContextRefresh collect_hash_context_refresh_snapshot (int currentNutc)
  {
    QElapsedTimer timer;
    timer.start ();
    HashContextRefresh refresh;
    refresh.currentNutc = currentNutc;
    refresh.seedKnownCqReplay = external_known_cq_seed_enabled ();
    refresh.seedExternalA7 = external_a7_seed_enabled ();

    HashSeedCallAccumulator accumulator;
    QStringList const seedPaths =
      local_hash_seed_priority_paths (kHashSeedRefreshMaxFiles);
    for (int i = 0; i < seedPaths.size (); ++i)
      {
        QString const& path = seedPaths.at (i);
        if (hash_seed_shutdown_requested ())
          {
            break;
          }
        if (timer.elapsed () >= kHashSeedRefreshSoftBudgetMs
            && !hash_seed_path_is_required_under_budget (path))
          {
            if (hash_seed_remaining_required_under_budget (seedPaths, i + 1))
              {
                continue;
              }
            break;
          }
        collect_hash_seed_calls_from_tail (path, accumulator, kHashSeedRefreshTailBytes);
        if (refresh.seedKnownCqReplay && external_known_cq_source_allowed (path))
          {
            collect_known_cq_call_seeds_from_tail (path, refresh.cqCallSeeds,
                                                   kHashSeedRefreshTailBytes,
                                                   currentNutc);
            collect_known_cq_seeds_from_tail (path, refresh.cqSeeds,
                                              kHashSeedRefreshTailBytes,
                                              currentNutc);
          }
        if (refresh.seedExternalA7)
          {
            collect_a7_message_seeds_from_tail (path, refresh.a7Seeds,
                                                kHashSeedRefreshTailBytes,
                                                currentNutc);
          }
        ++refresh.filesRead;
        if (refresh.filesRead >= kHashSeedRefreshMaxFiles
            || (timer.elapsed () >= kHashSeedRefreshSoftBudgetMs
                && !hash_seed_remaining_required_under_budget (seedPaths, i + 1)))
          {
            break;
          }
      }

    refresh.calls = accumulator.to_list ();
    refresh.elapsedMs = timer.elapsed ();
    return refresh;
  }

  struct HashSeedAsyncState
  {
    std::mutex mutex;
    std::condition_variable workAvailable;
    bool workerStarted {false};
    bool shutdownRequested {false};
    std::thread worker;
    bool initialStarted {false};
    bool initialRequested {false};
    bool initialReady {false};
    bool initialApplied {false};
    HashSeedSnapshot initialCompleted;
    HashSeedSnapshot initialPending;
    int initialPendingRemaining {0};
    qint64 lastRefreshStartMs {0};
    bool refreshRequested {false};
    bool refreshReady {false};
    int refreshNutc {0};
    HashContextRefresh refreshCompleted;
  };

  HashSeedAsyncState& hash_seed_async_state ()
  {
    // Keep the state allocated until process exit, but explicitly join the
    // worker before Qt teardown. The state itself is intentionally leaked so
    // its condition variable and mutex remain valid during the final join.
    static HashSeedAsyncState* state = new HashSeedAsyncState;
    return *state;
  }

  void start_hash_seed_worker_locked (HashSeedAsyncState& state)
  {
    if (state.workerStarted || state.shutdownRequested)
      {
        return;
      }
    state.workerStarted = true;
    state.worker = std::thread ([&state] {
      QThread::currentThread ()->setObjectName (QStringLiteral ("FT8HashSeedWorker"));
      QThread::currentThread ()->setPriority (QThread::LowPriority);

      for (;;)
        {
          bool collectInitial = false;
          bool collectRefresh = false;
          int refreshNutc = 0;
          {
            std::unique_lock<std::mutex> lock {state.mutex};
            state.workAvailable.wait (lock, [&state] {
              return state.shutdownRequested
                     || state.initialRequested || state.refreshRequested;
            });
            if (state.shutdownRequested)
              {
                break;
              }
            if (state.initialRequested)
              {
                state.initialRequested = false;
                collectInitial = true;
              }
            else if (state.refreshRequested)
              {
                state.refreshRequested = false;
                refreshNutc = state.refreshNutc;
                collectRefresh = true;
              }
          }

          if (collectInitial)
            {
              HashSeedSnapshot snapshot = collect_initial_hash_seed_snapshot ();
              std::lock_guard<std::mutex> lock {state.mutex};
              state.initialCompleted = std::move (snapshot);
              state.initialReady = true;
            }
          else if (collectRefresh)
            {
              HashContextRefresh refresh =
                  collect_hash_context_refresh_snapshot (refreshNutc);
              std::lock_guard<std::mutex> lock {state.mutex};
              state.refreshCompleted = std::move (refresh);
              state.refreshReady = true;
            }
        }
    });
  }

  void shutdown_hash_seed_worker ()
  {
    g_hashSeedShutdownRequested.store (true, std::memory_order_release);
    HashSeedAsyncState& state = hash_seed_async_state ();
    std::thread worker;
    {
      std::lock_guard<std::mutex> lock {state.mutex};
      state.shutdownRequested = true;
      state.initialRequested = false;
      state.refreshRequested = false;
      state.workAvailable.notify_all ();
      worker = std::move (state.worker);
    }
    if (worker.joinable ())
      {
        worker.join ();
      }
  }

  void start_ft8_pack77_hash_seed_collection_once ()
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    if (qEnvironmentVariableIntValue ("DECODIUM_DISABLE_FT8_HASH_LOG_REFRESH") > 0)
      {
        return;
      }
    HashSeedAsyncState& state = hash_seed_async_state ();
    std::lock_guard<std::mutex> lock {state.mutex};
    if (state.initialStarted)
      {
        return;
      }
    start_hash_seed_worker_locked (state);
    state.initialStarted = true;
    state.initialRequested = true;
    state.workAvailable.notify_one ();
  }

  void apply_ready_ft8_pack77_hash_seed_collection ()
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    bool haveCompletedSnapshot = false;
    int collectedCalls = 0;
    int collectedFiles = 0;
    qint64 collectionElapsedMs = 0;
    {
      HashSeedAsyncState& state = hash_seed_async_state ();
      std::lock_guard<std::mutex> lock {state.mutex};
      if (!state.initialApplied && state.initialReady)
        {
          state.initialPending = std::move (state.initialCompleted);
          state.initialReady = false;
          state.initialPendingRemaining = state.initialPending.calls.size ();
          state.initialApplied = true;
          collectedCalls = state.initialPending.calls.size ();
          collectedFiles = state.initialPending.filesRead;
          collectionElapsedMs = state.initialPending.elapsedMs;
          haveCompletedSnapshot = true;
        }
    }

    QElapsedTimer applyTimer;
    applyTimer.start ();
    int applied = 0;
    qint64 slowestCallMs = 0;
    while (applied < kHashSeedApplyBatchCalls
           && applyTimer.elapsed () < kHashSeedApplyBudgetMs)
      {
        QString call;
        {
          HashSeedAsyncState& state = hash_seed_async_state ();
          std::lock_guard<std::mutex> lock {state.mutex};
          if (!state.initialApplied || state.initialPendingRemaining <= 0)
            {
              break;
            }
          // Apply newest calls first; they are the useful ones for live traffic.
          call = state.initialPending.calls.at (--state.initialPendingRemaining);
        }
        QByteArray const field = call.toLatin1 ().leftJustified (13, ' ', true);
        qint64 const callStartMs = applyTimer.elapsed ();
        ftx_ft8_stage4_seed_hash_call_c (field.constData ());
        slowestCallMs = std::max (slowestCallMs,
                                  applyTimer.elapsed () - callStartMs);
        ++applied;
      }

    if (applied > 0)
      {
        HashSeedAsyncState& state = hash_seed_async_state ();
        std::lock_guard<std::mutex> lock {state.mutex};
        if (haveCompletedSnapshot || applyTimer.elapsed () >= 50
            || state.initialPendingRemaining == 0)
          {
            qInfo().noquote ()
                << "[HASHMETRIC] initial_seed_batch"
                << "collected=" << (haveCompletedSnapshot ? 1 : 0)
                << "collected_calls=" << collectedCalls
                << "collected_files=" << collectedFiles
                << "collection_ms=" << collectionElapsedMs
                << "applied=" << applied
                << "elapsed_ms=" << applyTimer.elapsed ()
                << "slowest_call_ms=" << slowestCallMs
                << "remaining=" << state.initialPendingRemaining;
          }
        if (state.initialPendingRemaining == 0)
          {
            state.initialPending = {};
          }
      }
  }

  bool external_known_cq_seed_enabled ()
  {
    QString const value =
        qEnvironmentVariable ("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY").trimmed ().toLower ();
    if (value.isEmpty ())
      {
        return true;
      }
    return value == QStringLiteral ("1")
           || value == QStringLiteral ("true")
           || value == QStringLiteral ("yes");
  }

  bool external_known_cq_source_allowed (QString const& path)
  {
    QFileInfo const info {path};
    QString const dir = info.absolutePath ();
    QString const name = info.fileName ();
    return dir.contains (QStringLiteral ("JTDX"), Qt::CaseInsensitive)
           || dir.contains (QStringLiteral ("WSJT-X"), Qt::CaseInsensitive)
           || dir.contains (QStringLiteral ("Decodium"), Qt::CaseInsensitive)
           || name.startsWith (QStringLiteral ("JTDX"), Qt::CaseInsensitive)
           || name == QStringLiteral ("ALL.TXT")
           || name == QStringLiteral ("all.txt")
           || name.endsWith (QStringLiteral ("_ALL.TXT"), Qt::CaseInsensitive);
  }

  bool stable_recent_known_cq_seed (KnownCqSeed const& seed, int currentNutc)
  {
    if (seed.hits < 4 || currentNutc <= 0)
      {
        return false;
      }
    int const age = ft8_nutc_delta_seconds (currentNutc, seed.nutc);
    return age > 0 && age <= 150 && std::fabs (seed.dt) <= 2.5f;
  }

  bool stable_recent_known_cq_call_seed (KnownCqCallSeed const& seed,
                                         int currentNutc)
  {
    if (seed.hits < 4 || currentNutc <= 0)
      {
        return false;
    }
    int const age = ft8_nutc_delta_seconds (currentNutc, seed.nutc);
    return age > 0 && age <= 150 && std::fabs (seed.dt) <= 2.5f;
  }

  bool external_a7_seed_enabled ()
  {
    QString const value =
        qEnvironmentVariable ("DECODIUM_FT8_SEED_A7_HINTS").trimmed ().toLower ();
    if (value.isEmpty ())
      {
        return true;
      }
    if (value == QStringLiteral ("0")
        || value == QStringLiteral ("false")
        || value == QStringLiteral ("no"))
      {
        return false;
      }
    return value == QStringLiteral ("1")
           || value == QStringLiteral ("true")
           || value == QStringLiteral ("yes");
  }

  void start_ft8_hash_context_refresh_from_recent_logs (int currentNutc)
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    if (qEnvironmentVariableIntValue ("DECODIUM_DISABLE_FT8_HASH_LOG_REFRESH") > 0)
      {
        return;
      }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch ();
    HashSeedAsyncState& state = hash_seed_async_state ();
    std::lock_guard<std::mutex> lock {state.mutex};
    if (!state.initialApplied || state.initialPendingRemaining > 0)
      {
        return;
      }
    if (state.refreshRequested || state.refreshReady)
      {
        return;
      }
    if (state.lastRefreshStartMs > 0
        && nowMs - state.lastRefreshStartMs < kHashSeedRefreshIntervalMs)
      {
        return;
      }
    start_hash_seed_worker_locked (state);
    state.lastRefreshStartMs = nowMs;
    state.refreshNutc = currentNutc;
    state.refreshRequested = true;
    state.workAvailable.notify_one ();
  }

  void apply_ready_ft8_hash_context_refresh ()
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    HashContextRefresh refresh;
    bool haveRefresh = false;
    {
      HashSeedAsyncState& state = hash_seed_async_state ();
      std::lock_guard<std::mutex> lock {state.mutex};
      if (state.refreshReady)
        {
          refresh = std::move (state.refreshCompleted);
          state.refreshReady = false;
          haveRefresh = true;
        }
    }
    if (!haveRefresh)
      {
        return;
      }

    int const refreshCallCount = static_cast<int> (refresh.calls.size ());
    int const firstRefreshCall =
        std::max (0, refreshCallCount - kHashSeedRefreshApplyMaxCalls);
    for (int i = firstRefreshCall; i < refreshCallCount; ++i)
      {
        QString const& call = refresh.calls.at (i);
        QByteArray const field = call.toLatin1 ().leftJustified (13, ' ', true);
        ftx_ft8_stage4_seed_hash_call_c (field.constData ());
      }
    if (refresh.seedKnownCqReplay)
      {
        for (KnownCqSeed const& seed : refresh.cqSeeds)
          {
            if (!stable_recent_known_cq_seed (seed, refresh.currentNutc))
              {
                continue;
              }
            QByteArray const callField =
                seed.call.toLatin1 ().leftJustified (12, ' ', true);
            QByteArray const gridField =
                seed.grid.toLatin1 ().leftJustified (4, ' ', true);
            int const repeatCount = qBound (1, seed.hits, 4);
            for (int repeat = 0; repeat < repeatCount; ++repeat)
              {
                ftx_ft8_stage4_seed_known_cq_c (callField.constData (),
                                                gridField.constData (), seed.freq,
                                                seed.dt, seed.nutc);
              }
          }
        for (KnownCqCallSeed const& seed : refresh.cqCallSeeds)
          {
            if (!stable_recent_known_cq_call_seed (seed, refresh.currentNutc))
              {
                continue;
              }
            QByteArray const callField =
                seed.call.toLatin1 ().leftJustified (12, ' ', true);
            int const repeatCount = qBound (1, seed.hits, 4);
            for (int repeat = 0; repeat < repeatCount; ++repeat)
              {
                ftx_ft8_stage4_seed_known_cq_call_c (callField.constData (),
                                                     seed.freq, seed.dt, seed.nutc);
              }
          }
      }
    if (!refresh.seedExternalA7)
      {
        return;
      }
    QList<A7MessageSeed> a7SeedList = refresh.a7Seeds.values ();
	    std::sort (a7SeedList.begin (), a7SeedList.end (),
	               [&refresh] (A7MessageSeed const& lhs, A7MessageSeed const& rhs) {
                 if (lhs.priority != rhs.priority)
                   {
                     return lhs.priority > rhs.priority;
                   }
	                 int const lhsAge = refresh.currentNutc > 0
	                     ? ft8_nutc_delta_seconds (refresh.currentNutc, lhs.nutc)
	                     : 0;
                 int const rhsAge = refresh.currentNutc > 0
                     ? ft8_nutc_delta_seconds (refresh.currentNutc, rhs.nutc)
                     : 0;
                 if (lhsAge != rhsAge)
                   {
                     return lhsAge < rhsAge;
                   }
                 return lhs.hits > rhs.hits;
               });
    int seededA7 = 0;
    int insertedA7 = 0;
    int seededA7Pairs = 0;
	    for (A7MessageSeed const& seed : a7SeedList)
	      {
        if (seededA7 >= kExternalA7SeedLimit || insertedA7 >= kExternalA7InsertLimit)
	          {
	            break;
	          }
        bool const pairSeed = a7_seed_is_directed_pair (seed);
        if (pairSeed && seededA7Pairs >= kExternalA7PairSeedLimit)
          {
            continue;
          }
        int const jseq = std::abs ((seed.nutc / 5) % 2);
        QByteArray const messageField =
            seed.message.toLatin1 ().leftJustified (kDecodedChars, ' ', true);
        int const repeatCount = seed.priority >= 3 ? qBound (1, seed.hits, 3) : 1;
        int const allowedRepeats = std::min (repeatCount,
                                             kExternalA7InsertLimit - insertedA7);
        for (int repeat = 0; repeat < allowedRepeats; ++repeat)
          {
            ftx_ft8_a7_save_c (jseq, seed.dt, seed.freq, messageField.constData ());
          }
        insertedA7 += allowedRepeats;
        ++seededA7;
        if (pairSeed)
          {
            ++seededA7Pairs;
          }
      }
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

  QString normalized_call_token (QByteArray const& raw)
  {
    return QString::fromLatin1 (raw).trimmed ().toUpper ().remove (QLatin1Char {'<'})
        .remove (QLatin1Char {'>'});
  }

  QString base_call_token (QString const& call)
  {
    if (!call.contains (QLatin1Char {'/'}))
      {
        return call;
      }
    QString best;
    for (QString const& part : call.split (QLatin1Char {'/'}, Qt::SkipEmptyParts))
      {
        bool hasLetter = false;
        bool hasDigit = false;
        for (QChar const ch : part)
          {
            hasLetter = hasLetter || ch.isLetter ();
            hasDigit = hasDigit || ch.isDigit ();
          }
        if (hasLetter && hasDigit && part.size () > best.size ())
          {
            best = part;
          }
      }
    return best.isEmpty () ? call : best;
  }

  bool message_has_call (QString const& message, QString const& call)
  {
    if (call.isEmpty ())
      {
        return false;
      }
    QString const base = base_call_token (call);
    QString normalized = message.toUpper ();
    normalized.replace (QLatin1Char {'<'}, QLatin1Char {' '});
    normalized.replace (QLatin1Char {'>'}, QLatin1Char {' '});
    static QRegularExpression const split {QStringLiteral (R"([^A-Z0-9/]+)")};
    for (QString const& token : normalized.split (split, Qt::SkipEmptyParts))
      {
        if (token == call || token == base || base_call_token (token) == base)
          {
            return true;
          }
      }
    return false;
  }

  bool is_qso_exchange (QString const& message)
  {
    static QRegularExpression const exchange {
      QStringLiteral (R"((?:^|\s)(?:R?[+-]\d{1,2}|RRR|RR73|73)(?:\s|$))"),
      QRegularExpression::CaseInsensitiveOption
    };
    return exchange.match (message).hasMatch ();
  }

  QVector<decodium::ft8::DecodedEntry> build_entries (
      QString const& utcPrefix, decodium::ft8::DecodeRequest const& request, int nout,
      int const* snrs, float const* dts, float const* freqs,
      int const* naps, float const* quals, char const* decodeds)
  {
    QVector<decodium::ft8::DecodedEntry> entries;
    int const count = qBound (0, nout, kFt8MaxLines);
    entries.reserve (count);
    QString const myCall = normalized_call_token (request.mycall);
    QString const hisCall = normalized_call_token (request.hiscall);
    for (int i = 0; i < count; ++i)
      {
        decodium::ft8::DecodedEntry entry;
        entry.row = build_row (utcPrefix, snrs[i], dts[i], freqs[i], naps[i], quals[i],
                               decodeds, i);
        entry.message = decode_fallback (decodeds, i).trimmed ();
        entry.decodedText0.emplace (entry.row);
        entry.decodedText.emplace (QString {entry.row}.remove ("TU; "));
        entry.snr = snrs[i];
        entry.dt = dts[i];
        entry.audioFrequency = qRound (freqs[i]);
        entry.apType = naps[i];
        entry.quality = quals[i];
        entry.addressedToMe = message_has_call (entry.message, myCall);
        entry.fromActivePartner = message_has_call (entry.message, hisCall);
        entry.qsoExchange = is_qso_exchange (entry.message);
        entries.append (std::move (entry));
      }
    return entries;
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

void shutdownHashSeedWorker ()
{
  shutdown_hash_seed_worker ();
}

FT8DecodeWorker::FT8DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FT8DecodeWorker::markLatestDecodeSerial (quint64 serial)
{
  m_latestDecodeSerial.store (serial, std::memory_order_relaxed);
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
  QElapsedTimer totalTimer;
  totalTimer.start ();
  quint64 latestSerial = m_latestDecodeSerial.load (std::memory_order_relaxed);
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || latestSerial == ~quint64(0)) // P0 1.0.404: scarta SOLO su invalidazione esplicita (mode/band change -> sentinel UINT64_MAX, o shutdown); il normale avanzamento di serial NON scarta piu' i pass per-slot. Fix lista FT8 vuota: il gate per-serial 1.0.399 scartava il fast/final pass quando l'early predecode dello slot successivo sovrascriveva latestSerial. Backstop mode/band intatto: cancelCurrentDecode + removePostedEvents purgano le code; harvest subpass non piu' speciale (anch'esso si ferma su invalidazione).
    {
      return;
    }
  bool const pressureLimitedRequest = request.cpuPressureLimited;
  // A live deadline is not evidence of CPU pressure.  Fast hosts must retain
  // their requested parallelism and decode profile; only the bridge's measured
  // pressure signal is allowed to reduce work for slower or overloaded hosts.
  int const effectiveRequestedThreadLimit =
      pressureLimitedRequest ? std::min (request.threadCount, 2) : request.threadCount;
  int const effectiveDepth =
      pressureLimitedRequest ? std::min (request.ndepth, 2) : request.ndepth;
  bool const effectiveApEnabled =
      !pressureLimitedRequest && request.lft8apon;
  decodium::decode::applyCurrentDecodeWorkerPriority ();
  int const reservedThreadLimit = decodium::decode::threadLimitWithUiReserve (effectiveRequestedThreadLimit);
  apply_decode_thread_limit (reservedThreadLimit);
  int const activeThreads = active_decode_thread_limit ();
  decodium::decode::applyOpenMpDecodeWorkerPriority (activeThreads);
  set_ft8_stage4_cancel (false);
  start_ft8_pack77_hash_seed_collection_once ();
  if (!pressureLimitedRequest)
    {
      start_ft8_hash_context_refresh_from_recent_logs (request.nutc);
    }
  log_ft8_dsp_rollout_once ();
  QElapsedTimer waitTimer;
  waitTimer.start ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  qint64 const waitMs = waitTimer.elapsed ();

  latestSerial = m_latestDecodeSerial.load (std::memory_order_relaxed);
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || latestSerial == ~quint64(0)) // P0 1.0.404: scarta SOLO su invalidazione esplicita (mode/band change -> sentinel UINT64_MAX, o shutdown); il normale avanzamento di serial NON scarta piu' i pass per-slot. Fix lista FT8 vuota: il gate per-serial 1.0.399 scartava il fast/final pass quando l'early predecode dello slot successivo sovrascriveva latestSerial. Backstop mode/band intatto: cancelCurrentDecode + removePostedEvents purgano le code; harvest subpass non piu' speciale (anch'esso si ferma su invalidazione).
    {
      return;
    }
  qint64 const hashPrepStartMs = totalTimer.elapsed ();
  qint64 initialHashPrepMs = 0;
  qint64 refreshHashPrepMs = 0;
  if (!pressureLimitedRequest)
    {
      qint64 const initialHashPrepStartMs = totalTimer.elapsed ();
      apply_ready_ft8_pack77_hash_seed_collection ();
      initialHashPrepMs = totalTimer.elapsed () - initialHashPrepStartMs;
      qint64 const refreshHashPrepStartMs = totalTimer.elapsed ();
      apply_ready_ft8_hash_context_refresh ();
      refreshHashPrepMs = totalTimer.elapsed () - refreshHashPrepStartMs;
      // Every seed call is applied directly and the pack77 context deliberately
      // persists across candidates and slots. Replaying the whole cache here is
      // redundant and, on Windows, can block the shared runtime for many seconds.
    }
  qint64 const hashPrepMs = totalTimer.elapsed () - hashPrepStartMs;
  if (hashPrepMs >= 50)
    {
      qInfo().noquote ()
          << "[HASHMETRIC] decode_prep"
          << "serial=" << request.serial
          << "total_ms=" << hashPrepMs
          << "initial_ms=" << initialHashPrepMs
          << "refresh_ms=" << refreshHashPrepMs
          << "replay=disabled";
    }
  ftx_ft8_stage4_set_deadline_ms_c (ft8_stage4_deadline_ms_from_now (request.maxDecodeMs));
  // The promoted C++ Stage4 path still avoids the full JTDX ft8b subpass
  // engine, but a second candidate cycle with RX-frequency sensitivity 2
  // recovers repeatable JTDX-only decodes while staying under the existing
  // live deadline. Keep early and supplemental passes on the cheaper profile.
  constexpr bool effectiveLowThresholds {false};
  bool const effectiveSubpass {request.subpass};  // F0: era hardcoded false; ora pilotato dal toggle ft8SubpassHarvest
  bool const pressureConstrainedProfile =
      pressureLimitedRequest || request.threadCount <= 2;
  bool const fullLiveDecode =
      effectiveDepth >= 3
      && !request.supplemental
      && !pressureConstrainedProfile
      && request.maxDecodeMs >= 7000;
  int const effectiveCycles = fullLiveDecode ? 2 : 1;
  int const effectiveRxFreqSensitivity = fullLiveDecode ? 2 : 1;
  int const effectiveCandidateThin =
      pressureLimitedRequest ? std::min (qBound (1, request.ncandthin, 100), 20)
                             : qBound (1, request.ncandthin, 100);
  ftx_ft8_stage4_set_decode_options_c (effectiveLowThresholds ? 1 : 0,
                                       effectiveSubpass ? 1 : 0,
                                       effectiveCycles,
                                       effectiveRxFreqSensitivity,
                                       effectiveCandidateThin);
  // In the full live pass, request the Stage4 focused-window rescue without
  // promoting the whole-band decode to depth 4 or enabling global OSD.
  bool const fullLiveFocusedRescue = fullLiveDecode;
  bool const supplementalRequested =
      !pressureLimitedRequest
      && (request.supplemental || effectiveDepth >= 4 || fullLiveFocusedRescue);
  bool const osdSupplementalRequested =
      !pressureLimitedRequest
      && (request.supplemental || effectiveDepth >= 4);
  ftx_ft8_stage4_set_supplemental_c (supplementalRequested ? 1 : 0);
  ftx_ft8_stage4_set_superfox_options_c (request.superFoxEnabled ? 1 : 0,
                                         qBound (1, request.superFoxTolHz, 200));
  // Harvest subpass: dig FRESH (reset slot state, a7 preserved) so the
  // parallelized subpass re-scans for weak signals, not the deep residual.
  ftx_ft8_stage4_set_force_fresh_slot_c (effectiveSubpass ? 1 : 0);
  ftx_ft8_stage4_set_freqpart_c (effectiveSubpass ? 4 : 0);
  // Harvest: detection piu' aggressiva (sweep S4) -> +mid-weak. Deep invariato.
  ftx_ft8_stage4_set_syncmin_scale_c (effectiveSubpass ? 0.55f : 1.0f);
  ftx_ft8_stage4_set_decode_syncmin_c (effectiveSubpass ? 2 : -1);
  // Turbo Feedback: estende belief-propagation a 50 iter (default 30).
  ftx_ft8_stage4_set_ldpc_max_iter_c (request.turboFeedbackEnabled ? 50 : 30);
  bool const wantOsd = !pressureLimitedRequest
                       && (request.osdFollowup
                           || (osdSupplementalRequested && effectiveDepth >= 3)
                           || request.neuralSyncEnabled);
  if (osdSupplementalRequested && request.ndepth >= 4)
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
  int ndepth = qBound (1, effectiveDepth, 4);
  float emedelay = request.emedelay;
  int ncontest = qBound (0, request.ncontest, 16);
  int nagain = request.nagain ? 1 : 0;
  int lft8apon = effectiveApEnabled ? 1 : 0;
  int ltry_a8 = (nzhsym == 41 || request.lmultift8 != 0) ? 1 : 0;
  int lapcqonly = request.lapcqonly ? 1 : 0;
  int napwid = qBound (0, request.napwid, 200);
  int ldiskdat = request.ldiskdat ? 1 : 0;
  int nout = 0;
  int const requestedDepth = ndepth;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  auto hisgrid = to_fortran_field (request.hisgrid, 6);

  QElapsedTimer decodeTimer;
  decodeTimer.start ();
  ftx_ft8_async_decode_stage4_c (iwave, &nqsoprogress, &nfqso, &nftx, &nutc, &nfa, &nfb,
                                 &nzhsym, &ndepth, &emedelay, &ncontest, &nagain,
                                 &lft8apon, &ltry_a8, &lapcqonly, &napwid, mycall.constData (),
                                 hiscall.constData (), hisgrid.constData (), &ldiskdat,
                                 nullptr,
                                 &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                                 &bits77[0], &decodeds[0], &nout);
  qint64 const decodeMs = decodeTimer.elapsed ();
  ftx_ft8_stage4_set_deadline_ms_c (0);
  ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
  ftx_ft8_stage4_set_decode_options_c (0, 0, 1, 1, 100);
  ftx_ft8_stage4_set_supplemental_c (0);
  ftx_ft8_stage4_set_superfox_options_c (0, 0);
  ftx_ft8_stage4_set_ldpc_max_iter_c (30);

  latestSerial = m_latestDecodeSerial.load (std::memory_order_relaxed);
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || latestSerial == ~quint64(0)) // P0 1.0.404: scarta SOLO su invalidazione esplicita (mode/band change -> sentinel UINT64_MAX, o shutdown); il normale avanzamento di serial NON scarta piu' i pass per-slot. Fix lista FT8 vuota: il gate per-serial 1.0.399 scartava il fast/final pass quando l'early predecode dello slot successivo sovrascriveva latestSerial. Backstop mode/band intatto: cancelCurrentDecode + removePostedEvents purgano le code; harvest subpass non piu' speciale (anch'esso si ferma su invalidazione).
    {
      return;
    }
  QString const utcPrefix = format_decode_utc (request.nutc);
  auto entries = build_entries (utcPrefix, request, nout, snrs, dts, freqs, naps, quals,
                                decodeds);
  QStringList rows;
  rows.reserve (entries.size ());
  for (auto const& entry : entries)
    {
      rows.append (entry.row);
    }
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
  qint64 const totalMs = totalTimer.elapsed ();
  static std::atomic<qint64> lastMetricLogMs {0};
  if (decodium::logging::should_log_decode_metric (waitMs, decodeMs, totalMs, lastMetricLogMs))
    {
      qInfo().noquote()
          << QStringLiteral ("[DECODEMETRIC] mode=FT8 serial=%1 wait_ms=%2 decode_ms=%3 total_ms=%4 threads_req=%5 threads_active=%6 audio=%7 nout=%8 depth=%9 nfa=%10 nfb=%11 ap=%12 low=%13 subpass=%14 cycles=%15 requested_low=%16 requested_subpass=%17 requested_cycles=%18 requested_depth=%19 supplemental=%20 max_ms=%21 constrained=%22 hashprep_ms=%23 hashreplay=%24 candthin=%25 thread=0x%26 freqpart=%27 pressure_limited=%28 ap_storico=%29 ap_voci=%30 ap_ciclo=%31 ap_msg=%32 ap_msgmem=%33 ap_msgok=%34")
                 .arg (request.serial)
                 .arg (waitMs)
                 .arg (decodeMs)
                 .arg (totalMs)
                 .arg (effectiveRequestedThreadLimit)
                 .arg (activeThreads)
                 .arg (request.audio.size ())
                 .arg (nout)
                 .arg (ndepth)
                 .arg (nfa)
                 .arg (nfb)
                 .arg (lft8apon)
                 .arg (effectiveLowThresholds ? 1 : 0)
                 .arg (effectiveSubpass ? 1 : 0)
                 .arg (effectiveCycles)
                 .arg (request.lowThresholds ? 1 : 0)
                 .arg (request.subpass ? 1 : 0)
                 .arg (request.nft8Cycles)
                 .arg (requestedDepth)
                 .arg (supplementalRequested ? 1 : 0)
                 .arg (request.maxDecodeMs)
                 .arg (pressureConstrainedProfile ? 1 : 0)
                 .arg (hashPrepMs)
                 .arg (0)
                 .arg (effectiveCandidateThin)
                 .arg (current_thread_id_hex ())
                 .arg (ftx_ft8_freqpart_bins_used_c ())
                 .arg (pressureLimitedRequest ? 1 : 0)
                 .arg (ftx_ft8_ap_storico_tentativi_c ())
                 .arg (decodium::apstorico::quante ())
                 .arg (decodium::apstorico::ciclo_corrente ())
                 .arg (ftx_ft8_ap_msg_tentativi_c ())
                 .arg (decodium::apstorico::quanti_messaggi ())
                 .arg (ftx_ft8_ap_msg_successi_c ());
    }
  Q_EMIT decodeReady (request.serial, rows);
  Q_EMIT decodedEntriesReady (request.serial, entries);
}

void FT8DecodeWorker::resetDecoderState ()
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  ftx_ft8_stage4_reset_c ();
}

}
}
