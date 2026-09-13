// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTxCoordinator.h"

#include "SstvTxSources.h"

#include "../core/SstvTimingAccumulator.h"
#include "../diagnostics/SstvDiagnosticLogging.h"
#include "../tx/SstvToneGenerator.h"

#include <QImage>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

bool isCalibrationMode(std::string_view mode) noexcept
{
    constexpr std::string_view prefix {"calibration-"};
    return mode.size() >= prefix.size()
        && mode.substr(0U, prefix.size()) == prefix;
}

void recordTxEvent(const QString& event,
                   QtMsgType severity,
                   std::string_view mode,
                   SstvTxState state,
                   bool success,
                   SstvTxErrorCode error = SstvTxErrorCode::None,
                   std::uint64_t durationMs = 0U,
                   bool includeDuration = false) noexcept
{
    try {
        QVariantMap fields {
            {QStringLiteral("state"),
             QString::fromLatin1(SstvTxStateMachine::stateName(state))},
            {QStringLiteral("success"), success},
        };
        if (!mode.empty()) {
            fields.insert(
                QStringLiteral("modeId"),
                QString::fromLatin1(mode.data(),
                                    static_cast<qsizetype>(mode.size())));
        }
        if (error != SstvTxErrorCode::None) {
            fields.insert(QStringLiteral("errorCode"),
                          static_cast<int>(error));
        }
        if (includeDuration) {
            fields.insert(QStringLiteral("durationMs"),
                          QVariant::fromValue<qulonglong>(durationMs));
        }
        recordSstvDiagnosticEvent(sstvTxLog(), severity, event, fields);
    } catch (...) {
        // Diagnostic collection is best effort and must never change TX.
    }
}

class SequentialPcm16Source final : public SstvPcm16Source
{
public:
    SequentialPcm16Source(std::unique_ptr<SstvPcm16Source> image,
                          std::unique_ptr<SstvPcm16Source> fskId)
        : image_(std::move(image))
        , fskId_(std::move(fskId))
    {
        if (!image_) {
            throw std::invalid_argument(
                "SSTV TX composite image source must not be null");
        }
        sampleRate_ = image_->sampleRate();
        if (fskId_ && fskId_->sampleRate() != sampleRate_) {
            throw std::invalid_argument(
                "SSTV TX composite sources must use one sample rate");
        }
        const std::uint64_t imageSamples = image_->totalSamples();
        const std::uint64_t fskSamples = fskId_
            ? fskId_->totalSamples() : 0U;
        if (fskSamples
            > std::numeric_limits<std::uint64_t>::max() - imageSamples) {
            throw std::overflow_error(
                "SSTV TX composite sample count overflow");
        }
        totalSamples_ = imageSamples + fskSamples;
    }

    std::uint32_t sampleRate() const noexcept override
    {
        return sampleRate_;
    }

    std::uint64_t totalSamples() const noexcept override
    {
        return totalSamples_;
    }

    std::uint64_t producedSamples() const noexcept override
    {
        const std::uint64_t imageSamples = image_->producedSamples();
        const std::uint64_t fskSamples = fskId_
            ? fskId_->producedSamples() : 0U;
        return imageSamples > totalSamples_ - std::min(totalSamples_, fskSamples)
            ? totalSamples_
            : imageSamples + fskSamples;
    }

    bool complete() const noexcept override
    {
        return image_->complete() && (!fskId_ || fskId_->complete());
    }

    bool cancelled() const noexcept override
    {
        return cancelled_.load(std::memory_order_acquire)
            || image_->cancelled()
            || (fskId_ && fskId_->cancelled());
    }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        if (capacity != 0U && output == nullptr) {
            throw std::invalid_argument(
                "SSTV TX composite output must not be null");
        }
        if (capacity == 0U || cancelled()) {
            return 0U;
        }

        std::size_t produced = 0U;
        if (!image_->complete()) {
            produced = image_->pullPcm16(output, capacity);
            if (produced == capacity || !image_->complete()) {
                return produced;
            }
        }
        if (fskId_ && !fskId_->complete()) {
            produced += fskId_->pullPcm16(output + produced,
                                          capacity - produced);
        }
        return produced;
    }

    void cancel() noexcept override
    {
        cancelled_.store(true, std::memory_order_release);
        image_->cancel();
        if (fskId_) {
            fskId_->cancel();
        }
    }

    void reset() override
    {
        image_->reset();
        if (fskId_) {
            fskId_->reset();
        }
        cancelled_.store(false, std::memory_order_release);
    }

private:
    std::unique_ptr<SstvPcm16Source> image_;
    std::unique_ptr<SstvPcm16Source> fskId_;
    std::uint32_t sampleRate_ {0U};
    std::uint64_t totalSamples_ {0U};
    std::atomic_bool cancelled_ {false};
};

std::uint64_t checkedFrameSum(std::uint64_t left,
                              std::uint64_t right)
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        throw std::overflow_error("SSTV TX audio frame count overflow");
    }
    return left + right;
}

std::uint64_t framesForMilliseconds(std::uint32_t sampleRate,
                                    std::uint64_t milliseconds)
{
    constexpr std::uint64_t denominator = 1'000U;
    constexpr std::uint64_t roundUp = denominator - 1U;
    if (sampleRate == 0U
        || milliseconds
            > (std::numeric_limits<std::uint64_t>::max() - roundUp)
                / sampleRate) {
        throw std::overflow_error("SSTV TX millisecond frame count overflow");
    }
    return (milliseconds * sampleRate + roundUp) / denominator;
}

// A live-output-only VOX envelope.  It remains pull-oriented and bounded: no
// complete PCM waveform is materialised, and concurrent cancellation reaches
// both the tone generator and the native protocol source at a sample boundary.
class VoxEnvelopePcm16Source final : public SstvPcm16Source
{
public:
    VoxEnvelopePcm16Source(std::unique_ptr<SstvPcm16Source> payload,
                           std::uint64_t preKeySamples,
                           std::uint64_t hangSamples,
                           double toneFrequencyHz,
                           double toneLevel)
        : payload_(std::move(payload))
        , preKeySamples_(preKeySamples)
        , payloadSamples_(payload_ ? payload_->totalSamples() : 0U)
        , hangSamples_(hangSamples)
        , toneFrequencyHz_(toneFrequencyHz)
        , toneLevel_(toneLevel)
        , tone_(payload_ ? payload_->sampleRate() : 0U)
    {
        if (!payload_) {
            throw std::invalid_argument(
                "SSTV TX VOX payload source must not be null");
        }
        if (preKeySamples_ == 0U || hangSamples_ == 0U
            || payloadSamples_ == 0U) {
            throw std::invalid_argument(
                "SSTV TX VOX envelope phases must not be empty");
        }
        tone_.validateTone(toneFrequencyHz_, toneLevel_);
        totalSamples_ = checkedFrameSum(
            checkedFrameSum(preKeySamples_, payloadSamples_),
            hangSamples_);
    }

    std::uint32_t sampleRate() const noexcept override
    {
        return payload_->sampleRate();
    }

    std::uint64_t totalSamples() const noexcept override
    {
        return totalSamples_;
    }

    std::uint64_t producedSamples() const noexcept override
    {
        return preKeyProduced_ + payload_->producedSamples()
            + hangProduced_;
    }

    bool complete() const noexcept override
    {
        return preKeyProduced_ == preKeySamples_
            && payload_->complete()
            && payload_->producedSamples() == payloadSamples_
            && hangProduced_ == hangSamples_;
    }

    bool cancelled() const noexcept override
    {
        return cancelled_.load(std::memory_order_acquire)
            || tone_.cancelled() || payload_->cancelled();
    }

    std::size_t pullPcm16(std::int16_t* output,
                          std::size_t capacity) override
    {
        if (capacity != 0U && output == nullptr) {
            throw std::invalid_argument(
                "SSTV TX VOX output must not be null");
        }
        if (capacity == 0U || cancelled() || complete()) {
            return 0U;
        }

        std::size_t produced = 0U;
        while (produced < capacity && !cancelled()) {
            if (preKeyProduced_ < preKeySamples_) {
                const std::size_t count = static_cast<std::size_t>(
                    std::min<std::uint64_t>(
                        capacity - produced,
                        preKeySamples_ - preKeyProduced_));
                const std::size_t generated = tone_.generatePcm16(
                    toneFrequencyHz_, toneLevel_, output + produced, count);
                preKeyProduced_ += generated;
                produced += generated;
                if (generated != count) {
                    break;
                }
                continue;
            }

            if (!payload_->complete()) {
                const std::size_t generated = payload_->pullPcm16(
                    output + produced, capacity - produced);
                produced += generated;
                if (payload_->producedSamples() > payloadSamples_
                    || (payload_->complete()
                        && payload_->producedSamples() != payloadSamples_)) {
                    throw std::runtime_error(
                        "SSTV TX VOX payload violated its declared length");
                }
                if (generated == 0U && !payload_->complete()) {
                    break;
                }
                continue;
            }

            if (hangProduced_ < hangSamples_) {
                const std::size_t count = static_cast<std::size_t>(
                    std::min<std::uint64_t>(
                        capacity - produced,
                        hangSamples_ - hangProduced_));
                const std::size_t generated = tone_.generatePcm16(
                    toneFrequencyHz_, toneLevel_, output + produced, count);
                hangProduced_ += generated;
                produced += generated;
                if (generated != count) {
                    break;
                }
                continue;
            }
            break;
        }
        return produced;
    }

    void cancel() noexcept override
    {
        cancelled_.store(true, std::memory_order_release);
        tone_.cancel();
        payload_->cancel();
    }

    void reset() override
    {
        payload_->reset();
        tone_.reset();
        preKeyProduced_ = 0U;
        hangProduced_ = 0U;
        cancelled_.store(false, std::memory_order_release);
    }

private:
    std::unique_ptr<SstvPcm16Source> payload_;
    std::uint64_t preKeySamples_ {0U};
    std::uint64_t payloadSamples_ {0U};
    std::uint64_t hangSamples_ {0U};
    std::uint64_t totalSamples_ {0U};
    std::uint64_t preKeyProduced_ {0U};
    std::uint64_t hangProduced_ {0U};
    double toneFrequencyHz_ {0.0};
    double toneLevel_ {0.0};
    SstvToneGenerator tone_;
    std::atomic_bool cancelled_ {false};
};

bool knownChannelRoute(SstvTxChannelRoute route) noexcept
{
    switch (route) {
    case SstvTxChannelRoute::Both:
    case SstvTxChannelRoute::Left:
    case SstvTxChannelRoute::Right:
        return true;
    }
    return false;
}

std::string preflightDetail(const SstvTxCoordinatorPreflight& preflight,
                            const char* fallback)
{
    return preflight.detail.empty()
        ? std::string(fallback) : preflight.detail;
}

std::optional<SstvMartinMode> builderMartinMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::MartinM1:
        return SstvMartinMode::M1;
    case SstvTxCoordinatorMode::MartinM2:
        return SstvMartinMode::M2;
    case SstvTxCoordinatorMode::MartinM3:
        return SstvMartinMode::M3;
    case SstvTxCoordinatorMode::MartinM4:
        return SstvMartinMode::M4;
    case SstvTxCoordinatorMode::ScottieS1:
    case SstvTxCoordinatorMode::ScottieS2:
    case SstvTxCoordinatorMode::ScottieS3:
    case SstvTxCoordinatorMode::ScottieS4:
    case SstvTxCoordinatorMode::ScottieDx:
    case SstvTxCoordinatorMode::RobotColour12:
    case SstvTxCoordinatorMode::RobotColour24:
    case SstvTxCoordinatorMode::RobotColour36:
    case SstvTxCoordinatorMode::RobotColour72:
    case SstvTxCoordinatorMode::RobotBw8:
    case SstvTxCoordinatorMode::RobotBw12:
    case SstvTxCoordinatorMode::RobotBw24:
    case SstvTxCoordinatorMode::RobotBw36:
    case SstvTxCoordinatorMode::WraaseSc2_60:
    case SstvTxCoordinatorMode::WraaseSc2_120:
    case SstvTxCoordinatorMode::WraaseSc2_180:
    case SstvTxCoordinatorMode::PasokonP3:
    case SstvTxCoordinatorMode::PasokonP5:
    case SstvTxCoordinatorMode::PasokonP7:
    case SstvTxCoordinatorMode::Pd50:
    case SstvTxCoordinatorMode::Pd90:
    case SstvTxCoordinatorMode::Pd120:
    case SstvTxCoordinatorMode::Pd160:
    case SstvTxCoordinatorMode::Pd180:
    case SstvTxCoordinatorMode::Pd240:
    case SstvTxCoordinatorMode::Pd290:
        return std::nullopt;
    default:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<SstvScottieMode> builderScottieMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::ScottieS1:
        return SstvScottieMode::S1;
    case SstvTxCoordinatorMode::ScottieS2:
        return SstvScottieMode::S2;
    case SstvTxCoordinatorMode::ScottieS3:
        return SstvScottieMode::S3;
    case SstvTxCoordinatorMode::ScottieS4:
        return SstvScottieMode::S4;
    case SstvTxCoordinatorMode::ScottieDx:
        return SstvScottieMode::DX;
    case SstvTxCoordinatorMode::MartinM1:
    case SstvTxCoordinatorMode::MartinM2:
    case SstvTxCoordinatorMode::MartinM3:
    case SstvTxCoordinatorMode::MartinM4:
    case SstvTxCoordinatorMode::RobotColour12:
    case SstvTxCoordinatorMode::RobotColour24:
    case SstvTxCoordinatorMode::RobotColour36:
    case SstvTxCoordinatorMode::RobotColour72:
    case SstvTxCoordinatorMode::RobotBw8:
    case SstvTxCoordinatorMode::RobotBw12:
    case SstvTxCoordinatorMode::RobotBw24:
    case SstvTxCoordinatorMode::RobotBw36:
    case SstvTxCoordinatorMode::WraaseSc2_60:
    case SstvTxCoordinatorMode::WraaseSc2_120:
    case SstvTxCoordinatorMode::WraaseSc2_180:
    case SstvTxCoordinatorMode::PasokonP3:
    case SstvTxCoordinatorMode::PasokonP5:
    case SstvTxCoordinatorMode::PasokonP7:
    case SstvTxCoordinatorMode::Pd50:
    case SstvTxCoordinatorMode::Pd90:
    case SstvTxCoordinatorMode::Pd120:
    case SstvTxCoordinatorMode::Pd160:
    case SstvTxCoordinatorMode::Pd180:
    case SstvTxCoordinatorMode::Pd240:
    case SstvTxCoordinatorMode::Pd290:
        return std::nullopt;
    default:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<SstvRobotMode> builderRobotMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::RobotColour12:
        return SstvRobotMode::Colour12;
    case SstvTxCoordinatorMode::RobotColour24:
        return SstvRobotMode::Colour24;
    case SstvTxCoordinatorMode::RobotColour36:
        return SstvRobotMode::Colour36;
    case SstvTxCoordinatorMode::RobotColour72:
        return SstvRobotMode::Colour72;
    case SstvTxCoordinatorMode::RobotBw8:
        return SstvRobotMode::Bw8;
    case SstvTxCoordinatorMode::RobotBw12:
        return SstvRobotMode::Bw12;
    case SstvTxCoordinatorMode::RobotBw24:
        return SstvRobotMode::Bw24;
    case SstvTxCoordinatorMode::RobotBw36:
        return SstvRobotMode::Bw36;
    case SstvTxCoordinatorMode::MartinM1:
    case SstvTxCoordinatorMode::MartinM2:
    case SstvTxCoordinatorMode::MartinM3:
    case SstvTxCoordinatorMode::MartinM4:
    case SstvTxCoordinatorMode::ScottieS1:
    case SstvTxCoordinatorMode::ScottieS2:
    case SstvTxCoordinatorMode::ScottieS3:
    case SstvTxCoordinatorMode::ScottieS4:
    case SstvTxCoordinatorMode::ScottieDx:
    case SstvTxCoordinatorMode::WraaseSc2_60:
    case SstvTxCoordinatorMode::WraaseSc2_120:
    case SstvTxCoordinatorMode::WraaseSc2_180:
    case SstvTxCoordinatorMode::PasokonP3:
    case SstvTxCoordinatorMode::PasokonP5:
    case SstvTxCoordinatorMode::PasokonP7:
    case SstvTxCoordinatorMode::Pd50:
    case SstvTxCoordinatorMode::Pd90:
    case SstvTxCoordinatorMode::Pd120:
    case SstvTxCoordinatorMode::Pd160:
    case SstvTxCoordinatorMode::Pd180:
    case SstvTxCoordinatorMode::Pd240:
    case SstvTxCoordinatorMode::Pd290:
        return std::nullopt;
    default:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<SstvSequentialRgbMode> builderSequentialRgbMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::WraaseSc2_60:
        return SstvSequentialRgbMode::WraaseSc2_60;
    case SstvTxCoordinatorMode::WraaseSc2_120:
        return SstvSequentialRgbMode::WraaseSc2_120;
    case SstvTxCoordinatorMode::WraaseSc2_180:
        return SstvSequentialRgbMode::WraaseSc2_180;
    case SstvTxCoordinatorMode::PasokonP3:
        return SstvSequentialRgbMode::PasokonP3;
    case SstvTxCoordinatorMode::PasokonP5:
        return SstvSequentialRgbMode::PasokonP5;
    case SstvTxCoordinatorMode::PasokonP7:
        return SstvSequentialRgbMode::PasokonP7;
    case SstvTxCoordinatorMode::MartinM1:
    case SstvTxCoordinatorMode::MartinM2:
    case SstvTxCoordinatorMode::MartinM3:
    case SstvTxCoordinatorMode::MartinM4:
    case SstvTxCoordinatorMode::ScottieS1:
    case SstvTxCoordinatorMode::ScottieS2:
    case SstvTxCoordinatorMode::ScottieS3:
    case SstvTxCoordinatorMode::ScottieS4:
    case SstvTxCoordinatorMode::ScottieDx:
    case SstvTxCoordinatorMode::RobotColour12:
    case SstvTxCoordinatorMode::RobotColour24:
    case SstvTxCoordinatorMode::RobotColour36:
    case SstvTxCoordinatorMode::RobotColour72:
    case SstvTxCoordinatorMode::RobotBw8:
    case SstvTxCoordinatorMode::RobotBw12:
    case SstvTxCoordinatorMode::RobotBw24:
    case SstvTxCoordinatorMode::RobotBw36:
    case SstvTxCoordinatorMode::Pd50:
    case SstvTxCoordinatorMode::Pd90:
    case SstvTxCoordinatorMode::Pd120:
    case SstvTxCoordinatorMode::Pd160:
    case SstvTxCoordinatorMode::Pd180:
    case SstvTxCoordinatorMode::Pd240:
    case SstvTxCoordinatorMode::Pd290:
        return std::nullopt;
    default:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<SstvPdMode> builderPdMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::Pd50:
        return SstvPdMode::Pd50;
    case SstvTxCoordinatorMode::Pd90:
        return SstvPdMode::Pd90;
    case SstvTxCoordinatorMode::Pd120:
        return SstvPdMode::Pd120;
    case SstvTxCoordinatorMode::Pd160:
        return SstvPdMode::Pd160;
    case SstvTxCoordinatorMode::Pd180:
        return SstvPdMode::Pd180;
    case SstvTxCoordinatorMode::Pd240:
        return SstvPdMode::Pd240;
    case SstvTxCoordinatorMode::Pd290:
        return SstvPdMode::Pd290;
    case SstvTxCoordinatorMode::MartinM1:
    case SstvTxCoordinatorMode::MartinM2:
    case SstvTxCoordinatorMode::MartinM3:
    case SstvTxCoordinatorMode::MartinM4:
    case SstvTxCoordinatorMode::ScottieS1:
    case SstvTxCoordinatorMode::ScottieS2:
    case SstvTxCoordinatorMode::ScottieS3:
    case SstvTxCoordinatorMode::ScottieS4:
    case SstvTxCoordinatorMode::ScottieDx:
    case SstvTxCoordinatorMode::RobotColour12:
    case SstvTxCoordinatorMode::RobotColour24:
    case SstvTxCoordinatorMode::RobotColour36:
    case SstvTxCoordinatorMode::RobotColour72:
    case SstvTxCoordinatorMode::RobotBw8:
    case SstvTxCoordinatorMode::RobotBw12:
    case SstvTxCoordinatorMode::RobotBw24:
    case SstvTxCoordinatorMode::RobotBw36:
    case SstvTxCoordinatorMode::WraaseSc2_60:
    case SstvTxCoordinatorMode::WraaseSc2_120:
    case SstvTxCoordinatorMode::WraaseSc2_180:
    case SstvTxCoordinatorMode::PasokonP3:
    case SstvTxCoordinatorMode::PasokonP5:
    case SstvTxCoordinatorMode::PasokonP7:
        return std::nullopt;
    default:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<SstvMmsstvMode> builderMmsstvMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::Mp73: return SstvMmsstvMode::Mp73;
    case SstvTxCoordinatorMode::Mp115: return SstvMmsstvMode::Mp115;
    case SstvTxCoordinatorMode::Mp140: return SstvMmsstvMode::Mp140;
    case SstvTxCoordinatorMode::Mp175: return SstvMmsstvMode::Mp175;
    case SstvTxCoordinatorMode::Mr73: return SstvMmsstvMode::Mr73;
    case SstvTxCoordinatorMode::Mr90: return SstvMmsstvMode::Mr90;
    case SstvTxCoordinatorMode::Mr115: return SstvMmsstvMode::Mr115;
    case SstvTxCoordinatorMode::Mr140: return SstvMmsstvMode::Mr140;
    case SstvTxCoordinatorMode::Mr175: return SstvMmsstvMode::Mr175;
    case SstvTxCoordinatorMode::Ml180: return SstvMmsstvMode::Ml180;
    case SstvTxCoordinatorMode::Ml240: return SstvMmsstvMode::Ml240;
    case SstvTxCoordinatorMode::Ml280: return SstvMmsstvMode::Ml280;
    case SstvTxCoordinatorMode::Ml320: return SstvMmsstvMode::Ml320;
    case SstvTxCoordinatorMode::Mp73Narrow:
        return SstvMmsstvMode::Mp73Narrow;
    case SstvTxCoordinatorMode::Mp110Narrow:
        return SstvMmsstvMode::Mp110Narrow;
    case SstvTxCoordinatorMode::Mp140Narrow:
        return SstvMmsstvMode::Mp140Narrow;
    case SstvTxCoordinatorMode::Mc110Narrow:
        return SstvMmsstvMode::Mc110Narrow;
    case SstvTxCoordinatorMode::Mc140Narrow:
        return SstvMmsstvMode::Mc140Narrow;
    case SstvTxCoordinatorMode::Mc180Narrow:
        return SstvMmsstvMode::Mc180Narrow;
    default:
        return std::nullopt;
    }
}

std::optional<SstvAvtMode> builderAvtMode(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::Avt24:
        return SstvAvtMode::Avt24;
    case SstvTxCoordinatorMode::Avt90:
        return SstvAvtMode::Avt90;
    case SstvTxCoordinatorMode::Avt94:
        return SstvAvtMode::Avt94;
    default:
        return std::nullopt;
    }
}

std::pair<std::uint32_t, std::uint32_t> builderGeometry(
    SstvTxCoordinatorMode mode)
{
    const std::optional<SstvMartinMode> martin = builderMartinMode(mode);
    if (martin.has_value()) {
        const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(*martin);
        return {spec.width, spec.height};
    }
    const std::optional<SstvScottieMode> scottie = builderScottieMode(mode);
    if (scottie.has_value()) {
        const SstvScottieModeSpec spec = SstvScottieProtocol::spec(*scottie);
        return {spec.width, spec.height};
    }
    const std::optional<SstvSequentialRgbMode> sequentialRgb =
        builderSequentialRgbMode(mode);
    if (sequentialRgb.has_value()) {
        const SstvSequentialRgbModeSpec spec =
            SstvSequentialRgbProtocol::spec(*sequentialRgb);
        return {spec.width, spec.height};
    }
    const std::optional<SstvPdMode> pd = builderPdMode(mode);
    if (pd.has_value()) {
        const SstvPdModeSpec spec = SstvPdProtocol::spec(*pd);
        return {spec.width, spec.height};
    }
    const std::optional<SstvAvtMode> avt = builderAvtMode(mode);
    if (avt.has_value()) {
        const SstvAvtModeSpec spec = SstvAvtProtocol::spec(*avt);
        return {spec.width, spec.height};
    }
    const std::optional<SstvMmsstvMode> mmsstv = builderMmsstvMode(mode);
    if (mmsstv.has_value()) {
        const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(*mmsstv);
        return {spec.width, spec.height};
    }
    const std::optional<SstvRobotMode> robot = builderRobotMode(mode);
    if (!robot.has_value()) {
        throw std::invalid_argument("unsupported SSTV TX mode");
    }
    const SstvRobotModeSpec spec = SstvRobotProtocol::spec(*robot);
    return {spec.width, spec.height};
}

std::size_t builderPixelCount(SstvTxCoordinatorMode mode)
{
    const auto geometry = builderGeometry(mode);
    if (geometry.first != 0U
        && geometry.second > std::numeric_limits<std::size_t>::max()
            / geometry.first) {
        throw std::overflow_error("SSTV TX image pixel count overflow");
    }
    return static_cast<std::size_t>(geometry.first) * geometry.second;
}

} // namespace

const char* SstvTxSourceBuilder::modeId(
    SstvTxCoordinatorMode mode) noexcept
{
    switch (mode) {
    case SstvTxCoordinatorMode::MartinM1:
        return "martin-m1";
    case SstvTxCoordinatorMode::MartinM2:
        return "martin-m2";
    case SstvTxCoordinatorMode::MartinM3:
        return "martin-m3";
    case SstvTxCoordinatorMode::MartinM4:
        return "martin-m4";
    case SstvTxCoordinatorMode::ScottieS1:
        return "scottie-s1";
    case SstvTxCoordinatorMode::ScottieS2:
        return "scottie-s2";
    case SstvTxCoordinatorMode::ScottieS3:
        return "scottie-s3";
    case SstvTxCoordinatorMode::ScottieS4:
        return "scottie-s4";
    case SstvTxCoordinatorMode::ScottieDx:
        return "scottie-dx";
    case SstvTxCoordinatorMode::RobotColour12:
        return "robot-c12";
    case SstvTxCoordinatorMode::RobotColour24:
        return "robot-c24";
    case SstvTxCoordinatorMode::RobotColour36:
        return "robot-c36";
    case SstvTxCoordinatorMode::RobotColour72:
        return "robot-c72";
    case SstvTxCoordinatorMode::RobotBw8:
        return "robot-bw8";
    case SstvTxCoordinatorMode::RobotBw12:
        return "robot-bw12";
    case SstvTxCoordinatorMode::RobotBw24:
        return "robot-bw24";
    case SstvTxCoordinatorMode::RobotBw36:
        return "robot-bw36";
    case SstvTxCoordinatorMode::WraaseSc2_60:
        return "wraase-sc2-60";
    case SstvTxCoordinatorMode::WraaseSc2_120:
        return "wraase-sc2-120";
    case SstvTxCoordinatorMode::WraaseSc2_180:
        return "wraase-sc2-180";
    case SstvTxCoordinatorMode::PasokonP3:
        return "pasokon-p3";
    case SstvTxCoordinatorMode::PasokonP5:
        return "pasokon-p5";
    case SstvTxCoordinatorMode::PasokonP7:
        return "pasokon-p7";
    case SstvTxCoordinatorMode::Pd50:
        return "pd-50";
    case SstvTxCoordinatorMode::Pd90:
        return "pd-90";
    case SstvTxCoordinatorMode::Pd120:
        return "pd-120";
    case SstvTxCoordinatorMode::Pd160:
        return "pd-160";
    case SstvTxCoordinatorMode::Pd180:
        return "pd-180";
    case SstvTxCoordinatorMode::Pd240:
        return "pd-240";
    case SstvTxCoordinatorMode::Pd290:
        return "pd-290";
    case SstvTxCoordinatorMode::Avt24:
        return "avt-24";
    case SstvTxCoordinatorMode::Avt90:
        return "avt-90";
    case SstvTxCoordinatorMode::Avt94:
        return "avt-94";
    case SstvTxCoordinatorMode::Mp73:
        return "mp-73";
    case SstvTxCoordinatorMode::Mp115:
        return "mp-115";
    case SstvTxCoordinatorMode::Mp140:
        return "mp-140";
    case SstvTxCoordinatorMode::Mp175:
        return "mp-175";
    case SstvTxCoordinatorMode::Mr73:
        return "mr-73";
    case SstvTxCoordinatorMode::Mr90:
        return "mr-90";
    case SstvTxCoordinatorMode::Mr115:
        return "mr-115";
    case SstvTxCoordinatorMode::Mr140:
        return "mr-140";
    case SstvTxCoordinatorMode::Mr175:
        return "mr-175";
    case SstvTxCoordinatorMode::Ml180:
        return "ml-180";
    case SstvTxCoordinatorMode::Ml240:
        return "ml-240";
    case SstvTxCoordinatorMode::Ml280:
        return "ml-280";
    case SstvTxCoordinatorMode::Ml320:
        return "ml-320";
    case SstvTxCoordinatorMode::Mp73Narrow:
        return "mp-73-narrow";
    case SstvTxCoordinatorMode::Mp110Narrow:
        return "mp-110-narrow";
    case SstvTxCoordinatorMode::Mp140Narrow:
        return "mp-140-narrow";
    case SstvTxCoordinatorMode::Mc110Narrow:
        return "mc-110-narrow";
    case SstvTxCoordinatorMode::Mc140Narrow:
        return "mc-140-narrow";
    case SstvTxCoordinatorMode::Mc180Narrow:
        return "mc-180-narrow";
    }
    return "";
}

std::optional<SstvTxCoordinatorMode> SstvTxSourceBuilder::modeFromId(
    std::string_view modeId) noexcept
{
    if (modeId == "martin-m1") {
        return SstvTxCoordinatorMode::MartinM1;
    }
    if (modeId == "martin-m2") {
        return SstvTxCoordinatorMode::MartinM2;
    }
    if (modeId == "martin-m3") {
        return SstvTxCoordinatorMode::MartinM3;
    }
    if (modeId == "martin-m4") {
        return SstvTxCoordinatorMode::MartinM4;
    }
    if (modeId == "scottie-s1") {
        return SstvTxCoordinatorMode::ScottieS1;
    }
    if (modeId == "scottie-s2") {
        return SstvTxCoordinatorMode::ScottieS2;
    }
    if (modeId == "scottie-s3") {
        return SstvTxCoordinatorMode::ScottieS3;
    }
    if (modeId == "scottie-s4") {
        return SstvTxCoordinatorMode::ScottieS4;
    }
    if (modeId == "scottie-dx") {
        return SstvTxCoordinatorMode::ScottieDx;
    }
    if (modeId == "robot-c12") {
        return SstvTxCoordinatorMode::RobotColour12;
    }
    if (modeId == "robot-c24") {
        return SstvTxCoordinatorMode::RobotColour24;
    }
    if (modeId == "robot-c36") {
        return SstvTxCoordinatorMode::RobotColour36;
    }
    if (modeId == "robot-c72") {
        return SstvTxCoordinatorMode::RobotColour72;
    }
    if (modeId == "robot-bw8") {
        return SstvTxCoordinatorMode::RobotBw8;
    }
    if (modeId == "robot-bw12") {
        return SstvTxCoordinatorMode::RobotBw12;
    }
    if (modeId == "robot-bw24") {
        return SstvTxCoordinatorMode::RobotBw24;
    }
    if (modeId == "robot-bw36") {
        return SstvTxCoordinatorMode::RobotBw36;
    }
    if (modeId == "wraase-sc2-60") {
        return SstvTxCoordinatorMode::WraaseSc2_60;
    }
    if (modeId == "wraase-sc2-120") {
        return SstvTxCoordinatorMode::WraaseSc2_120;
    }
    if (modeId == "wraase-sc2-180") {
        return SstvTxCoordinatorMode::WraaseSc2_180;
    }
    if (modeId == "pasokon-p3") {
        return SstvTxCoordinatorMode::PasokonP3;
    }
    if (modeId == "pasokon-p5") {
        return SstvTxCoordinatorMode::PasokonP5;
    }
    if (modeId == "pasokon-p7") {
        return SstvTxCoordinatorMode::PasokonP7;
    }
    if (modeId == "pd-50") {
        return SstvTxCoordinatorMode::Pd50;
    }
    if (modeId == "pd-90") {
        return SstvTxCoordinatorMode::Pd90;
    }
    if (modeId == "pd-120") {
        return SstvTxCoordinatorMode::Pd120;
    }
    if (modeId == "pd-160") {
        return SstvTxCoordinatorMode::Pd160;
    }
    if (modeId == "pd-180") {
        return SstvTxCoordinatorMode::Pd180;
    }
    if (modeId == "pd-240") {
        return SstvTxCoordinatorMode::Pd240;
    }
    if (modeId == "pd-290") {
        return SstvTxCoordinatorMode::Pd290;
    }
    if (modeId == "avt-24") {
        return SstvTxCoordinatorMode::Avt24;
    }
    if (modeId == "avt-90") {
        return SstvTxCoordinatorMode::Avt90;
    }
    if (modeId == "avt-94") {
        return SstvTxCoordinatorMode::Avt94;
    }
    if (modeId == "mp-73") {
        return SstvTxCoordinatorMode::Mp73;
    }
    if (modeId == "mp-115") {
        return SstvTxCoordinatorMode::Mp115;
    }
    if (modeId == "mp-140") {
        return SstvTxCoordinatorMode::Mp140;
    }
    if (modeId == "mp-175") {
        return SstvTxCoordinatorMode::Mp175;
    }
    if (modeId == "mr-73") {
        return SstvTxCoordinatorMode::Mr73;
    }
    if (modeId == "mr-90") {
        return SstvTxCoordinatorMode::Mr90;
    }
    if (modeId == "mr-115") {
        return SstvTxCoordinatorMode::Mr115;
    }
    if (modeId == "mr-140") {
        return SstvTxCoordinatorMode::Mr140;
    }
    if (modeId == "mr-175") {
        return SstvTxCoordinatorMode::Mr175;
    }
    if (modeId == "ml-180") {
        return SstvTxCoordinatorMode::Ml180;
    }
    if (modeId == "ml-240") {
        return SstvTxCoordinatorMode::Ml240;
    }
    if (modeId == "ml-280") {
        return SstvTxCoordinatorMode::Ml280;
    }
    if (modeId == "ml-320") {
        return SstvTxCoordinatorMode::Ml320;
    }
    if (modeId == "mp-73-narrow") {
        return SstvTxCoordinatorMode::Mp73Narrow;
    }
    if (modeId == "mp-110-narrow") {
        return SstvTxCoordinatorMode::Mp110Narrow;
    }
    if (modeId == "mp-140-narrow") {
        return SstvTxCoordinatorMode::Mp140Narrow;
    }
    if (modeId == "mc-110-narrow") {
        return SstvTxCoordinatorMode::Mc110Narrow;
    }
    if (modeId == "mc-140-narrow") {
        return SstvTxCoordinatorMode::Mc140Narrow;
    }
    if (modeId == "mc-180-narrow") {
        return SstvTxCoordinatorMode::Mc180Narrow;
    }
    return std::nullopt;
}

std::vector<SstvRgbPixel> SstvTxSourceBuilder::pixelsFromImage(
    const QImage& image)
{
    return pixelsFromImage(image, SstvTxCoordinatorMode::MartinM1);
}

std::vector<SstvRgbPixel> SstvTxSourceBuilder::pixelsFromImage(
    const QImage& image,
    SstvTxCoordinatorMode mode)
{
    const auto geometry = builderGeometry(mode);
    if (image.isNull()
        || image.width() != static_cast<int>(geometry.first)
        || image.height() != static_cast<int>(geometry.second)) {
        throw std::invalid_argument(
            "SSTV TX QImage dimensions do not match the selected mode");
    }
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    if (rgb.isNull()) {
        throw std::runtime_error("SSTV TX QImage RGB conversion failed");
    }

    std::vector<SstvRgbPixel> pixels;
    const std::size_t expectedPixels = builderPixelCount(mode);
    pixels.reserve(expectedPixels);
    for (int y = 0; y < rgb.height(); ++y) {
        const uchar* const scan = rgb.constScanLine(y);
        if (scan == nullptr) {
            throw std::runtime_error("SSTV TX QImage scanline is unavailable");
        }
        for (int x = 0; x < rgb.width(); ++x) {
            const std::size_t byte = static_cast<std::size_t>(x) * 3U;
            pixels.push_back(SstvRgbPixel {
                scan[byte], scan[byte + 1U], scan[byte + 2U]});
        }
    }
    if (pixels.size() != expectedPixels) {
        throw std::runtime_error("SSTV TX QImage conversion was incomplete");
    }
    return pixels;
}

SstvTxBuiltSource SstvTxSourceBuilder::build(
    const std::vector<SstvRgbPixel>& pixels,
    const SstvTxSourceBuilderConfig& config)
{
    if (modeId(config.mode)[0] == '\0') {
        throw std::invalid_argument("unsupported SSTV TX mode");
    }
    const std::size_t expectedPixels = builderPixelCount(config.mode);
    if (pixels.size() != expectedPixels) {
        throw std::invalid_argument(
            "SSTV TX image pixel count does not match the selected mode");
    }

    std::unique_ptr<SstvPcm16Source> image;
    const std::optional<SstvMartinMode> martin = builderMartinMode(
        config.mode);
    if (martin.has_value()) {
        SstvMartinM1EncoderConfig encoder;
        encoder.mode = *martin;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makeMartinM1Pcm16Source(pixels, encoder);
    } else if (const std::optional<SstvScottieMode> mode =
                   builderScottieMode(config.mode);
               mode.has_value()) {
        SstvScottieEncoderConfig encoder;
        encoder.mode = *mode;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makeScottiePcm16Source(pixels, encoder);
    } else if (const std::optional<SstvSequentialRgbMode> mode =
                   builderSequentialRgbMode(config.mode);
               mode.has_value()) {
        SstvSequentialRgbEncoderConfig encoder;
        encoder.mode = *mode;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makeSequentialRgbPcm16Source(pixels, encoder);
    } else if (const std::optional<SstvPdMode> mode =
                   builderPdMode(config.mode);
               mode.has_value()) {
        SstvPdEncoderConfig encoder;
        encoder.mode = *mode;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makePdPcm16Source(pixels, encoder);
    } else if (const std::optional<SstvAvtMode> mode =
                   builderAvtMode(config.mode);
               mode.has_value()) {
        SstvAvtEncoderConfig encoder;
        encoder.mode = *mode;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makeAvtPcm16Source(pixels, encoder);
    } else if (const std::optional<SstvMmsstvMode> mode =
                   builderMmsstvMode(config.mode);
               mode.has_value()) {
        SstvMmsstvEncoderConfig encoder;
        encoder.mode = *mode;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makeMmsstvPcm16Source(pixels, encoder);
    } else {
        const std::optional<SstvRobotMode> robotMode = builderRobotMode(
            config.mode);
        if (!robotMode.has_value()) {
            throw std::invalid_argument("unsupported SSTV TX mode");
        }
        SstvRobotEncoderConfig encoder;
        encoder.mode = *robotMode;
        encoder.sampleRate = config.sampleRate;
        encoder.clockErrorPpm = config.clockErrorPpm;
        encoder.level = config.level;
        encoder.headroom = config.headroom;
        image = makeRobotPcm16Source(pixels, encoder);
    }
    const std::uint64_t imageFrames = image->totalSamples();

    std::unique_ptr<SstvPcm16Source> fskId;
    std::uint64_t fskIdFrames = 0U;
    if (config.fskId.has_value()) {
        SstvFskIdTxConfig fskConfig;
        fskConfig.sampleRate = config.sampleRate;
        fskConfig.textPolicy = config.fskId->textPolicy;
        fskConfig.inputHandling = config.fskId->inputHandling;
        fskConfig.level = config.level;
        fskConfig.headroom = config.headroom;
        fskId = makeFskIdPcm16Source(config.fskId->text, fskConfig);
        fskIdFrames = fskId->totalSamples();
    }

    auto source = std::make_unique<SequentialPcm16Source>(
        std::move(image), std::move(fskId));
    const std::uint64_t totalFrames = source->totalSamples();
    SstvTimingAccumulator timing(config.sampleRate);
    Picoseconds headerDuration = SstvRobotProtocol::HeaderDuration;
    if (martin.has_value()) {
        headerDuration = SstvMartinM1Protocol::HeaderDuration;
    } else if (builderScottieMode(config.mode).has_value()) {
        headerDuration = SstvScottieProtocol::HeaderDuration;
    } else if (builderSequentialRgbMode(config.mode).has_value()) {
        headerDuration = SstvSequentialRgbProtocol::HeaderDuration;
    } else if (builderPdMode(config.mode).has_value()) {
        headerDuration = SstvPdProtocol::HeaderDuration;
    } else if (builderAvtMode(config.mode).has_value()) {
        headerDuration = SstvAvtProtocol::HeaderDuration;
    } else if (const std::optional<SstvMmsstvMode> mode =
                   builderMmsstvMode(config.mode);
               mode.has_value()) {
        headerDuration = SstvMmsstvProtocol::spec(*mode).headerDuration;
    }
    const std::uint64_t headerFrames = timing.samplesFor(headerDuration);
    if (headerFrames == 0U || headerFrames >= imageFrames
        || imageFrames > totalFrames
        || totalFrames - imageFrames != fskIdFrames) {
        throw std::runtime_error("invalid SSTV TX audio phase boundaries");
    }

    SstvTxBuiltSource result;
    result.source = std::move(source);
    result.mode = modeId(config.mode);
    result.sampleRate = config.sampleRate;
    const auto geometry = builderGeometry(config.mode);
    result.width = geometry.first;
    result.height = geometry.second;
    result.headerFrames = headerFrames;
    result.imageEndFrame = imageFrames;
    result.totalFrames = totalFrames;
    result.fskIdFrames = fskIdFrames;
    result.headroom = config.headroom;
    result.fskIdPlanned = config.fskId.has_value();
    return result;
}

SstvTxBuiltSource SstvTxSourceBuilder::build(
    const QImage& image,
    const SstvTxSourceBuilderConfig& config)
{
    return build(pixelsFromImage(image, config.mode), config);
}

SstvTxCoordinator::SstvTxCoordinator(SstvTxCoordinatorConfig config,
                                     SstvTxCoordinatorHooks hooks)
    : config_(std::move(config))
    , hooks_(std::move(hooks))
    , stateMachine_(config_.stateMachinePolicy)
{
    validateConfig(config_, hooks_);
}

SstvTxCoordinator::~SstvTxCoordinator()
{
    emergencyShutdown();
}

SstvTxCoordinatorResult SstvTxCoordinator::enable(std::uint64_t nowMs)
{
    if (stateMachine_.enabled()) {
        return acceptedResult();
    }
    if (stateMachine_.state() != SstvTxState::Disabled) {
        return rejectPreflight(SstvTxErrorCode::TxBusy,
                               "SSTV TX coordinator is not disabled");
    }
    if (!dispatch(nowMs, SstvTxEnable {})) {
        return rejectPreflight(SstvTxErrorCode::InternalFailure,
                               "SSTV TX coordinator could not be enabled");
    }
    lastOperationError_ = SstvTxErrorCode::None;
    lastOperationDetail_.clear();
    publishState();
    return acceptedResult();
}

bool SstvTxCoordinator::updateTimingConfig(
    const SstvTxTimingConfig& timing)
{
    if (destroying_ || stateMachine_.active() || audioDevice_) {
        return false;
    }

    SstvTxCoordinatorConfig candidate = config_;
    candidate.pttLeadDelayMs = timing.pttLeadDelayMs;
    candidate.pttTailDelayMs = timing.pttTailDelayMs;
    candidate.pttReleaseRetryMs = timing.pttReleaseRetryMs;
    candidate.voxPreKeyMs = timing.voxPreKeyMs;
    candidate.voxHangMs = timing.voxHangMs;
    candidate.voxToneFrequencyHz = timing.voxToneFrequencyHz;
    candidate.voxToneLevel = timing.voxToneLevel;
    try {
        validateConfig(candidate, hooks_);
    } catch (...) {
        return false;
    }
    config_ = std::move(candidate);
    publishState();
    return true;
}

SstvTxCoordinatorResult SstvTxCoordinator::start(
    std::uint64_t nowMs,
    const SstvTxCoordinatorRequest& request)
{
    saturatingAdd(metrics_.startCalls);
    if (!tick(nowMs)) {
        return rejectStart(SstvTxErrorCode::ClockRegression,
                           "non-monotonic SSTV TX coordinator timestamp",
                           modeId(request.mode));
    }
    if (!stateMachine_.enabled()) {
        return rejectStart(SstvTxErrorCode::TxNotPermitted,
                           "SSTV TX coordinator is disabled",
                           modeId(request.mode));
    }
    if (stateMachine_.active() || audioDevice_) {
        return rejectStart(SstvTxErrorCode::TxBusy,
                           "another SSTV TX lifecycle is still active",
                           modeId(request.mode));
    }

    SstvTxErrorCode validationError = SstvTxErrorCode::None;
    std::string validationDetail;
    if (!validateRequest(request, validationError, validationDetail)) {
        return rejectStart(validationError,
                           std::move(validationDetail),
                           modeId(request.mode));
    }

    const auto imageGeometry = builderGeometry(request.mode);
    return startValidated(
        nowMs, imageGeometry.first, imageGeometry.second,
        modeId(request.mode),
        [this, &request](bool voxAudioActivation) {
            return buildAudio(request, voxAudioActivation);
        });
}

SstvTxCoordinatorResult SstvTxCoordinator::startPrepared(
    std::uint64_t nowMs,
    SstvTxPreparedAudioRequest request)
{
    saturatingAdd(metrics_.startCalls);
    const std::string diagnosticMode = request.mode;
    if (!tick(nowMs)) {
        return rejectStart(SstvTxErrorCode::ClockRegression,
                           "non-monotonic SSTV TX coordinator timestamp",
                           diagnosticMode);
    }
    if (!stateMachine_.enabled()) {
        return rejectStart(SstvTxErrorCode::TxNotPermitted,
                           "SSTV TX coordinator is disabled",
                           diagnosticMode);
    }
    if (stateMachine_.active() || audioDevice_) {
        return rejectStart(SstvTxErrorCode::TxBusy,
                           "another SSTV TX lifecycle is still active",
                           diagnosticMode);
    }

    SstvTxErrorCode validationError = SstvTxErrorCode::None;
    std::string validationDetail;
    if (!validatePreparedRequest(request, validationError,
                                 validationDetail)) {
        return rejectStart(validationError,
                           std::move(validationDetail),
                           diagnosticMode);
    }

    const std::uint32_t width = request.width;
    const std::uint32_t height = request.height;
    return startValidated(
        nowMs, width, height, diagnosticMode,
        [this, &request](bool voxAudioActivation) {
            return buildPreparedAudio(request, voxAudioActivation);
        });
}

SstvTxCoordinatorResult SstvTxCoordinator::startValidated(
    std::uint64_t nowMs,
    std::uint32_t width,
    std::uint32_t height,
    std::string diagnosticMode,
    const std::function<BuiltAudio(bool)>& build)
{

    SstvTxCoordinatorPreflight preflight;
    try {
        preflight = hooks_.queryPreflight();
    } catch (const std::exception& exception) {
        saturatingAdd(metrics_.hookFailures);
        return rejectStart(
            SstvTxErrorCode::InternalFailure,
            std::string("SSTV TX preflight hook failed: ")
                + exception.what(),
            diagnosticMode);
    } catch (...) {
        saturatingAdd(metrics_.hookFailures);
        return rejectStart(
            SstvTxErrorCode::InternalFailure,
            "SSTV TX preflight hook failed with an unknown exception",
            diagnosticMode);
    }
    if (!preflight.audioOutputReady) {
        return rejectStart(
            SstvTxErrorCode::AudioDeviceLoss,
            preflightDetail(preflight,
                            "SSTV TX audio output is not ready"),
            diagnosticMode);
    }
    if (!preflight.pttPathReady) {
        return rejectStart(
            SstvTxErrorCode::TxNotPermitted,
            preflightDetail(preflight,
                            "SSTV TX PTT path is not ready"),
            diagnosticMode);
    }
    if (preflight.weakSignalSequencerActive
        || preflight.transmitAlreadyActive) {
        return rejectStart(
            SstvTxErrorCode::TxBusy,
            preflightDetail(
                preflight,
                "weak-signal sequencer or another TX is active"),
            diagnosticMode);
    }

    BuiltAudio audio;
    try {
        audio = build(!preflight.pttReleaseRequired);
    } catch (const std::invalid_argument& exception) {
        return rejectStart(SstvTxErrorCode::EncodingFailure,
                           exception.what(), diagnosticMode);
    } catch (const std::exception& exception) {
        return rejectStart(SstvTxErrorCode::EncodingFailure,
                           exception.what(), diagnosticMode);
    } catch (...) {
        return rejectStart(
            SstvTxErrorCode::EncodingFailure,
            "SSTV TX source construction failed with an unknown exception",
            diagnosticMode);
    }
    if (audio.plan.totalFrames
        > config_.stateMachinePolicy.maximumEncodedSamples) {
        return rejectStart(
            SstvTxErrorCode::EncodingFailure,
            "SSTV TX job exceeds the configured sample bound",
            diagnosticMode);
    }

    clearForNewSession(std::move(audio));
    diagnosticSessionStartedAtMs_ = nowMs;
    pttReleaseRequired_ = preflight.pttReleaseRequired;

    const SstvTxTransition prepared = stateMachine_.dispatch(
        nowMs,
        SstvTxEvent {SstvTxPrepare {
            audioPlan_.mode,
            width,
            height}});
    if (!prepared.accepted) {
        releaseAudioLease();
        publishState();
        return rejectStart(SstvTxErrorCode::InternalFailure,
                           "SSTV TX prepare transition was rejected",
                           diagnosticMode);
    }
    activeSessionId_ = stateMachine_.metrics().currentSessionId;
    audioPlan_.sessionId = activeSessionId_;
    publishState();

    if (!dispatch(nowMs, SstvTxImagePrepared {})
        || !dispatch(nowMs,
                     SstvTxEncodingComplete {
                         audioPlan_.totalFrames,
                         audioPlan_.sampleRate,
                         audioPlan_.fskIdPlanned})
        || !dispatch(nowMs, SstvTxRequest {})
        || !dispatch(nowMs,
                     SstvTxPttRequestDispatched {
                         pttReleaseRequired_})) {
        static_cast<void>(fail(
            nowMs,
            SstvTxErrorCode::InternalFailure,
            "SSTV TX lifecycle preparation transition failed",
            SstvTxAudioDetachReason::Error));
        return {false,
                lastOperationError_,
                lastOperationDetail_,
                activeSessionId_};
    }

    pttOnAttempted_ = true;
    saturatingAdd(metrics_.pttOnAttempts);
    bool pttRequested = false;
    try {
        pttRequested = hooks_.requestPttOn(activeSessionId_);
    } catch (const std::exception& exception) {
        saturatingAdd(metrics_.hookFailures);
        lastOperationDetail_ = boundedDetail(
            std::string("SSTV TX PTT-on hook failed: ")
            + exception.what());
    } catch (...) {
        saturatingAdd(metrics_.hookFailures);
        lastOperationDetail_ =
            "SSTV TX PTT-on hook failed with an unknown exception";
    }
    if (!pttRequested) {
        const std::string detail = lastOperationDetail_.empty()
            ? "SSTV TX PTT-on request was not dispatched"
            : lastOperationDetail_;
        static_cast<void>(fail(nowMs,
                               SstvTxErrorCode::PttDispatchFailure,
                               detail,
                               SstvTxAudioDetachReason::Error));
        return {false,
                lastOperationError_,
                lastOperationDetail_,
                activeSessionId_};
    }

    saturatingAdd(metrics_.jobsAccepted);
    lastOperationError_ = SstvTxErrorCode::None;
    lastOperationDetail_.clear();
    publishState();
    recordTxEvent(
        isCalibrationMode(audioPlan_.mode)
            ? QStringLiteral("tx.calibration-started")
            : QStringLiteral("tx.start-accepted"),
        QtInfoMsg,
        audioPlan_.mode,
        stateMachine_.state(),
        true);
    return acceptedResult();
}

bool SstvTxCoordinator::notifyPttConfirmed(
    std::uint64_t nowMs,
    std::uint64_t sessionId)
{
    if (!sessionMatches(sessionId)
        || stateMachine_.state() != SstvTxState::WaitingForPtt
        || stateMachine_.metrics().pttConfirmed) {
        return false;
    }
    if (!dispatch(nowMs, SstvTxPttConfirmed {})) {
        if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
            static_cast<void>(detachAudio(
                stateMachine_.metrics().lastEventAtMs,
                SstvTxAudioDetachReason::Error));
            requestPttRelease(stateMachine_.metrics().lastEventAtMs);
        }
        return false;
    }
    pttConfirmedAtMs_ = nowMs;
    // CAT PTT uses the configured silent lead after positive feedback.  VOX
    // has no such feedback: start its real pre-key tone immediately so the
    // radio can key before protocolStartFrame.
    if (!pttReleaseRequired_ || config_.pttLeadDelayMs == 0U) {
        return startAudio(nowMs);
    }
    return true;
}

bool SstvTxCoordinator::notifyPlayback(
    std::uint64_t nowMs,
    std::uint64_t sessionId,
    const SstvTxPlaybackProgress& progress)
{
    if (!sessionMatches(sessionId)
        || !audioStartAttempted_
        || !audioAttached_
        || audioDetachPending_) {
        return false;
    }
    if (progress.failed) {
        return fail(nowMs,
                    SstvTxErrorCode::AudioDeviceLoss,
                    progress.detail.empty()
                        ? "SSTV TX audio output reported failure"
                        : progress.detail,
                    SstvTxAudioDetachReason::Error);
    }
    if (progress.playedFrames < playedFrames_
        || progress.playedFrames > audioPlan_.totalFrames
        || (progress.playbackComplete
            && progress.playedFrames != audioPlan_.totalFrames)) {
        return fail(nowMs,
                    SstvTxErrorCode::AudioUnderrun,
                    "SSTV TX playback progress violated its contract",
                    SstvTxAudioDetachReason::Error);
    }

    saturatingAdd(metrics_.playbackUpdates);
    playedFrames_ = progress.playedFrames;
    publishState();

    if (stateMachine_.state() == SstvTxState::TransmittingHeader
        && playedFrames_ >= audioPlan_.headerFrames
        && !dispatch(nowMs, SstvTxHeaderComplete {})) {
        return fail(nowMs,
                    SstvTxErrorCode::InternalFailure,
                    "SSTV TX header transition failed",
                    SstvTxAudioDetachReason::Error);
    }

    if (stateMachine_.state() == SstvTxState::TransmittingImage
        && playedFrames_ >= audioPlan_.imageEndFrame) {
        if (!dispatch(nowMs, SstvTxImageComplete {})) {
            return fail(nowMs,
                        SstvTxErrorCode::InternalFailure,
                        "SSTV TX image transition failed",
                        SstvTxAudioDetachReason::Error);
        }
        if (audioPlan_.fskIdPlanned) {
            fskIdStarted_ = true;
            publishState();
        }
    }

    if (stateMachine_.state() == SstvTxState::TransmittingFskId
        && playedFrames_ >= audioPlan_.protocolEndFrame) {
        if (!dispatch(nowMs, SstvTxFskIdComplete {})) {
            return fail(nowMs,
                        SstvTxErrorCode::InternalFailure,
                        "SSTV TX FSK ID transition failed",
                        SstvTxAudioDetachReason::Error);
        }
        fskIdCompleted_ = true;
        publishState();
    }

    if (progress.playbackComplete
        && stateMachine_.state() == SstvTxState::TailDelay) {
        playbackComplete_ = true;
        if (!detachAudio(nowMs, SstvTxAudioDetachReason::Completed)) {
            return fail(nowMs,
                        SstvTxErrorCode::AudioDeviceLoss,
                        "SoundOutput did not confirm SSTV device detach",
                        SstvTxAudioDetachReason::Error);
        }
    }
    return true;
}

bool SstvTxCoordinator::notifyAudioError(
    std::uint64_t nowMs,
    std::uint64_t sessionId,
    std::string detail)
{
    if (!sessionMatches(sessionId)
        || (!stateMachine_.active() && !audioDevice_)) {
        return false;
    }
    return fail(nowMs,
                SstvTxErrorCode::AudioDeviceLoss,
                detail.empty() ? "SSTV TX audio output failed"
                               : std::move(detail),
                SstvTxAudioDetachReason::Error);
}

bool SstvTxCoordinator::notifyAudioUnderrun(
    std::uint64_t nowMs,
    std::uint64_t sessionId,
    std::string detail)
{
    if (!sessionMatches(sessionId)
        || (!stateMachine_.active() && !audioDevice_)) {
        return false;
    }
    return fail(nowMs,
                SstvTxErrorCode::AudioUnderrun,
                detail.empty() ? "SSTV TX audio output underrun"
                               : std::move(detail),
                SstvTxAudioDetachReason::Error);
}

bool SstvTxCoordinator::notifyAudioDetached(
    std::uint64_t nowMs,
    std::uint64_t sessionId)
{
    if (!sessionMatches(sessionId) || !audioDetachPending_) {
        return false;
    }
    if (nowMs < stateMachine_.metrics().lastEventAtMs) {
        saturatingAdd(metrics_.staleCallbacks);
        return false;
    }
    if (!tick(nowMs)) {
        return false;
    }
    releaseAudioLease();
    publishState();
    return true;
}

bool SstvTxCoordinator::notifyPttReleased(
    std::uint64_t nowMs,
    std::uint64_t sessionId)
{
    if (!sessionMatches(sessionId)
        || stateMachine_.state() != SstvTxState::ReleasingPtt) {
        return false;
    }
    if (!dispatch(nowMs, SstvTxPttReleased {})) {
        requestPttRelease(stateMachine_.metrics().lastEventAtMs);
        return false;
    }
    pttReleased_ = true;
    if (!audioStartAttempted_ && audioDevice_) {
        releaseAudioLease();
    }
    publishState();
    return true;
}

bool SstvTxCoordinator::cancel(std::uint64_t nowMs)
{
    if (!stateMachine_.active()) {
        return false;
    }
    lastOperationError_ = SstvTxErrorCode::None;
    lastOperationDetail_.clear();
    if (!dispatch(nowMs, SstvTxCancel {})) {
        if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
            static_cast<void>(detachAudio(
                stateMachine_.metrics().lastEventAtMs,
                SstvTxAudioDetachReason::Error));
            requestPttRelease(stateMachine_.metrics().lastEventAtMs);
        }
        return false;
    }
    static_cast<void>(detachAudio(nowMs,
                                  SstvTxAudioDetachReason::Cancelled));
    if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
        requestPttRelease(nowMs);
    } else if (SstvTxStateMachine::isTerminal(stateMachine_.state())
               && !audioDetachPending_) {
        releaseAudioLease();
    }
    publishState();
    recordTxEvent(
        isCalibrationMode(audioPlan_.mode)
            ? QStringLiteral("tx.calibration-cancel-requested")
            : QStringLiteral("tx.cancel-requested"),
        QtInfoMsg,
        audioPlan_.mode,
        stateMachine_.state(),
        true,
        SstvTxErrorCode::None,
        diagnosticDurationMs(nowMs),
        true);
    return true;
}

bool SstvTxCoordinator::tick(std::uint64_t nowMs)
{
    const SstvTxState before = stateMachine_.state();
    if (!dispatch(nowMs, SstvTxTick {})) {
        if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
            static_cast<void>(detachAudio(
                stateMachine_.metrics().lastEventAtMs,
                SstvTxAudioDetachReason::Error));
            requestPttRelease(stateMachine_.metrics().lastEventAtMs);
        }
        return false;
    }

    if (before != SstvTxState::ReleasingPtt
        && stateMachine_.state() == SstvTxState::ReleasingPtt) {
        lastOperationError_ = SstvTxErrorCode::WatchdogExpired;
        lastOperationDetail_ = "SSTV TX phase watchdog expired";
        recordFailureDiagnostic(nowMs, lastOperationError_);
        static_cast<void>(detachAudio(nowMs,
                                      SstvTxAudioDetachReason::Error));
        requestPttRelease(nowMs);
        return true;
    }
    if (stateMachine_.state() == SstvTxState::Error
        || stateMachine_.state() == SstvTxState::Cancelled
        || stateMachine_.state() == SstvTxState::Disabled) {
        if (!audioStartAttempted_) {
            releaseAudioLease();
        }
        return true;
    }

    if (stateMachine_.state() == SstvTxState::WaitingForPtt
        && stateMachine_.metrics().pttConfirmed
        && nowMs >= pttConfirmedAtMs_
        && (!pttReleaseRequired_
            || nowMs - pttConfirmedAtMs_ >= config_.pttLeadDelayMs)) {
        return startAudio(nowMs);
    }

    if (stateMachine_.state() == SstvTxState::TailDelay) {
        const std::uint64_t entered =
            stateMachine_.metrics().stateEnteredAtMs;
        const std::uint64_t tailDelay = pttReleaseRequired_
            ? config_.pttTailDelayMs : 0U;
        if (playbackComplete_ && nowMs >= entered
            && nowMs - entered >= tailDelay) {
            if (!dispatch(nowMs, SstvTxTailElapsed {})) {
                return fail(nowMs,
                            SstvTxErrorCode::InternalFailure,
                            "SSTV TX tail transition failed",
                            SstvTxAudioDetachReason::Error);
            }
            requestPttRelease(nowMs);
        }
    } else if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
        requestPttRelease(nowMs);
    }
    return true;
}

bool SstvTxCoordinator::shutdown(std::uint64_t nowMs)
{
    if (stateMachine_.state() == SstvTxState::Disabled) {
        return true;
    }
    if (stateMachine_.enabled()) {
        if (!dispatch(nowMs, SstvTxDisable {})) {
            if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
                static_cast<void>(detachAudio(
                    stateMachine_.metrics().lastEventAtMs,
                    SstvTxAudioDetachReason::Error));
                requestPttRelease(stateMachine_.metrics().lastEventAtMs);
            }
            return false;
        }
    }
    static_cast<void>(detachAudio(nowMs,
                                  SstvTxAudioDetachReason::Shutdown));
    if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
        requestPttRelease(nowMs);
    } else if (stateMachine_.state() == SstvTxState::Disabled
               && !audioDetachPending_) {
        releaseAudioLease();
    }
    publishState();
    return true;
}

SstvTxCoordinatorSnapshot SstvTxCoordinator::snapshot() const
{
    SstvTxCoordinatorSnapshot result;
    result.stateMachine = stateMachine_.metrics();
    result.coordinator = metrics_;
    result.audioPlan = audioPlan_;
    result.lastOperationError = lastOperationError_;
    result.lastOperationDetail = lastOperationDetail_;
    result.playedFrames = playedFrames_;
    const std::uint64_t protocolFrames =
        audioPlan_.protocolEndFrame >= audioPlan_.protocolStartFrame
        ? audioPlan_.protocolEndFrame - audioPlan_.protocolStartFrame : 0U;
    if (playedFrames_ > audioPlan_.protocolStartFrame) {
        result.protocolPlayedFrames = std::min<std::uint64_t>(
            protocolFrames,
            playedFrames_ - audioPlan_.protocolStartFrame);
    }
    result.progress = protocolFrames == 0U
        ? 0.0
        : static_cast<double>(
            static_cast<long double>(result.protocolPlayedFrames)
            / static_cast<long double>(protocolFrames));
    result.pcmPeak = audioDevice_
        ? audioDevice_->peakNormalized() : lastPcmPeak_;
    result.headroom = audioPlan_.headroom;
    result.clippedFrames = audioDevice_
        ? audioDevice_->clippedFrames() : lastClippedFrames_;
    result.audioStartAttempted = audioStartAttempted_;
    result.audioAttached = audioAttached_;
    result.audioDetachPending = audioDetachPending_;
    result.audioLeaseRetained = static_cast<bool>(audioDevice_);
    result.pttOnAttempted = pttOnAttempted_;
    result.pttOffAttempted = pttOffAttempted_;
    result.pttReleased = pttReleased_;
    result.fskIdPlanned = audioPlan_.fskIdPlanned;
    result.fskIdOnAir = fskIdStarted_ && !fskIdCompleted_
        && stateMachine_.state() == SstvTxState::TransmittingFskId;
    result.fskIdCompleted = fskIdCompleted_;
    return result;
}

const SstvTxStateMachine& SstvTxCoordinator::stateMachine() const noexcept
{
    return stateMachine_;
}

const char* SstvTxCoordinator::modeId(
    SstvTxCoordinatorMode mode) noexcept
{
    return SstvTxSourceBuilder::modeId(mode);
}

void SstvTxCoordinator::validateConfig(
    const SstvTxCoordinatorConfig& config,
    const SstvTxCoordinatorHooks& hooks)
{
    if (!SstvToneGenerator::isSupportedSampleRate(config.sampleRate)
        || config.sampleRate
            < config.stateMachinePolicy.minimumSampleRate
        || config.sampleRate
            > config.stateMachinePolicy.maximumSampleRate
        || config.pttLeadDelayMs
            >= config.stateMachinePolicy.timeouts.waitingForPttMs
        || config.pttTailDelayMs
            >= config.stateMachinePolicy.timeouts.tailDelayMs
        || config.voxPreKeyMs == 0U || config.voxHangMs == 0U
        || config.voxPreKeyMs
            >= config.stateMachinePolicy.timeouts.transmittingHeaderMs
        || config.voxHangMs
            >= config.stateMachinePolicy.timeouts.tailDelayMs
        || !std::isfinite(config.voxToneFrequencyHz)
        || config.voxToneFrequencyHz <= 0.0
        || config.voxToneFrequencyHz
            >= static_cast<double>(config.sampleRate) / 2.0
        || !std::isfinite(config.voxToneLevel)
        || config.voxToneLevel <= 0.0 || config.voxToneLevel > 1.0
        || config.pttReleaseRetryMs == 0U
        || config.pttReleaseRetryMs
            > config.stateMachinePolicy.timeouts.releasingPttMs
        || !hooks.queryPreflight || !hooks.requestPttOn
        || !hooks.requestPttOff || !hooks.startAudio
        || !hooks.detachAudio) {
        throw std::invalid_argument(
            "invalid SSTV TX coordinator configuration or hooks");
    }
}

void SstvTxCoordinator::saturatingAdd(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) {
        ++value;
    }
}

SstvTxCoordinatorResult SstvTxCoordinator::rejectPreflight(
    SstvTxErrorCode error,
    std::string detail)
{
    saturatingAdd(metrics_.preflightRejections);
    lastOperationError_ = error == SstvTxErrorCode::None
        ? SstvTxErrorCode::InternalFailure : error;
    lastOperationDetail_ = boundedDetail(std::move(detail));
    publishState();
    return {false,
            lastOperationError_,
            lastOperationDetail_,
            stateMachine_.metrics().currentSessionId};
}

SstvTxCoordinatorResult SstvTxCoordinator::rejectStart(
    SstvTxErrorCode error,
    std::string detail,
    std::string_view mode)
{
    const SstvTxCoordinatorResult result = rejectPreflight(
        error, std::move(detail));
    recordTxEvent(
        isCalibrationMode(mode)
            ? QStringLiteral("tx.calibration-rejected")
            : QStringLiteral("tx.start-rejected"),
        QtWarningMsg,
        mode,
        stateMachine_.state(),
        false,
        result.error);
    return result;
}

SstvTxCoordinatorResult SstvTxCoordinator::acceptedResult() const
{
    return {true,
            SstvTxErrorCode::None,
            {},
            stateMachine_.metrics().currentSessionId};
}

SstvTxCoordinator::BuiltAudio SstvTxCoordinator::buildAudio(
    const SstvTxCoordinatorRequest& request,
    bool voxAudioActivation) const
{
    SstvTxSourceBuilderConfig sourceConfig;
    sourceConfig.mode = request.mode;
    sourceConfig.sampleRate = config_.sampleRate;
    sourceConfig.fskId = request.fskId;
    SstvTxBuiltSource source = SstvTxSourceBuilder::build(
        request.pixels, sourceConfig);

    std::uint64_t voxPreKeyFrames = 0U;
    std::uint64_t voxHangFrames = 0U;
    if (voxAudioActivation && config_.voxEnvelopeEnabled) {
        voxPreKeyFrames = framesForMilliseconds(
            source.sampleRate, config_.voxPreKeyMs);
        voxHangFrames = framesForMilliseconds(
            source.sampleRate, config_.voxHangMs);
        source.source = std::make_unique<VoxEnvelopePcm16Source>(
            std::move(source.source),
            voxPreKeyFrames,
            voxHangFrames,
            config_.voxToneFrequencyHz,
            config_.voxToneLevel);
    }

    const std::uint64_t protocolStartFrame = voxPreKeyFrames;
    const std::uint64_t headerEndFrame = checkedFrameSum(
        protocolStartFrame, source.headerFrames);
    const std::uint64_t imageEndFrame = checkedFrameSum(
        protocolStartFrame, source.imageEndFrame);
    const std::uint64_t protocolEndFrame = checkedFrameSum(
        protocolStartFrame, source.totalFrames);
    const std::uint64_t totalFrames = checkedFrameSum(
        protocolEndFrame, voxHangFrames);
    auto device = std::make_shared<SstvTxAudioDevice>(
        std::move(source.source),
        request.channelCount,
        request.channelRoute);
    if (!device->open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            std::string("SSTV TX audio device open failed: ")
            + device->errorString().toStdString());
    }

    SstvTxAudioPlan plan;
    plan.mode = source.mode;
    plan.sampleRate = source.sampleRate;
    plan.channelCount = request.channelCount;
    plan.channelRoute = request.channelRoute;
    plan.protocolStartFrame = protocolStartFrame;
    plan.headerFrames = headerEndFrame;
    plan.imageEndFrame = imageEndFrame;
    plan.protocolEndFrame = protocolEndFrame;
    plan.totalFrames = totalFrames;
    plan.fskIdFrames = source.fskIdFrames;
    plan.voxPreKeyFrames = voxPreKeyFrames;
    plan.voxHangFrames = voxHangFrames;
    plan.headroom = source.headroom;
    plan.fskIdPlanned = source.fskIdPlanned;
    plan.voxEnvelopeEnabled = voxAudioActivation
        && config_.voxEnvelopeEnabled;
    if (device->totalFrames() != plan.totalFrames
        || plan.protocolStartFrame > plan.headerFrames
        || plan.headerFrames >= plan.imageEndFrame
        || plan.imageEndFrame > plan.protocolEndFrame
        || plan.protocolEndFrame > plan.totalFrames
        || plan.protocolEndFrame - plan.imageEndFrame
            != plan.fskIdFrames) {
        throw std::runtime_error("invalid SSTV TX live audio phase plan");
    }
    return {std::move(device), std::move(plan)};
}

SstvTxCoordinator::BuiltAudio SstvTxCoordinator::buildPreparedAudio(
    SstvTxPreparedAudioRequest& request,
    bool voxAudioActivation) const
{
    if (!request.source) {
        throw std::invalid_argument(
            "prepared SSTV TX source must not be null");
    }
    const std::uint32_t sampleRate = request.source->sampleRate();
    const std::uint64_t protocolFrames = request.source->totalSamples();
    std::unique_ptr<SstvPcm16Source> source = std::move(request.source);

    std::uint64_t voxPreKeyFrames = 0U;
    std::uint64_t voxHangFrames = 0U;
    if (voxAudioActivation && config_.voxEnvelopeEnabled) {
        voxPreKeyFrames = framesForMilliseconds(
            sampleRate, config_.voxPreKeyMs);
        voxHangFrames = framesForMilliseconds(
            sampleRate, config_.voxHangMs);
        source = std::make_unique<VoxEnvelopePcm16Source>(
            std::move(source), voxPreKeyFrames, voxHangFrames,
            config_.voxToneFrequencyHz, config_.voxToneLevel);
    }

    const std::uint64_t protocolStartFrame = voxPreKeyFrames;
    const std::uint64_t headerEndFrame = checkedFrameSum(
        protocolStartFrame, request.headerEndFrame);
    const std::uint64_t imageEndFrame = checkedFrameSum(
        protocolStartFrame, request.imageEndFrame);
    const std::uint64_t protocolEndFrame = checkedFrameSum(
        protocolStartFrame, protocolFrames);
    const std::uint64_t totalFrames = checkedFrameSum(
        protocolEndFrame, voxHangFrames);
    auto device = std::make_shared<SstvTxAudioDevice>(
        std::move(source), request.channelCount, request.channelRoute);
    if (!device->open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            std::string("prepared SSTV TX audio device open failed: ")
            + device->errorString().toStdString());
    }

    SstvTxAudioPlan plan;
    plan.mode = request.mode;
    plan.sampleRate = sampleRate;
    plan.channelCount = request.channelCount;
    plan.channelRoute = request.channelRoute;
    plan.protocolStartFrame = protocolStartFrame;
    plan.headerFrames = headerEndFrame;
    plan.imageEndFrame = imageEndFrame;
    plan.protocolEndFrame = protocolEndFrame;
    plan.totalFrames = totalFrames;
    plan.voxPreKeyFrames = voxPreKeyFrames;
    plan.voxHangFrames = voxHangFrames;
    plan.headroom = request.headroom;
    plan.voxEnvelopeEnabled = voxAudioActivation
        && config_.voxEnvelopeEnabled;
    if (device->totalFrames() != plan.totalFrames
        || plan.protocolStartFrame > plan.headerFrames
        || plan.headerFrames >= plan.imageEndFrame
        || plan.imageEndFrame > plan.protocolEndFrame
        || plan.protocolEndFrame > plan.totalFrames) {
        throw std::runtime_error(
            "invalid prepared SSTV TX live audio phase plan");
    }
    return {std::move(device), std::move(plan)};
}

bool SstvTxCoordinator::validateRequest(
    const SstvTxCoordinatorRequest& request,
    SstvTxErrorCode& error,
    std::string& detail) const
{
    if (modeId(request.mode)[0] == '\0') {
        error = SstvTxErrorCode::UnsupportedMode;
        detail = "unsupported SSTV TX mode";
        return false;
    }
    std::size_t expectedPixels = 0U;
    try {
        expectedPixels = builderPixelCount(request.mode);
    } catch (const std::exception&) {
        error = SstvTxErrorCode::UnsupportedMode;
        detail = "unsupported SSTV TX mode";
        return false;
    }
    if (request.pixels.size() != expectedPixels) {
        error = SstvTxErrorCode::InvalidImage;
        detail = "SSTV TX image pixel count does not match the selected mode";
        return false;
    }
    if (request.channelCount == 0U
        || request.channelCount > SstvTxAudioDevice::MaximumChannelCount
        || !knownChannelRoute(request.channelRoute)
        || (request.channelCount == 1U
            && request.channelRoute != SstvTxChannelRoute::Both)) {
        error = SstvTxErrorCode::AudioDeviceLoss;
        detail = "invalid SSTV TX audio channel configuration";
        return false;
    }
    if (request.fskId.has_value()) {
        SstvFskIdCodec::TextValidation validation;
        switch (request.fskId->textPolicy) {
        case SstvFskIdCodec::TextPolicy::Callsign:
        case SstvFskIdCodec::TextPolicy::PermittedText:
            break;
        default:
            error = SstvTxErrorCode::EncodingFailure;
            detail = "invalid SSTV TX FSK ID text policy";
            return false;
        }
        switch (request.fskId->inputHandling) {
        case SstvFskIdCodec::InputHandling::Strict:
            validation = SstvFskIdCodec::validateText(
                request.fskId->text, request.fskId->textPolicy);
            break;
        case SstvFskIdCodec::InputHandling::Sanitize:
            validation = SstvFskIdCodec::sanitizeText(
                request.fskId->text, request.fskId->textPolicy);
            break;
        default:
            error = SstvTxErrorCode::EncodingFailure;
            detail = "invalid SSTV TX FSK ID input policy";
            return false;
        }
        if (!validation.valid()) {
            error = SstvTxErrorCode::EncodingFailure;
            detail = "invalid SSTV TX FSK ID text";
            return false;
        }
    }
    return true;
}

bool SstvTxCoordinator::validatePreparedRequest(
    const SstvTxPreparedAudioRequest& request,
    SstvTxErrorCode& error,
    std::string& detail) const
{
    if (!request.source) {
        error = SstvTxErrorCode::EncodingFailure;
        detail = "prepared SSTV TX source is missing";
        return false;
    }
    if (request.mode.empty()
        || request.mode.size()
            > config_.stateMachinePolicy.maximumModeCharacters
        || request.mode.find('\0') != std::string::npos) {
        error = SstvTxErrorCode::UnsupportedMode;
        detail = "invalid prepared SSTV TX mode label";
        return false;
    }
    if (request.width == 0U || request.height == 0U
        || request.width
            > config_.stateMachinePolicy.maximumImageDimension
        || request.height
            > config_.stateMachinePolicy.maximumImageDimension) {
        error = SstvTxErrorCode::InvalidImage;
        detail = "invalid prepared SSTV TX logical geometry";
        return false;
    }
    if (request.channelCount == 0U
        || request.channelCount > SstvTxAudioDevice::MaximumChannelCount
        || !knownChannelRoute(request.channelRoute)
        || (request.channelCount == 1U
            && request.channelRoute != SstvTxChannelRoute::Both)) {
        error = SstvTxErrorCode::AudioDeviceLoss;
        detail = "invalid prepared SSTV TX audio channel configuration";
        return false;
    }
    if (!std::isfinite(request.headroom)
        || request.headroom <= 0.0 || request.headroom > 1.0) {
        error = SstvTxErrorCode::EncodingFailure;
        detail = "invalid prepared SSTV TX headroom";
        return false;
    }
    const std::uint64_t totalFrames = request.source->totalSamples();
    if (request.source->sampleRate() != config_.sampleRate
        || totalFrames == 0U
        || totalFrames
            > config_.stateMachinePolicy.maximumEncodedSamples
        || request.source->producedSamples() != 0U
        || request.source->complete() || request.source->cancelled()
        || request.headerEndFrame >= request.imageEndFrame
        || request.imageEndFrame > totalFrames) {
        error = SstvTxErrorCode::EncodingFailure;
        detail = "prepared SSTV TX source or phase boundaries are invalid";
        return false;
    }
    return true;
}

bool SstvTxCoordinator::dispatch(std::uint64_t nowMs,
                                 const SstvTxEvent& event)
{
    const SstvTxTransition transition = stateMachine_.dispatch(nowMs, event);
    publishState();
    if (transition.accepted && transition.stateChanged) {
        if (transition.after == SstvTxState::Completed) {
            recordTxEvent(
                isCalibrationMode(audioPlan_.mode)
                    ? QStringLiteral("tx.calibration-completed")
                    : QStringLiteral("tx.completed"),
                QtInfoMsg,
                audioPlan_.mode,
                transition.after,
                true,
                SstvTxErrorCode::None,
                diagnosticDurationMs(nowMs),
                true);
        } else if (transition.after == SstvTxState::Cancelled) {
            recordTxEvent(
                isCalibrationMode(audioPlan_.mode)
                    ? QStringLiteral("tx.calibration-cancelled")
                    : QStringLiteral("tx.cancelled"),
                QtInfoMsg,
                audioPlan_.mode,
                transition.after,
                true,
                SstvTxErrorCode::None,
                diagnosticDurationMs(nowMs),
                true);
        } else if (transition.after == SstvTxState::Error) {
            recordFailureDiagnostic(nowMs,
                                    stateMachine_.metrics().lastErrorCode);
        }
    }
    return transition.accepted && stateMachine_.invariantsHold();
}

bool SstvTxCoordinator::sessionMatches(std::uint64_t sessionId) noexcept
{
    if (sessionId != 0U && sessionId == activeSessionId_) {
        return true;
    }
    saturatingAdd(metrics_.staleCallbacks);
    return false;
}

bool SstvTxCoordinator::startAudio(std::uint64_t nowMs)
{
    if (audioStartAttempted_
        || !audioDevice_
        || stateMachine_.state() != SstvTxState::WaitingForPtt
        || !stateMachine_.metrics().pttConfirmed) {
        return false;
    }
    if (!dispatch(nowMs, SstvTxLeadElapsed {})) {
        return fail(nowMs,
                    SstvTxErrorCode::InternalFailure,
                    "SSTV TX lead transition failed",
                    SstvTxAudioDetachReason::Error);
    }

    audioStartAttempted_ = true;
    // Conservative until startAudio returns: the hook may have handed the raw
    // QIODevice to SoundOutput before reporting a failure.
    audioAttached_ = true;
    saturatingAdd(metrics_.audioStartAttempts);
    bool started = false;
    try {
        started = hooks_.startAudio(audioDevice_, audioPlan_);
    } catch (const std::exception& exception) {
        saturatingAdd(metrics_.hookFailures);
        lastOperationDetail_ = boundedDetail(
            std::string("SSTV TX SoundOutput start hook failed: ")
            + exception.what());
    } catch (...) {
        saturatingAdd(metrics_.hookFailures);
        lastOperationDetail_ =
            "SSTV TX SoundOutput start hook failed with an unknown exception";
    }
    if (!started) {
        return fail(nowMs,
                    SstvTxErrorCode::AudioDeviceLoss,
                    lastOperationDetail_.empty()
                        ? "SoundOutput rejected the SSTV audio device"
                        : lastOperationDetail_,
                    SstvTxAudioDetachReason::Error);
    }
    lastOperationError_ = SstvTxErrorCode::None;
    lastOperationDetail_.clear();
    publishState();
    return true;
}

bool SstvTxCoordinator::detachAudio(
    std::uint64_t,
    SstvTxAudioDetachReason reason)
{
    if (!audioDevice_) {
        return true;
    }
    if (!audioStartAttempted_) {
        audioDevice_->cancel();
        releaseAudioLease();
        return true;
    }
    if (audioDetachPending_) {
        return false;
    }
    if (audioDetachAttempted_) {
        return !audioDevice_;
    }

    audioDetachAttempted_ = true;
    audioDetachPending_ = true;
    saturatingAdd(metrics_.audioDetachAttempts);
    if (reason != SstvTxAudioDetachReason::Completed) {
        audioDevice_->cancel();
    }

    bool detached = false;
    try {
        detached = hooks_.detachAudio(audioDevice_, reason);
    } catch (...) {
        saturatingAdd(metrics_.hookFailures);
    }
    if (!audioDevice_) {
        return true;
    }
    if (detached) {
        releaseAudioLease();
        publishState();
        return true;
    }

    saturatingAdd(metrics_.hookFailures);
    lastOperationError_ = SstvTxErrorCode::AudioDeviceLoss;
    lastOperationDetail_ =
        "SoundOutput detach is pending; SSTV audio lease retained";
    publishState();
    return false;
}

void SstvTxCoordinator::releaseAudioLease() noexcept
{
    audioAttached_ = false;
    audioDetachPending_ = false;
    if (audioDevice_) {
        lastPcmPeak_ = audioDevice_->peakNormalized();
        lastClippedFrames_ = audioDevice_->clippedFrames();
        audioDevice_->close();
        audioDevice_.reset();
    }
}

void SstvTxCoordinator::requestPttRelease(std::uint64_t nowMs)
{
    if (stateMachine_.state() != SstvTxState::ReleasingPtt) {
        return;
    }
    if (!stateMachine_.releaseRequired()) {
        if (dispatch(nowMs, SstvTxPttReleased {})) {
            pttReleased_ = true;
        }
        return;
    }
    if (pttOffAttempted_
        && (nowMs < pttOffLastAttemptAtMs_
            || nowMs - pttOffLastAttemptAtMs_
                < config_.pttReleaseRetryMs)) {
        return;
    }

    pttOffAttempted_ = true;
    pttOffLastAttemptAtMs_ = nowMs;
    saturatingAdd(metrics_.pttOffAttempts);
    bool requested = false;
    try {
        requested = hooks_.requestPttOff(activeSessionId_);
    } catch (...) {
        saturatingAdd(metrics_.hookFailures);
    }
    if (!requested
        && stateMachine_.state() == SstvTxState::ReleasingPtt) {
        lastOperationError_ = SstvTxErrorCode::PttDispatchFailure;
        lastOperationDetail_ = "SSTV TX PTT-off request was not dispatched";
        static_cast<void>(dispatch(
            nowMs,
            SstvTxFailure {SstvTxErrorCode::PttDispatchFailure,
                           lastOperationDetail_}));
        recordFailureDiagnostic(nowMs, lastOperationError_);
    }
    publishState();
}

bool SstvTxCoordinator::fail(
    std::uint64_t nowMs,
    SstvTxErrorCode error,
    std::string detail,
    SstvTxAudioDetachReason detachReason)
{
    lastOperationError_ = error == SstvTxErrorCode::None
        ? SstvTxErrorCode::InternalFailure : error;
    lastOperationDetail_ = boundedDetail(std::move(detail));
    if (stateMachine_.active()) {
        static_cast<void>(dispatch(
            nowMs,
            SstvTxFailure {lastOperationError_, lastOperationDetail_}));
    }
    recordFailureDiagnostic(nowMs, lastOperationError_);
    static_cast<void>(detachAudio(nowMs, detachReason));
    if (stateMachine_.state() == SstvTxState::ReleasingPtt) {
        requestPttRelease(nowMs);
    } else if (SstvTxStateMachine::isTerminal(stateMachine_.state())
               && !audioDetachPending_) {
        releaseAudioLease();
    }
    publishState();
    return false;
}

void SstvTxCoordinator::recordFailureDiagnostic(
    std::uint64_t nowMs,
    SstvTxErrorCode error) noexcept
{
    if (diagnosticFailureRecorded_) {
        return;
    }
    diagnosticFailureRecorded_ = true;
    const SstvTxErrorCode safeError = error == SstvTxErrorCode::None
        ? SstvTxErrorCode::InternalFailure : error;
    recordTxEvent(
        isCalibrationMode(audioPlan_.mode)
            ? QStringLiteral("tx.calibration-failed")
            : QStringLiteral("tx.failed"),
        QtWarningMsg,
        audioPlan_.mode,
        stateMachine_.state(),
        false,
        safeError,
        diagnosticDurationMs(nowMs),
        true);
}

std::uint64_t SstvTxCoordinator::diagnosticDurationMs(
    std::uint64_t nowMs) const noexcept
{
    return nowMs >= diagnosticSessionStartedAtMs_
        ? nowMs - diagnosticSessionStartedAtMs_ : 0U;
}

void SstvTxCoordinator::clearForNewSession(BuiltAudio audio)
{
    audioDevice_ = std::move(audio.device);
    audioPlan_ = std::move(audio.plan);
    lastOperationError_ = SstvTxErrorCode::None;
    lastOperationDetail_.clear();
    activeSessionId_ = 0U;
    pttConfirmedAtMs_ = 0U;
    pttOffLastAttemptAtMs_ = 0U;
    playedFrames_ = 0U;
    diagnosticSessionStartedAtMs_ = 0U;
    lastPcmPeak_ = 0.0;
    lastClippedFrames_ = 0U;
    pttOnAttempted_ = false;
    pttOffAttempted_ = false;
    pttReleased_ = false;
    audioStartAttempted_ = false;
    audioAttached_ = false;
    audioDetachAttempted_ = false;
    audioDetachPending_ = false;
    playbackComplete_ = false;
    fskIdStarted_ = false;
    fskIdCompleted_ = false;
    diagnosticFailureRecorded_ = false;
}

void SstvTxCoordinator::publishState() noexcept
{
    if (destroying_ || !hooks_.stateChanged) {
        return;
    }
    try {
        hooks_.stateChanged(snapshot());
    } catch (...) {
        saturatingAdd(metrics_.hookFailures);
    }
}

std::string SstvTxCoordinator::boundedDetail(std::string detail) const
{
    if (detail.size()
        > config_.stateMachinePolicy.maximumErrorCharacters) {
        detail.resize(config_.stateMachinePolicy.maximumErrorCharacters);
    }
    return detail;
}

void SstvTxCoordinator::emergencyShutdown() noexcept
{
    destroying_ = true;
    const std::uint64_t nowMs = stateMachine_.metrics().lastEventAtMs;
    try {
        static_cast<void>(shutdown(nowMs));
    } catch (...) {
        if (audioDevice_) {
            audioDevice_->cancel();
        }
        if (stateMachine_.releaseRequired() && !pttOffAttempted_) {
            pttOffAttempted_ = true;
            try {
                static_cast<void>(hooks_.requestPttOff(activeSessionId_));
            } catch (...) {
            }
        }
    }
}

} // namespace decodium::sstv
