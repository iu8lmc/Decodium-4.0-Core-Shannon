// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvFskIdCodec.h"
#include "SstvTxStream.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace decodium::sstv {

struct SstvFskIdTxConfig final
{
    std::uint32_t sampleRate {48'000U};
    SstvFskIdCodec::TextPolicy textPolicy {
        SstvFskIdCodec::TextPolicy::Callsign};
    SstvFskIdCodec::InputHandling inputHandling {
        SstvFskIdCodec::InputHandling::Strict};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

// Pull-oriented FSK-ID waveform encoder.  The bounded logical/tone frame is
// produced by SstvFskIdCodec and rendered through the same phase-continuous
// DDS and fractional timing accumulator used by analogue SSTV TX.  PCM is
// generated directly into caller-owned chunks; no complete waveform is kept.
//
// This class implements the audited waveform lineage recorded by the codec.
// It does not elevate that lineage to independent interoperability evidence.
class SstvFskIdTxStream final
{
public:
    static constexpr double MaximumLevel = 16.0;

    explicit SstvFskIdTxStream(
        std::string_view text,
        SstvFskIdTxConfig config = {});

    SstvFskIdTxStream(const SstvFskIdTxStream&) = delete;
    SstvFskIdTxStream& operator=(const SstvFskIdTxStream&) = delete;

    const SstvFskIdTxConfig& config() const noexcept;
    const SstvFskIdCodec::EncodedFrame& frame() const noexcept;

    std::uint32_t sampleRate() const noexcept;
    Picoseconds totalDuration() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    std::uint64_t remainingSamples() const noexcept;
    double progress() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    std::optional<SstvTxSegmentCursor> currentSegment() const noexcept;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);

    const SstvToneMetrics& metrics() const noexcept;
    void cancel() noexcept;
    void reset();

private:
    static SstvFskIdCodec::EncodedFrame encode(
        std::string_view text,
        const SstvFskIdTxConfig& config);
    static std::vector<SstvToneSegment> makePlan(
        const SstvFskIdCodec::EncodedFrame& frame,
        const SstvFskIdTxConfig& config);
    static void validateConfig(const SstvFskIdTxConfig& config);

    SstvFskIdTxConfig config_;
    SstvFskIdCodec::EncodedFrame frame_;
    SstvTxStream stream_;
};

} // namespace decodium::sstv
