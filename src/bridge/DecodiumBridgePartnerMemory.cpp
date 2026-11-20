#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"

#include <QDateTime>
#include <QSettings>

void decodiumBridgePartnerMemoryLog(const QString& message);

void DecodiumBridge::setResumeQsoOnReply(bool v)
{
    if (m_resumeQsoOnReply == v) return;
    m_resumeQsoOnReply = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("ResumeQsoOnReply"), v);
    if (!v) {
        clearResumeQsoOnReplyArm(QStringLiteral("disabled"));
    }
    emit resumeQsoOnReplyChanged();
    decodiumBridgePartnerMemoryLog(QStringLiteral("[Resume] resume-on-reply %1").arg(v ? "ON" : "OFF"));
}

// 1.0.187 — FT2 Weak-Signal Pack F v2: partner-memory helpers RISCRITTI
// dopo il revert di 1.0.186 (memoria project_186_reverted_partner_memory_bug).
// Differenze chiave dalla versione revertita:
//  - Default OFF (opt-in via Settings), gate strettissimo
//  - Log IMMEDIATO ogni invocazione (anche se guardrail rifiuta) → debug visibile
//  - Chiamati SOLO da setDxCall (resume) e advanceQsoState (remember), MAI da startTx
//  - Resume bloccato se non siamo gia' in un QSO attivo: TX6/CQ e idle non devono ripescare stato
//  - Resume bloccato se m_transmitting (no contesa col TX in corso)
void DecodiumBridge::rememberPartnerStateV2(QString const& tag)
{
    QString const dx = m_dxCall.trimmed().toUpper();
    if (!m_ft2PartnerMemoryEnabled) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] remember skipped (toggle off) tag=%1 dx=%2")
                      .arg(tag).arg(dx));
        return;
    }
    if (!m_ft2Conservative) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] remember skipped (Conservative off) tag=%1 dx=%2")
                      .arg(tag).arg(dx));
        return;
    }
    if (m_mode != QStringLiteral("FT2")) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] remember skipped (mode=%1 not FT2) tag=%2 dx=%3")
                      .arg(m_mode).arg(tag).arg(dx));
        return;
    }
    if (dx.isEmpty()) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] remember skipped (no dxCall) tag=%1").arg(tag));
        return;
    }
    if (m_qsoProgress <= 1) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] remember skipped (qsoProgress=%1 in CQ/idle) tag=%2 dx=%3")
                      .arg(m_qsoProgress).arg(tag).arg(dx));
        return;
    }

    PartnerMemoryEntry& entry = m_partnerMemory[dx];
    entry.callUpper     = dx;
    entry.lastTxNum     = m_currentTx;
    entry.qsoProgress   = m_qsoProgress;
    entry.lastSnrDb     = m_currentPartnerSnrDb;
    entry.lastSeenMs    = QDateTime::currentMSecsSinceEpoch();
    entry.lastTxPayload = m_lastTransmittedMessage;
    decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] remember OK tag=%1 dx=%2 tx=%3 progress=%4 snr=%5 entries=%6")
                  .arg(tag).arg(dx)
                  .arg(entry.lastTxNum).arg(entry.qsoProgress)
                  .arg(entry.lastSnrDb).arg(m_partnerMemory.size()));
}

bool DecodiumBridge::tryResumeFromPartnerMemoryV2(QString const& newDxCall)
{
    QString const dx = newDxCall.trimmed().toUpper();
    if (!m_ft2PartnerMemoryEnabled) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (toggle off) candidate=%1").arg(dx));
        return false;
    }
    if (!m_ft2Conservative) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (Conservative off) candidate=%1").arg(dx));
        return false;
    }
    if (m_mode != QStringLiteral("FT2")) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (mode=%1 not FT2) candidate=%2")
                      .arg(m_mode).arg(dx));
        return false;
    }
    if (dx.isEmpty()) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (empty candidate)"));
        return false;
    }
    bool const currentStateCanResume =
        m_qsoProgress > 1
        && m_qsoProgress < 6
        && m_currentTx >= 1
        && m_currentTx <= 5;
    if (!currentStateCanResume) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (not in active QSO state tx=%1 progress=%2) candidate=%3")
                      .arg(m_currentTx)
                      .arg(m_qsoProgress)
                      .arg(dx));
        return false;
    }
    if (m_transmitting) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (transmitting) candidate=%1").arg(dx));
        return false;
    }

    auto it = m_partnerMemory.constFind(dx);
    if (it == m_partnerMemory.constEnd()) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (no entry) candidate=%1 cache=%2 entries")
                      .arg(dx).arg(m_partnerMemory.size()));
        return false;
    }

    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    qint64 const ageMs = now - it->lastSeenMs;
    if (ageMs > kPartnerMemoryWindowMs) {
        m_partnerMemory.remove(dx);
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (entry stale age=%1s) candidate=%2")
                      .arg(ageMs / 1000).arg(dx));
        return false;
    }

    int const resumedTxNum    = it->lastTxNum;
    int const resumedProgress = it->qsoProgress;
    int const resumedSnr      = it->lastSnrDb;
    bool const cachedStateCanResume =
        resumedProgress > 1
        && resumedProgress < 6
        && resumedTxNum >= 1
        && resumedTxNum <= 5;
    if (!cachedStateCanResume) {
        m_partnerMemory.remove(dx);
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume skipped (cached state not active tx=%1 progress=%2) candidate=%3")
                      .arg(resumedTxNum)
                      .arg(resumedProgress)
                      .arg(dx));
        return false;
    }
    if (resumedProgress > 1) {
        m_qsoProgress = resumedProgress;
        emit qsoProgressChanged();
    }
    if (resumedSnr != 127) {
        m_currentPartnerSnrDb = resumedSnr;
    }
    // NOTA: NON chiamiamo setCurrentTx qui. Lasciamo che il sequencer ricalcoli
    // il prossimo TX naturalmente dal nuovo qsoProgress + decode in arrivo.
    // Cambiare m_currentTx forzatamente fu uno degli errori del 186 revertito.
    decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] resume OK candidate=%1 restored progress=%2 snr=%3 age=%4s "
                             "(NOT touching currentTx, sequencer decide)")
                  .arg(dx).arg(resumedProgress).arg(resumedSnr).arg(ageMs / 1000));
    Q_UNUSED(resumedTxNum);
    return true;
}

void DecodiumBridge::prunePartnerMemoryIfDueV2()
{
    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_partnerMemoryLastPruneMs < kPartnerMemoryWindowMs) return;
    m_partnerMemoryLastPruneMs = now;
    int removed = 0;
    for (auto it = m_partnerMemory.begin(); it != m_partnerMemory.end(); ) {
        if (now - it->lastSeenMs > kPartnerMemoryPruneMs) {
            it = m_partnerMemory.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (removed > 0) {
        decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] pruned %1 stale (kept %2)")
                      .arg(removed).arg(m_partnerMemory.size()));
    }
}

void DecodiumBridge::setFt2PartnerMemoryEnabled(bool v)
{
    if (m_ft2PartnerMemoryEnabled == v) return;
    m_ft2PartnerMemoryEnabled = v;
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(settings);
        settings.setValue(QStringLiteral("Ft2PartnerMemoryEnabled"), v);
    }
    if (!v) {
        m_partnerMemory.clear();  // se disattivo, svuota cache
        clearPendingAutoSeqTx(QStringLiteral("PartnerMemory OFF"));
    }
    emit ft2PartnerMemoryEnabledChanged();
    decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-F] PartnerMemory %1").arg(v ? "ON" : "OFF"));
}

// 1.0.187 — FT2 Weak-Signal Pack G: TX2 re-send forzato pre-fallback.
// Quando il sequencer e' in TX3 (R+report) e il partner non ack in 2+ periodi,
// forza un re-send TX2 (signal report) prima di lasciare il QSO. Cap: max 1
// re-send per QSO per evitare loop infiniti. Gated su Conservative+FT2.
// Ritorna true se ha forzato una transizione.
bool DecodiumBridge::maybeForceTx2ResendOnStall()
{
    if (!m_ft2Tx2ResendOnStall) return false;
    if (!m_ft2Conservative) return false;
    if (m_mode != QStringLiteral("FT2")) return false;
    if (m_currentTx != 3) return false;          // siamo in TX3 (R+report)
    if (m_qsoProgress != 4) return false;        // ROGER_REPORT in attesa
    if (m_ft2Tx2ResendsThisQso >= 1) return false;  // cap a 1 re-send per QSO
    // Cap basato su tick: 2 periodi = ~7.5s FT2 (period 3750ms)
    int const stallTicks = m_periodTicksMax * 2;
    if (m_txWatchdogTicks < stallTicks) return false;

    m_ft2Tx2ResendsThisQso = 1;
    m_ft2LastForcedTxBefore = m_currentTx;
    if (!selectCurrentTxIfAllowed(2, QStringLiteral("FT2WS-G forced TX2 resend"))) {
        return false;
    }
    m_qsoProgress = 3;                            // REPORT (re-inviare)
    emit qsoProgressChanged();
    m_txWatchdogTicks = 0;
    decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-G] TX3 stall %1 ticks → forced re-send TX2 "
                             "(1x cap, partner=%2)")
                  .arg(stallTicks).arg(m_dxCall));
    return true;
}

void DecodiumBridge::setFt2Tx2ResendOnStall(bool v)
{
    if (m_ft2Tx2ResendOnStall == v) return;
    m_ft2Tx2ResendOnStall = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2Tx2ResendOnStall"), v);
    emit ft2Tx2ResendOnStallChanged();
    decodiumBridgePartnerMemoryLog(QStringLiteral("[FT2WS-G] TX2ResendOnStall %1").arg(v ? "ON" : "OFF"));
}

