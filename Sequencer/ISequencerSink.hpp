#ifndef DECODIUM_ISEQUENCER_SINK_HPP
#define DECODIUM_ISEQUENCER_SINK_HPP

// Fase 1 port mobile — step C strangler (doc/mobile-port-plan.md): il SEAM tra
// la logica del sequencer QSO e il mondo che la ospita (desktop Qt o app
// mobile). Il sequencer NON conosce bridgeLog / emit di segnali Qt / QTimer /
// startTx: dichiara solo INTENZIONI attraverso questa interfaccia, e chi la
// implementa le realizza. Sul desktop l'impl delega ai path esistenti di
// DecodiumBridge byte-identici; sul mobile a un'impl leggera (audio USB, ecc.).
//
// I corpi delle funzioni del sequencer che oggi chiamano quei side-effect
// verranno spostati (step D) in QsoSequencer, che riceve QsoSequencerState& +
// ISequencerSink& e chiama SOLO i metodi qui sotto.

#include <QString>
#include <QtGlobal>

namespace decodium
{
namespace seq
{

// Motivo/esito di una richiesta di log QSO, per non accoppiare al logging desktop.
enum class LogQsoReason
{
    CompletedOwn73,      // chiuso dal nostro 73
    DeferredSignoffCap,  // cap ripetizioni signoff raggiunto
    PartnerLeft,         // partner sparito, log comunque (scambio bidirezionale ok)
    RetryLimit,          // limite retry TX raggiunto
    ManualOrOther,
};

class ISequencerSink
{
public:
    virtual ~ISequencerSink() = default;

    // --- Comandi (output del sequencer) ---

    // Trasmetti lo step txNum col messaggio dato sulla frequenza audio indicata
    // (audioFreqHz<=0 = usa la freq corrente). Astrae startTx/checkAndStartPeriodicTx.
    virtual void requestTransmit(int txNum, const QString& message, int audioFreqHz) = 0;

    // Ferma la trasmissione corrente (motivo per il log diagnostico).
    virtual void requestHaltTx(const QString& reason) = 0;

    // Logga il QSO corrente. logNow=true forza il log anche senza 73 finale.
    virtual void requestLogQso(LogQsoReason reason, bool logNow) = 0;

    // --- Notifiche (il sequencer ha cambiato stato) ---

    // Lo stato QSO e' cambiato: la UI/host aggiorni la visualizzazione.
    virtual void stateChanged(int currentTx, int qsoProgress, const QString& dxCall) = 0;

    // Riga diagnostica (astrae bridgeLog). Non deve avere effetti sulla logica.
    virtual void diag(const QString& line) = 0;

    // Richiama il sequencer tra delayMs (astrae QTimer::singleShot). token
    // identifica quale callback (0 = tick periodico di default). Il tempo e'
    // iniettabile → i test possono simulare l'avanzamento senza QTimer reale.
    virtual void scheduleCallback(int delayMs, int token) = 0;

    // --- Estensioni step D (corpo di DEFAULT: NON pure → FakeSink e impl
    //     parziali restano validi; desktop e mobile le sovrascrivono). Servono
    //     ai corpi di autoSequenceStep/checkAndStartPeriodicTx quando migrati. ---

    // Clock iniettabile (sostituisce correctedUtcEpochMs / ...MsecsSinceStartOfDay).
    virtual qint64 nowEpochMs() const { return 0; }
    virtual qint64 utcMsSinceStartOfDay() const { return 0; }

    // Costruzione messaggi Tx host (genStdMsgs / buildCurrentTxMessage).
    virtual QString buildTxMessage(int txNum) { Q_UNUSED(txNum); return QString(); }
    virtual void regenerateTxMessages() {}

    // Armamento TX host.
    virtual void setTxEnabled(bool on) { Q_UNUSED(on); }
    virtual bool txEnabled() const { return false; }

    // Periodo effettivo per modo (desktop: override MSK144; mobile: periodMsForMode).
    virtual int effectivePeriodMsForMode(const QString& mode) const { Q_UNUSED(mode); return 15000; }

    // Maschera Tx disabilitati (host applica m_txDisabledMask).
    virtual bool isTxDisabled(int txNum) const { Q_UNUSED(txNum); return false; }

    // Stub multi-QSO / WorldMap / partner-memory (no-op su mobile mono-QSO).
    virtual void enqueueCaller(const QString& call, const QString& message) { Q_UNUSED(call); Q_UNUSED(message); }
    virtual void markWorldMapClosed(const QString& call) { Q_UNUSED(call); }
    virtual void rememberPartnerState() {}
};

}
}

#endif
