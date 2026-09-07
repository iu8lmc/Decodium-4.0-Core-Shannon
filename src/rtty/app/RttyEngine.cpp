#include "app/RttyEngine.h"

#include "link/RadioHub.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace decortty::app {

namespace {
constexpr int kRadioRate  = 24000;
constexpr int kFftSize    = 2048;
// L'audio di trasmissione si genera a pezzi di 20 ms: era il doppio del frame
// Opus da 10 ms quando l'audio andava in rete compresso. Opus non c'e' piu', ma
// la misura resta buona - da' respiro al timer senza allungare la latenza.
constexpr int kTxChunkMs  = 20;
constexpr int kTxChunkSamples = kRadioRate * kTxChunkMs / 1000;
}

RttyEngine::RttyEngine(QObject* parent)
    : QObject(parent)
    , m_decimator(kRadioRate / dsp::kWorkRate, kRadioRate)
    , m_demod(m_params, dsp::kWorkRate)
    , m_modulator(m_params, kRadioRate)
    , m_fft(kFftSize)
{
    m_fftWindow = dsp::blackmanHarrisWindow(kFftSize);
    m_fftBuffer.assign(kFftSize, 0.0f);
    m_fftScratch.assign(kFftSize, {});
    m_spectrumDb.resize(kFftSize / 2);
    m_spectrumDb.fill(-120.0);

    m_modulator.setAmplitude(m_configuredTransmitLevel);
    applyParams();

    m_txTimer.setInterval(kTxChunkMs);
    m_txTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_txTimer, &QTimer::timeout, this, &RttyEngine::onTransmitTick);

    // Telemetry changes continuously; repainting the UI on every sample would be
    // absurd, so it is sampled at a rate a human can actually read.
    // How long transmit audio takes to actually reach the air after it is
    // generated: the network hop, the gateway's interpolator and the sound
    // card's own buffer. PTT is held this long past the last sample.
    m_txTailTimer.setSingleShot(true);
    m_txTailTimer.setInterval(350);
    connect(&m_txTailTimer, &QTimer::timeout, this, [this] {
        m_modulator.clearQueue();
        finishTransmit();
    });

    // A hard ceiling on how long one transmission may last. Every RTTY program
    // has one: it is the last line of defence against a stuck PTT, a runaway
    // macro or an operator who walked away.
    m_txTimeout.setSingleShot(true);
    m_txTimeout.setInterval(180000);
    connect(&m_txTimeout, &QTimer::timeout, this, [this] {
        emit errorOccurred(tr("Transmit ran for three minutes — stopped"));
        stopTransmit(true);
    });

    // Auto-CQ: ripete la chiamata a intervalli, ma non mentre si sta gia'
    // trasmettendo — due chiamate sovrapposte sono peggio di nessuna.
    connect(&m_autoCqTimer, &QTimer::timeout, this, [this] {
        if (!m_transmitting && !m_autoCqText.isEmpty())
            transmitText(m_autoCqText);
    });

    m_telemetryTimer.setInterval(100);
    connect(&m_telemetryTimer, &QTimer::timeout, this, &RttyEngine::telemetryChanged);
    m_telemetryTimer.start();

    // Quattro volte al secondo: piu' spesso sarebbe inutile, perche' lo spettro
    // medio su cui si decide si rinnova ogni quarto di secondo.
    m_autoTuner.setRange(400.0f, 2600.0f);
    m_autoTuneTimer.setInterval(250);
    connect(&m_autoTuneTimer, &QTimer::timeout, this, [this] {
        // Mai mentre si trasmette: il decodificatore sta ascoltando la propria
        // nota, e inseguirla sarebbe il modo perfetto per perdere la frequenza
        // del corrispondente.
        if (m_transmitting || m_mutedByTransmit)
            return;
        // Appena rientrati in ascolto lo spettro medio contiene ancora la
        // propria nota, rimandata dal monitor della radio: e' il segnale piu'
        // forte in banda e la ricerca ci si poserebbe sopra. Si aspetta che la
        // media si sia rifatta su quello che c'e' davvero.
        if (m_sinceReceive.isValid() && !m_sinceReceive.hasExpired(2000))
            return;
        const auto decision = m_autoTuner.update(m_demod, m_params, 0.25);
        if (!decision.changed)
            return;
        const bool flipped = decision.action == dsp::AutoTuner::Decision::Action::FlipReverse;
        m_params.markHz  = std::clamp(decision.markHz, 300.0f, 3500.0f);
        m_params.reverse = decision.reverse;
        applyParams();
        emit autoTuned(flipped
                           ? tr("Trying the opposite polarity (%1)")
                                 .arg(decision.reverse ? tr("reversed") : tr("direct"))
                           : tr("Signal at %1 Hz: moving there")
                                 .arg(m_params.markHz, 0, 'f', 0));
    });
}

RttyEngine::~RttyEngine() = default;

void RttyEngine::attachRadio(link::RadioHub* radio)
{
    if (m_radio)
        disconnect(m_radio, nullptr, this, nullptr);

    m_radio = radio;
    if (!m_radio)
        return;

    connect(m_radio, &link::RadioHub::audioReady,
            this, &RttyEngine::processRadioAudio);
    connect(m_radio, &link::RadioHub::connectionChanged,
            this, &RttyEngine::applyRadioTransmitLevelPolicy);
    // Follow the radio's own idea of whether it is transmitting, rather than
    // assuming our PTT request succeeded.
    connect(m_radio, &link::RadioHub::transmittingChanged, this, [this] {
        const bool radioTx = m_radio->transmitting();
        if (!radioTx && m_transmitting) {
            m_transmitting = false;
            m_txTimer.stop();
            emit transmitStateChanged();
        }
    });
    applyRadioTransmitLevelPolicy();
}

void RttyEngine::applyRadioTransmitLevelPolicy()
{
    const float effective = m_radio && m_radio->requiresFullScaleTransmitAudio()
                                ? 1.0f
                                : m_configuredTransmitLevel;
    if (qFuzzyCompare(m_modulator.amplitude(), effective)) {
        return;
    }

    // QMX/QMX+ detect the tone only after it crosses a rise threshold that
    // defaults to 80%, so they need full-scale application audio. Restore the
    // configured AFSK level for every other radio; power then remains an
    // operator-controlled radio setting rather than a leaked QMX override.
    m_modulator.setAmplitude(effective);
    emit paramsChanged();
}

void RttyEngine::rebuildFilters()
{
    // Il passa-banda è centrato fra i due toni e largo abbastanza da lasciarli
    // passare entrambi con il margine dell'AFC.
    if (m_params.bandpassEnabled) {
        m_bandpass.configure(static_cast<float>(m_params.centreHz()),
                             m_params.bandpassWidthHz, dsp::kWorkRate);
    }
    if (m_params.notchEnabled && m_params.notchHz > 50.0f) {
        m_notch.configure(m_params.notchHz, m_params.notchWidthHz, dsp::kWorkRate);
    }
    if (m_params.lmsEnabled) {
        // Ritardo di decorrelazione di due bit. Il valore è misurato, non
        // scelto: con mezzo bit il filtro predice benissimo anche il RTTY e lo
        // cancella insieme al disturbo (l'errore di carattere andava a 1.000,
        // cioè nulla di leggibile); a due bit il RTTY è ormai imprevedibile
        // mentre una portante ferma resta tale, e l'errore scende da 0.92 a
        // 0.03. Oltre i quattro bit ricomincia a peggiorare.
        const int delay = static_cast<int>(m_params.samplesPerBit(dsp::kWorkRate) * 2.0f);
        m_lms.configure(32, 0.004f, std::max(4, delay));
    }
}

void RttyEngine::applyParams()
{
    m_demod.setParams(m_params);
    m_modulator.setParams(m_params);
    rebuildFilters();

    dsp::ViterbiBaudot::Config cfg;
    cfg.depth          = m_correctionDepth;
    cfg.unshiftOnSpace = m_params.unshiftOnSpace;
    cfg.figuresSet     = m_params.figuresSet;
    m_viterbi.setConfig(cfg);

    emit paramsChanged();
}

void RttyEngine::setMarkHz(double hz)
{
    if (!std::isfinite(hz))
        return;
    if (qFuzzyCompare(m_params.markHz, static_cast<float>(hz)))
        return;
    m_params.markHz = static_cast<float>(std::clamp(hz, 300.0, 3500.0));
    // Sintonia mossa a mano: la caccia riparte da qui invece di riportare dove
    // era arrivata da sola.
    m_autoTuner.reset();
    applyParams();
}

void RttyEngine::setShiftHz(double hz)
{
    // Un valore non finito non entra nel decodificatore. Ci arrivava da un
    // cursore dell'interfaccia — quello e' sistemato — ma la difesa resta: da
    // qui un NaN si propagherebbe ai filtri, poi alle impostazioni salvate, e
    // al riavvio successivo tornerebbe da solo senza che nessuno l'abbia
    // toccato.
    if (!std::isfinite(hz))
        return;
    if (qFuzzyCompare(m_params.shiftHz, static_cast<float>(hz)))
        return;
    m_params.shiftHz = static_cast<float>(std::clamp(hz, 20.0, 1000.0));
    applyParams();
}

void RttyEngine::setBaud(double baud)
{
    if (!std::isfinite(baud))
        return;
    if (qFuzzyCompare(m_params.baud, static_cast<float>(baud)))
        return;
    m_params.baud = static_cast<float>(std::clamp(baud, 10.0, 300.0));
    applyParams();
}

void RttyEngine::setReverse(bool on)
{
    if (m_params.reverse == on)
        return;
    m_params.reverse = on;
    m_autoTuner.reset();
    applyParams();
}

void RttyEngine::setUnshiftOnSpace(bool on)
{
    if (m_params.unshiftOnSpace == on)
        return;
    m_params.unshiftOnSpace = on;
    applyParams();
}

void RttyEngine::setAutoTuneEnabled(bool on)
{
    if (m_autoTuner.enabled() == on)
        return;
    m_autoTuner.setEnabled(on);
    m_autoTuner.reset();
    if (on)
        m_autoTuneTimer.start();
    else
        m_autoTuneTimer.stop();
    emit paramsChanged();
}

// La ricerca su richiesta. A differenza dell'automatismo non aspetta di essere
// senza copia: se l'operatore preme il tasto, vuole muoversi adesso.
bool RttyEngine::centreOnSignal()
{
    const auto pair = m_demod.searchTonePair(400.0f, 2600.0f);
    if (!pair.found || pair.marginDb < 6.0f || pair.balanceDb > 10.0f) {
        emit autoTuned(tr("No recognisable RTTY signal in the band"));
        return false;
    }
    m_params.markHz = std::clamp(pair.markHz, 300.0f, 3500.0f);
    m_autoTuner.reset();
    applyParams();
    emit autoTuned(tr("Centred on %1 Hz (%2 dB above the floor)")
                       .arg(m_params.markHz, 0, 'f', 0)
                       .arg(pair.marginDb, 0, 'f', 1));
    return true;
}

void RttyEngine::setAfcEnabled(bool on)
{
    if (m_params.afcEnabled == on)
        return;
    m_params.afcEnabled = on;
    applyParams();
}

void RttyEngine::setSquelchDb(double db)
{
    if (!std::isfinite(db))
        return;
    const float value = static_cast<float>(std::clamp(db, 0.0, 30.0));
    if (qFuzzyCompare(m_params.squelchDb, value))
        return;
    m_params.squelchDb = value;
    m_params.squelchEnabled = value > 0.05f;
    applyParams();
}

void RttyEngine::setCorrectionDepth(int depth)
{
    depth = std::clamp(depth, 0, 16);
    if (m_correctionDepth == depth)
        return;
    m_correctionDepth = depth;
    applyParams();
}

void RttyEngine::setTransmitLevel(double level)
{
    const float configured = static_cast<float>(std::clamp(level, 0.02, 1.0));
    const float effective = m_radio && m_radio->requiresFullScaleTransmitAudio()
                                ? 1.0f
                                : configured;
    const bool configuredChanged = !qFuzzyCompare(m_configuredTransmitLevel, configured);
    const bool effectiveChanged = !qFuzzyCompare(m_modulator.amplitude(), effective);
    if (!configuredChanged && !effectiveChanged)
        return;
    m_configuredTransmitLevel = configured;
    m_modulator.setAmplitude(effective);
    emit paramsChanged();
}

void RttyEngine::setBandpassEnabled(bool on)
{
    if (m_params.bandpassEnabled == on) return;
    m_params.bandpassEnabled = on;
    m_bandpass.reset();
    applyParams();
}

void RttyEngine::setBandpassWidthHz(double hz)
{
    const float v = static_cast<float>(std::clamp(hz, 150.0, 2000.0));
    if (qFuzzyCompare(m_params.bandpassWidthHz, v)) return;
    m_params.bandpassWidthHz = v;
    applyParams();
}

void RttyEngine::setNotchEnabled(bool on)
{
    if (m_params.notchEnabled == on) return;
    m_params.notchEnabled = on;
    m_notch.reset();
    applyParams();
}

void RttyEngine::setNotchHz(double hz)
{
    const float v = static_cast<float>(std::clamp(hz, 50.0, 3800.0));
    if (qFuzzyCompare(m_params.notchHz, v)) return;
    m_params.notchHz = v;
    applyParams();
}

void RttyEngine::setLmsEnabled(bool on)
{
    if (m_params.lmsEnabled == on) return;
    m_params.lmsEnabled = on;
    m_lms.reset();
    applyParams();
}

void RttyEngine::setStopBits(double bits)
{
    // 1, 1.5 o 2: gli unici valori che una stazione reale usa.
    const float v = bits < 1.25 ? 1.0f : (bits < 1.75 ? 1.5f : 2.0f);
    if (qFuzzyCompare(m_params.stopBits, v)) return;
    m_params.stopBits = v;
    applyParams();
}

void RttyEngine::setDataBits(int bits)
{
    bits = std::clamp(bits, 5, 8);
    if (m_params.dataBits == bits) return;
    m_params.dataBits = bits;
    applyParams();
}

void RttyEngine::setParity(int parity)
{
    parity = std::clamp(parity, 0, 4);
    if (m_params.parity == parity) return;
    m_params.parity = parity;
    applyParams();
}

void RttyEngine::setIgnoreFramingErrors(bool on)
{
    if (m_params.ignoreFramingErrors == on) return;
    m_params.ignoreFramingErrors = on;
    applyParams();
}

void RttyEngine::setFiguresSet(int set)
{
    const auto v = set == 1 ? dsp::FiguresSet::Ita2 : dsp::FiguresSet::US;
    if (m_params.figuresSet == v) return;
    m_params.figuresSet = v;
    applyParams();
}

void RttyEngine::setDiddleMode(int mode)
{
    const auto v = static_cast<dsp::DiddleMode>(std::clamp(mode, 0, 2));
    if (m_modulator.diddleMode() == v) return;
    m_modulator.setDiddleMode(v);
    emit paramsChanged();
}

void RttyEngine::setCharacterWaitBits(double bits)
{
    const float v = static_cast<float>(std::clamp(bits, 0.0, 5.0));
    if (qFuzzyCompare(m_modulator.characterWaitBits(), v)) return;
    m_modulator.setCharacterWaitBits(v);
    emit paramsChanged();
}

void RttyEngine::startAutoCq(const QString& text, int intervalSeconds)
{
    if (text.isEmpty())
        return;
    m_autoCqText = text;
    m_autoCqTimer.setInterval(std::clamp(intervalSeconds, 3, 600) * 1000);
    m_autoCqTimer.start();
    transmitText(text);          // la prima chiamata parte subito
    emit transmitStateChanged();
}

void RttyEngine::stopAutoCq()
{
    m_autoCqTimer.stop();
    emit transmitStateChanged();
}

void RttyEngine::forgetBand()
{
    m_demod.reset();
    m_demod.clearSpectrum();
}

void RttyEngine::clearReceiveText()
{
    m_viterbi.reset();
}

void RttyEngine::processRadioAudio(const std::vector<float>& samples, int frames)
{
    if (frames <= 0 || samples.size() < static_cast<size_t>(frames) * 2)
        return;

    m_mono.resize(static_cast<size_t>(frames));
    dsp::stereoToMono(samples.data(), frames, m_mono.data());

    m_work.resize(static_cast<size_t>(frames) / static_cast<size_t>(m_decimator.factor()) + 2);
    const int decimated = m_decimator.process(m_mono.data(), frames, m_work.data());
    if (decimated <= 0)
        return;

    // Lo spettro mostra il segnale com'è arrivato, prima dei filtri: il
    // waterfall serve a vedere cosa c'è sulla banda, notch compreso, non a
    // mostrare una banda ripulita in cui il disturbo è invisibile.
    updateSpectrum(m_work.data(), decimated);

    // I filtri anti-QRM stanno prima del rivelatore, non dopo: il demodulatore
    // lavora sul rapporto fra i due toni, e una portante dentro uno dei due lo
    // falsa comunque, per quanto sia selettivo.
    float* audio = m_work.data();
    if (m_params.bandpassEnabled || m_params.notchEnabled || m_params.lmsEnabled) {
        m_filtered.resize(static_cast<size_t>(decimated));
        std::copy(audio, audio + decimated, m_filtered.data());
        if (m_params.bandpassEnabled && m_bandpass.isConfigured())
            m_bandpass.process(m_filtered.data(), decimated, m_filtered.data());
        if (m_params.notchEnabled && m_notch.isConfigured())
            m_notch.process(m_filtered.data(), decimated, m_filtered.data());
        if (m_params.lmsEnabled && m_lms.isConfigured())
            m_lms.process(m_filtered.data(), decimated, m_filtered.data());
        audio = m_filtered.data();
    }

    // The decoder does not. A transceiver feeds its transmit monitor back into
    // the same USB codec the gateway is reading, so without this the decoder
    // reads back every character it just sent, and the AGC and AFC retune
    // themselves onto a signal that is fifty dB above anything on the band. That
    // is why transmitting appears to fill the receive window with nonsense and
    // leaves the decoder deaf for seconds afterwards.
    const bool radioTransmitting = m_radio && m_radio->transmitting();
    if (radioTransmitting || m_transmitting) {
        m_mutedByTransmit = true;
        return;
    }
    if (m_mutedByTransmit) {
        // Come back with the levels and the timing loop cleared, rather than
        // carrying the transmit blast's AGC state into the first received
        // character.
        m_demod.reset();
        // E anche senza la memoria spettrale: il monitor della radio ha appena
        // rimandato la nostra nota nell'ingresso, ed e' il segnale piu' forte
        // che l'auto-centratura possa trovare.
        m_demod.clearSpectrum();
        m_sinceReceive.restart();
        m_mutedByTransmit = false;
    }

    m_demod.process(audio, decimated, [this](const dsp::SoftFrame& frame) {
        m_viterbi.push(frame, [this](const dsp::DecodedChar& dc) {
            // Alla finestra di ricezione si manda la certezza, non la qualita'.
            //
            // La qualita' e' la media delle fiducie sui cinque bit e serve a
            // decidere se il carattere si stampa; la certezza e' il loro
            // prodotto — la probabilita' che il carattere sia davvero quello — e
            // serve a decidere quanto mostrarlo convinto. Misurate sul canale
            // ionosferico, la seconda separa i caratteri azzeccati da quelli
            // sbagliati con uno scarto di 0.50 contro 0.14: sullo schermo e' la
            // differenza fra una sfumatura che vuol dire qualcosa e una che non
            // dice niente.
            emit characterDecoded(QString(QChar::fromLatin1(dc.text)),
                                  static_cast<double>(dc.certainty),
                                  dc.corrected);
        });
    });

    float x = 0.0f, y = 0.0f;
    m_demod.scopePoint(x, y);
    emit scopePointReady(static_cast<double>(x), static_cast<double>(y));
}

void RttyEngine::updateSpectrum(const float* mono, int count)
{
    // Finestre sovrapposte, non affiancate. Una trasformata da 2048 punti a
    // 8 kHz copre un quarto di secondo: prendendone una ogni volta che il
    // buffer si riempie si ottengono quattro righe al secondo, e un carattere
    // RTTY — che dura un secondo e mezzo scarso in tutto, centocinquanta
    // millisecondi per bit — sul waterfall non si vede proprio. Avanzando di un
    // quarto di finestra per volta le righe diventano sedici al secondo e i
    // singoli bit si distinguono, che e' il motivo per cui si guarda un
    // waterfall invece di un contatore.
    constexpr int kHop = kFftSize / 4;

    for (int i = 0; i < count; ++i) {
        m_fftBuffer[static_cast<size_t>(m_fftFill++)] = mono[i];
        if (m_fftFill < kFftSize)
            continue;

        m_fft.forwardReal(m_fftBuffer.data(), m_fftWindow.data(), m_fftScratch.data());
        for (int bin = 0; bin < kFftSize / 2; ++bin) {
            const float mag = std::abs(m_fftScratch[static_cast<size_t>(bin)]) / (kFftSize / 2.0f);
            const double db = 20.0 * std::log10(std::max(1e-7f, mag));
            // Lisciatura leggera: con le finestre sovrapposte le righe sono
            // quattro volte piu' fitte, e una media troppo lunga spalmerebbe
            // proprio i bit che si sono guadagnati.
            m_spectrumDb[bin] = 0.75 * m_spectrumDb[bin] + 0.25 * db;
        }

        updateSpectrumFloor();
        emit spectrumUpdated();

        // Si tiene la coda della finestra e si riparte da li'.
        std::memmove(m_fftBuffer.data(), m_fftBuffer.data() + kHop,
                     static_cast<size_t>(kFftSize - kHop) * sizeof(float));
        m_fftFill = kFftSize - kHop;
    }
}

void RttyEngine::updateSpectrumFloor()
{
    // Il fondo per quantile e non per media: una portante forte, o il proprio
    // monitor di trasmissione, sposterebbe la media di parecchi dB — e proprio
    // nei momenti in cui la scala dovrebbe restare ferma.
    // Solo la banda che la radio lascia passare. Sopra i 2700 Hz il filtro
    // taglia e i bin scendono di trenta dB: prendendo il fondo anche di li' si
    // ottiene un valore che in banda non esiste, la scala parte troppo in basso
    // e il waterfall satura di bianco per meta' larghezza.
    const double binHz = 8000.0 / 2048.0;
    const int first = static_cast<int>(300.0 / binHz);
    const int last  = std::min(static_cast<int>(2700.0 / binHz),
                               static_cast<int>(m_spectrumDb.size()) - 1);
    if (last <= first)
        return;

    m_floorScratch.assign(m_spectrumDb.begin() + first, m_spectrumDb.begin() + last);
    const size_t middle = m_floorScratch.size() / 2;
    std::nth_element(m_floorScratch.begin(), m_floorScratch.begin() + middle,
                     m_floorScratch.end());
    const double measured = m_floorScratch[middle];

    // Insegue in fretta quando il fondo sale (una banda che si apre, il
    // guadagno alzato) e con calma quando scende, cosi' una pausa fra due
    // trasmissioni non fa lampeggiare tutto il display.
    const double rate = measured > m_spectrumFloorDb ? 0.20 : 0.02;
    m_spectrumFloorDb += rate * (measured - m_spectrumFloorDb);
}

void RttyEngine::transmitText(const QString& text)
{
    if (text.isEmpty())
        return;

    m_modulator.enqueueText(text.toUpper().toStdString());
    // Un breve postambolo di LTRS: senza di esso l'ultimo carattere finisce
    // contro la fine della portante e il decodificatore all'altro capo, che sta
    // ancora aspettando il bit di stop, lo scarta. Sono caratteri invisibili,
    // costano un decimo di secondo e salvano l'ultima parola di ogni over.
    for (int i = 0; i < 2; ++i)
        m_modulator.enqueueCode(dsp::kBaudotLtrs);

    m_drainingTransmit = false;
    if (!m_transmitting)
        beginTransmit(true);
    emit transmitStateChanged();
}

void RttyEngine::startTransmit()
{
    // Il pulsante TRANSMIT: resta su finche' l'operatore non lo ferma.
    beginTransmit(false);
}

void RttyEngine::beginTransmit(bool autoReturn)
{
    if (m_transmitting || !m_radio)
        return;
    if (!m_radio->canTransmit())
        return;

    // Il modo della radio decide se l'audio che generiamo puo' uscire. In SSB
    // fonia il ricetrasmettitore modula dal microfono e ignora l'ingresso USB:
    // la trasmissione parte, il PTT chiude, il software non ha nulla da
    // segnalare — e in aria non esce niente. Peggio, se il microfono e' aperto
    // va in onda il rumore della stanza. Meglio fermarsi e dirlo.
    const QString mode = m_radio->mode().trimmed().toUpper();
    const bool nativeFsk = mode.startsWith(QLatin1String("RTTY"))
        || mode.startsWith(QLatin1String("FSK"));
    if (nativeFsk && !m_radio->requiresFullScaleTransmitAudio()) {
        emit errorOccurred(tr("The radio is in %1 (native FSK). Decodium transmits "
                              "RTTY as audio/AFSK. Press 'Set radio' in the DECODER "
                              "panel to select DATA-U/DIGU before transmitting.")
                               .arg(mode));
        return;
    }
    if (!mode.isEmpty()
        && !mode.startsWith(QLatin1String("DIG"))
        && !mode.startsWith(QLatin1String("RTTY"))
        && !(nativeFsk && m_radio->requiresFullScaleTransmitAudio())
        && !mode.startsWith(QLatin1String("PKT"))
        && !mode.startsWith(QLatin1String("DATA"))) {
        emit errorOccurred(tr("The radio is in %1: on voice modes the USB audio does "
                              "not modulate. Press 'Set radio' in the DECODER panel.")
                               .arg(mode));
        return;
    }

    applyRadioTransmitLevelPolicy();

    m_transmitting     = true;
    m_drainingTransmit = false;
    m_autoReturnToRx   = autoReturn;
    m_modulator.reset();
    // Il diddle serve a chi scrive dal vivo, per non lasciare buchi fra un
    // carattere e il successivo. In una macro invece terrebbe il trasmettitore
    // in aria per sempre: la coda si svuota, il modulatore continua a mandare
    // LTRS e nulla arriva mai a considerare finita la trasmissione.
    m_modulator.setDiddle(!autoReturn);

    m_radio->setTransmit(true);
    m_txClock.start();
    m_txSamplesSent = 0;
    m_txTimer.start();
    m_txTimeout.start();
    emit transmitStateChanged();
}

void RttyEngine::stopTransmit(bool immediate)
{
    if (!m_transmitting)
        return;

    if (immediate) {
        m_modulator.clearQueue();
    } else if (m_modulator.pending() > 0) {
        // Let the queue drain, then drop PTT — cutting the carrier mid-character
        // would leave the far end with a corrupted last word.
        m_drainingTransmit = true;
        m_modulator.setDiddle(false);
        emit transmitStateChanged();
        return;
    }

    finishTransmit();
}

void RttyEngine::finishTransmit()
{
    m_transmitting     = false;
    m_drainingTransmit = false;
    m_autoReturnToRx   = false;
    m_txTimer.stop();
    m_txTailTimer.stop();
    m_txTimeout.stop();
    if (m_radio)
        m_radio->setTransmit(false);
    emit transmitStateChanged();
}

void RttyEngine::onTransmitTick()
{
    pumpTransmit();

    if ((m_drainingTransmit || m_autoReturnToRx)
        && m_modulator.idle() && !m_txTailTimer.isActive()) {
        // The last characters have been generated, but they have not been heard
        // yet: they are still in the network, in the gateway's resampler and in
        // the sound card's buffer. Dropping PTT now would cut the tail off every
        // transmission — the far end would lose the last word of every over.
        m_txTailTimer.start();
    }
}

void RttyEngine::pumpTransmit()
{
    if (!m_radio || !m_transmitting)
        return;

    // Generate against the wall clock rather than one chunk per timer tick: a
    // late timer would otherwise starve the radio's buffer and put a gap in the
    // transmitted signal, which the far end decodes as garbage.
    const qint64 due = m_txClock.elapsed() * kRadioRate / 1000 + kTxChunkSamples;
    qint64 needed = due - m_txSamplesSent;
    if (needed <= 0)
        return;
    // Never try to catch up more than a quarter second at once.
    needed = std::min<qint64>(needed, kRadioRate / 4);

    m_txBuffer.resize(static_cast<size_t>(needed));
    m_modulator.generate(m_txBuffer.data(), static_cast<int>(needed));
    m_txSamplesSent += needed;

    m_radio->sendTransmitAudio(m_txBuffer.data(), static_cast<int>(needed));

    if (const char echo = m_modulator.takeEchoChar())
        emit characterTransmitted(QString(QChar::fromLatin1(echo)));
}

} // namespace decortty::app
