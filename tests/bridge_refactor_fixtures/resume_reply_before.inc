void DecodiumBridge::clearResumeQsoOnReplyArm(const QString& reason)
{
    bool const hadArm = !m_resumeTargetCall.isEmpty() || m_resumeArmedMs > 0;
    QString const oldTarget = m_resumeTargetCall;
    m_resumeTargetCall.clear();
    m_resumeArmedMs = 0;
    m_resumeArmedMode.clear();
    m_resumeArmedTx = 0;
    m_resumeArmedProgress = 0;
    if (hadArm) {
        bridgeLog(QStringLiteral("[Resume] disarmato (%1) target=%2")
                      .arg(reason,
                           oldTarget.isEmpty() ? QStringLiteral("<none>") : oldTarget));
    }
}

// 1.0.304 (#9) — resume-on-reply. Se armato (Halt con partner, toggle ON) e quel partner
// torna a rispondere a NOI (decode con suo call + mio call) entro 2 min, riprende il QSO via
// processDecodeDoubleClick (stessa via del doppio-click: re-ingaggia + sceglie il TX giusto +
// rilascia il manual hold). Chiamato all'inizio dei path decode, PRIMA dei gate su
// m_manualTxHold. Ritorna true se ha ripreso. Inerte (return rapido) se OFF/non-armato.
bool DecodiumBridge::tryResumeQsoOnReply(const QStringList& rows)
{
    if (!m_resumeQsoOnReply || !m_manualTxHold || m_resumeTargetCall.isEmpty())
        return false;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_resumeArmedMs <= 0 || nowMs - m_resumeArmedMs > 120000) {  // 2 min
        clearResumeQsoOnReplyArm(QStringLiteral("timeout 2 min"));
        return false;
    }
    QString const modeUpper = m_mode.trimmed().toUpper();
    if (m_resumeArmedMode.isEmpty()
        || modeUpper != m_resumeArmedMode
        || m_resumeArmedTx < 1
        || m_resumeArmedTx > 5
        || m_resumeArmedProgress <= 1
        || m_resumeArmedProgress >= 6) {
        clearResumeQsoOnReplyArm(QStringLiteral("stale resume context"));
        return false;
    }
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    if (myCallUpper.isEmpty()) return false;
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    QString const targetBase = m_resumeTargetCall;  // già base-call upper
    for (QString const& row : rows) {
        QStringList f = parseFt8Row(row);
        if (f.size() < 5) continue;
        QString const msg = f[4];
        if (!shouldAcceptDecodedMessage(msg)) continue;
        QStringList const parts = msg.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;
        QString const firstBase = normalizedBaseCall(normalizedUsableCallToken(parts.at(0)));
        QString const secondBase = normalizedBaseCall(normalizedUsableCallToken(parts.at(1)));
        if (firstBase != myBaseUpper || secondBase != targetBase) continue;
        // Deve contenere SIA il partner sospeso SIA il mio call → mi sta rispondendo.
        if (!messageContainsCallToken(msg, targetBase, targetBase)) continue;
        if (!messageContainsCallToken(msg, myCallUpper, myBaseUpper)) continue;
        QString echoReason;
        if (shouldSuppressRecentLocalTxEchoDecode(msg, QStringLiteral("resume-on-reply"), &echoReason))
            continue;
        if (shouldSuppressDirectedGhostDecode(f, QStringLiteral("resume-on-reply")))
            continue;
        bridgeLog(QStringLiteral("[Resume] %1 è tornato a rispondere → riprendo il QSO (msg='%2')")
                      .arg(targetBase, msg));
        clearResumeQsoOnReplyArm(QStringLiteral("matched direct reply"));
        processDecodeDoubleClick(msg, f[0], f[1], f[3].toInt());
        return true;
    }
    return false;
}
