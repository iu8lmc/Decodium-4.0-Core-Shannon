// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvAvtSyncCodec.h"

#include <array>
#include <cstdint>

using namespace decodium::sstv;

namespace {

std::array<double, SstvAvtSyncCodec::TonesPerFrame> frequencies(
    const SstvAvtSyncFrame& frame)
{
    std::array<double, SstvAvtSyncCodec::TonesPerFrame> result {};
    for (std::size_t index = 0U; index < result.size(); ++index) {
        result[index] = frame.tones[index].frequencyHz;
    }
    return result;
}

} // namespace

class TestSstvAvtSync final : public QObject
{
    Q_OBJECT

private slots:
    void exactTimingAndVisVariants()
    {
        QCOMPARE(SstvAvtSyncCodec::SymbolDuration.count,
                 std::int64_t {9'765'625'000LL});
        QCOMPARE(SstvAvtSyncCodec::FrameDuration.count,
                 std::int64_t {166'015'625'000LL});
        QCOMPARE(SstvAvtSyncCodec::CountdownDuration.count,
                 std::int64_t {5'312'500'000'000LL});

        const std::array<SstvAvtMode, 3U> modes {{
            SstvAvtMode::Avt24,
            SstvAvtMode::Avt90,
            SstvAvtMode::Avt94}};
        const std::array<std::uint8_t, 3U> bases {{64U, 68U, 72U}};
        for (std::size_t modeIndex = 0U;
             modeIndex < modes.size();
             ++modeIndex) {
            for (std::uint8_t variant = 0U; variant < 4U; ++variant) {
                const auto typed = static_cast<SstvAvtVariant>(variant);
                const std::uint8_t payload = SstvAvtSyncCodec::visPayload(
                    modes[modeIndex], typed);
                QCOMPARE(payload,
                         static_cast<std::uint8_t>(bases[modeIndex] + variant));
                QVERIFY(SstvAvtSyncCodec::modeForVis(payload).has_value());
                QCOMPARE(*SstvAvtSyncCodec::modeForVis(payload),
                         modes[modeIndex]);
                QCOMPARE(SstvAvtSyncCodec::variantForVis(payload), typed);
                QCOMPARE(SstvAvtSyncCodec::isNarrow(typed),
                         (variant & 1U) != 0U);
                QCOMPARE(SstvAvtSyncCodec::isQrm(typed),
                         (variant & 2U) != 0U);
            }
        }
        QVERIFY(!SstvAvtSyncCodec::modeForVis(63U).has_value());
        QVERIFY(!SstvAvtSyncCodec::modeForVis(76U).has_value());
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAvtSyncCodec::variantForVis(0U));
    }

    void wordsAreMsbFirstAndExactlyComplemented()
    {
        const SstvAvtSyncFrame first = SstvAvtSyncCodec::encodeFrame(
            SstvAvtMode::Avt24, 0U, false);
        QCOMPARE(first.normalWord, std::uint8_t {0x40U});
        QCOMPARE(first.invertedWord, std::uint8_t {0xbfU});
        QCOMPARE(first.tones[0].frequencyHz, 1'900.0);
        const std::array<double, 8U> normal {{
            1'600.0, 2'200.0, 1'600.0, 1'600.0,
            1'600.0, 1'600.0, 1'600.0, 1'600.0}};
        for (std::size_t index = 0U; index < normal.size(); ++index) {
            QCOMPARE(first.tones[index + 1U].frequencyHz, normal[index]);
            QCOMPARE(first.tones[index + 9U].frequencyHz,
                     normal[index] == 1'600.0 ? 2'200.0 : 1'600.0);
            QCOMPARE(first.tones[index + 1U].duration,
                     SstvAvtSyncCodec::SymbolDuration);
        }

        const SstvAvtSyncFrame last = SstvAvtSyncCodec::encodeFrame(
            SstvAvtMode::Avt90, 31U, false);
        QCOMPARE(last.normalWord, std::uint8_t {0xbfU});
        QCOMPARE(last.invertedWord, std::uint8_t {0x40U});

        const SstvAvtSyncFrame narrow = SstvAvtSyncCodec::encodeFrame(
            SstvAvtMode::Avt94, 17U, true);
        QCOMPARE(narrow.normalWord, std::uint8_t {0x71U});
        for (std::size_t index = 1U; index < narrow.tones.size(); ++index) {
            QVERIFY(narrow.tones[index].frequencyHz == 1'700.0
                    || narrow.tones[index].frequencyHz == 2'100.0);
        }
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAvtSyncCodec::encodeFrame(
                SstvAvtMode::Avt24, 32U, false));
    }

    void anyValidFrameRecoversModeAndImageDelay()
    {
        for (const SstvAvtMode mode : {
                 SstvAvtMode::Avt24,
                 SstvAvtMode::Avt90,
                 SstvAvtMode::Avt94}) {
            for (std::uint8_t counter = 0U; counter < 32U; ++counter) {
                const SstvAvtSyncFrame encoded =
                    SstvAvtSyncCodec::encodeFrame(mode, counter, false);
                auto observed = frequencies(encoded);
                for (double& frequency : observed) {
                    frequency += 75.0;
                }
                const SstvAvtDecodedSyncFrame decoded =
                    SstvAvtSyncCodec::decodeFrame(observed, false, 120.0);
                QVERIFY(decoded.valid);
                QVERIFY(decoded.mode.has_value());
                QCOMPARE(*decoded.mode, mode);
                QCOMPARE(decoded.counter, counter);
                QCOMPARE(decoded.remainingFrames,
                         static_cast<std::uint8_t>(31U - counter));
                QCOMPARE(decoded.remainingDuration.count,
                         SstvAvtSyncCodec::FrameDuration.count
                             * decoded.remainingFrames);
                QVERIFY(decoded.confidence > 0.0);
                QVERIFY(decoded.confidence <= 1.0);
            }
        }
    }

    void corruptionAndAmbiguousTonesFailClosed()
    {
        const SstvAvtSyncFrame frame = SstvAvtSyncCodec::encodeFrame(
            SstvAvtMode::Avt90, 12U, false);
        auto observed = frequencies(frame);
        observed[9] = observed[9] == 1'600.0 ? 2'200.0 : 1'600.0;
        QVERIFY(!SstvAvtSyncCodec::decodeFrame(observed).valid);

        observed = frequencies(frame);
        observed[0] = 1'500.0;
        QVERIFY(!SstvAvtSyncCodec::decodeFrame(observed).valid);

        observed = frequencies(frame);
        observed[4] = 1'900.0;
        QVERIFY(!SstvAvtSyncCodec::decodeFrame(observed).valid);

        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAvtSyncCodec::decodeFrame(
                observed.data(), observed.size() - 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAvtSyncCodec::decodeFrame(observed, false, 0.0));
    }

    void fullCountdownIsBoundedAndMonotonic()
    {
        const auto tones = SstvAvtSyncCodec::encodeCountdown(
            SstvAvtMode::Avt94, false);
        QCOMPARE(tones.size(), SstvAvtSyncCodec::CountdownToneCount);
        std::int64_t total = 0LL;
        for (std::size_t frameIndex = 0U;
             frameIndex < SstvAvtSyncCodec::FrameCount;
             ++frameIndex) {
            std::array<double, SstvAvtSyncCodec::TonesPerFrame> observed {};
            for (std::size_t tone = 0U; tone < observed.size(); ++tone) {
                const auto& value = tones[
                    frameIndex * observed.size() + tone];
                observed[tone] = value.frequencyHz;
                total += value.duration.count;
            }
            const auto decoded = SstvAvtSyncCodec::decodeFrame(observed);
            QVERIFY(decoded.valid);
            QCOMPARE(decoded.counter,
                     static_cast<std::uint8_t>(frameIndex));
        }
        QCOMPARE(total, SstvAvtSyncCodec::CountdownDuration.count);
    }
};

QTEST_APPLESS_MAIN(TestSstvAvtSync)

#include "test_sstv_avt_sync.moc"
