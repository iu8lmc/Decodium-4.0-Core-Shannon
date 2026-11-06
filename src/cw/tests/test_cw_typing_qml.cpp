#include "CwController.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace decodium::cw;

class TestCwTypingQml : public QObject {
    Q_OBJECT
private slots:
    void centerTargetIsLocalAndExplained_data()
    {
        QTest::addColumn<int>("width");
        QTest::newRow("compact") << 640;
        QTest::newRow("normal") << 920;
    }

    void centerTargetIsLocalAndExplained()
    {
        QFETCH(int, width);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings settings(dir.filePath("cw.ini"), QSettings::IniFormat);
        settings.setValue("CW/toneHz", 600);
        CwController cw;
        int tunes = 0, sends = 0, pttChanges = 0;
        CwController::Hooks hooks;
        hooks.rigKeyerAvailable = [] { return true; };
        hooks.tuneFrequency = [&](double) { ++tunes; return true; };
        hooks.sendRigMorse = [&](const QString&, int) { ++sends; return true; };
        hooks.rigPtt = [&](bool) { ++pttChanges; };
        cw.setHooks(hooks);
        cw.deferStart(&settings);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("cwModule", &cw);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CW_WINDOW_QML)));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        window->setProperty("width", width);
        auto* label = window->findChild<QObject*>(QStringLiteral("cwCenterTargetLabel"));
        auto* target = window->findChild<QObject*>(QStringLiteral("cwCenterTarget"));
        auto* explanation = window->findChild<QObject*>(QStringLiteral("cwCenterTargetHelp"));
        QVERIFY(label);
        QVERIFY(target);
        QVERIFY(explanation);
        QCOMPARE(label->property("text").toString(), QStringLiteral("Center target (RX)"));
        QVERIFY(explanation->property("text").toString().contains("does not read or change"));
        auto* quickWindow = qobject_cast<QQuickWindow*>(window.get());
        QVERIFY(quickWindow);
        quickWindow->show();
        // The explanation must remain visible (and wrapped) in both layouts.
        for (QObject* control : {label, target, explanation}) {
            auto* item = qobject_cast<QQuickItem*>(control);
            QVERIFY(item);
            QTRY_VERIFY(item->isVisible() && item->width() > 0 && item->height() > 0);
            const QPointF origin = item->mapToScene(QPointF(0, 0));
            QVERIFY(origin.x() >= 0 && origin.y() >= 0);
            QVERIFY(origin.x() + item->width() <= quickWindow->width());
            QVERIFY(origin.y() + item->height() <= quickWindow->height());
        }
        QCOMPARE(cw.toneHz(), 600);
        QCOMPARE(target->property("currentText").toString(), QStringLiteral("600 Hz"));
        // Selecting 700 is a local target change, not a CAT operation. The
        // VFO must only move when the operator explicitly presses Center CW.
        target->setProperty("currentIndex", 5);
        QVERIFY(QMetaObject::invokeMethod(target, "activated", Q_ARG(int, 5)));
        QCOMPARE(cw.toneHz(), 700);
        QCOMPARE(settings.value("CW/toneHz").toInt(), 700);
        QCOMPARE(cw.decoderToneLock(), 0); // Does not replace the decoder filter.
        QCOMPARE(tunes, 0);
        QCOMPARE(sends, 0);
        QCOMPARE(pttChanges, 0);
    }

    void stopButtonAlwaysRequestsEmergencyRelease()
    {
        CwController cw;
        int stops = 0;
        QVector<bool> ptt;
        CwController::Hooks hooks;
        hooks.stopRigMorse = [&] { ++stops; };
        hooks.rigPtt = [&](bool on) { ptt.append(on); };
        cw.setHooks(hooks);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("cwModule", &cw);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CW_WINDOW_QML)));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto* stop = object->findChild<QObject*>(QStringLiteral("cwStopButton"));
        QVERIFY(stop);
        // Btn's MouseArea emits this same signal on a real left click. Calling
        // it directly keeps the test deterministic on the off-screen QPA
        // backend while still exercising the QML onClicked wiring.
        QVERIFY(QMetaObject::invokeMethod(stop, "clicked", Qt::DirectConnection));

        QCOMPARE(stops, 1);
        QCOMPARE(ptt, (QVector<bool>{false}));
    }

    void typingPreservesEveryLetterAndWordBoundary()
    {
        CwController cw;
        QStringList sent;
        CwController::Hooks hooks;
        hooks.rigKeyerAvailable = [] { return true; };
        hooks.sendRigMorse = [&](const QString& text, int) { sent.append(text); return true; };
        cw.setHooks(hooks);
        cw.setWpm(60);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("cwModule", &cw);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CW_WINDOW_QML)));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        window->setProperty("transmitWhileTyping", true);
        QVERIFY(QMetaObject::invokeMethod(window.get(), "queueTypedInput", Q_ARG(QVariant, QVariant("C"))));
        QCOMPARE(sent, (QStringList{"C"}));
        for (const QString& value : {QString("CQ"), QString("CQ "), QString("CQ D"), QString("CQ DE"), QString("CQ DE2")})
            QVERIFY(QMetaObject::invokeMethod(window.get(), "queueTypedInput", Q_ARG(QVariant, QVariant(value))));
        QTRY_VERIFY_WITH_TIMEOUT(!cw.sending() && window->property("queuedTypedText").toString().isEmpty(), 5000);
        QCOMPARE(sent.join(QString()), QStringLiteral("CQ DE2"));
    }

    void stopClearsThePendingKeyboardQueue()
    {
        CwController cw;
        QStringList sent;
        CwController::Hooks hooks;
        hooks.rigKeyerAvailable = [] { return true; };
        hooks.sendRigMorse = [&](const QString& text, int) { sent.append(text); return true; };
        cw.setHooks(hooks);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("cwModule", &cw);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CW_WINDOW_QML)));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        window->setProperty("transmitWhileTyping", true);
        QVERIFY(QMetaObject::invokeMethod(window.get(), "queueTypedInput", Q_ARG(QVariant, QVariant("C"))));
        QVERIFY(QMetaObject::invokeMethod(window.get(), "queueTypedInput", Q_ARG(QVariant, QVariant("CQ"))));
        QCOMPARE(window->property("queuedTypedText").toString(), QStringLiteral("Q"));
        QVERIFY(QMetaObject::invokeMethod(window.get(), "stopSending"));
        QTest::qWait(300);
        QVERIFY(!cw.sending());
        QVERIFY(window->property("queuedTypedText").toString().isEmpty());
        QCOMPARE(sent, (QStringList{"C"}));
    }

    void rejectedInputIsRetainedWithoutAnAutomaticRetryLoop()
    {
        CwController cw;
        bool accept = false;
        int attempts = 0;
        QStringList sent;
        CwController::Hooks hooks;
        hooks.rigKeyerAvailable = [] { return true; };
        hooks.sendRigMorse = [&](const QString& text, int) {
            ++attempts;
            if (accept) sent.append(text);
            return accept;
        };
        cw.setHooks(hooks);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("cwModule", &cw);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CW_WINDOW_QML)));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        window->setProperty("transmitWhileTyping", true);
        QVERIFY(QMetaObject::invokeMethod(window.get(), "queueTypedInput", Q_ARG(QVariant, QVariant("C"))));
        QTest::qWait(100);
        QCOMPARE(attempts, 1);
        QCOMPARE(window->property("queuedTypedText").toString(), QStringLiteral("C"));
        accept = true;
        QVERIFY(QMetaObject::invokeMethod(window.get(), "queueTypedInput", Q_ARG(QVariant, QVariant("CQ"))));
        QCOMPARE(sent, (QStringList{"CQ"}));
        cw.stop();
    }
};

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    TestCwTypingQml test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_cw_typing_qml.moc"
