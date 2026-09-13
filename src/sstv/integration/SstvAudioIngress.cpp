// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvAudioIngress.h"

#include "../dsp/SstvResampler.h"

#include <QMetaObject>
#include <QThread>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kSourceKindMask = 0xffU;
constexpr unsigned int kSourceStreamShift = 8U;
constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000U;

} // namespace

static_assert(std::numeric_limits<short>::min() == -32'768,
              "SSTV ingress requires 16-bit signed short PCM");
static_assert(std::numeric_limits<short>::max() == 32'767,
              "SSTV ingress requires 16-bit signed short PCM");

SstvAudioIngress::SstvAudioIngress(QObject* parent)
    : SstvAudioIngress(Config {}, parent)
{
}

SstvAudioIngress::SstvAudioIngress(Config config, QObject* parent)
    : QObject(parent)
    , m_config(validateConfig(config))
    , m_queue(m_config.maximumChunks, m_config.maximumQueuedSamples)
{
}

SstvAudioIngress::~SstvAudioIngress()
{
    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    m_state.store(State::Shutdown, std::memory_order_release);
    m_queue.cancel();
    m_wakePosted.store(false, std::memory_order_release);
}

SstvAudioIngress::Config SstvAudioIngress::configuration() const noexcept
{
    return m_config;
}

SstvAudioIngress::State SstvAudioIngress::state() const noexcept
{
    return m_state.load(std::memory_order_acquire);
}

bool SstvAudioIngress::isActive() const noexcept
{
    return state() == State::Active;
}

SstvAudioSource SstvAudioIngress::activeSource() const noexcept
{
    return unpackSource(m_activeSource.load(std::memory_order_acquire));
}

std::uint64_t SstvAudioIngress::generation() const noexcept
{
    return m_generation.load(std::memory_order_acquire);
}

bool SstvAudioIngress::tryPop(SstvPcm16Chunk& chunk)
{
    return m_queue.tryPop(chunk);
}

SstvAudioIngress::WaitResult SstvAudioIngress::waitPop(
    SstvPcm16Chunk& chunk)
{
    return m_queue.waitPop(chunk);
}

SstvAudioIngress::Stats SstvAudioIngress::stats() const noexcept
{
    Stats result;
    result.state = state();
    result.activeSource = activeSource();
    result.generation = generation();
    result.nextSequence = m_nextSequence.load(std::memory_order_relaxed);
    result.sequenceExhausted =
        m_sequenceExhausted.load(std::memory_order_acquire);
    result.generationSampleRate =
        m_generationSampleRate.load(std::memory_order_acquire);
    result.hasAcceptedTimestamp =
        m_hasTimestamp.load(std::memory_order_acquire);
    result.lastAcceptedTimestampNs =
        m_lastTimestampNs.load(std::memory_order_relaxed);
    result.nextAllowedTimestampNs =
        m_nextAllowedTimestampNs.load(std::memory_order_relaxed);

    result.receivedCalls =
        m_counters.receivedCalls.load(std::memory_order_relaxed);
    result.acceptedChunks =
        m_counters.acceptedChunks.load(std::memory_order_relaxed);
    result.acceptedSamples =
        m_counters.acceptedSamples.load(std::memory_order_relaxed);
    result.rejectedInactiveCalls =
        m_counters.rejectedInactiveCalls.load(std::memory_order_relaxed);
    result.rejectedCancelledCalls =
        m_counters.rejectedCancelledCalls.load(std::memory_order_relaxed);
    result.rejectedShutdownCalls =
        m_counters.rejectedShutdownCalls.load(std::memory_order_relaxed);
    result.rejectedEmptyCalls =
        m_counters.rejectedEmptyCalls.load(std::memory_order_relaxed);
    result.rejectedOversizeCalls =
        m_counters.rejectedOversizeCalls.load(std::memory_order_relaxed);
    result.rejectedInvalidRateCalls =
        m_counters.rejectedInvalidRateCalls.load(std::memory_order_relaxed);
    result.rejectedRateChangeCalls =
        m_counters.rejectedRateChangeCalls.load(std::memory_order_relaxed);
    result.rejectedInvalidTimestampCalls =
        m_counters.rejectedInvalidTimestampCalls.load(
            std::memory_order_relaxed);
    result.rejectedOverlappingTimestampCalls =
        m_counters.rejectedOverlappingTimestampCalls.load(
            std::memory_order_relaxed);
    result.rejectedInvalidSourceCalls =
        m_counters.rejectedInvalidSourceCalls.load(std::memory_order_relaxed);
    result.rejectedSourceMismatchCalls =
        m_counters.rejectedSourceMismatchCalls.load(std::memory_order_relaxed);
    result.rejectedStaleGenerationCalls =
        m_counters.rejectedStaleGenerationCalls.load(
            std::memory_order_relaxed);
    result.rejectedSequenceExhaustedCalls =
        m_counters.rejectedSequenceExhaustedCalls.load(
            std::memory_order_relaxed);
    result.queueRejectedCalls =
        m_counters.queueRejectedCalls.load(std::memory_order_relaxed);
    result.enqueueFailures =
        m_counters.enqueueFailures.load(std::memory_order_relaxed);

    result.sourceActivations =
        m_counters.sourceActivations.load(std::memory_order_relaxed);
    result.sourceSwitches =
        m_counters.sourceSwitches.load(std::memory_order_relaxed);
    result.streamResets =
        m_counters.streamResets.load(std::memory_order_relaxed);
    result.cancellations =
        m_counters.cancellations.load(std::memory_order_relaxed);
    result.restarts = m_counters.restarts.load(std::memory_order_relaxed);
    result.deactivations =
        m_counters.deactivations.load(std::memory_order_relaxed);
    result.shutdowns =
        m_counters.shutdowns.load(std::memory_order_relaxed);
    result.rejectedWrongThreadLifecycleCalls =
        m_counters.rejectedWrongThreadLifecycleCalls.load(
            std::memory_order_relaxed);
    result.rejectedLifecycleCalls =
        m_counters.rejectedLifecycleCalls.load(std::memory_order_relaxed);
    result.generationExhaustions =
        m_counters.generationExhaustions.load(std::memory_order_relaxed);

    result.coalescedWakePosts =
        m_counters.coalescedWakePosts.load(std::memory_order_relaxed);
    result.coalescedWakeSuppressions =
        m_counters.coalescedWakeSuppressions.load(std::memory_order_relaxed);
    result.coalescedWakeDeliveries =
        m_counters.coalescedWakeDeliveries.load(std::memory_order_relaxed);
    result.coalescedWakePostFailures =
        m_counters.coalescedWakePostFailures.load(std::memory_order_relaxed);
    result.queue = m_queue.stats();
    return result;
}

bool SstvAudioIngress::enqueuePcm16(QVector<short> samples,
                                    SstvPcm16Metadata metadata) noexcept
{
    saturatingAdd(m_counters.receivedCalls);

    // Hot inactive path: one state read and one bounded diagnostic update.
    // In particular, the QVector is not inspected, detached, or copied.
    State ingressState = m_state.load(std::memory_order_acquire);
    if (ingressState != State::Active) {
        recordStateRejection(ingressState);
        return false;
    }

    const std::uint64_t currentGeneration =
        m_generation.load(std::memory_order_acquire);
    if (metadata.generation == 0U
        || metadata.generation != currentGeneration) {
        saturatingAdd(m_counters.rejectedStaleGenerationCalls);
        return false;
    }
    if (!sourceKindIsValid(metadata.source.kind)) {
        saturatingAdd(m_counters.rejectedInvalidSourceCalls);
        return false;
    }
    const std::uint64_t packedSource = packSource(metadata.source);
    if (packedSource
        != m_activeSource.load(std::memory_order_acquire)) {
        saturatingAdd(m_counters.rejectedSourceMismatchCalls);
        return false;
    }

    const qsizetype signedSampleCount = samples.size();
    if (signedSampleCount <= 0) {
        saturatingAdd(m_counters.rejectedEmptyCalls);
        return false;
    }
    const auto sampleCount = static_cast<std::size_t>(signedSampleCount);
    if (sampleCount > m_config.maximumSamplesPerCall) {
        saturatingAdd(m_counters.rejectedOversizeCalls);
        return false;
    }
    if (metadata.sampleRate <= 0
        || !SstvResampler::isSupportedInputRate(
            static_cast<std::uint32_t>(metadata.sampleRate))) {
        saturatingAdd(m_counters.rejectedInvalidRateCalls);
        return false;
    }
    if (metadata.monotonicTimestampNs < 0) {
        saturatingAdd(m_counters.rejectedInvalidTimestampCalls);
        return false;
    }

    const auto sampleRate = static_cast<std::uint32_t>(metadata.sampleRate);
    qint64 blockEndNs = 0;
    if (!blockEndTimestamp(metadata.monotonicTimestampNs,
                           sampleCount,
                           sampleRate,
                           blockEndNs)) {
        saturatingAdd(m_counters.rejectedInvalidTimestampCalls);
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(m_enqueueMutex);

        ingressState = m_state.load(std::memory_order_acquire);
        if (ingressState != State::Active) {
            recordStateRejection(ingressState);
            return false;
        }
        if (metadata.generation
            != m_generation.load(std::memory_order_acquire)) {
            saturatingAdd(m_counters.rejectedStaleGenerationCalls);
            return false;
        }
        if (packedSource
            != m_activeSource.load(std::memory_order_acquire)) {
            saturatingAdd(m_counters.rejectedSourceMismatchCalls);
            return false;
        }

        const std::uint32_t fixedRate =
            m_generationSampleRate.load(std::memory_order_relaxed);
        if (fixedRate != 0U && fixedRate != sampleRate) {
            saturatingAdd(m_counters.rejectedRateChangeCalls);
            return false;
        }
        if (m_sequenceExhausted.load(std::memory_order_relaxed)) {
            saturatingAdd(m_counters.rejectedSequenceExhaustedCalls);
            return false;
        }
        if (m_hasTimestamp.load(std::memory_order_relaxed)
            && metadata.monotonicTimestampNs
                   < m_nextAllowedTimestampNs.load(
                       std::memory_order_relaxed)) {
            saturatingAdd(m_counters.rejectedOverlappingTimestampCalls);
            return false;
        }

        const std::uint64_t sequence =
            m_nextSequence.load(std::memory_order_relaxed);
        SstvPcm16Chunk chunk;
        chunk.samples = std::move(samples);
        chunk.source = metadata.source;
        chunk.sampleRate = sampleRate;
        chunk.startTime =
            std::chrono::nanoseconds {metadata.monotonicTimestampNs};
        chunk.sequence = sequence;
        chunk.generation = metadata.generation;

        bool queued = false;
        try {
            queued = m_queue.push(std::move(chunk));
        } catch (...) {
            saturatingAdd(m_counters.enqueueFailures);
            return false;
        }
        if (!queued) {
            saturatingAdd(m_counters.queueRejectedCalls);
            return false;
        }

        if (fixedRate == 0U) {
            m_generationSampleRate.store(sampleRate,
                                         std::memory_order_release);
        }
        m_hasTimestamp.store(true, std::memory_order_release);
        m_lastTimestampNs.store(metadata.monotonicTimestampNs,
                                std::memory_order_relaxed);
        m_nextAllowedTimestampNs.store(blockEndNs,
                                       std::memory_order_relaxed);

        if (sequence == std::numeric_limits<std::uint64_t>::max()) {
            m_sequenceExhausted.store(true, std::memory_order_release);
        } else {
            m_nextSequence.store(sequence + 1U, std::memory_order_relaxed);
        }

        saturatingAdd(m_counters.acceptedChunks);
        saturatingAdd(m_counters.acceptedSamples,
                      static_cast<std::uint64_t>(sampleCount));
    }

    scheduleCoalescedWake();
    return true;
}

bool SstvAudioIngress::enqueuePcm16(QVector<short> samples,
                                    int sampleRate,
                                    SstvAudioSourceKind kind,
                                    quint32 streamId,
                                    qint64 monotonicTimestampNs,
                                    quint64 eventGeneration) noexcept
{
    SstvPcm16Metadata metadata;
    metadata.sampleRate = sampleRate;
    metadata.source = {kind, static_cast<std::uint32_t>(streamId)};
    metadata.monotonicTimestampNs = monotonicTimestampNs;
    metadata.generation = static_cast<std::uint64_t>(eventGeneration);
    return enqueuePcm16(std::move(samples), metadata);
}

bool SstvAudioIngress::activateSource(SstvAudioSourceKind kind,
                                      quint32 streamId,
                                      quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }
    if (!sourceKindIsValid(kind)) {
        saturatingAdd(m_counters.rejectedInvalidSourceCalls);
        return rejectLifecycleCall();
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    if (m_state.load(std::memory_order_acquire) != State::Inactive) {
        return rejectLifecycleCall();
    }
    if (!advanceGenerationLocked()) {
        return rejectLifecycleCall();
    }

    m_queue.reset();
    configureGenerationLocked(
        {kind, static_cast<std::uint32_t>(streamId)},
        static_cast<std::uint64_t>(firstSequence));
    m_state.store(State::Active, std::memory_order_release);
    saturatingAdd(m_counters.sourceActivations);
    return true;
}

bool SstvAudioIngress::switchSource(SstvAudioSourceKind kind,
                                    quint32 streamId,
                                    quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }
    if (!sourceKindIsValid(kind)) {
        saturatingAdd(m_counters.rejectedInvalidSourceCalls);
        return rejectLifecycleCall();
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    const State current = m_state.load(std::memory_order_acquire);
    if (current != State::Active && current != State::Cancelled) {
        return rejectLifecycleCall();
    }
    m_state.store(State::Inactive, std::memory_order_release);
    if (!advanceGenerationLocked()) {
        m_state.store(current, std::memory_order_release);
        return rejectLifecycleCall();
    }

    m_queue.reset();
    configureGenerationLocked(
        {kind, static_cast<std::uint32_t>(streamId)},
        static_cast<std::uint64_t>(firstSequence));
    m_state.store(State::Active, std::memory_order_release);
    saturatingAdd(m_counters.sourceSwitches);
    return true;
}

bool SstvAudioIngress::resetStream(quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    if (m_state.load(std::memory_order_acquire) != State::Active) {
        return rejectLifecycleCall();
    }
    m_state.store(State::Inactive, std::memory_order_release);
    if (!advanceGenerationLocked()) {
        m_state.store(State::Active, std::memory_order_release);
        return rejectLifecycleCall();
    }

    const SstvAudioSource retained = unpackSource(
        m_activeSource.load(std::memory_order_relaxed));
    m_queue.reset();
    configureGenerationLocked(
        retained, static_cast<std::uint64_t>(firstSequence));
    m_state.store(State::Active, std::memory_order_release);
    saturatingAdd(m_counters.streamResets);
    return true;
}

bool SstvAudioIngress::deactivate()
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    const State current = m_state.load(std::memory_order_acquire);
    if (current != State::Active && current != State::Cancelled) {
        return rejectLifecycleCall();
    }

    m_state.store(State::Inactive, std::memory_order_release);
    m_queue.cancel();
    configureGenerationLocked({}, 0U);
    saturatingAdd(m_counters.deactivations);
    return true;
}

bool SstvAudioIngress::cancel()
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    if (m_state.load(std::memory_order_acquire) != State::Active) {
        return rejectLifecycleCall();
    }

    m_state.store(State::Cancelled, std::memory_order_release);
    m_queue.cancel();
    m_nextSequence.store(0U, std::memory_order_relaxed);
    m_sequenceExhausted.store(false, std::memory_order_release);
    m_generationSampleRate.store(0U, std::memory_order_release);
    m_hasTimestamp.store(false, std::memory_order_release);
    m_lastTimestampNs.store(0, std::memory_order_relaxed);
    m_nextAllowedTimestampNs.store(0, std::memory_order_relaxed);
    saturatingAdd(m_counters.cancellations);
    return true;
}

bool SstvAudioIngress::restart(quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    if (m_state.load(std::memory_order_acquire) != State::Cancelled) {
        return rejectLifecycleCall();
    }
    const SstvAudioSource retained = unpackSource(
        m_activeSource.load(std::memory_order_relaxed));
    if (!sourceKindIsValid(retained.kind)
        || !advanceGenerationLocked()) {
        return rejectLifecycleCall();
    }

    m_queue.restart();
    configureGenerationLocked(
        retained, static_cast<std::uint64_t>(firstSequence));
    m_state.store(State::Active, std::memory_order_release);
    saturatingAdd(m_counters.restarts);
    return true;
}

bool SstvAudioIngress::shutdown()
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall(true);
    }

    std::lock_guard<std::mutex> lock(m_enqueueMutex);
    if (m_state.load(std::memory_order_acquire) == State::Shutdown) {
        return rejectLifecycleCall();
    }

    m_state.store(State::Shutdown, std::memory_order_release);
    m_queue.cancel();
    configureGenerationLocked({}, 0U);
    saturatingAdd(m_counters.shutdowns);
    return true;
}

SstvAudioIngress::Config SstvAudioIngress::validateConfig(Config config)
{
    if (config.maximumChunks == 0U
        || config.maximumChunks > SstvPcm16Queue::kHardMaximumChunks) {
        throw std::invalid_argument("invalid SSTV ingress chunk capacity");
    }
    if (config.maximumQueuedSamples == 0U
        || config.maximumQueuedSamples
               > SstvPcm16Queue::kHardMaximumSamples) {
        throw std::invalid_argument("invalid SSTV ingress sample capacity");
    }
    if (config.maximumSamplesPerCall == 0U
        || config.maximumSamplesPerCall > kHardMaximumSamplesPerCall
        || config.maximumSamplesPerCall
               > config.maximumQueuedSamples) {
        throw std::invalid_argument("invalid SSTV ingress block capacity");
    }
    return config;
}

bool SstvAudioIngress::sourceKindIsValid(
    SstvAudioSourceKind kind) noexcept
{
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::LegacyBackend:
    case SstvAudioSourceKind::DecoPort:
    case SstvAudioSourceKind::Tci:
    case SstvAudioSourceKind::WebSdr:
    case SstvAudioSourceKind::RtlSdr:
    case SstvAudioSourceKind::Replay:
        return true;
    case SstvAudioSourceKind::Unknown:
        return false;
    }
    return false;
}

bool SstvAudioIngress::blockEndTimestamp(qint64 startTimestampNs,
                                         std::size_t sampleCount,
                                         std::uint32_t sampleRate,
                                         qint64& endTimestampNs) noexcept
{
    if (startTimestampNs < 0 || sampleCount == 0U || sampleRate == 0U
        || sampleCount
               > std::numeric_limits<std::uint64_t>::max()
                     / kNanosecondsPerSecond) {
        return false;
    }

    const std::uint64_t numerator =
        static_cast<std::uint64_t>(sampleCount) * kNanosecondsPerSecond;
    std::uint64_t durationNs = numerator / sampleRate;
    if (numerator % sampleRate != 0U) {
        ++durationNs;
    }

    const auto maximumTimestamp =
        static_cast<std::uint64_t>(std::numeric_limits<qint64>::max());
    if (durationNs > maximumTimestamp
        || static_cast<std::uint64_t>(startTimestampNs)
               > maximumTimestamp - durationNs) {
        return false;
    }

    endTimestampNs = startTimestampNs + static_cast<qint64>(durationNs);
    return true;
}

std::uint64_t SstvAudioIngress::packSource(
    SstvAudioSource source) noexcept
{
    return static_cast<std::uint64_t>(source.kind)
        | (static_cast<std::uint64_t>(source.streamId)
           << kSourceStreamShift);
}

SstvAudioSource SstvAudioIngress::unpackSource(
    std::uint64_t packed) noexcept
{
    return {
        static_cast<SstvAudioSourceKind>(packed & kSourceKindMask),
        static_cast<std::uint32_t>(packed >> kSourceStreamShift),
    };
}

void SstvAudioIngress::saturatingAdd(
    std::atomic<std::uint64_t>& value,
    std::uint64_t increment) noexcept
{
    std::uint64_t observed = value.load(std::memory_order_relaxed);
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    for (;;) {
        const std::uint64_t desired = increment > maximum - observed
            ? maximum
            : observed + increment;
        if (value.compare_exchange_weak(observed,
                                        desired,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
            return;
        }
    }
}

bool SstvAudioIngress::isOnOwnerThread() const noexcept
{
    return QThread::currentThread() == thread();
}

bool SstvAudioIngress::rejectLifecycleCall(bool wrongThread) noexcept
{
    if (wrongThread) {
        saturatingAdd(m_counters.rejectedWrongThreadLifecycleCalls);
    }
    saturatingAdd(m_counters.rejectedLifecycleCalls);
    return false;
}

bool SstvAudioIngress::advanceGenerationLocked() noexcept
{
    const std::uint64_t current =
        m_generation.load(std::memory_order_relaxed);
    if (current == std::numeric_limits<std::uint64_t>::max()) {
        saturatingAdd(m_counters.generationExhaustions);
        return false;
    }
    m_generation.store(current + 1U, std::memory_order_release);
    return true;
}

void SstvAudioIngress::configureGenerationLocked(
    SstvAudioSource source,
    std::uint64_t firstSequence) noexcept
{
    m_activeSource.store(packSource(source), std::memory_order_release);
    m_nextSequence.store(firstSequence, std::memory_order_relaxed);
    m_sequenceExhausted.store(false, std::memory_order_release);
    m_generationSampleRate.store(0U, std::memory_order_release);
    m_hasTimestamp.store(false, std::memory_order_release);
    m_lastTimestampNs.store(0, std::memory_order_relaxed);
    m_nextAllowedTimestampNs.store(0, std::memory_order_relaxed);
}

void SstvAudioIngress::recordStateRejection(State rejectedState) noexcept
{
    switch (rejectedState) {
    case State::Inactive:
        saturatingAdd(m_counters.rejectedInactiveCalls);
        return;
    case State::Cancelled:
        saturatingAdd(m_counters.rejectedCancelledCalls);
        return;
    case State::Shutdown:
        saturatingAdd(m_counters.rejectedShutdownCalls);
        return;
    case State::Active:
        // The caller only invokes this helper for a non-active state.
        return;
    }
}

void SstvAudioIngress::scheduleCoalescedWake() noexcept
{
    bool expected = false;
    if (!m_wakePosted.compare_exchange_strong(expected,
                                              true,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
        saturatingAdd(m_counters.coalescedWakeSuppressions);
        return;
    }

    const bool posted = QMetaObject::invokeMethod(
        this,
        [this] { deliverCoalescedWake(); },
        Qt::QueuedConnection);
    if (posted) {
        saturatingAdd(m_counters.coalescedWakePosts);
    } else {
        m_wakePosted.store(false, std::memory_order_release);
        saturatingAdd(m_counters.coalescedWakePostFailures);
    }
}

void SstvAudioIngress::deliverCoalescedWake() noexcept
{
    m_wakePosted.store(false, std::memory_order_release);
    if (m_state.load(std::memory_order_acquire) != State::Active
        || m_queue.stats().queuedChunks == 0U) {
        return;
    }

    saturatingAdd(m_counters.coalescedWakeDeliveries);
    Q_EMIT pcmAvailable();
}

} // namespace decodium::sstv
