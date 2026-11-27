bool DecodiumBridge::prepareHoundTxSelectionForStart(const QString& reason)
{
    if (m_specialOperationActivity != kSpecialOpHound) {
        return true;
    }

    int const desiredTx = (m_currentTx == 3) ? 3 : 1;
    if (m_currentTx != desiredTx) {
        bridgeLog(QStringLiteral("Hound TX: forcing TX%1 before %2 (requested TX%3)")
                      .arg(desiredTx)
                      .arg(reason)
                      .arg(m_currentTx));
        if (!selectCurrentTxIfAllowed(desiredTx, QStringLiteral("Hound TX prepare"))) {
            return false;
        }
    }

    auto payloadForTx = [this, desiredTx]() {
        return (desiredTx == 3 ? m_tx3 : m_tx1).trimmed();
    };

    QString payload = payloadForTx();
    bool badPayload = payload.contains(QStringLiteral("bad message"), Qt::CaseInsensitive);
    if ((payload.isEmpty() || badPayload) && !m_dxCall.trimmed().isEmpty()) {
        bridgeLog(QStringLiteral("Hound TX: regenerating TX messages before %1 (tx=%2)")
                      .arg(reason)
                      .arg(desiredTx));
        regenerateTxMessages();
        payload = payloadForTx();
        badPayload = payload.contains(QStringLiteral("bad message"), Qt::CaseInsensitive);
    }
    if (!payload.isEmpty() && !badPayload) {
        return true;
    }

    QString const text = m_dxCall.trimmed().isEmpty()
        ? QStringLiteral("Hound: seleziona prima la stazione Fox con doppio click o inserisci DX Call")
        : QStringLiteral("Hound: il messaggio TX%1 non e' pronto").arg(desiredTx);
    bridgeLog(QStringLiteral("Hound TX blocked (%1): tx=%2 dx=[%3] tx1=[%4] tx3=[%5]")
                  .arg(reason)
                  .arg(desiredTx)
                  .arg(m_dxCall.trimmed())
                  .arg(m_tx1.trimmed())
                  .arg(m_tx3.trimmed()));
    emit errorMessage(text);
    emit statusMessage(text);
    return false;
}

