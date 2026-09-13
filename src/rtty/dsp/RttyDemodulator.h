// DecoRTTY — FSK front end: matched tone filters, AGC, timing recovery and
// UART framing, producing soft-decision character frames.
#pragma once

#include "dsp/Fft.h"
#include "dsp/RttyTypes.h"

#include <complex>
#include <functional>
#include <vector>

namespace decortty::dsp {

// Demodulates one RTTY signal out of a real-valued audio stream.
//
// Signal path, per sample:
//   audio → two complex matched filters (mark, space) → magnitudes →
//   AGC-normalised difference → edge-locked timing → UART framing.
//
// The matched filters are sliding integrations of the input mixed down by each
// tone, one bit period long. That is the optimum non-coherent FSK detector: at
// 45.45 baud with a 170 Hz shift the tones sit almost four bit-rates apart, so a
// bit-length integration separates them cleanly while averaging away most of the
// noise.
//
// Timing is recovered from the data transitions themselves. Every zero crossing
// of the normalised difference is interpolated to sub-sample resolution and
// compared against where the bit boundary was predicted; a PI loop then corrects
// both the sampling instant and the estimated symbol period. That tracks the
// several-percent clock error a real transmitter can have without ever needing
// the nominal baud rate to be exactly right.
class RttyDemodulator {
public:
    explicit RttyDemodulator(const RttyParams& params = {}, int sampleRate = kWorkRate);

    void setParams(const RttyParams& params);
    const RttyParams& params() const { return m_params; }

    // Restart timing and framing without discarding the tuning.
    void reset();

    // Butta via la stima spettrale accumulata. Serve al rientro dalla
    // trasmissione: la nota che il monitor della radio rimanda nell'ingresso e'
    // il segnale piu' forte che ci sia, e resta nella media per qualche secondo
    // dopo che il trasmettitore ha smesso. Una ricerca fatta in quel momento
    // troverebbe la propria eco e ci porterebbe sopra la sintonia.
    void clearSpectrum();

    // Feed audio. Every completed character frame goes to the callback.
    void process(const float* samples, int count,
                 const std::function<void(const SoftFrame&)>& onFrame);

    // ── live telemetry, for the UI ──────────────────────────────────────
    float markLevel() const   { return m_markLevelSmooth; }
    float spaceLevel() const  { return m_spaceLevelSmooth; }
    float snrDb() const       { return m_snrDb; }
    float afcOffsetHz() const { return m_afcOffsetHz; }
    // Measured symbol rate, which drifts from the nominal as the loop tracks
    // the far-end clock. Shown in the status bar as a tuning aid.
    float measuredBaud() const {
        return m_spbEst > 1.0 ? static_cast<float>(m_sampleRate / m_spbEst) : 0.0f;
    }
    bool  locked() const { return m_lockCounter > kLockThreshold; }
    // Mark/space separation in dB — what the squelch gates on, and a far
    // better signal-present indicator for the UI than raw audio level.
    float toneSeparationDb() const { return m_toneRatioDb; }
    // ── ricerca del segnale ─────────────────────────────────────────────
    // Dove sta davvero la coppia di toni, cercata su tutta la banda invece che
    // nell'intorno della sintonia corrente. L'AFC corregge la deriva; questa
    // risponde a una domanda diversa — «su che frequenza e' il segnale?» — e
    // serve quando la risposta e' «non dove stai guardando».
    struct TonePair {
        bool  found{false};
        float markHz{0.0f};
        // Di quanto il piu' debole dei due toni supera il fondo di banda: e' la
        // misura che dice se vale la pena spostarsi.
        float marginDb{0.0f};
        // Squilibrio fra i due toni. In RTTY, mediati su qualche decimo di
        // secondo, mark e space hanno energia simile; una portante sola no.
        float balanceDb{0.0f};
    };
    TonePair searchTonePair(float minHz, float maxHz) const;

    // The most recent mark/space pair as an XY-scope point (the crossed-ellipse
    // display every RTTY operator tunes by). Roughly +/-1 after AGC.
    void scopePoint(float& x, float& y) const { x = m_scopeX; y = m_scopeY; }

private:
    static constexpr int kLockThreshold = 8;   // good frames before we claim lock
    // Frames held back while the decoder decides whether this is really RTTY.
    // Long enough to cover the run-up to lock, so a genuine transmission is
    // printed from its first character rather than from wherever we happened to
    // become confident.
    static constexpr size_t kMaxPending = 12;
    // How long the flywheel keeps spinning through idle mark before giving up
    // and hunting again — about a second and a half at 45.45 baud.
    static constexpr int kMaxIdleSlots  = 64;

    struct ToneFilter {
        std::complex<float>              osc{1.0f, 0.0f};
        std::complex<float>              step{1.0f, 0.0f};
        std::vector<std::complex<float>> history;
        std::complex<float>              running{0.0f, 0.0f};
        int                              pos{0};
        int                              len{0};

        void  configure(float hz, int rate, int lengthSamples);
        // Change the tone without discarding the integrator. AFC nudges the
        // filters while a character is in flight; rebuilding them there would
        // blank the history and corrupt the character being received — which
        // shows up as one mangled letter every few seconds, exactly the sort of
        // fault that is maddening to chase from the screen.
        void  retune(float hz, int rate);
        float push(float sample);   // magnitude of the sliding integration
    };

    void rebuildFilters();
    void sampleBit(float soft, float llr, bool squelchOpen,
                   const std::function<void(const SoftFrame&)>& onFrame);
    void trackEdge(double excess);
    void updateSpectrum(float sample);
    void steerAfc();

    RttyParams m_params;
    int        m_sampleRate{kWorkRate};

    ToneFilter m_mark;
    ToneFilter m_space;

    // Levels and AGC
    float m_markLevelSmooth{0.0f};
    float m_spaceLevelSmooth{0.0f};
    float m_agc{1e-6f};
    float m_noiseFloor{1e-6f};
    float m_snrDb{0.0f};
    float m_toneRatioDb{0.0f};
    bool  m_squelchOpen{false};
    // Fraction of recent time spent on the space tone. Real RTTY alternates
    // constantly and sits near a third; a carrier, a CW signal or a data mode
    // parked on the mark filter never visits space at all. Without this test a
    // steady carrier looks, to a tone-separation squelch, exactly like a
    // perfectly solid signal.
    float m_spaceFraction{0.0f};
    float m_scopeX{0.0f};
    float m_scopeY{0.0f};

    // Timing
    double m_spbNominal{176.0};
    double m_spbEst{176.0};
    double m_bitTimer{0.0};     // samples until the next mid-bit sampling point
    float  m_prevSoft{0.0f};
    int    m_samplesSinceEdge{0};
    float  m_bitPhase{0.0f};

    // Characters decoded but not yet released, waiting for the decoder to be
    // sure this is really RTTY.
    std::vector<SoftFrame> m_pending;

    // Framing
    // Hunt  — no timing lock: wait for a mark→space edge.
    // Frame — clocking the seven bits of a character out.
    // Sync  — locked and between characters, running the flywheel.
    enum class State { Hunt, Frame, Sync };
    State   m_state{State::Hunt};
    int     m_bitIndex{-1};     // -1 = start bit, 0..4 = data, 5 = stop
    float   m_frameBits[8]{};   // fino a 8 bit di dati per l'ASCII
    float   m_startConfidence{0.0f};
    float   m_stopConfidence{0.0f};
    float   m_frameSnr{0.0f};
    int     m_lockCounter{0};
    int     m_idleSlots{0};
    int64_t m_sampleIndex{0};
    int64_t m_frameStartIndex{0};

    // Stima spettrale lenta, tenuta aggiornata sempre: l'AFC ci si appoggia per
    // seguire la deriva, e la ricerca del segnale per sapere dove guardare.
    Fft                              m_afcFft;
    std::vector<float>               m_afcWindow;
    std::vector<float>               m_afcBuffer;
    std::vector<std::complex<float>> m_afcScratch;
    std::vector<float>               m_afcSpectrum;
    int                              m_afcFill{0};
    float                            m_afcOffsetHz{0.0f};
};

} // namespace decortty::dsp
