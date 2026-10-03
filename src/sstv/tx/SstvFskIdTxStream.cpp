// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvFskIdTxStream.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

const char* validationMessage(SstvFskIdCodec::ValidationStatus status) noexcept
{
    using Status = SstvFskIdCodec::ValidationStatus;
    switch (status) {
    case Status::Valid:
        return "valid";
    case Status::Empty:
        return "FSK ID text must not be empty";
    case Status::TooLong:
        return "FSK ID text exceeds the interoperable length limit";
    case Status::InvalidCharacter:
        return "FSK ID text contains a disallowed character";
    }
    return "FSK ID text validation failed";
}

} // namespace

SstvFskIdTxStream::SstvFskIdTxStream(
    std::string_view text,
    SstvFskIdTxConfig config)
    : config_(config)
    , frame_(encode(text, config_))
    , stream_(config_.sampleRate,
              makePlan(frame_, config_),
              config_.headroom)
{
}

const SstvFskIdTxConfig& SstvFskIdTxStream::config() const noexcept
{
    return config_;
}

const SstvFskIdCodec::EncodedFrame& SstvFskIdTxStream::frame() const noexcept
{
    return frame_;
}

std::uint32_t SstvFskIdTxStream::sampleRate() const noexcept
{
    return stream_.sampleRate();
}

Picoseconds SstvFskIdTxStream::totalDuration() const noexcept
{
    return stream_.totalDuration();
}

std::uint64_t SstvFskIdTxStream::totalSamples() const noexcept
{
    return stream_.totalSamples();
}

std::uint64_t SstvFskIdTxStream::producedSamples() const noexcept
{
    return stream_.producedSamples();
}

std::uint64_t SstvFskIdTxStream::remainingSamples() const noexcept
{
    return stream_.remainingSamples();
}

double SstvFskIdTxStream::progress() const noexcept
{
    return stream_.progress();
}

bool SstvFskIdTxStream::complete() const noexcept
{
    return stream_.complete();
}

bool SstvFskIdTxStream::cancelled() const noexcept
{
    return stream_.cancelled();
}

std::optional<SstvTxSegmentCursor>
SstvFskIdTxStream::currentSegment() const noexcept
{
    return stream_.currentSegment();
}

std::size_t SstvFskIdTxStream::pullFloat(float* output,
                                         std::size_t capacity)
{
    return stream_.pullFloat(output, capacity);
}

std::size_t SstvFskIdTxStream::pullPcm16(std::int16_t* output,
                                         std::size_t capacity)
{
    return stream_.pullPcm16(output, capacity);
}

const SstvToneMetrics& SstvFskIdTxStream::metrics() const noexcept
{
    return stream_.metrics();
}

void SstvFskIdTxStream::cancel() noexcept
{
    stream_.cancel();
}

void SstvFskIdTxStream::reset()
{
    stream_.reset();
}

SstvFskIdCodec::EncodedFrame SstvFskIdTxStream::encode(
    std::string_view text,
    const SstvFskIdTxConfig& config)
{
    validateConfig(config);
    auto frame = SstvFskIdCodec::encode(
        text,
        config.textPolicy,
        config.inputHandling);
    if (!frame.valid()) {
        throw std::invalid_argument(validationMessage(frame.validation.status));
    }
    return frame;
}

std::vector<SstvToneSegment> SstvFskIdTxStream::makePlan(
    const SstvFskIdCodec::EncodedFrame& frame,
    const SstvFskIdTxConfig& config)
{
    std::vector<SstvToneSegment> plan;
    plan.reserve(frame.tones.size());
    for (const auto& tone : frame.tones) {
        plan.push_back({
            static_cast<double>(tone.frequencyHz),
            Picoseconds {
                static_cast<std::int64_t>(tone.durationMicroseconds)
                * kPicosecondsPerMicrosecond},
            config.level,
            SstvTxSegmentRole::FskId
        });
    }
    return plan;
}

void SstvFskIdTxStream::validateConfig(
    const SstvFskIdTxConfig& config)
{
    if (!SstvToneGenerator::isSupportedSampleRate(config.sampleRate)) {
        throw std::invalid_argument("unsupported FSK ID sample rate");
    }
    if (!std::isfinite(config.level)
        || config.level < 0.0
        || config.level > MaximumLevel) {
        throw std::invalid_argument("FSK ID level must be finite and in [0, 16]");
    }
    if (!std::isfinite(config.headroom)
        || config.headroom <= 0.0
        || config.headroom > 1.0) {
        throw std::invalid_argument("FSK ID headroom must be finite and in (0, 1]");
    }

    using Policy = SstvFskIdCodec::TextPolicy;
    switch (config.textPolicy) {
    case Policy::Callsign:
    case Policy::PermittedText:
        break;
    default:
        throw std::invalid_argument("unknown FSK ID text policy");
    }

    using Handling = SstvFskIdCodec::InputHandling;
    switch (config.inputHandling) {
    case Handling::Strict:
    case Handling::Sanitize:
        break;
    default:
        throw std::invalid_argument("unknown FSK ID input policy");
    }
}

} // namespace decodium::sstv
