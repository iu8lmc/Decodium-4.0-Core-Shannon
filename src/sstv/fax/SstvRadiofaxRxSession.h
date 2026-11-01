// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SstvRadiofax.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include "../image/SstvImageFrame.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

enum class SstvRadiofaxRxState : std::uint8_t { Receiving, Complete, Partial, Cancelled };

// Explicit image anchor/profile: acquisition of an unknown WEFAX signal is
// separate. Only observed pixels are published; missing pixels stay uncovered.
class SstvRadiofaxRxSession final {
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8192U;
    SstvRadiofaxRxSession(SstvFaxConfig config, std::uint64_t imageStartSample,
                          double frequencyOffsetHz = 0.0);
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
    double frequencyOffset_;
    std::vector<double> sums_;
    std::vector<double> weights_;
    std::uint32_t line_ {0U};
    std::uint64_t lastSample_ {0U};
    bool hasLine_ {false};
    bool hasLastSample_ {false};
    SstvRadiofaxRxState state_ {SstvRadiofaxRxState::Receiving};
};

} // namespace decodium::sstv
