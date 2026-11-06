#include <QColor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QtTest>

#include <memory>

class CwThemeStub final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isGlassTheme READ isGlassTheme CONSTANT)
    Q_PROPERTY(bool isLightTheme READ isLightTheme CONSTANT)
    Q_PROPERTY(bool glassTransparencySupported READ glassTransparencySupported CONSTANT)
    Q_PROPERTY(QColor bgDeep READ bgDeep CONSTANT)
    Q_PROPERTY(QColor bgMedium READ bgMedium CONSTANT)
    Q_PROPERTY(QColor panelColor READ panelColor CONSTANT)
    Q_PROPERTY(QColor borderColor READ borderColor CONSTANT)
    Q_PROPERTY(QColor glassBorder READ glassBorder CONSTANT)
    Q_PROPERTY(QColor textPrimary READ textPrimary CONSTANT)
    Q_PROPERTY(QColor textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QColor accentColor READ accentColor CONSTANT)
    Q_PROPERTY(QColor successColor READ successColor CONSTANT)
    Q_PROPERTY(QColor warningColor READ warningColor CONSTANT)

public:
    bool isGlassTheme() const { return true; }
    bool isLightTheme() const { return false; }
    bool glassTransparencySupported() const { return true; }
    QColor bgDeep() const { return QColor(QStringLiteral("#0A1020")); }
    QColor bgMedium() const { return QColor(QStringLiteral("#14223A")); }
    QColor panelColor() const { return QColor(QStringLiteral("#192B48")); }
    QColor borderColor() const { return QColor(QStringLiteral("#DCF2FF")); }
    QColor glassBorder() const { return QColor(QStringLiteral("#DCF2FF")); }
    QColor textPrimary() const { return QColor(QStringLiteral("#F5FAFF")); }
    QColor textSecondary() const { return QColor(QStringLiteral("#AFC6DF")); }
    QColor accentColor() const { return QColor(QStringLiteral("#8DEBFF")); }
    QColor successColor() const { return QColor(QStringLiteral("#7CF4C8")); }
    QColor warningColor() const { return QColor(QStringLiteral("#FFD18A")); }
};

class CwEngineStub final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *themeManager READ themeManager CONSTANT)
    Q_PROPERTY(bool cwInAscolto READ cwInAscolto WRITE setCwInAscolto NOTIFY cwInAscoltoChanged)
    Q_PROPERTY(bool catConnected READ catConnected CONSTANT)
    Q_PROPERTY(bool transmitting READ transmitting CONSTANT)
    Q_PROPERTY(bool decoPortUseRemote READ decoPortUseRemote CONSTANT)

public:
    explicit CwEngineStub(CwThemeStub *theme)
        : m_theme(theme)
    {
        m_settings.insert(QStringLiteral("uiCwTrafficSplitRatio"), 0.68);
    }

    QObject *themeManager() const { return m_theme; }
    bool cwInAscolto() const { return m_cwInAscolto; }
    void setCwInAscolto(bool value)
    {
        if (m_cwInAscolto == value)
            return;
        m_cwInAscolto = value;
        emit cwInAscoltoChanged();
    }
    bool catConnected() const { return false; }
    bool transmitting() const { return false; }
    bool decoPortUseRemote() const { return false; }

    Q_INVOKABLE QVariant getSetting(const QString &key, const QVariant &fallback) const
    {
        return m_settings.value(key, fallback);
    }
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value)
    {
        m_settings.insert(key, value);
    }
    QVariant setting(const QString &key) const { return m_settings.value(key); }

signals:
    void cwInAscoltoChanged();

private:
    CwThemeStub *m_theme {};
    bool m_cwInAscolto {};
    QHash<QString, QVariant> m_settings;
};

class TestCwWindow final : public QObject
{
    Q_OBJECT

private slots:
    void liquidGlassCwSurfaceStaysReadableAndTrafficSplitPersists();
};

void TestCwWindow::liquidGlassCwSurfaceStaysReadableAndTrafficSplitPersists()
{
    CwThemeStub theme;
    CwEngineStub bridge(&theme);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appEngine"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QQmlComponent component(
        &engine,
        QUrl::fromLocalFile(QStringLiteral(
            DECODIUM_SOURCE_DIR "/qml/decodium/components/CwWindow.qml")));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));

    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window != nullptr);
    QTRY_VERIFY(window->property("liquidGlassEnabled").toBool());

    const QColor contentSurface = qvariant_cast<QColor>(window->property("cContentSurface"));
    const QColor readSurface = qvariant_cast<QColor>(window->property("cReadSurface"));
    QCOMPARE(contentSurface.alpha(), 245); // 96% opacity: readable over any backdrop.
    QCOMPARE(readSurface.alpha(), 251);    // Transcript needs still more contrast.
    QTRY_COMPARE(window->property("trafficSplitRatio").toDouble(), 0.68);

    // Exercise the actual MouseArea, not merely its helper function: a drag
    // must change the split and commit the result on release.
    auto *divider = window->findChild<QQuickItem *>(QStringLiteral("cwTrafficDivider"));
    QVERIFY(divider != nullptr);
    window->show();
    QTRY_VERIFY(window->isVisible());
    QTRY_VERIFY(divider->width() > 0 && divider->height() > 100);

    // RX and TX content must belong to their bordered transcript panels.  In
    // particular, the first visible line must not be laid out in the header
    // gap above a ScrollView background (the original CW-window regression).
    auto *rxBox = window->findChild<QQuickItem *>(QStringLiteral("cwRxTranscriptBox"));
    auto *txBox = window->findChild<QQuickItem *>(QStringLiteral("cwTxTranscriptBox"));
    auto *rxText = window->findChild<QQuickItem *>(QStringLiteral("cwRxTranscriptText"));
    auto *txText = window->findChild<QQuickItem *>(QStringLiteral("cwTxTranscriptText"));
    QVERIFY(rxBox != nullptr);
    QVERIFY(txBox != nullptr);
    QVERIFY(rxText != nullptr);
    QVERIFY(txText != nullptr);
    QVERIFY(rxBox->property("clip").toBool());
    QVERIFY(txBox->property("clip").toBool());
    rxText->setProperty("text", QStringLiteral("[RX] TEST MESSAGE"));
    txText->setProperty("text", QStringLiteral("[TX] TEST MESSAGE"));
    QTRY_VERIFY(rxBox->width() > 0 && rxBox->height() >= 70);
    QTRY_VERIFY(txBox->width() > 0 && txBox->height() >= 70);
    const QRectF rxBoxScene(rxBox->mapToScene(QPointF(0, 0)),
                            QSizeF(rxBox->width(), rxBox->height()));
    const QRectF txBoxScene(txBox->mapToScene(QPointF(0, 0)),
                            QSizeF(txBox->width(), txBox->height()));
    const QPointF rxTextOrigin = rxText->mapToScene(
        QPointF(rxText->property("leftPadding").toReal(),
                rxText->property("topPadding").toReal()));
    const QPointF txTextOrigin = txText->mapToScene(
        QPointF(txText->property("leftPadding").toReal(),
                txText->property("topPadding").toReal()));
    QVERIFY(rxBoxScene.adjusted(1, 1, -1, -1).contains(rxTextOrigin));
    QVERIFY(txBoxScene.adjusted(1, 1, -1, -1).contains(txTextOrigin));

    const double ratioBeforeDrag = window->property("trafficSplitRatio").toDouble();
    const QPoint pressPoint = divider->mapToScene(
        QPointF(divider->width() / 2.0, divider->height() / 2.0)).toPoint();
    const QPoint dragPoint(qMin(window->width() - 190, pressPoint.x() + 80), pressPoint.y());
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pressPoint);
    QTest::mouseMove(window, dragPoint, 20);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, dragPoint);
    QTRY_VERIFY(window->property("trafficSplitRatio").toDouble() > ratioBeforeDrag);
    QTRY_COMPARE(bridge.setting(QStringLiteral("uiCwTrafficSplitRatio")).toDouble(),
                 window->property("trafficSplitRatio").toDouble());

    // A drag near the edge cannot collapse a pane below its 180 px minimum.
    QVERIFY(QMetaObject::invokeMethod(window, "setTrafficSplitFromX",
                                      Q_ARG(QVariant, QVariant(10.0)),
                                      Q_ARG(QVariant, QVariant(1000.0))));
    QTRY_COMPARE(window->property("trafficSplitRatio").toDouble(), 0.18);

    // The final ratio is stored only when the drag finishes (or is cancelled),
    // exactly as the UI's MouseArea does.
    window->setProperty("trafficSplitRatio", 0.61);
    QVERIFY(QMetaObject::invokeMethod(window, "persistTrafficSplitRatio"));
    QCOMPARE(bridge.setting(QStringLiteral("uiCwTrafficSplitRatio")).toDouble(), 0.61);
    QSignalSpy releaseSpy(window, SIGNAL(releaseRequested()));
    QVERIFY(releaseSpy.isValid());
    QVERIFY(bridge.cwInAscolto());
    window->close();
    QTRY_COMPARE(releaseSpy.count(), 1);
    QTRY_VERIFY(!window->isVisible());
    QTRY_VERIFY(!bridge.cwInAscolto());
}

QTEST_MAIN(TestCwWindow)
#include "test_cw_window.moc"
