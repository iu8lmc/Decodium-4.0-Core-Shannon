#include "DecodiumBridge.h"
#include <QDateTime>

extern void decodiumBridgeLogbookLog(const QString&);
extern int decodiumBridgeNormalizeWsprPowerDbm(int);
extern QString decodiumBridgeWsprPowerDbmLabel(int);
extern double decodiumBridgeWsprPowerDbmToWatts(int);

bool DecodiumBridge::checkSwrAllowsTransmission(const QString& reason)
{
    if (!getSetting(QStringLiteral("CheckSWR"), false).toBool()) {
        return true;
    }

    // Soglia SWR configurabile (default 2.5), clamp di sicurezza 1.5..5.0.
    double const kSWRStopThreshold =
        qBound(1.5, getSetting(QStringLiteral("SWRStopThreshold"), 2.5).toDouble(), 5.0);
    if (m_rigSwr <= kSWRStopThreshold) {
        return true;
    }

    decodiumBridgeLogbookLog(QStringLiteral("SWR protection blocked TX (%1): swr=%2 threshold=%3")
                  .arg(reason)
                  .arg(m_rigSwr, 0, 'f', 2)
                  .arg(kSWRStopThreshold, 0, 'f', 1));
    emit errorMessage(QStringLiteral("SWR > %1 !!!\n\nTrasmissione bloccata\n\nControlla antenna")
                          .arg(kSWRStopThreshold, 0, 'f', 1));
    emit statusMessage(QStringLiteral("TX bloccato: SWR %1 > %2")
                           .arg(m_rigSwr, 0, 'f', 2).arg(kSWRStopThreshold, 0, 'f', 1));
    return false;
}

void DecodiumBridge::enforceSwrTransmissionLimit(const QString& reason)
{
    if (!getSetting(QStringLiteral("CheckSWR"), false).toBool()) {
        return;
    }

    // Soglia SWR configurabile (default 2.5), clamp di sicurezza 1.5..5.0.
    double const kSWRStopThreshold =
        qBound(1.5, getSetting(QStringLiteral("SWRStopThreshold"), 2.5).toDouble(), 5.0);
    if (m_rigSwr <= kSWRStopThreshold || !m_transmitting) {
        return;
    }

    decodiumBridgeLogbookLog(QStringLiteral("SWR protection stopping TX (%1): swr=%2 threshold=%3")
                  .arg(reason)
                  .arg(m_rigSwr, 0, 'f', 2)
                  .arg(kSWRStopThreshold, 0, 'f', 1));

    if (m_transmitting) {
        stopTx();
    }

    // A stopped Auto Call leaves the selected callsign/QSO progress behind.
    // That stale state used to make the next Start Auto Call fail with
    // "active QSO or transmission", even after the SWR had returned to a
    // safe value. Reset only this automatic path; manual QSOs retain their
    // normal recovery behaviour.
    abortAutoCallAfterSwrStop(QStringLiteral("%1 swr=%2")
                                  .arg(reason)
                                  .arg(m_rigSwr, 0, 'f', 2));

    emit errorMessage(QStringLiteral("SWR > %1 !!!\n\nTrasmissione interrotta\n\nControlla antenna")
                          .arg(kSWRStopThreshold, 0, 'f', 1));
    emit statusMessage(QStringLiteral("TX interrotto: SWR %1 > %2")
                           .arg(m_rigSwr, 0, 'f', 2).arg(kSWRStopThreshold, 0, 'f', 1));
}

void DecodiumBridge::enforceWsprPowerTransmissionLimit(const QString& reason)
{
    if (m_mode.trimmed().compare(QStringLiteral("WSPR"), Qt::CaseInsensitive) != 0
        || !m_transmitting
        || !getSetting(QStringLiteral("PWRandSWR"), false).toBool()
        || m_rigPowerWatts <= 0.05) {
        return;
    }

    double const declaredWatts = decodiumBridgeWsprPowerDbmToWatts(m_wsprPowerDbm);
    double const stopThresholdWatts = qMax(declaredWatts * 2.5, declaredWatts + 5.0);
    if (m_rigPowerWatts <= stopThresholdWatts) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastWsprPowerGuardMs < 1000) {
        return;
    }
    m_lastWsprPowerGuardMs = nowMs;

    decodiumBridgeLogbookLog(QStringLiteral("WSPR power guard stopping TX (%1): rig=%2 W declared=%3 W threshold=%4 W dbm=%5")
                  .arg(reason)
                  .arg(m_rigPowerWatts, 0, 'f', 1)
                  .arg(declaredWatts, 0, 'f', 1)
                  .arg(stopThresholdWatts, 0, 'f', 1)
                  .arg(decodiumBridgeNormalizeWsprPowerDbm(m_wsprPowerDbm)));

    stopTx();
    emit errorMessage(QStringLiteral("WSPR power mismatch: rig %1 W, message %2.\n\nTX interrotta: abbassa la potenza RF del rig o scegli il dBm corretto.")
                          .arg(m_rigPowerWatts, 0, 'f', 1)
                          .arg(decodiumBridgeWsprPowerDbmLabel(m_wsprPowerDbm)));
    emit statusMessage(QStringLiteral("WSPR TX interrotta: PWR %1 W > %2 W")
                           .arg(m_rigPowerWatts, 0, 'f', 1)
                           .arg(stopThresholdWatts, 0, 'f', 1));
}

