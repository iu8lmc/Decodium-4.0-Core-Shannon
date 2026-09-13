#pragma once

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QtGlobal>

#include <limits>

namespace decodium::gpu_usage {

// A process can expose the same DRM client through several file descriptors.
// Each fdinfo copy contains cumulative engine time for that client, so callers
// must keep the greatest value per engine rather than summing duplicate FDs.
struct LinuxDrmFdInfo
{
    QString descriptor;
    QString contents;
};

struct LinuxDrmCycleSample
{
    quint64 busyCycles {0};
    quint64 totalCycles {0};
};

// Some Linux DRM drivers expose a per-process counter but stop advancing it
// even while Qt Quick is still presenting frames.  Keep this decision logic
// independent from the bridge so it can be covered by a small unit test.
inline int nextLinuxDrmUnchangedSampleCount(quint64 previousValue,
                                            quint64 currentValue,
                                            int previousCount)
{
    if (currentValue != previousValue)
        return 0;
    return qMin(qMax(0, previousCount) + 1, 1000);
}

inline bool linuxDrmCounterIsStale(int unchangedSampleCount,
                                   int threshold = 3)
{
    return unchangedSampleCount >= qMax(1, threshold);
}

// Pick the DRM PCI device that owns the most active memory. This identifies
// the Qt Quick rendering GPU on hybrid systems even when an application has
// descriptors open on more than one adapter. Older drivers without active
// memory counters fall back to the most frequently referenced device.
inline QString primaryLinuxDrmPciDevice(QList<LinuxDrmFdInfo> const& descriptors)
{
    static QRegularExpression const pdevRe(QStringLiteral("^drm-pdev:\\s*(.+)$"));
    static QRegularExpression const activeMemoryRe(
        QStringLiteral("^drm-active-[^:]+:\\s*(\\d+)"));

    QHash<QString, quint64> maxActiveMemoryByDevice;
    QHash<QString, int> descriptorCountByDevice;
    for (LinuxDrmFdInfo const& descriptor : descriptors) {
        QString pciDevice;
        quint64 activeMemory = 0;
        for (QString const& rawLine : descriptor.contents.split(QLatin1Char('\n'))) {
            QString const line = rawLine.trimmed();
            QRegularExpressionMatch const pdevMatch = pdevRe.match(line);
            if (pdevMatch.hasMatch()) {
                pciDevice = pdevMatch.captured(1).trimmed();
                continue;
            }
            QRegularExpressionMatch const activeMatch = activeMemoryRe.match(line);
            if (activeMatch.hasMatch()) {
                bool ok = false;
                quint64 const value = activeMatch.captured(1).toULongLong(&ok);
                if (ok)
                    activeMemory = qMax(activeMemory, value);
            }
        }
        if (pciDevice.isEmpty())
            continue;
        maxActiveMemoryByDevice[pciDevice] = qMax(
            maxActiveMemoryByDevice.value(pciDevice), activeMemory);
        ++descriptorCountByDevice[pciDevice];
    }

    QString selected;
    quint64 selectedActiveMemory = 0;
    int selectedDescriptorCount = 0;
    for (auto it = descriptorCountByDevice.cbegin(); it != descriptorCountByDevice.cend(); ++it) {
        quint64 const activeMemory = maxActiveMemoryByDevice.value(it.key());
        if (selected.isEmpty()
            || activeMemory > selectedActiveMemory
            || (activeMemory == selectedActiveMemory && it.value() > selectedDescriptorCount)) {
            selected = it.key();
            selectedActiveMemory = activeMemory;
            selectedDescriptorCount = it.value();
        }
    }
    return selected;
}

inline quint64 saturatingAdd(quint64 left, quint64 right)
{
    return std::numeric_limits<quint64>::max() - left < right
        ? std::numeric_limits<quint64>::max()
        : left + right;
}

inline quint64 saturatingMultiply(quint64 value, quint64 multiplier)
{
    if (value == 0 || multiplier == 0)
        return 0;
    return std::numeric_limits<quint64>::max() / value < multiplier
        ? std::numeric_limits<quint64>::max()
        : value * multiplier;
}

inline bool aggregateLinuxDrmEngineTimeNs(QList<LinuxDrmFdInfo> const& descriptors,
                                          quint64* gpuTimeNs)
{
    static QRegularExpression const driverRe(QStringLiteral("^drm-driver:\\s*(.+)$"));
    static QRegularExpression const clientIdRe(QStringLiteral("^drm-client-id:\\s*(.+)$"));
    static QRegularExpression const pdevRe(QStringLiteral("^drm-pdev:\\s*(.+)$"));
    static QRegularExpression const engineTimeRe(
        QStringLiteral("^drm-engine-([^:]+):\\s*(\\d+)\\s*([a-zA-Z]*)"));

    QHash<QString, QHash<QString, quint64>> maxEngineTimeByClient;
    bool found = false;

    for (LinuxDrmFdInfo const& descriptor : descriptors) {
        QString driver;
        QString clientId;
        QString pdev;
        QHash<QString, quint64> engineTimes;

        for (QString const& rawLine : descriptor.contents.split(QLatin1Char('\n'))) {
            QString const line = rawLine.trimmed();
            QRegularExpressionMatch const driverMatch = driverRe.match(line);
            if (driverMatch.hasMatch()) {
                driver = driverMatch.captured(1).trimmed();
                continue;
            }
            QRegularExpressionMatch const clientIdMatch = clientIdRe.match(line);
            if (clientIdMatch.hasMatch()) {
                clientId = clientIdMatch.captured(1).trimmed();
                continue;
            }
            QRegularExpressionMatch const pdevMatch = pdevRe.match(line);
            if (pdevMatch.hasMatch()) {
                pdev = pdevMatch.captured(1).trimmed();
                continue;
            }

            QRegularExpressionMatch const engineMatch = engineTimeRe.match(line);
            if (!engineMatch.hasMatch())
                continue;

            bool ok = false;
            quint64 value = engineMatch.captured(2).toULongLong(&ok);
            if (!ok)
                continue;

            QString const unit = engineMatch.captured(3).toLower();
            if (unit == QStringLiteral("us"))
                value *= 1000ULL;
            else if (unit == QStringLiteral("ms"))
                value *= 1000000ULL;

            QString const engine = engineMatch.captured(1);
            engineTimes[engine] = qMax(engineTimes.value(engine), value);
        }

        if (engineTimes.isEmpty())
            continue;

        // Old kernels might not expose a client id.  In that case retain the
        // descriptor as an independent source rather than merging unrelated
        // clients that happen to use the same driver.
        QString const clientKey = clientId.isEmpty()
            ? QStringLiteral("fd:%1").arg(descriptor.descriptor)
            : driver + QChar(0x1f) + clientId + QChar(0x1f) + pdev;
        QHash<QString, quint64>& maxEngineTime = maxEngineTimeByClient[clientKey];
        for (auto it = engineTimes.cbegin(); it != engineTimes.cend(); ++it) {
            maxEngineTime[it.key()] = qMax(maxEngineTime.value(it.key()), it.value());
        }
        found = true;
    }

    if (!found)
        return false;

    quint64 totalNs = 0;
    for (auto client = maxEngineTimeByClient.cbegin(); client != maxEngineTimeByClient.cend(); ++client) {
        for (auto engine = client->cbegin(); engine != client->cend(); ++engine) {
            if (std::numeric_limits<quint64>::max() - totalNs < engine.value()) {
                totalNs = std::numeric_limits<quint64>::max();
                break;
            }
            totalNs += engine.value();
        }
    }

    if (gpuTimeNs)
        *gpuTimeNs = totalNs;
    return true;
}

// Newer DRM drivers, notably Intel Xe, may expose drm-cycles-* and
// drm-total-cycles-* instead of nanosecond drm-engine-* counters.  Duplicate
// file descriptors still describe the same DRM client and must be collapsed.
// Busy cycles are summed across clients, whereas the matching total-cycle
// clock is counted once per device/engine.  Engine capacity is included in the
// denominator for groups containing more than one identical hardware engine.
inline bool aggregateLinuxDrmCycleSample(QList<LinuxDrmFdInfo> const& descriptors,
                                         LinuxDrmCycleSample* sample)
{
    static QRegularExpression const driverRe(QStringLiteral("^drm-driver:\\s*(.+)$"));
    static QRegularExpression const clientIdRe(QStringLiteral("^drm-client-id:\\s*(.+)$"));
    static QRegularExpression const pdevRe(QStringLiteral("^drm-pdev:\\s*(.+)$"));
    static QRegularExpression const busyCyclesRe(
        QStringLiteral("^drm-cycles-([^:]+):\\s*(\\d+)"));
    static QRegularExpression const totalCyclesRe(
        QStringLiteral("^drm-total-cycles-([^:]+):\\s*(\\d+)"));
    static QRegularExpression const capacityRe(
        QStringLiteral("^drm-engine-capacity-([^:]+):\\s*(\\d+)"));

    QHash<QString, QHash<QString, quint64>> maxBusyByClient;
    QHash<QString, QHash<QString, quint64>> maxTotalByClient;
    QHash<QString, QHash<QString, quint64>> maxCapacityByClient;
    QHash<QString, QString> deviceByClient;

    for (LinuxDrmFdInfo const& descriptor : descriptors) {
        QString driver;
        QString clientId;
        QString pdev;
        QHash<QString, quint64> busyCycles;
        QHash<QString, quint64> totalCycles;
        QHash<QString, quint64> capacities;

        for (QString const& rawLine : descriptor.contents.split(QLatin1Char('\n'))) {
            QString const line = rawLine.trimmed();
            QRegularExpressionMatch match = driverRe.match(line);
            if (match.hasMatch()) {
                driver = match.captured(1).trimmed();
                continue;
            }
            match = clientIdRe.match(line);
            if (match.hasMatch()) {
                clientId = match.captured(1).trimmed();
                continue;
            }
            match = pdevRe.match(line);
            if (match.hasMatch()) {
                pdev = match.captured(1).trimmed();
                continue;
            }

            auto captureCounter = [&line](QRegularExpression const& expression,
                                          QHash<QString, quint64>* values) {
                QRegularExpressionMatch const counterMatch = expression.match(line);
                if (!counterMatch.hasMatch())
                    return false;
                bool ok = false;
                quint64 const value = counterMatch.captured(2).toULongLong(&ok);
                if (ok) {
                    QString const engine = counterMatch.captured(1);
                    (*values)[engine] = qMax(values->value(engine), value);
                }
                return true;
            };
            if (captureCounter(busyCyclesRe, &busyCycles)
                || captureCounter(totalCyclesRe, &totalCycles)
                || captureCounter(capacityRe, &capacities)) {
                continue;
            }
        }

        if (busyCycles.isEmpty() || totalCycles.isEmpty())
            continue;

        QString const clientKey = clientId.isEmpty()
            ? QStringLiteral("fd:%1").arg(descriptor.descriptor)
            : driver + QChar(0x1f) + clientId + QChar(0x1f) + pdev;
        QString const deviceKey = driver + QChar(0x1f)
            + (pdev.isEmpty() ? QStringLiteral("unknown") : pdev);
        deviceByClient[clientKey] = deviceKey;

        for (auto it = busyCycles.cbegin(); it != busyCycles.cend(); ++it)
            maxBusyByClient[clientKey][it.key()] = qMax(maxBusyByClient[clientKey].value(it.key()), it.value());
        for (auto it = totalCycles.cbegin(); it != totalCycles.cend(); ++it)
            maxTotalByClient[clientKey][it.key()] = qMax(maxTotalByClient[clientKey].value(it.key()), it.value());
        for (auto it = capacities.cbegin(); it != capacities.cend(); ++it)
            maxCapacityByClient[clientKey][it.key()] = qMax(maxCapacityByClient[clientKey].value(it.key()), it.value());
    }

    QHash<QString, quint64> busyByDeviceEngine;
    QHash<QString, quint64> totalByDeviceEngine;
    QHash<QString, quint64> capacityByDeviceEngine;
    for (auto client = maxBusyByClient.cbegin(); client != maxBusyByClient.cend(); ++client) {
        QString const deviceKey = deviceByClient.value(client.key());
        QHash<QString, quint64> const totals = maxTotalByClient.value(client.key());
        QHash<QString, quint64> const capacities = maxCapacityByClient.value(client.key());
        for (auto engine = client->cbegin(); engine != client->cend(); ++engine) {
            if (!totals.contains(engine.key()))
                continue;
            QString const groupKey = deviceKey + QChar(0x1f) + engine.key();
            busyByDeviceEngine[groupKey] = saturatingAdd(busyByDeviceEngine.value(groupKey),
                                                         engine.value());
            totalByDeviceEngine[groupKey] = qMax(totalByDeviceEngine.value(groupKey),
                                                  totals.value(engine.key()));
            capacityByDeviceEngine[groupKey] = qMax<quint64>(
                qMax<quint64>(1, capacityByDeviceEngine.value(groupKey)),
                qMax<quint64>(1, capacities.value(engine.key(), 1)));
        }
    }

    if (busyByDeviceEngine.isEmpty())
        return false;

    LinuxDrmCycleSample result;
    for (auto group = busyByDeviceEngine.cbegin(); group != busyByDeviceEngine.cend(); ++group) {
        quint64 const total = totalByDeviceEngine.value(group.key());
        if (total == 0)
            continue;
        result.busyCycles = saturatingAdd(result.busyCycles, group.value());
        result.totalCycles = saturatingAdd(
            result.totalCycles,
            saturatingMultiply(total, capacityByDeviceEngine.value(group.key(), 1)));
    }

    if (result.totalCycles == 0)
        return false;
    if (sample)
        *sample = result;
    return true;
}

} // namespace decodium::gpu_usage
