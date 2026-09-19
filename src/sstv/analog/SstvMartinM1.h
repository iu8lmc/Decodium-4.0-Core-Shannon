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

enum class SstvMartinMode : std::uint8_t
{
    M1,
    M2,
    M3,
    M4,
};

struct SstvMartinModeSpec final
{
    SstvMartinMode mode {SstvMartinMode::M1};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    std::uint32_t width {320U};
    std::uint32_t height {256U};
    std::uint32_t effectiveSampledWidth {320U};
    std::uint8_t visPayload {44U};
    Picoseconds syncDuration;
    Picoseconds porchDuration;
    Picoseconds separatorDuration;
    Picoseconds pixelDuration;
    Picoseconds componentDuration;
    Picoseconds lineDuration;
    Picoseconds imageDuration;
};

// Normative Martin wire values.  The legacy class name and its public static
// constants remain M1-compatible; spec() selects M1/M2/M3/M4 without cloning
// the bounded mapper/codec.  Timings are integer picoseconds.  Every scanline
// is sync, porch, G, separator, B, separator, R, trailing separator.
class SstvMartinM1Protocol final
{
public:
    SstvMartinM1Protocol() = delete;

    static constexpr std::uint32_t Width = 320U;
    static constexpr std::uint32_t Height = 256U;
    static constexpr std::uint8_t VisCode = 44U;

    static constexpr double SyncFrequencyHz = 1'200.0;
    static constexpr double SeparatorFrequencyHz = 1'500.0;
    static constexpr double BlackFrequencyHz = 1'500.0;
    static constexpr double WhiteFrequencyHz = 2'300.0;

    static constexpr Picoseconds SyncDuration {4'862'000'000LL};
    static constexpr Picoseconds PorchDuration {572'000'000LL};
    static constexpr Picoseconds SeparatorDuration {572'000'000LL};
    static constexpr Picoseconds PixelDuration {457'600'000LL};
    static constexpr Picoseconds ComponentDuration {146'432'000'000LL};
    static constexpr Picoseconds LineDuration {446'446'000'000LL};
    static constexpr Picoseconds ImageDuration {114'290'176'000'000LL};
    static constexpr Picoseconds HeaderDuration {910'000'000'000LL};

    static SstvMartinModeSpec spec(SstvMartinMode mode);
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequencyHz) noexcept;
};

enum class SstvMartinM1Region : std::uint8_t
{
    Outside,
    Sync,
    Porch,
    Pixel,
    Separator,
    Complete,
};

struct SstvMartinM1MapperConfig final
{
    std::uint32_t sampleRate {12'000U};

    // Positive values lengthen the transmitted scan clock.  The common
    // +/-300 ppm correction range is fully represented; the wider hard limit
    // permits deliberate reacquisition without unbounded arithmetic.
    std::int32_t clockErrorPpm {0};
    SstvMartinMode mode {SstvMartinMode::M1};
};

struct SstvMartinM1Position final
{
    SstvMartinM1Region region {SstvMartinM1Region::Outside};
    std::uint32_t line {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U}; // exclusive

    bool valid() const noexcept
    {
        return region != SstvMartinM1Region::Outside
            && region != SstvMartinM1Region::Complete;
    }
};

// Pure, immutable layout mapper.  Every query has logarithmic, hard-bounded
// work and allocates no memory.  Sample boundaries are cumulative rational
// floors, so fractional pixels and ppm scaling cannot accumulate per-segment
// rounding error.
class SstvMartinM1Mapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvMartinM1Mapper(SstvMartinM1MapperConfig config = {});

    SstvMartinM1MapperConfig config() const noexcept;
    SstvMartinModeSpec modeSpec() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t lineStartSample(std::uint32_t line) const;
    std::uint64_t lineEndSample(std::uint32_t line) const;

    SstvMartinM1Position positionAtSample(std::uint64_t imageSample) const;
    SstvMartinM1Position positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvMartinM1Position positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvMartinM1Position pixelPositionAtSample(
        std::uint32_t line,
        ColourComponent component,
        std::uint64_t componentStartPicoseconds,
        std::uint64_t sample) const;
    SstvMartinM1Position makeNonPixelPosition(
        SstvMartinM1Region region,
        std::uint32_t line,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvMartinM1MapperConfig config_;
    SstvMartinModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvMartinM1EncoderConfig final
{
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
    SstvMartinMode mode {SstvMartinMode::M1};
};

enum class SstvMartinM1EncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvMartinM1EncoderPosition final
{
    SstvMartinM1EncoderStage stage {SstvMartinM1EncoderStage::Header};
    std::size_t headerSegment {0U};
    SstvMartinM1Position image;
    std::uint64_t producedSamples {0U};
    std::uint64_t totalSamples {0U};
    double frequencyHz {0.0};
};

struct SstvMartinM1EncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    SstvToneMetrics tone;
};

// Pull-oriented Martin-family encoder.  It owns exactly one RGB8 frame, thirteen
// short header entries, a mapper and fixed DDS state.  Pixel tones are looked
// up on demand and are never expanded into a 245,760-entry segment plan.
// Rendering/reset are single-owner; cancel() is safe from another thread.
class SstvMartinM1Encoder final
{
public:
    // Kept as the maximum/full-height Martin frame size for source
    // compatibility with M1 callers.  M3/M4 use pixelCount(mode).
    static constexpr std::size_t PixelCount =
        static_cast<std::size_t>(SstvMartinM1Protocol::Width)
        * SstvMartinM1Protocol::Height;
    static constexpr std::size_t HeaderSegmentCount = 13U;
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvMartinM1Encoder(
        const SstvRgbPixel* pixels,
        std::size_t count,
        SstvMartinM1EncoderConfig config = {});
    explicit SstvMartinM1Encoder(
        const std::vector<SstvRgbPixel>& pixels,
        SstvMartinM1EncoderConfig config = {});

    static std::size_t pixelCount(SstvMartinMode mode);

    SstvMartinM1Encoder(const SstvMartinM1Encoder&) = delete;
    SstvMartinM1Encoder& operator=(const SstvMartinM1Encoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);

    SstvMartinMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvMartinM1EncoderPosition position() const;
    SstvMartinM1EncoderMetrics metrics() const noexcept;

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
    makeHeaderBoundaries(std::uint32_t sampleRate,
                         const std::array<HeaderSegment,
                                          HeaderSegmentCount>& header);
    std::size_t headerIndexAt(std::uint64_t sample) const noexcept;
    double imageFrequency(const SstvMartinM1Position& position) const;
    void noteTransition(SstvMartinM1EncoderStage stage,
                        std::size_t headerIndex,
                        const SstvMartinM1Position& imagePosition) noexcept;

    SstvMartinM1EncoderConfig config_;
    SstvMartinModeSpec spec_;
    std::vector<SstvRgbPixel> pixels_;
    SstvMartinM1Mapper mapper_;
    SstvToneGenerator generator_;
    std::array<HeaderSegment, HeaderSegmentCount> header_;
    std::array<std::uint64_t, HeaderSegmentCount + 1U> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvMartinM1EncoderMetrics metrics_;
    SstvMartinM1EncoderStage lastStage_ {SstvMartinM1EncoderStage::Header};
    std::size_t lastHeaderIndex_ {0U};
    SstvMartinM1Region lastImageRegion_ {SstvMartinM1Region::Outside};
    std::uint32_t lastLine_ {0U};
    ColourComponent lastComponent_ {ColourComponent::ModeSpecific};
    std::uint32_t lastPixel_ {0U};
    bool haveLastSegment_ {false};
};

struct SstvMartinM1LineSync final
{
    std::uint32_t lineIndex {0U};
    std::uint64_t syncStartSample {0U};
    double confidence {0.0};
    bool predicted {false};
};

struct SstvMartinM1DecoderConfig final
{
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};

    // This is an explicit receiver calibration.  It is subtracted from
    // correctedFrequencyHz; no hidden AFC inference occurs in the decoder.
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
    SstvMartinMode mode {SstvMartinMode::M1};
};

enum class SstvMartinM1DecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvMartinM1DecoderMetrics final
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

// Martin-family image assembler.  A controller supplies observed or predicted
// line-sync anchors from SstvSyncTracker and then chronological frequency
// observations.  The decoder keeps one fixed scanline of accumulators and a
// fixed maximum-size 256-entry anchor table; M3/M4 expose only 128 rows.  It
// does not retain audio or arbitrary history.
// State-changing methods are single-owner.  imageFrame()/snapshot() use the
// stable, thread-safe SstvImageFrame surface; reset never replaces that object.
class SstvMartinM1Decoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr std::size_t MaximumSyncsPerConsume = 512U;
    static constexpr std::size_t MaximumBufferedPixelAccumulators =
        static_cast<std::size_t>(SstvMartinM1Protocol::Width) * 3U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;

    explicit SstvMartinM1Decoder(SstvMartinM1DecoderConfig config = {});

    std::size_t consumeLineSyncs(const SstvMartinM1LineSync* syncs,
                                 std::size_t count);
    std::size_t consumeLineSyncs(
        const std::vector<SstvMartinM1LineSync>& syncs);
    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);

    SstvMartinM1DecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;

    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvMartinMode mode() const noexcept;
    SstvMartinM1DecodeState state() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvMartinM1DecoderMetrics metrics() const noexcept;

private:
    struct Anchor final
    {
        std::uint64_t startSample {0U};
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

    static void validateConfig(const SstvMartinM1DecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static std::size_t accumulatorIndex(ColourComponent component,
                                        std::uint32_t pixel);
    static SstvImageChannel imageChannel(ColourComponent component);

    bool acceptSync(const SstvMartinM1LineSync& sync);
    const Anchor* anchorFor(std::uint64_t sample,
                            std::uint32_t& line) noexcept;
    bool beginLine(std::uint32_t line);
    void accumulate(const SstvMartinM1Position& position,
                    double frequencyHz,
                    double confidence) noexcept;
    void publishCurrentLine();
    void clearLineAccumulators() noexcept;
    void refreshBufferMetrics() noexcept;

    SstvMartinM1DecoderConfig config_;
    SstvMartinModeSpec spec_;
    SstvMartinM1Mapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::array<Anchor, SstvMartinM1Protocol::Height> anchors_;
    std::array<PixelAccumulator, MaximumBufferedPixelAccumulators>
        accumulators_;
    SstvMartinM1DecoderMetrics metrics_;
    SstvMartinM1DecodeState state_ {SstvMartinM1DecodeState::Receiving};
    std::uint32_t currentLine_ {0U};
    bool haveCurrentLine_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
    std::uint32_t highestStoredAnchorLine_ {0U};
    std::uint32_t anchorCursorLine_ {0U};
    bool haveAnchorCursor_ {false};
};

} // namespace decodium::sstv
