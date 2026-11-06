#include <QColor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

class JttyThemeStub final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isGlassTheme READ isGlassTheme NOTIFY paletteChanged)
    Q_PROPERTY(bool isLightTheme READ isLightTheme NOTIFY paletteChanged)
    Q_PROPERTY(bool glassTransparencySupported READ glassTransparencySupported NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgDeep READ bgDeep NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgMedium READ bgMedium NOTIFY paletteChanged)
    Q_PROPERTY(QColor panelColor READ panelColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor glassBorder READ glassBorder NOTIFY paletteChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY paletteChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor successColor READ successColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor warningColor READ warningColor NOTIFY paletteChanged)

public:
    bool isGlassTheme() const { return m_glassTheme; }
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

    void setGlassTheme(bool enabled)
    {
        if (m_glassTheme == enabled)
            return;
        m_glassTheme = enabled;
        emit paletteChanged();
    }

signals:
    void paletteChanged();

private:
    bool m_glassTheme {true};
};

class JttyEngineStub final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *themeManager READ themeManager CONSTANT)
    Q_PROPERTY(int rxFrequency READ rxFrequency CONSTANT)
    Q_PROPERTY(int txFrequency READ txFrequency WRITE setTxFrequency NOTIFY txFrequencyChanged)
    Q_PROPERTY(QString dxCall READ dxCall WRITE setDxCall NOTIFY dxCallChanged)

public:
    explicit JttyEngineStub(JttyThemeStub *theme)
        : m_theme(theme)
    {
    }

    QObject *themeManager() const { return m_theme; }
    int rxFrequency() const { return 1478; }
    int txFrequency() const { return m_txFrequency; }
    void setTxFrequency(int frequency)
    {
        if (m_txFrequency == frequency)
            return;
        m_txFrequency = frequency;
        emit txFrequencyChanged();
    }
    QString dxCall() const { return m_dxCall; }
    void setDxCall(const QString &call)
    {
        if (m_dxCall == call)
            return;
        m_dxCall = call;
        emit dxCallChanged();
    }

signals:
    void txFrequencyChanged();
    void dxCallChanged();

private:
    JttyThemeStub *m_theme {};
    int m_txFrequency {2301};
    QString m_dxCall;
};

class TestJttyWindow final : public QObject
{
    Q_OBJECT

private slots:
    void liquidGlassJttySurfaceStaysReadableAndClassicFallbackStaysOpaque();
};

void TestJttyWindow::liquidGlassJttySurfaceStaysReadableAndClassicFallbackStaysOpaque()
{
    JttyThemeStub theme;
    JttyEngineStub bridge(&theme);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appEngine"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QQmlComponent component(
        &engine,
        QUrl::fromLocalFile(QStringLiteral(
            DECODIUM_SOURCE_DIR "/qml/decodium/components/JttyWindow.qml")));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));

    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window != nullptr);
    QTRY_VERIFY(window->property("liquidGlassEnabled").toBool());

    const auto colorProperty = [window](const char *name) {
        return qvariant_cast<QColor>(window->property(name));
    };
    QCOMPARE(colorProperty("cContentSurface").alpha(), 245);
    QCOMPARE(colorProperty("cPanel").alpha(), 240);
    QCOMPARE(colorProperty("cInputSurface").alpha(), 250);
    QCOMPARE(colorProperty("cReadSurface").alpha(), 251);

    auto *readabilitySurface = window->findChild<QQuickItem *>(
        QStringLiteral("jttyReadabilitySurface"));
    QVERIFY(readabilitySurface != nullptr);
    QCOMPARE(qvariant_cast<QColor>(readabilitySurface->property("color")).alpha(), 245);

    window->show();
    QTRY_VERIFY(window->isVisible());
    QTRY_VERIFY(readabilitySurface->width() > 0 && readabilitySurface->height() > 0);

    // Switching to a classic theme must undo only the Glass treatment; it
    // must not leave an alpha work surface behind in another theme.
    theme.setGlassTheme(false);
    QTRY_VERIFY(!window->property("liquidGlassEnabled").toBool());
    QTRY_COMPARE(colorProperty("cContentSurface").alpha(), 255);
    QTRY_COMPARE(colorProperty("cPanel").alpha(), 255);
    QTRY_COMPARE(colorProperty("cInputSurface").alpha(), 255);
    QTRY_COMPARE(colorProperty("cReadSurface").alpha(), 255);
    QTRY_COMPARE(qvariant_cast<QColor>(readabilitySurface->property("color")).alpha(), 255);
    window->hide();
}

QTEST_MAIN(TestJttyWindow)
#include "test_jtty_window.moc"
