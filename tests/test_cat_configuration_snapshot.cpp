#include <QtTest>

#include "src/radio/CatConfigurationSnapshot.h"

namespace {

decodium::cat::ConfigurationSnapshot configuredSnapshot()
{
    decodium::cat::ConfigurationSnapshot snapshot;
    snapshot.rigName = QStringLiteral("Icom IC-7300");
    snapshot.serialPort = QStringLiteral("/dev/cu.usbserial-CAT");
    snapshot.networkPort = QStringLiteral("localhost:4532");
    snapshot.tciPort = QStringLiteral("localhost:50001");
    snapshot.portType = QStringLiteral("serial");
    snapshot.baudRate = 115200;
    snapshot.dataBits = QStringLiteral("8");
    snapshot.stopBits = QStringLiteral("2");
    snapshot.handshake = QStringLiteral("none");
    snapshot.forceDtr = true;
    snapshot.dtrHigh = true;
    snapshot.forceRts = true;
    snapshot.rtsHigh = false;
    snapshot.pttMethod = QStringLiteral("CAT");
    snapshot.pttPort = QStringLiteral("CAT");
    snapshot.splitMode = QStringLiteral("emulate");
    snapshot.civAddress = 0x94;
    snapshot.catKeepAlive = true;
    snapshot.pollInterval = 2;
    snapshot.tciAudioEnabled = true;
    return snapshot;
}

} // namespace

class TestCatConfigurationSnapshot final : public QObject
{
    Q_OBJECT

private slots:
    void everySessionFieldParticipatesInEquality()
    {
        const auto baseline = configuredSnapshot();
        auto changed = baseline;
        auto verify = [&baseline, &changed](auto update) {
            changed = baseline;
            update(changed);
            QVERIFY(changed != baseline);
        };

        verify([](auto& value) { value.rigName = QStringLiteral("Kenwood TS-590S"); });
        verify([](auto& value) { value.serialPort = QStringLiteral("/dev/cu.other"); });
        verify([](auto& value) { value.networkPort = QStringLiteral("127.0.0.1:4533"); });
        verify([](auto& value) { value.tciPort = QStringLiteral("127.0.0.1:50002"); });
        verify([](auto& value) { value.portType = QStringLiteral("network"); });
        verify([](auto& value) { value.baudRate = 57600; });
        verify([](auto& value) { value.dataBits = QStringLiteral("7"); });
        verify([](auto& value) { value.stopBits = QStringLiteral("1"); });
        verify([](auto& value) { value.handshake = QStringLiteral("hardware"); });
        verify([](auto& value) { value.forceDtr = false; });
        verify([](auto& value) { value.dtrHigh = false; });
        verify([](auto& value) { value.forceRts = false; });
        verify([](auto& value) { value.rtsHigh = true; });
        verify([](auto& value) { value.pttMethod = QStringLiteral("RTS"); });
        verify([](auto& value) { value.pttPort = QStringLiteral("/dev/cu.ptt"); });
        verify([](auto& value) { value.splitMode = QStringLiteral("rig"); });
        verify([](auto& value) { value.civAddress = 0x8e; });
        verify([](auto& value) { value.catKeepAlive = false; });
        verify([](auto& value) { value.pollInterval = 5; });
        verify([](auto& value) { value.tciAudioEnabled = false; });
    }

    void onlyEndpointChangesDiscardRememberedDeviceIdentity()
    {
        const auto baseline = configuredSnapshot();

        auto baudChanged = baseline;
        baudChanged.baudRate = 57600;
        QVERIFY(!baudChanged.endpointDiffers(baseline));

        auto pttChanged = baseline;
        pttChanged.pttPort = QStringLiteral("/dev/cu.ptt");
        QVERIFY(!pttChanged.endpointDiffers(baseline));

        auto serialChanged = baseline;
        serialChanged.serialPort = QStringLiteral("/dev/cu.other");
        QVERIFY(serialChanged.endpointDiffers(baseline));

        auto rigChanged = baseline;
        rigChanged.rigName = QStringLiteral("Icom IC-705");
        QVERIFY(rigChanged.endpointDiffers(baseline));

        auto networkChanged = baseline;
        networkChanged.networkPort = QStringLiteral("127.0.0.1:4532");
        QVERIFY(networkChanged.endpointDiffers(baseline));

        auto tciChanged = baseline;
        tciChanged.tciPort = QStringLiteral("127.0.0.1:50002");
        QVERIFY(tciChanged.endpointDiffers(baseline));

        auto transportChanged = baseline;
        transportChanged.portType = QStringLiteral("usb");
        QVERIFY(transportChanged.endpointDiffers(baseline));
    }
};

QTEST_APPLESS_MAIN(TestCatConfigurationSnapshot)
#include "test_cat_configuration_snapshot.moc"
