// -*- Mode: C++ -*-
#include "Detector/FT4DecodeWorker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>

#include "Logger.hpp"
#include "commons.h"
#include "Detector/DecodeMetricLogging.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#ifdef _OPENMP
#include <omp.h>
#endif
extern "C"
{
  void legacy_pack77_save_hash_call_c (char const c13[13], int* n10, int* n12, int* n22);
  void ftx_ft8_stage4_seed_hash_call_c (char const* call);
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
  std::atomic_bool g_hashSeedShutdownRequested {false};

  constexpr int kFt4SampleCount {72576};
  constexpr int kFt4MaxLines {100};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};
  constexpr int kFt4StableDspStage {4};
  constexpr qint64 kFt4HashSeedTailBytes {8 * 1024 * 1024};
  constexpr qint64 kFt4HashSeedRefreshTailBytes {512 * 1024};
  constexpr int kFt4HashSeedMaxCalls {4096};
  constexpr qint64 kFt4HashSeedInitialSoftBudgetMs {750};
  constexpr qint64 kFt4HashSeedRefreshSoftBudgetMs {160};
  constexpr qint64 kFt4HashSeedRefreshIntervalMs {60000};
  constexpr int kFt4HashSeedRefreshMaxFiles {6};
  constexpr int kFt4HashSeedPriorityMonths {2};
  constexpr int kFt4HashSeedCacheVersion {1};
  constexpr qint64 kFt4HashSeedCacheMaxBytes {1024 * 1024};
  constexpr int kFt4HashSeedApplyBatchCalls {64};
  constexpr qint64 kFt4HashSeedApplyBudgetMs {25};
  char constexpr kFt4DspStageEnv[] {"DECODIUM_FT4_CPP_DSP_STAGE"};
  [[maybe_unused]] constexpr int kMaxDecodeThreads {24};

  void apply_decode_thread_limit (int threads)
  {
#ifdef _OPENMP
    omp_set_dynamic (0);
    omp_set_num_threads (std::max (1, std::min (threads, kMaxDecodeThreads)));
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
      result.reserve (std::min (static_cast<int> (calls.size ()), kFt4HashSeedMaxCalls));
      for (auto it = calls.rbegin (); it != calls.rend ()
           && result.size () < kFt4HashSeedMaxCalls; ++it)
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
    while (static_cast<int> (accumulator.calls.size ()) > kFt4HashSeedMaxCalls * 4)
      {
        accumulator.calls.pop_front ();
      }
  }

  bool collect_hash_seed_calls_from_tail (QString const& path,
                                          HashSeedCallAccumulator& accumulator,
                                          qint64 tailBytes = kFt4HashSeedTailBytes)
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

  QStringList hash_seed_monthly_all_files ()
  {
    QStringList names;
    QDate const currentMonth = QDate::currentDate ();
    for (int i = kFt4HashSeedPriorityMonths - 1; i >= 0; --i)
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
      seen.insert (clean);
      paths.append (clean);
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
    return paths;
  }

  struct HashSeedCacheData
  {
    QStringList calls;
    QStringList sourcePaths;
    int staleSources {0};
    bool valid {false};
  };

  std::mutex& hash_seed_cache_file_mutex ()
  {
    static std::mutex mutex;
    return mutex;
  }

  QString hash_seed_cache_path ()
  {
    QString root = QStandardPaths::writableLocation (QStandardPaths::CacheLocation);
    if (root.isEmpty ())
      {
        root = QStandardPaths::writableLocation (QStandardPaths::TempLocation);
      }
    QDir dir {root};
    if (!dir.exists () && !dir.mkpath (QStringLiteral (".")))
      {
        return QString {};
      }
    return dir.filePath (QStringLiteral ("ft4_hash_seed_cache_v1.json"));
  }

  HashSeedCacheData load_hash_seed_cache_unlocked ()
  {
    HashSeedCacheData cache;
    QString const path = hash_seed_cache_path ();
    QFile file {path};
    if (path.isEmpty () || !file.open (QIODevice::ReadOnly)
        || file.size () <= 0 || file.size () > kFt4HashSeedCacheMaxBytes)
      {
        return cache;
      }

    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson (file.readAll (), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject ())
      {
        return cache;
      }
    QJsonObject const root = document.object ();
    if (root.value (QStringLiteral ("version")).toInt () != kFt4HashSeedCacheVersion)
      {
        return cache;
      }

    QSet<QString> seenCalls;
    for (QJsonValue const& value : root.value (QStringLiteral ("calls")).toArray ())
      {
        QString const call = value.toString ().trimmed ().toUpper ();
        if (!plausible_hash_seed_token (call) || seenCalls.contains (call))
          {
            continue;
          }
        seenCalls.insert (call);
        cache.calls.append (call);
        if (cache.calls.size () >= kFt4HashSeedMaxCalls)
          {
            break;
          }
      }
    if (cache.calls.isEmpty ())
      {
        return cache;
      }

    QSet<QString> seenSources;
    for (QJsonValue const& value : root.value (QStringLiteral ("sources")).toArray ())
      {
        QJsonObject const source = value.toObject ();
        QString const sourcePath = QDir::cleanPath (
            source.value (QStringLiteral ("path")).toString ());
        if (sourcePath.isEmpty () || seenSources.contains (sourcePath))
          {
            continue;
          }
        seenSources.insert (sourcePath);
        cache.sourcePaths.append (sourcePath);

        bool sizeOk = false;
        bool mtimeOk = false;
        qint64 const cachedSize = source.value (QStringLiteral ("size")).toString ()
                                      .toLongLong (&sizeOk);
        qint64 const cachedMtime = source.value (QStringLiteral ("mtimeMs")).toString ()
                                       .toLongLong (&mtimeOk);
        QFileInfo const info {sourcePath};
        if (!info.exists () || !info.isFile () || !sizeOk || !mtimeOk
            || info.size () != cachedSize
            || info.lastModified ().toMSecsSinceEpoch () != cachedMtime)
          {
            ++cache.staleSources;
          }
      }
    cache.valid = true;
    return cache;
  }

  HashSeedCacheData load_hash_seed_cache ()
  {
    std::lock_guard<std::mutex> lock {hash_seed_cache_file_mutex ()};
    return load_hash_seed_cache_unlocked ();
  }

  bool write_hash_seed_cache_unlocked (QStringList const& calls,
                                       QStringList const& sourcePaths)
  {
    QString const path = hash_seed_cache_path ();
    if (path.isEmpty () || calls.isEmpty ())
      {
        return false;
      }

    QJsonArray callArray;
    for (QString const& call : calls)
      {
        callArray.append (call);
      }

    QJsonArray sourceArray;
    QSet<QString> seenSources;
    for (QString const& sourcePath : sourcePaths)
      {
        QString const clean = QDir::cleanPath (sourcePath);
        if (clean.isEmpty () || seenSources.contains (clean))
          {
            continue;
          }
        seenSources.insert (clean);
        QFileInfo const info {clean};
        QJsonObject source;
        source.insert (QStringLiteral ("path"), clean);
        source.insert (QStringLiteral ("size"), QString::number (info.exists () ? info.size () : -1));
        source.insert (QStringLiteral ("mtimeMs"),
                       QString::number (info.exists ()
                                          ? info.lastModified ().toMSecsSinceEpoch ()
                                          : -1));
        sourceArray.append (source);
      }

    QJsonObject root;
    root.insert (QStringLiteral ("version"), kFt4HashSeedCacheVersion);
    root.insert (QStringLiteral ("generatedMs"),
                 QString::number (QDateTime::currentMSecsSinceEpoch ()));
    root.insert (QStringLiteral ("calls"), callArray);
    root.insert (QStringLiteral ("sources"), sourceArray);

    QSaveFile file {path};
    if (!file.open (QIODevice::WriteOnly))
      {
        return false;
      }
    QByteArray const bytes = QJsonDocument {root}.toJson (QJsonDocument::Compact);
    return file.write (bytes) == bytes.size () && file.commit ();
  }

  void merge_into_hash_seed_cache (QStringList const& calls,
                                   QStringList const& sourcePaths)
  {
    if (calls.isEmpty ())
      {
        return;
      }
    std::lock_guard<std::mutex> lock {hash_seed_cache_file_mutex ()};
    HashSeedCacheData const existing = load_hash_seed_cache_unlocked ();
    HashSeedCallAccumulator accumulator;
    for (QString const& call : existing.calls)
      {
        remember_hash_seed_call (accumulator, call);
      }
    for (QString const& call : calls)
      {
        remember_hash_seed_call (accumulator, call);
      }
    QStringList mergedSources = existing.sourcePaths;
    for (QString const& sourcePath : sourcePaths)
      {
        if (!mergedSources.contains (sourcePath))
          {
            mergedSources.append (sourcePath);
          }
      }
    write_hash_seed_cache_unlocked (accumulator.to_list (), mergedSources);
  }

  int hash_seed_refresh_priority (QString const& path)
  {
    QFileInfo const info {path};
    QString const cleanPath = QDir::cleanPath (path).toUpper ();
    QString const name = info.fileName ().toUpper ();
    int priority = 0;

    if (name == QStringLiteral ("CALL3.TXT"))
      {
        priority += 10;
      }
    if (name == QStringLiteral ("ALL.TXT") || name.endsWith (QStringLiteral ("_ALL.TXT")))
      {
        priority += 20;
      }

    if (cleanPath.contains (QStringLiteral ("/IU8LMC/DECODIUM/EMBEDDED-FT2/")))
      {
        priority += 20;
      }
    else if (cleanPath.contains (QStringLiteral ("/IU8LMC/DECODIUM/")))
      {
        priority += 10;
      }
    if (cleanPath.contains (QStringLiteral ("/WSJT-X/")))
      {
        priority += 30;
      }
    if (cleanPath.contains (QStringLiteral ("/JTDX/")))
      {
        priority += 40;
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

  QStringList local_hash_seed_refresh_paths ()
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
        candidates.append ({path, hash_seed_refresh_priority (path), info.lastModified ()});
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

    if (candidates.size () > kFt4HashSeedRefreshMaxFiles)
      {
        candidates.erase (candidates.begin () + kFt4HashSeedRefreshMaxFiles, candidates.end ());
      }

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

  struct HashSeedSnapshot
  {
    QStringList calls;
    QStringList sources;
    QStringList sourcePaths;
    int filesRead {0};
    int staleSources {0};
    qint64 elapsedMs {0};
    bool cacheHit {false};
  };

  HashSeedSnapshot collect_initial_hash_seed_snapshot ()
  {
    QElapsedTimer timer;
    timer.start ();
    HashSeedSnapshot snapshot;
    HashSeedCacheData const cache = load_hash_seed_cache ();
    if (cache.valid)
      {
        snapshot.calls = cache.calls;
        snapshot.sourcePaths = cache.sourcePaths;
        snapshot.staleSources = cache.staleSources;
        snapshot.cacheHit = true;
        snapshot.sources.append (QStringLiteral ("cache/")
                                 + QFileInfo {hash_seed_cache_path ()}.fileName ());
        snapshot.elapsedMs = timer.elapsed ();
        return snapshot;
      }

    HashSeedCallAccumulator accumulator;
    for (QString const& path : local_hash_seed_paths ())
      {
        if (hash_seed_shutdown_requested ())
          {
            break;
          }
        if (collect_hash_seed_calls_from_tail (path, accumulator))
          {
            ++snapshot.filesRead;
            snapshot.sources.append (hash_seed_source_label (path));
            snapshot.sourcePaths.append (path);
          }
        if (timer.elapsed () >= kFt4HashSeedInitialSoftBudgetMs)
          {
            break;
          }
      }
    snapshot.calls = accumulator.to_list ();
    merge_into_hash_seed_cache (snapshot.calls, snapshot.sourcePaths);
    snapshot.elapsedMs = timer.elapsed ();
    return snapshot;
  }

  HashSeedSnapshot collect_hash_seed_refresh_snapshot ()
  {
    QElapsedTimer timer;
    timer.start ();
    HashSeedSnapshot snapshot;
    HashSeedCallAccumulator accumulator;
    for (QString const& path : local_hash_seed_refresh_paths ())
      {
        if (hash_seed_shutdown_requested ())
          {
            break;
          }
        if (collect_hash_seed_calls_from_tail (path, accumulator,
                                               kFt4HashSeedRefreshTailBytes))
          {
            ++snapshot.filesRead;
            snapshot.sources.append (hash_seed_source_label (path));
            snapshot.sourcePaths.append (path);
          }
        if (snapshot.filesRead >= kFt4HashSeedRefreshMaxFiles
            || timer.elapsed () >= kFt4HashSeedRefreshSoftBudgetMs)
          {
            break;
          }
      }
    snapshot.calls = accumulator.to_list ();
    merge_into_hash_seed_cache (snapshot.calls, snapshot.sourcePaths);
    snapshot.elapsedMs = timer.elapsed ();
    return snapshot;
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
    qint64 lastRefreshStartMs {0};
    bool refreshRequested {false};
    bool refreshReady {false};
    HashSeedSnapshot refreshCompleted;
    std::deque<QString> pendingCalls;
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
      QThread::currentThread ()->setObjectName (QStringLiteral ("FT4HashSeedWorker"));
      QThread::currentThread ()->setPriority (QThread::LowPriority);

      for (;;)
        {
          bool collectInitial = false;
          bool collectRefresh = false;
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
              HashSeedSnapshot snapshot = collect_hash_seed_refresh_snapshot ();
              std::lock_guard<std::mutex> lock {state.mutex};
              state.refreshCompleted = std::move (snapshot);
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

  void seed_hash_calls (QStringList const& calls)
  {
    for (QString const& call : calls)
      {
        QByteArray const field = call.toLatin1 ().leftJustified (13, ' ', true);
        legacy_pack77_save_hash_call_c (field.constData (), nullptr, nullptr, nullptr);
        ftx_ft8_stage4_seed_hash_call_c (field.constData ());
      }
  }

  void start_ft4_pack77_hash_seed_collection_once ()
  {
    if (hash_seed_shutdown_requested ())
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
    state.lastRefreshStartMs = QDateTime::currentMSecsSinceEpoch ();
    state.workAvailable.notify_one ();
  }

  void apply_ready_ft4_pack77_hash_seed_collection ()
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    HashSeedSnapshot snapshot;
    bool haveSnapshot = false;
    {
      HashSeedAsyncState& state = hash_seed_async_state ();
      std::lock_guard<std::mutex> lock {state.mutex};
      if (!state.initialApplied && state.initialReady)
        {
          snapshot = std::move (state.initialCompleted);
          state.initialReady = false;
          state.initialApplied = true;
          for (QString const& call : snapshot.calls)
            {
              state.pendingCalls.push_back (call);
            }
          haveSnapshot = true;
        }
    }
    if (!haveSnapshot)
      {
        return;
      }

    if (!snapshot.calls.isEmpty ())
      {
        LOG_INFO ("FT4 pack77 hash seed initialized: calls=" << snapshot.calls.size ()
                  << " files=" << snapshot.filesRead
                  << " elapsed_ms=" << snapshot.elapsedMs
                  << " cache=" << (snapshot.cacheHit ? "hit" : "miss")
                  << " stale_sources=" << snapshot.staleSources
                  << " sources="
                  << snapshot.sources.join (QStringLiteral (",")).toStdString ());
      }
  }

  void start_ft4_hash_context_refresh_from_recent_logs ()
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch ();
    HashSeedAsyncState& state = hash_seed_async_state ();
    std::lock_guard<std::mutex> lock {state.mutex};
    if (!state.initialApplied || !state.pendingCalls.empty ()
        || state.refreshRequested || state.refreshReady)
      {
        return;
      }
    if (state.lastRefreshStartMs > 0
        && nowMs - state.lastRefreshStartMs < kFt4HashSeedRefreshIntervalMs)
      {
        return;
      }
    start_hash_seed_worker_locked (state);
    state.lastRefreshStartMs = nowMs;
    state.refreshRequested = true;
    state.workAvailable.notify_one ();
  }

  void apply_ready_ft4_hash_context_refresh ()
  {
    if (hash_seed_shutdown_requested ())
      {
        return;
      }
    HashSeedSnapshot snapshot;
    bool haveSnapshot = false;
    {
      HashSeedAsyncState& state = hash_seed_async_state ();
      std::lock_guard<std::mutex> lock {state.mutex};
      if (state.refreshReady)
        {
          snapshot = std::move (state.refreshCompleted);
          state.refreshReady = false;
          for (QString const& call : snapshot.calls)
            {
              state.pendingCalls.push_back (call);
            }
          haveSnapshot = true;
        }
    }
    if (!haveSnapshot)
      {
        return;
      }

  }

  void apply_pending_ft4_hash_seed_calls ()
  {
    QElapsedTimer timer;
    timer.start ();
    int applied = 0;
    while (applied < kFt4HashSeedApplyBatchCalls
           && timer.elapsed () < kFt4HashSeedApplyBudgetMs)
      {
        QString call;
        {
          HashSeedAsyncState& state = hash_seed_async_state ();
          std::lock_guard<std::mutex> lock {state.mutex};
          if (state.pendingCalls.empty ())
            {
              break;
            }
          // Seed the newest calls first; they are the useful ones for live traffic.
          call = std::move (state.pendingCalls.back ());
          state.pendingCalls.pop_back ();
        }
        QByteArray const field = call.toLatin1 ().leftJustified (13, ' ', true);
        legacy_pack77_save_hash_call_c (field.constData (), nullptr, nullptr, nullptr);
        ftx_ft8_stage4_seed_hash_call_c (field.constData ());
        ++applied;
      }

    if (timer.elapsed () >= 50)
      {
        HashSeedAsyncState& state = hash_seed_async_state ();
        std::lock_guard<std::mutex> lock {state.mutex};
        qInfo().noquote ()
            << "[HASHMETRIC] ft4_seed_batch"
            << "applied=" << applied
            << "elapsed_ms=" << timer.elapsed ()
            << "remaining=" << state.pendingCalls.size ();
      }
  }

  void seed_ft4_pack77_hashes_from_rows (QStringList const& rows)
  {
    HashSeedCallAccumulator accumulator;
    static QRegularExpression const tokenRx {
      QStringLiteral (R"((?:<[A-Z0-9/]{3,13}>|[A-Z0-9/]{3,13}))")
    };
    for (QString const& row : rows)
      {
        auto it = tokenRx.globalMatch (row.toUpper ());
        while (it.hasNext ())
          {
            remember_hash_seed_call (accumulator, it.next ().captured (0));
          }
      }
    seed_hash_calls (accumulator.to_list ());
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

void shutdownHashSeedWorker ()
{
  shutdown_hash_seed_worker ();
}

FT4DecodeWorker::FT4DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FT4DecodeWorker::markLatestDecodeSerial (quint64 serial)
{
  m_latestDecodeSerial.store (serial, std::memory_order_relaxed);
}

void FT4DecodeWorker::beginShutdown ()
{
  m_shuttingDown.store (true, std::memory_order_relaxed);
  m_latestDecodeSerial.store (~quint64(0), std::memory_order_relaxed);
}

void FT4DecodeWorker::decode (DecodeRequest const& request)
{
  QElapsedTimer totalTimer;
  totalTimer.start ();
  quint64 latestSerial = m_latestDecodeSerial.load (std::memory_order_relaxed);
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || latestSerial == ~quint64(0))
    {
      return;
    }
  apply_decode_thread_limit (request.threadCount);
  int const activeThreads = active_decode_thread_limit ();
  start_ft4_pack77_hash_seed_collection_once ();
  start_ft4_hash_context_refresh_from_recent_logs ();
  log_ft4_dsp_rollout_once ();
  QElapsedTimer waitTimer;
  waitTimer.start ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  qint64 const waitMs = waitTimer.elapsed ();

  latestSerial = m_latestDecodeSerial.load (std::memory_order_relaxed);
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || latestSerial == ~quint64(0))
    {
      return;
    }
  apply_ready_ft4_pack77_hash_seed_collection ();
  apply_ready_ft4_hash_context_refresh ();
  apply_pending_ft4_hash_seed_calls ();

  short int iwave[kFt4SampleCount] {};
  int const copyCount = std::min (static_cast<int>(request.audio.size ()), static_cast<int>(kFt4SampleCount));
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
  int lapcqonly = request.lapcqonly ? 1 : 0;
  int ncontest = qBound (0, request.ncontest, 16);
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);

  QElapsedTimer decodeTimer;
  decodeTimer.start ();
  ftx_ft4_decode_c (iwave, &nqsoprogress, &nfqso, &nfa, &nfb,
                    &ndepth, &lapcqonly, &ncontest, mycall.data (), hiscall.data (),
                    &syncs[0], &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                    &bits77[0], &decodeds[0], &nout,
                    static_cast<fortran_charlen_t> (12),
                    static_cast<fortran_charlen_t> (12),
                    static_cast<fortran_charlen_t> (kFt4MaxLines * kDecodedChars));
  qint64 const decodeMs = decodeTimer.elapsed ();

  latestSerial = m_latestDecodeSerial.load (std::memory_order_relaxed);
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || latestSerial == ~quint64(0))
    {
      return;
    }

  LOG_DEBUG ("FT4 decode completed: stage=" << ft4_dsp_rollout_stage ()
             << " nout=" << nout);
  QString const utcPrefix = format_decode_utc (request.nutc);
  qint64 const totalMs = totalTimer.elapsed ();
  static std::atomic<qint64> lastMetricLogMs {0};
  if (decodium::logging::should_log_decode_metric (waitMs, decodeMs, totalMs, lastMetricLogMs))
    {
      qInfo().noquote()
          << QStringLiteral ("[DECODEMETRIC] mode=FT4 serial=%1 wait_ms=%2 decode_ms=%3 total_ms=%4 threads_req=%5 threads_active=%6 audio=%7 nout=%8 depth=%9 nfa=%10 nfb=%11 thread=0x%12")
                 .arg (request.serial)
                 .arg (waitMs)
                 .arg (decodeMs)
                 .arg (totalMs)
                 .arg (request.threadCount)
                 .arg (activeThreads)
                 .arg (request.audio.size ())
                 .arg (nout)
                 .arg (ndepth)
                 .arg (nfa)
                 .arg (nfb)
                 .arg (current_thread_id_hex ());
    }
  QStringList rows = build_rows (utcPrefix, nout, snrs, dts, freqs, naps, quals,
                                 decodeds);
  seed_ft4_pack77_hashes_from_rows (rows);
  Q_EMIT decodeReady (request.serial, rows);
}

}
}
