// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvRxRuntime.h"
#include "SstvWavPcmReader.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace decodium::sstv {

// Owner-thread facade for deterministic WAV replay through the one native RX
// runtime. File I/O and PCM conversion live on a dedicated std::thread; audio
// blocks cross into SstvRxRuntime only through its direct, bounded producer
// API. No QVector audio is ever posted through the Qt event queue.
class SstvWavReplayController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY stateChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY replayChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY replayChanged)
    Q_PROPERTY(quint32 sampleRate READ sampleRate NOTIFY replayChanged)
    Q_PROPERTY(quint64 durationMs READ durationMs NOTIFY replayChanged)

public:
    struct Config final
    {
        SstvWavPcmReaderLimits readerLimits;
        quint32 tailSilenceMs {500U};
        quint32 drainTimeoutMs {120'000U};
        quint32 backpressurePollMs {10U};
        std::size_t maximumBufferedChunks {4U};
        std::size_t maximumBufferedSamples {131'072U};
        std::size_t maximumErrorCharacters {512U};
    };

    enum class State : std::uint8_t
    {
        Idle,
        Preparing,
        Replaying,
        Draining,
        Cancelling,
        Completed,
        Cancelled,
        Error,
        Shutdown,
    };
    Q_ENUM(State)

    explicit SstvWavReplayController(SstvRxRuntime* runtime,
                                     QObject* parent = nullptr);
    SstvWavReplayController(SstvRxRuntime* runtime,
                            Config config,
                            QObject* parent = nullptr);
    ~SstvWavReplayController() override;

    SstvWavReplayController(const SstvWavReplayController&) = delete;
    SstvWavReplayController& operator=(const SstvWavReplayController&) = delete;

    Config configuration() const noexcept;
    State state() const noexcept;
    bool active() const noexcept;
    bool canStart() const noexcept;
    QString stateName() const;
    double progress() const noexcept;
    QString fileName() const;
    QString lastError() const;
    quint32 sampleRate() const noexcept;
    quint64 durationMs() const noexcept;
    quint64 sessionId() const noexcept;

    // Owner-thread only. Only local file URLs (or absolute local paths encoded
    // as QUrl) are accepted; the reader subsequently canonicalizes and checks
    // the regular non-symlink file.
    Q_INVOKABLE bool startReplay(const QUrl& localFile);
    Q_INVOKABLE void cancel();
    void shutdown();

Q_SIGNALS:
    void stateChanged();
    void progressChanged();
    void replayChanged();
    void errorOccurred(QString detail);
    void replayFinished(bool completed, bool cancelled, quint64 sessionId);

private:
    enum class WorkerOutcome : std::uint8_t
    {
        Completed,
        Cancelled,
        Error,
    };

    struct WorkerResult final
    {
        WorkerOutcome outcome {WorkerOutcome::Error};
        quint64 sessionId {0U};
        quint64 chunksEnqueued {0U};
        quint64 framesRead {0U};
        QString error;
    };

    static Config validateConfig(SstvRxRuntime* runtime, Config config);
    static QString displayName(State state);
    static qint64 addBlockDuration(qint64 timestampNs,
                                   qsizetype sampleCount,
                                   quint32 sampleRate,
                                   bool* ok) noexcept;
    static quint32 streamIdForSession(quint64 sessionId) noexcept;

    bool isOnOwnerThread() const noexcept;
    bool prepareRuntime(quint32 streamId, QString* error);
    bool enqueueWithBackpressure(QVector<short> samples,
                                 quint32 sampleRate,
                                 SstvRxRouteToken token,
                                 qint64* timestampNs,
                                 quint64 sessionId,
                                 quint64 baselineDrops,
                                 quint64 baselineFailures,
                                 quint64 baselineStale,
                                 quint64* acceptedChunks,
                                 QString* error);
    bool waitForDrain(SstvRxRouteToken token,
                      quint64 sessionId,
                      quint64 acceptedChunks,
                      quint64 baselineDrops,
                      quint64 baselineFailures,
                      quint64 baselineStale,
                      QString* error);
    bool replayWasCancelled(quint64 sessionId) const noexcept;
    void workerMain(QString path,
                    SstvRxRouteToken token,
                    quint64 sessionId,
                    quint64 baselineDrops,
                    quint64 baselineFailures,
                    quint64 baselineStale) noexcept;
    void setState(State state);
    void setError(QString error);
    void scheduleProgress(quint64 sessionId, double progress) noexcept;
    void postFormat(quint64 sessionId,
                    quint32 sampleRate,
                    quint64 durationMs) noexcept;
    void postDraining(quint64 sessionId) noexcept;
    void postFinished(WorkerResult result) noexcept;
    void finishOnOwner(WorkerResult result);
    void cancelReader() noexcept;
    void joinFinishedWorker() noexcept;

    SstvRxRuntime* const m_runtime;
    const Config m_config;
    SstvWavPcmReaderLimits m_effectiveReaderLimits;

    State m_state {State::Idle};
    QString m_fileName;
    QString m_lastError;
    double m_progress {0.0};
    quint32 m_sampleRate {0U};
    quint64 m_durationMs {0U};
    quint64 m_sessionId {0U};

    std::thread m_worker;
    std::atomic_bool m_workerRunning {false};
    std::atomic_bool m_cancelRequested {false};
    std::atomic_bool m_progressPostPending {false};
    std::atomic<quint32> m_progressMillionths {0U};
    std::atomic<quint64> m_activeSession {0U};
    mutable std::mutex m_waitMutex;
    std::condition_variable m_waitChanged;
    std::mutex m_readerMutex;
    SstvWavPcmReader* m_activeReader {nullptr};
};

} // namespace decodium::sstv
