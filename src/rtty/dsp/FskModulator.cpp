#include "dsp/FskModulator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace decortty::dsp {

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi_v<double>;
}

FskModulator::FskModulator(const RttyParams& params, int sampleRate)
    : m_params(params)
    , m_sampleRate(sampleRate)
{
    reset();
}

void FskModulator::setParams(const RttyParams& params)
{
    m_params = params;
}

void FskModulator::setSampleRate(int rate)
{
    m_sampleRate = std::max(8000, rate);
}

void FskModulator::reset()
{
    m_bitCount        = 0;
    m_bitIndex        = 0;
    m_bitsLeft        = 0;
    m_samplesIntoBit  = 0.0;
    m_currentBitSamples = 0.0;
    m_phase           = 0.0;
    m_encodeShift     = Shift::Letters;
    m_txShift         = Shift::Letters;
    m_echoChar        = 0;
    m_pendingEcho     = 0;
}

void FskModulator::enqueueText(const std::string& text)
{
    Shift endShift = m_encodeShift;
    const auto codes = encodeBaudot(text, m_encodeShift, endShift, m_params.figuresSet);
    // encodeBaudot assumes the receiver honours unshift-on-space. When the
    // operator has USOS off, re-encode without that assumption by simply
    // trusting the shift the encoder ended in.
    for (const uint8_t c : codes)
        m_codes.push_back(c);
    m_encodeShift = endShift;
}

void FskModulator::enqueueCode(uint8_t code)
{
    code &= 0x1F;
    if (code == kBaudotLtrs)
        m_encodeShift = Shift::Letters;
    else if (code == kBaudotFigs)
        m_encodeShift = Shift::Figures;
    m_codes.push_back(code);
}

void FskModulator::clearQueue()
{
    m_codes.clear();
}

char FskModulator::takeEchoChar()
{
    const char c = m_echoChar;
    m_echoChar = 0;
    return c;
}

void FskModulator::loadNextCode()
{
    uint8_t code = 0;
    if (!m_codes.empty()) {
        code = m_codes.front();
        m_codes.pop_front();
    } else if (m_diddleMode == DiddleMode::Ltrs) {
        code = kBaudotLtrs;   // il riempimento classico: tiene il sincronismo
    } else if (m_diddleMode == DiddleMode::Blank) {
        code = kBaudotNull;   // invisibile anche su una telescrivente meccanica
    } else {
        m_bitsLeft = 0;
        return;
    }

    // Lo shift della trasmissione segue i codici mentre escono, esattamente come
    // fara' il decodificatore all'altro capo.
    if (code == kBaudotLtrs)
        m_txShift = Shift::Letters;
    else if (code == kBaudotFigs)
        m_txShift = Shift::Figures;
    else if (code == kBaudotSpace && m_params.unshiftOnSpace)
        m_txShift = Shift::Letters;

    // Il carattere che l'operatore vede nell'eco. LTRS e FIGS non stampano
    // nulla, e il diddle non deve mai comparire nella finestra.
    m_pendingEcho = baudotToChar(code, m_txShift, m_params.figuresSet);
    if (code == kBaudotLtrs || code == kBaudotFigs)
        m_pendingEcho = 0;

    const float samplesPerBit = m_params.samplesPerBit(m_sampleRate);

    m_bitCount = 0;
    // Start bit: space.
    m_bitValue[m_bitCount]  = false;
    m_bitLength[m_bitCount] = 1.0f;
    ++m_bitCount;
    // Bit di dati, il meno significativo per primo.
    for (int b = 0; b < 5; ++b) {
        m_bitValue[m_bitCount]  = (code & (1u << b)) != 0;
        m_bitLength[m_bitCount] = 1.0f;
        ++m_bitCount;
    }
    // Stop: mark, da 1 a 2 tempi di bit secondo lo standard in uso, più
    // l'eventuale attesa dopo il carattere — che è mark anch'essa, quindi si
    // somma allo stop invece di occupare un elemento a sé.
    m_bitValue[m_bitCount]  = true;
    m_bitLength[m_bitCount] = std::max(1.0f, m_params.stopBits) + m_charWaitBits;
    ++m_bitCount;

    m_bitIndex          = 0;
    m_bitsLeft          = m_bitCount;
    m_samplesIntoBit    = 0.0;
    m_currentBitSamples = static_cast<double>(samplesPerBit) * m_bitLength[0];
}

bool FskModulator::generate(float* out, int count)
{
    if (m_bitsLeft == 0 && m_codes.empty() && m_diddleMode == DiddleMode::None) {
        std::fill(out, out + count, 0.0f);
        return false;
    }

    const double markHz  = m_params.reverse ? m_params.spaceHz() : m_params.markHz;
    const double spaceHz = m_params.reverse ? m_params.markHz : m_params.spaceHz();

    for (int i = 0; i < count; ++i) {
        if (m_bitsLeft == 0) {
            loadNextCode();
            if (m_bitsLeft == 0) {
                std::fill(out + i, out + count, 0.0f);
                return false;
            }
        }

        const bool   isMark = m_bitValue[m_bitIndex];
        const double hz     = isMark ? markHz : spaceHz;

        m_phase += kTwoPi * hz / static_cast<double>(m_sampleRate);
        if (m_phase >= kTwoPi)
            m_phase -= kTwoPi;

        out[i] = m_amplitude * static_cast<float>(std::sin(m_phase));

        if (++m_samplesIntoBit >= m_currentBitSamples) {
            m_samplesIntoBit -= m_currentBitSamples;
            ++m_bitIndex;
            --m_bitsLeft;
            if (m_bitsLeft > 0) {
                m_currentBitSamples = static_cast<double>(m_params.samplesPerBit(m_sampleRate))
                                    * m_bitLength[m_bitIndex];
            } else if (m_pendingEcho) {
                // The character is fully on the air now — release it to the UI.
                m_echoChar    = m_pendingEcho;
                m_pendingEcho = 0;
            }
        }
    }
    return true;
}

} // namespace decortty::dsp
