// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvAvtSyncCodec.h"

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

// Only the normal AVT variants are executable.  Narrow, QRM and Narrow-QRM
// identities remain catalogue-only until their complete picture semantics
// have independent evidence; SstvAvtSyncCodec represents their header bits
// without promoting them to RX/TX support.
struct SstvAvtModeSpec final
{
    SstvAvtMode mode {SstvAvtMode::Avt24};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    // The bounded QImage/frame raster used by interoperable implementations.
    // AVT90 deliberately keeps its 256-column effective resolution separate
    // from the common 320-column prepared/transmitted raster.
    std::uint32_t width {128U};
    std::uint32_t effectiveSampledWidth {128U};
    std::uint32_t height {120U};
    std::uint8_t visPayload {64U};
    Picoseconds componentDuration;
    Picoseconds lineDuration;
    Picoseconds imageDuration;
};

struct SstvAvtToneSegment final
{
    double frequencyHz {0.0};
    Picoseconds duration;
};

class SstvAvtProtocol final
{
public:
    SstvAvtProtocol() = delete;

    static constexpr double BlackFrequencyHz = 1'500.0;
    static constexpr double WhiteFrequencyHz = 2'300.0;
    static constexpr Picoseconds StandardVisFrameDuration {
        910'000'000'000LL};
    static constexpr Picoseconds TripleVisDuration {2'730'000'000'000LL};
    static constexpr Picoseconds HeaderDuration {8'042'500'000'000LL};
    static constexpr std::size_t StandardVisSegmentCount = 13U;
    static constexpr std::size_t TripleVisSegmentCount =
        StandardVisSegmentCount * 3U;
    static constexpr std::size_t HeaderSegmentCount =
        TripleVisSegmentCount + SstvAvtSyncCodec::CountdownToneCount;

    static SstvAvtModeSpec spec(SstvAvtMode mode);
    // Normal AVT payloads only. Variant payloads intentionally return null.
    static std::optional<SstvAvtMode> normalModeForVis(
        std::uint8_t payload) noexcept;
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequencyHz) noexcept;
    static std::vector<SstvAvtToneSegment> normalHeader(SstvAvtMode mode);
};

enum class SstvAvtRegion : std::uint8_t
{
    Outside,
    Pixel,
    Complete,
};

struct SstvAvtMapperConfig final
{
    SstvAvtMode mode {SstvAvtMode::Avt24};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
};

struct SstvAvtPosition final
{
    SstvAvtRegion region {SstvAvtRegion::Outside};
    std::uint32_t line {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U};

    bool valid() const noexcept
    {
        return region == SstvAvtRegion::Pixel;
    }
};

// AVT has no per-line anchors. RX and TX therefore share one cumulative,
// absolute-time mapper for the complete R-G-B raster. Pixel boundaries are
// fractions of a component, never repeated rounded durations.
class SstvAvtMapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvAvtMapper(SstvAvtMapperConfig config = {});

    SstvAvtMapperConfig config() const noexcept;
    SstvAvtModeSpec modeSpec() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t lineStartSample(std::uint32_t line) const;
    std::uint64_t lineEndSample(std::uint32_t line) const;
    SstvAvtPosition positionAtSample(std::uint64_t imageSample) const;
    SstvAvtPosition positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvAvtPosition positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvAvtPosition makePosition(
        std::uint32_t line,
        ColourComponent component,
        std::uint32_t pixel,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvAvtMapperConfig config_;
    SstvAvtModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvAvtEncoderConfig final
{
    SstvAvtMode mode {SstvAvtMode::Avt24};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

enum class SstvAvtEncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvAvtEncoderPosition final
{
    SstvAvtEncoderStage stage {SstvAvtEncoderStage::Header};
    std::size_t headerSegment {0U};
    SstvAvtPosition image;
    std::uint64_t producedSamples {0U};
    std::uint64_t totalSamples {0U};
    double frequencyHz {0.0};
};

struct SstvAvtEncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    std::size_t residentHeaderBytes {0U};
    SstvToneMetrics tone;
};

class SstvAvtEncoder final
{
public:
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvAvtEncoder(const SstvRgbPixel* pixels,
                   std::size_t count,
                   SstvAvtEncoderConfig config = {});
    explicit SstvAvtEncoder(const std::vector<SstvRgbPixel>& pixels,
                            SstvAvtEncoderConfig config = {});

    static std::size_t pixelCount(SstvAvtMode mode);

    SstvAvtEncoder(const SstvAvtEncoder&) = delete;
    SstvAvtEncoder& operator=(const SstvAvtEncoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);
    SstvAvtMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    std::uint64_t headerSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvAvtEncoderPosition position() const;
    SstvAvtEncoderMetrics metrics() const noexcept;
    void cancel() noexcept;
    void reset() noexcept;

private:
    template<typename Sample>
    std::size_t pull(Sample* output, std::size_t capacity);
    template<typename Sample>
    std::size_t generate(double frequencyHz,
                         Sample* output,
                         std::size_t count);
    static std::vector<std::uint64_t> makeHeaderBoundaries(
        std::uint32_t sampleRate,
        const std::vector<SstvAvtToneSegment>& header);
    std::size_t headerIndexAt(std::uint64_t sample) const noexcept;
    double imageFrequency(const SstvAvtPosition& position) const;
    void noteTransition(const SstvAvtEncoderPosition& position) noexcept;

    SstvAvtEncoderConfig config_;
    SstvAvtModeSpec spec_;
    std::vector<SstvRgbPixel> pixels_;
    SstvAvtMapper mapper_;
    SstvToneGenerator generator_;
    std::vector<SstvAvtToneSegment> header_;
    std::vector<std::uint64_t> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvAvtEncoderMetrics metrics_;
    SstvAvtEncoderPosition lastPosition_;
    bool haveLastSegment_ {false};
};

struct SstvAvtDecoderConfig final
{
    SstvAvtMode mode {SstvAvtMode::Avt24};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {6U};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    // Sparse demodulator centres may leave a small number of wire pixels
    // between observations. Reconstruction is bounded within each component.
    std::uint32_t maximumInterpolationGapPixels {4U};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvAvtDecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvAvtDecoderMetrics final
{
    std::uint64_t observationInputs {0U};
    std::uint64_t acceptedObservations {0U};
    std::uint64_t invalidObservations {0U};
    std::uint64_t observationsBeforeImage {0U};
    std::uint64_t observationsAfterImage {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t componentsPublished {0U};
    std::uint64_t interpolatedPixels {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::size_t bufferedPixelAccumulators {0U};
    std::size_t peakBufferedPixelAccumulators {0U};
};

class SstvAvtDecoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;
    static constexpr std::uint32_t MaximumObservationSpanSamples = 4'096U;
    static constexpr std::uint32_t MaximumInterpolationGapPixels = 32U;

    explicit SstvAvtDecoder(SstvAvtDecoderConfig config = {});

    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvAvtDecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;
    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvAvtMode mode() const noexcept;
    SstvAvtDecodeState state() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvAvtDecoderMetrics metrics() const noexcept;

private:
    struct PixelAccumulator final
    {
        double weightedFrequencyHz {0.0};
        double confidenceWeight {0.0};
        std::uint32_t count {0U};
    };

    static void validateConfig(const SstvAvtDecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static std::size_t componentIndex(ColourComponent component);
    static SstvImageChannel imageChannel(ColourComponent component);
    bool beginLine(std::uint32_t line);
    void accumulate(const SstvAvtPosition& position,
                    double frequencyHz,
                    double confidence) noexcept;
    bool reconstructComponent(std::size_t base,
                              std::vector<std::uint8_t>& values);
    void publishCurrentLine();
    void clearAccumulators() noexcept;
    void refreshBufferMetrics() noexcept;

    SstvAvtDecoderConfig config_;
    SstvAvtModeSpec spec_;
    SstvAvtMapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::vector<PixelAccumulator> accumulators_;
    SstvAvtDecoderMetrics metrics_;
    SstvAvtDecodeState state_ {SstvAvtDecodeState::Receiving};
    std::uint64_t imageEndSample_ {0U};
    std::uint32_t currentLine_ {0U};
    bool haveCurrentLine_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
};

struct SstvAvtCountdownDetectorConfig final
{
    SstvAvtMode expectedMode {SstvAvtMode::Avt24};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t searchStartSample {0U};
    // Width of the frontend analysis window, used to recover the leading
    // edge from its centre timestamp. This is not the observation hop.
    std::uint32_t observationSpanSamples {6U};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    double toneToleranceHz {120.0};
};

enum class SstvAvtCountdownDetectorState : std::uint8_t
{
    Searching,
    Acquired,
    Exhausted,
    Cancelled,
};

struct SstvAvtCountdownDetection final
{
    bool acquired {false};
    SstvAvtMode mode {SstvAvtMode::Avt24};
    std::uint8_t counter {0U};
    std::uint64_t frameStartSample {0U};
    std::uint64_t frameEndSample {0U};
    std::uint64_t imageStartSample {0U};
    double confidence {0.0};
};

struct SstvAvtCountdownDetectorMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t observationInputs {0U};
    std::uint64_t candidatesStarted {0U};
    std::uint64_t framesDecoded {0U};
    std::uint64_t framesRejected {0U};
    std::uint64_t invalidObservations {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::size_t bufferedObservations {0U};
    std::size_t peakBufferedObservations {0U};
};

// Streaming detector for the mandatory 32x17 AVT countdown. It is armed only
// after the runtime has observed the repeated normal VIS identity. One exact
// inverse-protected frame is sufficient, as specified by the AVT protocol.
class SstvAvtCountdownDetector final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    // Only the nearest observation for each protected symbol is retained.
    // This is independent of the frontend hop (Decodium normally uses one
    // observation per sample) and therefore stays fixed for every rate.
    static constexpr std::size_t MaximumBufferedObservations =
        SstvAvtSyncCodec::TonesPerFrame;

    explicit SstvAvtCountdownDetector(
        SstvAvtCountdownDetectorConfig config = {});

    std::optional<SstvAvtCountdownDetection> consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    std::optional<SstvAvtCountdownDetection> consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvAvtCountdownDetectorState finish() noexcept;
    void cancel() noexcept;
    SstvAvtCountdownDetectorState state() const noexcept;
    std::optional<SstvAvtCountdownDetection> detection() const noexcept;
    SstvAvtCountdownDetectorMetrics metrics() const noexcept;

private:
    static void validateConfig(const SstvAvtCountdownDetectorConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    std::uint64_t samplesFor(Picoseconds duration) const;
    bool isStartTone(const SstvFrequencyObservation& observation) const noexcept;
    void startCandidate(const SstvFrequencyObservation& observation);
    void observeCandidate(
        const SstvFrequencyObservation& observation) noexcept;
    std::optional<SstvAvtCountdownDetection> tryDecodeCandidate(
        std::uint64_t observationSample);
    void rejectCandidate() noexcept;

    SstvAvtCountdownDetectorConfig config_;
    SstvAvtCountdownDetectorState state_ {
        SstvAvtCountdownDetectorState::Searching};
    SstvAvtCountdownDetectorMetrics metrics_;
    std::array<std::uint64_t, SstvAvtSyncCodec::TonesPerFrame>
        candidateTargets_ {};
    std::array<std::uint64_t, SstvAvtSyncCodec::TonesPerFrame>
        candidateDistances_ {};
    std::array<double, SstvAvtSyncCodec::TonesPerFrame>
        candidateFrequencies_ {};
    std::array<double, SstvAvtSyncCodec::TonesPerFrame>
        candidateConfidences_ {};
    std::array<bool, SstvAvtSyncCodec::TonesPerFrame>
        candidateSlotsPresent_ {};
    std::optional<SstvAvtCountdownDetection> detection_;
    std::uint64_t candidateStartSample_ {0U};
    std::uint64_t searchEndSample_ {0U};
    std::size_t candidateSlotsFilled_ {0U};
    bool candidateActive_ {false};
    bool previousWasStartTone_ {false};
};

} // namespace decodium::sstv
