// -*- Mode: C++ -*-
#pragma once

#include <algorithm>

#include <QThread>

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
#include <pthread.h>
#elif defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

namespace decodium
{
namespace decode
{

inline int threadLimitWithUiReserveForCores (int requested, int logicalCores)
{
  logicalCores = std::max (1, logicalCores);
  int const workerCap = logicalCores > 1 ? logicalCores - 1 : 1;
  return std::max (1, std::min (requested, workerCap));
}

inline int threadLimitWithUiReserve (int requested)
{
  return threadLimitWithUiReserveForCores (requested,
                                           QThread::idealThreadCount ());
}

inline void applyCurrentDecodeWorkerPriority ()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
  pthread_set_qos_class_self_np (QOS_CLASS_UTILITY, 0);
#elif defined(Q_OS_WIN)
  SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_BELOW_NORMAL);
#elif defined(Q_OS_LINUX)
  // Linux nice values are per thread when addressed by TID.
  setpriority (PRIO_PROCESS, static_cast<id_t> (syscall (SYS_gettid)), 5);
#endif
}

inline void applyOpenMpDecodeWorkerPriority (int threads)
{
#ifdef _OPENMP
#pragma omp parallel num_threads(threads)
  {
    applyCurrentDecodeWorkerPriority ();
  }
#else
  (void) threads;
#endif
}

} // namespace decode
} // namespace decodium
