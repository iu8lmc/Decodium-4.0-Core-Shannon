// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SstvFax480Receiver.h"
#include "../rx/SstvVisDetector.h"
#include <deque>
#include <memory>
#include <optional>

namespace decodium::sstv {
// Experimental acquisition for an explicitly selected FAX480 convention.
// It requires the start tone and first line sync, with bounded replay.
class SstvFax480AutoRx final {
public:
    SstvFax480AutoRx(SstvFax480Profile profile, std::uint32_t sampleRate);
    void consume(const SstvFrequencyObservation* observations, std::size_t count);
    bool acquired() const noexcept { return bool(receiver_); }
    bool complete() const noexcept { return complete_; }
    std::optional<std::uint64_t> anchor() const noexcept { return anchor_; }
    std::vector<std::uint8_t> image() const;
    std::vector<std::uint8_t> coverage() const;
    std::uint32_t height() const noexcept { return mapper_.height(); }
private:
    void observe(const SstvFrequencyObservation& obs);
    void feed(const SstvFrequencyObservation& obs);
    void observeVis(const SstvFrequencyObservation& obs);
    SstvFax480Profile profile_;
    std::uint32_t rate_;
    SstvFax480Mapper mapper_;
    std::unique_ptr<SstvFax480Receiver> receiver_;
    SstvVisDetector vis_;
    double visFrequency_ {0.0};
    std::uint64_t visRunStart_ {0}, visRunEnd_ {0};
    std::deque<SstvFrequencyObservation> replay_;
    std::optional<std::uint64_t> anchor_;
    std::uint64_t last_ {0}, runStart_ {0}, lastChange_ {0}, syncStart_ {0};
    unsigned transitions_ {0};
    int tone_ {0};
    bool haveLast_ {false}, header_ {false}, inSync_ {false}, complete_ {false};
};
} // namespace decodium::sstv
