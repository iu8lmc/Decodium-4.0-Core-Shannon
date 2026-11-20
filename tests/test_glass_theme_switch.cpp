#include <QColor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

class ThemeStub final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isGlassTheme READ isGlassTheme NOTIFY paletteChanged)
    Q_PROPERTY(bool glassTransparencySupported READ glassTransparencySupported NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgDeep READ bgDeep NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgMedium READ bgMedium NOTIFY paletteChanged)
    Q_PROPERTY(QColor glassBorder READ glassBorder NOTIFY paletteChanged)

public:
    bool isGlassTheme() const { return m_isGlassTheme; }
    bool glassTransparencySupported() const { return true; }
    QColor bgDeep() const { return m_isGlassTheme ? QColor("#0A1020") : QColor("#0A0F1A"); }
    QColor bgMedium() const { return m_isGlassTheme ? QColor("#14223A") : QColor("#111827"); }
    QColor glassBorder() const { return m_isGlassTheme ? QColor("#DCF2FF") : QColor("#4A90E2"); }

    void setGlassTheme(bool value)
    {
        if (m_isGlassTheme == value)
            return;
        m_isGlassTheme = value;
        emit paletteChanged();
    }

signals:
    void paletteChanged();

private:
    bool m_isGlassTheme {true};
};

class TestGlassThemeSwitch final : public QObject
{
    Q_OBJECT

private slots:
    void switchingFromGlassToClassicMakesSharedSurfaceOpaque();
};

void TestGlassThemeSwitch::switchingFromGlassToClassicMakesSharedSurfaceOpaque()
{
    ThemeStub theme;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("testTheme"), &theme);

    QQmlComponent component(
        &engine,
        QUrl::fromLocalFile(QStringLiteral(
            DECODIUM_SOURCE_DIR "/tests/qml/glass_theme_switch_harness.qml")));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));

    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window != nullptr);

    auto colorProperty = [window](const char *name) {
        return qvariant_cast<QColor>(window->property(name));
    };

    QTRY_VERIFY(window->property("glassTheme").toBool());
    QCOMPARE(colorProperty("glassSurfaceColor").alpha(), 77);
    QCOMPARE(colorProperty("color").alpha(), 0);

    theme.setGlassTheme(false);

    QTRY_VERIFY(!window->property("glassTheme").toBool());
    QTRY_VERIFY(!window->property("transparentCorners").toBool());
    QTRY_COMPARE(colorProperty("glassSurfaceColor").alpha(), 255);
    QTRY_COMPARE(colorProperty("color").alpha(), 255);
}

QTEST_MAIN(TestGlassThemeSwitch)
#include "test_glass_theme_switch.moc"
