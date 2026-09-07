// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvPcm16Queue.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace decodium::sstv {

struct SstvPcm16Metadata final
{
    int sampleRate {0};
    SstvAudioSource source;
    qint64 monotonicTimestampNs {-1};
    std::uint64_t generation {0U};
};

// Native adapter for Decodium's existing decoded mono PCM fanout.
//
// Lifecycle methods are QObject-thread-affine.  enqueuePcm16(), tryPop(),
// waitPop(), and stats() are thread-safe and may be called directly by the
// audio callback or SSTV worker.  The callback boundary intentionally does no
// sample conversion, resampling, or DSP: it validates bounded metadata, moves
// the exact QVector<short> into a preallocated bounded queue, updates atomics,
// and wakes consumers.
//
// Do not queue QVector audio through Qt: queued delivery has no backpressure.
// Connect a real Decodium audio signal to a tiny relay with DirectConnection,
// have that relay call enqueuePcm16(), then connect the zero-payload
// pcmAvailable() wake to the worker with QueuedConnection.  Wake posting is
// coalesced, and BlockingQueuedConnection is neither needed nor supported by
// this contract.
class SstvAudioIngress final : public QObject
{
    Q_OBJECT

public:
    static constexpr std::size_t kHardMaximumSamplesPerCall = 262'144U;

    struct Config final
    {
        std::size_t maximumChunks {128U};
        std::size_t maximumQueuedSamples {1'200'000U};
        std::size_t maximumSamplesPerCall {65'536U};
    };

    enum class State : std::uint8_t
    {
        Inactive,
        Active,
        Cancelled,
        Shutdown,
    };
    Q_ENUM(State)

    using WaitResult = SstvPcm16Queue::WaitResult;

    struct Stats final
    {
        State state {State::Inactive};
        SstvAudioSource activeSource;
        std::uint64_t generation {0U};
        std::uint64_t nextSequence {0U};
        bool sequenceExhausted {false};
        std::uint32_t generationSampleRate {0U};
        bool hasAcceptedTimestamp {false};
        qint64 lastAcceptedTimestampNs {0};
        qint64 nextAllowedTimestampNs {0};

        std::uint64_t receivedCalls {0U};
        std::uint64_t acceptedChunks {0U};
        std::uint64_t acceptedSamples {0U};
        std::uint64_t rejectedInactiveCalls {0U};
        std::uint64_t rejectedCancelledCalls {0U};
        std::uint64_t rejectedShutdownCalls {0U};
        std::uint64_t rejectedEmptyCalls {0U};
        std::uint64_t rejectedOversizeCalls {0U};
        std::uint64_t rejectedInvalidRateCalls {0U};
        std::uint64_t rejectedRateChangeCalls {0U};
        std::uint64_t rejectedInvalidTimestampCalls {0U};
        std::uint64_t rejectedOverlappingTimestampCalls {0U};
        std::uint64_t rejectedInvalidSourceCalls {0U};
        std::uint64_t rejectedSourceMismatchCalls {0U};
        std::uint64_t rejectedStaleGenerationCalls {0U};
        std::uint64_t rejectedSequenceExhaustedCalls {0U};
        std::uint64_t queueRejectedCalls {0U};
        std::uint64_t enqueueFailures {0U};

        std::uint64_t sourceActivations {0U};
        std::uint64_t sourceSwitches {0U};
        std::uint64_t streamResets {0U};
        std::uint64_t cancellations {0U};
        std::uint64_t restarts {0U};
        std::uint64_t deactivations {0U};
        std::uint64_t shutdowns {0U};
        std::uint64_t rejectedWrongThreadLifecycleCalls {0U};
        std::uint64_t rejectedLifecycleCalls {0U};
        std::uint64_t generationExhaustions {0U};

        std::uint64_t coalescedWakePosts {0U};
        std::uint64_t coalescedWakeSuppressions {0U};
        std::uint64_t coalescedWakeDeliveries {0U};
        std::uint64_t coalescedWakePostFailures {0U};

        SstvPcm16Queue::Stats queue;
    };

    explicit SstvAudioIngress(QObject* parent = nullptr);
    explicit SstvAudioIngress(Config config, QObject* parent = nullptr);
    ~SstvAudioIngress() override;

    SstvAudioIngress(const SstvAudioIngress&) = delete;
    SstvAudioIngress& operator=(const SstvAudioIngress&) = delete;
    SstvAudioIngress(SstvAudioIngress&&) = delete;
    SstvAudioIngress& operator=(SstvAudioIngress&&) = delete;

    Config configuration() const noexcept;
    State state() const noexcept;
    bool isActive() const noexcept;
    SstvAudioSource activeSource() const noexcept;
    std::uint64_t generation() const noexcept;

    bool tryPop(SstvPcm16Chunk& chunk);
    WaitResult waitPop(SstvPcm16Chunk& chunk);
    Stats stats() const noexcept;

    // Direct, bounded callback API.  QVector is by value so existing by-value
    // signals can move it, while const-reference producers incur only Qt's
    // implicit-share refcount operation.  Every call must carry the generation
    // captured after the corresponding owner-thread lifecycle transition.
    // The timestamp is the block start in a local monotonic clock domain.
    bool enqueuePcm16(QVector<short> samples,
                      SstvPcm16Metadata metadata) noexcept;
    bool enqueuePcm16(QVector<short> samples,
                      int sampleRate,
                      SstvAudioSourceKind kind,
                      quint32 streamId,
                      qint64 monotonicTimestampNs,
                      quint64 generation) noexcept;

public Q_SLOTS:
    // Lifecycle is deliberately owner-thread only.  switchSource(),
    // resetStream(), and restart() always create a new generation, including
    // when the source identity is unchanged.  shutdown() is terminal and
    // wakes waitPop(); external producers must be disconnected and consumer
    // threads joined before this callback target is destroyed.
    bool activateSource(SstvAudioSourceKind kind,
                        quint32 streamId,
                        quint64 firstSequence = 0U);
    bool switchSource(SstvAudioSourceKind kind,
                      quint32 streamId,
                      quint64 firstSequence = 0U);
    bool resetStream(quint64 firstSequence = 0U);
    bool deactivate();
    bool cancel();
    bool restart(quint64 firstSequence = 0U);
    bool shutdown();

Q_SIGNALS:
    // Zero-payload, coalesced readiness notification.  The receiver must drain
    // tryPop() until empty (or use waitPop() instead of this Qt wake).
    void pcmAvailable();

private:
    struct AtomicCounters final
    {
        std::atomic<std::uint64_t> receivedCalls {0U};
        std::atomic<std::uint64_t> acceptedChunks {0U};
        std::atomic<std::uint64_t> acceptedSamples {0U};
        std::atomic<std::uint64_t> rejectedInactiveCalls {0U};
        std::atomic<std::uint64_t> rejectedCancelledCalls {0U};
        std::atomic<std::uint64_t> rejectedShutdownCalls {0U};
        std::atomic<std::uint64_t> rejectedEmptyCalls {0U};
        std::atomic<std::uint64_t> rejectedOversizeCalls {0U};
        std::atomic<std::uint64_t> rejectedInvalidRateCalls {0U};
        std::atomic<std::uint64_t> rejectedRateChangeCalls {0U};
        std::atomic<std::uint64_t> rejectedInvalidTimestampCalls {0U};
        std::atomic<std::uint64_t> rejectedOverlappingTimestampCalls {0U};
        std::atomic<std::uint64_t> rejectedInvalidSourceCalls {0U};
        std::atomic<std::uint64_t> rejectedSourceMismatchCalls {0U};
        std::atomic<std::uint64_t> rejectedStaleGenerationCalls {0U};
        std::atomic<std::uint64_t> rejectedSequenceExhaustedCalls {0U};
        std::atomic<std::uint64_t> queueRejectedCalls {0U};
        std::atomic<std::uint64_t> enqueueFailures {0U};
        std::atomic<std::uint64_t> sourceActivations {0U};
        std::atomic<std::uint64_t> sourceSwitches {0U};
        std::atomic<std::uint64_t> streamResets {0U};
        std::atomic<std::uint64_t> cancellations {0U};
        std::atomic<std::uint64_t> restarts {0U};
        std::atomic<std::uint64_t> deactivations {0U};
        std::atomic<std::uint64_t> shutdowns {0U};
        std::atomic<std::uint64_t> rejectedWrongThreadLifecycleCalls {0U};
        std::atomic<std::uint64_t> rejectedLifecycleCalls {0U};
        std::atomic<std::uint64_t> generationExhaustions {0U};
        std::atomic<std::uint64_t> coalescedWakePosts {0U};
        std::atomic<std::uint64_t> coalescedWakeSuppressions {0U};
        std::atomic<std::uint64_t> coalescedWakeDeliveries {0U};
        std::atomic<std::uint64_t> coalescedWakePostFailures {0U};
    };

    static Config validateConfig(Config config);
    static bool sourceKindIsValid(SstvAudioSourceKind kind) noexcept;
    static bool blockEndTimestamp(qint64 startTimestampNs,
                                  std::size_t sampleCount,
                                  std::uint32_t sampleRate,
                                  qint64& endTimestampNs) noexcept;
    static std::uint64_t packSource(SstvAudioSource source) noexcept;
    static SstvAudioSource unpackSource(std::uint64_t packed) noexcept;
    static void saturatingAdd(std::atomic<std::uint64_t>& value,
                              std::uint64_t increment = 1U) noexcept;

    bool isOnOwnerThread() const noexcept;
    bool rejectLifecycleCall(bool wrongThread = false) noexcept;
    bool advanceGenerationLocked() noexcept;
    void configureGenerationLocked(SstvAudioSource source,
                                   std::uint64_t firstSequence) noexcept;
    void recordStateRejection(State rejectedState) noexcept;
    void scheduleCoalescedWake() noexcept;
    void deliverCoalescedWake() noexcept;

    const Config m_config;
    SstvPcm16Queue m_queue;
    mutable std::mutex m_enqueueMutex;
    std::atomic<State> m_state {State::Inactive};
    std::atomic<std::uint64_t> m_activeSource {0U};
    std::atomic<std::uint64_t> m_generation {0U};
    std::atomic<std::uint64_t> m_nextSequence {0U};
    std::atomic<bool> m_sequenceExhausted {false};
    std::atomic<std::uint32_t> m_generationSampleRate {0U};
    std::atomic<bool> m_hasTimestamp {false};
    std::atomic<qint64> m_lastTimestampNs {0};
    std::atomic<qint64> m_nextAllowedTimestampNs {0};
    std::atomic<bool> m_wakePosted {false};
    AtomicCounters m_counters;
};

} // namespace decodium::sstv

Q_DECLARE_METATYPE(decodium::sstv::SstvAudioSourceKind)
