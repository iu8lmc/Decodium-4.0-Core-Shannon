#include "src/services/DxCalendarCache.h"
#include "src/app/QmlRootCleanup.h"
#include <QDir>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class DxCalendarIntegrationTest : public QObject {
    Q_OBJECT
private slots:
    void rootCleanupBeforeEngine_data()
    {
        QTest::addColumn<bool>("calendarEnabled");
        QTest::newRow("A-without-calendar") << false;
        QTest::newRow("B-with-calendar-worker") << true;
    }
    void rootCleanupBeforeEngine()
    {
        QFETCH(bool, calendarEnabled);
        QQmlApplicationEngine engine;
        const QString location = QDir(QStringLiteral(DECODIUM_SOURCE_DIR)).filePath(QStringLiteral("qml/decodium/components/TestHarness.qml"));
        const QByteArray qml = calendarEnabled
            ? QByteArrayLiteral("import QtQuick\nimport QtQuick.Controls\nimport \".\"\nApplicationWindow { property var mainWindowObject: factory.createObject(null); Component { id: factory; ApplicationWindow { visible: true; DxCalendar { persistenceEnabled: false } } } }")
            : QByteArrayLiteral("import QtQuick\nimport QtQuick.Controls\nApplicationWindow { property var mainWindowObject: factory.createObject(null); Component { id: factory; ApplicationWindow { visible: true } } }");
        engine.loadData(qml, QUrl::fromLocalFile(location));
        QCOMPARE(engine.rootObjects().size(), 1);
        QPointer<QObject> window(engine.rootObjects().first());
        const QVariant mainValue = window->property("mainWindowObject");
        QPointer<QObject> mainWindow(mainValue.metaType() == QMetaType::fromType<QJSValue>()
            ? mainValue.value<QJSValue>().toQObject() : mainValue.value<QObject*>());
        QVERIFY(mainWindow);
        QTest::qWait(100);
        destroyQmlRoots(engine);
        QVERIFY(window.isNull());
        QVERIFY(mainWindow.isNull());
        QVERIFY(engine.rootObjects().isEmpty());
    }
    void restoresAsynchronousCacheIntoQml()
    {
        QTemporaryDir dir(QDir(QStringLiteral(DECODIUM_TEST_DIR)).filePath(QStringLiteral("dxcalendar-ui-XXXXXX")));
        QVERIFY(dir.isValid());
        DxCalendarCache cache(nullptr, dir.filePath(QStringLiteral("calendar.json")));
        QSignalSpy writes(&cache, &DxCalendarCache::cacheWritten);
        const QString feed = QStringLiteral("BEGIN:VCALENDAR\nBEGIN:VEVENT\nSUMMARY:Example (ZZ0ABC)\nDTSTART;VALUE=DATE:20261110\nDTEND;VALUE=DATE:20261112\nEND:VEVENT\nEND:VCALENDAR\n");
        cache.writeCache(feed, QStringLiteral("synthetic timestamp"));
        QTRY_COMPARE_WITH_TIMEOUT(writes.count(), 1, 5000);
        QCOMPARE(writes.takeFirst().at(0).toString(), QString());
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("dxCalendarCache"), &cache);
        QQmlComponent component(&engine);
        const QByteArray qml = R"(import QtQuick
import QtQuick.Controls
import "."
ApplicationWindow {
    width: 800; height: 650; visible: true
    DxCalendar { objectName: "calendar" }
})";
        const QString location = QDir(QStringLiteral(DECODIUM_SOURCE_DIR)).filePath(QStringLiteral("qml/decodium/components/TestHarness.qml"));
        component.setData(qml, QUrl::fromLocalFile(location));
        if (component.isError()) qWarning().noquote() << component.errorString();
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject* calendar = window->findChild<QObject*>(QStringLiteral("calendar"));
        QVERIFY(calendar);
        QTRY_VERIFY_WITH_TIMEOUT(calendar->property("statusText").toString().contains(QStringLiteral("synthetic timestamp")), 5000);
        const auto entries = calendar->property("entries").value<QJSValue>().toVariant().toList();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().toMap().value(QStringLiteral("call")).toString(), QStringLiteral("ZZ0ABC"));
    }
};
QTEST_MAIN(DxCalendarIntegrationTest)
#include "test_dxcalendar_integration.moc"
