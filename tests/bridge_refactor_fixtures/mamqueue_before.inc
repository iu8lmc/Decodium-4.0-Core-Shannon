void DecodiumBridge::mamPromoteQueuedCaller(const QString& callFull)
{
    if (!mamMultiStreamSequencerActive() || m_mamSlots.size() >= m_mamMaxStreams) {
        return;
    }

    QString const target = normalizedBaseCall(callFull);
    if (target.isEmpty()) {
        return;
    }

    int queueIndex = -1;
    for (int i = 0; i < m_callerQueue.size(); ++i) {
        QStringList const parts = m_callerQueue.at(i).split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty() && normalizedBaseCall(parts.constFirst()) == target) {
            queueIndex = i;
            break;
        }
    }
    if (queueIndex < 0) {
        return;
    }

    // mamPromoteFromQueue() contiene la singola fonte di verita' per creare
    // uno slot da un caller in attesa (TX2, report e frequenza). Portiamo la
    // riga scelta in testa e riutilizziamo esattamente quel percorso.
    m_callerQueueManualOrder = true;   // la scelta dell'utente batte il punteggio
    if (queueIndex > 0) {
        m_callerQueue.move(queueIndex, 0);
        emit callerQueueChanged();
    }
    mamPromoteFromQueue();
}

int DecodiumBridge::mamSlotIndexForCall(const QString& base) const
{
    QString const target = normalizedBaseCall(base);
    if (target.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_mamSlots.size(); ++i) {
        if (m_mamSlots.at(i).call == target) {
            return i;
        }
    }
    return -1;
}

