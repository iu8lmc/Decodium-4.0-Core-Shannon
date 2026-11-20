#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Radio.hpp"
#include "CallsignIntelligenceService.h"
#include "DxccLookup.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumQrzLogbookLite.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>

Radio::Frequency decodiumBridgeParseFrequencyHzValue(const QVariant&, bool*);
void decodiumBridgeDecodeFlowLog(const QString&);
void decodiumBridgeStartLogUiStallProbe(QObject*, const QString&);
void decodiumBridgePruneDuplicateCache(QHash<QString, QDateTime>&, const QDateTime&);
bool decodiumBridge_isWithinRecentDuplicateWindow(const QDateTime&, const QDateTime&);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define pruneRecentDuplicateCache decodiumBridgePruneDuplicateCache
#define isWithinRecentDuplicateWindow decodiumBridge_isWithinRecentDuplicateWindow

QString bridgeAdifRecordText(const QString& dxCall, const QString& dxGrid,
                                    double freqHz, const QString& mode,
                                    const QDateTime& timeOnUtc,
                                    const QDateTime& timeOffUtc,
                                    const QString& rstSent, const QString& rstRcvd,
                                    const QString& myCall, const QString& myGrid,
                                    const QString& comments = QString(),
                                    const QString& propMode = QString(),
                                    const QString& satellite = QString(),
                                    const QString& satMode = QString(),
                                    const QString& freqRx = QString(),
                                    const QString& name = QString(),
                                    const QString& qth = QString(),
                                    int cqZone = 0,
                                    int ituZone = 0);

void DecodiumBridge::logQsoNow()
{
    QElapsedTimer logTimer;
    logTimer.start();
    qint64 lastTraceMs = 0;
    auto traceLogStep = [&logTimer, &lastTraceMs](const QString& step) {
        qint64 const now = logTimer.elapsed();
        bridgeLog(QStringLiteral("logQsoNow timing: %1 delta=%2ms total=%3ms")
                      .arg(step)
                      .arg(now - lastTraceMs)
                      .arg(now));
        lastTraceMs = now;
    };

    m_logPromptOpen = false;
    bool const clusterSpotOverrideValid = m_nextLogClusterSpotOverrideValid;
    bool const clusterSpotRequested = clusterSpotOverrideValid
        ? m_nextLogClusterSpotEnabled
        : m_autoSpotEnabled;
    clearNextLogClusterSpotOverride();
    auto completedLogCallForStateClear = [this]() -> QString {
        if (m_promptLogSnapshotValid && !m_promptLogCall.trimmed().isEmpty()) {
            return m_promptLogCall.trimmed();
        }
        if (m_pendingAutoLogValid && !m_pendingAutoLogCall.trimmed().isEmpty()) {
            return m_pendingAutoLogCall.trimmed();
        }
        if (m_lateAutoLogValid && !m_lateAutoLogCall.trimmed().isEmpty()) {
            return m_lateAutoLogCall.trimmed();
        }
        return m_dxCall.trimmed();
    };

    // Anti-doppio log: previeni log multipli dello stesso QSO
    if (m_qsoLogged) {
        bridgeLog("logQso: skipped (already logged this QSO)");
        clearTxArmedAfterCompletedQso(completedLogCallForStateClear(), QStringLiteral("already-logged"));
        clearPromptLogSnapshot();
        return;
    }

    // Multi-logbook support: D4 must write the active ADIF logbook itself,
    // otherwise the embedded legacy backend always logs to its own fixed file
    // and operators get mixed. Keep the legacy writer as hidden fallback only.
    if (usingLegacyBackendForTx()
        && getSetting(QStringLiteral("UseLegacyBackendLogWriter"), false).toBool()) {
        bridgeLog("logQso: delegating to legacy backend");
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        QString legacyLogPropMode;
        QString legacyLogSatellite;
        QString legacyLogSatMode;
        bool const legacySaveSatellite = getSetting(QStringLiteral("SaveSatellite"), false).toBool();
        bool const legacySaveSatMode = getSetting(QStringLiteral("SaveSatMode"), false).toBool();
        if (legacySaveSatellite) {
            legacyLogSatellite = getSetting(QStringLiteral("Satellite"), QString()).toString().trimmed();
            if (!legacyLogSatellite.isEmpty()) {
                legacyLogPropMode = QStringLiteral("SAT");
                if (legacySaveSatMode) {
                    legacyLogSatMode = getSetting(QStringLiteral("SatMode"), QString()).toString().trimmed();
                }
            }
        }
        QString legacyLogComment = m_promptLogComment;
        bool legacyLogCommentValid = m_promptLogCommentOverrideValid;
        if (!legacyLogCommentValid && getSetting(QStringLiteral("dBReportsToComments"), false).toBool()) {
            legacyLogComment = pendingLogQsoPreview().value(QStringLiteral("comment")).toString().trimmed();
            legacyLogCommentValid = !legacyLogComment.isEmpty();
        }
        m_legacyBackend->setNextLogPromptFields(legacyLogComment,
                                                legacyLogCommentValid,
                                                legacyLogPropMode,
                                                legacyLogSatellite,
                                                legacyLogSatMode,
                                                m_promptLogSnapshotValid);
        QDateTime legacyFallbackTimeOn = effectiveQsoLogTimeOnUtc();
        if (!legacyFallbackTimeOn.isValid()) {
            legacyFallbackTimeOn = QDateTime::currentDateTimeUtc();
        }
        QDateTime legacyTimeOn = m_promptLogOn.isValid()
            ? m_promptLogOn
            : (m_pendingAutoLogOn.isValid()
                   ? m_pendingAutoLogOn
                   : (m_lateAutoLogOn.isValid() ? m_lateAutoLogOn : legacyFallbackTimeOn));
        QDateTime legacyTimeOff = m_promptLogOff.isValid()
            ? m_promptLogOff
            : (m_pendingAutoLogOff.isValid()
                   ? m_pendingAutoLogOff
                   : (m_lateAutoLogOff.isValid() ? m_lateAutoLogOff : legacyTimeOn));
        if (legacyTimeOff < legacyTimeOn) {
            legacyTimeOff = legacyTimeOn;
        }
        m_legacyBackend->setNextLogPromptTimes(legacyTimeOn, legacyTimeOff, legacyTimeOn.isValid());
        m_legacyBackend->setNextLogClusterSpotState(dxClusterConnected(),
                                                    clusterSpotRequested && dxClusterConnected());
        if (m_promptLogSnapshotValid) {
            m_legacyBackend->setNextLogPromptAlreadyAccepted();
        }
        m_legacyBackend->logQso();
        syncLegacyBackendState();
        const QString legacyCompletedCall = completedLogCallForStateClear();
        QString legacyCompletedGrid = m_dxGrid;
        QString legacyCompletedMode = m_mode;
        double legacyCompletedFrequency = m_frequency;
        QDateTime legacyCompletedOn = legacyTimeOn;
        if (m_promptLogSnapshotValid) {
            legacyCompletedGrid = m_promptLogGrid;
            if (!m_promptLogMode.trimmed().isEmpty())
                legacyCompletedMode = m_promptLogMode;
            if (m_promptLogDialFreq > 0.0)
                legacyCompletedFrequency = m_promptLogDialFreq;
        } else if (m_pendingAutoLogValid) {
            if (!m_pendingAutoLogGrid.trimmed().isEmpty())
                legacyCompletedGrid = m_pendingAutoLogGrid;
            if (m_pendingAutoLogDialFreq > 0.0)
                legacyCompletedFrequency = m_pendingAutoLogDialFreq;
            if (m_pendingAutoLogOn.isValid())
                legacyCompletedOn = m_pendingAutoLogOn;
        } else if (m_lateAutoLogValid) {
            if (!m_lateAutoLogGrid.trimmed().isEmpty())
                legacyCompletedGrid = m_lateAutoLogGrid;
            if (m_lateAutoLogDialFreq > 0.0)
                legacyCompletedFrequency = m_lateAutoLogDialFreq;
            if (m_lateAutoLogOn.isValid())
                legacyCompletedOn = m_lateAutoLogOn;
        }
        const QString legacyQsoDate =
            (legacyCompletedOn.isValid() ? legacyCompletedOn.toUTC()
                                         : QDateTime::currentDateTimeUtc())
                .date().toString(QStringLiteral("yyyyMMdd"));
        m_workedCalls.insert(legacyCompletedCall.trimmed().toUpper());
        appendWorkedQso(legacyCompletedCall, legacyCompletedGrid,
                        static_cast<quint64>(qMax(0.0, legacyCompletedFrequency)),
                        legacyCompletedMode, legacyQsoDate);
        refreshWorkedBeforeDecodeEntriesForCall(legacyCompletedCall);
        QTimer::singleShot(250, this, [this]() {
            emit qsoCountChanged();
            emit workedCountChanged();
        });
        clearPromptLogSnapshot();
        clearPendingAutoLogSnapshot();
        m_qsoLogged = true;
        if (m_callsignIntelligence) {
            m_callsignIntelligence->notifyQsoLogged(legacyCompletedCall);
        }
        clearTxArmedAfterCompletedQso(legacyCompletedCall, QStringLiteral("legacy-log"));
        m_lastLoggedQsoCall = legacyCompletedCall.trimmed().toUpper();
        m_lastLoggedQsoTimestampMs = QDateTime::currentMSecsSinceEpoch();
        if (getSetting(QStringLiteral("SendStationTelemetry"), false).toBool()) {
            QTimer::singleShot(0, this, &DecodiumBridge::sendStationTelemetry);
        }
        emit qsoLogCompleted(legacyCompletedCall);
        emit statusMessage("Log QSO via backend legacy");
        return;
    }

    QString logDxCall = m_dxCall.trimmed();
    QString logDxGrid = m_dxGrid.trimmed();
    QString logRptSent = m_reportSent.trimmed();
    QString logRptRcvd = m_reportReceived.trimmed();
    QString logMode = m_mode.trimmed();
    // Un QSO registrato dal modulo CW porta il suo modo, una volta sola.
    if (!m_logModeOverride.isEmpty()) {
        logMode = m_logModeOverride;
        m_logModeOverride.clear();
    }
    double  logFreqHz = m_frequency;
    QDateTime utcOn = effectiveQsoLogTimeOnUtc();
    if (!utcOn.isValid()) {
        utcOn = QDateTime::currentDateTimeUtc();
    }
    QDateTime utcOff = utcOn;

    if (m_promptLogSnapshotValid) {
        logDxCall = m_promptLogCall;
        logDxGrid = m_promptLogGrid;
        logRptSent = m_promptLogRptSent;
        logRptRcvd = m_promptLogRptRcvd;
        if (!m_promptLogMode.isEmpty()) logMode = m_promptLogMode;
        if (m_promptLogDialFreq > 0.0) logFreqHz = m_promptLogDialFreq;
        if (m_promptLogOn.isValid()) utcOn = m_promptLogOn;
        if (m_promptLogOff.isValid()) utcOff = m_promptLogOff;
    } else if (m_pendingAutoLogValid) {
        if (!m_pendingAutoLogCall.isEmpty()) logDxCall = m_pendingAutoLogCall;
        if (!m_pendingAutoLogGrid.isEmpty()) logDxGrid = m_pendingAutoLogGrid;
        if (!m_pendingAutoLogRptSent.isEmpty()) logRptSent = m_pendingAutoLogRptSent;
        if (!m_pendingAutoLogRptRcvd.isEmpty()) logRptRcvd = m_pendingAutoLogRptRcvd;
        if (m_pendingAutoLogDialFreq > 0.0) logFreqHz = m_pendingAutoLogDialFreq;
        if (m_pendingAutoLogOn.isValid()) utcOn = m_pendingAutoLogOn;
        if (m_pendingAutoLogOff.isValid()) utcOff = m_pendingAutoLogOff;
    }

    if (logDxCall.isEmpty() && m_lateAutoLogValid && m_lateAutoLogExpires >= QDateTime::currentDateTimeUtc()) {
        logDxCall = m_lateAutoLogCall;
        logDxGrid = m_lateAutoLogGrid;
        logRptSent = m_lateAutoLogRptSent;
        logRptRcvd = m_lateAutoLogRptRcvd;
        if (m_lateAutoLogDialFreq > 0.0) logFreqHz = m_lateAutoLogDialFreq;
        if (m_lateAutoLogOn.isValid()) utcOn = m_lateAutoLogOn;
        if (m_lateAutoLogOff.isValid()) utcOff = m_lateAutoLogOff;
    }

    if (logRptSent.isEmpty()) {
        logRptSent = decodium::seq::signalReportFromMessage(m_lastTransmittedMessage);
    }
    if (logRptSent.isEmpty()) {
        logRptSent = decodium::seq::signalReportFromMessage(buildCurrentTxMessage());
    }

    if (!utcOn.isValid()) {
        utcOn = QDateTime::currentDateTimeUtc();
    }
    if (!utcOff.isValid()) {
        utcOff = utcOn;
    }
    if (utcOff < utcOn) {
        utcOff = utcOn;
    }

    if (logDxCall.isEmpty()) {
        clearPromptLogSnapshot();
        traceLogStep(QStringLiteral("empty-call"));
        return;
    }
    traceLogStep(QStringLiteral("snapshot-ready"));
    decodiumBridgeStartLogUiStallProbe(this, logDxCall);

    QString const dedupeCall = Radio::base_callsign(logDxCall).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(logFreqHz).trimmed().toUpper();
    QString const dedupeMode = logMode.trimmed().toUpper();
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    bool suppressDuplicateLog = false;
    if (!dedupeCall.isEmpty() && !dedupeBand.isEmpty() && dedupeBand != QStringLiteral("--") && !dedupeMode.isEmpty()) {
        auto const it = m_recentQsoLogUtcByKey.constFind(dedupeKey);
        if (it != m_recentQsoLogUtcByKey.constEnd()) {
            suppressDuplicateLog = isWithinRecentDuplicateWindow(it.value(), nowUtc);
        }
        if (!suppressDuplicateLog) {
            m_recentQsoLogUtcByKey.insert(dedupeKey, nowUtc);
            // 1.0.731: count it for the per-call limit (reset at the start of each UTC day)
            if (m_qsoCountDay != nowUtc.date()) {
                m_qsoCountByKey.clear();
                m_qsoCountDay = nowUtc.date();
            }
            m_qsoCountByKey[dedupeKey] += 1;
        }
        pruneRecentDuplicateCache(m_recentQsoLogUtcByKey, nowUtc);
    }

    if (suppressDuplicateLog) {
        emit statusMessage(QStringLiteral("Duplicate log suppressed for %1").arg(logDxCall));
        clearTxArmedAfterCompletedQso(logDxCall, QStringLiteral("duplicate-log-suppressed"));
        clearPromptLogSnapshot();
        clearPendingAutoLogSnapshot();
        traceLogStep(QStringLiteral("duplicate-suppressed"));
        return;
    }

    QString logPropMode;
    QString logSatellite;
    QString logSatMode;
    QString logFreqRx;
    QString logComments = m_promptLogCommentOverrideValid
        ? m_promptLogComment
        : defaultLogCommentForQso(logMode, logRptSent, logRptRcvd, logDxGrid);
    bool const saveSatellite = getSetting(QStringLiteral("SaveSatellite"), false).toBool();
    bool const saveSatMode = getSetting(QStringLiteral("SaveSatMode"), false).toBool();
    bool const saveFreqRx = getSetting(QStringLiteral("SaveFreqRx"), false).toBool();
    if (saveSatellite) {
        logSatellite = getSetting(QStringLiteral("Satellite"), QString()).toString().trimmed();
        if (!logSatellite.isEmpty()) {
            logPropMode = QStringLiteral("SAT");
            if (saveSatMode) {
                logSatMode = getSetting(QStringLiteral("SatMode"), QString()).toString().trimmed();
            }
            if (saveFreqRx) {
                bool freqRxOk = false;
                Radio::Frequency const freqRxHz = decodiumBridgeParseFrequencyHzValue(
                    getSetting(QStringLiteral("FreqRx"), QString()), &freqRxOk);
                if (freqRxOk && freqRxHz > 0) {
                    logFreqRx = QString::number(
                        static_cast<double>(freqRxHz) / 1000000.0, 'f', 6);
                } else {
                    bridgeLog(QStringLiteral("QSO log: invalid saved satellite RX frequency omitted"));
                }
            }
        }
    }

    // CQZ/ITUZ are optional ADIF fields. Add them only after a successful
    // cty.dat lookup: emitting a literal zero makes external loggers record
    // an invalid zone rather than leaving an unknown zone unset.
    int logCqZone = 0;
    int logItuZone = 0;
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        DxccEntity const entity = m_dxccLookup->lookup(logDxCall);
        if (entity.cqZone >= 1 && entity.cqZone <= 40) {
            logCqZone = entity.cqZone;
        }
        if (entity.ituZone >= 1 && entity.ituZone <= 90) {
            logItuZone = entity.ituZone;
        }
    }

    // 1) Log custom (all.txt) — compatibilità esistente
    QString logPath = logAllTxtPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    QFile f(logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << utcOn.toString("yyyyMMdd_hhmmss")
          << " " << m_callsign
          << " " << logDxCall
          << " " << logDxGrid
          << " " << QString::number(logFreqHz / 1e6, 'f', 6) << "MHz"
          << " " << logMode
          << "\n";
    }
    traceLogStep(QStringLiteral("alltxt-written"));

    // 2) Log ADIF (decodium_log.adi) — per import/export e B4 check
    QByteArray const adifRecord = bridgeAdifRecordText(logDxCall, logDxGrid, logFreqHz, logMode, utcOn, utcOff,
                                                       logRptSent, logRptRcvd, m_callsign, m_grid,
                                                       logComments, logPropMode, logSatellite, logSatMode, logFreqRx,
                                                       m_nextLogName, m_nextLogQth, logCqZone, logItuZone).toUtf8();
    traceLogStep(QStringLiteral("adif-record-built"));
    if (!appendAdifRecord(logDxCall, logDxGrid, logFreqHz, logMode,
                          utcOn, utcOff, logRptSent, logRptRcvd,
                          logComments, logPropMode, logSatellite, logSatMode,
                          logFreqRx, logCqZone, logItuZone)) {
        if (!dedupeKey.isEmpty()) {
            m_recentQsoLogUtcByKey.remove(dedupeKey);
        }
        emit errorMessage(QStringLiteral(
            "QSO log failed: the active ADIF logbook could not be written"));
        traceLogStep(QStringLiteral("adif-append-failed"));
        return;
    }
    traceLogStep(QStringLiteral("adif-appended"));

    // 3) Inoltro ai log esterni compatibili WSJT-X UDP / N1MM.
    udpSendLoggedQso(logDxCall, logDxGrid, logFreqHz, logMode, utcOn, utcOff,
                     logRptSent, logRptRcvd, adifRecord, logComments,
                     logPropMode, logSatellite, logSatMode, logFreqRx);
    traceLogStep(QStringLiteral("udp-sent"));
    udpSendN1mmLoggedQso(logDxCall, adifRecord);
    traceLogStep(QStringLiteral("n1mm-sent"));
    tcpSendLoggedAdifQso(logDxCall, adifRecord);
    traceLogStep(QStringLiteral("adif-tcp-started"));

    // 4) QRZ Logbook upload
    if (m_qrzLogbookEnabled && m_qrzLogbook) {
        m_qrzLogbook->uploadAdif(logDxCall, adifRecord);
    }
    traceLogStep(QStringLiteral("qrz-logbook-started"));

    // 5) Cloudlog upload
    bool snrOk = false;
    int const snr = logRptSent.toInt(&snrOk);
    if (m_cloudlogEnabled && m_cloudlog) {
        m_cloudlog->setEnabled(m_cloudlogEnabled);
        m_cloudlog->setApiUrl(m_cloudlogUrl);
        m_cloudlog->setApiKey(m_cloudlogApiKey);
        m_cloudlog->setStationId(qBound(0, getSetting(QStringLiteral("CloudLogStationID"), 1).toInt(), 999));
        m_cloudlog->logQso(logDxCall, logDxGrid, logFreqHz, logMode, utcOn,
                           snrOk ? snr : 0, logRptSent, logRptRcvd,
                           m_callsign, m_grid);
    }
    traceLogStep(QStringLiteral("cloudlog-started"));

    if (clusterSpotRequested) {
        if (m_dxCluster && m_dxCluster->connected()) {
            QString const spotCall = !dedupeCall.isEmpty()
                ? dedupeCall
                : Radio::base_callsign(logDxCall).trimmed().toUpper();
            QStringList commentParts;
            QString const cleanMode = logMode.trimmed().toUpper();
            if (!cleanMode.isEmpty()) {
                commentParts << cleanMode;
            }
            if (!logDxGrid.trimmed().isEmpty()) {
                commentParts << logDxGrid.trimmed().toUpper();
            }
            commentParts << QStringLiteral("Decodium");
            m_dxCluster->submitSpotVerified(spotCall,
                                             logFreqHz / 1000.0,
                                             commentParts.join(QLatin1Char(' ')));
        } else {
            emit statusMessage(QStringLiteral("DX Cluster spot non inviato: cluster non connesso"));
        }
    }
    traceLogStep(QStringLiteral("cluster-handled"));

    rememberRecentAutoCqWorked(logDxCall, logFreqHz, logMode);
    removeCallerFromQueue(dedupeCall);
    if (m_lateAutoLogValid &&
        dedupeCall == Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper()) {
        clearLateAutoLogSnapshot();
    }
    clearPromptLogSnapshot();
    clearPendingAutoLogSnapshot();
    m_qsoLogged = true;  // impedisce doppio log per questo QSO
    if (m_callsignIntelligence) {
        m_callsignIntelligence->notifyQsoLogged(logDxCall);
    }
    clearTxArmedAfterCompletedQso(logDxCall, QStringLiteral("log"));
    m_lastLoggedQsoCall = logDxCall.trimmed().toUpper();
    m_lastLoggedQsoTimestampMs = QDateTime::currentMSecsSinceEpoch();
    if (getSetting(QStringLiteral("SendStationTelemetry"), false).toBool()) {
        QTimer::singleShot(0, this, &DecodiumBridge::sendStationTelemetry);
    }
    emit qsoLogCompleted(logDxCall);
    emit statusMessage("QSO loggato: " + logDxCall);
    traceLogStep(QStringLiteral("done"));
}
