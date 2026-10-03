#include "src/bridge/AdifExportSanitizer.h"

#include <QtTest>

class AdifExportSanitizerTest : public QObject
{
    Q_OBJECT

private slots:
    void omitsUnsetMyIota();
    void preservesValidMyIotaAndOtherNoneValues();
};

void AdifExportSanitizerTest::omitsUnsetMyIota()
{
    const QByteArray input =
        "Decodium ADIF\n<EOH>\n"
        "<CALL:5>LY3PW <MY_IOTA:4>NONE <MY_COUNTRY:5>Italy <EOR>\n";
    const QByteArray output = decodium::adif::sanitizeExport(input);

    QVERIFY(!output.contains("MY_IOTA"));
    QVERIFY(output.contains("<CALL:5>LY3PW"));
    QVERIFY(output.contains("<MY_COUNTRY:5>Italy"));
    QCOMPARE(output.count("<EOR>"), 1);
}

void AdifExportSanitizerTest::preservesValidMyIotaAndOtherNoneValues()
{
    const QByteArray input =
        "<my_iota:6:STRING>EU-005 <COMMENT:4>NONE <EOR>\n";
    const QByteArray output = decodium::adif::sanitizeExport(input);

    QVERIFY(output.contains("<my_iota:6:STRING>EU-005"));
    QVERIFY(output.contains("<COMMENT:4>NONE"));
    QVERIFY(decodium::adif::isInvalidUnsetValue("my_iota", " NONE "));
    QVERIFY(!decodium::adif::isInvalidUnsetValue("comment", "NONE"));
}

QTEST_MAIN(AdifExportSanitizerTest)
#include "test_adif_export_sanitizer.moc"
