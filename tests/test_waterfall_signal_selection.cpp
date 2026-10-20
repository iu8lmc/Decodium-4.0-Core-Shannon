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

class TestWaterfallSignalSelection final : public QObject
{
    Q_OBJECT

private slots:
    void cwRightClickSelectsTheActualDecoderTone();
    void cwSelectionCanReturnToAutomaticSearch();
    void markerAndOperatorHintRemainVisible();
};

void TestWaterfallSignalSelection::cwRightClickSelectsTheActualDecoderTone()
{
    const QString waterfall = readSource(QStringLiteral("qml/decodium/components/Waterfall.qml"));
    QVERIFY(!waterfall.isEmpty());

    const QString select = functionBody(
        waterfall,
        QStringLiteral("function selectReceiveSignal(freq)"),
        QStringLiteral("function clearCwSignalSelection()"));
    QVERIFY(select.contains(QStringLiteral("bridge.setRxAudioFreqFromClick(clean)")));
    QVERIFY(select.contains(QStringLiteral("cwModule.ensureStarted()")));
    QVERIFY(select.contains(QStringLiteral("cwModule.decoderToneLock = clean")));
    QVERIFY(select.indexOf(QStringLiteral("cwModule.ensureStarted()"))
             < select.indexOf(QStringLiteral("cwModule.decoderToneLock = clean")));
    QVERIFY(waterfall.contains(QStringLiteral("waterfallPanel.selectReceiveSignal(freq)")));
}

void TestWaterfallSignalSelection::cwSelectionCanReturnToAutomaticSearch()
{
    const QString waterfall = readSource(QStringLiteral("qml/decodium/components/Waterfall.qml"));
    const QString clear = functionBody(
        waterfall,
        QStringLiteral("function clearCwSignalSelection()"),
        QStringLiteral("function setPaletteIndex("));
    QVERIFY(clear.contains(QStringLiteral("cwModule.decoderToneLock = 0")));
    QVERIFY(waterfall.contains(QStringLiteral("waterfallPanel.clearCwSignalSelection()")));
}

void TestWaterfallSignalSelection::markerAndOperatorHintRemainVisible()
{
    const QString waterfall = readSource(QStringLiteral("qml/decodium/components/Waterfall.qml"));
    QVERIFY(waterfall.contains(QStringLiteral("id: cwDecoderLockMarker")));
    QVERIFY(waterfall.contains(QStringLiteral("id: cwSignalSelectionControl")));
    QVERIFY(waterfall.contains(QStringLiteral("CW: right-click signal")));
    QVERIFY(waterfall.contains(QStringLiteral("This does not retune the radio.")));
}

QTEST_GUILESS_MAIN(TestWaterfallSignalSelection)
#include "test_waterfall_signal_selection.moc"
