#include "DecodiumBridge.h"
#include "DecodiumAudioSink.h"
#include "DecodiumDecoPortGateway.h"
#include "DecoPortLink.h"
#include "Ft2LinkPcmStats.h"
#include <QDateTime>
#include <QTimer>
#include <algorithm>
#include <cmath>

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);
#define bridgeLog decodiumBridgeAudioLevelLog
#define isFt2LinkApplicationMode decodiumBridgeIsFt2LinkApplicationMode

void DecodiumBridge::ensureAudioSink()
{
    // Create the audio sink (once, reused across start/stop cycles).
    // downSampleFactor=4: QAudioSource a 48000 Hz → buffer a 12000 Hz
    if (!m_audioSink) {
        m_audioSink = new DecodiumAudioSink(m_audioBuffer, 4, this, &m_audioBufferMutex);
        // Ring buffer per il path async FT2: ogni campione decimato va anche nel buffer circolare
        // + feed campioni al WavManager per registrazione audio
        m_audioSink->setSampleCallback([this](short s) {
            // Writer single-thread (callback audio). Pubblichiamo il campione
            // prima dell'incremento di posizione con release semantics così che
            // onAsyncDecodeTimer (acquire load) veda dati validi nel range.
            // uint64_t: wrap-around well-defined dal memory model C++.
            //
            // Il commit usa compare_exchange per eliminare la race con il reset
            // m_asyncAudioPos.store(0) eseguito dal main thread al cambio modo.
            // Senza CAS, il writer potrebbe leggere w=X, il main resettare a 0,
            // e il writer committare X+1 — annullando il reset.  Con CAS, se il
            // reset avviene fra load e commit il CAS fallisce e il campione
            // viene scartato (perdita accettabile durante il boundary di modo).
            uint64_t w = m_asyncAudioPos.load(std::memory_order_acquire);
            m_asyncAudio[w % ASYNC_BUF_SIZE] = s;
            m_asyncAudioPos.compare_exchange_strong(
                w, w + 1,
                std::memory_order_release,
                std::memory_order_relaxed);
            ++m_driftExpectedFrames;  // A2: count frames for drift detection
            if (m_wavManager) m_wavManager->feedSample(s);
        });
        connect(m_audioSink, &DecodiumAudioSink::audioLevelChanged,
                this, [this](double level) {
            // A queued callback from the just-stopped CW capture must not
            // overwrite the level after the legacy backend takes ownership.
            if (usesLegacyRxLevels()) return;
            static int cnt = 0;
            if (cnt++ < 5) bridgeLog("audioLevelChanged received level=" + QString::number(level));
            m_audioLevel = level;
            emit audioLevelChanged();
        });
        connect(m_audioSink, &DecodiumAudioSink::audioHealthChanged,
                this, [this](double rms, double peak, int dynamicRange, int clippedSamples, int samples) {
            handleAudioHealth(rms, peak, dynamicRange, clippedSamples, samples);
        }, Qt::QueuedConnection);
        // DecoPort: l'audio ricevuto va anche ai client di rete. E' un
        // rubinetto a parte da quello di FT2-Link, perche' non deve dipendere
        // dal modo: il gateway pubblica quello che la radio sta ascoltando,
        // qualunque cosa stia facendo l'applicazione. Costa nulla quando il
        // gateway e' spento — prima riga, ed esce.
        connect(m_audioSink, &DecodiumAudioSink::audioSamplesReady,
                this, [this](QVector<short> samples) {
            if (!m_decoPortGateway || !m_decoPortGateway->running())
                return;
            if (samples.isEmpty() || !m_monitoring || m_transmitting || m_tuning)
                return;
            // Quello che si pubblica e' la radio attaccata a questo computer.
            // Con una radio remota in uso i campioni sono i suoi, e rimandarli
            // in giro vorrebbe dire ripubblicare la radio di qualcun altro —
            // e, se i due si collegassero a vicenda, farla girare all'infinito.
            if (m_decoPortUseRemote)
                return;
            m_decoPortGateway->setAudioFormat(static_cast<quint32>(SAMPLE_RATE),
                                              1);
            m_decoPortGateway->pushRxAudio(samples, decoport::nowUnixNs());
        });
        // RTTY: lo stesso audio, quando la sua finestra e' aperta. Rubinetto a
        // parte da quello di DecoPort perche' i due si accendono per ragioni
        // diverse e nessuno dei due deve dipendere dall'altro. Prima riga ed
        // esce quando la finestra e' chiusa, che e' quasi sempre.
        connect(m_audioSink, &DecodiumAudioSink::audioSamplesReady,
                this, [this](QVector<short> samples) {
            if (samples.isEmpty())
                return;
            if (m_mode == "SSB" && !m_decoPortUseRemote) ssbMonitorAudio(samples);
            // The CW decoder is a window-owned activity. Once the operator
            // closes that window, continuing to run the decoder and publish
            // scope updates keeps CPU/GPU work alive invisibly (and used to
            // aggravate Linux/XFCE scene-graph shutdown). An open CW window
            // may still listen while another application mode is selected.
            const bool cwTap = m_cwInAscolto && !m_transmitting && !m_tuning;
            if (cwTap) {
                m_cwAudioTapSamples += static_cast<quint64>(samples.size());
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                if (nowMs - m_lastCwAudioTapLogMs >= 2000) {
                    m_lastCwAudioTapLogMs = nowMs;
                    bridgeLog(QStringLiteral("[CW-RX] audio tap active samples=%1 total=%2 mode=%3 window=%4 tx=%5 tune=%6")
                                  .arg(samples.size())
                                  .arg(m_cwAudioTapSamples)
                                  .arg(m_mode)
                                  .arg(m_cwInAscolto ? 1 : 0)
                                  .arg(m_transmitting ? 1 : 0)
                                  .arg(m_tuning ? 1 : 0));
                }
                emit campioniRxCw(samples);
            }
            // JTTY: il ricevitore ascolta sempre quando il modo e' attivo e
            // il monitor e' acceso, anche a finestra chiusa (le righe
            // complete vanno comunque nella lista dei decodificati).
            if (m_mode == QStringLiteral("JTTY") && m_monitoring
                && !m_transmitting && !m_tuning && !m_rttyTxActive) {
                emit campioniRxJtty(samples);
                return;
            }
            if (!m_rttyInAscolto)
                return;
            // In trasmissione l'ingresso non e' la radio: dare quei campioni al
            // demodulatore vorrebbe dire decodificare la propria coda.
            if (m_transmitting || m_tuning)
                return;
            emit campioniRxRtty(samples);
        });

        connect(m_audioSink, &DecodiumAudioSink::audioSamplesReady,
                this, [this](QVector<short> samples) {
            // IU8LMC fix 1.0.449: gate FT2-Link audio tap — skip DSP entirely
            // when not in FT2-LINK mode (avoids 50x/sec main-thread DSP in FT8/FT2/FT4).
            bool const modeOk = isFt2LinkApplicationMode(m_mode);
            bool const shouldEmit = modeOk
                && !samples.isEmpty()
                && m_monitoring
                && !m_transmitting
                && !m_tuning;
            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            if (modeOk && nowMs - m_lastFt2LinkModernRxTapLogMs >= 2000) {
                m_lastFt2LinkModernRxTapLogMs = nowMs;
                double sumSquares = 0.0;
                int peak = 0;
                for (short sample : samples) {
                    int const absSample = std::abs(static_cast<int>(sample));
                    peak = std::max(peak, absSample);
                    double const normalized = static_cast<double>(sample) / 32768.0;
                    sumSquares += normalized * normalized;
                }
                double const rms = samples.isEmpty()
                    ? 0.0
                    : std::sqrt(sumSquares / static_cast<double>(samples.size()));
                qint64 const lastEmitAgeMs = m_lastFt2LinkModernRxEmitMs > 0
                    ? nowMs - m_lastFt2LinkModernRxEmitMs
                    : -1;
                bridgeLog(QStringLiteral(
                    "[Ft2Link][RXTAP] source=modern samples=%1 emit=%2 mode=%3 monitor=%4 tx=%5 tune=%6 rxSuspended=%7 sink=%8 rms=%9 peak=%10 pending=%11 scheduled=%12 lastEmitAgeMs=%13")
                    .arg(samples.size())
                    .arg(shouldEmit ? 1 : 0)
                    .arg(m_mode)
                    .arg(m_monitoring ? 1 : 0)
                    .arg(m_transmitting ? 1 : 0)
                    .arg(m_tuning ? 1 : 0)
                    .arg(m_rxAudioSuspendedForTx ? 1 : 0)
                    .arg(m_audioSink ? QStringLiteral("alive") : QStringLiteral("null"))
                    .arg(rms, 0, 'f', 4)
                    .arg(static_cast<double>(peak) / 32768.0, 0, 'f', 4)
                    .arg(m_ft2LinkModernRxPending.size())
                    .arg(m_ft2LinkModernRxDrainScheduled ? 1 : 0)
                    .arg(lastEmitAgeMs));
            }
            if (!modeOk) {
                m_ft2LinkModernRxPending.clear();
                return;
            }
            if (shouldEmit) {
                if (!samples.isEmpty()) {
                    m_ft2LinkModernRxPending += samples;
                }
                // QAudioSource can deliver tiny downsampled chunks (128 samples
                // on BlackHole/CoreAudio). Batch them before handing audio to
                // the FT2-Link worker so acquisition sees continuous windows
                // and the worker is not flooded with thousands of queued calls.
                static constexpr int kFt2LinkModernDrainMs = 200;
                static constexpr int kFt2LinkModernMaxPendingSamples = 24000;
                if (m_ft2LinkModernRxPending.size()
                    > kFt2LinkModernMaxPendingSamples) {
                    m_ft2LinkModernRxPending.erase(
                        m_ft2LinkModernRxPending.begin(),
                        m_ft2LinkModernRxPending.begin()
                            + (m_ft2LinkModernRxPending.size()
                               - kFt2LinkModernMaxPendingSamples));
                }
                if (!m_ft2LinkModernRxPending.isEmpty()
                    && !m_ft2LinkModernRxDrainScheduled) {
                    m_ft2LinkModernRxDrainScheduled = true;
                    QTimer::singleShot(kFt2LinkModernDrainMs, this, [this]() {
                        m_ft2LinkModernRxDrainScheduled = false;

                        bool const canEmit = isFt2LinkApplicationMode(m_mode)
                            && m_monitoring
                            && !m_transmitting
                            && !m_tuning;
                        if (!canEmit) {
                            m_ft2LinkModernRxPending.clear();
                            return;
                        }
                        if (m_ft2LinkModernRxPending.isEmpty()) {
                            return;
                        }

                        qint64 const drainNowMs = QDateTime::currentMSecsSinceEpoch();
                        m_lastFt2LinkModernRxEmitMs = drainNowMs;
                        QVector<short> batchedSamples;
                        batchedSamples.swap(m_ft2LinkModernRxPending);
                        if (drainNowMs - m_lastFt2LinkModernRxEmitLogMs >= 2000) {
                            m_lastFt2LinkModernRxEmitLogMs = drainNowMs;
                            Ft2LinkPcmStats const stats = ft2LinkPcmStats(batchedSamples);
                            bridgeLog(QStringLiteral(
                                "[Ft2Link][RXEMIT] source=modern samples=%1 rms=%2 peak=%3 nonzero=%4 aboveNoise=%5")
                                .arg(batchedSamples.size())
                                .arg(stats.rms, 0, 'f', 4)
                                .arg(stats.peak, 0, 'f', 4)
                                .arg(stats.nonZero)
                                .arg(stats.aboveNoise));
                        }
                        emit ft2LinkRxSamplesReady(std::move(batchedSamples),
                                                   static_cast<quint64>(drainNowMs));
                    });
                }
            } else if (!m_ft2LinkModernRxPending.isEmpty()) {
                m_ft2LinkModernRxPending.clear();
            }
        }, Qt::QueuedConnection);
#if DECODIUM_HAS_SSTV
        // The selected-source relay owns the only SSTV payload tap. It is
        // rebound with an immutable generation token and feeds the bounded
        // ingress directly from the producer thread.
        refreshSstvProducerTaps();
#endif
        bridgeLog("audioSink created and connected");
    }
}

