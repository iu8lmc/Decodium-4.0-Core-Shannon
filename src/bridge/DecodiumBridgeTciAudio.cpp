#include "DecodiumBridge.h"
#include "widgets/itoneAndicw.h"
#include <QDateTime>
#include <QMutexLocker>
#include <cmath>

extern void decodiumBridgeLogbookLog(const QString&);
extern void decodiumBridgeStorePrecomputedTxWave(QString const&, QVector<float> const&);
extern bool decodiumBridgeIsFt2LinkApplicationMode(QString);

bool DecodiumBridge::usingTciAudioInput() const
{
    // 1.0.537 iu8lmc - conta il trasporto realmente in uso, non solo
    // l'etichetta del backend scelta nelle impostazioni. Un rig "TCI Client
    // RX1" si collega con portType=tci anche quando il selettore dice
    // "hamlib": prima di questa correzione l'audio TCI restava spento in
    // silenzio pur essendo il collegamento gia' su TCI e l'opzione attiva.
    // Il flag tci__audio nel gestore CAT usa gia' portType: le due
    // condizioni erano incoerenti fra loro.
    if (!m_hamlibCat || !m_hamlibCat->tciAudioEnabled()
        || !m_hamlibCat->connected()) {
        return false;
    }
    return m_catBackend == QStringLiteral("tci")
        || 0 == m_hamlibCat->portType().compare(QStringLiteral("tci"), Qt::CaseInsensitive);
}

bool DecodiumBridge::startTciTxAudioStream(QVector<float> const& wave, QString const& mode,
                                           unsigned symbolsLength, double framesPerSymbol,
                                           double frequency, double toneSpacing,
                                           bool synchronize, double periodSeconds)
{
    if (!m_hamlibCat || !usingTciAudioInput() || !m_hamlibCat->connected()) {
        return false;
    }

    if (toneSpacing < 0.0) {
        if (wave.isEmpty()) {
            decodiumBridgeLogbookLog(QStringLiteral("TCI TX audio stream refused: empty precomputed wave mode=%1").arg(mode));
            return false;
        }
        m_hamlibCat->setRigTune(false);
        decodiumBridgeStorePrecomputedTxWave(mode, wave);
    } else {
        itone[0] = 0;
    }

    m_hamlibCat->startRigTxAudio(mode,
                                 symbolsLength,
                                 framesPerSymbol,
                                 frequency,
                                 toneSpacing,
                                 synchronize,
                                 false,
                                 99.0,
                                 qMax(0.1, periodSeconds));
    decodiumBridgeLogbookLog(QStringLiteral("TCI TX audio stream armed: mode=%1 freq=%2 toneSpacing=%3 samples=%4")
                  .arg(mode)
                  .arg(frequency, 0, 'f', 1)
                  .arg(toneSpacing, 0, 'f', 1)
                  .arg(wave.size()));
    return true;
}

bool DecodiumBridge::startTciTuneAudioStream(double frequency)
{
    if (!m_hamlibCat || !usingTciAudioInput() || !m_hamlibCat->connected()) {
        return false;
    }

    itone[0] = 0;
    m_hamlibCat->setRigTune(true);
    m_hamlibCat->startRigTxAudio(QStringLiteral("FT8"),
                                 79,
                                 1920.0,
                                 frequency,
                                 0.0,
                                 false,
                                 false,
                                 99.0,
                                 15.0);
    decodiumBridgeLogbookLog(QStringLiteral("TCI TUNE audio stream armed: freq=%1").arg(frequency, 0, 'f', 1));
    return true;
}

void DecodiumBridge::stopTciTxAudioStream(bool quick)
{
    if (!m_hamlibCat || !usingTciAudioInput()) {
        return;
    }

    m_hamlibCat->stopRigTxAudio(quick);
    m_hamlibCat->setRigTune(false);
    decodiumBridgeLogbookLog(QStringLiteral("TCI TX audio stream stopped: quick=%1").arg(quick ? 1 : 0));
}

void DecodiumBridge::startTciAudioCapture()
{
    if (!m_hamlibCat || !usingTciAudioInput()) {
        return;
    }

    double const periodSeconds = qMax(0.1, periodMsForMode(m_mode) / 1000.0);
    m_tciAudioCaptureActive = true;
    m_rxAudioSuspendedForTx = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;
    m_lastTciAudioLogMs = 0;

    m_hamlibCat->setRigAudio(true, periodSeconds, 6912 / 2);
    emit statusMessage(QStringLiteral("Audio RX via TCI"));
    decodiumBridgeLogbookLog(QStringLiteral("TCI audio capture started: period=%1 mode=%2")
                  .arg(periodSeconds, 0, 'f', 3)
                  .arg(m_mode));
}

void DecodiumBridge::stopTciAudioCapture()
{
    if (m_hamlibCat && m_tciAudioCaptureActive) {
        m_hamlibCat->setRigAudio(false, qMax(0.1, periodMsForMode(m_mode) / 1000.0), 6912 / 2);
    }
    if (m_tciAudioCaptureActive) {
        decodiumBridgeLogbookLog(QStringLiteral("TCI audio capture stopped"));
    }
    m_tciAudioCaptureActive = false;
    m_rxAudioSuspendedForTx = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;
}

void DecodiumBridge::onTciPcmSamplesReady(const QVector<short>& samples)
{
    if (samples.isEmpty()
        || !m_monitoring
        || !m_tciAudioCaptureActive
        || !usingTciAudioInput()
        || m_transmitting
        || m_tuning) {
        return;
    }

    double sumSq = 0.0;
    int minSample = 32767;
    int maxSample = -32768;
    int peakAbs = 0;
    int clippedSamples = 0;
    for (short sample : samples) {
        int const value = sample;
        int const absSample = value < 0 ? -value : value;
        sumSq += static_cast<double>(value) * static_cast<double>(value);
        if (value < minSample) minSample = value;
        if (value > maxSample) maxSample = value;
        if (absSample > peakAbs) peakAbs = absSample;
        if (absSample >= 32700) ++clippedSamples;
    }

    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer += samples;
    }

    for (short sample : samples) {
        uint64_t w = m_asyncAudioPos.load(std::memory_order_acquire);
        m_asyncAudio[w % ASYNC_BUF_SIZE] = sample;
        m_asyncAudioPos.compare_exchange_strong(
            w, w + 1,
            std::memory_order_release,
            std::memory_order_relaxed);
        if (m_wavManager) {
            m_wavManager->feedSample(sample);
        }
    }
    m_driftExpectedFrames.fetch_add(samples.size(), std::memory_order_relaxed);

    double const rms = std::sqrt(sumSq / samples.size()) / 32768.0;
    double const peak = static_cast<double>(peakAbs) / 32768.0;
    int const dynamicRange = maxSample - minSample;
    m_audioLevel = rms;
    emit audioLevelChanged();
    handleAudioHealth(rms, peak, dynamicRange, clippedSamples, samples.size());
    // 1.0.469 iu8lmc — FIX busy-loop 91% CPU (thread Ft2LinkDecode gira in
    // FT8/FT2). Questo emit del path TCI/WebSDR (onTciPcmSamplesReady) era
    // l'UNICO dei tre feed RX senza gate sul mode: alimentava il worker
    // FT2-Link in QUALSIASI mode. Dal 1.0.466 (estimateNarrowCenter: 88 iter
    // su buffer 20s ogni 200ms) su canale WebSDR sempre rumoroso girava a
    // piena CPU affamando il decode. Ora gated come l'audioSink (38089) e il
    // legacy (37617): fuori da FT2-Link il worker riceve zero feed.
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode) && m_monitoring
        && !m_transmitting && !m_tuning) {
        emit ft2LinkRxSamplesReady(samples,
                                   static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastTciAudioLogMs > 5000) {
        m_lastTciAudioLogMs = nowMs;
        decodiumBridgeLogbookLog(QStringLiteral("TCI audio samples: chunk=%1 buffered=%2 rms=%3 peak=%4 range=%5 clipped=%6")
                      .arg(samples.size())
                      .arg(m_audioBuffer.size())
                      .arg(rms, 0, 'g', 4)
                      .arg(peak, 0, 'g', 4)
                      .arg(dynamicRange)
                      .arg(clippedSamples));
    }
}

