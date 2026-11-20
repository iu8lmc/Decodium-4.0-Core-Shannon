#include "DecodiumBridge.h"
#include "Network/RemoteCommandServer.hpp"
#include <QDateTime>

QString decodiumBridgeNormalizeRemoteActivityTime(QString raw);

void DecodiumBridge::clearRemoteActivityCache(bool notifyRemote)
{
    m_remoteActivityKeys.clear();
    m_remoteActivityKeyOrder.clear();
    if (notifyRemote && m_remoteServer && m_remoteServer->isRunning()) {
        m_remoteServer->clearBandActivity();
    }
}

void DecodiumBridge::publishRemoteActivityEntries(QVariantList const& entries)
{
    if (!m_remoteServer || !m_remoteServer->isRunning()) {
        return;
    }

    for (QVariant const& value : entries) {
        publishRemoteActivityEntry(value.toMap());
    }
}

void DecodiumBridge::publishRemoteActivityEntry(QVariantMap const& entry)
{
    if (!m_remoteServer || !m_remoteServer->isRunning()) {
        return;
    }

    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return;
    }

    QString time = decodiumBridgeNormalizeRemoteActivityTime(entry.value(QStringLiteral("time")).toString());
    if (time.isEmpty()) {
        time = QDateTime::currentDateTimeUtc().toString(QStringLiteral("HHmmss"));
    }

    QString db = entry.value(QStringLiteral("db")).toString().trimmed();
    if (db.isEmpty()) {
        db = entry.value(QStringLiteral("isTx")).toBool() ? QStringLiteral("TX") : QStringLiteral("0");
    }

    QString dt = entry.value(QStringLiteral("dt")).toString().trimmed();
    if (dt.isEmpty()) {
        dt = QStringLiteral("0.0");
    }

    QString freq = entry.value(QStringLiteral("freq")).toString().trimmed();
    if (freq.isEmpty()) {
        freq = QString::number(entry.value(QStringLiteral("isTx")).toBool() ? m_txFrequency : m_rxFrequency);
    }

    QString const line = QStringLiteral("%1 %2 %3 %4 %5")
                             .arg(time, db, dt, freq, message)
                             .simplified();
    QString const key = line + QStringLiteral("|") + entry.value(QStringLiteral("mode"), m_mode).toString();
    if (m_remoteActivityKeys.contains(key)) {
        return;
    }

    m_remoteActivityKeys.insert(key);
    m_remoteActivityKeyOrder.append(key);
    while (m_remoteActivityKeyOrder.size() > 4096) {
        m_remoteActivityKeys.remove(m_remoteActivityKeyOrder.takeFirst());
    }

    m_remoteServer->publishBandActivityLine(line);
}

