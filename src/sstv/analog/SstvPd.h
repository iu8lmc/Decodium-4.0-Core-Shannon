// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvTypes.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include "../image/SstvColourConverter.h"
#include "../image/SstvImageFrame.h"
#include "../tx/SstvToneGenerator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvPdMode : std::uint8_t
{
    Pd50,
    Pd90,
    Pd120,
    Pd160,
    Pd180,
    Pd240,
    Pd290,
};

// One PD radio line carries two destination rows in this exact order:
// Y(even), average Cr, average Cb, Y(odd).  Chroma is shared by the pair.
struct SstvPdModeSpec final
{
    SstvPdMode mode {SstvPdMode::Pd50};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    std::uint32_t width {320U};
    std::uint32_t height {256U};
    std::uint8_t visPayload {93U};
    Picoseconds syncDuration;
    Picoseconds porchDuration;
    Picoseconds pixelDuration;
    Picoseconds componentDuration;
    Picoseconds linePairDuration;
    Picoseconds imageDuration;
};

class SstvPdProtocol final
{
public:
    SstvPdProtocol() = delete;

    static constexpr double SyncFrequencyHz = 1'200.0;
    static constexpr double PorchFrequencyHz = 1'500.0;
    static constexpr double BlackFrequencyHz = 1'500.0;
    static constexpr double WhiteFrequencyHz = 2'300.0;
    static constexpr Picoseconds HeaderDuration {910'000'000'000LL};

    static SstvPdModeSpec spec(SstvPdMode mode);
    static std::optional<SstvPdMode> modeForVis(
        std::uint8_t visPayload) noexcept;
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequencyHz) noexcept;
};

enum class SstvPdRegion : std::uint8_t
{
    Outside,
    Sync,
    Porch,
    Pixel,
    Complete,
};

struct SstvPdMapperConfig final
{
    SstvPdMode mode {SstvPdMode::Pd50};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
};

struct SstvPdPosition final
{
    SstvPdRegion region {SstvPdRegion::Outside};
    std::uint32_t linePair {0U};
    std::uint32_t firstDestinationLine {0U};
    std::uint8_t scanIndex {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U};

    bool valid() const noexcept
    {
        return region != SstvPdRegion::Outside
            && region != SstvPdRegion::Complete;
    }
};

// Cumulative rational mapper shared by RX and TX.  No scan or pixel rounds
// independently, so fractional boundaries remain stable over PD290.
class SstvPdMapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvPdMapper(SstvPdMapperConfig config = {});

    SstvPdMapperConfig config() const noexcept;
    SstvPdModeSpec modeSpec() const noexcept;
    std::uint32_t linePairCount() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t linePairStartSample(std::uint32_t linePair) const;
    std::uint64_t linePairEndSample(std::uint32_t linePair) const;
    SstvPdPosition positionAtSample(std::uint64_t imageSample) const;
    SstvPdPosition positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvPdPosition positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvPdPosition makePosition(
        SstvPdRegion region,
        std::uint32_t linePair,
        std::uint8_t scanIndex,
        ColourComponent component,
        std::uint32_t pixel,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvPdMapperConfig config_;
    SstvPdModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvPdEncoderConfig final
{
    SstvPdMode mode {SstvPdMode::Pd50};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

enum class SstvPdEncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvPdEncoderPosition final
{
    SstvPdEncoderStage stage {SstvPdEncoderStage::Header};
    std::size_t headerSegment {0U};
    SstvPdPosition image;
    std::uint64_t producedSamples {0U};
    std::uint64_t totalSamples {0U};
    double frequencyHz {0.0};
};

struct SstvPdEncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    SstvToneMetrics tone;
};

class SstvPdEncoder final
{
public:
    static constexpr std::size_t HeaderSegmentCount = 13U;
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvPdEncoder(const SstvRgbPixel* pixels,
                  std::size_t count,
                  SstvPdEncoderConfig config = {});
    explicit SstvPdEncoder(const std::vector<SstvRgbPixel>& pixels,
                           SstvPdEncoderConfig config = {});

    static std::size_t pixelCount(SstvPdMode mode);

    SstvPdEncoder(const SstvPdEncoder&) = delete;
    SstvPdEncoder& operator=(const SstvPdEncoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);
    SstvPdEncoderPosition position() const;
    SstvPdMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvPdEncoderMetrics metrics() const noexcept;
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
    static std::vector<SstvYCbCrPixel> convertPixels(
        const SstvRgbPixel* pixels,
        std::size_t count,
        SstvPdMode mode);
    std::size_t headerIndexAt(std::uint64_t sample) const noexcept;
    double imageFrequency(const SstvPdPosition& position) const;
    void noteTransition(const SstvPdEncoderPosition& position) noexcept;

    SstvPdEncoderConfig config_;
    SstvPdModeSpec spec_;
    std::vector<SstvYCbCrPixel> pixels_;
    SstvPdMapper mapper_;
    SstvToneGenerator generator_;
    std::array<HeaderSegment, HeaderSegmentCount> header_;
    std::array<std::uint64_t, HeaderSegmentCount + 1U> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvPdEncoderMetrics metrics_;
    SstvPdEncoderPosition lastPosition_;
    bool haveLastSegment_ {false};
};

struct SstvPdDecoderConfig final
{
    SstvPdMode mode {SstvPdMode::Pd50};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    std::uint64_t imageStartSample {0U};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvPdDecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvPdDecoderMetrics final
{
    std::uint64_t observationInputs {0U};
    std::uint64_t acceptedObservations {0U};
    std::uint64_t invalidObservations {0U};
    std::uint64_t observationsBeforeImage {0U};
    std::uint64_t observationsAfterImage {0U};
    std::uint64_t nonPixelObservations {0U};
    std::uint64_t observedPairSyncs {0U};
    std::uint64_t linePairsPublished {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t componentsPublished {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::size_t bufferedPixelAccumulators {0U};
    std::size_t peakBufferedPixelAccumulators {0U};
};

// Bounded chronological decoder for one VIS-acquired PD image.  It retains at
// most four scans and publishes two RGB rows together after Y/Cr/Cb/Y closes.
class SstvPdDecoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;

    explicit SstvPdDecoder(SstvPdDecoderConfig config = {});

    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvPdDecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;
    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvPdMode mode() const noexcept;
    SstvPdDecodeState state() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvPdDecoderMetrics metrics() const noexcept;

private:
    struct PixelAccumulator final
    {
        double weightedFrequencyHz {0.0};
        double confidenceWeight {0.0};
        std::uint32_t count {0U};
    };

    static void validateConfig(const SstvPdDecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    bool beginLinePair(std::uint32_t linePair);
    void accumulate(const SstvPdPosition& position,
                    double frequencyHz,
                    double confidence) noexcept;
    void publishCurrentLinePair();
    void clearAccumulators() noexcept;
    void refreshBufferMetrics() noexcept;

    SstvPdDecoderConfig config_;
    SstvPdModeSpec spec_;
    SstvPdMapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::vector<PixelAccumulator> accumulators_;
    SstvPdDecoderMetrics metrics_;
    SstvPdDecodeState state_ {SstvPdDecodeState::Receiving};
    std::uint64_t imageEndSample_ {0U};
    std::uint32_t currentLinePair_ {0U};
    bool haveCurrentLinePair_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::uint32_t lastObservedSyncPair_ {0U};
    bool haveObservedSyncPair_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
};

} // namespace decodium::sstv
