#include "CwController.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTest>
#include <memory>

using namespace decodium::cw;

class TestCwTypingQml : public QObject {
    Q_OBJECT
private slots:
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
