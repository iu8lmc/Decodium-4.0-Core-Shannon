#include "DecodiumBridge.h"
#include "DecodeValidationRules.h"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>
#include <limits>
using namespace decodium::seq;
using decodium::decode_validation::isPlainSignalReportToken;
using decodium::decode_validation::isRogerSignalReportToken;
void decodiumBridgeDecodeFlowLog(const QString&);
QString decodiumBridgeInferPartnerForAutolog(const QString&, const QString&, const QString&);
int decodiumBridgeSpecialOpHound();
#define bridgeLog decodiumBridgeDecodeFlowLog
#define inferPartnerFromDirectedMessage decodiumBridgeInferPartnerForAutolog
#define kSpecialOpHound decodiumBridgeSpecialOpHound()

bool DecodiumBridge::forceRecentRogerReportSignoffIfNeeded(QString& message, const QString& reason)
{
    if (m_specialOperationActivity == kSpecialOpHound
        || m_currentTx < 1
        || m_currentTx > 5
        || m_currentTx == 4
        || m_currentTx == 5
        || isTxDisabled(4)) {
        return false;
    }
    if (!m_autoSeq && !m_autoCqRepeat) {
        return false;
    }

    QString const activeCall = m_dxCall.trimmed();
    QString const activeBase = normalizedBaseCall(activeCall);
    QString const myCall = normalizeCallToken(m_callsign).trimmed().toUpper();
    QString const myBase = normalizedBaseCall(myCall);
    if (activeBase.isEmpty() || myCall.isEmpty()) {
        return false;
    }

    QString const payload = message.trimmed();
    QStringList const payloadTokens = normalizedMessageTokens(payload);
    bool const selectedReportPayload =
        m_currentTx == 2
        || m_currentTx == 3
        || (!payloadTokens.isEmpty()
            && (isPlainSignalReportToken(payloadTokens.constLast())
                || isRogerSignalReportToken(payloadTokens.constLast())));
    if (!selectedReportPayload
        || !messageContainsCallToken(payload, activeCall, activeBase)) {
        return false;
    }

    bool const priorReportSentToActivePartner =
        (m_lastNtx == 2 || m_lastNtx == 3)
        && messageContainsCallToken(m_lastTransmittedMessage, activeCall, activeBase);
    bool const autoSeqAlreadySawRogerReport =
        !m_autoSeqRogerReportBase.trimmed().isEmpty()
        && m_autoSeqRogerReportBase.trimmed().compare(activeBase, Qt::CaseInsensitive) == 0;
    if (!priorReportSentToActivePartner && !autoSeqAlreadySawRogerReport) {
        return false;
    }

    QStringList toCandidates;
    QStringList fromCandidates;
    auto addUnique = [](QStringList& list, QString value) {
        value = normalizeCallToken(value).trimmed().toUpper();
        if (!value.isEmpty() && !list.contains(value)) {
            list.append(value);
        }
    };
    addUnique(toCandidates, myCall);
    addUnique(toCandidates, myBase);
    addUnique(fromCandidates, activeCall);
    addUnique(fromCandidates, activeBase);

    QString const modeKey = m_mode.trimmed().toUpper();
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const periodMs = qMax<qint64>(1, periodMsForMode(m_mode));
    qint64 const validWindowMs = qMax<qint64>(6000, periodMs + 2500);
    qint64 bestAgeMs = std::numeric_limits<qint64>::max();
    QString bestMessage;
    QString bestKey;
    for (const QString& toCall : toCandidates) {
        for (const QString& fromCall : fromCandidates) {
            QString const key = modeKey
                + QLatin1Char('|') + toCall
                + QLatin1Char('|') + fromCall
                + QStringLiteral("|R_REPORT");
            auto const it = m_recentDirectedReportDecodeMs.constFind(key);
            if (it == m_recentDirectedReportDecodeMs.cend()) {
                continue;
            }
            qint64 const ageMs = nowMs - it.value();
            if (ageMs >= 0 && ageMs <= validWindowMs && ageMs < bestAgeMs) {
                bestAgeMs = ageMs;
                bestMessage = m_recentDirectedReportDecodeMessage.value(key);
                bestKey = key;
            }
        }
    }
    if (bestMessage.isEmpty()) {
        return false;
    }

    genStdMsgs(activeCall, m_dxGrid.trimmed());
    QString const forcedMessage = m_tx4.trimmed();
    if (forcedMessage.isEmpty()
        || !messageContainsCallToken(forcedMessage, activeCall, activeBase)) {
        bridgeLog(QStringLiteral("TX late R+report guard could not force TX4 (%1): active=%2 tx4=[%3] decode=[%4]")
                      .arg(reason,
                           activeBase,
                           forcedMessage,
                           bestMessage));
        return false;
    }

    QString const oldMessage = payload;
    m_autoSeqRogerReportBase = activeBase;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    if (!advanceQsoState(4)) {
        return false;
    }
    updateAutoCqPartnerLock();
    invalidateTxAudioCache();
    scheduleTxAudioPrecompute();
    message = buildCurrentTxMessage().trimmed();

    bridgeLog(QStringLiteral("TX late R+report guard forced TX4/RR73 (%1): active=%2 age=%3ms key=%4 old=[%5] new=[%6] decode=[%7]")
                  .arg(reason,
                       activeBase,
                       QString::number(bestAgeMs),
                       bestKey,
                       oldMessage,
                       message,
                       bestMessage));
    return true;
}

bool DecodiumBridge::repairOrRejectStalePartnerTxMessage(QString& message, const QString& reason)
{
    if (m_currentTx < 1 || m_currentTx > 5) {
        return true;
    }
    if (m_specialOperationActivity == kSpecialOpHound) {
        return true;
    }

    QString const activeCall = m_dxCall.trimmed();
    QString const activeBase = Radio::base_callsign(activeCall).trimmed().toUpper();
    if (activeBase.isEmpty()) {
        return true;
    }

    QString const payload = message.trimmed();
    if (payload.isEmpty() || messageContainsCallToken(payload, activeCall, activeBase)) {
        return true;
    }

    bool const staleSignoffState =
        m_currentTx >= 4
        || messageCarries73Payload(payload)
        || m_logAfterOwn73
        || m_ft2DeferredLogPending
        || m_nTx73 > 0;

    if (!staleSignoffState) {
        QString const stalePayload = payload;
        genStdMsgs(activeCall, m_dxGrid.trimmed());
        message = buildCurrentTxMessage().trimmed();
        if (!message.isEmpty() && messageContainsCallToken(message, activeCall, activeBase)) {
            bridgeLog(QStringLiteral("TX stale partner payload repaired (%1): active=%2 tx=%3 old=[%4] new=[%5]")
                          .arg(reason,
                               activeBase,
                               QString::number(m_currentTx),
                               stalePayload,
                               message.trimmed()));
            return true;
        }
    }

    bridgeLog(QStringLiteral("TX stale partner payload blocked (%1): active=%2 tx=%3 payload=[%4] lastTx=%5 lastMsg=[%6]")
                  .arg(reason,
                       activeBase,
                       QString::number(m_currentTx),
                       payload,
                       QString::number(m_lastNtx),
                       m_lastTransmittedMessage.trimmed()));

    m_pendingAutoSeqTxAfterActiveTx = 0;
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;

    QString const lastTxBase = Radio::base_callsign(inferPartnerFromDirectedMessage(
        m_lastTransmittedMessage.trimmed(),
        m_callsign.trimmed().toUpper(),
        Radio::base_callsign(m_callsign).trimmed().toUpper())).trimmed().toUpper();
    bool const canRetryLastActiveStep =
        lastTxBase == activeBase
        && m_lastNtx >= 1
        && m_lastNtx <= 3
        && messageContainsCallToken(m_lastTransmittedMessage, activeCall, activeBase);
    if (canRetryLastActiveStep) {
        int const retryTx = m_lastNtx;
        m_lastNtx = -1;
        if (!advanceQsoState(retryTx)) {
            return false;
        }
        if (!m_txEnabled) {
            setTxEnabled(true);
        }
        bridgeLog(QStringLiteral("TX stale partner guard restored active QSO retry TX%1 for %2")
                      .arg(retryTx)
                      .arg(activeBase));
    } else if (autoCqCanRestartTx6()) {
        m_lastNtx = -1;
        m_lastTransmittedMessage.clear();
        setDxCall(QString());
        setDxGrid(QString());
        if (!advanceQsoState(6)) {
            return false;
        }
        if (!m_txEnabled) {
            setTxEnabled(true);
        }
    } else {
        m_lastNtx = -1;
        m_lastTransmittedMessage.clear();
        if (m_txEnabled) {
            setTxEnabled(false);
        }
    }

    return false;
}

