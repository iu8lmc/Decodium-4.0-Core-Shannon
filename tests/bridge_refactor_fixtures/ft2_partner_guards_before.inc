void DecodiumBridge::clearAutoCqPartnerLock()
{
    m_autoCqLockedCall.clear();
    m_autoCqLockedGrid.clear();
    m_autoCqLockedNtx = 6;
    m_autoCqLockedProgress = 1;
    m_ft2AutoCqAwaitingPartnerTx = 0;
    m_ft2AutoCqAwaitingPartnerBase.clear();
    m_ft2AutoCqAwaitingPartnerDecodeIdentity.clear();
    m_ft2AutoCqAwaitingPartnerSinceMs = 0;
}

bool DecodiumBridge::ft2AutoCqAwaitingPartnerDecode() const
{
    if (m_mode != QStringLiteral("FT2")
        || !m_asyncTxEnabled
        || !m_autoCqRepeat
        || m_ft2AutoCqAwaitingPartnerTx <= 0
        || m_pendingAutoSeqTxAfterActiveTx > 0) {
        return false;
    }
    if (m_currentTx != m_ft2AutoCqAwaitingPartnerTx) {
        return false;
    }

    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    QString const activeBase = Radio::base_callsign(activePartner).trimmed().toUpper();
    return !activeBase.isEmpty()
        && activeBase == m_ft2AutoCqAwaitingPartnerBase;
}

bool DecodiumBridge::isDirectedToLocalHashFromActivePartner(const QString& message,
                                                            QString* partnerOut) const
{
    if (partnerOut) {
        partnerOut->clear();
    }

    QString const msg = message.trimmed();
    if (msg.isEmpty()) {
        return false;
    }

    // CASO 1: messaggio di tipo 4 con l'hash GIA' RISOLTO dal decoder, cioe'
    // "<IU8LMC> II8IHBC". Qui il destinatario e' scritto: non serve indovinarlo
    // dal QSO in corso come nel caso 2, basta confrontarlo col proprio
    // nominativo, ed e' un controllo piu' stringente di qualunque euristica.
    //
    // Senza questo ramo il messaggio arrivava alla lista ma non al sequencer:
    // l'operatore vedeva la stazione chiamarlo e il contatto non si chiudeva.
    // Due erano gli ostacoli, entrambi invisibili: i due soli token (il
    // controllo sotto ne pretende tre) e il fatto che i token normalizzati
    // hanno gia' perso le parentesi angolari, quindi cercarvi un hash e'
    // inutile. Segnalato in aria da IU8LMC il 12/9/2026 su II8IHBC.
    {
        QString destinatario;
        QString mittente;
        if (decodium::seq::splitNonStandardDirected(msg, &destinatario, &mittente)) {
            QString const myBase = normalizedBaseCall(m_callsign.trimmed().toUpper());
            QString const destBase = normalizedBaseCall(destinatario);
            if (!myBase.isEmpty() && destBase == myBase
                && isPlausibleDecodedCallsign(mittente)
                && !hasHighConfidenceGhostPrefix(mittente)) {
                if (partnerOut) {
                    *partnerOut = normalizedUsableCallToken(mittente);
                }
                return true;
            }
            // Tipo 4 diretto a qualcun altro: non e' roba nostra.
            return false;
        }
    }

    // CASO 2: hash NON risolto ("<...>"). Qui non c'e' piu' niente da dedurre,
    // e non si deve dedurre: la prova l'ha gia' fatta il decoder.
    //
    // Nei messaggi di tipo 4 il destinatario viaggia come hash a 12 bit. In
    // unpack77 (FtxMessageEncoder.cpp) quell'hash viene confrontato con quello
    // del NOSTRO nominativo e, se coincide, il testo esce col nominativo
    // scritto: "<IT9MRM> 8Z96ND RR73". FT8, FT4 e FT2 passano tutti e tre da
    // quel decode77 con il nostro nominativo nel contesto.
    //
    // Se quindi leggiamo ancora "<...>", l'hash non era il nostro: il messaggio
    // e' per un altro. Segnalato in aria il 22/09/2026 da IT9MRM mentre
    // lavorava 8Z96ND (nominativo non standard): il QSO veniva chiuso sulla
    // risposta che 8Z96ND dava a una terza stazione. Con un corrispondente non
    // standard TUTTI i suoi messaggi hanno quella forma, quindi dedurre il
    // destinatario dal QSO in corso non poteva che sbagliare, prima o poi.
    return false;
}

bool DecodiumBridge::isDirectedActivePartnerSignoffDecode(const QStringList& fields) const
{
    if (fields.size() < 5) {
        return false;
    }

    QString const msg = fields.value(4).trimmed();
    if (msg.isEmpty()) {
        return false;
    }

    QStringList const parts = msg.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    QString const last = parts.last().trimmed().toUpper();
    bool const signoff =
        last == QStringLiteral("73")
        || last == QStringLiteral("RR73")
        || last == QStringLiteral("RRR");
    if (!signoff) {
        return false;
    }

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    QString localHashPartner;
    bool const directedToLocalHash =
        !myCallUpper.isEmpty()
        && isDirectedToLocalHashFromActivePartner(msg, &localHashPartner);
    bool const directedToLiteralCall =
        !myCallUpper.isEmpty()
        && messageContainsCallToken(msg, myCallUpper, myBaseUpper);
    if (!directedToLiteralCall && !directedToLocalHash) {
        return false;
    }

    QString const peer = directedToLocalHash
        ? localHashPartner
        : normalizedUsableCallToken(directedPeerTokenFromMessage(msg, myCallUpper, myBaseUpper));
    QString const peerBase = normalizedBaseCall(peer);
    if (peer.isEmpty() || peerBase.isEmpty() || peerBase == myBaseUpper) {
        return false;
    }

    auto const peerMatchesCall = [&peer, &peerBase](QString const& call) {
        QString const normalized = normalizeCallToken(call).trimmed().toUpper();
        QString const callBase = normalizedBaseCall(normalized);
        return (!normalized.isEmpty() && normalized == peer)
            || (!callBase.isEmpty() && callBase == peerBase);
    };

    bool const matchesAwaitingFt2Partner =
        !m_ft2AutoCqAwaitingPartnerBase.isEmpty()
        && peerBase == m_ft2AutoCqAwaitingPartnerBase;
    bool const knownExchangePartner =
        peerMatchesCall(m_dxCall)
        || peerMatchesCall(m_autoCqLockedCall)
        || peerMatchesCall(m_resumeTargetCall)
        || peerMatchesCall(m_pendingAutoSeqPartnerBase)
        || peerMatchesCall(inferredPartnerForAutolog())
        || matchesAwaitingFt2Partner
        || messageContainsCallToken(buildCurrentTxMessage(), peer, peerBase)
        || messageContainsCallToken(m_lastTransmittedMessage, peer, peerBase)
        || messageContainsCallToken(m_pendingAutoSeqMessage, peer, peerBase);
    if (!knownExchangePartner) {
        return false;
    }

    return m_qsoProgress > 1
        || m_currentTx != 6
        || m_pendingAutoSeqTxAfterActiveTx > 0
        || matchesAwaitingFt2Partner;
}

bool DecodiumBridge::signoffDecodeWatchActive() const
{
    QString const modeKey = m_mode.trimmed().toUpper();
    if (modeKey != QStringLiteral("FT2")
        && modeKey != QStringLiteral("FT4")
        && modeKey != QStringLiteral("FT8")) {
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
    if (activeBase.isEmpty()) {
        return false;
    }

    bool const lastSignoffForPartner =
        messageCarries73PayloadForCall(m_lastTransmittedMessage, activePartner, activeBase);
    bool const signoffStep =
        m_currentTx == 4
        || m_currentTx == 5
        || m_lastNtx == 4
        || m_lastNtx == 5
        || m_qsoProgress >= 5;
    bool const signoffAlreadySent =
        m_nTx73 > 0
        || ((m_lastNtx == 4 || m_lastNtx == 5) && lastSignoffForPartner);

    return m_ft2DeferredLogPending
        || m_logAfterOwn73
        || (signoffStep && signoffAlreadySent);
}

void DecodiumBridge::armFt2AutoCqAwaitingPartnerDecode(int txNum, const QString& reason)
{
    if (m_mode != QStringLiteral("FT2")
        || !m_asyncTxEnabled
        || !m_autoCqRepeat
        || txNum < 2
        || txNum > 4) {
        return;
    }

    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    QString const activeBase = Radio::base_callsign(activePartner).trimmed().toUpper();
    if (activeBase.isEmpty()) {
        return;
    }

    bool const changed = m_ft2AutoCqAwaitingPartnerTx != txNum
        || m_ft2AutoCqAwaitingPartnerBase != activeBase;
    m_ft2AutoCqAwaitingPartnerTx = txNum;
    m_ft2AutoCqAwaitingPartnerBase = activeBase;
    m_ft2AutoCqAwaitingPartnerDecodeIdentity = m_lastAutoSeqDecodeIdentity;
    m_ft2AutoCqAwaitingPartnerSinceMs = QDateTime::currentMSecsSinceEpoch();
    if (changed) {
        bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: TX%1 for %2, wait fresh partner decode (%3)")
                      .arg(txNum)
                      .arg(activeBase, reason));
    }
}

void DecodiumBridge::clearFt2AutoCqAwaitingPartnerDecode(const QString& reason)
{
    if (m_ft2AutoCqAwaitingPartnerTx <= 0
        && m_ft2AutoCqAwaitingPartnerBase.isEmpty()) {
        return;
    }

    bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: release wait TX%1 partner=%2 (%3)")
                  .arg(m_ft2AutoCqAwaitingPartnerTx)
                  .arg(m_ft2AutoCqAwaitingPartnerBase.isEmpty()
                           ? QStringLiteral("<none>")
                           : m_ft2AutoCqAwaitingPartnerBase,
                       reason));
    m_ft2AutoCqAwaitingPartnerTx = 0;
    m_ft2AutoCqAwaitingPartnerBase.clear();
    m_ft2AutoCqAwaitingPartnerDecodeIdentity.clear();
    m_ft2AutoCqAwaitingPartnerSinceMs = 0;
}
