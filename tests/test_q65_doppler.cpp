#include "src/services/Q65DopplerTracker.h"
#include <QtTest/QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <memory>
#include <cmath>

extern "C" bool decodium_moon_doppler_c(int, int, int, double, double,
                                       char const*, double*, double*);

class TestQ65Doppler : public QObject
{
    Q_OBJECT
    using Tracker = Q65DopplerTracker;
    static Tracker::Context context()
    {
        Tracker::Context c;
        c.mode = "Q65-60B";
        c.myGrid = "JM75FU";
        c.dxGrid = "FN20QI";
        c.nominalHz = 144120000;
        c.connected = c.supported = c.split = c.monitoring = true;
        return c;
    }
    static QDateTime utc() { return QDateTime::fromString("2026-09-17T12:00:00Z", Qt::ISODate); }
private slots:
    void correctionConventions()
    {
        QCOMPARE(Tracker::offsets(Tracker::ConstantOnMoon, 100, -30), qMakePair(100.0, -100.0));
        QCOMPARE(Tracker::offsets(Tracker::FullDoppler, 100, -30), qMakePair(70.0, -70.0));
        QCOMPARE(Tracker::offsets(Tracker::OwnEcho, 100, -30), qMakePair(200.0, 0.0));
        QCOMPARE(Tracker::offsets(Tracker::FullDoppler, -100, 30), qMakePair(-70.0, 70.0));
    }
    void periodsAndMidnight()
    {
        QCOMPARE(Tracker::txMidpoint(utc(), 60000), utc().addSecs(30));
        QCOMPARE(Tracker::txMidpoint(utc().addSecs(59), 60000), utc().addSecs(90));
        QCOMPARE(Tracker::txMidpoint(utc(), 30000), utc().addSecs(15));
        auto const midnight = QDateTime::fromString("2026-09-17T23:59:59Z", Qt::ISODate);
        QCOMPARE(Tracker::txMidpoint(midnight, 120000),
                 QDateTime::fromString("2026-09-18T00:01:00Z", Qt::ISODate));
    }
    void lunarFrequencyScaling()
    {
        double a, b, elA, elB;
        QVERIFY(decodium_moon_doppler_c(2026, 9, 17, 12.0, 144120000, "JM75FU", &a, &elA));
        QVERIFY(decodium_moon_doppler_c(2026, 9, 17, 12.0, 432360000, "JM75FU", &b, &elB));
        QVERIFY(std::abs(a * 3 - b) < 1e-8);
        QCOMPARE(elA, elB);
        QVERIFY(std::abs(a) < 1000);
        QVERIFY(!decodium_moon_doppler_c(2026, 9, 17, 12.0, -1, "JM75FU", &a, &elA));
    }
    void interlocks_data()
    {
        QTest::addColumn<int>("invalid");
        for (int i = 0; i != 9; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void interlocks()
    {
        QFETCH(int, invalid);
        Tracker tracker;
        auto c = context();
        switch (invalid) {
        case 0: c.connected = false; break;
        case 1: c.supported = false; break;
        case 2: c.split = false; break;
        case 3: c.mode = "FT8"; break;
        case 4: c.nominalHz = 14074000; break;
        case 5: c.myGrid = "JM75"; break;
        case 6: c.conflict = true; break;
        case 7: c.monitoring = false; break;
        case 8: tracker.setMethod(Tracker::FullDoppler); c.dxGrid = "ZZ99XX"; break;
        }
        QVERIFY(!tracker.update(c, utc()).tune);
        tracker.setEnabled(true);
        QVERIFY(!tracker.enabled());
        QVERIFY(!tracker.update(c, utc()).tune);
        QVERIFY(!tracker.snapshot().value("ready").toBool());
    }
    void fullDopplerAndOwnEcho()
    {
        auto c = context();
        Tracker tracker;
        tracker.setMethod(Tracker::FullDoppler);
        tracker.update(c, utc());
        tracker.setEnabled(true);
        auto full = tracker.update(c, utc());
        QVERIFY(full.tune);
        double own, dx, elevation;
        decodium_moon_doppler_c(2026, 9, 17, 12, c.nominalHz, "JM75FU", &own, &elevation);
        decodium_moon_doppler_c(2026, 9, 17, 12, c.nominalHz, "FN20QI", &dx, &elevation);
        QCOMPARE(full.rxHz, c.nominalHz + std::round(own + dx));
        tracker.setMethod(Tracker::OwnEcho);
        auto echo = tracker.update(c, utc());
        QCOMPARE(echo.txHz, c.nominalHz);
        QCOMPARE(echo.rxHz, c.nominalHz + std::round(2 * own));
    }
    void txFreezeAndDeferredRestore()
    {
        Tracker tracker;
        auto c = context();
        QVERIFY(!tracker.update(c, utc()).tune);
        tracker.setEnabled(true);
        auto start = tracker.update(c, utc());
        QVERIFY(start.tune && !start.restore);
        QCOMPARE(tracker.snapshot().value("nominalHz").toDouble(), c.nominalHz);
        c.transmitting = true;
        QVERIFY(!tracker.update(c, utc().addSecs(10)).tune);
        QCOMPARE(tracker.txDialHz(), start.txHz);
        tracker.setMethod(Tracker::FullDoppler);
        QCOMPARE(tracker.method(), int(Tracker::ConstantOnMoon));
        tracker.setEnabled(false);
        QVERIFY(!tracker.update(c, utc().addSecs(20)).tune);
        QVERIFY(tracker.applied());
        c.transmitting = false;
        auto stop = tracker.update(c, utc().addSecs(30));
        QVERIFY(stop.tune && stop.restore);
        QCOMPARE(stop.rxHz, c.nominalHz);
        QVERIFY(!tracker.applied());
        QVERIFY(!tracker.update(c, utc().addSecs(31)).tune);
    }
    void reportsAndManualTuning()
    {
        Tracker tracker;
        auto c = context();
        tracker.update(c, utc());
        tracker.setEnabled(true);
        auto a = tracker.update(c, utc());
        auto const ms = utc().toMSecsSinceEpoch();
        QVERIFY(tracker.consumeFrequencyReport(c.nominalHz, ms + 100));
        QVERIFY(tracker.consumeFrequencyReport(a.rxHz, ms + 200));
        QVERIFY(tracker.consumeFrequencyReport(a.txHz, ms + 300));
        tracker.update(c, utc().addSecs(5));
        QVERIFY(tracker.consumeFrequencyReport(a.rxHz, ms + 5100));
        QVERIFY(tracker.enabled());
        QVERIFY(!tracker.consumeFrequencyReport(c.nominalHz + 100000, ms + 5500));
        QVERIFY(!tracker.enabled() && !tracker.applied());
        // No restoration to the old frequency after the operator tunes.
        QVERIFY(!tracker.update(c, utc().addSecs(6)).tune);
    }
    void disconnectAndModeChange()
    {
        Tracker tracker;
        auto c = context();
        tracker.update(c, utc()); tracker.setEnabled(true); tracker.update(c, utc());
        c.connected = false;
        QVERIFY(!tracker.update(c, utc().addSecs(1)).tune);
        c.connected = true;
        QVERIFY(!tracker.update(c, utc().addSecs(2)).tune);
        QVERIFY(!tracker.enabled());
        tracker.setEnabled(true); tracker.update(c, utc());
        c.mode = "FT8";
        auto stop = tracker.update(c, utc().addSecs(3));
        QVERIFY(stop.tune && stop.restore && !tracker.enabled());
    }
    void panel()
    {
        Tracker tracker;
        auto c = context();
        tracker.update(c, utc());
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(DECODIUM_SOURCE_DIR "/qml/decodium/components/Q65DopplerPanel.qml")));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
            {{"tracker", QVariant::fromValue<QObject*>(&tracker)}, {"stationGrid", c.myGrid}, {"width", 430}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto* item = qobject_cast<QQuickItem*>(object.get());
        QVERIFY(item);
        item->setHeight(item->implicitHeight());
        auto* enabled = object->findChild<QObject*>("emeEnabled");
        auto* method = object->findChild<QObject*>("emeMethod");
        QVERIFY(enabled && method);
        QVERIFY(enabled->property("enabled").toBool());
        QVERIFY(!enabled->property("checked").toBool());
        tracker.setEnabled(true); tracker.update(c, utc());
        QTRY_VERIFY(enabled->property("checked").toBool());
        c.transmitting = true; tracker.update(c, utc().addSecs(1));
        QTRY_VERIFY(!method->property("enabled").toBool());
        QQuickWindow window;
        window.resize(430, qCeil(item->height()));
        item->setParentItem(window.contentItem());
        window.show();
        QTest::qWait(100);
        for (auto* child : item->findChildren<QQuickItem*>()) {
            if (!child->isVisible() || child->width() <= 0) continue;
            // Only layout-managed controls: internal text glyphs are not panels.
            if (child->parentItem() && child->parentItem()->objectName().isEmpty()
                && !child->objectName().isEmpty()) {
                auto const point = child->mapToItem(item, QPointF());
                QVERIFY(point.x() >= -1);
                QVERIFY(point.x() + child->width() <= item->width() + 1);
            }
        }
        if (qEnvironmentVariableIsSet("DECODIUM_EME_SCREENSHOT"))
            QVERIFY(window.grabWindow().save(qEnvironmentVariable("DECODIUM_EME_SCREENSHOT")));
        item->setParentItem(nullptr);
    }
};
QTEST_MAIN(TestQ65Doppler)
#include "test_q65_doppler.moc"
