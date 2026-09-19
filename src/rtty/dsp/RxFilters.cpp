#include "dsp/RxFilters.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace decortty::dsp {

namespace {

constexpr float kPi    = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;

// Finestra di Blackman: 74 dB di attenuazione fuori banda, che è quanto serve
// per far sparire una portante forte invece di attenuarla soltanto.
float blackman(int i, int n)
{
    const float t = static_cast<float>(i) / static_cast<float>(n - 1);
    return 0.42f - 0.5f * std::cos(kTwoPi * t) + 0.08f * std::cos(2.0f * kTwoPi * t);
}

// Convoluzione con storia circolare. I coefficienti sono simmetrici, quindi
// l'ordine di percorrenza non conta.
float convolve(const std::vector<float>& taps, const std::vector<float>& history, int pos)
{
    const int n = static_cast<int>(taps.size());
    float acc = 0.0f;
    int   h   = pos;
    for (int t = 0; t < n; ++t) {
        acc += taps[static_cast<size_t>(t)] * history[static_cast<size_t>(h)];
        h = (h + 1) % n;
    }
    return acc;
}

} // namespace

// ── passa-banda ─────────────────────────────────────────────────────────────

void ToneBandpass::configure(float centreHz, float widthHz, int sampleRate, int taps)
{
    taps = std::max(15, taps | 1);   // dispari: fase lineare e ritardo intero

    const float lo = std::max(20.0f, centreHz - widthHz * 0.5f) / sampleRate;
    const float hi = std::min(static_cast<float>(sampleRate) * 0.49f,
                              centreHz + widthHz * 0.5f) / sampleRate;

    m_taps.assign(static_cast<size_t>(taps), 0.0f);
    const int mid = taps / 2;
    for (int i = 0; i < taps; ++i) {
        const int n = i - mid;
        // Differenza di due passa-basso: il passa-banda è la loro sottrazione.
        const float sHi = (n == 0) ? 2.0f * hi
                                   : std::sin(kTwoPi * hi * n) / (kPi * n);
        const float sLo = (n == 0) ? 2.0f * lo
                                   : std::sin(kTwoPi * lo * n) / (kPi * n);
        m_taps[static_cast<size_t>(i)] = (sHi - sLo) * blackman(i, taps);
    }

    // Guadagno unitario al centro della banda passante, così inserire il filtro
    // non cambia il livello che l'AGC vede.
    const float centre = centreHz / sampleRate;
    float re = 0.0f, im = 0.0f;
    for (int i = 0; i < taps; ++i) {
        re += m_taps[static_cast<size_t>(i)] * std::cos(kTwoPi * centre * (i - mid));
        im += m_taps[static_cast<size_t>(i)] * std::sin(kTwoPi * centre * (i - mid));
    }
    const float gain = std::sqrt(re * re + im * im);
    if (gain > 1e-6f)
        for (float& t : m_taps)
            t /= gain;

    reset();
}

void ToneBandpass::reset()
{
    m_history.assign(m_taps.size(), 0.0f);
    m_pos = 0;
}

void ToneBandpass::process(const float* in, int count, float* out)
{
    if (m_taps.empty()) {
        if (in != out)
            std::copy(in, in + count, out);
        return;
    }
    const int n = static_cast<int>(m_taps.size());
    for (int i = 0; i < count; ++i) {
        m_history[static_cast<size_t>(m_pos)] = in[i];
        m_pos = (m_pos + 1) % n;
        out[i] = convolve(m_taps, m_history, m_pos);
    }
}

// ── notch ───────────────────────────────────────────────────────────────────

void NotchFilter::configure(float hz, float widthHz, int sampleRate, int taps)
{
    taps = std::max(15, taps | 1);
    m_hz = hz;

    const float lo = std::max(10.0f, hz - widthHz * 0.5f) / sampleRate;
    const float hi = std::min(static_cast<float>(sampleRate) * 0.49f,
                              hz + widthHz * 0.5f) / sampleRate;

    m_taps.assign(static_cast<size_t>(taps), 0.0f);
    const int mid = taps / 2;
    for (int i = 0; i < taps; ++i) {
        const int n = i - mid;
        const float sHi = (n == 0) ? 2.0f * hi : std::sin(kTwoPi * hi * n) / (kPi * n);
        const float sLo = (n == 0) ? 2.0f * lo : std::sin(kTwoPi * lo * n) / (kPi * n);
        const float band = (sHi - sLo) * blackman(i, taps);
        // Elimina-banda = tutto meno il passa-banda: l'impulso unitario al
        // centro meno la banda da togliere.
        m_taps[static_cast<size_t>(i)] = (n == 0 ? 1.0f : 0.0f) - band;
    }
    reset();
}

void NotchFilter::reset()
{
    m_history.assign(m_taps.size(), 0.0f);
    m_pos = 0;
}

void NotchFilter::process(const float* in, int count, float* out)
{
    if (m_taps.empty()) {
        if (in != out)
            std::copy(in, in + count, out);
        return;
    }
    const int n = static_cast<int>(m_taps.size());
    for (int i = 0; i < count; ++i) {
        m_history[static_cast<size_t>(m_pos)] = in[i];
        m_pos = (m_pos + 1) % n;
        out[i] = convolve(m_taps, m_history, m_pos);
    }
}

// ── LMS ─────────────────────────────────────────────────────────────────────

void LmsCanceller::configure(int taps, float mu, int delay)
{
    m_weights.assign(static_cast<size_t>(std::max(4, taps)), 0.0f);
    m_mu    = std::clamp(mu, 1e-5f, 0.05f);
    m_delay = std::max(1, delay);
    reset();
}

void LmsCanceller::reset()
{
    m_delayLine.assign(m_weights.size() + static_cast<size_t>(m_delay) + 1, 0.0f);
    m_pos = 0;
    std::fill(m_weights.begin(), m_weights.end(), 0.0f);
}

void LmsCanceller::process(const float* in, int count, float* out)
{
    if (m_weights.empty()) {
        if (in != out)
            std::copy(in, in + count, out);
        return;
    }

    const int taps = static_cast<int>(m_weights.size());
    const int len  = static_cast<int>(m_delayLine.size());

    for (int i = 0; i < count; ++i) {
        m_delayLine[static_cast<size_t>(m_pos)] = in[i];

        // Predizione dai campioni più vecchi del ritardo di decorrelazione.
        float predicted = 0.0f;
        float power     = 1e-6f;
        for (int t = 0; t < taps; ++t) {
            const int idx = ((m_pos - m_delay - t) % len + len) % len;
            const float sample = m_delayLine[static_cast<size_t>(idx)];
            predicted += m_weights[static_cast<size_t>(t)] * sample;
            power     += sample * sample;
        }

        // L'errore di predizione è l'uscita: resta ciò che non era prevedibile.
        const float error = in[i] - predicted;

        // Passo normalizzato sulla potenza dell'ingresso: senza, lo stesso mu
        // diverge su un segnale forte e non converge su uno debole.
        const float step = m_mu / power;
        for (int t = 0; t < taps; ++t) {
            const int idx = ((m_pos - m_delay - t) % len + len) % len;
            float& w = m_weights[static_cast<size_t>(t)];
            w = m_leak * w + step * error * m_delayLine[static_cast<size_t>(idx)];
        }

        out[i] = error;
        m_pos = (m_pos + 1) % len;
    }
}

} // namespace decortty::dsp
