#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>
#include <QThread>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeTxPeriodIsEven(int);

void DecodiumBridge::armStationTelemetryForNextSlot(const QString& reason, qint64 forcedDelayMs)
{
    if (!m_stationTelemetryPending) {
        m_stationTelemetryPending = true;
        m_stationTelemetryPendingSinceMs = QDateTime::currentMSecsSinceEpoch();
    }
    // Prossimo inizio di un periodo nostro, +40 ms come i TX sincroni rimandati.
    qint64 delayMs = 1000;
    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (forcedDelayMs >= 0) {
        delayMs = forcedDelayMs;   // il chiamante conosce gia' il prossimo slot utile
    } else if (periodMs > 0) {
        qint64 const msNow = correctedUtcMsecsSinceStartOfDay();
        qint64 const slotIndex = msNow / periodMs;
        qint64 const elapsedMs = msNow % periodMs;
        for (int ahead = 1; ahead <= 3; ++ahead) {
            qint64 const index = slotIndex + ahead;
            bool const even = (index % 2) == 0;
            if (decodiumBridgeTxPeriodIsEven(m_txPeriod) ? even : !even) {
                delayMs = ahead * static_cast<qint64>(periodMs) - elapsedMs + 40;
                break;
            }
        }
    }
    decodiumBridgeAudioLevelLog(QStringLiteral("sendStationTelemetry: rimandata al prossimo slot TX (%1) fra %2 ms")
                  .arg(reason)
                  .arg(delayMs));
    QTimer::singleShot(static_cast<int>(delayMs), this, [this]() {
        if (m_stationTelemetryPending && !m_transmitting) {
            sendStationTelemetry();
        }
    });
}

using decodium::seq::isGridTokenStrict;

void DecodiumBridge::sendStationTelemetry()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "sendStationTelemetry", Qt::QueuedConnection);
        return;
    }
    if (m_tuning || m_manualTxHold) {
        m_stationTelemetryPending = false;
        decodiumBridgeAudioLevelLog(QStringLiteral("sendStationTelemetry: Tune o TX fermato, salto"));
        return;
    }
    if (m_stationTelemetryPending) {
        int const pendingPeriodMs = qMax(1, effectivePeriodMsForMode(m_mode));
        if (QDateTime::currentMSecsSinceEpoch() - m_stationTelemetryPendingSinceMs
            > 4LL * pendingPeriodMs) {
            m_stationTelemetryPending = false;
            decodiumBridgeAudioLevelLog(QStringLiteral("sendStationTelemetry: in attesa da troppo tempo, lasciata cadere"));
            return;
        }
    }
    // Nei modi a slot il messaggio deve partire all'inizio di un periodo
    // nostro. Partito subito dopo il log finiva a meta' slot: il 15/9/2026 in
    // FT2 e' partito a 2,7 s su 3,75 e startTx, allineando l'audio allo slot,
    // ne ha trasmessi 70 ms con il PTT chiuso per un secondo. Durante il 73
    // invece veniva scartato del tutto.
    if (m_transmitting || shouldDeferManualSyncTxStart()) {
        armStationTelemetryForNextSlot(m_transmitting ? QStringLiteral("TX in corso")
                                                      : QStringLiteral("fuori dal suo slot"));
        return;
    }
    // FT2 asincrono: shouldDeferManualSyncTxStart() non lo copre (usesDeferred-
    // ManualSyncTx() e' falso con l'async acceso), quindi la telemetria partiva
    // subito dopo il log, a meta' slot. startTx allinea l'audio allo slot e, a
    // 3,2 s su 3,75, non ne resta niente da trasmettere: il PTT si chiude per
    // circa 0,4 s senza audio, ed e' il ticchettio che si sente fra il log e la
    // ripresa del TX. Stesso gate dell'avvio asincrono: oltre il punto utile
    // dello slot si aspetta il prossimo.
    {
        int elapsedMs = 0;
        int latestStartMs = 0;
        int delayToNextSlotMs = 0;
        if (isFt2AsyncTxStartTooLate(&elapsedMs, &latestStartMs, &delayToNextSlotMs)) {
            armStationTelemetryForNextSlot(
                QStringLiteral("FT2 async: troppo tardi nello slot (%1 ms, limite %2 ms)")
                    .arg(elapsedMs).arg(latestStartMs),
                delayToNextSlotMs);
            return;
        }
    }
    m_stationTelemetryPending = false;
    if (!isGridTokenStrict(m_grid.trimmed().toUpper())) {
        decodiumBridgeAudioLevelLog(QStringLiteral("sendStationTelemetry: locatore non valido, salto"));
        return;
    }

    bool const weatherEnabled = getSetting(QStringLiteral("WeatherApiEnable"), false).toBool();
    qint64 const ageMs = QDateTime::currentMSecsSinceEpoch() - m_weatherFetchedAtMs;
    bool const weatherFresh = weatherEnabled && m_weatherFetchedAtMs > 0 && ageMs < (30 * 60 * 1000);

    decodium::telemetry::StationTelemetryFields fields;
    fields.grid4 = m_grid.trimmed().toUpper().left(4);
    fields.tempC = weatherFresh ? m_weatherTempC : decodium::telemetry::kTempUnknown;
    fields.windSpeedKmh = weatherFresh ? m_weatherWindKmh : 0;
    fields.windDirDeg16 = weatherFresh
        ? decodium::telemetry::windDirDegToIndex16(m_weatherWindDirDeg) : 0;
    fields.sky = weatherFresh ? m_weatherSky : decodium::telemetry::SkyCondition::Unknown;
    fields.powerWatts = m_stationPowerWatts;
    fields.radioModelId = getSetting(QStringLiteral("RadioModelId"), 0).toInt();
    fields.antennaTypeId = getSetting(QStringLiteral("AntennaTypeId"), 0).toInt();

    QString const hex = decodium::telemetry::encodeStationTelemetryHex(fields);

    m_pendingTelemetryHex = hex;
    m_telemetryTxActive = true;

    decodiumBridgeAudioLevelLog(QStringLiteral("sendStationTelemetry: hex=[%1] grid=%2 tempC=%3 wind=%4/%5 power=%6")
                  .arg(hex, fields.grid4)
                  .arg(fields.tempC).arg(fields.windSpeedKmh).arg(fields.windDirDeg16)
                  .arg(fields.powerWatts));

    startTx();

    if (!m_transmitting) {
        // Come sendCwAudio: non lasciare lo stato armato se il TX non e'
        // partito, altrimenti il prossimo TX FT normale verrebbe deviato.
        m_telemetryTxActive = false;
        m_pendingTelemetryHex.clear();
        emit statusMessage(QStringLiteral("Info stazione: avvio TX non riuscito"));
    }
}

