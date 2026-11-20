#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

class TestSsbWindowFocus final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void mouseOpensMenuWithoutPtt_data();
    void mouseOpensMenuWithoutPtt();
    void selectingAgcSendsValueWithoutPtt();
    void mouseSelectsAgcPreset_data();
    void mouseSelectsAgcPreset();
    void keyboardMenuDoesNotTriggerPtt();
    void openMenuBlocksPttEvenIfFocusReturnsToKeyCatcher();
    void textEntryDoesNotTriggerPtt();
    void spaceStillStartsAndStopsPtt();
    void heldMousePttKeepsFocusUntilRelease_data();
    void heldMousePttKeepsFocusUntilRelease();
    void heldMousePttStillStopsSafely_data();
    void heldMousePttStillStopsSafely();
    void heldMousePttSurvivesTransientMicrophoneEnumeration();
    void closeRequestsLoaderReleaseAndStopsActivity();

private:
    QQuickItem *item(const char *name) const;
    void clickWithEventLoop(QQuickItem *control);
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QObject> m_harness;
    QQuickWindow *m_window {};
    QObject *m_voice {};
    QObject *m_bridge {};
};

void TestSsbWindowFocus::init()
{
    m_engine.reset(new QQmlEngine);
    QQmlComponent component(m_engine.get(), QUrl::fromLocalFile(QStringLiteral(
        DECODIUM_SOURCE_DIR "/tests/qml/ssb_window_focus_harness.qml")));
    m_harness.reset(component.create());
    QVERIFY2(m_harness, qPrintable(component.errorString()));
    m_window = qobject_cast<QQuickWindow *>(m_harness->property("window").value<QObject *>());
    m_voice = m_harness->property("ssbModule").value<QObject *>();
    m_bridge = m_harness->property("appEngine").value<QObject *>();
    QVERIFY(m_window && m_voice && m_bridge);
    QVERIFY(item("ssbRadioAgc"));
    m_window->show();
    m_window->requestActivate();
    QTRY_VERIFY(m_window->isActive());
    QTRY_VERIFY(item("ssbRadioAgc")->isEnabled());
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::cleanup()
{
    m_harness.reset();
    m_engine.reset();
    m_window = nullptr;
    m_voice = nullptr;
    m_bridge = nullptr;
}

QQuickItem *TestSsbWindowFocus::item(const char *name) const
{
    return m_window->findChild<QQuickItem *>(QString::fromLatin1(name));
}

void TestSsbWindowFocus::clickWithEventLoop(QQuickItem *control)
{
    QVERIFY(control);
    QTRY_VERIFY(control->width() > 0 && control->height() > 0);
    const QPoint point = control->mapToScene(
        QPointF(control->width() / 2, control->height() / 2)).toPoint();
    QTest::mousePress(control->window(), Qt::LeftButton, Qt::NoModifier, point);
    // A real click lets Qt.callLater run between press and release. An atomic
    // mouseClick hides the original focus-stealing regression.
    QTest::qWait(100);
    QTest::mouseRelease(control->window(), Qt::LeftButton, Qt::NoModifier, point);
}

void TestSsbWindowFocus::mouseOpensMenuWithoutPtt_data()
{
    QTest::addColumn<QString>("controlName");
    QTest::newRow("radio-agc") << QStringLiteral("ssbRadioAgc");
    QTest::newRow("sideband") << QStringLiteral("ssbSideband");
    // This menu was not included in the old hard-coded popupOpen list.
    QTest::newRow("keyer-language") << QStringLiteral("ssbKeyerLanguage");
}

void TestSsbWindowFocus::mouseOpensMenuWithoutPtt()
{
    QFETCH(QString, controlName);
    auto *combo = item(qPrintable(controlName));
    QVERIFY(combo);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup);
    clickWithEventLoop(combo);
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
    QTest::qWait(100);
    QVERIFY(popup->property("visible").toBool());
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
    QTest::keyClick(m_window, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::selectingAgcSendsValueWithoutPtt()
{
    auto *combo = item("ssbRadioAgc");
    QVERIFY(combo);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup);
    QCOMPARE(combo->property("currentIndex").toInt(), 1); // FAST's value is 5, not its index.
    clickWithEventLoop(combo);
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
    QTest::keyClick(m_window, Qt::Key_Down);
    QTest::keyClick(m_window, Qt::Key_Return);
    QTRY_COMPARE(m_bridge->property("writes").toInt(), 1);
    QCOMPARE(m_bridge->property("lastKey").toString(), QStringLiteral("agc"));
    QCOMPARE(m_bridge->property("lastValue").toDouble(), 2.0); // MID.
    QTRY_VERIFY(combo->isEnabled());
    QTRY_COMPARE(combo->property("currentIndex").toInt(), 2);
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::mouseSelectsAgcPreset_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<int>("value");
    QTest::newRow("off") << 0 << 0;
    QTest::newRow("fast") << 1 << 5; // Hamlib value, not the row index.
    QTest::newRow("mid") << 2 << 2;
    QTest::newRow("slow") << 3 << 3;
}

void TestSsbWindowFocus::mouseSelectsAgcPreset()
{
    QFETCH(int, index);
    QFETCH(int, value);
    auto *combo = item("ssbRadioAgc");
    QVERIFY(combo);
    if (value == 5) {
        QVERIFY(QMetaObject::invokeMethod(m_bridge, "setAgcForTest", Q_ARG(QVariant, 0)));
        QTRY_COMPARE(combo->property("currentIndex").toInt(), 0);
    }
    clickWithEventLoop(combo);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup);
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
    auto *view = popup->property("contentItem").value<QObject *>();
    QVERIFY(view);
    QQuickItem *delegate = nullptr;
    // ListView creates off-current rows lazily on the next layout pass.
    auto findDelegate = [&]() {
        return QMetaObject::invokeMethod(view, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, delegate),
                                         Q_ARG(int, index)) && delegate;
    };
    QTRY_VERIFY_WITH_TIMEOUT(findDelegate(), 1000);
    clickWithEventLoop(delegate);
    QTRY_COMPARE(m_bridge->property("writes").toInt(), 1);
    QCOMPARE(m_bridge->property("lastKey").toString(), QStringLiteral("agc"));
    QCOMPARE(m_bridge->property("lastValue").toDouble(), double(value));
    QTRY_VERIFY(combo->isEnabled());
    QTRY_COMPARE(combo->property("currentIndex").toInt(), index);
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::keyboardMenuDoesNotTriggerPtt()
{
    auto *combo = item("ssbRadioAgc");
    QVERIFY(combo);
    combo->forceActiveFocus(Qt::TabFocusReason);
    QTest::qWait(100);
    QVERIFY(combo->hasActiveFocus());
    QTest::keyClick(m_window, Qt::Key_Space);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup);
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::openMenuBlocksPttEvenIfFocusReturnsToKeyCatcher()
{
    auto *combo = item("ssbKeyerLanguage");
    auto *keyCatcher = item("ssbPttKeyCatcher");
    QVERIFY(combo && keyCatcher);
    clickWithEventLoop(combo);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup);
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
    keyCatcher->forceActiveFocus();
    QTest::qWait(100);
    QVERIFY(popup->property("visible").toBool());
    QTest::keyClick(m_window, Qt::Key_Space);
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::textEntryDoesNotTriggerPtt()
{
    auto *frequency = item("ssbFrequency");
    QVERIFY(frequency);
    frequency->forceActiveFocus();
    QTest::qWait(100);
    QVERIFY(frequency->hasActiveFocus());
    QTest::keyClick(m_window, Qt::Key_Space);
    QVERIFY(frequency->property("text").toString().contains(QLatin1Char(' ')));
    QCOMPARE(m_voice->property("startCalls").toInt(), 0);
}

void TestSsbWindowFocus::spaceStillStartsAndStopsPtt()
{
    auto *keyCatcher = item("ssbPttKeyCatcher");
    QVERIFY(keyCatcher);
    keyCatcher->forceActiveFocus();
    QTest::qWait(100);
    QTest::keyPress(m_window, Qt::Key_Space);
    QCOMPARE(m_voice->property("startCalls").toInt(), 1);
    QVERIFY(m_voice->property("transmitting").toBool());
    const int stopCalls = m_voice->property("stopCalls").toInt();
    QTest::keyRelease(m_window, Qt::Key_Space);
    QCOMPARE(m_voice->property("stopCalls").toInt(), stopCalls + 1);
    QVERIFY(!m_voice->property("transmitting").toBool());
}

void TestSsbWindowFocus::heldMousePttKeepsFocusUntilRelease_data()
{
    QTest::addColumn<int>("focusPolicy");
    QTest::addColumn<QSize>("windowSize");
    // Qt's native default is TabFocus on macOS, StrongFocus on Linux/Windows.
    // Exercise both explicitly so a passing Mac test also covers the reporter.
    QTest::newRow("mac-tab-focus") << int(Qt::TabFocus) << QSize(1200, 820);
    QTest::newRow("linux-windows-click-focus") << int(Qt::StrongFocus) << QSize(1200, 820);
    QTest::newRow("linux-windows-small-window") << int(Qt::StrongFocus) << QSize(760, 560);
}

void TestSsbWindowFocus::heldMousePttKeepsFocusUntilRelease()
{
    QFETCH(int, focusPolicy);
    QFETCH(QSize, windowSize);
    m_window->resize(windowSize);
    auto *button = item("ssbHoldPttButton");
    auto *keyCatcher = item("ssbPttKeyCatcher");
    QVERIFY(button && keyCatcher);
    QVERIFY(button->setProperty("focusPolicy", focusPolicy));
    keyCatcher->forceActiveFocus();
    QTest::qWait(50);
    QSignalSpy cancelled(button, SIGNAL(canceled()));
    QVERIFY(cancelled.isValid());
    const QPoint point = button->mapToScene(
        QPointF(button->width() / 2, button->height() / 2)).toPoint();
    const int stopCalls = m_voice->property("stopCalls").toInt();
    QTest::mousePress(m_window, Qt::LeftButton, Qt::NoModifier, point);
    QCOMPARE(m_voice->property("startCalls").toInt(), 1);
    QVERIFY(m_voice->property("transmitting").toBool());
    // Crucially, process the deferred focus callback before releasing the mouse.
    QTest::qWait(250);
    QCOMPARE(cancelled.count(), 0);
    QVERIFY(button->property("pressed").toBool());
    QVERIFY(m_voice->property("transmitting").toBool());
    QCOMPARE(m_voice->property("stopCalls").toInt(), stopCalls);
    QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, point);
    QTRY_VERIFY(!m_voice->property("transmitting").toBool());
    QCOMPARE(m_voice->property("stopCalls").toInt(), stopCalls + 1);
    QTRY_VERIFY(keyCatcher->hasActiveFocus());
    QTest::keyPress(m_window, Qt::Key_Space);
    QCOMPARE(m_voice->property("startCalls").toInt(), 2);
    QVERIFY(m_voice->property("transmitting").toBool());
    QTest::keyRelease(m_window, Qt::Key_Space);
    QVERIFY(!m_voice->property("transmitting").toBool());
}

void TestSsbWindowFocus::heldMousePttStillStopsSafely_data()
{
    QTest::addColumn<QString>("reason");
    for (const auto *reason : {"release-outside", "escape", "focus-loss",
                              "deactivation", "cat-disconnected", "mode-change", "close"})
        QTest::newRow(reason) << QString::fromLatin1(reason);
}

void TestSsbWindowFocus::heldMousePttStillStopsSafely()
{
    QFETCH(QString, reason);
    auto *button = item("ssbHoldPttButton");
    QVERIFY(button);
    QVERIFY(button->setProperty("focusPolicy", int(Qt::StrongFocus)));
    item("ssbPttKeyCatcher")->forceActiveFocus();
    QTest::qWait(50);
    const QPoint point = button->mapToScene(
        QPointF(button->width() / 2, button->height() / 2)).toPoint();
    QTest::mousePress(m_window, Qt::LeftButton, Qt::NoModifier, point);
    QTest::qWait(100);
    QVERIFY(m_voice->property("transmitting").toBool());
    QWindow other;
    if (reason == "release-outside")
        QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    else if (reason == "escape")
        QTest::keyClick(m_window, Qt::Key_Escape);
    else if (reason == "focus-loss")
        item("ssbFrequency")->forceActiveFocus();
    else if (reason == "deactivation") {
        other.show();
        other.requestActivate();
        QTRY_VERIFY(!m_window->isActive());
    } else if (reason == "cat-disconnected")
        QVERIFY(m_bridge->setProperty("catConnected", false));
    else if (reason == "mode-change")
        QVERIFY(m_bridge->setProperty("mode", QStringLiteral("FT8")));
    else if (reason == "close")
        m_window->close();
    QTRY_VERIFY(!m_voice->property("transmitting").toBool());
    if (reason != "release-outside")
        QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, point);
    QTest::qWait(100);
    QVERIFY(!m_voice->property("transmitting").toBool());
    QCOMPARE(m_voice->property("startCalls").toInt(), 1);
}

void TestSsbWindowFocus::heldMousePttSurvivesTransientMicrophoneEnumeration()
{
    auto *button = item("ssbHoldPttButton");
    QVERIFY(button);
    QVERIFY(button->setProperty("focusPolicy", int(Qt::StrongFocus)));
    QTRY_VERIFY(button->isEnabled());
    const QPoint point = button->mapToScene(
        QPointF(button->width() / 2, button->height() / 2)).toPoint();

    QTest::mousePress(m_window, Qt::LeftButton, Qt::NoModifier, point);
    QTRY_VERIFY(m_voice->property("transmitting").toBool());
    const int stopCalls = m_voice->property("stopCalls").toInt();

    // PipeWire can momentarily publish an empty input list while rebuilding
    // its graph. The selected ComboBox row disappears, but a held PTT must
    // remain pressed so SsbController can perform its bounded stream reopen.
    QVERIFY(m_voice->setProperty("microphones", QVariantList {}));
    QTest::qWait(150);
    QVERIFY(button->isEnabled());
    QVERIFY(m_voice->property("transmitting").toBool());
    QCOMPARE(m_voice->property("stopCalls").toInt(), stopCalls);

    QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, point);
    QTRY_VERIFY(!m_voice->property("transmitting").toBool());
    QCOMPARE(m_voice->property("stopCalls").toInt(), stopCalls + 1);
}

void TestSsbWindowFocus::closeRequestsLoaderReleaseAndStopsActivity()
{
    QSignalSpy releaseSpy(m_window, SIGNAL(releaseRequested()));
    QVERIFY(releaseSpy.isValid());
    const int stopCalls = m_voice->property("stopCalls").toInt();
    m_window->close();
    QTRY_COMPARE(releaseSpy.count(), 1);
    QTRY_VERIFY(!m_window->isVisible());
    QVERIFY(m_voice->property("stopCalls").toInt() > stopCalls);
}

QTEST_MAIN(TestSsbWindowFocus)
#include "test_ssb_window_focus.moc"
