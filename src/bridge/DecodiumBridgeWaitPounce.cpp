#include "DecodiumBridge.h"
#include "BridgeConfigConstants.h"
#include "DecodeUserFilterConfig.h"
#include "DecodeUtcTime.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <cmath>

using decodium::seq::normalizedBaseCall;
void decodiumBridgeDecodeFlowLog(const QString&);
QString decodiumBridgeMirrorEntryKey(const QVariantMap&);

bool DecodiumBridge::tryStartWaitPounceFromEntry(const QVariantMap& entry,
                                                 const QVariantList& previousEntries,
                                                 const QString& source)
{
    if (!m_waitPounceActive || m_callsign.trimmed().isEmpty()) {
        return false;
    }

    if (!getSetting(QStringLiteral("WaitFeaturesEnabled"), true).toBool()) {
        return false;
    }

    if (m_specialOperationActivity == kSpecialOpFox
        || m_specialOperationActivity == kSpecialOpHound) {
        return false;
    }

    bool const qsoBusy =
        m_transmitting
        || m_tuning
        || m_autoCqRepeat
        || m_manualTxHold
        || !m_dxCall.trimmed().isEmpty()
        || (m_qsoProgress > 1 && m_qsoProgress < 6);
    if (qsoBusy || entry.value(QStringLiteral("isTx")).toBool()) {
        return false;
    }

    bool const passiveLabWaitPounce =
        QCoreApplication::instance()
        && QCoreApplication::instance()->property("decodiumLabPassiveWaitPounce").toBool();
    bool const txCqArmed =
        (m_txEnabled || passiveLabWaitPounce)
        && m_currentTx == 6
        && m_qsoProgress <= 1
        && m_dxCall.trimmed().isEmpty();
    if (!txCqArmed) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("Wait & Pounce ignored: TX/CQ is not armed; RF decode cannot arm TX by itself"));
        return false;
    }

    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        return false;
    }

    bool const cqMessage =
        entry.value(QStringLiteral("isCQ")).toBool()
        || message.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
        || message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive);
    if (!cqMessage) {
        return false;
    }

    QString rejectReason;
    if (!shouldAcceptDecodedMessage(message, &rejectReason)) {
        return false;
    }
    QString userFilterRejectReason;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    if (!shouldAcceptDecodeEntryByUserFilters(entry,
                                              userFilterConfig,
                                              true,
                                              &userFilterRejectReason)) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("Wait/Pounce filter reject (%1): %2")
                      .arg(userFilterRejectReason, message));
        return false;
    }

    QString const key = decodiumBridgeMirrorEntryKey(entry);
    if (!key.isEmpty()) {
        for (QVariant const& value : previousEntries) {
            if (decodiumBridgeMirrorEntryKey(value.toMap()) == key) {
                return false;
            }
        }
    }

    int const decodeSeconds = secondsFromUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
    if (decodeSeconds >= 0) {
        int const nowSeconds =
            QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;
        int const periodMs = periodMsForMode(entry.value(QStringLiteral("mode"), m_mode).toString());
        int const freshnessWindowSeconds = qMax(90, (periodMs > 0 ? periodMs / 1000 : 15) * 4);
        int diff = std::abs(nowSeconds - decodeSeconds);
        diff = qMin(diff, 86400 - diff);
        if (diff > freshnessWindowSeconds) {
            return false;
        }
    }

    QString fromCall = entry.value(QStringLiteral("fromCall")).toString().trimmed().toUpper();
    if (fromCall.isEmpty()) {
        fromCall = extractDecodedCallsign(message, true).trimmed().toUpper();
    }
    QString const baseCall = Radio::base_callsign(fromCall).trimmed().toUpper();
    QString const myBase = normalizedBaseCall(m_callsign.trimmed().toUpper());
    if (baseCall.isEmpty() || baseCall == myBase) {
        return false;
    }

    QString const mode = entry.value(QStringLiteral("mode"), m_mode).toString();
    if (isRecentAutoCqDuplicate(baseCall, m_frequency, mode)) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    QString const throttleKey = key.isEmpty()
        ? QStringLiteral("%1|%2").arg(baseCall, message)
        : key;
    if (throttleKey == m_lastWaitPounceKey
        && m_lastWaitPounceMs > 0
        && nowMs - m_lastWaitPounceMs < 60000) {
        return false;
    }

    m_lastWaitPounceKey = throttleKey;
    m_lastWaitPounceMs = nowMs;

    decodiumBridgeDecodeFlowLog(QStringLiteral("Wait & Pounce trigger source=%1 call=%2 msg='%3'")
                  .arg(source, baseCall, message));
    emit statusMessage(QStringLiteral("Wait & Pounce: risposta a %1").arg(baseCall));

    if (passiveLabWaitPounce && !m_txEnabled) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("Wait & Pounce passive lab: CQ decode accepted, processDecodeDoubleClick will arm reply call=%1").arg(baseCall));
    }
    processDecodeDoubleClick(message,
                             entry.value(QStringLiteral("time")).toString(),
                             entry.value(QStringLiteral("db")).toString(),
                             entry.value(QStringLiteral("freq")).toString().toInt());
    return true;
}
