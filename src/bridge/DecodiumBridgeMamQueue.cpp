#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QDateTime>

using decodium::seq::normalizedBaseCall;
using decodium::seq::normalizeCallToken;
void decodiumBridgeMamQueueLog(const QString&);

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


void DecodiumBridge::mamEnqueueClickedStation(const QString& callFull, int audioFreqHz)
{
    if (!mamMultiStreamSequencerActive()) {
        return;
    }
    QString const base = Radio::base_callsign(callFull).trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(m_callsign.trimmed().toUpper());
    if (base.isEmpty() || base == myBaseUpper) {
        return;
    }
    // Gia' in lista: non duplicare (ne' come slot ne' ripartendo da TX1).
    if (mamSlotIndexForCall(base) >= 0) {
        return;
    }
    // Niente capacita' libera: accoda; mamPromoteFromQueue() lo aprira' quando
    // uno slot si libera (stessa via dei caller spontanei).
    if (m_mamSlots.size() >= m_mamMaxStreams) {
        int const qFreq = qBound(200, audioFreqHz > 0 ? audioFreqHz : m_rxFrequency, 4000);
        m_callerQueuePinned.insert(normalizedBaseCall(base));   // scelta dell'operatore: il punteggio non la supera
        enqueueCallerInternal(base, qFreq, -10, true);
        rankCallerQueue(QStringLiteral("click"));
        decodiumBridgeMamQueueLog(QStringLiteral("MAM click-enqueue (queued, no free slot): %1 @ %2Hz")
                      .arg(base).arg(qFreq));
        return;
    }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    MamQsoSlot s;
    s.call = base;
    s.callFull = normalizeCallToken(callFull).toUpper();
    if (s.callFull.isEmpty()) {
        s.callFull = base;
    }
    s.grid = QString {};
    // Freq audio cliccata: clamp 200..4000 + de-dup ~50Hz vs gli slot esistenti
    // (sposta se collide), come mamIngestDecode/mamPromoteFromQueue.
    int fHz = qBound(200, audioFreqHz > 0 ? audioFreqHz : m_rxFrequency, 4000);
    for (int guard = 0; guard < m_mamSlots.size() + 2; ++guard) {
        bool collision = false;
        for (MamQsoSlot const& other : m_mamSlots) {
            if (qAbs(other.audioFreqHz - fHz) < mamMinStreamGapHz()) { collision = true; break; }
        }
        if (!collision) break;
        fHz = qBound(200, fHz + mamStreamStepHz(), 4000);
    }
    s.audioFreqHz = fHz;
    s.progress = 1;     // CQ-stage: IO inizio la chiamata
    s.currentTx = 1;    // TX1 = "CALL MYCALL GRID"
    s.startedOnMs = nowMs;
    s.lastHeardMs = nowMs;
    s.state = MamQsoSlot::State::Active;
    m_mamSlots.append(s);
    decodiumBridgeMamQueueLog(QStringLiteral("MAM click-enqueue: %1 @ %2Hz slot=%3")
                  .arg(s.callFull).arg(s.audioFreqHz).arg(m_mamSlots.size()));
    emit mamActiveSlotsChanged();
}

