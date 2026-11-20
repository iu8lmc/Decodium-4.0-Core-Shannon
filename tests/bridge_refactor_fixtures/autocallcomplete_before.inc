void DecodiumBridge::noteAutoCallQsoCompleted()
{
    if (!m_autoCallEnabled || m_autoCallCurrentCandidate.isEmpty()) {
        return;
    }
    const QString completedCall = m_autoCallCurrentCandidate;
    m_autoCallCurrentCandidate.clear();
    ++m_autoCallQsoCount;
    emit autoCallQsoCountChanged();
    bridgeLog(QStringLiteral("[AUTOCALL] completed %1 (%2/%3)")
                  .arg(completedCall)
                  .arg(m_autoCallQsoCount)
                  .arg(m_autoCallMaxQsos == 0 ? QStringLiteral("inf")
                                               : QString::number(m_autoCallMaxQsos)));
    if (m_autoCallMaxQsos > 0 && m_autoCallQsoCount >= m_autoCallMaxQsos) {
        emit statusMessage(QStringLiteral("Auto Call limit reached; reset the counter to continue"));
    }
}

