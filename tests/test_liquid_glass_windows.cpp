#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestLiquidGlassWindows final : public QObject
{
    Q_OBJECT

private slots:
    void shellIsCrossPlatformAndThemeGated();
    void classicThemeRestoresOpaqueSurface();
    void mainAndPopoutsUseGlassShell();
    void classicMainSurfacesAreOpaque();
    void dashboardMenusKeepReadableGlassContrast();
    void liveMapFramelessPopoutHasAllResizeHandles();
    void dialogsAndHostedPanelsUseGlassSurfaces();
    void cwWindowKeepsOperationalContrastAndHasPersistentTrafficSplitter();
    void cwFunctionKeysOverrideDashboardShortcuts();
    void jttyWindowKeepsOperationalContrast();
    void restoredPopoutsWaitForStartupPresentation();
    void linuxUsesSafeOpaqueGlassFallback();
    void closedCwAndSsbPopoutsReleaseTheirSceneGraphs();
};

void TestLiquidGlassWindows::shellIsCrossPlatformAndThemeGated()
{
    const QString window = readSource(QStringLiteral("qml/decodium/components/GlassWindow.qml"));
    const QString applicationWindow = readSource(
        QStringLiteral("qml/decodium/components/GlassApplicationWindow.qml"));
    QVERIFY(!window.isEmpty());
    QVERIFY(!applicationWindow.isEmpty());

    for (const QString& shell : {window, applicationWindow}) {
        QVERIFY(shell.contains(QStringLiteral("isGlassTheme")));
        QVERIFY(shell.contains(QStringLiteral("glassTransparencySupported")));
        QVERIFY(shell.contains(QStringLiteral("Qt.rgba")));
        QVERIFY(!shell.contains(QStringLiteral("Qt.platform.os !== \"linux\"")));
        QVERIFY(shell.contains(QStringLiteral("default property alias windowContent")));
        // Do not make the Linux Qt 6.4 runtime depend on MultiEffect/native
        // material modules just to display the optional theme.
        QVERIFY(!shell.contains(QStringLiteral("import QtQuick.Effects")));
        QVERIFY(!shell.contains(QStringLiteral("MultiEffect {")));
    }
}

void TestLiquidGlassWindows::cwFunctionKeysOverrideDashboardShortcuts()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    const QString cw = readSource(QStringLiteral("qml/decodium/components/CwWindow.qml"));
    QVERIFY(!main.isEmpty());
    QVERIFY(!cw.isEmpty());

    QVERIFY(main.contains(QStringLiteral("cwFunctionKeyWindowActive")));
    QVERIFY(main.contains(QStringLiteral("enabled: !mainWindow.cwFunctionKeyWindowActive")));
    QVERIFY(cw.contains(QStringLiteral("context: Qt.WindowShortcut")));
    QVERIFY(cw.contains(QStringLiteral("sequence: \"F\" + (index + 1)")));
    QVERIFY(cw.contains(QStringLiteral("win.cw.sendMacro(index, win.ctx())")));
}

void TestLiquidGlassWindows::classicThemeRestoresOpaqueSurface()
{
    const QStringList shellPaths {
        QStringLiteral("qml/decodium/components/GlassWindow.qml"),
        QStringLiteral("qml/decodium/components/GlassApplicationWindow.qml")
    };

    for (const QString &path : shellPaths) {
        const QString shell = readSource(path);
        QVERIFY2(!shell.isEmpty(), qPrintable(path));

        const int surfaceStart = shell.indexOf(
            QStringLiteral("readonly property color glassSurfaceColor:"));
        const int nextSection = shell.indexOf(
            QStringLiteral("default property alias windowContent"), surfaceStart);
        QVERIFY2(surfaceStart >= 0, qPrintable(path));
        QVERIFY2(nextSection > surfaceStart, qPrintable(path));

        const QString surfaceBinding = shell.mid(surfaceStart, nextSection - surfaceStart);
        QVERIFY(surfaceBinding.contains(QStringLiteral("root.glassTheme")));
        QVERIFY(surfaceBinding.contains(
            QStringLiteral("root.glassTransparencySupported")));
        // This is the regression guard for a live Liquid Glass -> classic
        // theme switch: any pop-out that binds directly to glassSurfaceColor
        // must be repainted with an alpha=1 surface.
        QVERIFY(surfaceBinding.contains(QStringLiteral(": root.opaqueSurfaceColor")));
    }
}

void TestLiquidGlassWindows::mainAndPopoutsUseGlassShell()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!main.isEmpty());
    QVERIFY(main.startsWith(QStringLiteral("/* Decodium 4.0")));
    QVERIFY(main.contains(QStringLiteral("GlassApplicationWindow {")));
    QVERIFY(main.contains(QStringLiteral("id: waterfallWindow")));
    QVERIFY(main.contains(QStringLiteral("id: logFloatingWindow")));
    QVERIFY(main.contains(QStringLiteral("id: settingsFloatingWindow")));
    QVERIFY(main.contains(QStringLiteral("id: txPanelFloatingWindow")));
    QVERIFY(main.contains(QStringLiteral("property bool closeInProgress: false")));
    QVERIFY(main.contains(QStringLiteral("id: logNativePopupLoader")));
    QVERIFY(!main.contains(QStringLiteral("id: logWindowLoader")));
    QCOMPARE(main.count(QStringLiteral("source: \"components/LogWindow.qml\"")), 1);

    const QString liveMap = readSource(QStringLiteral("qml/decodium/components/LiveMapPanel.qml"));
    QVERIFY(!liveMap.isEmpty());
    QVERIFY(liveMap.contains(QStringLiteral("mapSurfaceOpacity")));
    QVERIFY(liveMap.contains(QStringLiteral("root.detached && !!(engine")));
    QVERIFY(liveMap.contains(QStringLiteral("opacity: root.mapSurfaceOpacity")));
    QVERIFY(liveMap.contains(QStringLiteral("detachedWindow")));
    QVERIFY(liveMap.contains(QStringLiteral("startSystemMove")));
    QVERIFY(liveMap.contains(QStringLiteral("root.detached ? \"transparent\" : root.bgDeep")));
    QVERIFY(main.contains(QStringLiteral("function glassWindowSurface(fallbackAlpha)")));
    QVERIFY(main.contains(QStringLiteral("function glassDialogSurface(fallbackAlpha)")));
    QVERIFY(main.contains(QStringLiteral("flags: Qt.Window | Qt.FramelessWindowHint")));

    const QString dxCluster = readSource(QStringLiteral("qml/decodium/components/DxClusterPanel.qml"));
    QVERIFY(!dxCluster.isEmpty());
    QVERIFY(dxCluster.contains(QStringLiteral("glassSurfaceEnabled")));
    QVERIFY(dxCluster.contains(QStringLiteral("!root.embedded && bridge.themeManager")));
    QVERIFY(dxCluster.contains(QStringLiteral("root.embedded\n                   ? root.bgDeep")));
    QVERIFY(dxCluster.contains(QStringLiteral("0.30")));
    const QString txPanel = readSource(QStringLiteral("qml/decodium/components/TxPanel.qml"));
    QVERIFY(txPanel.contains(QStringLiteral("property bool detachedSurface: false")));
    QVERIFY(txPanel.contains(QStringLiteral("txPanel.detachedSurface && engine")));
    const QString ft2Link = readSource(QStringLiteral("qml/panels/FT2LinkPanel.qml"));
    QVERIFY(ft2Link.contains(QStringLiteral("readonly property bool liquidGlassEnabled: poppedOut")));
    QVERIFY(main.contains(QStringLiteral("detachedSurface: true")));
    QVERIFY(main.contains(QStringLiteral("item.poppedOut = false")));
    QVERIFY(main.contains(QStringLiteral("item.poppedOut = true")));
    QVERIFY(main.contains(QStringLiteral("color: mainWindow.period1Detached ? \"transparent\" : bgDeep")));
    QVERIFY(main.contains(QStringLiteral("color: mainWindow.rxFreqDetached ? \"transparent\" : bgDeep")));
    QVERIFY(main.contains(QStringLiteral("The docked TX host is opaque")));
    QVERIFY(main.contains(QStringLiteral("id: dxcTitleBar")));
    QVERIFY(main.contains(QStringLiteral("Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint")));

    const QStringList popouts = {
        QStringLiteral("qml/decodium/components/CallDialog.qml"),
        QStringLiteral("qml/decodium/components/CwWindow.qml"),
        QStringLiteral("qml/decodium/components/DecodeHistoryDialog.qml"),
        QStringLiteral("qml/decodium/components/DecodeWindow.qml"),
        QStringLiteral("qml/decodium/components/DecoPortWindow.qml"),
        QStringLiteral("qml/decodium/components/DecolinkWindow.qml"),
        QStringLiteral("qml/decodium/components/DxPedFloatWindow.qml"),
        QStringLiteral("qml/decodium/components/JttyWindow.qml"),
        QStringLiteral("qml/decodium/components/QsyQuickPickerDialog.qml"),
        QStringLiteral("qml/decodium/components/SsbWindow.qml"),
        QStringLiteral("qml/decodium/components/RotorWindow.qml"),
        QStringLiteral("qml/decodium/components/sstv/SstvWorkspace.qml"),
        QStringLiteral("qml/decodium/rtty/RttyMain.qml")
    };
    for (const QString& path : popouts) {
        const QString source = readSource(path);
        QVERIFY2(!source.isEmpty(), qPrintable(path));
        QVERIFY2(source.contains(QStringLiteral("GlassWindow {"))
                         || source.contains(QStringLiteral("GlassApplicationWindow {")),
                  qPrintable(path));
        QVERIFY2(source.contains(QStringLiteral("glassSurfaceColor"))
                         || source.contains(QStringLiteral("glassTransparencySupported"))
                         || source.contains(QStringLiteral("liquidGlassTheme")),
                 qPrintable(path));
    }
}

void TestLiquidGlassWindows::classicMainSurfacesAreOpaque()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!main.isEmpty());

    const int windowStart = main.indexOf(
        QStringLiteral("function glassWindowSurface(fallbackAlpha)"));
    const int dialogStart = main.indexOf(
        QStringLiteral("function glassDialogSurface(fallbackAlpha)"));
    const int menuStart = main.indexOf(
        QStringLiteral("function glassMenuSurface()"), dialogStart);
    QVERIFY(windowStart >= 0);
    QVERIFY(dialogStart > windowStart);
    QVERIFY(menuStart > dialogStart);

    const QString windowSurface = main.mid(windowStart, dialogStart - windowStart);
    const QString dialogSurface = main.mid(
        dialogStart, menuStart - dialogStart);
    QVERIFY(windowSurface.contains(QStringLiteral("return bgDeep")));
    QVERIFY(dialogSurface.contains(QStringLiteral("return bgDeep")));
    QVERIFY(!windowSurface.contains(
        QStringLiteral("Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, fallbackAlpha)")));
    QVERIFY(!dialogSurface.contains(
        QStringLiteral("Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, fallbackAlpha)")));
}

void TestLiquidGlassWindows::dashboardMenusKeepReadableGlassContrast()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!main.isEmpty());

    const int menuSurfaceStart = main.indexOf(
        QStringLiteral("function glassMenuSurface()"));
    const int nextFunction = main.indexOf(
        QStringLiteral("function minimizeFloatingWindow"), menuSurfaceStart);
    QVERIFY(menuSurfaceStart >= 0);
    QVERIFY(nextFunction > menuSurfaceStart);

    const QString menuSurface = main.mid(
        menuSurfaceStart, nextFunction - menuSurfaceStart);
    QVERIFY(menuSurface.contains(QStringLiteral("glassTransparencySupported")));
    QVERIFY(menuSurface.contains(QStringLiteral("Math.max(0.88, panel.a)")));
    QVERIFY(menuSurface.contains(QStringLiteral("return bgDeep")));

    const int mainMenuStart = main.indexOf(QStringLiteral("id: mainMenu"));
    const int languageMenuStart = main.indexOf(
        QStringLiteral("id: languageSubMenu"), mainMenuStart);
    QVERIFY(mainMenuStart >= 0);
    QVERIFY(languageMenuStart > mainMenuStart);

    const QString mainMenu = main.mid(
        mainMenuStart, languageMenuStart - mainMenuStart);
    QVERIFY(mainMenu.contains(QStringLiteral("mainWindow.glassMenuSurface()")));

    const int languageMenuEnd = main.indexOf(
        QStringLiteral("MenuSeparator"), languageMenuStart);
    QVERIFY(languageMenuEnd > languageMenuStart);
    const QString languageMenu = main.mid(
        languageMenuStart, languageMenuEnd - languageMenuStart);
    QVERIFY(languageMenu.contains(QStringLiteral("mainWindow.glassMenuSurface()")));

    const int clockMenuStart = main.indexOf(QStringLiteral("id: worldClockMenu"));
    const int clockMenuEnd = main.indexOf(
        QStringLiteral("MenuItem"), clockMenuStart);
    QVERIFY(clockMenuStart >= 0);
    QVERIFY(clockMenuEnd > clockMenuStart);
    const QString clockMenu = main.mid(clockMenuStart, clockMenuEnd - clockMenuStart);
    QVERIFY(clockMenu.contains(QStringLiteral("mainWindow.glassMenuSurface()")));
}

void TestLiquidGlassWindows::liveMapFramelessPopoutHasAllResizeHandles()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!main.isEmpty());

    const int liveMapStart = main.indexOf(QStringLiteral("id: liveMapFloatingWindow"));
    const int nextPopout = main.indexOf(
        QStringLiteral("// ========== DETACHABLE FULL SPECTRUM WINDOW =========="),
        liveMapStart);
    QVERIFY(liveMapStart >= 0);
    QVERIFY(nextPopout > liveMapStart);

    const QString liveMapWindow = main.mid(liveMapStart, nextPopout - liveMapStart);
    QVERIFY(liveMapWindow.contains(QStringLiteral("Qt.FramelessWindowHint")));
    // A frameless window has no native border.  FloatingResizeHandles provides
    // the four edges plus all four corners for independent width/height resize.
    QVERIFY(liveMapWindow.contains(QStringLiteral("FloatingResizeHandles {")));
    QVERIFY(liveMapWindow.contains(
        QStringLiteral("targetWindow: liveMapFloatingWindow")));

    const int handlesStart = main.indexOf(
        QStringLiteral("component FloatingResizeHandles: Item"));
    const int nextHandleComponent = main.indexOf(
        QStringLiteral("component ProportionalResizeHandles: Item"), handlesStart);
    QVERIFY(handlesStart >= 0);
    QVERIFY(nextHandleComponent > handlesStart);
    const QString handles = main.mid(handlesStart, nextHandleComponent - handlesStart);
    // Keep this as an explicit contract: each detached frameless popout that
    // uses the common handles is resizable from all four sides and corners.
    const QStringList requiredHandles {
        QStringLiteral("resizeTopHandle"),
        QStringLiteral("resizeBottomHandle"),
        QStringLiteral("resizeLeftHandle"),
        QStringLiteral("resizeRightHandle"),
        QStringLiteral("resizeTopLeftHandle"),
        QStringLiteral("resizeTopRightHandle"),
        QStringLiteral("resizeBottomLeftHandle"),
        QStringLiteral("resizeBottomRightHandle")
    };
    for (const QString &handle : requiredHandles)
        QVERIFY(handles.contains(handle));
}

void TestLiquidGlassWindows::dialogsAndHostedPanelsUseGlassSurfaces()
{
    const QStringList surfaces = {
        QStringLiteral("qml/decodium/CatSettingsDialog.qml"),
        QStringLiteral("qml/decodium/components/LogWindow.qml"),
        QStringLiteral("qml/decodium/components/LogWindowContent.qml"),
        QStringLiteral("qml/decodium/components/SettingsDialog.qml"),
        QStringLiteral("qml/decodium/components/StyledComboBox.qml"),
        QStringLiteral("qml/decodium/components/UpdateDialog.qml"),
        QStringLiteral("qml/decodium/components/BugReportDialog.qml"),
        QStringLiteral("qml/decodium/components/StationTelemetryDialog.qml"),
        QStringLiteral("qml/decodium/components/RigDetectResults.qml"),
        QStringLiteral("qml/dialogs/ColorHighlightingDialog.qml"),
        QStringLiteral("qml/dialogs/QSYDialog.qml"),
        QStringLiteral("qml/panels/ActiveStationsPanel.qml"),
        QStringLiteral("qml/panels/FT2LinkPanel.qml"),
        QStringLiteral("qml/panels/TimeSyncPanel.qml"),
        QStringLiteral("qml/decodium/components/DecoSyncPanel.qml"),
        QStringLiteral("qml/decodium/components/DecometerWindow.qml"),
        QStringLiteral("qml/decodium/components/AstroWindow.qml")
    };

    for (const QString& path : surfaces) {
        const QString source = readSource(path);
        QVERIFY2(!source.isEmpty(), qPrintable(path));
        QVERIFY2(source.contains(QStringLiteral("glassTransparencySupported"))
                     || source.contains(QStringLiteral("dialogSurface"))
                     || source.contains(QStringLiteral("popupSurface"))
                     || source.contains(QStringLiteral("liquidGlassEnabled")),
                 qPrintable(path));
    }
}

void TestLiquidGlassWindows::cwWindowKeepsOperationalContrastAndHasPersistentTrafficSplitter()
{
    const QString cw = readSource(QStringLiteral("qml/decodium/components/CwWindow.qml"));
    QVERIFY(!cw.isEmpty());

    // The glass frame remains optional, but dense CW controls and transcripts
    // must not be rendered on the generic low-alpha pop-out surface.
    QVERIFY(cw.contains(QStringLiteral("readonly property bool liquidGlassEnabled")));
    QVERIFY(cw.contains(QStringLiteral("readonly property color cContentSurface")));
    QVERIFY(cw.contains(QStringLiteral("readonly property color cReadSurface")));
    QVERIFY(cw.contains(QStringLiteral("id: cwReadabilitySurface")));
    QVERIFY(cw.contains(QStringLiteral("color: win.cContentSurface")));
    QVERIFY(cw.contains(QStringLiteral("color: win.cReadSurface")));

    // RX and TX must be independently resizable and the user's choice must
    // survive close/reopen without allowing either transcript to collapse.
    QVERIFY(cw.contains(QStringLiteral("property real trafficSplitRatio: 0.5")));
    QVERIFY(cw.contains(QStringLiteral("trafficMinimumPaneWidth: 180")));
    QVERIFY(cw.contains(QStringLiteral("function clampTrafficSplitRatio")));
    QVERIFY(cw.contains(QStringLiteral("function restoreTrafficSplitRatio")));
    QVERIFY(cw.contains(QStringLiteral("function persistTrafficSplitRatio")));
    QVERIFY(cw.contains(QStringLiteral("uiCwTrafficSplitRatio")));
    QVERIFY(cw.contains(QStringLiteral("id: cwTrafficDivider")));
    QVERIFY(cw.contains(QStringLiteral("cursorShape: Qt.SizeHorCursor")));
    QVERIFY(cw.contains(QStringLiteral("onDoubleClicked:")));
}

void TestLiquidGlassWindows::jttyWindowKeepsOperationalContrast()
{
    const QString jtty = readSource(QStringLiteral("qml/decodium/components/JttyWindow.qml"));
    QVERIFY(!jtty.isEmpty());

    // A JTTY window has two continuously-read receive lists, compact macro
    // previews and a transmit editor.  Those cannot use the generic low-alpha
    // pop-out panel that intentionally reveals the desktop behind it.
    QVERIFY(jtty.contains(QStringLiteral("import QtQuick.Controls.Material")));
    QVERIFY(jtty.contains(QStringLiteral("readonly property bool liquidGlassEnabled")));
    QVERIFY(jtty.contains(QStringLiteral("readonly property color cContentSurface")));
    QVERIFY(jtty.contains(QStringLiteral("readonly property color cInputSurface")));
    QVERIFY(jtty.contains(QStringLiteral("readonly property color cReadSurface")));
    QVERIFY(jtty.contains(QStringLiteral("id: jttyReadabilitySurface")));
    QVERIFY(jtty.contains(QStringLiteral("color: win.cContentSurface")));
    QVERIFY(jtty.contains(QStringLiteral("color: win.cReadSurface")));
    QVERIFY(jtty.contains(QStringLiteral("Material.theme:")));
    QVERIFY(jtty.contains(QStringLiteral("Material.background: cContentSurface")));
}

void TestLiquidGlassWindows::restoredPopoutsWaitForStartupPresentation()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(main.contains(QStringLiteral("property bool startupPresentationFinished: false")));
    QVERIFY(main.contains(QStringLiteral("function syncDxClusterFloatingVisibility(activate)")));
    QVERIFY(main.contains(QStringLiteral("suppressVisibleRestore")));
    QVERIFY(main.contains(QStringLiteral("\"dxClusterDetached\", \"dxClusterMinimized\", true)")));
    QVERIFY(main.contains(QStringLiteral("!startupPresentationFinished")));
    QVERIFY(main.contains(QStringLiteral("mainWindow.startupPresentationFinished\n                 && mainWindow.dxClusterPanelVisible")));
    QVERIFY(main.contains(QStringLiteral("mainWindow.finishStartupPresentation()")));
    QVERIFY(main.contains(QStringLiteral("visible: mainWindow.startupPresentationFinished\n                             && showWorldClock")));
}

void TestLiquidGlassWindows::linuxUsesSafeOpaqueGlassFallback()
{
    const QString themeManager = readSource(
        QStringLiteral("src/ui/DecodiumThemeManager.cpp"));
    const QString themeHeader = readSource(
        QStringLiteral("src/ui/DecodiumThemeManager.h"));
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!themeManager.isEmpty());
    QVERIFY(!themeHeader.isEmpty());
    QVERIFY(!main.isEmpty());

    // Linux does not enable alpha top-level surfaces merely because Qt says
    // the session is xcb/Wayland: that is insufficient to establish a safe
    // compositor/driver combination.  It is an explicit opt-in only.
    QVERIFY(themeManager.contains(QStringLiteral("DECODIUM_ENABLE_LINUX_GLASS")));
    QVERIFY(themeHeader.contains(QStringLiteral("DECODIUM_ENABLE_LINUX_GLASS=1")));

    // The saved initial mode must not reopen CW/SSB on the next launch after
    // an operator closed its pop-out in the previous session.
    const int modeConnection = main.indexOf(QStringLiteral("function onModeChanged()"));
    QVERIFY(modeConnection >= 0);
    const QString modeHandler = main.mid(modeConnection, 900);
    QVERIFY(modeHandler.contains(QStringLiteral("!mainWindow.startupPresentationFinished")));
    QVERIFY(!main.contains(QStringLiteral(
        "Component.onCompleted: if (bridge && bridge.mode === \"SSB\") active = true")));
}

void TestLiquidGlassWindows::closedCwAndSsbPopoutsReleaseTheirSceneGraphs()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    const QString cw = readSource(QStringLiteral("qml/decodium/components/CwWindow.qml"));
    const QString ssb = readSource(QStringLiteral("qml/decodium/components/SsbWindow.qml"));
    const QString bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!main.isEmpty());
    QVERIFY(!cw.isEmpty());
    QVERIFY(!ssb.isEmpty());
    QVERIFY(!bridge.isEmpty());

    // QQuickWindow::close() hides a window; it does not destroy a Loader item.
    // Both operator windows must explicitly ask their owner to release them.
    QVERIFY(cw.contains(QStringLiteral("signal releaseRequested()")));
    QVERIFY(cw.contains(QStringLiteral("onClosing: function(close)")));
    QVERIFY(ssb.contains(QStringLiteral("signal releaseRequested()")));
    QVERIFY(ssb.contains(QStringLiteral("onClosing: function(close)")));
    QCOMPARE(main.count(QStringLiteral("mainWindow.releaseLoadedWindow(")), 2);

    const int shutdownStart = main.indexOf(QStringLiteral("function shutdownAsyncLoaders()"));
    const int shutdownEnd = main.indexOf(QStringLiteral("function openSstvWorkspace()"), shutdownStart);
    QVERIFY(shutdownStart >= 0);
    QVERIFY(shutdownEnd > shutdownStart);
    const QString shutdown = main.mid(shutdownStart, shutdownEnd - shutdownStart);
    QVERIFY(shutdown.contains(QStringLiteral("shutdownLoader(cwWindowLoader)")));
    QVERIFY(shutdown.contains(QStringLiteral("shutdownLoader(ssbWindowLoader)")));

    // The process exits only after the transient native windows have released
    // their scene-graph resources and their Loader items are actually gone.
    const int closeStart = main.indexOf(QStringLiteral("onClosing: function(close)"));
    const int closeEnd = main.indexOf(QStringLiteral("onXChanged:"), closeStart);
    QVERIFY(closeStart >= 0);
    QVERIFY(closeEnd > closeStart);
    const QString closeHandler = main.mid(closeStart, closeEnd - closeStart);
    QVERIFY(closeHandler.contains(QStringLiteral("shutdownAsyncLoaders()")));
    QVERIFY(closeHandler.contains(QStringLiteral("mainWindow.releaseResources()")));
    QVERIFY(closeHandler.contains(QStringLiteral("applicationQuitTimer.restart()")));
    QVERIFY(!closeHandler.contains(QStringLiteral("Qt.quit()")));

    QVERIFY(main.contains(QStringLiteral("function releaseLoaderGraphics(loader)")));
    QVERIFY(main.contains(QStringLiteral("loader.item.releaseResources()")));
    QVERIFY(main.contains(QStringLiteral("function shutdownTopLevelLoadersReleased()")));
    QVERIFY(main.contains(QStringLiteral("shutdownDrainPasses >= 20")));
    QVERIFY(!main.contains(QStringLiteral("interval: 75")));

    const QString startup = readSource(QStringLiteral("src/app/main_qml.cpp"));
    QVERIFY(!startup.isEmpty());
    QVERIFY(startup.contains(QStringLiteral(
        "&cwModule, &decodium::cw::CwController::suspend")));
    QVERIFY(main.contains(QStringLiteral("cwModule.ensureStarted()")));

    // Closing CW must also stop its invisible decoder/scope workload.
    QVERIFY(bridge.contains(QStringLiteral(
        "const bool cwTap = m_cwInAscolto && !m_transmitting && !m_tuning;")));
}

QTEST_APPLESS_MAIN(TestLiquidGlassWindows)
#include "test_liquid_glass_windows.moc"
