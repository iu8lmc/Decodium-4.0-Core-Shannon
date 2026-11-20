#include "FtDecodeThreadBudget.hpp"
#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QThread>
#include <QtGlobal>
#ifdef _OPENMP
#include <omp.h>
#endif
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int decodiumBridgeFtAutoUiReserveForCores(int);
namespace {
constexpr int kMaxFtDecodeThreads {decodium::decode::kMaxFtDecodeThreads};
void applyFtOpenMpThreadLimit(int threads)
{
#ifdef _OPENMP
    int const bounded = qBound(1, threads, kMaxFtDecodeThreads);
    omp_set_dynamic(0);
    omp_set_max_active_levels(1);
    omp_set_num_threads(bounded);
#else
    Q_UNUSED(threads)
#endif
}



#if defined(_OPENMP) && defined(Q_OS_WIN)
QString ftOpenMpAffinityForCores(int cores)
{
    int const boundedCores = qMax(1, cores);
    int const reserve = qBound(0, decodiumBridgeFtAutoUiReserveForCores(boundedCores), qMax(0, boundedCores - 1));
    int const firstFtCore = reserve;
    int const lastFtCore = boundedCores - 1;
    if (firstFtCore > lastFtCore) {
        return QString();
    }
    if (firstFtCore == lastFtCore) {
        return QString::number(firstFtCore);
    }
    return QStringLiteral("%1-%2").arg(firstFtCore).arg(lastFtCore);
}
#endif

void applyFtOpenMpSchedulerProfile()
{
#ifdef _OPENMP
#ifdef Q_OS_WIN
    static bool applied = false;
    if (applied) {
        return;
    }
    applied = true;

    auto setDefaultEnv = [](char const* name, char const* value) {
        if (!qEnvironmentVariableIsSet(name)) {
            qputenv(name, QByteArray(value));
            return true;
        }
        return false;
    };

    bool const waitDefault = setDefaultEnv("OMP_WAIT_POLICY", "PASSIVE");
    bool const spinDefault = setDefaultEnv("GOMP_SPINCOUNT", "0");
    bool const dynamicDefault = setDefaultEnv("OMP_DYNAMIC", "FALSE");
    bool affinityDefault = false;
    if (qEnvironmentVariableIsSet("DECODIUM_ENABLE_FT_OPENMP_AFFINITY")
        && !qEnvironmentVariableIsSet("GOMP_CPU_AFFINITY")) {
        QString const affinity = ftOpenMpAffinityForCores(QThread::idealThreadCount());
        if (!affinity.isEmpty()) {
            qputenv("GOMP_CPU_AFFINITY", affinity.toLatin1());
            affinityDefault = true;
        }
    }

    qInfo().noquote()
        << "FT OpenMP scheduler profile:"
        << "OMP_WAIT_POLICY=" << qEnvironmentVariable("OMP_WAIT_POLICY")
        << "GOMP_SPINCOUNT=" << qEnvironmentVariable("GOMP_SPINCOUNT")
        << "OMP_DYNAMIC=" << qEnvironmentVariable("OMP_DYNAMIC")
        << "GOMP_CPU_AFFINITY=" << qEnvironmentVariable("GOMP_CPU_AFFINITY")
        << "defaults=" << QStringLiteral("%1/%2/%3/%4")
                              .arg(waitDefault ? 1 : 0)
                              .arg(spinDefault ? 1 : 0)
                              .arg(dynamicDefault ? 1 : 0)
                              .arg(affinityDefault ? 1 : 0);
#endif
#endif
}

#ifdef Q_OS_WIN
void applyWindowsInteractiveSchedulingProfile()
{
    static bool applied = false;
    if (applied) {
        return;
    }
    applied = true;

    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_WINDOWS_INTERACTIVE_SCHEDULING")) {
        qInfo().noquote() << "Windows interactive scheduling profile disabled by environment";
        return;
    }

    HANDLE const process = GetCurrentProcess();
    DWORD const previousClass = GetPriorityClass(process);
    DWORD const targetClass = ABOVE_NORMAL_PRIORITY_CLASS;
    if (previousClass == 0) {
        qWarning().noquote()
            << "Windows interactive scheduling profile: GetPriorityClass failed"
            << "error=" << static_cast<qulonglong>(GetLastError());
    } else if (previousClass == IDLE_PRIORITY_CLASS
               || previousClass == BELOW_NORMAL_PRIORITY_CLASS
               || previousClass == NORMAL_PRIORITY_CLASS) {
        if (SetPriorityClass(process, targetClass)) {
            qInfo().noquote()
                << "Windows interactive scheduling profile: process priority"
                << static_cast<qulonglong>(previousClass)
                << "->"
                << static_cast<qulonglong>(targetClass);
        } else {
            qWarning().noquote()
                << "Windows interactive scheduling profile: SetPriorityClass failed"
                << "error=" << static_cast<qulonglong>(GetLastError());
        }
    } else {
        qInfo().noquote()
            << "Windows interactive scheduling profile: keeping process priority"
            << static_cast<qulonglong>(previousClass);
    }

    HANDLE const thread = GetCurrentThread();
    int const previousThreadPriority = GetThreadPriority(thread);
    if (previousThreadPriority == THREAD_PRIORITY_ERROR_RETURN && GetLastError() != ERROR_SUCCESS) {
        qWarning().noquote()
            << "Windows interactive scheduling profile: GetThreadPriority failed"
            << "error=" << static_cast<qulonglong>(GetLastError());
        return;
    }
    if (previousThreadPriority < THREAD_PRIORITY_ABOVE_NORMAL) {
        if (SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL)) {
            qInfo().noquote()
                << "Windows interactive scheduling profile: main thread priority"
                << previousThreadPriority
                << "->"
                << THREAD_PRIORITY_ABOVE_NORMAL;
        } else {
            qWarning().noquote()
                << "Windows interactive scheduling profile: SetThreadPriority failed"
                << "error=" << static_cast<qulonglong>(GetLastError());
        }
    }
}

#endif

}

void decodiumBridgeApplyFtOpenMpThreadLimit(int count) { applyFtOpenMpThreadLimit(count); }
void decodiumBridgeApplyFtOpenMpSchedulerProfile() { applyFtOpenMpSchedulerProfile(); }
#ifdef Q_OS_WIN
void decodiumBridgeApplyWindowsInteractiveSchedulingProfile() { applyWindowsInteractiveSchedulingProfile(); }
#endif
