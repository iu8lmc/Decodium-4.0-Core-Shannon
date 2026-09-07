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

enum class SstvRobotMode : std::uint8_t
{
    Colour12,
    Colour24,
    Colour36,
    Colour72,
    Bw8,
    Bw12,
    Bw24,
    Bw36,
};

struct SstvRobotModeSpec final
{
    SstvRobotMode mode {SstvRobotMode::Colour36};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    std::uint32_t width {320U};
    std::uint32_t height {240U};
    std::uint32_t chromaWidth {160U};
    std::uint8_t visPayload {8U};
    std::array<std::uint8_t, 2U> visAliases {{0U, 0U}};
    std::uint8_t visAliasCount {0U};
    bool colour {true};
    ChromaSubsampling chromaSubsampling {ChromaSubsampling::Cs420};
    Picoseconds syncDuration;
    Picoseconds markerDuration;
    Picoseconds luminanceDuration;
    Picoseconds chromaDuration;
    Picoseconds luminancePixelDuration;
    Picoseconds chromaPixelDuration;
    Picoseconds lineDuration;
    Picoseconds imageDuration;
};

// Canonical Robot wire values from SSTV Handbook table 4.1/table 4.3 and
// chapter 5.  Colour modes are Y, then either alternating Cr/Cb (4:2:0) or
// Cr then Cb (4:2:2).  A chroma marker spends its first two thirds at 1500 Hz
// for Cr or 2300 Hz for Cb and its last third at 1900 Hz.  Chroma samples are
// represented at their effective half horizontal resolution instead of as a
// faster duplicate full-width raster.
class SstvRobotProtocol final
{
public:
    SstvRobotProtocol() = delete;

    static constexpr std::uint32_t MaximumWidth = 320U;
    static constexpr std::uint32_t MaximumHeight = 240U;
    static constexpr double SyncFrequencyHz = 1'200.0;
    static constexpr double CrMarkerFrequencyHz = 1'500.0;
    static constexpr double CbMarkerFrequencyHz = 2'300.0;
    static constexpr double MarkerPorchFrequencyHz = 1'900.0;
    static constexpr double BlackFrequencyHz = 1'500.0;
    static constexpr double WhiteFrequencyHz = 2'300.0;
    static constexpr Picoseconds HeaderDuration {910'000'000'000LL};

    static SstvRobotModeSpec spec(SstvRobotMode mode);
    static std::optional<SstvRobotMode> modeForVis(
        std::uint8_t visPayload) noexcept;
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequencyHz) noexcept;
};

enum class SstvRobotRegion : std::uint8_t
{
    Outside,
    Sync,
    Pixel,
    ChromaMarker,
    MarkerPorch,
    Complete,
};

struct SstvRobotMapperConfig final
{
    SstvRobotMode mode {SstvRobotMode::Colour36};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
};

struct SstvRobotPosition final
{
    SstvRobotRegion region {SstvRobotRegion::Outside};
    std::uint32_t line {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U};

    bool valid() const noexcept
    {
        return region != SstvRobotRegion::Outside
            && region != SstvRobotRegion::Complete;
    }
};

class SstvRobotMapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvRobotMapper(SstvRobotMapperConfig config = {});

    SstvRobotMapperConfig config() const noexcept;
    SstvRobotModeSpec modeSpec() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t lineStartSample(std::uint32_t line) const;
    std::uint64_t lineEndSample(std::uint32_t line) const;
    SstvRobotPosition positionAtSample(std::uint64_t imageSample) const;
    SstvRobotPosition positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvRobotPosition positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvRobotPosition pixelPositionAtSample(
        std::uint32_t line,
        ColourComponent component,
        std::uint64_t componentStartPicoseconds,
        std::uint32_t pixelCount,
        Picoseconds pixelDuration,
        std::uint64_t sample) const;
    SstvRobotPosition makeNonPixelPosition(
        SstvRobotRegion region,
        std::uint32_t line,
        ColourComponent component,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvRobotMapperConfig config_;
    SstvRobotModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvRobotEncoderConfig final
{
    SstvRobotMode mode {SstvRobotMode::Colour36};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

enum class SstvRobotEncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvRobotEncoderPosition final
{
    SstvRobotEncoderStage stage {SstvRobotEncoderStage::Header};
    std::size_t headerSegment {0U};
    SstvRobotPosition image;
    std::uint64_t producedSamples {0U};
    std::uint64_t totalSamples {0U};
    double frequencyHz {0.0};
};

struct SstvRobotEncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    SstvToneMetrics tone;
};

class SstvRobotEncoder final
{
public:
    static constexpr std::size_t HeaderSegmentCount = 13U;
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvRobotEncoder(const SstvRgbPixel* pixels,
                     std::size_t count,
                     SstvRobotEncoderConfig config = {});
    explicit SstvRobotEncoder(const std::vector<SstvRgbPixel>& pixels,
                              SstvRobotEncoderConfig config = {});

    static std::size_t pixelCount(SstvRobotMode mode);

    SstvRobotEncoder(const SstvRobotEncoder&) = delete;
    SstvRobotEncoder& operator=(const SstvRobotEncoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);

    SstvRobotMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvRobotEncoderPosition position() const;
    SstvRobotEncoderMetrics metrics() const noexcept;
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
    double imageFrequency(const SstvRobotPosition& position) const;
    std::uint8_t chromaValue(const SstvRobotPosition& position) const;
    void noteTransition(SstvRobotEncoderStage stage,
                        std::size_t headerIndex,
                        const SstvRobotPosition& imagePosition) noexcept;

    SstvRobotEncoderConfig config_;
    SstvRobotModeSpec spec_;
    std::vector<SstvRgbPixel> pixels_;
    SstvRobotMapper mapper_;
    SstvToneGenerator generator_;
    std::array<HeaderSegment, HeaderSegmentCount> header_;
    std::array<std::uint64_t, HeaderSegmentCount + 1U> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvRobotEncoderMetrics metrics_;
    SstvRobotEncoderStage lastStage_ {SstvRobotEncoderStage::Header};
    std::size_t lastHeaderIndex_ {0U};
    SstvRobotRegion lastImageRegion_ {SstvRobotRegion::Outside};
    std::uint32_t lastLine_ {0U};
    ColourComponent lastComponent_ {ColourComponent::ModeSpecific};
    std::uint32_t lastPixel_ {0U};
    bool haveLastSegment_ {false};
};

struct SstvRobotLineSync final
{
    std::uint32_t lineIndex {0U};
    std::uint64_t syncStartSample {0U};
    double confidence {0.0};
    bool predicted {false};
};

struct SstvRobotDecoderConfig final
{
    SstvRobotMode mode {SstvRobotMode::Colour36};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
    bool allowTerminalRowRecovery {false};
};

enum class SstvRobotDecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvRobotDecoderMetrics final
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

class SstvRobotDecoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr std::size_t MaximumSyncsPerConsume = 512U;
    static constexpr std::size_t MaximumBufferedPixelAccumulators = 640U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;

    explicit SstvRobotDecoder(SstvRobotDecoderConfig config = {});

    std::size_t consumeLineSyncs(const SstvRobotLineSync* syncs,
                                 std::size_t count);
    std::size_t consumeLineSyncs(
        const std::vector<SstvRobotLineSync>& syncs);
    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);

    SstvRobotDecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;
    SstvRobotMode mode() const noexcept;
    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvRobotDecodeState state() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvRobotDecoderMetrics metrics() const noexcept;

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

    static void validateConfig(const SstvRobotDecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static std::size_t accumulatorIndex(ColourComponent component,
                                        std::uint32_t pixel);

    bool acceptSync(const SstvRobotLineSync& sync);
    const Anchor* anchorFor(std::uint64_t sample,
                            std::uint32_t& line) noexcept;
    bool beginLine(std::uint32_t line);
    void accumulate(const SstvRobotPosition& position,
                    double frequencyHz,
                    double confidence) noexcept;
    bool fillTrailingComponent(ColourComponent component,
                               std::uint32_t count,
                               std::uint32_t maximumGap) noexcept;
    bool fillLeadingComponent(ColourComponent component,
                              std::uint32_t count,
                              std::uint32_t maximumGap) noexcept;
    void fillBoundedCompatibilityEdges() noexcept;
    void fillBoundedTerminalSuffix() noexcept;
    void fillBoundedTerminalRows();
    void publishCurrentLine();
    void publishMonochromeLine();
    void publishCs422Line();
    void publishCs420Line();
    void clearLineAccumulators() noexcept;
    void clearPendingPair() noexcept;
    void refreshBufferMetrics() noexcept;
    bool writePixel(std::uint32_t x,
                    std::uint32_t y,
                    SstvRgbPixel pixel);

    SstvRobotDecoderConfig config_;
    SstvRobotModeSpec spec_;
    SstvRobotMapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::array<Anchor, SstvRobotProtocol::MaximumHeight> anchors_;
    std::array<PixelAccumulator, MaximumBufferedPixelAccumulators>
        accumulators_;
    std::array<std::uint8_t, SstvRobotProtocol::MaximumWidth> pendingLuma_;
    std::array<bool, SstvRobotProtocol::MaximumWidth> pendingLumaPresent_;
    std::array<std::uint8_t, SstvRobotProtocol::MaximumWidth / 2U>
        pendingCr_;
    std::array<bool, SstvRobotProtocol::MaximumWidth / 2U>
        pendingCrPresent_;
    SstvRobotDecoderMetrics metrics_;
    SstvRobotDecodeState state_ {SstvRobotDecodeState::Receiving};
    std::uint32_t currentLine_ {0U};
    std::uint32_t pendingPairLine_ {0U};
    bool haveCurrentLine_ {false};
    bool havePendingPair_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
    std::uint32_t highestStoredAnchorLine_ {0U};
    std::uint32_t anchorCursorLine_ {0U};
    bool haveAnchorCursor_ {false};
    // Robot B/W 8 can lose the final sync pulses when a virtual audio
    // device closes its queue on an exact line boundary.  Keep a transient
    // tail anchor for observations after the last real anchor; it is never
    // counted as a stored sync and is only used for the bounded final tail.
    Anchor syntheticTailAnchor_ {};
    std::uint64_t canonicalLineSamples_ {0U};
    std::uint64_t canonicalSyncSamples_ {0U};
    bool compatibilityBw8Observed_ {false};
};

} // namespace decodium::sstv
