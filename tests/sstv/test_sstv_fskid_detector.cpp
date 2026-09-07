// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/core/SstvFskIdCodec.h"
#include "../../src/sstv/dsp/SstvToneDetector.h"
#include "../../src/sstv/rx/SstvFskIdDetector.h"
#include "../../src/sstv/tx/SstvTxStream.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::int64_t kPicosecondsPerMicrosecond = 1'000'000LL;

std::vector<SstvToneSegment> txPlan(
    const SstvFskIdCodec::EncodedFrame& frame,
    double frequencyOffsetHz = 0.0)
{
    if (!frame.valid()) {
        throw std::invalid_argument("test FSK-ID frame must be valid");
    }
    std::vector<SstvToneSegment> plan;
    plan.reserve(frame.tones.size());
    for (const auto& tone : frame.tones) {
        const std::int64_t duration =
            static_cast<std::int64_t>(tone.durationMicroseconds)
            * kPicosecondsPerMicrosecond;
        plan.push_back({static_cast<double>(tone.frequencyHz)
                            + frequencyOffsetHz,
                        Picoseconds {duration},
                        0.72,
                        SstvTxSegmentRole::FskId});
    }
    return plan;
}

std::vector<float> render(const std::vector<SstvToneSegment>& plan,
                          std::size_t chunkSize = 257U)
{
    SstvTxStream stream(kSampleRate, plan, 1.0);
    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(stream.totalSamples()));
    std::vector<float> chunk(chunkSize);
    while (!stream.complete()) {
        const std::size_t count = stream.pullFloat(chunk.data(), chunk.size());
        if (count == 0U) {
            throw std::runtime_error("test FSK-ID TX stream stalled");
        }
        samples.insert(
            samples.end(),
            chunk.cbegin(),
            chunk.cbegin() + static_cast<std::ptrdiff_t>(count));
    }
    return samples;
}

SstvToneDetectorConfig toneConfig()
{
    SstvToneDetectorConfig config;
    config.sampleRateHz = static_cast<double>(kSampleRate);
    config.windowSamples = 132U; // 11 ms
    config.hopSamples = 66U; // 5.5 ms
    config.nominalFrequenciesHz = {1'500.0, 1'900.0, 2'100.0};
    config.maximumOffsetHz = 80.0;
    config.searchStepHz = 5.0;
    config.minimumRms = 0.001;
    config.minimumSnrDb = 4.0;
    config.minimumDominanceDb = 1.0;
    config.minimumConfidence = 0.20;
    return config;
}

struct DetectionRun final
{
    std::vector<SstvFskIdCandidate> candidates;
    SstvFskIdDetectorMetrics metrics;
    SstvToneDetectorMetrics toneMetrics;
    SstvFskIdDetectorState state {
        SstvFskIdDetectorState::SearchingLeader};
};

DetectionRun detect(const std::vector<float>& samples,
                    const std::vector<std::size_t>& chunkSizes,
                    SstvFskIdDetectorConfig detectorConfig = {})
{
    if (chunkSizes.empty()
        || std::any_of(chunkSizes.cbegin(), chunkSizes.cend(),
                       [](std::size_t value) { return value == 0U; })) {
        throw std::invalid_argument("test chunk sizes must be positive");
    }

    SstvToneDetector toneDetector(toneConfig());
    SstvFskIdDetector detector(detectorConfig);
    std::vector<SstvFskIdCandidate> candidates;
    std::size_t offset = 0U;
    std::size_t chunkIndex = 0U;
    while (offset < samples.size()) {
        const std::size_t count = std::min(
            chunkSizes[chunkIndex++ % chunkSizes.size()],
            samples.size() - offset);
        const auto observations = toneDetector.consume(
            samples.data() + offset, count);
        auto emitted = detector.consume(observations);
        candidates.insert(candidates.end(),
                          std::make_move_iterator(emitted.begin()),
                          std::make_move_iterator(emitted.end()));
        offset += count;
    }
    return {std::move(candidates), detector.metrics(), toneDetector.metrics(),
            detector.state()};
}

void addDeterministicNoise(std::vector<float>& samples, float peak)
{
    std::uint32_t state = 0x4d595df4U;
    for (float& sample : samples) {
        state = state * 1'664'525U + 1'013'904'223U;
        const double unit = static_cast<double>(state)
            / static_cast<double>(std::numeric_limits<std::uint32_t>::max());
        const float noise = static_cast<float>((unit * 2.0 - 1.0) * peak);
        sample = std::clamp(sample + noise, -1.0F, 1.0F);
    }
}

std::vector<SstvToneObservation> nominalObservations(
    const SstvFskIdCodec::EncodedFrame& frame)
{
    const auto plan = txPlan(frame);
    std::vector<SstvToneObservation> observations;
    std::uint64_t sampleStart = 0U;
    std::uint64_t sequence = 0U;
    constexpr std::uint64_t hop = 66U;

    for (const auto& segment : plan) {
        const std::uint64_t segmentSamples = static_cast<std::uint64_t>(
            static_cast<long double>(segment.duration.count)
            * static_cast<long double>(kSampleRate)
            / static_cast<long double>(kPicosecondsPerSecond));
        for (std::uint64_t local = hop / 2U;
             local < segmentSamples;
             local += hop) {
            SstvToneObservation observation;
            observation.status = SstvToneStatus::Detected;
            observation.sequence = sequence++;
            observation.startSample = sampleStart + local - hop / 2U;
            observation.centreSample = sampleStart + local;
            observation.nominalFrequencyHz = segment.frequencyHz;
            observation.detectedFrequencyHz = segment.frequencyHz;
            observation.rms = 0.5;
            observation.snrDb = 30.0;
            observation.dominanceDb = 20.0;
            observation.confidence = 0.95;
            observations.push_back(observation);
        }
        sampleStart += segmentSamples;
    }
    return observations;
}

std::vector<SstvToneObservation> literalProtocolObservations(
    const std::vector<SstvFskIdCodec::Bit>& bodyBits,
    std::uint64_t trailerSamples = 1'200U)
{
    constexpr std::uint64_t hop = 66U;
    constexpr std::uint64_t leaderSamples = 3'600U;
    constexpr std::uint64_t preambleSamples = 1'200U;
    constexpr std::uint64_t bitSamples = 264U;
    std::vector<SstvToneObservation> observations;
    std::uint64_t sampleStart = 0U;
    std::uint64_t sequence = 0U;
    const auto appendRun = [&observations, &sampleStart, &sequence](
                               double frequencyHz,
                               std::uint64_t durationSamples) {
        for (std::uint64_t local = hop / 2U;
             local < durationSamples;
             local += hop) {
            SstvToneObservation observation;
            observation.status = SstvToneStatus::Detected;
            observation.sequence = sequence++;
            observation.startSample = sampleStart + local - hop / 2U;
            observation.centreSample = sampleStart + local;
            observation.nominalFrequencyHz = frequencyHz;
            observation.detectedFrequencyHz = frequencyHz;
            observation.rms = 0.5;
            observation.snrDb = 30.0;
            observation.dominanceDb = 20.0;
            observation.confidence = 0.90;
            observations.push_back(observation);
        }
        sampleStart += durationSamples;
    };

    appendRun(1'500.0, leaderSamples);
    appendRun(2'100.0, preambleSamples);
    appendRun(1'900.0, bitSamples);
    for (const auto bit : bodyBits) {
        appendRun(bit == 0U ? 2'100.0 : 1'900.0, bitSamples);
    }
    appendRun(1'900.0, trailerSamples);
    return observations;
}

// Independent detector oracle: these are literal protocol cells for the
// identifier "A", not output from SstvFskIdCodec::encode, SstvTxStream or the
// audio tone classifier.  Body symbols are 0x2a, 0x21, 0x01, 0x21 and are
// transmitted six-bit LSB first.
std::vector<SstvToneObservation> independentAObservations()
{
    const std::vector<SstvFskIdCodec::Bit> bodyBits {
        0U, 1U, 0U, 1U, 0U, 1U, // 0x2a
        1U, 0U, 0U, 0U, 0U, 1U, // 0x21 ('A')
        1U, 0U, 0U, 0U, 0U, 0U, // 0x01
        1U, 0U, 0U, 0U, 0U, 1U, // checksum 0x21
    };
    return literalProtocolObservations(bodyBits);
}

std::vector<SstvFskIdCandidate> consumeFragmentedObservations(
    SstvFskIdDetector& detector,
    const std::vector<SstvToneObservation>& observations,
    const std::vector<std::size_t>& chunkSizes)
{
    std::vector<SstvFskIdCandidate> candidates;
    std::size_t offset = 0U;
    std::size_t chunkIndex = 0U;
    while (offset < observations.size()) {
        const std::size_t count = std::min(
            chunkSizes[chunkIndex++ % chunkSizes.size()],
            observations.size() - offset);
        auto emitted = detector.consume(observations.data() + offset, count);
        candidates.insert(candidates.end(),
                          std::make_move_iterator(emitted.begin()),
                          std::make_move_iterator(emitted.end()));
        offset += count;
    }
    return candidates;
}

} // namespace

class TestSstvFskIdDetector final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidConfigurationAndHostileCalls();
    void decodesPhaseContinuousAudioAcrossChunkBoundaries();
    void decodesIndependentObservationOracleAcrossChunks();
    void toleratesAmbiguousTransitionWindows();
    void toleratesFrequencyOffsetAndModerateNoise();
    void rejectsAValidCustomIdentifierUnderCallsignPolicy();
    void exposesChecksumFailureAndRawDiagnostics();
    void reportsMalformedHeaderAndFrameLimit();
    void handlesBackToBackIdentifiers();
    void doesNotTriggerWithoutAQualifiedLeader();
    void reportsNonMonotonicInputAndBoundsMemory();
    void resetsOnSequenceRegressionAndLeaderGap();
    void boundsHostileObservationStreamsNearCounterWrap();
    void returnsPartialDiagnosticsAtEndOfStream();
    void doesNotInventTruncatedChecksumBitAtEndOfStream();
};

void TestSstvFskIdDetector::rejectsInvalidConfigurationAndHostileCalls()
{
    SstvFskIdDetectorConfig invalid;
    invalid.sampleRate = 7'999U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});
    invalid = {};
    invalid.minimumPreambleMicroseconds =
        invalid.maximumPreambleMicroseconds;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});
    invalid = {};
    invalid.maximumNominalToneErrorHz =
        std::numeric_limits<double>::quiet_NaN();
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});
    invalid = {};
    invalid.maximumObservationGapMicroseconds =
        invalid.maximumStartBitMicroseconds - 1U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});
    invalid = {};
    invalid.minimumLeaderObservedMicroseconds = 0U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});
    invalid = {};
    invalid.maximumNominalToneErrorHz = 100.0;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});
    invalid = {};
    invalid.textPolicy = static_cast<SstvFskIdCodec::TextPolicy>(0xffU);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvFskIdDetector {invalid});

    SstvFskIdDetector detector;
    QVERIFY(detector.consume(nullptr, 0U).empty());
    SstvToneObservation observation;
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        detector.consume(nullptr, 1U));
    QVERIFY_THROWS_EXCEPTION(
        std::length_error,
        detector.consume(&observation,
                         SstvFskIdDetector::MaximumObservationsPerConsume
                             + 1U));
    QCOMPARE(detector.metrics().observationsConsumed, std::uint64_t {0U});
}

void TestSstvFskIdDetector::decodesPhaseContinuousAudioAcrossChunkBoundaries()
{
    const auto encoded = SstvFskIdCodec::encode("IU8LMC/P");
    QVERIFY(encoded.valid());
    const auto samples = render(txPlan(encoded), 113U);

    const auto one = detect(samples, {samples.size()});
    const auto fragmented = detect(samples, {1U, 17U, 251U, 4'093U, 37U});
    QCOMPARE(one.candidates.size(), std::size_t {1U});
    QCOMPARE(fragmented.candidates.size(), std::size_t {1U});
    QVERIFY(one.candidates.front().valid());
    QVERIFY(fragmented.candidates.front().valid());
    QCOMPARE(one.candidates.front().decoded.text, std::string("IU8LMC/P"));
    QCOMPARE(fragmented.candidates.front().decoded.text,
             one.candidates.front().decoded.text);
    QCOMPARE(fragmented.candidates.front().wireBits,
             one.candidates.front().wireBits);
    QVERIFY(fragmented.candidates.front().logicalStartSymbolReconstructed);
    QCOMPARE(fragmented.candidates.front().end,
             SstvFskIdCandidateEnd::ChecksumReceived);
    QVERIFY(fragmented.candidates.front().confidence > 0.45);
    QCOMPARE(fragmented.metrics.peakBufferedBits,
             fragmented.candidates.front().wireBits.size());
    QVERIFY(fragmented.metrics.peakBufferedBits
            <= SstvFskIdDetector::MaximumWireBits);
    QCOMPARE(fragmented.metrics.validIdentifiers, std::uint64_t {1U});
    QVERIFY(fragmented.toneMetrics.peakBufferedSamples
            <= toneConfig().windowSamples);
}

void TestSstvFskIdDetector::decodesIndependentObservationOracleAcrossChunks()
{
    const auto observations = independentAObservations();
    SstvFskIdDetector allAtOnce;
    SstvFskIdDetector fragmented;
    const auto whole = allAtOnce.consume(observations);
    const auto pieces = consumeFragmentedObservations(
        fragmented, observations, {1U, 7U, 113U, 2U, 509U});

    QCOMPARE(whole.size(), std::size_t {1U});
    QCOMPARE(pieces.size(), std::size_t {1U});
    QVERIFY(whole.front().valid());
    QVERIFY(pieces.front().valid());
    QCOMPARE(pieces.front().decoded.text, std::string("A"));
    const std::vector<SstvFskIdCodec::Bit> expectedBits {
        0U, 1U, 0U, 1U, 0U, 1U,
        1U, 0U, 0U, 0U, 0U, 1U,
        1U, 0U, 0U, 0U, 0U, 0U,
        1U, 0U, 0U, 0U, 0U, 1U,
    };
    QCOMPARE(pieces.front().wireBits, expectedBits);
    QCOMPARE(pieces.front().wireBits, whole.front().wireBits);
    QCOMPARE(pieces.front().bitConfidences.size(), expectedBits.size());
    QCOMPARE(pieces.front().leaderStartSample, std::uint64_t {33U});
    QCOMPARE(pieces.front().frameDataStartSample, std::uint64_t {5'064U});
    QCOMPARE(pieces.front().completedAtSample, std::uint64_t {11'433U});
    QCOMPARE(pieces.front().decoded.rawSymbols,
             std::vector<SstvFskIdCodec::Symbol>(
                 {0x20U, 0x2aU, 0x21U, 0x01U, 0x21U}));
}

void TestSstvFskIdDetector::toleratesAmbiguousTransitionWindows()
{
    auto observations = independentAObservations();
    std::size_t ambiguousCount = 0U;
    for (std::size_t index = 1U; index < observations.size(); ++index) {
        if (observations[index].nominalFrequencyHz
            == observations[index - 1U].nominalFrequencyHz) {
            continue;
        }
        observations[index].status = SstvToneStatus::Ambiguous;
        observations[index].confidence = 0.0;
        ++ambiguousCount;
    }
    QVERIFY(ambiguousCount > 10U);

    SstvFskIdDetector detector;
    const auto candidates = consumeFragmentedObservations(
        detector, observations, {3U, 1U, 97U, 11U});
    QCOMPARE(candidates.size(), std::size_t {1U});
    QVERIFY(candidates.front().valid());
    QCOMPARE(candidates.front().decoded.text, std::string("A"));
    QCOMPARE(detector.metrics().invalidToneObservations,
             static_cast<std::uint64_t>(ambiguousCount));
}

void TestSstvFskIdDetector::toleratesFrequencyOffsetAndModerateNoise()
{
    const auto encoded = SstvFskIdCodec::encode("9H1TEST");
    auto samples = render(txPlan(encoded, 35.0), 431U);
    addDeterministicNoise(samples, 0.025F);
    const auto result = detect(samples, {73U, 997U, 5U, 2'047U});
    QCOMPARE(result.candidates.size(), std::size_t {1U});
    QVERIFY(result.candidates.front().valid());
    QCOMPARE(result.candidates.front().decoded.text, std::string("9H1TEST"));
    QVERIFY(result.candidates.front().confidence > 0.25);
}

void TestSstvFskIdDetector::rejectsAValidCustomIdentifierUnderCallsignPolicy()
{
    const auto encoded = SstvFskIdCodec::encode(
        "-", SstvFskIdCodec::TextPolicy::PermittedText);
    QVERIFY(encoded.valid());
    SstvFskIdDetectorConfig config;
    config.textPolicy = SstvFskIdCodec::TextPolicy::Callsign;
    const auto result = detect(render(txPlan(encoded)), {211U, 19U}, config);
    QCOMPARE(result.candidates.size(), std::size_t {1U});
    QCOMPARE(result.candidates.front().decoded.status,
             SstvFskIdCodec::DecodeStatus::InvalidCharacter);
    QCOMPARE(result.candidates.front().decoded.text, std::string("-"));
    QCOMPARE(result.candidates.front().decoded.invalidCharacterIndices,
             std::vector<std::size_t>({0U}));
    QCOMPARE(result.metrics.malformedIdentifiers, std::uint64_t {1U});
}

void TestSstvFskIdDetector::exposesChecksumFailureAndRawDiagnostics()
{
    const auto encoded = SstvFskIdCodec::encode("TEST");
    QVERIFY(encoded.valid());
    auto plan = txPlan(encoded);
    const std::size_t checksumSymbol = encoded.symbols.size() - 1U;
    const auto tone = std::find_if(
        encoded.tones.cbegin(), encoded.tones.cend(),
        [checksumSymbol](const SstvFskIdCodec::ToneSegment& segment) {
            return segment.symbolIndex == checksumSymbol
                && segment.bitIndex == 0U;
        });
    QVERIFY(tone != encoded.tones.cend());
    const std::size_t planIndex = static_cast<std::size_t>(
        std::distance(encoded.tones.cbegin(), tone));
    plan[planIndex].frequencyHz =
        plan[planIndex].frequencyHz == SstvFskIdCodec::kOneFrequencyHz
        ? SstvFskIdCodec::kZeroFrequencyHz
        : SstvFskIdCodec::kOneFrequencyHz;

    const auto result = detect(render(plan), {47U, 509U, 3U});
    QCOMPARE(result.candidates.size(), std::size_t {1U});
    const auto& candidate = result.candidates.front();
    QCOMPARE(candidate.decoded.status,
             SstvFskIdCodec::DecodeStatus::ChecksumMismatch);
    QVERIFY(candidate.decoded.checksumPresent);
    QVERIFY(!candidate.decoded.checksumValid);
    QCOMPARE(candidate.decoded.text, std::string("TEST"));
    QCOMPARE(candidate.wireBits.size(), std::size_t {42U});
    QCOMPARE(candidate.decoded.rawBits.size(), std::size_t {48U});
    QVERIFY(candidate.decoded.receivedChecksum.has_value());
    QVERIFY(candidate.decoded.computedChecksum.has_value());
}

void TestSstvFskIdDetector::reportsMalformedHeaderAndFrameLimit()
{
    const std::vector<SstvFskIdCodec::Bit> badHeader {
        0U, 0U, 0U, 1U, 0U, 1U,
    };
    SstvFskIdDetector headerDetector;
    const auto malformed = headerDetector.consume(
        literalProtocolObservations(badHeader));
    QCOMPARE(malformed.size(), std::size_t {1U});
    QCOMPARE(malformed.front().end,
             SstvFskIdCandidateEnd::MalformedHeader);
    QVERIFY(!malformed.front().valid());
    QCOMPARE(malformed.front().wireBits, badHeader);
    QCOMPARE(malformed.front().decoded.status,
             SstvFskIdCodec::DecodeStatus::MissingHeader);

    const std::vector<SstvFskIdCodec::Bit> header {
        0U, 1U, 0U, 1U, 0U, 1U,
    };
    const std::vector<SstvFskIdCodec::Bit> letterA {
        1U, 0U, 0U, 0U, 0U, 1U,
    };
    std::vector<SstvFskIdCodec::Bit> noTerminator = header;
    for (std::size_t index = 0U;
         index <= SstvFskIdCodec::kMaximumTextLength;
         ++index) {
        noTerminator.insert(noTerminator.end(),
                            letterA.cbegin(), letterA.cend());
    }
    SstvFskIdDetector limitDetector;
    const auto limited = limitDetector.consume(
        literalProtocolObservations(noTerminator));
    QCOMPARE(limited.size(), std::size_t {1U});
    QCOMPARE(limited.front().end, SstvFskIdCandidateEnd::FrameLimit);
    QVERIFY(!limited.front().valid());
    QVERIFY(limited.front().wireBits.size()
            <= SstvFskIdDetector::MaximumWireBits);
    QCOMPARE(limitDetector.bufferedBitCount(), std::size_t {0U});
    QVERIFY(limitDetector.metrics().peakBufferedBits
            <= SstvFskIdDetector::MaximumWireBits);
}

void TestSstvFskIdDetector::handlesBackToBackIdentifiers()
{
    auto first = render(txPlan(SstvFskIdCodec::encode("FIRST")));
    const auto second = render(txPlan(SstvFskIdCodec::encode("SECOND")));
    first.insert(first.end(), second.cbegin(), second.cend());
    const auto result = detect(first, {127U, 4'001U, 29U});
    QCOMPARE(result.candidates.size(), std::size_t {2U});
    QVERIFY(result.candidates[0].valid());
    QVERIFY(result.candidates[1].valid());
    QCOMPARE(result.candidates[0].decoded.text, std::string("FIRST"));
    QCOMPARE(result.candidates[1].decoded.text, std::string("SECOND"));
    QCOMPARE(result.metrics.validIdentifiers, std::uint64_t {2U});
}

void TestSstvFskIdDetector::doesNotTriggerWithoutAQualifiedLeader()
{
    std::vector<SstvToneSegment> plan {
        {1'500.0, Picoseconds {100'000'000'000LL}, 0.7,
         SstvTxSegmentRole::FskId},
        {2'100.0, Picoseconds {100'000'000'000LL}, 0.7,
         SstvTxSegmentRole::FskId},
        {1'900.0, Picoseconds {400'000'000'000LL}, 0.7,
         SstvTxSegmentRole::FskId},
    };
    const auto result = detect(render(plan), {13U, 701U});
    QVERIFY(result.candidates.empty());
    QCOMPARE(result.metrics.qualifiedLeaders, std::uint64_t {0U});
    QCOMPARE(result.metrics.peakBufferedBits, std::size_t {0U});
}

void TestSstvFskIdDetector::reportsNonMonotonicInputAndBoundsMemory()
{
    SstvFskIdDetector detector;
    std::vector<SstvToneObservation> invalid(
        SstvFskIdDetector::MaximumObservationsPerConsume);
    for (std::size_t index = 0U; index < invalid.size(); ++index) {
        invalid[index].sequence = index;
        invalid[index].startSample = index;
        invalid[index].centreSample = index + 1U;
        invalid[index].status = SstvToneStatus::LowSignal;
    }
    QVERIFY(detector.consume(invalid).empty());
    QCOMPARE(detector.bufferedBitCount(), std::size_t {0U});
    QCOMPARE(detector.metrics().invalidToneObservations,
             static_cast<std::uint64_t>(invalid.size()));

    SstvToneObservation backwards;
    backwards.centreSample = 1U;
    backwards.sequence = static_cast<std::uint64_t>(invalid.size());
    backwards.status = SstvToneStatus::Detected;
    backwards.nominalFrequencyHz = 1'500.0;
    backwards.confidence = 1.0;
    QVERIFY(detector.consume(&backwards, 1U).empty());
    QCOMPARE(detector.metrics().nonMonotonicObservations,
             std::uint64_t {1U});
    QCOMPARE(detector.state(), SstvFskIdDetectorState::SearchingLeader);
    QVERIFY(detector.metrics().peakBufferedBits
            <= SstvFskIdDetector::MaximumWireBits);
}

void TestSstvFskIdDetector::resetsOnSequenceRegressionAndLeaderGap()
{
    auto observations = independentAObservations();
    QVERIFY(observations.size() > 20U);
    observations[20].sequence = observations[19].sequence;

    SstvFskIdDetector sequenceDetector;
    const auto sequenceCandidates = sequenceDetector.consume(observations);
    QVERIFY(sequenceCandidates.empty());
    QCOMPARE(sequenceDetector.metrics().nonMonotonicObservations,
             std::uint64_t {1U});
    QCOMPARE(sequenceDetector.state(),
             SstvFskIdDetectorState::SearchingLeader);

    SstvFskIdDetector gapDetector;
    const auto oracle = independentAObservations();
    const auto leaderEnd = std::find_if(
        oracle.cbegin(), oracle.cend(),
        [](const SstvToneObservation& observation) {
            return observation.centreSample >= 2'673U;
        });
    QVERIFY(leaderEnd != oracle.cend());
    std::vector<SstvToneObservation> leader(
        oracle.cbegin(), std::next(leaderEnd));
    QVERIFY(gapDetector.consume(leader).empty());
    QCOMPARE(gapDetector.metrics().qualifiedLeaders, std::uint64_t {1U});

    SstvToneObservation afterGap = leader.back();
    ++afterGap.sequence;
    afterGap.centreSample += 793U;
    QVERIFY(gapDetector.consume(&afterGap, 1U).empty());
    QCOMPARE(gapDetector.metrics().gapResets, std::uint64_t {1U});
    QCOMPARE(gapDetector.state(),
             SstvFskIdDetectorState::SearchingLeader);
}

void TestSstvFskIdDetector::boundsHostileObservationStreamsNearCounterWrap()
{
    SstvFskIdDetector detector;
    std::uint32_t randomState = 0x6d2b79f5U;
    std::uint64_t sample =
        std::numeric_limits<std::uint64_t>::max() - 10'000U;
    std::uint64_t sequence =
        std::numeric_limits<std::uint64_t>::max() - 10'000U;
    std::uint64_t total = 0U;

    for (std::size_t batchIndex = 0U; batchIndex < 32U; ++batchIndex) {
        std::vector<SstvToneObservation> batch(2'048U);
        for (std::size_t index = 0U; index < batch.size(); ++index) {
            randomState = randomState * 1'664'525U + 1'013'904'223U;
            sample += static_cast<std::uint64_t>(randomState % 2'047U) + 1U;
            sequence += 1U;
            auto& observation = batch[index];
            observation.sequence = sequence;
            observation.startSample = sample > 32U ? sample - 32U : 0U;
            observation.centreSample = sample;
            observation.status = static_cast<SstvToneStatus>(
                randomState % 7U);
            switch ((randomState >> 4U) % 7U) {
            case 0U:
                observation.nominalFrequencyHz = 1'500.0;
                break;
            case 1U:
                observation.nominalFrequencyHz = 1'900.0;
                break;
            case 2U:
                observation.nominalFrequencyHz = 2'100.0;
                break;
            case 3U:
                observation.nominalFrequencyHz =
                    std::numeric_limits<double>::quiet_NaN();
                break;
            case 4U:
                observation.nominalFrequencyHz =
                    std::numeric_limits<double>::infinity();
                break;
            default:
                observation.nominalFrequencyHz =
                    static_cast<double>(randomState % 5'000U);
                break;
            }
            switch ((randomState >> 8U) % 5U) {
            case 0U:
                observation.confidence = -1.0;
                break;
            case 1U:
                observation.confidence = 2.0;
                break;
            case 2U:
                observation.confidence =
                    std::numeric_limits<double>::quiet_NaN();
                break;
            default:
                observation.confidence = 0.75;
                break;
            }
        }

        const auto candidates = detector.consume(batch);
        for (const auto& candidate : candidates) {
            QVERIFY(candidate.wireBits.size()
                    <= SstvFskIdDetector::MaximumWireBits);
            QCOMPARE(candidate.bitConfidences.size(),
                     candidate.wireBits.size());
            QVERIFY(std::isfinite(candidate.confidence));
            QVERIFY(candidate.confidence >= 0.0);
            QVERIFY(candidate.confidence <= 1.0);
            QVERIFY(candidate.decoded.rawBits.size()
                    <= SstvFskIdDetector::MaximumWireBits
                        + SstvFskIdCodec::kBitsPerSymbol);
        }
        total += static_cast<std::uint64_t>(batch.size());
        QVERIFY(detector.bufferedBitCount()
                <= SstvFskIdDetector::MaximumWireBits);
    }

    QCOMPARE(detector.metrics().observationsConsumed, total);
    QVERIFY(detector.metrics().peakBufferedBits
            <= SstvFskIdDetector::MaximumWireBits);
    detector.finishPending();
    QCOMPARE(detector.state(), SstvFskIdDetectorState::SearchingLeader);
}

void TestSstvFskIdDetector::returnsPartialDiagnosticsAtEndOfStream()
{
    const auto encoded = SstvFskIdCodec::encode("PARTIAL");
    auto observations = nominalObservations(encoded);
    QVERIFY(observations.size() > 100U);

    // Keep the envelope, header and only a prefix of the payload.
    observations.resize(observations.size() * 2U / 3U);
    SstvFskIdDetector detector;
    const auto emitted = detector.consume(observations);
    QVERIFY(emitted.empty());
    const auto partial = detector.finishPending();
    QVERIFY(partial.has_value());
    QCOMPARE(partial->end, SstvFskIdCandidateEnd::EndOfStream);
    QVERIFY(!partial->valid());
    QVERIFY(!partial->wireBits.empty());
    QVERIFY(partial->wireBits.size() <= SstvFskIdDetector::MaximumWireBits);
    QVERIFY(partial->decoded.status
            == SstvFskIdCodec::DecodeStatus::TruncatedBits
            || partial->decoded.status
                == SstvFskIdCodec::DecodeStatus::MissingTerminator
            || partial->decoded.status
                == SstvFskIdCodec::DecodeStatus::MissingChecksum);
    QCOMPARE(detector.state(), SstvFskIdDetectorState::SearchingLeader);
}

void TestSstvFskIdDetector::doesNotInventTruncatedChecksumBitAtEndOfStream()
{
    auto observations = independentAObservations();
    constexpr std::uint64_t frameDataStart = 5'064U;
    constexpr std::uint64_t bitSamples = 264U;
    constexpr std::uint64_t finalChecksumBit = 23U;
    const std::uint64_t firstFinalBitWindow =
        frameDataStart + finalChecksumBit * bitSamples + 33U;
    const auto end = std::find_if(
        observations.cbegin(), observations.cend(),
        [](const SstvToneObservation& observation) {
            return observation.centreSample > firstFinalBitWindow;
        });
    QVERIFY(end != observations.cbegin());
    observations.erase(end, observations.cend());
    QCOMPARE(observations.back().centreSample, firstFinalBitWindow);

    SstvFskIdDetector detector;
    QVERIFY(detector.consume(observations).empty());
    const auto partial = detector.finishPending();
    QVERIFY(partial.has_value());
    QCOMPARE(partial->end, SstvFskIdCandidateEnd::EndOfStream);
    QVERIFY(!partial->valid());
    QCOMPARE(partial->wireBits.size(), std::size_t {23U});
    QCOMPARE(partial->bitConfidences.size(), partial->wireBits.size());
    QCOMPARE(partial->decoded.status,
             SstvFskIdCodec::DecodeStatus::TruncatedBits);
    QCOMPARE(detector.metrics().bitsCompleted, std::uint64_t {23U});
    QCOMPARE(detector.state(), SstvFskIdDetectorState::SearchingLeader);
}

QTEST_APPLESS_MAIN(TestSstvFskIdDetector)
#include "test_sstv_fskid_detector.moc"
