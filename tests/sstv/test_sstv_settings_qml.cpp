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

namespace {

class SettingsShareFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(bool configured READ configured CONSTANT)
    Q_PROPERTY(bool secureStorageAvailable READ secureStorageAvailable CONSTANT)

public:
    bool ready() const noexcept { return true; }
    bool enabled() const noexcept { return m_enabled; }
    bool configured() const noexcept { return true; }
    bool secureStorageAvailable() const noexcept { return true; }

    Q_INVOKABLE bool setEnabled(bool enabled)
    {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            Q_EMIT stateChanged();
        }
        return true;
    }

Q_SIGNALS:
    void stateChanged();

private:
    bool m_enabled {false};
};

class SettingsDigitalFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles CONSTANT)
    Q_PROPERTY(QString selectedProfileId READ selectedProfileId
               WRITE setSelectedProfileId NOTIFY selectedProfileChanged)
    Q_PROPERTY(bool busy READ busy CONSTANT)
    Q_PROPERTY(QString capabilityMessage READ capabilityMessage CONSTANT)

public:
    QVariantList profiles() const
    {
        return {
            QVariantMap {{QStringLiteral("id"), QStringLiteral("hamdrm-e-2500")},
                         {QStringLiteral("displayName"),
                          QStringLiteral("E / 2.5 kHz")}},
            QVariantMap {{QStringLiteral("id"), QStringLiteral("hamdrm-b-2300")},
                         {QStringLiteral("displayName"),
                          QStringLiteral("B / 2.3 kHz")}},
        };
    }
    QString selectedProfileId() const { return m_selectedProfile; }
    void setSelectedProfileId(const QString& value)
    {
        if (m_selectedProfile == value) {
            return;
        }
        m_selectedProfile = value;
        Q_EMIT selectedProfileChanged();
    }
    bool busy() const noexcept { return false; }
    QString capabilityMessage() const
    {
        return QStringLiteral("Native HAMDRM capability fixture");
    }

Q_SIGNALS:
    void selectedProfileChanged();

private:
    QString m_selectedProfile {QStringLiteral("hamdrm-e-2500")};
};

class SettingsGalleryFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap retentionSettings READ retentionSettings
               NOTIFY retentionSettingsChanged)
    Q_PROPERTY(bool retentionBusy READ retentionBusy CONSTANT)

public:
    SettingsGalleryFixture()
    {
        m_settings = {
            {QStringLiteral("automaticEnabled"), false},
            {QStringLiteral("maximumAgeDays"), 0},
            {QStringLiteral("imageQuotaBytes"), qint64(0)},
            {QStringLiteral("thumbnailQuotaBytes"), qint64(0)},
            {QStringLiteral("rawAudioQuotaBytes"), qint64(0)},
            {QStringLiteral("sharedPolicy"), 0},
            {QStringLiteral("maximumDeletesPerRun"), 100},
        };
    }

    QVariantMap retentionSettings() const { return m_settings; }
    bool retentionBusy() const noexcept { return false; }

    Q_INVOKABLE quint64 updateRetentionSettings(const QVariantMap& values)
    {
        static const QSet<QString> required {
            QStringLiteral("automaticEnabled"),
            QStringLiteral("maximumAgeDays"),
            QStringLiteral("imageQuotaBytes"),
            QStringLiteral("thumbnailQuotaBytes"),
            QStringLiteral("rawAudioQuotaBytes"),
            QStringLiteral("sharedPolicy"),
            QStringLiteral("maximumDeletesPerRun"),
        };
        if (QSet<QString>(values.keyBegin(), values.keyEnd()) != required) {
            return 0;
        }
        m_settings = values;
        ++updates;
        Q_EMIT retentionSettingsChanged();
        return static_cast<quint64>(updates);
    }

    int updates {0};

Q_SIGNALS:
    void retentionSettingsChanged();

private:
    QVariantMap m_settings;
};

class SettingsEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sstvStorageReady READ sstvStorageReady CONSTANT)
    Q_PROPERTY(bool sstvRxAutoSaveEnabled READ sstvRxAutoSaveEnabled
               WRITE setSstvRxAutoSaveEnabled NOTIFY storageChanged)
    Q_PROPERTY(bool sstvTxActive READ sstvTxActive NOTIFY txChanged)
    Q_PROPERTY(int sstvPttLeadMs MEMBER pttLead NOTIFY timingChanged)
    Q_PROPERTY(int sstvPttTailMs MEMBER pttTail NOTIFY timingChanged)
    Q_PROPERTY(int sstvPttReleaseRetryMs MEMBER pttRetry NOTIFY timingChanged)
    Q_PROPERTY(int sstvVoxPreKeyMs MEMBER voxPreKey NOTIFY timingChanged)
    Q_PROPERTY(int sstvVoxHangMs MEMBER voxHang NOTIFY timingChanged)
    Q_PROPERTY(double sstvVoxToneFrequencyHz MEMBER voxFrequency
               NOTIFY timingChanged)
    Q_PROPERTY(double sstvVoxToneLevel MEMBER voxLevel NOTIFY timingChanged)
    Q_PROPERTY(QString sstvRxSource READ sstvRxSource CONSTANT)
    Q_PROPERTY(QString sstvRxState READ sstvRxState CONSTANT)
    Q_PROPERTY(bool sstvRxActive READ sstvRxActive CONSTANT)
    Q_PROPERTY(QVariantMap sstvRxDiagnostics READ sstvRxDiagnostics CONSTANT)
    Q_PROPERTY(QVariantMap sstvRxControls READ sstvRxControls
               NOTIFY rxControlsChanged)
    Q_PROPERTY(QObject* sstvShare READ sstvShare CONSTANT)
    Q_PROPERTY(QObject* sstvDigital READ sstvDigital CONSTANT)
    Q_PROPERTY(QObject* sstvGallery READ sstvGallery CONSTANT)

public:
    bool sstvStorageReady() const noexcept { return true; }
    bool sstvRxAutoSaveEnabled() const noexcept { return autoSave; }
    void setSstvRxAutoSaveEnabled(bool value)
    {
        if (autoSave == value) {
            return;
        }
        autoSave = value;
        Q_EMIT storageChanged();
    }
    bool sstvTxActive() const noexcept { return txActive; }
    void setTxActive(bool value)
    {
        if (txActive == value) {
            return;
        }
        txActive = value;
        Q_EMIT txChanged();
    }
    QString sstvRxSource() const { return QStringLiteral("DecoPort"); }
    QString sstvRxState() const { return QStringLiteral("Running"); }
    bool sstvRxActive() const noexcept { return true; }
    QVariantMap sstvRxDiagnostics() const
    {
        return {{QStringLiteral("queuedSamples"), 1'200},
                {QStringLiteral("droppedSamples"), 3},
                {QStringLiteral("chunksProcessed"), 42},
                {QStringLiteral("processingFailures"), 0}};
    }
    QVariantMap sstvRxControls() const { return rxControls; }
    Q_INVOKABLE bool updateSstvRxControls(const QVariantMap& values)
    {
        for (auto iterator = values.constBegin(); iterator != values.constEnd();
             ++iterator) {
            rxControls.insert(iterator.key(), iterator.value());
        }
        ++rxControlUpdates;
        Q_EMIT rxControlsChanged();
        return true;
    }
    QObject* sstvShare() noexcept { return &share; }
    QObject* sstvDigital() noexcept { return &digital; }
    QObject* sstvGallery() noexcept { return &gallery; }

    int pttLead {250};
    int pttTail {600};
    int pttRetry {500};
    int voxPreKey {800};
    int voxHang {550};
    double voxFrequency {1'900.0};
    double voxLevel {0.5};
    SettingsShareFixture share;
    SettingsDigitalFixture digital;
    SettingsGalleryFixture gallery;
    int rxControlUpdates {0};

Q_SIGNALS:
    void storageChanged();
    void txChanged();
    void timingChanged();
    void rxControlsChanged();

private:
    bool autoSave {false};
    bool txActive {false};
    QVariantMap rxControls {
        {QStringLiteral("afcMode"), QStringLiteral("auto")},
        {QStringLiteral("manualFrequencyCorrectionHz"), 0.0},
        {QStringLiteral("slantMode"), QStringLiteral("auto")},
        {QStringLiteral("manualClockErrorPpm"), 0.0},
        {QStringLiteral("replayRetentionSeconds"), 180},
        {QStringLiteral("receiveWithoutVis"), false},
        {QStringLiteral("timingFallbackEnabled"), true},
        {QStringLiteral("retainRawAudio"), false},
        {QStringLiteral("diagnosticScopeEnabled"), false},
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

class TestSstvSettingsQml final : public QObject
{
    Q_OBJECT

private slots:
    void timingControlsBindLockAndRenderOffscreen()
    {
        SettingsEngineFixture fixture;
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
                DECODIUM_SSTV_SETTINGS_QML_SOURCE)),
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

        QObject* grid = page->findChild<QObject*>(
            QStringLiteral("sstvTxTimingGrid"));
        QObject* lead = page->findChild<QObject*>(
            QStringLiteral("sstvPttLeadMsControl"));
        QObject* tail = page->findChild<QObject*>(
            QStringLiteral("sstvPttTailMsControl"));
        QObject* retry = page->findChild<QObject*>(
            QStringLiteral("sstvPttRetryMsControl"));
        QObject* preKey = page->findChild<QObject*>(
            QStringLiteral("sstvVoxPreKeyMsControl"));
        QObject* hang = page->findChild<QObject*>(
            QStringLiteral("sstvVoxHangMsControl"));
        QObject* tone = page->findChild<QObject*>(
            QStringLiteral("sstvVoxToneHzControl"));
        QObject* level = page->findChild<QObject*>(
            QStringLiteral("sstvVoxLevelControl"));
        QObject* rxSource = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxSource"));
        QObject* sharing = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsSharingEnabled"));
        QObject* digitalProfile = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsHamDrmProfile"));
        QObject* capability = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsHamDrmCapability"));
        QObject* diagnostics = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsDiagnostics"));
        QObject* retentionAutomatic = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionAutomaticEnabled"));
        QObject* retentionGrid = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionSettingsGrid"));
        QObject* retentionAge = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionMaximumAgeDays"));
        QObject* retentionImageQuota = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionImageQuotaMiB"));
        QObject* retentionThumbnailQuota = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionThumbnailQuotaMiB"));
        QObject* retentionRawQuota = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionRawAudioQuotaMiB"));
        QObject* retentionShared = page->findChild<QObject*>(
            QStringLiteral("sstvRetentionSharedPolicy"));
        QObject* receiverDefaults = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxDefaults"));
        QObject* rxAfc = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxAfcMode"));
        QObject* rxSlant = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxSlantMode"));
        QObject* rxRetention = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxReplayRetention"));
        QObject* rxNoVis = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxNoVis"));
        QObject* rxFallback = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxTimingFallback"));
        QObject* rxRetainRaw = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxRetainRaw"));
        QObject* rxScope = page->findChild<QObject*>(
            QStringLiteral("sstvSettingsRxScope"));
        QVERIFY(grid);
        QVERIFY(lead);
        QVERIFY(tail);
        QVERIFY(retry);
        QVERIFY(preKey);
        QVERIFY(hang);
        QVERIFY(tone);
        QVERIFY(level);
        QVERIFY(rxSource);
        QVERIFY(sharing);
        QVERIFY(digitalProfile);
        QVERIFY(capability);
        QVERIFY(diagnostics);
        QVERIFY(retentionAutomatic);
        QVERIFY(retentionGrid);
        QVERIFY(retentionAge);
        QVERIFY(retentionImageQuota);
        QVERIFY(retentionThumbnailQuota);
        QVERIFY(retentionRawQuota);
        QVERIFY(retentionShared);
        QVERIFY(receiverDefaults);
        QVERIFY(rxAfc);
        QVERIFY(rxSlant);
        QVERIFY(rxRetention);
        QVERIFY(rxNoVis);
        QVERIFY(rxFallback);
        QVERIFY(rxRetainRaw);
        QVERIFY(rxScope);
        QCOMPARE(lead->property("value").toInt(), fixture.pttLead);
        QCOMPARE(tail->property("value").toInt(), fixture.pttTail);
        QCOMPARE(retry->property("value").toInt(), fixture.pttRetry);
        QCOMPARE(preKey->property("value").toInt(), fixture.voxPreKey);
        QCOMPARE(hang->property("value").toInt(), fixture.voxHang);
        QCOMPARE(tone->property("value").toInt(),
                 static_cast<int>(fixture.voxFrequency));
        QCOMPARE(level->property("value").toInt(), 50);
        QCOMPARE(grid->property("enabled").toBool(), true);
        QCOMPARE(rxSource->property("text").toString(),
                 QStringLiteral("DecoPort"));
        QCOMPARE(sharing->property("enabled").toBool(), true);
        QCOMPARE(sharing->property("checked").toBool(), false);
        QCOMPARE(digitalProfile->property("count").toInt(), 2);
        QCOMPARE(digitalProfile->property("currentValue").toString(),
                 fixture.digital.selectedProfileId());
        QVERIFY(capability->property("text").toString().contains(
            QStringLiteral("HAMDRM")));
        QVERIFY(!retentionAutomatic->property("checked").toBool());
        QVERIFY(retentionGrid->property("enabled").toBool());
        QCOMPARE(retentionAge->property("value").toInt(), 0);
        QCOMPARE(retentionImageQuota->property("value").toInt(), 0);
        QCOMPARE(retentionThumbnailQuota->property("value").toInt(), 0);
        QCOMPARE(retentionRawQuota->property("value").toInt(), 0);
        QCOMPARE(retentionShared->property("currentValue").toInt(), 0);
        QCOMPARE(rxAfc->property("currentIndex").toInt(), 1);
        QCOMPARE(rxSlant->property("currentIndex").toInt(), 1);
        QCOMPARE(rxRetention->property("value").toInt(), 180);
        QCOMPARE(rxFallback->property("checked").toBool(), true);
        QCOMPARE(rxRetainRaw->property("checked").toBool(), false);
        QCOMPARE(rxScope->property("checked").toBool(), false);
        QVERIFY(QMetaObject::invokeMethod(rxNoVis, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(fixture.rxControlUpdates > 0, 2'000);
        QVERIFY(fixture.sstvRxControls().value(
            QStringLiteral("receiveWithoutVis")).toBool());
        QVERIFY(QMetaObject::invokeMethod(retentionAutomatic, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(fixture.gallery.updates > 0, 2'000);
        QVERIFY(fixture.gallery.retentionSettings().value(
            QStringLiteral("automaticEnabled")).toBool());

        fixture.setTxActive(true);
        QTRY_COMPARE(grid->property("enabled").toBool(), false);
        fixture.setTxActive(false);
        QTRY_COMPARE(grid->property("enabled").toBool(), true);

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#111c25")));
        window.resize(940, 800);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(940.0, 800.0));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTest::qWait(200);
        const QImage rendered = window.grabWindow();
        QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.size(), QSize(940, 800));
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

QTEST_MAIN(TestSstvSettingsQml)
#include "test_sstv_settings_qml.moc"
