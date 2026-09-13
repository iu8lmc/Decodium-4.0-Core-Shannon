// Round-trip e casi limite per StationTelemetryCodec (pack/unpack del
// payload FT8 tipo-telemetria "stazione + meteo").
//
// By IU8LMC

#include "../src/radio/StationTelemetryCodec.hpp"

#include <QtTest/QtTest>

using namespace decodium::telemetry;

class TestStationTelemetryCodec : public QObject
{
    Q_OBJECT

private slots:
    void roundTripTypical ();
    void roundTripNegativeTemp ();
    void roundTripAllZero ();
    void roundTripAllMax ();
    void unknownTempSentinel ();
    void hexAlwaysNonZeroLeadingChar ();
    void rejectsForeignHex ();
    void rejectsWrongLength ();
    void windDirRoundTrip ();
};

void TestStationTelemetryCodec::roundTripTypical ()
{
    StationTelemetryFields in;
    in.grid4 = QStringLiteral ("JN63");
    in.tempC = 21;
    in.windSpeedKmh = 12;
    in.windDirDeg16 = windDirDegToIndex16 (315); // NW
    in.sky = SkyCondition::PartlyCloudy;
    in.powerWatts = 100;
    in.radioModelId = 3;
    in.antennaTypeId = 5;

    QString const hex = encodeStationTelemetryHex (in);
    QCOMPARE (hex.size (), 18);

    StationTelemetryFields out;
    QVERIFY (decodeStationTelemetryHex (hex, out));
    QCOMPARE (out.grid4, in.grid4);
    QCOMPARE (out.tempC, in.tempC);
    QCOMPARE (out.windSpeedKmh, in.windSpeedKmh);
    QCOMPARE (out.windDirDeg16, in.windDirDeg16);
    QCOMPARE (static_cast<int> (out.sky), static_cast<int> (in.sky));
    QCOMPARE (out.powerWatts, in.powerWatts);
    QCOMPARE (out.radioModelId, in.radioModelId);
    QCOMPARE (out.antennaTypeId, in.antennaTypeId);
}

void TestStationTelemetryCodec::roundTripNegativeTemp ()
{
    StationTelemetryFields in;
    in.grid4 = QStringLiteral ("AA00");
    in.tempC = -35;
    in.windSpeedKmh = 0;
    in.windDirDeg16 = 0;
    in.sky = SkyCondition::Snow;
    in.powerWatts = 5;
    in.radioModelId = 0;
    in.antennaTypeId = 0;

    QString const hex = encodeStationTelemetryHex (in);
    StationTelemetryFields out;
    QVERIFY (decodeStationTelemetryHex (hex, out));
    QCOMPARE (out.tempC, -35);
    QCOMPARE (static_cast<int> (out.sky), static_cast<int> (SkyCondition::Snow));
}

void TestStationTelemetryCodec::roundTripAllZero ()
{
    StationTelemetryFields in;
    in.grid4 = QStringLiteral ("AA00");
    in.tempC = -50;
    in.windSpeedKmh = 0;
    in.windDirDeg16 = 0;
    in.sky = SkyCondition::Clear;
    in.powerWatts = 0;
    in.radioModelId = 0;
    in.antennaTypeId = 0;

    QString const hex = encodeStationTelemetryHex (in);
    StationTelemetryFields out;
    QVERIFY (decodeStationTelemetryHex (hex, out));
    QCOMPARE (out.tempC, -50);
    QCOMPARE (out.windSpeedKmh, 0);
    QCOMPARE (out.powerWatts, 0);
}

void TestStationTelemetryCodec::roundTripAllMax ()
{
    StationTelemetryFields in;
    in.grid4 = QStringLiteral ("RR99");
    in.tempC = 77;
    in.windSpeedKmh = 255;
    in.windDirDeg16 = 15;
    in.sky = SkyCondition::Unknown; // enum max, non il sentinella temperatura
    in.powerWatts = 1023;
    in.radioModelId = stationRadioModelNames ().size () - 1;
    in.antennaTypeId = stationAntennaTypeNames ().size () - 1;

    QString const hex = encodeStationTelemetryHex (in);
    StationTelemetryFields out;
    QVERIFY (decodeStationTelemetryHex (hex, out));
    QCOMPARE (out.tempC, 76); // 77 sfora il range codificabile (max reale +76, 127 e' il sentinella)
    QCOMPARE (out.windSpeedKmh, 255);
    QCOMPARE (out.windDirDeg16, 15);
    QCOMPARE (out.powerWatts, 1023);
    QCOMPARE (out.radioModelId, in.radioModelId);
    QCOMPARE (out.antennaTypeId, in.antennaTypeId);
}

void TestStationTelemetryCodec::unknownTempSentinel ()
{
    StationTelemetryFields in;
    in.grid4 = QStringLiteral ("JN63");
    in.tempC = kTempUnknown;
    in.windSpeedKmh = 0;
    in.windDirDeg16 = 0;
    in.sky = SkyCondition::Unknown;
    in.powerWatts = 100;

    QString const hex = encodeStationTelemetryHex (in);
    StationTelemetryFields out;
    QVERIFY (decodeStationTelemetryHex (hex, out));
    QCOMPARE (out.tempC, kTempUnknown);
}

void TestStationTelemetryCodec::hexAlwaysNonZeroLeadingChar ()
{
    // Anche col caso "tutto zero" salvo la firma, il primo carattere non
    // deve mai essere '0' — altrimenti unpack77_cpp lo troncherebbe.
    StationTelemetryFields in;
    in.grid4 = QStringLiteral ("AA00");
    in.tempC = -50;
    in.windSpeedKmh = 0;
    in.windDirDeg16 = 0;
    in.sky = SkyCondition::Clear;
    in.powerWatts = 0;
    in.radioModelId = 0;
    in.antennaTypeId = 0;

    QString const hex = encodeStationTelemetryHex (in);
    QCOMPARE (hex.size (), 18);
    QVERIFY (hex.at (0) != QLatin1Char ('0'));
}

void TestStationTelemetryCodec::rejectsForeignHex ()
{
    // 18 caratteri esadecimali validi ma senza la nostra firma (0xC3 nei
    // primi 8 bit): deve essere rifiutato, non interpretato a caso.
    StationTelemetryFields out;
    QVERIFY (!decodeStationTelemetryHex (QStringLiteral ("000000000000000000"), out)); // 18 zeri
    QVERIFY (!decodeStationTelemetryHex (QStringLiteral ("DEADBEEFCAFE123456"), out));
}

void TestStationTelemetryCodec::rejectsWrongLength ()
{
    StationTelemetryFields out;
    QVERIFY (!decodeStationTelemetryHex (QStringLiteral ("C3"), out));
    QVERIFY (!decodeStationTelemetryHex (QString (), out));
    QVERIFY (!decodeStationTelemetryHex (QStringLiteral ("GGGGGGGGGGGGGGGGGG"), out));
}

void TestStationTelemetryCodec::windDirRoundTrip ()
{
    QCOMPARE (windDirDegToIndex16 (0), 0);
    QCOMPARE (windDirDegToIndex16 (359), 0);
    QCOMPARE (windDirDegToIndex16 (180), 8);
    QCOMPARE (windDirDegToIndex16 (315), 14);
    QCOMPARE (windDirIndex16ToLabel (0), QStringLiteral ("N"));
    QCOMPARE (windDirIndex16ToLabel (8), QStringLiteral ("S"));
    QCOMPARE (windDirIndex16ToLabel (14), QStringLiteral ("NW"));
}

QTEST_MAIN (TestStationTelemetryCodec)
#include "test_station_telemetry_codec.moc"
