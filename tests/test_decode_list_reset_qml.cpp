#include "DecodeListModel.h"
#include <QtTest>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <memory>

class TestDecodeListResetQml : public QObject
{
    Q_OBJECT
private slots:
    void resetViews_data()
    {
        QTest::addColumn<bool>("newestFirst");
        QTest::addColumn<bool>("detachedDuringReset");
        QTest::newRow("docked-oldest-first") << false << false;
        QTest::newRow("docked-newest-first") << true << false;
        QTest::newRow("detached-oldest-first") << false << true;
        QTest::newRow("detached-newest-first") << true << true;
    }
    void resetViews()
    {
        QFETCH(bool, newestFirst);
        QFETCH(bool, detachedDuringReset);
        DecodeListModel model;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("decodeTestModel", &model);
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(DECODIUM_SOURCE_DIR "/tests/qml/decode_reset_harness.qml")));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto* item = qobject_cast<QQuickItem*>(root.get());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(480, 280);
        item->setParentItem(window.contentItem());
        root->setProperty("newestFirst", newestFirst);
        window.show();
        auto* list = root->findChild<QObject*>("decodeList");
        auto* scroll = root->findChild<QObject*>("scroll");
        QVERIFY(list && scroll);
        auto hasGreenRows = [](QImage const& image) {
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    QColor const c = image.pixelColor(x, y);
                    if (c.green() > 150 && c.red() < 50) return true;
                }
            return false;
        };
        for (int cycle = 0; cycle < 3; ++cycle) {
            QVariantList rows;
            for (int i = 0; i < 30; ++i)
                rows.append(QVariantMap{{"time", QString::number(i)}, {"message", "CQ OLD FT4"}});
            model.setEntries(rows);
            QTRY_COMPARE(list->property("count").toInt(), 30);
            QVERIFY(QMetaObject::invokeMethod(list, "showRows"));
            QTest::qWait(130);
            QTRY_VERIFY2(hasGreenRows(window.grabWindow()), qPrintable(QString::number(cycle)));
            scroll->setProperty("running", true);
            model.appendEntriesBudgeted({QVariantMap{{"message", "LATE FT4"}}}, false, 1);
            if (detachedDuringReset) root->setProperty("attached", false);
            model.resetForContextChange();
            QVERIFY(!scroll->property("running").toBool());
            QVERIFY(list->property("followTail").toBool());
            QVERIFY(!list->property("tailFollowQueued").toBool());
            QCOMPARE(list->property("pendingNewDecodes").toInt(), 0);
            root->setProperty("attached", true);
            QTRY_COMPARE(list->property("count").toInt(), 0);
            QTest::qWait(150);
            QCOMPARE(model.count(), 0);
            QCOMPARE(list->property("contentY").toDouble(),
                     list->property("originY").toDouble() - (newestFirst ? 280 : 0));
            QImage const cleared = window.grabWindow();
            QVERIFY(!cleared.isNull());
            QVERIFY(!hasGreenRows(cleared));
        }
        item->setParentItem(nullptr);
    }
};
QTEST_MAIN(TestDecodeListResetQml)
#include "test_decode_list_reset_qml.moc"
