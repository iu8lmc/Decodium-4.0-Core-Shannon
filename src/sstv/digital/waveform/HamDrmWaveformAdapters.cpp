// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmWaveformAdapters.h"

#include "HamDrmPacketCodec.h"
#include "HamDrmWaveformCodec.h"
#include "../../dsp/SstvResampler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace decodium::sstv::hamdrm::waveform {
namespace {

constexpr std::uint32_t kTxSampleRate = kHamDrmNativePcmSampleRateHz;
constexpr std::size_t kHardMaximumRxQueuedChunks = 1'024U;
constexpr std::size_t kHardMaximumRxQueuedSamples = 12'000'000U;
constexpr std::size_t kHardMaximumRxChunkSamples = 1U << 20U;
constexpr std::size_t kHardMaximumRxSessionFrames = 216'000U;
constexpr std::size_t kHardMaximumTxGroups = 4'096U;
constexpr std::size_t kHardMaximumTxQueuedBytes = 8U * 1'024U * 1'024U;
constexpr std::size_t kHardMaximumTxFrames = 18'000U;

bool adapterLimitsValid(const HamDrmNativeAdapterLimits& limits) noexcept
{
    return limits.maximumRxQueuedChunks != 0U
        && limits.maximumRxQueuedChunks <= kHardMaximumRxQueuedChunks
        && limits.maximumRxQueuedSamples != 0U
        && limits.maximumRxQueuedSamples <= kHardMaximumRxQueuedSamples
        && limits.maximumRxChunkSamples != 0U
        && limits.maximumRxChunkSamples <= kHardMaximumRxChunkSamples
        && limits.maximumRxSessionFrames != 0U
        && limits.maximumRxSessionFrames <= kHardMaximumRxSessionFrames
        && limits.maximumTxGroups != 0U
        && limits.maximumTxGroups <= kHardMaximumTxGroups
        && limits.maximumTxQueuedBytes != 0U
        && limits.maximumTxQueuedBytes <= kHardMaximumTxQueuedBytes
        && limits.maximumTxFrames != 0U
        && limits.maximumTxFrames <= kHardMaximumTxFrames;
}

void saturatingAdd(std::uint64_t& value,
                   std::uint64_t increment = 1U) noexcept
{
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    value = increment > maximum - value ? maximum : value + increment;
}

HamDrmWaveformConfig waveformConfig(const HamDrmProfile& profile,
                                    std::uint32_t sampleRate,
                                    std::string callsign = {})
{
    HamDrmWaveformConfig config;
    config.robustness = profile.robustness;
    config.occupiedBandwidth = profile.occupiedBandwidth;
    config.constellation = profile.constellation;
    config.protection = profile.protection;
    config.interleaver = profile.interleaver;
    config.pcmSampleRateHz = sampleRate;
    if (!callsign.empty()) {
        config.callsign = std::move(callsign);
    }
    return config;
}

bool validConfiguredCallsign(const std::string& callsign) noexcept
{
    if (callsign.empty() || callsign.size() > 9U) {
        return false;
    }
    return std::all_of(callsign.begin(), callsign.end(), [](char character) {
        const unsigned char value = static_cast<unsigned char>(character);
        return (value >= static_cast<unsigned char>('A')
                    && value <= static_cast<unsigned char>('Z'))
            || (value >= static_cast<unsigned char>('0')
                    && value <= static_cast<unsigned char>('9'))
            || value == static_cast<unsigned char>('/');
    });
}

HamDrmWaveformCapability nativeCapability(bool connected,
                                          const char* direction)
{
    HamDrmWaveformCapability result;
    result.completeBackend = connected;
    result.backendName = QStringLiteral("Decodium native HAMDRM waveform");
    result.detail = connected
        ? QStringLiteral(
              "Native FAC/MSC subset over Decodium shared audio and TX authority; no independent RF interoperability fixture")
        : QStringLiteral("Native HAMDRM %1 hooks are not connected")
              .arg(QString::fromLatin1(direction));
    return result;
}

class HamDrmSourceError final : public std::runtime_error
{
public:
    explicit HamDrmSourceError(HamDrmStatus sourceStatus)
        : std::runtime_error(sourceStatus.detail)
        , status(std::move(sourceStatus))
    {
    }

    HamDrmStatus status;
};

std::vector<std::vector<std::uint8_t>> orderedGroups(
    HamDrmEncodedObject object)
{
    std::vector<std::vector<std::uint8_t>> groups;
    groups.reserve(object.headerGroups.size() + object.bodyGroups.size());
    for (auto& group : object.headerGroups) {
        groups.push_back(std::move(group));
    }
    for (auto& group : object.bodyGroups) {
        groups.push_back(std::move(group));
    }
    return groups;
}

class HamDrmPcm16Source final : public SstvPcm16Source
{
public:
    HamDrmPcm16Source(HamDrmWaveformConfig config,
                      HamDrmWaveformLimits limits,
                      std::vector<std::vector<std::uint8_t>> groups)
        : config_(std::move(config))
        , limits_(limits)
        , groups_(std::move(groups))
    {
        const auto capacity = hamDrmWaveformCapacity(config_, limits_);
        if (!capacity.ok()) {
            throw HamDrmSourceError(capacity.status);
        }
        std::uint8_t continuity = 0U;
        std::size_t dataFrames = 0U;
        for (const auto& group : groups_) {
            const auto packetized = hamDrmPacketizeDataUnit(
                group,
                {capacity.value->packetBodyBytes,
                 config_.packetId,
                 limits_.maximumDataGroupBytes},
                continuity);
            if (!packetized.ok()) {
                throw HamDrmSourceError(packetized.status);
            }
            if (packetized.value->packets.size()
                > limits_.maximumFrames - std::min(
                      limits_.maximumFrames, dataFrames)) {
                throw HamDrmSourceError(HamDrmStatus::failure(
                    HamDrmErrorCode::LimitExceeded,
                    "HAMDRM TX packet plan exceeds the frame limit"));
            }
            dataFrames += packetized.value->packets.size();
            continuity = packetized.value->nextContinuityIndex;
        }
        const std::size_t flushFrames = dataFrames == 0U ? 0U
            : capacity.value->interleaverDepthFrames - 1U;
        if (dataFrames == 0U
            || flushFrames > limits_.maximumFrames - dataFrames) {
            throw HamDrmSourceError(HamDrmStatus::failure(
                HamDrmErrorCode::LimitExceeded,
                "HAMDRM TX object has no packets or exceeds the frame limit"));
        }
        const std::uint64_t frameCount = static_cast<std::uint64_t>(
            dataFrames + flushFrames);
        const std::uint64_t samplesPerFrame = static_cast<std::uint64_t>(
            capacity.value->samplesPerFrame);
        if (samplesPerFrame == 0U
            || frameCount > std::numeric_limits<std::uint64_t>::max()
                    / samplesPerFrame) {
            throw HamDrmSourceError(HamDrmStatus::failure(
                HamDrmErrorCode::LimitExceeded,
                "HAMDRM TX sample plan overflow"));
        }
        totalSamples_ = frameCount * samplesPerFrame;
        samplesPerFrame_ = static_cast<std::size_t>(samplesPerFrame);
        constexpr std::size_t bufferedFrames = 4U;
        if (samplesPerFrame_ > std::numeric_limits<std::size_t>::max()
                / bufferedFrames) {
            throw HamDrmSourceError(HamDrmStatus::failure(
                HamDrmErrorCode::LimitExceeded,
                "HAMDRM TX ring-buffer size overflow"));
        }
        ring_.resize(samplesPerFrame_ * bufferedFrames);
        rebuild();
    }

    ~HamDrmPcm16Source() override
    {
        stopWorker();
    }

    std::uint32_t sampleRate() const noexcept override
    {
        return config_.pcmSampleRateHz;
    }

    std::uint64_t totalSamples() const noexcept override
    {
        return totalSamples_;
    }

    std::uint64_t producedSamples() const noexcept override
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return producedSamples_;
    }

    bool complete() const noexcept override
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return generationFinished_ && !terminalStatus_.has_value()
            && bufferedSamples_ == 0U
            && producedSamples_ == totalSamples_;
    }

    bool cancelled() const noexcept override
    {
        return cancelled_.load(std::memory_order_acquire);
    }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        if (capacity != 0U && output == nullptr) {
            throw std::invalid_argument("null HAMDRM TX PCM output");
        }
        if (capacity == 0U || cancelled()) {
            return 0U;
        }

        // This method is called by SoundOutput's QIODevice pull path.  The
        // constructor/reset path prebuffers the worker ring; once playback has
        // started an empty ring is an underrun and must fail closed through
        // SstvTxAudioDevice instead of waiting on the audio callback.
        std::unique_lock<std::mutex> lock(mutex_);
        if (terminalStatus_.has_value()) {
            HamDrmStatus status = *terminalStatus_;
            lock.unlock();
            throw HamDrmSourceError(std::move(status));
        }
        if (cancelled() || bufferedSamples_ == 0U) {
            return 0U;
        }
        const std::size_t count = std::min(capacity, bufferedSamples_);
        const std::size_t first = std::min(count,
                                           ring_.size() - readOffset_);
        std::copy_n(ring_.data() + readOffset_, first, output);
        if (first != count) {
            std::copy_n(ring_.data(), count - first, output + first);
        }
        readOffset_ = (readOffset_ + count) % ring_.size();
        bufferedSamples_ -= count;
        producedSamples_ += static_cast<std::uint64_t>(count);
        lock.unlock();
        spaceAvailable_.notify_one();
        return count;
    }

    void cancel() noexcept override
    {
        cancelled_.store(true, std::memory_order_release);
        dataAvailable_.notify_all();
        spaceAvailable_.notify_all();
    }

    void reset() override
    {
        rebuild();
    }

private:
    std::unique_ptr<HamDrmWaveformTransmitter> makeTransmitter() const
    {
        auto transmitter = std::make_unique<HamDrmWaveformTransmitter>(
            config_, limits_);
        for (const auto& group : groups_) {
            const HamDrmStatus status = transmitter->enqueueMotDataGroup(group);
            if (!status.ok()) {
                throw HamDrmSourceError(status);
            }
        }
        const HamDrmStatus finishStatus = transmitter->finish();
        if (!finishStatus.ok()) {
            throw HamDrmSourceError(finishStatus);
        }
        return transmitter;
    }

    void stopWorker() noexcept
    {
        cancel();
        if (worker_.joinable()
            && worker_.get_id() != std::this_thread::get_id()) {
            worker_.join();
        }
    }

    void rebuild()
    {
        stopWorker();
        auto transmitter = makeTransmitter();
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            producedSamples_ = 0U;
            generatedSamples_ = 0U;
            bufferedSamples_ = 0U;
            readOffset_ = 0U;
            writeOffset_ = 0U;
            generationFinished_ = false;
            terminalStatus_.reset();
        }
        cancelled_.store(false, std::memory_order_release);
        try {
            worker_ = std::thread(
                [this, transmitter = std::move(transmitter)]() mutable {
                    workerMain(std::move(transmitter));
                });
        } catch (...) {
            cancelled_.store(true, std::memory_order_release);
            throw;
        }

        const std::uint64_t target64 = std::min<std::uint64_t>(
            totalSamples_, static_cast<std::uint64_t>(samplesPerFrame_) * 2U);
        const std::size_t target = static_cast<std::size_t>(target64);
        std::unique_lock<std::mutex> lock(mutex_);
        const bool ready = dataAvailable_.wait_for(
            lock, std::chrono::seconds(5), [this, target]() {
                return bufferedSamples_ >= target || generationFinished_
                    || terminalStatus_.has_value();
            });
        if (ready && !terminalStatus_.has_value()
            && (bufferedSamples_ >= target || generationFinished_)) {
            return;
        }
        HamDrmStatus status = terminalStatus_.value_or(
            HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM TX prebuffer did not become ready"));
        lock.unlock();
        stopWorker();
        throw HamDrmSourceError(std::move(status));
    }

    void failWorker(HamDrmStatus status) noexcept
    {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            terminalStatus_ = std::move(status);
            generationFinished_ = true;
        }
        dataAvailable_.notify_all();
        spaceAvailable_.notify_all();
    }

    void workerMain(
        std::unique_ptr<HamDrmWaveformTransmitter> transmitter) noexcept
    {
        try {
            std::vector<std::int16_t> converted(samplesPerFrame_);
            constexpr double scale = static_cast<double>(
                std::numeric_limits<std::int16_t>::max());
            while (!cancelled()) {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    spaceAvailable_.wait(lock, [this]() {
                        return cancelled()
                            || ring_.size() - bufferedSamples_
                                >= samplesPerFrame_;
                    });
                }
                if (cancelled()) {
                    return;
                }

                const std::vector<double> pcm = transmitter->pullPcm(
                    samplesPerFrame_);
                if (pcm.empty()) {
                    if (!transmitter->done()) {
                        HamDrmStatus status = transmitter->lastStatus();
                        if (status.ok()) {
                            status = HamDrmStatus::failure(
                                HamDrmErrorCode::IoFailure,
                                "HAMDRM TX waveform producer stalled");
                        }
                        failWorker(std::move(status));
                        return;
                    }
                    const std::lock_guard<std::mutex> lock(mutex_);
                    generationFinished_ = true;
                    if (generatedSamples_ != totalSamples_) {
                        terminalStatus_ = HamDrmStatus::failure(
                            HamDrmErrorCode::InconsistentObject,
                            "HAMDRM TX waveform length differed from its plan");
                    }
                    dataAvailable_.notify_all();
                    return;
                }
                if (pcm.size() != samplesPerFrame_) {
                    failWorker(HamDrmStatus::failure(
                        HamDrmErrorCode::InconsistentObject,
                        "HAMDRM TX producer emitted a partial frame"));
                    return;
                }
                for (std::size_t index = 0U; index < pcm.size(); ++index) {
                    const double bounded = std::clamp(pcm[index], -1.0, 1.0);
                    converted[index] = static_cast<std::int16_t>(
                        std::lround(bounded * scale));
                }

                {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    if (cancelled()) {
                        return;
                    }
                    const std::size_t first = std::min(
                        converted.size(), ring_.size() - writeOffset_);
                    std::copy_n(converted.data(), first,
                                ring_.data() + writeOffset_);
                    if (first != converted.size()) {
                        std::copy_n(converted.data() + first,
                                    converted.size() - first, ring_.data());
                    }
                    writeOffset_ = (writeOffset_ + converted.size())
                        % ring_.size();
                    bufferedSamples_ += converted.size();
                    generatedSamples_ += static_cast<std::uint64_t>(
                        converted.size());
                    if (generatedSamples_ > totalSamples_) {
                        terminalStatus_ = HamDrmStatus::failure(
                            HamDrmErrorCode::InconsistentObject,
                            "HAMDRM TX producer exceeded its sample plan");
                        generationFinished_ = true;
                    }
                }
                dataAvailable_.notify_all();
                if (transmitter->done()) {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    generationFinished_ = true;
                    if (generatedSamples_ != totalSamples_) {
                        terminalStatus_ = HamDrmStatus::failure(
                            HamDrmErrorCode::InconsistentObject,
                            "HAMDRM TX waveform length differed from its plan");
                    }
                    dataAvailable_.notify_all();
                    return;
                }
            }
        } catch (const HamDrmSourceError& error) {
            failWorker(error.status);
        } catch (const std::exception& error) {
            failWorker(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure, error.what()));
        } catch (...) {
            failWorker(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM TX waveform worker failed"));
        }
    }

    HamDrmWaveformConfig config_;
    HamDrmWaveformLimits limits_;
    std::vector<std::vector<std::uint8_t>> groups_;
    mutable std::mutex mutex_;
    std::condition_variable dataAvailable_;
    std::condition_variable spaceAvailable_;
    std::thread worker_;
    std::vector<std::int16_t> ring_;
    std::optional<HamDrmStatus> terminalStatus_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    std::uint64_t generatedSamples_ {0U};
    std::size_t samplesPerFrame_ {0U};
    std::size_t bufferedSamples_ {0U};
    std::size_t readOffset_ {0U};
    std::size_t writeOffset_ {0U};
    std::atomic_bool cancelled_ {false};
    bool generationFinished_ {false};
};

bool terminalRxStatus(const HamDrmStatus& status) noexcept
{
    switch (status.code) {
    case HamDrmErrorCode::InvalidArgument:
    case HamDrmErrorCode::LimitExceeded:
    case HamDrmErrorCode::IoFailure:
        return true;
    case HamDrmErrorCode::None:
    case HamDrmErrorCode::UnsupportedProfile:
    case HamDrmErrorCode::UnsupportedFeature:
    case HamDrmErrorCode::UnsupportedContent:
    case HamDrmErrorCode::UnsafeFilename:
    case HamDrmErrorCode::Truncated:
    case HamDrmErrorCode::Malformed:
    case HamDrmErrorCode::CrcMismatch:
    case HamDrmErrorCode::TransportMismatch:
    case HamDrmErrorCode::ConflictingDuplicate:
    case HamDrmErrorCode::InconsistentObject:
    case HamDrmErrorCode::Incomplete:
        return false;
    }
    return true;
}

} // namespace

class HamDrmNativeRxBackend::Implementation final
{
public:
    struct Chunk final
    {
        std::vector<std::int16_t> samples;
        std::uint32_t sampleRate {0U};
        std::uint64_t generation {0U};
    };

    Implementation(HamDrmNativeRxHooks configuredHooks,
                   HamDrmNativeAdapterLimits configuredLimits)
        : hooks(std::move(configuredHooks))
        , limits(configuredLimits)
    {
        if (!adapterLimitsValid(limits)) {
            throw std::invalid_argument("invalid HAMDRM native adapter limits");
        }
    }

    ~Implementation()
    {
        shutdown();
    }

    HamDrmStatus start(const HamDrmProfile& profile,
                       std::uint64_t requestedSession,
                       HamDrmWaveformRxSink& requestedSink)
    {
        if (requestedSession == 0U) {
            return HamDrmStatus::failure(
                HamDrmErrorCode::InvalidArgument,
                "HAMDRM RX session ID must be non-zero");
        }
        const HamDrmStatus profileStatus = HamDrmProfileRegistry::validate(
            profile);
        if (!profileStatus.ok()) {
            return profileStatus;
        }
        if (!hooks.activateSharedAudioTap
            || !hooks.deactivateSharedAudioTap) {
            return HamDrmStatus::failure(
                HamDrmErrorCode::UnsupportedFeature,
                "HAMDRM RX shared-audio hooks are not connected");
        }

        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (activeFlag) {
                return HamDrmStatus::failure(
                    HamDrmErrorCode::Incomplete,
                    "another HAMDRM RX session is active");
            }
        }
        joinWorker();
        const HamDrmWaveformConfig requestedConfig = waveformConfig(
            profile, kHamDrmPcmSampleRateHz);
        HamDrmWaveformLimits requestedWaveformLimits;
        requestedWaveformLimits.maximumFrames = limits.maximumRxSessionFrames;
        try {
            static_cast<void>(HamDrmWaveformReceiver(
                requestedConfig, requestedWaveformLimits));
        } catch (const std::exception& exception) {
            return HamDrmStatus::failure(HamDrmErrorCode::UnsupportedProfile,
                                         exception.what());
        }

        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (activeFlag || worker.joinable()) {
                return HamDrmStatus::failure(
                    HamDrmErrorCode::Incomplete,
                    "another HAMDRM RX session is active");
            }
            config = requestedConfig;
            waveformLimits = requestedWaveformLimits;
            sessionId = requestedSession;
            sink = &requestedSink;
            queue.clear();
            queuedSamples = 0U;
            fatalStatus.reset();
            stopRequested = false;
            audioActivated = true;
            activeFlag = true;
            ++generation;
        }

        try {
            worker = std::thread([this]() { workerMain(); });
        } catch (const std::exception& exception) {
            clearFailedStart();
            return HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                         exception.what());
        }

        HamDrmStatus activation;
        try {
            activation = hooks.activateSharedAudioTap();
        } catch (const std::exception& exception) {
            activation = HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                                exception.what());
        } catch (...) {
            activation = HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM RX audio activation threw an unknown exception");
        }
        if (!activation.ok()) {
            clearFailedStart();
            return activation;
        }
        return HamDrmStatus::success();
    }

    bool submit(const std::int16_t* samples,
                std::size_t sampleCount,
                std::uint32_t sampleRate) noexcept
    {
        if (samples == nullptr || sampleCount == 0U
            || sampleCount > limits.maximumRxChunkSamples
            || sampleCount > limits.maximumRxQueuedSamples
            || !SstvResampler::isSupportedInputRate(sampleRate)) {
            rejectChunk(HamDrmStatus::failure(
                HamDrmErrorCode::InvalidArgument,
                "HAMDRM RX received an unsupported PCM block"));
            return false;
        }
        try {
            std::vector<std::int16_t> copy(samples, samples + sampleCount);
            {
                const std::lock_guard<std::mutex> lock(mutex);
                if (!activeFlag || stopRequested
                    || queue.size() >= limits.maximumRxQueuedChunks
                    || sampleCount > limits.maximumRxQueuedSamples
                    || queuedSamples
                        > limits.maximumRxQueuedSamples - sampleCount) {
                    saturatingAdd(stats.rejectedChunks);
                    if (activeFlag && !stopRequested) {
                        fatalStatus = HamDrmStatus::failure(
                            HamDrmErrorCode::LimitExceeded,
                            "HAMDRM RX audio queue capacity was exceeded");
                        stopRequested = true;
                        activeFlag = false;
                        queue.clear();
                        queuedSamples = 0U;
                        changed.notify_all();
                    }
                    return false;
                }
                queue.push_back({std::move(copy), sampleRate, generation});
                queuedSamples += sampleCount;
                saturatingAdd(stats.acceptedChunks);
                saturatingAdd(stats.acceptedSamples,
                              static_cast<std::uint64_t>(sampleCount));
            }
            changed.notify_one();
            return true;
        } catch (...) {
            rejectChunk(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM RX could not allocate a bounded PCM chunk"));
            return false;
        }
    }

    void resetStream() noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (!activeFlag || stopRequested) {
            return;
        }
        ++generation;
        queue.clear();
        queuedSamples = 0U;
        saturatingAdd(stats.streamResets);
        changed.notify_all();
    }

    void fail(HamDrmStatus status) noexcept
    {
        if (status.ok()) {
            status = HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM RX shared audio became unavailable");
        }
        rejectChunk(std::move(status));
    }

    void cancel(std::uint64_t requestedSession) noexcept
    {
        bool deactivate = false;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (requestedSession == 0U || requestedSession != sessionId) {
                return;
            }
            sink = nullptr;
            sessionId = 0U;
            activeFlag = false;
            stopRequested = true;
            fatalStatus.reset();
            queue.clear();
            queuedSamples = 0U;
            deactivate = std::exchange(audioActivated, false);
            changed.notify_all();
        }
        joinWorker();
        if (deactivate) {
            deactivateAudio();
        }
    }

    void shutdown() noexcept
    {
        std::uint64_t current = 0U;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            current = sessionId;
            if (current == 0U && worker.joinable()) {
                stopRequested = true;
                changed.notify_all();
            }
        }
        if (current != 0U) {
            cancel(current);
        } else {
            joinWorker();
        }
    }

    bool active() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return activeFlag;
    }

    HamDrmNativeRxStatistics statistics() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return stats;
    }

    bool connected() const noexcept
    {
        return static_cast<bool>(hooks.activateSharedAudioTap)
            && static_cast<bool>(hooks.deactivateSharedAudioTap);
    }

private:
    void clearFailedStart() noexcept
    {
        bool deactivate = false;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            sink = nullptr;
            sessionId = 0U;
            activeFlag = false;
            stopRequested = true;
            fatalStatus.reset();
            queue.clear();
            queuedSamples = 0U;
            deactivate = std::exchange(audioActivated, false);
            changed.notify_all();
        }
        joinWorker();
        if (deactivate) {
            deactivateAudio();
        }
    }

    void rejectChunk(HamDrmStatus status) noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex);
        saturatingAdd(stats.rejectedChunks);
        if (activeFlag && !stopRequested) {
            fatalStatus = std::move(status);
            stopRequested = true;
            activeFlag = false;
            queue.clear();
            queuedSamples = 0U;
            changed.notify_all();
        }
    }

    void joinWorker() noexcept
    {
        if (worker.joinable()
            && worker.get_id() != std::this_thread::get_id()) {
            worker.join();
        }
    }

    void deactivateAudio() noexcept
    {
        try {
            if (hooks.deactivateSharedAudioTap) {
                hooks.deactivateSharedAudioTap();
            }
        } catch (...) {
        }
    }

    void finishFromWorker(const HamDrmStatus& status) noexcept
    {
        HamDrmWaveformRxSink* callbackSink = nullptr;
        std::uint64_t callbackSession = 0U;
        bool deactivate = false;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            callbackSink = sink;
            callbackSession = sessionId;
            sink = nullptr;
            activeFlag = false;
            stopRequested = true;
            queue.clear();
            queuedSamples = 0U;
            deactivate = std::exchange(audioActivated, false);
        }
        if (callbackSink && callbackSession != 0U) {
            try {
                callbackSink->hamDrmRxFinished(callbackSession, status);
            } catch (...) {
            }
        }
        if (deactivate) {
            deactivateAudio();
        }
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (sessionId == callbackSession) {
                sessionId = 0U;
            }
        }
    }

    bool deliverBatch(const HamDrmWaveformReceiveBatch& batch,
                      std::uint64_t expectedGeneration)
    {
        HamDrmWaveformRxSink* callbackSink = nullptr;
        std::uint64_t callbackSession = 0U;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (!activeFlag || stopRequested
                || generation != expectedGeneration || !sink) {
                return false;
            }
            callbackSink = sink;
            callbackSession = sessionId;
        }
        try {
            callbackSink->hamDrmRxProgress(
                callbackSession, batch.synchronized ? 0.5 : 0.0);
            for (const auto& group : batch.dataGroups) {
                const QByteArray encoded(
                    reinterpret_cast<const char*>(group.data()),
                    static_cast<qsizetype>(group.size()));
                callbackSink->hamDrmRxMotGroup(callbackSession, encoded);
                const std::lock_guard<std::mutex> lock(mutex);
                saturatingAdd(stats.deliveredGroups);
            }
        } catch (...) {
            finishFromWorker(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM RX sink callback failed"));
            return false;
        }
        return true;
    }

    void workerMain() noexcept
    {
        try {
            HamDrmWaveformReceiver receiver(config, waveformLimits);
            std::unique_ptr<SstvResampler> resampler;
            std::uint32_t inputRate = 0U;
            std::uint64_t workerGeneration = 0U;
            for (;;) {
                Chunk chunk;
                std::optional<HamDrmStatus> fatal;
                bool stop = false;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    changed.wait(lock, [this, workerGeneration]() {
                        return stopRequested || !queue.empty()
                            || generation != workerGeneration;
                    });
                    if (stopRequested) {
                        stop = true;
                        fatal = fatalStatus;
                    } else if (generation != workerGeneration) {
                        workerGeneration = generation;
                        receiver.reset();
                        resampler.reset();
                        inputRate = 0U;
                        continue;
                    } else {
                        chunk = std::move(queue.front());
                        queue.pop_front();
                        queuedSamples -= chunk.samples.size();
                    }
                }
                if (fatal.has_value()) {
                    finishFromWorker(*fatal);
                    return;
                }
                if (stop) {
                    return;
                }

                std::vector<double> pcm;
                if (chunk.sampleRate == kHamDrmPcmSampleRateHz) {
                    if (inputRate != chunk.sampleRate) {
                        receiver.reset();
                        resampler.reset();
                        inputRate = chunk.sampleRate;
                    }
                    pcm.resize(chunk.samples.size());
                    constexpr double divisor = 32'768.0;
                    for (std::size_t index = 0U;
                         index < chunk.samples.size(); ++index) {
                        pcm[index] = static_cast<double>(chunk.samples[index])
                            / divisor;
                    }
                } else {
                    if (!resampler || inputRate != chunk.sampleRate) {
                        receiver.reset();
                        resampler = std::make_unique<SstvResampler>(
                            chunk.sampleRate);
                        inputRate = chunk.sampleRate;
                    }
                    std::vector<float> normalized(chunk.samples.size());
                    constexpr float divisor = 32'768.0F;
                    for (std::size_t index = 0U;
                         index < chunk.samples.size(); ++index) {
                        normalized[index] = static_cast<float>(
                            chunk.samples[index]) / divisor;
                    }
                    const std::vector<float> converted = resampler->process(
                        normalized);
                    pcm.assign(converted.begin(), converted.end());
                }
                if (pcm.empty()) {
                    continue;
                }
                const HamDrmWaveformReceiveBatch batch = receiver.pushPcm(
                    pcm.data(), pcm.size(), false);
                if (!deliverBatch(batch, chunk.generation)) {
                    return;
                }
                if (!batch.status.ok() && terminalRxStatus(batch.status)) {
                    finishFromWorker(batch.status);
                    return;
                }
            }
        } catch (const std::exception& exception) {
            finishFromWorker(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure, exception.what()));
        } catch (...) {
            finishFromWorker(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM RX worker failed with an unknown exception"));
        }
    }

    HamDrmNativeRxHooks hooks;
    HamDrmNativeAdapterLimits limits;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::deque<Chunk> queue;
    std::thread worker;
    HamDrmWaveformConfig config;
    HamDrmWaveformLimits waveformLimits;
    HamDrmWaveformRxSink* sink {nullptr};
    std::optional<HamDrmStatus> fatalStatus;
    std::size_t queuedSamples {0U};
    std::uint64_t generation {0U};
    std::uint64_t sessionId {0U};
    HamDrmNativeRxStatistics stats;
    bool stopRequested {false};
    bool audioActivated {false};
    bool activeFlag {false};
};

HamDrmNativeRxBackend::HamDrmNativeRxBackend(
    HamDrmNativeRxHooks hooks,
    HamDrmNativeAdapterLimits limits)
    : implementation_(std::make_unique<Implementation>(
          std::move(hooks), limits))
{
}

HamDrmNativeRxBackend::~HamDrmNativeRxBackend() = default;

HamDrmWaveformCapability HamDrmNativeRxBackend::capability() const
{
    return nativeCapability(implementation_->connected(), "RX");
}

HamDrmStatus HamDrmNativeRxBackend::start(
    const HamDrmProfile& profile,
    std::uint64_t sessionId,
    HamDrmWaveformRxSink& sink)
{
    return implementation_->start(profile, sessionId, sink);
}

void HamDrmNativeRxBackend::cancel(std::uint64_t sessionId) noexcept
{
    implementation_->cancel(sessionId);
}

bool HamDrmNativeRxBackend::submitPcm16(
    const std::int16_t* samples,
    std::size_t sampleCount,
    std::uint32_t sampleRate) noexcept
{
    return implementation_->submit(samples, sampleCount, sampleRate);
}

void HamDrmNativeRxBackend::resetAudioStream() noexcept
{
    implementation_->resetStream();
}

void HamDrmNativeRxBackend::failAudio(HamDrmStatus status) noexcept
{
    implementation_->fail(std::move(status));
}

void HamDrmNativeRxBackend::shutdown() noexcept
{
    implementation_->shutdown();
}

bool HamDrmNativeRxBackend::active() const noexcept
{
    return implementation_->active();
}

HamDrmNativeRxStatistics HamDrmNativeRxBackend::statistics() const noexcept
{
    return implementation_->statistics();
}

class HamDrmNativeTxBackend::Implementation final
{
public:
    Implementation(HamDrmNativeTxHooks configuredHooks,
                   HamDrmNativeAdapterLimits configuredLimits)
        : hooks(std::move(configuredHooks))
        , limits(configuredLimits)
    {
        if (!adapterLimitsValid(limits)) {
            throw std::invalid_argument("invalid HAMDRM native adapter limits");
        }
    }

    HamDrmWaveformCapability capability() const
    {
        return nativeCapability(
            static_cast<bool>(hooks.configuredCallsign)
                && static_cast<bool>(hooks.startPreparedAudio)
                && static_cast<bool>(hooks.cancelAudio),
            "TX");
    }

    HamDrmStatus start(const HamDrmProfile& profile,
                       HamDrmEncodedObject object,
                       std::uint64_t requestedSession,
                       HamDrmWaveformTxSink& requestedSink)
    {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            saturatingAdd(stats.startCalls);
            if (activeFlag || starting) {
                saturatingAdd(stats.rejectedJobs);
                return HamDrmStatus::failure(
                    HamDrmErrorCode::Incomplete,
                    "another HAMDRM TX session is active");
            }
        }
        if (requestedSession == 0U) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::InvalidArgument,
                "HAMDRM TX session ID must be non-zero"));
        }
        const HamDrmStatus profileStatus = HamDrmProfileRegistry::validate(
            profile);
        if (!profileStatus.ok()) {
            return reject(profileStatus);
        }
        if (!hooks.configuredCallsign || !hooks.startPreparedAudio
            || !hooks.cancelAudio) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::UnsupportedFeature,
                "HAMDRM TX coordinator hooks are not connected"));
        }
        std::string callsign;
        try {
            callsign = hooks.configuredCallsign();
        } catch (const std::exception& exception) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure, exception.what()));
        } catch (...) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                "HAMDRM TX callsign hook failed"));
        }
        if (!validConfiguredCallsign(callsign)) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::InvalidArgument,
                "Decodium station callsign cannot be represented in HAMDRM FAC"));
        }
        const std::size_t groupCount = object.headerGroups.size()
            + object.bodyGroups.size();
        std::size_t groupBytes = 0U;
        for (const auto& group : object.headerGroups) {
            if (group.size() > limits.maximumTxQueuedBytes - std::min(
                    limits.maximumTxQueuedBytes, groupBytes)) {
                return reject(HamDrmStatus::failure(
                    HamDrmErrorCode::LimitExceeded,
                    "HAMDRM TX object exceeds the adapter byte limit"));
            }
            groupBytes += group.size();
        }
        for (const auto& group : object.bodyGroups) {
            if (group.size() > limits.maximumTxQueuedBytes - std::min(
                    limits.maximumTxQueuedBytes, groupBytes)) {
                return reject(HamDrmStatus::failure(
                    HamDrmErrorCode::LimitExceeded,
                    "HAMDRM TX object exceeds the adapter byte limit"));
            }
            groupBytes += group.size();
        }
        if (groupCount == 0U || groupCount > limits.maximumTxGroups
            || groupBytes > limits.maximumTxQueuedBytes) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::LimitExceeded,
                "HAMDRM TX object exceeds the adapter group limit"));
        }

        std::unique_ptr<SstvPcm16Source> source;
        try {
            HamDrmWaveformLimits waveformLimits;
            waveformLimits.maximumQueuedGroups = limits.maximumTxGroups;
            waveformLimits.maximumQueuedDataBytes =
                limits.maximumTxQueuedBytes;
            waveformLimits.maximumFrames = limits.maximumTxFrames;
            source = std::make_unique<HamDrmPcm16Source>(
                waveformConfig(profile, kTxSampleRate, std::move(callsign)),
                waveformLimits,
                orderedGroups(std::move(object)));
        } catch (const HamDrmSourceError& error) {
            return reject(error.status);
        } catch (const std::exception& exception) {
            return reject(HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure, exception.what()));
        }

        {
            const std::lock_guard<std::mutex> lock(mutex);
            starting = true;
            activeFlag = false;
            controllerSessionId = requestedSession;
            coordinatorSessionId = 0U;
            sink = &requestedSink;
        }
        SstvTxCoordinatorResult result;
        try {
            result = hooks.startPreparedAudio(
                std::move(source), "HAMDRM " + profile.id);
        } catch (const std::exception& exception) {
            result.accepted = false;
            result.detail = exception.what();
        } catch (...) {
            result.accepted = false;
            result.detail =
                "HAMDRM TX coordinator hook threw an unknown exception";
        }
        if (!result.accepted || result.sessionId == 0U) {
            const HamDrmStatus status = HamDrmStatus::failure(
                HamDrmErrorCode::IoFailure,
                result.detail.empty()
                    ? "HAMDRM TX coordinator rejected the prepared source"
                    : result.detail);
            {
                const std::lock_guard<std::mutex> lock(mutex);
                starting = false;
                activeFlag = false;
                controllerSessionId = 0U;
                coordinatorSessionId = 0U;
                sink = nullptr;
                saturatingAdd(stats.rejectedJobs);
            }
            return status;
        }
        {
            const std::lock_guard<std::mutex> lock(mutex);
            starting = false;
            activeFlag = true;
            coordinatorSessionId = result.sessionId;
            saturatingAdd(stats.acceptedJobs);
        }
        return HamDrmStatus::success();
    }

    void cancel(std::uint64_t requestedSession) noexcept
    {
        std::uint64_t coordinator = 0U;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (requestedSession == 0U
                || requestedSession != controllerSessionId) {
                return;
            }
            coordinator = coordinatorSessionId;
            sink = nullptr;
            controllerSessionId = 0U;
            coordinatorSessionId = 0U;
            starting = false;
            activeFlag = false;
        }
        try {
            if (coordinator != 0U && hooks.cancelAudio) {
                static_cast<void>(hooks.cancelAudio(coordinator));
            }
        } catch (...) {
        }
    }

    void stateChanged(const SstvTxCoordinatorSnapshot& snapshot) noexcept
    {
        HamDrmWaveformTxSink* callbackSink = nullptr;
        std::uint64_t callbackSession = 0U;
        bool terminal = false;
        HamDrmStatus terminalStatus = HamDrmStatus::success();
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (!activeFlag || !sink || coordinatorSessionId == 0U) {
                return;
            }
            if (snapshot.stateMachine.currentSessionId
                != coordinatorSessionId) {
                saturatingAdd(stats.staleSnapshots);
                return;
            }
            callbackSink = sink;
            callbackSession = controllerSessionId;
            switch (snapshot.stateMachine.state) {
            case SstvTxState::Completed:
                terminal = true;
                saturatingAdd(stats.completedJobs);
                break;
            case SstvTxState::Cancelled:
                terminal = true;
                terminalStatus = HamDrmStatus::failure(
                    HamDrmErrorCode::Incomplete,
                    "HAMDRM TX was cancelled by the shared coordinator");
                saturatingAdd(stats.failedJobs);
                break;
            case SstvTxState::Error:
            case SstvTxState::Disabled:
                terminal = true;
                terminalStatus = HamDrmStatus::failure(
                    HamDrmErrorCode::IoFailure,
                    snapshot.lastOperationDetail.empty()
                        ? "HAMDRM TX failed in the shared coordinator"
                        : snapshot.lastOperationDetail);
                saturatingAdd(stats.failedJobs);
                break;
            case SstvTxState::Idle:
            case SstvTxState::PreparingImage:
            case SstvTxState::Encoding:
            case SstvTxState::Ready:
            case SstvTxState::RequestingPtt:
            case SstvTxState::WaitingForPtt:
            case SstvTxState::TransmittingHeader:
            case SstvTxState::TransmittingImage:
            case SstvTxState::TransmittingFskId:
            case SstvTxState::TailDelay:
            case SstvTxState::ReleasingPtt:
                break;
            }
            if (terminal) {
                sink = nullptr;
                controllerSessionId = 0U;
                coordinatorSessionId = 0U;
                activeFlag = false;
                starting = false;
            }
        }
        if (!callbackSink || callbackSession == 0U) {
            return;
        }
        try {
            if (terminal) {
                callbackSink->hamDrmTxFinished(callbackSession,
                                                terminalStatus);
            } else {
                callbackSink->hamDrmTxProgress(
                    callbackSession,
                    std::clamp(snapshot.progress, 0.0, 1.0));
            }
        } catch (...) {
        }
    }

    void shutdown() noexcept
    {
        std::uint64_t current = 0U;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            current = controllerSessionId;
        }
        if (current != 0U) {
            cancel(current);
        }
    }

    bool active() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return activeFlag || starting;
    }

    HamDrmNativeTxStatistics statistics() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return stats;
    }

private:
    HamDrmStatus reject(HamDrmStatus status)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        saturatingAdd(stats.rejectedJobs);
        return status;
    }

    HamDrmNativeTxHooks hooks;
    HamDrmNativeAdapterLimits limits;
    mutable std::mutex mutex;
    HamDrmWaveformTxSink* sink {nullptr};
    std::uint64_t controllerSessionId {0U};
    std::uint64_t coordinatorSessionId {0U};
    HamDrmNativeTxStatistics stats;
    bool starting {false};
    bool activeFlag {false};
};

HamDrmNativeTxBackend::HamDrmNativeTxBackend(
    HamDrmNativeTxHooks hooks,
    HamDrmNativeAdapterLimits limits)
    : implementation_(std::make_unique<Implementation>(
          std::move(hooks), limits))
{
}

HamDrmNativeTxBackend::~HamDrmNativeTxBackend()
{
    shutdown();
}

HamDrmWaveformCapability HamDrmNativeTxBackend::capability() const
{
    return implementation_->capability();
}

HamDrmStatus HamDrmNativeTxBackend::start(
    const HamDrmProfile& profile,
    HamDrmEncodedObject object,
    std::uint64_t sessionId,
    HamDrmWaveformTxSink& sink)
{
    return implementation_->start(profile, std::move(object),
                                  sessionId, sink);
}

void HamDrmNativeTxBackend::cancel(std::uint64_t sessionId) noexcept
{
    implementation_->cancel(sessionId);
}

void HamDrmNativeTxBackend::coordinatorStateChanged(
    const SstvTxCoordinatorSnapshot& snapshot) noexcept
{
    implementation_->stateChanged(snapshot);
}

void HamDrmNativeTxBackend::shutdown() noexcept
{
    implementation_->shutdown();
}

bool HamDrmNativeTxBackend::active() const noexcept
{
    return implementation_->active();
}

HamDrmNativeTxStatistics HamDrmNativeTxBackend::statistics() const noexcept
{
    return implementation_->statistics();
}

} // namespace decodium::sstv::hamdrm::waveform
