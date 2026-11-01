// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SstvRadiofaxPhasing.h"
#include "SstvRadiofaxRxSession.h"
#include "SstvRadiofaxStop.h"
#include <deque>
#include <memory>
#include <optional>

namespace decodium::sstv {

enum class SstvRadiofaxAutoRxState : std::uint8_t {
    SearchingPhasing, SearchingImage, Receiving, Complete, Partial, Cancelled, Failed
};

// Experimental in-process pipeline. IOC, output dimensions and clock rate are
// explicitly configured. Image start and stop detection are heuristic: image
// patterns can resemble either phasing or a sustained stop signal.
class SstvRadiofaxAutoRx final {
public:
    explicit SstvRadiofaxAutoRx(SstvFaxConfig config, double frequencyOffsetHz = 0.0);
    void consume(const SstvFrequencyObservation* data, std::size_t count);
    void consume(const std::vector<SstvFrequencyObservation>& data);
    bool stopAtAudioSample(std::uint64_t sample);
    SstvRadiofaxAutoRxState finish();
    SstvRadiofaxAutoRxState notifyDiscontinuity();
    void cancel() noexcept;
    SstvRadiofaxAutoRxState state() const noexcept;
    std::optional<std::uint64_t> imageStartSample() const noexcept;
    std::optional<SstvRadiofaxPhaseLock> phasing() const noexcept;
    std::optional<std::uint64_t> stopSample() const noexcept;
    std::size_t bufferedObservations() const noexcept;
    std::size_t maximumBufferedObservations() const noexcept;
    std::optional<SstvImageSnapshot> snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
private:
    void updateRxState() noexcept;
    void buffer(const SstvFrequencyObservation& observation);
    void beginImage(std::uint64_t sample);
    void consumeImageObservation(const SstvFrequencyObservation& observation);
    void flushPending(std::optional<std::uint64_t> before = std::nullopt);
    std::uint64_t refineStopBoundary(std::uint64_t candidate) const;
    SstvFaxConfig config_;
    double offset_;
    SstvRadiofaxPhasingDetector phase_;
    SstvRadiofaxStopDetector stop_;
    std::optional<SstvRadiofaxPhaseLock> lock_;
    std::unique_ptr<SstvRadiofaxImageStartDetector> start_;
    std::unique_ptr<SstvRadiofaxRxSession> receiver_;
    std::deque<SstvFrequencyObservation> replay_;
    std::deque<SstvFrequencyObservation> pendingImage_;
    std::optional<std::uint64_t> stopSample_;
    std::optional<std::uint64_t> imageStart_;
    std::uint64_t lastSample_ {0U};
    bool hasLast_ {false};
    SstvRadiofaxAutoRxState state_ {SstvRadiofaxAutoRxState::SearchingPhasing};
};

} // namespace decodium::sstv
