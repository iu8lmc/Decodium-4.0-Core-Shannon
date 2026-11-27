void DecodiumBridge::mamLogSlot(MamQsoSlot& s)
{
    if (s.logged) {
        s.state = MamQsoSlot::State::Done;
        return;
    }
    QString const logCall = s.callFull.trimmed().isEmpty() ? s.call : s.callFull;
    if (logCall.trimmed().isEmpty()) {
        s.state = MamQsoSlot::State::Done;
        s.logged = true;
        return;
    }
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    QDateTime const onUtc = s.startedOnMs > 0
        ? QDateTime::fromMSecsSinceEpoch(s.startedOnMs, Qt::UTC)
        : nowUtc;
    m_pendingAutoLogValid = true;
    m_pendingAutoLogCall = logCall;
    m_pendingAutoLogGrid = s.grid.trimmed();
    m_pendingAutoLogRptSent = s.reportSent.trimmed();
    m_pendingAutoLogRptRcvd = s.reportReceived.trimmed();
    m_pendingAutoLogOn = onUtc;
    m_pendingAutoLogOff = (nowUtc < onUtc) ? onUtc : nowUtc;
    m_pendingAutoLogDialFreq = m_frequency;
    m_qsoLogged = false;
    logQsoNow();
    clearPendingAutoLogSnapshot();
    s.logged = true;
    s.state = MamQsoSlot::State::Done;
    m_qsoCooldown[s.call] = QDateTime::currentMSecsSinceEpoch();
    rememberRecentAutoCqWorked(s.call, m_frequency, m_mode);
    bridgeLog(QStringLiteral("MAM slot %1 logged (rptSent=%2 rptRcvd=%3)")
                  .arg(logCall, s.reportSent, s.reportReceived));
}

