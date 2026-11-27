#include "DecodiumBridge.h"
#include "LinuxDrmGpuUsage.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QVector>
#include <cmath>
#include <cwchar>
#include <string>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#if defined(_MSC_VER)
#pragma comment(lib, "pdh.lib")
#endif
#else
#include <sys/resource.h>
#include <sys/time.h>
#endif
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/task_info.h>
#endif

void decodiumBridgeApplyFtOpenMpThreadLimit(int);
#define applyFtOpenMpThreadLimit decodiumBridgeApplyFtOpenMpThreadLimit

#ifdef Q_OS_WIN
quint64 decodiumFileTimeToUsec(FILETIME const& value)
{
    ULARGE_INTEGER ticks;
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    return static_cast<quint64>(ticks.QuadPart / 10ULL);
}

class WindowsProcessGpuUsageMonitor
{
public:
    ~WindowsProcessGpuUsageMonitor()
    {
        closeQuery();
    }

    double sample(bool* ok)
    {
        if (ok)
            *ok = false;

        if (m_counters.isEmpty() && !rebuildQuery())
            return -1.0;

        PDH_STATUS status = PdhCollectQueryData(m_query);
        if (status != ERROR_SUCCESS) {
            closeQuery();
            return -1.0;
        }

        double totalPercent = 0.0;
        int validCounters = 0;
        for (PDH_HCOUNTER counter : std::as_const(m_counters)) {
            PDH_FMT_COUNTERVALUE value {};
            status = PdhGetFormattedCounterValue(counter,
                                                 PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
                                                 nullptr,
                                                 &value);
            if (status == ERROR_SUCCESS
                && value.CStatus == ERROR_SUCCESS
                && std::isfinite(value.doubleValue)) {
                totalPercent += qMax(0.0, value.doubleValue);
                ++validCounters;
            }
        }

        if (validCounters <= 0) {
            closeQuery();
            return -1.0;
        }

        // GPU Engine instances are created lazily by D3D/RHI. Re-expand while
        // idle so Windows catches engines that appear after startup.
        if (totalPercent <= 0.0001) {
            ++m_zeroSamples;
            if (m_zeroSamples >= 4) {
                m_zeroSamples = 0;
                rebuildQuery();
            }
        } else {
            m_zeroSamples = 0;
        }

        if (ok)
            *ok = true;
        return qBound(0.0, totalPercent / 100.0, 1.0);
    }

private:
    void closeQuery()
    {
        if (m_query) {
            PdhCloseQuery(m_query);
            m_query = nullptr;
        }
        m_counters.clear();
        m_zeroSamples = 0;
    }

    bool rebuildQuery()
    {
        closeQuery();

        if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS)
            return false;

        QString const wildcardPath = QStringLiteral("\\GPU Engine(pid_%1*)\\Utilization Percentage")
            .arg(static_cast<qulonglong>(GetCurrentProcessId()));
        std::wstring const wildcard = wildcardPath.toStdWString();
        DWORD bufferChars = 0;
        PDH_STATUS status = PdhExpandWildCardPathW(nullptr,
                                                   wildcard.c_str(),
                                                   nullptr,
                                                   &bufferChars,
                                                   0);
        if (status != static_cast<PDH_STATUS>(PDH_MORE_DATA) || bufferChars == 0) {
            closeQuery();
            return false;
        }

        std::wstring buffer(bufferChars, L'\0');
        status = PdhExpandWildCardPathW(nullptr,
                                        wildcard.c_str(),
                                        buffer.data(),
                                        &bufferChars,
                                        0);
        if (status != ERROR_SUCCESS) {
            closeQuery();
            return false;
        }

        wchar_t const* path = buffer.c_str();
        while (*path) {
            PDH_HCOUNTER counter = nullptr;
            if (PdhAddEnglishCounterW(m_query, path, 0, &counter) == ERROR_SUCCESS)
                m_counters.append(counter);
            path += std::wcslen(path) + 1;
        }

        if (m_counters.isEmpty()) {
            closeQuery();
            return false;
        }

        PdhCollectQueryData(m_query);
        return true;
    }

    PDH_HQUERY m_query = nullptr;
    QVector<PDH_HCOUNTER> m_counters;
    int m_zeroSamples = 0;
};
#endif

quint64 decodiumCurrentProcessCpuUsec()
{
#ifdef Q_OS_WIN
    FILETIME creation;
    FILETIME exit;
    FILETIME kernel;
    FILETIME user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        return 0;
    }
    return decodiumFileTimeToUsec(kernel) + decodiumFileTimeToUsec(user);
#else
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0;
    }
    auto const timevalToUsec = [](timeval const& tv) -> quint64 {
        return static_cast<quint64>(tv.tv_sec) * 1000000ULL
            + static_cast<quint64>(tv.tv_usec);
    };
    return timevalToUsec(usage.ru_utime) + timevalToUsec(usage.ru_stime);
#endif
}

#ifdef Q_OS_LINUX
bool decodiumCurrentProcessLinuxGpuSamples(
    quint64* gpuTimeNs,
    decodium::gpu_usage::LinuxDrmCycleSample* cycleSample,
    bool* hasEngineTime,
    bool* hasCycleSample,
    QString* pciDevice = nullptr)
{
    QDir fdInfoDir(QStringLiteral("/proc/self/fdinfo"));
    QStringList const entries = fdInfoDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (entries.isEmpty())
        return false;

    QList<decodium::gpu_usage::LinuxDrmFdInfo> descriptors;
    descriptors.reserve(entries.size());
    for (QString const& entry : entries) {
        QFile file(fdInfoDir.absoluteFilePath(entry));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        descriptors.push_back({ entry, QString::fromLatin1(file.readAll()) });
    }

    bool const engineAvailable =
        decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, gpuTimeNs);
    bool const cyclesAvailable =
        decodium::gpu_usage::aggregateLinuxDrmCycleSample(descriptors, cycleSample);
    if (pciDevice)
        *pciDevice = decodium::gpu_usage::primaryLinuxDrmPciDevice(descriptors);
    if (hasEngineTime)
        *hasEngineTime = engineAvailable;
    if (hasCycleSample)
        *hasCycleSample = cyclesAvailable;
    return engineAvailable || cyclesAvailable;
}

struct LinuxDrmDeviceUsageSample
{
    double immediateUsage {-1.0};
};

static bool readLinuxGpuCounter(QString const& path, quint64* value)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    bool ok = false;
    quint64 const parsed = QString::fromLatin1(file.readAll()).trimmed().toULongLong(&ok);
    if (ok && value)
        *value = parsed;
    return ok;
}

// DRM fdinfo is the preferred per-process source. Some drivers instead expose
// a device-wide gpu_busy_percent counter. Do not derive utilization from i915
// RC6 residency: it measures deep-idle time, so its complement is GPU awake
// time rather than GPU busy time and can misleadingly remain at 100%.
static bool decodiumCurrentLinuxGpuDeviceUsage(QString const& pciDevice,
                                               LinuxDrmDeviceUsageSample* sample)
{
    static QRegularExpression const pciDeviceRe(
        QStringLiteral("^[0-9a-fA-F]{4,8}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\\.[0-7]$"));
    if (!sample || !pciDeviceRe.match(pciDevice).hasMatch())
        return false;

    QString const pciRoot = QStringLiteral("/sys/bus/pci/devices/") + pciDevice;
    quint64 busyPercent = 0;
    if (readLinuxGpuCounter(pciRoot + QStringLiteral("/gpu_busy_percent"), &busyPercent)) {
        sample->immediateUsage = qBound(0.0, static_cast<double>(busyPercent) / 100.0, 1.0);
        return true;
    }

    QDir drmDir(pciRoot + QStringLiteral("/drm"));
    QStringList const cards = drmDir.entryList(
        {QStringLiteral("card[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (QString const& card : cards) {
        QString const cardRoot = drmDir.absoluteFilePath(card);
        if (readLinuxGpuCounter(cardRoot + QStringLiteral("/device/gpu_busy_percent"),
                                &busyPercent)) {
            sample->immediateUsage = qBound(
                0.0, static_cast<double>(busyPercent) / 100.0, 1.0);
            return true;
        }
    }
    return false;
}
#endif

bool decodiumCurrentProcessGpuTimeNs(quint64* gpuTimeNs)
{
#ifdef Q_OS_MAC
#if defined(__x86_64__)
    task_power_info_v2_data_t info {};
    mach_msg_type_number_t count = TASK_POWER_INFO_V2_COUNT;
    kern_return_t const result = task_info(mach_task_self(),
                                           TASK_POWER_INFO_V2,
                                           reinterpret_cast<task_info_t>(&info),
                                           &count);
    if (result != KERN_SUCCESS) {
        return false;
    }
    if (gpuTimeNs) {
        *gpuTimeNs = info.gpu_energy.task_gpu_utilisation;
    }
    return true;
#else
    // Apple Silicon currently reports zero through TASK_POWER_INFO_V2 for
    // task_gpu_utilisation. Activity Monitor uses private sysmon counters, so
    // presenting this as a real process-GPU value would be misleading.
    Q_UNUSED(gpuTimeNs);
    return false;
#endif
#elif defined(Q_OS_LINUX)
    decodium::gpu_usage::LinuxDrmCycleSample ignoredCycles;
    bool hasEngineTime = false;
    bool hasCycleSample = false;
    decodiumCurrentProcessLinuxGpuSamples(gpuTimeNs,
                                          &ignoredCycles,
                                          &hasEngineTime,
                                          &hasCycleSample,
                                          nullptr);
    Q_UNUSED(hasCycleSample);
    return hasEngineTime;
#else
    Q_UNUSED(gpuTimeNs);
    return false;
#endif
}

#ifdef Q_OS_WIN
double decodiumCurrentProcessGpuUsage(bool* available)
{
    static WindowsProcessGpuUsageMonitor monitor;
    return monitor.sample(available);
}
#endif


void DecodiumBridge::updateProcessCpuUsage()
{
    quint64 const currentCpuUsec = decodiumCurrentProcessCpuUsec();
    qint64 const wallUsec = m_processCpuSampleClock.isValid()
        ? m_processCpuSampleClock.nsecsElapsed() / 1000
        : 0;

    if (!m_processCpuSampleInitialized || wallUsec <= 0 || currentCpuUsec < m_lastProcessCpuUsec) {
        m_lastProcessCpuUsec = currentCpuUsec;
        m_processCpuSampleClock.restart();
        m_processCpuSampleInitialized = true;
        return;
    }

    quint64 const cpuDeltaUsec = currentCpuUsec - m_lastProcessCpuUsec;
    double usage = static_cast<double>(cpuDeltaUsec)
        / (static_cast<double>(wallUsec) * qMax(1, m_processCpuLogicalCores));
    if (!std::isfinite(usage)) {
        usage = 0.0;
    }
    usage = qBound(0.0, usage, 1.0);

    m_lastProcessCpuUsec = currentCpuUsec;
    m_processCpuSampleClock.restart();

    if ((m_monitoring || m_transmitting || m_tuning) && usage >= 0.45) {
        noteCpuPressure(QStringLiteral("process CPU %1%")
                            .arg(usage * 100.0, 0, 'f', 0),
                        10000,
                        usage >= 0.60);
    } else if (!cpuPressureActive()) {
        applyFtOpenMpThreadLimit(effectiveFtThreadLimit());
    }

    if (std::abs(m_processCpuUsage - usage) < 0.005) {
        return;
    }
    m_processCpuUsage = usage;
    emit processCpuUsageChanged();
}

void DecodiumBridge::updateProcessGpuUsage()
{
    double usage = -1.0;
    QString usageSource = QStringLiteral("unavailable");

#ifdef Q_OS_WIN
    bool available = false;
    usage = decodiumCurrentProcessGpuUsage(&available);
    if (!available || !std::isfinite(usage))
        usage = -1.0;
    else
        usageSource = QStringLiteral("process");
#elif defined(Q_OS_LINUX)
    quint64 currentGpuTimeNs = 0;
    decodium::gpu_usage::LinuxDrmCycleSample cycleSample;
    bool hasEngineTime = false;
    bool hasCycleSample = false;
    QString pciDevice;
    decodiumCurrentProcessLinuxGpuSamples(&currentGpuTimeNs,
                                          &cycleSample,
                                          &hasEngineTime,
                                          &hasCycleSample,
                                          &pciDevice);
    qint64 const wallNs = m_processGpuSampleClock.isValid()
        ? m_processGpuSampleClock.nsecsElapsed()
        : 0;
    double cycleUsage = -1.0;
    bool cycleUsageAvailable = false;
    if (hasCycleSample) {
        bool const resetCycleSample =
            !m_processGpuCycleSampleInitialized
            || cycleSample.busyCycles < m_lastProcessGpuBusyCycles
            || cycleSample.totalCycles <= m_lastProcessGpuTotalCycles;
        if (!resetCycleSample) {
            quint64 const busyDelta = cycleSample.busyCycles - m_lastProcessGpuBusyCycles;
            quint64 const totalDelta = cycleSample.totalCycles - m_lastProcessGpuTotalCycles;
            if (totalDelta > 0) {
                cycleUsage = static_cast<double>(busyDelta) / static_cast<double>(totalDelta);
                cycleUsageAvailable = true;
            }
        }
        m_lastProcessGpuBusyCycles = cycleSample.busyCycles;
        m_lastProcessGpuTotalCycles = cycleSample.totalCycles;
        m_processGpuCycleSampleInitialized = true;
    } else {
        m_processGpuCycleSampleInitialized = false;
    }
    if (hasEngineTime) {
        usageSource = QStringLiteral("drm-process");
        if (!m_processGpuSampleInitialized
            || wallNs <= 0
            || currentGpuTimeNs < m_lastProcessGpuTimeNs) {
            m_lastProcessGpuTimeNs = currentGpuTimeNs;
            m_processGpuZeroSampleCount = 0;
            m_processGpuSampleClock.restart();
            m_processGpuSampleInitialized = true;
            return;
        }

        quint64 const gpuDeltaNs = currentGpuTimeNs - m_lastProcessGpuTimeNs;
        m_processGpuZeroSampleCount = decodium::gpu_usage::nextLinuxDrmUnchangedSampleCount(
            m_lastProcessGpuTimeNs,
            currentGpuTimeNs,
            m_processGpuZeroSampleCount);
        if (decodium::gpu_usage::linuxDrmCounterIsStale(m_processGpuZeroSampleCount)) {
            if (cycleUsageAvailable) {
                usage = cycleUsage;
                usageSource = QStringLiteral("drm-process-cycles");
            } else {
                LinuxDrmDeviceUsageSample deviceSample;
                if (decodiumCurrentLinuxGpuDeviceUsage(pciDevice, &deviceSample)
                    && deviceSample.immediateUsage >= 0.0) {
                    usage = deviceSample.immediateUsage;
                    usageSource = QStringLiteral("drm-device");
                } else {
                    // Do not advertise a permanently frozen counter as a
                    // genuine 0% load. QML can now fall back to measured
                    // render activity.
                    usage = -1.0;
                    usageSource = QStringLiteral("drm-process-stale");
                }
            }
        } else {
            usage = static_cast<double>(gpuDeltaNs) / static_cast<double>(wallNs);
        }
        m_lastProcessGpuTimeNs = currentGpuTimeNs;
        m_processGpuSampleClock.restart();
    } else if (hasCycleSample) {
        m_processGpuSampleInitialized = false;
        m_processGpuZeroSampleCount = 0;
        usageSource = QStringLiteral("drm-process-cycles");
        if (!cycleUsageAvailable) {
            m_processGpuSampleClock.restart();
            return;
        }
        usage = cycleUsage;
        m_processGpuSampleClock.restart();
    } else {
        m_processGpuSampleInitialized = false;
        m_processGpuCycleSampleInitialized = false;
        m_processGpuZeroSampleCount = 0;
        LinuxDrmDeviceUsageSample deviceSample;
        if (decodiumCurrentLinuxGpuDeviceUsage(pciDevice, &deviceSample)
            && deviceSample.immediateUsage >= 0.0) {
            usage = deviceSample.immediateUsage;
            usageSource = QStringLiteral("drm-device");
            m_processGpuSampleClock.restart();
        } else {
            m_processGpuSampleClock.restart();
            usage = -1.0;
        }
    }

    if (usage >= 0.0) {
        usage = std::isfinite(usage) ? qBound(0.0, usage, 1.0) : -1.0;
    }
#elif defined(Q_OS_MAC)
    quint64 currentGpuTimeNs = 0;
    bool const available = decodiumCurrentProcessGpuTimeNs(&currentGpuTimeNs);
    qint64 const wallNs = m_processGpuSampleClock.isValid()
        ? m_processGpuSampleClock.nsecsElapsed()
        : 0;

    bool gpuCounterUnavailable = !available;
    if (available && currentGpuTimeNs == 0) {
        m_processGpuZeroSampleCount = qMin(m_processGpuZeroSampleCount + 1, 1000);
        if (m_processGpuZeroSampleCount >= 4) {
            gpuCounterUnavailable = true;
        }
    } else if (available) {
        m_processGpuZeroSampleCount = 0;
    }

    if (gpuCounterUnavailable) {
        m_processGpuSampleInitialized = false;
        m_processGpuSampleClock.restart();
        usage = -1.0;
    } else if (!m_processGpuSampleInitialized
               || wallNs <= 0
               || currentGpuTimeNs < m_lastProcessGpuTimeNs) {
        m_lastProcessGpuTimeNs = currentGpuTimeNs;
        m_processGpuSampleClock.restart();
        m_processGpuSampleInitialized = true;
        return;
    } else {
        quint64 const gpuDeltaNs = currentGpuTimeNs - m_lastProcessGpuTimeNs;
        usage = static_cast<double>(gpuDeltaNs) / static_cast<double>(wallNs);
        if (!std::isfinite(usage))
            usage = -1.0;
        else
            usage = qBound(0.0, usage, 1.0);
        m_lastProcessGpuTimeNs = currentGpuTimeNs;
        m_processGpuSampleClock.restart();
        usageSource = QStringLiteral("process");
    }
#else
    usage = -1.0;
#endif

    bool const sourceChanged = m_processGpuUsageSource != usageSource;
    if (std::abs(m_processGpuUsage - usage) < 0.0005 && !sourceChanged)
        return;
    m_processGpuUsage = usage;
    m_processGpuUsageSource = usageSource;
    emit processGpuUsageChanged();
    if (sourceChanged) {
        qInfo().noquote()
            << "[GPUDBG] GPU usage counter source"
            << "source=" << usageSource
            << "scope=" << (usageSource == QStringLiteral("drm-device")
                                  ? QStringLiteral("whole_device")
                                  : QStringLiteral("process"));
    }
}

#undef applyFtOpenMpThreadLimit
