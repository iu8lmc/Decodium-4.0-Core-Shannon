#include "dsp/RttyDemodulator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace decortty::dsp {

namespace {

constexpr float  kTwoPi   = 2.0f * std::numbers::pi_v<float>;
constexpr float  kEps     = 1e-9f;
constexpr int    kAfcSize = 2048;

// Timing-loop gains. Deliberately slow: RTTY transitions are sparse (a run of
// LTRS gives none at all), so a fast loop would be pulled apart by noise between
// edges. These track a several-percent clock error within a few characters.
constexpr double kTimingKp = 0.08;
// Converts the noise-referred tone difference into a log-likelihood ratio.
// Calibrated so a bit sitting one sigma away from the decision boundary scores
// about 1 nat — the point where the language model starts to earn its keep.
constexpr float kLlrGain = 0.9f;
constexpr double kTimingKi = 0.0012;

float smooth(float current, float target, float coeff)
{
    return current + coeff * (target - current);
}

} // namespace

// ── ToneFilter ──────────────────────────────────────────────────────────────

void RttyDemodulator::ToneFilter::configure(float hz, int rate, int lengthSamples)
{
    len = std::max(4, lengthSamples);
    history.assign(static_cast<size_t>(len), std::complex<float>(0.0f, 0.0f));
    running = {0.0f, 0.0f};
    pos     = 0;
    osc     = {1.0f, 0.0f};

    const float w = kTwoPi * hz / static_cast<float>(rate);
    step = std::complex<float>(std::cos(-w), std::sin(-w));
}

void RttyDemodulator::ToneFilter::retune(float hz, int rate)
{
    const float w = kTwoPi * hz / static_cast<float>(rate);
    step = std::complex<float>(std::cos(-w), std::sin(-w));
}

float RttyDemodulator::ToneFilter::push(float sample)
{
    // Mix down by the tone, then integrate over exactly one bit period. The
    // running sum is updated incrementally, so cost is O(1) per sample no matter
    // how long the window is.
    const std::complex<float> mixed = osc * sample;

    running -= history[static_cast<size_t>(pos)];
    history[static_cast<size_t>(pos)] = mixed;
    running += mixed;
    pos = (pos + 1) % len;

    // Renormalise the oscillator occasionally — repeated complex multiplies
    // drift in magnitude over millions of samples.
    osc *= step;
    const float mag2 = osc.real() * osc.real() + osc.imag() * osc.imag();
    if (mag2 < 0.999f || mag2 > 1.001f)
        osc /= std::sqrt(mag2);

    return std::abs(running) / static_cast<float>(len);
}

// ── RttyDemodulator ─────────────────────────────────────────────────────────

RttyDemodulator::RttyDemodulator(const RttyParams& params, int sampleRate)
    : m_params(params)
    , m_sampleRate(sampleRate)
    , m_afcFft(kAfcSize)
{
    m_afcWindow   = blackmanHarrisWindow(kAfcSize);
    m_afcBuffer.assign(kAfcSize, 0.0f);
    m_afcScratch.assign(kAfcSize, {});
    m_afcSpectrum.assign(kAfcSize / 2, 0.0f);
    rebuildFilters();
    reset();
}

void RttyDemodulator::setParams(const RttyParams& params)
{
    const bool retune = params.markHz  != m_params.markHz
                     || params.shiftHz != m_params.shiftHz
                     || params.baud    != m_params.baud;
    m_params = params;
    if (retune) {
        rebuildFilters();
        reset();
    }
}

void RttyDemodulator::rebuildFilters()
{
    m_spbNominal = static_cast<double>(m_sampleRate) / std::max(1.0f, m_params.baud);
    m_spbEst     = m_spbNominal;

    const int windowLen = static_cast<int>(std::lround(m_spbNominal));
    const float mark  = m_params.markHz + m_afcOffsetHz;
    const float space = m_params.spaceHz() + m_afcOffsetHz;

    // `reverse` swaps which tone counts as a logical 1. Doing it here rather
    // than at the decision point keeps the scope display honest too.
    m_mark.configure(m_params.reverse ? space : mark, m_sampleRate, windowLen);
    m_space.configure(m_params.reverse ? mark : space, m_sampleRate, windowLen);
}

void RttyDemodulator::reset()
{
    m_state          = State::Hunt;
    m_bitIndex       = -1;
    m_bitTimer       = 0.0;
    m_prevSoft       = 0.0f;
    m_lockCounter    = 0;
    m_startConfidence = 0.0f;
    m_stopConfidence  = 0.0f;
    m_spbEst         = m_spbNominal;
    m_squelchOpen    = false;
    m_spaceFraction  = 0.0f;
    m_pending.clear();
    // Levels too: a reset after a transmission must not leave the AGC sitting
    // where a fifty-dB-stronger signal put it, or the first seconds of receive
    // are deaf.
    m_agc         = 1e-6f;
    m_noiseFloor  = 1e-6f;
    m_toneRatioDb = 0.0f;
    std::fill(std::begin(m_frameBits), std::end(m_frameBits), 0.0f);
}

void RttyDemodulator::process(const float* samples, int count,
                              const std::function<void(const SoftFrame&)>& onFrame)
{
    for (int i = 0; i < count; ++i) {
        const float x = samples[i];
        ++m_sampleIndex;

        const float markMag  = m_mark.push(x);
        const float spaceMag = m_space.push(x);

        m_markLevelSmooth  = smooth(m_markLevelSmooth,  markMag,  0.01f);
        m_spaceLevelSmooth = smooth(m_spaceLevelSmooth, spaceMag, 0.01f);

        // AGC follows the stronger tone, which is what carries the signal at any
        // instant; the weaker one is the noise-plus-leakage reference.
        const float strong = std::max(markMag, spaceMag);
        const float weak   = std::min(markMag, spaceMag);
        m_agc = strong > m_agc ? smooth(m_agc, strong, 0.05f)     // fast attack
                               : smooth(m_agc, strong, 0.0005f);  // slow release
        m_noiseFloor = smooth(m_noiseFloor, weak, 0.0008f);

        const float snrLin = (m_agc + kEps) / (m_noiseFloor + kEps);
        m_snrDb = 20.0f * std::log10(std::max(1.0f, snrLin));

        // Normalised difference: +1 = solid mark, -1 = solid space. Dividing by
        // the sum rather than by the AGC makes the decision insensitive to
        // fading, which is exactly what a limiter would do in an analogue TU.
        const float soft = (markMag - spaceMag) / (markMag + spaceMag + kEps);

        // Squelch measure: how far apart the two tone filters are right now,
        // averaged over about a bit. FSK drives them apart; noise does not.
        // Averaging over a bit rather than instantaneously stops the momentary
        // crossover at every transition from opening and closing the gate.
        // Averaged over about two seconds: long enough to span a character or
        // two of any real transmission, short enough to react when the band
        // changes.
        m_spaceFraction = smooth(m_spaceFraction, soft < 0.0f ? 1.0f : 0.0f,
                                 1.0f / (2.0f * static_cast<float>(m_sampleRate)));

        const float ratioDb = 20.0f * std::log10((strong + kEps) / (weak + kEps));
        m_toneRatioDb = smooth(m_toneRatioDb, ratioDb, 1.0f / static_cast<float>(m_spbNominal));
        // Hysteresis. A single threshold chatters: the ratio crosses it many
        // times a second on noise alone, and every crossing is a chance for the
        // framing to latch onto nothing. Opening needs the full threshold,
        // staying open needs 2 dB less.
        if (m_params.squelchEnabled) {
            if (m_squelchOpen)
                m_squelchOpen = m_toneRatioDb >= m_params.squelchDb - 2.0f;
            else
                m_squelchOpen = m_toneRatioDb >= m_params.squelchDb;
        } else {
            m_squelchOpen = true;
        }
        const bool open = m_squelchOpen;

        m_scopeX = markMag / (m_agc + kEps);
        m_scopeY = spaceMag / (m_agc + kEps);

        // The value the decision stages actually use: the tone difference
        // measured in units of the noise. For a Rayleigh-distributed envelope
        // the mean of the weaker filter is sigma*sqrt(pi/2), so scaling by
        // 1/1.2533 recovers sigma itself. Clamped because a strong signal would
        // otherwise produce an LLR large enough to swamp every prior.
        // Sulla correzione automatica della soglia, che ogni decodificatore RTTY
        // storico ha e questo no.
        //
        // L'ATC nasce per i rivelatori che decidono con una soglia rigida: se il
        // fading selettivo spegne un tono, la soglia va spostata o si legge il
        // disequilibrio come se fosse segnale. Qui e' stato implementato due
        // volte e misurato sul canale ionosferico del banco di prova, e tutte e
        // due le volte ha peggiorato la copia — pareggiare i due toni: 0.145
        // contro 0.124; sottrarre lo sbilanciamento medio: 0.161. Il motivo e'
        // che questo rivelatore una soglia rigida non ce l'ha: divide per il
        // fondo di rumore, che segue il tono piu' debole, cosi' quando uno
        // svanisce il rapporto cala da se' e il decisore diventa cauto invece di
        // sbagliare con sicurezza. E' l'informazione giusta da passare alla
        // ricerca di Viterbi, e l'ATC gliela toglieva.
        const float sigma = m_noiseFloor / 1.2533f + kEps;
        const float llr   = std::clamp((markMag - spaceMag) / sigma * kLlrGain, -18.0f, 18.0f);

        // Lo spettro si aggiorna comunque: costa una trasformata ogni quarto di
        // secondo e serve anche a chi cerca il segnale, non solo all'AFC.
        updateSpectrum(x);

        // ── timing: interpolate every zero crossing and steer the loop ──
        ++m_samplesSinceEdge;
        // One edge per transition. Noise makes the difference signal cross zero
        // several times within a few samples of every real transition, and the
        // spurious crossings always arrive AFTER the true one — so they all
        // report the same sign of timing error. Fed to an integrator, that bias
        // walks the symbol-rate estimate away until it hits its limit, which
        // costs far more than the tracking ever gains. Ignoring anything within
        // a third of a bit of the last edge removes the bias at the source.
        const bool edgeAllowed =
            static_cast<double>(m_samplesSinceEdge) > m_spbEst * 0.33;

        if ((m_prevSoft < 0.0f) != (soft < 0.0f) && edgeAllowed) {
            m_samplesSinceEdge = 0;
            const float denom = soft - m_prevSoft;
            // Fractional position of the crossing between the previous sample
            // and this one.
            const double frac = std::abs(denom) > kEps
                              ? static_cast<double>(-m_prevSoft / denom)
                              : 0.5;
            // A crossing should land half a bit before the next sampling
            // instant. Whatever it misses by is the timing error.
            const double expected = m_bitTimer + (1.0 - frac);
            trackEdge(expected - m_spbEst * 0.5);

            if (m_state != State::Frame && soft < 0.0f && open) {
                // A mark→space edge with no frame in progress is a start bit.
                // Arm the sampler for the middle of it. Taking the edge in
                // preference to the flywheel's predicted slot is deliberate: when
                // an edge is visible it locates the character far more precisely
                // than the extrapolated clock can.
                m_state           = State::Frame;
                m_bitIndex        = -1;
                m_bitTimer        = m_spbEst * 0.5 - (1.0 - frac);
                m_frameStartIndex = m_sampleIndex;
                m_idleSlots       = 0;
            }
        }
        m_prevSoft = soft;

        if (m_state != State::Hunt) {
            m_bitTimer -= 1.0;
            m_bitPhase = static_cast<float>(1.0 - std::clamp(m_bitTimer / m_spbEst, 0.0, 1.0));
            if (m_bitTimer <= 0.0) {
                if (m_state == State::Sync) {
                    // Flywheel. Once the loop has locked, the clock alone says
                    // where the next character must begin, so a start edge buried
                    // in noise no longer costs the whole character — which is the
                    // failure that dominates a classic edge-triggered decoder.
                    if (soft < 0.0f && open) {
                        m_state           = State::Frame;
                        m_bitIndex        = -1;
                        m_frameStartIndex = m_sampleIndex;
                        m_idleSlots       = 0;
                        m_bitTimer       += m_spbEst;
                        sampleBit(soft, llr, open, onFrame);
                    } else {
                        // Idle mark between characters. Keep the clock running
                        // for a while in case the operator is just pausing.
                        m_bitTimer += m_spbEst;
                        if (++m_idleSlots > kMaxIdleSlots)
                            m_state = State::Hunt;
                    }
                } else {
                    m_bitTimer += m_spbEst;
                    sampleBit(soft, llr, open, onFrame);
                }
            }
        }
    }
}

void RttyDemodulator::trackEdge(double excess)
{
    if (m_state == State::Hunt)
        return;

    // Ignore absurd errors — they are noise crossings, not real bit edges.
    const double limit = m_spbEst * 0.35;
    if (excess > limit || excess < -limit)
        return;

    // Phase always follows the edge. The symbol-rate estimate, however, is only
    // updated by edges that land close to where they were predicted: a large
    // error means the edge was probably not a clean bit boundary, and feeding it
    // to an integrator is how a timing loop walks itself into its own limits.
    m_bitTimer -= kTimingKp * excess;
    if (std::abs(excess) < m_spbEst * 0.15)
        m_spbEst += kTimingKi * excess;
    // Never let the estimate wander more than 5% from nominal; beyond that the
    // operator has the wrong baud rate selected and tracking would only make the
    // decode worse.
    m_spbEst = std::clamp(m_spbEst, m_spbNominal * 0.95, m_spbNominal * 1.05);
}

void RttyDemodulator::sampleBit(float soft, float llr, bool squelchOpen,
                                const std::function<void(const SoftFrame&)>& onFrame)
{
    // A frame that lost its signal part way through is not worth completing —
    // the remaining bits would be pure noise dressed up as data.
    if (!squelchOpen) {
        m_state    = State::Hunt;
        m_bitIndex = -1;
        return;
    }


    if (m_bitIndex < 0) {
        // Start bit. It must still read as space at its centre, or the edge that
        // armed us was noise.
        if (soft > -0.05f) {
            m_state = State::Hunt;
            if (m_lockCounter > 0)
                --m_lockCounter;
            return;
        }
        m_startConfidence = -soft;
        m_frameSnr        = m_snrDb;
        m_bitIndex        = 0;
        return;
    }

    const int dataBits = std::clamp(m_params.dataBits, 5, 8);
    if (m_bitIndex < dataBits) {
        m_frameBits[m_bitIndex] = llr;
        ++m_bitIndex;
        return;
    }

    // Bit di parità, quando la stazione lo manda: viene letto e scartato. Serve
    // solo a mantenere il passo del framing — un carattere letto con un bit di
    // troppo scivola, e da lì in poi non torna più a posto.
    if (m_params.parity != 0 && m_bitIndex == dataBits) {
        ++m_bitIndex;
        return;
    }

    // Stop bit. RTTY sends 1.5 of them; sampling the centre of the first is
    // enough to validate the frame, and hunting for the next start edge from
    // here re-syncs the half bit for free.
    m_stopConfidence = soft;

    SoftFrame frame;
    frame.startConfidence = m_startConfidence;
    frame.stopConfidence  = m_stopConfidence;
    frame.snrDb           = m_frameSnr;
    frame.sampleIndex     = m_frameStartIndex;

    uint8_t code = 0;
    for (int b = 0; b < 5; ++b)
        frame.bitLlr[b] = b < dataBits ? m_frameBits[b] : 0.0f;
    for (int b = 0; b < std::min(dataBits, 5); ++b) {
        if (m_frameBits[b] > 0.0f)
            code |= static_cast<uint8_t>(1u << b);
    }
    frame.hardCode = code;

    // A carrier or a CW signal can frame convincingly by accident; one that
    // never visits the space tone is not RTTY whatever its framing looks like.
    const bool alternates   = m_spaceFraction > 0.04f && m_spaceFraction < 0.9f;
    const bool goodFraming  = m_stopConfidence > 0.0f && m_startConfidence > 0.2f && alternates;
    if (goodFraming)
        m_lockCounter = std::min(m_lockCounter + 1, kLockThreshold * 4);
    else if (m_lockCounter > 0) {
        --m_lockCounter;
        if (!locked())
            m_pending.clear();
    }

    // Stop-bit validation. A frame whose stop bit did not read as mark was not
    // a character — it is the framing having latched onto noise, or onto the
    // middle of a real character after a slip. Printing it would put a garbage
    // letter on screen that no amount of downstream cleverness can remove, so it
    // is dropped here. This is the single biggest source of the spurious
    // characters that make a bad decoder unpleasant to watch.
    if (m_stopConfidence <= 0.0f && m_params.squelchEnabled
        && !m_params.ignoreFramingErrors) {
        m_state    = State::Hunt;
        m_bitIndex = -1;
        return;
    }

    // Confidence gate. Everything above tests the framing; this tests whether
    // the bits themselves were ever convincing. Without it a run of noise that
    // happens to frame correctly still reaches the screen.
    if (m_params.squelchEnabled && frame.quality() < m_params.minFrameQuality) {
        m_state    = State::Hunt;
        m_bitIndex = -1;
        return;
    }

    // Hold the character back until the decoder is actually locked, then release
    // everything that was waiting. This is what stops a passing CW signal or a
    // burst of noise from writing a line of nonsense: garbage never produces the
    // run of consistently well-framed characters that a lock needs, so it is
    // discarded instead of printed. A real transmission still appears in full —
    // the held frames are released the moment lock is reached.
    if (locked()) {
        for (const SoftFrame& held : m_pending)
            onFrame(held);
        m_pending.clear();
        onFrame(frame);
    } else {
        if (m_pending.size() >= kMaxPending)
            m_pending.erase(m_pending.begin());
        m_pending.push_back(frame);
    }

    m_bitIndex = -1;
    if (locked()) {
        // Il prossimo bit di start dista mezzo bit di stop (quello appena
        // campionato) più il resto dello stop, più mezzo bit per arrivare al
        // centro. Con stop a 1.5 fa 1.5 bit; con stop a 2 ne fa 2.
        m_state     = State::Sync;
        m_bitTimer  = m_spbEst * (m_params.stopBits + 0.5 - 0.5);
        m_idleSlots = 0;
    } else {
        m_state = State::Hunt;
    }
}

void RttyDemodulator::clearSpectrum()
{
    std::fill(m_afcSpectrum.begin(), m_afcSpectrum.end(), 0.0f);
    m_afcFill     = 0;
    m_afcOffsetHz = 0.0f;
}

void RttyDemodulator::updateSpectrum(float sample)
{
    m_afcBuffer[static_cast<size_t>(m_afcFill++)] = sample;
    if (m_afcFill < kAfcSize)
        return;
    m_afcFill = 0;

    m_afcFft.forwardReal(m_afcBuffer.data(), m_afcWindow.data(), m_afcScratch.data());

    for (int i = 0; i < kAfcSize / 2; ++i) {
        const float mag = std::abs(m_afcScratch[static_cast<size_t>(i)]);
        // Averaged over several frames so a single noisy transform cannot yank
        // the tuning.
        m_afcSpectrum[static_cast<size_t>(i)] =
            smooth(m_afcSpectrum[static_cast<size_t>(i)], mag, 0.25f);
    }

    if (m_params.afcEnabled)
        steerAfc();
}

void RttyDemodulator::steerAfc()
{
    const float binHz = static_cast<float>(m_sampleRate) / kAfcSize;

    // Search for the strongest pair of bins spaced by the configured shift,
    // within the AFC window. Scoring the pair rather than each tone alone is
    // what keeps AFC from locking onto a lone carrier next door.
    const float nominalSpace = m_params.spaceHz();
    const int   shiftBins    = static_cast<int>(std::lround(m_params.shiftHz / binHz));
    const int   range        = static_cast<int>(std::lround(m_params.afcRangeHz / binHz));
    const int   centreBin    = static_cast<int>(std::lround(nominalSpace / binHz));

    float bestScore = 0.0f;
    int   bestBin   = centreBin;
    for (int b = centreBin - range; b <= centreBin + range; ++b) {
        if (b < 1 || b + shiftBins >= kAfcSize / 2)
            continue;
        const float score = m_afcSpectrum[static_cast<size_t>(b)]
                          + m_afcSpectrum[static_cast<size_t>(b + shiftBins)];
        if (score > bestScore) {
            bestScore = score;
            bestBin   = b;
        }
    }

    if (bestScore <= 0.0f)
        return;

    // Parabolic interpolation around the space tone for sub-bin resolution.
    float refined = static_cast<float>(bestBin);
    if (bestBin > 0 && bestBin + 1 < kAfcSize / 2) {
        const float ym = m_afcSpectrum[static_cast<size_t>(bestBin - 1)];
        const float y0 = m_afcSpectrum[static_cast<size_t>(bestBin)];
        const float yp = m_afcSpectrum[static_cast<size_t>(bestBin + 1)];
        const float den = ym - 2.0f * y0 + yp;
        if (std::abs(den) > kEps)
            refined += 0.5f * (ym - yp) / den;
    }

    const float offset = refined * binHz - nominalSpace;
    const float clamped = std::clamp(offset, -m_params.afcRangeHz, m_params.afcRangeHz);

    // Move a fraction of the way each time: the tone filters get rebuilt on
    // every change, and a jumpy AFC would keep resetting their integrators.
    const float updated = m_afcOffsetHz + 0.2f * (clamped - m_afcOffsetHz);
    if (std::abs(updated - m_afcOffsetHz) > 0.5f) {
        m_afcOffsetHz = updated;
        const float mark  = m_params.markHz + m_afcOffsetHz;
        const float space = m_params.spaceHz() + m_afcOffsetHz;
        m_mark.retune(m_params.reverse ? space : mark, m_sampleRate);
        m_space.retune(m_params.reverse ? mark : space, m_sampleRate);
    }
}


RttyDemodulator::TonePair RttyDemodulator::searchTonePair(float minHz, float maxHz) const
{
    TonePair result;

    const float binHz     = static_cast<float>(m_sampleRate) / kAfcSize;
    const int   half      = kAfcSize / 2;
    const int   shiftBins = static_cast<int>(std::lround(m_params.shiftHz / binHz));
    const int   first     = std::max(1, static_cast<int>(std::lround(minHz / binHz)));
    const int   last      = std::min(half - shiftBins - 2,
                                     static_cast<int>(std::lround(maxHz / binHz)));
    if (shiftBins < 2 || last <= first)
        return result;

    // Fondo di banda per mediana e non per media: una portante forte sposta la
    // media di parecchio, e proprio nei casi in cui la soglia deve restare ferma.
    std::vector<float> sorted(m_afcSpectrum.begin() + first,
                              m_afcSpectrum.begin() + last + shiftBins);
    if (sorted.empty())
        return result;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const float floorMag = sorted[sorted.size() / 2] + kEps;

    // Il punteggio e' il PIU' DEBOLE dei due toni, non la loro somma. Con la
    // somma una portante isolata vince ogni volta — le basta essere forte il
    // doppio del rumore — mentre qui deve esserci energia in entrambe le
    // posizioni, che e' la firma di un segnale a spostamento di frequenza.
    float bestScore = 0.0f;
    int   bestBin   = -1;
    for (int b = first; b <= last; ++b) {
        const float lower = m_afcSpectrum[static_cast<size_t>(b)];
        const float upper = m_afcSpectrum[static_cast<size_t>(b + shiftBins)];
        const float score = std::min(lower, upper);
        if (score > bestScore) {
            bestScore = score;
            bestBin   = b;
        }
    }
    if (bestBin < 0)
        return result;

    const float lower = m_afcSpectrum[static_cast<size_t>(bestBin)];
    const float upper = m_afcSpectrum[static_cast<size_t>(bestBin + shiftBins)];

    // Interpolazione parabolica sul tono inferiore: i bin sono larghi quasi
    // quattro hertz, e centrarsi a mezzo bin di distanza si sente.
    float refined = static_cast<float>(bestBin);
    if (bestBin > 0 && bestBin + 1 < half) {
        const float ym  = m_afcSpectrum[static_cast<size_t>(bestBin - 1)];
        const float y0  = m_afcSpectrum[static_cast<size_t>(bestBin)];
        const float yp  = m_afcSpectrum[static_cast<size_t>(bestBin + 1)];
        const float den = ym - 2.0f * y0 + yp;
        if (std::abs(den) > kEps)
            refined += std::clamp(0.5f * (ym - yp) / den, -0.5f, 0.5f);
    }

    result.found      = true;
    result.markHz     = refined * binHz + m_params.shiftHz;
    result.marginDb   = 20.0f * std::log10(bestScore / floorMag);
    result.balanceDb  = 20.0f * std::log10((std::max(lower, upper) + kEps)
                                           / (std::min(lower, upper) + kEps));
    return result;
}

} // namespace decortty::dsp
