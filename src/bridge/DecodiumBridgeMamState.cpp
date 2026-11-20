#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include "FtxMessageHelpers.h"
#include "MamTxPayloadPolicy.h"
#include <QSettings>

void decodiumBridgeMamQueueLog(const QString&);

bool DecodiumBridge::multiStreamActive() const
{
    return decodium::mamtx::multiStreamPayloadIsActive(
        mamMultiStreamSequencerActive(), m_mamMessages, m_mamF0sHz);
}

// 1.0.365+ — modi ammessi al MAM multi-stream. FT8 (path sync), FT4 (path
// sync via onFt4DecodeReady->onFt8DecodeReady), FT2 (path async: i decode
// arrivano da onFt2AsyncDecodeReady, il TX dal boundary di onPeriodTimer ->
// checkAndStartPeriodicTx -> mamDispatchPeriod). Default OFF resta byte-
// identico: il gate scatta solo se anche m_mamMultiStream e' true.
bool DecodiumBridge::isMamMultiStreamMode() const
{
    QString const m = m_mode.trimmed().toUpper();
    return m == QStringLiteral("FT8")
        || m == QStringLiteral("FT4")
        || m == QStringLiteral("FT2");
}

void DecodiumBridge::setMamMultiStream(bool on)
{
    if (m_mamMultiStream == on) {
        return;
    }
    m_mamMultiStream = on;
    if (!on) {
        // OFF: smonta tutto lo stato multi-stream. m_mamMessages/m_mamF0sHz
        // vuoti -> multiStreamActive() torna false -> seam TX mono invariato.
        m_mamSlots.clear();
        m_mamLateReportRecoveries.clear();
        clearMamPendingTxPayload(QStringLiteral("multi-stream-disabled"));
    }
    // FASE 3: persisti nello store canonico Decodium3 (come i toggle FT2).
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("MamMultiStream"), on);
    // A running AutoCQ session may have armed legacy AutoCQ before this toggle
    // changed. Reapply it so macOS releases the legacy mono sequencer.
    if (usingLegacyBackendForTx() && legacyBackendAvailable()) {
        m_legacyBackend->setAutoCq(on ? false : m_autoCqRepeat);
        m_legacyBackend->setTxEnabled(on ? false : m_txEnabled);
        scheduleLegacyStateRefreshBurst();
    }
    if (on && m_monitoring && (m_autoCqRepeat || m_multiAnswerMode)) {
        quint64 const sessionId = ++m_periodTimerSessionId;
        armPeriodTimerForCurrentMode(sessionId, QStringLiteral("MAM multi-stream enabled"));
    }
    emit mamMultiStreamChanged();
    emit digiPileupChanged();
    emit mamActiveSlotsChanged();
    decodiumBridgeMamQueueLog(QStringLiteral("MAM multi-stream sequencer toggled: %1").arg(on ? 1 : 0));
}

QString DecodiumBridge::mamBuildSlotMessage(MamQsoSlot& s)
{
    return standardFtxQsoMessage(s.currentTx, s.callFull, m_callsign,
                                 m_grid.left(4), s.reportSent,
                                 m_sendRR73, m_quickQsoEnabled,
                                 m_mode);
}


void DecodiumBridge::setMultiAnswerMode(bool v)
{
    if (v && m_autoCallEnabled) {
        setAutoCallEnabled(false);
    }
    if (m_multiAnswerMode == v) {
        return;
    }

    m_multiAnswerMode = v;
    if (m_multiAnswerMode) {
        clearManualTxHold(QStringLiteral("mam-enabled"));
    } else if (!m_autoCqRepeat) {
        clearCallerQueue();
        clearAutoCqPartnerLock();
        clearPendingAutoLogSnapshot();
        clearLateAutoLogSnapshot();
        clearPendingAutoSeqTx(QStringLiteral("mam-disabled"));
        clearMamPendingTxPayload(QStringLiteral("mam-disabled"));
    }
    decodiumBridgeMamQueueLog(QStringLiteral("MultiAnswerMode: %1").arg(m_multiAnswerMode ? 1 : 0));
    emit multiAnswerModeChanged();
}

