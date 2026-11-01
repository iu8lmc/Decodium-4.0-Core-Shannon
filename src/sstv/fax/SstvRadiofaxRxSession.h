// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SstvRadiofax.h"
#include "SstvRadiofaxPhasing.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include "../image/SstvImageFrame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvRadiofaxRxState : std::uint8_t { Receiving, Complete, Partial, Cancelled };

struct SstvRadiofaxLevels final { double blackHz {1500.0}; double whiteHz {2300.0}; };

// Explicit image anchor/profile: acquisition of an unknown WEFAX signal is
// separate. Only observed pixels are published; missing pixels stay uncovered.
class SstvRadiofaxRxSession final {
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8192U;
    SstvRadiofaxRxSession(SstvFaxConfig config, std::uint64_t imageStartSample,
                          double frequencyOffsetHz = 0.0,
                          std::optional<SstvRadiofaxClockEstimate> clock = std::nullopt,
                          SstvRadiofaxLevels levels = {});
    std::size_t consume(const SstvFrequencyObservation* observations, std::size_t count);
    std::size_t consume(const std::vector<SstvFrequencyObservation>& observations);
    SstvRadiofaxRxState finish();
    SstvRadiofaxRxState notifyDiscontinuity();
    void cancel() noexcept;
    SstvRadiofaxRxState state() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
private:
    void publishLine();
    SstvRadiofaxMapper mapper_;
    SstvImageFrame image_;
    std::uint64_t imageStart_;
    std::uint64_t imageEnd_;
    std::uint64_t lineSamples_;
    std::uint64_t lineCount_;
    double frequencyOffset_;
    SstvRadiofaxLevels levels_;
    std::vector<double> sums_;
    std::vector<double> weights_;
    std::uint32_t line_ {0U};
    std::uint64_t lastSample_ {0U};
    bool hasLine_ {false};
    bool hasLastSample_ {false};
    SstvRadiofaxRxState state_ {SstvRadiofaxRxState::Receiving};
};

} // namespace decodium::sstv
