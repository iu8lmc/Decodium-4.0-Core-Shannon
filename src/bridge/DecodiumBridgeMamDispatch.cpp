#include "DecodiumBridge.h"
#include "FoxPowerBudget.h"
#include <QDateTime>
#include <QTimer>
#include <utility>
#include <vector>

void decodiumBridgeMamQueueLog(const QString&);
bool decodiumBridgeTxPeriodIsEven(int);
int decodiumBridgeLatestD3SyncStartMs(const QString&, int, bool);

bool DecodiumBridge::foxElasticActive() const
{
    return m_ft2ElasticFox
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && mamMultiStreamSequencerActive();
}

void DecodiumBridge::foxElasticOnTxFinished()
{
    if (!foxElasticActive()) {
        return;
    }
    m_foxElastic.txEnded(QDateTime::currentMSecsSinceEpoch());
    if (!m_foxElasticTimer) {
        m_foxElasticTimer = new QTimer(this);
        m_foxElasticTimer->setInterval(40);
        connect(m_foxElasticTimer, &QTimer::timeout, this, &DecodiumBridge::foxElasticPoll);
    }
    m_foxElasticTimer->start();
    decodiumBridgeMamQueueLog(QStringLiteral("FoxElastic: window open, waiting for %1 station(s), newcomer delay %2 ms")
                  .arg(m_foxElastic.pendingCount())
                  .arg(static_cast<int>(m_foxElastic.newcomerDelayMs())));
}

void DecodiumBridge::foxElasticPoll()
{
    auto closeWindow = [this]() {
        if (m_foxElasticTimer) {
            m_foxElasticTimer->stop();
        }
        m_foxElastic.dispatched();
    };
    if (!foxElasticActive() || !m_foxElastic.windowOpen() || m_transmitting || m_tuning || m_manualTxHold) {
        closeWindow();
        return;
    }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_foxElastic.shouldGo(nowMs)) {
        return;
    }
    if (m_periodicTxInFlight) {
        return;     // un altro avvio e' in corso: riprovo al prossimo giro
    }
    decodiumBridgeMamQueueLog(QStringLiteral("FoxElastic: go after %1 ms, still waiting for %2 station(s)")
                  .arg(nowMs - m_foxElastic.windowStartMs())
                  .arg(m_foxElastic.pendingCount()));
    closeWindow();
    m_foxElasticGo = true;
    checkAndStartPeriodicTx();
    m_foxElasticGo = false;
}

// Dispatch di un periodo TX MAM. Sostituisce checkAndStartPeriodicTx quando il
// sequencer multi-stream e' attivo. Calcola la parita' slot (come
// checkAndStartPeriodicTx), prune/promote, costruisce m_mamMessages/m_mamF0sHz
// per gli slot attivi e avvia startTx() (la seam FASE 1 produce il wave
// multi-stream). Il caso 1-slot e' gestito da multiStreamActive()>=1.
void DecodiumBridge::mamDispatchPeriod(bool elastic)
{
    if (!m_txEnabled) {
        const bool hadTxStreams = !m_mamMessages.isEmpty() || !m_mamF0sHz.isEmpty();
        m_mamMessages.clear();
        m_mamF0sHz.clear();
        m_mamGains.clear();
        if (hadTxStreams) {
            emit mamTxStreamsChanged();
        }
        decodiumBridgeMamQueueLog(QStringLiteral("MAM dispatch blocked: TX disabled"));
        return;
    }

    // Ciclo elastico (FT2, opt-in): finche' la finestra di ascolto e' aperta e'
    // foxElasticPoll() a decidere quando parte il frame successivo; il confine
    // di periodo non deve trasmettere nel mezzo delle risposte. Se l'elastico
    // si bloccasse, passati 6,5 s dalla fine del TX il confine riprende il
    // controllo (ritorno alla griglia).
    if (!elastic && foxElasticActive() && m_foxElastic.windowOpen()
        && QDateTime::currentMSecsSinceEpoch() - m_foxElastic.windowStartMs() < 6500) {
        return;
    }

    // (1) Parita' slot + late-start guard (come il ramo sync di
    // checkAndStartPeriodicTx). Se non e' il nostro periodo: ascolto.
    qint64 const msNow = correctedUtcMsecsSinceStartOfDay();
    int const pMs = periodMsForMode(m_mode);
    if (pMs <= 0) {
        return;
    }
    int const pidx = static_cast<int>(msNow / static_cast<qint64>(pMs));
    bool const isEven = (pidx % 2 == 0);
    bool const isOurPeriod = decodiumBridgeTxPeriodIsEven(m_txPeriod) ? isEven : !isEven;
    if (!isOurPeriod && !elastic) {
        return;
    }
    int const elapsedMs = static_cast<int>(msNow % static_cast<qint64>(pMs));
    int const latestStartMs = decodiumBridgeLatestD3SyncStartMs(m_mode, pMs, effectiveRelaxLatestCap());
    if (!elastic && latestStartMs > 0 && elapsedMs >= latestStartMs) {
        // Troppo tardi nello slot: rinvio al prossimo (no troncamento payload).
        scheduleSyncTxAtNextValidSlot(QStringLiteral("mamDispatchPeriod"), elapsedMs, latestStartMs);
        return;
    }

    // (2)(3) manutenzione slot e promozione dalla coda.
    mamPruneSlots();
    mamPromoteFromQueue();

    // (4) Costruisci i payload per gli slot attivi (cap m_mamMaxStreams).
    const bool hadPreviousTxStreams = !m_mamMessages.isEmpty() || !m_mamF0sHz.isEmpty();
    m_mamMessages.clear();
    m_mamF0sHz.clear();
    m_mamGains.clear();
    // A listening interval suppresses only new CQ streams. Existing MAM QSO
    // slots remain free to send their directed reply/signoff traffic.
    bool const autoCqBurstListeningNow = autoCqBurstListening();
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<int> txSlotIdx;
    // Budget di potenza (opt-in): al posto di "i primi N slot con ampiezza
    // uguale" si ammettono tanti QSO quanti ne stanno nel budget, ciascuno con
    // l'ampiezza che il suo collegamento richiede (FoxPowerBudget.h).
    bool const budgetOn = m_mamPowerBudget;
    QHash<int, float> budgetGain;
    int budgetDeferred = 0;
    if (budgetOn) {
        decodium::foxbudget::Config bcfg;
        bcfg.thresholdDb = (m_mode == QStringLiteral("FT2")) ? -13.5
                         : (m_mode == QStringLiteral("FT4")) ? -17.0 : -20.0;
        bcfg.maxStreams = m_mamMaxStreams;
        std::vector<decodium::foxbudget::Candidate> cands;
        // chi chiude un QSO (RR73/73) prima, poi R+report, report, chiamata
        for (int pass = 0; pass < 4; ++pass) {
            for (int i = 0; i < m_mamSlots.size(); ++i) {
                MamQsoSlot const& s = m_mamSlots.at(i);
                if (s.state == MamQsoSlot::State::Done || s.logged || s.currentTx < 1 || s.currentTx > 6) {
                    continue;
                }
                int const rank = (s.currentTx >= 4) ? 0 : (s.currentTx == 3) ? 1 : (s.currentTx == 2) ? 2 : 3;
                if (rank != pass) {
                    continue;
                }
                bool const measured = s.linkSnrDb != 127;
                double const link = measured ? s.linkSnrDb : (s.partnerSnrDb != 127 ? s.partnerSnrDb : -8);
                cands.emplace_back(i, link, measured, s.waitFrames, !measured && s.partnerSnrDb == 127);
            }
        }
        decodium::foxbudget::Allocation const alloc = decodium::foxbudget::allocate(cands, bcfg);
        budgetDeferred = static_cast<int>(alloc.deferred.size());
        QStringList chosenLog, deferredLog;
        for (size_t k = 0; k < alloc.ids.size(); ++k) {
            budgetGain.insert(alloc.ids[k], static_cast<float>(alloc.gains[k]));
            m_mamSlots[alloc.ids[k]].waitFrames = 0;
            chosenLog << QStringLiteral("%1:%2").arg(m_mamSlots.at(alloc.ids[k]).call).arg(alloc.gains[k], 0, 'f', 2);
        }
        for (int id : alloc.deferred) {
            ++m_mamSlots[id].waitFrames;
            deferredLog << m_mamSlots.at(id).call;
        }
        if (!cands.empty()) {
            decodiumBridgeMamQueueLog(QStringLiteral("MAM budget: streams=[%1] deferred=[%2]")
                          .arg(chosenLog.join(QLatin1Char(' ')), deferredLog.join(QLatin1Char(' '))));
        }
    }
    for (int i = 0; i < m_mamSlots.size() && m_mamMessages.size() < m_mamMaxStreams; ++i) {
        MamQsoSlot& s = m_mamSlots[i];
        if (budgetOn && !budgetGain.contains(i)) {
            continue;
        }
        if (s.state == MamQsoSlot::State::Done || s.logged) {
            continue;
        }
        if (s.currentTx < 1 || s.currentTx > 6) {
            continue;
        }
        QString const msg = mamBuildSlotMessage(s);
        if (msg.trimmed().isEmpty()) {
            continue;
        }
        m_mamMessages.append(msg);
        m_mamF0sHz.append(s.audioFreqHz);
        if (budgetOn) {
            m_mamGains.append(budgetGain.value(i, 1.0f));
            s.lastTxGain = budgetGain.value(i, 1.0f);
        }
        s.lastTransmittedMessage = msg;
        txSlotIdx.append(i);
        s.lastTxSlotMs = nowMs;
        if (s.currentTx == s.lastNtx) {
            ++s.retryCount;
        } else {
            s.retryCount = 1;
        }
        s.lastNtx = s.currentTx;
        s.lastTransmittedMessage = msg;
    }

    // Slot che hanno appena inviato il 73 (TX5): se sendRR73 il QSO e' di
    // fatto chiuso lato mio; loggali ora (se non gia' loggati dal 73 ricevuto).
    for (int j = 0; j < txSlotIdx.size(); ++j) {
        MamQsoSlot& s = m_mamSlots[txSlotIdx.at(j)];
        if (s.currentTx == 5) {
            s.state = MamQsoSlot::State::Signoff;
            ++s.nTx73;
            if (m_sendRR73 && !s.logged) {
                mamLogSlot(s);
            }
        }
    }

    // (4b) 1.0.569+ - CQ PARALLELI sugli slot liberi. Il multi-slot non serve
    // solo a rispondere: ogni posizione non occupata da un QSO chiama CQ su una
    // frequenza propria, cosi' il pileup si distribuisce su tutta la larghezza
    // scelta invece di ammassarsi su una sola. Con m_mamCqSlots spento resta il
    // comportamento precedente (un solo CQ, e solo se non c'e' nessuno slot).
    QString const grid4 = m_grid.left(4);
    QString cqMessage = m_tx6.trimmed();
    if (cqMessage.isEmpty() && !m_callsign.trimmed().isEmpty()) {
        cqMessage = QStringLiteral("CQ %1%2")
                        .arg(m_callsign.trimmed().toUpper(),
                             grid4.isEmpty() ? QString() : QStringLiteral(" ") + grid4);
    }
    if (m_mamCqSlots
        && m_autoCqRepeat
        && !autoCqBurstListeningNow
        && !cqMessage.isEmpty()
        && !(budgetOn && !m_mamMessages.isEmpty())
        && m_mamMessages.size() < m_mamMaxStreams) {
        int const want = budgetOn ? 1 : m_mamMaxStreams - m_mamMessages.size();
        QVector<int> const cqFreqs = mamPlanCqFrequencies(want, m_mamF0sHz);
        for (int f : cqFreqs) {
            m_mamMessages.append(cqMessage);
            m_mamF0sHz.append(f);
        }
        if (!cqFreqs.isEmpty()) {
            QStringList shown;
            shown.reserve(cqFreqs.size());
            for (int f : cqFreqs) {
                shown << QString::number(f);
            }
            decodiumBridgeMamQueueLog(QStringLiteral("MAM dispatch: %1 parallel CQ stream(s) [%2] on %3Hz")
                          .arg(cqFreqs.size())
                          .arg(cqMessage, shown.join(QLatin1Char('/'))));
        }
    }

    if (m_mamMessages.isEmpty()) {
        // (5) Nessuno slot da servire e nessun CQ parallelo: CQ singolo sulla
        // frequenza TX, restando nel path MAM (non deleghiamo a
        // checkAndStartPeriodicTx per evitare ricorsione, dato che quello
        // rientra in mamDispatchPeriod).
        if (m_autoCqRepeat && !autoCqBurstListeningNow && !cqMessage.isEmpty()) {
            m_mamMessages.append(cqMessage);
            m_mamF0sHz.append(qBound(200, m_txFrequency > 0 ? m_txFrequency : m_rxFrequency, 4000));
            decodiumBridgeMamQueueLog(QStringLiteral("MAM dispatch: no active slots, sending CQ [%1]").arg(cqMessage));
        } else {
            // Niente da trasmettere: resto in ascolto.
            if (hadPreviousTxStreams) {
                emit mamTxStreamsChanged();
            }
            return;
        }
    }

    // (5) startTx(): il generatore vede multiStreamActive() (>=1) e produce
    // la waveform multi-stream da m_mamMessages/m_mamF0sHz. Il caso 1-slot
    // e quello N-slot passano entrambi di qui.
    decodiumBridgeMamQueueLog(QStringLiteral("MAM dispatch TX: streams=%1 slots_active=%2 pidx=%3 elapsed=%4ms")
                  .arg(m_mamMessages.size())
                  .arg(m_mamSlots.size())
                  .arg(pidx)
                  .arg(elapsedMs));
    // The array is now exactly the one handed to startTx()/the composite
    // generator. Notify before keying PTT so QML has every TX marker ready
    // when transmitting becomes true.
    emit mamTxStreamsChanged();
    emit mamActiveSlotsChanged();
    if (budgetOn) {
        while (m_mamGains.size() < m_mamMessages.size()) {
            m_mamGains.append(1.0f);     // CQ: a single stream at full scale
        }
    } else {
        m_mamGains.clear();
    }
    // Ciclo elastico: le stazioni a cui e' indirizzato un messaggio che attende
    // risposta (TX1..TX3); i CQ e gli RR73/73 non aspettano nessuno.
    {
        std::vector<std::string> expected;
        for (int idx : std::as_const(txSlotIdx)) {
            if (idx < 0 || idx >= m_mamSlots.size()) {
                continue;
            }
            MamQsoSlot const& es = m_mamSlots.at(idx);
            if (es.currentTx >= 1 && es.currentTx <= 3) {
                expected.push_back(es.call.toStdString());
            }
        }
        m_foxElastic.setExpected(std::move(expected));
    }
    {
        qint64 const txStartMs = QDateTime::currentMSecsSinceEpoch();
        if (m_digiLastTxStartMs > 0) {
            m_digiLastCycleMs = static_cast<int>(txStartMs - m_digiLastTxStartMs);
        }
        m_digiLastTxStartMs = txStartMs;
        m_digiLastStreams = m_mamMessages.size();
        m_digiLastDeferred = budgetDeferred;
        // An elastic start lands anywhere in the slot by design: the late-start
        // guard of startTx() would push it to the next slot and give back the
        // 7.5 s grid cycle (seen on the air, 24/10).
        m_foxElasticTxNow = elastic;
    }
    startTx();
}

