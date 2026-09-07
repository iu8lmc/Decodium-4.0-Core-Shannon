// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmController.h"
#include "../../integration/SstvTxCoordinator.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace decodium::sstv::hamdrm::waveform {

struct HamDrmNativeAdapterLimits final
{
    std::size_t maximumRxQueuedChunks {128U};
    std::size_t maximumRxQueuedSamples {12U * 12'000U};
    std::size_t maximumRxChunkSamples {1U << 20U};
    std::size_t maximumRxSessionFrames {216'000U};
    std::size_t maximumTxGroups {4'096U};
    std::size_t maximumTxQueuedBytes {8U * 1024U * 1024U};
    std::size_t maximumTxFrames {18'000U};
};

struct HamDrmNativeRxHooks final
{
    // Owner-thread hook which enables the existing Decodium audio tap.  It
    // must never create a second capture path.
    std::function<HamDrmStatus()> activateSharedAudioTap;
    // May be invoked by the RX worker after a terminal failure.  Production
    // adapters must marshal bridge mutations back to the bridge owner thread.
    std::function<void()> deactivateSharedAudioTap;
};

struct HamDrmNativeRxStatistics final
{
    std::uint64_t acceptedChunks {0U};
    std::uint64_t acceptedSamples {0U};
    std::uint64_t rejectedChunks {0U};
    std::uint64_t streamResets {0U};
    std::uint64_t deliveredGroups {0U};
};

// Complete controller-facing RX adapter for the supported native subset.  The
// producer method is the only cross-thread hot path; DSP runs on one bounded
// worker and callbacks use HamDrmController's documented marshalling path.
class HamDrmNativeRxBackend final : public HamDrmWaveformRxBackend
{
public:
    explicit HamDrmNativeRxBackend(
        HamDrmNativeRxHooks hooks,
        HamDrmNativeAdapterLimits limits = {});
    ~HamDrmNativeRxBackend() override;

    HamDrmNativeRxBackend(const HamDrmNativeRxBackend&) = delete;
    HamDrmNativeRxBackend& operator=(const HamDrmNativeRxBackend&) = delete;

    HamDrmWaveformCapability capability() const override;
    HamDrmStatus start(const HamDrmProfile& profile,
                       std::uint64_t sessionId,
                       HamDrmWaveformRxSink& sink) override;
    void cancel(std::uint64_t sessionId) noexcept override;

    bool submitPcm16(const std::int16_t* samples,
                     std::size_t sampleCount,
                     std::uint32_t sampleRate) noexcept;
    void resetAudioStream() noexcept;
    void failAudio(HamDrmStatus status) noexcept;
    void shutdown() noexcept;
    bool active() const noexcept;
    HamDrmNativeRxStatistics statistics() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

struct HamDrmNativeTxHooks final
{
    // Read at job start from Decodium's current station configuration.  The
    // adapter rejects an empty/unrepresentable FAC callsign; it never invents
    // a fallback identity over the shared TX path.
    std::function<std::string()> configuredCallsign;
    // The bridge resolves and pins the selected output, then passes the source
    // to SstvTxCoordinator::startPrepared().
    std::function<SstvTxCoordinatorResult(
        std::unique_ptr<SstvPcm16Source> source,
        std::string mode)> startPreparedAudio;
    std::function<bool(std::uint64_t coordinatorSessionId)> cancelAudio;
};

struct HamDrmNativeTxStatistics final
{
    std::uint64_t startCalls {0U};
    std::uint64_t acceptedJobs {0U};
    std::uint64_t rejectedJobs {0U};
    std::uint64_t completedJobs {0U};
    std::uint64_t failedJobs {0U};
    std::uint64_t staleSnapshots {0U};
};

// MOT groups -> native 48 kHz PCM source -> the existing Decodium TX
// coordinator.  coordinatorStateChanged() is called by the coordinator's
// existing observer, so completion reflects played/drained audio rather than
// samples merely generated into an OS buffer.
class HamDrmNativeTxBackend final : public HamDrmWaveformTxBackend
{
public:
    explicit HamDrmNativeTxBackend(
        HamDrmNativeTxHooks hooks,
        HamDrmNativeAdapterLimits limits = {});
    ~HamDrmNativeTxBackend() override;

    HamDrmNativeTxBackend(const HamDrmNativeTxBackend&) = delete;
    HamDrmNativeTxBackend& operator=(const HamDrmNativeTxBackend&) = delete;

    HamDrmWaveformCapability capability() const override;
    HamDrmStatus start(const HamDrmProfile& profile,
                       HamDrmEncodedObject object,
                       std::uint64_t sessionId,
                       HamDrmWaveformTxSink& sink) override;
    void cancel(std::uint64_t sessionId) noexcept override;

    void coordinatorStateChanged(
        const SstvTxCoordinatorSnapshot& snapshot) noexcept;
    void shutdown() noexcept;
    bool active() const noexcept;
    HamDrmNativeTxStatistics statistics() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace decodium::sstv::hamdrm::waveform
