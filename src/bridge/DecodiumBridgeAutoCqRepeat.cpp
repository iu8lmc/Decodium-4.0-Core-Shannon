#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QCoreApplication>
#include <QTimer>

using decodium::seq::messageCarries73Payload;
void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::setAutoCqRepeat(bool v)
{
    if (v && m_autoCallEnabled) {
        setAutoCallEnabled(false);
    }
    if (m_autoCqRepeat != v) {
        m_autoCqRepeat = v;
        resetAutoCqBurstCadence(m_autoCqRepeat
                                     ? QStringLiteral("AutoCQ enabled")
                                     : QStringLiteral("AutoCQ disabled"));
        if (m_autoCqRepeat) {
            bool const cqOrCompletedState = m_currentTx == 6
                || m_qsoProgress <= 1
                || m_qsoProgress >= 5
                || m_dxCall.trimmed().isEmpty()
                || m_logAfterOwn73
                || m_ft2DeferredLogPending;
            if (cqOrCompletedState) {
                resetCompletedQsoLatchesForCq(QStringLiteral("autocq-enabled"));
            }
            clearManualTxHold(QStringLiteral("autocq-enabled"));
            m_autoCqCycleCount = 0;  // reset contatore cicli
            armCqAutoReplyWindow(QStringLiteral("autocq-enabled"));
        }
        if (m_autoCqRepeat && !m_autoSeq) {
            // AutoCQ must always behave like legacy auto-reply mode.
            m_autoSeq = true;
            emit autoSeqChanged();
            bridgeLog("setAutoCqRepeat: forcing autoSeq=true");
        }
        if (!m_autoCqRepeat) {
            // The legacy FT4/FT8 backend can turn AutoCQ off immediately
            // after the operator's final 73, before its asynchronous
            // end-of-transmission callback reaches noteTxPlaybackFinished().
            // At this point a received RR73 plus a prepared local TX5 is an
            // already complete exchange.  Do not clear its immutable snapshot
            // first: that used to lose the only log opportunity (JA5JQH).
            bool const final73WasStarted =
                m_currentTx == 5
                && m_ft2DeferredLogPending
                && m_pendingAutoLogValid
                && messageCarries73Payload(m_activeTxMessage.isEmpty()
                                                ? m_lastTransmittedMessage
                                                : m_activeTxMessage);
            if (final73WasStarted && !m_qsoLogged) {
                QString completedPartner = inferredPartnerForAutolog();
                if (completedPartner.trimmed().isEmpty()) {
                    completedPartner = m_pendingAutoLogCall;
                }
                QString const reason = QStringLiteral("AutoCQ disabled after final TX5 was started");
                markWorldMapQsoClosed(completedPartner, reason);
                rememberCompletedAutoCqPartner(completedPartner, true, reason);
                bridgeLog(QStringLiteral("AutoCQ disable: logging completed final-TX5 QSO partner=%1")
                              .arg(completedPartner));
                logQso();
                if (m_qsoProgress != 6) {
                    m_qsoProgress = 6;
                    emit qsoProgressChanged();
                }
                setDxCall(QString());
                setDxGrid(QString());
                m_logAfterOwn73 = false;
                m_ft2DeferredLogPending = false;
            }
            invalidateDeferredTxRequests(QStringLiteral("AutoCQ disabled"));
            clearCallerQueue();
            clearAutoCqPartnerLock();
            clearCqAutoReplyWindow(QStringLiteral("autocq-disabled"));
            clearPendingAutoLogSnapshot();
            clearLateAutoLogSnapshot();
            clearPendingAutoSeqTx(QStringLiteral("autocq-disabled"));
            // Keep a running Multi-Answer QSO intact if that mode remains
            // enabled. Otherwise this is the terminal AutoCQ transition and
            // the generated multi-stream payload must be discarded.
            if (!m_multiAnswerMode) {
                clearMamPendingTxPayload(QStringLiteral("autocq-disabled"));
            }
        }
        emit autoCqRepeatChanged();
        // MAM's bridge dispatcher is the authoritative AutoCQ sequencer on
        // macOS. Keeping legacy AutoCQ enabled starts a second mono
        // sequencer, which can transmit only one active MAM stream.
        bool const bridgeOwnsMamTx = m_mamMultiStream
            && isMamMultiStreamMode()
            && shouldUseBridgeAudioForLegacyDigitalTx();
        if (usingLegacyBackendForTx()) {
            syncLegacyBackendTxState();
            m_legacyBackend->setAutoCq(bridgeOwnsMamTx ? false : v);
            applyAutoCqBurstCadenceToLegacyBackend();
        }
        if (m_autoCqRepeat) {
            if (!m_txEnabled) {
                bridgeLog(QStringLiteral("setAutoCqRepeat: arming TX for AutoCQ"));
                setTxEnabled(true);
            } else if (usingLegacyBackendForTx() && !bridgeOwnsMamTx) {
                m_legacyBackend->setTxEnabled(true);
                scheduleLegacyStateRefreshBurst();
            }
            scheduleTxAudioPrecompute(25);
            ensureSyncTxSchedulerActive(QStringLiteral("autocq-enable"));
            if (bridgeOwnsMamTx && m_monitoring && m_periodTimer) {
                // Legacy RX normally stops the bridge period timer. Native
                // MAM still needs that UTC boundary to dispatch its composite
                // FT4/FT8/FT2 waveform, so start it explicitly here.
                quint64 const sessionId = ++m_periodTimerSessionId;
                armPeriodTimerForCurrentMode(sessionId,
                                             QStringLiteral("AutoCQ enable MAM bridge TX"));
            }
            if (!usingLegacyBackendForTx()) {
                quint64 const requestGeneration = m_txRequestGeneration;
                QTimer::singleShot(0, this, [this, requestGeneration]() {
                    if (m_shuttingDown || QCoreApplication::closingDown()
                        || usingLegacyBackendForTx()
                        || requestGeneration != m_txRequestGeneration) {
                        return;
                    }
                    checkAndStartPeriodicTx();
                });
            }
        }
    }
}

#undef bridgeLog
