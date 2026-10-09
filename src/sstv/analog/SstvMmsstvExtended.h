// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvTypes.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include "../image/SstvColourConverter.h"
#include "../image/SstvImageFrame.h"
#include "../tx/SstvToneGenerator.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvMmsstvMode : std::uint8_t
{
    Mp73,
    Mp115,
    Mp140,
    Mp175,
    Mr73,
    Mr90,
    Mr115,
    Mr140,
    Mr175,
    Ml180,
    Ml240,
    Ml280,
    Ml320,
    Mp73Narrow,
    Mp110Narrow,
    Mp140Narrow,
    Mc110Narrow,
    Mc140Narrow,
    Mc180Narrow,
};

enum class SstvMmsstvLayout : std::uint8_t
{
    MpPairedYCbCr,
    MrHorizontal422,
    McSequentialRgb,
};

struct SstvMmsstvModeSpec final
{
    SstvMmsstvMode mode {SstvMmsstvMode::Mp73};
    const char* stableId {nullptr};
    const char* displayName {nullptr};
    SstvMmsstvLayout layout {SstvMmsstvLayout::MpPairedYCbCr};
    bool narrow {false};
    std::uint32_t width {320U};
    std::uint32_t height {256U};
    std::uint32_t scanCount {128U};
    std::uint8_t linesPerScan {2U};
    // For wide modes this is the complete odd-parity raw extension octet.
    // For narrow modes it is the six-bit N-VIS payload.
    std::uint8_t visWireCode {0U};
    double syncFrequencyHz {1'200.0};
    double porchFrequencyHz {1'500.0};
    double blackFrequencyHz {1'500.0};
    double whiteFrequencyHz {2'300.0};
    Picoseconds headerDuration;
    Picoseconds syncDuration;
    Picoseconds porchDuration;
    Picoseconds primaryComponentDuration;
    Picoseconds secondaryComponentDuration;
    Picoseconds holdLastDuration;
    Picoseconds scanDuration;
    Picoseconds imageDuration;
};

class SstvMmsstvProtocol final
{
public:
    SstvMmsstvProtocol() = delete;

    static constexpr Picoseconds WideHeaderDuration {1'150'000'000'000LL};
    static constexpr Picoseconds NarrowHeaderDuration {950'000'000'000LL};

    static SstvMmsstvModeSpec spec(SstvMmsstvMode mode);
    static std::optional<SstvMmsstvMode> modeForExtendedRaw(
        std::uint8_t rawOctet) noexcept;
    static std::optional<SstvMmsstvMode> modeForNarrowPayload(
        std::uint8_t payload) noexcept;
    static double frequencyForValue(const SstvMmsstvModeSpec& spec,
                                    std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(const SstvMmsstvModeSpec& spec,
                                          double frequencyHz) noexcept;
};

enum class SstvMmsstvRegion : std::uint8_t
{
    Outside,
    Sync,
    Porch,
    HoldLast,
    Pixel,
    Complete,
};

struct SstvMmsstvMapperConfig final
{
    SstvMmsstvMode mode {SstvMmsstvMode::Mp73};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
};

struct SstvMmsstvPosition final
{
    SstvMmsstvRegion region {SstvMmsstvRegion::Outside};
    std::uint32_t scan {0U};
    std::uint32_t firstDestinationLine {0U};
    std::uint8_t componentIndex {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixel {0U};
    std::uint32_t transmittedPixelCount {0U};
    std::uint64_t segmentStartSample {0U};
    std::uint64_t segmentEndSample {0U};

    bool valid() const noexcept
    {
        return region != SstvMmsstvRegion::Outside
            && region != SstvMmsstvRegion::Complete;
    }
};

// One cumulative rational mapper is shared by RX and TX.  Segment and pixel
// boundaries are derived from absolute protocol time, never independently
// rounded, including the 0.1 ms MR/ML terminal holds.
class SstvMmsstvMapper final
{
public:
    static constexpr std::int32_t MaximumAbsoluteClockErrorPpm = 100'000;
    static constexpr std::uint32_t MinimumSampleRate = 8'000U;
    static constexpr std::uint32_t MaximumSampleRate = 384'000U;

    explicit SstvMmsstvMapper(SstvMmsstvMapperConfig config = {});

    SstvMmsstvMapperConfig config() const noexcept;
    SstvMmsstvModeSpec modeSpec() const noexcept;
    std::uint64_t imageSampleCount() const noexcept;
    std::uint64_t scanStartSample(std::uint32_t scan) const;
    std::uint64_t scanEndSample(std::uint32_t scan) const;
    SstvMmsstvPosition positionAtSample(std::uint64_t imageSample) const;
    SstvMmsstvPosition positionAtElapsedTime(Picoseconds elapsed) const;

private:
    std::uint64_t samplesAtProtocolTime(std::uint64_t picoseconds) const;
    SstvMmsstvPosition positionAtProtocolTime(
        std::uint64_t protocolPicoseconds) const;
    SstvMmsstvPosition makePosition(
        SstvMmsstvRegion region,
        std::uint32_t scan,
        std::uint8_t componentIndex,
        ColourComponent component,
        std::uint32_t pixel,
        std::uint32_t transmittedPixelCount,
        std::uint64_t startPicoseconds,
        std::uint64_t endPicoseconds) const;

    SstvMmsstvMapperConfig config_;
    SstvMmsstvModeSpec spec_;
    std::uint32_t clockScaleNumerator_ {1'000'000U};
    std::uint64_t imageSamples_ {0U};
};

struct SstvMmsstvEncoderConfig final
{
    SstvMmsstvMode mode {SstvMmsstvMode::Mp73};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    double level {1.0};
    double headroom {kDefaultSstvTxHeadroom};
};

enum class SstvMmsstvEncoderStage : std::uint8_t
{
    Header,
    Image,
    Complete,
    Cancelled,
};

struct SstvMmsstvEncoderMetrics final
{
    std::uint64_t pullCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t segmentTransitions {0U};
    std::uint64_t producedSamples {0U};
    std::size_t residentImageBytes {0U};
    SstvToneMetrics tone;
};

class SstvMmsstvEncoder final
{
public:
    static constexpr std::size_t MaximumSamplesPerPull = 262'144U;
    static constexpr double MaximumLevel = 16.0;

    SstvMmsstvEncoder(const SstvRgbPixel* pixels,
                      std::size_t count,
                      SstvMmsstvEncoderConfig config = {});
    explicit SstvMmsstvEncoder(
        const std::vector<SstvRgbPixel>& pixels,
        SstvMmsstvEncoderConfig config = {});

    static std::size_t pixelCount(SstvMmsstvMode mode);

    SstvMmsstvEncoder(const SstvMmsstvEncoder&) = delete;
    SstvMmsstvEncoder& operator=(const SstvMmsstvEncoder&) = delete;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);
    SstvMmsstvMode mode() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    std::uint64_t headerSamples() const noexcept;
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    SstvMmsstvEncoderMetrics metrics() const noexcept;
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
    static std::vector<HeaderSegment> makeHeader(
        const SstvMmsstvModeSpec& spec);
    static std::vector<std::uint64_t> makeHeaderBoundaries(
        std::uint32_t sampleRate,
        const std::vector<HeaderSegment>& header);
    double imageFrequency(const SstvMmsstvPosition& position) const;
    std::size_t headerIndexAt(std::uint64_t sample) const noexcept;
    void noteTransition(SstvMmsstvEncoderStage stage,
                        std::size_t headerSegment,
                        const SstvMmsstvPosition& image) noexcept;

    SstvMmsstvEncoderConfig config_;
    SstvMmsstvModeSpec spec_;
    std::vector<SstvRgbPixel> rgbPixels_;
    std::vector<SstvYCbCrPixel> yCbCrPixels_;
    SstvMmsstvMapper mapper_;
    SstvToneGenerator generator_;
    std::vector<HeaderSegment> header_;
    std::vector<std::uint64_t> headerBoundaries_;
    std::uint64_t totalSamples_ {0U};
    std::uint64_t producedSamples_ {0U};
    SstvMmsstvEncoderMetrics metrics_;
    SstvMmsstvEncoderStage lastStage_ {SstvMmsstvEncoderStage::Header};
    std::size_t lastHeaderSegment_ {0U};
    SstvMmsstvPosition lastImagePosition_;
    bool haveLastSegment_ {false};
};

struct SstvMmsstvDecoderConfig final
{
    SstvMmsstvMode mode {SstvMmsstvMode::Mp73};
    std::uint32_t sampleRate {12'000U};
    std::int32_t clockErrorPpm {0};
    std::uint64_t imageStartSample {0U};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvMmsstvDecodeState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvMmsstvDecoderMetrics final
{
    std::uint64_t observationInputs {0U};
    std::uint64_t acceptedObservations {0U};
    std::uint64_t invalidObservations {0U};
    std::uint64_t observationsBeforeImage {0U};
    std::uint64_t observationsAfterImage {0U};
    std::uint64_t nonPixelObservations {0U};
    std::uint64_t observedScanSyncs {0U};
    std::uint64_t scansPublished {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t componentsPublished {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::size_t bufferedPixelAccumulators {0U};
    std::size_t peakBufferedPixelAccumulators {0U};
};

class SstvMmsstvDecoder final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr double MaximumAbsoluteFrequencyOffsetHz = 500.0;

    explicit SstvMmsstvDecoder(SstvMmsstvDecoderConfig config = {});

    std::size_t consume(const SstvFrequencyObservation* observations,
                        std::size_t count);
    std::size_t consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvMmsstvDecodeState finish();
    void cancel() noexcept;
    void reset() noexcept;
    double setFrequencyOffsetHz(double offsetHz);
    double frequencyOffsetHz() const noexcept;
    SstvMmsstvMode mode() const noexcept;
    SstvMmsstvDecodeState state() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvMmsstvDecoderMetrics metrics() const noexcept;

private:
    struct PixelAccumulator final
    {
        double weightedFrequencyHz {0.0};
        double confidenceWeight {0.0};
        std::uint32_t count {0U};
    };

    static void validateConfig(const SstvMmsstvDecoderConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    bool beginScan(std::uint32_t scan);
    void accumulate(const SstvMmsstvPosition& position,
                    double frequencyHz,
                    double confidence) noexcept;
    void publishCurrentScan();
    std::optional<std::uint8_t> accumulatedValue(
        std::uint8_t component,
        std::uint32_t pixel) const noexcept;
    void clearAccumulators() noexcept;
    void refreshBufferMetrics() noexcept;

    SstvMmsstvDecoderConfig config_;
    SstvMmsstvModeSpec spec_;
    SstvMmsstvMapper mapper_;
    std::unique_ptr<SstvImageFrame> frame_;
    std::vector<PixelAccumulator> accumulators_;
    SstvMmsstvDecoderMetrics metrics_;
    SstvMmsstvDecodeState state_ {SstvMmsstvDecodeState::Receiving};
    std::uint64_t imageEndSample_ {0U};
    std::uint32_t currentScan_ {0U};
    bool haveCurrentScan_ {false};
    std::uint64_t lastObservationSample_ {0U};
    bool haveLastObservation_ {false};
    std::uint32_t lastObservedSyncScan_ {0U};
    bool haveObservedSyncScan_ {false};
    std::size_t nonEmptyAccumulators_ {0U};
};

} // namespace decodium::sstv
