// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/integration/SstvQsoLog.h"

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QTest>
#include <QTime>
#include <QTimeZone>
#include <QUuid>

using namespace decodium::sstv;

namespace {

QString field(const QString& name, const QString& value)
{
    return QStringLiteral("<%1:%2>%3 ")
        .arg(name)
        .arg(value.toUtf8().size())
        .arg(value);
}

QString validRecord(const QString& comment = QStringLiteral("Málaga portable"))
{
    return field(QStringLiteral("CALL"), QStringLiteral("9H1TEST/P"))
        + field(QStringLiteral("GRIDSQUARE"), QStringLiteral("JM75FV"))
        + field(QStringLiteral("MODE"), QStringLiteral("SSTV"))
        + field(QStringLiteral("RST_SENT"), QStringLiteral("59"))
        + field(QStringLiteral("RST_RCVD"), QStringLiteral("57"))
        + field(QStringLiteral("QSO_DATE"), QStringLiteral("20260824"))
        + field(QStringLiteral("TIME_ON"), QStringLiteral("121314"))
        + field(QStringLiteral("QSO_DATE_OFF"), QStringLiteral("20260824"))
        + field(QStringLiteral("TIME_OFF"), QStringLiteral("121512"))
        + field(QStringLiteral("BAND"), QStringLiteral("20M"))
        + field(QStringLiteral("FREQ"), QStringLiteral("14.230000"))
        + field(QStringLiteral("STATION_CALLSIGN"), QStringLiteral("IU8LMC"))
        + field(QStringLiteral("MY_GRIDSQUARE"), QStringLiteral("JM89AE"))
        + field(QStringLiteral("COMMENT"), comment)
        + QStringLiteral("<EOR>\n");
}

SstvQsoLogRequest request()
{
    SstvQsoLogRequest value;
    value.imageRecordId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    value.remoteCallsign = QStringLiteral("9H1TEST/P");
    value.remoteGrid = QStringLiteral("JM75FV");
    value.frequencyHz = 14'230'000;
    value.timeOnUtc = QDateTime(QDate(2026, 8, 24), QTime(12, 13, 14),
                                QTimeZone(QTimeZone::UTC));
    value.timeOffUtc = value.timeOnUtc.addSecs(118);
    value.reportSent = QStringLiteral("59");
    value.reportReceived = QStringLiteral("57");
    value.comments = QStringLiteral("portable test");
    value.imageMode = QStringLiteral("Martin M1");
    return value;
}

QVariantMap requestMap()
{
    return {
        {QStringLiteral("imageRecordId"),
         QStringLiteral("2acb07fa-f7f6-46da-9f1f-38d5ab75f3e1")},
        {QStringLiteral("createNewQso"), true},
        {QStringLiteral("existingQsoId"), QString {}},
        {QStringLiteral("remoteCallsign"), QStringLiteral("9H1TEST/P")},
        {QStringLiteral("remoteGrid"), QStringLiteral("JM75FV")},
        {QStringLiteral("frequencyHz"), 14'230'000.0},
        {QStringLiteral("timeOnUtc"),
         QStringLiteral("2026-08-24T12:13:14.000Z")},
        {QStringLiteral("timeOffUtc"),
         QStringLiteral("2026-08-24T12:15:12Z")},
        {QStringLiteral("reportSent"), QStringLiteral("59")},
        {QStringLiteral("reportReceived"), QStringLiteral("57")},
        {QStringLiteral("comments"), QStringLiteral("portable test")},
        {QStringLiteral("imageMode"), QStringLiteral("Martin M1")}
    };
}

} // namespace

class TestSstvQsoLog final : public QObject
{
    Q_OBJECT

private slots:
    void validatesNewAndExistingRequests()
    {
        SstvQsoLogRequest value = request();
        QString error;
        QVERIFY2(value.validate(&error), qPrintable(error));

        const auto validated = SstvQsoLog::validateGeneratedAdif(
            validRecord(), {value.imageRecordId});
        QVERIFY2(validated.ok, qPrintable(validated.error));

        value.createNewQso = false;
        value.existingQsoId = validated.associationId;
        value.remoteCallsign.clear();
        value.remoteGrid.clear();
        value.frequencyHz = 0;
        value.timeOnUtc = {};
        value.timeOffUtc = {};
        QVERIFY2(value.validate(&error), qPrintable(error));
    }

    void rejectsInvalidRequests()
    {
        QString error;
        SstvQsoLogRequest value = request();

        value.imageRecordId = QStringLiteral("not-a-uuid");
        QVERIFY(!value.validate(&error));
        value = request();
        value.imageMode = QStringLiteral("/tmp/martin-m1.png");
        QVERIFY(!value.validate(&error));
        value = request();
        value.remoteCallsign = QStringLiteral("NOT A CALL");
        QVERIFY(!value.validate(&error));
        value = request();
        value.remoteGrid = QStringLiteral("ZZ99ZZ");
        QVERIFY(!value.validate(&error));
        value = request();
        value.frequencyHz = 0;
        QVERIFY(!value.validate(&error));
        value = request();
        value.timeOffUtc = value.timeOnUtc.addSecs(-1);
        QVERIFY(!value.validate(&error));
        value = request();
        value.comments = QStringLiteral("file:///Users/operator/image.png");
        QVERIFY(!value.validate(&error));
        value = request();
        value.existingQsoId = QStringLiteral("unexpected");
        QVERIFY(!value.validate(&error));
        value = request();
        value.createNewQso = false;
        value.existingQsoId = QStringLiteral("adif-sha256:short");
        QVERIFY(!value.validate(&error));
        value.existingQsoId = QStringLiteral("adif-sha256:")
            + QString(64, QLatin1Char('z'));
        QVERIFY(!value.validate(&error));
    }

    void parsesStrictPathFreeVariantContract()
    {
        QString error;
        SstvQsoLogRequest parsed;
        const QVariantMap values = requestMap();
        QVERIFY2(SstvQsoLog::requestFromVariantMap(
                     values, &parsed, &error), qPrintable(error));
        QCOMPARE(parsed.imageRecordId,
                 QStringLiteral("2acb07fa-f7f6-46da-9f1f-38d5ab75f3e1"));
        QCOMPARE(parsed.frequencyHz, 14'230'000LL);
        QCOMPARE(parsed.timeOnUtc.timeSpec(), Qt::UTC);
        QCOMPARE(parsed.timeOnUtc.toString(Qt::ISODateWithMs),
                 QStringLiteral("2026-08-24T12:13:14.000Z"));

        QVariantMap existing {
            {QStringLiteral("imageRecordId"), parsed.imageRecordId},
            {QStringLiteral("createNewQso"), false},
            {QStringLiteral("existingQsoId"),
             QStringLiteral("adif-sha256:")
                 + QString(64, QLatin1Char('a'))},
            {QStringLiteral("imageMode"), QStringLiteral("Martin M1")}
        };
        QVERIFY2(SstvQsoLog::requestFromVariantMap(
                     existing, &parsed, &error), qPrintable(error));
        QVERIFY(!parsed.createNewQso);
        QCOMPARE(parsed.frequencyHz, 0);
        QVERIFY(!parsed.timeOnUtc.isValid());
    }

    void rejectsVariantCoercionPathsAndImplicitTimes()
    {
        QString error;
        SstvQsoLogRequest parsed;

        QVariantMap values = requestMap();
        values.insert(QStringLiteral("imagePath"),
                      QStringLiteral("/Users/operator/frame.png"));
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("createNewQso"),
                      QStringLiteral("false"));
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("frequencyHz"), 14'230'000.5);
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("frequencyHz"),
                      QStringLiteral("14230000"));
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("comments"), 1234);
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("timeOnUtc"),
                      QDateTime(QDate(2026, 8, 24), QTime(12, 13, 14),
                                QTimeZone(QTimeZone::UTC)));
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("timeOnUtc"),
                      QStringLiteral("2026-08-24T12:13:14"));
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));

        values = requestMap();
        values.insert(QStringLiteral("comments"),
                      QStringLiteral("file:///tmp/frame.png"));
        QVERIFY(!SstvQsoLog::requestFromVariantMap(
            values, &parsed, &error));
    }

    void validatesFinalSerializedAdifAndByteLengths()
    {
        const auto result = SstvQsoLog::validateGeneratedAdif(validRecord());
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.fields.value(QStringLiteral("MODE")),
                 QStringLiteral("SSTV"));
        QVERIFY(!result.fields.contains(QStringLiteral("SUBMODE")));
        QCOMPARE(result.fields.value(QStringLiteral("COMMENT")),
                 QStringLiteral("Málaga portable"));
        QVERIFY(result.associationId.startsWith(QStringLiteral("adif-sha256:")));
        QCOMPARE(result.associationId.size(), 76);

        QString bridgePayload = validRecord();
        bridgePayload.chop(QStringLiteral("<EOR>\n").size());
        const auto withoutEor = SstvQsoLog::validateGeneratedAdif(
            bridgePayload);
        QVERIFY2(withoutEor.ok, qPrintable(withoutEor.error));
        QCOMPARE(withoutEor.associationId, result.associationId);
    }

    void associationIdentityIsCanonicalAndMatchesExistingRows()
    {
        const auto result = SstvQsoLog::validateGeneratedAdif(validRecord());
        QVERIFY(result.ok);
        QString error;
        const QString fromExisting = SstvQsoLog::associationIdForExistingQso(
            QStringLiteral("9h1test/p"),
            QStringLiteral("2026-08-24 12:13:14"),
            QStringLiteral("sstv"),
            QStringLiteral("20m"), &error);
        QVERIFY2(!fromExisting.isEmpty(), qPrintable(error));
        QCOMPARE(fromExisting, result.associationId);

        QMap<QString, QString> fields = result.fields;
        fields.insert(QStringLiteral("TIME_ON"), QStringLiteral("1213"));
        const QString minutePrecision = SstvQsoLog::associationIdForFields(
            fields, &error);
        QCOMPARE(minutePrecision,
                 SstvQsoLog::associationIdForExistingQso(
                     QStringLiteral("9H1TEST/P"),
                     QStringLiteral("2026-08-24 12:13:00"),
                     QStringLiteral("SSTV"), QStringLiteral("20M"), &error));
    }

    void rejectsNonCompliantOrLeakingAdif_data()
    {
        QTest::addColumn<QString>("record");
        QTest::addColumn<QStringList>("forbidden");

        QTest::newRow("wrong-mode")
            << validRecord().replace(QStringLiteral("<MODE:4>SSTV"),
                                     QStringLiteral("<MODE:3>FAX"))
            << QStringList {};
        QTest::newRow("invented-submode")
            << (validRecord()
                + field(QStringLiteral("SUBMODE"), QStringLiteral("MARTIN_M1")))
            << QStringList {};
        QTest::newRow("path-field")
            << (validRecord()
                + field(QStringLiteral("APP_DECODIUM_IMAGE_PATH"),
                        QStringLiteral("gallery/image.png")))
            << QStringList {};
        QTest::newRow("file-uri-comment")
            << validRecord(QStringLiteral(
                   "operator attachment file:///Users/test/image.png"))
            << QStringList {};
        QTest::newRow("windows-path-comment")
            << validRecord(QStringLiteral("C:\\Users\\test\\image.png"))
            << QStringList {};
        QTest::newRow("forbidden-image-id")
            << validRecord(QStringLiteral(
                   "attachment 12345678-1234-1234-1234-123456789abc"))
            << QStringList {QStringLiteral(
                   "12345678-1234-1234-1234-123456789abc")};
        QTest::newRow("invalid-byte-length")
            << validRecord().replace(QStringLiteral("<COMMENT:16>"),
                                     QStringLiteral("<COMMENT:15>"))
            << QStringList {};
        QTest::newRow("duplicate-call")
            << (validRecord() + field(QStringLiteral("CALL"),
                                      QStringLiteral("9H1TEST/P")))
            << QStringList {};
        QTest::newRow("invalid-date")
            << validRecord().replace(QStringLiteral("20260824"),
                                     QStringLiteral("20261340"))
            << QStringList {};
        QTest::newRow("invalid-time")
            << validRecord().replace(QStringLiteral("121314"),
                                     QStringLiteral("256199"))
            << QStringList {};
        QTest::newRow("oob-band")
            << validRecord().replace(QStringLiteral("<BAND:3>20M"),
                                     QStringLiteral("<BAND:3>OOB"))
            << QStringList {};
        QTest::newRow("invalid-freq")
            << validRecord().replace(QStringLiteral("<FREQ:9>14.230000"),
                                     QStringLiteral("<FREQ:9>00.000000"))
            << QStringList {};
    }

    void rejectsNonCompliantOrLeakingAdif()
    {
        QFETCH(QString, record);
        QFETCH(QStringList, forbidden);
        const auto result = SstvQsoLog::validateGeneratedAdif(record,
                                                              forbidden);
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
    }

    void createsBoundedModeCommentWithoutAttachmentData()
    {
        QString error;
        const QString comment = SstvQsoLog::mergedComment(
            QStringLiteral("good colour, some QRM"),
            QStringLiteral("Scottie S1"), &error);
        QVERIFY2(!comment.isEmpty(), qPrintable(error));
        QCOMPARE(comment,
                 QStringLiteral(
                     "SSTV image mode: Scottie S1 | good colour, some QRM"));
        QVERIFY(SstvQsoLog::mergedComment(
                    QStringLiteral("file:///tmp/frame.png"),
                    QStringLiteral("Scottie S1"), &error).isEmpty());
    }
};

QTEST_MAIN(TestSstvQsoLog)

#include "test_sstv_qso_log.moc"
