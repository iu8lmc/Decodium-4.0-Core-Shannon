QString DecodiumBridge::buildCurrentTxMessage() const
{
    QString selectedMessage;
    switch (m_currentTx) {
        case 1: selectedMessage = m_tx1; break;
        case 2: selectedMessage = m_tx2; break;
        case 3: selectedMessage = m_tx3; break;
        case 4: selectedMessage = m_tx4; break;
        case 5: selectedMessage = m_tx5; break;
        case 6: selectedMessage = m_tx6; break;
        default: selectedMessage = m_tx1; break;
    }

    if (selectedMessage.trimmed().isEmpty() &&
        m_currentTx >= 1 && m_currentTx <= 5 &&
        m_dxCall.trimmed().isEmpty() &&
        m_specialOperationActivity != kSpecialOpHound &&
        !m_tx6.trimmed().isEmpty()) {
        return m_tx6;
    }

    return selectedMessage;
}

