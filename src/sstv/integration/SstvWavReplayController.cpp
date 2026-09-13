// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvWavReplayController.h"

#include "../dsp/SstvResampler.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr quint64 kNanosecondsPerSecond = 1'000'000'000ULL;
constexpr quint32 kProgressScale = 1'000'000U;

QString bounded(QString value, std::size_t maximum)
{
    value.replace(QLatin1Char('\0'), QChar::ReplacementCharacter);
    return value.left(static_cast<qsizetype>(maximum));
}

} // namespace

SstvWavReplayController::SstvWavReplayController(SstvRxRuntime* runtime,
                                                 QObject* parent)
    : SstvWavReplayController(runtime, Config {}, parent)
{
}

SstvWavReplayController::SstvWavReplayController(SstvRxRuntime* runtime,
                                                 Config config,
                                                 QObject* parent)
    : QObject(parent)
    , m_runtime(runtime)
    , m_config(validateConfig(runtime, std::move(config)))
    , m_effectiveReaderLimits(m_config.readerLimits)
{
    const auto ingress = m_runtime->configuration().ingress;
    m_effectiveReaderLimits.maximumFramesPerRead = static_cast<quint32>(
        std::min<std::size_t>({
            m_effectiveReaderLimits.maximumFramesPerRead,
            ingress.maximumSamplesPerCall,
            m_config.maximumBufferedSamples,
        }));

    connect(m_runtime, &SstvRxRuntime::snapshotAvailable,
            this, [this](quint64) { m_waitChanged.notify_all(); },
            Qt::DirectConnection);
    connect(m_runtime, &SstvRxRuntime::runtimeStateChanged,
            this, [this](SstvRxRuntime::State, quint64) {
                m_waitChanged.notify_all();
            }, Qt::DirectConnection);
}

SstvWavReplayController::~SstvWavReplayController()
{
    if (!isOnOwnerThread()) {
        qFatal("SstvWavReplayController must be destroyed on its owner thread");
    }
    shutdown();
}

SstvWavReplayController::Config
SstvWavReplayController::configuration() const noexcept
{
    return m_config;
}

SstvWavReplayController::State SstvWavReplayController::state() const noexcept
{
    return m_state;
}

bool SstvWavReplayController::active() const noexcept
{
    switch (m_state) {
    case State::Preparing:
    case State::Replaying:
    case State::Draining:
    case State::Cancelling:
        return true;
    case State::Idle:
    case State::Completed:
    case State::Cancelled:
    case State::Error:
    case State::Shutdown:
        return false;
    }
    return false;
}

bool SstvWavReplayController::canStart() const noexcept
{
    return m_state != State::Shutdown && !active();
}

QString SstvWavReplayController::stateName() const
{
    return displayName(m_state);
}

double SstvWavReplayController::progress() const noexcept
{
    return m_progress;
}

QString SstvWavReplayController::fileName() const
{
    return m_fileName;
}

QString SstvWavReplayController::lastError() const
{
    return m_lastError;
}

quint32 SstvWavReplayController::sampleRate() const noexcept
{
    return m_sampleRate;
}

quint64 SstvWavReplayController::durationMs() const noexcept
{
    return m_durationMs;
}

quint64 SstvWavReplayController::sessionId() const noexcept
{
    return m_sessionId;
}

bool SstvWavReplayController::startReplay(const QUrl& localFile)
{
    if (!isOnOwnerThread() || !canStart()) {
        return false;
    }
    joinFinishedWorker();

    QString path;
    if (localFile.isLocalFile()) {
        path = localFile.toLocalFile();
    } else if (localFile.scheme().isEmpty()) {
        path = localFile.toString();
    }
    if (path.trimmed().isEmpty() || !QFileInfo(path).isAbsolute()) {
        setError(QStringLiteral("SSTV replay requires a local absolute WAV path"));
        setState(State::Error);
        return false;
    }

    if (++m_sessionId == 0U) {
        ++m_sessionId;
    }
    const quint64 session = m_sessionId;
    m_activeSession.store(session, std::memory_order_release);
    m_cancelRequested.store(false, std::memory_order_release);
    m_progressPostPending.store(false, std::memory_order_release);
    m_progressMillionths.store(0U, std::memory_order_release);
    m_progress = 0.0;
    m_sampleRate = 0U;
    m_durationMs = 0U;
    m_fileName = QFileInfo(path).fileName();
    m_lastError.clear();
    Q_EMIT progressChanged();
    Q_EMIT replayChanged();
    setState(State::Preparing);

    QString runtimeError;
    if (!prepareRuntime(streamIdForSession(session), &runtimeError)) {
        m_activeSession.store(0U, std::memory_order_release);
        setError(std::move(runtimeError));
        setState(State::Error);
        return false;
    }
    const SstvRxRouteToken token = m_runtime->routeToken();
    const SstvRxRuntime::Snapshot baseline = m_runtime->snapshot();
    if (!token.valid() || token.source.kind != SstvAudioSourceKind::Replay) {
        m_activeSession.store(0U, std::memory_order_release);
        setError(QStringLiteral("SSTV replay did not acquire its RX route"));
        setState(State::Error);
        return false;
    }

    setState(State::Replaying);
    m_workerRunning.store(true, std::memory_order_release);
    try {
        m_worker = std::thread(
            &SstvWavReplayController::workerMain,
            this,
            path,
            token,
            session,
            baseline.ingress.queue.droppedChunks,
            baseline.processingFailures,
            baseline.staleChunksDiscarded);
    } catch (...) {
        m_workerRunning.store(false, std::memory_order_release);
        m_activeSession.store(0U, std::memory_order_release);
        setError(QStringLiteral("Could not create the SSTV WAV replay worker"));
        setState(State::Error);
        return false;
    }
    return true;
}

void SstvWavReplayController::cancel()
{
    if (!isOnOwnerThread() || !active()) {
        return;
    }
    m_cancelRequested.store(true, std::memory_order_release);
    cancelReader();
    m_waitChanged.notify_all();
    setState(State::Cancelling);
}

void SstvWavReplayController::shutdown()
{
    if (!isOnOwnerThread() || m_state == State::Shutdown) {
        return;
    }
    m_cancelRequested.store(true, std::memory_order_release);
    m_activeSession.store(0U, std::memory_order_release);
    cancelReader();
    m_waitChanged.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    m_workerRunning.store(false, std::memory_order_release);
    setState(State::Shutdown);
}

SstvWavReplayController::Config SstvWavReplayController::validateConfig(
    SstvRxRuntime* runtime, Config config)
{
    if (!runtime || runtime->thread() != QThread::currentThread()
        || !config.readerLimits.valid()
        || config.tailSilenceMs < 10U || config.tailSilenceMs > 30'000U
        || config.drainTimeoutMs < 100U
        || config.drainTimeoutMs > 600'000U
        || config.backpressurePollMs == 0U
        || config.backpressurePollMs > 1'000U
        || config.maximumBufferedChunks == 0U
        || config.maximumBufferedSamples == 0U
        || config.maximumErrorCharacters == 0U
        || config.maximumErrorCharacters > 4'096U) {
        throw std::invalid_argument("invalid SSTV WAV replay configuration");
    }
    const auto ingress = runtime->configuration().ingress;
    if (config.maximumBufferedChunks > ingress.maximumChunks
        || config.maximumBufferedSamples > ingress.maximumQueuedSamples) {
        throw std::invalid_argument(
            "SSTV WAV replay backpressure exceeds the RX ingress bounds");
    }
    return config;
}

QString SstvWavReplayController::displayName(State state)
{
    switch (state) {
    case State::Idle: return QStringLiteral("Idle");
    case State::Preparing: return QStringLiteral("Preparing");
    case State::Replaying: return QStringLiteral("Replaying");
    case State::Draining: return QStringLiteral("Draining");
    case State::Cancelling: return QStringLiteral("Cancelling");
    case State::Completed: return QStringLiteral("Completed");
    case State::Cancelled: return QStringLiteral("Cancelled");
    case State::Error: return QStringLiteral("Error");
    case State::Shutdown: return QStringLiteral("Shutdown");
    }
    return QStringLiteral("Unknown");
}

qint64 SstvWavReplayController::addBlockDuration(qint64 timestampNs,
                                                qsizetype sampleCount,
                                                quint32 sampleRate,
                                                bool* ok) noexcept
{
    if (ok) {
        *ok = false;
    }
    if (timestampNs < 0 || sampleCount <= 0 || sampleRate == 0U) {
        return 0;
    }
    const quint64 count = static_cast<quint64>(sampleCount);
    if (count > std::numeric_limits<quint64>::max()
                    / kNanosecondsPerSecond) {
        return 0;
    }
    const quint64 numerator = count * kNanosecondsPerSecond;
    quint64 duration = numerator / sampleRate;
    if ((numerator % sampleRate) != 0U) {
        ++duration;
    }
    const quint64 maximum = static_cast<quint64>(
        std::numeric_limits<qint64>::max());
    if (duration > maximum
        || static_cast<quint64>(timestampNs) > maximum - duration) {
        return 0;
    }
    if (ok) {
        *ok = true;
    }
    return timestampNs + static_cast<qint64>(duration);
}

quint32 SstvWavReplayController::streamIdForSession(
    quint64 sessionId) noexcept
{
    quint32 value = static_cast<quint32>(sessionId)
        ^ static_cast<quint32>(sessionId >> 32U) ^ 0x53535456U;
    return value == 0U ? 1U : value;
}

bool SstvWavReplayController::isOnOwnerThread() const noexcept
{
    return QThread::currentThread() == thread();
}

bool SstvWavReplayController::prepareRuntime(quint32 streamId,
                                             QString* error)
{
    const auto current = m_runtime->state();
    bool ready = false;
    switch (current) {
    case SstvRxRuntime::State::Inactive:
        ready = m_runtime->start(SstvAudioSourceKind::Replay, streamId);
        break;
    case SstvRxRuntime::State::Running:
    case SstvRxRuntime::State::Cancelled:
        ready = m_runtime->switchSource(
            SstvAudioSourceKind::Replay, streamId);
        break;
    case SstvRxRuntime::State::Error:
        ready = m_runtime->stop()
            && m_runtime->start(SstvAudioSourceKind::Replay, streamId);
        break;
    case SstvRxRuntime::State::Stopping:
    case SstvRxRuntime::State::Shutdown:
        break;
    }
    if (!ready && error) {
        *error = QStringLiteral("The native SSTV RX runtime could not enter replay mode");
    }
    return ready;
}

bool SstvWavReplayController::enqueueWithBackpressure(
    QVector<short> samples,
    quint32 sampleRate,
    SstvRxRouteToken token,
    qint64* timestampNs,
    quint64 sessionId,
    quint64 baselineDrops,
    quint64 baselineFailures,
    quint64 baselineStale,
    quint64* acceptedChunks,
    QString* error)
{
    if (!timestampNs || !acceptedChunks || samples.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Invalid SSTV replay audio block");
        }
        return false;
    }
    const std::size_t sampleCount = static_cast<std::size_t>(samples.size());
    for (;;) {
        if (replayWasCancelled(sessionId)) {
            return false;
        }
        const SstvRxRuntime::Snapshot snapshot = m_runtime->snapshot();
        if (snapshot.state != SstvRxRuntime::State::Running
            || snapshot.route != token) {
            if (error) {
                *error = QStringLiteral("The SSTV RX route changed during WAV replay");
            }
            return false;
        }
        if (snapshot.ingress.queue.droppedChunks != baselineDrops
            || snapshot.processingFailures != baselineFailures
            || snapshot.staleChunksDiscarded != baselineStale) {
            if (error) {
                *error = QStringLiteral("The SSTV WAV replay pipeline reported a loss or processing failure");
            }
            return false;
        }
        const auto queuedChunks = snapshot.ingress.queue.queuedChunks;
        const auto queuedSamples = snapshot.ingress.queue.queuedSamples;
        if (queuedChunks < m_config.maximumBufferedChunks
            && sampleCount <= m_config.maximumBufferedSamples
                                   - std::min(queuedSamples,
                                              m_config.maximumBufferedSamples)) {
            break;
        }
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_waitChanged.wait_for(
            lock, std::chrono::milliseconds(m_config.backpressurePollMs));
    }

    bool timestampOk = false;
    const qint64 nextTimestamp = addBlockDuration(
        *timestampNs, samples.size(), sampleRate, &timestampOk);
    if (!timestampOk
        || !m_runtime->enqueuePcm16At(std::move(samples),
                                      static_cast<int>(sampleRate),
                                      token,
                                      *timestampNs)) {
        if (!replayWasCancelled(sessionId) && error) {
            *error = QStringLiteral("The bounded SSTV RX ingress rejected WAV audio");
        }
        return false;
    }
    *timestampNs = nextTimestamp;
    ++(*acceptedChunks);
    return true;
}

bool SstvWavReplayController::waitForDrain(
    SstvRxRouteToken token,
    quint64 sessionId,
    quint64 acceptedChunks,
    quint64 baselineDrops,
    quint64 baselineFailures,
    quint64 baselineStale,
    QString* error)
{
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(m_config.drainTimeoutMs);
    for (;;) {
        if (replayWasCancelled(sessionId)) {
            return false;
        }
        const SstvRxRuntime::Snapshot snapshot = m_runtime->snapshot();
        if (snapshot.state != SstvRxRuntime::State::Running
            || snapshot.route != token) {
            if (error) {
                *error = QStringLiteral("The SSTV RX route changed before replay drain completed");
            }
            return false;
        }
        if (snapshot.ingress.queue.droppedChunks != baselineDrops
            || snapshot.processingFailures != baselineFailures
            || snapshot.staleChunksDiscarded != baselineStale) {
            if (error) {
                *error = QStringLiteral("The SSTV WAV replay did not drain losslessly");
            }
            return false;
        }
        if (snapshot.generationChunksProcessed >= acceptedChunks
            && snapshot.ingress.queue.queuedChunks == 0U
            && snapshot.ingress.queue.queuedSamples == 0U) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            if (error) {
                *error = QStringLiteral("Timed out waiting for the native SSTV decoder to drain WAV audio");
            }
            return false;
        }
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_waitChanged.wait_for(
            lock, std::chrono::milliseconds(m_config.backpressurePollMs));
    }
}

bool SstvWavReplayController::replayWasCancelled(
    quint64 sessionId) const noexcept
{
    return m_cancelRequested.load(std::memory_order_acquire)
        || sessionId == 0U
        || m_activeSession.load(std::memory_order_acquire) != sessionId;
}

void SstvWavReplayController::workerMain(
    QString path,
    SstvRxRouteToken token,
    quint64 sessionId,
    quint64 baselineDrops,
    quint64 baselineFailures,
    quint64 baselineStale) noexcept
{
    WorkerResult result;
    result.sessionId = sessionId;
    QString error;
    std::unique_ptr<SstvWavPcmReader> reader;
    try {
        reader = std::make_unique<SstvWavPcmReader>(
            m_effectiveReaderLimits);
        {
            const std::lock_guard<std::mutex> lock(m_readerMutex);
            m_activeReader = reader.get();
            if (replayWasCancelled(sessionId)) {
                reader->cancel();
            }
        }

        if (!reader->open(path, &error)) {
            result.outcome = replayWasCancelled(sessionId)
                ? WorkerOutcome::Cancelled : WorkerOutcome::Error;
        } else if (!SstvResampler::isSupportedInputRate(
                       reader->format().sampleRate)) {
            error = QStringLiteral(
                "WAV sample rate %1 Hz is not supported by the native SSTV resampler")
                        .arg(reader->format().sampleRate);
            result.outcome = WorkerOutcome::Error;
        } else {
            const quint32 rate = reader->format().sampleRate;
            postFormat(sessionId, rate, reader->format().durationMs);
            qint64 timestampNs = SstvRxRuntime::localMonotonicNowNs();
            quint64 acceptedChunks = 0U;
            bool inputComplete = false;
            while (!replayWasCancelled(sessionId)) {
                QVector<short> samples;
                const SstvWavReadStatus status = reader->readNext(
                    &samples, &error);
                if (status == SstvWavReadStatus::End) {
                    inputComplete = true;
                    break;
                }
                if (status == SstvWavReadStatus::Cancelled) {
                    break;
                }
                if (status == SstvWavReadStatus::Error
                    || !enqueueWithBackpressure(
                        std::move(samples), rate, token, &timestampNs,
                        sessionId, baselineDrops, baselineFailures,
                        baselineStale, &acceptedChunks, &error)) {
                    break;
                }
                result.framesRead = reader->framesRead();
                scheduleProgress(sessionId, reader->progress());
            }

            if (inputComplete && !replayWasCancelled(sessionId)
                && error.isEmpty()) {
                quint64 remainingSilence =
                    (static_cast<quint64>(rate) * m_config.tailSilenceMs
                     + 999U) / 1000U;
                const quint64 maximumBlock =
                    m_effectiveReaderLimits.maximumFramesPerRead;
                while (remainingSilence > 0U
                       && !replayWasCancelled(sessionId)) {
                    const quint64 count = std::min(
                        remainingSilence, maximumBlock);
                    QVector<short> silence(
                        static_cast<qsizetype>(count), short {0});
                    if (!enqueueWithBackpressure(
                            std::move(silence), rate, token, &timestampNs,
                            sessionId, baselineDrops, baselineFailures,
                            baselineStale, &acceptedChunks, &error)) {
                        break;
                    }
                    remainingSilence -= count;
                }
                if (remainingSilence == 0U
                    && !replayWasCancelled(sessionId) && error.isEmpty()) {
                    scheduleProgress(sessionId, 1.0);
                    postDraining(sessionId);
                    if (waitForDrain(token, sessionId, acceptedChunks,
                                     baselineDrops, baselineFailures,
                                     baselineStale, &error)) {
                        result.outcome = WorkerOutcome::Completed;
                    }
                }
            }
            result.chunksEnqueued = acceptedChunks;
            if (replayWasCancelled(sessionId)) {
                result.outcome = WorkerOutcome::Cancelled;
                error.clear();
            } else if (result.outcome != WorkerOutcome::Completed) {
                result.outcome = WorkerOutcome::Error;
                if (error.isEmpty()) {
                    error = QStringLiteral("Native SSTV WAV replay failed");
                }
            }
        }
    } catch (const std::exception& exception) {
        result.outcome = replayWasCancelled(sessionId)
            ? WorkerOutcome::Cancelled : WorkerOutcome::Error;
        error = QString::fromUtf8(exception.what());
    } catch (...) {
        result.outcome = replayWasCancelled(sessionId)
            ? WorkerOutcome::Cancelled : WorkerOutcome::Error;
        error = QStringLiteral("Unknown SSTV WAV replay worker failure");
    }
    {
        const std::lock_guard<std::mutex> lock(m_readerMutex);
        if (m_activeReader == reader.get()) {
            m_activeReader = nullptr;
        }
    }
    reader.reset();
    result.error = bounded(std::move(error), m_config.maximumErrorCharacters);
    m_workerRunning.store(false, std::memory_order_release);
    m_waitChanged.notify_all();
    postFinished(std::move(result));
}

void SstvWavReplayController::setState(State next)
{
    if (m_state == next) {
        return;
    }
    m_state = next;
    Q_EMIT stateChanged();
}

void SstvWavReplayController::setError(QString error)
{
    error = bounded(std::move(error), m_config.maximumErrorCharacters);
    if (m_lastError == error) {
        return;
    }
    m_lastError = std::move(error);
    Q_EMIT replayChanged();
    if (!m_lastError.isEmpty()) {
        Q_EMIT errorOccurred(m_lastError);
    }
}

void SstvWavReplayController::scheduleProgress(
    quint64 sessionId, double value) noexcept
{
    const double safe = std::clamp(value, 0.0, 1.0);
    m_progressMillionths.store(
        static_cast<quint32>(safe * kProgressScale + 0.5),
        std::memory_order_release);
    if (m_progressPostPending.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (!QMetaObject::invokeMethod(
            this,
            [this, sessionId] {
                m_progressPostPending.store(false, std::memory_order_release);
                if (m_activeSession.load(std::memory_order_acquire)
                    != sessionId) {
                    return;
                }
                const double next = static_cast<double>(
                    m_progressMillionths.load(std::memory_order_acquire))
                    / kProgressScale;
                if (m_progress != next) {
                    m_progress = next;
                    Q_EMIT progressChanged();
                }
            }, Qt::QueuedConnection)) {
        m_progressPostPending.store(false, std::memory_order_release);
    }
}

void SstvWavReplayController::postFormat(quint64 sessionId,
                                         quint32 rate,
                                         quint64 milliseconds) noexcept
{
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, sessionId, rate, milliseconds] {
            if (m_activeSession.load(std::memory_order_acquire)
                != sessionId) {
                return;
            }
            m_sampleRate = rate;
            m_durationMs = milliseconds;
            Q_EMIT replayChanged();
        }, Qt::QueuedConnection));
}

void SstvWavReplayController::postDraining(quint64 sessionId) noexcept
{
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, sessionId] {
            if (m_activeSession.load(std::memory_order_acquire) == sessionId
                && m_state == State::Replaying) {
                setState(State::Draining);
            }
        }, Qt::QueuedConnection));
}

void SstvWavReplayController::postFinished(WorkerResult result) noexcept
{
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, result = std::move(result)]() mutable {
            finishOnOwner(std::move(result));
        }, Qt::QueuedConnection));
}

void SstvWavReplayController::finishOnOwner(WorkerResult result)
{
    if (!isOnOwnerThread()
        || m_activeSession.load(std::memory_order_acquire)
            != result.sessionId
        || m_state == State::Shutdown) {
        return;
    }
    joinFinishedWorker();
    m_activeSession.store(0U, std::memory_order_release);
    const bool completed = result.outcome == WorkerOutcome::Completed;
    const bool cancelled = result.outcome == WorkerOutcome::Cancelled;
    if (completed) {
        m_progress = 1.0;
        Q_EMIT progressChanged();
        setError(QString {});
        setState(State::Completed);
    } else if (cancelled) {
        setError(QString {});
        setState(State::Cancelled);
    } else {
        setError(result.error.isEmpty()
                     ? QStringLiteral("Native SSTV WAV replay failed")
                     : std::move(result.error));
        setState(State::Error);
    }
    Q_EMIT replayFinished(completed, cancelled, result.sessionId);
}

void SstvWavReplayController::cancelReader() noexcept
{
    const std::lock_guard<std::mutex> lock(m_readerMutex);
    if (m_activeReader) {
        m_activeReader->cancel();
    }
}

void SstvWavReplayController::joinFinishedWorker() noexcept
{
    if (!m_worker.joinable()) {
        return;
    }
    if (m_workerRunning.load(std::memory_order_acquire)) {
        return;
    }
    m_worker.join();
}

} // namespace decodium::sstv
