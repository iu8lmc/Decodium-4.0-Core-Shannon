// -*- Mode: C++ -*-
#ifndef DECODIUM_DETECTOR_DECODE_METRIC_LOGGING_HPP__
#define DECODIUM_DETECTOR_DECODE_METRIC_LOGGING_HPP__

#include <atomic>

#include <QByteArray>
#include <QDateTime>
#include <QtGlobal>

namespace decodium::logging
{
  inline bool decode_metric_verbose_enabled ()
  {
    static bool const enabled = []
    {
      if (qEnvironmentVariableIsSet ("DECODIUM_DECODEMETRIC_VERBOSE"))
        {
          return true;
        }
      QByteArray const value = qgetenv ("DECODIUM_DECODEMETRIC").trimmed ().toLower ();
      return value == "1" || value == "true" || value == "yes"
          || value == "full" || value == "verbose";
    } ();
    return enabled;
  }

  inline qint64 decode_metric_interval_ms ()
  {
    static qint64 const interval = []
    {
      bool ok = false;
      int const envValue = qEnvironmentVariableIntValue ("DECODIUM_DECODEMETRIC_INTERVAL_MS", &ok);
      if (!ok)
        {
          return qint64 {60000};
        }
      if (envValue <= 0)
        {
          return qint64 {0};
        }
      return qint64 {qMax (5000, envValue)};
    } ();
    return interval;
  }

  inline bool should_log_decode_metric (qint64 waitMs,
                                        qint64 decodeMs,
                                        qint64 totalMs,
                                        std::atomic<qint64>& lastLogMs,
                                        qint64 slowWaitMs = 500,
                                        qint64 slowDecodeMs = 3000,
                                        qint64 slowTotalMs = 3500)
  {
    if (decode_metric_verbose_enabled ())
      {
        return true;
      }
    if (waitMs >= slowWaitMs || decodeMs >= slowDecodeMs || totalMs >= slowTotalMs)
      {
        return true;
      }

    qint64 const intervalMs = decode_metric_interval_ms ();
    if (intervalMs <= 0)
      {
        return false;
      }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch ();
    qint64 previousMs = lastLogMs.load (std::memory_order_relaxed);
    while (previousMs == 0 || nowMs - previousMs >= intervalMs)
      {
        if (lastLogMs.compare_exchange_weak (previousMs, nowMs, std::memory_order_relaxed))
          {
            return true;
          }
      }
    return false;
  }
}

#endif
