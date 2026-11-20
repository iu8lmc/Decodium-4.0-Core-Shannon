#include "DecodiumBridge.h"
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);

bool DecodiumBridge::promptToLogEnabled() const
{
    return getSetting(QStringLiteral("PromptToLog"), false).toBool();
}

void DecodiumBridge::promptLogQso()
{
    if (m_qsoLogged) {
        decodiumBridgeAudioLevelLog(QStringLiteral("promptLogQso: skipped (already logged this QSO)"));
        return;
    }

    QVariantMap const preview = pendingLogQsoPreview();
    if (preview.value(QStringLiteral("call")).toString().trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("No QSO to log"));
        return;
    }

    if (!m_promptLogSnapshotValid) {
        capturePromptLogSnapshot(preview);
    }
    if (!m_logPromptOpen) {
        m_logPromptOpen = true;
        decodiumBridgeAudioLevelLog(QStringLiteral("logQso: QML prompt requested for %1").arg(m_promptLogCall));
    }
    m_logPromptShownByQml = false;
    QTimer::singleShot(0, this, [this]() {
        if (!m_mainQmlReady) {
            showLogQsoPromptDialog();
            return;
        }
        emit logQsoPromptRequested();
        // La finestra di conferma appartiene a UNA sola istanza di TxPanel
        // (guardia handleLogPrompt && visible). In certi assetti nessuna e'
        // idonea e la richiesta cadeva nel vuoto: il QSO restava non
        // registrato senza che l'utente vedesse niente. Se nessuno conferma
        // di averla aperta, si usa il dialogo nativo, che non dipende dal
        // layout della finestra principale.
        QTimer::singleShot(500, this, [this]() {
            if (m_logPromptShownByQml || !m_logPromptOpen || m_qsoLogged) {
                return;
            }
            decodiumBridgeAudioLevelLog(QStringLiteral("logQso: nessun pannello ha aperto la conferma, uso il dialogo nativo"));
            showLogQsoPromptDialog();
        });
    });
}

void DecodiumBridge::notifyLogPromptShown()
{
    m_logPromptShownByQml = true;
}

void DecodiumBridge::requestManualLogQso()
{
    // A manual log commits and clears the active QSO state. Do not allow it to
    // race an on-air transmission; automatic logging still uses promptLogQso().
    if (m_transmitting || m_tuning) {
        decodiumBridgeAudioLevelLog(QStringLiteral("manual log ignored while TX is active transmitting=%1 tuning=%2")
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_tuning ? 1 : 0));
        emit statusMessage(QStringLiteral("Finish TX before logging the QSO"));
        return;
    }

    promptLogQso();
}

void DecodiumBridge::logQso()
{
    if (promptToLogEnabled() && !m_qsoLogged) {
        QVariantMap const preview = pendingLogQsoPreview();
        if (!preview.value(QStringLiteral("call")).toString().trimmed().isEmpty()) {
            promptLogQso();
            return;
        }
    }

    logQsoNow();
}

void DecodiumBridge::confirmLogQso()
{
    decodiumBridgeAudioLevelLog(QStringLiteral("logQso: prompt accepted for %1")
                  .arg(m_promptLogSnapshotValid ? m_promptLogCall : m_dxCall));
    m_logPromptOpen = false;
    QTimer::singleShot(0, this, [this]() {
        logQsoNow();
        applyDeferredSpecialOperationActivityIfReady();
    });
}

void DecodiumBridge::rejectPromptedLogQso()
{
    m_logModeOverride.clear();
    if (!m_logPromptOpen) {
        return;
    }

    QString skippedCall = m_dxCall.trimmed();
    if (m_promptLogSnapshotValid && !m_promptLogCall.trimmed().isEmpty()) {
        skippedCall = m_promptLogCall.trimmed();
    } else if (m_pendingAutoLogValid && !m_pendingAutoLogCall.trimmed().isEmpty()) {
        skippedCall = m_pendingAutoLogCall.trimmed();
    } else if (m_lateAutoLogValid && !m_lateAutoLogCall.trimmed().isEmpty()) {
        skippedCall = m_lateAutoLogCall.trimmed();
    }

    m_logPromptOpen = false;
    clearNextLogClusterSpotOverride();
    clearPromptLogSnapshot();
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    m_qsoLogged = true;
    clearTxArmedAfterCompletedQso(skippedCall, QStringLiteral("prompt-skip"));
    applyDeferredSpecialOperationActivityIfReady();
    emit statusMessage(QStringLiteral("Log QSO skipped"));
}

