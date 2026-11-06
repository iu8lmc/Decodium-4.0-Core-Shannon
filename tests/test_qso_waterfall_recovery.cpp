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

QString functionBody(const QString& source, const QString& signature,
                     const QString& nextSignature)
{
    const int start = source.indexOf(signature);
    if (start < 0)
        return {};
    const int end = source.indexOf(nextSignature, start + signature.size());
    return source.mid(start, end > start ? end - start : -1);
}

} // namespace

class TestQsoWaterfallRecovery final : public QObject
{
    Q_OBJECT

private slots:
    void bridgeExposesDedicatedVisualRearm();
    void successfulLogPathsNotifyWaterfall();
    void qmlKeepsWaterfallItemAndRearmsAfterLayoutSettles();
};

void TestQsoWaterfallRecovery::bridgeExposesDedicatedVisualRearm()
{
    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!header.isEmpty());
    QVERIFY(!cpp.isEmpty());
    QVERIFY(header.contains(QStringLiteral("Q_INVOKABLE void rearmSpectrumAfterQsoLog();")));
    QVERIFY(header.contains(QStringLiteral("void qsoLogCompleted(const QString& call);")));

    const QString helper = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::rearmSpectrumAfterQsoLog()"),
        QStringLiteral("bool DecodiumBridge::cpuPressureActive() const"));
    QVERIFY(!helper.isEmpty());
    QVERIFY(helper.contains(QStringLiteral("!m_monitoring")));
    QVERIFY(helper.contains(QStringLiteral("m_lastPanadapterFrameMs = nowMs - 1000")));
    QVERIFY(helper.contains(QStringLiteral("m_spectrumTimer->start()")));
    QVERIFY(!helper.contains(QStringLiteral("stopAudioCapture()")));
    QVERIFY(!helper.contains(QStringLiteral("startAudioCapture()")));
}

void TestQsoWaterfallRecovery::successfulLogPathsNotifyWaterfall()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());
    const QString body = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::logQsoNow()"),
        QStringLiteral("QString DecodiumBridge::logAllTxtPath() const"));
    QVERIFY(!body.isEmpty());

    const int legacyNotification = body.indexOf(
        QStringLiteral("emit qsoLogCompleted(legacyCompletedCall);"));
    const int nativeNotification = body.indexOf(
        QStringLiteral("emit qsoLogCompleted(logDxCall);"));
    QVERIFY(legacyNotification >= 0);
    QVERIFY(nativeNotification > legacyNotification);
    QVERIFY(body.indexOf(QStringLiteral("appendAdifRecord(")) >= 0);
}

void TestQsoWaterfallRecovery::qmlKeepsWaterfallItemAndRearmsAfterLayoutSettles()
{
    const QString qml = readSource(
        QStringLiteral("qml/decodium/components/Waterfall.qml"));
    QVERIFY(!qml.isEmpty());
    QVERIFY(qml.contains(QStringLiteral("function onQsoLogCompleted(call)")));
    QVERIFY(qml.contains(QStringLiteral("bridge.rearmSpectrumAfterQsoLog()")));
    QVERIFY(qml.contains(QStringLiteral("Qt.callLater(function()")));
    QVERIFY(qml.contains(QStringLiteral("waterfallDisplay.resetWaterfall()")));
}

QTEST_MAIN(TestQsoWaterfallRecovery)
#include "test_qso_waterfall_recovery.moc"
