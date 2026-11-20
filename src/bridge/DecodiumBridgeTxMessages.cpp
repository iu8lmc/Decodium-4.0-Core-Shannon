#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Modulator/FtxMessageEncoder.hpp"

extern QString decodiumBridgeCustomCqPrefix(const QString&, const QString&);
extern QString decodiumBridgeStandardFtxCq(const QString&, const QString&, const QString&);
extern bool decodiumBridgeGeneratedTxWaveMode(const QString&);

QString DecodiumBridge::tx1() const { return m_tx1; }
void DecodiumBridge::setTx1(const QString& v) {
    if (m_tx1!=v) {
        m_tx1=v;
        emit tx1Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(1, v);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
QString DecodiumBridge::tx2() const { return m_tx2; }
void DecodiumBridge::setTx2(const QString& v) {
    if (m_tx2!=v) {
        m_tx2=v;
        emit tx2Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(2, v);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
QString DecodiumBridge::tx3() const { return m_tx3; }
void DecodiumBridge::setTx3(const QString& v) {
    if (m_tx3!=v) {
        m_tx3=v;
        emit tx3Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(3, v);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
QString DecodiumBridge::tx4() const { return m_tx4; }
void DecodiumBridge::setTx4(const QString& v) {
    if (m_tx4!=v) {
        m_tx4=v;
        emit tx4Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(4, v);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
QString DecodiumBridge::tx5() const { return m_tx5; }
void DecodiumBridge::setTx5(const QString& v) {
    if (m_tx5!=v) {
        m_tx5=v;
        emit tx5Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(5, v);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}
QString DecodiumBridge::tx6() const { return m_tx6; }
void DecodiumBridge::setTx6(const QString& v) {
    if (m_tx6!=v) {
        m_tx6=v;
        emit tx6Changed();
        emit txMessagesChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->setTxMessage(6, v);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
}

QString DecodiumBridge::txMessage(int n) const
{
    switch (qBound(1, n, 6)) {
    case 1: return m_tx1;
    case 2: return m_tx2;
    case 3: return m_tx3;
    case 4: return m_tx4;
    case 5: return m_tx5;
    case 6: return m_tx6;
    default: return QString();
    }
}

void DecodiumBridge::setTxMessage(int n, const QString& message)
{
    QString const normalized = message.trimmed().toUpper();
    switch (qBound(1, n, 6)) {
    case 1: setTx1(normalized); break;
    case 2: setTx2(normalized); break;
    case 3: setTx3(normalized); break;
    case 4: setTx4(normalized); break;
    case 5: setTx5(normalized); break;
    case 6:
        m_customCqPrefix = decodiumBridgeCustomCqPrefix(normalized, m_callsign);
        // The TX panel is an explicit operator edit. Preserve the complete
        // CQ so a QSO completion or restart cannot restore a removed grid.
        m_customCqMessage = normalized == decodiumBridgeStandardFtxCq(
            m_callsign, m_grid.left(4), m_mode.trimmed().toUpper())
            ? QString {}
            : normalized;
        setTx6(normalized);
        break;
    default: break;
    }
}

QString DecodiumBridge::validateTxMessage(const QString& message) const
{
    QString const msg = message.trimmed().toUpper();
    if (msg.isEmpty()) {
        return QStringLiteral("Messaggio TX vuoto");
    }

    QString const mode = m_mode.trimmed().toUpper();
    if (!mode.isEmpty() && !decodiumBridgeGeneratedTxWaveMode(mode)) {
        return QString();
    }

    bool ok = false;
    if (mode == QStringLiteral("FT2")) {
        ok = decodium::txmsg::encodeFt2(msg, true).ok;
    } else if (mode == QStringLiteral("FT4")) {
        ok = decodium::txmsg::encodeFt4(msg, true).ok;
    } else if (mode == QStringLiteral("FT8") || mode.isEmpty()) {
        ok = decodium::txmsg::encodeFt8(msg).ok;
    } else if (mode == QStringLiteral("JT65")) {
        ok = decodium::txmsg::encodeJt65(msg, true).ok;
    } else if (mode == QStringLiteral("JT4")) {
        ok = decodium::txmsg::encodeJt4(msg, true).ok;
    } else if (mode == QStringLiteral("JT9")) {
        ok = decodium::txmsg::encodeJt9(msg, true).ok;
    } else if (mode == QStringLiteral("WSPR")) {
        ok = decodium::txmsg::encodeWspr(msg, true).ok;
    } else if (mode == QStringLiteral("MSK144")) {
        ok = decodium::txmsg::encodeMsk144(msg, true).ok;
    } else if (mode == QStringLiteral("Q65") || mode.startsWith(QStringLiteral("Q65-"))) {
        ok = decodium::txmsg::encodeQ65(msg, true).ok;
    } else if (mode == QStringLiteral("FST4")
               || mode == QStringLiteral("FST4W")
               || mode.startsWith(QStringLiteral("FST4-"))
               || mode.startsWith(QStringLiteral("FST4W-"))) {
        ok = decodium::txmsg::encodeFst4WithHint(msg, mode.startsWith(QStringLiteral("FST4W")), true).ok;
    }

    return ok ? QString()
              : QStringLiteral("Messaggio non codificabile in %1").arg(mode.isEmpty() ? QStringLiteral("FT8") : mode);
}

void DecodiumBridge::resetStandardTxMessages()
{
    m_customCqPrefix.clear();
    m_customCqMessage.clear();
    m_skipCustomCqPrefixRecovery = true;
    regenerateTxMessages();
}


void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::clearTxMessages()
{
    bridgeLog("clearTxMessages: reset QSO/TX state");
    // 1.0.364+ MAM multi-stream nativo (FASE 2): reset slot multi-QSO.
    m_mamSlots.clear();
    m_mamLateReportRecoveries.clear();
    m_mamMessages.clear();
    m_mamF0sHz.clear();
    m_mamGains.clear();
    emit mamTxStreamsChanged();
    clearDeferredManualSyncTx(QStringLiteral("clear-tx-messages"));

    if (m_transmitting || m_tuning)
        halt();

    setTxEnabled(false);
    m_qsoCooldown.clear();
    m_txRetryCount = 0;
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_lastNtx = -1;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    clearPendingAutoSeqTx(QStringLiteral("clear-tx-messages"));

    if (m_qsoProgress != 0) {
        m_qsoProgress = 0;
        emit qsoProgressChanged();
    }
    if (m_reportSent != "-10") {
        m_reportSent = "-10";
        emit reportSentChanged();
    }
    if (m_reportReceived != "-10") {
        m_reportReceived = "-10"; m_reportReceivedReal = false;
        emit reportReceivedChanged();
    }

    setDxCall(QString());
    setDxGrid(QString());
    setTx1(QString());
    setTx2(QString());
    setTx3(QString());
    setTx4(QString());
    setTx5(QString());
    setCurrentTx(m_tx6.trimmed().isEmpty() ? 1 : 6);
}

#undef bridgeLog
