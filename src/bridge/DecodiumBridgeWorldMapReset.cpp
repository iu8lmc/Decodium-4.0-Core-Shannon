#include "DecodiumBridge.h"
#include <QDateTime>
#include <utility>

QString decodiumBridgeMirrorEntryKey(const QVariantMap&);
int decodiumBridgeSecondsFromUtcDisplayToken(QString);
int decodiumBridgeSignedUtcSecondDelta(int, int);

QString DecodiumBridge::worldMapFeedEntryKey(const QVariantMap& entry) const
{
    QString key = decodiumBridgeMirrorEntryKey(entry);
    if (key.isEmpty()) {
        key = entry.value(QStringLiteral("message")).toString().trimmed();
    }
    return key;
}

bool DecodiumBridge::worldMapEntryFreshEnough(const QVariantMap& entry,
                                              int maxAgeSeconds,
                                              QString* reason) const
{
    maxAgeSeconds = qMax(1, maxAgeSeconds);

    bool timestampOk = false;
    qint64 const timestampMs = entry.value(QStringLiteral("timestamp")).toLongLong(&timestampOk);
    if (timestampOk && timestampMs > 0) {
        qint64 const ageMs = QDateTime::currentMSecsSinceEpoch() - timestampMs;
        if (ageMs > static_cast<qint64>(maxAgeSeconds) * 1000) {
            if (reason) {
                *reason = QStringLiteral("timestamp age %1s").arg(ageMs / 1000);
            }
            return false;
        }
        return true;
    }

    int const entrySeconds = decodiumBridgeSecondsFromUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
    if (entrySeconds < 0) {
        return true;
    }

    int const nowSeconds = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;
    int const ageSeconds = decodiumBridgeSignedUtcSecondDelta(entrySeconds, nowSeconds);
    if (ageSeconds > maxAgeSeconds) {
        if (reason) {
            *reason = QStringLiteral("decode age %1s").arg(ageSeconds);
        }
        return false;
    }
    return true;
}

void DecodiumBridge::resetWorldMapDisplayFromCurrentDecodes()
{
    m_deferredWorldMapFeedQueue.clear();
    m_deferredWorldMapFeedKeys.clear();
    m_worldMapFullReplayDeferred = false;
    m_worldMapDeferredLogActive = false;
    m_worldMapClearedDecodeKeys.clear();

    auto rememberEntry = [this](QVariantMap const& entry) {
        if (entry.value(QStringLiteral("isTx")).toBool()) {
            return;
        }
        QString const key = worldMapFeedEntryKey(entry);
        if (!key.isEmpty()) {
            m_worldMapClearedDecodeKeys.insert(key);
        }
    };
    auto rememberList = [&rememberEntry](QVariantList const& entries) {
        for (QVariant const& value : entries) {
            rememberEntry(value.toMap());
        }
    };

    rememberList(m_decodeList);
    rememberList(m_rxDecodeList);
    for (QVariantMap const& entry : std::as_const(m_pendingDecodeReleaseQueue)) {
        rememberEntry(entry);
    }

    emit worldMapResetRequested();
}

