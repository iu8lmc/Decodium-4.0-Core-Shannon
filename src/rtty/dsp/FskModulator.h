// DecoRTTY — AFSK transmitter: text → Baudot → phase-continuous audio tones.
#pragma once

#include "dsp/RttyTypes.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>

namespace decortty::dsp {

// Generates the audio a Flex slice in DIGU/USB turns into an on-air RTTY
// signal. The oscillator phase carries across every tone change, which is what
// keeps the transmitted spectrum narrow — a modulator that restarted the phase
// at each bit would splatter several hundred hertz either side of the shift.
class FskModulator {
public:
    explicit FskModulator(const RttyParams& params = {}, int sampleRate = 24000);

    void setParams(const RttyParams& params);
    void setSampleRate(int rate);
    void setAmplitude(float amplitude) { m_amplitude = amplitude; }
    float amplitude() const { return m_amplitude; }

    // Cosa mandare quando la coda si svuota ma il trasmettitore resta su.
    // Riempire tiene agganciato il decodificatore all'altro capo mentre
    // l'operatore pensa alla frase successiva; smettere lo lascia scivolare.
    void setDiddle(bool on) { m_diddleMode = on ? DiddleMode::Ltrs : DiddleMode::None; }
    void setDiddleMode(DiddleMode mode) { m_diddleMode = mode; }
    DiddleMode diddleMode() const { return m_diddleMode; }
    bool diddle() const { return m_diddleMode != DiddleMode::None; }

    // Pausa aggiuntiva dopo ogni carattere, in bit. Serve alle telescriventi
    // meccaniche, che dopo un ritorno a capo hanno bisogno di tempo per portare
    // il carrello a inizio riga: senza attesa il carattere successivo si perde.
    void setCharacterWaitBits(float bits) { m_charWaitBits = std::max(0.0f, bits); }
    float characterWaitBits() const { return m_charWaitBits; }

    // Queue text. Non-ITA2 characters are dropped; the shift state carries
    // across calls so a figure group split over two calls still works.
    void enqueueText(const std::string& text);
    // Queue raw 5-bit codes (macro expansion, LTRS preamble, …).
    void enqueueCode(uint8_t code);

    void clearQueue();
    // Text queued but not yet modulated, in characters.
    size_t pending() const { return m_codes.size(); }
    // True when nothing is left to send and the current character has finished.
    bool idle() const { return m_codes.empty() && m_bitsLeft == 0; }

    // Reset the oscillator and framing. Does not touch the queue.
    void reset();

    // Fill `out` with `count` samples. Returns true while the modulator is
    // producing signal; when there is nothing to send it writes silence and
    // returns false, so the caller can decide whether to drop PTT.
    bool generate(float* out, int count);

    // The character the modulator is sending right now, for TX echo in the UI.
    // Returns 0 when nothing was completed since the last call.
    char takeEchoChar();

private:
    void loadNextCode();

    RttyParams m_params;
    int        m_sampleRate{24000};
    float      m_amplitude{0.5f};
    DiddleMode m_diddleMode{DiddleMode::Ltrs};
    float      m_charWaitBits{0.0f};

    std::deque<uint8_t> m_codes;
    // Due stati di shift, non uno. `m_encodeShift` e' dove e' arrivato
    // l'encoder mentre accodava il testo; `m_txShift` e' dove si trova la
    // trasmissione adesso, carattere per carattere. Confonderli fa mostrare
    // all'operatore le cifre come lettere — 599 diventa TOO, IU8LMC diventa
    // IUILMC — perche' condividono lo stesso codice a cinque bit.
    Shift               m_encodeShift{Shift::Letters};
    Shift               m_txShift{Shift::Letters};

    // Current character being clocked out: start bit, 5 data bits, stop bits.
    // Held as a small bit plan so the fractional stop length stays exact.
    static constexpr int kMaxBits = 8;
    bool   m_bitValue[kMaxBits]{};
    float  m_bitLength[kMaxBits]{};   // in bits, so the 1.5 stop is one entry
    int    m_bitCount{0};
    int    m_bitIndex{0};
    int    m_bitsLeft{0};
    double m_samplesIntoBit{0.0};
    double m_currentBitSamples{0.0};

    double m_phase{0.0};
    char   m_echoChar{0};
    char   m_pendingEcho{0};
};

} // namespace decortty::dsp
