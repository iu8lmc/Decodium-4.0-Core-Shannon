// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTxSources.h"

#include "../tx/SstvToneGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

template<typename Encoder>
class EncoderPcm16Source final : public SstvPcm16Source
{
public:
    template<typename... Arguments>
    explicit EncoderPcm16Source(std::uint32_t sampleRate,
                                Arguments&&... arguments)
        : sampleRate_(sampleRate)
        , encoder_(std::forward<Arguments>(arguments)...)
    {
    }

    std::uint32_t sampleRate() const noexcept override
    {
        return sampleRate_;
    }

    std::uint64_t totalSamples() const noexcept override
    {
        return encoder_.totalSamples();
    }

    std::uint64_t producedSamples() const noexcept override
    {
        return encoder_.producedSamples();
    }

    bool complete() const noexcept override
    {
        return encoder_.complete();
    }

    bool cancelled() const noexcept override
    {
        return encoder_.cancelled();
    }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        return encoder_.pullPcm16(output, capacity);
    }

    void cancel() noexcept override
    {
        encoder_.cancel();
    }

    void reset() override
    {
        encoder_.reset();
    }

private:
    std::uint32_t sampleRate_ {0U};
    Encoder encoder_;
};

class CalibrationTonePcm16Source final : public SstvPcm16Source
{
public:
    CalibrationTonePcm16Source(SstvCalibrationToneKind kind,
                               std::uint32_t sampleRate,
                               std::uint32_t durationMilliseconds,
                               double level,
                               double headroom)
        : sampleRate_(sampleRate)
        , frequencyHz_(calibrationToneSpec(kind).frequencyHz)
        , level_(level)
        , generator_(sampleRate, headroom)
    {
        constexpr std::uint32_t minimumDurationMilliseconds = 250U;
        constexpr std::uint32_t maximumDurationMilliseconds = 5'000U;
        if (durationMilliseconds < minimumDurationMilliseconds
            || durationMilliseconds > maximumDurationMilliseconds) {
            throw std::invalid_argument(
                "SSTV calibration tone duration must be in [250, 5000] ms");
        }
        if (!std::isfinite(level_) || level_ < 0.0 || level_ > 1.0) {
            throw std::invalid_argument(
                "SSTV calibration tone level must be in [0, 1]");
        }
        generator_.validateTone(frequencyHz_, level_);
        const std::uint64_t product = static_cast<std::uint64_t>(sampleRate_)
            * static_cast<std::uint64_t>(durationMilliseconds);
        totalSamples_ = product / 1'000U;
        if (totalSamples_ == 0U) {
            throw std::invalid_argument("SSTV calibration tone is empty");
        }
    }

    std::uint32_t sampleRate() const noexcept override { return sampleRate_; }
    std::uint64_t totalSamples() const noexcept override { return totalSamples_; }
    std::uint64_t producedSamples() const noexcept override { return producedSamples_; }
    bool complete() const noexcept override
    {
        return producedSamples_ >= totalSamples_ || generator_.cancelled();
    }
    bool cancelled() const noexcept override { return generator_.cancelled(); }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        const std::uint64_t remaining = totalSamples_ - producedSamples_;
        const std::size_t requested = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, capacity));
        const std::size_t produced = generator_.generatePcm16(
            frequencyHz_, level_, output, requested);
        producedSamples_ += static_cast<std::uint64_t>(produced);
        return produced;
    }

    void cancel() noexcept override { generator_.cancel(); }
    void reset() override
    {
        producedSamples_ = 0U;
        generator_.reset();
    }

private:
    std::uint32_t sampleRate_ {0U};
    double frequencyHz_ {0.0};
    double level_ {1.0};
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvToneGenerator generator_;
};

constexpr std::array<SstvCalibrationToneSpec, 4U> kCalibrationTones {{
    {SstvCalibrationToneKind::SyncReference,
     "sync-1200", "1200 Hz sync", 1'200.0},
    {SstvCalibrationToneKind::BlackReference,
     "black-1500", "1500 Hz black", 1'500.0},
    {SstvCalibrationToneKind::LeaderReference,
     "leader-1900", "1900 Hz leader", 1'900.0},
    {SstvCalibrationToneKind::WhiteReference,
     "white-2300", "2300 Hz white", 2'300.0},
}};

} // namespace

const SstvCalibrationToneSpec& calibrationToneSpec(
    SstvCalibrationToneKind kind)
{
    const auto match = std::find_if(
        kCalibrationTones.cbegin(),
        kCalibrationTones.cend(),
        [kind](const SstvCalibrationToneSpec& spec) {
            return spec.kind == kind;
        });
    if (match == kCalibrationTones.cend()) {
        throw std::invalid_argument("unknown SSTV calibration tone");
    }
    return *match;
}

std::optional<SstvCalibrationToneKind> calibrationToneKindFromId(
    std::string_view id) noexcept
{
    const auto match = std::find_if(
        kCalibrationTones.cbegin(),
        kCalibrationTones.cend(),
        [id](const SstvCalibrationToneSpec& spec) {
            return id == spec.id;
        });
    return match == kCalibrationTones.cend()
        ? std::nullopt
        : std::optional<SstvCalibrationToneKind> {match->kind};
}

std::unique_ptr<SstvPcm16Source> makeCalibrationTonePcm16Source(
    SstvCalibrationToneKind kind,
    std::uint32_t sampleRate,
    std::uint32_t durationMilliseconds,
    double level,
    double headroom)
{
    return std::make_unique<CalibrationTonePcm16Source>(
        kind, sampleRate, durationMilliseconds, level, headroom);
}

std::unique_ptr<SstvPcm16Source> makeAvtPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvAvtEncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvAvtEncoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makeMartinM1Pcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvMartinM1EncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvMartinM1Encoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makeMmsstvPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvMmsstvEncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvMmsstvEncoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makeScottiePcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvScottieEncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvScottieEncoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makePdPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvPdEncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvPdEncoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makeRobotPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvRobotEncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvRobotEncoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makeSequentialRgbPcm16Source(
    const std::vector<SstvRgbPixel>& pixels,
    SstvSequentialRgbEncoderConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvSequentialRgbEncoder>>(
        config.sampleRate, pixels, config);
}

std::unique_ptr<SstvPcm16Source> makeFskIdPcm16Source(
    std::string_view text,
    SstvFskIdTxConfig config)
{
    return std::make_unique<EncoderPcm16Source<SstvFskIdTxStream>>(
        config.sampleRate, text, config);
}

} // namespace decodium::sstv
