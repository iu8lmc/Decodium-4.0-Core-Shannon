#include "DecodiumBridge.h"
#include "controllers/ActiveStationsModel.hpp"
#include "Network/DecoSyncTime.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <cmath>
#include <utility>

QString decodiumBridgeMirrorEntryKey(const QVariantMap&);
int decodiumBridgeSecondsFromUtcDisplayToken(QString);
void decodiumBridgeSecondaryDecodeLog(const QString&);

void DecodiumBridge::queueLegacyDecodeSecondaryWork(QVariantList const& entries,
                                                     QString const& source)
{
    for (QVariant const& value : entries) {
        QVariantMap const entry = value.toMap();
        QString key = decodiumBridgeMirrorEntryKey(entry);
        if (key.isEmpty()) {
            key = entry.value(QStringLiteral("time")).toString()
                + QLatin1Char('|') + entry.value(QStringLiteral("freq")).toString()
                + QLatin1Char('|') + entry.value(QStringLiteral("message")).toString();
        }
        if (m_legacyDecodeSecondaryPendingKeys.contains(key)) {
            continue;
        }
        m_legacyDecodeSecondaryPendingKeys.insert(key);
        m_legacyDecodeSecondaryQueue.enqueue({entry, source, key});
    }
    if (!m_legacyDecodeSecondaryQueue.isEmpty()) {
        scheduleLegacyDecodeSecondaryDrain();
    }
}

void DecodiumBridge::scheduleLegacyDecodeSecondaryDrain(int delayMs)
{
    if (m_legacyDecodeSecondaryDrainScheduled) {
        return;
    }
    m_legacyDecodeSecondaryDrainScheduled = true;
    QTimer::singleShot(qMax(0, delayMs), this, [this] {
        m_legacyDecodeSecondaryDrainScheduled = false;
        drainLegacyDecodeSecondaryWork();
    });
}

void DecodiumBridge::drainLegacyDecodeSecondaryWork()
{
    constexpr qint64 kSecondaryBudgetMs = 4;
    constexpr int kSecondaryRowsPerCycle = 8;
    QElapsedTimer budget;
    budget.start();
    int processed = 0;
    int persisted = 0;

    auto isFreshForPsk = [this](QString const& token) {
        int const decodeSeconds = decodiumBridgeSecondsFromUtcDisplayToken(token);
        if (decodeSeconds < 0) {
            return true;
        }
        int const nowSeconds =
            QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;
        int const periodMs = periodMsForMode(m_mode);
        int const freshnessWindowSeconds = qMax(90, (periodMs > 0 ? periodMs / 1000 : 15) * 4);
        int diff = std::abs(nowSeconds - decodeSeconds);
        diff = qMin(diff, 86400 - diff);
        return diff <= freshnessWindowSeconds;
    };

    while (processed < kSecondaryRowsPerCycle
           && budget.elapsed() < kSecondaryBudgetMs
           && !m_legacyDecodeSecondaryQueue.isEmpty()) {
        LegacyDecodeSecondaryWork const work = m_legacyDecodeSecondaryQueue.dequeue();
        m_legacyDecodeSecondaryPendingKeys.remove(work.key);
        QVariantMap const& entry = work.entry;
        QString const entryMode = entry.value(QStringLiteral("mode")).toString().trimmed().toUpper();
        if (!entryMode.isEmpty() && entryMode != m_mode.trimmed().toUpper()) {
            ++processed;
            continue;
        }

        queueWorldMapEntryForReplay(entry, false, 250);
        QVariantList singleEntry;
        singleEntry.append(entry);
        publishRemoteActivityEntries(singleEntry);
        if (!entry.value(QStringLiteral("isTx")).toBool()) {
            QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
            if (!message.isEmpty()
                && isFreshForPsk(entry.value(QStringLiteral("time")).toString())) {
                maybeQueuePskReporterSpot(entry,
                                          message,
                                          entry.value(QStringLiteral("isCQ")).toBool(),
                                          entry.value(QStringLiteral("freq")).toString(),
                                          entry.value(QStringLiteral("db")).toString(),
                                          entry.value(QStringLiteral("mode"), m_mode).toString());
            }
        }
        if (persistDecodeHistoryEntry(entry)) {
            ++persisted;
        }
        ++processed;
    }

    if (persisted > 0) {
        decodiumBridgeSecondaryDecodeLog(QStringLiteral("decode-history deferred: %1 new rows").arg(persisted));
    }
    if (!m_legacyDecodeSecondaryQueue.isEmpty()) {
        scheduleLegacyDecodeSecondaryDrain();
    }
}

void DecodiumBridge::queueNativeDecodeSecondaryWork(NativeDecodeSecondaryWork work)
{
    if (work.key.isEmpty()) {
        work.key = decodiumBridgeMirrorEntryKey(work.entry);
    }
    if (work.key.isEmpty()) {
        work.key = QStringLiteral("%1|%2|%3")
            .arg(work.entry.value(QStringLiteral("time")).toString(),
                 work.entry.value(QStringLiteral("freq")).toString(),
                 work.entry.value(QStringLiteral("message")).toString());
    }
    work.key = QString::number(m_decodeSessionId) + QLatin1Char('|') + work.key;
    if (m_nativeDecodeSecondaryPendingKeys.contains(work.key)) {
        return;
    }

    m_nativeDecodeSecondaryPendingKeys.insert(work.key);
    m_nativeDecodeSecondaryQueue.enqueue(std::move(work));
    scheduleNativeDecodeSecondaryDrain();
}

void DecodiumBridge::scheduleNativeDecodeSecondaryDrain(int delayMs)
{
    if (m_nativeDecodeSecondaryDrainScheduled) {
        return;
    }
    m_nativeDecodeSecondaryDrainScheduled = true;
    QTimer::singleShot(qMax(0, delayMs), this, [this] {
        m_nativeDecodeSecondaryDrainScheduled = false;
        drainNativeDecodeSecondaryWork();
    });
}

void DecodiumBridge::drainNativeDecodeSecondaryWork()
{
    constexpr qint64 kSecondaryBudgetMs = 4;
    constexpr int kSecondaryRowsPerCycle = 8;
    QElapsedTimer budget;
    budget.start();
    int processed = 0;

    auto isFreshForPsk = [this](QString const& token) {
        int const decodeSeconds = decodiumBridgeSecondsFromUtcDisplayToken(token);
        if (decodeSeconds < 0) {
            return true;
        }
        int const nowSeconds =
            QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;
        int const periodMs = periodMsForMode(m_mode);
        int const freshnessWindowSeconds = qMax(90, (periodMs > 0 ? periodMs / 1000 : 15) * 4);
        int diff = std::abs(nowSeconds - decodeSeconds);
        diff = qMin(diff, 86400 - diff);
        return diff <= freshnessWindowSeconds;
    };

    while (processed < kSecondaryRowsPerCycle
           && budget.elapsed() < kSecondaryBudgetMs
           && !m_nativeDecodeSecondaryQueue.isEmpty()) {
        NativeDecodeSecondaryWork work = m_nativeDecodeSecondaryQueue.dequeue();
        m_nativeDecodeSecondaryPendingKeys.remove(work.key);
        QVariantMap const& entry = work.entry;
        QString const entryMode = entry.value(QStringLiteral("mode")).toString().trimmed().toUpper();
        if (!entryMode.isEmpty() && entryMode != m_mode.trimmed().toUpper()) {
            ++processed;
            continue;
        }

        QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
        bool const isTx = entry.value(QStringLiteral("isTx")).toBool();
        bool const isCQ = entry.value(QStringLiteral("isCQ")).toBool();
        bool const isMyCall = entry.value(QStringLiteral("isMyCall")).toBool();
        if (work.publishPsk && !isTx && !message.isEmpty()
            && isFreshForPsk(entry.value(QStringLiteral("time")).toString())) {
            maybeQueuePskReporterSpot(entry,
                                      message,
                                      isCQ,
                                      entry.value(QStringLiteral("freq")).toString(),
                                      entry.value(QStringLiteral("db")).toString(),
                                      entry.value(QStringLiteral("mode"), m_mode).toString());
        }
        if (work.updateActiveStation && isCQ && m_activeStations) {
            QString const call = entry.value(QStringLiteral("fromCall")).toString().trimmed();
            if (!call.isEmpty()) {
                m_activeStations->addStation(call,
                                             entry.value(QStringLiteral("freq")).toInt(),
                                             entry.value(QStringLiteral("db")).toInt(),
                                             entry.value(QStringLiteral("dxGrid")).toString(),
                                             entry.value(QStringLiteral("time")).toString());
            }
        }
        if (work.updateWorldMap) {
            queueWorldMapEntryForReplay(entry, false, 250);
        }
        if (work.reportDecodeTiming && m_decoSyncTime && !isTx) {
            bool dtOk = false;
            bool snrOk = false;
            double const dt = entry.value(QStringLiteral("dt")).toString().toDouble(&dtOk);
            int const db = entry.value(QStringLiteral("db")).toString().toInt(&snrOk);
            if (dtOk && snrOk) {
                m_decoSyncTime->reportDecodeDt(dt, db);
            }
        }
        if (work.sendUdp && !work.rawRow.isEmpty()) {
            udpSendDecode(true, work.rawRow, work.serial);
        }
        if (work.playAlert) {
            maybePlayDecodeAlert(isCQ, isMyCall, message);
        }
        ++processed;
    }

    if (!m_nativeDecodeSecondaryQueue.isEmpty()) {
        scheduleNativeDecodeSecondaryDrain();
    }
}

