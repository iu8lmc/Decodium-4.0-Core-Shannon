// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SstvFax480.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {
// Receives observations with sample coordinates relative to an explicitly
// anchored FAX480 header. Automatic acquisition is a separate stage.
class SstvFax480Receiver final {
public:
    static constexpr std::size_t MaximumObservationsPerConsume=8192U;
    SstvFax480Receiver(SstvFax480Profile profile, std::uint32_t sampleRate);
    void consume(const SstvFrequencyObservation* observations, std::size_t count);
    std::vector<std::uint8_t> image() const;
    std::vector<std::uint8_t> coverage() const;
private:
    SstvFax480Mapper mapper_;
    std::vector<double> sums_, weights_;
    std::uint64_t last_ {0};
    bool haveLast_ {false};
};
} // namespace decodium::sstv
