// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include "../../src/sstv/core/SstvVisCodec.h"

#include <cstdint>

using namespace decodium::sstv;

class TestSstvMmsstvVis final : public QObject
{
    Q_OBJECT

private slots:
    void everyRequiredWideModeHasExactExtendedCodeword()
    {
#ifndef DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE
#error "MMSSTV extended protocol fixture path is required"
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
        const QJsonObject extended = document.object()
                                         .value(QStringLiteral(
                                             "wideExtendedVis"))
                                         .toObject();
        QCOMPARE(extended.value(
                     QStringLiteral("headerDurationUs")).toInteger(),
                 qint64 {1'150'000});
        QCOMPARE(extended.value(
                     QStringLiteral("markerRawOctet")).toInteger(),
                 qint64 {0x23});
        QCOMPARE(extended.value(QStringLiteral("bitOrder")).toString(),
                 QStringLiteral("LSB-first"));

        const QJsonArray modes = extended.value(QStringLiteral("modes"))
                                     .toArray();
        QCOMPARE(modes.size(), qsizetype {13});
        QSet<int> rawOctets;
        for (const QJsonValue value : modes) {
            const QJsonObject row = value.toObject();
            const int raw = static_cast<int>(row.value(
                QStringLiteral("extensionRawOctet")).toInteger(-1));
            QVERIFY2(raw >= 0 && raw <= 0xff,
                     qPrintable(row.value(QStringLiteral("id")).toString()));
            rawOctets.insert(raw);
            const std::uint8_t payload = static_cast<std::uint8_t>(
                raw & SstvVisCodec::PayloadMask);
            const SstvVisEncodedFrame encoded =
                SstvVisCodec::encodeExtended(payload);
            QCOMPARE(encoded.format, SstvVisFormat::Extended);
            QCOMPARE(encoded.primary.rawOctet,
                     SstvVisCodec::ExtendedMarkerRawOctet);
            QVERIFY(encoded.extension.has_value());
            QCOMPARE(encoded.extension->rawOctet,
                     static_cast<std::uint8_t>(raw));

            const SstvVisDecodeResult decoded =
                SstvVisCodec::decodeFrame(encoded.symbols);
            QVERIFY(decoded.valid);
            QCOMPARE(decoded.format, SstvVisFormat::Extended);
            QVERIFY(decoded.extension.has_value());
            QCOMPARE(decoded.extension->rawOctet,
                     static_cast<std::uint8_t>(raw));
        }
        QCOMPARE(rawOctets.size(), modes.size());
        QVERIFY(rawOctets.contains(0x4a));
        QVERIFY(rawOctets.contains(0x4c));
    }
};

QTEST_APPLESS_MAIN(TestSstvMmsstvVis)

#include "test_sstv_mmsstv_vis.moc"
