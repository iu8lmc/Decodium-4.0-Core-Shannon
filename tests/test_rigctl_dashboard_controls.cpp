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

} // namespace

class TestRigctlDashboardControls final : public QObject
{
    Q_OBJECT

private slots:
    void liveRigctlCanPowerOn();
    void capabilitiesRefreshAfterRigctlOpen();
    void liveRigctlPowerOnDoesNotReopenCat();
    void networkRigctlConnectIsReadOnly();
};

void TestRigctlDashboardControls::liveRigctlCanPowerOn()
{
    const QString qml = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!qml.isEmpty());
    QVERIFY(qml.contains(QStringLiteral("bridge.hamlibCat.connected && bridge.hamlibCat.powerControlAvailable")));
    QVERIFY(qml.contains(QStringLiteral("!bridge.hamlibCat.connected\n                && (bridge.hamlibCat.portType === \"serial\"")));
}

void TestRigctlDashboardControls::capabilitiesRefreshAfterRigctlOpen()
{
    const QString source = readSource(QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("NET rigctl discovers its effective capabilities while")));
    QVERIFY(source.contains(QStringLiteral("capabilityXcv && capabilityXcv->supports_atu_control()")));
    QVERIFY(source.contains(QStringLiteral("owner->setCatCapabilities(atu, power)")));
}

void TestRigctlDashboardControls::liveRigctlPowerOnDoesNotReopenCat()
{
    const QString source = readSource(QStringLiteral("src/radio/DecodiumTransceiverManager.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral(
        "m_connected && d->transceiver && m_radioPoweredOffByUser && directCat")));
    QVERIFY(source.contains(QStringLiteral(
        "Una sessione NET rigctl, invece, deve restare aperta")));
    QVERIFY(source.contains(QStringLiteral(
        "Power On after power off: using the live CAT session")));
    QVERIFY(source.contains(QStringLiteral(
        "Power On over live network CAT: deferred state replay skipped")));
    QVERIFY(source.contains(QStringLiteral("if (!directCat)")));
    QVERIFY(source.contains(QStringLiteral("owner->m_radioPoweredOffByUser = false;")));
}

void TestRigctlDashboardControls::networkRigctlConnectIsReadOnly()
{
    const QString bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    QVERIFY(!bridge.isEmpty());
    QVERIFY(!header.isEmpty());
    QVERIFY(bridge.contains(QStringLiteral("isNetworkRigctlCat")));
    QVERIFY(bridge.contains(QStringLiteral(
        "CAT NET rigctl startup: read-only state adoption")));
    QVERIFY(bridge.contains(QStringLiteral(
        "CAT NET rigctl startup: skipped idle TX-frequency/split write")));
    QVERIFY(bridge.contains(QStringLiteral(
        "CAT NET rigctl startup: deferred mode write until state is known")));
    QVERIFY(header.contains(QStringLiteral("m_networkRigctlStartupReadOnly")));
}

QTEST_APPLESS_MAIN(TestRigctlDashboardControls)
#include "test_rigctl_dashboard_controls.moc"
