/* Decodium 4.0 - Main Window
 * Based on WSJT-X by K1JT et al.
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
// import Qt.labs.settings 1.1  // non disponibile in questa build Qt
import "components"

ApplicationWindow {
    id: mainWindow
    readonly property int preferredMinimumWidth: 1200
    readonly property int preferredMinimumHeight: 480
    readonly property int currentScreenAvailableWidth: (Screen.desktopAvailableWidth > 0
                                                        ? Screen.desktopAvailableWidth
                                                        : (Screen.width > 0 ? Screen.width : preferredMinimumWidth))
    readonly property int currentScreenAvailableHeight: (Screen.desktopAvailableHeight > 0
                                                         ? Screen.desktopAvailableHeight
                                                         : (Screen.height > 0 ? Screen.height : preferredMinimumHeight))

    minimumWidth: Math.min(preferredMinimumWidth, currentScreenAvailableWidth)
    minimumHeight: Math.min(preferredMinimumHeight, currentScreenAvailableHeight)
    visible: true
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
         | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
    title: "Decodium 4.0 — " + (bridge ? bridge.mode : "") + " — " + (bridge ? bridge.callsign : "")
    property bool windowStateRestoreInProgress: true
    readonly property bool txVisualActive: !!(bridge && (bridge.transmitting || bridge.tuning))
    property bool decodePanelLayoutSaved: false
    property int savedPeriod1PanelWidth: 400
    property int savedRxFreqPanelWidth: 400
    property int savedLiveMapPanelWidth: 360
    property double startupCompletedStartedMs: 0

    function startupElapsedMs() {
        return startupCompletedStartedMs > 0 ? Math.round(Date.now() - startupCompletedStartedMs) : -1
    }

    function startupLog(phase) {
        console.log("Main.qml startup +" + startupElapsedMs() + " ms: " + phase)
    }

    function availableScreenGeometries() {
        var geometries = []
        if (Qt.application && Qt.application.screens) {
            for (var i = 0; i < Qt.application.screens.length; ++i) {
                var screen = Qt.application.screens[i]
                if (!screen) continue
                var geom = normalizedScreenGeometry(screen)
                if (geom) {
                    geometries.push(geom)
                }
            }
        }
        if (geometries.length === 0) {
            geometries.push({
                x: 0,
                y: 0,
                width: safeNumber(Screen.desktopAvailableWidth, safeNumber(Screen.width, 1920)),
                height: safeNumber(Screen.desktopAvailableHeight, safeNumber(Screen.height, 1080))
            })
        }
        return geometries
    }

    function safeNumber(value, fallback) {
        var numeric = Number(value)
        return isFinite(numeric) ? numeric : fallback
    }

    function normalizedScreenGeometry(screen) {
        var g = null
        try { g = screen.availableGeometry } catch(e) { g = null }
        if (!g || !(safeNumber(g.width, 0) > 0) || !(safeNumber(g.height, 0) > 0)) {
            try { g = screen.virtualGeometry } catch(e2) { g = null }
        }
        if (g && safeNumber(g.width, 0) > 0 && safeNumber(g.height, 0) > 0) {
            return {
                x: Math.round(safeNumber(g.x, 0)),
                y: Math.round(safeNumber(g.y, 0)),
                width: Math.round(safeNumber(g.width, preferredMinimumWidth)),
                height: Math.round(safeNumber(g.height, preferredMinimumHeight))
            }
        }

        var widthValue = safeNumber(screen.width, 0)
        var heightValue = safeNumber(screen.height, 0)
        if (widthValue > 0 && heightValue > 0) {
            return {
                x: Math.round(safeNumber(screen.virtualX, 0)),
                y: Math.round(safeNumber(screen.virtualY, 0)),
                width: Math.round(widthValue),
                height: Math.round(heightValue)
            }
        }
        return null
    }

    function safeBridgeSetting(key, fallback) {
        try {
            if (bridge && typeof bridge.getSetting === "function") {
                var value = bridge.getSetting(key, fallback)
                if (value !== undefined && value !== null)
                    return value
            }
        } catch(e) {
            console.log("getSetting error for " + key + ": " + e)
        }
        return fallback
    }

    function safeWindowState(key) {
        try {
            if (bridge && typeof bridge.loadWindowState === "function")
                return bridge.loadWindowState(key) || {}
        } catch(e) {
            console.log("loadWindowState error for " + key + ": " + e)
        }
        return {}
    }

    function clampWindowPosition(savedX, savedY, winW, winH) {
        var screens = availableScreenGeometries()
        var centerX = savedX + winW / 2
        var centerY = savedY + winH / 2

        for (var i = 0; i < screens.length; ++i) {
            var g = screens[i]
            if (centerX >= g.x && centerX < g.x + g.width &&
                centerY >= g.y && centerY < g.y + g.height) {
                return {
                    x: Math.max(g.x, Math.min(savedX, g.x + Math.max(0, g.width - winW))),
                    y: Math.max(g.y, Math.min(savedY, g.y + Math.max(0, g.height - winH)))
                }
            }
        }

        var fallback = screens[0]
        return {
            x: Math.round(fallback.x + Math.max(0, (fallback.width - winW) / 2)),
            y: Math.round(fallback.y + Math.max(0, (fallback.height - winH) / 2))
        }
    }

    function safeStoredPanelWidth(value, fallback, minimum) {
        var numeric = Number(value)
        if (!isFinite(numeric) || numeric <= 0)
            numeric = fallback
        return Math.max(minimum, Math.round(numeric))
    }

    function restoreDecodePanelWidths() {
        if (typeof decodePanelsSplit === "undefined" || !decodePanelsSplit ||
            typeof period1Panel === "undefined" || !period1Panel ||
            typeof rxFreqPanel === "undefined" || !rxFreqPanel ||
            typeof liveMapPanelHost === "undefined" || !liveMapPanelHost) {
            Qt.callLater(restoreDecodePanelWidths)
            return
        }

        var totalWidth = decodePanelsSplit.width
        if (!(totalWidth > 0)) {
            Qt.callLater(restoreDecodePanelWidths)
            return
        }

        liveMapPanelHost.targetPanelWidth = safeStoredPanelWidth(savedLiveMapPanelWidth, 360, 280)

        if (!decodePanelLayoutSaved) {
            period1Panel.userDraggedSplit = false
            period1Panel.applyCenterSplit()
            return
        }

        var period1Min = 360
        var rxMin = 260
        var mapMin = 280
        var savedPeriod1 = safeStoredPanelWidth(savedPeriod1PanelWidth, 400, period1Min)
        var savedRx = safeStoredPanelWidth(savedRxFreqPanelWidth, 400, rxMin)
        var savedMap = safeStoredPanelWidth(savedLiveMapPanelWidth, 360, mapMin)
        var mapWidth = 0

        if (mainWindow.liveMapPanelVisible && !mainWindow.liveMapDetached) {
            var maxMapWidth = Math.max(mapMin, totalWidth - period1Min - rxMin)
            mapWidth = Math.min(savedMap, maxMapWidth)
        }

        var remainingWidth = Math.max(period1Min + rxMin, totalWidth - mapWidth)
        var savedCombined = Math.max(1, savedPeriod1 + savedRx)
        var period1Width = Math.round(remainingWidth * (savedPeriod1 / savedCombined))
        period1Width = Math.max(period1Min, Math.min(period1Width, remainingWidth - rxMin))
        var rxWidth = Math.max(rxMin, remainingWidth - period1Width)

        period1Panel.userDraggedSplit = true
        period1Panel.targetPanelWidth = period1Width
        rxFreqPanel.targetPanelWidth = rxWidth
        if (mainWindow.liveMapPanelVisible && !mainWindow.liveMapDetached)
            liveMapPanelHost.targetPanelWidth = mapWidth
    }

    function persistDecodePanelWidths() {
        if (!bridge)
            return

        if (typeof period1Panel !== "undefined" && period1Panel &&
            !period1Detached && period1Panel.width >= 360) {
            savedPeriod1PanelWidth = Math.round(period1Panel.width)
            bridge.setSetting("uiFullSpectrumPanelWidth", savedPeriod1PanelWidth)
        }

        if (typeof rxFreqPanel !== "undefined" && rxFreqPanel &&
            !rxFreqDetached && rxFreqPanel.width >= 260) {
            savedRxFreqPanelWidth = Math.round(rxFreqPanel.width)
            bridge.setSetting("uiSignalRxPanelWidth", savedRxFreqPanelWidth)
        }

        if (typeof liveMapPanelHost !== "undefined" && liveMapPanelHost) {
            var liveMapWidth = liveMapPanelHost.visible ? liveMapPanelHost.width : liveMapPanelHost.targetPanelWidth
            if (liveMapWidth >= 280) {
                savedLiveMapPanelWidth = Math.round(liveMapWidth)
                liveMapPanelHost.targetPanelWidth = savedLiveMapPanelWidth
                bridge.setSetting("uiLiveMapPanelWidth", savedLiveMapPanelWidth)
            }
        }

        decodePanelLayoutSaved = true
        bridge.setSetting("uiDecodePanelsLayoutSaved", true)
    }

    Component.onCompleted: {
        startupCompletedStartedMs = Date.now()
        startupLog("Component.onCompleted begin")
        callerQueuePanelVisible = !!safeBridgeSetting("uiCallerQueuePanelVisible", !!(bridge && bridge.foxMode))
        startupLog("fox/caller queue state restored")
        decodePanelLayoutSaved = !!safeBridgeSetting("uiDecodePanelsLayoutSaved", false)
        savedPeriod1PanelWidth = safeStoredPanelWidth(safeBridgeSetting("uiFullSpectrumPanelWidth", 400), 400, 360)
        savedRxFreqPanelWidth = safeStoredPanelWidth(safeBridgeSetting("uiSignalRxPanelWidth", 400), 400, 260)
        savedLiveMapPanelWidth = safeStoredPanelWidth(safeBridgeSetting("uiLiveMapPanelWidth", 360), 360, 280)
        startupLog("decode panel settings restored")

        var state = safeWindowState("mainWindow")
        startupLog("main window state read")
        var restoredWidth = safeNumber(state.width, width)
        var restoredHeight = safeNumber(state.height, height)
        if (restoredWidth > 0) width = restoredWidth
        if (restoredHeight > 0) height = restoredHeight
        var pos
        var restoredX = safeNumber(state.x, NaN)
        var restoredY = safeNumber(state.y, NaN)
        if (isFinite(restoredX) && isFinite(restoredY)) {
            pos = clampWindowPosition(restoredX, restoredY, width, height)
        } else {
            var fallback = availableScreenGeometries()[0]
            pos = {
                x: Math.round(fallback.x + Math.max(0, (fallback.width - width) / 2)),
                y: Math.round(fallback.y + Math.max(0, (fallback.height - height) / 2))
            }
        }
        x = pos.x
        y = pos.y
        startupLog("main window geometry applied x=" + x + " y=" + y + " w=" + width + " h=" + height)
        windowStateRestoreInProgress = false

        // Force window visible on Windows — some WM/GPU combos need explicit calls
        visible = true
        show()
        raise()
        requestActivate()
        startupLog("main window show/raise/requestActivate done")
        Qt.callLater(restoreDecodePanelWidths)
        bridge.notifyMainQmlReady()
        startupLog("bridge notified ready")
        console.log("Main.qml window shown at " + x + "," + y + " size " + width + "x" + height)
    }

    // Altezza pannello waterfall — caricata da bridge.uiWaterfallHeight.
    // Default 420px così all'avvio la cascata è già ben aperta (feedback IK8OLM).
    property int  waterfallPanelHeight: bridge.uiWaterfallHeight > 0 ? bridge.uiWaterfallHeight : 420

    // Timer che salva le impostazioni 2s dopo ogni modifica (debounce)
    Timer {
        id: saveTimer
        interval: 2000
        repeat: false
        onTriggered: bridge.saveSettings()
    }
    Timer {
        id: windowStateSaveTimer
        interval: 500
        repeat: false
        onTriggered: persistWindowLayouts()
    }
    Timer {
        id: firstUseWarmupTimer
        interval: 2500
        repeat: false
        running: true
        onTriggered: {
            if (bridge && bridge.warmLogCacheAsync)
                bridge.warmLogCacheAsync()
            if (settingsDialog && settingsDialog.warmUpPopup)
                settingsDialog.warmUpPopup()
        }
    }
    // Funzione helper chiamabile da qualsiasi parte del QML
    function scheduleSave() { saveTimer.restart() }
    function scheduleWindowStateSave() {
        if (!windowStateRestoreInProgress) {
            windowStateSaveTimer.restart()
        }
    }

    function persistSettingsDialogIfOpen() {
        if (settingsDialog && settingsDialog.visible && settingsDialog.persistSettingsNow)
            settingsDialog.persistSettingsNow()
    }

    function restoreFloatingWindowState(windowRef, key, detachedPropName, minimizedPropName) {
        if (!windowRef)
            return
        var state = safeWindowState(key)
        var restoredWidth = safeNumber(state.width, windowRef.width)
        var restoredHeight = safeNumber(state.height, windowRef.height)
        if (restoredWidth > 0) windowRef.width = restoredWidth
        if (restoredHeight > 0) windowRef.height = restoredHeight
        var restoredX = safeNumber(state.x, NaN)
        var restoredY = safeNumber(state.y, NaN)
        if (isFinite(restoredX) && isFinite(restoredY)) {
            var pos = clampWindowPosition(restoredX, restoredY, windowRef.width, windowRef.height)
            windowRef.x = pos.x
            windowRef.y = pos.y
        }

        if (detachedPropName && detachedPropName.length > 0) {
            var detached = state.detached !== undefined ? !!state.detached : mainWindow[detachedPropName]
            var minimized = minimizedPropName && state.minimized !== undefined ? (!!state.minimized && detached) : (minimizedPropName ? mainWindow[minimizedPropName] : false)
            mainWindow[detachedPropName] = detached
            if (minimizedPropName) {
                mainWindow[minimizedPropName] = minimized
            }
            if (detached && !minimized) {
                windowRef.visible = true
            }
        }
    }

    function minimizeFloatingWindow(windowRef, minimizedPropName) {
        if (!windowRef || !minimizedPropName || minimizedPropName.length === 0)
            return
        mainWindow[minimizedPropName] = true
        if (typeof windowRef.showMinimized === "function")
            windowRef.showMinimized()
        else
            windowRef.visibility = Window.Minimized
    }

    function restoreFloatingWindow(windowRef, minimizedPropName) {
        if (!windowRef)
            return
        if (minimizedPropName && minimizedPropName.length > 0)
            mainWindow[minimizedPropName] = false
        windowRef.visibility = Window.Windowed
        windowRef.show()
        windowRef.raise()
        windowRef.requestActivate()
    }

    function persistWindowLayouts() {
        persistDecodePanelWidths()
        bridge.saveWindowState("mainWindow", Math.round(x), Math.round(y), Math.round(width), Math.round(height), false, visibility === Window.Minimized)
        bridge.saveWindowState("waterfallWindow", Math.round(waterfallWindow.x), Math.round(waterfallWindow.y), Math.round(waterfallWindow.width), Math.round(waterfallWindow.height), waterfallDetached, waterfallMinimized)
        bridge.saveWindowState("logFloatingWindow", Math.round(logFloatingWindow.x), Math.round(logFloatingWindow.y), Math.round(logFloatingWindow.width), Math.round(logFloatingWindow.height), logWindowDetached, logWindowMinimized)
        bridge.saveWindowState("astroFloatingWindow", Math.round(astroFloatingWindow.x), Math.round(astroFloatingWindow.y), Math.round(astroFloatingWindow.width), Math.round(astroFloatingWindow.height), astroWindowDetached, astroWindowMinimized)
        bridge.saveWindowState("macroFloatingWindow", Math.round(macroFloatingWindow.x), Math.round(macroFloatingWindow.y), Math.round(macroFloatingWindow.width), Math.round(macroFloatingWindow.height), macroDialogDetached, macroDialogMinimized)
        bridge.saveWindowState("rigFloatingWindow", Math.round(rigFloatingWindow.x), Math.round(rigFloatingWindow.y), Math.round(rigFloatingWindow.width), Math.round(rigFloatingWindow.height), rigControlDetached, rigControlMinimized)
        bridge.saveWindowState("period1FloatingWindow", Math.round(period1FloatingWindow.x), Math.round(period1FloatingWindow.y), Math.round(period1FloatingWindow.width), Math.round(period1FloatingWindow.height), period1Detached, period1Minimized)
        bridge.saveWindowState("period2FloatingWindow", Math.round(period2FloatingWindow.x), Math.round(period2FloatingWindow.y), Math.round(period2FloatingWindow.width), Math.round(period2FloatingWindow.height), period2Detached, period2Minimized)
        bridge.saveWindowState("rxFreqFloatingWindow", Math.round(rxFreqFloatingWindow.x), Math.round(rxFreqFloatingWindow.y), Math.round(rxFreqFloatingWindow.width), Math.round(rxFreqFloatingWindow.height), rxFreqDetached, rxFreqMinimized)
        bridge.saveWindowState("txPanelFloatingWindow", Math.round(txPanelFloatingWindow.x), Math.round(txPanelFloatingWindow.y), Math.round(txPanelFloatingWindow.width), Math.round(txPanelFloatingWindow.height), txPanelDetached, txPanelMinimized)
        bridge.saveWindowState("liveMapFloatingWindow", Math.round(liveMapFloatingWindow.x), Math.round(liveMapFloatingWindow.y), Math.round(liveMapFloatingWindow.width), Math.round(liveMapFloatingWindow.height), liveMapDetached, false)
        bridge.saveWindowState("decoSyncMonitorWindow", Math.round(decoSyncMonitorWindow.x), Math.round(decoSyncMonitorWindow.y), Math.round(decoSyncMonitorWindow.width), Math.round(decoSyncMonitorWindow.height), false, decoSyncMonitorWindow.visibility === Window.Minimized)
    }

    // Salva impostazioni bridge al close
    onClosing: function(close) {
        saveTimer.stop()
        windowStateSaveTimer.stop()
        applicationClosing = true
        persistSettingsDialogIfOpen()
        persistWindowLayouts()
        bridge.saveSettings()
        console.log("Main window closing - shutting down application")
        // Close all floating windows
        if (waterfallWindow) waterfallWindow.close()
        if (logWindow) logWindow.close()
        closeLoaded(astroWindowLoader)
        closeLoaded(macroDialogLoader)
        if (txPanelFloatingWindow) txPanelFloatingWindow.close()
        if (period1FloatingWindow) period1FloatingWindow.close()
        if (period2FloatingWindow) period2FloatingWindow.close()
        if (rxFreqFloatingWindow) rxFreqFloatingWindow.close()
        if (liveMapFloatingWindow) liveMapFloatingWindow.close()
        // Stop monitoring and cleanup
        bridge.stopMonitor()
        bridge.shutdown()
        // Quit application
        Qt.quit()
    }

    onXChanged: scheduleWindowStateSave()
    onYChanged: scheduleWindowStateSave()
    onWidthChanged: scheduleWindowStateSave()
    onHeightChanged: scheduleWindowStateSave()

    component FloatingResizeHandles: Item {
        id: resizeRoot
        property var targetWindow
        property int edgeSize: 7
        property int cornerSize: 16
        property int maxWidth: 10000
        property int maxHeight: 6000

        anchors.fill: parent
        enabled: !!targetWindow
        z: 1000

        function boundedWidth(value) {
            if (!targetWindow)
                return 0
            return Math.max(targetWindow.minimumWidth,
                            Math.min(value, maxWidth))
        }

        function boundedHeight(value) {
            if (!targetWindow)
                return 0
            return Math.max(targetWindow.minimumHeight,
                            Math.min(value, maxHeight))
        }

        function resizeRight(delta) {
            targetWindow.width = boundedWidth(targetWindow.width + delta)
        }

        function resizeBottom(delta) {
            targetWindow.height = boundedHeight(targetWindow.height + delta)
        }

        function resizeLeft(delta) {
            var oldWidth = targetWindow.width
            var newWidth = boundedWidth(oldWidth - delta)
            var applied = oldWidth - newWidth
            targetWindow.x += applied
            targetWindow.width = newWidth
        }

        function resizeTop(delta) {
            var oldHeight = targetWindow.height
            var newHeight = boundedHeight(oldHeight - delta)
            var applied = oldHeight - newHeight
            targetWindow.y += applied
            targetWindow.height = newHeight
        }

        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: resizeRoot.cornerSize
            anchors.bottomMargin: resizeRoot.cornerSize
            width: resizeRoot.edgeSize
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.resizeRight(mouse.x - width / 2)
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: resizeRoot.cornerSize
            anchors.bottomMargin: resizeRoot.cornerSize
            width: resizeRoot.edgeSize
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.resizeLeft(mouse.x)
            }
        }

        MouseArea {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: resizeRoot.cornerSize
            anchors.rightMargin: resizeRoot.cornerSize
            height: resizeRoot.edgeSize
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.resizeBottom(mouse.y - height / 2)
            }
        }

        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: resizeRoot.cornerSize
            anchors.rightMargin: resizeRoot.cornerSize
            height: resizeRoot.edgeSize
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.resizeTop(mouse.y)
            }
        }

        MouseArea {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed) {
                    resizeRoot.resizeRight(mouse.x - width / 2)
                    resizeRoot.resizeBottom(mouse.y - height / 2)
                }
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed) {
                    resizeRoot.resizeLeft(mouse.x)
                    resizeRoot.resizeBottom(mouse.y - height / 2)
                }
            }
        }

        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed) {
                    resizeRoot.resizeRight(mouse.x - width / 2)
                    resizeRoot.resizeTop(mouse.y)
                }
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            onPositionChanged: function(mouse) {
                if (pressed) {
                    resizeRoot.resizeLeft(mouse.x)
                    resizeRoot.resizeTop(mouse.y)
                }
            }
        }
    }

    // Decodium: Keyboard shortcut handler
    Item {
        id: keyboardHandler
        focus: true
        anchors.fill: parent

        Keys.onPressed: function(event) {
            // Keyboard shortcuts handled by bridge
        }
    }

    // Waterfall detached state
    property bool waterfallDetached: false
    property bool waterfallMinimized: false

    // 1.0.180 — Dock state machine unified: invece di booleano flat
    // (waterfallDetached), abbiamo uno stato esplicito che permette
    // animazioni di transizione future.
    property string waterfallDockState: waterfallDetached ? "detached" : "embedded"
    // Valori: "embedded", "detaching", "detached", "embedding"

    // Window detached and minimized states
    property bool logWindowDetached: false
    property bool logWindowMinimized: false
    property bool astroWindowDetached: false
    property bool astroWindowMinimized: false
    property bool macroDialogDetached: false
    property bool macroDialogMinimized: false
    property bool rigControlDetached: false
    property bool rigControlMinimized: false

    // Decode panels detached and minimized states
    property bool period1Detached: false
    property bool period1Minimized: false
    property bool period1DockHighlighted: false
    // 1.0.229 — Compact mode Full Spectrum: row height ridotta per
    // raddoppiare le righe visibili nella stessa viewport quando i
    // decode sono tanti (es. FT8 burst 20+ per slot). Opt-in default OFF.
    property bool compactFullSpectrum: bridge ? !!bridge.getSetting("CompactFullSpectrum", false) : false
    property int fullSpectrumRowHeight: compactFullSpectrum ? 14 : 26
    function toggleCompactFullSpectrum() {
        compactFullSpectrum = !compactFullSpectrum
        if (bridge)
            bridge.setSetting("CompactFullSpectrum", compactFullSpectrum)
    }
    property bool period2Detached: false
    property bool period2Minimized: false
    property bool period2DockHighlighted: false
    property bool rxFreqDetached: false
    property bool rxFreqMinimized: false
    property bool rxFreqDockHighlighted: false

    // TxPanel detached and minimized states
    property bool txPanelDetached: false
    property bool txPanelMinimized: false
    property bool liveMapDetached: false
    property bool liveMapMinimized: false
    property bool applicationClosing: false
    onWaterfallDetachedChanged: scheduleWindowStateSave()
    onWaterfallMinimizedChanged: scheduleWindowStateSave()
    onLogWindowDetachedChanged: scheduleWindowStateSave()
    onLogWindowMinimizedChanged: scheduleWindowStateSave()
    onAstroWindowDetachedChanged: scheduleWindowStateSave()
    onAstroWindowMinimizedChanged: scheduleWindowStateSave()
    onMacroDialogDetachedChanged: scheduleWindowStateSave()
    onMacroDialogMinimizedChanged: scheduleWindowStateSave()
    onRigControlDetachedChanged: scheduleWindowStateSave()
    onRigControlMinimizedChanged: scheduleWindowStateSave()
    onPeriod1DetachedChanged: scheduleWindowStateSave()
    onPeriod1MinimizedChanged: scheduleWindowStateSave()
    onPeriod2DetachedChanged: scheduleWindowStateSave()
    onPeriod2MinimizedChanged: scheduleWindowStateSave()
    onRxFreqDetachedChanged: scheduleWindowStateSave()
    onRxFreqMinimizedChanged: scheduleWindowStateSave()
    onTxPanelDetachedChanged: scheduleWindowStateSave()
    onTxPanelMinimizedChanged: scheduleWindowStateSave()
    onLiveMapDetachedChanged: {
        scheduleWindowStateSave()
        Qt.callLater(function() {
            mainWindow.syncLiveMapFloatingVisibility(false)
            mainWindow.restoreDecodePanelWidths()
        })
    }
    onLiveMapMinimizedChanged: scheduleWindowStateSave()

    // === GAP 3 — Nuovi pannelli (A3, B9, A4, C14) ===
    property bool timeSyncPanelVisible:       !!bridge.getSetting("uiTimeSyncPanelVisible", false)
    property bool activeStationsPanelVisible: !!bridge.getSetting("uiActiveStationsPanelVisible", false)
    property bool callerQueuePanelVisible:    !!bridge.getSetting("uiCallerQueuePanelVisible", false)
    property bool astroPanelVisible:          !!bridge.getSetting("uiAstroPanelVisible", false)
    property bool dxClusterPanelVisible:      !!bridge.getSetting("uiDxClusterPanelVisible", false)
    property bool dxClusterToolbarVisible:    !!bridge.getSetting("uiDxClusterToolbarVisible", true)
    property bool pskReporterToolbarVisible: !!bridge.getSetting("uiPskReporterToolbarVisible", true)
    property bool asyncIconVisible:          !!bridge.getSetting("uiAsyncIconVisible", true)
    property bool liveMapPanelVisible:        bridge.getSetting("WorldMapDisplayed", true)
    property bool decoSyncMonitorVisible:     !!bridge.getSetting("uiDecoSyncMonitorVisible", false)
    property string uiLanguage: normalizeUiLanguage(String(bridge.getSetting("UILanguage", "en") || "en"))
    readonly property var uiLanguageOptions: [
        { code: "en", name: "English" },
        { code: "ca", name: "Català" },
        { code: "da", name: "Dansk" },
        { code: "de", name: "Deutsch" },
        { code: "es", name: "Español" },
        { code: "fr", name: "Français" },
        { code: "hu", name: "Magyar" },
        { code: "it", name: "Italiano" },
        { code: "ja", name: "日本語" },
        { code: "ru", name: "Русский" },
        { code: "zh", name: "简体中文" },
        { code: "zh_TW", name: "繁體中文" }
    ]
    function normalizeUiLanguage(code) {
        code = String(code || "en").replace("-", "_")
        return code.indexOf("en_") === 0 ? "en" : code
    }
    function uiLanguageName(code) {
        code = normalizeUiLanguage(code)
        for (var i = 0; i < uiLanguageOptions.length; ++i) {
            if (uiLanguageOptions[i].code === code)
                return uiLanguageOptions[i].name
        }
        return "English"
    }
    function uiLanguageLabel(code) {
        switch (normalizeUiLanguage(code)) {
        case "ca": return "Llengua"
        case "da": return "Sprog"
        case "de": return "Sprache"
        case "es": return "Idioma"
        case "fr": return "Langue"
        case "hu": return "Nyelv"
        case "it": return "Lingua"
        case "ja": return "言語"
        case "ru": return "Язык"
        case "zh": return "语言"
        case "zh_TW": return "語言"
        default: return "Language"
        }
    }
    function setUiLanguage(code) {
        code = normalizeUiLanguage(code)
        if (!code || code === uiLanguage)
            return
        uiLanguage = code
        bridge.setSetting("UILanguage", code)
        showStatusToast("Language set to " + uiLanguageName(code) + ". Restart Decodium to apply it.", accentOrange)
    }
    function setDxClusterToolbarVisible(visible) {
        dxClusterToolbarVisible = visible
        bridge.setSetting("uiDxClusterToolbarVisible", visible)
        if (!visible)
            dxClusterPanelVisible = false
    }
    function setPskReporterToolbarVisible(visible) {
        pskReporterToolbarVisible = visible
        bridge.setSetting("uiPskReporterToolbarVisible", visible)
        if (!visible && typeof pskSearchPopup !== "undefined")
            pskSearchPopup.close()
    }
    function setAsyncIconVisible(visible) {
        asyncIconVisible = visible
        bridge.setSetting("uiAsyncIconVisible", visible)
    }
    onTimeSyncPanelVisibleChanged: bridge.setSetting("uiTimeSyncPanelVisible", timeSyncPanelVisible)
    onActiveStationsPanelVisibleChanged: bridge.setSetting("uiActiveStationsPanelVisible", activeStationsPanelVisible)
    onCallerQueuePanelVisibleChanged: bridge.setSetting("uiCallerQueuePanelVisible", callerQueuePanelVisible)
    onAstroPanelVisibleChanged: bridge.setSetting("uiAstroPanelVisible", astroPanelVisible)
    onDxClusterPanelVisibleChanged: bridge.setSetting("uiDxClusterPanelVisible", dxClusterPanelVisible)
    onDecoSyncMonitorVisibleChanged: bridge.setSetting("uiDecoSyncMonitorVisible", decoSyncMonitorVisible)
    function syncLiveMapFloatingVisibility(activate) {
        if (typeof liveMapFloatingWindow === "undefined" || !liveMapFloatingWindow)
            return

        if (mainWindow.liveMapPanelVisible && mainWindow.liveMapDetached && !mainWindow.liveMapMinimized) {
            liveMapFloatingWindow.show()
            if (activate) {
                liveMapFloatingWindow.raise()
                liveMapFloatingWindow.requestActivate()
            }
        } else {
            liveMapFloatingWindow.hide()
        }
    }
	    function detachWaterfallPanel() {
	        mainWindow.waterfallDetached = true
	        mainWindow.waterfallMinimized = false
	        waterfallPanel.isDockHighlighted = false
	        waterfallWindow.show()
	        waterfallWindow.raise()
	        waterfallWindow.requestActivate()
	    }
	    function dockWaterfallPanel() {
	        waterfallPanel.isDockHighlighted = false
	        mainWindow.waterfallDetached = false
	        mainWindow.waterfallMinimized = false
	        waterfallWindow.hide()
	    }
	    function detachLiveMapPanel() {
	        mainWindow.liveMapPanelVisible = true
	        bridge.setSetting("WorldMapDisplayed", true)
	        mainWindow.liveMapDetached = true
        mainWindow.liveMapMinimized = false
        mainWindow.syncLiveMapFloatingVisibility(true)
        Qt.callLater(mainWindow.restoreDecodePanelWidths)
    }
    function dockLiveMapPanel() {
        mainWindow.liveMapDetached = false
        mainWindow.liveMapMinimized = false
	        mainWindow.syncLiveMapFloatingVisibility(false)
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
	    }
	    function detachFullSpectrumPanel() {
	        mainWindow.period1Detached = true
	        mainWindow.period1Minimized = false
	        period1FloatingWindow.show()
	        period1FloatingWindow.visibility = Window.Windowed
	        period1FloatingWindow.raise()
	        period1FloatingWindow.requestActivate()
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
	    }
	    function dockFullSpectrumPanel() {
	        mainWindow.period1DockHighlighted = false
	        mainWindow.period1Detached = false
	        mainWindow.period1Minimized = false
	        period1FloatingWindow.hide()
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
	    }
	    function detachSignalRxPanel() {
	        mainWindow.rxFreqDetached = true
	        mainWindow.rxFreqMinimized = false
	        rxFreqFloatingWindow.show()
	        rxFreqFloatingWindow.visibility = Window.Windowed
	        rxFreqFloatingWindow.raise()
	        rxFreqFloatingWindow.requestActivate()
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
	    }
	    function dockSignalRxPanel() {
	        mainWindow.rxFreqDockHighlighted = false
	        mainWindow.rxFreqDetached = false
	        mainWindow.rxFreqMinimized = false
	        rxFreqFloatingWindow.hide()
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
	    }
    onLiveMapPanelVisibleChanged: {
        bridge.setSetting("WorldMapDisplayed", liveMapPanelVisible)
        Qt.callLater(function() {
            mainWindow.syncLiveMapFloatingVisibility(false)
            if (mainWindow.decodePanelLayoutSaved)
                mainWindow.restoreDecodePanelWidths()
            else if (typeof period1Panel !== "undefined" && period1Panel)
                period1Panel.applyCenterSplit()
        })
    }

    // === Dialoghi ===
    Loader { id: colorDialogLoader; source: "../dialogs/ColorHighlightingDialog.qml"; active: false }
    Loader { id: qsyDialogLoader;   source: "../dialogs/QSYDialog.qml";              active: false }

    function openColorDialog() {
        colorDialogLoader.active = true
        colorDialogLoader.item.open()
    }
    function openQsyDialog() {
        qsyDialogLoader.active = true
        qsyDialogLoader.item.open()
    }

    function runWhenLoaded(loader, action) {
        if (loader.item) {
            action(loader.item)
            return
        }
        loader.pendingAction = action
        loader.active = true
    }

    function closeLoaded(loader) {
        if (loader.item && loader.item.close)
            loader.item.close()
    }

    function openLogWindow() { logWindow.open() }
    function openMacroDialog() { runWhenLoaded(macroDialogLoader, function(item) { item.open() }) }
    function openAstroWindow() { runWhenLoaded(astroWindowLoader, function(item) { item.open() }) }
    function openSettingsDialog() { settingsDialog.open() }
    function openSettingsTab(tabIndex) {
        settingsDialog.openTab(tabIndex)
    }

    property string rigErrorDialogTitle: ""
    property string rigErrorSummary: ""
    property string rigErrorDetails: ""
    property bool rigErrorDetailsVisible: false
    property string warningDialogTitle: ""
    property string warningDialogSummary: ""
    property string warningDialogDetails: ""
    property bool warningDialogDetailsVisible: false

    // WAV file open dialog - single file
    FileDialog {
        id: wavOpenDialog
        title: "Open WAV file for decoding"
        nameFilters: ["File WAV (*.wav)"]
        onAccepted: {
            var path = selectedFile.toString()
            if (Qt.platform.os === "windows") {
                path = path.replace("file:///", "")
            } else {
                path = path.replace("file://", "")
            }
            console.log("Opening WAV for decode: " + path)
            bridge.openWavForDecode(path)
        }
    }

    // WAV folder dialog - batch decode all WAVs in folder
    FolderDialog {
        id: wavFolderDialog
        title: "Select folder with WAV files"
        onAccepted: {
            var path = selectedFolder.toString()
            if (Qt.platform.os === "windows") {
                path = path.replace("file:///", "")
            } else {
                path = path.replace("file://", "")
            }
            console.log("Batch decode folder: " + path)
            bridge.openWavFolderDecode(path)
        }
    }
    property bool txPanelDockHighlighted: false

    // Dynamic theme colors from ThemeManager
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color bgLight: bridge.themeManager.bgLight
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color accentOrange: bridge.themeManager.warningColor
    property color warningOrange: accentOrange
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color successGreen: bridge.themeManager.successColor
    property color glassOverlay: bridge.themeManager.glassOverlay
    property color glassBorder: bridge.themeManager.glassBorder
    property bool showDxccInfo: bridge.getSetting("ShowDXCC", true)
    property bool showTxMessagesInRx: bridge.getSetting("TXMessagesToRX", true)
    property bool highlight73: bridge.getSetting("Highlight73", true)
    property bool highlightOrange: bridge.getSetting("HighlightOrange", false)
    property bool highlightBlue: bridge.getSetting("HighlightBlue", false)
    property string highlightOrangeCallsigns: bridge.getSetting("HighlightOrangeCallsigns", "")
    property string highlightBlueCallsigns: bridge.getSetting("HighlightBlueCallsigns", "")
    property string decodedTextFontFamily: bridge.fontSettingFamily("DecodedTextFont", "Courier", 10)
    property int decodedTextFontPixelSize: bridge.fontSettingPixelSize("DecodedTextFont", "Courier", 10)
    property int decodedTextHeaderPixelSize: Math.max(8, decodedTextFontPixelSize - 1)
    property int decodeColorRevision: 0

    function refreshDecodedTextFont() {
        decodedTextFontFamily = bridge.fontSettingFamily("DecodedTextFont", "Courier", 10)
        decodedTextFontPixelSize = bridge.fontSettingPixelSize("DecodedTextFont", "Courier", 10)
    }

    function refreshDecodeColors() {
        decodeColorRevision = (decodeColorRevision + 1) % 1000000
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            if (key === "ShowDXCC" || key === "DXCCEntity")
                mainWindow.showDxccInfo = !!value
            else if (key === "TXMessagesToRX" || key === "Tx2QSO")
                mainWindow.showTxMessagesInRx = !!value
            else if (key === "Highlight73")
                mainWindow.highlight73 = !!value
            else if (key === "HighlightOrange")
                mainWindow.highlightOrange = !!value
            else if (key === "HighlightBlue")
                mainWindow.highlightBlue = !!value
            else if (key === "HighlightOrangeCallsigns" || key === "OrangeCallsigns")
                mainWindow.highlightOrangeCallsigns = String(value || "")
            else if (key === "HighlightBlueCallsigns" || key === "BlueCallsigns")
                mainWindow.highlightBlueCallsigns = String(value || "")
            else if (key === "DecodedTextFont")
                mainWindow.refreshDecodedTextFont()
            else if (key === "WorldMapDisplayed")
                mainWindow.liveMapPanelVisible = !!value
            else if (key === "uiDxClusterToolbarVisible")
                mainWindow.dxClusterToolbarVisible = !!value
            else if (key === "uiPskReporterToolbarVisible")
                mainWindow.pskReporterToolbarVisible = !!value
            else if (key === "UILanguage")
                mainWindow.uiLanguage = mainWindow.normalizeUiLanguage(String(value || "en"))
            else if (key === "uiDecodePanelsLayoutSaved")
                mainWindow.decodePanelLayoutSaved = !!value
        }
        function onColorCQChanged() { mainWindow.refreshDecodeColors() }
        function onColorMyCallChanged() { mainWindow.refreshDecodeColors() }
        function onColorDXEntityChanged() { mainWindow.refreshDecodeColors() }
        function onColor73Changed() { mainWindow.refreshDecodeColors() }
        function onColorB4Changed() { mainWindow.refreshDecodeColors() }
        function onColorTxMessageChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewDxccChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewDxccBandChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewContinentChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewContinentBandChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewCqZoneChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewCqZoneBandChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewItuZoneChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewItuZoneBandChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewGridChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewGridBandChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewCallChanged() { mainWindow.refreshDecodeColors() }
        function onColorNewCallBandChanged() { mainWindow.refreshDecodeColors() }
        function onColorLotwUserChanged() { mainWindow.refreshDecodeColors() }
    }

    // IU8LMC: DXCC color scheme (JTDX-style)
    readonly property color colorWorked: "#808080"       // Gray - already worked
    readonly property color colorNewBand: bridge.themeManager.ledYellow      // Gold - new on this band
    readonly property color colorNewCountry: "#00FF00"   // Bright green - new country
    readonly property color colorMostWanted: bridge.themeManager.ledMagenta   // Magenta - most wanted

    // IU8LMC: Tooltip properties
    property string dxccTooltipText: ""
    property bool dxccTooltipVisible: false
    property real dxccTooltipX: 0
    property real dxccTooltipY: 0

    function isSignoffMessage(message) {
        var words = String(message || "").toUpperCase().replace(/[<>;,]/g, " ").split(/\s+/)
        for (var i = 0; i < words.length; ++i) {
            if (words[i] === "73" || words[i] === "RR73" || words[i] === "RRR")
                return true
        }
        return false
    }

    function highlightListMatches(message, listText) {
        var wanted = String(listText || "").toUpperCase().split(/[,\s;]+/)
        var messageText = " " + String(message || "").toUpperCase().replace(/[<>;,]/g, " ") + " "
        for (var i = 0; i < wanted.length; ++i) {
            var token = wanted[i].trim()
            if (token.length > 0 && messageText.indexOf(" " + token + " ") !== -1)
                return true
        }
        return false
    }

	    function customHighlightColor(modelData) {
	        if (!modelData)
	            return ""
	        var message = modelData.message || ""
	        if (highlightOrange && highlightListMatches(message, highlightOrangeCallsigns))
	            return "#E14B00"
        if (highlightBlue && highlightListMatches(message, highlightBlueCallsigns))
            return "#0064FF"
        return ""
    }

	    // Shannon-compatible color function (allineato a DecodeWindow.qml)
	    function getDxccColor(modelData) {
	        if (!modelData)
	            return textPrimary
	        var customColor = customHighlightColor(modelData)
	        if (modelData.isTx)     return bridge.themeManager.warningColor
        if (modelData.isMyCall) return bridge.colorMyCall
        if (customColor !== "") return customColor
        if (highlight73 && isSignoffMessage(modelData.message)) return bridge.color73
        if (modelData.isB4 === true || modelData.dxIsWorked === true) return bridge.colorB4
        if (modelData.isLotw === true) return "#44BBFF"
        if ((modelData.dxCountry && String(modelData.dxCountry).length > 0)
            || modelData.dxIsMostWanted === true || modelData.dxIsNewCountry === true || modelData.dxIsNewBand === true)
            return bridge.colorDXEntity
        if (modelData.isCQ) return bridge.colorCQ
        return textPrimary
    }

    function decodeHighlightHex(modelData) {
        mainWindow.decodeColorRevision
        var hex = bridge.decodeHighlightBg(modelData)
        if (!hex || hex.length === 0)
            return ""
        return hex
    }

    function decodeRowHighlightHex(modelData) {
        var hex = decodeHighlightHex(modelData)
        if (hex.length === 0 || !modelData)
            return ""

        // Broad WSJT-X "new ..." classes can match nearly every decode when
        // the worked history is sparse. Keep them as text colors so they do
        // not wash the entire table with one background color.
        if (modelData.isTx === true || modelData.isMyCall === true)
            return hex
        return ""
    }

    function decodePassiveHighlightTextColor(modelData) {
        var hex = decodeHighlightHex(modelData)
        if (hex.length === 0 || !modelData)
            return ""
        if (modelData.isTx === true || modelData.isMyCall === true)
            return ""
        return hex
    }

    function readableTextOnHighlight(hex) {
        var c = Qt.color(hex)
        var luminance = (0.299 * c.r) + (0.587 * c.g) + (0.114 * c.b)
        return luminance > 0.55 ? "#000000" : "#FFFFFF"
    }

    function decodeHighlightFill(modelData) {
        var hex = decodeRowHighlightHex(modelData)
        if (hex.length === 0)
            return null
        var c = Qt.color(hex)
        return Qt.rgba(c.r, c.g, c.b, 0.35)
    }

    function decodeHighlightBorder(modelData) {
        var hex = decodeRowHighlightHex(modelData)
        if (hex.length === 0)
            return null
        var c = Qt.color(hex)
        return Qt.rgba(c.r, c.g, c.b, 0.85)
    }

    function fullSpectrumTextColor(modelData) {
        var rowHex = decodeRowHighlightHex(modelData)
        if (rowHex.length > 0)
            return readableTextOnHighlight(rowHex)

        var customColor = customHighlightColor(modelData)
        if (customColor !== "")
            return customColor
        if (highlight73 && isSignoffMessage(modelData.message))
            return bridge.color73
        if (modelData.isB4 === true || modelData.dxIsWorked === true)
            return bridge.colorB4

        var textHex = decodePassiveHighlightTextColor(modelData)
        if (textHex.length > 0)
            return textHex

        return getDxccColor(modelData)
    }

    function formatBearingDegrees(value) {
        return value !== undefined && value >= 0 ? Math.round(value) + "°" : ""
    }

    function normalizedCallToken(token) {
        return String(token || "").toUpperCase().replace(/[<>;,]/g, "").trim()
    }

    function isTelemetryHexToken(token) {
        var text = normalizedCallToken(token)
        return text.length >= 7
            && text.length <= 18
            && /[A-F]/.test(text)
            && /[0-9]/.test(text)
            && /^[0-9A-F]+$/.test(text)
    }

    function isTelemetryOnlyDecodeMessage(message) {
        var parts = String(message || "").split(/\s+/).filter(function(part) {
            return String(part || "").trim().length > 0
        })
        return parts.length === 1 && isTelemetryHexToken(parts[0])
    }

    function looksLikeCallsignTokenValue(token) {
        var text = normalizedCallToken(token)
        if (text.length === 0)
            return false
        if (text === "CQ" || text === "DX" || text === "QRZ" || text === "DE" || text === "TEST")
            return false
        if (isTelemetryHexToken(text))
            return false
        var hasLetter = /[A-Z]/.test(text)
        var hasDigit = /[0-9]/.test(text)
        return hasLetter && hasDigit && /^[A-Z0-9/-]+$/.test(text)
    }

    function callsignBase(call) {
        var text = normalizedCallToken(call)
        if (text.length === 0)
            return ""
        var parts = text.split("/")
        var best = ""
        for (var i = 0; i < parts.length; ++i) {
            var part = normalizedCallToken(parts[i])
            if (looksLikeCallsignTokenValue(part) && part.length > best.length)
                best = part
        }
        return best.length > 0 ? best : text
    }

    function isOwnCallValue(call) {
        var myCall = normalizedCallToken((bridge && bridge.callsign) || "")
        var value = normalizedCallToken(call)
        if (myCall.length === 0 || value.length === 0)
            return false
        return value === myCall || callsignBase(value) === callsignBase(myCall)
    }

    function firstMessageCallsign(message) {
        var parts = String(message || "").split(/\s+/)
        for (var i = 0; i < parts.length; ++i) {
            var token = normalizedCallToken(parts[i])
            if (looksLikeCallsignTokenValue(token))
                return token
        }
        return ""
    }

    function isLocalDistanceEntry(modelData) {
        if (!modelData)
            return true
        if (modelData.isTx === true)
            return true
        if (String(modelData.db || "").trim().toUpperCase() === "TX")
            return true
        if (isOwnCallValue(modelData.fromCall))
            return true
        return isOwnCallValue(firstMessageCallsign(modelData.message))
    }

    function distanceText(modelData) {
        if (isLocalDistanceEntry(modelData))
            return ""
        if (modelData && modelData.dxDistance !== undefined && modelData.dxDistance > 0)
            return Math.round(modelData.dxDistance) + "km"
        return ""
    }

    // IU8LMC: Function to build tooltip text
    function getDxccTooltipText(modelData) {
        if (!modelData.dxCountry) return ""
        var lines = []
        var header = modelData.dxCallsign + " - " + modelData.dxCountry
        if (modelData.dxContinent) header += " (" + modelData.dxContinent + ")"
        lines.push(header)
        // Bearing and distance to DX station
        if (!isLocalDistanceEntry(modelData) && modelData.dxBearing !== undefined && modelData.dxBearing >= 0) {
            var bearingDist = "Az: " + Math.round(modelData.dxBearing) + "°"
            if (modelData.dxDistance !== undefined && modelData.dxDistance > 0) {
                bearingDist += "  Dist: " + Math.round(modelData.dxDistance) + " km"
            }
            lines.push(bearingDist)
        }
        if (modelData.dxPrefix) lines.push("Prefix: " + modelData.dxPrefix)
        if (modelData.dxIsMostWanted && !modelData.dxIsWorked) {
            lines.push("MOST WANTED - NEW!")
        } else if (modelData.dxIsNewCountry) {
            lines.push("NEW COUNTRY!")
        } else if (modelData.dxIsNewBand) {
            lines.push("Worked - NEW BAND!")
        } else if (modelData.dxIsWorked) {
            lines.push("Worked")
        }
        return lines.join("\n")
    }

    // IU8LMC: Show QSO progress badge overlay
    function showBadge(text, subText, color, icon) {
        badgeText = text
        badgeSubText = subText
        badgeColor = color
        badgeIcon = icon
        badgeVisible = true
        badgeHideTimer.restart()
    }

    function shouldShowStatusToast(message) {
        var lower = String(message || "").toLowerCase()
        return lower.indexOf("cty.dat") >= 0
            || (lower.indexOf("qso ") === 0
                && (lower.indexOf("-> udp") >= 0
                    || lower.indexOf("-> n1mm") >= 0
                    || lower.indexOf("-> easylog") >= 0))
    }

    function showStatusToast(message, color) {
        if (!message || message.length === 0)
            return
        statusToastText = message
        statusToastColor = color ? color : secondaryCyan
        statusToastVisible = true
        statusToastHideTimer.restart()
    }

    function messageElideMode(message) {
        var myCall = String((bridge && bridge.callsign) || "").trim().toUpperCase()
        if (!myCall.length)
            return Text.ElideRight

        var normalized = " " + String(message || "").toUpperCase().replace(/[<>;,]/g, " ") + " "
        return normalized.indexOf(" " + myCall + " ") >= 0
            ? Text.ElideMiddle
            : Text.ElideRight
    }

    // Dock zones positions
    property rect waterfallDockZone: Qt.rect(8, 64, 450, contentArea.height - 108)

    // Magnetic snap threshold
    readonly property int snapThreshold: 80

    Material.theme: bridge.themeManager.isLightTheme ? Material.Light : Material.Dark
    Material.accent: bridge.themeManager.primaryColor
    Material.primary: bridge.themeManager.secondaryColor
    Material.foreground: bridge.themeManager.textPrimary
    Material.background: bridge.themeManager.bgDeep
    color: bridge.themeManager.bgDeep

    // Font scale from settings (0.7 - 1.5)
    property double fs: bridge.fontScale || 1.0

    // Splash screen state
    property bool splashVisible: true

    // QSO Progress Badge
    property int previousQsoProgress: 0
    property string badgeText: ""
    property string badgeSubText: ""
    property color badgeColor: secondaryCyan
    property string badgeIcon: ""
    property bool badgeVisible: false
    property string statusToastText: ""
    property color statusToastColor: secondaryCyan
    property bool statusToastVisible: false

    // Main background gradient (flat in light themes per design mockup)
    background: Rectangle {
        color: bgDeep
        gradient: Gradient {
            GradientStop { position: 0.0; color: bgDeep }
            GradientStop { position: 0.5; color: bridge.themeManager.isLightTheme ? bgDeep : bgMedium }
            GradientStop { position: 1.0; color: bgDeep }
        }
    }

    // 1.0.182 — UI Visual Boost: gradient sottile theme-aware sul background
    // ApplicationWindow root. Gated su bridge.uiQuality !== "Low" per
    // risparmiare GPU su PC modesti. Effetto "vetro" coerente con i Glass
    // panel esistenti. Usa palette themeManager.
    Rectangle {
        id: appBackgroundGradient
        anchors.fill: parent
        z: -100  // sotto a TUTTO
        visible: bridge && bridge.uiQuality !== "Low"
        gradient: Gradient {
            GradientStop { position: 0.0; color: (bridge && bridge.themeManager && bridge.themeManager.isLightTheme) ? Qt.lighter(bgDeep, 1.08) : Qt.darker(bgDeep, 1.20) }
            GradientStop { position: 0.5; color: bgDeep }
            GradientStop { position: 1.0; color: (bridge && bridge.themeManager && bridge.themeManager.isLightTheme) ? Qt.darker(bgDeep, 1.05) : Qt.darker(bgDeep, 1.10) }
        }
    }

    // ========== SPLASH SCREEN ==========
    Rectangle {
        id: splashOverlay
        anchors.fill: parent
        z: 9999
        visible: splashVisible
        color: bgDeep

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.darker(bgDeep, 1.3) }
            GradientStop { position: 0.4; color: bgDeep }
            GradientStop { position: 0.6; color: bgDeep }
            GradientStop { position: 1.0; color: Qt.darker(bgDeep, 1.3) }
        }

        Column {
            anchors.centerIn: parent
            spacing: 16

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "DECODIUM"
                font.pixelSize: 48
                font.bold: true
                font.letterSpacing: 8
                color: secondaryCyan

                SequentialAnimation on opacity {
                    running: splashVisible
                    loops: 1
                    NumberAnimation { from: 0; to: 1; duration: 800; easing.type: Easing.OutQuad }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "v" + bridge.version()
                font.pixelSize: 16
                font.letterSpacing: 2
                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
            }

            Item { width: 1; height: 12 }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 300
                height: 1
                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
            }

            Item { width: 1; height: 8 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Based on WSJT-X by K1JT et al."
                font.pixelSize: 14
                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "QML Interface by IU8LMC"
                font.pixelSize: 13
                color: accentGreen
            }

            Item { width: 1; height: 20 }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 3
                radius: 1.5
                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)

                Rectangle {
                    id: splashProgress
                    width: 0
                    height: parent.height
                    radius: 1.5
                    color: secondaryCyan

                    NumberAnimation on width {
                        running: splashVisible
                        from: 0; to: 200
                        duration: 2800
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        // Fade out after 3 seconds
        NumberAnimation on opacity {
            id: splashFadeOut
            running: false
            from: 1; to: 0
            duration: 500
            easing.type: Easing.InQuad
            onFinished: splashVisible = false
        }

        Timer {
            interval: 3000
            running: splashVisible
            onTriggered: splashFadeOut.running = true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: splashFadeOut.running = true
        }
    }



    // ========== PSK REPORTER SEARCH RESULT POPUP ==========
    Popup {
        id: pskSearchPopup
        modal: false
        x: (parent.width - width) / 2
        y: 100
        width: 320
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
            border.color: bridge.pskSearchFound ? accentGreen : bridge.themeManager.ledRed
            border.width: 2
            radius: 12
        }

        contentItem: ColumnLayout {
            spacing: 12

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: bridge.pskSearchFound ? accentGreen : bridge.themeManager.ledRed
                }

                Text {
                    text: bridge.pskSearchCallsign
                    font.pixelSize: 18
                    font.bold: true
                    color: textPrimary
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: bridge.pskSearching ? "Searching..." : (bridge.pskSearchFound ? "ONLINE" : "OFFLINE")
                    font.pixelSize: 12
                    font.bold: true
                    color: bridge.pskSearchFound ? accentGreen : bridge.themeManager.ledRed
                }
            }

            // Loading indicator
            Rectangle {
                id: loadingIndicatorBg
                Layout.fillWidth: true
                height: 3
                color: glassBorder
                radius: 1.5
                visible: bridge.pskSearching

                Rectangle {
                    width: loadingIndicatorBg.width * 0.3
                    height: loadingIndicatorBg.height
                    radius: 1.5
                    color: secondaryCyan

                    SequentialAnimation on x {
                        running: bridge.pskSearching && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        NumberAnimation { to: loadingIndicatorBg.width * 0.7; duration: 800; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 0; duration: 800; easing.type: Easing.InOutQuad }
                    }
                }
            }

            // Bands section
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: !bridge.pskSearching && bridge.pskSearchFound

                Text {
                    text: "Active on bands:"
                    font.pixelSize: 12
                    color: textSecondary
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: bridge.pskSearchBands

                        Rectangle {
                            width: bandLabel.implicitWidth + 16
                            height: 28
                            radius: 6
                            color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2)
                            border.color: accentGreen
                            border.width: 1

                            Text {
                                id: bandLabel
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: 13
                                font.bold: true
                                color: accentGreen
                            }

                            MouseArea {
                                id: bandMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    // Change to this band and tune radio
                                    if (bridge.bandManager) {
                                        // Convert lowercase to uppercase (e.g., "70cm" -> "70CM")
                                        var bandLambda = modelData.toUpperCase()
                                        console.log("PSK Search: Switching to band", bandLambda)
                                        bridge.bandManager.changeBandByLambda(bandLambda)
                                        pskSearchPopup.close()
                                    }
                                }
                            }

                            ToolTip.visible: bandMouseArea.containsMouse
                            ToolTip.text: "Click to switch to " + modelData
                            ToolTip.delay: 300
                        }
                    }
                }
            }

            // Not found message
            Text {
                Layout.fillWidth: true
                text: "No recent activity found\n(last 15 minutes)"
                font.pixelSize: 12
                color: textSecondary
                horizontalAlignment: Text.AlignHCenter
                visible: !bridge.pskSearching && !bridge.pskSearchFound
            }
        }

        // Auto-close timer
        Timer {
            id: pskPopupTimer
            interval: 8000
            onTriggered: pskSearchPopup.close()
        }

        onOpened: pskPopupTimer.restart()
    }

    // Listen for PSK search results
    // In 1.0.40 searchPskReporter() è passato da stub a query HTTP reale verso
    // pskreporter.info, quindi il popup è ora l'unico modo per vedere l'esito
    // della ricerca (ONLINE/OFFLINE + bande cliccabili). Apriamo il popup all'INIZIO
    // della ricerca così l'utente vede "Searching…" e poi il risultato; il timer
    // pskPopupTimer (8s) lo chiude da solo se resta aperto troppo a lungo.
    // Il vecchio gate uiPskSearchPopupEnabled è rimosso: bloccava anche le ricerche
    // esplicite dall'utente (l'unico caller di searchPskReporter è la barra manuale).
    Connections {
        target: bridge
        function onPskSearchingChanged() {
            if (bridge.pskSearching) {
                pskSearchPopup.open()
            }
        }
    }

    // ========== FREQUENCY INPUT POPUP ==========
    Popup {
        id: freqInputPopup
        property bool isTx: true
        modal: true
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 360
        height: 156
        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
            border.color: glassBorder
            radius: 10
        }
        onOpened: {
            freqInput.text = isTx ? bridge.txFrequency.toString() : bridge.rxFrequency.toString()
            freqInput.selectAll()
            freqInput.forceActiveFocus()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Text {
                text: freqInputPopup.isTx ? "TX Frequency (Hz)" : "RX Frequency (Hz)"
                font.pixelSize: 13
                font.bold: true
                color: freqInputPopup.isTx ? bridge.themeManager.ledRed : accentGreen
            }

            Text {
                text: "Range valido: 100-5000 Hz"
                font.pixelSize: 11
                color: textSecondary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                TextField {
                    id: freqInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    font.pixelSize: 18
                    font.family: "Monospace"
                    color: textPrimary
                    placeholderText: ""
                    selectByMouse: true
                    leftPadding: 12
                    rightPadding: 12
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 100; top: 5000 }
                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                        border.color: freqInput.activeFocus ? secondaryCyan : glassBorder
                        border.width: freqInput.activeFocus ? 2 : 1
                        radius: 6
                    }
                    Keys.onReturnPressed: freqApplyBtn.clicked()
                    Keys.onEscapePressed: freqInputPopup.close()
                }

                Button {
                    id: freqApplyBtn
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 40
                    text: "OK"
                    padding: 0
                    font.pixelSize: 12
                    font.bold: true
                    background: Rectangle {
                        color: freqApplyBtn.pressed ? Qt.rgba(0,188/255,212/255,0.4) : Qt.rgba(0,188/255,212/255,0.2)
                        border.color: secondaryCyan
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: secondaryCyan
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        var f = parseInt(freqInput.text)
                        if (f >= 100 && f <= 5000) {
                            if (freqInputPopup.isTx) bridge.setTxAudioFreqFromClick(f)
                            else bridge.setRxAudioFreqFromClick(f)
                            freqInputPopup.close()
                        }
                    }
                }
            }
        }
    }

    // Dock zone indicators (shown when dragging)
    Rectangle {
        id: waterfallDockIndicator
        x: waterfallDockZone.x
        y: waterfallDockZone.y
        width: waterfallDockZone.width
        height: waterfallDockZone.height
        color: "transparent"
        border.color: secondaryCyan
        border.width: 3
        radius: 10
        opacity: 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        Text {
            anchors.centerIn: parent
            text: "Dock Waterfall Here"
            color: secondaryCyan
            font.pixelSize: 16
            font.bold: true
            opacity: parent.opacity
        }
    }


    // Main Layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header Bar - Responsive with Flow layout
        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            Layout.preferredHeight: headerFlow.height + 12
            Layout.minimumHeight: 86
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
            z: 100

            Flow {
                id: headerFlow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 6
                spacing: 8

                // Hamburger Menu Button
                Rectangle {
                    width: 40
                    height: 74
                    radius: 8
                    color: menuButtonMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15)
                    border.color: menuButtonMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: menuButtonMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 5

                        Repeater {
                            model: 3
                            Rectangle {
                                width: 20
                                height: 3
                                radius: 1
                                color: secondaryCyan
                            }
                        }
                    }

                    MouseArea {
                        id: menuButtonMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mainMenu.open()
                    }

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.width { NumberAnimation { duration: 150 } }

                    ToolTip.visible: menuButtonMA.containsMouse
                    ToolTip.text: "Menu"
                    ToolTip.delay: 500
                }

                // Logo group
                Rectangle {
                    width: 90
                    height: 74
                    color: "transparent"

                    Column {
                        anchors.centerIn: parent
                        Text {
                            text: "DECODIUM"
                            font.pixelSize: 14
                            font.bold: true
                            font.letterSpacing: 1
                            color: secondaryCyan
                        }
                        Text {
                            text: "v" + bridge.version()
                            font.pixelSize: 9
                            color: textSecondary
                        }
                    }
                }

                // Radio Frequency Display with CAT status
                Rectangle {
                    width: bridge.catConnected ? 340 : 290
                    height: 74
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: bridge.catConnected ? accentGreen : glassBorder
                    border.width: bridge.catConnected ? 2 : 1
                    radius: 6

                    Behavior on width { NumberAnimation { duration: 200 } }

                    Column {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.topMargin: 4
                        spacing: 2

                        RowLayout {
                            width: parent.width
                            spacing: 8

                            // CAT connection indicator
                            Rectangle {
                                width: 12
                                height: 12
                                radius: 6
                                color: bridge.catConnected ? accentGreen : "#555"

                                SequentialAnimation on opacity {
                                    running: bridge.catConnected && bridge.uiQuality !== "Low"
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.4; duration: 800 }
                                    OpacityAnimator { to: 1.0; duration: 800 }
                                }
                            }

                            // Frequency display - syncs with radio or band selection
                            Text {
                                id: frequencyDisplay
                                // bridge.frequency is synced with both CAT and BandManager
                                text: (bridge.frequency / 1000000).toFixed(6)
                                font.pixelSize: Math.round(26 * fs)
                                font.family: "Monospace"
                                font.bold: true
                                color: mainWindow.txVisualActive ? "#ff6b6b" : accentGreen
                                Layout.fillWidth: true

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }

                            Text {
                                text: "MHz"
                                font.pixelSize: 11
                                color: textSecondary
                            }

                            // Mode from radio (when CAT connected)
                            Rectangle {
                                visible: bridge.catConnected
                                width: 65
                                height: 26
                                radius: 4
                                color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                                border.color: secondaryCyan

                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.catMode || "---"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: secondaryCyan
                                }
                            }
                        }

                        // TX/RX Frequency row (click values to edit)
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 8

                            Text {
                                text: "TX:"
                                font.pixelSize: 10
                                font.bold: true
                                color: mainWindow.txVisualActive ? bridge.themeManager.ledRed : textSecondary
                            }
                            // TX frequency - click to edit
                            Rectangle {
                                width: txFreqText.width + 6; height: 14; radius: 2
                                color: txFreqMouseArea.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1) : "transparent"
                                Text {
                                    id: txFreqText; anchors.centerIn: parent
                                    text: bridge.txFrequency + " Hz"
                                    font.pixelSize: 10; font.family: "Monospace"
                                    color: bridge.txFrequency === bridge.rxFrequency ? accentGreen : bridge.themeManager.ledRed
                                }
                                MouseArea {
                                    id: txFreqMouseArea; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { freqInputPopup.isTx = true; freqInputPopup.open() }
                                }
                            }

                            Rectangle { width: 1; height: 12; color: glassBorder }

                            Text {
                                text: "RX:"
                                font.pixelSize: 10
                                font.bold: true
                                color: textSecondary
                            }
                            // RX frequency - click to edit
                            Rectangle {
                                width: rxFreqText.width + 6; height: 14; radius: 2
                                color: rxFreqMouseArea.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1) : "transparent"
                                Text {
                                    id: rxFreqText; anchors.centerIn: parent
                                    text: bridge.rxFrequency + " Hz"
                                    font.pixelSize: 10; font.family: "Monospace"
                                    color: accentGreen
                                }
                                MouseArea {
                                    id: rxFreqMouseArea; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { freqInputPopup.isTx = false; freqInputPopup.open() }
                                }
                            }

                            // TX=RX sync button
                            Rectangle {
                                width: 40
                                height: 14
                                radius: 3
                                color: bridge.txFrequency === bridge.rxFrequency ?
                                       Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) :
                                       Qt.rgba(244/255, 67/255, 54/255, 0.3)
                                border.color: bridge.txFrequency === bridge.rxFrequency ? accentGreen : bridge.themeManager.ledRed

                                Text {
                                    anchors.centerIn: parent
                                    text: "TX=RX"
                                    font.pixelSize: 8
                                    font.bold: true
                                    color: bridge.txFrequency === bridge.rxFrequency ? accentGreen : bridge.themeManager.ledRed
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.txFrequency = bridge.rxFrequency
                                }
                            }
                        }
                    }

                    // FT8 Advanced Decoding LED Panel - Below frequency
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 3
                        spacing: 6

                        Text {
                            text: "ADV"
                            font.pixelSize: 8
                            font.bold: true
                            color: ((bridge.coherentAvgEnabled && bridge.ledCoherentAveraging)
                                 || (bridge.neuralSyncEnabled && bridge.ledNeuralSync)
                                 || (bridge.turboFeedbackEnabled && bridge.ledTurboFeedback)) ? accentGreen : "#555"
                        }

                        // LED 1: Coherent Averaging (Blue)
                        Rectangle {
                            id: ledCoherent
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.coherentAvgEnabled
                                ? (bridge.ledCoherentAveraging ? bridge.themeManager.ledBlue : "#0D47A1")
                                : "#333"
                            border.color: bridge.coherentAvgEnabled
                                ? (bridge.ledCoherentAveraging ? "#64B5F6" : "#1565C0")
                                : "#444"
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: bridge.coherentAvgEnabled && bridge.ledCoherentAveraging && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.5; duration: 400 }
                                OpacityAnimator { to: 1.0; duration: 400 }
                            }

                            ToolTip.visible: maCoherent.containsMouse
                            ToolTip.text: "Coherent Avg: " + (bridge.coherentAvgEnabled
                                ? (bridge.ledCoherentAveraging ? bridge.coherentCount + " signals" : "ON (idle)")
                                : "OFF (disabled)") + "  -  click to toggle"

                            MouseArea {
                                id: maCoherent
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.coherentAvgEnabled = !bridge.coherentAvgEnabled
                            }
                        }

                        // LED 2: Neural Sync (Purple)
                        Rectangle {
                            id: ledNeural
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.neuralSyncEnabled
                                ? (bridge.ledNeuralSync ? "#9C27B0" : "#4A148C")
                                : "#333"
                            border.color: bridge.neuralSyncEnabled
                                ? (bridge.ledNeuralSync ? "#CE93D8" : "#7B1FA2")
                                : "#444"
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: bridge.neuralSyncEnabled && bridge.ledNeuralSync && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.5; duration: 300 }
                                OpacityAnimator { to: 1.0; duration: 300 }
                            }

                            ToolTip.visible: maNeural.containsMouse
                            ToolTip.text: "Neural Sync: " + (bridge.neuralSyncEnabled
                                ? (bridge.ledNeuralSync ? (bridge.neuralScore * 100).toFixed(0) + "%" : "ON (idle)")
                                : "OFF (disabled)") + "  -  click to toggle"

                            MouseArea {
                                id: maNeural
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.neuralSyncEnabled = !bridge.neuralSyncEnabled
                            }
                        }

                        // LED 3: Turbo Feedback (Orange)
                        Rectangle {
                            id: ledTurbo
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.turboFeedbackEnabled
                                ? (bridge.ledTurboFeedback ? "#FF9800" : "#E65100")
                                : "#333"
                            border.color: bridge.turboFeedbackEnabled
                                ? (bridge.ledTurboFeedback ? "#FFCC80" : "#F57C00")
                                : "#444"
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: bridge.turboFeedbackEnabled && bridge.ledTurboFeedback && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.5; duration: 350 }
                                OpacityAnimator { to: 1.0; duration: 350 }
                            }

                            ToolTip.visible: maTurbo.containsMouse
                            ToolTip.text: "Turbo Feedback: " + (bridge.turboFeedbackEnabled
                                ? (bridge.ledTurboFeedback ? bridge.turboIterations + " iter" : "ON (idle)")
                                : "OFF (disabled)") + "  -  click to toggle"

                            MouseArea {
                                id: maTurbo
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.turboFeedbackEnabled = !bridge.turboFeedbackEnabled
                            }
                        }

                        // Separator
                        Rectangle {
                            width: 1
                            height: 8
                            color: glassBorder
                        }

                        // Time Sync LED (Green when synced)
                        Rectangle {
                            id: ledTimeSync
                            width: 8
                            height: 8
                            radius: 4
                            color: accentGreen
                            border.color: bridge.themeManager.successColor
                            border.width: 1
                        }

                        // UTC Time display — click to trigger NTP sync (FT8/FT4 DT alignment)
                        Text {
                            id: utcTimeLabel
                            text: bridge.utcTime
                            font.pixelSize: 8
                            font.family: "Monospace"
                            font.bold: true
                            color: clockMouseArea.containsMouse
                                       ? bridge.themeManager.warningColor
                                       : accentGreen

                            MouseArea {
                                id: clockMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.syncNtpNow()
                                ToolTip.visible: containsMouse
                                ToolTip.delay: 600
                                ToolTip.text: qsTr("Click: immediate NTP sync to align DT (FT8/FT4)")
                            }
                        }
                    }

                    // TX indicator border
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.color: bridge.themeManager.ledRed
                        border.width: 3
                        visible: mainWindow.txVisualActive
                        opacity: 0

                        SequentialAnimation on opacity {
                            running: mainWindow.txVisualActive && bridge && bridge.uiQuality !== "Low"
                            loops: Animation.Infinite
                            OpacityAnimator { to: 1.0; duration: 250 }
                            OpacityAnimator { to: 0.3; duration: 250 }
                        }
                    }
                }

                // RX/TX Sliders + LVL/Monitor
                Item {
                    width: 200
                    height: 74

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 2

                        // Row 1: RX/TX Sliders (top)
                        Rectangle {
                            Layout.preferredHeight: 32
                            Layout.fillWidth: true
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                            border.color: glassBorder
                            radius: 4

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 2
                                spacing: 0

                                RowLayout {
                                    spacing: 2
                                    Text { text: "RX"; color: secondaryCyan; font.pixelSize: 8; font.bold: true; Layout.preferredWidth: 16 }
                                    Slider {
                                        id: rxSliderHeader
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 14
                                        from: 0; to: 100; live: true; stepSize: 1
                                        Component.onCompleted: if (bridge) value = bridge.rxInputLevel
                                        onMoved: if (bridge) bridge.rxInputLevel = value
                                        onPressedChanged: {
                                            if (!pressed && bridge && Math.abs(bridge.rxInputLevel - value) >= 0.5)
                                                bridge.rxInputLevel = value
                                        }
                                        Connections {
                                            target: bridge
                                            function onRxInputLevelChanged() {
                                                if (!rxSliderHeader.pressed && Math.abs(rxSliderHeader.value - bridge.rxInputLevel) >= 0.5)
                                                    rxSliderHeader.value = bridge.rxInputLevel
                                            }
                                        }
                                        background: Rectangle {
                                            x: rxSliderHeader.leftPadding; y: rxSliderHeader.topPadding + rxSliderHeader.availableHeight / 2 - height / 2
                                            width: rxSliderHeader.availableWidth; height: 3; radius: 1; color: bgMedium
                                            Rectangle { width: rxSliderHeader.visualPosition * parent.width; height: parent.height; color: secondaryCyan; radius: 1 }
                                        }
                                        handle: Rectangle {
                                            x: rxSliderHeader.leftPadding + rxSliderHeader.visualPosition * (rxSliderHeader.availableWidth - width)
                                            y: rxSliderHeader.topPadding + rxSliderHeader.availableHeight / 2 - height / 2
                                            width: 8; height: 8; radius: 4; color: secondaryCyan
                                        }
                                    }
                                    Text { text: Math.round(bridge.rxInputLevel); color: secondaryCyan; font.pixelSize: 8; font.family: "Monospace"; Layout.preferredWidth: 18 }
                                }

                                RowLayout {
                                    spacing: 2
                                    Text { text: "TX"; color: accentGreen; font.pixelSize: 8; font.bold: true; Layout.preferredWidth: 16 }
                                    Slider {
                                        id: txSliderHeader
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 14
                                        from: 450; to: 0; live: true; stepSize: 1
                                        onMoved: bridge.txOutputLevel = value
                                        Binding on value { value: bridge.txOutputLevel; when: !txSliderHeader.pressed }
                                        background: Rectangle {
                                            x: txSliderHeader.leftPadding; y: txSliderHeader.topPadding + txSliderHeader.availableHeight / 2 - height / 2
                                            width: txSliderHeader.availableWidth; height: 3; radius: 1; color: bgMedium
                                            Rectangle { width: txSliderHeader.visualPosition * parent.width; height: parent.height; color: accentGreen; radius: 1 }
                                        }
                                        handle: Rectangle {
                                            x: txSliderHeader.leftPadding + txSliderHeader.visualPosition * (txSliderHeader.availableWidth - width)
                                            y: txSliderHeader.topPadding + txSliderHeader.availableHeight / 2 - height / 2
                                            width: 8; height: 8; radius: 4; color: accentGreen
                                        }
                                    }
                                    Text {
                                        text: bridge.txOutputLevel > 0 ? ("-" + (bridge.txOutputLevel / 10).toFixed(1)) : "0.0"
                                        color: accentGreen
                                        font.pixelSize: 8
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 28
                                    }
                                }
                            }
                        }

                        // Row 2: Mode selector
                        RowLayout {
                            spacing: 2

                            // Mode selector
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                border.color: glassBorder; radius: 4

                                ComboBox {
                                    id: compactModeCombo
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    model: bridge.availableModes()
                                    currentIndex: model.indexOf(bridge.mode)
                                    onActivated: function(idx) {
                                        bridge.mode = model[idx]
                                    }
                                    font.pixelSize: 11; font.bold: true
                                    leftPadding: 8
                                    rightPadding: 22
                                    topPadding: 4
                                    bottomPadding: 4
                                    background: Rectangle { color: "transparent" }
                                    contentItem: Text {
                                        text: compactModeCombo.displayText
                                        font.pixelSize: 11; font.bold: true
                                        color: secondaryCyan
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignLeft
                                        elide: Text.ElideRight
                                    }
                                    delegate: ItemDelegate {
                                        width: compactModePopup.width
                                        height: 34
                                        contentItem: Text {
                                            text: modelData
                                            font.pixelSize: 11; font.bold: true
                                            color: bridge.mode === modelData ? secondaryCyan : textPrimary
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }
                                        background: Rectangle {
                                            color: hovered ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                                        }
                                    }
                                    popup: Popup {
                                        id: compactModePopup
                                        y: compactModeCombo.height + 1
                                        width: Math.max(compactModeCombo.width, 168)
                                        implicitHeight: Math.min(contentItem.implicitHeight + 2, 360)
                                        padding: 1

                                        contentItem: ListView {
                                            clip: true
                                            implicitHeight: contentHeight
                                            model: compactModeCombo.popup.visible ? compactModeCombo.delegateModel : null
                                            currentIndex: compactModeCombo.highlightedIndex

                                            ScrollBar.vertical: ScrollBar {
                                                policy: ScrollBar.AsNeeded
                                            }
                                        }

                                        background: Rectangle {
                                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.97)
                                            border.color: glassBorder
                                            radius: 4
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Select decoder mode")
                                }
                            }

                            // Monitor/Stop Button
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 16
                                color: bridge.monitoring ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                border.color: bridge.monitoring ? accentGreen : glassBorder
                                border.width: 1
                                radius: 2

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 2

                                    Text {
                                        text: bridge.monitoring ? "⏹" : "▶"
                                        font.pixelSize: 9
                                        color: bridge.monitoring ? accentGreen : secondaryCyan
                                        anchors.verticalCenter: parent.verticalCenter

                                        SequentialAnimation on opacity {
                                            running: bridge.monitoring && bridge.uiQuality !== "Low"
                                            loops: Animation.Infinite
                                            OpacityAnimator { to: 0.4; duration: 500 }
                                            OpacityAnimator { to: 1.0; duration: 500 }
                                        }
                                    }

                                    Text {
                                        text: bridge.monitoring ? "STOP" : "MON"
                                        font.pixelSize: 7
                                        font.bold: true
                                        color: bridge.monitoring ? accentGreen : textPrimary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    id: monitorMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.monitoring ? bridge.stopMonitor() : bridge.startMonitor()
                                }

                                ToolTip.visible: monitorMA.containsMouse
                                ToolTip.text: bridge.monitoring ? "Stop monitoring" : "Start monitoring"
                            }
                        }
                    }
                } // End Sliders Item

                // Grouped buttons: Settings, REC, WAV, Log, Macro, Astro, CAT
                Item {
                    id: headerUtilityButtons
                    width: 360
                    height: 74

                    Rectangle {
                        width: parent.width
                        height: 28
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                        border.color: glassBorder
                        radius: 4

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 1

                        // Settings
                        Rectangle {
                            id: settingsButton
                            Layout.preferredWidth: 50
                            Layout.fillHeight: true
                            radius: 3
                            color: settingsMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "⚙"
                                    font.pixelSize: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "Setup"
                                    font.pixelSize: 9
                                    color: textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: settingsMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: openSettingsDialog()
                            }

                            ToolTip.visible: settingsMA.containsMouse
                            ToolTip.text: "Settings"
                        }

                        // REC
                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.fillHeight: true
                            radius: 3
                            color: recMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) :
                                   (bridge.recordRxEnabled ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent")
                            border.color: bridge.recordRxEnabled ? bridge.themeManager.ledRed : "transparent"
                            border.width: bridge.recordRxEnabled ? 1 : 0

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: bridge.recordRxEnabled ? "●" : "○"
                                    font.pixelSize: 12
                                    color: bridge.recordRxEnabled ? bridge.themeManager.ledRed : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "REC"
                                    font.pixelSize: 9
                                    color: bridge.recordRxEnabled ? bridge.themeManager.ledRed : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: recMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.recordRxEnabled = !bridge.recordRxEnabled
                            }

                            ToolTip.visible: recMA.containsMouse
                            ToolTip.text: bridge.recordRxEnabled && bridge.wavManager ?
                                          "Recording: " + bridge.wavManager.recordedSeconds + "s" : "Start Recording"
                        }

                        // Open WAV for decode
                        Rectangle {
                            Layout.preferredWidth: 45
                            Layout.fillHeight: true
                            radius: 3
                            color: wavMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "📂"
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "WAV"
                                    font.pixelSize: 9
                                    color: textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: wavMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton)
                                        wavFolderDialog.open()  // Right-click: batch folder
                                    else
                                        wavOpenDialog.open()    // Left-click: single file
                                }
                            }

                            ToolTip.visible: wavMA.containsMouse
                            ToolTip.text: qsTr("Click: open one WAV file\nRight-click: decode a folder")
                        }

                        // Separator
                        Rectangle { width: 1; Layout.fillHeight: true; Layout.topMargin: 4; Layout.bottomMargin: 4; color: glassBorder }

                        // Log
                        Rectangle {
                            Layout.preferredWidth: 45
                            Layout.fillHeight: true
                            radius: 3
                            color: logMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "📋"
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "Log"
                                    font.pixelSize: 9
                                    color: secondaryCyan
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: logMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: openLogWindow()
                            }

                            ToolTip.visible: logMA.containsMouse
                            ToolTip.text: "QSO Log"
                        }

                        // Macro
                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.fillHeight: true
                            radius: 3
                            color: macroMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) :
                                   (bridge.macroManager && bridge.macroManager.contestMode ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : "transparent")
                            border.color: bridge.macroManager && bridge.macroManager.contestMode ? accentGreen : "transparent"
                            border.width: bridge.macroManager && bridge.macroManager.contestMode ? 1 : 0

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "M"
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: bridge.macroManager && bridge.macroManager.contestMode ? accentGreen : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "Macro"
                                    font.pixelSize: 9
                                    color: bridge.macroManager && bridge.macroManager.contestMode ? accentGreen : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: macroMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: openMacroDialog()
                            }

                            ToolTip.visible: macroMA.containsMouse
                            ToolTip.text: bridge.macroManager && bridge.macroManager.contestMode ?
                                          "Contest: " + bridge.macroManager.contestName : "TX Macros"
                        }

                        // Astro
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.fillHeight: true
                            radius: 3
                            color: astroMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "🌙"
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "Astro"
                                    font.pixelSize: 9
                                    color: textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: astroMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: openAstroWindow()
                            }

                            ToolTip.visible: astroMA.containsMouse
                            ToolTip.text: "Astronomical Data"
                        }

                        // Separator
                        Rectangle { width: 1; Layout.fillHeight: true; Layout.topMargin: 4; Layout.bottomMargin: 4; color: glassBorder }

                        // CAT - native HvRigControl
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.fillHeight: true
                            radius: 3
                            color: catMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) :
                                   (bridge.catConnected ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : "transparent")
                            border.color: bridge.catConnected ? accentGreen : "transparent"
                            border.width: bridge.catConnected ? 1 : 0

                            Row {
                                anchors.centerIn: parent
                                spacing: 2

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: bridge.catConnected ? accentGreen : bridge.themeManager.ledRed
                                }

                                Text {
                                    text: "CAT"
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: bridge.catConnected ? accentGreen : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: catMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: openSettingsTab(1)
                            }

                            ToolTip.visible: catMA.containsMouse
                            ToolTip.text: bridge.catConnected ? "CAT: " + bridge.catRigName : "Click to configure CAT"
                        }
                    }
                    } // End Rectangle
                } // End Grouped buttons Item

	                // World Clock with Analog Display
	                Item {
	                    id: worldClock
	                    visible: showWorldClock
	                    width: showWorldClock ? compactWidth : 0
	                    height: 80
	                    readonly property int analogClockWidth: 60
	                    readonly property int cardMargins: 10
	                    readonly property int rowSpacing: 12
	                    readonly property bool hasInfoColumn: showDigitalClock || showWorldClockCities
	                    readonly property int infoColumnWidth: showWorldClockCities ? 190 : 126
	                    readonly property int compactWidth: Math.max(74,
	                        (cardMargins * 2)
	                        + (showAnalogClock ? analogClockWidth : 0)
	                        + (showAnalogClock && hasInfoColumn ? rowSpacing : 0)
	                        + (hasInfoColumn ? infoColumnWidth : 0))

	                    property string selectedZoneId: String(bridge.getSetting("uiWorldClockZoneId", "UTC") || "UTC")
	                    property string selectedCityName: String(bridge.getSetting("uiWorldClockCityName", "UTC") || "UTC")
	                    property string citySearchText: ""
	                    property var citySearchResults: bridge.worldClockCityOptions("", 24)
	                    property int hours: 0
	                    property int minutes: 0
	                    property int seconds: 0
	                    property string dateStr: ""
	                    property bool showWorldClock: !!bridge.getSetting("uiWorldClockVisible", true)
	                    property bool showAnalogClock: !!bridge.getSetting("uiWorldClockShowAnalog", true)
	                    property bool showDigitalClock: !!bridge.getSetting("uiWorldClockShowDigital", true)
	                    property bool showWorldClockCities: !!bridge.getSetting("uiWorldClockShowCities", true)

	                    Timer {
	                        interval: 1000
	                        running: worldClock.showWorldClock
	                        repeat: true
	                        onTriggered: worldClock.updateTime()
	                    }

	                    Component.onCompleted: {
	                        ensureVisiblePart()
	                        refreshCitySearch()
	                        updateTime()
	                    }

	                    function ensureVisiblePart() {
	                        if (!showAnalogClock && !showDigitalClock && !showWorldClockCities) {
	                            showDigitalClock = true
	                        }
	                    }

	                    function setClockPart(part, visible) {
	                        if (part === "analog") {
	                            showAnalogClock = visible
	                        } else if (part === "digital") {
	                            showDigitalClock = visible
	                        } else if (part === "cities") {
	                            showWorldClockCities = visible
	                        }

	                        ensureVisiblePart()
	                        bridge.setSetting("uiWorldClockShowAnalog", showAnalogClock)
	                        bridge.setSetting("uiWorldClockShowDigital", showDigitalClock)
	                        bridge.setSetting("uiWorldClockShowCities", showWorldClockCities)
	                    }

	                    function setClockVisible(visible) {
	                        showWorldClock = visible
	                        if (showWorldClock) {
	                            ensureVisiblePart()
	                            updateTime()
	                        }
	                        bridge.setSetting("uiWorldClockVisible", showWorldClock)
	                    }

                    function refreshCitySearch() {
                        citySearchResults = bridge.worldClockCityOptions(citySearchText, 40)
                        Qt.callLater(function() {
                            if (citySearchList)
                                citySearchList.positionViewAtBeginning()
                        })
                    }

                    function selectTimezone(option) {
                        if (!option || !option.zoneId)
                            return
                        selectedZoneId = option.zoneId
                        selectedCityName = option.name || option.zoneId
                        bridge.setSetting("uiWorldClockZoneId", selectedZoneId)
                        bridge.setSetting("uiWorldClockCityName", selectedCityName)
                        updateTime()
                        citySearchPopup.close()
                    }

                    function updateTime() {
                        var snapshot = bridge.worldClockSnapshot(selectedZoneId)
                        selectedZoneId = snapshot.timeZoneId || "UTC"
                        hours = snapshot.hours
                        minutes = snapshot.minutes
                        seconds = snapshot.seconds
                        dateStr = snapshot.date || ""
                    }

                    function nextTimezone() {
                        var defaults = bridge.worldClockCityOptions("", 24)
                        if (!defaults || defaults.length === 0)
                            return
                        var idx = 0
                        for (var i = 0; i < defaults.length; ++i) {
                            if (defaults[i].zoneId === selectedZoneId) {
                                idx = (i + 1) % defaults.length
                                break
                            }
                        }
                        selectTimezone(defaults[idx])
                    }

                    Rectangle {
                        anchors.fill: parent
                        clip: true
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: clockHover.hovered ? secondaryCyan : glassBorder
                        border.width: clockHover.hovered ? 2 : 1
                        radius: 6

                        HoverHandler {
                            id: clockHover
                        }

	                        Rectangle {
	                            id: analogClockFace
	                            visible: worldClock.showAnalogClock
	                            anchors.left: parent.left
	                            anchors.leftMargin: worldClock.showAnalogClock ? worldClock.cardMargins : 0
	                            anchors.verticalCenter: parent.verticalCenter
	                            width: worldClock.showAnalogClock ? worldClock.analogClockWidth : 0
	                            height: 60
	                            radius: 30
	                            color: bgMedium
                            border.color: secondaryCyan
                            border.width: 1

                            Repeater {
                                model: 12
                                Rectangle {
                                    width: index % 3 === 0 ? 3 : 2
                                    height: index % 3 === 0 ? 6 : 3
                                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.5)
                                    x: 30 + 24 * Math.sin(index * 30 * Math.PI / 180) - width/2
                                    y: 30 - 24 * Math.cos(index * 30 * Math.PI / 180) - height/2
                                }
                            }

                            Rectangle {
                                width: 3; height: 18; color: textPrimary
                                x: 28.5; y: 14
                                transformOrigin: Item.Bottom
                                rotation: (worldClock.hours % 12 + worldClock.minutes / 60) * 30
                            }

                            Rectangle {
                                width: 2; height: 22; color: secondaryCyan
                                x: 29; y: 10
                                transformOrigin: Item.Bottom
                                rotation: worldClock.minutes * 6
                            }

                            Rectangle {
                                width: 1.5; height: 26; color: accentGreen
                                x: 29.25; y: 6
                                transformOrigin: Item.Bottom
                                rotation: worldClock.seconds * 6
                            }

                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: accentGreen; x: 27; y: 27
                            }
                        }

	                        Item {
	                            id: worldClockInfoColumn
	                            visible: worldClock.hasInfoColumn
	                            anchors.left: analogClockFace.right
	                            anchors.leftMargin: worldClock.showAnalogClock ? worldClock.rowSpacing : worldClock.cardMargins
	                            anchors.right: parent.right
	                            anchors.rightMargin: worldClock.cardMargins
	                            anchors.verticalCenter: parent.verticalCenter
	                            height: (worldClock.showDigitalClock ? 39 : 0)
	                                    + (worldClock.showWorldClockCities ? 32 : 0)
	                                    + (worldClock.showDigitalClock && worldClock.showWorldClockCities ? 4 : 0)

	                            Text {
	                                id: worldClockTimeText
	                                visible: worldClock.showDigitalClock
	                                anchors.left: parent.left
	                                anchors.right: parent.right
	                                anchors.top: parent.top
	                                height: 25
	                                font.pixelSize: 22
	                                minimumPixelSize: 17
	                                fontSizeMode: Text.Fit
	                                font.family: "Monospace"
	                                font.bold: true
	                                color: textPrimary
	                                elide: Text.ElideRight
	                                text: ("0" + worldClock.hours).slice(-2) + ":" +
	                                      ("0" + worldClock.minutes).slice(-2) + ":" +
	                                      ("0" + worldClock.seconds).slice(-2)
	                            }

	                            Text {
	                                id: worldClockDateText
	                                visible: worldClock.showDigitalClock
	                                anchors.left: parent.left
	                                anchors.right: parent.right
	                                anchors.top: worldClockTimeText.bottom
	                                anchors.topMargin: 1
	                                height: 13
	                                text: worldClock.dateStr
	                                font.pixelSize: 11
	                                font.family: "Monospace"
	                                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.7)
	                                elide: Text.ElideRight
	                            }

	                            Rectangle {
	                                id: timezoneSelector
	                                visible: worldClock.showWorldClockCities
	                                anchors.left: parent.left
	                                anchors.right: parent.right
	                                anchors.top: worldClock.showDigitalClock ? worldClockDateText.bottom : parent.top
	                                anchors.topMargin: worldClock.showDigitalClock ? 4 : 0
	                                height: 32
	                                radius: 6
	                                color: timezoneSelectorMA.containsMouse || citySearchPopup.opened
	                                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
	                                       : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.92)
	                                border.color: citySearchPopup.opened ? secondaryCyan : glassBorder
	                                border.width: citySearchPopup.opened ? 2 : 1

	                                Text {
	                                    anchors.left: parent.left
	                                    anchors.right: timezoneArrow.left
	                                    anchors.verticalCenter: parent.verticalCenter
	                                    anchors.leftMargin: 12
	                                    anchors.rightMargin: 6
	                                    text: worldClock.selectedCityName
	                                    font.pixelSize: 12
	                                    color: textPrimary
	                                    elide: Text.ElideRight
	                                    verticalAlignment: Text.AlignVCenter
	                                }

	                                Text {
	                                    id: timezoneArrow
	                                    anchors.right: parent.right
	                                    anchors.rightMargin: 10
	                                    anchors.verticalCenter: parent.verticalCenter
	                                    text: citySearchPopup.opened ? "▲" : "▼"
	                                    font.pixelSize: 11
	                                    color: secondaryCyan
	                                }

	                                MouseArea {
	                                    id: timezoneSelectorMA
	                                    anchors.fill: parent
	                                    hoverEnabled: true
	                                    cursorShape: Qt.PointingHandCursor
	                                    onClicked: {
	                                        worldClock.citySearchText = ""
	                                        worldClock.refreshCitySearch()
	                                        citySearchPopup.open()
	                                    }
	                                }
	                            }

			                            Popup {
			                                id: citySearchPopup
			                                x: timezoneSelector.x
			                                y: timezoneSelector.y + timezoneSelector.height + 4
			                                width: Math.max(timezoneSelector.width, 340)
			                                property int visibleResultRows: Math.max(1, Math.min(7, worldClock.citySearchResults.length))
			                                property int preferredHeight: 64 + visibleResultRows * 46 + (worldClock.citySearchResults.length === 0 ? 36 : 8)
                                            property real availableHeightBelow: {
                                                var p = timezoneSelector.mapToItem(mainWindow.contentItem, 0, timezoneSelector.height + 4)
                                                return Math.max(154, mainWindow.height - p.y - 16)
                                            }
			                                height: Math.min(420, Math.min(Math.max(154, preferredHeight),
			                                    availableHeightBelow))
			                                modal: false
			                                focus: true
			                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
			                                padding: 0
			                                onOpened: {
                                                worldClock.refreshCitySearch()
                                                citySearchField.forceActiveFocus()
                                            }

	                                background: Rectangle {
	                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
	                                    border.color: secondaryCyan
	                                    border.width: 1
	                                    radius: 8
	                                }

		                                contentItem: Item {
		                                    TextField {
		                                        id: citySearchField
		                                        anchors.left: parent.left
		                                        anchors.right: parent.right
		                                        anchors.top: parent.top
		                                        anchors.margins: 10
		                                        height: 34
		                                        text: worldClock.citySearchText
		                                        placeholderText: qsTr("Search city...")
		                                        placeholderTextColor: textSecondary
	                                        color: textPrimary
		                                        selectionColor: secondaryCyan
		                                        selectedTextColor: bgDeep
		                                        font.pixelSize: 13
                                                verticalAlignment: TextInput.AlignVCenter
		                                        background: Rectangle {
	                                            color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.85)
	                                            border.color: citySearchField.activeFocus ? secondaryCyan : glassBorder
	                                            border.width: citySearchField.activeFocus ? 2 : 1
	                                            radius: 5
	                                        }
	                                        onTextChanged: {
	                                            if (worldClock.citySearchText !== text) {
	                                                worldClock.citySearchText = text
	                                                worldClock.refreshCitySearch()
	                                            }
	                                        }
	                                        Keys.onReturnPressed: {
	                                            if (worldClock.citySearchResults.length > 0)
	                                                worldClock.selectTimezone(worldClock.citySearchResults[0])
	                                        }
	                                    }

		                                    ListView {
		                                        id: citySearchList
		                                        anchors.left: parent.left
		                                        anchors.right: parent.right
		                                        anchors.top: citySearchField.bottom
		                                        anchors.bottom: parent.bottom
		                                        anchors.leftMargin: 8
		                                        anchors.rightMargin: 8
		                                        anchors.topMargin: 8
		                                        anchors.bottomMargin: 8
		                                        clip: true
		                                        visible: worldClock.citySearchResults.length > 0
		                                        model: worldClock.citySearchResults
		                                        spacing: 2
		                                        boundsBehavior: Flickable.StopAtBounds

		                                        delegate: Rectangle {
		                                            width: citySearchList.width
		                                            height: 42
		                                            radius: 5
	                                            color: cityRowMA.containsMouse
	                                                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
	                                                   : (modelData.zoneId === worldClock.selectedZoneId
	                                                      ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.14)
	                                                      : "transparent")

	                                            Column {
	                                                anchors.left: parent.left
	                                                anchors.right: parent.right
	                                                anchors.verticalCenter: parent.verticalCenter
	                                                anchors.leftMargin: 10
	                                                anchors.rightMargin: 10
	                                                spacing: 1

	                                                Text {
	                                                    width: parent.width
	                                                    text: modelData.name || ""
	                                                    color: textPrimary
	                                                    font.pixelSize: 13
	                                                    font.bold: modelData.zoneId === worldClock.selectedZoneId
	                                                    elide: Text.ElideRight
	                                                }

	                                                Text {
	                                                    width: parent.width
	                                                    text: modelData.subtitle || modelData.zoneId || ""
	                                                    color: textSecondary
	                                                    font.pixelSize: 10
	                                                    elide: Text.ElideRight
	                                                }
	                                            }

	                                            MouseArea {
	                                                id: cityRowMA
	                                                anchors.fill: parent
	                                                hoverEnabled: true
	                                                cursorShape: Qt.PointingHandCursor
	                                                onClicked: worldClock.selectTimezone(modelData)
	                                            }
	                                        }

	                                        ScrollBar.vertical: ScrollBar {
	                                            policy: ScrollBar.AsNeeded
	                                            width: 8
		                                        }
		                                    }

		                                    Rectangle {
		                                        anchors.left: parent.left
		                                        anchors.right: parent.right
		                                        anchors.top: citySearchField.bottom
		                                        anchors.bottom: parent.bottom
		                                        anchors.margins: 10
		                                        anchors.topMargin: 8
		                                        visible: worldClock.citySearchResults.length === 0
		                                        radius: 5
		                                        color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.06)
		                                        border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
		                                        border.width: 1

		                                        Text {
		                                            anchors.centerIn: parent
		                                            width: parent.width - 20
		                                            text: worldClock.citySearchText.trim().length === 0 ? qsTr("Type a city name") : qsTr("No matching city")
		                                            color: textSecondary
		                                            font.pixelSize: 13
		                                            horizontalAlignment: Text.AlignHCenter
		                                            wrapMode: Text.WordWrap
		                                        }
		                                    }
		                                }
		                            }
	                        }

	                        MouseArea {
	                            anchors.fill: parent
	                            acceptedButtons: Qt.RightButton
	                            cursorShape: Qt.PointingHandCursor
	                            onClicked: function(mouse) {
	                                if (mouse.button === Qt.RightButton) {
	                                    worldClockMenu.popup(mouse.x, mouse.y)
	                                }
	                            }
	                        }

	                        Menu {
	                            id: worldClockMenu
	                            padding: 6
	                            width: 230
	                            background: Rectangle {
	                                implicitWidth: 230
	                                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
	                                border.color: secondaryCyan
	                                border.width: 1
	                                radius: 8
	                            }

	                            MenuItem {
	                                text: (worldClock.showAnalogClock ? "✓ " : "☐ ") + qsTr("Analog clock")
	                                onTriggered: worldClock.setClockPart("analog", !worldClock.showAnalogClock)
	                            }
	                            MenuItem {
	                                text: (worldClock.showDigitalClock ? "✓ " : "☐ ") + qsTr("Digital clock")
	                                onTriggered: worldClock.setClockPart("digital", !worldClock.showDigitalClock)
	                            }
	                            MenuItem {
	                                text: (worldClock.showWorldClockCities ? "✓ " : "☐ ") + qsTr("City labels")
	                                onTriggered: worldClock.setClockPart("cities", !worldClock.showWorldClockCities)
	                            }
	                            MenuSeparator {
	                                contentItem: Rectangle { implicitHeight: 1; color: glassBorder }
	                            }
	                            MenuItem {
	                                text: qsTr("Hide clock")
	                                onTriggered: worldClock.setClockVisible(false)
	                            }
	                        }
	                    }
	                }

                // Waterfall restore button (visible when minimized)
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: waterfallMinimized
                    color: waterfallRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: waterfallRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: waterfallRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📊"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Waterfall"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: waterfallRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            waterfallMinimized = false
                            waterfallWindow.show()
                        }
                    }

                    ToolTip.visible: waterfallRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Waterfall")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: waterfallMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // Log Window restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: logWindowMinimized
                    color: logRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: logRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: logRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📋"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Log"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: logRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            logWindowMinimized = false
                            openLogWindow()
                        }
                    }

                    ToolTip.visible: logRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore QSO Log")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: logWindowMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // Astro Window restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: astroWindowMinimized
                    color: astroRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: astroRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: astroRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "🌙"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Astro"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: astroRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            astroWindowMinimized = false
                            openAstroWindow()
                        }
                    }

                    ToolTip.visible: astroRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Astronomical Data")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: astroWindowMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // 1.0.156: Live Map restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: liveMapMinimized
                    color: liveMapRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: liveMapRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: liveMapRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "🗺️"
                            font.pixelSize: 24
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Live Map"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: liveMapRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            mainWindow.liveMapMinimized = false
                            if (mainWindow.liveMapDetached && liveMapFloatingWindow) {
                                liveMapFloatingWindow.show()
                                liveMapFloatingWindow.visibility = Window.Windowed
                                liveMapFloatingWindow.raise()
                                liveMapFloatingWindow.requestActivate()
                            } else {
                                mainWindow.liveMapPanelVisible = true
                                bridge.setSetting("WorldMapDisplayed", true)
                            }
                        }
                    }

                    ToolTip.visible: liveMapRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Live Map")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: liveMapMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // DX Cluster toggle button
                Rectangle {
                    visible: dxClusterToolbarVisible
                    width: dxClusterToolbarVisible ? (bridge.dxCluster && bridge.dxCluster.connected ? 96 : 86) : 0
                    height: bridge.dxCluster && bridge.dxCluster.connected ? 96 : 74
                    radius: 8
                    clip: true
                    color: dxClusterPanelVisible
                           ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                           : dxcBtnMA.containsMouse
                             ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
                             : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.85)
                    border.color: dxClusterPanelVisible ? secondaryCyan
                                  : dxcBtnMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: dxClusterPanelVisible || dxcBtnMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: bridge.dxCluster && bridge.dxCluster.connected ? -8 : 0
                        spacing: 2
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📡"
                            font.pixelSize: 22
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "DX Cluster"
                            font.pixelSize: 9
                            font.bold: true
                            color: dxClusterPanelVisible ? secondaryCyan : textSecondary
                        }
                    }

                    // pallino stato connessione
                    Rectangle {
                        anchors { top: parent.top; right: parent.right; margins: 5 }
                        width: 8; height: 8; radius: 4
                        color: bridge.dxCluster && bridge.dxCluster.connected ? "#00e676" : "#ef5350"
                        visible: true
                    }

                    MouseArea {
                        id: dxcBtnMA
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: parent.height - (autoSpotRow.visible ? 20 : 0)
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                // Destro: disconnetti
                                bridge.disconnectDxCluster()
                            } else {
                                // Sinistro: mostra pannello e connetti se non connesso
                                dxClusterPanelVisible = true
                                if (bridge.dxCluster && !bridge.dxCluster.connected) {
                                    bridge.connectDxCluster(bridge.dxCluster.host, bridge.dxCluster.port)
                                }
                            }
                        }
                    }

                    Item {
                        id: autoSpotRow
                        z: 3
                        visible: Boolean(bridge.dxCluster && bridge.dxCluster.connected)
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 4
                        height: 14

                        Row {
                            id: autoSpotContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3

                            Rectangle {
                                id: autoSpotBox
                                width: 10
                                height: 10
                                radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: bridge.autoSpotEnabled ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.9) : "transparent"
                                border.color: bridge.autoSpotEnabled ? accentGreen : textSecondary
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.autoSpotEnabled ? "✓" : ""
                                    color: bgDeep
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }

                            Text {
                                text: "Auto Spot"
                                width: Math.max(0, autoSpotContent.width - autoSpotBox.width - autoSpotContent.spacing)
                                color: bridge.autoSpotEnabled ? accentGreen : textSecondary
                                font.pixelSize: 8
                                font.bold: true
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignLeft
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            z: 10
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.LeftButton
                            onClicked: {
                                bridge.autoSpotEnabled = !bridge.autoSpotEnabled
                            }
                        }
                    }

                    ToolTip.visible: dxcBtnMA.containsMouse
                    ToolTip.text: qsTr("DX Cluster\nLeft-click: open and connect\nRight-click: disconnect")
                    ToolTip.delay: 400

                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                // Decode filters bypass toggle
                Rectangle {
                    width: 86
                    height: 74
                    radius: 8
                    color: bridge.filtersBypassed
                           ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.26)
                           : bypassFiltersMA.containsMouse
                             ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.14)
                             : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.85)
                    border.color: bridge.filtersBypassed ? accentOrange
                                  : bypassFiltersMA.containsMouse ? accentOrange : glassBorder
                    border.width: bridge.filtersBypassed || bypassFiltersMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "⌀"
                            font.pixelSize: 24
                            font.bold: true
                            color: bridge.filtersBypassed ? accentOrange : textSecondary
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Bypass"
                            font.pixelSize: 9
                            font.bold: true
                            color: bridge.filtersBypassed ? accentOrange : textSecondary
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Filters"
                            font.pixelSize: 9
                            font.bold: true
                            color: bridge.filtersBypassed ? accentOrange : textSecondary
                        }
                    }

                    MouseArea {
                        id: bypassFiltersMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bridge.filtersBypassed = !bridge.filtersBypassed
                    }

                    ToolTip.visible: bypassFiltersMA.containsMouse
                    ToolTip.text: bridge.filtersBypassed
                                  ? qsTr("Disable filter bypass")
                                  : qsTr("Bypass CQ/My Call and setup decode filters")
                    ToolTip.delay: 500

                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                // Macro Dialog restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: macroDialogMinimized
                    color: macroRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: macroRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: macroRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "⌨️"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Macro"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: macroRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            macroDialogMinimized = false
                            openMacroDialog()
                        }
                    }

                    ToolTip.visible: macroRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Macro Configuration")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: macroDialogMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // Rig Control restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: rigControlMinimized
                    color: rigRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: rigRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: rigRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📻"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "CAT"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: rigRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            rigControlMinimized = false
                            openSettingsTab(1)
                        }
                    }

                    ToolTip.visible: rigRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Rig Control")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: rigControlMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

	                // Full Spectrum restore button
	                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: period1Minimized
                    color: p1RestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: p1RestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: p1RestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

	                        Text {
	                            anchors.horizontalCenter: parent.horizontalCenter
	                            text: "FS"
	                            font.pixelSize: 20
	                            font.bold: true
	                            color: bridge.themeManager.successColor
	                        }

	                        Text {
	                            anchors.horizontalCenter: parent.horizontalCenter
	                            text: "Full Spectrum"
	                            font.pixelSize: 10
	                            font.bold: true
	                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: p1RestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            mainWindow.restoreFloatingWindow(period1FloatingWindow, "period1Minimized")
                        }
                    }

	                    ToolTip.visible: p1RestoreMA.containsMouse
	                    ToolTip.text: qsTr("Restore Full Spectrum")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: period1Minimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // Period 2 restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: period2Minimized
                    color: p2RestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: p2RestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: p2RestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "2️⃣"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Period 2"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: p2RestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            period2Minimized = false
                            period2FloatingWindow.show()
                        }
                    }

                    ToolTip.visible: p2RestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Period 2")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: period2Minimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // RX Frequency restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: rxFreqMinimized
                    color: rxRestoreMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: rxRestoreMA.containsMouse ? secondaryCyan : glassBorder
                    border.width: rxRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📡"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Signal RX"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                        }
                    }

                    MouseArea {
                        id: rxRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            mainWindow.restoreFloatingWindow(rxFreqFloatingWindow, "rxFreqMinimized")
                        }
                    }

                    ToolTip.visible: rxRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Signal RX")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: rxFreqMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // TX Panel restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: txPanelMinimized
                    color: txRestoreMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: txRestoreMA.containsMouse ? bridge.themeManager.ledRed : glassBorder
                    border.width: txRestoreMA.containsMouse ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📻"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "TX Panel"
                            font.pixelSize: 10
                            font.bold: true
                            color: bridge.themeManager.ledRed
                        }
                    }

                    MouseArea {
                        id: txRestoreMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            txPanelMinimized = false
                            txPanelFloatingWindow.show()
                        }
                    }

                    ToolTip.visible: txRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore TX Panel")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: txPanelMinimized && bridge && bridge.uiQuality !== "Low"
                        loops: Animation.Infinite
                        OpacityAnimator { to: 0.7; duration: 800 }
                        OpacityAnimator { to: 1.0; duration: 800 }
                    }
                }

                // PSK Reporter Search
                Rectangle {
                    visible: pskReporterToolbarVisible
                    width: pskReporterToolbarVisible ? 160 : 0
                    height: 74
                    radius: 8
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: pskSearchInput.activeFocus ? secondaryCyan : glassBorder
                    border.width: pskSearchInput.activeFocus ? 2 : 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        Text {
                            text: "PSK Reporter"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4

                            Rectangle {
                                width: 110
                                height: 28
                                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                                border.color: pskSearchInput.activeFocus ? secondaryCyan : glassBorder
                                radius: 4

                                TextField {
                                    id: pskSearchInput
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    placeholderText: (activeFocus || text.length > 0) ? "" : "Callsign..."
                                    font.pixelSize: 11
                                    font.capitalization: Font.AllUppercase
                                    font.family: "Monospace"
                                    color: textPrimary
                                    placeholderTextColor: textSecondary
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 8
                                    rightPadding: 8
                                    topPadding: 0
                                    bottomPadding: 0
                                    background: Rectangle { color: "transparent" }
                                    onAccepted: {
                                        if (text.trim().length > 0) {
                                            bridge.searchPskReporter(text.trim().toUpperCase())
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                width: 28
                                height: 28
                                radius: 4
                                color: pskSearchBtn.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
                                border.color: secondaryCyan

                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.pskSearching ? "\u23F3" : "\uD83D\uDD0D"
                                    font.pixelSize: 13
                                }

                                MouseArea {
                                    id: pskSearchBtn
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (pskSearchInput.text.trim().length > 0) {
                                            bridge.searchPskReporter(pskSearchInput.text.trim().toUpperCase())
                                        }
                                    }
                                }

                                ToolTip.visible: pskSearchBtn.containsMouse
                                ToolTip.text: bridge.pskSearchFound ? "\u2713 Online" : "Search PSK Reporter"
                                ToolTip.delay: 500
                            }
                        }

                        // Status indicator
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: bridge.pskSearching ? "Searching..." : (bridge.pskSearchCallsign.length > 0 ? (bridge.pskSearchFound ? bridge.pskSearchCallsign + " ONLINE" : bridge.pskSearchCallsign + " offline") : "")
                            font.pixelSize: 9
                            font.bold: bridge.pskSearchFound
                            color: bridge.pskSearchFound ? accentGreen : textSecondary
                        }
                    }
                }

                // Font Scale Aa+/Aa- buttons
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: glassBorder
                    border.width: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 2

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Aa " + Math.round(fs * 100) + "%"
                            font.pixelSize: 9
                            font.bold: true
                            color: secondaryCyan
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4

                            Rectangle {
                                width: 30; height: 24; radius: 4
                                color: fontMinusMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                                border.color: fontMinusMA.containsMouse ? secondaryCyan : glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "A-"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: textPrimary
                                }

                                MouseArea {
                                    id: fontMinusMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.decreaseFontScale()
                                }
                            }

                            Rectangle {
                                width: 30; height: 24; radius: 4
                                color: fontPlusMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                                border.color: fontPlusMA.containsMouse ? secondaryCyan : glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "A+"
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: textPrimary
                                }

                                MouseArea {
                                    id: fontPlusMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.increaseFontScale()
                                }
                            }
                        }

                        // Reset button
                        Rectangle {
                            width: 64; height: 16; radius: 3
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: fontResetMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "Reset"
                                font.pixelSize: 8
                                color: textSecondary
                            }

                            MouseArea {
                                id: fontResetMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.setFontScale(1.0)
                            }
                        }
                    }
                }


            } // End headerFlow
        } // End Header Bar Rectangle

        // Content area for dockable panels (wrapped in Flickable for vertical scroll
        // when the window is shorter than the minimum usable layout height)
        Flickable {
            id: contentScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            Layout.topMargin: 0
            contentWidth: width
            contentHeight: Math.max(height, 540)
            clip: true
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: contentScroll.contentHeight > contentScroll.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
            }

        Item {
            id: contentArea
            width: contentScroll.width
            height: contentScroll.contentHeight

            // Main vertical split: Waterfall on top, Decode panels below
            SplitView {
                id: mainVerticalSplit
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: contentArea.height - txPanelContainer.height - 12
                orientation: Qt.Vertical

                // Magnetic snap points for waterfall height
                property var snapPoints: [150, 200, 250, 300, 350, 400]
                property int snapThreshold: 20  // Pixels to trigger snap

                // Vertical drag handle with magnetic snap indicator
                handle: Rectangle {
                    id: splitHandle
                    implicitWidth: 6
                    implicitHeight: 6
                    color: SplitHandle.hovered || SplitHandle.pressed ? "#00e6e6" : "#505070"
                    Behavior on color { ColorAnimation { duration: 150 } }

                    // Magnetic snap indicator (glows when near snap point)
                    property bool nearSnapPoint: {
                        var h = waterfallPanel.height
                        for (var i = 0; i < mainVerticalSplit.snapPoints.length; i++) {
                            if (Math.abs(h - mainVerticalSplit.snapPoints[i]) < mainVerticalSplit.snapThreshold) {
                                return true
                            }
                        }
                        return false
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 50
                        height: 3
                        radius: 1
                        color: parent.nearSnapPoint ? "#00ffff" : parent.color

                        // Glow effect when near snap
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: 3
                            color: "transparent"
                            border.color: parent.parent.nearSnapPoint ? "#00ffff" : "transparent"
                            border.width: 1
                            opacity: 0.5
                        }
                    }

                    SplitHandle.onPressedChanged: {
                        if (!SplitHandle.pressed) {
                            // Find nearest snap point
                            var h = waterfallPanel.height
                            var nearestSnap = h
                            var minDist = mainVerticalSplit.snapThreshold

                            for (var i = 0; i < mainVerticalSplit.snapPoints.length; i++) {
                                var dist = Math.abs(h - mainVerticalSplit.snapPoints[i])
                                if (dist < minDist) {
                                    minDist = dist
                                    nearestSnap = mainVerticalSplit.snapPoints[i]
                                }
                            }

                            // Apply snap with animation
                            if (nearestSnap !== h) {
                                snapAnimation.to = nearestSnap
                                snapAnimation.start()
                            }
                        }
                    }
                }

                // Snap animation
                NumberAnimation {
                    id: snapAnimation
                    target: waterfallPanel
                    property: "SplitView.preferredHeight"
                    duration: 150
                    easing.type: Easing.OutQuad
                }

                // ========== TOP: Waterfall Panel (embedded or placeholder) ==========
                Rectangle {
                    id: waterfallPanel
                    SplitView.preferredHeight: waterfallDetached ? 40 : mainWindow.waterfallPanelHeight
                    SplitView.minimumHeight: waterfallDetached ? 40 : 260
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                    radius: 8
                    border.color: isDockHighlighted ? secondaryCyan : glassBorder
                    border.width: isDockHighlighted ? 3 : 1

                    // Dock zone detection
                    property bool isDockHighlighted: false
                    property rect globalDockZone: Qt.rect(0, 0, 0, 0)

                    // Update dock zone position
                    function updateDockZone() {
                        var globalPos = waterfallPanel.mapToGlobal(0, 0)
                        globalDockZone = Qt.rect(globalPos.x, globalPos.y, waterfallPanel.width, waterfallPanel.height)
                    }

                    Component.onCompleted: updateDockZone()
                    onWidthChanged: updateDockZone()
                    onHeightChanged: {
                        updateDockZone()
                        if (!waterfallDetached && height > 40) {
                            mainWindow.waterfallPanelHeight = height
                            bridge.uiWaterfallHeight = height
                            mainWindow.scheduleSave()
                        }
                    }

                    // Placeholder when detached - magnetic dock zone
                    Rectangle {
                        anchors.fill: parent
                        visible: waterfallDetached
                        color: waterfallPanel.isDockHighlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                        radius: 8
                        border.color: waterfallPanel.isDockHighlighted ? secondaryCyan : glassBorder
                        border.width: waterfallPanel.isDockHighlighted ? 3 : 1

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Column {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
	                            text: waterfallPanel.isDockHighlighted ? "Dock Waterfall" : "Waterfall detached"
                                color: waterfallPanel.isDockHighlighted ? secondaryCyan : textSecondary
                                font.pixelSize: waterfallPanel.isDockHighlighted ? 16 : 12
                                font.bold: waterfallPanel.isDockHighlighted
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
	                                text: "Use Dock to reattach it"
                                color: textSecondary
                                font.pixelSize: 10
                                visible: !waterfallPanel.isDockHighlighted
                            }
                        }

                        // Pulsing border when highlighted
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            radius: 8
                            border.color: secondaryCyan
                            border.width: 4
                            visible: waterfallPanel.isDockHighlighted
                            opacity: 0.8

                            SequentialAnimation on opacity {
                                running: waterfallPanel.isDockHighlighted && bridge && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.4; duration: 300 }
                                OpacityAnimator { to: 1.0; duration: 300 }
                            }
                        }
                    }

                    // Embedded waterfall content
                    Rectangle {
                        anchors.fill: parent
                        visible: !waterfallDetached
                        color: "transparent"


                        Loader {
                            id: waterfallEmbeddedLoader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            visible: !waterfallDetached
                            active: !waterfallDetached
                            // 1.0.175 — Carica off-thread come gia' fa il
                            // detached (Loader asynchronous:true a r.8304),
                            // per evitare stallo del main thread sul mount
                            // del PanadapterItem QQuickItem.
                            asynchronous: true
                            sourceComponent: waterfallEmbeddedComponent
                        }

                        Component {
                            id: waterfallEmbeddedComponent

                            Waterfall {
                                id: waterfallDisplayEmbedded
                                anchors.fill: parent
                                visible: !waterfallDetached
                                showControls: true
                                minFreq: 0
                                maxFreq: 3200
                                spectrumHeight: 150
                                // 1.0.178 — Rimosso layer.enabled FBO (1.0.175):
                                // su PC modesti aggiungeva overhead GPU + sync
                                // barriers → UI stall > 600ms → cpuPressure
                                // → FT8 early predecode skip → no decode.
                                // Loader async (sopra) + label throttle 4Hz
                                // (Waterfall.qml) restano in place.

                                onFrequencySelected: function(freq) {
                                    bridge.rxFrequency = freq      // tasto destro = RX
                                }
                                onTxFrequencySelected: function(freq) {
                                    bridge.txFrequency = freq      // tasto sinistro = TX
                                }
                            }
                        }

                        // Etichetta "Waterfall" integrata come overlay (non occupa spazio)
                        Text {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.topMargin: 6
                            anchors.leftMargin: 10
                            text: "Waterfall"
                            font.pixelSize: 10
                            font.bold: true
                            color: secondaryCyan
                            opacity: 0.55
                            z: 5
                        }

                        // Pallino cyan + bottone Pop come overlay top-right
                        Row {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 5
                            anchors.rightMargin: 8
                            spacing: 6
                            z: 5

                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: secondaryCyan
                                anchors.verticalCenter: parent.verticalCenter
                                opacity: 0.7
                            }

                            Rectangle {
                                width: 30; height: 14; radius: 3
                                color: waterfallPopMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.55)
                                border.color: waterfallPopMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.45)
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "Pop"
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: waterfallPopMA.containsMouse ? secondaryCyan : textSecondary
                                }

                                MouseArea {
                                    id: waterfallPopMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mainWindow.detachWaterfallPanel()
                                }

                                ToolTip.visible: waterfallPopMA.containsMouse
                                ToolTip.text: "Pop out Waterfall"
                                ToolTip.delay: 500
                            }
                        }
                    }
                }

                // ========== BOTTOM: Decode Panels (Period1 | Period2 | RX Freq) ==========
                Rectangle {
                    id: decodePanel
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 60
                    color: "transparent"

                    // Current active period tracking
                    property bool isCurrentPeriodEven: true
                    property int currentSecond: 0
                    property real periodLength: bridge.mode === "FT2" ? 3.75 : (bridge.mode === "FT4" ? 7.5 : (bridge.mode === "WSPR" ? 120 : 15))

                    // IU8LMC: Reactive property for all decodes (Band Activity)
                    property bool showTxMessagesInRx: mainWindow.showTxMessagesInRx
                    property bool hideTelemetryOnlyDecodes: Qt.platform.os === "windows"
                    property var allDecodes: visibleDecodeEntries(bridge.decodeList)
                    property var rxDecodes: currentRxDecodes()
                    property var clearedRxDecodeKeys: ({})
                    property int decodeListVersion: 0
                    property int rxDecodeListVersion: 0
                    property int lastSyncCount: 0
                    property real currentPeriodIndex: -1
                    property int currentPeriodDecodeCount: 0
                    property int heldPeriodDecodeCount: 0
                    property real heldPeriodDecodeCountUntilIndex: -1

                    function currentPeriodMs() {
                        return Math.max(1, Math.round(decodePanel.periodLength * 1000))
                    }

                    function displayedDecodeCount() {
                        if (decodePanel.currentPeriodDecodeCount > 0)
                            return decodePanel.currentPeriodDecodeCount
                        if (decodePanel.heldPeriodDecodeCount > 0
                            && decodePanel.currentPeriodIndex <= decodePanel.heldPeriodDecodeCountUntilIndex)
                            return decodePanel.heldPeriodDecodeCount
                        return 0
                    }

                    function fullSpectrumModelCount() {
                        void(decodePanel.decodeListVersion)
                        if (bridge && bridge.bandActivityModel)
                            return bridge.bandActivityModel.count()
                        return decodePanel.allDecodes ? decodePanel.allDecodes.length : 0
                    }

                    function signalRxModelCount() {
                        void(decodePanel.rxDecodeListVersion)
                        if (bridge && bridge.rxDecodeModel)
                            return bridge.rxDecodeModel.count()
                        return decodePanel.rxDecodes ? decodePanel.rxDecodes.length : 0
                    }

                    function updatePeriodState() {
                        var nowMs = Date.now()
                        var periodMs = currentPeriodMs()
                        var periodIndex = Math.floor(nowMs / periodMs)
                        decodePanel.currentSecond = Math.floor((nowMs % 60000) / 1000)
                        decodePanel.isCurrentPeriodEven = (periodIndex % 2) === 0
                        if (decodePanel.currentPeriodIndex !== periodIndex) {
                            if (decodePanel.currentPeriodDecodeCount > 0) {
                                decodePanel.heldPeriodDecodeCount = decodePanel.currentPeriodDecodeCount
                                decodePanel.heldPeriodDecodeCountUntilIndex = periodIndex
                            } else if (decodePanel.heldPeriodDecodeCountUntilIndex < periodIndex) {
                                decodePanel.heldPeriodDecodeCount = 0
                                decodePanel.heldPeriodDecodeCountUntilIndex = -1
                            }
                            decodePanel.currentPeriodIndex = periodIndex
                            decodePanel.currentPeriodDecodeCount = 0
                            decodePanel.decodeListVersion++
                        }
                    }

                    function updateCurrentPeriodDecodeCount(src) {
                        var newCount = src ? src.length : 0
                        if (newCount >= decodePanel.lastSyncCount) {
                            decodePanel.currentPeriodDecodeCount += newCount - decodePanel.lastSyncCount
                        } else {
                            decodePanel.currentPeriodDecodeCount = 0
                            decodePanel.heldPeriodDecodeCount = 0
                            decodePanel.heldPeriodDecodeCountUntilIndex = -1
                        }
                        decodePanel.lastSyncCount = newCount
                    }

	                    Component.onCompleted: {
	                        updatePeriodState()
	                        lastSyncCount = decodePanel.visibleDecodeEntries(bridge.decodeList).length
	                    }

	                    function refreshRxDecodeModel(resetCleared) {
	                        if (resetCleared)
	                            decodePanel.clearedRxDecodeKeys = ({})
	                        decodePanel.rxDecodes = decodePanel.currentRxDecodes()
	                        decodePanel.rxDecodeListVersion++
	                        if (rxFrequencyList)
	                            rxFrequencyList.forceTailFollow()
	                        if (rxFrequencyFloatingList)
	                            rxFrequencyFloatingList.forceTailFollow()
	                    }

	                    function rxEntryKey(item) {
	                        var key = [
	                            item.utc || item.time || "",
	                            item.freq || "",
	                            item.dt || "",
	                            item.snr || "",
	                            item.message || "",
	                            item.isTx === true ? "tx" : "rx"
	                        ].join("|")
	                        if (String(item.utc || item.time || "").trim().length === 0 && Number(item.timestamp || 0) > 0)
	                            key += "|ts=" + String(item.timestamp)
	                        return key
	                    }

	                    function clearSignalRxDecodes() {
	                        var hidden = {}
	                        for (var i = 0; i < decodePanel.rxDecodes.length; ++i) {
	                            var item = decodePanel.rxDecodes[i]
	                            if (!item || item.isSeparator === true)
	                                continue
	                            hidden[decodePanel.rxEntryKey(item)] = true
	                        }
	                        decodePanel.clearedRxDecodeKeys = hidden
	                        decodePanel.rxDecodes = []
	                        decodePanel.rxDecodeListVersion++
	                        bridge.clearRxDecodes()
	                        if (rxFrequencyList)
	                            rxFrequencyList.forceTailFollow()
	                        if (rxFrequencyFloatingList)
	                            rxFrequencyFloatingList.forceTailFollow()
	                    }

	                    // Update decode list incrementalmente (solo nuovi elementi)
	                    Connections {
                        target: bridge
                        function onDecodeListChanged() {
                            decodePanel.decodeListVersion++
                            var src = decodePanel.visibleDecodeEntries(bridge.decodeList)
                            decodePanel.updateCurrentPeriodDecodeCount(src)
                            decodePanel.allDecodes = src
                            decodePanel.rxDecodes = decodePanel.currentRxDecodes()
                            decodePanel.rxDecodeListVersion++
                            // 1.0.227 — forceTailFollow solo sulla ListView attiva
                            // (embedded VS floating in base a period1Detached).
                            // Pre-1.0.227 chiamava SEMPRE entrambe: ognuna schedulava
                            // Qt.callLater + NumberAnimation main thread anche se
                            // invisibile = main saturation = freeze Full Spectrum.
                            if (period1Detached) {
                                if (period1FloatingList)
                                    period1FloatingList.forceTailFollow()
                            } else {
                                if (evenPeriodList)
                                    evenPeriodList.forceTailFollow()
                            }
                            if (rxFrequencyList)
                                rxFrequencyList.forceTailFollow()
                            if (rxFrequencyFloatingList)
                                rxFrequencyFloatingList.forceTailFollow()
                        }
                        function onRxDecodeListChanged() {
                            decodePanel.rxDecodeListVersion++
                            decodePanel.rxDecodes = decodePanel.currentRxDecodes()
                            if (rxFrequencyList)
                                rxFrequencyList.forceTailFollow()
	                            if (rxFrequencyFloatingList)
	                                rxFrequencyFloatingList.forceTailFollow()
	                        }
	                        function onDxCallChanged() {
	                            decodePanel.refreshRxDecodeModel(true)
	                        }
	                        function onRxFrequencyChanged() {
	                            decodePanel.refreshRxDecodeModel(true)
	                        }
	                    }

                    onShowTxMessagesInRxChanged: {
                        decodePanel.rxDecodes = currentRxDecodes()
                        decodePanel.rxDecodeListVersion++
                        if (rxFrequencyList)
                            rxFrequencyList.forceTailFollow()
                        if (rxFrequencyFloatingList)
                            rxFrequencyFloatingList.forceTailFollow()
                    }

                    Timer {
                        id: periodTimer
                        interval: 200  // Update 5 times per second for smooth indicator
                        running: true
                        repeat: true
                        onTriggered: {
                            decodePanel.updatePeriodState()
                        }
                    }

                    // LED Status Timer - adapts to mode period
                    Timer {
                        id: ledStatusTimer
                        interval: 500  // Update LED status every 500ms
                        running: true
                        repeat: true
                        property int resetCounter: 0
                        onTriggered: {
                            // Refresh LED status from backend
                            bridge.refreshLedStatus()

                            // Reset LED status at end of each period
                            var pLen = decodePanel.periodLength
                            var secInPeriod = decodePanel.currentSecond % pLen
                            if (secInPeriod === pLen - 1) {
                                // At end of decode period, reset for next cycle
                                bridge.resetLedStatus()
                            }
                        }
                    }

	                    // Shannon RX frequency filter: ±200Hz OR messaggi per noi
	                    property int rxBandwidth: 200
		                    function isAtRxFrequency(freq, md) {
		                        var f = parseInt(freq)
		                        var inWindow = Math.abs(f - bridge.rxFrequency) <= rxBandwidth
		                        var relevant = !!(md && ((md.isMyCall === true) || (md.isTx === true)))
		                        return !!(inWindow || relevant)
		                    }

		                    function decodeEntryBold(md) {
		                        return !!(md && ((md.isTx === true) ||
		                                        (md.isCQ === true) ||
		                                        (md.isMyCall === true) ||
		                                        (md.dxIsNewCountry === true) ||
		                                        (md.dxIsMostWanted === true)))
		                    }

		                    function decodeEntryStrikeout(md) {
		                        return !!(md && (md.isB4 === true) && (bridge.b4Strikethrough === true))
		                    }

	                    function messageContainsCallBase(message, base) {
	                        var wanted = String(base || "").trim().toUpperCase()
	                        if (wanted.length === 0)
	                            return false
	                        var parts = String(message || "").split(/\s+/)
	                        for (var i = 0; i < parts.length; ++i) {
	                            if (mainWindow.callsignBase(parts[i]) === wanted)
	                                return true
	                        }
	                        return false
	                    }

	                    function shouldDisplayDecodeEntry(item) {
	                        if (!item)
	                            return false
	                        // 1.0.226 — ghost filter via bridge (single source of truth).
	                        // Pre-1.0.226 filtrava SOLO telemetry-only, ignorando ghost
	                        // (myCall + partner sintatticamente invalido / corrupted token).
	                        if (bridge && bridge.entryLooksLikeGhost && bridge.entryLooksLikeGhost(item))
	                            return false
	                        if (!decodePanel.hideTelemetryOnlyDecodes)
	                            return true
	                        return !mainWindow.isTelemetryOnlyDecodeMessage(item.displayMessage || item.message)
	                    }

	                    function visibleDecodeEntries(list) {
	                        var filtered = []
	                        if (!list)
	                            return filtered
	                        for (var i = 0; i < list.length; ++i) {
	                            var item = list[i]
	                            if (shouldDisplayDecodeEntry(item))
	                                filtered.push(item)
	                        }
	                        return injectSeparatorsJS(filtered)
	                    }

	                    // Fallback JS per ambienti senza i model C++ nativi.
	                    function injectSeparatorsJS(filtered) {
	                        if (!filtered || filtered.length <= 1)
	                            return filtered
	                        var enabled = true
	                        if (bridge && bridge.decodeShowPeriodSeparator)
	                            enabled = bridge.decodeShowPeriodSeparator()
	                        if (!enabled)
	                            return filtered
	                        var withSep = []
	                        var prevPeriod = ""
	                        var prevTs = 0
	                        for (var i = 0; i < filtered.length; ++i) {
	                            var it = filtered[i]
	                            var t = String(it.time || it.utc || "")
	                            var ts = Number(it.timestamp || 0)
	                            var newPeriod = false
	                            if (t.length > 0) {
	                                if (prevPeriod && t !== prevPeriod) newPeriod = true
	                            } else {
	                                if (prevTs > 0 && ts > 0 && (ts - prevTs) > 1500) newPeriod = true
	                            }
	                            if (newPeriod)
	                                withSep.push({ isSeparator: true, time: t, timestamp: ts })
	                            if (t.length > 0) prevPeriod = t
	                            if (ts > 0) prevTs = ts
	                            withSep.push(it)
	                        }
	                        return withSep
	                    }

	                    function currentQsoPartnerBase() {
	                        return mainWindow.callsignBase(bridge.dxCall || "")
	                    }

	                    function rxEntryBelongsToCurrentQso(item) {
	                        if (!item)
	                            return false

	                        var activeBase = currentQsoPartnerBase()
	                        if (item.isTx === true)
	                            return true

	                        var myBase = mainWindow.callsignBase(bridge.callsign || "")
	                        var message = item.message || ""
	                        var myMatch = item.isMyCall === true
	                            || messageContainsCallBase(message, myBase)
	                        if (myMatch)
	                            return true

	                        if (activeBase.length === 0)
	                            return isAtRxFrequency(item.freq || "0", item)

	                        var activeMatch = messageContainsCallBase(message, activeBase)
	                            || mainWindow.callsignBase(item.fromCall || "") === activeBase
	                            || mainWindow.callsignBase(item.dxCallsign || "") === activeBase
	                        return activeMatch && myMatch
	                    }

	                    function rxSortSeconds(item) {
	                        var digits = String((item && (item.utc || item.time)) || "").replace(/[^0-9]/g, "")
	                        if (digits.length >= 6)
	                            digits = digits.substring(0, 6)
	                        else if (digits.length === 4)
	                            digits = digits + "00"
	                        else
	                            return -1

	                        var hh = parseInt(digits.substring(0, 2))
	                        var mm = parseInt(digits.substring(2, 4))
	                        var ss = parseInt(digits.substring(4, 6))
	                        if (isNaN(hh) || isNaN(mm) || isNaN(ss)
	                                || hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59)
	                            return -1
	                        return hh * 3600 + mm * 60 + ss
	                    }

	                    function rxSortKey(item) {
	                        var seconds = rxSortSeconds(item)
	                        if (seconds < 0)
	                            return 9007199254740991
	                        var now = new Date()
	                        var nowSeconds = now.getUTCHours() * 3600 + now.getUTCMinutes() * 60 + now.getUTCSeconds()
	                        var delta = seconds - nowSeconds
	                        if (delta > 60)
	                            seconds -= 86400
	                        else if (delta < -86340)
	                            seconds += 86400
	                        return seconds * 1000
	                    }

	                    function sortedRxDecodes(items) {
	                        var indexed = []
	                        for (var i = 0; i < items.length; ++i)
	                            indexed.push({ item: items[i], order: i, key: rxSortKey(items[i]) })
	                        indexed.sort(function(a, b) {
	                            if (a.key !== b.key)
	                                return a.key - b.key
	                            return a.order - b.order
	                        })
	                        var out = []
	                        for (var j = 0; j < indexed.length; ++j)
	                            out.push(indexed[j].item)
	                        return out
	                    }

	                    function currentRxDecodes() {
	                        var merged = []
	                        var seen = {}
	                        function appendIfNeeded(item, allowTx) {
	                            if (!item)
	                                return
	                            if (!allowTx && item.isTx === true)
	                                return
	                            if (!mainWindow.showTxMessagesInRx && item.isTx)
	                                return
	                            if (!shouldDisplayDecodeEntry(item))
	                                return
	                            if (!rxEntryBelongsToCurrentQso(item))
	                                return
	                            var key = decodePanel.rxEntryKey(item)
	                            if (decodePanel.clearedRxDecodeKeys[key])
	                                return
	                            if (seen[key])
	                                return
	                            seen[key] = true
	                            merged.push(item)
	                        }
	                        if (bridge.rxDecodeList) {
	                            for (var j = 0; j < bridge.rxDecodeList.length; j++) {
                                appendIfNeeded(bridge.rxDecodeList[j], true)
	                            }
                        }
                        return injectSeparatorsJS(sortedRxDecodes(merged))
                    }

                    function formatUtcForDisplay(timeStr) {
                        var digits = String(timeStr || "").replace(/[^0-9]/g, "")
                        if (digits.length >= 6)
                            return digits.substring(0, 2) + ":" + digits.substring(2, 4) + ":" + digits.substring(4, 6)
                        if (digits.length === 4)
                            return digits.substring(0, 2) + ":" + digits.substring(2, 4)
                        return timeStr || ""
                    }

                    function distanceText(modelData) {
                        return mainWindow.distanceText(modelData)
                    }

                    // Period filtering function - adapts to mode (FT4=7.5s, FT8=15s)
                    function isEvenPeriod(timeStr) {
                        // Time format: "HH:MM:SS" or "HHMMSS"
                        var seconds = 0
                        if (timeStr.indexOf(":") >= 0) {
                            var parts = timeStr.split(":")
                            if (parts.length >= 3) {
                                seconds = parseInt(parts[2])
                            }
                        } else if (timeStr.length >= 6) {
                            seconds = parseInt(timeStr.substring(4, 6))
                        }
                        var pLen = decodePanel.periodLength
                        var periodIndex = Math.floor(seconds / pLen)
                        return (periodIndex % 2) === 0
                    }

                    function isOddPeriod(timeStr) {
                        return !isEvenPeriod(timeStr)
                    }

                    // Filter decodes by period
                    function getEvenPeriodDecodes() {
                        var filtered = []
                        for (var i = 0; i < decodePanel.allDecodes.length; i++) {
                            var d = decodePanel.allDecodes[i]
                            if (!d.isTx && isEvenPeriod(d.time)) {
                                filtered.push(d)
                            }
                        }
                        return filtered
                    }

                    function getOddPeriodDecodes() {
                        var filtered = []
                        for (var i = 0; i < decodePanel.allDecodes.length; i++) {
                            var d = decodePanel.allDecodes[i]
                            if (!d.isTx && isOddPeriod(d.time)) {
                                filtered.push(d)
                            }
                        }
                        return filtered
                    }

                    // Handle double-click on decode item
                    function handleDecodeDoubleClick(modelData) {
                        if (!modelData || !modelData.message) return
                        // Delegate entirely to Decodium native handler (HvTxW::DecListTextAll)
                        // It handles: DX setup, message generation, TX button selection, auto-TX
                        bridge.processDecodeDoubleClick(
                            modelData.message || "",
                            modelData.time || "",
                            modelData.db || "",
                            parseInt(modelData.freq || "0")
                        )
                    }

                    // ========== PERIOD TIMING BAR ==========
                    Rectangle {
                        id: timingBar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        anchors.topMargin: 4
                        height: 28
                        radius: 4
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: timingBar.isTxPhase ? bridge.themeManager.ledRed : Qt.rgba(76/255, 175/255, 80/255, 0.55)
                        border.width: 1

                        property real periodLen: decodePanel.periodLength
                        property real txDuration: bridge.mode === "FT2" ? 2.87 : (bridge.mode === "FT4" ? 5.04 : (bridge.mode === "WSPR" ? 110.6 : 12.64))
                        property real progress: 0.0
                        property real secInPeriod: 0.0
                        property bool isTxPhase: !!(bridge && bridge.transmitting)
                        property bool isEvenPeriod: true
                        property string periodLabel: isTxPhase ? "TX" : "RX"

                        Timer {
                            interval: 50
                            running: true
                            repeat: true
                            onTriggered: {
                                var now = new Date()
                                var totalMs = (now.getUTCHours() * 3600 + now.getUTCMinutes() * 60 + now.getUTCSeconds()) * 1000 + now.getUTCMilliseconds()
                                var periodMs = timingBar.periodLen * 1000
                                var elapsed = totalMs % periodMs
                                timingBar.secInPeriod = elapsed / 1000.0
                                timingBar.progress = elapsed / periodMs
                                var periodIndex = Math.floor(totalMs / periodMs)
                                timingBar.isEvenPeriod = (periodIndex % 2) === 0
                            }
                        }

                        // Background track
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 60
                            height: 8
                            radius: 4
                            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)

                            // TX zone marker
                            Rectangle {
                                visible: timingBar.isTxPhase
                                width: parent.width * (timingBar.txDuration / timingBar.periodLen)
                                height: parent.height
                                radius: 4
                                color: Qt.rgba(244/255, 67/255, 54/255, 0.15)
                            }

                            // Progress fill
                            Rectangle {
                                width: parent.width * timingBar.progress
                                height: parent.height
                                radius: 4
                                color: timingBar.isTxPhase ? bridge.themeManager.ledRed : accentGreen
                                Behavior on width { NumberAnimation { duration: 50 } }
                                Behavior on color { ColorAnimation { duration: 200 } }
                            }

                            // Playhead marker
                            Rectangle {
                                x: parent.width * timingBar.progress - 2
                                y: -2
                                width: 4
                                height: parent.height + 4
                                radius: 2
                                color: textPrimary
                                opacity: 0.9
                            }

                            // TX/RX boundary marker
                            Rectangle {
                                visible: timingBar.isTxPhase
                                x: parent.width * (timingBar.txDuration / timingBar.periodLen) - 1
                                y: -4
                                width: 2
                                height: parent.height + 8
                                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            }
                        }

                        // Period label (left)
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: timingBar.periodLabel
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Monospace"
                            color: timingBar.isTxPhase ? bridge.themeManager.ledRed : bridge.themeManager.successColor
                        }

                        // Mode + phase label (left of bar)
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 26
                            anchors.verticalCenter: parent.verticalCenter
                            visible: false
                            text: ""
                            font.pixelSize: 10
                            font.bold: true
                            font.family: "Monospace"
                            color: timingBar.isTxPhase ? bridge.themeManager.ledRed : accentGreen
                        }

                        // Time counter (right)
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: timingBar.secInPeriod.toFixed(1) + " / " + timingBar.periodLen.toFixed(1) + "s"
                            font.pixelSize: 10
                            font.family: "Monospace"
                            color: textSecondary
                        }
                    }

                    SplitView {
                        id: decodePanelsSplit
                        anchors.fill: parent
                        anchors.margins: 4
                        anchors.topMargin: 36
                        orientation: Qt.Horizontal

                        // Visible drag handles for decode panels
                        handle: Rectangle {
                            implicitWidth: 10
                            implicitHeight: 100
                            color: SplitHandle.hovered || SplitHandle.pressed ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(96/255, 96/255, 128/255, 0.3)
                            radius: 3

                            Rectangle {
                                anchors.centerIn: parent
                                width: 4
                                height: 50
                                radius: 2
                                color: SplitHandle.hovered || SplitHandle.pressed ? "#00bcd4" : "#909090"
                            }

                            // Marca il divisore come "spostato a mano" non appena l'utente
                            // inizia a trascinare — disattiva il ri-centramento automatico
                            // al resize della finestra.
                            SplitHandle.onPressedChanged: {
                                if (SplitHandle.pressed && typeof period1Panel !== "undefined")
                                    period1Panel.userDraggedSplit = true
                            }
                        }

                        // ========== LEFT: Band Activity ==========
                        Rectangle {
                            id: period1Panel
                            property int targetPanelWidth: mainWindow.savedPeriod1PanelWidth
                            SplitView.preferredWidth: targetPanelWidth
                            SplitView.minimumWidth: 360
                            readonly property bool compactColumns: width < 620
                            readonly property int utcColumnWidth: compactColumns ? 66 : 86
                            readonly property int dbColumnWidth: compactColumns ? 34 : 38
                            readonly property int dbDtGapWidth: compactColumns ? 4 : 6
                            readonly property int dtColumnWidth: compactColumns ? 42 : 48
                            readonly property int dtFreqGapWidth: compactColumns ? 6 : 8
                            readonly property int freqColumnWidth: compactColumns ? 42 : 45
                            readonly property int gapColumnWidth: compactColumns ? 8 : 12
                            readonly property int distanceColumnWidth: compactColumns ? 0 : 58
                            readonly property int dxccColumnWidth: mainWindow.showDxccInfo ? (compactColumns ? 108 : Math.min(300, Math.max(190, Math.round(width * 0.24)))) : 0
                            readonly property int azColumnWidth: mainWindow.showDxccInfo ? (compactColumns ? 42 : 52) : 0
                            readonly property int messageMinWidth: compactColumns ? 72 : 140
                            // Divisore Full Spectrum / Signal RX: 50/50 affidabile.
                            // Se `parent.width==0` al momento del callback (timing race),
                            // applyCenterSplit() si ri-schedula finché il parent non ha width.
                            // Il flag userDraggedSplit (settato su onPressedChanged del handle)
                            // disattiva il ri-centramento se l'utente ha trascinato il separatore.
                            property bool userDraggedSplit: false
                            function applyCenterSplit() {
                                if (userDraggedSplit) return
                                if (parent && parent.width > 0) {
                                    var liveMapWidth = (typeof liveMapPanelHost !== "undefined" && liveMapPanelHost.visible)
                                                     ? liveMapPanelHost.targetPanelWidth
                                                     : 0
                                    period1Panel.targetPanelWidth = Math.max(360, Math.round((parent.width - liveMapWidth) * 0.5))
                                } else {
                                    Qt.callLater(applyCenterSplit)
                                }
                            }
                            Component.onCompleted: Qt.callLater(function() {
                                if (mainWindow.decodePanelLayoutSaved)
                                    mainWindow.restoreDecodePanelWidths()
                                else
                                    applyCenterSplit()
                            })
                            onWidthChanged: {
                                if (width >= 360 && Math.abs(targetPanelWidth - width) >= 1) {
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }
                            Connections {
                                target: period1Panel.parent
                                ignoreUnknownSignals: true
                                function onWidthChanged() {
                                    if (!period1Panel.userDraggedSplit)
                                        period1Panel.applyCenterSplit()
                                }
                            }
                            color: "transparent"

                            // Placeholder when detached - magnetic dock zone
                            Rectangle {
                                anchors.fill: parent
                                visible: period1Detached
                                color: period1DockHighlighted ? Qt.rgba(76/255, 175/255, 80/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                                radius: 8
                                border.color: period1DockHighlighted ? bridge.themeManager.successColor : glassBorder
                                border.width: period1DockHighlighted ? 3 : 1

                                Behavior on color { ColorAnimation { duration: 100 } }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 6

	                                    Text {
	                                        anchors.horizontalCenter: parent.horizontalCenter
	                                        text: period1DockHighlighted ? "🧲 Rilascia qui!" : "Full Spectrum detached"
	                                        color: period1DockHighlighted ? bridge.themeManager.successColor : textSecondary
                                        font.pixelSize: period1DockHighlighted ? 16 : 12
                                        font.bold: period1DockHighlighted
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Trascina la finestra qui"
                                        color: textSecondary
                                        font.pixelSize: 10
                                        visible: !period1DockHighlighted
                                    }

                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: 82
                                        height: 24
                                        radius: 4
                                        visible: !period1DockHighlighted
                                        color: period1PlaceholderRestoreMA.containsMouse ? Qt.rgba(76/255, 175/255, 80/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.72)
                                        border.color: period1PlaceholderRestoreMA.containsMouse ? bridge.themeManager.successColor : Qt.rgba(76/255, 175/255, 80/255, 0.45)
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Restore"
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: period1PlaceholderRestoreMA.containsMouse ? bridge.themeManager.successColor : textPrimary
                                        }

                                        MouseArea {
                                            id: period1PlaceholderRestoreMA
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: mainWindow.restoreFloatingWindow(period1FloatingWindow, "period1Minimized")
                                        }
                                    }
                                }

                                // Pulsing border when highlighted
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    radius: 8
                                    border.color: bridge.themeManager.successColor
                                    border.width: 4
                                    visible: period1DockHighlighted
                                    opacity: 0.8

                                    SequentialAnimation on opacity {
                                        running: period1DockHighlighted && bridge && bridge.uiQuality !== "Low"
                                        loops: Animation.Infinite
                                        OpacityAnimator { to: 0.4; duration: 300 }
                                        OpacityAnimator { to: 1.0; duration: 300 }
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 2
                                visible: !period1Detached

                                // Header
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    color: decodePanel.isCurrentPeriodEven ? Qt.rgba(76/255, 175/255, 80/255, 0.25) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                    radius: 4
                                    border.color: decodePanel.isCurrentPeriodEven ? bridge.themeManager.successColor : "transparent"
                                    border.width: decodePanel.isCurrentPeriodEven ? 2 : 0

                                    Behavior on color { ColorAnimation { duration: 300 } }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 8

                                        // Active period indicator (pulsing)
                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: bridge.themeManager.successColor

                                            SequentialAnimation on opacity {
                                                running: decodePanel.isCurrentPeriodEven && bridge && bridge.uiQuality !== "Low"
                                                loops: Animation.Infinite
                                                OpacityAnimator { to: 0.3; duration: 500 }
                                                OpacityAnimator { to: 1.0; duration: 500 }
                                            }

                                            // Reset opacity when not active
                                            opacity: decodePanel.isCurrentPeriodEven ? 1.0 : 0.5
                                        }

                                        Text {
                                            text: "Full Spectrum"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: bridge.themeManager.successColor
                                        }

                                        // ACTIVE badge when current period
                                        Rectangle {
                                            visible: decodePanel.isCurrentPeriodEven
                                            width: 50
                                            height: 16
                                            radius: 8
                                            color: Qt.rgba(76/255, 175/255, 80/255, 0.4)
                                            border.color: bridge.themeManager.successColor

                                            Text {
                                                anchors.centerIn: parent
                                                text: "ACTIVE"
                                                font.pixelSize: 9
                                                font.bold: true
                                                color: bridge.themeManager.successColor
                                            }

                                            SequentialAnimation on opacity {
                                                running: decodePanel.isCurrentPeriodEven && bridge && bridge.uiQuality !== "Low"
                                                loops: Animation.Infinite
                                                OpacityAnimator { to: 0.6; duration: 600 }
                                                OpacityAnimator { to: 1.0; duration: 600 }
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: decodePanel.fullSpectrumModelCount() + " decodes"
                                            font.pixelSize: 10
                                            color: textSecondary
                                        }

                                        Text {
                                            text: "Clear"
                                            font.pixelSize: 10
                                            color: textSecondary
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: bridge.clearDecodes()
                                            }
                                        }

	                                        // 1.0.229 — Compact mode toggle Full Spectrum.
	                                        // Quando ON, row height passa da 26px a 14px:
	                                        // ~2x decode visibili in stessa viewport.
	                                        Rectangle {
	                                            Layout.preferredWidth: 34
	                                            Layout.preferredHeight: 18
	                                            radius: 4
	                                            color: p1CompactMA.containsMouse
	                                                ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
	                                                : (mainWindow.compactFullSpectrum
	                                                    ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
	                                                    : "transparent")
	                                            border.color: (p1CompactMA.containsMouse || mainWindow.compactFullSpectrum)
	                                                ? secondaryCyan
	                                                : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
	                                            border.width: 1
	                                            Text {
	                                                anchors.centerIn: parent
	                                                text: mainWindow.compactFullSpectrum ? "Full" : "Compact"
	                                                font.pixelSize: mainWindow.compactFullSpectrum ? 10 : 9
	                                                font.bold: true
	                                                color: (p1CompactMA.containsMouse || mainWindow.compactFullSpectrum)
	                                                    ? secondaryCyan : textSecondary
	                                            }
	                                            MouseArea {
	                                                id: p1CompactMA
	                                                anchors.fill: parent
	                                                hoverEnabled: true
	                                                cursorShape: Qt.PointingHandCursor
	                                                onClicked: mainWindow.toggleCompactFullSpectrum()
	                                            }
	                                            ToolTip.visible: p1CompactMA.containsMouse
	                                            ToolTip.text: mainWindow.compactFullSpectrum
	                                                ? qsTr("Switch to normal row height")
	                                                : qsTr("Compact rows (2x more visible decodes)")
	                                            ToolTip.delay: 500
	                                        }

	                                        // Pop button
	                                        Rectangle {
	                                            Layout.preferredWidth: 34
	                                            Layout.preferredHeight: 18
	                                            radius: 4
	                                            color: p1DetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
	                                            border.color: p1DetachMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
	                                            border.width: 1

	                                            Text {
	                                                anchors.centerIn: parent
	                                                text: "Pop"
	                                                font.pixelSize: 10
	                                                font.bold: true
	                                                color: p1DetachMA.containsMouse ? secondaryCyan : textSecondary
	                                            }

                                            MouseArea {
                                                id: p1DetachMA
	                                                anchors.fill: parent
	                                                hoverEnabled: true
	                                                cursorShape: Qt.PointingHandCursor
	                                                onClicked: mainWindow.detachFullSpectrumPanel()
	                                            }

	                                            ToolTip.visible: p1DetachMA.containsMouse
	                                            ToolTip.text: qsTr("Detach Full Spectrum")
                                            ToolTip.delay: 500
                                        }
                                    }
                                }

                                // Column headers
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 20
                                    color: Qt.rgba(76/255, 175/255, 80/255, 0.2)
                                    radius: 2

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 0

                                        Text { text: "UTC"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; Layout.preferredWidth: period1Panel.utcColumnWidth }
                                        Text { text: "dB"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dbColumnWidth }
                                        Item { Layout.preferredWidth: period1Panel.dbDtGapWidth }
                                        Text { text: "DT"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dtColumnWidth }
                                        Item { Layout.preferredWidth: period1Panel.dtFreqGapWidth }
                                        Text { text: "Freq"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.freqColumnWidth }
                                        Item { Layout.preferredWidth: period1Panel.gapColumnWidth }
                                        Text { text: "Message"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; Layout.fillWidth: true }
                                        Text { visible: period1Panel.distanceColumnWidth > 0; text: "Dist"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.distanceColumnWidth }
                                        Item {
                                            visible: mainWindow.showDxccInfo
                                            Layout.preferredWidth: period1Panel.dxccColumnWidth
                                            Layout.fillHeight: true
                                            Text {
                                                anchors.fill: parent
                                                text: "DXCC"
                                                font.family: mainWindow.decodedTextFontFamily
                                                font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs)
                                                font.bold: true
                                                color: bridge.themeManager.successColor
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                        Item {
                                            visible: mainWindow.showDxccInfo
                                            Layout.preferredWidth: period1Panel.azColumnWidth
                                            Layout.fillHeight: true
                                            Text {
                                                anchors.fill: parent
                                                text: "Az"
                                                font.family: mainWindow.decodedTextFontFamily
                                                font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs)
                                                font.bold: true
                                                color: bridge.themeManager.successColor
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }
                                }

                                // Even Period List
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                                    border.color: Qt.rgba(76/255, 175/255, 80/255, 0.3)
                                    border.width: 1
                                    radius: 4
                                    clip: true

                                    ListView {
                                        id: evenPeriodList
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        clip: true
                                        model: (bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : decodePanel.allDecodes
                                        spacing: 1
                                        // 1.0.228 — cacheBuffer 3000 → 600 (allineato a DecodeList.qml).
                                        // Pre-1.0.228 con 3000px buffer e delegate complessi (RowLayout
                                        // + 9 Text con highlight checks) si istanziavano ~115 row out-of-
                                        // viewport, ognuna ricalcolata su decodeListVersion++ -> picco
                                        // CPU che contribuiva al "effetto molla" su scrolling tail-follow.
                                        cacheBuffer: 600
                                        reuseItems: true
                                        interactive: true
                                        property bool followTail: true
                                        property bool tailFollowPending: false
	                                        property bool tailFollowQueued: false
	                                        // 1.0.231 — counter pending decodes mentre user scrolla up.
	                                        // Permette al floating button "↓ N new" di sapere quanti
	                                        // decode sono arrivati dopo la perdita di tail-follow.
	                                        property int pendingNewDecodes: 0
	                                        function isNearTail() {
	                                            return contentHeight <= height + 2
	                                                || contentY >= Math.max(0, contentHeight - height - 48)
	                                        }
                                        function updateFollowTail() {
                                            if (tailFollowPending)
                                                return
                                            followTail = isNearTail()
                                            // 1.0.231 — reset counter "↓ N new" quando torna a tail
                                            if (followTail) pendingNewDecodes = 0
                                        }
                                        function tailContentY() {
                                            return Math.max(0, contentHeight - height)
                                        }
                                        function finishTailFollow() {
                                            tailFollowPending = false
                                            followTail = isNearTail()
                                            if (followTail) pendingNewDecodes = 0
                                        }
                                        function forceTailFollow() {
    followTail = true
    tailFollowPending = true
    if (tailFollowQueued)
        return
    tailFollowQueued = true
    Qt.callLater(function() {
        tailFollowQueued = false
        if (!evenPeriodList)
            return
        var targetY = evenPeriodList.tailContentY()
        var distance = Math.abs(evenPeriodList.contentY - targetY)
        evenPeriodTailAnimation.stop()
        evenPeriodList.tailFollowPending = true
        // 1.0.228 — Assignment diretto (no NumberAnimation main-thread)
        // sempre per dock mode. L'animation con duration 180-620ms si
        // sovrapponeva ai nuovi decode -> target shift mid-animation
        // -> effetto molla + call temporaneamente fuori viewport ("scompaiono").
        // L'addDisplaced YAnimator delegate gia' fornisce smooth visual sui
        // delegate appena aggiunti; tail-follow content scroll non serve
        // animation per essere fluido. In floating mode (vedi
        // period1FloatingList) mantenuto NumberAnimation perche' UX detached.
        evenPeriodList.contentY = targetY
        evenPeriodList.finishTailFollow()
    })
}
NumberAnimation {
    id: evenPeriodTailAnimation
                                            target: evenPeriodList
                                            property: "contentY"
	                                            duration: 300
	                                            easing.type: Easing.OutCubic
	                                            onStopped: evenPeriodList.finishTailFollow()
	                                        }
	                                        Timer {
	                                            id: evenPeriodTailSettleTimer
	                                            interval: 32
	                                            repeat: false
	                                            onTriggered: {
	                                                if (evenPeriodList.followTail || evenPeriodList.tailFollowPending)
	                                                    evenPeriodList.forceTailFollow()
	                                            }
	                                        }
                                        Component.onCompleted: Qt.callLater(function() {
                                            positionViewAtEnd()
                                            updateFollowTail()
                                        })
                                        onContentYChanged: updateFollowTail()
	                                        onContentHeightChanged: {
	                                            if (period1Detached) return
	                                            if (followTail || tailFollowPending)
	                                                evenPeriodTailSettleTimer.restart()
	                                        }
	                                        onHeightChanged: {
	                                            if (period1Detached) return
	                                            if (followTail || tailFollowPending)
	                                                forceTailFollow()
	                                            else
	                                                updateFollowTail()
	                                        }
	                                        onDraggingChanged: {
	                                            if (dragging) {
	                                                followTail = false
	                                                tailFollowPending = false
	                                                evenPeriodTailAnimation.stop()
	                                            }
	                                        }
                                        // 1.0.227 — gate i callback su period1Detached. Quando la
                                        // floating window è detached questo ListView è invisibile
                                        // (ColumnLayout visible:!period1Detached) ma resta vivo
                                        // come QObject -> i suoi onCountChanged/on_VerChanged
                                        // attivano forceTailFollow + NumberAnimation main thread
                                        // anche se non si vede nulla. Pre-1.0.227 quindi 2 ListView
                                        // identiche (embedded + floating) animavano ognuna -> main
                                        // thread saturation -> Full Spectrum freeze.
                                        onCountChanged: {
                                            if (period1Detached) return
                                            // 1.0.231 — se user e' in scroll-back, no forced tail
                                            // ma incrementa counter per il floating "↓ N new" button.
                                            if (!followTail) {
                                                pendingNewDecodes++
                                                return
                                            }
                                            forceTailFollow()
                                        }
                                        property int _ver: decodePanel.decodeListVersion
                                        on_VerChanged: {
                                            if (period1Detached) return
                                            if (!followTail) return
                                            forceTailFollow()
                                        }
	                                        // 1.0.186: Animator (render thread) + gate uiQuality !== Low.
	                                        // OpacityAnimator/YAnimator non si fermano durante stall main thread,
	                                        // pattern allineato a DecodeList.qml:243-251.
	                                        add: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            OpacityAnimator { from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        addDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        moveDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        removeDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                                        }

                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                            interactive: true
                                            width: 8
                                        }

                                        delegate: Rectangle {
                                            // 1.0.155: separator meno invasivo — riga sottile, no label.
                                            readonly property bool isPeriodSeparator: !!(modelData && modelData.isSeparator === true)
                                            width: evenPeriodList.width
                                            // 1.0.229 — height adattiva via mainWindow.fullSpectrumRowHeight
                                            // (compact 14px / normal 26px). Toggle via toolbar o Ctrl+Shift+C.
                                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(mainWindow.fullSpectrumRowHeight * fs)
	                                            property var highlightFill: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightFill(modelData)
	                                            property var highlightBorder: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightBorder(modelData)
	                                            // 1.0.205 — guard !modelData per evitare TypeError flood (~46/s) durante
	                                            // model swap transients che saturava il main thread via logger sincrono.
	                                            color: !modelData ? "transparent" :
	                                                   isPeriodSeparator ? "transparent" :
	                                                   highlightFill ? highlightFill :
	                                                   modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12) :
	                                                   decodePanel.isAtRxFrequency(modelData.freq || "0", modelData) ? Qt.rgba(76/255, 175/255, 80/255, 0.2) :
	                                                   index % 2 === 0 ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                                            border.color: highlightBorder ? highlightBorder : "transparent"
                                            border.width: highlightFill ? 1 : 0
                                            radius: 2

                                            // Linea sottile discreta (1px, rosso scuro)
                                            Rectangle {
                                                visible: parent.isPeriodSeparator
                                                anchors.verticalCenter: parent.verticalCenter
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 12
                                                height: 1
                                                color: Qt.rgba(0.85, 0.25, 0.25, 0.55)
                                            }

                                            MouseArea {
                                                enabled: !parent.isPeriodSeparator
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                onClicked: function(mouse) {
                                                    if (parent.isPeriodSeparator) return
                                                    if (modelData.isTx) return
                                                    if (mouse.button === Qt.LeftButton) {
                                                        // Sinistro = imposta TX freq
                                                        if (!bridge.holdTxFreq)
                                                            bridge.txFrequency = parseInt(modelData.freq || "0")
                                                    } else if (mouse.button === Qt.RightButton) {
                                                        // Destro = imposta RX freq
                                                        bridge.rxFrequency = parseInt(modelData.freq || "0")
                                                    }
                                                }
                                                onDoubleClicked: function(mouse) {
                                                    if (parent.isPeriodSeparator) return
                                                    if (!modelData.isTx && mouse.button === Qt.LeftButton)
                                                        decodePanel.handleDecodeDoubleClick(modelData)
                                                }
                                                // IU8LMC: Show tooltip on hover
                                                onContainsMouseChanged: {
                                                    if (parent.isPeriodSeparator) { dxccTooltipVisible = false; return }
                                                    if (containsMouse) {
                                                        dxccTooltipText = getDxccTooltipText(modelData)
                                                        var pos = mapToGlobal(mouseX, mouseY)
                                                        dxccTooltipX = pos.x - mainWindow.x
                                                        dxccTooltipY = pos.y - mainWindow.y
                                                        dxccTooltipVisible = true
                                                    } else {
                                                        dxccTooltipVisible = false
                                                    }
                                                }
                                                onPositionChanged: {
                                                    if (containsMouse) {
                                                        var pos = mapToGlobal(mouseX, mouseY)
                                                        dxccTooltipX = pos.x - mainWindow.x
                                                        dxccTooltipY = pos.y - mainWindow.y
                                                    }
                                                }
                                            }

	                                            RowLayout {
	                                                visible: !parent.isPeriodSeparator
	                                                anchors.fill: parent
	                                                anchors.leftMargin: 6
	                                                anchors.rightMargin: 6
	                                                spacing: 0

                                                Text { text: modelData.formattedTime || decodePanel.formatUtcForDisplay(modelData.time); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; Layout.preferredWidth: period1Panel.utcColumnWidth }
                                                Text { text: modelData.db || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.snrColor || (modelData.isTx ? "#f1c40f" : textSecondary); font.bold: modelData.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dbColumnWidth }
                                                Item { Layout.preferredWidth: period1Panel.dbDtGapWidth }
                                                Text { text: modelData.dt || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dtColumnWidth }
                                                Item { Layout.preferredWidth: period1Panel.dtFreqGapWidth }
                                                Text { text: modelData.freq || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : decodePanel.isAtRxFrequency(modelData.freq || "0", modelData) ? bridge.themeManager.successColor : secondaryCyan; font.bold: (modelData.isTx === true) || decodePanel.isAtRxFrequency(modelData.freq || "0", modelData); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.freqColumnWidth }
                                                Item { Layout.preferredWidth: period1Panel.gapColumnWidth }
                                                Text { text: modelData.displayMessage || modelData.message || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); font.bold: decodePanel.decodeEntryBold(modelData); font.strikeout: decodePanel.decodeEntryStrikeout(modelData); color: mainWindow.fullSpectrumTextColor(modelData); Layout.fillWidth: true; Layout.minimumWidth: period1Panel.messageMinWidth; elide: messageElideMode(modelData.displayMessage || modelData.message) }
                                                Text { visible: period1Panel.distanceColumnWidth > 0; text: decodePanel.distanceText(modelData); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.distanceColumnWidth }
                                                Item {
                                                    visible: mainWindow.showDxccInfo
                                                    Layout.preferredWidth: period1Panel.dxccColumnWidth
                                                    Layout.fillHeight: true
                                                    Text {
                                                        anchors.fill: parent
                                                        text: modelData.dxCountry || ""
                                                        font.family: mainWindow.decodedTextFontFamily
                                                        font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs)
                                                        color: modelData.dxCountry ? bridge.colorDXEntity : textSecondary
                                                        horizontalAlignment: Text.AlignRight
                                                        verticalAlignment: Text.AlignVCenter
                                                        elide: Text.ElideNone
                                                        fontSizeMode: Text.HorizontalFit
                                                        minimumPixelSize: Math.max(8, Math.round(mainWindow.decodedTextFontPixelSize * fs * 0.65))
                                                        maximumLineCount: 1
                                                    }
                                                }
                                                Item {
                                                    visible: mainWindow.showDxccInfo
                                                    Layout.preferredWidth: period1Panel.azColumnWidth
                                                    Layout.fillHeight: true
                                                    Text {
                                                        anchors.fill: parent
                                                        text: formatBearingDegrees(modelData.dxBearing)
                                                        font.family: mainWindow.decodedTextFontFamily
                                                        font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs)
                                                        color: secondaryCyan
                                                        horizontalAlignment: Text.AlignRight
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                }
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "No decodes"
                                            font.pixelSize: 12
                                            color: textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            visible: evenPeriodList.count === 0
                                        }

                                        // 1.0.231 — floating button "↓ N new decodes" visibile quando
                                        // l'utente scrolla up e arrivano nuovi decode in fondo.
                                        // Click → torna in fondo + reset counter. Allineato a
                                        // pattern Twitter "show N new tweets" / Slack "new messages".
                                        Rectangle {
                                            id: evenPeriodPendingButton
                                            anchors.bottom: parent.bottom
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.bottomMargin: 8
                                            width: pendingLabel.implicitWidth + 24
                                            height: 24
                                            radius: 12
                                            visible: evenPeriodList.pendingNewDecodes > 0 && !period1Detached
                                            color: pendingMA.containsMouse
                                                ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 1.0)
                                                : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.85)
                                            border.color: secondaryCyan
                                            border.width: 1
                                            opacity: visible ? 1.0 : 0.0
                                            Behavior on opacity { NumberAnimation { duration: 150 } }
                                            z: 100
                                            Text {
                                                id: pendingLabel
                                                anchors.centerIn: parent
                                                text: "↓ " + evenPeriodList.pendingNewDecodes + " new"
                                                font.pixelSize: 11
                                                font.bold: true
                                                color: bgDeep
                                            }
                                            MouseArea {
                                                id: pendingMA
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    evenPeriodList.pendingNewDecodes = 0
                                                    evenPeriodList.followTail = true
                                                    evenPeriodList.forceTailFollow()
                                                }
                                            }
                                            ToolTip.visible: pendingMA.containsMouse
                                            ToolTip.text: qsTr("Jump to latest decode")
                                            ToolTip.delay: 500
                                        }
                                    }
                                }
                            }
                        }

                        // ========== RIGHT: RX Frequency ==========
                        Rectangle {
                            id: rxFreqPanel
                            property int targetPanelWidth: mainWindow.savedRxFreqPanelWidth
                            SplitView.fillWidth: true
                            SplitView.preferredWidth: targetPanelWidth
                            SplitView.minimumWidth: 260
                            readonly property bool compactColumns: width < 450
                            readonly property bool compactHeader: width < 350
                            readonly property int utcColumnWidth: compactColumns ? 62 : 78
                            readonly property int dbColumnWidth: compactColumns ? 34 : 38
                            readonly property int dbDtGapWidth: compactColumns ? 4 : 6
                            readonly property int dtColumnWidth: compactColumns ? 42 : 48
                            readonly property int gapColumnWidth: compactColumns ? 8 : 12
                            readonly property int distanceColumnWidth: compactColumns ? 0 : 56
                            readonly property int headerBadgeWidth: compactHeader ? 62 : 70
                            color: "transparent"
                            onWidthChanged: {
                                if (width >= 260 && Math.abs(targetPanelWidth - width) >= 1) {
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }

                            // Placeholder when detached - magnetic dock zone
                            Rectangle {
                                anchors.fill: parent
                                visible: rxFreqDetached
                                color: rxFreqDockHighlighted ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                                radius: 8
                                border.color: rxFreqDockHighlighted ? primaryBlue : glassBorder
                                border.width: rxFreqDockHighlighted ? 3 : 1

                                Behavior on color { ColorAnimation { duration: 100 } }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: rxFreqDockHighlighted ? "🧲 Rilascia qui!" : "📡 Signal RX Detached"
                                        color: rxFreqDockHighlighted ? primaryBlue : textSecondary
                                        font.pixelSize: rxFreqDockHighlighted ? 16 : 12
                                        font.bold: rxFreqDockHighlighted
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Trascina la finestra qui"
                                        color: textSecondary
                                        font.pixelSize: 10
                                        visible: !rxFreqDockHighlighted
                                    }

                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: 82
                                        height: 24
                                        radius: 4
                                        visible: !rxFreqDockHighlighted
                                        color: rxPlaceholderRestoreMA.containsMouse ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.72)
                                        border.color: rxPlaceholderRestoreMA.containsMouse ? primaryBlue : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.45)
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Restore"
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: rxPlaceholderRestoreMA.containsMouse ? primaryBlue : textPrimary
                                        }

                                        MouseArea {
                                            id: rxPlaceholderRestoreMA
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: mainWindow.restoreFloatingWindow(rxFreqFloatingWindow, "rxFreqMinimized")
                                        }
                                    }
                                }

                                // Pulsing border when highlighted
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    radius: 8
                                    border.color: primaryBlue
                                    border.width: 4
                                    visible: rxFreqDockHighlighted
                                    opacity: 0.8

                                    SequentialAnimation on opacity {
                                        running: rxFreqDockHighlighted && bridge && bridge.uiQuality !== "Low"
                                        loops: Animation.Infinite
                                        OpacityAnimator { to: 0.4; duration: 300 }
                                        OpacityAnimator { to: 1.0; duration: 300 }
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 2
                                visible: !rxFreqDetached

                                // Header
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                    radius: 4

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 6

                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: primaryBlue
                                        }

                                        Text {
                                            text: "Signal RX"
                                            font.pixelSize: rxFreqPanel.compactHeader ? 12 : 14
                                            font.bold: true
                                            color: primaryBlue
                                        }

                                        Rectangle {
                                            width: rxFreqPanel.headerBadgeWidth
                                            height: 18
                                            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                                            radius: 4
                                            border.color: primaryBlue

                                            Text {
                                                anchors.centerIn: parent
                                                text: bridge.rxFrequency + " Hz"
                                                font.family: "Monospace"
                                                font.pixelSize: 10
                                                font.bold: true
                                                color: primaryBlue
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: {
                                                void(decodePanel.rxDecodeListVersion)
                                                return decodePanel.signalRxModelCount() + " msgs"
                                            }
                                            font.pixelSize: 10
                                            color: textSecondary
                                            visible: !rxFreqPanel.compactHeader
                                        }

                                        // Clear button
                                        Rectangle {
                                            width: 40; height: 20; radius: 4
                                            color: rxClearMA.containsMouse ? Qt.rgba(1, 0.3, 0.3, 0.3) : "transparent"
                                            border.color: rxClearMA.containsMouse ? bridge.themeManager.ledRed : Qt.rgba(textPrimary.r,textPrimary.g,textPrimary.b,0.15)
                                            Text { anchors.centerIn: parent; text: "Clear"; font.pixelSize: 9; color: rxClearMA.containsMouse ? bridge.themeManager.ledRed : textSecondary }
                                            MouseArea {
                                                id: rxClearMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: decodePanel.clearSignalRxDecodes()
                                            }
                                            ToolTip.visible: rxClearMA.containsMouse
                                            ToolTip.text: qsTr("Clear Signal RX")
                                        }

	                                        // Pop button
	                                        Rectangle {
	                                            Layout.preferredWidth: 34
	                                            Layout.preferredHeight: 18
	                                            radius: 4
	                                            color: rxDetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
	                                            border.color: rxDetachMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
	                                            border.width: 1

	                                            Text {
	                                                anchors.centerIn: parent
	                                                text: "Pop"
	                                                font.pixelSize: 10
	                                                font.bold: true
	                                                color: rxDetachMA.containsMouse ? secondaryCyan : textSecondary
	                                            }

                                            MouseArea {
                                                id: rxDetachMA
	                                                anchors.fill: parent
	                                                hoverEnabled: true
	                                                cursorShape: Qt.PointingHandCursor
	                                                onClicked: mainWindow.detachSignalRxPanel()
	                                            }

	                                            ToolTip.visible: rxDetachMA.containsMouse
	                                            ToolTip.text: qsTr("Detach Signal RX")
                                            ToolTip.delay: 500
                                        }
                                    }
                                }

                                // Column headers
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 20
                                    color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2)
                                    radius: 2

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 4
                                        anchors.rightMargin: 4
                                        spacing: 0

                                        Text { text: "UTC"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; Layout.preferredWidth: rxFreqPanel.utcColumnWidth }
                                        Text { text: "dB"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dbColumnWidth }
                                        Item { Layout.preferredWidth: rxFreqPanel.dbDtGapWidth }
                                        Text { text: "DT"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dtColumnWidth }
                                        Item { Layout.preferredWidth: rxFreqPanel.gapColumnWidth }
                                        Text { text: "Message"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; Layout.fillWidth: true }
                                        Text { visible: rxFreqPanel.distanceColumnWidth > 0; text: "Dist"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.distanceColumnWidth }
                                    }
                                }

                                // RX Frequency List (filtered)
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                                    border.color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.4)
                                    border.width: 1
                                    radius: 4
                                    clip: true

                                    ListView {
                                        id: rxFrequencyList
                                        anchors.fill: parent
                                        anchors.margins: 2
	                                        clip: true
	                                        spacing: 1
	                                        cacheBuffer: 600  // 1.0.228 — 3000 era eccessivo per delegate complessi
	                                        reuseItems: true
	                                        interactive: true
                                        property bool followTail: true
                                        property bool tailFollowPending: false
	                                        property bool tailFollowQueued: false
	                                        function isNearTail() {
	                                            return contentHeight <= height + 2
	                                                || contentY >= Math.max(0, contentHeight - height - 48)
	                                        }
                                        function updateFollowTail() {
                                            if (tailFollowPending)
                                                return
                                            followTail = isNearTail()
                                            // 1.0.231 — reset counter "↓ N new" quando torna a tail
                                            if (followTail) pendingNewDecodes = 0
                                        }
                                        function tailContentY() {
                                            return Math.max(0, contentHeight - height)
                                        }
                                        function finishTailFollow() {
                                            tailFollowPending = false
                                            followTail = isNearTail()
                                            if (followTail) pendingNewDecodes = 0
                                        }
                                        function forceTailFollow() {
    followTail = true
    tailFollowPending = true
    if (tailFollowQueued)
        return
    tailFollowQueued = true
    Qt.callLater(function() {
        tailFollowQueued = false
        if (!rxFrequencyList)
            return
        var targetY = rxFrequencyList.tailContentY()
        var distance = Math.abs(rxFrequencyList.contentY - targetY)
        rxFrequencyTailAnimation.stop()
        rxFrequencyList.tailFollowPending = true
        if (distance < 1 || distance > Math.max(12000, rxFrequencyList.height * 18)) {
            rxFrequencyList.contentY = targetY
            rxFrequencyList.finishTailFollow()
            return
        }
        rxFrequencyTailAnimation.from = rxFrequencyList.contentY
        rxFrequencyTailAnimation.to = targetY
        rxFrequencyTailAnimation.duration = Math.max(180, Math.min(620, 130 + distance * 0.24))
        rxFrequencyTailAnimation.start()
    })
}
NumberAnimation {
    id: rxFrequencyTailAnimation
                                            target: rxFrequencyList
                                            property: "contentY"
	                                            duration: 300
	                                            easing.type: Easing.OutCubic
	                                            onStopped: rxFrequencyList.finishTailFollow()
	                                        }
	                                        Timer {
	                                            id: rxFrequencyTailSettleTimer
	                                            interval: 32
	                                            repeat: false
	                                            onTriggered: {
	                                                if (rxFrequencyList.followTail || rxFrequencyList.tailFollowPending)
	                                                    rxFrequencyList.forceTailFollow()
	                                            }
	                                        }
                                        Component.onCompleted: Qt.callLater(function() {
                                            positionViewAtEnd()
                                            updateFollowTail()
                                        })
                                        onContentYChanged: updateFollowTail()
	                                        onContentHeightChanged: {
	                                            if (followTail || tailFollowPending)
	                                                rxFrequencyTailSettleTimer.restart()
	                                        }
	                                        onHeightChanged: {
	                                            if (followTail || tailFollowPending)
	                                                forceTailFollow()
	                                            else
	                                                updateFollowTail()
	                                        }
	                                        onDraggingChanged: {
	                                            if (dragging) {
	                                                followTail = false
	                                                tailFollowPending = false
	                                                rxFrequencyTailAnimation.stop()
	                                            }
	                                        }
                                        onCountChanged: {
                                            forceTailFollow()
                                        }

                                        property int _ver: decodePanel.rxDecodeListVersion
                                        on_VerChanged: {
                                            forceTailFollow()
                                        }
                                        model: (bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : decodePanel.rxDecodes
	                                        // 1.0.186: Animator (render thread) + gate uiQuality !== Low.
	                                        // OpacityAnimator/YAnimator non si fermano durante stall main thread,
	                                        // pattern allineato a DecodeList.qml:243-251.
	                                        add: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            OpacityAnimator { from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        addDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        moveDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        removeDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                                        }

                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                            interactive: true
                                            width: 8
                                        }

	                                        delegate: Rectangle {
	                                            id: rxFrequencyDelegate
	                                            // 1.0.155: separator meno invasivo nel pannello RX
	                                            readonly property bool hasEntry: !!modelData
	                                            readonly property var entry: modelData || ({})
                                            readonly property bool isPeriodSeparator: hasEntry && entry.isSeparator === true
                                            width: rxFrequencyList.width - 8
                                            height: !hasEntry ? 0 : isPeriodSeparator ? Math.round(4 * fs) : Math.round(26 * fs)
                                            color: isPeriodSeparator ? "transparent" :
                                                   entry.isTx ? Qt.rgba(241/255, 196/255, 15/255, 0.3) :
                                                   entry.isMyCall ? Qt.rgba(244/255, 67/255, 54/255, 0.3) :
                                                   entry.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) :
                                                   index % 2 === 0 ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.08) : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15)
                                            radius: 2

	                                            Rectangle {
	                                                visible: rxFrequencyDelegate.isPeriodSeparator
	                                                anchors.verticalCenter: rxFrequencyDelegate.verticalCenter
	                                                anchors.left: rxFrequencyDelegate.left
	                                                anchors.right: rxFrequencyDelegate.right
	                                                anchors.leftMargin: 12
	                                                anchors.rightMargin: 12
	                                                height: 1
                                                color: Qt.rgba(0.85, 0.25, 0.25, 0.55)
	                                            }

	                                            MouseArea {
	                                                enabled: rxFrequencyDelegate.hasEntry && !rxFrequencyDelegate.isPeriodSeparator
	                                                anchors.fill: rxFrequencyDelegate
	                                                hoverEnabled: true
	                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
	                                                onClicked: function(mouse) {
	                                                    if (!rxFrequencyDelegate.hasEntry || rxFrequencyDelegate.isPeriodSeparator) return
	                                                    if (rxFrequencyDelegate.entry.isTx) return
	                                                    if (mouse.button === Qt.LeftButton) {
	                                                        // Sinistro = imposta TX freq
	                                                        if (!bridge.holdTxFreq)
	                                                            bridge.txFrequency = parseInt(rxFrequencyDelegate.entry.freq || "0")
	                                                    } else if (mouse.button === Qt.RightButton) {
	                                                        // Destro = imposta RX freq
	                                                        bridge.rxFrequency = parseInt(rxFrequencyDelegate.entry.freq || "0")
	                                                    }
	                                                }
	                                                onDoubleClicked: function(mouse) {
	                                                    if (!rxFrequencyDelegate.hasEntry || rxFrequencyDelegate.isPeriodSeparator) return
	                                                    if (!rxFrequencyDelegate.entry.isTx && mouse.button === Qt.LeftButton)
	                                                        decodePanel.handleDecodeDoubleClick(rxFrequencyDelegate.entry)
	                                                }
	                                                // IU8LMC: Show DXCC tooltip on hover
	                                                onContainsMouseChanged: {
	                                                    if (!rxFrequencyDelegate.hasEntry || rxFrequencyDelegate.isPeriodSeparator) { dxccTooltipVisible = false; return }
	                                                    if (containsMouse && rxFrequencyDelegate.entry.dxCountry && rxFrequencyDelegate.entry.dxCountry !== "") {
	                                                        dxccTooltipText = getDxccTooltipText(rxFrequencyDelegate.entry)
	                                                        var pos = mapToGlobal(mouseX, mouseY)
	                                                        dxccTooltipX = pos.x - mainWindow.x
	                                                        dxccTooltipY = pos.y - mainWindow.y
                                                        dxccTooltipVisible = true
                                                    } else {
                                                        dxccTooltipVisible = false
                                                    }
                                                }
                                                onPositionChanged: {
                                                    if (containsMouse && dxccTooltipVisible) {
                                                        var pos = mapToGlobal(mouseX, mouseY)
                                                        dxccTooltipX = pos.x - mainWindow.x
                                                        dxccTooltipY = pos.y - mainWindow.y
                                                    }
                                                }
	                                            }

		                                            RowLayout {
		                                                visible: rxFrequencyDelegate.hasEntry && !rxFrequencyDelegate.isPeriodSeparator
		                                                anchors.fill: rxFrequencyDelegate
		                                                anchors.leftMargin: 4
		                                                anchors.rightMargin: 4
	                                                spacing: 0

	                                                Text { text: rxFrequencyDelegate.entry.formattedTime || decodePanel.formatUtcForDisplay(rxFrequencyDelegate.entry.time); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: rxFrequencyDelegate.entry.isTx ? "#f1c40f" : textSecondary; Layout.preferredWidth: rxFreqPanel.utcColumnWidth }
	                                                Text { text: rxFrequencyDelegate.entry.db || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: rxFrequencyDelegate.entry.snrColor || (rxFrequencyDelegate.entry.isTx ? "#f1c40f" : textSecondary); font.bold: rxFrequencyDelegate.entry.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dbColumnWidth }
	                                                Item { Layout.preferredWidth: rxFreqPanel.dbDtGapWidth }
	                                                Text { text: rxFrequencyDelegate.entry.dt || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: rxFrequencyDelegate.entry.isTx ? "#f1c40f" : textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dtColumnWidth }
	                                                Item { Layout.preferredWidth: rxFreqPanel.gapColumnWidth }
	                                                Text { text: rxFrequencyDelegate.entry.displayMessage || rxFrequencyDelegate.entry.message || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); font.bold: decodePanel.decodeEntryBold(rxFrequencyDelegate.entry); font.strikeout: decodePanel.decodeEntryStrikeout(rxFrequencyDelegate.entry); color: getDxccColor(rxFrequencyDelegate.entry); Layout.fillWidth: true; elide: messageElideMode(rxFrequencyDelegate.entry.displayMessage || rxFrequencyDelegate.entry.message) }
	                                                Text { visible: rxFreqPanel.distanceColumnWidth > 0; text: decodePanel.distanceText(rxFrequencyDelegate.entry); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.distanceColumnWidth }
	                                            }
	                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "No messages at\n" + bridge.rxFrequency + " Hz"
                                            font.pixelSize: 12
                                            color: textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            visible: rxFrequencyList.count === 0
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: liveMapPanelHost
                            visible: mainWindow.liveMapPanelVisible && !mainWindow.liveMapDetached
                            color: "transparent"
                            property int targetPanelWidth: mainWindow.savedLiveMapPanelWidth
                            SplitView.preferredWidth: visible ? targetPanelWidth : 0
                            SplitView.minimumWidth: visible ? 280 : 0
                            onWidthChanged: {
                                if (visible && width >= 280 && Math.abs(targetPanelWidth - width) >= 1) {
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }

                            LiveMapPanel {
                                anchors.fill: parent
                                engine: bridge
                                detachable: true
                                detached: false
                                visible: parent.visible
                                onDetachRequested: mainWindow.detachLiveMapPanel()
                            }
                        }
                    }
                }
            } // End mainVerticalSplit (Waterfall top, Decode panels bottom)

            // Resize handle between decode panels and TX panel
            Rectangle {
                id: verticalResizeHandle
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: txPanelContainer.top
                anchors.bottomMargin: 2
                height: 8
                color: vertResizeMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
                z: 50

                Rectangle {
                    anchors.centerIn: parent
                    width: 80
                    height: 4
                    radius: 2
                    color: vertResizeMA.containsMouse ? "#00bcd4" : "#606080"
                }

                MouseArea {
                    id: vertResizeMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SizeVerCursor

                    property int startMouseY: 0
                    property int startTxHeight: 0

                    onPressed: {
                        startMouseY = mouseY
                        startTxHeight = txPanelContainer.height
                    }

                    onPositionChanged: {
                        if (pressed) {
                            var dy = startMouseY - mouseY
                            var newHeight = startTxHeight + dy
                            if (newHeight >= 100 && newHeight <= 350) {
                                txPanelContainer.height = newHeight
                            }
                        }
                    }
                }
            }

            // Queue Panel disabilitato — queue e slot già visibili nella barra principale
            // QueuePanel { id: queuePanelComponent; visible: false }

            // TX Panel Container (resizable at bottom) - delegates to HvTxW via bridge
            Rectangle {
                id: txPanelContainer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 160
                color: "transparent"

                property int minHeight: 100
                property int maxHeight: 350

                // Placeholder when detached - magnetic dock zone
                Rectangle {
                    anchors.fill: parent
                    visible: txPanelDetached
                    color: txPanelDockHighlighted ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                    radius: 12
                    border.color: txPanelDockHighlighted ? bridge.themeManager.ledRed : glassBorder
                    border.width: txPanelDockHighlighted ? 3 : 1

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: txPanelDockHighlighted ? "🧲 Rilascia qui!" : "📻 TX Panel Detached"
                            color: txPanelDockHighlighted ? bridge.themeManager.ledRed : textSecondary
                            font.pixelSize: txPanelDockHighlighted ? 16 : 12
                            font.bold: txPanelDockHighlighted
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Trascina la finestra qui"
                            color: textSecondary
                            font.pixelSize: 10
                            visible: !txPanelDockHighlighted
                        }
                    }

                    // Pulsing border when highlighted
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        radius: 12
                        border.color: bridge.themeManager.ledRed
                        border.width: 4
                        visible: txPanelDockHighlighted
                        opacity: 0.8

                        SequentialAnimation on opacity {
                            running: txPanelDockHighlighted && bridge && bridge.uiQuality !== "Low"
                            loops: Animation.Infinite
                            OpacityAnimator { to: 0.4; duration: 300 }
                            OpacityAnimator { to: 1.0; duration: 300 }
                        }
                    }
                }

                // Actual TxPanel content
                TxPanel {
                    id: txPanelComponent
                    anchors.fill: parent
                    engine: bridge
                    showAsyncIcon: mainWindow.asyncIconVisible
                    visible: !txPanelDetached
                    onMamWindowRequested: mamWindow.open()

                    // Detach button overlay at top-right
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 5
                        anchors.rightMargin: 8
                        width: 34
                        height: 18
                        radius: 4
                        z: 200
                        color: txDetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
                        border.color: txDetachMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Pop"
                            font.pixelSize: 10
                            font.bold: true
                            color: txDetachMA.containsMouse ? secondaryCyan : textSecondary
                        }

                        MouseArea {
                            id: txDetachMA
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                txPanelDetached = true
                                txPanelFloatingWindow.show()
                            }
                        }

                        ToolTip.visible: txDetachMA.containsMouse
                        ToolTip.text: qsTr("Detach TX Panel")
                        ToolTip.delay: 500
                    }
                }

                // Resize handle at top
                Rectangle {
                    id: txPanelResizeHandle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 6
                    color: txResizeMA.containsMouse || txResizeMA.drag.active ? secondaryCyan : "transparent"
                    z: 100
                    visible: !txPanelDetached

                    Rectangle {
                        anchors.centerIn: parent
                        width: 40
                        height: 3
                        radius: 1.5
                        color: txResizeMA.containsMouse || txResizeMA.drag.active ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.3)
                    }

                    MouseArea {
                        id: txResizeMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.SizeVerCursor

                        property real startY: 0
                        property real startHeight: 0

                        onPressed: function(mouse) {
                            startY = mouse.y + txPanelResizeHandle.mapToGlobal(0, 0).y
                            startHeight = txPanelContainer.height
                        }

                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var currentY = mouse.y + txPanelResizeHandle.mapToGlobal(0, 0).y
                                var deltaY = startY - currentY
                                var newHeight = startHeight + deltaY
                                newHeight = Math.max(txPanelContainer.minHeight, Math.min(txPanelContainer.maxHeight, newHeight))
                                txPanelContainer.height = newHeight
                            }
                        }
                    }
                }
            }

            // IU8LMC: DXCC Tooltip overlay
            Rectangle {
                id: dxccTooltip
                visible: dxccTooltipVisible && dxccTooltipText !== ""
                x: Math.min(dxccTooltipX + 15, parent.width - width - 10)
                y: Math.max(dxccTooltipY - height - 5, 10)
                z: 9999
                width: dxccTooltipLabel.width + 16
                height: dxccTooltipLabel.height + 12
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.94)
                border.color: glassBorder
                border.width: 1
                radius: 4

                Text {
                    id: dxccTooltipLabel
                    anchors.centerIn: parent
                    text: dxccTooltipText
                    color: textPrimary
                    font.pixelSize: 11
                    font.family: "Segoe UI"
                }
            }
        }
        } // End contentScroll Flickable

        // Status Bar
        StatusBar {
            Layout.fillWidth: true
            audioLevel: bridge ? bridge.audioLevel : 0.0
            signalLevel: bridge ? bridge.sMeter : 0.0
            monitoring: bridge ? bridge.monitoring : false
            transmitting: bridge ? bridge.transmitting : false
            tuning: bridge ? bridge.tuning : false
            decoding: bridge ? bridge.decoding : false
            catStatus: bridge && bridge.catConnected ? "Connected" : "Disconnected"
        }
    }

    Dialog {
        id: rigErrorDialog
        modal: true
        width: 580
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        title: rigErrorDialogTitle

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: accentOrange
            border.width: 1
            radius: 14
        }

        header: Rectangle {
            height: 62
            color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.96)
            radius: 14

            Row {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 12

                Rectangle {
                    width: 34
                    height: 34
                    radius: 17
                    anchors.verticalCenter: parent.verticalCenter
                    color: Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.18)
                    border.color: accentOrange
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        font.pixelSize: 18
                        font.bold: true
                        color: accentOrange
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: rigErrorDialogTitle.length > 0 ? rigErrorDialogTitle : "Rig Control Error"
                        font.pixelSize: 18
                        font.bold: true
                        color: accentOrange
                    }

                    Text {
                        text: "The legacy radio backend reported a problem."
                        font.pixelSize: 11
                        color: textSecondary
                    }
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: rigErrorSummary
                font.pixelSize: 15
                color: textPrimary
                wrapMode: Text.WordWrap
            }

            Button {
                text: rigErrorDetailsVisible ? "Hide details" : "Show details"
                Layout.alignment: Qt.AlignLeft
                onClicked: rigErrorDetailsVisible = !rigErrorDetailsVisible

                contentItem: Text {
                    text: parent.text
                    color: secondaryCyan
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.1)
                    border.color: glassBorder
                    border.width: 1
                    radius: 8
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: rigErrorDetailsVisible ? 150 : 0
                visible: rigErrorDetailsVisible
                clip: true

                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.WrapAnywhere
                    text: rigErrorDetails
                    color: textSecondary
                    font.pixelSize: 12
                    background: Rectangle {
                        color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.8)
                        border.color: glassBorder
                        border.width: 1
                        radius: 10
                    }
                }
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignRight

            Button {
                text: "Configure radio"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    rigErrorDialog.close()
                    openSettingsTab(1)
                }
            }

            Button {
                text: "Retry"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    rigErrorDialog.close()
                    bridge.retryRigConnection()
                }
            }

            Button {
                text: "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: rigErrorDialog.close()
            }
        }
    }

    // ========== QSO PROGRESS BADGE OVERLAY ==========
    Rectangle {
        id: qsoBadge
        z: 9998
        width: 360
        height: 130
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.3
        radius: 16
        visible: badgeVisible
        color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.12)
        border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.5)
        border.width: 1

        // Inner glow border
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 15
            color: "transparent"
            border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.08)
            border.width: 1
        }

        // Glass shine top
        Rectangle {
            width: parent.width
            height: parent.height * 0.5
            radius: 16
            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.04)
        }

        // Content
        Row {
            anchors.centerIn: parent
            spacing: 16

            // Icon
            Text {
                text: badgeIcon
                font.pixelSize: 40
                anchors.verticalCenter: parent.verticalCenter
            }

            // Text column
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    text: badgeText
                    font.family: "Monospace"
                    font.pixelSize: 36
                    font.bold: true
                    font.letterSpacing: 3
                    color: badgeColor
                }

                Text {
                    text: badgeSubText
                    font.family: "Monospace"
                    font.pixelSize: 18
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.8)
                }
            }
        }

        // Don't block clicks underneath
        MouseArea {
            anchors.fill: parent
            enabled: false
        }

        // Entry animation: scale
        scale: badgeVisible ? 1.0 : 0.7
        Behavior on scale {
            NumberAnimation { duration: 300; easing.type: Easing.OutBack }
        }

        // Entry/exit animation: opacity
        opacity: badgeVisible ? 1.0 : 0.0
        Behavior on opacity {
            NumberAnimation { duration: badgeVisible ? 250 : 400; easing.type: badgeVisible ? Easing.OutQuad : Easing.InQuad }
        }
    }

    Rectangle {
        id: statusToast
        z: 9997
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 72
        anchors.rightMargin: 24
        width: Math.min(parent.width * 0.42, 520)
        implicitHeight: toastContent.implicitHeight + 24
        radius: 12
        visible: statusToastVisible
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)
        border.color: Qt.rgba(statusToastColor.r, statusToastColor.g, statusToastColor.b, 0.55)
        border.width: 1
        opacity: statusToastVisible ? 1.0 : 0.0

        Column {
            id: toastContent
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Text {
                text: "Aggiornamento"
                font.pixelSize: 12
                font.bold: true
                color: statusToastColor
            }

            Text {
                width: parent.width
                text: statusToastText
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: textPrimary
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: statusToastVisible ? 180 : 260; easing.type: statusToastVisible ? Easing.OutQuad : Easing.InQuad }
        }
    }

    LogWindow {
        id: logWindow
    }

    Loader {
        id: macroDialogLoader
        active: false
        asynchronous: true
        source: "components/MacroDialog.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: MacroDialog")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    Loader {
        id: astroWindowLoader
        active: false
        asynchronous: true
        source: "components/AstroWindow.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: AstroWindow")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
    }

    // 1.0.195 — QSY Quick Picker (F2 shortcut). Lazy Loader async per evitare
    // overhead startup; vive in Window separata (render thread isolato).
    Loader {
        id: qsyQuickPickerLoader
        active: false
        asynchronous: true
        source: "components/QsyQuickPickerDialog.qml"
        onLoaded: {
            if (item) {
                item.show()
                item.raise()
                item.requestActivate()
            }
        }
    }
    function openQsyQuickPicker() {
        if (!qsyQuickPickerLoader.active) {
            qsyQuickPickerLoader.active = true
        } else if (qsyQuickPickerLoader.item) {
            qsyQuickPickerLoader.item.show()
            qsyQuickPickerLoader.item.raise()
            qsyQuickPickerLoader.item.requestActivate()
        }
    }
    Shortcut {
        sequence: "F2"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.openQsyQuickPicker()
    }
    // 1.0.229 — toggle compact mode Full Spectrum
    Shortcut {
        sequence: "Ctrl+Shift+C"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.toggleCompactFullSpectrum()
    }

    Loader {
        id: bugReportDialogLoader
        active: false
        asynchronous: true
        source: "components/BugReportDialog.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: BugReportDialog")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    // Apri CAT dialog quando bridge.openCatSettings() viene chiamato
    Connections {
        target: bridge
        function onStatusMessage(msg) {
            console.log("[Bridge]", msg)
            if (shouldShowStatusToast(msg))
                showStatusToast(msg, secondaryCyan)
        }
        function onErrorMessage(msg) {
            // Ignora TUTTI gli errori rig/Hamlib/CAT/COM quando il CAT nativo gestisce il rig
            // Questi vengono dal legacy backend che tenta di connettersi sulla stessa porta
            if (bridge.catBackend === "native") {
                var lower = msg.toLowerCase()
                if (lower.indexOf("hamlib") >= 0 || lower.indexOf("com") >= 0 ||
                    lower.indexOf("access") >= 0 || lower.indexOf("cat failure") >= 0 ||
                    lower.indexOf("cat ") >= 0 || lower.indexOf("rig") >= 0 ||
                    lower.indexOf("serial") >= 0 || lower.indexOf("timed out") >= 0 ||
                    lower.indexOf("kenwood") >= 0 || lower.indexOf("communication") >= 0)
                    return
            }
            console.error("[Bridge ERROR]", msg)
            // Estrai prefisso "Sorgente: dettaglio" per titolo specifico
            // (es. "DX Cluster: Cannot send spot..." → title=DX Cluster, summary=Cannot send spot...)
            var prefixMatch = String(msg).match(/^([^:]{1,40}):\s*([\s\S]+)$/)
            if (prefixMatch) {
                warningDialogTitle = prefixMatch[1].trim()
                warningDialogSummary = prefixMatch[2].trim()
            } else {
                warningDialogTitle = "Error"
                warningDialogSummary = msg
            }
            warningDialogDetails = ""
            warningDialogDetailsVisible = false
            warningDialog.open()
        }
        function onWarningRaised(title, summary, details) {
            // Quando il CAT nativo gestisce il rig, i warning Hamlib dal legacy
            // backend sono falsi positivi (conflitto porta COM) — li ignoriamo.
            // PRIMA: il return sopprimeva TUTTI i warning con CAT nativo, inclusi
            // quelli legittimi (es. logger UDP non raggiunto). Ora filtriamo solo
            // i warning effettivamente legati a CAT/Hamlib/serial.
            if (bridge.catBackend === "native") {
                var lower = (String(title) + " " + String(summary) + " " + String(details)).toLowerCase()
                var catLike = lower.indexOf("cat ") >= 0 || lower.indexOf("cat:") >= 0 ||
                    lower.indexOf("cat[") >= 0 || lower.indexOf("[cat") >= 0 ||
                    lower.indexOf(" cat") >= 0 || lower.indexOf("cat/") >= 0 ||
                    lower.substr(0, 3) === "cat"
                if (lower.indexOf("hamlib") >= 0 || catLike ||
                    lower.indexOf("rig") >= 0 || lower.indexOf("serial") >= 0 ||
                    lower.indexOf("com ") >= 0 || lower.indexOf("timed out") >= 0)
                    return
            }
            warningDialogTitle = title
            warningDialogSummary = summary
            warningDialogDetails = details
            warningDialogDetailsVisible = details.length > 0
            warningDialog.open()
        }
        function onTimeSyncSettingsRequested() {
            timeSyncPanelVisible = true
            openSettingsTab(9)
        }
        function onSetupSettingsRequested(tabIndex) {
            openSettingsTab(tabIndex !== undefined && tabIndex >= 0 ? tabIndex : 0)
        }
        function onCatSettingsRequested() {
            openSettingsTab(1)
        }
        function onQuitRequested() {
            mainWindow.close()
        }
        function onRigErrorRaised(title, summary, details) {
            if (bridge.catBackend === "native") return
            rigErrorDialogTitle = title
            rigErrorSummary = summary
            rigErrorDetails = details
            rigErrorDetailsVisible = false
            rigErrorDialog.open()
        }
    }

    Dialog {
        id: warningDialog
        modal: true
        width: Math.max(360, Math.min(parent ? parent.width - 48 : 560, 620))
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        title: warningDialogTitle
        padding: 18

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.44)
        }

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: accentOrange
            border.width: 1
            radius: 8
        }

        header: Rectangle {
            height: 58
            color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.96)
            radius: 8

            Row {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    anchors.verticalCenter: parent.verticalCenter
                    color: Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.18)
                    border.color: accentOrange
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        font.pixelSize: 17
                        font.bold: true
                        color: accentOrange
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: warningDialogTitle.length > 0 ? warningDialogTitle : "Error"
                        font.pixelSize: 17
                        font.bold: true
                        color: accentOrange
                        elide: Text.ElideRight
                        width: Math.max(180, warningDialog.width - 96)
                    }

                    Text {
                        // Sottotitolo generico solo quando il title è il fallback "Errore";
                        // altrimenti il title è già descrittivo (es. "DX Cluster") e il
                        // sottotitolo statico aggiunge solo rumore.
                        visible: warningDialogTitle === "" || warningDialogTitle === "Error" || warningDialogTitle === "Errore"
                        text: "Decodium reported a non-blocking problem."
                        font.pixelSize: 11
                        color: textSecondary
                        elide: Text.ElideRight
                        width: Math.max(180, warningDialog.width - 96)
                    }
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: warningDialogSummary
                font.pixelSize: 15
                color: textPrimary
                wrapMode: Text.WordWrap
            }

            Button {
                visible: warningDialogDetails.length > 0
                text: warningDialogDetailsVisible ? "Hide details" : "Show details"
                Layout.alignment: Qt.AlignLeft
                onClicked: warningDialogDetailsVisible = !warningDialogDetailsVisible

                contentItem: Text {
                    text: parent.text
                    color: secondaryCyan
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.1)
                    border.color: glassBorder
                    border.width: 1
                    radius: 8
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: warningDialogDetailsVisible ? 150 : 0
                visible: warningDialogDetailsVisible && warningDialogDetails.length > 0
                clip: true

                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.WrapAnywhere
                    text: warningDialogDetails
                    color: textSecondary
                    font.pixelSize: 12
                    background: Rectangle {
                        color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.8)
                        border.color: glassBorder
                        border.width: 1
                        radius: 10
                    }
                }
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignRight
            background: Rectangle {
                color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.72)
                radius: 8
            }

            Button {
                text: "OK"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: warningDialog.close()

                contentItem: Text {
                    text: parent.text
                    color: textPrimary
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 112
                    implicitHeight: 38
                    color: parent.hovered ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.22)
                                          : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
                    border.color: secondaryCyan
                    border.width: 1
                    radius: 8
                }
            }
        }
    }

    // Info Dialog
    InfoDialog {
        id: infoDialog
    }

    // MAM Window - Multi-Answer Mode
    MamWindow {
        id: mamWindow
        engine: bridge
    }

    // Auto-open MAM window when MAM mode is enabled
    Connections {
        target: bridge
        function onMultiAnswerModeChanged() {
            if (bridge.multiAnswerMode) {
                mamWindow.open()
            }
        }
    }

    // QSO Progress Badge - Connections
    Connections {
        target: bridge
        function onQsoProgressChanged() {
            var p = bridge.qsoProgress
            var prev = previousQsoProgress
            previousQsoProgress = p
            if (p === 1 || p === 2) {
                showBadge("CALLING", bridge.dxCall || "CQ", secondaryCyan, "\uD83D\uDCE1")
            } else if (p === 3 && prev < 3) {
                showBadge("REPORT", bridge.reportSent || "", accentOrange, "\u26A1")
            } else if ((p === 4 || p === 5) && prev < 4) {
                showBadge("QSO!", bridge.dxCall || "", accentGreen, "\u2713")
            }
        }
    }

    // QSO Progress Badge - Auto-hide timer
    Timer { id: badgeHideTimer; interval: 2500; onTriggered: badgeVisible = false }
    Timer { id: statusToastHideTimer; interval: 3200; onTriggered: statusToastVisible = false }

    // Main Menu (Hamburger)
    Menu {
        id: mainMenu
        x: 60
        y: 90
        padding: 6
        width: 230
        readonly property real maxVisibleHeight: Math.max(180, mainWindow.height - mainMenu.y - 16)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, maxVisibleHeight)
        background: Rectangle {
            implicitWidth: 230
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 1
            radius: 10
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: mainMenu.contentModel
            currentIndex: mainMenu.currentIndex
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            reuseItems: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded

                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.32)
                }

                background: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: "transparent"
                }
            }
        }

        MenuItem {
            text: qsTr("About Decodium")
            icon.source: ""
            onTriggered: { infoDialog.currentTab = 0; infoDialog.open() }

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: textPrimary
                leftPadding: 10
            }
        }

        MenuItem {
            text: qsTr("Useful Links...")
            onTriggered: { infoDialog.currentTab = 4; infoDialog.open() }

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== DECODIUM FEATURES SUBMENU =====
        MenuItem {
            text: "\u2328 " + qsTr("Keyboard Shortcuts")
            onTriggered: keyboardShortcutsDialog.open()

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: accentOrange
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.swlMode ? "✓ " : "☐ ") + qsTr("SWL Mode (RX Only)")
            onTriggered: bridge.swlMode = !bridge.swlMode

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.swlMode ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.multiAnswerMode ? "✓ " : "☐ ") + qsTr("Multi-Answer Mode")
            onTriggered: bridge.multiAnswerMode = !bridge.multiAnswerMode

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.multiAnswerMode ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: qsTr("MAM Window...")
            onTriggered: mamWindow.open()

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: "📂 " + qsTr("Open ALL.TXT Folder")
            onTriggered: bridge.openAllTxtFolder()

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== TX OPTIONS =====
        MenuItem {
            text: (bridge.txWatchdogMode > 0 ? "✓ " : "☐ ") + qsTr("TX Watchdog")
            onTriggered: txWatchdogDialog.open()

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.txWatchdogMode > 0 ? accentOrange : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.contestType > 0 ? "✓ " : "☐ ") + qsTr("Contest Mode")
            onTriggered: contestDialog.open()

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.contestType > 0 ? accentOrange : textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== DECODE FILTERS =====
        MenuItem {
            text: (bridge.filtersBypassed ? "✓ " : "☐ ") + qsTr("Bypass Filters")
            onTriggered: bridge.filtersBypassed = !bridge.filtersBypassed

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.18) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.filtersBypassed ? accentOrange : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.filterCqOnly ? "✓ " : "☐ ") + qsTr("CQ Only")
            onTriggered: bridge.filterCqOnly = !bridge.filterCqOnly

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.filterCqOnly ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.filterMyCallOnly ? "✓ " : "☐ ") + qsTr("My Call Only")
            onTriggered: bridge.filterMyCallOnly = !bridge.filterMyCallOnly

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.filterMyCallOnly ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== DECODER OPTIONS =====
        MenuItem {
            text: (bridge.deepSearchEnabled ? "✓ " : "☐ ") + qsTr("Deep Search")
            onTriggered: bridge.deepSearchEnabled = !bridge.deepSearchEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.deepSearchEnabled ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.avgDecodeEnabled ? "✓ " : "☐ ") + qsTr("Avg Decode")
            onTriggered: bridge.avgDecodeEnabled = !bridge.avgDecodeEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.avgDecodeEnabled ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: mainWindow.asyncIconVisible
                  ? "✓ " + qsTr("Hide ASYNC icon")
                  : "☐ " + qsTr("Show ASYNC icon")
            onTriggered: mainWindow.setAsyncIconVisible(!mainWindow.asyncIconVisible)

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: mainWindow.asyncIconVisible ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.vhfUhfFeatures ? "✓ " : "☐ ") + qsTr("VHF/UHF Features")
            onTriggered: bridge.vhfUhfFeatures = !bridge.vhfUhfFeatures

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.vhfUhfFeatures ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== RECORDING =====
        MenuItem {
            text: (bridge.recordRxEnabled ? "✓ " : "☐ ") + qsTr("Record RX")
            onTriggered: bridge.recordRxEnabled = !bridge.recordRxEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.recordRxEnabled ? bridge.themeManager.ledRed : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.recordTxEnabled ? "✓ " : "☐ ") + qsTr("Record TX")
            onTriggered: bridge.recordTxEnabled = !bridge.recordTxEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.recordTxEnabled ? bridge.themeManager.ledRed : textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== ALERTS =====
        MenuItem {
            text: (bridge.alertOnCq ? "✓ " : "☐ ") + qsTr("Alert on CQ")
            onTriggered: bridge.alertOnCq = !bridge.alertOnCq

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.alertOnCq ? accentOrange : textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle { implicitHeight: 1; color: glassBorder }
        }

        // ===== PANNELLI FLOATING (GAP 3) =====
        MenuItem {
            text: "🎨 " + qsTr("Color Highlighting...")
            onTriggered: openColorDialog()
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12; color: textSecondary; leftPadding: 10
            }
        }

        MenuItem {
            text: "📡 " + qsTr("QSY...")
            onTriggered: openQsyDialog()
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12; color: textSecondary; leftPadding: 10
            }
        }

        MenuItem {
            enabled: false
            text: "☁ " + qsTr("Update checks disabled")
            onTriggered: {
                bridge.checkForUpdates()
            }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: textSecondary; leftPadding: 10
            }
        }

        MenuItem {
            text: "📂 " + qsTr("Export Cabrillo...")
            onTriggered: cabrilloDlg.open()
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12; color: textSecondary; leftPadding: 10
            }
        }

        MenuItem {
            text: bridge.ctyDatUpdating ? "⏳ " + qsTr("cty.dat downloading...") : "🌍 " + qsTr("Update cty.dat")
            enabled: !bridge.ctyDatUpdating
            onTriggered: bridge.checkCtyDatUpdate()
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: bridge.ctyDatUpdating ? "#FF9800" : textSecondary; leftPadding: 10
            }
        }

	        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: glassBorder } }

		        MenuItem {
		            text: (worldClock.showWorldClock ? "✓ " : "☐ ") + qsTr("Show Clock")
		            onTriggered: worldClock.setClockVisible(!worldClock.showWorldClock)
		            background: Rectangle {
	                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
	                radius: 6
	            }
	            contentItem: Text {
	                text: parent.text
	                font.pixelSize: 12
	                color: worldClock.showWorldClock ? successGreen : textSecondary
		                leftPadding: 10
		            }
		        }

		        MenuItem {
		            text: (dxClusterToolbarVisible ? "✓ " : "☐ ") + qsTr("Show DX Cluster")
		            onTriggered: mainWindow.setDxClusterToolbarVisible(!dxClusterToolbarVisible)
		            background: Rectangle {
		                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
		                radius: 6
		            }
		            contentItem: Text {
		                text: parent.text
		                font.pixelSize: 12
		                color: dxClusterToolbarVisible ? successGreen : textSecondary
		                leftPadding: 10
		            }
		        }

		        MenuItem {
		            text: (pskReporterToolbarVisible ? "✓ " : "☐ ") + qsTr("Show PSK Reporter")
		            onTriggered: mainWindow.setPskReporterToolbarVisible(!pskReporterToolbarVisible)
		            background: Rectangle {
		                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
		                radius: 6
		            }
		            contentItem: Text {
		                text: parent.text
		                font.pixelSize: 12
		                color: pskReporterToolbarVisible ? successGreen : textSecondary
		                leftPadding: 10
		            }
		        }

		        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: glassBorder } }

		        Menu {
		            id: languageSubMenu
		            title: mainWindow.uiLanguageLabel(mainWindow.uiLanguage) + ": " + mainWindow.uiLanguageName(mainWindow.uiLanguage)
		            padding: 6
		            width: 205
		            background: Rectangle {
		                implicitWidth: 205
		                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
		                border.color: secondaryCyan
		                border.width: 1
		                radius: 10
		            }

		            MenuItem {
		                text: uiLanguage === "en" ? "✓ English" : "☐ English"
		                onTriggered: mainWindow.setUiLanguage("en")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "en" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "ca" ? "✓ Català" : "☐ Català"
		                onTriggered: mainWindow.setUiLanguage("ca")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "ca" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "da" ? "✓ Dansk" : "☐ Dansk"
		                onTriggered: mainWindow.setUiLanguage("da")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "da" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "de" ? "✓ Deutsch" : "☐ Deutsch"
		                onTriggered: mainWindow.setUiLanguage("de")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "de" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "es" ? "✓ Español" : "☐ Español"
		                onTriggered: mainWindow.setUiLanguage("es")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "es" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "fr" ? "✓ Français" : "☐ Français"
		                onTriggered: mainWindow.setUiLanguage("fr")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "fr" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "hu" ? "✓ Magyar" : "☐ Magyar"
		                onTriggered: mainWindow.setUiLanguage("hu")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "hu" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "it" ? "✓ Italiano" : "☐ Italiano"
		                onTriggered: mainWindow.setUiLanguage("it")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "it" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "ja" ? "✓ 日本語" : "☐ 日本語"
		                onTriggered: mainWindow.setUiLanguage("ja")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "ja" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "ru" ? "✓ Русский" : "☐ Русский"
		                onTriggered: mainWindow.setUiLanguage("ru")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "ru" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "zh" ? "✓ 简体中文" : "☐ 简体中文"
		                onTriggered: mainWindow.setUiLanguage("zh")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "zh" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "zh_TW" ? "✓ 繁體中文" : "☐ 繁體中文"
		                onTriggered: mainWindow.setUiLanguage("zh_TW")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "zh_TW" ? successGreen : textSecondary; leftPadding: 10 }
		            }
		        }

		        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: glassBorder } }

		        MenuItem {
		            text: (timeSyncPanelVisible ? "✓ " : "☐ ") + qsTr("Time Sync Panel")
		            onTriggered: timeSyncPanelVisible = !timeSyncPanelVisible
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: timeSyncPanelVisible ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (activeStationsPanelVisible ? "✓ " : "☐ ") + qsTr("Active Stations")
            onTriggered: activeStationsPanelVisible = !activeStationsPanelVisible
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: activeStationsPanelVisible ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (liveMapPanelVisible ? "✓ " : "☐ ") + qsTr("Live Map")
            onTriggered: {
                liveMapPanelVisible = !liveMapPanelVisible
                bridge.setSetting("WorldMapDisplayed", liveMapPanelVisible)
            }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: liveMapPanelVisible ? successGreen : textSecondary
                leftPadding: 10
            }
        }

	        MenuItem {
	            text: (bridge.foxMode ? "✓ " : "☐ ") + qsTr("Fox Mode (Caller Queue)")
	            onTriggered: { bridge.foxMode = !bridge.foxMode; callerQueuePanelVisible = bridge.foxMode }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: bridge.foxMode ? "#FF9800" : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.houndMode ? "✓ " : "☐ ") + qsTr("Hound Mode")
            onTriggered: { bridge.houndMode = !bridge.houndMode; callerQueuePanelVisible = bridge.foxMode }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: bridge.houndMode ? "#FF9800" : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (astroPanelVisible ? "✓ " : "☐ ") + qsTr("Astro / EME")
            onTriggered: astroPanelVisible = !astroPanelVisible
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: astroPanelVisible ? successGreen : textSecondary
                leftPadding: 10
            }
	        }

        MenuItem {
            // 1.0.164 fase 5: monitor DecoSyncTime
            text: "⏱  " + qsTr("DecoSyncTime Monitor")
            onTriggered: mainWindow.openDecoSyncMonitor()
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: textSecondary
                leftPadding: 10
            }
        }

	        MenuItem {
	            text: (dxClusterPanelVisible ? "✓ " : "☐ ") + qsTr("DX Cluster Panel")
	            onTriggered: dxClusterPanelVisible = !dxClusterPanelVisible
	            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: dxClusterPanelVisible ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (bridge.alertOnMyCall ? "✓ " : "☐ ") + qsTr("Alert on My Call")
            onTriggered: bridge.alertOnMyCall = !bridge.alertOnMyCall

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.alertOnMyCall ? accentOrange : textSecondary
                leftPadding: 10
            }
        }
    }

    // ===== B11 CABRILLO EXPORT DIALOG =====
    Dialog {
        id: cabrilloDlg
        title: "Export Cabrillo"
        anchors.centerIn: parent
        width: 400
        modal: true

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan; border.width: 1; radius: 10
        }

        contentItem: Column {
            spacing: 12; padding: 16
            Text { text: "Output file path:"; font.pixelSize: 12; color: textPrimary }
            TextField {
                id: cabrilloPath
                width: 360
                text: (Qt.platform.os === "windows"
                       ? "C:/Users/IU8LMC/Documents/" : "~/")
                      + bridge.callsign + "_" + Qt.formatDate(new Date(), "yyyyMMdd") + ".cbr"
                font.family: "Monospace"; font.pixelSize: 11
                color: textPrimary
                background: Rectangle {
                    color: Qt.rgba(textPrimary.r,textPrimary.g,textPrimary.b,0.07); border.color: glassBorder; radius: 4
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "Export"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    if (bridge.exportCabrillo(cabrilloPath.text))
                        cabrilloDlg.close()
                }
            }
            Button {
                text: "Cancel"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }

    // ===== TX WATCHDOG DIALOG =====
    Dialog {
        id: txWatchdogDialog
        title: qsTr("TX Watchdog")
        anchors.centerIn: parent
        width: 350
        height: 280
        modal: true

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 2
            radius: 12
        }

        contentItem: Column {
            spacing: 15
            padding: 20

            Text {
                text: "Watchdog Mode"
                font.pixelSize: 14
                font.bold: true
                color: textPrimary
            }

            Row {
                spacing: 10
                RadioButton {
                    id: wdOff
                    text: "Off"
                    checked: bridge.txWatchdogMode === 0
                    onClicked: bridge.txWatchdogMode = 0
                }
                RadioButton {
                    id: wdTime
                    text: "Time"
                    checked: bridge.txWatchdogMode === 1
                    onClicked: bridge.txWatchdogMode = 1
                }
                RadioButton {
                    id: wdCount
                    text: "Count"
                    checked: bridge.txWatchdogMode === 2
                    onClicked: bridge.txWatchdogMode = 2
                }
            }

            Row {
                spacing: 10
                visible: bridge.txWatchdogMode === 1
                Text { text: "Time (min):"; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    from: 1; to: 30
                    value: bridge.txWatchdogTime
                    onValueChanged: bridge.txWatchdogTime = value
                }
            }

            Row {
                spacing: 10
                visible: bridge.txWatchdogMode === 2
                Text { text: "Max TX:"; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    from: 1; to: 50
                    value: bridge.txWatchdogCount
                    onValueChanged: bridge.txWatchdogCount = value
                }
            }
        }

        footer: DialogButtonBox {
            Button { text: "Close"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
    }

    // ===== CONTEST MODE DIALOG =====
    Dialog {
        id: contestDialog
        title: qsTr("Contest Mode")
        anchors.centerIn: parent
        width: 400
        height: 320
        modal: true

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 2
            radius: 12
        }

        contentItem: Column {
            spacing: 15
            padding: 20

            Text {
                text: "Contest Type"
                font.pixelSize: 14
                font.bold: true
                color: textPrimary
            }

            ComboBox {
                id: contestTypeCombo
                width: 300
                model: bridge.contestTypeNames
                currentIndex: bridge.contestType
                onCurrentIndexChanged: bridge.contestType = currentIndex
            }

            Text {
                text: "Exchange"
                font.pixelSize: 14
                font.bold: true
                color: textPrimary
                visible: bridge.contestType > 0
            }

            TextField {
                width: 300
                text: bridge.contestExchange
                onTextChanged: bridge.contestExchange = text
                placeholderText: "Example: 599 001"
                visible: bridge.contestType > 0
            }

            Row {
                spacing: 10
                visible: bridge.contestType > 0
                Text { text: "Serial Number:"; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    from: 1; to: 9999
                    value: bridge.contestNumber
                    onValueChanged: bridge.contestNumber = value
                }
            }
        }

        footer: DialogButtonBox {
            Button { text: "Close"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
    }

    // ===== KEYBOARD SHORTCUTS DIALOG =====
    Dialog {
        id: keyboardShortcutsDialog
        title: qsTr("Keyboard Shortcuts")
        anchors.centerIn: parent
        width: 400
        height: 450
        modal: true

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 2
            radius: 12
        }

        contentItem: Column {
            spacing: 8
            padding: 20

            Text {
                text: "TRANSMISSION"
                font.pixelSize: 14
                font.bold: true
                color: accentOrange
            }
            Text { text: "F1 - F7: Select TX1 - TX7"; font.pixelSize: 12; color: textPrimary }
            Text { text: "F9: Toggle RX Only First/Second"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Escape: Halt (immediate TX stop)"; font.pixelSize: 12; color: textPrimary }

            Rectangle { height: 1; width: parent.width - 40; color: glassBorder }

            Text {
                text: "CONTROLS (Ctrl+)"
                font.pixelSize: 14
                font.bold: true
                color: secondaryCyan
            }
            Text { text: "Ctrl+A: Toggle Auto Sequence"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Ctrl+G: Generate all TX messages"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Ctrl+Z: Toggle ZAP mode"; font.pixelSize: 12; color: textPrimary }

            Rectangle { height: 1; width: parent.width - 40; color: glassBorder }

            Text {
                text: "ACTIONS (Alt+)"
                font.pixelSize: 14
                font.bold: true
                color: successGreen
            }
            Text { text: "Alt+L: Log current QSO"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Alt+M: Clear decode list (Monitor)"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Alt+S: Stop TX"; font.pixelSize: 12; color: textPrimary }
        }

        footer: DialogButtonBox {
            Button {
                text: "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    // ========== DETACHABLE WATERFALL WINDOW ==========
    Window {
        id: waterfallWindow
        width: 900
        height: 450
        minimumWidth: 600
        minimumHeight: 300
        visible: false
        // 1.0.180 — Frameless opzionale via bridge.uiFramelessPopouts
        flags: (bridge && bridge.uiFramelessPopouts)
               ? (Qt.Window | Qt.FramelessWindowHint)
               : (Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        title: "Waterfall - Decodium"
        color: "transparent"

        // 1.0.180 — Drag handle per Frameless windows: usa native window
        // manager (DragHandler.startSystemMove non blocca main thread).
        DragHandler {
            id: waterfallDragHandler
            target: null
            enabled: bridge ? bridge.uiFramelessPopouts : false
            onActiveChanged: {
                if (active) waterfallWindow.startSystemMove()
            }
        }

        // Position to right of main window initially
        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 50

        // 1.0.180 — Async restore della geometria con delay 150ms.
        // Riduce stall startup su multi-monitor / DPI scaling complesso.
        Timer {
            id: waterfallWindowRestoreTimer
            interval: 150
            running: false
            repeat: false
            onTriggered: {
                mainWindow.restoreFloatingWindowState(waterfallWindow, "waterfallWindow", "waterfallDetached", "waterfallMinimized")
            }
        }

        Component.onCompleted: waterfallWindowRestoreTimer.start()
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

	        // Handle window close
	        onClosing: function(close) {
	            mainWindow.dockWaterfallPanel()
	            close.accepted = true
	        }

        // Drag support for frameless window
        property point dragStartPos: Qt.point(0, 0)
        property bool isDragging: false

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: secondaryCyan
            border.width: 2

            // ===== RESIZE HANDLES (Smooth) =====
            // Right edge
            MouseArea {
                id: rightResize
                width: 10
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                cursorShape: Qt.SizeHorCursor
                preventStealing: true

                onMouseXChanged: {
                    if (pressed) {
                        var newW = waterfallWindow.width + (mouseX - width/2)
                        if (newW >= waterfallWindow.minimumWidth && newW <= 1600)
                            waterfallWindow.width = newW
                    }
                }
            }

            // Left edge
            MouseArea {
                id: leftResize
                width: 10
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                cursorShape: Qt.SizeHorCursor
                preventStealing: true

                onMouseXChanged: {
                    if (pressed) {
                        var delta = mouseX
                        var newW = waterfallWindow.width - delta
                        if (newW >= waterfallWindow.minimumWidth && newW <= 1600) {
                            waterfallWindow.x += delta
                            waterfallWindow.width = newW
                        }
                    }
                }
            }

            // Bottom edge
            MouseArea {
                id: bottomResize
                height: 10
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                cursorShape: Qt.SizeVerCursor
                preventStealing: true

                onMouseYChanged: {
                    if (pressed) {
                        var newH = waterfallWindow.height + (mouseY - height/2)
                        if (newH >= waterfallWindow.minimumHeight && newH <= 900)
                            waterfallWindow.height = newH
                    }
                }
            }

            // Top edge
            MouseArea {
                id: topResize
                height: 10
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                cursorShape: Qt.SizeVerCursor
                preventStealing: true

                onMouseYChanged: {
                    if (pressed) {
                        var delta = mouseY
                        var newH = waterfallWindow.height - delta
                        if (newH >= waterfallWindow.minimumHeight && newH <= 900) {
                            waterfallWindow.y += delta
                            waterfallWindow.height = newH
                        }
                    }
                }
            }

            // Bottom-right corner
            MouseArea {
                id: brResize
                width: 20; height: 20
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                cursorShape: Qt.SizeFDiagCursor
                preventStealing: true

                onMouseXChanged: {
                    if (pressed) {
                        var newW = waterfallWindow.width + (mouseX - width/2)
                        if (newW >= waterfallWindow.minimumWidth && newW <= 1600)
                            waterfallWindow.width = newW
                    }
                }
                onMouseYChanged: {
                    if (pressed) {
                        var newH = waterfallWindow.height + (mouseY - height/2)
                        if (newH >= waterfallWindow.minimumHeight && newH <= 900)
                            waterfallWindow.height = newH
                    }
                }

                // Visual indicator
                Text {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    text: "◢"
                    font.pixelSize: 14
                    color: secondaryCyan
                    opacity: 0.8
                }
            }

            // Bottom-left corner
            MouseArea {
                id: blResize
                width: 20; height: 20
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                cursorShape: Qt.SizeBDiagCursor
                preventStealing: true

                onMouseXChanged: {
                    if (pressed) {
                        var delta = mouseX
                        var newW = waterfallWindow.width - delta
                        if (newW >= waterfallWindow.minimumWidth && newW <= 1600) {
                            waterfallWindow.x += delta
                            waterfallWindow.width = newW
                        }
                    }
                }
                onMouseYChanged: {
                    if (pressed) {
                        var newH = waterfallWindow.height + (mouseY - height/2)
                        if (newH >= waterfallWindow.minimumHeight && newH <= 900)
                            waterfallWindow.height = newH
                    }
                }
            }

            // Top-right corner
            MouseArea {
                id: trResize
                width: 20; height: 20
                anchors.right: parent.right
                anchors.top: parent.top
                cursorShape: Qt.SizeBDiagCursor
                preventStealing: true

                onMouseXChanged: {
                    if (pressed) {
                        var newW = waterfallWindow.width + (mouseX - width/2)
                        if (newW >= waterfallWindow.minimumWidth && newW <= 1600)
                            waterfallWindow.width = newW
                    }
                }
                onMouseYChanged: {
                    if (pressed) {
                        var delta = mouseY
                        var newH = waterfallWindow.height - delta
                        if (newH >= waterfallWindow.minimumHeight && newH <= 900) {
                            waterfallWindow.y += delta
                            waterfallWindow.height = newH
                        }
                    }
                }
            }

            // Top-left corner
            MouseArea {
                id: tlResize
                width: 20; height: 20
                anchors.left: parent.left
                anchors.top: parent.top
                cursorShape: Qt.SizeFDiagCursor
                preventStealing: true

                onMouseXChanged: {
                    if (pressed) {
                        var delta = mouseX
                        var newW = waterfallWindow.width - delta
                        if (newW >= waterfallWindow.minimumWidth && newW <= 1600) {
                            waterfallWindow.x += delta
                            waterfallWindow.width = newW
                        }
                    }
                }
                onMouseYChanged: {
                    if (pressed) {
                        var delta = mouseY
                        var newH = waterfallWindow.height - delta
                        if (newH >= waterfallWindow.minimumHeight && newH <= 900) {
                            waterfallWindow.y += delta
                            waterfallWindow.height = newH
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                // Header with title and Attach button - DRAGGABLE
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                    radius: 6

                    // Drag area for moving window with magnetic dock
                    MouseArea {
                        id: dragArea
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        property bool isDraggingWindow: false

                        cursorShape: Qt.SizeAllCursor

                        onPressed: function(mouse) {
                            clickPos = Qt.point(mouse.x, mouse.y)
                            isDraggingWindow = true
                        }

                        // Track if window has moved away from dock zone
                        property bool hasMovedAway: false

                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                waterfallWindow.x += delta.x
                                waterfallWindow.y += delta.y

                                // Dock zone: inside main window, top area only
                                var dockLeft = mainWindow.x + 50
                                var dockRight = mainWindow.x + mainWindow.width - 50
                                var dockTop = mainWindow.y + 80
                                var dockBottom = mainWindow.y + 180  // Small zone at top

                                // Window position (use top-center of window)
                                var winCenterX = waterfallWindow.x + waterfallWindow.width / 2
                                var winTopY = waterfallWindow.y + 20

                                // Check if inside dock zone
                                var inDockZone = (winCenterX > dockLeft && winCenterX < dockRight &&
                                                  winTopY > dockTop && winTopY < dockBottom)

                                // Check if far from dock zone (to reset hasMovedAway)
                                var farFromDock = (winTopY > dockBottom + 100) ||
                                                  (winCenterX < dockLeft - 100) ||
                                                  (winCenterX > dockRight + 100)

                                if (farFromDock) {
                                    hasMovedAway = true
                                }

                                // Only allow docking if window has moved away first
                                if (hasMovedAway && inDockZone) {
                                    waterfallPanel.isDockHighlighted = true
                                } else {
                                    waterfallPanel.isDockHighlighted = false
                                }
                            }
                        }

                        onReleased: {
                            isDraggingWindow = false
                            // Attach on release if in dock zone
                            if (waterfallPanel.isDockHighlighted) {
                                waterfallPanel.isDockHighlighted = false
                                waterfallDetached = false
                                waterfallWindow.close()
                            }
                            hasMovedAway = false
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 12

                        // Drag handle icon
                        Text {
                            text: "⋮⋮"
                            font.pixelSize: 14
                            color: textSecondary
                        }

                        Text {
                            text: "Waterfall"
                            font.pixelSize: 16
                            font.bold: true
                            color: secondaryCyan
                        }

                        Text {
                            text: "RX: " + bridge.rxFrequency + " Hz | TX: " + bridge.txFrequency + " Hz"
                            font.pixelSize: 12
                            font.family: "Monospace"
                            color: textSecondary
                        }

                        Item { Layout.fillWidth: true }

                        // Frequency controls
                        RowLayout {
                            spacing: 8

                            Text {
                                text: "Zoom:"
                                color: textSecondary
                                font.pixelSize: 11
                            }

                            Rectangle {
                                width: 28
                                height: 24
                                radius: 4
                                color: zoomOutMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                                border.color: glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "-"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: textPrimary
                                }

                                MouseArea {
                                    id: zoomOutMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var wf = waterfallDetachedLoader.item
                                        if (wf && wf.maxFreq < 5000)
                                            wf.maxFreq += 500
                                    }
                                }
                            }

                            Rectangle {
                                width: 28
                                height: 24
                                radius: 4
                                color: zoomInMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                                border.color: glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "+"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: textPrimary
                                }

                                MouseArea {
                                    id: zoomInMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var wf = waterfallDetachedLoader.item
                                        if (wf && wf.maxFreq > 1000)
                                            wf.maxFreq -= 500
                                    }
                                }
                            }

                            Text {
                                text: waterfallDetachedLoader.item
                                      ? waterfallDetachedLoader.item.minFreq + "-" + waterfallDetachedLoader.item.maxFreq + " Hz"
                                      : "0-3200 Hz"
                                font.pixelSize: 10
                                font.family: "Monospace"
                                color: textSecondary
                            }
                        }

	                        Rectangle {
	                            width: 42
	                            height: 24
	                            radius: 4
	                            color: waterfallDockMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.08)
	                            border.color: waterfallDockMA.containsMouse ? secondaryCyan : glassBorder

	                            Text {
	                                anchors.centerIn: parent
		                                text: qsTr("Dock")
	                                font.pixelSize: 10
	                                font.bold: true
	                                color: waterfallDockMA.containsMouse ? secondaryCyan : textPrimary
	                            }

	                            MouseArea {
	                                id: waterfallDockMA
	                                anchors.fill: parent
	                                hoverEnabled: true
	                                cursorShape: Qt.PointingHandCursor
	                                onClicked: mainWindow.dockWaterfallPanel()
	                            }

	                            ToolTip.visible: waterfallDockMA.containsMouse
	                            ToolTip.text: "Dock Waterfall"
	                            ToolTip.delay: 500
	                        }

                        // Minimize button
                        Rectangle {
                            width: 28
                            height: 24
                            radius: 4
                            color: minimizeMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: minimizeMA.containsMouse ? "#ffc107" : glassBorder

                            Text {
                                anchors.centerIn: parent
                                text: "−"
                                font.pixelSize: 18
                                font.bold: true
                                color: minimizeMA.containsMouse ? "#ffc107" : textPrimary
                            }

                            MouseArea {
                                id: minimizeMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    waterfallMinimized = true
                                    waterfallWindow.hide()
                                }
                            }

                            ToolTip.visible: minimizeMA.containsMouse
	                            ToolTip.text: "Minimize"
                            ToolTip.delay: 500
                        }

                    }
                }

                Loader {
                    id: waterfallDetachedLoader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: waterfallWindow.visible
                    asynchronous: true
                    sourceComponent: waterfallDetachedComponent
                }

                Component {
                    id: waterfallDetachedComponent

                    Waterfall {
                        id: waterfallDisplayDetached
                        visible: waterfallDetached
                        showControls: true
                        minFreq: 0
                        maxFreq: 3200
                        spectrumHeight: 150

                        onFrequencySelected: function(freq) {
                            bridge.rxFrequency = freq              // tasto destro = RX
                        }
                        onTxFrequencySelected: function(freq) {
                            bridge.txFrequency = freq              // tasto sinistro = TX
                        }
                    }
                }

                // Bottom info bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 16

                        Text {
                            text: "Mode: " + bridge.mode
                            font.pixelSize: 11
                            color: secondaryCyan
                        }

                        Text {
                            text: "Freq: " + (bridge.frequency / 1000000).toFixed(6) + " MHz"
                            font.pixelSize: 11
                            font.family: "Monospace"
                            color: accentGreen
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: bridge.monitoring ? "Monitoring" : "Stopped"
                            font.pixelSize: 11
                            font.bold: true
                            color: bridge.monitoring ? accentGreen : textSecondary
                        }

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: bridge.monitoring ? accentGreen : "#555"

                            SequentialAnimation on opacity {
                                running: bridge.monitoring && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.4; duration: 600 }
                                OpacityAnimator { to: 1.0; duration: 600 }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== DETACHABLE LOG WINDOW ==========
    Window {
        id: logFloatingWindow
        width: 900
        height: 600
        minimumWidth: 600
        minimumHeight: 400
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "QSO Log - Decodium"
        color: "transparent"

        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 50
        Component.onCompleted: mainWindow.restoreFloatingWindowState(logFloatingWindow, "logFloatingWindow", "logWindowDetached", "logWindowMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            logWindowDetached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: secondaryCyan
            border.width: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                // Header - DRAGGABLE
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                    radius: 6

                    MouseArea {
                        id: logDragArea
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        cursorShape: Qt.SizeAllCursor

                        onPressed: function(mouse) { clickPos = Qt.point(mouse.x, mouse.y) }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                logFloatingWindow.x += delta.x
                                logFloatingWindow.y += delta.y
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        Text { text: "⋮⋮"; font.pixelSize: 14; color: textSecondary }
                        Text { text: "📋 QSO Log"; font.pixelSize: 16; font.bold: true; color: secondaryCyan }
                        Item { Layout.fillWidth: true }

                        // Minimize button
                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: logFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: logFloatMinMA.containsMouse ? "#ffc107" : glassBorder
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 18; font.bold: true; color: logFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: logFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { logWindowMinimized = true; logFloatingWindow.hide() }
                            }
                        }

                        // Close button
                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: logFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: logFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: logFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
                            MouseArea { id: logFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { logWindowDetached = false; logWindowMinimized = false; logFloatingWindow.close() }
                            }
                        }
                    }
                }

                // Log Content - Loader
                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: logFloatingWindow.visible
                    asynchronous: true
                    sourceComponent: logContentComponent
                }
            }
        }
    }

    // Log content component (shared)
    Component {
        id: logContentComponent
        LogWindowContent {
            refreshActive: logFloatingWindow.visible
        }
    }

    // ========== DETACHABLE ASTRO WINDOW ==========
    Window {
        id: astroFloatingWindow
        width: 500
        height: 550
        minimumWidth: 400
        minimumHeight: 400
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "Astro - Decodium"
        color: "transparent"

        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 100
        Component.onCompleted: mainWindow.restoreFloatingWindowState(astroFloatingWindow, "astroFloatingWindow", "astroWindowDetached", "astroWindowMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            astroWindowDetached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: secondaryCyan
            border.width: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                    radius: 6

                    MouseArea {
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        cursorShape: Qt.SizeAllCursor
                        onPressed: function(mouse) { clickPos = Qt.point(mouse.x, mouse.y) }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                astroFloatingWindow.x += delta.x
                                astroFloatingWindow.y += delta.y
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        Text { text: "⋮⋮"; font.pixelSize: 14; color: textSecondary }
                        Text { text: "🌙 Astronomical Data"; font.pixelSize: 16; font.bold: true; color: secondaryCyan }
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: astroFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: astroFloatMinMA.containsMouse ? "#ffc107" : glassBorder
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 18; font.bold: true; color: astroFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: astroFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { astroWindowMinimized = true; astroFloatingWindow.hide() }
                            }
                        }

                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: astroFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: astroFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: astroFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
                            MouseArea { id: astroFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { astroWindowDetached = false; astroWindowMinimized = false; astroFloatingWindow.close() }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: astroFloatingWindow.visible
                    asynchronous: true
                    sourceComponent: astroContentComponent
                }
            }
        }
    }

    Component {
        id: astroContentComponent
        AstroWindowContent { }
    }

    // ========== DETACHABLE MACRO WINDOW ==========
    Window {
        id: macroFloatingWindow
        width: 700
        height: 600
        minimumWidth: 500
        minimumHeight: 400
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "Macro - Decodium"
        color: "transparent"

        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 150
        Component.onCompleted: mainWindow.restoreFloatingWindowState(macroFloatingWindow, "macroFloatingWindow", "macroDialogDetached", "macroDialogMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            macroDialogDetached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: secondaryCyan
            border.width: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                    radius: 6

                    MouseArea {
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        cursorShape: Qt.SizeAllCursor
                        onPressed: function(mouse) { clickPos = Qt.point(mouse.x, mouse.y) }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                macroFloatingWindow.x += delta.x
                                macroFloatingWindow.y += delta.y
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        Text { text: "⋮⋮"; font.pixelSize: 14; color: textSecondary }
                        Text { text: "⌨️ TX Macro Configuration"; font.pixelSize: 16; font.bold: true; color: secondaryCyan }
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: macroFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: macroFloatMinMA.containsMouse ? "#ffc107" : glassBorder
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 18; font.bold: true; color: macroFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: macroFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { macroDialogMinimized = true; macroFloatingWindow.hide() }
                            }
                        }

                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: macroFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: macroFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: macroFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
                            MouseArea { id: macroFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { macroDialogDetached = false; macroDialogMinimized = false; macroFloatingWindow.close() }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: macroFloatingWindow.visible
                    asynchronous: true
                    sourceComponent: macroContentComponent
                }
            }
        }
    }

    Component {
        id: macroContentComponent
        MacroDialogContent { }
    }

    // ========== DETACHABLE RIG CONTROL WINDOW ==========
    Window {
        id: rigFloatingWindow
        width: 600
        height: 550
        minimumWidth: 450
        minimumHeight: 400
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "Rig Control - Decodium"
        color: "transparent"

        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 200
        Component.onCompleted: mainWindow.restoreFloatingWindowState(rigFloatingWindow, "rigFloatingWindow", "rigControlDetached", "rigControlMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            rigControlDetached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: secondaryCyan
            border.width: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                    radius: 6

                    MouseArea {
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        cursorShape: Qt.SizeAllCursor
                        onPressed: function(mouse) { clickPos = Qt.point(mouse.x, mouse.y) }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                rigFloatingWindow.x += delta.x
                                rigFloatingWindow.y += delta.y
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        Text { text: "⋮⋮"; font.pixelSize: 14; color: textSecondary }
                        Text { text: "📻 Rig Control (CAT)"; font.pixelSize: 16; font.bold: true; color: secondaryCyan }
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: rigFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: rigFloatMinMA.containsMouse ? "#ffc107" : glassBorder
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 18; font.bold: true; color: rigFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: rigFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { rigControlMinimized = true; rigFloatingWindow.hide() }
                            }
                        }

                        Rectangle {
                            width: 28; height: 28; radius: 4
                            color: rigFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: rigFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: rigFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
                            MouseArea { id: rigFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { rigControlDetached = false; rigControlMinimized = false; rigFloatingWindow.close() }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: rigFloatingWindow.visible
                    asynchronous: true
                    sourceComponent: rigContentComponent
                }
            }
        }
    }

    Component {
        id: rigContentComponent
        RigControlDialogContent { }
    }

    // ========== DETACHABLE LIVE MAP WINDOW ==========
    Window {
        id: liveMapFloatingWindow
        width: 900
        height: 560
        minimumWidth: 480
        minimumHeight: 320
        visible: false
        flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
             | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
        title: "Live Map - Decodium"
        color: bgDeep

        x: mainWindow.x + 80
        y: mainWindow.y + 80
        Component.onCompleted: {
            mainWindow.restoreFloatingWindowState(liveMapFloatingWindow, "liveMapFloatingWindow", "liveMapDetached", "")
            if (!mainWindow.liveMapPanelVisible)
                liveMapFloatingWindow.hide()
        }
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            mainWindow.dockLiveMapPanel()
            close.accepted = true
        }

        // 1.0.156: traccia minimize/restore via OS chrome — quando l'utente
        // minimizza la finestra flottante, marca liveMapMinimized=true così
        // appare il pulsante "Restore Live Map" nella barra centrale.
        onVisibilityChanged: function(visibility) {
            if (visibility === Window.Minimized) {
                mainWindow.liveMapMinimized = true
            } else if (visibility === Window.Windowed
                       || visibility === Window.Maximized
                       || visibility === Window.FullScreen) {
                mainWindow.liveMapMinimized = false
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.45)
            border.width: 1

            Loader {
                id: liveMapFloatingLoader
                anchors.fill: parent
                anchors.margins: 8
                active: liveMapFloatingWindow.visible
                asynchronous: true
                sourceComponent: liveMapFloatingComponent
            }

            Component {
                id: liveMapFloatingComponent

                LiveMapPanel {
                    engine: bridge
                    detachable: true
                    detached: true
                    onDetachRequested: mainWindow.dockLiveMapPanel()
                }
            }
        }
    }

	    // ========== DETACHABLE FULL SPECTRUM WINDOW ==========
    Window {
	        id: period1FloatingWindow
        width: 680
        height: 400
        minimumWidth: 350
        minimumHeight: 250
        visible: false
	        // 1.0.180 — Frameless opzionale via bridge.uiFramelessPopouts
	        flags: (bridge && bridge.uiFramelessPopouts)
	               ? (Qt.Window | Qt.FramelessWindowHint)
	               : (Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
		        title: "Full Spectrum - Decodium"
	        color: "transparent"

	        // 1.0.180 — Drag handle per Frameless windows: usa native window
	        // manager (DragHandler.startSystemMove non blocca main thread).
	        DragHandler {
	            id: period1FloatingDragHandler
	            target: null
	            enabled: bridge ? bridge.uiFramelessPopouts : false
	            onActiveChanged: {
	                if (active) period1FloatingWindow.startSystemMove()
	            }
	        }
	        readonly property bool compactColumns: width < 560
	        readonly property int utcColumnWidth: compactColumns ? 66 : 84
	        readonly property int dbColumnWidth: compactColumns ? 34 : 38
	        readonly property int dbDtGapWidth: compactColumns ? 4 : 6
	        readonly property int dtColumnWidth: compactColumns ? 42 : 48
	        readonly property int dtFreqGapWidth: compactColumns ? 6 : 8
	        readonly property int freqColumnWidth: compactColumns ? 42 : 46
	        readonly property int gapColumnWidth: compactColumns ? 8 : 12
	        readonly property int distanceColumnWidth: compactColumns ? 0 : 56
	        readonly property int dxccColumnWidth: compactColumns ? 108 : Math.min(300, Math.max(190, Math.round(width * 0.24)))
	        readonly property int azColumnWidth: compactColumns ? 38 : 48

	        x: mainWindow.x + 100
        y: mainWindow.y + 150
        // 1.0.180 — Async restore della geometria con delay 150ms.
        // Riduce stall startup su multi-monitor / DPI scaling complesso.
        Timer {
            id: period1FloatingWindowRestoreTimer
            interval: 150
            running: false
            repeat: false
            onTriggered: {
                mainWindow.restoreFloatingWindowState(period1FloatingWindow, "period1FloatingWindow", "period1Detached", "period1Minimized")
                // 1.0.186 — Auto-detach Full Spectrum di default. Pasquale-pattern:
                // pop-out in Window separata -> render thread isolato -> niente stall
                // main-thread durante drain ListView / texture upload waterfall.
                // 1.0.201 — default ripristinato a true (1.0.197 upstream lo aveva
                // spento, causando regressione performance progressiva).
                // Disattivabile da Settings -> "Detach Full Spectrum".
                if (!mainWindow.period1Detached
                        && bridge && bridge.autoDetachFullSpectrum) {
                    mainWindow.detachFullSpectrumPanel()
                }
            }
        }

        Component.onCompleted: period1FloatingWindowRestoreTimer.start()
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibilityChanged: function(visibility) {
            if (period1Detached && visibility === Window.Minimized) {
                period1Minimized = true
            } else if (period1Detached && period1FloatingWindow.visible && period1Minimized) {
                period1Minimized = false
            }
        }

	        onClosing: function(close) {
	            mainWindow.dockFullSpectrumPanel()
	            close.accepted = true
	        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: bridge.themeManager.successColor
            border.width: 2

            FloatingResizeHandles {
                targetWindow: period1FloatingWindow
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                // Header with magnetic dock
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: Qt.rgba(76/255, 175/255, 80/255, 0.25)
                    radius: 6

                    MouseArea {
                        id: p1DragArea
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        property bool hasMovedAway: false
                        cursorShape: Qt.SizeAllCursor

                        onPressed: function(mouse) {
                            clickPos = Qt.point(mouse.x, mouse.y)
                            hasMovedAway = false
                        }

                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                period1FloatingWindow.x += delta.x
                                period1FloatingWindow.y += delta.y

                                // Get period1Panel global position
                                var panelPos = period1Panel.mapToGlobal(0, 0)
                                var dockLeft = panelPos.x
                                var dockRight = panelPos.x + period1Panel.width
                                var dockTop = panelPos.y
                                var dockBottom = panelPos.y + period1Panel.height

                                // Window center position
                                var winCenterX = period1FloatingWindow.x + period1FloatingWindow.width / 2
                                var winCenterY = period1FloatingWindow.y + period1FloatingWindow.height / 2

                                // Check if inside dock zone
                                var inDockZone = (winCenterX > dockLeft && winCenterX < dockRight &&
                                                  winCenterY > dockTop && winCenterY < dockBottom)

                                // Check if far from dock zone
                                var farFromDock = (winCenterY > dockBottom + 100) || (winCenterY < dockTop - 100) ||
                                                  (winCenterX < dockLeft - 100) || (winCenterX > dockRight + 100)

                                if (farFromDock) hasMovedAway = true

                                period1DockHighlighted = (hasMovedAway && inDockZone)
                            }
                        }

	                        onReleased: {
	                            if (period1DockHighlighted) {
	                                mainWindow.dockFullSpectrumPanel()
	                            }
	                            hasMovedAway = false
	                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text { text: "⋮⋮"; font.pixelSize: 12; color: bridge.themeManager.successColor }
		                        Rectangle { Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5; color: bridge.themeManager.successColor }
		                        Text { text: "Full Spectrum"; font.pixelSize: 14; font.bold: true; color: bridge.themeManager.successColor }
		                        Item { Layout.fillWidth: true }

	                            Text {
	                                visible: period1FloatingWindow.width >= 470
	                                text: decodePanel.fullSpectrumModelCount() + " " + qsTr("decodes")
	                                font.pixelSize: 10
	                                color: textSecondary
	                            }

	                            Rectangle {
	                                Layout.preferredWidth: 42
	                                Layout.preferredHeight: 22
	                                radius: 4
	                                color: p1FloatClearMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.25) : "transparent"
	                                border.color: p1FloatClearMA.containsMouse ? bridge.themeManager.ledRed : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.16)
	                                border.width: 1
	                                Text {
	                                    anchors.centerIn: parent
	                                    text: qsTr("Clear")
	                                    font.pixelSize: 10
	                                    color: p1FloatClearMA.containsMouse ? bridge.themeManager.ledRed : textSecondary
	                                }
	                                MouseArea {
	                                    id: p1FloatClearMA
	                                    anchors.fill: parent
	                                    hoverEnabled: true
	                                    cursorShape: Qt.PointingHandCursor
	                                    onClicked: bridge.clearDecodes()
	                                }
	                            }

		                        // 1.0.229 — Compact toggle anche su floating window
		                        Rectangle {
		                            Layout.preferredWidth: 42
		                            Layout.preferredHeight: 22
		                            radius: 4
		                            color: p1FloatCompactMA.containsMouse
		                                ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
		                                : (mainWindow.compactFullSpectrum
		                                    ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
		                                    : "transparent")
		                            border.color: (p1FloatCompactMA.containsMouse || mainWindow.compactFullSpectrum)
		                                ? secondaryCyan
		                                : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.45)
		                            border.width: 1
		                            Text {
		                                anchors.centerIn: parent
		                                text: mainWindow.compactFullSpectrum ? "Full" : "Compact"
		                                font.pixelSize: mainWindow.compactFullSpectrum ? 11 : 10
		                                font.bold: true
		                                color: (p1FloatCompactMA.containsMouse || mainWindow.compactFullSpectrum)
		                                    ? secondaryCyan : textPrimary
		                            }
		                            MouseArea {
		                                id: p1FloatCompactMA
		                                anchors.fill: parent
		                                hoverEnabled: true
		                                cursorShape: Qt.PointingHandCursor
		                                onClicked: mainWindow.toggleCompactFullSpectrum()
		                            }
		                            ToolTip.visible: p1FloatCompactMA.containsMouse
		                            ToolTip.text: mainWindow.compactFullSpectrum
		                                ? qsTr("Switch to normal row height")
		                                : qsTr("Compact rows (2x more visible decodes)")
		                        }

		                        Rectangle {
	                            Layout.preferredWidth: 42
	                            Layout.preferredHeight: 22
	                            radius: 4
	                            color: p1FloatDockMA.containsMouse ? Qt.rgba(76/255, 175/255, 80/255, 0.3) : "transparent"
	                            border.color: p1FloatDockMA.containsMouse ? bridge.themeManager.successColor : Qt.rgba(76/255, 175/255, 80/255, 0.45)
	                            border.width: 1
	                            Text {
	                                anchors.centerIn: parent
	                                text: "Dock"
	                                font.pixelSize: 10
	                                font.bold: true
	                                color: p1FloatDockMA.containsMouse ? bridge.themeManager.successColor : textPrimary
	                            }
	                            MouseArea {
	                                id: p1FloatDockMA
	                                anchors.fill: parent
	                                hoverEnabled: true
	                                cursorShape: Qt.PointingHandCursor
	                                onClicked: mainWindow.dockFullSpectrumPanel()
	                            }
	                            ToolTip.visible: p1FloatDockMA.containsMouse
		                            ToolTip.text: qsTr("Dock")
	                        }

		                        Rectangle {
		                            Layout.preferredWidth: 24
		                            Layout.preferredHeight: 24
		                            radius: 4
                            color: p1FloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 16; font.bold: true; color: p1FloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: p1FloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: mainWindow.minimizeFloatingWindow(period1FloatingWindow, "period1Minimized")
                            }
                        }

		                        Rectangle {
		                            Layout.preferredWidth: 24
		                            Layout.preferredHeight: 24
		                            radius: 4
	                            color: p1FloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
	                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 11; font.bold: true; color: p1FloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
	                            MouseArea { id: p1FloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
	                                onClicked: mainWindow.dockFullSpectrumPanel()
	                            }
	                        }
                    }
	                }

	                Rectangle {
	                    Layout.fillWidth: true
	                    Layout.preferredHeight: 20
	                    color: Qt.rgba(76/255, 175/255, 80/255, 0.2)
	                    radius: 2

	                    RowLayout {
	                        anchors.fill: parent
		                        anchors.leftMargin: 8
		                        anchors.rightMargin: 8
	                        spacing: 0

	                        Text { text: "UTC"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; Layout.preferredWidth: period1FloatingWindow.utcColumnWidth }
	                        Text { text: "dB"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.dbColumnWidth }
	                        Item { Layout.preferredWidth: period1FloatingWindow.dbDtGapWidth }
	                        Text { text: "DT"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.dtColumnWidth }
	                        Item { Layout.preferredWidth: period1FloatingWindow.dtFreqGapWidth }
	                        Text { text: "Freq"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.freqColumnWidth }
	                        Item { Layout.preferredWidth: period1FloatingWindow.gapColumnWidth }
	                        Text { text: "Message"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; Layout.fillWidth: true }
	                        Text { visible: period1FloatingWindow.distanceColumnWidth > 0; text: "Dist"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.distanceColumnWidth }
	                        Text { visible: mainWindow.showDxccInfo; text: "DXCC"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.dxccColumnWidth }
	                        Text { visible: mainWindow.showDxccInfo; text: "Az"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: bridge.themeManager.successColor; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.azColumnWidth }
	                    }
	                }

	                // Content - Decode List
	                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                    border.color: Qt.rgba(76/255, 175/255, 80/255, 0.4)
                    radius: 4
                    clip: true

                    ListView {
                        id: period1FloatingList
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        spacing: 1
                        model: (bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : decodePanel.allDecodes
                        cacheBuffer: 600  // 1.0.228 — 3000 era eccessivo per delegate complessi
                        reuseItems: true
                        interactive: true
                        property bool followTail: true
                        property bool tailFollowPending: false
	                        property bool tailFollowQueued: false
	                        // 1.0.231 — counter pending decodes (floating mode)
	                        property int pendingNewDecodes: 0
	                        function isNearTail() {
	                            return contentHeight <= height + 2
	                                || contentY >= Math.max(0, contentHeight - height - 48)
	                        }
                        function updateFollowTail() {
                            if (tailFollowPending)
                                return
                            followTail = isNearTail()
                            if (followTail) pendingNewDecodes = 0
                        }
                        function tailContentY() {
                            return Math.max(0, contentHeight - height)
                        }
                        function finishTailFollow() {
                            tailFollowPending = false
                            followTail = isNearTail()
                            if (followTail) pendingNewDecodes = 0
                        }
                        function forceTailFollow() {
    followTail = true
    tailFollowPending = true
    if (tailFollowQueued)
        return
    tailFollowQueued = true
    Qt.callLater(function() {
        tailFollowQueued = false
        if (!period1FloatingList)
            return
        var targetY = period1FloatingList.tailContentY()
        var distance = Math.abs(period1FloatingList.contentY - targetY)
        period1FloatingTailAnimation.stop()
        period1FloatingList.tailFollowPending = true
        if (distance < 1 || distance > Math.max(12000, period1FloatingList.height * 18)) {
            period1FloatingList.contentY = targetY
            period1FloatingList.finishTailFollow()
            return
        }
        period1FloatingTailAnimation.from = period1FloatingList.contentY
        period1FloatingTailAnimation.to = targetY
        period1FloatingTailAnimation.duration = Math.max(180, Math.min(620, 130 + distance * 0.24))
        period1FloatingTailAnimation.start()
    })
}
NumberAnimation {
    id: period1FloatingTailAnimation
                            target: period1FloatingList
                            property: "contentY"
	                            duration: 300
	                            easing.type: Easing.OutCubic
	                            onStopped: period1FloatingList.finishTailFollow()
	                        }
	                        Timer {
	                            id: period1FloatingTailSettleTimer
	                            interval: 32
	                            repeat: false
	                            onTriggered: {
	                                if (period1FloatingList.followTail || period1FloatingList.tailFollowPending)
	                                    period1FloatingList.forceTailFollow()
	                            }
	                        }
                        Component.onCompleted: Qt.callLater(function() {
                            positionViewAtEnd()
                            updateFollowTail()
                        })
                        onContentYChanged: updateFollowTail()
	                        onContentHeightChanged: {
	                            if (!period1Detached) return
	                            if (followTail || tailFollowPending)
	                                period1FloatingTailSettleTimer.restart()
	                        }
	                        onHeightChanged: {
	                            if (!period1Detached) return
	                            if (followTail || tailFollowPending)
	                                forceTailFollow()
	                            else
	                                updateFollowTail()
	                        }
	                        onDraggingChanged: {
	                            if (dragging) {
	                                followTail = false
	                                tailFollowPending = false
	                                period1FloatingTailAnimation.stop()
	                            }
	                        }
                        // 1.0.227 — gate simmetrico al evenPeriodList: questo
                        // floating ListView e' attivo solo quando period1Detached=true.
                        onCountChanged: {
                            if (!period1Detached) return
                            // 1.0.231 — se user in scroll-back, counter ↓N
                            if (!followTail) {
                                pendingNewDecodes++
                                return
                            }
                            forceTailFollow()
                        }
                        property int _ver: decodePanel.decodeListVersion
                        on_VerChanged: {
                            if (!period1Detached) return
                            if (!followTail) return
                            forceTailFollow()
                        }
	                        // 1.0.186: Animator (render thread) + gate uiQuality !== Low.
	                        // OpacityAnimator/YAnimator non si fermano durante stall main thread,
	                        // pattern allineato a DecodeList.qml:243-251.
	                        add: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            OpacityAnimator { from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        addDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        moveDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        removeDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            interactive: true
                            width: 8
                        }

	                        delegate: Rectangle {
		                            width: parent ? parent.width : 100
	                            readonly property bool isPeriodSeparator: !!(modelData && modelData.isSeparator === true)
	                            // 1.0.229 — height adattiva compact mode (vedi mainWindow.fullSpectrumRowHeight)
	                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(mainWindow.fullSpectrumRowHeight * fs)
		                            radius: 3
		                            property var highlightFill: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightFill(modelData)
		                            property var highlightBorder: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightBorder(modelData)
		                            // 1.0.205 — guard !modelData per evitare TypeError flood (~46/s) durante
		                            // model swap transients che saturava il main thread via logger sincrono.
		                            color: !modelData ? "transparent" :
		                                   isPeriodSeparator ? "transparent" :
		                                   highlightFill ? highlightFill :
		                                   modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) :
		                                   Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
	                            border.color: !isPeriodSeparator && highlightBorder ? highlightBorder : "transparent"
	                            border.width: !isPeriodSeparator && highlightFill ? 1 : 0

	                            Rectangle {
	                                visible: parent.isPeriodSeparator
	                                anchors.verticalCenter: parent.verticalCenter
	                                anchors.left: parent.left
	                                anchors.right: parent.right
	                                anchors.leftMargin: 12
	                                anchors.rightMargin: 12
	                                height: 1
	                                color: Qt.rgba(0.85, 0.25, 0.25, 0.55)
	                            }

	                            RowLayout {
	                                visible: !parent.isPeriodSeparator
	                                anchors.fill: parent
	                                anchors.margins: 4
		                                spacing: 0
	                                Text { text: modelData.formattedTime || decodePanel.formatUtcForDisplay(modelData.time); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; Layout.preferredWidth: period1FloatingWindow.utcColumnWidth }
	                                Text { text: modelData.db || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.snrColor || (modelData.isTx ? "#f1c40f" : textSecondary); font.bold: modelData.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.dbColumnWidth }
	                                Item { Layout.preferredWidth: period1FloatingWindow.dbDtGapWidth }
	                                Text { text: modelData.dt || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.dtColumnWidth }
	                                Item { Layout.preferredWidth: period1FloatingWindow.dtFreqGapWidth }
	                                Text { text: modelData.freq || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : decodePanel.isAtRxFrequency(modelData.freq || "0", modelData) ? bridge.themeManager.successColor : secondaryCyan; font.bold: (modelData.isTx === true) || decodePanel.isAtRxFrequency(modelData.freq || "0", modelData); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.freqColumnWidth }
	                                Item { Layout.preferredWidth: period1FloatingWindow.gapColumnWidth }
	                                Text { text: modelData.displayMessage || modelData.message || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); font.bold: decodePanel.decodeEntryBold(modelData); font.strikeout: decodePanel.decodeEntryStrikeout(modelData); color: mainWindow.fullSpectrumTextColor(modelData); Layout.fillWidth: true; elide: messageElideMode(modelData.displayMessage || modelData.message) }
	                                Text { visible: period1FloatingWindow.distanceColumnWidth > 0; text: decodePanel.distanceText(modelData); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.distanceColumnWidth }
	                                Text { visible: mainWindow.showDxccInfo; text: modelData.dxCountry || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); fontSizeMode: Text.HorizontalFit; minimumPixelSize: Math.max(8, Math.round(mainWindow.decodedTextFontPixelSize * fs * 0.65)); maximumLineCount: 1; color: modelData.dxCountry ? bridge.colorDXEntity : textSecondary; horizontalAlignment: Text.AlignRight; elide: Text.ElideNone; Layout.preferredWidth: period1FloatingWindow.dxccColumnWidth }
	                                Text { visible: mainWindow.showDxccInfo; text: formatBearingDegrees(modelData.dxBearing); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1FloatingWindow.azColumnWidth }
                            }

	                            MouseArea {
	                                enabled: !parent.isPeriodSeparator
	                                anchors.fill: parent
	                                onDoubleClicked: { if (!parent.isPeriodSeparator && !modelData.isTx) decodePanel.handleDecodeDoubleClick(modelData) }
	                            }
	                        }
                    }

                    // 1.0.231 — floating "↓ N new" button (floating window)
                    Rectangle {
                        id: period1FloatingPendingButton
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 8
                        width: period1FloatingPendingLabel.implicitWidth + 24
                        height: 24
                        radius: 12
                        visible: period1FloatingList.pendingNewDecodes > 0 && period1Detached
                        color: period1FloatingPendingMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 1.0) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.85)
                        border.color: secondaryCyan
                        border.width: 1
                        z: 100
                        Text {
                            id: period1FloatingPendingLabel
                            anchors.centerIn: parent
                            text: "↓ " + period1FloatingList.pendingNewDecodes + " new"
                            font.pixelSize: 11
                            font.bold: true
                            color: "#000000"
                        }
                        MouseArea {
                            id: period1FloatingPendingMA
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                period1FloatingList.pendingNewDecodes = 0
                                period1FloatingList.followTail = true
                                period1FloatingList.forceTailFollow()
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== DETACHABLE PERIOD 2 WINDOW (placeholder) ==========
    Window {
        id: period2FloatingWindow
        width: 500
        height: 400
        minimumWidth: 350
        minimumHeight: 250
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "Period 2 - Decodium"
        color: "transparent"
        x: mainWindow.x + 180
        y: mainWindow.y + 180
        Component.onCompleted: mainWindow.restoreFloatingWindowState(period2FloatingWindow, "period2FloatingWindow", "period2Detached", "period2Minimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onClosing: function(close) {
            period2Detached = false
            close.accepted = true
        }
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Period 2"
                color: textPrimary
                font.pixelSize: 14
            }
        }
    }

    // ========== DETACHABLE RX FREQUENCY WINDOW ==========
    Window {
        id: rxFreqFloatingWindow
        width: 450
        height: 350
        minimumWidth: 300
        minimumHeight: 200
        visible: false
	        flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
	        title: "Signal RX - Decodium"
	        color: "transparent"
	        readonly property bool compactColumns: width < 520
	        readonly property int utcColumnWidth: compactColumns ? 66 : 84
	        readonly property int dbColumnWidth: compactColumns ? 34 : 38
	        readonly property int dbDtGapWidth: compactColumns ? 4 : 6
	        readonly property int dtColumnWidth: compactColumns ? 42 : 48
	        readonly property int gapColumnWidth: compactColumns ? 8 : 12
	        readonly property int distanceColumnWidth: compactColumns ? 0 : 56

	        x: mainWindow.x + 300
        y: mainWindow.y + 250
        Component.onCompleted: mainWindow.restoreFloatingWindowState(rxFreqFloatingWindow, "rxFreqFloatingWindow", "rxFreqDetached", "rxFreqMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibilityChanged: function(visibility) {
            if (rxFreqDetached && visibility === Window.Minimized) {
                rxFreqMinimized = true
            } else if (rxFreqDetached && rxFreqFloatingWindow.visible && rxFreqMinimized) {
                rxFreqMinimized = false
            }
        }

	        onClosing: function(close) {
	            mainWindow.dockSignalRxPanel()
	            close.accepted = true
	        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: primaryBlue
            border.width: 2

            FloatingResizeHandles {
                targetWindow: rxFreqFloatingWindow
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                // Header with magnetic dock
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25)
                    radius: 6

                    MouseArea {
                        id: rxDragArea
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        property bool hasMovedAway: false
                        cursorShape: Qt.SizeAllCursor

                        onPressed: function(mouse) {
                            clickPos = Qt.point(mouse.x, mouse.y)
                            hasMovedAway = false
                        }

                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                rxFreqFloatingWindow.x += delta.x
                                rxFreqFloatingWindow.y += delta.y

                                // Get rxFreqPanel global position
                                var panelPos = rxFreqPanel.mapToGlobal(0, 0)
                                var dockLeft = panelPos.x
                                var dockRight = panelPos.x + rxFreqPanel.width
                                var dockTop = panelPos.y
                                var dockBottom = panelPos.y + rxFreqPanel.height

                                // Window center position
                                var winCenterX = rxFreqFloatingWindow.x + rxFreqFloatingWindow.width / 2
                                var winCenterY = rxFreqFloatingWindow.y + rxFreqFloatingWindow.height / 2

                                // Check if inside dock zone
                                var inDockZone = (winCenterX > dockLeft && winCenterX < dockRight &&
                                                  winCenterY > dockTop && winCenterY < dockBottom)

                                // Check if far from dock zone
                                var farFromDock = (winCenterY > dockBottom + 100) || (winCenterY < dockTop - 100) ||
                                                  (winCenterX < dockLeft - 100) || (winCenterX > dockRight + 100)

                                if (farFromDock) hasMovedAway = true

                                rxFreqDockHighlighted = (hasMovedAway && inDockZone)
                            }
                        }

	                        onReleased: {
	                            if (rxFreqDockHighlighted) {
	                                mainWindow.dockSignalRxPanel()
	                            }
	                            hasMovedAway = false
	                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text { text: "⋮⋮"; font.pixelSize: 12; color: primaryBlue }
	                        Rectangle { Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5; color: primaryBlue }
	                        Text { text: "Signal RX"; font.pixelSize: 14; font.bold: true; color: primaryBlue }

	                        Rectangle {
	                            Layout.preferredWidth: 70
	                            Layout.preferredHeight: 20
                            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                            radius: 4
                            border.color: primaryBlue

                            Text {
                                anchors.centerIn: parent
                                text: bridge.rxFrequency + " Hz"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                            }
	                        }

		                        Item { Layout.fillWidth: true }

	                            Text {
	                                visible: rxFreqFloatingWindow.width >= 500
	                                text: {
	                                    void(decodePanel.rxDecodeListVersion)
	                                    return decodePanel.signalRxModelCount() + " " + qsTr("msgs")
	                                }
	                                font.pixelSize: 10
	                                color: textSecondary
	                            }

	                            Rectangle {
	                                Layout.preferredWidth: 42
	                                Layout.preferredHeight: 22
	                                radius: 4
	                                color: rxFloatClearMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.25) : "transparent"
	                                border.color: rxFloatClearMA.containsMouse ? bridge.themeManager.ledRed : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.16)
	                                border.width: 1
	                                Text {
	                                    anchors.centerIn: parent
	                                    text: qsTr("Clear")
	                                    font.pixelSize: 10
	                                    color: rxFloatClearMA.containsMouse ? bridge.themeManager.ledRed : textSecondary
	                                }
	                                MouseArea {
	                                    id: rxFloatClearMA
	                                    anchors.fill: parent
	                                    hoverEnabled: true
	                                    cursorShape: Qt.PointingHandCursor
	                                    onClicked: decodePanel.clearSignalRxDecodes()
	                                }
	                            }

		                        Rectangle {
	                            Layout.preferredWidth: 42
	                            Layout.preferredHeight: 22
	                            radius: 4
	                            color: rxFloatDockMA.containsMouse ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3) : "transparent"
	                            border.color: rxFloatDockMA.containsMouse ? primaryBlue : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.45)
	                            border.width: 1
	                            Text {
	                                anchors.centerIn: parent
		                                text: qsTr("Dock")
	                                font.pixelSize: 10
	                                font.bold: true
	                                color: rxFloatDockMA.containsMouse ? primaryBlue : textPrimary
	                            }
	                            MouseArea {
	                                id: rxFloatDockMA
	                                anchors.fill: parent
	                                hoverEnabled: true
	                                cursorShape: Qt.PointingHandCursor
	                                onClicked: mainWindow.dockSignalRxPanel()
	                            }
	                            ToolTip.visible: rxFloatDockMA.containsMouse
		                            ToolTip.text: qsTr("Dock")
		                }

		                Rectangle {
			                            Layout.preferredWidth: 24
			                            Layout.preferredHeight: 24
		                            radius: 4
                            color: rxFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 16; font.bold: true; color: rxFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: rxFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: mainWindow.minimizeFloatingWindow(rxFreqFloatingWindow, "rxFreqMinimized")
                            }
                        }

		                        Rectangle {
		                            Layout.preferredWidth: 24
		                            Layout.preferredHeight: 24
		                            radius: 4
	                            color: rxFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
	                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 11; font.bold: true; color: rxFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
	                            MouseArea { id: rxFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
	                                onClicked: mainWindow.dockSignalRxPanel()
	                            }
		                        }
	                    }
	                }

	                Rectangle {
	                    Layout.fillWidth: true
	                    Layout.preferredHeight: 20
	                    color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2)
	                    radius: 2

	                    RowLayout {
	                        anchors.fill: parent
	                        anchors.leftMargin: 6
	                        anchors.rightMargin: 6
	                        spacing: 0

	                        Text { text: "UTC"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; Layout.preferredWidth: rxFreqFloatingWindow.utcColumnWidth }
	                        Text { text: "dB"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.dbColumnWidth }
	                        Item { Layout.preferredWidth: rxFreqFloatingWindow.dbDtGapWidth }
	                        Text { text: "DT"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.dtColumnWidth }
	                        Item { Layout.preferredWidth: rxFreqFloatingWindow.gapColumnWidth }
	                        Text { text: "Message"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; Layout.fillWidth: true }
	                        Text { visible: rxFreqFloatingWindow.distanceColumnWidth > 0; text: "Dist"; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.distanceColumnWidth }
	                    }
	                }

	                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                    border.color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.4)
                    radius: 4
                    clip: true

                    ListView {
                        id: rxFrequencyFloatingList
                        anchors.fill: parent
                        anchors.margins: 4
	                        clip: true
	                        spacing: 1
	                        cacheBuffer: 600  // 1.0.228 — 3000 era eccessivo per delegate complessi
	                        reuseItems: true
	                        interactive: true
                        property bool followTail: true
                        property bool tailFollowPending: false
	                        property bool tailFollowQueued: false
	                        function isNearTail() {
	                            return contentHeight <= height + 2
	                                || contentY >= Math.max(0, contentHeight - height - 48)
	                        }
                        function updateFollowTail() {
                            if (tailFollowPending)
                                return
                            followTail = isNearTail()
                        }
                        function tailContentY() {
                            return Math.max(0, contentHeight - height)
                        }
                        function finishTailFollow() {
                            tailFollowPending = false
                            followTail = isNearTail()
                        }
                        function forceTailFollow() {
    followTail = true
    tailFollowPending = true
    if (tailFollowQueued)
        return
    tailFollowQueued = true
    Qt.callLater(function() {
        tailFollowQueued = false
        if (!rxFrequencyFloatingList)
            return
        var targetY = rxFrequencyFloatingList.tailContentY()
        var distance = Math.abs(rxFrequencyFloatingList.contentY - targetY)
        rxFrequencyFloatingTailAnimation.stop()
        rxFrequencyFloatingList.tailFollowPending = true
        if (distance < 1 || distance > Math.max(12000, rxFrequencyFloatingList.height * 18)) {
            rxFrequencyFloatingList.contentY = targetY
            rxFrequencyFloatingList.finishTailFollow()
            return
        }
        rxFrequencyFloatingTailAnimation.from = rxFrequencyFloatingList.contentY
        rxFrequencyFloatingTailAnimation.to = targetY
        rxFrequencyFloatingTailAnimation.duration = Math.max(180, Math.min(620, 130 + distance * 0.24))
        rxFrequencyFloatingTailAnimation.start()
    })
}
NumberAnimation {
    id: rxFrequencyFloatingTailAnimation
                            target: rxFrequencyFloatingList
                            property: "contentY"
	                            duration: 300
	                            easing.type: Easing.OutCubic
	                            onStopped: rxFrequencyFloatingList.finishTailFollow()
	                        }
	                        Timer {
	                            id: rxFrequencyFloatingTailSettleTimer
	                            interval: 32
	                            repeat: false
	                            onTriggered: {
	                                if (rxFrequencyFloatingList.followTail || rxFrequencyFloatingList.tailFollowPending)
	                                    rxFrequencyFloatingList.forceTailFollow()
	                            }
	                        }
                        Component.onCompleted: Qt.callLater(function() {
                            positionViewAtEnd()
                            updateFollowTail()
                        })
                        onContentYChanged: updateFollowTail()
	                        onContentHeightChanged: {
	                            if (followTail || tailFollowPending)
	                                rxFrequencyFloatingTailSettleTimer.restart()
	                        }
	                        onHeightChanged: {
	                            if (followTail || tailFollowPending)
	                                forceTailFollow()
	                            else
	                                updateFollowTail()
	                        }
	                        onDraggingChanged: {
	                            if (dragging) {
	                                followTail = false
	                                tailFollowPending = false
	                                rxFrequencyFloatingTailAnimation.stop()
	                            }
	                        }
	                        onCountChanged: {
                            forceTailFollow()
                        }
                        property int _ver: decodePanel.rxDecodeListVersion
	                        on_VerChanged: {
                            forceTailFollow()
                        }
                        model: (bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : decodePanel.rxDecodes
	                        // 1.0.186: Animator (render thread) + gate uiQuality !== Low.
	                        // OpacityAnimator/YAnimator non si fermano durante stall main thread,
	                        // pattern allineato a DecodeList.qml:243-251.
	                        add: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            OpacityAnimator { from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        addDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        moveDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        removeDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low"
	                            YAnimator { duration: 100; easing.type: Easing.OutQuad }
	                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            interactive: true
                            width: 8
                        }

	                        delegate: Rectangle {
	                            width: parent ? parent.width - 8 : 100
	                            readonly property bool isPeriodSeparator: !!(modelData && modelData.isSeparator === true)
	                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(24 * fs)
	                            radius: 3
	                            color: isPeriodSeparator ? "transparent" :
	                                   modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.05)

	                            Rectangle {
	                                visible: parent.isPeriodSeparator
	                                anchors.verticalCenter: parent.verticalCenter
	                                anchors.left: parent.left
	                                anchors.right: parent.right
	                                anchors.leftMargin: 12
	                                anchors.rightMargin: 12
	                                height: 1
	                                color: Qt.rgba(0.85, 0.25, 0.25, 0.55)
	                            }

	                            RowLayout {
	                                visible: !parent.isPeriodSeparator
	                                anchors.fill: parent
	                                anchors.margins: 4
		                                spacing: 0
	                                Text { text: modelData.formattedTime || decodePanel.formatUtcForDisplay(modelData.time); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; Layout.preferredWidth: rxFreqFloatingWindow.utcColumnWidth }
	                                Text { text: modelData.db || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.snrColor || (modelData.isTx ? "#f1c40f" : textSecondary); font.bold: modelData.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.dbColumnWidth }
	                                Item { Layout.preferredWidth: rxFreqFloatingWindow.dbDtGapWidth }
	                                Text { text: modelData.dt || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.dtColumnWidth }
	                                Item { Layout.preferredWidth: rxFreqFloatingWindow.gapColumnWidth }
	                                Text { text: modelData.displayMessage || modelData.message || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); font.bold: decodePanel.decodeEntryBold(modelData); font.strikeout: decodePanel.decodeEntryStrikeout(modelData); color: getDxccColor(modelData); Layout.fillWidth: true; elide: messageElideMode(modelData.displayMessage || modelData.message) }
	                                Text { visible: rxFreqFloatingWindow.distanceColumnWidth > 0; text: decodePanel.distanceText(modelData); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.distanceColumnWidth }
                            }

	                            MouseArea {
	                                enabled: !parent.isPeriodSeparator
	                                anchors.fill: parent
	                                onDoubleClicked: { if (!parent.isPeriodSeparator && !modelData.isTx) decodePanel.handleDecodeDoubleClick(modelData) }
	                            }
	                        }
                    }
                }
            }
        }
    }

    // ========== DETACHABLE TX PANEL WINDOW ==========
    Window {
        id: txPanelFloatingWindow
        width: 700
        height: 280
        minimumWidth: 500
        minimumHeight: 200
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "TX Panel - Decodium"
        color: "transparent"

        x: mainWindow.x + 150
        y: mainWindow.y + 400
        Component.onCompleted: mainWindow.restoreFloatingWindowState(txPanelFloatingWindow, "txPanelFloatingWindow", "txPanelDetached", "txPanelMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            txPanelDetached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 12
            border.color: bridge.themeManager.ledRed
            border.width: 2

            // Resize handles
            // Right edge
            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                width: 6
                cursorShape: Qt.SizeHorCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var newW = txPanelFloatingWindow.width + (mouseX - width/2)
                        if (newW >= txPanelFloatingWindow.minimumWidth && newW <= 1200)
                            txPanelFloatingWindow.width = newW
                    }
                }
            }
            // Left edge
            MouseArea {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                width: 6
                cursorShape: Qt.SizeHorCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var delta = mouseX
                        var newW = txPanelFloatingWindow.width - delta
                        if (newW >= txPanelFloatingWindow.minimumWidth && newW <= 1200) {
                            txPanelFloatingWindow.x += delta
                            txPanelFloatingWindow.width = newW
                        }
                    }
                }
            }
            // Bottom edge
            MouseArea {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                height: 6
                cursorShape: Qt.SizeVerCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var newH = txPanelFloatingWindow.height + (mouseY - height/2)
                        if (newH >= txPanelFloatingWindow.minimumHeight && newH <= 600)
                            txPanelFloatingWindow.height = newH
                    }
                }
            }
            // Top edge
            MouseArea {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                height: 6
                cursorShape: Qt.SizeVerCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var delta = mouseY
                        var newH = txPanelFloatingWindow.height - delta
                        if (newH >= txPanelFloatingWindow.minimumHeight && newH <= 600) {
                            txPanelFloatingWindow.y += delta
                            txPanelFloatingWindow.height = newH
                        }
                    }
                }
            }
            // Bottom-right corner
            MouseArea {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 12
                height: 12
                cursorShape: Qt.SizeFDiagCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var newW = txPanelFloatingWindow.width + (mouseX - width/2)
                        if (newW >= txPanelFloatingWindow.minimumWidth && newW <= 1200)
                            txPanelFloatingWindow.width = newW
                        var newH = txPanelFloatingWindow.height + (mouseY - height/2)
                        if (newH >= txPanelFloatingWindow.minimumHeight && newH <= 600)
                            txPanelFloatingWindow.height = newH
                    }
                }
            }
            // Bottom-left corner
            MouseArea {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: 12
                height: 12
                cursorShape: Qt.SizeBDiagCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var delta = mouseX
                        var newW = txPanelFloatingWindow.width - delta
                        if (newW >= txPanelFloatingWindow.minimumWidth && newW <= 1200) {
                            txPanelFloatingWindow.x += delta
                            txPanelFloatingWindow.width = newW
                        }
                        var newH = txPanelFloatingWindow.height + (mouseY - height/2)
                        if (newH >= txPanelFloatingWindow.minimumHeight && newH <= 600)
                            txPanelFloatingWindow.height = newH
                    }
                }
            }
            // Top-right corner
            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                width: 12
                height: 12
                cursorShape: Qt.SizeBDiagCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var newW = txPanelFloatingWindow.width + (mouseX - width/2)
                        if (newW >= txPanelFloatingWindow.minimumWidth && newW <= 1200)
                            txPanelFloatingWindow.width = newW
                        var delta = mouseY
                        var newH = txPanelFloatingWindow.height - delta
                        if (newH >= txPanelFloatingWindow.minimumHeight && newH <= 600) {
                            txPanelFloatingWindow.y += delta
                            txPanelFloatingWindow.height = newH
                        }
                    }
                }
            }
            // Top-left corner
            MouseArea {
                anchors.left: parent.left
                anchors.top: parent.top
                width: 12
                height: 12
                cursorShape: Qt.SizeFDiagCursor
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var deltaX = mouseX
                        var newW = txPanelFloatingWindow.width - deltaX
                        if (newW >= txPanelFloatingWindow.minimumWidth && newW <= 1200) {
                            txPanelFloatingWindow.x += deltaX
                            txPanelFloatingWindow.width = newW
                        }
                        var deltaY = mouseY
                        var newH = txPanelFloatingWindow.height - deltaY
                        if (newH >= txPanelFloatingWindow.minimumHeight && newH <= 600) {
                            txPanelFloatingWindow.y += deltaY
                            txPanelFloatingWindow.height = newH
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                // Header with magnetic dock
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: Qt.rgba(244/255, 67/255, 54/255, 0.25)
                    radius: 6

                    MouseArea {
                        id: txDragArea
                        anchors.fill: parent
                        property point clickPos: Qt.point(0, 0)
                        property bool hasMovedAway: false
                        cursorShape: Qt.SizeAllCursor

                        onPressed: function(mouse) {
                            clickPos = Qt.point(mouse.x, mouse.y)
                            hasMovedAway = false
                        }

                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                                txPanelFloatingWindow.x += delta.x
                                txPanelFloatingWindow.y += delta.y

                                // Get txPanelContainer global position
                                var panelPos = txPanelContainer.mapToGlobal(0, 0)
                                var dockLeft = panelPos.x
                                var dockRight = panelPos.x + txPanelContainer.width
                                var dockTop = panelPos.y
                                var dockBottom = panelPos.y + txPanelContainer.height

                                // Window center position
                                var winCenterX = txPanelFloatingWindow.x + txPanelFloatingWindow.width / 2
                                var winCenterY = txPanelFloatingWindow.y + txPanelFloatingWindow.height / 2

                                // Check if inside dock zone
                                var inDockZone = (winCenterX > dockLeft && winCenterX < dockRight &&
                                                  winCenterY > dockTop && winCenterY < dockBottom)

                                // Check if far from dock zone
                                var farFromDock = (winCenterY > dockBottom + 100) || (winCenterY < dockTop - 100) ||
                                                  (winCenterX < dockLeft - 100) || (winCenterX > dockRight + 100)

                                if (farFromDock) hasMovedAway = true

                                txPanelDockHighlighted = (hasMovedAway && inDockZone)
                            }
                        }

                        onReleased: {
                            if (txPanelDockHighlighted) {
                                txPanelDockHighlighted = false
                                txPanelDetached = false
                                txPanelFloatingWindow.close()
                            }
                            hasMovedAway = false
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

	                        Text { text: "⋮⋮"; font.pixelSize: 12; color: bridge.themeManager.ledRed }
	                        Rectangle { Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5; color: bridge.themeManager.ledRed }
	                        Text { text: "TX Panel"; font.pixelSize: 14; font.bold: true; color: bridge.themeManager.ledRed }
	                        Item { Layout.fillWidth: true }

	                        Rectangle {
	                            Layout.preferredWidth: 42
	                            Layout.preferredHeight: 22
	                            radius: 4
	                            color: txFloatDockMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
	                            border.color: txFloatDockMA.containsMouse ? bridge.themeManager.ledRed : Qt.rgba(244/255, 67/255, 54/255, 0.45)
	                            border.width: 1
	                            Text { anchors.centerIn: parent; text: qsTr("Dock"); font.pixelSize: 10; font.bold: true; color: txFloatDockMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
	                            MouseArea {
	                                id: txFloatDockMA
	                                anchors.fill: parent
	                                hoverEnabled: true
	                                cursorShape: Qt.PointingHandCursor
	                                onClicked: { txPanelDockHighlighted = false; txPanelDetached = false; txPanelMinimized = false; txPanelFloatingWindow.close() }
	                            }
	                            ToolTip.visible: txFloatDockMA.containsMouse
	                            ToolTip.text: qsTr("Dock TX Panel")
	                        }

	                        Rectangle {
	                            Layout.preferredWidth: 24
	                            Layout.preferredHeight: 24
	                            radius: 4
                            color: txFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 16; font.bold: true; color: txFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: txFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { txPanelMinimized = true; txPanelFloatingWindow.hide() }
                            }
	                        }

	                        Rectangle {
	                            Layout.preferredWidth: 24
	                            Layout.preferredHeight: 24
	                            radius: 4
                            color: txFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 11; font.bold: true; color: txFloatCloseMA.containsMouse ? bridge.themeManager.ledRed : textPrimary }
                            MouseArea { id: txFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { txPanelDetached = false; txPanelMinimized = false; txPanelFloatingWindow.close() }
                            }
                        }
                    }
                }

                // Content - TxPanel
                TxPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    engine: bridge
                    showAsyncIcon: mainWindow.asyncIconVisible
                    handleLogPrompt: false
                    onMamWindowRequested: mamWindow.open()
                }
            }
        }
    }

    // ===== GAP 3 — Pannelli floating draggabili =====

    // TimeSyncPanel — sotto il blocco Setup/REC/WAV, togglabile da menu
    Item {
        id: timeSyncOverlay
        readonly property int panelMargin: 12
        readonly property int topRailY: 10
        property bool userPositioned: !!bridge.getSetting("uiTimeSyncPanelUserPositioned", false)
        property real savedX: Number(bridge.getSetting("uiTimeSyncPanelX", -1))
        property real savedY: Number(bridge.getSetting("uiTimeSyncPanelY", -1))
        readonly property bool headerWrapped: headerFlow.height > 110
        readonly property bool headerRightRailOccupied: headerFlow.childrenRect.x + headerFlow.childrenRect.width
                                                   > mainWindow.width - width - panelMargin * 2
        readonly property bool dockInsideHeader: headerWrapped || headerRightRailOccupied
        function boundedX(value) {
            return Math.round(Math.min(Math.max(0, Number(value) || 0),
                                       Math.max(0, mainWindow.width - width)))
        }
        function boundedY(value) {
            return Math.round(Math.min(Math.max(0, Number(value) || 0),
                                       Math.max(0, mainWindow.height - height)))
        }
        function savePosition() {
            userPositioned = true
            savedX = boundedX(x)
            savedY = boundedY(y)
            x = savedX
            y = savedY
            bridge.setSetting("uiTimeSyncPanelUserPositioned", true)
            bridge.setSetting("uiTimeSyncPanelX", savedX)
            bridge.setSetting("uiTimeSyncPanelY", savedY)
        }
        function clampSavedPosition() {
            if (!userPositioned)
                return
            savedX = boundedX(savedX)
            savedY = boundedY(savedY)
            x = savedX
            y = savedY
            bridge.setSetting("uiTimeSyncPanelX", savedX)
            bridge.setSetting("uiTimeSyncPanelY", savedY)
        }
        function headerRows() {
            var rows = []
            var children = headerFlow.children
            for (var i = 0; i < children.length; ++i) {
                var child = children[i]
                if (!child || !child.visible || child.width <= 0 || child.height <= 0)
                    continue

                var top = Math.round(child.y)
                var row = null
                for (var r = 0; r < rows.length; ++r) {
                    if (Math.abs(rows[r].top - top) <= 2) {
                        row = rows[r]
                        break
                    }
                }
                if (!row) {
                    row = {
                        top: top,
                        left: child.x,
                        right: child.x + child.width,
                        height: child.height
                    }
                    rows.push(row)
                } else {
                    row.left = Math.min(row.left, child.x)
                    row.right = Math.max(row.right, child.x + child.width)
                    row.height = Math.max(row.height, child.height)
                }
            }
            rows.sort(function(a, b) { return b.top - a.top })
            return rows
        }
        function headerSlot() {
            if (!dockInsideHeader)
                return { valid: false, x: 0, y: 0 }

            var rows = headerRows()
            var headerLeft = headerBar.x + headerFlow.x
            var headerTop = headerBar.y + headerFlow.y
            var rightLimit = mainWindow.width - panelMargin
            var maxHeaderY = Math.max(topRailY, headerBar.y + headerBar.height - height - 6)

            for (var i = 0; i < rows.length; ++i) {
                var row = rows[i]
                var rowY = headerTop + row.top + Math.max(0, (row.height - height) / 2)
                rowY = Math.min(Math.max(topRailY, rowY), maxHeaderY)

                var afterRow = headerLeft + row.right + headerFlow.spacing
                if (afterRow + width <= rightLimit)
                    return { valid: true, x: Math.max(panelMargin, afterRow), y: rowY }

                var beforeRow = headerLeft + row.left - headerFlow.spacing - width
                if (beforeRow >= panelMargin)
                    return { valid: true, x: beforeRow, y: rowY }
            }

            return { valid: false, x: 0, y: 0 }
        }
        function utilityButtonsSlot() {
            if (!headerUtilityButtons || !headerUtilityButtons.visible || headerUtilityButtons.width <= 0)
                return { valid: false, x: 0, y: 0 }

            var mapped = headerUtilityButtons.mapToItem(null, 0, 38)
            var maxX = Math.max(panelMargin, mainWindow.width - width - panelMargin)
            var maxY = Math.max(topRailY, mainWindow.height - height - panelMargin)
            return {
                valid: true,
                x: Math.min(Math.max(panelMargin, mapped.x), maxX),
                y: Math.min(Math.max(topRailY, mapped.y), maxY)
            }
        }
        function automaticX() {
            var utilitySlot = utilityButtonsSlot()
            if (utilitySlot.valid)
                return utilitySlot.x

            var slot = headerSlot()
            if (slot.valid)
                return slot.x
            return Math.max(panelMargin, mainWindow.width - width - panelMargin)
        }
        function automaticY() {
            var utilitySlot = utilityButtonsSlot()
            if (utilitySlot.valid)
                return utilitySlot.y

            var slot = headerSlot()
            if (slot.valid)
                return slot.y
            return topRailY
        }
        visible: timeSyncPanelVisible
        z: 200
        x: userPositioned ? boundedX(savedX) : automaticX()
        y: userPositioned
           ? boundedY(savedY)
           : Math.min(Math.max(topRailY, automaticY()),
                      Math.max(topRailY, mainWindow.height - height - panelMargin))
        width: Math.min(360, Math.max(280, mainWindow.width - panelMargin * 2))
        height: timeSyncLoader.item ? timeSyncLoader.item.implicitHeight : 28
        onWidthChanged: Qt.callLater(clampSavedPosition)
        onHeightChanged: Qt.callLater(clampSavedPosition)

        Loader {
            id: timeSyncLoader
            anchors.fill: parent
            active: timeSyncPanelVisible
            source: "../panels/TimeSyncPanel.qml"
            onLoaded: {
                if (item) {
                    item.dragTarget = timeSyncOverlay
                    item.showCloseButton = true
                }
            }
        }

        Connections {
            target: timeSyncLoader.item
            ignoreUnknownSignals: true
            function onCloseRequested() {
                timeSyncPanelVisible = false
            }
        }
    }

    // ActiveStationsPanel — posizione sotto TimeSyncPanel
    Item {
        id: activeStationsOverlay
        visible: activeStationsPanelVisible
        z: 200
        x: Math.max(12, mainWindow.width - width - 12)
        y: Math.min(timeSyncPanelVisible ? timeSyncOverlay.y + timeSyncOverlay.height + 8
                                         : Math.max(100, headerBar.y + headerBar.height + 8),
                    Math.max(12, mainWindow.height - height - 12))
        width: Math.min(360, Math.max(280, mainWindow.width - 24))
        height: 280

        MouseArea {
            anchors.fill: parent
            drag.target: activeStationsOverlay
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0; drag.maximumX: Math.max(0, mainWindow.width - activeStationsOverlay.width)
            drag.minimumY: 0; drag.maximumY: Math.max(0, mainWindow.height - activeStationsOverlay.height)
        }

        Loader {
            id: activeStationsLoader
            anchors.fill: parent
            active: activeStationsPanelVisible
            source: "../panels/ActiveStationsPanel.qml"
        }

        Connections {
            target: activeStationsLoader.item
            ignoreUnknownSignals: true
            function onCloseRequested() {
                activeStationsPanelVisible = false
            }
        }
    }

    // CallerQueuePanel — visibile solo in Fox mode, angolo in basso a destra
    Item {
        id: callerQueueOverlay
        visible: bridge.foxMode && callerQueuePanelVisible
        z: 200
        x: mainWindow.width - width - 12
        y: mainWindow.height - height - 200
        width: 260
        height: callerQueueLoader.item ? callerQueueLoader.item.implicitHeight : 50

        MouseArea {
            anchors.fill: parent
            drag.target: callerQueueOverlay
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0; drag.maximumX: mainWindow.width - callerQueueOverlay.width
            drag.minimumY: 0; drag.maximumY: mainWindow.height - 50
        }

        Loader {
            id: callerQueueLoader
            anchors.fill: parent
            active: bridge.foxMode && callerQueuePanelVisible
            source: "../panels/CallerQueuePanel.qml"
        }

        Connections {
            target: callerQueueLoader.item
            ignoreUnknownSignals: true
            function onCloseRequested() {
                callerQueuePanelVisible = false
            }
        }
    }

    Connections {
        target: bridge
        ignoreUnknownSignals: true
        function onFoxModeChanged() {
            callerQueuePanelVisible = bridge.foxMode
        }
    }

    // AstroPanel — C14: calcolatrice astronomica EME, angolo in basso a sinistra
    Item {
        id: astroPanelOverlay
        visible: astroPanelVisible
        z: 200
        x: 12
        y: mainWindow.height - height - 180
        width: 320
        height: 230

        MouseArea {
            anchors.fill: parent
            drag.target: astroPanelOverlay
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0; drag.maximumX: mainWindow.width - astroPanelOverlay.width
            drag.minimumY: 0; drag.maximumY: mainWindow.height - 50
        }

        Loader {
            id: astroPanelLoader
            anchors.fill: parent
            active: astroPanelVisible
            source: "../panels/AstroPanel.qml"
        }

        Connections {
            target: astroPanelLoader.item
            ignoreUnknownSignals: true
            function onCloseRequested() {
                astroPanelVisible = false
            }
        }
    }

    // DxClusterPanel — spot DX Cluster in tempo reale, draggabile
    DxClusterPanel {
        id: dxClusterOverlay
        visible: dxClusterPanelVisible
        z: 9000
        x: Math.max(0, mainWindow.width - width - 12)
        y: 60
        onCloseRequested: dxClusterPanelVisible = false
    }

    // ── DecoSyncTime Monitor floating window ─────────────────────────────
    // 1.0.164 fase 5: pannello monitoring per tutti i source time sync.
    Window {
        id: decoSyncMonitorWindow
        width: 480
        height: 380
        minimumWidth: 360
        minimumHeight: 260
        visible: mainWindow.decoSyncMonitorVisible
        flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
             | Qt.WindowCloseButtonHint
        title: "DecoSyncTime Monitor"
        color: "transparent"

        Component.onCompleted: {
            mainWindow.restoreFloatingWindowState(decoSyncMonitorWindow, "decoSyncMonitorWindow", "", "")
            if (mainWindow.decoSyncMonitorVisible)
                decoSyncMonitorWindow.show()
        }
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibilityChanged: {
            if (visibility === Window.Minimized)
                mainWindow.scheduleWindowStateSave()
        }
        onClosing: function(close) {
            if (!mainWindow.applicationClosing)
                mainWindow.decoSyncMonitorVisible = false
            close.accepted = true
        }

        DecoSyncPanel {
            anchors.fill: parent
        }
    }

    function openDecoSyncMonitor() {
        decoSyncMonitorVisible = true
        decoSyncMonitorWindow.show()
        decoSyncMonitorWindow.raise()
        decoSyncMonitorWindow.requestActivate()
    }

    // ── Splash Screen Decodium 4.0 Core Shannon ─────────────────────────────
    SplashScreen {
        id: splash
        anchors.fill: parent
        visible: true
        onFinished: splash.visible = false
    }
}
