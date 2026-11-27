#include "DecodiumBridge.h"
#include "DecodeValidationRules.h"
#include "SpecialOperationBounds.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QDateTime>

using namespace decodium::seq;
using namespace decodium::decode_validation;
bool decodiumBridgeDecodeTelemetryOnly(QString const&);

bool DecodiumBridge::contestDecodeExchangesEnabled() const
{
    int const activity = qBound(kSpecialOpNone, m_specialOperationActivity, kSpecialOpMax);
    bool const vhfGridContest = activity == 1 || activity == 2;
    return vhfGridContest
        || m_contestType > 0
        || getSetting(QStringLiteral("ContestingOnly"), false).toBool();
}

bool DecodiumBridge::shouldAcceptDecodedMessage(const QString& message,
                                                QString* reason,
                                                bool allowUnresolvedPlaceholder) const
{
    QStringList const tokens = normalizedMessageTokens(message);

    // Il messaggio e' nella forma canonica del tipo 4 ("<IU8LMC> II8IHBC")?
    // La domanda va fatta sul testo GREZZO, perche' i token qui sopra hanno
    // gia' perso le parentesi angolari (normalizeCallToken le rimuove) e un
    // hash e' ormai indistinguibile da un nominativo qualsiasi. La regola sta
    // in MessageTokenRules ed e' coperta dai test: la prima versione di questa
    // correzione interrogava i token normalizzati ed era INERTE.
    bool const hashedCallInMessage = decodium::seq::isNonStandardDirectedForm(message);

    if (tokens.isEmpty()) {
        if (reason) *reason = QStringLiteral("empty message");
        return false;
    }

    // FST4W carries beacon reports (call [grid] power), not a directed
    // two-callsign exchange. Do not treat its grid/power as a peer callsign.
    if (m_mode.trimmed().startsWith(QStringLiteral("FST4W"), Qt::CaseInsensitive)) {
        QStringList const raw = message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool powerOk = false;
        if (tokens.size() == 2 || (tokens.size() == 3 && isGridTokenStrict(tokens.at(1)))) {
            tokens.constLast().toInt(&powerOk);
            bool const placeholder = isPlaceholderCallToken(tokens.constFirst());
            bool const hash = !raw.isEmpty() && isHashedCallToken(raw.constFirst());
            if (powerOk && (isPlausibleDecodedCallsign(tokens.constFirst()) || hash
                            || (allowUnresolvedPlaceholder && placeholder))) {
                return true;
            }
        }
    }

    bool const ft2StrictSemanticGate =
        m_mode.trimmed().compare(QStringLiteral("FT2"), Qt::CaseInsensitive) == 0;

    // FT2 non deve promuovere payload raw/telemetry a decode utente. In
    // pratica il caso visto e' una singola stringa hex lunga a -26 dB che
    // finiva nello storico come se fosse una chiamata.
    if (ft2StrictSemanticGate && decodiumBridgeDecodeTelemetryOnly(message)) {
        if (reason) *reason = QStringLiteral("FT2 standalone telemetry/hex payload (ghost)");
        return false;
    }

    if (ft2StrictSemanticGate
        && tokens.size() == 1
        && tokens.constFirst() != QStringLiteral("CQ")
        && tokens.constFirst() != QStringLiteral("QRZ")) {
        if (reason) *reason = QStringLiteral("FT2 standalone non-QSO token (ghost)");
        return false;
    }

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    bool const syntheticLabCallContext = isSyntheticLabCallsign(myCallUpper);
    if (myCallUpper.isEmpty() && myBaseUpper.isEmpty()) {
        return true;
    }

    auto tokenIsMine = [&](int index) {
        return index >= 0
            && index < tokens.size()
            && tokenMatchesCall(tokens.at(index), myCallUpper, myBaseUpper);
    };

    QString const first = tokens.constFirst();
    if (first == QStringLiteral("CQ") || first == QStringLiteral("QRZ")) {
        for (int i = 1; i < tokens.size(); ++i) {
            if (tokenIsMine(i)) {
                if (reason) *reason = QStringLiteral("self CQ decoded on RX");
                return false;
            }
        }
        // Sanity gate: il call dopo "CQ" (eventualmente con modifier
        // tipo "CQ DX K1JT") deve essere un callsign amateur valido.
        // CQ K1JT FN20 → tokens = [CQ, K1JT, FN20], call=tokens[1]
        // CQ DX K1JT FN20 → tokens = [CQ, DX, K1JT, FN20], call=tokens[2]
        if (tokens.size() >= 2) {
            int callIdx = 1;
            QString const t1 = tokens.at(1);
            if (tokens.size() >= 3 && isCqRoutingModifierToken(t1)) {
                callIdx = 2;
            }
            if (callIdx < tokens.size()
                && !isPlausibleDecodedCallsign(tokens.at(callIdx))
                && !(syntheticLabCallContext && isSyntheticLabCallsign(tokens.at(callIdx)))) {
                if (reason) *reason = QStringLiteral("CQ from invalid callsign (ghost)");
                return false;
            }
            if (ft2StrictSemanticGate && callIdx < tokens.size()) {
                if (!isPlausibleFt2CqCallsign(tokens.at(callIdx))) {
                    if (reason) *reason = QStringLiteral("FT2 CQ from invalid callsign (ghost)");
                    return false;
                }
                QString cqPayloadReason;
                if (!isValidFt2CqTrailingPayload(tokens, callIdx, &cqPayloadReason)) {
                    if (reason) *reason = cqPayloadReason;
                    return false;
                }
            }
        }
        return true;
    }

    bool const directedToOrFromMe = tokenIsMine(0) || tokenIsMine(1);
    bool const directedToLocalHash =
        !directedToOrFromMe
        && isDirectedToLocalHashFromActivePartner(message);
    if (directedToLocalHash) {
        return true;
    }
    if (allowUnresolvedPlaceholder
        && !directedToOrFromMe
        && tokens.size() >= 2
        && (isPlaceholderCallToken(tokens.at(0)) || isPlaceholderCallToken(tokens.at(1)))) {
        int const realCallIdx = isPlaceholderCallToken(tokens.at(0)) ? 1 : 0;
        if (realCallIdx < tokens.size()
            && isPlausibleDecodedCallsign(tokens.at(realCallIdx))
            && !hasHighConfidenceGhostPrefix(tokens.at(realCallIdx))) {
            return true;
        }
    }

    if (!directedToOrFromMe) {
        // Per traffico non diretto a noi, gate stretto: se ALMENO UNO dei
        // due primi token (DX/ME) non e' un callsign plausibile, e' ghost.
        // Il decoder LDPC produce coppie come "ZH7RTG A29TTX/P R KR39":
        // A29TTX/P passerebbe (handler portatile), ZH7RTG non e' allocato
        // ma matcha la regex strutturale, quindi entrambi passerebbero. Qui
        // restiamo conservativi solo sui prefissi impossibili ad alta fiducia:
        // una blocklist ampia tagliava call special-event reali (es. AO...).
        if (tokens.size() >= 2) {
            bool const t0Bad = !isPlausibleDecodedCallsign(tokens.at(0))
                || hasHighConfidenceGhostPrefix(tokens.at(0));
            bool const t1Bad = !isPlausibleDecodedCallsign(tokens.at(1))
                || hasHighConfidenceGhostPrefix(tokens.at(1));
            if (t0Bad || t1Bad) {
                if (reason) *reason = QStringLiteral("source or dest callsign invalid/ghost");
                return false;
            }
        }
        if (ft2StrictSemanticGate) {
            if (tokens.size() < 3) {
                if (reason) *reason = QStringLiteral("FT2 third-party directed message without payload");
                return false;
            }

            QStringList thirdPartyPayload = tokens.mid(2);
            while (!thirdPartyPayload.isEmpty()
                   && thirdPartyPayload.constLast() == QStringLiteral("?")) {
                thirdPartyPayload.removeLast();
            }

            if (thirdPartyPayload.size() > 2) {
                if (reason) *reason = QStringLiteral("FT2 third-party free-text payload (ghost)");
                return false;
            }

            // Firma ricorrente dei falsi positivi FT2: due nominativi
            // portable/rover non diretti a noi seguiti solo da locator
            // (es. "UF7QJY/P BF0OWX/P HJ26"). Il formato e' astrattamente
            // valido, ma in FT2 terza-stazione ha prodotto ghost visibili in
            // UI/LiveMap. Non tocca CQ, messaggi diretti a noi, report o 73.
            if (thirdPartyPayload.size() == 1
                && isGridTokenStrict(thirdPartyPayload.constFirst())
                && hasPortableOperatingDesignator(tokens.at(0))
                && hasPortableOperatingDesignator(tokens.at(1))) {
                if (reason) *reason = QStringLiteral("FT2 third-party portable locator ghost");
                return false;
            }

            QString thirdPartyPayloadReason;
            if (!isDirectedDecodePayloadValid(thirdPartyPayload,
                                              contestDecodeExchangesEnabled(),
                                              &thirdPartyPayloadReason)) {
                if (reason) {
                    *reason = QStringLiteral("FT2 third-party %1").arg(thirdPartyPayloadReason);
                }
                return false;
            }
        }
        return true;
    }

    // Il tipo 4 ("<IU8LMC> II8IHBC") ha DUE soli token ed e' gia' completo:
    // il nominativo non standard per esteso e quello dell'altro come hash.
    // Senza questa eccezione veniva scartato qui, prima ancora del controllo
    // sul payload: 67 chiamate perse in una sola serata.
    bool const hashedPairOnly = tokens.size() == 2 && hashedCallInMessage;
    if (tokens.size() < 3 && !hashedPairOnly) {
        if (reason) *reason = QStringLiteral("directed message without payload");
        return false;
    }

    QStringList payload = tokens.mid(2);
    // 1.0.147: il decoder FT8/FT4/FT2 appende "?" come marker "low-confidence"
    // (es. "IU8LMC M0NXD -12 ?"). Non e' parte del payload semantico, va
    // strippato prima della validation - altrimenti two-token payloads come
    // "-12 ?" vengono rifiutati e il sequencer perde il signal report,
    // restando bloccato su TX1 fino al successivo decode senza "?".
    while (!payload.isEmpty() && payload.constLast() == QStringLiteral("?")) {
        payload.removeLast();
    }

    bool const contestExchanges = contestDecodeExchangesEnabled();
    QString payloadRejectReason;
    bool const payloadValid = isDirectedDecodePayloadValid(payload,
                                                           contestExchanges,
                                                           &payloadRejectReason);

    // Sanity gate: il PEER (l'altro callsign) deve essere un amateur plausibile.
    // Il messaggio e' "<DX> <ME> ...": peer = tokens[0] se ME e' tokens[1],
    // altrimenti peer = tokens[1]. Applica anche il blocco sui prefissi
    // impossibili ad alta fiducia per fermare i ghost LDPC.
    int const peerIdx = tokenIsMine(1) ? 0 : 1;
    QString peerToken = tokens.at(peerIdx);
    bool const peerIsUnresolvedPlaceholder = isPlaceholderCallToken(peerToken);
    if (peerIsUnresolvedPlaceholder) {
        if (!payloadValid) {
            if (reason) *reason = payloadRejectReason;
            return false;
        }
        if (!isResolvablePlaceholderDirectedPayload(payload, contestExchanges)) {
            if (reason) *reason = QStringLiteral("directed placeholder peer unsupported payload");
            return false;
        }

        QString activePartner = m_dxCall.trimmed();
        if (activePartner.isEmpty()) {
            activePartner = m_autoCqLockedCall.trimmed();
        }
        if (activePartner.isEmpty()) {
            activePartner = inferredPartnerForAutolog().trimmed();
        }

        QString const activeBase = normalizedBaseCall(activePartner);
        bool const activePartnerValid =
            !activeBase.isEmpty()
            && isPlausibleDecodedCallsign(activePartner)
            && !hasHighConfidenceGhostPrefix(activePartner);
        bool const activeQsoContext =
            activePartnerValid
            && (m_qsoProgress > 1
                || (m_currentTx >= 1 && m_currentTx <= 5)
                || m_txEnabled
                || m_transmitting
                || messageContainsCallToken(m_lastTransmittedMessage, activePartner, activeBase)
                || messageContainsCallToken(m_pendingAutoSeqMessage, activePartner, activeBase));

        if (!activeQsoContext) {
            if (reason) *reason = QStringLiteral("directed placeholder peer without active QSO context");
            return false;
        }

        peerToken = activePartner;
    } else {
        if (!isPlausibleDecodedCallsign(peerToken)
            && !(syntheticLabCallContext && isSyntheticLabCallsign(peerToken))) {
            if (reason) *reason = QStringLiteral("directed from/to invalid callsign (ghost)");
            return false;
        }
        if (hasHighConfidenceGhostPrefix(peerToken)) {
            if (reason) *reason = QStringLiteral("directed peer has impossible prefix (ghost)");
            return false;
        }
    }

    // 1.0.626: "<IU8LMC> II8IHBC" senza coda NON e' un messaggio monco, e' la
    // forma canonica del tipo 4, quella dei nominativi NON STANDARD (oltre i
    // sei caratteri: prefissi speciali come II8IHBC, portatili come PJ4/K1ABC).
    // Il protocollo manda il nominativo dell'altro come hash fra parentesi
    // angolari e il proprio per esteso, senza rapporto, e significa "il secondo
    // chiama il primo". Il filtro lo rifiutava come "missing directed payload":
    // i CQ di quella stazione passavano, le sue RISPOSTE no, e l'operatore non
    // vedeva chi lo stava chiamando. Segnalato in aria da IU8LMC l'11/9/2026.
    bool const hashedNonStandardForm = payload.isEmpty() && hashedCallInMessage;

    if (!payloadValid && !hashedNonStandardForm) {
        if (reason) *reason = payloadRejectReason;
        return false;
    }

    if (payload.size() == 2) {
        QString const firstPayload = normalizeCallToken(payload.at(0)).toUpper();
        QString const secondPayload = normalizeCallToken(payload.at(1)).toUpper();
        bool const contestRogerGridPayload =
            firstPayload == QStringLiteral("R") && isGridTokenStrict(secondPayload);
        if (contestRogerGridPayload) {
            QString const peerBase = Radio::base_callsign(peerToken).trimmed().toUpper();
            QString const activeBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
            QString const lockedBase = Radio::base_callsign(m_autoCqLockedCall).trimmed().toUpper();
            bool const activePeerMatches =
                !peerBase.isEmpty()
                && ((!activeBase.isEmpty() && peerBase == activeBase)
                    || (!lockedBase.isEmpty() && peerBase == lockedBase));
            bool const lastTxMentionsPeer =
                !peerBase.isEmpty()
                && messageContainsCallToken(m_lastTransmittedMessage, peerToken, peerBase);
            bool const pendingMentionsPeer =
                !peerBase.isEmpty()
                && messageContainsCallToken(m_pendingAutoSeqMessage, peerToken, peerBase);
            bool const txRecentlyAddressedPeer =
                lastTxMentionsPeer
                && m_lastTxActivityUtc.isValid()
                && m_lastTxActivityUtc.secsTo(QDateTime::currentDateTimeUtc()) <= 180;
            bool const activeContestExchangeContext =
                (activePeerMatches && (m_qsoProgress > 1 || m_txEnabled || m_transmitting || m_tuning))
                || txRecentlyAddressedPeer
                || pendingMentionsPeer;
            if (!activeContestExchangeContext) {
                if (reason) {
                    *reason = QStringLiteral("contest R+grid directed payload without active QSO context");
                }
                return false;
            }
        }
    }

    return true;
}

