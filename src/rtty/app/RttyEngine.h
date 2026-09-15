// DecoRTTY — the engine the interface talks to.
//
// Owns the receive chain (stereo 24 kHz from the radio → mono 8 kHz → FSK
// demodulator → soft-decision Baudot decoder → text) and the transmit chain
// (testo → Baudot → AFSK a fase continua), piu' lo spettro
// the waterfall draws.
//
// Everything runs on the thread that owns this object. At 8 kHz the decoder
// costs well under a percent of one core, and the analysis FFT runs once per
// 2048 samples, so there is nothing here that justifies the synchronisation
// cost of a worker thread.
#pragma once

#include "dsp/Fft.h"
#include "dsp/FirDecimator.h"
#include "dsp/FskModulator.h"
#include "dsp/AutoTuner.h"
#include "dsp/RttyDemodulator.h"
#include "dsp/RxFilters.h"
#include "dsp/RttyTypes.h"
#include "dsp/ViterbiBaudot.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include <memory>
#include <vector>

namespace decortty::link { class RadioHub; }

namespace decortty::app {

class RttyEngine : public QObject {
    Q_OBJECT

    Q_PROPERTY(double markHz READ markHz WRITE setMarkHz NOTIFY paramsChanged)
    Q_PROPERTY(double shiftHz READ shiftHz WRITE setShiftHz NOTIFY paramsChanged)
    Q_PROPERTY(double baud READ baud WRITE setBaud NOTIFY paramsChanged)
    Q_PROPERTY(bool reverse READ reverse WRITE setReverse NOTIFY paramsChanged)
    Q_PROPERTY(bool unshiftOnSpace READ unshiftOnSpace WRITE setUnshiftOnSpace NOTIFY paramsChanged)
    Q_PROPERTY(bool afcEnabled READ afcEnabled WRITE setAfcEnabled NOTIFY paramsChanged)
    // Auto-centratura: cerca il segnale in banda e ci porta sopra la sintonia.
    // Distinta dall'AFC, che invece insegue la deriva di quello che sta gia'
    // decodificando — e che continua a lavorare qui sotto.
    Q_PROPERTY(bool autoTuneEnabled READ autoTuneEnabled WRITE setAutoTuneEnabled NOTIFY paramsChanged)
    Q_PROPERTY(double squelchDb READ squelchDb WRITE setSquelchDb NOTIFY paramsChanged)
    Q_PROPERTY(int correctionDepth READ correctionDepth WRITE setCorrectionDepth NOTIFY paramsChanged)
    // Audio level sent to the radio, 0..1. On AFSK this is the single most
    // important transmit adjustment: too high drives the transceiver's ALC, and
    // an ALC that is working is an ALC that is flattening the tones and putting
    // intermodulation either side of your signal. Set it so the ALC meter barely
    // moves.
    Q_PROPERTY(double transmitLevel READ transmitLevel WRITE setTransmitLevel NOTIFY paramsChanged)

    // ── filtri anti-QRM ─────────────────────────────────────────────────
    Q_PROPERTY(bool bandpassEnabled READ bandpassEnabled WRITE setBandpassEnabled NOTIFY paramsChanged)
    Q_PROPERTY(double bandpassWidthHz READ bandpassWidthHz WRITE setBandpassWidthHz NOTIFY paramsChanged)
    Q_PROPERTY(bool notchEnabled READ notchEnabled WRITE setNotchEnabled NOTIFY paramsChanged)
    Q_PROPERTY(double notchHz READ notchHz WRITE setNotchHz NOTIFY paramsChanged)
    Q_PROPERTY(bool lmsEnabled READ lmsEnabled WRITE setLmsEnabled NOTIFY paramsChanged)

    // ── parametri RTTY estesi ───────────────────────────────────────────
    Q_PROPERTY(double stopBits READ stopBits WRITE setStopBits NOTIFY paramsChanged)
    Q_PROPERTY(int dataBits READ dataBits WRITE setDataBits NOTIFY paramsChanged)
    Q_PROPERTY(int parity READ parity WRITE setParity NOTIFY paramsChanged)
    Q_PROPERTY(bool ignoreFramingErrors READ ignoreFramingErrors WRITE setIgnoreFramingErrors NOTIFY paramsChanged)
    Q_PROPERTY(int figuresSet READ figuresSet WRITE setFiguresSet NOTIFY paramsChanged)

    // ── trasmissione ────────────────────────────────────────────────────
    Q_PROPERTY(int diddleMode READ diddleMode WRITE setDiddleMode NOTIFY paramsChanged)
    Q_PROPERTY(double characterWaitBits READ characterWaitBits WRITE setCharacterWaitBits NOTIFY paramsChanged)
    Q_PROPERTY(bool autoCqEnabled READ autoCqEnabled NOTIFY transmitStateChanged)

    Q_PROPERTY(bool locked READ locked NOTIFY telemetryChanged)
    Q_PROPERTY(double snrDb READ snrDb NOTIFY telemetryChanged)
    Q_PROPERTY(double toneSeparationDb READ toneSeparationDb NOTIFY telemetryChanged)
    Q_PROPERTY(double afcOffsetHz READ afcOffsetHz NOTIFY telemetryChanged)
    Q_PROPERTY(double measuredBaud READ measuredBaud NOTIFY telemetryChanged)
    // Il fondo di banda, in dB. Il waterfall ci appoggia sopra la propria scala
    // invece di usare due estremi fissi: il livello che esce dal codec dipende
    // dal guadagno della radio e dal cavo, e una scala fissa mostra la stessa
    // banda ora tutta nera e ora tutta accesa.
    Q_PROPERTY(double spectrumFloorDb READ spectrumFloorDb NOTIFY spectrumUpdated)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY transmitStateChanged)
    Q_PROPERTY(int transmitQueueLength READ transmitQueueLength NOTIFY transmitStateChanged)

public:
    explicit RttyEngine(QObject* parent = nullptr);
    ~RttyEngine() override;

    void attachRadio(link::RadioHub* radio);

    // ── parameters ──────────────────────────────────────────────────────
    double markHz() const  { return m_params.markHz; }
    double shiftHz() const { return m_params.shiftHz; }
    double baud() const    { return m_params.baud; }
    bool   reverse() const { return m_params.reverse; }
    bool   unshiftOnSpace() const { return m_params.unshiftOnSpace; }
    bool   afcEnabled() const { return m_params.afcEnabled; }
    double squelchDb() const  { return m_params.squelchDb; }
    int    correctionDepth() const { return m_correctionDepth; }
    double transmitLevel() const { return m_modulator.amplitude(); }
    // Keep the operator's AFSK level separate from a radio-specific effective
    // override. Switching away from a QMX must not leave another rig at 100%.
    double configuredTransmitLevel() const { return m_configuredTransmitLevel; }

    void setMarkHz(double hz);
    void setShiftHz(double hz);
    void setBaud(double baud);
    void setReverse(bool on);
    void setUnshiftOnSpace(bool on);
    void setAfcEnabled(bool on);
    void setAutoTuneEnabled(bool on);
    void setSquelchDb(double db);
    void setCorrectionDepth(int depth);
    void setTransmitLevel(double level);

    bool   bandpassEnabled() const { return m_params.bandpassEnabled; }
    double bandpassWidthHz() const { return m_params.bandpassWidthHz; }
    bool   notchEnabled() const { return m_params.notchEnabled; }
    double notchHz() const { return m_params.notchHz; }
    bool   lmsEnabled() const { return m_params.lmsEnabled; }
    void setBandpassEnabled(bool on);
    void setBandpassWidthHz(double hz);
    void setNotchEnabled(bool on);
    void setNotchHz(double hz);
    void setLmsEnabled(bool on);

    double stopBits() const { return m_params.stopBits; }
    int    dataBits() const { return m_params.dataBits; }
    int    parity() const { return m_params.parity; }
    bool   ignoreFramingErrors() const { return m_params.ignoreFramingErrors; }
    int    figuresSet() const { return static_cast<int>(m_params.figuresSet); }
    void setStopBits(double bits);
    void setDataBits(int bits);
    void setParity(int parity);
    void setIgnoreFramingErrors(bool on);
    void setFiguresSet(int set);

    int    diddleMode() const { return static_cast<int>(m_modulator.diddleMode()); }
    double characterWaitBits() const { return m_modulator.characterWaitBits(); }
    bool   autoCqEnabled() const { return m_autoCqTimer.isActive(); }
    void setDiddleMode(int mode);
    void setCharacterWaitBits(double bits);
    // Ripete un testo a intervalli regolari finché non lo si ferma: è il modo in
    // cui si chiama CQ senza restare incollati alla tastiera.
    Q_INVOKABLE void startAutoCq(const QString& text, int intervalSeconds);
    Q_INVOKABLE void stopAutoCq();

    // ── telemetry ───────────────────────────────────────────────────────
    bool   autoTuneEnabled() const { return m_autoTuner.enabled(); }
    bool   locked() const { return m_demod.locked(); }
    double snrDb() const  { return m_demod.snrDb(); }
    double toneSeparationDb() const { return m_demod.toneSeparationDb(); }
    double afcOffsetHz() const { return m_demod.afcOffsetHz(); }
    double measuredBaud() const { return m_demod.measuredBaud(); }
    double spectrumFloorDb() const { return m_spectrumFloorDb; }
    bool   transmitting() const { return m_transmitting; }
    int    transmitQueueLength() const { return static_cast<int>(m_modulator.pending()); }

    // ── operating ───────────────────────────────────────────────────────
    // Centra una volta sola, adesso: quello che l'operatore si aspetta da un
    // tasto, senza doversi accendere l'automatismo.
    Q_INVOKABLE bool centreOnSignal();

    Q_INVOKABLE void clearReceiveText();
    // Dimentica la banda vista finora. Serve al cambio banda: la memoria
    // spettrale su cui si appoggiano fondo e auto-centratura descrive un pezzo
    // di spettro che non esiste piu', e portarsela dietro vuol dire cercare un
    // segnale dove non c'e' mai stato.
    Q_INVOKABLE void forgetBand();
    // Queue text for transmission. Starts the transmitter if it is not already up.
    Q_INVOKABLE void transmitText(const QString& text);
    Q_INVOKABLE void startTransmit();
    // `immediate` abandons anything still queued instead of draining it first.
    Q_INVOKABLE void stopTransmit(bool immediate = false);
    Q_INVOKABLE void abortTransmit() { stopTransmit(true); }

    // The spectrum the waterfall and tuning display read, in dB, covering
    // 0..4000 Hz. Refreshed about 30 times a second.
    Q_INVOKABLE QVector<double> spectrum() const { return m_spectrumDb; }

public slots:
    // Interleaved stereo at 24 kHz, straight from the radio.
    void processRadioAudio(const std::vector<float>& samples, int frames);

signals:
    void paramsChanged();
    void telemetryChanged();
    void transmitStateChanged();
    void spectrumUpdated();
    // One decoded character. `quality` is 0..1 and drives the on-screen
    // confidence shading; `corrected` marks a character the search overruled.
    void characterDecoded(const QString& text, double quality, bool corrected);
    // A character that has actually gone out on the air, for the transmit echo.
    void characterTransmitted(const QString& text);
    void scopePointReady(double mark, double space);
    // L'auto-centratura ha spostato qualcosa. Il messaggio va in barra di stato:
    // una sintonia che si muove da sola, e non lo dice, sembra un guasto.
    void autoTuned(const QString& message);
    void errorOccurred(const QString& message);

private slots:
    void onTransmitTick();

private:
    void applyParams();
    void applyRadioTransmitLevelPolicy();
    void rebuildFilters();
    void updateSpectrum(const float* mono, int count);
    void updateSpectrumFloor();
    void pumpTransmit();
    void finishTransmit();
    // `autoReturn` distingue i due modi di trasmettere. Una macro o una riga
    // inviata con Enter manda il suo testo e torna in ricezione da sola; il
    // pulsante TRANSMIT invece tiene su il trasmettitore, con il diddle, perche'
    // l'operatore sta scrivendo dal vivo.
    void beginTransmit(bool autoReturn);

    dsp::RttyParams      m_params;
    dsp::FirDecimator    m_decimator;
    dsp::RttyDemodulator m_demod;
    dsp::AutoTuner       m_autoTuner;
    dsp::ViterbiBaudot   m_viterbi;
    dsp::FskModulator    m_modulator;
    dsp::ToneBandpass    m_bandpass;
    dsp::NotchFilter     m_notch;
    dsp::LmsCanceller    m_lms;
    std::vector<float>   m_filtered;

    link::RadioHub* m_radio{nullptr};

    double m_spectrumFloorDb{-100.0};
    std::vector<double> m_floorScratch;
    int  m_correctionDepth{4};
    float m_configuredTransmitLevel{0.35f};
    bool m_transmitting{false};
    bool m_drainingTransmit{false};
    // True while the decoder is held off because the radio is transmitting.
    bool m_mutedByTransmit{false};
    // Vero quando questa trasmissione deve chiudersi da sola a coda vuota.
    bool m_autoReturnToRx{false};

    std::vector<float> m_mono;
    std::vector<float> m_work;
    std::vector<float> m_txBuffer;

    // Spectrum analysis
    dsp::Fft                       m_fft;
    std::vector<float>             m_fftWindow;
    std::vector<float>             m_fftBuffer;
    std::vector<dsp::Cpx>          m_fftScratch;
    int                            m_fftFill{0};
    QVector<double>                m_spectrumDb;

    QTimer        m_txTimer;
    QTimer        m_txTailTimer;
    QTimer        m_txTimeout;
    QTimer        m_autoCqTimer;
    QString       m_autoCqText;
    QTimer        m_telemetryTimer;
    QTimer        m_autoTuneTimer;
    // Da quanto si e' tornati in ascolto. La ricerca aspetta che lo spettro si
    // sia rifatto sull'aria vera prima di dare retta a quello che ci legge.
    QElapsedTimer m_sinceReceive;
    QElapsedTimer m_txClock;
    qint64        m_txSamplesSent{0};
};

} // namespace decortty::app
