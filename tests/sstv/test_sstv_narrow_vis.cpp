// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../../src/sstv/core/SstvNarrowVisCodec.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace decodium::sstv;

namespace {

struct Fixture final
{
    const char* id;
    SstvNarrowVisMode mode;
    std::uint8_t payload;
    std::uint32_t packed;
};

constexpr std::array<Fixture, 6U> kFixtures {{
    {"mp-73-narrow", SstvNarrowVisMode::Mp73, 0x02U, 0x5c256dU},
    {"mp-110-narrow", SstvNarrowVisMode::Mp110, 0x04U, 0x44456dU},
    {"mp-140-narrow", SstvNarrowVisMode::Mp140, 0x05U, 0x40556dU},
    {"mc-110-narrow", SstvNarrowVisMode::Mc110, 0x14U, 0x05456dU},
    {"mc-140-narrow", SstvNarrowVisMode::Mc140, 0x15U, 0x01556dU},
    {"mc-180-narrow", SstvNarrowVisMode::Mc180, 0x16U, 0x0d656dU},
}};

std::array<double, SstvNarrowVisCodec::ToneCount> frequencies(
    const SstvNarrowVisEncodedFrame& frame)
{
    std::array<double, SstvNarrowVisCodec::ToneCount> result {};
    for (std::size_t index = 0U; index < result.size(); ++index) {
        result[index] = frame.tones[index].frequencyHz;
    }
    return result;
}

void flip(std::array<double, SstvNarrowVisCodec::ToneCount>& observed,
          std::size_t dataBit)
{
    const std::size_t index = dataBit + 3U;
    observed[index] = observed[index] == SstvNarrowVisCodec::OneFrequencyHz
                          ? SstvNarrowVisCodec::ZeroFrequencyHz
                          : SstvNarrowVisCodec::OneFrequencyHz;
}

} // namespace

class TestSstvNarrowVis final : public QObject
{
    Q_OBJECT

private slots:
    void authoritativeModeMappingsAndWireOrder()
    {
        for (const Fixture& fixture : kFixtures) {
            QCOMPARE(SstvNarrowVisCodec::payloadForMode(fixture.mode),
                     fixture.payload);
            QVERIFY(SstvNarrowVisCodec::modeForPayload(
                        fixture.payload).has_value());
            QCOMPARE(*SstvNarrowVisCodec::modeForPayload(fixture.payload),
                     fixture.mode);
            const auto frame = SstvNarrowVisCodec::encode(fixture.mode);
            QCOMPARE(frame.payload, fixture.payload);
            QCOMPARE(frame.groups[0U], std::uint8_t {0x2dU});
            QCOMPARE(frame.groups[1U], std::uint8_t {0x15U});
            QCOMPARE(frame.groups[2U], fixture.payload);
            QCOMPARE(frame.groups[3U],
                     static_cast<std::uint8_t>(0x15U ^ fixture.payload));
            QCOMPARE(frame.packedWireValue, fixture.packed);
        }
        QVERIFY(!SstvNarrowVisCodec::modeForPayload(0x03U).has_value());
    }

    void pinnedMmsstvAndQsstvLandmarksMatchCodec()
    {
#ifndef DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE
#error "Narrow VIS protocol fixture path is required"
#endif
        QFile file(QString::fromUtf8(
            DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE));
        QVERIFY2(file.open(QIODevice::ReadOnly),
                 qPrintable(file.errorString()));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            file.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());
        const QJsonObject narrow = document.object()
                                       .value(QStringLiteral("narrowVis"))
                                       .toObject();
        QCOMPARE(narrow.value(QStringLiteral("headerDurationUs")).toInteger(),
                 qint64 {950'000});
        QCOMPARE(narrow.value(QStringLiteral("groupBitOrder")).toString(),
                 QStringLiteral("LSB-first"));
        const QJsonArray modes = narrow.value(QStringLiteral("modes"))
                                     .toArray();
        QCOMPARE(modes.size(), static_cast<qsizetype>(kFixtures.size()));
        for (const Fixture& fixture : kFixtures) {
            QJsonObject row;
            for (const QJsonValue value : modes) {
                const QJsonObject candidate = value.toObject();
                if (candidate.value(QStringLiteral("id")).toString()
                    == QString::fromLatin1(fixture.id)) {
                    row = candidate;
                    break;
                }
            }
            QVERIFY2(!row.isEmpty(), fixture.id);
            QCOMPARE(row.value(QStringLiteral("payload")).toInteger(),
                     qint64 {fixture.payload});
            QCOMPARE(row.value(
                         QStringLiteral("packedWireValue")).toInteger(),
                     qint64 {fixture.packed});
            const auto encoded = SstvNarrowVisCodec::encode(fixture.mode);
            QCOMPARE(encoded.payload, fixture.payload);
            QCOMPARE(encoded.packedWireValue, fixture.packed);
        }
    }

    void exactPhysicalHeaderAndDuration()
    {
        const auto frame = SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mp73);
        QCOMPARE(frame.tones[0U].frequencyHz, 1'900.0);
        QCOMPARE(frame.tones[0U].duration.count,
                 std::int64_t {300'000'000'000LL});
        QCOMPARE(frame.tones[1U].frequencyHz, 2'100.0);
        QCOMPARE(frame.tones[1U].duration.count,
                 std::int64_t {100'000'000'000LL});
        QCOMPARE(frame.tones[2U].frequencyHz, 1'900.0);
        QCOMPARE(frame.tones[2U].duration.count,
                 std::int64_t {22'000'000'000LL});
        std::int64_t total = 0LL;
        for (const auto& tone : frame.tones) {
            total += tone.duration.count;
        }
        QCOMPARE(total, SstvNarrowVisCodec::FrameDuration.count);

        // 0x2d is emitted LSB-first: 1,0,1,1,0,1.
        const std::array<double, 6U> preamble {{
            1'900.0, 2'100.0, 1'900.0,
            1'900.0, 2'100.0, 1'900.0}};
        for (std::size_t index = 0U; index < preamble.size(); ++index) {
            QCOMPARE(frame.tones[index + 3U].frequencyHz, preamble[index]);
        }
    }

    void allModesRoundTripWithFrequencyOffset()
    {
        for (const Fixture& fixture : kFixtures) {
            const auto encoded = SstvNarrowVisCodec::encode(fixture.mode);
            auto observed = frequencies(encoded);
            for (double& frequency : observed) {
                frequency += 60.0;
            }
            const auto decoded = SstvNarrowVisCodec::decode(observed, 90.0);
            QVERIFY(decoded.valid);
            QCOMPARE(decoded.error, SstvNarrowVisError::None);
            QVERIFY(decoded.mode.has_value());
            QCOMPARE(*decoded.mode, fixture.mode);
            QCOMPARE(decoded.payload, fixture.payload);
            QCOMPARE(decoded.packedWireValue, fixture.packed);
            QVERIFY(decoded.confidence > 0.0);
            QVERIFY(decoded.confidence < 1.0);
        }
    }

    void corruptionFailsClosedWithSpecificCause()
    {
        auto observed = frequencies(SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mp110));
        observed[0U] = 1'700.0;
        QCOMPARE(SstvNarrowVisCodec::decode(observed).error,
                 SstvNarrowVisError::InvalidHeader);

        observed = frequencies(SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mp110));
        observed[8U] = 2'000.0;
        QCOMPARE(SstvNarrowVisCodec::decode(observed).error,
                 SstvNarrowVisError::InvalidSymbol);

        observed = frequencies(SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mp110));
        flip(observed, 0U);
        QCOMPARE(SstvNarrowVisCodec::decode(observed).error,
                 SstvNarrowVisError::InvalidPreamble);

        observed = frequencies(SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mp110));
        flip(observed, 18U);
        QCOMPARE(SstvNarrowVisCodec::decode(observed).error,
                 SstvNarrowVisError::ComplementMismatch);

        observed = frequencies(SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mp110));
        // Change payload 0x04 into unsupported 0x06 and keep its XOR valid.
        flip(observed, 13U);
        flip(observed, 19U);
        const auto unknown = SstvNarrowVisCodec::decode(observed);
        QVERIFY(!unknown.valid);
        QCOMPARE(unknown.payload, std::uint8_t {0x06U});
        QCOMPARE(unknown.error, SstvNarrowVisError::UnknownMode);
    }

    void hostileBoundsAreRejected()
    {
        const auto frame = SstvNarrowVisCodec::encode(
            SstvNarrowVisMode::Mc180);
        auto observed = frequencies(frame);
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvNarrowVisCodec::decode(
                observed.data(), observed.size() - 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvNarrowVisCodec::decode(observed, 100.0));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvNarrowVisCodec::decode(observed, 0.0));

        observed[10U] = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(SstvNarrowVisCodec::decode(observed).error,
                 SstvNarrowVisError::InvalidSymbol);
    }
};

QTEST_APPLESS_MAIN(TestSstvNarrowVis)

#include "test_sstv_narrow_vis.moc"
