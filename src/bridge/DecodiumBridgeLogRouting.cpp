// SPDX-License-Identifier: GPL-3.0-or-later
#include <QString>
#include <QDebug>
#include "DecodiumLogging.hpp"

bool decodiumBridgeQsoJournalWanted(const QString&);
void decodiumBridgeQsoJournal(const QString&);

static void bridgeLog(const QString& msg) {
    DIAG_INFO(msg);
    if (decodiumBridgeQsoJournalWanted(msg)) decodiumBridgeQsoJournal(msg);
    if (msg.contains(QStringLiteral("Ft2Link"))
        || msg.contains(QStringLiteral("FT2-Link"))) {
        // FT2-Link can emit several operational traces per audio callback.
        // Request a writer-thread flush for live tails, never synchronously
        // block the GUI/decode path on a filesystem flush.
        DecodiumLogging::requestDiagnosticFlush();
    }
}

void decodiumBridgeUiActionLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeAudioDeviceInitLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeRtlInitLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeQsoLimitLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeDecodeTxWaitLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeDxClusterLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeSmartFt2Log(const QString& message) { bridgeLog(message); }
void decodiumBridgeSharingConfigLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeAmplifierLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeLegacyAudioStartLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeTuneWatchdogLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeRemoteFrequencyLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeUdpRestartLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeRemoteAudioLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeDecoPortCredentialsLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeDecoPortGatewayLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeRigDiscoveryLog(const QString& message) { bridgeLog(message); }
void decodiumBridgePeriodTimerLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeRttyRigModeLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeTxRecordingLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeHoundTxPrepareLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeQsoFinishLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeFt8GateLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeQsoTransitionLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeDeferredSeqLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeManualHoldLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeCourtesyLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeAutoCqHistoryLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeTxFinishLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeTxAudioTelemetryLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeSatelliteHdLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeTxPrecomputeLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeSpecialOpLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeSyncScheduleLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeLocalUpdateLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeTxSlotLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeMamQueueLog(const QString& message) { bridgeLog(message); }

void decodiumBridgeAdifTcpLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeExternalAdifLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeAdifUdpLog(const QString& message) { bridgeLog(message); }

void decodiumBridgeGhostLog(const QString& message) { bridgeLog(message); }
void decodiumBridgeOverlayLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeKnownCqLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeStartLogUiStallProbe(QObject*, const QString&);

void decodiumBridgeAudioLevelLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeWebServerLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeCpuProfileLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgePartnerMemoryLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeWorkerLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeDecodeBacklogLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeDecodeClearLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeAsyncHistoryLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeCpuPressureLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeSecondaryDecodeLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeStationLog(const QString& message) { bridgeLog(message); }

void decodiumBridgeDecodePersistenceLog(const QString& message) { bridgeLog(message); }

void decodiumBridgeDecodeFlowLog(const QString& message) { bridgeLog(message); }

void decodiumBridgeAutoCqLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeNtpLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeLogbookLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeFtxSettingsLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeCatProfileLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeKeyboardAudioLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeCallerQueueLog(const QString& message)
{
    bridgeLog(message);
}

void decodiumBridgeWindowStateLog(const QString& message)
{
    bridgeLog(message);
}

static void txTimelineLog(const QString& msg) {
    bridgeLog(msg);
}

static void mainThreadTimelineLog(const QString& label,
                                  qint64 elapsedMs,
                                  const QString& details = {})
{
    QString msg = QStringLiteral("[MAIN-TL] %1 elapsed_ms=%2")
        .arg(label)
        .arg(elapsedMs);
    if (!details.trimmed().isEmpty()) {
        msg += QLatin1Char(' ');
        msg += details.trimmed();
    }
    bridgeLog(msg);
    qInfo().noquote() << msg;
}

void decodiumBridgeMainThreadTimelineLog(const QString& label, qint64 elapsedMs, const QString& details)
{
    mainThreadTimelineLog(label, elapsedMs, details);
}


void decodiumBridgeTxPrepareTimelineLog(const QString& message) { txTimelineLog(message); }
