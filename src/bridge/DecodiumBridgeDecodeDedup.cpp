#include "DecodiumBridge.h"
#include <utility>

extern QString decodiumBridgeDedupKey(const QString&, const QString&, const QString&);
extern QString decodiumBridgeFt2AsyncDedupKey(const QString&, const QString&, const QString&, qint64);
extern QString decodiumBridgeMirrorEntryKey(const QVariantMap&);

void DecodiumBridge::resetNativeDecodeDedupIndex()
{
    m_nativeDecodeDedupKeys.clear();
    m_nativeDecodeDedupOrder.clear();
    m_nativeDecodeDedupSessionId = m_decodeSessionId;
}

void DecodiumBridge::rememberNativeDecodeDedupKey(const QString& key)
{
    if (key.isEmpty() || m_nativeDecodeDedupKeys.contains(key)) {
        return;
    }

    static constexpr int kNativeDecodeDedupCapacity = 768;
    m_nativeDecodeDedupKeys.insert(key);
    m_nativeDecodeDedupOrder.enqueue(key);
    while (m_nativeDecodeDedupOrder.size() > kNativeDecodeDedupCapacity) {
        m_nativeDecodeDedupKeys.remove(m_nativeDecodeDedupOrder.dequeue());
    }
}

void DecodiumBridge::resetFt2AsyncDecodeDedupIndex()
{
    m_ft2AsyncDecodeDedupKeys.clear();
    m_ft2AsyncDecodeDedupOrder.clear();
    m_ft2AsyncDecodeDedupSessionId = m_decodeSessionId;
}

void DecodiumBridge::rememberFt2AsyncDecodeDedupKey(const QString& key)
{
    if (key.isEmpty() || m_ft2AsyncDecodeDedupKeys.contains(key)) {
        return;
    }

    static constexpr int kFt2AsyncDecodeDedupCapacity = 768;
    m_ft2AsyncDecodeDedupKeys.insert(key);
    m_ft2AsyncDecodeDedupOrder.enqueue(key);
    while (m_ft2AsyncDecodeDedupOrder.size() > kFt2AsyncDecodeDedupCapacity) {
        m_ft2AsyncDecodeDedupKeys.remove(m_ft2AsyncDecodeDedupOrder.dequeue());
    }
}

void DecodiumBridge::rememberDecodeDedupEntry(const QVariantMap& entry)
{
    if (entry.value(QStringLiteral("isTx")).toBool()) {
        return;
    }
    if (entry.contains(QStringLiteral("decodeSessionId"))) {
        quint64 const entrySessionId = entry.value(QStringLiteral("decodeSessionId")).toULongLong();
        if (entrySessionId != 0 && entrySessionId != m_decodeSessionId) {
            return;
        }
    }
    if (entry.value(QStringLiteral("message")).toString().trimmed().isEmpty()) {
        return;
    }

    if (m_nativeDecodeDedupSessionId != m_decodeSessionId) {
        resetNativeDecodeDedupIndex();
    }
    rememberNativeDecodeDedupKey(decodiumBridgeDedupKey(
        entry.value(QStringLiteral("time")).toString(),
        entry.value(QStringLiteral("freq")).toString(),
        entry.value(QStringLiteral("message")).toString()));

    QString const mode = entry.value(QStringLiteral("mode"), m_mode)
                             .toString().trimmed().toUpper();
    if (mode != QLatin1String("FT2")) {
        return;
    }
    if (m_ft2AsyncDecodeDedupSessionId != m_decodeSessionId) {
        resetFt2AsyncDecodeDedupIndex();
    }
    rememberFt2AsyncDecodeDedupKey(decodiumBridgeFt2AsyncDedupKey(
        entry.value(QStringLiteral("time")).toString(),
        entry.value(QStringLiteral("freq")).toString(),
        entry.value(QStringLiteral("message")).toString(),
        entry.value(QStringLiteral("timestamp")).toLongLong()));
}

void DecodiumBridge::ensureNativeDecodeDedupIndex()
{
    if (m_nativeDecodeDedupSessionId == m_decodeSessionId
        && (!m_nativeDecodeDedupKeys.isEmpty() || m_decodeList.isEmpty())) {
        return;
    }

    resetNativeDecodeDedupIndex();
    for (QVariant const& value : std::as_const(m_decodeList)) {
        QVariantMap const entry = value.toMap();
        if (entry.value(QStringLiteral("isTx")).toBool()
            || entry.value(QStringLiteral("decodeSessionId")).toULongLong() != m_decodeSessionId) {
            continue;
        }
        rememberNativeDecodeDedupKey(decodiumBridgeDedupKey(
            entry.value(QStringLiteral("time")).toString(),
            entry.value(QStringLiteral("freq")).toString(),
            entry.value(QStringLiteral("message")).toString()));
    }
}

void DecodiumBridge::rebuildRxDecodeMirrorIndex()
{
    m_rxDecodeMirrorKeys.clear();
    m_rxDecodeMirrorKeys.reserve(m_rxDecodeList.size());
    for (QVariant const& value : std::as_const(m_rxDecodeList)) {
        QString const key = decodiumBridgeMirrorEntryKey(value.toMap());
        if (!key.isEmpty()) {
            m_rxDecodeMirrorKeys.insert(key);
        }
    }
}

