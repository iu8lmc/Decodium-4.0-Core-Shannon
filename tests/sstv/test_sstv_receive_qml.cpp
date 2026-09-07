// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QSet>
#include <QUrl>

#include <cmath>

namespace {

class ReceiveEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sstvAvailable READ sstvAvailable CONSTANT)
    Q_PROPERTY(bool sstvRxActive READ sstvRxActive NOTIFY rxChanged)
    Q_PROPERTY(QString sstvRxSource READ sstvRxSource NOTIFY rxChanged)
    Q_PROPERTY(QString sstvRxImageSource READ sstvRxImageSource NOTIFY rxChanged)
    Q_PROPERTY(QString sstvDetectedMode READ sstvDetectedMode NOTIFY rxChanged)
    Q_PROPERTY(QVariantMap sstvRxDiagnostics READ sstvRxDiagnostics NOTIFY rxChanged)
    Q_PROPERTY(QVariantMap sstvRxControls READ sstvRxControls NOTIFY rxChanged)
    Q_PROPERTY(QVariantList sstvRxModeChoices READ sstvRxModeChoices CONSTANT)
    Q_PROPERTY(bool sstvRxAudioJobBusy READ sstvRxAudioJobBusy NOTIFY audioJobChanged)
    Q_PROPERTY(QString sstvRxAudioJobState READ sstvRxAudioJobState NOTIFY audioJobChanged)
    Q_PROPERTY(QString sstvRxAudioJobError READ sstvRxAudioJobError NOTIFY audioJobChanged)
    Q_PROPERTY(QString sstvRxRawAudioPath READ sstvRxRawAudioPath NOTIFY audioJobChanged)
    Q_PROPERTY(bool sstvWavReplayActive READ sstvWavReplayActive NOTIFY replayChanged)
    Q_PROPERTY(QString sstvWavReplayState READ sstvWavReplayState NOTIFY replayChanged)
    Q_PROPERTY(double sstvWavReplayProgress READ sstvWavReplayProgress NOTIFY replayChanged)
    Q_PROPERTY(QString sstvWavReplayFileName READ sstvWavReplayFileName NOTIFY replayChanged)
    Q_PROPERTY(QString sstvWavReplayError READ sstvWavReplayError NOTIFY replayChanged)
    Q_PROPERTY(bool sstvStorageReady READ sstvStorageReady NOTIFY storageChanged)
    Q_PROPERTY(bool sstvRxAutoSaveEnabled READ sstvRxAutoSaveEnabled WRITE setSstvRxAutoSaveEnabled NOTIFY storageChanged)
    Q_PROPERTY(QString sstvRxSaveState READ sstvRxSaveState NOTIFY storageChanged)
    Q_PROPERTY(QString sstvRxSaveError READ sstvRxSaveError NOTIFY storageChanged)

public:
    bool sstvAvailable() const noexcept { return true; }
    bool sstvRxActive() const noexcept { return m_rxActive; }
    QString sstvRxSource() const { return m_replayActive
        ? QStringLiteral("Replay/WAV") : QStringLiteral("Local sound card"); }
    QString sstvRxImageSource() const { return {}; }
    QString sstvDetectedMode() const { return {}; }
    QVariantMap sstvRxDiagnostics() const
    {
        return {{QStringLiteral("chunksProcessed"), 12},
                {QStringLiteral("samplesResampled"), 48'000},
                {QStringLiteral("droppedChunks"), 0},
                {QStringLiteral("discontinuities"), 0},
                {QStringLiteral("revision"), 3},
                {QStringLiteral("replayRetainedSamples"), 24'000},
                {QStringLiteral("replayCapacitySamples"), 120'000},
                {QStringLiteral("afcCorrectionHz"), 7.0},
                {QStringLiteral("afcMeasuredOffsetHz"), -7.0},
                {QStringLiteral("afcConfidence"), 0.8},
                {QStringLiteral("slantAppliedPpm"), 120.0},
                {QStringLiteral("slantMeasuredPpm"), 121.0},
                {QStringLiteral("slantConfidence"), 0.7},
                {QStringLiteral("scope"), QVariantList {
                    QVariantMap {{QStringLiteral("frequencyHz"), 1'200.0}},
                    QVariantMap {{QStringLiteral("frequencyHz"), 1'900.0}},
                    QVariantMap {{QStringLiteral("frequencyHz"), 2'300.0}},
                }}};
    }
    QVariantMap sstvRxControls() const { return m_controls; }
    QVariantList sstvRxModeChoices() const
    {
        return {
            QVariantMap {{QStringLiteral("id"), QStringLiteral("martin-m1")},
                         {QStringLiteral("name"), QStringLiteral("Martin M1")},
                         {QStringLiteral("family"), QStringLiteral("Martin")}},
            QVariantMap {{QStringLiteral("id"), QStringLiteral("robot-36")},
                         {QStringLiteral("name"), QStringLiteral("Robot 36")},
                         {QStringLiteral("family"), QStringLiteral("Robot")}},
        };
    }
    bool sstvRxAudioJobBusy() const noexcept { return m_audioBusy; }
    QString sstvRxAudioJobState() const { return QStringLiteral("Idle"); }
    QString sstvRxAudioJobError() const { return {}; }
    QString sstvRxRawAudioPath() const { return {}; }
    bool sstvWavReplayActive() const noexcept { return m_replayActive; }
    QString sstvWavReplayState() const
    {
        return m_replayActive ? QStringLiteral("Replaying")
                              : QStringLiteral("Idle");
    }
    double sstvWavReplayProgress() const noexcept { return m_progress; }
    QString sstvWavReplayFileName() const
    {
        return m_replayActive ? QStringLiteral("field-recording.wav")
                              : QString {};
    }
    QString sstvWavReplayError() const { return {}; }
    bool sstvStorageReady() const noexcept { return true; }
    bool sstvRxAutoSaveEnabled() const noexcept { return m_autoSave; }
    void setSstvRxAutoSaveEnabled(bool enabled)
    {
        if (m_autoSave == enabled) {
            return;
        }
        m_autoSave = enabled;
        Q_EMIT storageChanged();
    }
    QString sstvRxSaveState() const { return QStringLiteral("idle"); }
    QString sstvRxSaveError() const { return {}; }

    void simulateReplay(double progress)
    {
        m_replayActive = true;
        m_progress = progress;
        Q_EMIT replayChanged();
        Q_EMIT rxChanged();
    }

    Q_INVOKABLE bool startSstvRx() { m_rxActive = true; Q_EMIT rxChanged(); return true; }
    Q_INVOKABLE void stopSstvRx() { m_rxActive = false; Q_EMIT rxChanged(); }
    Q_INVOKABLE bool resetSstvRx() { return !m_replayActive; }
    Q_INVOKABLE bool abortSstvRxFrame() { ++abortCount; return !m_replayActive; }
    Q_INVOKABLE bool updateSstvRxControls(const QVariantMap& values)
    {
        for (auto iterator = values.constBegin(); iterator != values.constEnd();
             ++iterator) {
            m_controls.insert(iterator.key(), iterator.value());
        }
        ++controlUpdateCount;
        Q_EMIT rxChanged();
        return true;
    }
    Q_INVOKABLE void resetSstvRxAfc() { ++afcResetCount; }
    Q_INVOKABLE void resetSstvRxSlant() { ++slantResetCount; }
    Q_INVOKABLE bool redecodeRecentSstv(const QVariantMap&)
    {
        ++redecodeCount;
        return true;
    }
    Q_INVOKABLE bool saveSstvRxRawAudio() { ++rawSaveCount; return true; }
    Q_INVOKABLE void cancelSstvRxAudioJob() { m_audioBusy = false; Q_EMIT audioJobChanged(); }
    Q_INVOKABLE bool startSstvWavReplay(const QUrl&)
    {
        simulateReplay(0.0);
        return true;
    }
    Q_INVOKABLE void cancelSstvWavReplay()
    {
        m_replayActive = false;
        Q_EMIT replayChanged();
        Q_EMIT rxChanged();
    }
    Q_INVOKABLE bool saveSstvRxImage() { return false; }

    int abortCount {0};
    int controlUpdateCount {0};
    int afcResetCount {0};
    int slantResetCount {0};
    int redecodeCount {0};
    int rawSaveCount {0};

Q_SIGNALS:
    void rxChanged();
    void replayChanged();
    void storageChanged();
    void audioJobChanged();

private:
    bool m_rxActive {true};
    bool m_replayActive {false};
    double m_progress {0.0};
    bool m_autoSave {false};
    bool m_audioBusy {false};
    QVariantMap m_controls {
        {QStringLiteral("modeControl"), QStringLiteral("auto")},
        {QStringLiteral("manualMode"), QStringLiteral("martin-m1")},
        {QStringLiteral("modeLockEnabled"), false},
        {QStringLiteral("lockedMode"), QStringLiteral("martin-m1")},
        {QStringLiteral("receiveWithoutVis"), false},
        {QStringLiteral("timingFallbackEnabled"), true},
        {QStringLiteral("afcMode"), QStringLiteral("auto")},
        {QStringLiteral("manualFrequencyCorrectionHz"), 0.0},
        {QStringLiteral("slantMode"), QStringLiteral("auto")},
        {QStringLiteral("manualClockErrorPpm"), 0.0},
        {QStringLiteral("replayRetentionSeconds"), 180},
        {QStringLiteral("retainRawAudio"), true},
        {QStringLiteral("diagnosticScopeEnabled"), true},
    };
};

QString qmlErrors(const QList<QQmlError>& errors)
{
    QStringList lines;
    for (const QQmlError& error : errors) {
        lines.push_back(error.toString());
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

class TestSstvReceiveQml final : public QObject
{
    Q_OBJECT

private slots:
    void replayControlsReactAndPageRendersOffscreen()
    {
        ReceiveEngineFixture fixture;
        QQmlEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this,
                [&warnings](const QList<QQmlError>& values) {
                    for (const QQmlError& warning : values) {
                        warnings.push_back(warning.toString());
                    }
                });
        QQmlComponent component(
            &engine,
            QUrl::fromLocalFile(QString::fromUtf8(
                DECODIUM_SSTV_RECEIVE_QML_SOURCE)),
            QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(),
                 qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {
            {QStringLiteral("engine"),
             QVariant::fromValue(static_cast<QObject*>(&fixture))},
        };
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* page = qobject_cast<QQuickItem*>(object.data());
        QVERIFY(page);

        QObject* open = page->findChild<QObject*>(
            QStringLiteral("sstvReplayWavOpen"));
        QObject* cancel = page->findChild<QObject*>(
            QStringLiteral("sstvReplayWavCancel"));
        QObject* progress = page->findChild<QObject*>(
            QStringLiteral("sstvReplayWavProgress"));
        QObject* autoSave = page->findChild<QObject*>(
            QStringLiteral("sstvRxAutoSave"));
        QObject* saveImage = page->findChild<QObject*>(
            QStringLiteral("sstvRxSaveImage"));
        QObject* controls = page->findChild<QObject*>(
            QStringLiteral("sstvRxCorrectionControls"));
        QObject* mode = page->findChild<QObject*>(
            QStringLiteral("sstvRxModeControl"));
        QObject* lock = page->findChild<QObject*>(
            QStringLiteral("sstvRxModeLock"));
        QObject* noVis = page->findChild<QObject*>(
            QStringLiteral("sstvRxNoVis"));
        QObject* abort = page->findChild<QObject*>(
            QStringLiteral("sstvRxAbortFrame"));
        QObject* afcReset = page->findChild<QObject*>(
            QStringLiteral("sstvRxAfcReset"));
        QObject* slantReset = page->findChild<QObject*>(
            QStringLiteral("sstvRxSlantReset"));
        QObject* redecode = page->findChild<QObject*>(
            QStringLiteral("sstvRxRedecodeRecent"));
        QObject* saveRaw = page->findChild<QObject*>(
            QStringLiteral("sstvRxSaveRawAudio"));
        QObject* imageFit = page->findChild<QObject*>(
            QStringLiteral("sstvRxImageFit"));
        QObject* imagePixel = page->findChild<QObject*>(
            QStringLiteral("sstvRxImagePixel"));
        QObject* imageZoom = page->findChild<QObject*>(
            QStringLiteral("sstvRxImageZoomIn"));
        QObject* scope = page->findChild<QObject*>(
            QStringLiteral("sstvRxDiagnosticScope"));
        QVERIFY(open);
        QVERIFY(cancel);
        QVERIFY(progress);
        QVERIFY(autoSave);
        QVERIFY(saveImage);
        QVERIFY(controls);
        QVERIFY(mode);
        QVERIFY(lock);
        QVERIFY(noVis);
        QVERIFY(abort);
        QVERIFY(afcReset);
        QVERIFY(slantReset);
        QVERIFY(redecode);
        QVERIFY(saveRaw);
        QVERIFY(imageFit);
        QVERIFY(imagePixel);
        QVERIFY(imageZoom);
        QVERIFY(scope);
        QCOMPARE(open->property("enabled").toBool(), true);
        QCOMPARE(cancel->property("visible").toBool(), false);
        QCOMPARE(autoSave->property("enabled").toBool(), true);
        QCOMPARE(saveImage->property("enabled").toBool(), false);
        QCOMPARE(redecode->property("enabled").toBool(), true);
        QCOMPARE(saveRaw->property("enabled").toBool(), true);
        QCOMPARE(scope->property("visible").toBool(), true);

        QVERIFY(QMetaObject::invokeMethod(abort, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(afcReset, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(slantReset, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(redecode, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(saveRaw, "clicked"));
        QCOMPARE(fixture.abortCount, 1);
        QCOMPARE(fixture.afcResetCount, 1);
        QCOMPARE(fixture.slantResetCount, 1);
        QCOMPARE(fixture.redecodeCount, 1);
        QCOMPARE(fixture.rawSaveCount, 1);
        QVERIFY(QMetaObject::invokeMethod(noVis, "click"));
        QTRY_VERIFY(fixture.controlUpdateCount > 0);
        QCOMPARE(fixture.sstvRxControls()
                     .value(QStringLiteral("receiveWithoutVis")).toBool(),
                 true);

        const int updatesBeforeLock = fixture.controlUpdateCount;
        QVERIFY(QMetaObject::invokeMethod(lock, "click"));
        QTRY_COMPARE(fixture.controlUpdateCount, updatesBeforeLock + 1);
        QCOMPARE(fixture.sstvRxControls()
                     .value(QStringLiteral("modeLockEnabled")).toBool(),
                 true);
        QCOMPARE(fixture.sstvRxControls()
                     .value(QStringLiteral("lockedMode")).toString(),
                 QStringLiteral("martin-m1"));

        fixture.simulateReplay(0.42);
        QTRY_COMPARE(open->property("enabled").toBool(), false);
        QTRY_COMPARE(cancel->property("visible").toBool(), true);
        QTRY_VERIFY(std::abs(progress->property("value").toDouble() - 0.42)
                    < 1.0e-9);

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#111c25")));
        window.resize(940, 700);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(940.0, 700.0));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTest::qWait(200);
        const QImage rendered = window.grabWindow();
        QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.size(), QSize(940, 700));
        QSet<QRgb> colours;
        for (int y = 0; y < rendered.height(); y += 17) {
            for (int x = 0; x < rendered.width(); x += 17) {
                colours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY(colours.size() > 10);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
};

QTEST_MAIN(TestSstvReceiveQml)
#include "test_sstv_receive_qml.moc"
