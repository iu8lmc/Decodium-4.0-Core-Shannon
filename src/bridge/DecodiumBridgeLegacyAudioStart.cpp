#include "DecodiumBridge.h"
#include <QCoreApplication>

void decodiumBridgeLegacyAudioStartLog(const QString& message);

quint64 DecodiumBridge::armPendingLegacyBridgeAudioStart(const QString& reason)
{
#if defined(Q_OS_MAC)
    Q_UNUSED(reason);
    m_legacyBridgeAudioTxStartPending = true;
    return ++m_legacyBridgeAudioTxStartSerial;
#else
    Q_UNUSED(reason);
    return 0;
#endif
}

bool DecodiumBridge::consumePendingLegacyBridgeAudioStart(quint64 serial, const QString& reason)
{
#if defined(Q_OS_MAC)
    if (!m_legacyBridgeAudioTxStartPending || serial != m_legacyBridgeAudioTxStartSerial) {
        decodiumBridgeLegacyAudioStartLog(QStringLiteral("legacyBridgeTxAudio delayed start ignored (%1): stale serial").arg(reason));
        return false;
    }
    if (m_shuttingDown || QCoreApplication::closingDown()) {
        m_legacyBridgeAudioTxStartPending = false;
        decodiumBridgeLegacyAudioStartLog(QStringLiteral("legacyBridgeTxAudio delayed start cancelled (%1): shutdown").arg(reason));
        return false;
    }
    if (!shouldUseBridgeAudioForLegacyDigitalTx()) {
        m_legacyBridgeAudioTxStartPending = false;
        decodiumBridgeLegacyAudioStartLog(QStringLiteral("legacyBridgeTxAudio delayed start cancelled (%1): legacy bridge no longer active").arg(reason));
        return false;
    }
    if (!m_txEnabled && !m_deferredManualSyncTx && !m_transmitting) {
        m_legacyBridgeAudioTxStartPending = false;
        decodiumBridgeLegacyAudioStartLog(QStringLiteral("legacyBridgeTxAudio delayed start cancelled (%1): TX no longer armed").arg(reason));
        return false;
    }
    m_legacyBridgeAudioTxStartPending = false;
    return true;
#else
    Q_UNUSED(serial);
    Q_UNUSED(reason);
    return false;
#endif
}

void DecodiumBridge::cancelPendingLegacyBridgeAudioStart(const QString& reason)
{
#if defined(Q_OS_MAC)
    bool const hadPending = m_legacyBridgeAudioTxStartPending;
    ++m_legacyBridgeAudioTxStartSerial;
    m_legacyBridgeAudioTxStartPending = false;
    if (hadPending) {
        decodiumBridgeLegacyAudioStartLog(QStringLiteral("legacyBridgeTxAudio delayed start cancelled: %1").arg(reason));
    }
#else
    Q_UNUSED(reason);
#endif
}

