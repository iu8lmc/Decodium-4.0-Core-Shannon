// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvTypes.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include "../image/SstvImageFrame.h"
#include "../tx/SstvToneGenerator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace decodium::sstv {

// Wraase SC2 and Pasokon are both line-leading-sync, full-range RGB systems.
// The mode table is the only protocol-specific state; mapper, encoder and
// decoder remain one bounded implementation for the whole family.
enum class SstvSequentialRgbMode : std::uint8_t
{
    WraaseSc2_60,
    WraaseSc2_120,
    WraaseSc2_180,
    PasokonP3,
    PasokonP5,
    PasokonP7,
};

struct SstvSequentialRgbModeSpec final
{
    SstvSequentialRgbMode mode {SstvSequentialRgbMode::WraaseSc2_60};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    const char* family {nullptr};
    const char* compatibilityProfile {nullptr};
    std::uint32_t width {320U};
    std::uint32_t height {256U};
    std::uint32_t effectiveSampledWidth {320U};
    std::uint8_t visPayload {0U};
    Picoseconds syncDuration;
    // Black gaps before R, between R/G, between G/B and after B.
    std::array<Picoseconds, 4U> gapDurations;
    Picoseconds pixelDuration;
    Picoseconds componentDuration;
    Picoseconds lineDuration;
    Picoseconds imageDuration;
};

class SstvSequentialRgbProtocol final
{
public:
    SstvSequentialRgbProtocol() = delete;

    static constexpr double SyncFrequencyHz = 1'200.0;
    static constexpr double GapFrequencyHz = 1'500.0;
    static constexpr double BlackFrequencyHz = 1'500.0;
    static constexpr double WhiteFrequencyHz = 2'300.0;
    static constexpr Picoseconds HeaderDuration {910'000'000'000LL};

    static SstvSequentialRgbModeSpec spec(SstvSequentialRgbMode mode);
    static std::optional<SstvSequentialRgbMode> modeForVis(
        std::uint8_t visPayload) noexcept;
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequencyHz) noexcept;
};

enum class SstvSequentialRgbRegion : std::uint8_t
{
    Outside,
    Sync,
    Gap,
    Pixel,
    Complete,
};

struct SstvSequentialRgbMapperConfig final
{
    SstvSequentialRgbMode mode {SstvSequentialRgbMode::WraaseSc2_60};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
};

struct SstvSequentialRgbPosition final
{
    SstvSequentialRgbRegion region {SstvSequentialRgbRegion::Outside};
    std::uint32_t line {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U};

    bool valid() const noexcept
    {
        return region != SstvSequentialRgbRegion::Outside
            && region != SstvSequentialRgbRegion::Complete;
    }
};

// Immutable cumulative-time mapper.  Pixel boundaries are fractions of the
// complete component duration, never repeated rounded pixel durations.  This
// is important for the 1/2400 s Pasokon P7 time unit.
class SstvSequentialRgbMapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvSequentialRgbMapper(
        SstvSequentialRgbMapperConfig config = {});

    SstvSequentialRgbMapperConfig config() const noexcept;
    SstvSequentialRgbModeSpec modeSpec() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t lineStartSample(std::uint32_t line) const;
    std::uint64_t lineEndSample(std::uint32_t line) const;
    SstvSequentialRgbPosition positionAtSample(
        std::uint64_t imageSample) const;
    SstvSequentialRgbPosition positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvSequentialRgbPosition positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvSequentialRgbPosition makePosition(
        SstvSequentialRgbRegion region,
        std::uint32_t line,
        ColourComponent component,
        std::uint32_t pixel,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvSequentialRgbMapperConfig config_;
    SstvSequentialRgbModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvSequentialRgbEncoderConfig final
{
    SstvSequentialRgbMode mode {SstvSequentialRgbMode::WraaseSc2_60};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

enum class SstvSequentialRgbEncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvSequentialRgbEncoderPosition final
{
    SstvSequentialRgbEncoderStage stage {
        SstvSequentialRgbEncoderStage::Header};
    std::size_t headerSegment {0U};
    SstvSequentialRgbPosition image;
    std::uint64_t producedSamples {0U};
    std::uint64_t totalSamples {0U};
    double frequencyHz {0.0};
};

struct SstvSequentialRgbEncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    SstvToneMetrics tone;
};

class SstvSequentialRgbEncoder final
{
public:
    static constexpr std::size_t HeaderSegmentCount = 13U;
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvSequentialRgbEncoder(
        const SstvRgbPixel* pixels,
        std::size_t count,
        SstvSequentialRgbEncoderConfig config = {});
    explicit SstvSequentialRgbEncoder(
        const std::vector<SstvRgbPixel>& pixels,
        SstvSequentialRgbEncoderConfig config = {});

    static std::size_t pixelCount(SstvSequentialRgbMode mode);

    SstvSequentialRgbEncoder(const SstvSequentialRgbEncoder&) = delete;
    SstvSequentialRgbEncoder& operator=(const SstvSequentialRgbEncoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);
    SstvSequentialRgbMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvSequentialRgbEncoderPosition position() const;
    SstvSequentialRgbEncoderMetrics metrics() const noexcept;
    void cancel() noexcept;
    void reset() noexcept;

private:
    struct HeaderSegment final
    {
        double frequencyHz {0.0};
        Picoseconds duration;
    };

    template<typename Sample>
    std::size_t pull(Sample* output, std::size_t capacity);
    template<typename Sample>
    std::size_t generate(double frequencyHz,
                         Sample* output,
                         std::size_t count);
    static std::array<HeaderSegment, HeaderSegmentCount> makeHeader(
        std::uint8_t visPayload);
    static std::array<std::uint64_t, HeaderSegmentCount + 1U>
    makeHeaderBoundaries(
        std::uint32_t sampleRate,
        const std::array<HeaderSegment, HeaderSegmentCount>& header);
    std::size_t headerIndexAt(std::uint64_t sample) const noexcept;
    double imageFrequency(const SstvSequentialRgbPosition& position) const;
    void noteTransition(const SstvSequentialRgbEncoderPosition& position) noexcept;

    SstvSequentialRgbEncoderConfig config_;
    SstvSequentialRgbModeSpec spec_;
    std::vector<SstvRgbPixel> pixels_;
    SstvSequentialRgbMapper mapper_;
    SstvToneGenerator generator_;
    std::array<HeaderSegment, HeaderSegmentCount> header_;
    std::array<std::uint64_t, HeaderSegmentCount + 1U> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvSequentialRgbEncoderMetrics metrics_;
    SstvSequentialRgbEncoderPosition lastPosition_;
    bool haveLastSegment_ {false};
};

struct SstvSequentialRgbDecoderConfig final
{
    SstvSequentialRgbMode mode {SstvSequentialRgbMode::WraaseSc2_60};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    std::uint64_t imageStartSample {0U};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvSequentialRgbDecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvSequentialRgbDecoderMetrics final
{
    std::uint64_t observationInputs {0U};
    std::uint64_t acceptedObservations {0U};
    std::uint64_t invalidObservations {0U};
    std::uint64_t observationsBeforeImage {0U};
    std::uint64_t observationsAfterImage {0U};
    std::uint64_t nonPixelObservations {0U};
    std::uint64_t observedLineSyncs {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t componentsPublished {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::size_t bufferedPixelAccumulators {0U};
    std::size_t peakBufferedPixelAccumulators {0U};
};

// Bounded chronological decoder.  VIS supplies the absolute first-line sync
// anchor; the cumulative mapper then decodes one fixed RGB scanline at a time.
// It intentionally makes no unverified timing-only mode claim.
class SstvSequentialRgbDecoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;

    explicit SstvSequentialRgbDecoder(
        SstvSequentialRgbDecoderConfig config = {});

    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvSequentialRgbDecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;
    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvSequentialRgbMode mode() const noexcept;
    SstvSequentialRgbDecodeState state() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvSequentialRgbDecoderMetrics metrics() const noexcept;

private:
    struct PixelAccumulator final
    {
        double weightedFrequencyHz {0.0};
        double confidenceWeight {0.0};
        std::uint32_t count {0U};
    };

    static void validateConfig(const SstvSequentialRgbDecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static std::size_t componentIndex(ColourComponent component);
    static SstvImageChannel imageChannel(ColourComponent component);
    bool beginLine(std::uint32_t line);
    void accumulate(const SstvSequentialRgbPosition& position,
                    double frequencyHz,
                    double confidence) noexcept;
    void publishCurrentLine();
    void clearAccumulators() noexcept;
    void refreshBufferMetrics() noexcept;

    SstvSequentialRgbDecoderConfig config_;
    SstvSequentialRgbModeSpec spec_;
    SstvSequentialRgbMapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::vector<PixelAccumulator> accumulators_;
    SstvSequentialRgbDecoderMetrics metrics_;
    SstvSequentialRgbDecodeState state_ {
        SstvSequentialRgbDecodeState::Receiving};
    std::uint64_t imageEndSample_ {0U};
    std::uint32_t currentLine_ {0U};
    bool haveCurrentLine_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
    std::uint32_t lastObservedSyncLine_ {0U};
    bool haveObservedSyncLine_ {false};
};

} // namespace decodium::sstv
