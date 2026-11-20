#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "MamTxPayloadPolicy.h"
#include <QDateTime>
#include <QSettings>

extern void decodiumBridgeLogbookLog(const QString&);

void DecodiumBridge::setMamMaxStreams(int v)
{
    int const clamped = qBound(1, v, 10);
    if (m_mamMaxStreams == clamped) {
        return;
    }
    m_mamMaxStreams = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("MamMaxStreams"), m_mamMaxStreams);
    emit mamMaxStreamsChanged();
    decodiumBridgeLogbookLog(QStringLiteral("MAM max streams set: %1").arg(m_mamMaxStreams));
}

// 1.0.569+ - CQ paralleli sugli slot liberi. Persistito nello store canonico
// Decodium3. Default ON, ma agisce solo quando il multi-stream e' attivo (che
// e' a sua volta opt-in, default OFF): con multi-slot spento non cambia nulla.
void DecodiumBridge::setMamCqSlots(bool on)
{
    if (m_mamCqSlots == on) {
        return;
    }
    m_mamCqSlots = on;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("MamCqSlots"), on);
    emit mamCqSlotsChanged();
    decodiumBridgeLogbookLog(QStringLiteral("MAM parallel CQ slots toggled: %1").arg(on ? 1 : 0));
}

void DecodiumBridge::setDigiPileupElastic(bool on)
{
    if (m_ft2ElasticFox == on) {
        return;
    }
    m_ft2ElasticFox = on;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("Ft2ElasticFox"), on);
    decodiumBridgeLogbookLog(QStringLiteral("DigiPileup elastic cycle toggled: %1").arg(on ? 1 : 0));
    emit digiPileupChanged();
}

void DecodiumBridge::setDigiPileupBudget(bool on)
{
    if (m_mamPowerBudget == on) {
        return;
    }
    m_mamPowerBudget = on;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("MamPowerBudget"), on);
    decodiumBridgeLogbookLog(QStringLiteral("DigiPileup power budget toggled: %1").arg(on ? 1 : 0));
    emit digiPileupChanged();
}

// Interruttore generale: configura tutto, ma NON abilita il TX ne' l'AutoCQ
// (restano decisioni dell'operatore, come il pulsante ARM della DX-Pedition).
void DecodiumBridge::setDigiPileupEnabled(bool on)
{
    if (on) {
        setMamMultiStream(true);
    }
    setDigiPileupElastic(on);
    setDigiPileupBudget(on);
    emit digiPileupChanged();
}

QVariantList DecodiumBridge::mamActiveSlots() const
{
    QVariantList out;
    out.reserve(m_mamSlots.size());
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    for (MamQsoSlot const& s : m_mamSlots) {
        QVariantMap m;
        m.insert(QStringLiteral("call"), s.callFull.isEmpty() ? s.call : s.callFull);
        m.insert(QStringLiteral("freq"), s.audioFreqHz);
        m.insert(QStringLiteral("progress"), s.progress);
        m.insert(QStringLiteral("tx"), s.currentTx);
        m.insert(QStringLiteral("snr"), s.partnerSnrDb);
        m.insert(QStringLiteral("retry"), s.retryCount);
        // 1.0.569+ - campi aggiuntivi per il pannello DX-Pedition multi-slot.
        // Read-only: nessuno muta lo stato dello slot.
        m.insert(QStringLiteral("baseCall"), s.call);
        m.insert(QStringLiteral("grid"), s.grid);
        m.insert(QStringLiteral("rptSent"), s.reportSent);
        m.insert(QStringLiteral("rptRcvd"), s.reportReceived);
        m.insert(QStringLiteral("logged"), s.logged);
        m.insert(QStringLiteral("msg"), s.lastTransmittedMessage);
        m.insert(QStringLiteral("state"),
                 s.state == MamQsoSlot::State::Done     ? 2
               : s.state == MamQsoSlot::State::Signoff  ? 1
                                                        : 0);
        m.insert(QStringLiteral("ageSec"),
                 s.startedOnMs > 0 ? static_cast<int>((nowMs - s.startedOnMs) / 1000) : 0);
        out.append(m);
    }
    return out;
}

// Snapshot read-only del payload che il generatore MAM passera' alla radio.
// A differenza di mamActiveSlots(), qui compaiono anche i CQ creati per gli
// slot liberi.  Il waterfall lo mostra solamente durante TX: in RX gli slot
// QSO restano visibili tramite mamActiveSlots() senza far sembrare occupate
// frequenze sulle quali non stiamo trasmettendo.
QVariantList DecodiumBridge::mamTxStreams() const
{
    QVariantList out;
    const int streamCount = qMin(m_mamMessages.size(), m_mamF0sHz.size());
    out.reserve(streamCount);
    for (int i = 0; i < streamCount; ++i) {
        const QString message = m_mamMessages.at(i).trimmed();
        const int frequency = m_mamF0sHz.at(i);
        if (message.isEmpty() || frequency <= 0) {
            continue;
        }

        const bool isCq = message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive);
        QVariantMap stream;
        stream.insert(QStringLiteral("freq"), frequency);
        stream.insert(QStringLiteral("message"), message);
        stream.insert(QStringLiteral("call"),
                      isCq ? QStringLiteral("CQ")
                           : message.section(QLatin1Char(' '), 0, 0));
        stream.insert(QStringLiteral("cq"), isCq);
        // Keep the key shape compatible with mamActiveSlots, allowing the
        // waterfall to switch models at TX start without a separate delegate.
        stream.insert(QStringLiteral("tx"), 1);
        out.append(stream);
    }
    return out;
}

// 1.0.569+ - FT2 e' l'unico modo in cui il multi-stream non e' mai stato
// validato on-air (primo pileup reale fallito: i chiamanti non decodificavano
// il report). Il pannello DX-Pedition usa questo flag per il banner di avviso;
// la scelta resta all'operatore.
bool DecodiumBridge::mamModeExperimental() const
{
    return m_mode.trimmed().toUpper() == QStringLiteral("FT2");
}

// 1.0.569+ - rimuove manualmente uno slot (pulsante X nella lista slot). Non
// logga: e' un abbandono esplicito. La stazione puo' rientrare piu' tardi.
void DecodiumBridge::mamDropSlot(const QString& call)
{
    int const idx = mamSlotIndexForCall(call);
    if (idx < 0) {
        return;
    }
    QString const dropped = m_mamSlots.at(idx).callFull;
    m_mamSlots.removeAt(idx);
    decodiumBridgeLogbookLog(QStringLiteral("MAM slot dropped by operator: %1 (%2 active)")
                  .arg(dropped).arg(m_mamSlots.size()));
    emit mamActiveSlotsChanged();
}

// 1.0.569+ - forza il log di uno slot senza attendere il signoff (il partner
// e' sparito ma lo scambio e' completo). Riusa mamLogSlot: stessa via del log
// automatico, quindi cooldown e anti-duplicato restano coerenti.
void DecodiumBridge::mamLogSlotNow(const QString& call)
{
    int const idx = mamSlotIndexForCall(call);
    if (idx < 0) {
        return;
    }
    mamLogSlot(m_mamSlots[idx]);
    mamPruneSlots();
    emit mamActiveSlotsChanged();
}

// 1.0.569+ - svuota tutti gli slot attivi (pulsante CLEAR). Non logga nulla.
void DecodiumBridge::mamClearSlots()
{
    if (m_mamSlots.isEmpty()
        && m_mamLateReportRecoveries.isEmpty()
        && m_mamMessages.isEmpty()
        && m_mamF0sHz.isEmpty()) {
        return;
    }
    int const n = m_mamSlots.size();
    m_mamSlots.clear();
    m_mamLateReportRecoveries.clear();
    clearMamPendingTxPayload(QStringLiteral("slots-cleared"));
    decodiumBridgeLogbookLog(QStringLiteral("MAM slots cleared by operator: %1 removed").arg(n));
    emit mamActiveSlotsChanged();
}

// 1.0.569+ - svuota la coda dei chiamanti in attesa (m_callerQueue), lasciando
// intatti gli slot gia' attivi.
void DecodiumBridge::mamClearQueue()
{
    int const lateCount = static_cast<int>(m_mamLateReportRecoveries.size());
    if (m_callerQueue.isEmpty() && lateCount == 0) {
        return;
    }
    int const n = m_callerQueue.size() + lateCount;
    m_callerQueue.clear();
    m_mamLateReportRecoveries.clear();
    decodiumBridgeLogbookLog(QStringLiteral("MAM caller queue cleared by operator: %1 removed").arg(n));
    emit callerQueueChanged();
}

bool DecodiumBridge::mamMultiStreamSequencerActive() const
{
    return decodium::mamtx::multiStreamSequencerIsActive(
        m_mamMultiStream,
        isMamMultiStreamMode(),
        m_txEnabled,
        m_autoCqRepeat || m_multiAnswerMode,
        !usingLegacyBackendForTx() || shouldUseBridgeAudioForLegacyDigitalTx(),
        m_manualTxHold);
}

// 1.0.365+ — path CLICK->SLOT (modello Fox/hunter): l'utente fa doppio-click
// (o spot DX cluster) su una stazione mentre il MAM multi-stream e' attivo;
// invece di spegnere il MAM e aprire un single-QSO, creiamo (o accodiamo) uno
// slot in cui IO chiamo lei partendo da TX1 ("CALL MYCALL GRID"). Tutto gated
// su mamMultiStreamSequencerActive(): con MAM OFF non viene mai invocata.
