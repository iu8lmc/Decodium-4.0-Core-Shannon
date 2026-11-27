#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumLogging.hpp"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::isPlaceholderCallToken;
using decodium::seq::normalizedBaseCall;
QString decodiumBridgeDiagnosticOneLine(QString);
#define bridgeDiagnosticOneLine decodiumBridgeDiagnosticOneLine

QString DecodiumBridge::dxCall() const { return m_dxCall; }
void DecodiumBridge::setDxCall(const QString& v) {
    QString next = v.trimmed();
    if (isPlaceholderCallToken(next)) {
        DIAG_WARN(QStringLiteral("setDxCall ignored unresolved placeholder callsign: %1")
                      .arg(bridgeDiagnosticOneLine(v)));
        next.clear();
    }
    if (m_dxCall != next) {
        m_qsoPartnerFreqHz = 0;
        QString const previousDx = m_dxCall.trimmed();
        QString const previousBase = Radio::base_callsign(previousDx).trimmed().toUpper();
        QString const nextBase = Radio::base_callsign(next).trimmed().toUpper();
        if (previousBase != nextBase && m_pendingAutoSeqTxAfterActiveTx > 0) {
            bool const pendingStillCurrent =
                !nextBase.isEmpty()
                && !m_pendingAutoSeqPartnerBase.trimmed().isEmpty()
                && m_pendingAutoSeqPartnerBase.trimmed().toUpper() == nextBase;
            if (!pendingStillCurrent) {
                clearPendingAutoSeqTx(QStringLiteral("dxCall changed %1->%2")
                                          .arg(previousBase.isEmpty() ? QStringLiteral("<none>") : previousBase,
                                               nextBase.isEmpty() ? QStringLiteral("<none>") : nextBase));
            }
        }
        if (previousBase != nextBase && m_ft2AutoCqAwaitingPartnerTx > 0) {
            clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("dxCall changed %1->%2")
                                                    .arg(previousBase.isEmpty() ? QStringLiteral("<none>") : previousBase,
                                                         nextBase.isEmpty() ? QStringLiteral("<none>") : nextBase));
        }
        if (!m_ft2PartnerMemoryEnabled && !m_partnerMemory.isEmpty()) {
            m_partnerMemory.clear();
        }
        m_dxCall = next;
        // 1.0.187 — FT2 Weak-Signal Pack F v2: tenta resume da partner-memory
        // PRIMA del reset SNR. Se cache colpisce, ripristina qsoProgress+SNR
        // (NON il currentTx, lasciamo decidere al sequencer). Gate strettissimo
        // dentro tryResumeFromPartnerMemoryV2; log immediato di ogni decision.
        bool const resumedFromMemory =
            m_ft2PartnerMemoryEnabled && !next.isEmpty() && tryResumeFromPartnerMemoryV2(next);
        // 1.0.187 — FT2 Pack G: reset cap re-send TX2 al cambio partner
        m_ft2Tx2ResendsThisQso = 0;
        m_ft2LastForcedTxBefore = 0;
        // 1.0.174 — Reset SNR tracking partner: il vecchio valore non e' piu'
        // significativo. Verra' ri-popolato dal prossimo decode del nuovo
        // partner in autoSequenceStep. 127 = sentinel "no data".
        if (!resumedFromMemory) {
            m_currentPartnerSnrDb = 127;
        }
        if (previousBase != nextBase) {
            m_nextLogName.clear();
            m_nextLogQth.clear();
        }
        QString const activeQueueCall = normalizedBaseCall(next);
        if (!activeQueueCall.isEmpty()) {
            clearWorldMapClosedQso(next);
        }
        // Cambio partner: resetta l'ultimo messaggio trasmesso, altrimenti il
        // controllo `messageCarries73Payload(m_lastTransmittedMessage)` in
        // autoSequenceStep ricorderebbe il "73" del QSO precedente e marcherebbe
        // localSignoffAlreadySent=true al primo decode del nuovo partner,
        // facendo saltare l'invio del nostro signoff e loggando un QSO
        // inesistente. Pulizia simmetrica al reset di setDxCall("").
        m_lastTransmittedMessage.clear();
        emit dxCallChanged();
        // 1.0.134: ri-popola matchesDxCall sulle decode già in lista quando
        // l'utente cambia DxCall (evita di dover re-decodificare). Il QML
        // legge SOLO modelData.matchesDxCall — niente funzioni live che
        // accedono a bridge.dxCall (= regressione 1.0.132 risolta).
        {
            QString const dxTrim = m_dxCall.trimmed().toUpper();
            bool anyChanged = false;
            for (int i = 0; i < m_decodeList.size(); ++i) {
                QVariantMap m = m_decodeList[i].toMap();
                QString const rc = m.value(QStringLiteral("dxCallsign")).toString().toUpper();
                QString const fc = m.value(QStringLiteral("fromCall")).toString().toUpper();
                bool const matches = !dxTrim.isEmpty()
                    && ((!rc.isEmpty() && rc == dxTrim)
                        || (!fc.isEmpty() && fc == dxTrim));
                if (m.value(QStringLiteral("matchesDxCall")).toBool() != matches) {
                    m[QStringLiteral("matchesDxCall")] = matches;
                    m_decodeList[i] = m;
                    anyChanged = true;
                }
            }
            if (anyChanged) emit decodeListChanged();
        }
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setDxCall(next);
        }
        regenerateTxMessages();
        if ((m_multiAnswerMode || m_autoCqRepeat) && !activeQueueCall.isEmpty()) {
            removeCallerFromQueue(activeQueueCall);
        }
        emitCurrentWorldMapQsoPath();
    }
}

#undef bridgeDiagnosticOneLine
