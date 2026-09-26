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
#include <vector>

namespace decodium::sstv {

enum class SstvScottieMode : std::uint8_t
{
    S1,
    S2,
    S3,
    S4,
    DX,
};

struct SstvScottieModeSpec final
{
    SstvScottieMode mode {SstvScottieMode::S1};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    std::uint32_t width {320U};
    std::uint32_t height {256U};
    std::uint8_t visPayload {0U};
    Picoseconds syncDuration;
    Picoseconds porchDuration;
    Picoseconds pixelDuration;
    Picoseconds componentDuration;
    Picoseconds embeddedSyncOffset;
    Picoseconds lineDuration;
    Picoseconds imageDuration;
};

// Clean-room Scottie wire constants.  The logical row begins with the green
// porch and is G, B, embedded sync, R.  In particular, the first image row
// starts immediately after the standard VIS stop symbol; it does not acquire
// a second, invented leading sync.  The embedded sync anchor for a row is
// therefore later than that row's green and blue samples.
class SstvScottieProtocol final
{
public:
    SstvScottieProtocol() = delete;

    static constexpr std::uint32_t Width = 320U;
    static constexpr std::uint32_t Height = 256U;
    static constexpr std::uint32_t HalfHeight = 128U;
    static constexpr double SyncFrequencyHz = 1'200.0;
    static constexpr double PorchFrequencyHz = 1'500.0;
    static constexpr double BlackFrequencyHz = 1'500.0;
    static constexpr double WhiteFrequencyHz = 2'300.0;
    static constexpr Picoseconds HeaderDuration {910'000'000'000LL};
    static constexpr Picoseconds SyncDuration {9'000'000'000LL};
    static constexpr Picoseconds PorchDuration {1'500'000'000LL};

    static SstvScottieModeSpec spec(SstvScottieMode mode);
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequencyHz) noexcept;
};

enum class SstvScottieRegion : std::uint8_t
{
    Outside,
    Porch,
    Pixel,
    Sync,
    Complete,
};

struct SstvScottieMapperConfig final
{
    SstvScottieMode mode {SstvScottieMode::S1};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
};

struct SstvScottiePosition final
{
    SstvScottieRegion region {SstvScottieRegion::Outside};
    std::uint32_t line {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U}; // exclusive

    bool valid() const noexcept
    {
        return region != SstvScottieRegion::Outside
            && region != SstvScottieRegion::Complete;
    }
};

// Immutable cumulative-time mapper.  Every sample boundary is the floor of
// the full elapsed rational duration, including explicit ppm scaling.  It
// never rounds each pixel independently and has bounded logarithmic lookup.
class SstvScottieMapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvScottieMapper(SstvScottieMapperConfig config = {});

    SstvScottieMapperConfig config() const noexcept;
    SstvScottieModeSpec modeSpec() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t lineStartSample(std::uint32_t line) const;
    std::uint64_t lineEndSample(std::uint32_t line) const;
    std::uint64_t embeddedSyncStartSample(std::uint32_t line) const;

    SstvScottiePosition positionAtSample(std::uint64_t imageSample) const;
    SstvScottiePosition positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvScottiePosition positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvScottiePosition pixelPositionAtSample(
        std::uint32_t line,
        ColourComponent component,
        std::uint64_t componentStartPicoseconds,
        std::uint64_t sample) const;
    SstvScottiePosition makeNonPixelPosition(
        SstvScottieRegion region,
        std::uint32_t line,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvScottieMapperConfig config_;
    SstvScottieModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvScottieEncoderConfig final
{
    SstvScottieMode mode {SstvScottieMode::S1};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

enum class SstvScottieEncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvScottieEncoderPosition final
{
    SstvScottieEncoderStage stage {SstvScottieEncoderStage::Header};
    std::size_t headerSegment {0U};
    SstvScottiePosition image;
    std::uint64_t producedSamples {0U};
    std::uint64_t totalSamples {0U};
    double frequencyHz {0.0};
};

struct SstvScottieEncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    SstvToneMetrics tone;
};

// Pull encoder with one resident RGB8 frame and fixed DDS/layout state.  It
// never materialises the roughly 246k pixel-tone segments.  Rendering/reset
// are single-owner operations; cancel() is the one concurrent operation.
class SstvScottieEncoder final
{
public:
    // Kept as the maximum/full-height Scottie frame size for source
    // compatibility with S1/S2/DX callers.  S3/S4 use pixelCount(mode).
    static constexpr std::size_t PixelCount =
        static_cast<std::size_t>(SstvScottieProtocol::Width)
        * SstvScottieProtocol::Height;
    static constexpr std::size_t HeaderSegmentCount = 13U;
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvScottieEncoder(
        const SstvRgbPixel* pixels,
        std::size_t count,
        SstvScottieEncoderConfig config = {});
    explicit SstvScottieEncoder(
        const std::vector<SstvRgbPixel>& pixels,
        SstvScottieEncoderConfig config = {});

    static std::size_t pixelCount(SstvScottieMode mode);

    SstvScottieEncoder(const SstvScottieEncoder&) = delete;
    SstvScottieEncoder& operator=(const SstvScottieEncoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);

    SstvScottieMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvScottieEncoderPosition position() const;
    SstvScottieEncoderMetrics metrics() const noexcept;
    double phaseTurns() const noexcept;

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
    double imageFrequency(const SstvScottiePosition& position) const;
    void noteTransition(SstvScottieEncoderStage stage,
                        std::size_t headerIndex,
                        const SstvScottiePosition& imagePosition) noexcept;

    SstvScottieEncoderConfig config_;
    SstvScottieModeSpec spec_;
    std::vector<SstvRgbPixel> pixels_;
    SstvScottieMapper mapper_;
    SstvToneGenerator generator_;
    std::array<HeaderSegment, HeaderSegmentCount> header_;
    std::array<std::uint64_t, HeaderSegmentCount + 1U> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvScottieEncoderMetrics metrics_;
    SstvScottieEncoderStage lastStage_ {SstvScottieEncoderStage::Header};
    std::size_t lastHeaderIndex_ {0U};
    SstvScottieRegion lastImageRegion_ {SstvScottieRegion::Outside};
    std::uint32_t lastLine_ {0U};
    ColourComponent lastComponent_ {ColourComponent::ModeSpecific};
    std::uint32_t lastPixel_ {0U};
    bool haveLastSegment_ {false};
};

// syncStartSample is the start of the row's embedded 1200 Hz sync (between B
// and R), not a conventional line-leading sync.  To recover the first G/B
// pair, a controller may install the anchor then replay retained observations.
struct SstvScottieLineSync final
{
    std::uint32_t lineIndex {0U};
    std::uint64_t syncStartSample {0U};
    double confidence {0.0};
    bool predicted {false};
};

struct SstvScottieDecoderConfig final
{
    SstvScottieMode mode {SstvScottieMode::S1};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvScottieDecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvScottieDecoderMetrics final
{
    std::uint64_t syncInputs {0U};
    std::uint64_t observedSyncs {0U};
    std::uint64_t predictedSyncs {0U};
    std::uint64_t rejectedSyncs {0U};
    std::uint64_t observationInputs {0U};
    std::uint64_t acceptedObservations {0U};
    std::uint64_t invalidObservations {0U};
    std::uint64_t unanchoredObservations {0U};
    std::uint64_t nonPixelObservations {0U};
    std::uint64_t outOfLineObservations {0U};
    std::uint64_t staleObservations {0U};
    std::uint64_t droppedObservationsAfterEnd {0U};
    std::uint64_t droppedSyncsAfterEnd {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t componentsPublished {0U};
    std::uint64_t numericFaults {0U};
    std::size_t bufferedPixelAccumulators {0U};
    std::size_t peakBufferedPixelAccumulators {0U};
    std::size_t storedSyncAnchors {0U};
};

// Bounded Scottie row assembler.  It keeps one row of frequency accumulators
// and at most 256 fixed anchor slots, never audio or an unbounded observation
// history.
// The stable SstvImageFrame supplies coherent progressive snapshots and a
// bounded dirty-event queue.  Mutating calls are single-owner.
class SstvScottieDecoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr std::size_t MaximumSyncsPerConsume = 512U;
    static constexpr std::size_t MaximumBufferedPixelAccumulators =
        static_cast<std::size_t>(SstvScottieProtocol::Width) * 3U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;

    explicit SstvScottieDecoder(SstvScottieDecoderConfig config = {});

    std::size_t consumeLineSyncs(const SstvScottieLineSync* syncs,
                                 std::size_t count);
    std::size_t consumeLineSyncs(
        const std::vector<SstvScottieLineSync>& syncs);
    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);

    SstvScottieDecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;

    SstvScottieMode mode() const noexcept;
    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvScottieDecodeState state() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvScottieDecoderMetrics metrics() const noexcept;

private:
    struct Anchor final
    {
        std::uint64_t syncStartSample {0U};
        std::uint64_t lineStartSample {0U};
        std::uint64_t lineEndSample {0U};
        double confidence {0.0};
        bool present {false};
        bool predicted {false};
    };

    struct PixelAccumulator final
    {
        double meanFrequencyHz {0.0};
        double meanConfidence {0.0};
        std::uint32_t count {0U};
    };

    static void validateConfig(const SstvScottieDecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static std::size_t accumulatorIndex(ColourComponent component,
                                        std::uint32_t pixel);
    static SstvImageChannel imageChannel(ColourComponent component);

    bool acceptSync(const SstvScottieLineSync& sync);
    const Anchor* anchorFor(std::uint64_t sample,
                            std::uint32_t& line) const noexcept;
    bool beginLine(std::uint32_t line);
    void accumulate(const SstvScottiePosition& position,
                    double frequencyHz,
                    double confidence) noexcept;
    void publishCurrentLine();
    void clearLineAccumulators() noexcept;
    void refreshBufferMetrics() noexcept;

    SstvScottieDecoderConfig config_;
    SstvScottieModeSpec spec_;
    SstvScottieMapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::array<Anchor, SstvScottieProtocol::Height> anchors_;
    std::array<PixelAccumulator, MaximumBufferedPixelAccumulators>
        accumulators_;
    SstvScottieDecoderMetrics metrics_;
    SstvScottieDecodeState state_ {SstvScottieDecodeState::Receiving};
    std::uint32_t currentLine_ {0U};
    bool haveCurrentLine_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
    std::uint32_t highestStoredAnchorLine_ {0U};
};

} // namespace decodium::sstv
