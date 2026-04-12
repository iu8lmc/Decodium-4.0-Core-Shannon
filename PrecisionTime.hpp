#ifndef PRECISION_TIME_HPP
#define PRECISION_TIME_HPP

#include <QtGlobal>
#include <QDateTime>
#include <atomic>
#include <cmath>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// windows.h defines MessageBox as a macro, which conflicts with WSJT-X MessageBox class
#ifdef MessageBox
#undef MessageBox
#endif
#endif

// Returns current UTC time in milliseconds since epoch with maximum precision.
// On Windows uses GetSystemTimePreciseAsFileTime() for microsecond resolution.
// On other platforms falls back to QDateTime (millisecond resolution).
inline qint64 preciseCurrentMSecsSinceEpoch()
{
#ifdef Q_OS_WIN
    // GetSystemTimePreciseAsFileTime provides ~1μs resolution on Windows 8+
    // vs ~15.6ms resolution from standard GetSystemTimeAsFileTime
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    // FILETIME is 100-nanosecond intervals since 1601-01-01
    // Convert to milliseconds since Unix epoch (1970-01-01)
    // Difference between 1601 and 1970 in 100-ns units: 116444736000000000LL
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    return static_cast<qint64>((ull.QuadPart - 116444736000000000LL) / 10000);
#else
    return QDateTime::currentMSecsSinceEpoch();
#endif
}

inline std::atomic<qint64>& globalNtpOffsetMsStorage()
{
  static std::atomic<qint64> offset_ms {0};
  return offset_ms;
}

inline void setGlobalNtpOffsetMs(double offsetMs)
{
  globalNtpOffsetMsStorage().store(static_cast<qint64>(std::llround(offsetMs)), std::memory_order_relaxed);
}

inline qint64 globalNtpOffsetMs()
{
  return globalNtpOffsetMsStorage().load(std::memory_order_relaxed);
}

inline qint64 ntpCorrectedCurrentMSecsSinceEpoch()
{
  return preciseCurrentMSecsSinceEpoch() + globalNtpOffsetMs();
}

// Returns fractional microseconds part (0-999) for sub-millisecond precision
inline int preciseCurrentMicrosecondFraction()
{
#ifdef Q_OS_WIN
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    // Get microsecond fraction (100ns units mod 10000 gives sub-ms, then /10 for μs)
    return static_cast<int>((ull.QuadPart / 10) % 1000);
#else
    return 0;
#endif
}

#endif // PRECISION_TIME_HPP
