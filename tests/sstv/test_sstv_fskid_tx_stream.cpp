// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/tx/SstvFskIdTxStream.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

std::vector<float> renderFloat(SstvFskIdTxStream& stream,
                               std::size_t chunkSize)
{
    if (chunkSize == 0U) {
        throw std::invalid_argument("test chunk size must be positive");
    }
    std::vector<float> result;
    result.reserve(static_cast<std::size_t>(stream.totalSamples()));
    std::vector<float> chunk(chunkSize);
    while (!stream.complete()) {
        const std::size_t produced = stream.pullFloat(
            chunk.data(), chunk.size());
        if (produced == 0U) {
            throw std::runtime_error("FSK ID stream made no progress");
        }
        result.insert(result.end(), chunk.cbegin(), chunk.cbegin() + produced);
    }
    return result;
}

} // namespace

class TestSstvFskIdTxStream final : public QObject
{
    Q_OBJECT

private slots:
    void rendersAuditedEnvelopeAsBoundedPcm()
    {
        SstvFskIdTxStream stream("A");

        QVERIFY(stream.frame().valid());
        QCOMPARE(stream.frame().validation.text, std::string("A"));
        QCOMPARE(stream.sampleRate(), std::uint32_t {48'000U});
        QCOMPARE(stream.totalDuration(), Picoseconds {1'050'000'000'000LL});
        QCOMPARE(stream.totalSamples(), std::uint64_t {50'400U});
        QCOMPARE(stream.remainingSamples(), stream.totalSamples());
        QCOMPARE(stream.progress(), 0.0);
        QVERIFY(!stream.complete());
        QVERIFY(!stream.cancelled());

        const auto first = stream.currentSegment();
        QVERIFY(first.has_value());
        QCOMPARE(first->role, SstvTxSegmentRole::FskId);
        QCOMPARE(first->frequencyHz, 1'500.0);
        QCOMPARE(first->duration, Picoseconds {300'000'000'000LL});
        QCOMPARE(first->sampleCount, std::uint64_t {14'400U});

        std::vector<std::int16_t> chunk(257U);
        std::uint64_t generated = 0U;
        while (!stream.complete()) {
            const std::size_t count = stream.pullPcm16(
                chunk.data(), chunk.size());
            QVERIFY(count > 0U);
            QVERIFY(count <= chunk.size());
            generated += count;
        }

        QCOMPARE(generated, stream.totalSamples());
        QCOMPARE(stream.producedSamples(), stream.totalSamples());
        QCOMPARE(stream.remainingSamples(), std::uint64_t {0U});
        QCOMPARE(stream.progress(), 1.0);
        QCOMPARE(stream.metrics().samplesGenerated, stream.totalSamples());
        QCOMPARE(stream.metrics().clippedSamples, std::uint64_t {0U});
        QVERIFY(!stream.currentSegment().has_value());
    }

    void outputIsChunkInvariantAndPhaseContinuous()
    {
        SstvFskIdTxConfig config;
        config.headroom = 1.0;

        SstvFskIdTxStream whole("A", config);
        SstvFskIdTxStream chunked("A", config);
        const auto wholeOutput = renderFloat(
            whole, static_cast<std::size_t>(whole.totalSamples()));
        const auto chunkedOutput = renderFloat(chunked, 137U);
        QVERIFY(wholeOutput == chunkedOutput);

        // Leader and space preamble both contain an integer number of cycles.
        // The 22 ms 1900 Hz start bit ends at 0.8 turns.  The following framed
        // bit must begin at that phase, rather than resetting its oscillator.
        constexpr std::size_t firstFramedBit =
            14'400U + 4'800U + 1'056U;
        QVERIFY(firstFramedBit < wholeOutput.size());
        constexpr double pi =
            3.141592653589793238462643383279502884;
        const float expected = static_cast<float>(
            std::sin(2.0 * pi * 0.8));
        QVERIFY(std::abs(wholeOutput[firstFramedBit] - expected) < 1.0e-5F);
        QVERIFY(std::abs(wholeOutput[firstFramedBit]) > 0.9F);
    }

    void fractionalTimingIsCarriedAcrossTheFrame()
    {
        SstvFskIdTxConfig config;
        config.sampleRate = 44'100U;
        SstvFskIdTxStream stream("IU8LMC", config);

        const auto duration = static_cast<std::uint64_t>(
            stream.totalDuration().count);
        const auto expected = duration * config.sampleRate
            / static_cast<std::uint64_t>(kPicosecondsPerSecond);
        QCOMPARE(stream.totalSamples(), expected);

        const auto output = renderFloat(stream, 1U);
        QCOMPARE(output.size(), static_cast<std::size_t>(expected));
        QCOMPARE(stream.metrics().samplesGenerated, expected);
    }

    void sanitizationAndPolicyAreExplicit()
    {
        SstvFskIdTxConfig config;
        config.inputHandling = SstvFskIdCodec::InputHandling::Sanitize;
        SstvFskIdTxStream callsign(" iu8lmc-/p!", config);
        QCOMPARE(callsign.frame().validation.text, std::string("IU8LMC/P"));
        QVERIFY(callsign.frame().validation.changed);

        config.textPolicy = SstvFskIdCodec::TextPolicy::PermittedText;
        SstvFskIdTxStream custom("cq dx!", config);
        QCOMPARE(custom.frame().validation.text, std::string("CQDX"));

        SstvFskIdTxStream maximum("123456789");
        QVERIFY(maximum.frame().valid());
        QVERIFY(maximum.totalSamples() > callsign.totalSamples());
    }

    void cancellationAndResetAreDeterministic()
    {
        SstvFskIdTxStream stream("IU8LMC");
        const auto total = stream.totalSamples();
        std::vector<float> chunk(311U, 7.0F);
        QCOMPARE(stream.pullFloat(chunk.data(), 19U), std::size_t {19U});
        QCOMPARE(stream.producedSamples(), std::uint64_t {19U});

        stream.cancel();
        QCOMPARE(stream.pullFloat(chunk.data(), chunk.size()), std::size_t {0U});
        QVERIFY(stream.cancelled());
        QVERIFY(!stream.complete());

        stream.reset();
        QVERIFY(!stream.cancelled());
        QCOMPARE(stream.totalSamples(), total);
        QCOMPARE(stream.producedSamples(), std::uint64_t {0U});
        QCOMPARE(stream.progress(), 0.0);
        QCOMPARE(stream.pullFloat(chunk.data(), chunk.size()), chunk.size());
    }

    void rejectsHostileConfigurationAndText()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream(""));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("lower"));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("1234567890"));

        SstvFskIdTxConfig config;
        config.sampleRate = 7'999U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));

        config = {};
        config.level = -1.0;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));
        config.level = SstvFskIdTxStream::MaximumLevel + 0.01;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));
        config.level = std::numeric_limits<double>::quiet_NaN();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));

        config = {};
        config.headroom = 0.0;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));
        config.headroom = std::numeric_limits<double>::infinity();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));

        config = {};
        config.textPolicy = static_cast<SstvFskIdCodec::TextPolicy>(255);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));
        config = {};
        config.inputHandling =
            static_cast<SstvFskIdCodec::InputHandling>(255);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvFskIdTxStream("TEST", config));
    }
};

QTEST_APPLESS_MAIN(TestSstvFskIdTxStream)
#include "test_sstv_fskid_tx_stream.moc"
