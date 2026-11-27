#include "DecodiumBridge.h"
#include "DecodeMessageNormalization.h"
#include "DecodeValidationRules.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>

using namespace decodium::decode_message;
using namespace decodium::decode_validation;
using namespace decodium::seq;
void decodiumBridgeDecodeFlowLog(const QString&);

bool DecodiumBridge::shouldSuppressConflictingDirectedReportDecode(const QStringList& fields,
                                                                   const QString& source,
                                                                   QString* reason)
{
    if (fields.size() < 5) {
        return false;
    }

    QString const message = fields.value(4).trimmed();
    QStringList const words = normalizedMessageTokens(message);
    if (words.size() < 3) {
        return false;
    }

    QString const payload = words.constLast().trimmed().toUpper();
    QString reportKind;
    if (isRogerSignalReportToken(payload)) {
        reportKind = QStringLiteral("R_REPORT");
    } else if (isPlainSignalReportToken(payload)) {
        reportKind = QStringLiteral("REPORT");
    } else {
        return false;
    }

    QString const toCall = normalizeCallToken(words.at(0)).toUpper();
    QString const fromCall = normalizeCallToken(words.at(1)).toUpper();
    if (normalizedUsableCallToken(toCall).isEmpty()
        || normalizedUsableCallToken(fromCall).isEmpty()) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int const periodMs = periodMsForMode(m_mode);
    qint64 const conflictWindowMs = qMax<qint64>(qint64(8000), qint64(qMax(1, periodMs)) * 3 / 4);
    qint64 const purgeMs = qMax<qint64>(qint64(30000), conflictWindowMs * 3);
    for (auto it = m_recentDirectedReportDecodeMs.begin(); it != m_recentDirectedReportDecodeMs.end(); ) {
        if (nowMs - it.value() > purgeMs) {
            m_recentDirectedReportDecodeMessage.remove(it.key());
            it = m_recentDirectedReportDecodeMs.erase(it);
        } else {
            ++it;
        }
    }

    QString const key = m_mode.trimmed().toUpper()
        + QLatin1Char('|') + toCall
        + QLatin1Char('|') + fromCall
        + QLatin1Char('|') + reportKind;
    QString const normalizedMessage = words.join(QLatin1Char(' ')).toUpper();

    auto const previousIt = m_recentDirectedReportDecodeMs.constFind(key);
    if (previousIt != m_recentDirectedReportDecodeMs.cend()
        && nowMs - previousIt.value() <= conflictWindowMs) {
        QString const previousMessage = m_recentDirectedReportDecodeMessage.value(key);
        if (previousMessage != normalizedMessage) {
            if (reason) {
                *reason = QStringLiteral("conflicting %1 for %2->%3 within %4ms: kept [%5], rejected [%6]")
                    .arg(reportKind,
                         fromCall,
                         toCall,
                         QString::number(nowMs - previousIt.value()),
                         previousMessage,
                         normalizedMessage);
            }
            decodiumBridgeDecodeFlowLog(QStringLiteral("decode semantic duplicate suppressed (%1): %2")
                          .arg(source, reason ? *reason : QString()));
            return true;
        }
        m_recentDirectedReportDecodeMs.insert(key, nowMs);
        return false;
    }

    m_recentDirectedReportDecodeMs.insert(key, nowMs);
    m_recentDirectedReportDecodeMessage.insert(key, normalizedMessage);
    return false;
}

bool DecodiumBridge::shouldSuppressRecentLocalTxEchoDecode(const QString& message,
                                                           const QString& source,
                                                           QString* reason) const
{
    if (m_mode.trimmed().toUpper() != QStringLiteral("FT2")) {
        return false;
    }

    QString const canonicalMessage = canonicalDecodeMessage(message);
    QString const canonicalLastTx = canonicalDecodeMessage(m_lastTransmittedMessage);
    if (canonicalMessage.isEmpty()
        || canonicalLastTx.isEmpty()
        || canonicalMessage != canonicalLastTx) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const periodMs = qMax<qint64>(1, periodMsForMode(QStringLiteral("FT2")));
    qint64 const echoWindowMs = qMax<qint64>(15000, periodMs * 4);
    bool const txStillActive = m_transmitting || m_tuning;
    qint64 lastLocalTxMs = m_asyncLastTxEndMs;
    if (m_lastTxActivityUtc.isValid()) {
        lastLocalTxMs = qMax(lastLocalTxMs, m_lastTxActivityUtc.toUTC().toMSecsSinceEpoch());
    }
    bool const txRecentlyEnded =
        lastLocalTxMs > 0
        && nowMs >= lastLocalTxMs
        && nowMs - lastLocalTxMs <= echoWindowMs;
    if (!txStillActive && !txRecentlyEnded) {
        return false;
    }

    if (reason) {
        *reason = txStillActive
            ? QStringLiteral("matches active local TX")
            : QStringLiteral("matches recent local TX %1ms ago").arg(nowMs - lastLocalTxMs);
    }
    decodiumBridgeDecodeFlowLog(QStringLiteral("decode self-echo suppressed (%1): %2%3")
                  .arg(source,
                       canonicalMessage,
                       reason && !reason->isEmpty()
                           ? QStringLiteral(" [") + *reason + QLatin1Char(']')
                           : QString()));
    return true;
}
