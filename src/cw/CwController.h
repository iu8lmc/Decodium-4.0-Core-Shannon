// Il modulo CW di Decodium: il pezzo che mette insieme decodificatore,
// manipolatori e macro, come JttyController fa per JTTY.
//
// Viene dal manipolatore di DecoDXLog (RigController), ridotto a quello che
// serve dentro Decodium: la radio, il CAT e l'audio sono quelli
// dell'applicazione e arrivano da qui attraverso i ganci (Hooks), cosi' questa
// libreria non include DecodiumBridge e si prova da sola.
//
// Strade per mandare il CW, nell'ordine in cui le sceglie l'operatore:
//   - "serial"    un piedino (DTR o RTS) di una porta seriale attaccata al
//                 circuitino di manipolazione della radio;
//   - "winkeyer"  un K1EL WinKeyer, che fa da se' i tempi;
//   - "audio"     un tono (sidetone) trasmesso come audio TX in USB/DATA-U: e' la
//                 strada che funziona anche con una radio remota, perche'
//                 attraversa lo stesso percorso audio di qualunque altro modo;
//   - "remotekey" solo con una radio remota Decolink: invece dell'audio si mandano
//                 gli istanti del tasto (pochi byte) e il tono lo rigenera il
//                 gateway accanto alla radio. Non dipende dalla rete per il ritmo e
//                 costa un centesimo del tono audio.
// Il decodificatore ascolta l'audio che esce dalla radio, locale o remota che
// sia: il bridge consegna i campioni a 12 kHz.
#pragma once

#include "CwDecoder.h"
#include "CwKeyer.h"
#include "CwMacros.h"
#include "CwTiming.h"
#include "WinKeyer.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <functional>

namespace decodium::cw {

class CwController : public QObject {
    Q_OBJECT

    // ── decodificatore ───────────────────────────────────────────────────
    Q_PROPERTY(bool tuningLowerSideband READ tuningLowerSideband WRITE setTuningLowerSideband NOTIFY txChanged)
    Q_PROPERTY(bool decoderOn READ decoderOn WRITE setDecoderOn NOTIFY decoderChanged)
    Q_PROPERTY(QString decoderText READ decoderText NOTIFY decoderChanged)
    // Callsigns recognised in the received CW transcript.  They are exposed
    // separately from decoderText so the UI can make each station clickable
    // without trying to edit or interpret the transcript itself.
    Q_PROPERTY(QStringList decoderCalls READ decoderCalls NOTIFY decoderChanged)
    // Expanded CW text submitted to the active transmitter.  This is kept
    // separate from decoderText so RX and TX can be shown with different
    // colours without mixing operator input with received decoding.
    Q_PROPERTY(QString txText READ txText NOTIFY txTranscriptChanged)
    Q_PROPERTY(int decoderWpm READ decoderWpm NOTIFY decoderChanged)
    Q_PROPERTY(int decoderTone READ decoderTone NOTIFY decoderChanged)
    Q_PROPERTY(int decoderToneLock READ decoderToneLock WRITE setDecoderToneLock NOTIFY decoderChanged)
    Q_PROPERTY(int decoderSpeedLock READ decoderSpeedLock WRITE setDecoderSpeedLock NOTIFY decoderChanged)
    Q_PROPERTY(QVariantMap decoderScope READ decoderScope NOTIFY decoderScopeChanged)

    // ── trasmissione ─────────────────────────────────────────────────────
    // "audio", "serial" o "winkeyer".
    Q_PROPERTY(QString txBackend READ txBackend WRITE setTxBackend NOTIFY txChanged)
    // La strada davvero in uso: con una radio remota e' sempre "audio", perche' un
    // manipolatore su una porta di questo computer non comanda la radio lontana.
    Q_PROPERTY(QString effectiveBackend READ effectiveBackend NOTIFY txChanged)
    // Disponibile solo quando la CAT corrente espone davvero Hamlib send_morse.
    // Una connessione rigctld riuscita non implica automaticamente un keyer CW.
    Q_PROPERTY(bool catCwKeyerAvailable READ catCwKeyerAvailable NOTIFY txChanged)
    Q_PROPERTY(bool remoteRadio READ remoteRadio NOTIFY txChanged)
    // Con una radio Decolink: mandare gli istanti del tasto (vero, predefinito) o il tono audio.
    Q_PROPERTY(bool remoteKey READ remoteKey WRITE setRemoteKey NOTIFY txChanged)
    // Local Center CW receive target, also used for Decolink key-event tone.
    // Not a readback or a CAT setter for the physical radio's CW pitch/sidetone.
    Q_PROPERTY(int toneHz READ toneHz WRITE setToneHz NOTIFY txChanged)
    Q_PROPERTY(QString keyerPort READ keyerPort WRITE setKeyerPort NOTIFY txChanged)
    Q_PROPERTY(QString keyerLine READ keyerLine WRITE setKeyerLine NOTIFY txChanged)
    Q_PROPERTY(QString winKeyerPort READ winKeyerPort WRITE setWinKeyerPort NOTIFY txChanged)
    // Errore persistente di configurazione mostrato anche se era stato salvato
    // prima che la finestra QML venisse aperta.
    Q_PROPERTY(QString txSetupError READ txSetupError NOTIFY txChanged)
    Q_PROPERTY(int winKeyerVersion READ winKeyerVersion NOTIFY txChanged)
    Q_PROPERTY(bool keyerOn READ keyerOn NOTIFY txChanged)
    Q_PROPERTY(int wpm READ wpm WRITE setWpm NOTIFY txChanged)
    Q_PROPERTY(bool canSend READ canSend NOTIFY txChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)

    // ── macro ────────────────────────────────────────────────────────────
    // Come {label, text}.
    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)
    // L'ultima macro mandata (per accendere il tasto giusto); -1 se nessuna.
    Q_PROPERTY(int activeMacroIndex READ activeMacroIndex NOTIFY sendingChanged)

public:
    // Quello che serve dall'applicazione. Tutti opzionali: un gancio mancante
    // vuol dire "non so", e il controllo si comporta di conseguenza.
    struct Hooks {
        std::function<double()> frequency;
        std::function<bool(double)> tuneFrequency;
        std::function<QString()> myCall;
        std::function<QString()> hisCall;
        // Manda il testo come audio CW (sidetone) sul percorso TX
        // dell'applicazione. Torna false se non e' partito.
        std::function<bool(const QString& text, int wpm)> sendAudio;
        // Interrompe l'audio CW in corso.
        std::function<void()> abortAudio;
        // Vero se adesso si puo' trasmettere (radio collegata, nessun altro
        // modo che usa l'uscita).
        std::function<bool()> canTransmit;
        // Vero se la radio in uso e' remota (DecoPort, Decolink).
        std::function<bool()> remoteRadio;
        // Il CW a tasto verso una radio remota: vero se la strada sa farlo...
        std::function<bool()> remoteKeySupported;
        std::function<bool()> rigKeyerAvailable;
        std::function<bool(const QString&, int)> sendRigMorse;
        // Completion runs on the controller thread after the CAT command has
        // actually executed; an empty error means the driver accepted it.
        using MorseCompletion = std::function<void(const QString&)>;
        std::function<bool(const QString&, int, MorseCompletion)> sendRigMorseAsync;
        std::function<int()> rigMorseTextLimit;
        // Most radios let their native CW keyer own the PTT/break-in timing.
        // The application supplies false for the CAT path so inter-character
        // and inter-word gaps are not held inside one explicit PTT interval.
        std::function<bool()> rigCwRequiresExplicitPtt;
        std::function<void()> stopRigMorse;
        // Optional explicit PTT ownership for legacy/embedded integrations.
        std::function<void(bool on)> rigPtt;
        // Restituisce chi sta gia' riservando una porta seriale locale, oppure
        // una stringa vuota se il keyer la puo' aprire.  In particolare, una
        // porta CAT Hamlib non puo' essere condivisa con QSerialPort su
        // Windows: il keyer deve fermarsi prima di rubarla al CAT.
        std::function<QString(const QString&)> serialPortReservation;
        // ...manda un pezzo di traccia (false se non e' partito)...
        std::function<bool(const QVector<KeyEvent>& events, int toneHz)> sendKey;
        // ...alza e abbassa il PTT della radio remota...
        std::function<void(bool on)> remotePtt;
        // ...e dice quanto tempo ci mette un comando ad arrivare (ms).
        std::function<int()> remoteLeadMs;
    };

    explicit CwController(QObject* parent = nullptr);
    ~CwController() override;

    void setHooks(Hooks hooks) { m_hooks = std::move(hooks); }
    // Carica le impostazioni (gruppo "CW") e apre il manipolatore scelto.
    void start(QSettings* settings);
    // Il modulo non parte all'avvio di Decodium: si attiva la prima volta che
    // l'operatore apre la finestra CW (ensureStarted), mai prima.
    void deferStart(QSettings* settings) { m_deferredSettings = settings; }
    Q_INVOKABLE void ensureStarted()
    {
        if (!m_started && m_deferredSettings) {
            m_started = true;
            start(m_deferredSettings);
        }
    }
    bool started() const { return m_started; }

    // ── decodificatore ──
    bool decoderOn() const { return m_decoderOn; }
    void setDecoderOn(bool on);
    QString decoderText() const { return m_decoderText; }
    QStringList decoderCalls() const { return m_decoderCalls; }
    QString txText() const { return m_txText; }
    int decoderWpm() const { return m_decoder.wpm(); }
    int decoderTone() const { return static_cast<int>(m_decoder.toneHz()); }
    int decoderToneLock() const { return m_decoderToneLock; }
    void setDecoderToneLock(int hz);
    int decoderSpeedLock() const { return m_decoderSpeedLock; }
    void setDecoderSpeedLock(int wpm);
    QVariantMap decoderScope() const { return m_scope; }
    Q_INVOKABLE void clearDecoder();
    Q_INVOKABLE void clearTxTranscript();
    Q_INVOKABLE bool tuneTo(double hz);
    Q_INVOKABLE bool tuneBy(int hz);
    Q_INVOKABLE bool centerSignal();
    void invalidateTuningSignal();
    bool tuningLowerSideband() const { return m_tuningLowerSideband; }
    void setTuningLowerSideband(bool lower);


    // L'audio che esce dalla radio, mono a 16 bit. 12 kHz e' quel che consegna
    // il bridge; ggmorse ricampiona da se'.
    void feedRxAudio(const QVector<short>& samples, int sampleRate = 12000);

    // ── trasmissione ──
    QString txBackend() const { return m_backend; }
    bool remoteRadio() const { return m_hooks.remoteRadio && m_hooks.remoteRadio(); }
    QString effectiveBackend() const;
    bool catCwKeyerAvailable() const
    {
        return m_hooks.rigKeyerAvailable && m_hooks.rigKeyerAvailable();
    }
    bool remoteKey() const { return m_remoteKey; }
    void setRemoteKey(bool on);
    int toneHz() const { return m_toneHz; }
    void setToneHz(int hz);
    void setTxBackend(const QString& backend);
    QString keyerPort() const { return m_keyerPort; }
    void setKeyerPort(const QString& port);
    QString keyerLine() const { return m_keyerLine; }
    void setKeyerLine(const QString& line);
    QString winKeyerPort() const { return m_winKeyerPort; }
    void setWinKeyerPort(const QString& port);
    QString txSetupError() const { return m_txSetupError; }
    int winKeyerVersion() const { return m_winKeyer.version(); }
    bool keyerOn() const { return m_keyer.isOpen() || m_winKeyer.isOpen(); }
    int wpm() const { return m_wpm; }
    void setWpm(int wpm);
    bool canSend() const;
    bool sending() const { return m_sending; }
    // Lo stato del keyer CAT dipende dalla radio e puo' cambiare dopo che il
    // controller CW e' stato avviato. Forza il refresh delle proprieta' QML.
    Q_INVOKABLE void refreshTransmitState();
    // Le impostazioni CAT possono cambiare mentre la finestra CW e' aperta.
    // Richiudi e rivalida il keyer locale prima che due sottosistemi provino
    // ad aprire la stessa COM/tty.
    void refreshKeyerSetup();
    Q_INVOKABLE QStringList availablePorts() const;
    // Un colpo di prova: la lettera "V" per sentire che il manipolatore
    // risponde.
    Q_INVOKABLE void testKeyer();

    // ── macro ──
    QVariantList macros() const;
    int activeMacroIndex() const { return m_activeMacro; }
    // `context` porta quello che sa solo la finestra: rst, nr, exch, name e,
    // volendo, call (altrimenti viene dal gancio hisCall).
    Q_INVOKABLE void sendMacro(int index, const QVariantMap& context);
    Q_INVOKABLE void sendText(const QString& text, const QVariantMap& context);
    // Literal keyboard input: preserve word separators across small chunks.
    Q_INVOKABLE bool sendTypedText(const QString& text);
    Q_INVOKABLE QString expandText(const QString& text, const QVariantMap& context) const;
    // Ferma tutto, subito.
    Q_INVOKABLE void stop();
    // Leaving the CW application mode must also release local keyer ports and
    // retry timers.  The next ensureStarted() call reopens the selected
    // backend, so an already-created CW window can safely be used again.
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void setMacro(int index, const QString& label, const QString& text);
    Q_INVOKABLE void addMacro();
    Q_INVOKABLE void removeMacro(int index);
    Q_INVOKABLE void resetMacros();

signals:
    void decoderChanged();
    void decoderScopeChanged();
    void txTranscriptChanged();
    void txChanged();
    void sendingChanged();
    void macrosChanged();
    // Per la barra di stato: cosa e' successo, e quanto e' grave.
    void message(const QString& text, const QString& level);

private:
    QSettings* m_deferredSettings {nullptr};
    bool m_started {false};
    void applyBackend();
    void scheduleKeyerReopen(const QString& reason);
    bool rejectNetworkKeyerPort(const QString& port, const QString& keyerName);
    bool rejectReservedKeyerPort(const QString& port, const QString& keyerName);
    void clearTxSetupError();
    void sendExpanded(const QString& ready);
    void sendNextRigChunk();
    void finishRigKey(bool disarm = false);
    void startRemoteKey(const QString& text);
    void pumpRemoteKey();
    void finishRemoteKey(bool immediate);
    void setSending(bool on, int macroIndex);
    void appendTxTranscript(const QString& text);
    void rebuildDecoderCalls();
    void publishScope(bool force = false);
    void save();
    Context contextFrom(const QVariantMap& map) const;

    Hooks m_hooks;
    QSettings* m_settings {nullptr};

    CwDecoder m_decoder {12000};
    QString m_decoderText;
    QStringList m_decoderCalls;
    QString m_txText;
    bool m_decoderOn {true};
    int m_decoderToneLock {0};
    int m_decoderSpeedLock {0};
    QVariantMap m_scope;
    QElapsedTimer m_scopeClock;
    QElapsedTimer m_lastRxClock;
    QElapsedTimer m_retuneClock;
    QElapsedTimer m_decoderLogClock;
    quint64 m_decoderLogSamples {0};
    bool m_tuningLowerSideband {false};

    CwKeyer m_keyer;
    WinKeyer m_winKeyer;
    QString m_backend {QStringLiteral("audio")};
    QString m_keyerPort;
    QString m_keyerLine {QStringLiteral("DTR")};
    QString m_winKeyerPort;
    QString m_txSetupError;
    int m_wpm {20};
    QTimer m_keyerRetryTimer;
    int m_keyerRetryAttempts {0};

    // CW a tasto verso la radio remota
    bool m_remoteKey {true};
    int m_toneHz {700};
    QVector<KeyEvent> m_keyQueue;
    int m_keySentMs {0};
    bool m_keyActive {false};
    bool m_keyPlayheadStarted {false};
    bool m_keyAllSent {false};
    QElapsedTimer m_keyClock;
    QTimer m_keyTimer;

    QList<Macro> m_macros;
    bool m_sending {false};
    int m_activeMacro {-1};
    QTimer m_audioDone;   // completion timer / CAT dispatch watchdog
    QString m_rigQueuedText;
    bool m_rigActive {false};
    bool m_rigWaiting {false};
    bool m_rigPttOwned {false};
    // Keep the CAT stop path armed after the controller's estimated CW
    // duration expires: the radio may still be draining its own keyer buffer.
    bool m_rigStopArmed {false};
    quint64 m_rigGeneration {0};
};

}  // namespace decodium::cw
