#include "src/radio/CatSerialOpenSafety.h"

#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

decodium::cat::SerialOpenLineSafetyInput ordinarySerialCat()
{
    decodium::cat::SerialOpenLineSafetyInput input;
    input.serialCat = true;
    input.canForceDtr = true;
    input.canForceRts = true;
    return input;
}

} // namespace

class TestCatSerialOpenSafety final : public QObject
{
    Q_OBJECT

private slots:
    void protectsUnusedControlLinesOnEveryPlatform();
    void preservesExplicitLineRequests();
    void neverStealsAPttLineOrHardwareHandshake();
    void defaultHandshakeDoesNotForceLines();
    void managerUsesTheSharedOpeningPolicy();
};

void TestCatSerialOpenSafety::protectsUnusedControlLinesOnEveryPlatform()
{
    const auto safety = decodium::cat::serialOpenLineSafety(ordinarySerialCat());
    QVERIFY(safety.automaticDtrLow);
    QVERIFY(safety.automaticRtsLow);
    QVERIFY(safety.forceDtr);
    QVERIFY(!safety.dtrHigh);
    QVERIFY(safety.forceRts);
    QVERIFY(!safety.rtsHigh);
}

void TestCatSerialOpenSafety::preservesExplicitLineRequests()
{
    auto input = ordinarySerialCat();
    input.userForceDtr = true;
    input.userDtrHigh = true;
    input.userForceRts = true;
    input.userRtsHigh = true;

    const auto safety = decodium::cat::serialOpenLineSafety(input);
    QVERIFY(!safety.automaticDtrLow);
    QVERIFY(!safety.automaticRtsLow);
    QVERIFY(safety.forceDtr);
    QVERIFY(safety.dtrHigh);
    QVERIFY(safety.forceRts);
    QVERIFY(safety.rtsHigh);
}

void TestCatSerialOpenSafety::neverStealsAPttLineOrHardwareHandshake()
{
    auto dtrPtt = ordinarySerialCat();
    dtrPtt.dtrIsPttOnCatPort = true;
    const auto dtrSafety = decodium::cat::serialOpenLineSafety(dtrPtt);
    QVERIFY(!dtrSafety.forceDtr);
    QVERIFY(!dtrSafety.automaticDtrLow);
    QVERIFY(dtrSafety.forceRts);

    auto rtsPtt = ordinarySerialCat();
    rtsPtt.rtsIsPttOnCatPort = true;
    const auto rtsSafety = decodium::cat::serialOpenLineSafety(rtsPtt);
    QVERIFY(!rtsSafety.forceRts);
    QVERIFY(!rtsSafety.automaticRtsLow);
    QVERIFY(rtsSafety.forceDtr);

    auto hardware = ordinarySerialCat();
    hardware.hardwareHandshake = true;
    const auto hardwareSafety = decodium::cat::serialOpenLineSafety(hardware);
    QVERIFY(!hardwareSafety.forceRts);
    QVERIFY(!hardwareSafety.automaticRtsLow);
    QVERIFY(hardwareSafety.forceDtr);
}

void TestCatSerialOpenSafety::defaultHandshakeDoesNotForceLines()
{
    // The manager represents an unresolved/default handshake by disabling
    // both force capabilities before this shared opening policy is called.
    auto input = ordinarySerialCat();
    input.canForceDtr = false;
    input.canForceRts = false;
    const auto safety = decodium::cat::serialOpenLineSafety(input);
    QVERIFY(!safety.automaticDtrLow);
    QVERIFY(!safety.automaticRtsLow);
    QVERIFY(!safety.forceDtr);
    QVERIFY(!safety.forceRts);
}

void TestCatSerialOpenSafety::managerUsesTheSharedOpeningPolicy()
{
    const QString source = readSource(QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("CatSerialOpenSafety.h")));
    QVERIFY(source.contains(QStringLiteral("SerialOpenLineSafetyInput lineSafetyInput")));
    QVERIFY(source.contains(QStringLiteral("serialOpenLineSafety(lineSafetyInput)")));
    QVERIFY(source.contains(QStringLiteral("d3ForceDtrAvailable(m->portType(), m->handshake()")));
    QVERIFY(source.contains(QStringLiteral("defaultHandshake")));
    QVERIFY(source.contains(QStringLiteral("autoDtrLow=")));
    QVERIFY(source.contains(QStringLiteral("autoRtsLow=")));
}

QTEST_APPLESS_MAIN(TestCatSerialOpenSafety)
#include "test_cat_serial_open_safety.moc"
