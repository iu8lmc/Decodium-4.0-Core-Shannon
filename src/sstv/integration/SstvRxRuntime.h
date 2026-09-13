// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvAudioIngress.h"

#include "../analog/SstvAvt.h"
#include "../analog/SstvMmsstvExtended.h"
#include "../rx/SstvRxControlPolicy.h"
#include "../rx/SstvRxCorrectionController.h"
#include "../rx/SstvRxRetainedAudio.h"
#include "../rx/SstvRxStateMachine.h"
#include "../rx/SstvTimingFallbackDetector.h"
#include "../rx/SstvNarrowVisDetector.h"
#include "../rx/SstvVisDetector.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace decodium::sstv {

struct SstvImageSnapshot;
struct SstvFrequencyObservation;

// A relay captures this token when Decodium selects an audio source.  Carrying
// the generation at the producer boundary prevents a queued or late callback
// from being relabelled as audio from a newer same-kind source generation.
struct SstvRxRouteToken final
{
    SstvAudioSource source;
    std::uint64_t generation {0U};

    bool valid() const noexcept
    {
        return source.kind != SstvAudioSourceKind::Unknown && generation != 0U;
    }
};

constexpr bool operator==(const SstvRxRouteToken& left,
                          const SstvRxRouteToken& right) noexcept
{
    return left.source == right.source && left.generation == right.generation;
}

constexpr bool operator!=(const SstvRxRouteToken& left,
                          const SstvRxRouteToken& right) noexcept
{
    return !(left == right);
}

// Produces block-start timestamps in one local steady-clock domain.  Callback
// bursts are placed immediately after the preceding block instead of
// overlapping it.  A delay beyond the configured scheduler-jitter tolerance is
// retained as a real gap, so queue drops and capture interruptions remain
// visible to the decoder.  Unix/DecoPort capture timestamps must not be passed
// to this helper without first being mapped to the local monotonic clock.
class SstvMonotonicTimeline final
{
public:
    struct Candidate final
    {
        bool valid {false};
        bool hadPreviousBlock {false};
        bool preservedGap {false};
        qint64 previousEndNs {0};
        qint64 startNs {0};
        qint64 endNs {0};
        qint64 gapNs {0};
    };

    // CoreAudio/AudioQueue callbacks are nominally 20 ms apart but can arrive
    // a few milliseconds late under GUI/decoder load.  Treat that bounded
    // scheduling jitter as continuous audio; larger gaps remain explicit
    // discontinuities and still terminate the active frame safely.
    explicit SstvMonotonicTimeline(qint64 jitterToleranceNs = 8'000'000);

    Candidate propose(qint64 observedLocalMonotonicNs,
                      std::size_t sampleCount,
                      std::uint32_t sampleRate) const noexcept;
    bool commit(const Candidate& candidate) noexcept;
    void reset() noexcept;

    qint64 jitterToleranceNs() const noexcept;
    bool hasTimestamp() const noexcept;
    qint64 nextTimestampNs() const noexcept;

private:
    qint64 m_jitterToleranceNs {0};
    qint64 m_nextTimestampNs {0};
    bool m_hasTimestamp {false};
};

// Owns the native analog-SSTV RX worker.  Lifecycle calls are QObject-owner
// thread affine.  enqueuePcm16*() is the only producer API and is safe for a
// DirectConnection from Decodium's existing audio fanout; it performs no DSP
// and moves PCM16 into SstvAudioIngress's bounded queue.  External producer
// relays must be disconnected before shutdown, and the QObject must be
// destroyed on its affinity thread (normally with deleteLater()).
class SstvRxRuntime final : public QObject
{
    Q_OBJECT

public:
    struct Config final
    {
        SstvAudioIngress::Config ingress;
        qint64 timestampJitterToleranceNs {8'000'000};
        std::uint32_t snapshotNotificationIntervalMs {100U};
        std::size_t maximumErrorCharacters {256U};
        std::size_t maximumDiagnosticScopePoints {256U};
    };

    enum class State : std::uint8_t
    {
        Inactive,
        Running,
        Cancelled,
        Stopping,
        Error,
        Shutdown,
    };
    Q_ENUM(State)

    struct VisSummary final
    {
        bool available {false};
        bool valid {false};
        bool modeMapped {false};
        SstvVisDetectionStatus status {SstvVisDetectionStatus::Rejected};
        SstvVisDetectionCause cause {SstvVisDetectionCause::None};
        SstvVisFormat format {SstvVisFormat::Unknown};
        int primaryPayload {-1};
        int extensionPayload {-1};
        double confidence {0.0};
        std::uint64_t frameStartedAtUs {0U};
        std::uint64_t frameEndedAtUs {0U};
        QString rawBits;
        QString mappedMode;
    };

    struct ControlSummary final
    {
        SstvRxModeControl modeControl {SstvRxModeControl::Automatic};
        QString manualMode;
        bool modeLockEnabled {false};
        QString lockedMode;
        bool receiveWithoutVis {false};
        bool timingFallbackEnabled {true};
        SstvRxAfcMode afcMode {SstvRxAfcMode::Automatic};
        double manualFrequencyCorrectionHz {0.0};
        SstvRxSlantMode slantMode {SstvRxSlantMode::Automatic};
        double manualClockErrorPpm {0.0};
        std::uint32_t replayRetentionSeconds {180U};
        bool retainRawAudio {false};
        bool diagnosticScopeEnabled {false};
        std::uint64_t revision {0U};
    };

    struct AfcSummary final
    {
        SstvRxAfcMode mode {SstvRxAfcMode::Automatic};
        double measuredOffsetHz {0.0};
        double correctionHz {0.0};
        double confidence {0.0};
        std::uint64_t acceptedReferences {0U};
        std::uint64_t rejectedReferences {0U};
        std::uint64_t rejectedImageObservations {0U};
    };

    struct SlantSummary final
    {
        SstvRxSlantMode mode {SstvRxSlantMode::Automatic};
        bool estimateValid {false};
        double measuredClockErrorPpm {0.0};
        double appliedClockErrorPpm {0.0};
        double confidence {0.0};
        std::uint64_t observedSyncs {0U};
        std::uint64_t rejectedSyncs {0U};
    };

    struct SyncSummary final
    {
        bool observed {false};
        bool locked {false};
        double confidence {0.0};
        std::uint64_t pulseCount {0U};
        std::uint64_t lastPulseStartSample {0U};
        std::uint64_t currentLine {0U};
        std::uint64_t missedLines {0U};
    };

    struct FallbackSummary final
    {
        SstvFallbackStatus status {SstvFallbackStatus::InsufficientData};
        QString selectedMode;
        std::size_t candidateCount {0U};
        double confidence {0.0};
        double observedLinePeriodSamples {0.0};
        double observedSyncDurationSamples {0.0};
        std::uint64_t ambiguityCount {0U};
    };

    struct FskIdSummary final
    {
        bool available {false};
        bool valid {false};
        QString identifier;
        double confidence {0.0};
        std::size_t rawSymbolCount {0U};
        std::uint64_t completedAtSample {0U};
    };

    struct SignalSummary final
    {
        double rms {0.0};
        double snrDb {0.0};
        double confidence {0.0};
        double clippingFraction {0.0};
    };

    struct ReplaySummary final
    {
        std::uint32_t sampleRate {12'000U};
        std::uint32_t retentionSeconds {180U};
        std::size_t retainedSamples {0U};
        std::size_t capacitySamples {0U};
        std::size_t acquisitionDescriptors {0U};
        std::uint64_t mostRecentAcquisitionId {0U};
    };

    struct PerformanceSummary final
    {
        std::uint64_t measuredDspBlocks {0U};
        std::uint64_t totalDspBlockNanoseconds {0U};
        std::uint64_t averageDspBlockNanoseconds {0U};
        std::uint64_t maximumDspBlockNanoseconds {0U};
        std::uint64_t progressiveUpdates {0U};
        double progressiveUpdatesPerSecond {0.0};
    };

    struct ScopePoint final
    {
        std::uint64_t sample {0U};
        double frequencyHz {0.0};
        double confidence {0.0};
        double rms {0.0};
        double snrDb {0.0};
    };

    // Scalar description of the latest coherent progressive image copy.  The
    // pixels themselves are exposed separately through latestImageSnapshot()
    // so ordinary diagnostics never copy a 320x256 frame.
    struct ImageSummary final
    {
        bool available {false};
        bool complete {false};
        bool partial {false};
        bool cancelled {false};
        // Stable receiver-session key.  Unlike the route generation and
        // progressive image revision, this changes for back-to-back frames
        // received without restarting the audio route.
        std::uint64_t acquisitionId {0U};
        std::uint64_t generation {0U};
        std::uint64_t revision {0U};
        std::uint64_t linesPublished {0U};
        std::uint32_t width {0U};
        std::uint32_t height {0U};
        std::size_t coveredComponents {0U};
        std::size_t completedPixels {0U};
        double coverage {0.0};
        QString mode;
    };

    // The snapshot contains no audio or image buffers.  QString fields are
    // explicitly capped by Config::maximumErrorCharacters, so cross-thread
    // diagnostics have a fixed public size bound.
    struct Snapshot final
    {
        State state {State::Inactive};
        bool workerRunning {false};
        std::uint64_t revision {0U};
        SstvRxRouteToken route;

        std::uint64_t workerStarts {0U};
        std::uint64_t workerStops {0U};
        std::uint64_t pipelineResets {0U};
        std::uint64_t generationChunksProcessed {0U};
        std::uint64_t chunksProcessed {0U};
        std::uint64_t samplesConverted {0U};
        std::uint64_t samplesResampled {0U};
        std::uint64_t samplesPreprocessed {0U};
        std::uint64_t frequencyObservations {0U};
        std::uint64_t toneObservations {0U};
        std::uint64_t staleChunksDiscarded {0U};
        std::uint64_t discontinuities {0U};
        std::uint64_t preservedGapNanoseconds {0U};
        std::uint64_t processingFailures {0U};
        std::uint64_t producerRejectedCalls {0U};
        std::uint64_t wrongThreadLifecycleCalls {0U};

        std::uint64_t lastChunkSequence {0U};
        qint64 lastChunkStartNs {0};
        std::uint32_t activeSampleRate {0U};
        double lastFrequencyHz {0.0};
        double lastFrequencyConfidence {0.0};
        std::uint64_t processedPcmHash {14'695'981'039'346'656'037ULL};

        SstvRxState rxState {SstvRxState::Disabled};
        SstvRxCause rxCause {SstvRxCause::None};
        ControlSummary controls;
        AfcSummary afc;
        SlantSummary slant;
        SyncSummary sync;
        FallbackSummary fallback;
        FskIdSummary fskId;
        SignalSummary signal;
        ReplaySummary replay;
        PerformanceSummary performance;
        QVector<ScopePoint> scope;
        VisSummary vis;
        ImageSummary image;
        QString lastError;
        SstvAudioIngress::Stats ingress;
    };

    explicit SstvRxRuntime(QObject* parent = nullptr);
    explicit SstvRxRuntime(Config config, QObject* parent = nullptr);
    ~SstvRxRuntime() override;

    SstvRxRuntime(const SstvRxRuntime&) = delete;
    SstvRxRuntime& operator=(const SstvRxRuntime&) = delete;
    SstvRxRuntime(SstvRxRuntime&&) = delete;
    SstvRxRuntime& operator=(SstvRxRuntime&&) = delete;

    Config configuration() const noexcept;
    State state() const noexcept;
    bool isRunning() const noexcept;
    SstvRxRouteToken routeToken() const noexcept;
    Snapshot snapshot() const noexcept;
    std::shared_ptr<const SstvImageSnapshot>
    latestImageSnapshot() const noexcept;

    SstvRxControlSnapshot rxControlSnapshot() const;
    bool replaceRxControlSettings(SstvRxControlSettings settings);
    bool setRxModeControl(SstvRxModeControl control, std::string manualMode);
    bool setRxModeLock(bool enabled, std::string lockedMode);
    bool setRxReceiveWithoutVis(bool enabled);
    bool setRxTimingFallbackEnabled(bool enabled);
    bool setRxAfc(SstvRxAfcMode mode, double manualCorrectionHz);
    bool setRxSlant(SstvRxSlantMode mode, double manualClockErrorPpm);
    bool setRxReplayRetentionSeconds(std::uint32_t seconds);
    bool setRxRetainRawAudio(bool enabled);
    bool setRxDiagnosticScopeEnabled(bool enabled);
    void resetRxAfc() noexcept;
    void resetRxSlant() noexcept;
    bool requestRxRedecode(SstvRxRedecodeParameters parameters);

    // These methods copy at most the configured hard-bounded rolling store.
    // Callers must use an exporter/replay worker, never the GUI/audio callback.
    std::optional<SstvRxRetainedAudioSnapshot> retainedAudioForAcquisition(
        std::uint64_t acquisitionId);
    SstvRxRetainedAudioSnapshot retainedRecentAudio();

    // Preferred live-source entry point.  The timestamp is sampled from
    // std::chrono::steady_clock, never from wall-clock or DecoPort Unix time.
    bool enqueuePcm16(QVector<short> samples,
                      int sampleRate,
                      SstvRxRouteToken token) noexcept;

    // Deterministic/replay entry point. observedLocalMonotonicNs must already
    // belong to this process's local monotonic domain.
    bool enqueuePcm16At(QVector<short> samples,
                        int sampleRate,
                        SstvRxRouteToken token,
                        qint64 observedLocalMonotonicNs) noexcept;

    static qint64 localMonotonicNowNs() noexcept;

public Q_SLOTS:
    bool start(SstvAudioSourceKind kind,
               quint32 streamId,
               quint64 firstSequence = 0U);
    bool switchSource(SstvAudioSourceKind kind,
                      quint32 streamId,
                      quint64 firstSequence = 0U);
    bool resetStream(quint64 firstSequence = 0U);
    bool cancel();
    bool restart(quint64 firstSequence = 0U);
    bool stop();
    bool shutdown();

Q_SIGNALS:
    // These signals contain only bounded scalar/QString payloads and are
    // emitted from coalesced owner-thread deliveries.  Consumers outside the
    // owner thread should connect with Qt::QueuedConnection.
    void runtimeStateChanged(decodium::sstv::SstvRxRuntime::State state,
                             quint64 generation);
    void snapshotAvailable(quint64 revision);
    void visDetectionAvailable(quint64 generation,
                               int status,
                               int cause,
                               int format,
                               int primaryPayload,
                               int extensionPayload,
                               double confidence,
                               QString mappedMode);
    void workerError(QString detail);

private Q_SLOTS:
    void deliverRuntimeStateNotification() noexcept;
    void deliverSnapshotNotification() noexcept;
    void deliverVisNotification() noexcept;
    void deliverWorkerErrorNotification() noexcept;

private:
    struct WorkerPipeline;

    static Config validateConfig(Config config);
    static bool sourceKindIsValid(SstvAudioSourceKind kind) noexcept;
    static void saturatingAdd(std::atomic<std::uint64_t>& value,
                              std::uint64_t increment = 1U) noexcept;
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;

    bool isOnOwnerThread() const noexcept;
    bool rejectLifecycleCall() noexcept;
    void setStateOnOwner(State next);
    void updateRouteAfterLifecycleLocked();
    void resetTimestampLocked() noexcept;
    void invalidatePublishedAcquisition(SstvRxCause cause) noexcept;
    void notifyControlWaiters() noexcept;
    void joinWorker() noexcept;
    void scheduleRuntimeStateNotification(State state,
                                          std::uint64_t generation) noexcept;

    void workerMain() noexcept;
    bool processChunk(WorkerPipeline& pipeline,
                      SstvPcm16Chunk chunk);
    void resetWorkerPipeline(WorkerPipeline& pipeline,
                             std::uint64_t generation,
                             qint64 epochNs);
    void recordVisResults(WorkerPipeline& pipeline,
                          const std::vector<SstvVisDetection>& results,
                          std::uint64_t generation);
    void recordNarrowVisResults(
        WorkerPipeline& pipeline,
        const std::vector<SstvNarrowVisDetection>& results,
        std::uint64_t generation);
    void applyControlSnapshot(WorkerPipeline& pipeline);
    void updateAfcFromTones(
        WorkerPipeline& pipeline,
        const std::vector<struct SstvToneObservation>& tones,
        std::vector<SstvFrequencyObservation>& frequencies,
        std::optional<std::uint64_t> resumeToneAtSample);
    void updateSyncFallbackAndFsk(
        WorkerPipeline& pipeline,
        const std::vector<struct SstvToneObservation>& tones,
        std::uint64_t eventMs);
    bool beginNativeSessionByModeId(WorkerPipeline& pipeline,
                                    const std::string& modeId,
                                    std::uint64_t imageStartSample,
                                    std::uint64_t eventMs,
                                    double clockErrorPpm);
    void beginRetainedAcquisition(WorkerPipeline& pipeline,
                                  std::uint64_t imageStartSample);
    void closeRetainedAcquisition(WorkerPipeline& pipeline,
                                  std::uint64_t imageEndSample,
                                  bool complete,
                                  const std::string& mode);
    void recordProgressiveUpdate(WorkerPipeline& pipeline) noexcept;
    bool beginMartinM1Session(WorkerPipeline& pipeline,
                              const SstvVisDetection& detection,
                              std::uint64_t eventMs,
                              std::uint8_t visPayload);
    bool beginScottieSession(WorkerPipeline& pipeline,
                             const SstvVisDetection& detection,
                             std::uint64_t eventMs,
                             std::uint8_t visPayload);
    bool beginRobotSession(WorkerPipeline& pipeline,
                           const SstvVisDetection& detection,
                           std::uint64_t eventMs,
                           std::uint8_t visPayload);
    bool beginSequentialRgbSession(WorkerPipeline& pipeline,
                                   const SstvVisDetection& detection,
                                   std::uint64_t eventMs,
                                   std::uint8_t visPayload);
    bool beginPdSession(WorkerPipeline& pipeline,
                        const SstvVisDetection& detection,
                        std::uint64_t eventMs,
                        std::uint8_t visPayload);
    bool armAvtCountdown(WorkerPipeline& pipeline,
                         const SstvVisDetection& detection,
                         std::uint8_t visPayload);
    void consumeAvtCountdownObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    bool beginAvtSession(WorkerPipeline& pipeline,
                         const SstvAvtCountdownDetection& detection,
                         std::uint64_t eventMs);
    bool beginMmsstvSession(WorkerPipeline& pipeline,
                            std::uint64_t eventMs,
                            SstvMmsstvMode mode,
                            std::uint64_t imageStartUs,
                            double estimatedFrequencyOffsetHz);
    void consumeMartinM1Observations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    void terminateMartinM1ForDiscontinuity(WorkerPipeline& pipeline,
                                           std::uint64_t eventMs);
    void publishMartinM1Image(WorkerPipeline& pipeline, bool force);
    void consumeScottieObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    void terminateScottieForDiscontinuity(WorkerPipeline& pipeline,
                                          std::uint64_t eventMs);
    void publishScottieImage(WorkerPipeline& pipeline, bool force);
    void consumeRobotObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs,
        std::uint64_t inputEndSample);
    void terminateRobotForDiscontinuity(WorkerPipeline& pipeline,
                                        std::uint64_t eventMs);
    void finishRobotAtImageEnd(WorkerPipeline& pipeline,
                               std::uint64_t eventMs);
    void publishRobotImage(WorkerPipeline& pipeline, bool force);
    void consumeSequentialRgbObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    void terminateSequentialRgbForDiscontinuity(WorkerPipeline& pipeline,
                                                 std::uint64_t eventMs);
    void publishSequentialRgbImage(WorkerPipeline& pipeline, bool force);
    void consumePdObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    void terminatePdForDiscontinuity(WorkerPipeline& pipeline,
                                     std::uint64_t eventMs);
    void publishPdImage(WorkerPipeline& pipeline, bool force);
    void consumeAvtObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    void terminateAvtForDiscontinuity(WorkerPipeline& pipeline,
                                      std::uint64_t eventMs);
    void publishAvtImage(WorkerPipeline& pipeline, bool force);
    void consumeMmsstvObservations(
        WorkerPipeline& pipeline,
        const std::vector<SstvFrequencyObservation>& observations,
        std::uint64_t eventMs);
    void terminateMmsstvForDiscontinuity(WorkerPipeline& pipeline,
                                          std::uint64_t eventMs);
    void publishMmsstvImage(WorkerPipeline& pipeline, bool force);
    void recordWorkerFailure(const QString& detail) noexcept;
    void scheduleSnapshotNotification(bool force = false) noexcept;

    const Config m_config;
    // QObject child rather than a by-value QObject member: Qt then moves the
    // ingress together with this runtime if the controller is moved before it
    // starts, and QObject base destruction owns the final teardown.
    SstvAudioIngress* const m_ingress;
    SstvRxControlPolicy m_rxControlPolicy;
    SstvRxRetainedAudio m_retainedAudio;

    mutable std::mutex m_timestampMutex;
    SstvRxRouteToken m_route;
    SstvMonotonicTimeline m_timeline;

    // Serializes a lifecycle generation boundary against an already-popped
    // worker chunk.  Once switch/reset/stop returns, old-generation DSP cannot
    // publish another result.
    mutable std::mutex m_pipelineBoundaryMutex;
    mutable std::mutex m_controlMutex;
    std::condition_variable m_controlChanged;
    std::thread m_worker;
    std::atomic<bool> m_stopRequested {false};
    std::atomic<bool> m_lifecycleBoundaryRequested {false};
    std::atomic<bool> m_workerRunning {false};
    std::atomic<State> m_state {State::Inactive};
    // Owner-thread only guard against future synchronous callbacks added to a
    // lifecycle path.  Public transitions are never recursively entered.
    bool m_lifecycleTransitionActive {false};

    mutable std::mutex m_snapshotMutex;
    Snapshot m_snapshot;
    std::shared_ptr<const SstvImageSnapshot> m_latestImageSnapshot;
    std::atomic<std::uint64_t> m_revision {0U};
    std::atomic<bool> m_snapshotNotificationPending {false};
    std::atomic<bool> m_snapshotForcePending {false};
    std::atomic<std::int64_t> m_lastSnapshotNotificationNs {0};
    std::atomic<std::uint64_t> m_wrongThreadLifecycleCalls {0U};
    std::atomic<std::uint64_t> m_producerRejectedCalls {0U};

    // One pending Qt delivery per notification class.  High-rate worker data
    // overwrites the bounded latest value instead of growing the event queue.
    mutable std::mutex m_notificationMutex;
    bool m_stateNotificationPending {false};
    State m_pendingState {State::Inactive};
    std::uint64_t m_pendingStateGeneration {0U};
    bool m_visNotificationPending {false};
    std::uint64_t m_pendingVisGeneration {0U};
    VisSummary m_pendingVis;
    bool m_workerErrorNotificationPending {false};
    QString m_pendingWorkerError;
};

} // namespace decodium::sstv

Q_DECLARE_METATYPE(decodium::sstv::SstvRxRuntime::State)
