// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRxAudioJobController.h"

#include "SstvTxAudioDevice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QThread>
#include <QUuid>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000ULL;

QString boundedError(QString value)
{
    value.replace(QLatin1Char('\0'), QChar::ReplacementCharacter);
    return value.left(SstvWavExporter::MaximumErrorCharacters);
}

std::int16_t pcm16ForFloat(float sample)
{
    if (!std::isfinite(sample)) {
        throw std::invalid_argument("retained SSTV audio contains non-finite samples");
    }
    const double bounded = std::clamp(static_cast<double>(sample), -1.0, 1.0);
    const double scaled = bounded < 0.0 ? bounded * 32'768.0
                                        : bounded * 32'767.0;
    const long rounded = std::lround(scaled);
    return static_cast<std::int16_t>(std::clamp<long>(
        rounded,
        std::numeric_limits<std::int16_t>::min(),
        std::numeric_limits<std::int16_t>::max()));
}

class RetainedPcm16Source final : public SstvPcm16Source
{
public:
    explicit RetainedPcm16Source(SstvRxRetainedAudioSnapshot snapshot)
        : sampleRate_(snapshot.sampleRate)
    {
        if (sampleRate_ == 0U || snapshot.chunks.empty()
            || snapshot.sampleCount == 0U) {
            throw std::invalid_argument("no retained SSTV audio is available");
        }
        const std::chrono::nanoseconds start =
            snapshot.chunks.front().startTime;
        std::uint64_t maximumEnd = 0U;
        struct Placement final
        {
            const SstvAudioChunk* chunk {nullptr};
            std::size_t first {0U};
        };
        std::vector<Placement> placements;
        placements.reserve(snapshot.chunks.size());
        for (const SstvAudioChunk& chunk : snapshot.chunks) {
            if (chunk.sampleRate != sampleRate_
                || chunk.startTime < start || chunk.samples.empty()) {
                throw std::invalid_argument(
                    "retained SSTV audio chunks are inconsistent");
            }
            const std::uint64_t delta = static_cast<std::uint64_t>(
                (chunk.startTime - start).count());
            if (delta > std::numeric_limits<std::uint64_t>::max()
                            / sampleRate_) {
                throw std::length_error("retained SSTV audio timeline overflow");
            }
            const std::uint64_t first =
                (delta * sampleRate_ + kNanosecondsPerSecond / 2U)
                / kNanosecondsPerSecond;
            if (first > SstvReplayBuffer::kMaximumRetainedSamples
                || chunk.samples.size()
                    > SstvReplayBuffer::kMaximumRetainedSamples - first) {
                throw std::length_error(
                    "retained SSTV audio exceeds its hard sample bound");
            }
            maximumEnd = std::max(
                maximumEnd,
                first + static_cast<std::uint64_t>(chunk.samples.size()));
            placements.push_back({&chunk, static_cast<std::size_t>(first)});
        }
        samples_.assign(static_cast<std::size_t>(maximumEnd), 0);
        for (const Placement& placement : placements) {
            for (std::size_t index = 0U;
                 index < placement.chunk->samples.size(); ++index) {
                samples_[placement.first + index] = pcm16ForFloat(
                    placement.chunk->samples[index]);
            }
        }
    }

    std::uint32_t sampleRate() const noexcept override { return sampleRate_; }
    std::uint64_t totalSamples() const noexcept override
    {
        return static_cast<std::uint64_t>(samples_.size());
    }
    std::uint64_t producedSamples() const noexcept override
    {
        return static_cast<std::uint64_t>(position_);
    }
    bool complete() const noexcept override
    {
        return position_ >= samples_.size();
    }
    bool cancelled() const noexcept override { return cancelled_; }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        if (cancelled_ || complete() || capacity == 0U) {
            return 0U;
        }
        if (!output) {
            throw std::invalid_argument("retained PCM output is null");
        }
        const std::size_t count = std::min(
            capacity, samples_.size() - position_);
        std::copy_n(samples_.data() + position_, count, output);
        position_ += count;
        return count;
    }

    void cancel() noexcept override { cancelled_ = true; }
    void reset() override
    {
        position_ = 0U;
        cancelled_ = false;
    }

private:
    std::vector<std::int16_t> samples_;
    std::uint32_t sampleRate_ {0U};
    std::size_t position_ {0U};
    bool cancelled_ {false};
};

} // namespace

SstvRxAudioJobController::SstvRxAudioJobController(
    SstvRxRuntime* runtime,
    QObject* parent)
    : QObject(parent)
    , m_runtime(runtime)
    , m_temporaryDirectory(std::make_unique<QTemporaryDir>(
          QDir::tempPath() + QStringLiteral("/decodium-sstv-rx-XXXXXX")))
{
    if (!m_runtime || m_runtime->thread() != QThread::currentThread()) {
        throw std::invalid_argument(
            "SSTV retained-audio controller requires an owner-thread runtime");
    }
    connect(&m_watcher,
            &QFutureWatcher<JobResult>::finished,
            this,
            &SstvRxAudioJobController::finishJob);
}

SstvRxAudioJobController::~SstvRxAudioJobController()
{
    if (!isOnOwnerThread()) {
        qFatal("SstvRxAudioJobController must be destroyed on its owner thread");
    }
    shutdown();
}

bool SstvRxAudioJobController::busy() const noexcept
{
    return m_state == State::PreparingRedecode
        || m_state == State::ExportingRawAudio;
}

SstvRxAudioJobController::State
SstvRxAudioJobController::state() const noexcept
{
    return m_state;
}

QString SstvRxAudioJobController::stateName() const
{
    return displayName(m_state);
}

QString SstvRxAudioJobController::lastError() const
{
    return m_lastError;
}

QString SstvRxAudioJobController::lastOutputPath() const
{
    return m_lastOutputPath;
}

std::uint64_t SstvRxAudioJobController::lastAcquisitionId() const noexcept
{
    return m_lastAcquisitionId;
}

SstvRxRedecodeParameters
SstvRxAudioJobController::preparedRedecodeParameters() const
{
    return m_preparedParameters;
}

bool SstvRxAudioJobController::prepareRecentRedecode(
    SstvRxRedecodeParameters parameters)
{
    if (!SstvRxControlPolicy::redecodeParametersAreValid(parameters)
        || !m_temporaryDirectory || !m_temporaryDirectory->isValid()) {
        setState(State::Error,
                 tr("A private retained-audio re-decode file could not be created"));
        return false;
    }
    const QString path = QDir(m_temporaryDirectory->path()).absoluteFilePath(
        QStringLiteral("redecode-%1.wav").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)));
    return startJob(Operation::Redecode, path, 0U, std::move(parameters));
}

bool SstvRxAudioJobController::exportRawAudio(
    const QUrl& destination,
    std::uint64_t acquisitionId)
{
    const QString path = destination.isLocalFile()
        ? destination.toLocalFile()
        : (destination.scheme().isEmpty() ? destination.toString()
                                           : QString {});
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()
        || !path.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        setState(State::Error,
                 tr("Raw SSTV audio requires an absolute local .wav destination"));
        return false;
    }
    return startJob(Operation::RawExport, path, acquisitionId, {});
}

void SstvRxAudioJobController::discardPreparedRedecode()
{
    if (m_operation == Operation::Redecode && !m_lastOutputPath.isEmpty()) {
        static_cast<void>(QFile::remove(m_lastOutputPath));
        m_lastOutputPath.clear();
        m_preparedParameters = {};
        Q_EMIT stateChanged();
    }
}

void SstvRxAudioJobController::cancel()
{
    if (!isOnOwnerThread() || !busy() || !m_cancelRequested) {
        return;
    }
    m_cancelRequested->store(true, std::memory_order_release);
}

void SstvRxAudioJobController::shutdown()
{
    if (!isOnOwnerThread() || m_state == State::Shutdown) {
        return;
    }
    if (m_cancelRequested) {
        m_cancelRequested->store(true, std::memory_order_release);
    }
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
    discardPreparedRedecode();
    m_state = State::Shutdown;
    m_operation = Operation::None;
    Q_EMIT stateChanged();
}

SstvRxAudioJobController::JobResult SstvRxAudioJobController::runJob(
    SstvRxRuntime* runtime,
    Operation operation,
    QString outputPath,
    std::uint64_t acquisitionId,
    SstvRxRedecodeParameters parameters,
    std::shared_ptr<std::atomic_bool> cancelRequested) noexcept
{
    JobResult result;
    result.operation = operation;
    result.path = outputPath;
    result.parameters = std::move(parameters);
    try {
        if (!runtime || !cancelRequested
            || cancelRequested->load(std::memory_order_acquire)) {
            result.cancelled = true;
            return result;
        }
        const SstvRxRuntime::Snapshot runtimeSnapshot = runtime->snapshot();
        std::uint64_t selectedId = acquisitionId;
        if (selectedId == 0U) {
            selectedId = runtimeSnapshot.image.acquisitionId != 0U
                ? runtimeSnapshot.image.acquisitionId
                : runtimeSnapshot.replay.mostRecentAcquisitionId;
        }
        std::optional<SstvRxRetainedAudioSnapshot> acquisition;
        if (selectedId != 0U) {
            acquisition = runtime->retainedAudioForAcquisition(selectedId);
        }
        SstvRxRetainedAudioSnapshot retained = acquisition.has_value()
            ? std::move(*acquisition) : runtime->retainedRecentAudio();
        result.acquisitionId = retained.acquisitionId != 0U
            ? retained.acquisitionId : selectedId;
        if (retained.chunks.empty() || retained.sampleCount == 0U) {
            result.error = tr("No retained SSTV audio is available");
            return result;
        }
        if (cancelRequested->load(std::memory_order_acquire)) {
            result.cancelled = true;
            return result;
        }

        auto source = std::make_unique<RetainedPcm16Source>(retained);
        SstvWavExportRequest request;
        request.outputPath = outputPath;
        request.mode = QString::fromStdString(retained.mode).left(
            SstvWavExporter::MaximumModeCharacters);
        if (request.mode.isEmpty()) {
            request.mode = QStringLiteral("sstv-rx-retained");
        }
        request.replaceExisting = false;
        request.writeMetadataSidecar = operation == Operation::RawExport;
        request.metadata.insert(QStringLiteral("diagnosticRawAudio"), true);
        request.metadata.insert(QStringLiteral("acquisitionId"),
                                static_cast<qint64>(std::min<std::uint64_t>(
                                    result.acquisitionId,
                                    static_cast<std::uint64_t>(
                                        std::numeric_limits<qint64>::max()))));
        request.metadata.insert(QStringLiteral("sampleRateHz"),
                                static_cast<int>(retained.sampleRate));
        request.metadata.insert(QStringLiteral("frequencyCorrectionHz"),
                                retained.frequencyCorrectionHz);
        request.metadata.insert(QStringLiteral("slantCorrectionPpm"),
                                retained.slantCorrectionPpm);
        request.metadata.insert(QStringLiteral("fskId"),
                                QString::fromStdString(retained.fskId));
        request.metadata.insert(QStringLiteral("truncatedAtStart"),
                                retained.truncatedAtStart);
        request.metadata.insert(QStringLiteral("truncatedAtEnd"),
                                retained.truncatedAtEnd);
        const SstvWavExportResult exportResult =
            SstvWavExporter::exportAtomic(
                std::move(source), request, cancelRequested);
        result.ok = exportResult.ok;
        result.path = exportResult.wavPath;
        result.cancelled = exportResult.code == SstvWavExportError::Cancelled;
        result.error = exportResult.error;
    } catch (const std::exception& exception) {
        result.error = QString::fromUtf8(exception.what());
    } catch (...) {
        result.error = tr("Unknown retained SSTV audio job failure");
    }
    result.error = boundedError(std::move(result.error));
    return result;
}

QString SstvRxAudioJobController::displayName(State state)
{
    switch (state) {
    case State::Idle: return tr("Idle");
    case State::PreparingRedecode: return tr("Preparing re-decode");
    case State::ExportingRawAudio: return tr("Saving raw audio");
    case State::Completed: return tr("Completed");
    case State::Cancelled: return tr("Cancelled");
    case State::Error: return tr("Error");
    case State::Shutdown: return tr("Shutdown");
    }
    return tr("Unknown");
}

bool SstvRxAudioJobController::isOnOwnerThread() const noexcept
{
    return QThread::currentThread() == thread();
}

void SstvRxAudioJobController::setState(State state, QString error)
{
    m_state = state;
    m_lastError = boundedError(std::move(error));
    Q_EMIT stateChanged();
}

bool SstvRxAudioJobController::startJob(
    Operation operation,
    QString outputPath,
    std::uint64_t acquisitionId,
    SstvRxRedecodeParameters parameters)
{
    if (!isOnOwnerThread() || busy() || m_state == State::Shutdown
        || operation == Operation::None || m_watcher.isRunning()) {
        return false;
    }
    discardPreparedRedecode();
    m_operation = operation;
    m_lastOutputPath.clear();
    m_lastAcquisitionId = 0U;
    m_lastError.clear();
    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    setState(operation == Operation::Redecode
                 ? State::PreparingRedecode : State::ExportingRawAudio);
    try {
        m_watcher.setFuture(QtConcurrent::run(
            &SstvRxAudioJobController::runJob,
            m_runtime,
            operation,
            std::move(outputPath),
            acquisitionId,
            std::move(parameters),
            m_cancelRequested));
    } catch (const std::exception& exception) {
        setState(State::Error, QString::fromUtf8(exception.what()));
        return false;
    } catch (...) {
        setState(State::Error,
                 tr("Could not start the retained SSTV audio worker"));
        return false;
    }
    return true;
}

void SstvRxAudioJobController::finishJob()
{
    if (!isOnOwnerThread() || m_state == State::Shutdown) {
        return;
    }
    const JobResult result = m_watcher.result();
    m_lastAcquisitionId = result.acquisitionId;
    m_lastOutputPath = result.ok ? result.path : QString {};
    m_preparedParameters = result.parameters;
    if (result.ok) {
        setState(State::Completed);
    } else if (result.cancelled) {
        setState(State::Cancelled);
    } else {
        setState(State::Error,
                 result.error.isEmpty()
                     ? tr("The retained SSTV audio job failed")
                     : result.error);
    }

    if (result.operation == Operation::Redecode) {
        Q_EMIT redecodePrepared(
            result.ok,
            result.ok ? QUrl::fromLocalFile(result.path) : QUrl {},
            m_lastError);
    } else if (result.operation == Operation::RawExport) {
        Q_EMIT rawAudioExportFinished(
            result.ok,
            result.ok ? result.path : QString {},
            result.acquisitionId,
            m_lastError);
    }
}

} // namespace decodium::sstv
