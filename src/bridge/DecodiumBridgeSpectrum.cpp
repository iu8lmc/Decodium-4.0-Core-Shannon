#include "DecodiumBridge.h"
#include "Network/RemoteCommandServer.hpp"
#include "Detector/FftCompat.hpp"
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

// Simple radix-2 in-place FFT (Cooley-Tukey)
static void fft_inplace(std::vector<std::complex<float>>& a)
{
    int n = (int)a.size();
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * 3.14159265f / len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                auto u = a[i+j], v = a[i+j+len/2] * w;
                a[i+j]         = u + v;
                a[i+j+len/2]   = u - v;
                w *= wlen;
            }
        }
    }
}

QVector<float> DecodiumBridge::computeSpectrum() const
{
    const int N = SPECTRUM_FFT_SIZE; // 512
    if (m_spectrumBuf.size() < N) return {};

    // Hanning window + FFT
    std::vector<std::complex<float>> fftBuf(N);
    for (int i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (N - 1)));
        fftBuf[i] = std::complex<float>(m_spectrumBuf[i] * w, 0.0f);
    }
    fft_inplace(fftBuf);

    // Magnitudes in dB: only positive frequencies (N/2 bins)
    // Frequency resolution: SAMPLE_RATE/N = 12000/512 ≈ 23.4 Hz/bin
    // We want 200-3000 Hz: bins 9 to 128
    const float sampleRate  = SAMPLE_RATE;
    const float freqPerBin  = sampleRate / N;
    const float freqMin     = m_nfa;    // 200 Hz
    const float freqMax     = m_nfb;    // 4000 Hz

    int binStart = (int)(freqMin / freqPerBin);
    int binEnd   = qMin((int)(freqMax / freqPerBin), N / 2 - 1);
    int nBins    = binEnd - binStart;
    if (nBins <= 0) return {};

    // Convert to dB and normalize
    float refLevel = (float)m_spectrumRefLevel;   // e.g. -10 dB
    float dynRange = (float)m_spectrumDynRange;    // e.g. 70 dB
    float floorDb  = refLevel - dynRange;

    QVector<float> result;
    result.reserve(nBins);
    for (int b = binStart; b < binEnd; ++b) {
        float mag = std::abs(fftBuf[b]) / (N / 2.0f);
        float db  = (mag > 1e-10f) ? 20.0f * std::log10(mag) : -200.0f;
        float norm = qBound(0.0f, (db - floorDb) / dynRange, 1.0f);
        result.append(norm);
    }
    return result;
}

void DecodiumBridge::publishRemoteWaterfallFrame(const QVector<float>& dbValues,
                                                 float minDb,
                                                 float maxDb,
                                                 float freqMinHz,
                                                 float freqMaxHz)
{
    if (!m_remoteServer || !m_remoteServer->isRunning() || !m_remoteServer->waterfallEnabled())
        return;
    if (dbValues.isEmpty() || !std::isfinite(freqMinHz) || !std::isfinite(freqMaxHz) || freqMaxHz <= freqMinHz)
        return;

    float lo = minDb;
    float hi = maxDb;
    if (!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo) {
        auto const [minIt, maxIt] = std::minmax_element(dbValues.cbegin(), dbValues.cend());
        if (minIt == dbValues.cend() || maxIt == dbValues.cend() || *maxIt <= *minIt)
            return;
        lo = *minIt;
        hi = *maxIt;
    }

    QByteArray row;
    row.resize(dbValues.size());
    float const scale = 255.0f / qMax(1.0f, hi - lo);
    for (int i = 0; i < dbValues.size(); ++i) {
        float const value = std::isfinite(dbValues[i]) ? dbValues[i] : lo;
        int const level = qBound(0, qRound((value - lo) * scale), 255);
        row[i] = static_cast<char>(level);
    }

    m_remoteServer->publishWaterfallRow(row,
                                        qRound(freqMinHz),
                                        qMax(1, qRound(freqMaxHz - freqMinHz)),
                                        m_rxFrequency,
                                        m_txFrequency,
                                        m_mode);
}

void DecodiumBridge::finishPanadapterFrame(QVector<float> values,
                                           float minDb,
                                           float maxDb,
                                           int usableSamples,
                                           float freqMinHz,
                                           float freqMaxHz,
                                           uint64_t serial)
{
    m_panadapterComputeBusy.store(false);

    if (serial != m_panadapterComputeSerial.load())
        return;
    if (!m_monitoring || m_transmitting || m_tuning)
        return;
    if (values.isEmpty())
        return;

    // Smooth con l'ultimo frame per transizioni uniformi
    if (m_lastPanadapterData.size() == values.size()) {
        float const alpha = (usableSamples >= PANADAPTER_FFT_SIZE) ? 1.0f : 0.7f;
        float const beta = 1.0f - alpha;
        for (int i = 0; i < values.size(); ++i)
            values[i] = alpha * values[i] + beta * m_lastPanadapterData[i];
    }

    m_lastPanadapterData = values;
    m_lastPanMinDb = minDb;
    m_lastPanMaxDb = maxDb;
    m_lastPanFreqMin = freqMinHz;
    m_lastPanFreqMax = freqMaxHz;
    publishRemoteWaterfallFrame(values, minDb, maxDb, freqMinHz, freqMaxHz);
    emit panadapterDataReady(values, minDb, maxDb, freqMinHz, freqMaxHz);
}


// ─── Alta risoluzione: FFTW 4096 bin, output in dB raw ─────────────────────
DecodiumBridge::PanadapterFrameResult DecodiumBridge::computePanadapterFrame(const QVector<short>& samples,
                                                                             int nfa,
                                                                             int nfb)
{
    const int N = PANADAPTER_FFT_SIZE; // 4096 → 2.93 Hz/bin @ 12kHz
    if (samples.size() < N) return {};

    static const QVector<float> blackmanHarrisWindow = [] {
        QVector<float> window(PANADAPTER_FFT_SIZE);
        constexpr float a0 = 0.35875f;
        constexpr float a1 = 0.48829f;
        constexpr float a2 = 0.14128f;
        constexpr float a3 = 0.01168f;
        constexpr float twoPi = 6.2831853071795864769f;
        for (int i = 0; i < PANADAPTER_FFT_SIZE; ++i) {
            float const phase = twoPi * static_cast<float>(i) / static_cast<float>(PANADAPTER_FFT_SIZE - 1);
            window[i] = a0
                      - a1 * std::cos(phase)
                      + a2 * std::cos(2.0f * phase)
                      - a3 * std::cos(3.0f * phase);
        }
        return window;
    }();

    // Usa FFTW se disponibile, altrimenti fallback al DFT interno
#ifdef FFTW3_SINGLE_FOUND
    struct FftwWorkspace {
        float* in = nullptr;
        fftwf_complex* out = nullptr;
        fftwf_plan plan = nullptr;
        int size = 0;

        ~FftwWorkspace()
        {
            if (plan)
                decodium::fft_compat::destroy_plan(plan);
            if (in)
                fftwf_free(in);
            if (out)
                fftwf_free(out);
        }

        bool ensure(int n)
        {
            if (size == n && in && out && plan)
                return true;

            if (plan) {
                decodium::fft_compat::destroy_plan(plan);
                plan = nullptr;
            }
            if (in) {
                fftwf_free(in);
                in = nullptr;
            }
            if (out) {
                fftwf_free(out);
                out = nullptr;
            }

            in = fftwf_alloc_real(n);
            out = fftwf_alloc_complex(n / 2 + 1);
            if (!in || !out)
                return false;

            static QMutex plannerMutex;
            QMutexLocker plannerLock(&plannerMutex);
            plan = decodium::fft_compat::plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE);
            size = plan ? n : 0;
            return plan != nullptr;
        }
    };

    thread_local FftwWorkspace fft;
    if (!fft.ensure(N))
        return {};

    static std::atomic_bool loggedFastPath {false};
    bool expected = false;
    if (loggedFastPath.compare_exchange_strong(expected, true)) {
        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT path active"
            << "engine=FFTW"
            << "workspace=thread_local"
            << "window=precomputed_blackman_harris"
            << "post=power_db_no_sqrt";
    }

    // Normalize raw int16 samples before the FFT. The display calibration
    // below then keeps CPU backends on Decodium's established panadapter scale.
    constexpr float pcmFullScale = 1.0f / 32768.0f;
    for (int i = 0; i < N; ++i)
        fft.in[i] = static_cast<float>(samples[i]) * pcmFullScale * blackmanHarrisWindow[i];

    fftwf_execute(fft.plan);

    const float freqPerBin = (float)SAMPLE_RATE / N;
    int binStart = (int)(nfa / freqPerBin);
    int binEnd   = qMin((int)(nfb / freqPerBin), N/2);
    int nBins    = binEnd - binStart;
    if (nBins <= 0) return {};

    QVector<float> result(nBins);
    float minDb =  200.f, maxDb = -200.f;
    constexpr float powerFloor = 1e-24f;
    constexpr float displayCalibrationDb = -40.0f;
    float const fftNorm = N / 2.0f;
    float const inverseNormSquared = 1.0f / (fftNorm * fftNorm);
    for (int b = binStart; b < binEnd; ++b) {
        float const re = fft.out[b][0];
        float const im = fft.out[b][1];
        float const normalizedPower = (re * re + im * im) * inverseNormSquared;
        float const db = (normalizedPower > powerFloor)
            ? 10.0f * std::log10(normalizedPower) + displayCalibrationDb
            : -200.0f;
        result[b - binStart] = db;
        if (db > -190.f) { minDb = qMin(minDb, db); maxDb = qMax(maxDb, db); }
    }
    return {std::move(result), minDb, maxDb};
#else
    // Fallback: DFT interno — molto più lento ma funzionale
    static std::atomic_bool loggedFallbackPath {false};
    bool expected = false;
    if (loggedFallbackPath.compare_exchange_strong(expected, true)) {
        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT path active"
            << "engine=internal_radix2"
            << "window=precomputed_blackman_harris"
            << "post=power_db_no_sqrt";
    }

    std::vector<std::complex<float>> fftBuf(N);
    constexpr float pcmFullScale = 1.0f / 32768.0f;
    for (int i = 0; i < N; ++i)
        fftBuf[i] = std::complex<float>(
            static_cast<float>(samples[i]) * pcmFullScale * blackmanHarrisWindow[i], 0.f);

    fft_inplace(fftBuf);
    const float freqPerBin = (float)SAMPLE_RATE / N;
    int binStart = (int)(nfa / freqPerBin);
    int binEnd   = qMin((int)(nfb / freqPerBin), N/2-1);
    int nBins    = binEnd - binStart;
    if (nBins <= 0) return {};
    QVector<float> result(nBins);
    float minDb = 200.f, maxDb = -200.f;
    constexpr float powerFloor = 1e-24f;
    constexpr float displayCalibrationDb = -40.0f;
    float const fftNorm = N / 2.0f;
    float const inverseNormSquared = 1.0f / (fftNorm * fftNorm);
    for (int b = binStart; b < binEnd; ++b) {
        float const normalizedPower = std::norm(fftBuf[b]) * inverseNormSquared;
        float const db = (normalizedPower > powerFloor)
            ? 10.0f * std::log10(normalizedPower) + displayCalibrationDb
            : -200.0f;
        result[b - binStart] = db;
        if (db > -190.f) { minDb = qMin(minDb, db); maxDb = qMax(maxDb, db); }
    }
    return {std::move(result), minDb, maxDb};
#endif
}

