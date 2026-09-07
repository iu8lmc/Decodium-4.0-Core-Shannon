/* Decodium 4.0 - Main Window
 * Based on WSJT-X by K1JT et al.
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
// import Qt.labs.settings 1.1  // non disponibile in questa build Qt
import "components"

ApplicationWindow {
    id: mainWindow
    font.family: bridge ? bridge.fontSettingFamily("Font", Qt.platform.os === "windows" ? "Segoe UI" : (Qt.platform.os === "osx" ? "Helvetica Neue" : ""), 10)
                        : (Qt.platform.os === "windows" ? "Segoe UI" : (Qt.platform.os === "osx" ? "Helvetica Neue" : ""))
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
    property bool superFoxOptionEnabled: bridge ? settingBool("SuperFox", true) : true
    readonly property string dxpeditionModeLabel: !bridge || bridge.mode !== "FT8" ? ""
        : bridge.houndMode ? (superFoxOptionEnabled ? "SuperHound" : "Hound")
        : bridge.foxMode ? (superFoxOptionEnabled ? "SuperFox" : "Fox") : ""
    title: "Decodium 4.0 — " + (bridge ? bridge.mode : "")
        + (dxpeditionModeLabel ? " [" + dxpeditionModeLabel + "]" : "")
        + " — " + (bridge ? bridge.callsign : "")
    property bool windowStateRestoreInProgress: true
    // Persist the native maximised state separately from the last usable
    // windowed geometry.  On Windows QWindow reports the maximised dimensions
    // through width/height; saving those as normal geometry reopens an
    // oversized window while losing the maximised state.
    property bool mainWindowMaximized: false
    property int normalWindowX: 0
    property int normalWindowY: 0
    property int normalWindowWidth: preferredMinimumWidth
    property int normalWindowHeight: 800
    property int floatingGeometryInteractionDepth: 0
    property bool deferredWindowStateSave: false
    readonly property bool txVisualActive: !!(bridge && (bridge.transmitting || bridge.tuning))
    readonly property bool txPttPending: !!(bridge && bridge.pttPending && !bridge.pttConfirmed)
    // Build the expensive visual surfaces after the first interactive frame on
    // every platform.  The Waterfall loader also owns PanadapterItem, so this
    // defers palette/scene-graph setup together with the visual itself.
    readonly property bool startupVisualStagingEnabled: true
    property bool startupWaterfallVisualReady: !startupVisualStagingEnabled
    property bool startupLiveMapVisualReady: !startupVisualStagingEnabled
    property bool startupSettingsSavePending: false
    // Settings is a QML Dialog in this main window, whereas detached panels
    // are native top-level Windows.  On Windows an always-on-top panel stays
    // above the QML overlay and can make Settings visible but impossible to
    // click.  Keep the exact visible set so it can be restored unchanged when
    // the modal dialog closes.
    property var settingsSuspendedTopmostPopouts: []
    property bool settingsTopmostPopoutsSuspended: false
    property bool decodePanelLayoutSaved: false
    property int savedPeriod1PanelWidth: 400
    property int savedRxFreqPanelWidth: 400
    property int savedLiveMapPanelWidth: 360
    property int savedDxClusterColumnWidth: Math.max(320, Number(bridge.getSetting("uiDxClusterColumnWidth", 380)))  // 1.0.385 — larghezza 4ª colonna DX Cluster
    property double startupCompletedStartedMs: 0

    function startupElapsedMs() {
        return startupCompletedStartedMs > 0 ? Math.round(Date.now() - startupCompletedStartedMs) : -1
    }

    function startupLog(phase) {
        console.log("Main.qml startup +" + startupElapsedMs() + " ms: " + phase)
    }

    function startupVisualStageCanRun(stage) {
        if (!startupVisualStagingEnabled || !bridge)
            return true
        if (bridge.transmitting || bridge.tuning)
            return false
        // Do not let a long first decode keep the normal dashboard hidden.
        // Prefer a quiet FT slot, but guarantee that both visuals appear.
        var deadlineMs = stage === "waterfall" ? 5000 : 7500
        if (startupElapsedMs() >= deadlineMs)
            return true
        var mode = String(bridge.mode || "").toUpperCase()
        if (mode === "FT8") {
            var progress = Number(bridge.periodProgress || 0)
            return progress >= 8 && progress <= 22
        }
        return true
    }

    function maybeFinishStartupWaterfallVisualStage(reason) {
        if (startupWaterfallVisualReady)
            return
        if (!startupVisualStageCanRun("waterfall")) {
            startupWaterfallStageRetryTimer.restart()
            return
        }
        finishStartupWaterfallVisualStage(reason)
    }

    function maybeFinishStartupLiveMapVisualStage(reason) {
        if (startupLiveMapVisualReady)
            return
        if (!startupWaterfallVisualReady || !startupVisualStageCanRun("livemap")) {
            startupLiveMapStageRetryTimer.restart()
            return
        }
        finishStartupLiveMapVisualStage(reason)
    }

    function finishStartupWaterfallVisualStage(reason) {
        if (startupWaterfallVisualReady)
            return
        startupWaterfallVisualReady = true
        startupLog("waterfall visual stage ready" + (reason ? " (" + reason + ")" : ""))
        syncSpectrumVisibility()
    }

    function finishStartupLiveMapVisualStage(reason) {
        if (startupLiveMapVisualReady)
            return
        startupLiveMapVisualReady = true
        startupLog("live map visual stage ready" + (reason ? " (" + reason + ")" : ""))
        if (startupSettingsSavePending) {
            startupSettingsSavePending = false
            saveTimer.restart()
        }
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
                height: Math.round(safeNumber(g.height, preferredMinimumHeight)),
                screen: screen
            }
        }

        var widthValue = safeNumber(screen.width, 0)
        var heightValue = safeNumber(screen.height, 0)
        if (widthValue > 0 && heightValue > 0) {
            return {
                x: Math.round(safeNumber(screen.virtualX, 0)),
                y: Math.round(safeNumber(screen.virtualY, 0)),
                width: Math.round(widthValue),
                height: Math.round(heightValue),
                screen: screen
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

    function coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return !!fallback
        if (typeof value === "boolean")
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return !!fallback
    }

    function settingBool(key, fallback) {
        return coerceBool(safeBridgeSetting(key, fallback), fallback)
    }

    function persistUiSetting(key, value) {
        if (!bridge)
            return
        bridge.setSetting(key, value)
        if (!windowStateRestoreInProgress)
            scheduleSave()
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
                    y: Math.max(g.y, Math.min(savedY, g.y + Math.max(0, g.height - winH))),
                    screen: g.screen
                }
            }
        }

        var fallback = screens[0]
        return {
            x: Math.round(fallback.x + Math.max(0, (fallback.width - winW) / 2)),
            y: Math.round(fallback.y + Math.max(0, (fallback.height - winH) / 2)),
            screen: fallback.screen
        }
    }

    function geometryForWindowScreen(windowRef) {
        var screens = availableScreenGeometries()
        if (screens.length === 0)
            return null

        // Use a point near the draggable header rather than the window centre:
        // an oversized window may still have its centre on the monitor it is
        // leaving, and some platforms update QWindow::screen only after move.
        if (windowRef) {
            var anchorX = safeNumber(windowRef.x, 0)
                          + Math.min(64, Math.max(1, safeNumber(windowRef.width, 1) / 2))
            var anchorY = safeNumber(windowRef.y, 0)
                          + Math.min(32, Math.max(1, safeNumber(windowRef.height, 1) / 2))
            for (var i = 0; i < screens.length; ++i) {
                var candidate = screens[i]
                if (anchorX >= candidate.x && anchorX < candidate.x + candidate.width
                        && anchorY >= candidate.y && anchorY < candidate.y + candidate.height)
                    return candidate
            }
        }

        if (windowRef && windowRef.screen) {
            for (var j = 0; j < screens.length; ++j) {
                if (screens[j].screen === windowRef.screen)
                    return screens[j]
            }
        }
        return screens[0]
    }

    function fitWindowSizeToGeometry(windowRef, target, preserveAspectRatio) {
        if (!windowRef || !target)
            return

        var currentWidth = safeNumber(windowRef.width, 0)
        var currentHeight = safeNumber(windowRef.height, 0)
        var minimumWidthValue = Math.max(1, safeNumber(windowRef.minimumWidth, 1))
        var minimumHeightValue = Math.max(1, safeNumber(windowRef.minimumHeight, 1))
        var declaredMaximumWidth = Math.max(minimumWidthValue,
                                            safeNumber(windowRef.maximumWidth, 10000))
        var declaredMaximumHeight = Math.max(minimumHeightValue,
                                             safeNumber(windowRef.maximumHeight, 6000))
        var maximumWidth = Math.max(minimumWidthValue,
                                    Math.min(declaredMaximumWidth, target.width - 16))
        var maximumHeight = Math.max(minimumHeightValue,
                                     Math.min(declaredMaximumHeight, target.height - 16))
        var aspectRatio = safeNumber(preserveAspectRatio, 0)

        if (aspectRatio > 0) {
            maximumWidth = Math.max(minimumWidthValue,
                                    Math.min(maximumWidth, maximumHeight * aspectRatio))
            var proportionalWidth = Math.max(minimumWidthValue,
                                             Math.min(currentWidth, maximumWidth))
            var proportionalHeight = Math.round(proportionalWidth / aspectRatio)
            if (proportionalHeight < minimumHeightValue) {
                proportionalHeight = minimumHeightValue
                proportionalWidth = Math.round(proportionalHeight * aspectRatio)
            }
            windowRef.width = Math.round(proportionalWidth)
            windowRef.height = proportionalHeight
            return
        }

        windowRef.width = Math.max(minimumWidthValue, Math.min(currentWidth, maximumWidth))
        windowRef.height = Math.max(minimumHeightValue, Math.min(currentHeight, maximumHeight))
    }

    // A saved window geometry may come from a larger monitor or from a
    // previous multi-monitor setup.  Clamping only x/y leaves the window
    // wider/taller than the current screen, which makes the right-hand part
    // of panels such as Signal RX unreachable until the user resizes it.
    function fitWindowSizeToAvailableScreen(windowRef, savedX, savedY) {
        if (!windowRef)
            return

        var screens = availableScreenGeometries()
        if (screens.length === 0)
            return

        var savedWidth = safeNumber(windowRef.width, 0)
        var savedHeight = safeNumber(windowRef.height, 0)
        var centerX = safeNumber(savedX, NaN) + savedWidth / 2
        var centerY = safeNumber(savedY, NaN) + savedHeight / 2
        var target = null

        if (isFinite(centerX) && isFinite(centerY)) {
            for (var i = 0; i < screens.length; ++i) {
                var candidate = screens[i]
                if (centerX >= candidate.x && centerX < candidate.x + candidate.width &&
                    centerY >= candidate.y && centerY < candidate.y + candidate.height) {
                    target = candidate
                    break
                }
            }
        }
        if (!target)
            target = screens[0]

        fitWindowSizeToGeometry(windowRef, target)
    }

    function dragFloatingWindowToGlobal(windowRef, pressWindowPos, pressGlobalPos, currentGlobalPos) {
        if (!windowRef || !pressWindowPos || !pressGlobalPos || !currentGlobalPos)
            return
        var nextX = Math.round(pressWindowPos.x + currentGlobalPos.x - pressGlobalPos.x)
        var nextY = Math.round(pressWindowPos.y + currentGlobalPos.y - pressGlobalPos.y)
        if (isFinite(nextX))
            windowRef.x = nextX
        if (isFinite(nextY))
            windowRef.y = nextY
    }

    function startNativeFloatingWindowMove(windowRef) {
        if (!windowRef || typeof windowRef.startSystemMove !== "function")
            return false
        try {
            return windowRef.startSystemMove()
        } catch(e) {
            console.log("startSystemMove failed: " + e)
        }
        return false
    }

    function finishFloatingWindowDrag(windowRef, preserveAspectRatio) {
        if (!windowRef)
            return
        var target = geometryForWindowScreen(windowRef)
        fitWindowSizeToGeometry(windowRef, target, preserveAspectRatio)
        if (target) {
            if (target.screen && windowRef.screen !== target.screen)
                windowRef.screen = target.screen
            windowRef.x = Math.max(target.x,
                                   Math.min(windowRef.x,
                                            target.x + Math.max(0, target.width - windowRef.width)))
            windowRef.y = Math.max(target.y,
                                   Math.min(windowRef.y,
                                            target.y + Math.max(0, target.height - windowRef.height)))
        }
        scheduleWindowStateSave()
    }

    function beginFloatingGeometryInteraction() {
        floatingGeometryInteractionDepth += 1
        deferredWindowStateSave = true
        if (typeof windowStateSaveTimer !== "undefined" && windowStateSaveTimer)
            windowStateSaveTimer.stop()
    }

    function endFloatingGeometryInteraction() {
        floatingGeometryInteractionDepth = Math.max(0, floatingGeometryInteractionDepth - 1)
        if (floatingGeometryInteractionDepth === 0 && deferredWindowStateSave)
            scheduleWindowStateSave(true)
    }

    function resetFloatingWindowGeometry(windowRef, preferredWidth, preferredHeight) {
        if (!windowRef)
            return

        var screens = availableScreenGeometries()
        if (screens.length === 0)
            return
        var target = screens[0]
        var minimumWidthValue = Math.max(1, safeNumber(windowRef.minimumWidth, 1))
        var minimumHeightValue = Math.max(1, safeNumber(windowRef.minimumHeight, 1))
        var availableWidth = Math.max(minimumWidthValue, safeNumber(target.width, preferredWidth) - 16)
        var availableHeight = Math.max(minimumHeightValue, safeNumber(target.height, preferredHeight) - 16)
        var resetWidth = Math.max(minimumWidthValue,
                                  Math.min(Math.round(preferredWidth), availableWidth))
        var resetHeight = Math.max(minimumHeightValue,
                                   Math.min(Math.round(preferredHeight), availableHeight))

        if (target.screen && windowRef.screen !== target.screen)
            windowRef.screen = target.screen
        windowRef.width = resetWidth
        windowRef.height = resetHeight
        windowRef.x = Math.round(target.x + Math.max(0, (target.width - resetWidth) / 2))
        windowRef.y = Math.round(target.y + Math.max(0, (target.height - resetHeight) / 2))
    }

    function safeStoredPanelWidth(value, fallback, minimum) {
        var numeric = Number(value)
        if (!isFinite(numeric) || numeric <= 0)
            numeric = fallback
        return Math.max(minimum, Math.round(numeric))
    }

    // Stadio 1: la larghezza-valore vive ora sugli SLOT-HOST (colSlot0/1/2) per POSIZIONE.
    // Le chiavi restano mappate per posizione (slot0=uiFullSpectrumPanelWidth,
    // slot1=uiSignalRxPanelWidth, slot2=uiLiveMapPanelWidth) indipendentemente da quale
    // pannello le occupa. I minimi sono per-slot (classicMinWidthForSlot) ma il default
    // (mappa = ordine attuale) coincide esattamente col comportamento precedente.
    function restoreDecodePanelWidths() {
        if (typeof decodePanelsSplit === "undefined" || !decodePanelsSplit ||
            typeof colSlot0 === "undefined" || !colSlot0 ||
            typeof colSlot1 === "undefined" || !colSlot1 ||
            typeof colSlot2 === "undefined" || !colSlot2) {
            Qt.callLater(restoreDecodePanelWidths)
            return
        }

        var totalWidth = decodePanelsSplit.width
        if (!(totalWidth > 0)) {
            Qt.callLater(restoreDecodePanelWidths)
            return
        }

        colSlot2.targetPanelWidth = safeStoredPanelWidth(savedLiveMapPanelWidth, 360, 280)

        if (!decodePanelLayoutSaved) {
            colSlot0.userDraggedSplit = false
            colSlot0.applyCenterSplit()
            return
        }

        // Minimi per posizione (seguono il pannello che occupa lo slot).
        var slot0Min = classicMinWidthForSlot(0)
        var slot1Min = classicMinWidthForSlot(1)
        var savedSlot0 = safeStoredPanelWidth(savedPeriod1PanelWidth, 400, slot0Min)
        var savedSlot1 = safeStoredPanelWidth(savedRxFreqPanelWidth, 400, slot1Min)
        var savedSlot2 = safeStoredPanelWidth(savedLiveMapPanelWidth, 360, 280)
        var slot2Width = 0

        // Lo slot2 mostra una larghezza solo se NON è collassato (Live Map visibile lì).
        if (!classicSlotCollapsed(2)) {
            var slot2Min = classicMinWidthForSlot(2)
            var maxSlot2Width = Math.max(slot2Min, totalWidth - slot0Min - slot1Min)
            slot2Width = Math.min(savedSlot2, maxSlot2Width)
        }

        // 1.0.385 — la 4ª colonna (DX Cluster) occupa larghezza solo se non collassata.
        var slot3Width = 0
        if (!classicSlotCollapsed(3)) {
            var slot3Min = classicMinWidthForSlot(3)
            var savedSlot3 = safeStoredPanelWidth(savedDxClusterColumnWidth, 380, slot3Min)
            var maxSlot3Width = Math.max(slot3Min, totalWidth - slot0Min - slot1Min - slot2Width)
            slot3Width = Math.min(savedSlot3, maxSlot3Width)
        }

        var remainingWidth = Math.max(slot0Min + slot1Min, totalWidth - slot2Width - slot3Width)
        var savedCombined = Math.max(1, savedSlot0 + savedSlot1)
        var slot0Width = Math.round(remainingWidth * (savedSlot0 / savedCombined))
        slot0Width = Math.max(slot0Min, Math.min(slot0Width, remainingWidth - slot1Min))
        var slot1Width = Math.max(slot1Min, remainingWidth - slot0Width)

        colSlot0.userDraggedSplit = true
        colSlot0.targetPanelWidth = slot0Width
        colSlot1.targetPanelWidth = slot1Width
        if (!classicSlotCollapsed(2))
            colSlot2.targetPanelWidth = slot2Width
        if (!classicSlotCollapsed(3) && typeof colSlot3 !== "undefined" && colSlot3)
            colSlot3.targetPanelWidth = slot3Width
    }

    function captureDecodePanelWidths() {
        var layoutSettings = ({})
        if (!bridge)
            return layoutSettings

        if (typeof colSlot0 !== "undefined" && colSlot0 &&
            !colSlot0.slotCollapsed && colSlot0.width >= classicMinWidthForSlot(0)) {
            savedPeriod1PanelWidth = Math.round(colSlot0.width)
            layoutSettings.uiFullSpectrumPanelWidth = savedPeriod1PanelWidth
        }

        if (typeof colSlot1 !== "undefined" && colSlot1 &&
            !colSlot1.slotCollapsed && colSlot1.width >= classicMinWidthForSlot(1)) {
            savedRxFreqPanelWidth = Math.round(colSlot1.width)
            layoutSettings.uiSignalRxPanelWidth = savedRxFreqPanelWidth
        }

        if (typeof colSlot2 !== "undefined" && colSlot2) {
            var slot2Width = !colSlot2.slotCollapsed ? colSlot2.width : colSlot2.targetPanelWidth
            if (slot2Width >= 280) {
                savedLiveMapPanelWidth = Math.round(slot2Width)
                colSlot2.targetPanelWidth = savedLiveMapPanelWidth
                layoutSettings.uiLiveMapPanelWidth = savedLiveMapPanelWidth
            }
        }

        // 1.0.385 — persisti la larghezza della 4ª colonna (DX Cluster)
        if (typeof colSlot3 !== "undefined" && colSlot3) {
            var slot3Width = !colSlot3.slotCollapsed ? colSlot3.width : colSlot3.targetPanelWidth
            if (slot3Width >= 320) {
                savedDxClusterColumnWidth = Math.round(slot3Width)
                colSlot3.targetPanelWidth = savedDxClusterColumnWidth
                layoutSettings.uiDxClusterColumnWidth = savedDxClusterColumnWidth
            }
        }

        decodePanelLayoutSaved = true
        layoutSettings.uiDecodePanelsLayoutSaved = true
        return layoutSettings
    }

    Component.onCompleted: {
        startupCompletedStartedMs = Date.now()
        startupLog("Component.onCompleted begin")
        syncSpectrumVisibility()
        callerQueuePanelVisible = settingBool("uiCallerQueuePanelVisible", !!(bridge && bridge.foxMode))
        startupLog("fox/caller queue state restored")
        decodePanelLayoutSaved = settingBool("uiDecodePanelsLayoutSaved", false)
        // 1.0.338: l'app parte SEMPRE in classico (footer+waterfall+TX). La
        // DX-Pedition si attiva solo a mano da Impostazioni e NON si auto-ripristina
        // all'avvio (scelta utente). Azzero anche il setting per coerenza col checkbox.
        if (settingBool("uiDxPeditionMode", false))
            bridge.setSetting("uiDxPeditionMode", false)
        dxPeditionMode = false
        startupLog("dx-pedition mode: always start classic (restore disabled by user)")
        savedPeriod1PanelWidth = safeStoredPanelWidth(safeBridgeSetting("uiFullSpectrumPanelWidth", 400), 400, 360)
        savedRxFreqPanelWidth = safeStoredPanelWidth(safeBridgeSetting("uiSignalRxPanelWidth", 400), 400, 360)
        savedLiveMapPanelWidth = safeStoredPanelWidth(safeBridgeSetting("uiLiveMapPanelWidth", 360), 360, 280)
        fsLoadColumns()
        startupLog("decode panel settings restored")

        var state = safeWindowState("mainWindow")
        startupLog("main window state read")
        mainWindowMaximized = state.maximized !== undefined
                ? coerceBool(state.maximized, false)
                : false
        var restoredWidth = safeNumber(state.width, width)
        var restoredHeight = safeNumber(state.height, height)
        if (restoredWidth > 0) width = restoredWidth
        if (restoredHeight > 0) height = restoredHeight
        var pos
        var restoredX = safeNumber(state.x, NaN)
        var restoredY = safeNumber(state.y, NaN)
        fitWindowSizeToAvailableScreen(mainWindow, restoredX, restoredY)
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
        normalWindowX = Math.round(x)
        normalWindowY = Math.round(y)
        normalWindowWidth = Math.round(width)
        normalWindowHeight = Math.round(height)
        startupLog("main window geometry applied x=" + x + " y=" + y + " w=" + width + " h=" + height)

        // Force the native state explicitly on Windows. BootLoader repeats this
        // presentation call after configuring the graphics backend, so it must
        // preserve rather than reset a restored maximised state.
        showRestoredWindowState()
        windowStateRestoreInProgress = false
        raise()
        requestActivate()
        startupLog("main window show/raise/requestActivate done")
        startupLiveMapPopoutRestoreTimer.restart()
        // The clock is re-parented into contentItem during header creation.  At
        // that point the final window geometry may not exist yet, so apply the
        // persisted position once more after the first layout pass.
        Qt.callLater(function() {
            mainWindow.applyWorldClockSlot()
            Qt.callLater(mainWindow.applyWorldClockSlot)
        })
        Qt.callLater(restoreDecodePanelWidths)
        // Stadio 1+2: applica l'ordine pannelli persistito re-parentando i 4 pannelli
        // (3 colonne + TX area) negli slot-host indicati dalla mappa (default = ordine
        // attuale -> no-op; una mappa salvata a 3 elementi migra con "txpanel" in slot 3).
        Qt.callLater(applyClassicColumnOrder)
        bridge.notifyMainQmlReady()
        startupLog("bridge notified ready")
        if (startupVisualStagingEnabled) {
            startupLog("visual startup staging armed")
            startupWaterfallStageTimer.restart()
            startupLiveMapStageTimer.restart()
        }
        firstUseWarmupTimer.restart()
        console.log("Main.qml window shown at " + x + "," + y + " size " + width + "x" + height)
    }

    // Altezza pannello waterfall — caricata da bridge.uiWaterfallHeight.
    // Default 420px così all'avvio la cascata è già ben aperta (feedback IK8OLM).
    property int  waterfallPanelHeight: bridge.uiWaterfallHeight > 0 ? bridge.uiWaterfallHeight : 420

    // Timer che salva le impostazioni 2s dopo ogni modifica (debounce)
    Timer {
        id: saveTimer
        objectName: "settingsSaveTimer"
        interval: 2000
        repeat: false
        onTriggered: bridge.saveSettingsAsync()
    }
    Timer {
        id: windowStateSaveTimer
        objectName: "windowStateSaveTimer"
        interval: 500
        repeat: false
        onTriggered: persistWindowLayouts()
    }
    Timer {
        id: normalWindowGeometryTimer
        objectName: "normalWindowGeometryTimer"
        interval: 180
        repeat: false
        onTriggered: captureNormalWindowGeometry()
    }
    Timer {
        id: firstUseWarmupTimer
        interval: 30000
        repeat: false
        running: false
        onTriggered: {
            if (bridge && (bridge.monitoring || bridge.transmitting || bridge.tuning || bridge.decoding)) {
                interval = 10000
                restart()
                return
            }
            if (bridge && bridge.warmLogCacheAsync)
                bridge.warmLogCacheAsync()
            // SettingsDialog contains all tabs in one StackLayout.  Loading it
            // on the first click can pause the GUI while RX is active. Warm it
            // once during a pressure-free window; low-end machines retry later
            // instead of moving that cost into decoder delivery.
            if (bridge && bridge.cpuPressureNow && bridge.cpuPressureNow()) {
                interval = 10000
                restart()
                return
            }
            if (!settingsDialogLoader.item) {
                settingsDialogLoader.pendingAction = function(item) {
                    if (item && item.warmUpPopup)
                        item.warmUpPopup()
                }
                settingsDialogLoader.active = true
            } else {
                var settings = settingsDialogLoader.item
                if (settings && settings.warmUpPopup)
                    settings.warmUpPopup()
            }
        }
    }
    Timer {
        id: startupWaterfallStageTimer
        interval: 1200
        repeat: false
        running: false
        onTriggered: maybeFinishStartupWaterfallVisualStage("timer")
    }
    Timer {
        id: startupWaterfallStageRetryTimer
        interval: 1000
        repeat: false
        running: false
        onTriggered: maybeFinishStartupWaterfallVisualStage("safe-slot")
    }
    Timer {
        id: startupLiveMapStageTimer
        interval: 3500
        repeat: false
        running: false
        onTriggered: maybeFinishStartupLiveMapVisualStage("timer")
    }
    Timer {
        id: startupLiveMapStageRetryTimer
        interval: 1000
        repeat: false
        running: false
        onTriggered: maybeFinishStartupLiveMapVisualStage("safe-slot")
    }
    Timer {
        id: startupLiveMapPopoutRestoreTimer
        interval: 350
        repeat: false
        running: false
        onTriggered: mainWindow.restoreLiveMapPopoutAfterStartup()
    }
    // Funzione helper chiamabile da qualsiasi parte del QML
    function scheduleSave() {
        if (startupVisualStagingEnabled && !startupLiveMapVisualReady) {
            startupSettingsSavePending = true
            return
        }
        if (typeof saveTimer !== "undefined" && saveTimer)
            saveTimer.restart()
    }
    function scheduleWindowStateSave(forceSave) {
        if (!forceSave && floatingGeometryInteractionDepth > 0) {
            deferredWindowStateSave = true
            return
        }
        if (!windowStateRestoreInProgress && typeof windowStateSaveTimer !== "undefined" && windowStateSaveTimer) {
            deferredWindowStateSave = false
            windowStateSaveTimer.restart()
        }
    }

    function captureNormalWindowGeometry() {
        if (windowStateRestoreInProgress || visibility !== Window.Windowed)
            return

        var capturedWidth = Math.round(safeNumber(width, normalWindowWidth))
        var capturedHeight = Math.round(safeNumber(height, normalWindowHeight))
        var capturedX = Math.round(safeNumber(x, normalWindowX))
        var capturedY = Math.round(safeNumber(y, normalWindowY))
        if (capturedWidth < minimumWidth || capturedHeight < minimumHeight
                || capturedWidth > 10000 || capturedHeight > 6000
                || !isFinite(capturedX) || !isFinite(capturedY))
            return

        normalWindowX = capturedX
        normalWindowY = capturedY
        normalWindowWidth = capturedWidth
        normalWindowHeight = capturedHeight
    }

    function scheduleNormalWindowGeometryCapture() {
        if (!windowStateRestoreInProgress && visibility === Window.Windowed
                && typeof normalWindowGeometryTimer !== "undefined"
                && normalWindowGeometryTimer)
            normalWindowGeometryTimer.restart()
    }

    function showRestoredWindowState() {
        visible = true
        if (mainWindowMaximized && typeof showMaximized === "function")
            showMaximized()
        else
            show()
    }

    function frequencyDisplayCells(freqHz) {
        var numericHz = Math.round(Number(freqHz) || 0)
        var text = (numericHz / 1000000).toFixed(6)
        var dot = text.indexOf(".")
        var cells = []
        for (var i = 0; i < text.length; ++i) {
            var ch = text.charAt(i)
            var isDigit = ch >= "0" && ch <= "9"
            var stepHz = 0
            if (isDigit && dot >= 0) {
                if (i < dot) {
                    stepHz = Math.pow(10, dot - i - 1) * 1000000
                } else if (i > dot) {
                    stepHz = Math.pow(10, 6 - (i - dot))
                }
                stepHz = Math.max(1, Math.round(stepHz))
            }
            cells.push({ text: ch, digit: isDigit, stepHz: stepHz })
        }
        return cells
    }

    function tuneDialByStep(stepHz, direction) {
        if (!bridge || !(stepHz > 0))
            return
        var currentHz = Math.round(Number(bridge.frequency) || 0)
        var nextHz = Math.max(0, currentHz + Math.round(stepHz) * direction)
        if (nextHz === currentHz)
            return
        bridge.qsyTo(nextHz, bridge.mode || "")
    }

    function persistSettingsDialogIfOpen() {
        var settings = settingsDialogLoader.item
        if (settings && settings.visible && settings.persistSettingsNow)
            settings.persistSettingsNow()
    }

    function restoreFloatingWindowState(windowRef, key, detachedPropName, minimizedPropName) {
        if (!windowRef)
            return {}
        var state = safeWindowState(key)
        var minimumWidthValue = Math.max(1, safeNumber(windowRef.minimumWidth, 1))
        var minimumHeightValue = Math.max(1, safeNumber(windowRef.minimumHeight, 1))
        var defaultWidth = Math.max(minimumWidthValue, safeNumber(windowRef.width, minimumWidthValue))
        var defaultHeight = Math.max(minimumHeightValue, safeNumber(windowRef.height, minimumHeightValue))
        var restoredWidth = Number(state.width)
        var restoredHeight = Number(state.height)

        // Reject corrupted or obsolete geometries instead of allowing a
        // floating window to shrink progressively at every restore.
        if (!isFinite(restoredWidth) || restoredWidth < minimumWidthValue || restoredWidth > 10000)
            restoredWidth = defaultWidth
        if (!isFinite(restoredHeight) || restoredHeight < minimumHeightValue || restoredHeight > 6000)
            restoredHeight = defaultHeight
        windowRef.width = Math.round(restoredWidth)
        windowRef.height = Math.round(restoredHeight)
        var restoredX = safeNumber(state.x, NaN)
        var restoredY = safeNumber(state.y, NaN)
        fitWindowSizeToAvailableScreen(windowRef, restoredX, restoredY)
        if (isFinite(restoredX) && isFinite(restoredY)) {
            var pos = clampWindowPosition(restoredX, restoredY, windowRef.width, windowRef.height)
            // Hidden QQuickWindows start on the primary display. Associate
            // the saved target screen before applying virtual coordinates so
            // Qt does not silently pull external-monitor windows back.
            if (pos.screen && windowRef.screen !== pos.screen)
                windowRef.screen = pos.screen
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
        return state
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
        function snapshot(windowRef, detached, minimized, geometryOverride, maximized) {
            var geometry = geometryOverride || windowRef
            var minimumWidthValue = Math.max(1, safeNumber(windowRef.minimumWidth, 1))
            var minimumHeightValue = Math.max(1, safeNumber(windowRef.minimumHeight, 1))
            var state = {
                x: Math.round(safeNumber(geometry.x, windowRef.x)),
                y: Math.round(safeNumber(geometry.y, windowRef.y)),
                width: Math.max(minimumWidthValue,
                                Math.round(safeNumber(geometry.width, minimumWidthValue))),
                height: Math.max(minimumHeightValue,
                                 Math.round(safeNumber(geometry.height, minimumHeightValue))),
                detached: !!detached,
                minimized: !!minimized
            }
            if (maximized !== undefined)
                state.maximized = !!maximized
            return state
        }

        var states = ({})
        var mainNormalGeometry = {
            x: normalWindowX,
            y: normalWindowY,
            width: normalWindowWidth,
            height: normalWindowHeight
        }
        states.mainWindow = snapshot(mainWindow, false,
                                     visibility === Window.Minimized,
                                     mainNormalGeometry,
                                     mainWindowMaximized)
        states.waterfallWindow = snapshot(waterfallWindow, waterfallDetached, waterfallMinimized)
        states.logFloatingWindow = snapshot(logFloatingWindow, logWindowDetached, logWindowMinimized)
        states.astroFloatingWindow = snapshot(astroFloatingWindow, astroWindowDetached, astroWindowMinimized)
        states.satelliteFloatingWindow = snapshot(satelliteFloatingWindow, satelliteWindowDetached, false)
        states.macroFloatingWindow = snapshot(macroFloatingWindow, macroDialogDetached, macroDialogMinimized)
        states.settingsFloatingWindow = snapshot(settingsFloatingWindow, false, false)
        states.mamFloatingWindow = snapshot(mamFloatingWindow, false, false)
        states.decometerFloatingWindow = snapshot(decometerFloatingWindow, false, false)
        states.activeStationsFloatingWindow = snapshot(activeStationsFloatingWindow, false, false)
        states.rigFloatingWindow = snapshot(rigFloatingWindow, rigControlDetached, rigControlMinimized)
        states.period1FloatingWindow = snapshot(period1FloatingWindow, period1Detached, period1Minimized)
        states.period2FloatingWindow = snapshot(period2FloatingWindow, period2Detached, period2Minimized)
        states.rxFreqFloatingWindow = snapshot(rxFreqFloatingWindow, rxFreqDetached, rxFreqMinimized)
        states.txPanelFloatingWindow = snapshot(txPanelFloatingWindow, txPanelDetached, txPanelMinimized)
        states.liveMapFloatingWindow = snapshot(liveMapFloatingWindow, liveMapDetached, false)
        states.decoSyncMonitorWindow = snapshot(decoSyncMonitorWindow, false, decoSyncMonitorWindow.visibility === Window.Minimized)
        // 1.0.275 — DX Cluster floating window
        if (dxClusterFloatingWindow)
            states.dxClusterFloatingWindow = snapshot(dxClusterFloatingWindow, dxClusterDetached, dxClusterMinimized)

        bridge.saveWindowStatesAsync(states, captureDecodePanelWidths())
    }

    // 1.0.263 (fork-only) — Reset Layout: ricevi signal dal backend e ripristina
    // tutte le floating windows a docked + centra mainWindow sul primary screen.
    Connections {
        target: bridge
        function onWindowLayoutResetRequested() {
            console.log("[ResetLayout] applying default layout")
            // 1) Re-dock di tutte le finestre floating (toglie detached + minimized)
            waterfallDetached = false;   waterfallMinimized = false
            logWindowDetached = false;   logWindowMinimized = false
            astroWindowDetached = false; astroWindowMinimized = false
            satelliteWindowDetached = false
            if (astroFloatingWindow) astroFloatingWindow.hide()
            if (satelliteFloatingWindow) satelliteFloatingWindow.hide()
            macroDialogDetached = false; macroDialogMinimized = false
            if (settingsFloatingWindow) {
                settingsFloatingWindow.hideHostedWindow()
                resetFloatingWindowGeometry(settingsFloatingWindow,
                                            settingsFloatingWindow.preferredWidth,
                                            settingsFloatingWindow.preferredHeight)
            }
            if (mamFloatingWindow) mamFloatingWindow.hideHostedWindow()
            if (decometerFloatingWindow) decometerFloatingWindow.hideHostedWindow()
            activeStationsPanelVisible = false
            if (activeStationsFloatingWindow) activeStationsFloatingWindow.hide()
            rigControlDetached = false;  rigControlMinimized = false
            period1Detached = false;     period1Minimized = false
            period2Detached = false;     period2Minimized = false
            rxFreqDetached = false;      rxFreqMinimized = false
            txPanelDetached = false;     txPanelMinimized = false
            liveMapDetached = false;     liveMapMinimized = false
            // 1.0.275 — DX Cluster: riporta alla posizione default vicino bordo destro mainWindow
            dxClusterMinimized = false
            if (dxClusterFloatingWindow) {
                dxClusterFloatingWindow.width = 560
                dxClusterFloatingWindow.height = 360
                dxClusterFloatingWindow.x = mainWindow.x + Math.max(0, mainWindow.width - 560 - 60)
                dxClusterFloatingWindow.y = mainWindow.y + 80
                dxClusterFloatingWindow.visibility = Window.Windowed
                if (dxClusterFloatingWindow.visible) dxClusterFloatingWindow.raise()
            }

            // 2) Centra mainWindow su primary screen con dimensioni default
            var geo = bridge.primaryScreenAvailableGeometry()
            if (geo && geo.width && geo.height) {
                var dw = Math.min(1600, Math.max(1024, geo.width  - 100))
                var dh = Math.min(1000, Math.max(720,  geo.height - 100))
                visibility = Window.Windowed
                width  = dw
                height = dh
                x = geo.x + Math.round((geo.width  - dw) / 2)
                y = geo.y + Math.round((geo.height - dh) / 2)
            } else {
                visibility = Window.Windowed
                width = 1280; height = 800; x = 100; y = 100
            }
            mainWindowMaximized = false
            captureNormalWindowGeometry()
            raise(); requestActivate()

            // 3) Forza salvataggio nuovo state pulito (sovrascrive QSettings)
            Qt.callLater(persistWindowLayouts)
        }
    }

    // Salva impostazioni bridge al close
    onClosing: function(close) {
        saveTimer.stop()
        windowStateSaveTimer.stop()
        normalWindowGeometryTimer.stop()
        applicationClosing = true
        // Keep the last position even when the user closes immediately after
        // dragging, before the normal settings debounce can run.
        persistWorldClockPos()
        persistSettingsDialogIfOpen()
        captureNormalWindowGeometry()
        persistWindowLayouts()
        console.log("Main window closing - shutting down application")
        // Close all floating windows
        if (waterfallWindow) waterfallWindow.close()
        if (logFloatingWindow) logFloatingWindow.close()
        if (macroFloatingWindow) macroFloatingWindow.close()
        if (settingsFloatingWindow) settingsFloatingWindow.close()
        if (mamFloatingWindow) mamFloatingWindow.close()
        if (decometerFloatingWindow) decometerFloatingWindow.close()
        if (activeStationsFloatingWindow) activeStationsFloatingWindow.close()
        if (logWindowLoader.item) logWindowLoader.item.close()
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
        shutdownAsyncLoaders()
        // Quit application
        Qt.quit()
    }

    onXChanged: {
        scheduleNormalWindowGeometryCapture()
        scheduleWindowStateSave()
    }
    onYChanged: {
        scheduleNormalWindowGeometryCapture()
        scheduleWindowStateSave()
    }
    onWidthChanged: {
        scheduleNormalWindowGeometryCapture()
        scheduleWindowStateSave()
    }
    onHeightChanged: {
        scheduleNormalWindowGeometryCapture()
        scheduleWindowStateSave()
    }
    onVisibilityChanged: {
        if (windowStateRestoreInProgress)
            return
        if (visibility === Window.Maximized) {
            normalWindowGeometryTimer.stop()
            mainWindowMaximized = true
        } else if (visibility === Window.Windowed) {
            mainWindowMaximized = false
            scheduleNormalWindowGeometryCapture()
        }
        if (visibility === Window.Maximized || visibility === Window.Windowed)
            scheduleWindowStateSave()
    }

    component FloatingResizeHandles: Item {
        id: resizeRoot
        property var targetWindow
        property int edgeSize: 7
        property int cornerSize: 16
        property int maxWidth: 10000
        property int maxHeight: 6000
        property bool resizeActive: false
        property bool resizeFromLeft: false
        property bool resizeFromTop: false
        property bool resizeFromRight: false
        property bool resizeFromBottom: false
        property point pressGlobalPos: Qt.point(0, 0)
        property point pressWindowPos: Qt.point(0, 0)
        property size pressWindowSize: Qt.size(0, 0)

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

        function beginResize(handle, mouse, fromLeft, fromTop, fromRight, fromBottom) {
            if (!targetWindow || resizeActive)
                return
            resizeActive = true
            resizeFromLeft = fromLeft
            resizeFromTop = fromTop
            resizeFromRight = fromRight
            resizeFromBottom = fromBottom
            pressGlobalPos = handle.mapToGlobal(mouse.x, mouse.y)
            pressWindowPos = Qt.point(targetWindow.x, targetWindow.y)
            pressWindowSize = Qt.size(targetWindow.width, targetWindow.height)
            mainWindow.beginFloatingGeometryInteraction()
        }

        function applyResize(handle, mouse) {
            if (!targetWindow || !resizeActive)
                return

            var currentGlobal = handle.mapToGlobal(mouse.x, mouse.y)
            var deltaX = currentGlobal.x - pressGlobalPos.x
            var deltaY = currentGlobal.y - pressGlobalPos.y
            var newWidth = pressWindowSize.width
            var newHeight = pressWindowSize.height
            var newX = pressWindowPos.x
            var newY = pressWindowPos.y

            if (resizeFromLeft) {
                newWidth = boundedWidth(pressWindowSize.width - deltaX)
                newX = pressWindowPos.x + pressWindowSize.width - newWidth
            } else if (resizeFromRight) {
                newWidth = boundedWidth(pressWindowSize.width + deltaX)
            }

            if (resizeFromTop) {
                newHeight = boundedHeight(pressWindowSize.height - deltaY)
                newY = pressWindowPos.y + pressWindowSize.height - newHeight
            } else if (resizeFromBottom) {
                newHeight = boundedHeight(pressWindowSize.height + deltaY)
            }

            if (resizeFromLeft)
                targetWindow.x = Math.round(newX)
            if (resizeFromTop)
                targetWindow.y = Math.round(newY)
            targetWindow.width = Math.round(newWidth)
            targetWindow.height = Math.round(newHeight)
        }

        function finishResize() {
            if (!resizeActive)
                return
            resizeActive = false
            mainWindow.finishFloatingWindowDrag(targetWindow)
            mainWindow.endFloatingGeometryInteraction()
        }

        MouseArea {
            id: resizeRightHandle
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: resizeRoot.cornerSize
            anchors.bottomMargin: resizeRoot.cornerSize
            width: resizeRoot.edgeSize
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeRightHandle, mouse, false, false, true, false)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeRightHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeLeftHandle
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: resizeRoot.cornerSize
            anchors.bottomMargin: resizeRoot.cornerSize
            width: resizeRoot.edgeSize
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeLeftHandle, mouse, true, false, false, false)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeLeftHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeBottomHandle
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: resizeRoot.cornerSize
            anchors.rightMargin: resizeRoot.cornerSize
            height: resizeRoot.edgeSize
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeBottomHandle, mouse, false, false, false, true)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeBottomHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeTopHandle
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: resizeRoot.cornerSize
            anchors.rightMargin: resizeRoot.cornerSize
            height: resizeRoot.edgeSize
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeTopHandle, mouse, false, true, false, false)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeTopHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeBottomRightHandle
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeBottomRightHandle, mouse, false, false, true, true)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeBottomRightHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeBottomLeftHandle
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeBottomLeftHandle, mouse, true, false, false, true)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeBottomLeftHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeTopRightHandle
            anchors.right: parent.right
            anchors.top: parent.top
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeTopRightHandle, mouse, false, true, true, false)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeTopRightHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }

        MouseArea {
            id: resizeTopLeftHandle
            anchors.left: parent.left
            anchors.top: parent.top
            width: resizeRoot.cornerSize
            height: resizeRoot.cornerSize
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: function(mouse) {
                resizeRoot.beginResize(resizeTopLeftHandle, mouse, true, true, false, false)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    resizeRoot.applyResize(resizeTopLeftHandle, mouse)
            }
            onReleased: resizeRoot.finishResize()
            onCanceled: resizeRoot.finishResize()
        }
    }

    // Corner-only resize for fixed-aspect instruments.  The handles live
    // inside the face, so they do not create a second frame or an external
    // input gutter.  Width drives height and the opposite corner stays fixed.
    component ProportionalResizeHandles: Item {
        id: proportionalResizeRoot
        property var targetWindow
        property real aspectRatio: 15 / 7
        property int minWidth: 450
        property int maxWidth: 1800
        // A frameless Windows window has no native resize border.  Keep a
        // generous hit target for the two proportional corner grips so the
        // resize remains usable with display scaling enabled.
        property int cornerSize: 32
        property bool resizeActive: false
        property bool nativeResizeActive: false
        property bool nativeResizeAdjusting: false

        anchors.fill: parent
        enabled: !!targetWindow && aspectRatio > 0
        z: 1000

        function boundedWidth(value) {
            return Math.max(minWidth, Math.min(maxWidth, Math.round(value)))
        }

        function beginResize() {
            if (resizeActive)
                return
            resizeActive = true
            mainWindow.beginFloatingGeometryInteraction()
        }

        // Il percorso nativo di Windows E' STATO PROVATO E NON FUNZIONA su
        // questa finestra, ed e' peggio di non averlo: startSystemResize()
        // risponde true — cioe' "il ridimensionamento me lo prendo io" — ma
        // poi non muove un pixel, perche' la finestra e' frameless e Windows
        // non ha un bordo di sistema da trascinare. Quella risposta accendeva
        // nativeResizeActive, che a sua volta spegne il percorso manuale qui
        // sotto: il risultato era una finestra semplicemente NON
        // ridimensionabile, con gli angoli che rispondevano al mouse senza
        // che succedesse niente.
        //
        // Verificato con una sonda sul posto: beginResize scatta, pressed e'
        // true, startSystemResize torna true, la geometria resta identica.
        //
        // Si tiene quindi il percorso manuale, che aggiorna width e height a
        // ogni movimento: meno fluido di quello di sistema, ma e' sotto il
        // nostro controllo, rispetta il rapporto 15:7 esatto e soprattutto
        // funziona. Se un domani si volesse riprovare la strada nativa, va
        // verificato che la finestra cambi davvero dimensione prima di
        // fidarsi del valore di ritorno.
        function startNativeResize(edges) {
            return false
        }

        function constrainNativeAspect() {
            if (!nativeResizeActive || nativeResizeAdjusting || !targetWindow)
                return
            var bounded = boundedWidth(targetWindow.width)
            var proportionalHeight = Math.max(targetWindow.minimumHeight,
                                              Math.min(targetWindow.maximumHeight,
                                                       Math.round(bounded / aspectRatio)))
            nativeResizeAdjusting = true
            targetWindow.width = bounded
            targetWindow.height = proportionalHeight
            nativeResizeAdjusting = false
        }

        function applyBottomResize(handle, mouse, fromLeft) {
            if (!targetWindow || !handle.pressed || nativeResizeActive)
                return
            var currentGlobal = handle.mapToGlobal(mouse.x, mouse.y)
            var deltaX = currentGlobal.x - handle.pressGlobalPos.x
            var deltaY = currentGlobal.y - handle.pressGlobalPos.y
            var horizontalGrowth = fromLeft ? -deltaX : deltaX
            var verticalGrowthAsWidth = deltaY * aspectRatio
            var widthGrowth = Math.abs(horizontalGrowth) >= Math.abs(verticalGrowthAsWidth)
                              ? horizontalGrowth : verticalGrowthAsWidth
            var newWidth = boundedWidth(handle.pressWindowSize.width + widthGrowth)
            var newHeight = Math.round(newWidth / aspectRatio)

            if (fromLeft)
                targetWindow.x = Math.round(handle.pressWindowPos.x
                                             + handle.pressWindowSize.width - newWidth)
            targetWindow.width = newWidth
            targetWindow.height = newHeight
        }

        function finishResize() {
            if (!targetWindow || !resizeActive)
                return
            nativeResizeActive = false
            resizeActive = false
            mainWindow.finishFloatingWindowDrag(targetWindow, aspectRatio)
            mainWindow.endFloatingGeometryInteraction()
        }

        MouseArea {
            id: proportionalBottomLeft
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: proportionalResizeRoot.cornerSize
            height: proportionalResizeRoot.cornerSize
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.SizeBDiagCursor
            preventStealing: true
            property point pressGlobalPos: Qt.point(0, 0)
            property point pressWindowPos: Qt.point(0, 0)
            property size pressWindowSize: Qt.size(0, 0)
            onPressed: function(mouse) {
                pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                pressWindowPos = Qt.point(proportionalResizeRoot.targetWindow.x,
                                          proportionalResizeRoot.targetWindow.y)
                pressWindowSize = Qt.size(proportionalResizeRoot.targetWindow.width,
                                          proportionalResizeRoot.targetWindow.height)
                proportionalResizeRoot.beginResize()
                proportionalResizeRoot.nativeResizeActive =
                        proportionalResizeRoot.startNativeResize(Qt.LeftEdge | Qt.BottomEdge)
            }
            onPositionChanged: function(mouse) {
                proportionalResizeRoot.applyBottomResize(proportionalBottomLeft, mouse, true)
            }
            onReleased: proportionalResizeRoot.finishResize()
            onCanceled: proportionalResizeRoot.finishResize()
        }

        MouseArea {
            id: proportionalBottomRight
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: proportionalResizeRoot.cornerSize
            height: proportionalResizeRoot.cornerSize
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.SizeFDiagCursor
            preventStealing: true
            property point pressGlobalPos: Qt.point(0, 0)
            property point pressWindowPos: Qt.point(0, 0)
            property size pressWindowSize: Qt.size(0, 0)
            onPressed: function(mouse) {
                pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                pressWindowPos = Qt.point(proportionalResizeRoot.targetWindow.x,
                                          proportionalResizeRoot.targetWindow.y)
                pressWindowSize = Qt.size(proportionalResizeRoot.targetWindow.width,
                                          proportionalResizeRoot.targetWindow.height)
                proportionalResizeRoot.beginResize()
                proportionalResizeRoot.nativeResizeActive =
                        proportionalResizeRoot.startNativeResize(Qt.RightEdge | Qt.BottomEdge)
            }
            onPositionChanged: function(mouse) {
                proportionalResizeRoot.applyBottomResize(proportionalBottomRight, mouse, false)
            }
            onReleased: proportionalResizeRoot.finishResize()
            onCanceled: proportionalResizeRoot.finishResize()
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
    property bool waterfallPanelVisible: settingBool("uiWaterfallPanelVisible", true)
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
    property bool satelliteWindowDetached: false
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
    property bool compactFullSpectrum: settingBool("CompactFullSpectrum", false)
    property int fullSpectrumRowHeight: compactFullSpectrum ? 14 : 26
    // 1.0.255 — flag transitioning per disabilitare YAnimator displaced
    // durante toggle compact/full (height cambia su TUTTI i delegate
    // contemporaneamente -> blink per 100ms di animation displaced).
    property bool compactToggling: false
    // 1.0.428 — calmante FT2 "slot-machine": durante una risposta partner il
    // modello RX puo' fare shift/reset rapidi (match-key instabile; fix radice
    // separato) e gli YAnimator displaced facevano scorrere le righe come reel.
    // false = riposizionamento istantaneo (calmo, default). Vedi diagnosi 2026-06-21.
    property bool decodeRowSlideAnim: false
    Timer {
        id: compactToggleTimer
        interval: 220  // > 100ms animation + 100ms padding
        repeat: false
        onTriggered: compactToggling = false
    }
    function toggleCompactFullSpectrum() {
        compactToggling = true
        compactToggleTimer.restart()
        compactFullSpectrum = !compactFullSpectrum
        persistUiSetting("CompactFullSpectrum", compactFullSpectrum)
    }

    // 1.0.412 — Schermo intero opt-in. Uscita SEMPRE disponibile: tasto F11, Esc, e il
    // pulsante ✕ dell'overlay in alto. NON persistito (saveWindowState non salva la
    // visibility) → un riavvio non riparte MAI bloccato in fullscreen senza barra titolo.
    function toggleFullScreen() {
        mainWindow.visibility = (mainWindow.visibility === Window.FullScreen)
            ? Window.Windowed : Window.FullScreen
    }
    function exitFullScreen() {
        if (mainWindow.visibility === Window.FullScreen)
            mainWindow.visibility = Window.Windowed
    }
    Shortcut {
        sequences: ["F11"]
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.toggleFullScreen()
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: mainWindow.visibility === Window.FullScreen
        onActivated: mainWindow.exitFullScreen()
    }
    // Overlay di uscita dallo schermo intero: sempre visibile e cliccabile in fullscreen,
    // così non si resta mai intrappolati senza barra del titolo (lezione stato-finestra).
    Rectangle {
        id: fullScreenExitBar
        visible: mainWindow.visibility === Window.FullScreen
        z: 100000
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        width: fsExitRow.implicitWidth + 26
        height: 32
        radius: 16
        color: Qt.rgba(0, 0, 0, 0.75)
        border.color: secondaryCyan
        border.width: 1
        Row {
            id: fsExitRow
            anchors.centerIn: parent
            spacing: 10
            Text { text: qsTr("Full screen"); color: "#ffffff"; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
            Text { text: qsTr("F11 / Esc"); color: textSecondary; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            Rectangle {
                width: 24; height: 24; radius: 12
                anchors.verticalCenter: parent.verticalCenter
                color: fsExitMA.containsMouse ? bridge.themeManager.ledRed : Qt.rgba(1, 1, 1, 0.18)
                Text { anchors.centerIn: parent; text: "✕"; color: "#ffffff"; font.pixelSize: 12; font.bold: true }
                MouseArea {
                    id: fsExitMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mainWindow.exitFullScreen()
                }
            }
        }
    }
    // 1.0.253 — Compact mode Signal RX: stesso pattern di Full Spectrum
    // ma indipendente. Opt-in default OFF.
    property bool compactSignalRx: settingBool("CompactSignalRx", false)
    property int signalRxRowHeight: compactSignalRx ? 14 : 26
    function toggleCompactSignalRx() {
        compactToggling = true
        compactToggleTimer.restart()
        compactSignalRx = !compactSignalRx
        persistUiSetting("CompactSignalRx", compactSignalRx)
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
    property bool ft2LinkPanelDetached: false
    property bool liveMapDetached: false
    property bool liveMapMinimized: false
    // 1.0.275 (fork-only) — DX Cluster floating window state
    property bool dxClusterDetached: true   // default detached (era sempre floating)
    property bool dxClusterMinimized: false
    property bool applicationClosing: false
    function shutdownLoader(loader) {
        if (!loader)
            return
        try {
            loader.active = false
        } catch (e) {
        }
        try {
            loader.sourceComponent = null
        } catch (e) {
        }
        try {
            loader.source = ""
        } catch (e) {
        }
    }
    function shutdownAsyncLoaders() {
        if (typeof waterfallEmbeddedLoader !== "undefined")
            shutdownLoader(waterfallEmbeddedLoader)
        if (typeof liveMapEmbeddedLoader !== "undefined")
            shutdownLoader(liveMapEmbeddedLoader)
        if (typeof dxPeditionLoader !== "undefined")
            shutdownLoader(dxPeditionLoader)
        if (typeof macroDialogLoader !== "undefined")
            shutdownLoader(macroDialogLoader)
        if (typeof astroWindowLoader !== "undefined")
            shutdownLoader(astroWindowLoader)
        if (typeof qsyQuickPickerLoader !== "undefined")
            shutdownLoader(qsyQuickPickerLoader)
        if (typeof devOverlayLoader !== "undefined")
            shutdownLoader(devOverlayLoader)
        if (typeof bugReportDialogLoader !== "undefined")
            shutdownLoader(bugReportDialogLoader)
        if (typeof settingsDialogLoader !== "undefined")
            shutdownLoader(settingsDialogLoader)
        if (typeof callsignLookupWindowLoader !== "undefined")
            shutdownLoader(callsignLookupWindowLoader)
        if (typeof logWindowLoader !== "undefined")
            shutdownLoader(logWindowLoader)
        if (typeof mamWindowLoader !== "undefined")
            shutdownLoader(mamWindowLoader)
        if (typeof infoDialogLoader !== "undefined")
            shutdownLoader(infoDialogLoader)
        if (typeof callDialogLoader !== "undefined")
            shutdownLoader(callDialogLoader)
        if (typeof historyDialogLoader !== "undefined")
            shutdownLoader(historyDialogLoader)
        if (typeof sstvWorkspaceLoader !== "undefined")
            shutdownLoader(sstvWorkspaceLoader)
    }

    // Native SSTV is loaded only on first use. The C++ facade exists in the
    // Decodium process, but its RX worker remains stopped until the user starts
    // reception from this workspace.
    function openSstvWorkspace() {
        // Entering SSTV is an exclusive workspace transition, like opening
        // RTTY: replace the previous decoder generation with an SSTV-only
        // audio capture. The panadapter remains alive; mode/CAT are preserved
        // and the normal monitor state is restored when SSTV closes.
        if (!bridge.enterSstvWorkspace())
            return
        sstvWorkspaceLoader.active = true
        if (sstvWorkspaceLoader.item) {
            sstvWorkspaceLoader.item.show()
            sstvWorkspaceLoader.item.raise()
            sstvWorkspaceLoader.item.requestActivate()
        }
    }

    Loader {
        id: sstvWorkspaceLoader
        active: false
        asynchronous: true
        source: "components/sstv/SstvWorkspace.qml"
        onLoaded: {
            item.engine = bridge
            item.show()
            item.raise()
            item.requestActivate()
        }
        onStatusChanged: {
            if (status === Loader.Error)
                bridge.leaveSstvWorkspace()
        }
    }

    // 1.0.571 - finestra DecoPort, creata alla prima apertura.
    function openDecoPortWindow() {
        decoPortWindowLoader.active = true
        if (decoPortWindowLoader.item) {
            decoPortWindowLoader.item.show()
            decoPortWindowLoader.item.raise()
            decoPortWindowLoader.item.requestActivate()
        }
    }

    // Mostra la finestra gia' nel modo RTTY. Separata dal comando pubblico per
    // evitare una ricorsione quando setMode emette modeChanged.
    function showRttyWindow() {
        rttyWindowLoader.active = true
        if (rttyWindowLoader.item) {
            rttyWindowLoader.item.show()
            rttyWindowLoader.item.raise()
            rttyWindowLoader.item.requestActivate()
        }
    }

    // Aprire RTTY equivale a sceglierlo dal selettore dei modi: il cambio
    // invalida le code del decoder precedente, ferma i suoi timer di slot e
    // commuta CAT/frequenza secondo il profilo RTTY. Prima questa strada
    // apriva soltanto la finestra e FT8/FT4/WSPR continuavano nel background.
    function openRttyWindow() {
        if (bridge.mode !== "RTTY")
            bridge.mode = "RTTY"
        if (bridge.mode === "RTTY")
            mainWindow.showRttyWindow()
    }

    // Scegliere RTTY dal selettore dei modi apre la finestra: il modo e' attivo,
    // la radio e' commutata, e senza la finestra non ci sarebbe niente da
    // leggere ne' da scrivere. Non la chiude tornando a FT8 — chi ha una
    // finestra aperta se la chiude da se', e chiudergliela sotto le mani mentre
    // magari sta leggendo un collegamento sarebbe peggio che lasciarla li'.
    Connections {
        target: bridge
        function onModeChanged() {
            if (bridge.mode === "RTTY")
                mainWindow.showRttyWindow()
        }
    }

    Loader {
        id: rttyWindowLoader
        active: false
        asynchronous: true
        source: "rtty/RttyMain.qml"
        onLoaded: {
            item.show()
            item.raise()
            item.requestActivate()
        }
    }

    Loader {
        id: decoPortWindowLoader
        active: false
        asynchronous: true
        source: "components/DecoPortWindow.qml"
        onLoaded: {
            item.show()
            item.raise()
            item.requestActivate()
        }
    }

    function syncSpectrumVisibility() {
        if (bridge)
            bridge.spectrumVisible = waterfallPanelVisible && startupWaterfallVisualReady
    }
    onWaterfallPanelVisibleChanged: {
        persistUiSetting("uiWaterfallPanelVisible", waterfallPanelVisible)
        syncSpectrumVisibility()
        if (!waterfallPanelVisible) {
            waterfallDetached = false
            waterfallMinimized = false
            if (typeof waterfallWindow !== "undefined" && waterfallWindow)
                waterfallWindow.hide()
        } else {
            Qt.callLater(function() {
                if (typeof waterfallPanel !== "undefined" && waterfallPanel)
                    waterfallPanel.SplitView.preferredHeight = mainWindow.waterfallPanelHeight
            })
        }
    }
    onWaterfallDetachedChanged: scheduleWindowStateSave()
    onWaterfallMinimizedChanged: scheduleWindowStateSave()
    onLogWindowDetachedChanged: scheduleWindowStateSave()
    onLogWindowMinimizedChanged: scheduleWindowStateSave()
    onAstroWindowDetachedChanged: scheduleWindowStateSave()
    onAstroWindowMinimizedChanged: scheduleWindowStateSave()
    onSatelliteWindowDetachedChanged: scheduleWindowStateSave()
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
    onFt2LinkModeActiveChanged: {
        if (ft2LinkModeActive) {
            if (bridge && !bridge.ft2LinkAccessUnlocked) {
                Qt.callLater(requestFt2LinkAccess)
                return
            }
            applyFt2LinkModeLayout()
        } else {
            dockFt2LinkPanel()
            restoreFt2LinkModeLayout()
        }
    }

    // === GAP 3 — Nuovi pannelli (A3, B9, A4, C14) ===
    property bool timeSyncPanelVisible:       settingBool("uiTimeSyncPanelVisible", false)
    property bool activeStationsPanelVisible: settingBool("uiActiveStationsPanelVisible", false)
    readonly property bool ft2LinkModeActive: bridge && String(bridge.mode || "").toUpperCase() === "FT2-LINK"
    property bool ft2LinkAccessPromptActive: false
    property string ft2LinkAccessError: ""
    property bool ft2LinkAccessPendingMode: false
    property bool callerQueuePanelVisible:    settingBool("uiCallerQueuePanelVisible", false)
    property bool astroPanelVisible:          settingBool("uiAstroPanelVisible", false)
    property bool dxClusterPanelVisible:      settingBool("uiDxClusterPanelVisible", false)
    property bool dxClusterToolbarVisible:    settingBool("uiDxClusterToolbarVisible", true)
    property bool pskReporterToolbarVisible: settingBool("uiPskReporterToolbarVisible", true)
    property bool asyncIconVisible:           settingBool("uiAsyncIconVisible", true)
    // 1.0.497 — in Modalità PC lento la Live Map (render mondo GPU, il pannello
    // più pesante) parte NASCOSTA di default; resta riattivabile a mano.
    property bool liveMapPanelVisible:        settingBool("WorldMapDisplayed", !(bridge && bridge.lowEndMode))
    property bool decoSyncMonitorVisible:     settingBool("uiDecoSyncMonitorVisible", false)

    // === Visibilità pulsanti UI (tab "Pulsanti UI" in Settings) — default tutti visibili ===
    property bool uiBtnMonitorVisible:        settingBool("uiBtnMonitorVisible", true)
    property bool uiBtnSetupVisible:          settingBool("uiBtnSetupVisible", true)
    property bool uiBtnRecVisible:            settingBool("uiBtnRecVisible", true)
    property bool uiBtnWavVisible:            settingBool("uiBtnWavVisible", true)
    property bool uiBtnLogVisible:            settingBool("uiBtnLogVisible", true)
    property bool uiBtnMacroVisible:          settingBool("uiBtnMacroVisible", true)
    property bool uiBtnAstroVisible:          settingBool("uiBtnAstroVisible", true)
    property bool uiBtnCatVisible:            settingBool("uiBtnCatVisible", true)
    // 1.0.569 — scorciatoia per entrare nel workspace DX-Pedition (il gemello
    // del pulsante EXIT che c'e' dentro il workspace).
    property bool uiBtnDxPedVisible:          settingBool("uiBtnDxPedVisible", true)

    // === Ordine pulsanti toolbar (drag&drop riordinabile, persistente) ===
    readonly property string uiToolbarOrderDefault: "setup,rec,wav,sep1,log,macro,astro,layout,history,dxped,sep2,cat"
    property var uiToolbarOrder: parseToolbarOrder(String(bridge.getSetting("uiToolbarOrder", "") || ""))

    // Tutti gli id validi (pulsanti + separatori). Usato per validare/normalizzare.
    readonly property var uiToolbarKnownIds: ["setup","rec","wav","log","macro","astro","layout","history","dxped","cat","sep1","sep2"]

    function applyFt2LinkModeLayout() {
        if (typeof period1FloatingWindow !== "undefined" && period1FloatingWindow)
            period1FloatingWindow.hide()
        if (typeof rxFreqFloatingWindow !== "undefined" && rxFreqFloatingWindow)
            rxFreqFloatingWindow.hide()
        Qt.callLater(restoreDecodePanelWidths)
    }

    function restoreFt2LinkModeLayout() {
        if (period1Detached && !period1Minimized
                && typeof period1FloatingWindow !== "undefined" && period1FloatingWindow)
            period1FloatingWindow.show()
        if (rxFreqDetached && !rxFreqMinimized
                && typeof rxFreqFloatingWindow !== "undefined" && rxFreqFloatingWindow)
            rxFreqFloatingWindow.show()
        Qt.callLater(restoreDecodePanelWidths)
    }

    function popFt2LinkPanel() {
        if (!ft2LinkModeActive)
            return
        ft2LinkPanelDetached = true
        Qt.callLater(function() {
            if (typeof ft2LinkFloatingWindow !== "undefined" && ft2LinkFloatingWindow)
                ft2LinkFloatingWindow.requestActivate()
        })
    }

    function dockFt2LinkPanel() {
        ft2LinkPanelDetached = false
    }

    function toggleFt2LinkPanelDock() {
        if (ft2LinkPanelDetached)
            dockFt2LinkPanel()
        else
            popFt2LinkPanel()
    }

    function requestFt2LinkAccess() {
        if (!bridge)
            return
        if (bridge.ft2LinkAccessUnlocked) {
            bridge.mode = "FT2-Link"
            applyFt2LinkModeLayout()
            return
        }
        if (!bridge.ft2LinkAccessPasswordConfigured()) {
            showStatusToast("FT2-Link locked: access hash not provisioned.", accentOrange)
            rejectFt2LinkAccess()
            return
        }
        ft2LinkAccessError = ""
        ft2LinkAccessPendingMode = true
        ft2LinkAccessPromptActive = true
        ft2LinkAccessDialog.open()
    }

    function rejectFt2LinkAccess() {
        ft2LinkAccessPromptActive = false
        ft2LinkAccessPendingMode = false
        ft2LinkAccessError = ""
        if (ft2LinkPasswordField)
            ft2LinkPasswordField.text = ""
        if (ft2LinkAccessDialog && ft2LinkAccessDialog.opened)
            ft2LinkAccessDialog.close()
        if (bridge)
            bridge.mode = "FT2"
    }

    function acceptFt2LinkAccess() {
        if (!bridge)
            return
        var password = ft2LinkPasswordField.text
        var ok = bridge.verifyFt2LinkAccessPassword(password)
        if (!ok) {
            rejectFt2LinkAccess()
            return
        }
        ft2LinkAccessPromptActive = false
        ft2LinkAccessPendingMode = false
        ft2LinkAccessError = ""
        ft2LinkPasswordField.text = ""
        ft2LinkAccessDialog.close()
        bridge.mode = "FT2-Link"
        applyFt2LinkModeLayout()
    }

    // Parsa il CSV salvato in una lista di id; ripristina il default se assente/corrotto.
    function parseToolbarOrder(csv) {
        var def = uiToolbarOrderDefault.split(",")
        if (!csv || csv.length === 0)
            return def
        var parts = String(csv).split(",")
        var out = []
        var seen = ({})
        for (var i = 0; i < parts.length; ++i) {
            var id = parts[i].trim()
            if (id.length === 0)
                continue
            if (uiToolbarKnownIds.indexOf(id) < 0)
                continue            // id sconosciuto -> scarta
            if (seen[id])
                continue            // dedup
            seen[id] = true
            out.push(id)
        }
        // Inserisci gli id mancanti (es. un pulsante nuovo) alla loro POSIZIONE
        // di default, non in coda: appesi in fondo finirebbero oltre il bordo
        // della toolbar e l'utente non li vedrebbe mai.
        for (var j = 0; j < def.length; ++j) {
            if (!seen[def[j]]) {
                out.splice(Math.min(j, out.length), 0, def[j])
                seen[def[j]] = true
            }
        }
        if (out.length === 0)
            return def
        return out
    }

    function persistToolbarOrder() {
        persistUiSetting("uiToolbarOrder", uiToolbarOrder.join(","))
    }

    function headerToolbarButtonVisible(id) {
        switch (id) {
        case "setup": return uiBtnSetupVisible
        case "rec": return uiBtnRecVisible
        case "wav": return uiBtnWavVisible
        case "log": return uiBtnLogVisible
        case "macro": return uiBtnMacroVisible
        case "astro": return uiBtnAstroVisible
        case "layout": return uiBtnFooterResetVisible
        case "history": return uiBtnFooterHistoryVisible
        case "dxped": return uiBtnDxPedVisible
        case "cat": return uiBtnCatVisible
        case "sep1":
        case "sep2": return true
        default: return false
        }
    }

    function headerToolbarButtonWidth(id) {
        switch (id) {
        case "setup": return 50
        case "rec": return 50
        case "wav": return 45
        case "log": return 45
        case "macro": return 50
        case "astro": return 48
        case "layout": return 64
        case "history": return 68
        case "dxped": return 66
        case "cat": return 48
        case "sep1":
        case "sep2": return 1
        default: return 0
        }
    }

    function headerToolbarPreferredWidth() {
        var total = 4
        var visibleCount = 0
        for (var i = 0; i < uiToolbarOrder.length; ++i) {
            var id = uiToolbarOrder[i]
            if (!headerToolbarButtonVisible(id))
                continue
            total += headerToolbarButtonWidth(id)
            if (visibleCount > 0)
                total += 1
            ++visibleCount
        }
        return visibleCount > 0 ? Math.max(1, total) : 0
    }

    // === Posizione World Clock fra i blocchi dell'header (snap magnetico via maniglia) ===
    // L'orologio resta IN LINEA nel Flow header (non finestra OS, non overlay x/y): viene
    // re-parentato in uno dei 6 host-slot fissi inseriti nei gap STABILI fra i blocchi
    // (vedi headerFlow.clockSlots). L'indice = lo slot che ospita l'orologio:
    //   0 = prima dell'hamburger      3 = prima dei Sliders
    //   1 = fra hamburger e logo      4 = prima del blocco pulsanti toolbar ("before")
    //   2 = fra logo e freq display   5 = subito DOPO il blocco pulsanti toolbar ("after", DEFAULT)
    // Default = 5 (posizione attuale, toolbar PRIMA / clock DOPO) → ZERO regressione.
    // I blocchi condizionali (restore/DX/PSK) NON sono posizioni di aggancio.
    readonly property int worldClockSlotBeforeToolbar: 4
    readonly property int worldClockSlotAfterToolbar: 5
    readonly property int worldClockSlotDefault: worldClockSlotAfterToolbar
    property int uiWorldClockHeaderSlot: resolveInitialWorldClockSlot()
    onUiWorldClockHeaderSlotChanged: {
        applyWorldClockSlot()
        persistUiSetting("uiWorldClockHeaderSlot", uiWorldClockHeaderSlot)
    }

    // Init dell'indice slot: legge la nuova chiave; in sua assenza migra la VECCHIA chiave
    // bool uiWorldClockBeforeToolbar (true→slot before, false→slot after) one-shot.
    function resolveInitialWorldClockSlot() {
        var raw = safeBridgeSetting("uiWorldClockHeaderSlot", null)
        if (raw !== null && raw !== undefined && String(raw).trim().length > 0) {
            var v = parseInt(raw, 10)
            if (!isNaN(v))
                return clampWorldClockSlot(v)
        }
        // Retrocompat: vecchio toggle bool (prima/dopo la toolbar).
        var legacy = safeBridgeSetting("uiWorldClockBeforeToolbar", null)
        if (legacy !== null && legacy !== undefined && String(legacy).trim().length > 0)
            return coerceBool(legacy, false) ? worldClockSlotBeforeToolbar : worldClockSlotAfterToolbar
        return worldClockSlotDefault
    }

    function clampWorldClockSlot(i) {
        var n = (headerFlow && headerFlow.clockSlots) ? headerFlow.clockSlots.length : 6
        var v = parseInt(i, 10)
        if (isNaN(v))
            v = worldClockSlotDefault
        if (v < 0) v = 0
        if (v > n - 1) v = n - 1
        return v
    }

    property real worldClockTargetX: NaN   // posizione voluta dall'utente (pre-clamp)
    property real worldClockTargetY: NaN
    // L'orologio è un OVERLAY FLOATING: vive in mainWindow.contentItem a coordinate
    // x,y libere (drag dalla maniglia), posizionabile ovunque nella finestra. NON usa
    // più gli host-slot dell'header. Posizione persistita in uiWorldClockX/Y.
    function applyWorldClockSlot() {
        if (typeof worldClock === "undefined" || !worldClock)
            return
        var host = mainWindow.contentItem
        if (!host)
            return
        if (worldClock.parent !== host)
            worldClock.parent = host
        worldClock.z = 1000
        var rx = safeBridgeSetting("uiWorldClockX", null)
        var ry = safeBridgeSetting("uiWorldClockY", null)
        var px = (rx === null || rx === undefined || String(rx).length === 0) ? NaN : parseFloat(rx)
        var py = (ry === null || ry === undefined || String(ry).length === 0) ? NaN : parseFloat(ry)
        if (isNaN(px)) px = host.width - worldClock.width - 12
        if (isNaN(py)) py = 6
        worldClockTargetX = px   // posizione VOLUTA (pre-clamp), per recupero al resize
        worldClockTargetY = py
        worldClock.x = clampClockX(px)
        worldClock.y = clampClockY(py)
    }

    function clampClockX(v) {
        var host = mainWindow.contentItem
        if (!host || host.width <= 0) return v
        var maxX = Math.max(0, host.width - worldClock.width)
        if (v < 0) return 0
        if (v > maxX) return maxX
        return v
    }

    function clampClockY(v) {
        var host = mainWindow.contentItem
        if (!host || host.height <= 0) return v
        var maxY = Math.max(0, host.height - worldClock.height)
        if (v < 0) return 0
        if (v > maxY) return maxY
        return v
    }

    function clampWorldClockNow() {
        if (typeof worldClock === "undefined" || !worldClock) return
        var tx = isNaN(worldClockTargetX) ? worldClock.x : worldClockTargetX
        var ty = isNaN(worldClockTargetY) ? worldClock.y : worldClockTargetY
        worldClock.x = clampClockX(tx)
        worldClock.y = clampClockY(ty)
    }

    function persistWorldClockPos() {
        if (typeof worldClock === "undefined" || !worldClock) return
        worldClockTargetX = worldClock.x
        worldClockTargetY = worldClock.y
        persistUiSetting("uiWorldClockX", Math.round(worldClock.x))
        persistUiSetting("uiWorldClockY", Math.round(worldClock.y))
    }

    function resetWorldClockPos() {
        persistUiSetting("uiWorldClockX", "")
        persistUiSetting("uiWorldClockY", "")
        applyWorldClockSlot()
    }

    // Snap magnetico al rilascio: data la X (spazio headerFlow) del puntatore, sceglie
    // l'host-slot col gap più vicino. Modellata su computeTargetIndex della toolbar:
    // per gli slot vuoti (width 0) il "centro" coincide con la loro x (posizione del gap).
    function computeClockSlot(sceneX) {
        if (!headerFlow || !headerFlow.clockSlots)
            return uiWorldClockHeaderSlot
        var slots = headerFlow.clockSlots
        var best = -1
        var bestDist = Number.MAX_VALUE
        for (var i = 0; i < slots.length; ++i) {
            var s = slots[i]
            if (!s)
                continue
            var mapped = s.mapToItem(headerFlow, s.width / 2, 0)
            var center = mapped.x
            var d = Math.abs(sceneX - center)
            if (d < bestDist) {
                bestDist = d
                best = i
            }
        }
        if (best < 0)
            return uiWorldClockHeaderSlot
        return best
    }

    // Sposta l'id dalla posizione 'from' alla posizione 'to' nel modello e committa.
    function moveToolbarButton(from, to) {
        if (from === to || from < 0 || to < 0)
            return
        var arr = uiToolbarOrder.slice()
        if (from >= arr.length || to >= arr.length)
            return
        var item = arr.splice(from, 1)[0]
        arr.splice(to, 0, item)
        uiToolbarOrder = arr
        persistToolbarOrder()
    }

    // === Ordine colonne pannelli decode (layout CLASSICO) — interscambiabili via drag ===
    // Stadio 1: Full Spectrum / Signal RX / Live Map riposizionabili fra i 3 slot-host
    // fissi dello SplitView (decodePanelsSplit). La mappa è un CSV di panelId; l'indice
    // nella lista = posizione dello slot-host che ospita quel pannello. Gemella di
    // uiToolbarOrder (parse/persist/listener). Default = ordine attuale → ZERO regressione.
    // Lo SWAP avviene fra DUE slot (il pannello trascinato e quello sotto al puntatore):
    // si scambiano i panelId nella mappa, si ri-assegnano i parent, si persiste.
    //
    // Stadio 2: aggiunto "txpanel" come 4° slot. Gli slot 0/1/2 = le 3 colonne dello
    // SplitView (colSlot0/1/2), lo slot 3 = la TX area (txSlot, dentro txPanelContainer,
    // FUORI dallo SplitView -> lo SplitView resta a 3 figli). Lo swap è ora CROSS-container:
    // re-parenta un pannello fra un colSlot e txSlot (anchors.fill -> assume la geometria
    // del nuovo host: colonna alta-stretta vs area TX larga-bassa, atteso/voluto).
    //
    // Stadio 3: aggiunta la Waterfall come 5° pannello (slot 4 = topSlot, il pannello
    // superiore dello SplitView VERTICALE mainVerticalSplit). Lo slot 4 è gestito ad
    // ALTEZZA (SplitView.preferredHeight) anziché a larghezza come i colSlot/txSlot; il
    // pannello "waterfall" (waterfallPanelHost, wrapper attorno al Loader+Waterfall) si
    // adatta via anchors.fill al nuovo host quando viene spostato. CRITICO: lo swap
    // RE-PARENTA l'Item Waterfall esistente SENZA distruggerlo/ricrearlo (il Loader
    // embedded NON tocca active/sourceComponent durante il re-parent -> il PanadapterItem
    // e il feed PCM via bridge restano vivi, niente "freeze waterfall"). Il default mette
    // "waterfall" in coda (indice 4 -> topSlot) per riflettere ESATTAMENTE il layout
    // attuale -> ZERO regressione; una mappa salvata a 3/4 elementi riceve gli id mancanti
    // (incluso "waterfall") appesi in coda nell'ordine di default -> migrazione indolore.
    // 1.0.385 — DX Cluster come 6° pannello con una 4ª COLONNA dedicata (colSlot3, indice 3).
    // La colonna collassa a 0 quando il Cluster è spento → default invariato (3 colonne).
    // TX passa all'indice 4, Waterfall all'indice 5.
    readonly property string uiClassicColumnOrderDefault: "fullspectrum,signalrx,livemap,dxcluster,txpanel,waterfall"
    readonly property var uiClassicColumnKnownIds: ["fullspectrum","signalrx","livemap","dxcluster","txpanel","waterfall"]
    property var uiClassicColumnOrder: parseClassicColumnOrder(String(bridge.getSetting("uiClassicColumnOrder", "") || ""))

    // Parsa il CSV in lista di panelId; ripristina/completa col default se assente/corrotto.
    // Garantisce sempre una permutazione completa dei 4 id noti (nessun doppione, nessun buco).
    // MIGRAZIONE Stadio 1->2: una mappa salvata a 3 elementi (senza "txpanel") mantiene le
    // 3 colonne dov'erano e riceve "txpanel" appeso in coda (slot 3) -> TX resta dov'è, zero
    // regressione. Lo stesso meccanismo (append degli id mancanti nell'ordine di default)
    // copre già qualunque sottoinsieme parziale.
    function parseClassicColumnOrder(csv) {
        var def = uiClassicColumnOrderDefault.split(",")
        if (!csv || csv.length === 0)
            return def
        var parts = String(csv).split(",")
        var out = []
        var seen = ({})
        for (var i = 0; i < parts.length; ++i) {
            var id = parts[i].trim()
            if (id.length === 0)
                continue
            if (uiClassicColumnKnownIds.indexOf(id) < 0)
                continue            // id sconosciuto -> scarta
            if (seen[id])
                continue            // dedup
            seen[id] = true
            out.push(id)
        }
        // 1.0.385 — inserisci gli id mancanti alla LORO posizione di default (non in coda):
        // così una mappa salvata a 5 id (senza "dxcluster") riceve dxcluster all'indice 3
        // e TX/Waterfall restano correttamente a 4/5, mantenendo permutazione completa.
        for (var j = 0; j < def.length; ++j) {
            if (!seen[def[j]]) {
                out.splice(Math.min(j, out.length), 0, def[j])
                seen[def[j]] = true
            }
        }
        if (out.length !== def.length)
            return def
        return out
    }

    function persistClassicColumnOrder() {
        persistUiSetting("uiClassicColumnOrder", uiClassicColumnOrder.join(","))
    }

    // Restituisce lo slot-host (Item) corrispondente all'indice 0/1/2/3/4, o null.
    // Slot 0/1/2 = colSlot0/1/2 (figli dello SplitView ORIZZONTALE decodePanelsSplit, a larghezza).
    // Slot 3     = txSlot (dentro txPanelContainer, FUORI dallo SplitView, larga-bassa).
    // Slot 4     = waterfallPanel (figlio TOP dello SplitView VERTICALE mainVerticalSplit, ad ALTEZZA).
    function classicSlotForIndex(idx) {
        switch (idx) {
            case 0: return (typeof decodePanelsSplit !== "undefined" && decodePanelsSplit
                            && typeof colSlot0 !== "undefined") ? colSlot0 : null
            case 1: return (typeof decodePanelsSplit !== "undefined" && decodePanelsSplit
                            && typeof colSlot1 !== "undefined") ? colSlot1 : null
            case 2: return (typeof decodePanelsSplit !== "undefined" && decodePanelsSplit
                            && typeof colSlot2 !== "undefined") ? colSlot2 : null
            // 1.0.385 — 4ª colonna dedicata al DX Cluster
            case 3: return (typeof decodePanelsSplit !== "undefined" && decodePanelsSplit
                            && typeof colSlot3 !== "undefined") ? colSlot3 : null
            case 4: return (typeof txSlot !== "undefined") ? txSlot : null
            case 5: return (typeof waterfallPanel !== "undefined") ? waterfallPanel : null
            default: return null
        }
    }

    // Restituisce il pannello (Item) corrispondente al panelId, o null.
    // "txpanel"   -> txPanelHostWrapper (wrapper re-parentabile attorno all'istanza TxPanel).
    // "waterfall" -> waterfallPanelHost (wrapper attorno al Loader+Waterfall embedded; il
    //                re-parent sposta QUESTO Item senza ricaricare il Loader -> feed PCM intatto).
    function classicPanelForId(panelId) {
        if (panelId === "fullspectrum")
            return (typeof period1Panel !== "undefined") ? period1Panel : null
        if (panelId === "signalrx")
            return (typeof rxFreqPanel !== "undefined") ? rxFreqPanel : null
        if (panelId === "livemap")
            return (typeof liveMapPanelHost !== "undefined") ? liveMapPanelHost : null
        if (panelId === "txpanel")
            return (typeof txPanelHostWrapper !== "undefined") ? txPanelHostWrapper : null
        if (panelId === "waterfall")
            return (typeof waterfallPanelHost !== "undefined") ? waterfallPanelHost : null
        if (panelId === "dxcluster")
            return (typeof dxClusterPanelHost !== "undefined") ? dxClusterPanelHost : null
        return null
    }

    // Assegna ad ogni pannello il parent = slot-host nella posizione indicata dalla mappa.
    // Operazione che preserva id/stato/binding (re-parent, come applyWorldClockSlot).
    // I pannelli usano anchors.fill: parent → riempiono lo slot che li ospita.
    // Stadio 2: gestisce 4 pannelli; il re-parent fra un colSlot (SplitView) e txSlot
    // (txPanelContainer) è cross-container ma `panel.parent = host` + anchors.fill funziona
    // comunque (i due host sono in sotto-alberi diversi della stessa finestra).
    // Stadio 3: gestisce 5 pannelli; il 5° (waterfall) può migrare fra il topSlot
    // (mainVerticalSplit, ad ALTEZZA) e un colSlot/txSlot (a LARGHEZZA). Il re-parent usa
    // SOLO `panel.parent = host` con guard `panel.parent !== slot` (no churn): NON tocca il
    // Loader embedded della Waterfall (active/sourceComponent restano invariati) -> l'Item
    // Waterfall e il suo PanadapterItem NON vengono mai distrutti/ricreati -> feed PCM e
    // Connections col bridge restano vivi, nessun "freeze waterfall".
    function applyClassicColumnOrder() {
        if (typeof decodePanelsSplit === "undefined" || !decodePanelsSplit) {
            Qt.callLater(applyClassicColumnOrder)
            return
        }
        for (var i = 0; i < uiClassicColumnOrder.length; ++i) {
            var panel = classicPanelForId(uiClassicColumnOrder[i])
            var slot = classicSlotForIndex(i)
            if (panel && slot && panel.parent !== slot)
                panel.parent = slot
        }
    }

    // Indice (0/1/2) della posizione che ospita il panelId nella mappa corrente; -1 se assente.
    function classicSlotIndexOfId(panelId) {
        return uiClassicColumnOrder.indexOf(panelId)
    }

    // panelId che occupa lo slot di indice idx (in base alla mappa corrente).
    function classicIdInSlot(idx) {
        return (idx >= 0 && idx < uiClassicColumnOrder.length) ? uiClassicColumnOrder[idx] : ""
    }

    // 1.0.388 — indice della colonna decode (0..3) che deve avere SplitView.fillWidth:
    // il primo slot che ospita un pannello NON collassabile (così c'è sempre esattamente
    // un riempitore visibile e lo spazio liberato da un pannello staccato viene assorbito).
    // Preferisce Full Spectrum; ripiega su qualunque slot non-livemap/non-dxcluster.
    function classicDecodeFillSlot() {
        if (ft2LinkModeActive)
            return -1
        var i
        for (i = 0; i < 4; ++i)
            if (classicIdInSlot(i) === "fullspectrum") return i
        for (i = 0; i < 4; ++i) {
            var id = classicIdInSlot(i)
            if (id !== "livemap" && id !== "dxcluster") return i
        }
        return 0
    }

    // Larghezza minima dello slot = minimo "naturale" del pannello che lo occupa
    // (segue il pannello, non lo slot, anche se la larghezza-valore è per-slot).
    // Read-only su mappa+occupante: non re-immette nulla nella width -> no binding loop.
    function classicMinWidthForSlot(idx) {
        switch (classicIdInSlot(idx)) {
            case "fullspectrum": return 360
            case "signalrx":     return 360
            case "livemap":      return 280
            case "txpanel":      return 320
            // La Waterfall, se messa in una COLONNA stretta, accetta geometria insolita
            // (scelta esplicita dell'utente): minimo modesto perché non collassi a 0.
            // Nel topSlot (slot 4, ad ALTEZZA) questa minWidth non viene usata dallo
            // SplitView verticale -> nessun effetto sul layout di default.
            case "waterfall":    return 280
            case "dxcluster":    return 320   // 1.0.385 — 4ª colonna DX Cluster
            default:             return 260
        }
    }

    // True se lo slot idx ospita la Live Map MA la Live Map è nascosta/staccata:
    // in tal caso lo slot collassa (preferredWidth/minimumWidth -> 0), come faceva
    // liveMapPanelHost.visible quando era figlio diretto dello SplitView.
    function classicSlotCollapsed(idx) {
        var id = classicIdInSlot(idx)
        if (mainWindow.ft2LinkModeActive
                && (id === "fullspectrum" || id === "signalrx"))
            return true
        if (id === "livemap")
            return !(mainWindow.liveMapPanelVisible && !mainWindow.liveMapDetached)
        // 1.0.385/386 — la 4ª colonna del DX Cluster esiste SOLO quando è dockato.
        // Se spento o staccato (finestra flottante) la colonna sparisce del tutto.
        if (id === "dxcluster")
            return !(mainWindow.dxClusterPanelVisible && !mainWindow.dxClusterDetached)
        return false
    }

    // SWAP dei panelId in due posizioni della mappa + ri-assegna parent + persiste.
    function swapClassicColumns(a, b) {
        if (a === b || a < 0 || b < 0)
            return
        var arr = uiClassicColumnOrder.slice()
        if (a >= arr.length || b >= arr.length)
            return
        var tmp = arr[a]
        arr[a] = arr[b]
        arr[b] = tmp
        uiClassicColumnOrder = arr
        applyClassicColumnOrder()
        persistClassicColumnOrder()
    }

    // Riporta l'ordine colonne al default (FS, Signal RX, Live Map) + re-parent + persist.
    // Agganciato al Reset Layout esistente (Ctrl+Shift+L / resetLayoutConfirmDialog).
    function resetClassicColumnOrder() {
        uiClassicColumnOrder = uiClassicColumnOrderDefault.split(",")
        applyClassicColumnOrder()
        persistClassicColumnOrder()
    }

    property bool uiBtnFooterResetVisible:    settingBool("uiBtnFooterResetVisible", true)
    property bool uiBtnFooterHistoryVisible:  settingBool("uiBtnFooterHistoryVisible", true)
    property bool uiBtnFooterDxcVisible:      settingBool("uiBtnFooterDxcVisible", true)

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
        { code: "lv", name: "Latviešu" },
        { code: "nl", name: "Nederlands" },
        { code: "ro", name: "Română" },
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
        case "lv": return "Valoda"
        case "nl": return "Taal"
        case "ro": return "Limbă"
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
        persistUiSetting("uiDxClusterToolbarVisible", visible)
        if (!visible)
            dxClusterPanelVisible = false
    }
    function raiseDxClusterPanel() {
        if (typeof dxClusterFloatingWindow === 'undefined' || !dxClusterFloatingWindow)
            return
        dxClusterMinimized = false
        dxClusterFloatingWindow.visibility = Window.Windowed
        dxClusterFloatingWindow.show()
        dxClusterFloatingWindow.raise()
        dxClusterFloatingWindow.requestActivate()
    }
    function openDxClusterPanel() {
        mainWindow.dxClusterPanelVisible = true
        Qt.callLater(function() { mainWindow.raiseDxClusterPanel() })
    }
    function setPskReporterToolbarVisible(visible) {
        pskReporterToolbarVisible = visible
        persistUiSetting("uiPskReporterToolbarVisible", visible)
        if (!visible && typeof pskSearchPopup !== "undefined")
            pskSearchPopup.close()
    }
    function setAsyncIconVisible(visible) {
        asyncIconVisible = visible
        persistUiSetting("uiAsyncIconVisible", visible)
    }
    onTimeSyncPanelVisibleChanged: persistUiSetting("uiTimeSyncPanelVisible", timeSyncPanelVisible)
    onActiveStationsPanelVisibleChanged: {
        persistUiSetting("uiActiveStationsPanelVisible", activeStationsPanelVisible)
        if (typeof activeStationsFloatingWindow !== "undefined") {
            if (activeStationsPanelVisible) {
                activeStationsFloatingWindow.showHostedWindow()
            } else if (activeStationsFloatingWindow.visible) {
                activeStationsFloatingWindow.hide()
            }
        }
    }
    onCallerQueuePanelVisibleChanged: persistUiSetting("uiCallerQueuePanelVisible", callerQueuePanelVisible)
    onAstroPanelVisibleChanged: persistUiSetting("uiAstroPanelVisible", astroPanelVisible)
    onDxClusterPanelVisibleChanged: {
        persistUiSetting("uiDxClusterPanelVisible", dxClusterPanelVisible)
        // 1.0.277 — dopo che la X della Window chiude DxClusterFloatingWindow,
        // Qt rompe il binding `visible: dxClusterPanelVisible` (la Window resta
        // hidden anche se il binding torna true). Forzo show()/hide() esplicito
        // sulla Window per garantire riapertura affidabile dal footer toggle.
        if (typeof dxClusterFloatingWindow !== 'undefined') {
            // 1.0.385 — la finestra flottante si mostra solo se il Cluster è STACCATO;
            // se è dockato (4ª colonna) la finestra resta nascosta e mostra l'inline.
            if (dxClusterPanelVisible && dxClusterDetached) {
                raiseDxClusterPanel()
            } else {
                dxClusterFloatingWindow.hide()
            }
        }
        Qt.callLater(mainWindow.restoreDecodePanelWidths)
    }
    onDecoSyncMonitorVisibleChanged: persistUiSetting("uiDecoSyncMonitorVisible", decoSyncMonitorVisible)
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

    // A detached map is a real secondary Window. Restoring it from the child
    // Window's Component.onCompleted is racy on some QPA backends: the menu
    // state is restored, while the Window misses its initial show event. Do a
    // second restore once the main Window has a native surface.
    function restoreLiveMapPopoutAfterStartup() {
        if (typeof liveMapFloatingWindow === "undefined" || !liveMapFloatingWindow)
            return

        var state = restoreFloatingWindowState(
                    liveMapFloatingWindow,
                    "liveMapFloatingWindow",
                    "liveMapDetached",
                    "")
        var shouldShow = mainWindow.liveMapPanelVisible
                && mainWindow.liveMapDetached
                && !mainWindow.liveMapMinimized
        if (!shouldShow) {
            liveMapFloatingWindow.hide()
            startupLog("live map popout restore: hidden visible="
                       + mainWindow.liveMapPanelVisible
                       + " detached=" + mainWindow.liveMapDetached)
            return
        }

        // Explicitly reset the visibility state before showing. This is needed
        // after a prior application shutdown closed the detached Window.
        liveMapFloatingWindow.visibility = Window.Windowed
        liveMapFloatingWindow.show()
        startupLog("live map popout restore: shown detached="
                   + mainWindow.liveMapDetached
                   + " x=" + liveMapFloatingWindow.x
                   + " y=" + liveMapFloatingWindow.y
                   + " saved=" + (state.detached === true))
    }
	    function detachWaterfallPanel() {
            mainWindow.waterfallPanelVisible = true
	        mainWindow.waterfallDetached = true
	        mainWindow.waterfallMinimized = false
	        waterfallPanel.isDockHighlighted = false
	        Qt.callLater(function() {
	            if (!mainWindow.waterfallDetached || mainWindow.waterfallMinimized)
	                return
	            waterfallWindow.show()
	            waterfallWindow.raise()
	            waterfallWindow.requestActivate()
	        })
	    }
	    function dockWaterfallPanel() {
            mainWindow.waterfallPanelVisible = true
	        waterfallPanel.isDockHighlighted = false
	        mainWindow.waterfallDetached = false
	        mainWindow.waterfallMinimized = false
	        waterfallWindow.hide()
	    }
	    function detachLiveMapPanel() {
	        mainWindow.liveMapPanelVisible = true
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
	        if (!mainWindow.ft2LinkModeActive) {
	            period1FloatingWindow.show()
	            period1FloatingWindow.visibility = Window.Windowed
	            period1FloatingWindow.raise()
	            period1FloatingWindow.requestActivate()
	        }
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
	        if (!mainWindow.ft2LinkModeActive) {
	            rxFreqFloatingWindow.show()
	            rxFreqFloatingWindow.visibility = Window.Windowed
	            rxFreqFloatingWindow.raise()
	            rxFreqFloatingWindow.requestActivate()
	        }
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
	    }
	    function dockSignalRxPanel() {
	        mainWindow.rxFreqDockHighlighted = false
	        mainWindow.rxFreqDetached = false
	        mainWindow.rxFreqMinimized = false
	        rxFreqFloatingWindow.hide()
	        Qt.callLater(mainWindow.restoreDecodePanelWidths)
    }
    // 1.0.385 — DX Cluster: stacca nella finestra flottante (colonna mostra il placeholder
    // di dock) / aggancia nella 4ª colonna (colSlot3) nascondendo la finestra.
    function detachDxClusterPanel() {
        mainWindow.dxClusterPanelVisible = true
        mainWindow.dxClusterDetached = true
        mainWindow.dxClusterMinimized = false
        raiseDxClusterPanel()
        Qt.callLater(mainWindow.restoreDecodePanelWidths)
    }
    function dockDxClusterPanel() {
        mainWindow.dxClusterPanelVisible = true
        mainWindow.dxClusterDetached = false
        mainWindow.dxClusterMinimized = false
        if (typeof dxClusterFloatingWindow !== "undefined" && dxClusterFloatingWindow)
            dxClusterFloatingWindow.hide()
        Qt.callLater(mainWindow.restoreDecodePanelWidths)
    }
    // 1.0.386 — inserisce la colonna DX Cluster SUBITO DOPO il pannello refId
    // ("fullspectrum" o "signalrx") riordinando la mappa, poi aggancia.
    function dockDxClusterNextTo(refId) {
        var arr = uiClassicColumnOrder.slice()
        var ci = arr.indexOf("dxcluster")
        if (ci >= 0)
            arr.splice(ci, 1)
        var ri = arr.indexOf(refId)
        if (ri < 0)
            ri = arr.indexOf("signalrx")   // fallback
        if (ri < 0)
            ri = 0
        arr.splice(ri + 1, 0, "dxcluster")
        uiClassicColumnOrder = arr
        applyClassicColumnOrder()
        persistClassicColumnOrder()
        dockDxClusterPanel()
    }
    onLiveMapPanelVisibleChanged: {
        persistUiSetting("WorldMapDisplayed", liveMapPanelVisible)
        Qt.callLater(function() {
            mainWindow.syncLiveMapFloatingVisibility(false)
            if (mainWindow.decodePanelLayoutSaved)
                mainWindow.restoreDecodePanelWidths()
            else if (typeof colSlot0 !== "undefined" && colSlot0)
                colSlot0.applyCenterSplit()
        })
    }

    // === Dialoghi ===
    Loader { id: colorDialogLoader; source: "../dialogs/ColorHighlightingDialog.qml"; active: false; asynchronous: true }
    Loader { id: qsyDialogLoader;   source: "../dialogs/QSYDialog.qml";              active: false; asynchronous: true }

    function openColorDialog() {
        colorDialogLoader.active = true
        colorDialogLoader.item.open()
    }
    function openQsyDialog() {
        qsyDialogLoader.active = true
        qsyDialogLoader.item.open()
    }

    // IU8LMC — Il BugReportDialog esisteva (con diagnostica, report e invio a
    // GitHub) ma NESSUNO lo apriva: loader active:false e zero punti di ingresso
    // = infrastruttura irraggiungibile. Da qui l'utente ci arriva davvero, e
    // l'autodiagnosi gli mostra le cause note prima che scriva.
    function openBugReportDialog() {
        bugReportDialogLoader.active = true
        runWhenLoaded(bugReportDialogLoader, function (item) { item.open() })
    }

    // IU8LMC — Aggiornamento: avviso + conferma.
    Loader {
        id: updateDialogLoader
        source: "components/UpdateDialog.qml"
        active: false
        asynchronous: true
        onLoaded: {
            // The first update request used to create the asynchronous Loader
            // without ever opening its item.  The second request worked only
            // because the item had finished loading by then.
            if (item)
                item.open()
        }
    }

    function openUpdateDialog() {
        updateDialogLoader.active = true
        if (updateDialogLoader.item)
            updateDialogLoader.item.open()
    }

    // IU8LMC — Popup "info stazione + meteo" ricevuta (opt-in, vedi
    // Settings > Station > "Show popup with correspondent's info").
    Loader {
        id: stationTelemetryLoader
        source: "components/StationTelemetryDialog.qml"
        active: false
        asynchronous: true
        property var pendingFields: ({})
        onLoaded: {
            if (item) {
                item.fields = pendingFields
                item.open()
            }
        }
    }

    Connections {
        target: bridge
        function onStationTelemetryDecoded(fields) {
            if (stationTelemetryLoader.item) {
                stationTelemetryLoader.item.fields = fields
                stationTelemetryLoader.item.open()
            } else {
                stationTelemetryLoader.pendingFields = fields
                stationTelemetryLoader.active = true
            }
        }
    }

    Connections {
        target: updater
        // Scatta sia dal controllo all'avvio sia da quello manuale: se c'e' una
        // versione nuova (e non e' stata saltata) l'utente lo deve sapere.
        function onUpdateFound(version) { openUpdateDialog() }
        // Controllo manuale: l'utente ha cliccato, una risposta gliela diamo
        // comunque (riuso il toast gia' esistente, niente UI nuova).
        function onUpToDate(version) {
            showStatusToast(qsTr("Decodium is up to date (%1).").arg(version), secondaryCyan)
        }
        function onErrorOccurred(message) {
            showStatusToast(message, "#ff6b6b")
        }
    }

    // Il controllo parte qualche secondo DOPO l'avvio: all'avvio audio e decoder
    // stanno gia' lavorando e la finestra deve comparire subito, una richiesta di
    // rete non deve entrarci in mezzo. checkOnStartupIfDue() rispetta comunque
    // l'impostazione dell'utente e non ricontrolla piu' di una volta al giorno.
    Timer {
        interval: 8000
        running: true
        repeat: false
        onTriggered: if (typeof updater !== "undefined") updater.checkOnStartupIfDue()
    }

    function runWhenLoaded(loader, action) {
        if (loader.item) {
            action(loader.item)
            return
        }
        loader.pendingAction = action
        // Let the mouse/touch event return before starting a lazy dialog.  The
        // loader is asynchronous, but activating it directly from a QML input
        // handler still makes the first parse/layout part of that handler.
        Qt.callLater(function() {
            if (!loader.item && !loader.active)
                loader.active = true
        })
    }

    function closeLoaded(loader) {
        if (loader.item && loader.item.close)
            loader.item.close()
    }

    function suspendTopmostPopoutsForSettings() {
        if (settingsTopmostPopoutsSuspended)
            return

        var candidates = [
            waterfallWindow,
            logFloatingWindow,
            astroFloatingWindow,
            satelliteFloatingWindow,
            macroFloatingWindow,
            mamFloatingWindow,
            decometerFloatingWindow,
            activeStationsFloatingWindow,
            rigFloatingWindow,
            period1FloatingWindow,
            period2FloatingWindow,
            rxFreqFloatingWindow,
            txPanelFloatingWindow
        ]
        var suspended = []
        for (var i = 0; i < candidates.length; ++i) {
            var floatingWindow = candidates[i]
            if (!floatingWindow || !floatingWindow.visible)
                continue
            suspended.push(floatingWindow)
            floatingWindow.hide()
        }

        settingsSuspendedTopmostPopouts = suspended
        settingsTopmostPopoutsSuspended = true
        if (suspended.length > 0)
            console.log("SETUP temporarily hid " + suspended.length + " always-on-top pop-out(s)")
    }

    function restoreTopmostPopoutsAfterSettings() {
        if (!settingsTopmostPopoutsSuspended)
            return

        var suspended = settingsSuspendedTopmostPopouts
        settingsSuspendedTopmostPopouts = []
        settingsTopmostPopoutsSuspended = false
        if (applicationClosing || !suspended || suspended.length === 0)
            return

        // Re-show after the QML modal overlay has been removed.  Do not raise
        // or activate the panels: closing Settings should leave focus where
        // the user expects it, while preserving every panel's geometry/state.
        Qt.callLater(function() {
            if (applicationClosing)
                return
            for (var i = 0; i < suspended.length; ++i) {
                var floatingWindow = suspended[i]
                if (floatingWindow && !floatingWindow.visible)
                    floatingWindow.show()
            }
        })
    }

    function openLogWindow() {
        logWindowDetached = true
        logWindowMinimized = false
        logFloatingWindow.showHostedWindow()
    }
    function openMacroDialog() {
        macroDialogDetached = true
        macroDialogMinimized = false
        macroFloatingWindow.showHostedWindow()
    }
    function openAstroWindow() {
        astroWindowDetached = true
        astroWindowMinimized = false
        astroFloatingWindow.showHostedWindow()
    }
    function openSatelliteWindow() {
        satelliteWindowDetached = true
        satelliteFloatingWindow.showHostedWindow()
    }
    function openSettingsDialog() {
        var requestedAt = Date.now()
        console.log("SETUP requested loaded=" + !!settingsDialogLoader.item)
        var openStartedAt = Date.now()
        console.log("SETUP open begin wait_ms=" + (openStartedAt - requestedAt))
        suspendTopmostPopoutsForSettings()
        settingsFloatingWindow.showHostedWindow(-1)
        console.log("SETUP open returned elapsed_ms=" + (Date.now() - openStartedAt))
    }
    function openSettingsTab(tabIndex) {
        suspendTopmostPopoutsForSettings()
        settingsFloatingWindow.showHostedWindow(tabIndex)
    }
    function openMamWindow() {
        mamFloatingWindow.showHostedWindow()
    }
    function openInfoDialog(tabIndex) {
        runWhenLoaded(infoDialogLoader, function(item) {
            item.currentTab = tabIndex
            item.open()
        })
    }
    function openCallDialog() {
        runWhenLoaded(callDialogLoader, function(item) { item.show() })
    }
    function openHistoryDialog() {
        runWhenLoaded(historyDialogLoader, function(item) { item.show() })
    }
    function openDecometerWindow() {
        decometerFloatingWindow.showHostedWindow()
    }

    function chooseWavFileForDecode() {
        var path = bridge.openFileDialog(qsTr("Open WAV file for decoding"),
                                         "",
                                         [qsTr("File WAV (*.wav)"), qsTr("All files (*)")])
        if (path.length === 0)
            return
        console.log("Opening WAV for decode: " + path)
        bridge.openWavForDecode(path)
    }

    function chooseWavFolderForDecode() {
        var path = bridge.openDirectoryDialog(qsTr("Select folder with WAV files"), "")
        if (path.length === 0)
            return
        console.log("Batch decode folder: " + path)
        bridge.openWavFolderDecode(path)
    }

    property string rigErrorDialogTitle: ""
    property string rigErrorSummary: ""
    property string rigErrorDetails: ""
    property bool rigErrorDetailsVisible: false
    property string warningDialogTitle: ""
    property string warningDialogSummary: ""
    property string warningDialogDetails: ""
    property bool warningDialogDetailsVisible: false
    property bool catFailureDialogShown: false

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
    // DX-Pedition Fase 1 — token aggiuntivi (validi su tutti i temi via fallback in ThemeManager)
    property color accentDim: bridge.themeManager.accentDim
    property color accentDeep: bridge.themeManager.accentDeep
    property color pileColor: bridge.themeManager.pileColor
    property color gridColor: bridge.themeManager.gridColor
    property color txColor: bridge.themeManager.txColor
    property color rxColor: bridge.themeManager.rxColor
    // DX-Pedition Fase 2a — opt-in 3-column tactical workspace (default OFF)
    property bool dxPeditionMode: false
    property bool showDxccInfo: bridge.getSetting("ShowDXCC", true)
    property bool fullSpectrumShowDistColumn: settingBool("uiFullSpectrumShowDistColumn", true)
    property bool fullSpectrumShowAzColumn: settingBool("uiFullSpectrumShowAzColumn", true)
    property bool signalRxShowFreqColumn: settingBool("uiSignalRxShowFreqColumn", true)
    property bool signalRxShowDistColumn: settingBool("uiSignalRxShowDistColumn", true)
    property bool signalRxShowAzColumn: settingBool("uiSignalRxShowAzColumn", true)
    property bool displayDistanceInMiles: settingBool("Miles", false)
    property bool showTxMessagesInRx: bridge.getSetting("TXMessagesToRX", true)
    property bool highlight73: bridge.getSetting("Highlight73", true)
    property bool highlightOrange: bridge.getSetting("HighlightOrange", false)
    property bool highlightBlue: bridge.getSetting("HighlightBlue", false)
    property string highlightOrangeCallsigns: bridge.getSetting("HighlightOrangeCallsigns", "")
    property string highlightBlueCallsigns: bridge.getSetting("HighlightBlueCallsigns", "")
    property int decodeColorBoost: Math.max(0, Math.min(100, Number(bridge.getSetting("uiDecodeColorBoost", 35))))
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
            if (key === "SuperFox")
                mainWindow.superFoxOptionEnabled = value === true || value === 1
                    || String(value).toLowerCase() === "true" || String(value) === "1"
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
            else if (key === "uiDecodeColorBoost") {
                mainWindow.decodeColorBoost = Math.max(0, Math.min(100, Number(value)))
                mainWindow.refreshDecodeColors()
            }
            else if (key === "WorldMapDisplayed")
                mainWindow.liveMapPanelVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiTimeSyncPanelVisible")
                mainWindow.timeSyncPanelVisible = mainWindow.coerceBool(value, false)
            else if (key === "uiActiveStationsPanelVisible")
                mainWindow.activeStationsPanelVisible = mainWindow.coerceBool(value, false)
            else if (key === "uiCallerQueuePanelVisible")
                mainWindow.callerQueuePanelVisible = mainWindow.coerceBool(value, false)
            else if (key === "uiAstroPanelVisible")
                mainWindow.astroPanelVisible = mainWindow.coerceBool(value, false)
            else if (key === "uiDxClusterPanelVisible")
                mainWindow.dxClusterPanelVisible = mainWindow.coerceBool(value, false)
            else if (key === "uiDxClusterToolbarVisible")
                mainWindow.dxClusterToolbarVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiPskReporterToolbarVisible")
                mainWindow.pskReporterToolbarVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiAsyncIconVisible")
                mainWindow.asyncIconVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiDecoSyncMonitorVisible")
                mainWindow.decoSyncMonitorVisible = mainWindow.coerceBool(value, false)
            else if (key === "uiBtnMonitorVisible")
                mainWindow.uiBtnMonitorVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnSetupVisible")
                mainWindow.uiBtnSetupVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnRecVisible")
                mainWindow.uiBtnRecVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnWavVisible")
                mainWindow.uiBtnWavVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnLogVisible")
                mainWindow.uiBtnLogVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnMacroVisible")
                mainWindow.uiBtnMacroVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnAstroVisible")
                mainWindow.uiBtnAstroVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnCatVisible")
                mainWindow.uiBtnCatVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiToolbarOrder") {
                mainWindow.uiToolbarOrder = mainWindow.parseToolbarOrder(String(value || ""))
            }
            else if (key === "uiClassicColumnOrder") {
                mainWindow.uiClassicColumnOrder = mainWindow.parseClassicColumnOrder(String(value || ""))
                mainWindow.applyClassicColumnOrder()
            }
            else if (key === "uiWorldClockHeaderSlot") {
                var slotVal = parseInt(value, 10)
                if (isNaN(slotVal))
                    slotVal = mainWindow.worldClockSlotDefault
                mainWindow.uiWorldClockHeaderSlot = mainWindow.clampWorldClockSlot(slotVal)
            }
            else if (key === "uiWorldClockBeforeToolbar")
                // Retrocompat: vecchio toggle bool → slot before/after toolbar.
                mainWindow.uiWorldClockHeaderSlot = mainWindow.coerceBool(value, false)
                    ? mainWindow.worldClockSlotBeforeToolbar : mainWindow.worldClockSlotAfterToolbar
            else if (key === "uiBtnFooterResetVisible")
                mainWindow.uiBtnFooterResetVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnFooterHistoryVisible")
                mainWindow.uiBtnFooterHistoryVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiBtnFooterDxcVisible")
                mainWindow.uiBtnFooterDxcVisible = mainWindow.coerceBool(value, true)
            else if (key === "uiWorldClockVisible")
                worldClock.showWorldClock = mainWindow.coerceBool(value, true)
            else if (key === "CompactFullSpectrum")
                mainWindow.compactFullSpectrum = mainWindow.coerceBool(value, false)
            else if (key === "CompactSignalRx")
                mainWindow.compactSignalRx = mainWindow.coerceBool(value, false)
            else if (key === "uiFullSpectrumShowDistColumn")
                mainWindow.fullSpectrumShowDistColumn = mainWindow.coerceBool(value, true)
            else if (key === "uiFullSpectrumShowAzColumn")
                mainWindow.fullSpectrumShowAzColumn = mainWindow.coerceBool(value, true)
            else if (key === "uiSignalRxShowFreqColumn")
                mainWindow.signalRxShowFreqColumn = mainWindow.coerceBool(value, true)
            else if (key === "uiSignalRxShowDistColumn")
                mainWindow.signalRxShowDistColumn = mainWindow.coerceBool(value, true)
            else if (key === "uiSignalRxShowAzColumn")
                mainWindow.signalRxShowAzColumn = mainWindow.coerceBool(value, true)
            else if (key === "Miles")
                mainWindow.displayDistanceInMiles = mainWindow.coerceBool(value, false)
            else if (key === "UILanguage")
                mainWindow.uiLanguage = mainWindow.normalizeUiLanguage(String(value || "en"))
            else if (key === "uiDecodePanelsLayoutSaved")
                mainWindow.decodePanelLayoutSaved = mainWindow.coerceBool(value, false)
        }
        function onColorCQChanged() { mainWindow.refreshDecodeColors() }
        function onColorMyCallChanged() { mainWindow.refreshDecodeColors() }
        function onColorDXEntityChanged() { mainWindow.refreshDecodeColors() }
        function onColor73Changed() { mainWindow.refreshDecodeColors() }
        function onColorB4Changed() { mainWindow.refreshDecodeColors() }
        function onColorDecodeTextChanged() { mainWindow.refreshDecodeColors() }
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
        function onDecodeColorEnabledChanged(prop, enabled) { mainWindow.refreshDecodeColors() }
        function onDecodeColorBoldChanged(prop, bold) { mainWindow.refreshDecodeColors() }
        function onDecodeColorBgChanged() { mainWindow.refreshDecodeColors() }
    }

    function decodeClamp01(value) {
        return Math.max(0, Math.min(1, Number(value)))
    }

    function decodeColorObject(value) {
        if (value === undefined || value === null)
            return null
        if (typeof value === "object" && value.r !== undefined)
            return value
        var text = String(value)
        if (text.length === 0)
            return null
        try {
            return Qt.color(text)
        } catch (e) {
            return null
        }
    }

    function boostedDecodeTextColor(value) {
        mainWindow.decodeColorRevision
        var boost = Math.max(0, Math.min(100, Number(mainWindow.decodeColorBoost))) / 100.0
        if (boost <= 0)
            return value
        var c = decodeColorObject(value)
        if (!c || c.a <= 0)
            return value

        var lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        var sat = 0.85 * boost
        var r = lum + (c.r - lum) * (1.0 + sat)
        var g = lum + (c.g - lum) * (1.0 + sat)
        var b = lum + (c.b - lum) * (1.0 + sat)
        r = decodeClamp01(r)
        g = decodeClamp01(g)
        b = decodeClamp01(b)

        var boostedLum = 0.2126 * r + 0.7152 * g + 0.0722 * b
        var targetLum = lum < 0.55
                ? lum + (0.68 - lum) * 0.75 * boost
                : lum + (0.95 - lum) * 0.35 * boost
        if (boostedLum < targetLum) {
            var mix = Math.min(0.75, (targetLum - boostedLum) / Math.max(0.001, 1.0 - boostedLum))
            r = r + (1.0 - r) * mix
            g = g + (1.0 - g) * mix
            b = b + (1.0 - b) * mix
        }
        return Qt.rgba(decodeClamp01(r), decodeClamp01(g), decodeClamp01(b), c.a)
    }

    function boostedDecodeBackgroundColor(value) {
        mainWindow.decodeColorRevision
        var boost = Math.max(0, Math.min(100, Number(mainWindow.decodeColorBoost))) / 100.0
        if (boost <= 0)
            return value
        var c = decodeColorObject(value)
        if (!c || c.a <= 0)
            return value

        var lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        var sat = 0.55 * boost
        var r = decodeClamp01(c.r + (c.r - lum) * sat)
        var g = decodeClamp01(c.g + (c.g - lum) * sat)
        var b = decodeClamp01(c.b + (c.b - lum) * sat)
        var alphaLift = c.a < 0.12 ? 0.18 * boost : 0.28 * boost
        var alphaCap = c.a < 0.12 ? 0.32 : 0.72
        return Qt.rgba(r, g, b, Math.min(alphaCap, c.a + alphaLift))
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

    function effectiveDecodeColor(prop) {
        mainWindow.decodeColorRevision
        return bridge.effectiveDecodeColor(prop)
    }

    function decodeColorBoldEnabled(prop) {
        mainWindow.decodeColorRevision
        return !!(bridge.decodeColorEnabled(prop) && bridge.decodeColorBold(prop))
    }

    function decodeColorCategoryEnabled(prop) {
        mainWindow.decodeColorRevision
        return !!(bridge && bridge.decodeColorEnabled(prop))
    }

    function decodeTextColorProp(modelData) {
        if (!modelData)
            return "colorDecodeText"
        if (modelData.isTx === true && decodeColorCategoryEnabled("colorTxMessage")) return "colorTxMessage"
        if (modelData.isMyCall === true && decodeColorCategoryEnabled("colorMyCall")) return "colorMyCall"
        if (highlight73 && isSignoffMessage(modelData.message) && decodeColorCategoryEnabled("color73")) return "color73"
        if ((modelData.isB4 === true || modelData.dxIsWorked === true) && decodeColorCategoryEnabled("colorB4")) return "colorB4"
        if (modelData.isCQ === true && decodeColorCategoryEnabled("colorCQ")) return "colorCQ"
        if (modelData.dxIsNewDxccBand === true && decodeColorCategoryEnabled("colorNewDxccBand")) return "colorNewDxccBand"
        if (modelData.dxIsNewDxcc === true && decodeColorCategoryEnabled("colorNewDxcc")) return "colorNewDxcc"
        if (modelData.dxIsNewContinentBand === true && decodeColorCategoryEnabled("colorNewContinentBand")) return "colorNewContinentBand"
        if (modelData.dxIsNewContinent === true && decodeColorCategoryEnabled("colorNewContinent")) return "colorNewContinent"
        if (modelData.dxIsNewCqZoneBand === true && decodeColorCategoryEnabled("colorNewCqZoneBand")) return "colorNewCqZoneBand"
        if (modelData.dxIsNewCqZone === true && decodeColorCategoryEnabled("colorNewCqZone")) return "colorNewCqZone"
        if (modelData.dxIsNewItuZoneBand === true && decodeColorCategoryEnabled("colorNewItuZoneBand")) return "colorNewItuZoneBand"
        if (modelData.dxIsNewItuZone === true && decodeColorCategoryEnabled("colorNewItuZone")) return "colorNewItuZone"
        if (modelData.dxIsNewGridBand === true && decodeColorCategoryEnabled("colorNewGridBand")) return "colorNewGridBand"
        if (modelData.dxIsNewGrid === true && decodeColorCategoryEnabled("colorNewGrid")) return "colorNewGrid"
        if (modelData.dxIsNewCallBand === true && decodeColorCategoryEnabled("colorNewCallBand")) return "colorNewCallBand"
        if (modelData.dxIsNewCall === true && decodeColorCategoryEnabled("colorNewCall")) return "colorNewCall"
        if ((modelData.dxIsMostWanted === true || modelData.dxIsNewCountry === true || modelData.dxIsNewBand === true)
                && decodeColorCategoryEnabled("colorDXEntity"))
            return "colorDXEntity"
        return "colorDecodeText"
    }

    function decodeEntryBoldForModel(modelData) {
        mainWindow.decodeColorRevision
        if (!modelData)
            return false
        if (customHighlightColor(modelData) !== "")
            return false
        return decodeColorBoldEnabled(decodeTextColorProp(modelData))
    }

    // 1.0.416 — colore di SFONDO riga scelto dall'utente per categoria (opt-in).
    // null se per la categoria della riga lo sfondo non è abilitato → fallback al
    // comportamento esistente. Dipende da decodeColorRevision per la reattività.
    function decodeUserBgFill(modelData) {
        mainWindow.decodeColorRevision
        if (!modelData) return null
        var hex = bridge.decodeHighlightUserBg(modelData)
        return (hex && hex.length > 0) ? Qt.color(hex) : null
    }

    function lotwMarkerColor() {
        return boostedDecodeTextColor(effectiveDecodeColor("colorLotwUser"))
    }

	    // Shannon-compatible color function (allineato a DecodeWindow.qml)
    function getDxccColor(modelData) {
        if (!modelData)
            return boostedDecodeTextColor(textPrimary)
        var customColor = customHighlightColor(modelData)
        if (customColor !== "") return boostedDecodeTextColor(customColor)
        return boostedDecodeTextColor(effectiveDecodeColor(decodeTextColorProp(modelData)))
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
        return boostedDecodeBackgroundColor(Qt.rgba(c.r, c.g, c.b, 0.35))
    }

    function decodeHighlightBorder(modelData) {
        var hex = decodeRowHighlightHex(modelData)
        if (hex.length === 0)
            return null
        var c = Qt.color(hex)
        return boostedDecodeTextColor(Qt.rgba(c.r, c.g, c.b, 0.85))
    }

    function fullSpectrumTextColor(modelData) {
        if (!modelData)
            return textPrimary
        var rowHex = decodeRowHighlightHex(modelData)
        if (rowHex.length > 0)
            return readableTextOnHighlight(rowHex)

        var customColor = customHighlightColor(modelData)
        if (customColor !== "")
            return boostedDecodeTextColor(customColor)
        if (highlight73 && isSignoffMessage(modelData.message))
            return boostedDecodeTextColor(effectiveDecodeColor("color73"))
        if (modelData.isB4 === true || modelData.dxIsWorked === true)
            return boostedDecodeTextColor(effectiveDecodeColor("colorB4"))

        var textHex = decodePassiveHighlightTextColor(modelData)
        if (textHex.length > 0)
            return boostedDecodeTextColor(textHex)

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

    // IU8LMC: click destro su un decode -> apre la scheda del nominativo su QRZ.com nel browser.
    // Usa la call base (i portable/prefix risolvono sulla scheda dell'operatore).
    function openQrzLookup(modelData) {
        if (!modelData)
            return
        var call = callsignBase(String(modelData.dxCallsign || ""))
        if (call.length === 0)
            call = callsignBase(firstMessageCallsign(modelData.message || ""))
        if (call.length === 0)
            return
        Qt.openUrlExternally("https://www.qrz.com/db/" + call.toUpperCase())
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
            return formatDistanceText(modelData.dxDistance, false)
        return ""
    }

    function formatDistanceText(distanceKm, withSpace) {
        var km = Number(distanceKm)
        if (!isFinite(km) || km <= 0)
            return ""
        var value = displayDistanceInMiles ? km * 0.621371192 : km
        return Math.round(value) + (withSpace ? " " : "") + (displayDistanceInMiles ? "mi" : "km")
    }

    function usStateLabel(modelData) {
        if (!bridge || !bridge.showUsState || !modelData || !modelData.usState)
            return ""
        return String(modelData.usState).trim().toUpperCase()
    }

    function dxccDisplayText(modelData) {
        if (!modelData)
            return ""
        var country = modelData.dxCountry ? String(modelData.dxCountry) : ""
        var state = usStateLabel(modelData)
        if (country.length > 0 && state.length > 0)
            return country + " · " + state
        return country.length > 0 ? country : state
    }

    // IU8LMC: Function to build tooltip text
    function getDxccTooltipText(modelData) {
        if (!modelData) return ""
        var dxccText = dxccDisplayText(modelData)
        if (!dxccText) return ""
        var lines = []
        var header = modelData.dxCallsign + " - " + dxccText
        if (modelData.dxContinent) header += " (" + modelData.dxContinent + ")"
        lines.push(header)
        // Bearing and distance to DX station
        if (!isLocalDistanceEntry(modelData) && modelData.dxBearing !== undefined && modelData.dxBearing >= 0) {
            var bearingDist = "Az: " + Math.round(modelData.dxBearing) + "°"
            if (modelData.dxDistance !== undefined && modelData.dxDistance > 0) {
                bearingDist += "  Dist: " + formatDistanceText(modelData.dxDistance, true)
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

    // ===================================================================
    // Full Spectrum — colonne configurabili (larghezza / visibilità /
    // ordine) + flip cronologico newest-first. Modello-config UNICO
    // condiviso dal pannello embedded (evenPeriodList) e dalla finestra
    // staccata (period1FloatingList). Solo UI/QML: il modello dati C++
    // (bandActivityModel) e la pipeline decode restano INTATTI.
    // By IU8LMC
    // ===================================================================
    // Ordine + visibilità: drive del Repeater header/righe. SEMPRE riassegnato
    // (mai mutato in place) per notificare i binding.
    property var fsColumnOrder: fsDefaultOrder()
    // Larghezze per-id (mappa). Cambia durante il drag SENZA ricreare i Repeater
    // (così il MouseArea trascinato non viene distrutto a metà gesto).
    property var fsColWidthMap: fsDefaultWidths()
    // Bump per forzare la rivalutazione delle Layout.preferredWidth durante il drag.
    property int fsColWidthVer: 0
    // Flip cronologico: true = più recenti in alto (ListView BottomToTop).
    property bool fsNewestFirst: false

    function fsDefaultOrder() {
        return [
            { id: "utc",  vis: true },
            { id: "db",   vis: true },
            { id: "dt",   vis: true },
            { id: "freq", vis: true },
            { id: "drift", vis: true },
            { id: "msg",  vis: true },
            { id: "dist", vis: true },
            { id: "dxcc", vis: true },
            { id: "az",   vis: true }
        ]
    }
    function fsDefaultWidths() {
        return { utc: 86, db: 38, dt: 48, freq: 45, drift: 42, msg: 140, dist: 58, dxcc: 200, az: 52 }
    }
    // Metadati statici per colonna (label, allineamento, flessibile, nascondibile, min px).
    function fsColMeta(id) {
        switch (id) {
        case "utc":  return { label: qsTr("UTC"),     align: "left",  fill: false, canHide: true,  minW: 44 }
        case "db":   return { label: qsTr("dB"),      align: "right", fill: false, canHide: true,  minW: 24 }
        case "dt":   return { label: qsTr("DT"),      align: "right", fill: false, canHide: true,  minW: 28 }
        case "freq": return { label: qsTr("Freq"),    align: "right", fill: false, canHide: true,  minW: 30 }
        case "drift": return { label: qsTr("Drift"),  align: "right", fill: false, canHide: true,  minW: 34 }
        case "msg":  return { label: qsTr("Message"), align: "left",  fill: true,  canHide: false, minW: 72 }
        case "dist": return { label: qsTr("Dist"),    align: "right", fill: false, canHide: true,  minW: 36 }
        case "dxcc": return { label: qsTr("DXCC"),    align: "right", fill: false, canHide: true,  minW: 90 }
        case "az":   return { label: qsTr("Az"),      align: "right", fill: false, canHide: true,  minW: 34 }
        }
        return { label: id, align: "left", fill: false, canHide: true, minW: 30 }
    }

    // Colonne effettivamente visibili (filtro vis + gate DXCC/Az quando il
    // lookup DXCC è disattivato, come da comportamento storico).
    readonly property var fsVisibleColumns: {
        var out = []
        var arr = fsColumnOrder || []
        for (var i = 0; i < arr.length; ++i) {
            var c = arr[i]
            if (!c || !c.vis) continue
            if (c.id === "drift" && (!bridge || bridge.mode !== "WSPR")) continue
            if ((c.id === "dxcc" || c.id === "az") && !mainWindow.showDxccInfo) continue
            out.push(c)
        }
        return out
    }

    // Responsive column set for narrow embedded/floating monitors. The
    // Message column must never overlap a trailing metadata column.
    function fsColWidthForPanel(id, panelWidth) {
        var base = fsColWidth(id)
        var width = Math.max(0, Number(panelWidth) || 0)
        if (width >= 760)
            return base

        // Persisted desktop widths are intentionally capped in compact
        // panels; otherwise one old 200px DXCC width can evict Message/Dist.
        var compact = {
            utc: 66, db: 34, dt: 42, freq: 42, drift: 34,
            dist: 52, dxcc: 120, az: 34
        }
        if (compact[id] !== undefined)
            return Math.max(fsColMeta(id).minW, Math.min(base, compact[id]))
        return base
    }

    function fsColumnsForWidth(panelWidth) {
        var out = (fsVisibleColumns || []).slice()
        var available = Math.max(0, Number(panelWidth) || 0)
        var minMessageWidth = fsColMeta("msg").minW
        var horizontalMargins = 16
        var columnSpacing = 6
        function requiredWidth(columns) {
            var total = horizontalMargins + Math.max(0, columns.length - 1) * columnSpacing + minMessageWidth
            for (var i = 0; i < columns.length; ++i) {
                if (columns[i].id !== "msg")
                    total += fsColWidthForPanel(columns[i].id, available)
            }
            return total
        }

        // Preserve Message and the core timing/frequency columns first.
        var removable = ["az", "drift", "dxcc", "dist"]
        for (var r = 0; r < removable.length && requiredWidth(out) > available; ++r) {
            for (var i = 0; i < out.length; ++i) {
                if (out[i].id === removable[r]) {
                    out.splice(i, 1)
                    break
                }
            }
        }
        return out
    }

    function fsColWidth(id) {
        var m = fsColWidthMap || {}
        var w = Number(m[id])
        if (!isFinite(w) || w <= 0)
            w = Number(fsDefaultWidths()[id]) || 48
        return Math.max(fsColMeta(id).minW, Math.round(w))
    }
    function fsColVisible(id) {
        var arr = fsColumnOrder || []
        for (var i = 0; i < arr.length; ++i)
            if (arr[i] && arr[i].id === id) return !!arr[i].vis
        return false
    }
    function fsColIndex(id) {
        var arr = fsColumnOrder || []
        for (var i = 0; i < arr.length; ++i)
            if (arr[i] && arr[i].id === id) return i
        return -1
    }
    function fsCanMove(id, dir) {
        var i = fsColIndex(id)
        if (i < 0) return false
        var j = i + dir
        return j >= 0 && j < (fsColumnOrder ? fsColumnOrder.length : 0)
    }

    function fsLoadColumns() {
        var order = fsDefaultOrder()
        var widths = fsDefaultWidths()
        try {
            var rawO = safeBridgeSetting("uiFullSpectrumColumns", "")
            if (rawO) {
                var savedO = JSON.parse(rawO)
                if (Array.isArray(savedO) && savedO.length) {
                    var def = fsDefaultOrder()
                    var known = {}
                    for (var d = 0; d < def.length; ++d) known[def[d].id] = true
                    var merged = []
                    var seen = {}
                    for (var s = 0; s < savedO.length; ++s) {
                        var so = savedO[s]
                        if (so && so.id && known[so.id] && !seen[so.id]) {
                            var vis = (so.vis === false) ? false : true
                            if (so.id === "msg") vis = true   // Message mai nascondibile
                            merged.push({ id: so.id, vis: vis })
                            seen[so.id] = true
                        }
                    }
                    for (var k = 0; k < def.length; ++k)
                        if (!seen[def[k].id]) merged.push(def[k])
                    if (!seen["drift"]) {
                        var driftAt = -1
                        var freqAt = -1
                        for (var mi = 0; mi < merged.length; ++mi) {
                            if (merged[mi].id === "drift") driftAt = mi
                            if (merged[mi].id === "freq") freqAt = mi
                        }
                        if (driftAt >= 0 && freqAt >= 0 && driftAt !== freqAt + 1) {
                            var driftCol = merged.splice(driftAt, 1)[0]
                            if (driftAt < freqAt) freqAt--
                            merged.splice(freqAt + 1, 0, driftCol)
                        }
                    }
                    if (merged.length) order = merged
                }
            }
        } catch (e1) { console.log("fsLoadColumns order parse error: " + e1) }
        try {
            var rawW = safeBridgeSetting("uiFullSpectrumColWidths", "")
            if (rawW) {
                var savedW = JSON.parse(rawW)
                if (savedW && typeof savedW === "object") {
                    var def2 = fsDefaultWidths()
                    for (var key in def2) {
                        var wv = Number(savedW[key])
                        if (isFinite(wv) && wv > 0)
                            widths[key] = Math.max(fsColMeta(key).minW, Math.round(wv))
                    }
                }
            }
        } catch (e2) { console.log("fsLoadColumns widths parse error: " + e2) }

        fsColumnOrder = order
        fsColWidthMap = widths
        fsColWidthVer++
        fsNewestFirst = settingBool("uiFullSpectrumNewestFirst", false)
    }

    function fsPersistOrder() {
        if (!bridge) return
        bridge.setSetting("uiFullSpectrumColumns", JSON.stringify(fsColumnOrder))
        if (!windowStateRestoreInProgress) scheduleSave()
    }
    function fsPersistWidths() {
        if (!bridge) return
        bridge.setSetting("uiFullSpectrumColWidths", JSON.stringify(fsColWidthMap))
        if (!windowStateRestoreInProgress) scheduleSave()
    }

    function fsSetColumnVisible(id, on) {
        var meta = fsColMeta(id)
        if (!meta.canHide && !on) return
        var arr = (fsColumnOrder || []).slice()
        var i = fsColIndex(id)
        if (i < 0) return
        arr[i] = { id: arr[i].id, vis: !!on }
        fsColumnOrder = arr
        fsPersistOrder()
    }
    function fsToggleColumnVisible(id) { fsSetColumnVisible(id, !fsColVisible(id)) }

    function fsMoveColumn(id, dir) {
        if (!fsCanMove(id, dir)) return
        var arr = (fsColumnOrder || []).slice()
        var i = fsColIndex(id)
        var j = i + dir
        var tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp
        fsColumnOrder = arr
        fsPersistOrder()
    }
    // Sposta la colonna `id` alla posizione della colonna `targetId` (drag libero).
    // NON persiste: il controller di drag chiama fsPersistOrder() al rilascio.
    function fsMoveColumnToId(id, targetId) {
        if (id === targetId) return
        var arr = (fsColumnOrder || []).slice()
        var from = -1, to = -1
        for (var i = 0; i < arr.length; ++i) {
            if (arr[i].id === id) from = i
            if (arr[i].id === targetId) to = i
        }
        if (from < 0 || to < 0 || from === to) return
        var item = arr.splice(from, 1)[0]
        // Inserisci alla posizione `to` dell'array già accorciato: la colonna
        // trascinata finisce sotto il cursore sia da sinistra che da destra.
        arr.splice(to, 0, item)
        fsColumnOrder = arr
    }

    function fsSetColumnWidth(id, w) {
        var meta = fsColMeta(id)
        if (meta.fill) return
        var m = {}
        var src = fsColWidthMap || {}
        for (var key in src) m[key] = src[key]
        m[id] = Math.max(meta.minW, Math.round(w))
        fsColWidthMap = m
        fsColWidthVer++          // forza re-eval delle preferredWidth senza ricreare i Repeater
    }

    function fsResetColumns() {
        fsColumnOrder = fsDefaultOrder()
        fsColWidthMap = fsDefaultWidths()
        fsColWidthVer++
        fsPersistOrder()
        fsPersistWidths()
    }
    function fsToggleNewestFirst() {
        fsNewestFirst = !fsNewestFirst
        persistUiSetting("uiFullSpectrumNewestFirst", fsNewestFirst)
        if (typeof evenPeriodList !== "undefined" && evenPeriodList) evenPeriodList.forceTailFollow()
        if (typeof period1FloatingList !== "undefined" && period1FloatingList) period1FloatingList.forceTailFollow()
    }

    // Risolutori contenuto cella. SEMPRE null-safe (guard !entry) per evitare il
    // TypeError flood ~46/s durante i model-swap transient (lezione 1.0.205).
    function fsCellText(entry, id) {
        if (!entry) return ""
        switch (id) {
        case "utc":  return entry.formattedTime || decodePanel.formatUtcForDisplay(entry.time)
        case "db":   return entry.db || ""
        case "dt":   return entry.dt || ""
        case "freq": return entry.freq || ""
        case "drift": return entry.mode === "WSPR" ? (entry.drift || "0") : ""
        case "msg":  return entry.displayMessage || entry.message || ""
        case "dist": return decodePanel.distanceText(entry)
        case "dxcc": return dxccDisplayText(entry)
        case "az":   return formatBearingDegrees(entry.dxBearing)
        }
        return ""
    }
    function fsCellColor(entry, id) {
        if (!entry) return boostedDecodeTextColor(textSecondary)
        switch (id) {
        case "msg":  return fullSpectrumTextColor(entry)
        case "freq": return boostedDecodeTextColor(entry.isTx ? "#f1c40f" : secondaryCyan)
        case "drift": return boostedDecodeTextColor(textSecondary)
        case "db":   return boostedDecodeTextColor(entry.snrColor || (entry.isTx ? "#f1c40f" : textSecondary))
        case "dxcc": return boostedDecodeTextColor((entry.dxCountry || entry.usState) && decodeColorCategoryEnabled("colorDXEntity") ? effectiveDecodeColor("colorDXEntity") : textSecondary)
        case "az":   return boostedDecodeTextColor(secondaryCyan)
        }
        return boostedDecodeTextColor(entry.isTx ? "#f1c40f" : textSecondary)
    }
    function fsCellBold(entry, id) {
        if (!entry) return false
        switch (id) {
        case "db":   return entry.isTx === true
        case "freq": return entry.isTx === true
        case "msg":  return decodePanel.decodeEntryBold(entry)
        }
        return false
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

    Dialog {
        id: ft2LinkAccessDialog
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        width: Math.min(430, Math.max(300, mainWindow.width - 48))
        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        padding: 0

        onOpened: {
            ft2LinkPasswordField.text = ""
            Qt.callLater(function() { ft2LinkPasswordField.forceActiveFocus() })
        }

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12
            anchors.margins: 16

            Text {
                Layout.fillWidth: true
                text: qsTr("FT2-Link access")
                color: secondaryCyan
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Enter password to unlock this mode.")
                color: textSecondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            TextField {
                id: ft2LinkPasswordField
                Layout.fillWidth: true
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                selectByMouse: true
                color: textPrimary
                placeholderTextColor: textSecondary
                background: Rectangle {
                    color: bgMedium
                    border.color: ft2LinkPasswordField.activeFocus
                                  ? secondaryCyan
                                  : glassBorder
                    radius: 5
                }
                Keys.onEscapePressed: mainWindow.rejectFt2LinkAccess()
                onAccepted: mainWindow.acceptFt2LinkAccess()
            }

            Text {
                Layout.fillWidth: true
                visible: mainWindow.ft2LinkAccessError.length > 0
                text: mainWindow.ft2LinkAccessError
                color: bridge.themeManager.warningColor
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }

                Button {
                    text: "Cancel"
                    onClicked: mainWindow.rejectFt2LinkAccess()
                }

                Button {
                    text: qsTr("Unlock")
                    highlighted: true
                    onClicked: mainWindow.acceptFt2LinkAccess()
                }
            }
        }
    }

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
                text: qsTr("Based on WSJT-X by K1JT et al.")
                font.pixelSize: 14
                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("QML Interface by IU8LMC")
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
                        running: bridge.pskSearching && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                    text: qsTr("Active on bands during the last %1 minutes:")
                          .arg(bridge.pskReporterTimeSpanMinutes)
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
                text: qsTr("No recent activity found\n(last 15 minutes)")
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
                text: qsTr("Valid range: 100-5000 Hz")
                font.pixelSize: 11
                color: textSecondary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                DecoTextField {
                    id: freqInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    font.pixelSize: 18
                    font.family: decodiumMonoFontFamily
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
            text: qsTr("Dock the Waterfall here")
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
            visible: !mainWindow.dxPeditionMode
            Layout.preferredHeight: visible ? headerFlow.height + 12 : 0
            Layout.minimumHeight: visible ? 86 : 0
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
            z: 100

            Flow {
                id: headerFlow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 6
                spacing: 8

                // ── Host-slot dell'orologio fra i blocchi dell'header ──
                // 6 Item "gancio" nei gap STABILI del Flow. L'orologio (worldClock) è
                // re-parentato in UNO di essi (vedi mainWindow.applyWorldClockSlot). Quando
                // uno slot NON ospita l'orologio è width 0 E visible:false, così il Flow lo
                // salta del tutto (niente micro-gap residuo dallo spacing fra invisibili).
                readonly property var clockSlots: [clockSlot0, clockSlot1, clockSlot2,
                                                   clockSlot3, clockSlot4, clockSlot5]

                Component.onCompleted: mainWindow.applyWorldClockSlot()

                // slot 0 — prima dell'hamburger
                Item {
                    id: clockSlot0
                    height: 80
                    width: worldClock.parent === clockSlot0 ? worldClock.width : 0
                    visible: worldClock.parent === clockSlot0
                }

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
                    ToolTip.text: qsTr("Menu")
                    ToolTip.delay: 500
                }

                // slot 1 — fra hamburger e logo
                Item {
                    id: clockSlot1
                    height: 80
                    width: worldClock.parent === clockSlot1 ? worldClock.width : 0
                    visible: worldClock.parent === clockSlot1
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
                            font.pixelSize: 12
                            font.bold: true
                            color: "#ffffff"
                        }
                    }
                }

                // slot 2 — fra logo e freq display
                Item {
                    id: clockSlot2
                    height: 80
                    width: worldClock.parent === clockSlot2 ? worldClock.width : 0
                    visible: worldClock.parent === clockSlot2
                }

                // Radio Frequency Display with CAT status
                Rectangle {
                    width: bridge.catConnected ? 340 : 290
                    height: 74
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: mainWindow.txVisualActive ? bridge.themeManager.ledRed
                                  : (mainWindow.txPttPending ? bridge.themeManager.warningColor
                                     : (bridge.catConnected ? accentGreen : glassBorder))
                    border.width: (mainWindow.txVisualActive || mainWindow.txPttPending || bridge.catConnected) ? 2 : 1
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
                                    running: bridge.catConnected && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.4; duration: 800 }
                                    OpacityAnimator { to: 1.0; duration: 800 }
                                }
                            }

                            // Frequency display - syncs with radio or band selection
                            Row {
                                id: frequencyDisplay
                                property int digitHeight: Math.round(34 * fs)
                                Layout.fillWidth: true
                                Layout.preferredHeight: digitHeight
                                spacing: 0

                                Repeater {
                                    model: mainWindow.frequencyDisplayCells(bridge.displayFrequency)

                                    delegate: Rectangle {
                                        id: frequencyDigitCell
                                        required property var modelData
                                        readonly property bool digitCell: !!modelData.digit
                                        readonly property int stepHz: Number(modelData.stepHz || 0)
                                        readonly property color baseDigitColor: mainWindow.txVisualActive ? "#ff6b6b"
                                                                           : (mainWindow.txPttPending ? bridge.themeManager.warningColor
                                                                                                      : accentGreen)
                                        width: digitCell ? Math.round(16 * fs) : Math.round(8 * fs)
                                        height: frequencyDisplay.digitHeight
                                        radius: 2
                                        color: digitCell && digitUpArea.containsMouse
                                               ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.20)
                                               : digitCell && digitDownArea.containsMouse
                                                 ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
                                                 : "transparent"

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            height: 1
                                            visible: frequencyDigitCell.digitCell && digitUpArea.containsMouse
                                            color: accentGreen
                                        }

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: 1
                                            visible: frequencyDigitCell.digitCell && digitDownArea.containsMouse
                                            color: secondaryCyan
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: frequencyDigitCell.modelData.text
                                            font.pixelSize: Math.round(26 * fs)
                                            font.family: decodiumMonoFontFamily
                                            font.bold: true
                                            color: digitCell && digitUpArea.containsMouse
                                                   ? "#8cffb8"
                                                   : digitCell && digitDownArea.containsMouse
                                                     ? "#8fd7ff"
                                                     : frequencyDigitCell.baseDigitColor

                                            Behavior on color { ColorAnimation { duration: 90 } }
                                        }

                                        MouseArea {
                                            id: digitUpArea
                                            enabled: frequencyDigitCell.digitCell && !bridge.transmitting && !bridge.tuning
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            height: parent.height / 2
                                            hoverEnabled: true
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: mainWindow.tuneDialByStep(frequencyDigitCell.stepHz, 1)
                                        }

                                        MouseArea {
                                            id: digitDownArea
                                            enabled: frequencyDigitCell.digitCell && !bridge.transmitting && !bridge.tuning
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: parent.height / 2
                                            hoverEnabled: true
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: mainWindow.tuneDialByStep(frequencyDigitCell.stepHz, -1)
                                        }
                                    }
                                }
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
                                color: mainWindow.txVisualActive ? bridge.themeManager.ledRed
                                       : (mainWindow.txPttPending ? bridge.themeManager.warningColor : textSecondary)
                            }
                            // TX frequency - click to edit
                            Rectangle {
                                width: txFreqText.width + 6; height: 14; radius: 2
                                color: txFreqMouseArea.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1) : "transparent"
                                Text {
                                    id: txFreqText; anchors.centerIn: parent
                                    text: bridge.txFrequency + " Hz"
                                    font.pixelSize: 10; font.family: decodiumMonoFontFamily
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
                                    font.pixelSize: 10; font.family: decodiumMonoFontFamily
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
                                running: bridge.coherentAvgEnabled && bridge.ledCoherentAveraging && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.5; duration: 400 }
                                OpacityAnimator { to: 1.0; duration: 400 }
                            }

                            ToolTip.visible: maCoherent.containsMouse
                            ToolTip.text: "Coherent averaging: " + (bridge.coherentAvgEnabled
                                ? (bridge.ledCoherentAveraging ? bridge.coherentCount + " signals" : "ON (waiting)")
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
                                running: bridge.neuralSyncEnabled && bridge.ledNeuralSync && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.5; duration: 300 }
                                OpacityAnimator { to: 1.0; duration: 300 }
                            }

                            ToolTip.visible: maNeural.containsMouse
                            ToolTip.text: "Neural sync: " + (bridge.neuralSyncEnabled
                                ? (bridge.ledNeuralSync ? (bridge.neuralScore * 100).toFixed(0) + "%" : "ON (waiting)")
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
                                running: bridge.turboFeedbackEnabled && bridge.ledTurboFeedback && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.5; duration: 350 }
                                OpacityAnimator { to: 1.0; duration: 350 }
                            }

                            ToolTip.visible: maTurbo.containsMouse
                            ToolTip.text: "Turbo feedback: " + (bridge.turboFeedbackEnabled
                                ? (bridge.ledTurboFeedback ? bridge.turboIterations + " iter" : "ON (waiting)")
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
                            font.family: decodiumMonoFontFamily
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
                                ToolTip.text: qsTr("Click: sync NTP now to align DT (FT8/FT4)")
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

                    // PTT command sent, waiting for positive rig feedback.
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.color: bridge.themeManager.warningColor
                        border.width: 2
                        visible: mainWindow.txPttPending
                        opacity: 0.85
                    }
                }

                // slot 3 — fra freq display e Sliders
                Item {
                    id: clockSlot3
                    height: 80
                    width: worldClock.parent === clockSlot3 ? worldClock.width : 0
                    visible: worldClock.parent === clockSlot3
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
	                                    Rectangle {
	                                        id: rxAutoLevelToggle
	                                        Layout.preferredWidth: 30
	                                        Layout.preferredHeight: 12
	                                        radius: 3
	                                        color: bridge && bridge.autoRxInputLevel
	                                               ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.22)
	                                               : Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.85)
	                                        border.color: bridge && bridge.autoRxInputLevel ? secondaryCyan : glassBorder
	                                        border.width: 1
	                                        Text {
	                                            anchors.centerIn: parent
	                                            text: "AUTO"
	                                            color: bridge && bridge.autoRxInputLevel ? secondaryCyan : textSecondary
	                                            font.pixelSize: 7
	                                            font.bold: true
	                                        }
	                                        MouseArea {
	                                            id: rxAutoLevelMouse
	                                            anchors.fill: parent
	                                            hoverEnabled: true
	                                            cursorShape: Qt.PointingHandCursor
	                                            onClicked: if (bridge) bridge.autoRxInputLevel = !bridge.autoRxInputLevel
	                                        }
	                                        ToolTip.visible: rxAutoLevelMouse.containsMouse
	                                        ToolTip.text: bridge && bridge.autoRxInputLevel
	                                                      ? qsTr("Auto RX level active")
	                                                      : qsTr("Auto RX level disabled")
	                                    }
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
                                    Text { text: Math.round(bridge.rxInputLevel); color: secondaryCyan; font.pixelSize: 8; font.family: decodiumMonoFontFamily; Layout.preferredWidth: 18 }
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
                                        font.family: decodiumMonoFontFamily
                                        Layout.preferredWidth: 28
                                    }
                                }
                            }
                        }

                        // Row 2: Monitor/Stop control
                        RowLayout {
                            spacing: 2

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 16
                                visible: mainWindow.uiBtnMonitorVisible
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
                                            running: bridge.monitoring && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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

                            // Gallager — toggle scavo profondo weak-signal (ft8SubpassHarvest)
                            Rectangle {
                                Layout.preferredWidth: 38
                                Layout.preferredHeight: 16
                                color: bridge.ft8SubpassHarvest ? Qt.rgba(0.55, 0.43, 0.92, 0.18) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                border.color: bridge.ft8SubpassHarvest ? "#8f74e8" : glassBorder
                                border.width: 1
                                radius: 2

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text {
                                        text: "◆"
                                        font.pixelSize: 8
                                        color: bridge.ft8SubpassHarvest ? "#bda9ff" : secondaryCyan
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: "GAL"
                                        font.pixelSize: 7
                                        font.bold: true
                                        color: bridge.ft8SubpassHarvest ? "#bda9ff" : textPrimary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    id: gallagerMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.ft8SubpassHarvest = !bridge.ft8SubpassHarvest
                                }

                                ToolTip.visible: gallagerMA.containsMouse
                                ToolTip.text: qsTr("Gallager — deep dig for weak signals.\nEnables a second decoding pass (LDPC subpass\nparallelized across cores, named after R. Gallager, father of LDPC)\nthat recovers near-noise stations missed by the normal decode.\nRequires a multi-core CPU: on old PCs it may burden the audio\n→ in that case leave it off.")
                            }

                            // fastldpc — decoder LDPC SIMD per FT2 (AVX2 o NEON)
                            Rectangle {
                                Layout.preferredWidth: 38
                                Layout.preferredHeight: 16
                                color: bridge.fastLdpcEnabled ? Qt.rgba(0.98, 0.68, 0.20, 0.18) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                border.color: bridge.fastLdpcEnabled ? "#f0ae33" : glassBorder
                                border.width: 1
                                radius: 2

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text {
                                        text: "⚡"
                                        font.pixelSize: 8
                                        color: bridge.fastLdpcEnabled ? "#ffd48a" : secondaryCyan
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: "LDPC"
                                        font.pixelSize: 7
                                        font.bold: true
                                        color: bridge.fastLdpcEnabled ? "#ffd48a" : textPrimary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    id: fastLdpcMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.fastLdpcEnabled = !bridge.fastLdpcEnabled
                                }

                                ToolTip.visible: fastLdpcMA.containsMouse
                                ToolTip.text: qsTr("Fast LDPC — vectorized FT2 decoder (NEON on Apple Silicon/ARM64, AVX2 on x86).\nDecodes the same words far quicker than the original decoder,\nfreeing CPU for the rest of the cycle. Falls back to the original\ndecoder automatically when the required SIMD backend is unavailable.\nTurn it off if you see decodes that look wrong.")
                            }
                        }
                    }
                } // End Sliders Item

                // slot 4 — prima del blocco pulsanti toolbar (= "before toolbar")
                Item {
                    id: clockSlot4
                    height: 80
                    width: worldClock.parent === clockSlot4 ? worldClock.width : 0
                    visible: worldClock.parent === clockSlot4
                }

                // Grouped buttons: Settings, REC, WAV, Log, Macro, Astro, CAT
                // (ex figlio del Row reorderableHeaderPair, ora figlio diretto del Flow:
                // l'orologio non è più appaiato qui ma vive negli host-slot — vedi clockSlots).
                Item {
                    id: headerUtilityButtons
                    readonly property int computedWidth: Math.ceil(mainWindow.headerToolbarPreferredWidth())
                    width: computedWidth
                    height: 74
                    visible: computedWidth > 0

                    Rectangle {
                        width: parent.width
                        height: 28
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                        border.color: glassBorder
                        radius: 4

                        // ── Toolbar riordinabile via drag&drop (vedi mainWindow.uiToolbarOrder) ──
                        // Ogni pulsante è un Component indicizzato per id; un Repeater itera
                        // il modello ORDINATO e un Loader per slot carica il Component giusto.
                        // Click breve = azione; long-press = drag magnetico (riordino in barra).
                        RowLayout {
                            id: headerUtilityRow
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 1

                            // Stato drag condiviso fra gli slot
                            property int dragIndex: -1        // indice modello in trascinamento (-1 = nessuno)
                            property int dropIndex: -1        // indice di inserimento proposto durante il drag

                            // Mappa id -> Component visuale
                            function componentForId(id) {
                                switch (id) {
                                    case "setup":  return comp_setup
                                    case "rec":    return comp_rec
                                    case "wav":    return comp_wav
                                    case "log":    return comp_log
                                    case "macro":  return comp_macro
                                    case "astro":  return comp_astro
                                    case "layout": return comp_layout
                                    case "history": return comp_history
                                    case "dxped": return comp_dxped
                                    case "cat":    return comp_cat
                                    case "sep1":
                                    case "sep2":   return comp_sep
                                    default:       return null
                                }
                            }
                            function isSeparator(id) { return id === "sep1" || id === "sep2" }

                            Repeater {
                                id: headerUtilityRepeater
                                model: mainWindow.uiToolbarOrder

                                // Ogni slot è un Item Layout-managed che ospita il Component del pulsante.
                                delegate: Item {
                                    id: slot
                                    property string buttonId: modelData
                                    property bool isSep: headerUtilityRow.isSeparator(buttonId)
                                    // Il pulsante interno espone btnVisible / prefWidth via il Loader.item
                                    property bool slotVisible: btnLoader.item ? btnLoader.item.btnVisible : true
                                    property bool dragging: headerUtilityRow.dragIndex === index

                                    visible: slotVisible
                                    Layout.preferredWidth: btnLoader.item ? btnLoader.item.prefWidth : 0
                                    Layout.fillHeight: true
                                    Layout.topMargin: isSep ? 4 : 0
                                    Layout.bottomMargin: isSep ? 4 : 0
                                    z: dragging ? 10 : 0

                                    Loader {
                                        id: btnLoader
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width
                                        height: parent.height
                                        sourceComponent: headerUtilityRow.componentForId(slot.buttonId)
                                        // L'opacità cala mentre lo slot è in drag (il ghost lo rappresenta)
                                        opacity: slot.dragging ? 0.25 : 1.0
                                        Behavior on opacity { NumberAnimation { duration: 120 } }
                                        // Spostamento magnetico: gli slot fra origine e destinazione del drag
                                        // scorrono per "aprire" il varco; animato (NO layer.enabled/FBO).
                                        x: {
                                            var d = headerUtilityRow.dragIndex
                                            var t = headerUtilityRow.dropIndex
                                            if (d < 0 || t < 0 || index === d)
                                                return 0
                                            var dragged = headerUtilityRepeater.itemAt(d)
                                            var shift = (dragged ? dragged.width : 0) + headerUtilityRow.spacing
                                            if (t > d && index > d && index <= t)
                                                return -shift   // si spostano a sinistra
                                            if (t < d && index >= t && index < d)
                                                return shift    // si spostano a destra
                                            return 0
                                        }
                                        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                                        onLoaded: {
                                            if (item) {
                                                item.hovered = Qt.binding(function() { return dragMA.containsMouse && !slot.isSep && headerUtilityRow.dragIndex < 0 })
                                            }
                                        }
                                    }

                                    // MouseArea unica per slot: hover + click + long-press->drag.
                                    // I separatori non sono interattivi.
                                    MouseArea {
                                        id: dragMA
                                        anchors.fill: parent
                                        enabled: !slot.isSep
                                        visible: !slot.isSep
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        preventStealing: true

                                        property bool armed: false      // long-press scattato -> in drag
                                        property real pressSceneX: 0
                                        property bool moved: false

                                        // Timer per armare il drag dopo ~350ms di pressione
                                        Timer {
                                            id: holdTimer
                                            interval: 350
                                            repeat: false
                                            onTriggered: {
                                                // Solo tasto sinistro avvia il drag
                                                if (dragMA.pressedButtons & Qt.LeftButton) {
                                                    dragMA.armed = true
                                                    headerUtilityRow.dragIndex = index
                                                    headerUtilityRow.dropIndex = index
                                                    dragGhost.startFor(slot, index)
                                                }
                                            }
                                        }

                                        onPressed: function(mouse) {
                                            armed = false
                                            moved = false
                                            pressSceneX = mapToItem(headerUtilityRow, mouse.x, mouse.y).x
                                            if (mouse.button === Qt.LeftButton)
                                                holdTimer.start()
                                        }

                                        onPositionChanged: function(mouse) {
                                            var sx = mapToItem(headerUtilityRow, mouse.x, mouse.y).x
                                            if (Math.abs(sx - pressSceneX) > 6)
                                                moved = true
                                            if (armed) {
                                                dragGhost.updateX(sx)
                                                // Feedback magnetico: marca lo slot target (gap) sotto al puntatore.
                                                // NON muta il modello durante il drag (eviterebbe la distruzione
                                                // del delegate e la perdita del mouse grab) -> commit al rilascio.
                                                headerUtilityRow.dropIndex = headerUtilityRow.computeTargetIndex(index, sx)
                                            } else if (moved) {
                                                // Movimento prima del long-press: annulla il click,
                                                // ma NON avvia drag (serve il long-press) -> niente azione.
                                                holdTimer.stop()
                                            }
                                        }

                                        onReleased: function(mouse) {
                                            holdTimer.stop()
                                            if (armed) {
                                                // Fine drag: snap allo slot più vicino e commit (una volta).
                                                var sx = mapToItem(headerUtilityRow, mouse.x, mouse.y).x
                                                var target = headerUtilityRow.computeTargetIndex(index, sx)
                                                dragGhost.stop()
                                                headerUtilityRow.dragIndex = -1
                                                headerUtilityRow.dropIndex = -1
                                                armed = false
                                                if (target !== index)
                                                    mainWindow.moveToolbarButton(index, target)
                                                return
                                            }
                                            // Click breve senza drag -> esegui azione del pulsante.
                                            if (!moved && btnLoader.item)
                                                btnLoader.item.activate(mouse)
                                        }

                                        onCanceled: {
                                            holdTimer.stop()
                                            if (armed) {
                                                dragGhost.stop()
                                                headerUtilityRow.dragIndex = -1
                                                headerUtilityRow.dropIndex = -1
                                                armed = false
                                            }
                                        }

                                        ToolTip.visible: containsMouse && !slot.isSep && headerUtilityRow.dragIndex < 0 && btnLoader.item
                                        ToolTip.text: btnLoader.item ? btnLoader.item.tip : ""
                                    }
                                }
                            }

                            // Calcola l'indice modello di destinazione in base alla X (scena) del puntatore.
                            // 'from' = indice dello slot trascinato; sceneX = X corrente nel RowLayout.
                            // Restituisce l'indice (post-spostamento) dove finirebbe il pulsante.
                            function computeTargetIndex(from, sceneX) {
                                var n = headerUtilityRepeater.count
                                var target = from
                                for (var i = 0; i < n; ++i) {
                                    if (i === from)
                                        continue
                                    var it = headerUtilityRepeater.itemAt(i)
                                    if (!it || !it.visible)
                                        continue
                                    var mid = it.x + it.width / 2
                                    if (i < from && sceneX < mid) { target = i; break }
                                    if (i > from) {
                                        if (sceneX > mid) target = i
                                    }
                                }
                                return target
                            }
                        }

                        // ── Ghost trascinato (proxy visuale che segue il puntatore) ──
                        // Parentato al Rectangle barra; assoluto SOLO durante il drag (non è nel layout).
                        Item {
                            id: dragGhost
                            visible: false
                            height: parent.height - 4
                            anchors.verticalCenter: parent.verticalCenter
                            z: 50
                            property Item sourceSlot: null

                            function startFor(s, idx) {
                                sourceSlot = s
                                width = s.width
                                ghostLoader.sourceComponent = headerUtilityRow.componentForId(s.buttonId)
                                visible = true
                            }
                            function updateX(sceneX) {
                                // Centra il ghost sotto il puntatore, clamp nella barra.
                                var nx = sceneX - width / 2
                                if (nx < 0) nx = 0
                                if (nx > parent.width - width) nx = parent.width - width
                                x = nx
                            }
                            function stop() {
                                visible = false
                                sourceSlot = null
                                ghostLoader.sourceComponent = null
                            }

                            Behavior on x { NumberAnimation { duration: 60; easing.type: Easing.OutQuad } }

                            Loader {
                                id: ghostLoader
                                anchors.fill: parent
                                opacity: 0.9
                                onLoaded: { if (item) item.hovered = true }
                            }
                        }
                    } // End Rectangle

                    // ════════ Component dei pulsanti toolbar ════════
                    // Ogni Component conserva l'aspetto e la logica originali del pulsante.
                    // Contratto comune: btnVisible (gate), prefWidth (larghezza Layout),
                    // hovered (impostato dallo slot/ghost), tip (testo ToolTip),
                    // activate(mouse) (azione eseguita al click breve).

                    // Settings ⚙
                    Component {
                        id: comp_setup
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnSetupVisible
                            readonly property real prefWidth: 50
                            readonly property string tip: "Settings"
                            function activate(mouse) { openSettingsDialog() }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "⚙"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: qsTr("Setup"); font.pixelSize: 9; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }

                    // REC
                    Component {
                        id: comp_rec
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnRecVisible
                            readonly property real prefWidth: 50
                            readonly property string tip: bridge.recordRxEnabled && bridge.wavManager ?
                                          "Recording: " + bridge.wavManager.recordedSeconds + "s" : "Start recording"
                            function activate(mouse) { bridge.recordRxEnabled = !bridge.recordRxEnabled }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) :
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
                        }
                    }

                    // WAV 📂 (sinistro: file; destro: cartella)
                    Component {
                        id: comp_wav
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnWavVisible
                            readonly property real prefWidth: 45
                            readonly property string tip: qsTr("Click: open a WAV file\nRight-click: decode a folder")
                            function activate(mouse) {
                                if (mouse && mouse.button === Qt.RightButton)
                                    mainWindow.chooseWavFolderForDecode()   // Right-click: batch folder
                                else
                                    mainWindow.chooseWavFileForDecode()     // Left-click: single file
                            }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "📂"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "WAV"; font.pixelSize: 9; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }

                    // Log 📋
                    Component {
                        id: comp_log
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnLogVisible
                            readonly property real prefWidth: 45
                            readonly property string tip: "Log QSO"
                            function activate(mouse) { openLogWindow() }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "📋"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "Log"; font.pixelSize: 9; color: secondaryCyan; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }

                    // Macro M
                    Component {
                        id: comp_macro
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnMacroVisible
                            readonly property real prefWidth: 50
                            readonly property string tip: bridge.macroManager && bridge.macroManager.contestMode ?
                                          "Contest: " + bridge.macroManager.contestName : "Macro TX"
                            function activate(mouse) { openMacroDialog() }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) :
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
                        }
                    }

                    // Astro 🌙
                    Component {
                        id: comp_astro
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnAstroVisible
                            readonly property real prefWidth: 48
                            readonly property string tip: "Astronomical data"
                            function activate(mouse) { openAstroWindow() }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "🌙"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "Astro"; font.pixelSize: 9; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }

                    // Layout reset
                    Component {
                        id: comp_layout
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnFooterResetVisible
                            readonly property real prefWidth: 64
                            readonly property string tip: qsTr("Reset layout (Ctrl+Shift+L)")
                            function activate(mouse) { resetLayoutConfirmDialog.open() }
                            radius: 3
                            color: hovered ? Qt.rgba(255/255, 155/255, 58/255, 0.20) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "☰"
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: Qt.rgba(255/255, 175/255, 88/255, 1.0)
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: qsTr("Layout")
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: Qt.rgba(255/255, 175/255, 88/255, 1.0)
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    // 1.0.569 — passa al workspace DX-Pedition. Si torna al
                    // classico con il pulsante EXIT della barra tattica.
                    Component {
                        id: comp_dxped
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnDxPedVisible
                            readonly property real prefWidth: 66
                            readonly property string tip: qsTr("Switch to the DX-Pedition workspace (3-column tactical layout)")
                            function activate(mouse) {
                                mainWindow.dxPeditionMode = true
                                bridge.setSetting("uiDxPeditionMode", true)
                            }
                            radius: 3
                            color: hovered ? Qt.rgba(25/255, 255/255, 136/255, 0.20) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "◤"
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: Qt.rgba(25/255, 255/255, 136/255, 1.0)
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: qsTr("DX-Ped")
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: Qt.rgba(25/255, 255/255, 136/255, 1.0)
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    // Decode history
                    Component {
                        id: comp_history
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnFooterHistoryVisible
                            readonly property real prefWidth: 68
                            readonly property string tip: qsTr("Decode history (Ctrl+Shift+H)")
                            function activate(mouse) {
                                mainWindow.openHistoryDialog()
                            }
                            radius: 3
                            color: hovered ? Qt.rgba(58/255, 157/255, 255/255, 0.20) : "transparent"
                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "▤"
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: Qt.rgba(88/255, 175/255, 255/255, 1.0)
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: qsTr("History")
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: Qt.rgba(88/255, 175/255, 255/255, 1.0)
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    // CAT
                    Component {
                        id: comp_cat
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: mainWindow.uiBtnCatVisible
                            readonly property real prefWidth: 48
                            readonly property string tip: bridge.catConnected ? "CAT: " + bridge.catRigName : "Click to configure CAT"
                            function activate(mouse) { openSettingsTab(1) }
                            radius: 3
                            color: hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.15) :
                                   (bridge.catConnected ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : "transparent")
                            border.color: bridge.catConnected ? accentGreen : "transparent"
                            border.width: bridge.catConnected ? 1 : 0
                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Rectangle {
                                    width: 8; height: 8; radius: 4
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
                        }
                    }

                    // Separatore (non interattivo, sempre visibile, larghezza 1px)
                    Component {
                        id: comp_sep
                        Rectangle {
                            property bool hovered: false
                            readonly property bool btnVisible: true
                            readonly property real prefWidth: 1
                            readonly property string tip: ""
                            function activate(mouse) {}
                            color: glassBorder
                        }
                    }
                } // End Grouped buttons Item

                // 1.0.384 — Profili pronti: selettore rapido in toolbar (accanto a Setup).
                // Applica in blocco i toggle FT2/decode; la sezione descrittiva è in Settings.
                Item {
                    id: readyProfileToolbarSlot
                    width: 132
                    height: 74
                    Rectangle {
                        width: parent.width
                        height: 28
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                        border.color: glassBorder
                        radius: 4
                        DecoComboBox {
                            id: readyProfileCombo
                            anchors.fill: parent
                            anchors.margins: 2
                            font.pixelSize: 10
                            readonly property var ids: ["balanced", "weak", "contest", "cpu"]
                            model: [qsTr("Balanced"), qsTr("Weak-signal / DX"), qsTr("Contest"), qsTr("CPU-limited")]
                            displayText: currentIndex < 0 ? qsTr("Profiles...") : model[currentIndex]
                            Component.onCompleted: currentIndex = bridge ? ids.indexOf(bridge.activeReadyProfile) : -1
                            onActivated: if (bridge && currentIndex >= 0) bridge.applyReadyProfile(ids[currentIndex])
                            ToolTip.visible: hovered
                            ToolTip.delay: 500
                            ToolTip.text: qsTr("Ready profiles - apply FT2/decode toggles as a group. Details in Setup -> TX.")
                            Connections {
                                target: bridge
                                function onActiveReadyProfileChanged() {
                                    readyProfileCombo.currentIndex = readyProfileCombo.ids.indexOf(bridge.activeReadyProfile)
                                }
                            }
                        }
                    }
                }

                // slot 5 — subito dopo il blocco pulsanti toolbar (= "after toolbar", DEFAULT)
                Item {
                    id: clockSlot5
                    height: 80
                    width: worldClock.parent === clockSlot5 ? worldClock.width : 0
                    visible: worldClock.parent === clockSlot5
                }

	                // World Clock with Analog Display
	                Item {
	                    id: worldClock
	                    // 1.0.569 — in DX-Pedition il workspace occupa tutto lo
	                    // schermo e l'orologio draggabile ci finiva sopra: qui la
	                    // barra tattica ha gia' il suo orologio UTC.
	                    visible: showWorldClock && !mainWindow.dxPeditionMode
	                    width: (showWorldClock && !mainWindow.dxPeditionMode) ? compactWidth : 0
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
	                    property bool showWorldClock: mainWindow.settingBool("uiWorldClockVisible", true)
	                    property bool showAnalogClock: mainWindow.settingBool("uiWorldClockShowAnalog", true)
	                    property bool showDigitalClock: mainWindow.settingBool("uiWorldClockShowDigital", true)
	                    property bool showWorldClockCities: mainWindow.settingBool("uiWorldClockShowCities", true)

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

	                    Connections {
	                        target: mainWindow
	                        function onWidthChanged() { mainWindow.clampWorldClockNow() }
	                        function onHeightChanged() { mainWindow.clampWorldClockNow() }
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
	                        mainWindow.persistUiSetting("uiWorldClockShowAnalog", showAnalogClock)
	                        mainWindow.persistUiSetting("uiWorldClockShowDigital", showDigitalClock)
	                        mainWindow.persistUiSetting("uiWorldClockShowCities", showWorldClockCities)
	                    }

	                    function setClockVisible(visible) {
	                        showWorldClock = visible
	                        if (showWorldClock) {
	                            ensureVisiblePart()
	                            updateTime()
	                        }
	                        mainWindow.persistUiSetting("uiWorldClockVisible", showWorldClock)
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
                        mainWindow.persistUiSetting("uiWorldClockZoneId", selectedZoneId)
                        mainWindow.persistUiSetting("uiWorldClockCityName", selectedCityName)
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

                        // ── Maniglia di trascinamento del World Clock ──
                        // SOLO questa presa avvia il drag: il resto del clock conserva
                        // intatti il selettore città (timezoneSelectorMA), il right-click
                        // (worldClockMenu) e ogni altra interazione. Long-press 350ms +
                        // soglia 6px (come STEP 1); al rilascio swap before/after toolbar
                        // se il puntatore supera la metà del blocco pulsanti.
                        Rectangle {
                            id: worldClockDragHandle
                            z: 30
                            width: 14
                            height: 14
                            radius: 3
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 3
                            anchors.rightMargin: 3
                            color: worldClockHandleMA.containsMouse || worldClockHandleMA.armed
                                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                                   : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.12)
                            border.color: worldClockHandleMA.containsMouse ? secondaryCyan : "transparent"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "⠿"               // ⠿ braille pattern (presa)
                                font.pixelSize: 11
                                color: worldClockHandleMA.containsMouse ? secondaryCyan
                                                                        : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
                            }

                            MouseArea {
                                id: worldClockHandleMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.SizeAllCursor
                                acceptedButtons: Qt.LeftButton
                                preventStealing: true

                                property bool armed: false
                                property real pressSceneX: 0
                                property bool moved: false
                                property real grabDX: 0
                                property real grabDY: 0

                                Timer {
                                    id: worldClockHoldTimer
                                    interval: 350
                                    repeat: false
                                    onTriggered: {
                                        if (worldClockHandleMA.pressedButtons & Qt.LeftButton) {
                                            worldClockHandleMA.armed = true
                                            worldClockDragGhost.startAt(worldClockHandleMA.pressSceneX)
                                        }
                                    }
                                }

                                function sceneX(mouse) {
                                    return mapToItem(headerFlow, mouse.x, mouse.y).x
                                }

                                onPressed: function(mouse) {
                                    armed = true
                                    moved = false
                                    var pp = mapToItem(worldClock.parent, mouse.x, mouse.y)
                                    grabDX = pp.x - worldClock.x
                                    grabDY = pp.y - worldClock.y
                                }
                                onPositionChanged: function(mouse) {
                                    if (!armed)
                                        return
                                    moved = true
                                    var pp = mapToItem(worldClock.parent, mouse.x, mouse.y)
                                    worldClock.x = mainWindow.clampClockX(pp.x - grabDX)
                                    worldClock.y = mainWindow.clampClockY(pp.y - grabDY)
                                }
                                onReleased: function(mouse) {
                                    if (armed) {
                                        armed = false
                                        if (moved)
                                            mainWindow.persistWorldClockPos()
                                    }
                                }
                                onCanceled: {
                                    armed = false
                                }
                                ToolTip.visible: containsMouse && !armed
                                ToolTip.text: qsTr("Drag to reposition the clock")
                                ToolTip.delay: 500
                            }
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
	                                font.family: decodiumMonoFontFamily
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
	                                font.family: decodiumMonoFontFamily
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
		                                    DecoTextField {
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
	                } // End World Clock Item

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
                        running: waterfallMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                        running: logWindowMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                    ToolTip.text: qsTr("Restore astronomical data")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: astroWindowMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                            }
                        }
                    }

                    ToolTip.visible: liveMapRestoreMA.containsMouse
                    ToolTip.text: qsTr("Restore Live Map")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: liveMapMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                                if (mainWindow.dxClusterPanelVisible)
                                    Qt.callLater(function() { mainWindow.raiseDxClusterPanel() })
                            } else {
                                // Sinistro: mostra pannello e connetti se non connesso
                                mainWindow.openDxClusterPanel()
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
                                text: qsTr("Auto Spot")
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
                    // 1.0.388 — bypass attivo = ROSSO (era arancione) con lampeggio fade
                    readonly property color bypassRed: (bridge.themeManager ? bridge.themeManager.ledRed : "#e53935")
                    color: bridge.filtersBypassed
                           ? Qt.rgba(bypassRed.r, bypassRed.g, bypassRed.b, 0.26)
                           : bypassFiltersMA.containsMouse
                             ? Qt.rgba(bypassRed.r, bypassRed.g, bypassRed.b, 0.14)
                             : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.85)
                    border.color: bridge.filtersBypassed ? bypassRed
                                  : bypassFiltersMA.containsMouse ? bypassRed : glassBorder
                    border.width: bridge.filtersBypassed || bypassFiltersMA.containsMouse ? 2 : 1

                    // Overlay rosso che pulsa in fade-in/fade-out quando il bypass è attivo.
                    // 1.0.388: usa OpacityAnimator (pattern affidabile come il pulse del MON)
                    // su un FILL rosso, così il lampeggio è ben visibile.
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Qt.rgba(parent.bypassRed.r, parent.bypassRed.g, parent.bypassRed.b, 0.45)
                        border.color: parent.bypassRed
                        border.width: 2
                        visible: bridge.filtersBypassed
                        opacity: 1.0
                        SequentialAnimation on opacity {
                            running: bridge.filtersBypassed && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                            loops: Animation.Infinite
                            OpacityAnimator { to: 0.15; duration: 650 }
                            OpacityAnimator { to: 1.0; duration: 650 }
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "⌀"
                            font.pixelSize: 24
                            font.bold: true
                            color: bridge.filtersBypassed ? parent.parent.bypassRed : textSecondary
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Bypass")
                            font.pixelSize: 9
                            font.bold: true
                            color: bridge.filtersBypassed ? parent.parent.bypassRed : textSecondary
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Filters")
                            font.pixelSize: 9
                            font.bold: true
                            color: bridge.filtersBypassed ? parent.parent.bypassRed : textSecondary
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
                                  : qsTr("Bypass CQ/My Call and setup filters")
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
                    ToolTip.text: qsTr("Restore Macro configuration")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: macroDialogMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                        running: rigControlMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                        running: period1Minimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                        running: period2Minimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                        running: rxFreqMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                    ToolTip.text: qsTr("Restore TX panel")
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: txPanelMinimized && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                            text: qsTr("PSK Reporter")
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

                                DecoTextField {
                                    id: pskSearchInput
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    placeholderText: (activeFocus || text.length > 0) ? "" : "Callsign..."
                                    font.pixelSize: 11
                                    font.capitalization: Font.AllUppercase
                                    font.family: decodiumMonoFontFamily
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
                            // 3 × 20 px + 2 × 2 px fits the 66 px inner width
                            // of this 74 px header tile.  The previous 30 px
                            // controls overflowed into the adjacent Reset area.
                            spacing: 2

                            Rectangle {
                                width: 20; height: 20; radius: 3
                                color: fontMinusMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                                border.color: fontMinusMA.containsMouse ? secondaryCyan : glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "A-"
                                    font.pixelSize: 10
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
                                width: 20; height: 20; radius: 3
                                color: fontPlusMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                                border.color: fontPlusMA.containsMouse ? secondaryCyan : glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "A+"
                                    font.pixelSize: 10
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

                            // Chiaro/scuro, stessa forma e misura dei due sopra.
                            // Mostra il simbolo di DOVE si va, non di dove si e':
                            // premendo la luna si passa allo scuro.
                            Rectangle {
                                id: themeToggle
                                width: 20; height: 20; radius: 3
                                property bool lightNow: !!(bridge.themeManager && bridge.themeManager.isLightTheme)
                                // Ricorda quale scuro si stava usando, cosi' il
                                // ritorno non butta via Darkcodium per Ocean Blue.
                                property string lastDarkTheme: "Ocean Blue"
                                color: themeToggleMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: themeToggleMA.containsMouse ? secondaryCyan : glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: themeToggle.lightNow ? "☾" : "☀"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: textPrimary
                                }

                                ToolTip.visible: themeToggleMA.containsMouse
                                ToolTip.delay: 600
                                ToolTip.text: themeToggle.lightNow ? qsTr("Switch to the dark theme")
                                                                   : qsTr("Switch to the light theme")

                                MouseArea {
                                    id: themeToggleMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var tm = bridge.themeManager
                                        if (!tm)
                                            return
                                        if (tm.isLightTheme) {
                                            tm.currentTheme = themeToggle.lastDarkTheme
                                        } else {
                                            themeToggle.lastDarkTheme = tm.currentTheme
                                            tm.currentTheme = "Stellar Light"
                                        }
                                    }
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
                                text: qsTr("Reset")
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

            // ── Ghost del World Clock durante il drag dalla maniglia ──
            // Proxy visuale parentato a headerBar (NON nel Flow, così non viene
            // posizionato dal layout); segue il puntatore con Behavior on x.
            // NESSUN layer.enabled/FBO. Le coordinate sono in spazio headerFlow,
            // riportate a headerBar aggiungendo l'offset headerFlow.x.
            Rectangle {
                id: worldClockDragGhost
                visible: false
                z: 200
                width: 90
                height: 30
                radius: 6
                y: headerFlow.y + 6
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.92)
                border.color: secondaryCyan
                border.width: 1
                opacity: 0.9

                function startAt(sceneX) {
                    visible = true
                    updateX(sceneX)
                }
                function updateX(sceneX) {
                    var nx = headerFlow.x + sceneX - width / 2
                    var minX = headerFlow.x
                    var maxX = headerFlow.x + headerFlow.width - width
                    if (nx < minX) nx = minX
                    if (nx > maxX) nx = maxX
                    x = nx
                }
                function stop() {
                    visible = false
                }

                Behavior on x { NumberAnimation { duration: 60; easing.type: Easing.OutQuad } }

                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        text: "⠿"
                        font.pixelSize: 12
                        color: secondaryCyan
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "🕐 " + qsTr("Clock")
                        font.pixelSize: 11
                        color: textPrimary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        } // End Header Bar Rectangle

        // Content area for dockable panels (wrapped in Flickable for vertical scroll
        // when the window is shorter than the minimum usable layout height)
        Flickable {
            id: contentScroll
            visible: !mainWindow.dxPeditionMode
            Layout.fillWidth: true
            Layout.fillHeight: visible
            Layout.preferredHeight: visible ? -1 : 0
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
                property var snapPoints: [120, 200, 280, 360, 440]  // 1.0.288: >= minimumHeight 120, range ampio per restringere/allargare
                property int snapThreshold: 0  // 1.0.288: snap disabilitato — resize altezza waterfall totalmente libero (richiesta utente)

                // Vertical drag handle with magnetic snap indicator
                handle: Rectangle {
                    id: splitHandle
                    implicitWidth: 10
                    implicitHeight: 10  // 1.0.288: era 6 → handle più alto = più facile da afferrare
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
                    // 1.0.288 — preferredHeight NON è più bindato reattivamente a
                    // waterfallPanelHeight: quel binding combatteva il drag dello splitter
                    // (loop preferredHeight←waterfallPanelHeight←uiWaterfallHeight←onHeightChanged)
                    // e impediva di restringere il waterfall ("superiore bloccata"). Ora è
                    // gestito imperativamente: init one-shot in Component.onCompleted, drag/snap
                    // liberi; un Binding dedicato forza 40px solo quando è staccato (placeholder).
                    // Stadio 3: waterfallPanel è il TOP SLOT-HOST (slot 4). Quando ospita
                    // davvero la Waterfall (hostsWaterfall) usa la logica visibilità/altezza
                    // waterfall-specifica; quando invece ospita un ALTRO pannello (Waterfall
                    // spostata in una colonna) lo slot resta visibile e senza vincoli di altezza
                    // waterfall, lasciando che il pannello ospite gestisca la propria visibilità.
                    readonly property bool hostsWaterfall: mainWindow.classicIdInSlot(5) === "waterfall"
                    visible: hostsWaterfall ? (mainWindow.waterfallPanelVisible || waterfallDetached) : true
                    SplitView.minimumHeight: !hostsWaterfall ? 0 : (!mainWindow.waterfallPanelVisible ? 0 : (waterfallDetached ? 40 : 0))  // 1.0.288: nessun vincolo di altezza quando ancorato (resize completamente libero, richiesta utente). Era 260 → 120 → 0.
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

                    Component.onCompleted: {
                        updateDockZone()
                        // 1.0.288 — init one-shot (no binding reattivo che combatte il drag)
                        if (mainWindow.waterfallPanelVisible && !waterfallDetached)
                            SplitView.preferredHeight = mainWindow.waterfallPanelHeight
                    }
                    onWidthChanged: updateDockZone()
                    onHeightChanged: {
                        updateDockZone()
                        // 1.0.288 — persisti SOLO su bridge.uiWaterfallHeight (per il save).
                        // NON riscrivere mainWindow.waterfallPanelHeight: romperebbe il binding
                        // di riga 310 e rialimenterebbe il loop che bloccava il resize.
                        if (mainWindow.waterfallPanelVisible && !waterfallDetached && height > 40) {
                            var roundedHeight = Math.round(height)
                            if (Math.abs(bridge.uiWaterfallHeight - roundedHeight) >= 1) {
                                bridge.uiWaterfallHeight = roundedHeight
                                mainWindow.scheduleSave()
                            }
                        }
                    }

                    // 1.0.288 — quando il waterfall è staccato, il pannello collassa al
                    // placeholder 40px; tornato embedded ripristina l'altezza precedente.
                    // Stadio 3: questi override di ALTEZZA sono specifici della Waterfall, ma
                    // waterfallPanel è ora un SLOT-HOST che può ospitare un ALTRO pannello (se
                    // la Waterfall è stata spostata in una colonna). Quindi si applicano SOLO
                    // quando il topSlot (slot 4) ospita davvero la Waterfall: altrimenti il
                    // topSlot conserva la sua preferredHeight normale per il pannello ospite.
                    Binding {
                        target: waterfallPanel
                        property: "SplitView.preferredHeight"
                        value: 40
                        when: mainWindow.waterfallPanelVisible && waterfallDetached
                              && mainWindow.classicIdInSlot(5) === "waterfall"
                        restoreMode: Binding.RestoreBindingOrValue
                    }

                    Binding {
                        target: waterfallPanel
                        property: "SplitView.preferredHeight"
                        value: 0
                        when: !mainWindow.waterfallPanelVisible
                              && mainWindow.classicIdInSlot(5) === "waterfall"
                        restoreMode: Binding.RestoreBindingOrValue
                    }

                    // Placeholder when detached - magnetic dock zone.
                    // Stadio 3: figlio diretto dello SLOT-HOST (waterfallPanel), NON del
                    // pannello re-parentabile -> resta nel topSlot e serve da zona di dock.
                    // Mostrato solo quando il topSlot ospita davvero la Waterfall (id "waterfall"
                    // in slot 4): se la Waterfall è stata spostata in una colonna e un altro
                    // pannello occupa il topSlot, il placeholder non deve coprirlo. Il dock-back
                    // resta possibile via la finestra flottante (zona = waterfallPanel) comunque.
                    Rectangle {
                        anchors.fill: parent
                        visible: waterfallDetached && mainWindow.classicIdInSlot(5) === "waterfall"
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
	                            text: waterfallPanel.isDockHighlighted ? qsTr("Dock Waterfall") : qsTr("Waterfall detached")
                                color: waterfallPanel.isDockHighlighted ? secondaryCyan : textSecondary
                                font.pixelSize: waterfallPanel.isDockHighlighted ? 16 : 12
                                font.bold: waterfallPanel.isDockHighlighted
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
	                                text: qsTr("Use Dock to re-attach it")
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
                                running: waterfallPanel.isDockHighlighted && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                                loops: Animation.Infinite
                                OpacityAnimator { to: 0.4; duration: 300 }
                                OpacityAnimator { to: 1.0; duration: 300 }
                            }
                        }
                    }

                    // ════════ PANNELLO RE-PARENTABILE "waterfall" (Stadio 3) ════════
                    // Questo Item è il contenuto re-parentabile (panelId "waterfall"): contiene
                    // il Loader+Waterfall embedded + gli overlay (label, maniglia ⠿, Pop).
                    // Di DEFAULT è figlio del topSlot (waterfallPanel) e lo riempie via
                    // anchors.fill: parent. Lo SWAP (applyClassicColumnOrder/swapClassicColumns)
                    // RE-PARENTA QUESTO Item in un colSlot/txSlot (o lo riporta nel topSlot)
                    // -> anchors.fill assume la geometria del nuovo host (colonna alta-stretta,
                    // area TX larga-bassa, o top largo-basso). CRITICO PCM: il re-parent NON
                    // tocca waterfallEmbeddedLoader.active/sourceComponent -> il Loader resta
                    // attivo, l'istanza Waterfall e il suo PanadapterItem NON vengono
                    // distrutti/ricreati, il feed PCM via bridge prosegue (no freeze).
                    Rectangle {
                        id: waterfallPanelHost
                        anchors.fill: parent
                        visible: mainWindow.waterfallPanelVisible && !waterfallDetached
                        color: "transparent"


                        Loader {
                            id: waterfallEmbeddedLoader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            // Reserve a header row for the drag handle, title and Pop.
                            // The controls inside Waterfall must not sit under them.
                            anchors.topMargin: 30
                            visible: mainWindow.waterfallPanelVisible && !waterfallDetached
                                     && mainWindow.startupWaterfallVisualReady
                            // active NON dipende dal parent: il re-parent dello swap NON lo
                            // cambia -> Loader mai ricaricato -> PanadapterItem/feed PCM vivi.
                            active: mainWindow.waterfallPanelVisible && !waterfallDetached
                                    && mainWindow.startupWaterfallVisualReady
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
                            visible: mainWindow.waterfallPanelVisible && !waterfallDetached
                                     && mainWindow.startupWaterfallVisualReady
                                showControls: true
                                minFreq: 200
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

                        // Maniglia di drag pannello (Stadio 3) — overlay sull'angolo
                        // alto-SINISTRO (il pulsante Pop è in alto a destra), sopra il
                        // Waterfall. Stesso colDragHandleComponent degli altri pannelli;
                        // panelId "waterfall". Viaggia con waterfallPanelHost quando
                        // re-parentato in un colSlot/txSlot. active: !waterfallDetached
                        // (mentre è staccato il pannello mostra il placeholder, niente drag).
                        Loader {
                            z: 50
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: 6
                            anchors.topMargin: 5
                            width: 16
                            height: 16
                            active: !waterfallDetached
                            sourceComponent: colDragHandleComponent
                            onLoaded: if (item) item.panelId = "waterfall"
                        }

                        // Etichetta "Waterfall" integrata come overlay (non occupa spazio)
                        Text {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.topMargin: 6
                            anchors.leftMargin: 28
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
                                ToolTip.text: qsTr("Detach Waterfall")
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
                    property string normalizedMode: bridge ? String(bridge.mode || "").toUpperCase() : ""
                    property bool ft2LinkPeriodMode: normalizedMode === "FT2-LINK" || normalizedMode === "FT2LINK"
                    property bool ft2CadenceMode: normalizedMode === "FT2"
                    function configuredPeriodSeconds() {
                        var ms = bridge ? Number(bridge.periodMilliseconds || 0) : 0
                        if (ms > 0)
                            return ms / 1000.0
                        if (ft2CadenceMode)
                            return 3.75
                        if (ft2LinkPeriodMode)
                            return 15
                        if (normalizedMode === "FT4")
                            return 7.5
                        if (normalizedMode === "WSPR")
                            return 120
                        if (normalizedMode.indexOf("Q65") === 0)
                            return 60
                        return 15
                    }
                    function txDurationSecondsForMode() {
                        var period = configuredPeriodSeconds()
                        if (ft2CadenceMode)
                            return 2.87
                        if (ft2LinkPeriodMode)
                            return 10.0
                        if (normalizedMode === "FT4")
                            return 5.04
                        if (normalizedMode === "WSPR")
                            return 110.6
                        if (normalizedMode === "MSK144")
                            return Math.max(0.5, period - 0.5)
                        if (normalizedMode.indexOf("Q65") === 0) {
                            if (period <= 15.5)
                                return 0.5 + (85 * 1800 / 12000.0)
                            if (period <= 30.5)
                                return 0.5 + (85 * 3600 / 12000.0)
                            if (period <= 60.5)
                                return 1.0 + (85 * 7200 / 12000.0)
                            if (period <= 120.5)
                                return 1.0 + (85 * 16000 / 12000.0)
                            if (period <= 300.5)
                                return 1.0 + (85 * 41472 / 12000.0)
                            return Math.min(period, 52.0)
                        }
                        return 12.64
                    }
                    property real periodLength: configuredPeriodSeconds()

                    // IU8LMC: Reactive property for all decodes (Band Activity)
                    property bool showTxMessagesInRx: mainWindow.showTxMessagesInRx
                    property bool hideTelemetryOnlyDecodes: Qt.platform.os === "windows"
                    property var allDecodes: (bridge && bridge.bandActivityModel) ? [] : visibleDecodeEntries(bridge.decodeList)
                    property var rxDecodes: (bridge && bridge.rxDecodeModel) ? [] : currentRxDecodes()
                    property var clearedRxDecodeKeys: ({})
                    property int decodeListVersion: 0
                    // Il modello applica le righe a tranche, ma QML aggiorna contatori
                    // e tail-follow una sola volta quando la snapshot e' completa.
                    property int bandActivityCountVersion: 0
                    property bool bandActivitySnapshotPending: false
                    property bool rxSnapshotPending: false
                    function queueDecodeSnapshotUiCommit(bandPending, rxPending) {
                        bandActivitySnapshotPending = bandActivitySnapshotPending || bandPending
                        rxSnapshotPending = rxSnapshotPending || rxPending
                        decodeSnapshotUiCommitTimer.restart()
                    }
                    Timer {
                        id: decodeSnapshotUiCommitTimer
                        interval: 60
                        repeat: false
                        onTriggered: {
                            if (decodePanel.bandActivitySnapshotPending) {
                                decodePanel.bandActivitySnapshotPending = false
                                decodePanel.bandActivityCountVersion++
                                decodePanel.decodeListVersion++
                                decodePanel.updateCurrentPeriodDecodeCountFromCount(
                                            bridge.bandActivityModel.count())
                            }
                            if (decodePanel.rxSnapshotPending) {
                                decodePanel.rxSnapshotPending = false
                                decodePanel.rxDecodeListVersion++
                            }
                        }
                    }
                    Connections {
                        target: (bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : null
                        ignoreUnknownSignals: true
                        function onSnapshotApplied() {
                            decodePanel.queueDecodeSnapshotUiCommit(true, false)
                        }
                    }
                    Connections {
                        target: (bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : null
                        ignoreUnknownSignals: true
                        function onSnapshotApplied() {
                            decodePanel.queueDecodeSnapshotUiCommit(false, true)
                        }
                    }
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
                        void(decodePanel.bandActivityCountVersion)
                        if (decodePanel.hasNativeBandActivityModel())
                            return bridge.bandActivityModel.count()
                        return decodePanel.allDecodes ? decodePanel.allDecodes.length : 0
                    }

                    function signalRxModelCount() {
                        void(decodePanel.rxDecodeListVersion)
                        if (decodePanel.hasNativeRxDecodeModel())
                            return bridge.rxDecodeModel.count()
                        return decodePanel.rxDecodes ? decodePanel.rxDecodes.length : 0
                    }

                    function hasNativeBandActivityModel() {
                        return bridge && bridge.bandActivityModel
                    }

                    function hasNativeRxDecodeModel() {
                        return bridge && bridge.rxDecodeModel
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

                    function updateCurrentPeriodDecodeCountFromCount(newCount) {
                        if (newCount >= decodePanel.lastSyncCount) {
                            decodePanel.currentPeriodDecodeCount += newCount - decodePanel.lastSyncCount
                        } else {
                            decodePanel.currentPeriodDecodeCount = 0
                            decodePanel.heldPeriodDecodeCount = 0
                            decodePanel.heldPeriodDecodeCountUntilIndex = -1
                        }
                        decodePanel.lastSyncCount = newCount
                    }

                    function updateCurrentPeriodDecodeCount(src) {
                        updateCurrentPeriodDecodeCountFromCount(src ? src.length : 0)
                    }

	                    Component.onCompleted: {
	                        updatePeriodState()
	                        lastSyncCount = decodePanel.hasNativeBandActivityModel()
	                            ? bridge.bandActivityModel.count()
	                            : decodePanel.visibleDecodeEntries(bridge.decodeList).length
	                    }

	                    function refreshRxDecodeModel(resetCleared) {
	                        if (resetCleared)
	                            decodePanel.clearedRxDecodeKeys = ({})
	                        if (decodePanel.hasNativeRxDecodeModel())
	                            return
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
	                        if (decodePanel.hasNativeRxDecodeModel()) {
	                            decodePanel.clearedRxDecodeKeys = ({})
	                        } else {
	                            var hidden = {}
	                            for (var i = 0; i < decodePanel.rxDecodes.length; ++i) {
	                                var item = decodePanel.rxDecodes[i]
	                                if (!item || item.isSeparator === true)
	                                    continue
	                                hidden[decodePanel.rxEntryKey(item)] = true
	                            }
	                            decodePanel.clearedRxDecodeKeys = hidden
	                            decodePanel.rxDecodes = []
	                        }
	                        bridge.clearRxDecodes()
	                        if (!decodePanel.hasNativeRxDecodeModel()) {
	                            decodePanel.rxDecodeListVersion++
	                            if (rxFrequencyList)
	                                rxFrequencyList.forceTailFollow()
	                            if (rxFrequencyFloatingList)
	                                rxFrequencyFloatingList.forceTailFollow()
	                        }
	                    }

	                    // Update decode list incrementalmente (solo nuovi elementi)
	                    Connections {
                        target: bridge
                        function onDecodeListChanged() {
	                            if (!decodePanel.hasNativeBandActivityModel()) {
	                                decodePanel.decodeListVersion++
	                                var src = decodePanel.visibleDecodeEntries(bridge.decodeList)
	                                decodePanel.updateCurrentPeriodDecodeCount(src)
	                                decodePanel.allDecodes = src
	                                if (period1Detached) {
	                                    if (period1FloatingList)
	                                        period1FloatingList.forceTailFollow()
	                                } else if (evenPeriodList) {
	                                    evenPeriodList.forceTailFollow()
	                                }
	                            }
	                            if (!decodePanel.hasNativeRxDecodeModel()) {
	                                decodePanel.rxDecodes = decodePanel.currentRxDecodes()
	                                decodePanel.rxDecodeListVersion++
	                                if (rxFrequencyList)
	                                    rxFrequencyList.forceTailFollow()
	                                if (rxFrequencyFloatingList)
	                                    rxFrequencyFloatingList.forceTailFollow()
	                            }
                        }
                        function onRxDecodeListChanged() {
	                            if (!decodePanel.hasNativeRxDecodeModel()) {
	                                decodePanel.rxDecodes = decodePanel.currentRxDecodes()
	                                decodePanel.rxDecodeListVersion++
	                                if (rxFrequencyList)
	                                    rxFrequencyList.forceTailFollow()
	                                if (rxFrequencyFloatingList)
	                                    rxFrequencyFloatingList.forceTailFollow()
	                            }
	                        }
	                        function onDxCallChanged() {
	                            decodePanel.refreshRxDecodeModel(true)
	                        }
	                        function onRxFrequencyChanged() {
	                            decodePanel.refreshRxDecodeModel(true)
	                        }
	                    }

                    onShowTxMessagesInRxChanged: {
	                        if (decodePanel.hasNativeRxDecodeModel())
	                            return
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
                        running: !mainWindow.ft2LinkModeActive
                        repeat: true
                        onTriggered: {
                            decodePanel.updatePeriodState()
                        }
                    }

                    // LED Status Timer - adapts to mode period
                    Timer {
                        id: ledStatusTimer
                        interval: 500  // Update LED status every 500ms
                        running: !mainWindow.ft2LinkModeActive
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
		                        return mainWindow.decodeEntryBoldForModel(md)
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
	                        return activeMatch
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
                        border.color: !timingBar.timerActive ? glassBorder : (timingBar.isTxPhase ? bridge.themeManager.ledRed : Qt.rgba(76/255, 175/255, 80/255, 0.55))
                        border.width: 1

                        property real periodLen: decodePanel.periodLength
                        property real txDuration: decodePanel.txDurationSecondsForMode()
                        property real progress: 0.0
                        property real secInPeriod: 0.0
                        property bool isTxPhase: !!(bridge && bridge.transmitting)
                        property bool timerActive: !!(bridge && ((bridge.transmitting || bridge.tuning) || (!mainWindow.ft2LinkModeActive && bridge.monitoring)))
                        property bool isEvenPeriod: true
                        property string periodLabel: isTxPhase ? "TX" : "RX"
                        onTimerActiveChanged: {
                            if (!timerActive) {
                                progress = 0.0
                                secInPeriod = 0.0
                            }
                        }

                        Timer {
                            interval: 50
                            running: timingBar.timerActive
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
                                visible: timingBar.timerActive
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
                            font.family: decodiumMonoFontFamily
                            color: !timingBar.timerActive ? textSecondary : (timingBar.isTxPhase ? bridge.themeManager.ledRed : bridge.themeManager.successColor)
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
                            font.family: decodiumMonoFontFamily
                            color: timingBar.isTxPhase ? bridge.themeManager.ledRed : accentGreen
                        }

                        // Time counter (right)
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: timingBar.secInPeriod.toFixed(1) + " / " + timingBar.periodLen.toFixed(1) + "s"
                            font.pixelSize: 10
                            font.family: decodiumMonoFontFamily
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
                                if (SplitHandle.pressed && typeof colSlot0 !== "undefined")
                                    colSlot0.userDraggedSplit = true
                            }
                        }

                        // ════════ SLOT-HOST FISSI (Stadio 1 pannelli interscambiabili) ════════
                        // ft2LinkModeSlot è il pannello applicativo inline, visibile solo nel
                        // modo FT2-Link. Gli slot classici restano a ordine fisso
                        // (colSlot0=sx, colSlot1=centro, colSlot2=dx, colSlot3=extra): portano loro le
                        // attached property SplitView.* + la larghezza-valore (per-slot). Ogni
                        // colSlot DICHIARA al suo interno il proprio pannello di DEFAULT come
                        // figlio naturale (anchors.fill: parent): colSlot0->period1Panel,
                        // colSlot1->rxFreqPanel, colSlot2->liveMapPanelHost. Lo SWAP re-parenta
                        // imperativamente i pannelli tra i colSlot (applyClassicColumnOrder /
                        // swapClassicColumns), mentre ft2LinkModeSlot resta un host separato
                        // che prende larghezza solo quando il modo FT2-Link è attivo.
                        //
                        // Larghezza-valore: per-SLOT (posizione). Larghezza-MINIMA: segue il
                        // pannello che occupa lo slot (classicMinWidthForSlot). Lo slot che
                        // ospita la Live Map collassa a 0 quando la mappa è nascosta/staccata
                        // (classicSlotCollapsed), com'era con liveMapPanelHost.visible.

                        Item {
                            id: ft2LinkModeSlot
                            visible: mainWindow.ft2LinkModeActive
                            SplitView.fillWidth: mainWindow.ft2LinkModeActive
                            SplitView.preferredWidth: mainWindow.ft2LinkModeActive ? 820 : 0
                            SplitView.minimumWidth: mainWindow.ft2LinkModeActive ? 560 : 0

	                            Loader {
	                                id: ft2LinkInlineLoader
	                                anchors.fill: parent
	                                active: mainWindow.ft2LinkModeActive && !mainWindow.ft2LinkPanelDetached
	                                asynchronous: true
	                                source: "../panels/FT2LinkPanel.qml"
	                                onLoaded: {
	                                    if (item) {
	                                        item.dragTarget = null
	                                        item.toolTabsExternal = true
	                                        item.poppedOut = false
	                                    }
	                                }
	                            }

                                Rectangle {
                                    anchors.fill: parent
                                    visible: mainWindow.ft2LinkModeActive && mainWindow.ft2LinkPanelDetached
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.55)
                                    radius: 8
                                    border.color: glassBorder
                                    border.width: 1

                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 10

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("FT2-Link popped out")
                                            font.pixelSize: 13
                                            font.bold: true
                                            color: textSecondary
                                        }

                                        Rectangle {
                                            width: 54
                                            height: 24
                                            radius: 4
                                            color: ft2InlineDockMA.containsMouse
                                                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.30)
                                                   : "transparent"
                                            border.color: ft2InlineDockMA.containsMouse
                                                          ? secondaryCyan
                                                          : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.45)
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: "Dock"
                                                font.pixelSize: 10
                                                font.bold: true
                                                color: ft2InlineDockMA.containsMouse ? secondaryCyan : textPrimary
                                            }

                                            MouseArea {
                                                id: ft2InlineDockMA
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: mainWindow.dockFt2LinkPanel()
                                            }
                                        }
                                    }
                                }

	                            Connections {
	                                target: ft2LinkInlineLoader.item
	                                ignoreUnknownSignals: true
	                                function onCloseRequested() {
	                                    mainWindow.dockFt2LinkPanel()
	                                }
	                                function onPopDockRequested() {
	                                    mainWindow.popFt2LinkPanel()
	                                }
	                            }
	                        }

	                        // ========== SLOT 0 (default: LEFT Band Activity / Full Spectrum) ==========
	                        Item {
                            id: colSlot0
                            property int targetPanelWidth: mainWindow.savedPeriod1PanelWidth
                            // Centro-split affidabile 50/50 (ex-period1Panel): ricalcola la metà
                            // sinistra finché il parent non ha width; disattivato se l'utente
                            // ha trascinato il separatore.
                            property bool userDraggedSplit: false
                            readonly property bool slotCollapsed: mainWindow.classicSlotCollapsed(0)
                            // 1.0.388 — slot collassato = invisibile (lo SplitView lo esclude del
                            // tutto: niente colonna, niente maniglia, niente spazio residuo).
                            visible: !slotCollapsed
                            // fillWidth segue lo slot che ospita Full Spectrum (mai collassabile,
                            // sempre presente) → c'è sempre esattamente un riempitore valido.
                            SplitView.fillWidth: mainWindow.classicDecodeFillSlot() === 0
                            SplitView.preferredWidth: slotCollapsed ? 0 : targetPanelWidth
                            SplitView.minimumWidth: slotCollapsed ? 0 : mainWindow.classicMinWidthForSlot(0)
                            function applyCenterSplit() {
                                if (userDraggedSplit) return
                                if (mainWindow.decodePanelLayoutSaved) return
                                if (parent && parent.width > 0) {
                                    var mapW = 0
                                    if (typeof colSlot2 !== "undefined" && !colSlot2.slotCollapsed)
                                        mapW = colSlot2.targetPanelWidth
                                    colSlot0.targetPanelWidth = Math.max(360, Math.round((parent.width - mapW) * 0.5))
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
                                if (!slotCollapsed && width >= 360 && Math.abs(targetPanelWidth - width) >= 1) {
                                    // Variazione di width non originata da applyCenterSplit/restore
                                    // = drag manuale del separatore: marca lo split come spostato a
                                    // mano (ridondante col SplitHandle.onPressedChanged, ma copre il
                                    // caso in cui il press non venga intercettato) e persiste.
                                    if (!mainWindow.windowStateRestoreInProgress && !userDraggedSplit)
                                        userDraggedSplit = true
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }
                            Connections {
                                target: colSlot0.parent
                                ignoreUnknownSignals: true
                                function onWidthChanged() {
                                    if (!colSlot0.userDraggedSplit && !mainWindow.decodePanelLayoutSaved)
                                        colSlot0.applyCenterSplit()
                                }
                            }

                        // ========== LEFT: Band Activity ==========
                        Rectangle {
                            id: period1Panel
                            anchors.fill: parent
                            readonly property bool compactColumns: width < 620
                            readonly property int utcColumnWidth: compactColumns ? 66 : 86
                            readonly property int dbColumnWidth: compactColumns ? 34 : 38
                            readonly property int dbDtGapWidth: compactColumns ? 4 : 6
                            readonly property int dtColumnWidth: compactColumns ? 42 : 48
                            readonly property int dtFreqGapWidth: compactColumns ? 6 : 8
                            readonly property int freqColumnWidth: compactColumns ? 42 : 45
                            readonly property int gapColumnWidth: compactColumns ? 8 : 12
                            readonly property int distanceColumnWidth: mainWindow.fullSpectrumShowDistColumn && !compactColumns ? 58 : 0
                            readonly property int dxccColumnWidth: mainWindow.showDxccInfo ? (compactColumns ? 108 : Math.min(300, Math.max(190, Math.round(width * 0.24)))) : 0
                            readonly property int azColumnWidth: mainWindow.showDxccInfo && mainWindow.fullSpectrumShowAzColumn ? (compactColumns ? 42 : 52) : 0
                            readonly property int messageMinWidth: compactColumns ? 72 : 140
                            // Stadio 1: questo pannello riempie il suo slot-host (colSlot0 di
                            // default; spostabile via mappa). La larghezza-valore e il
                            // centro-split sono ora su colSlot0; qui resta solo il contenuto.
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
                                        text: qsTr("Drag the window here")
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
                                        running: period1DockHighlighted && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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

                                        // Maniglia di drag colonna (Stadio 1)
                                        Loader {
                                            Layout.preferredWidth: 16
                                            Layout.preferredHeight: 16
                                            Layout.alignment: Qt.AlignVCenter
                                            sourceComponent: colDragHandleComponent
                                            onLoaded: if (item) item.panelId = "fullspectrum"
                                        }

                                        // Active period indicator (pulsing)
                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: bridge.themeManager.successColor

                                            SequentialAnimation on opacity {
                                                running: decodePanel.isCurrentPeriodEven && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
                                                running: decodePanel.isCurrentPeriodEven && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                                                loops: Animation.Infinite
                                                OpacityAnimator { to: 0.6; duration: 600 }
                                                OpacityAnimator { to: 1.0; duration: 600 }
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: decodePanel.displayedDecodeCount() + " " + qsTr("decodes")
                                            font.pixelSize: 10
                                            color: textSecondary
                                        }

	                                        Rectangle {
	                                            Layout.preferredWidth: 40
	                                            Layout.preferredHeight: 18
	                                            radius: 4
	                                            color: p1ClearMA.containsMouse
	                                                ? Qt.rgba(244/255, 67/255, 54/255, 0.25)
	                                                : "transparent"
	                                            border.color: p1ClearMA.containsMouse
	                                                ? bridge.themeManager.ledRed
	                                                : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.16)
	                                            border.width: 1

	                                            Text {
	                                                anchors.centerIn: parent
	                                                text: qsTr("Clear")
	                                                font.pixelSize: 10
	                                                color: p1ClearMA.containsMouse
	                                                    ? bridge.themeManager.ledRed
	                                                    : textSecondary
	                                            }

	                                            MouseArea {
	                                                id: p1ClearMA
	                                                anchors.fill: parent
	                                                hoverEnabled: true
	                                                cursorShape: Qt.PointingHandCursor
	                                                onClicked: bridge.clearDecodes()
	                                            }

	                                            ToolTip.visible: p1ClearMA.containsMouse
	                                            ToolTip.text: qsTr("Clear Full Spectrum")
	                                            ToolTip.delay: 500
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
		                                                text: mainWindow.compactFullSpectrum ? "Full" : "Cmp"
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
	                                                ? qsTr("Return to normal row height")
	                                                : qsTr("Compact rows (2x visible decodes)")
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

                                // Column headers — data-driven (larghezza/visibilità/ordine
                                // configurabili). Tasto destro = menu colonne; trascina il
                                // bordo destro di una colonna per ridimensionarla.
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 20
                                    color: Qt.rgba(76/255, 175/255, 80/255, 0.2)
                                    radius: 2

                                    Menu {
                                        id: fsHeaderMenuEmbedded
                                        property string targetId: ""
                                        MenuItem { text: qsTr("◀  Move left"); enabled: mainWindow.fsCanMove(fsHeaderMenuEmbedded.targetId, -1); onTriggered: mainWindow.fsMoveColumn(fsHeaderMenuEmbedded.targetId, -1) }
                                        MenuItem { text: qsTr("Move right  ▶"); enabled: mainWindow.fsCanMove(fsHeaderMenuEmbedded.targetId, 1); onTriggered: mainWindow.fsMoveColumn(fsHeaderMenuEmbedded.targetId, 1) }
                                        MenuItem { text: qsTr("Hide this column"); enabled: mainWindow.fsColMeta(fsHeaderMenuEmbedded.targetId).canHide; onTriggered: mainWindow.fsSetColumnVisible(fsHeaderMenuEmbedded.targetId, false) }
                                        MenuSeparator {}
                                        MenuItem { text: (mainWindow.fsColVisible("utc")  ? "✓  " : "      ") + qsTr("UTC");     onTriggered: mainWindow.fsToggleColumnVisible("utc") }
                                        MenuItem { text: (mainWindow.fsColVisible("db")   ? "✓  " : "      ") + qsTr("dB");      onTriggered: mainWindow.fsToggleColumnVisible("db") }
                                        MenuItem { text: (mainWindow.fsColVisible("dt")   ? "✓  " : "      ") + qsTr("DT");      onTriggered: mainWindow.fsToggleColumnVisible("dt") }
                                        MenuItem { text: (mainWindow.fsColVisible("freq") ? "✓  " : "      ") + qsTr("Freq");    onTriggered: mainWindow.fsToggleColumnVisible("freq") }
                                        MenuItem { text: qsTr("✓  Message"); enabled: false }
                                        MenuItem { text: (mainWindow.fsColVisible("dist") ? "✓  " : "      ") + qsTr("Dist");    onTriggered: mainWindow.fsToggleColumnVisible("dist") }
                                        MenuItem { text: (mainWindow.fsColVisible("dxcc") ? "✓  " : "      ") + qsTr("DXCC");    enabled: mainWindow.showDxccInfo; onTriggered: mainWindow.fsToggleColumnVisible("dxcc") }
                                        MenuItem { text: (mainWindow.fsColVisible("az")   ? "✓  " : "      ") + qsTr("Az");      enabled: mainWindow.showDxccInfo; onTriggered: mainWindow.fsToggleColumnVisible("az") }
                                        MenuSeparator {}
                                        MenuItem { text: (mainWindow.fsNewestFirst ? "✓  " : "      ") + qsTr("Newest on top"); onTriggered: mainWindow.fsToggleNewestFirst() }
                                        MenuSeparator {}
                                        MenuItem { text: qsTr("Reset columns"); onTriggered: mainWindow.fsResetColumns() }
                                    }

                                    RowLayout {
                                        id: fsHeaderRowE
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 6

                                        Repeater {
                                            model: mainWindow.fsColumnsForWidth(period1Panel.width)
                                            delegate: Item {
                                                id: fsHCell
                                                readonly property var col: modelData
                                                readonly property var meta: mainWindow.fsColMeta(col.id)
                                                Layout.fillWidth: meta.fill
                                                Layout.preferredWidth: meta.fill ? -1 : mainWindow.fsColWidthForPanel(col.id, period1Panel.width)
                                                Layout.minimumWidth: meta.fill ? meta.minW : mainWindow.fsColWidthForPanel(col.id, period1Panel.width)
                                                Layout.fillHeight: true

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.rightMargin: fsHCell.meta.fill ? 0 : 5
                                                    text: fsHCell.meta.label
                                                    font.family: mainWindow.decodedTextFontFamily
                                                    font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs)
                                                    font.bold: true
                                                    color: bridge.themeManager.successColor
                                                    horizontalAlignment: fsHCell.meta.align === "right" ? Text.AlignRight : Text.AlignLeft
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideRight
                                                }
                                                // Linea divisoria/grip visibile sul bordo destro (zona di resize).
                                                Rectangle {
                                                    visible: !fsHCell.meta.fill
                                                    width: 1
                                                    anchors.right: parent.right
                                                    anchors.top: parent.top
                                                    anchors.bottom: parent.bottom
                                                    anchors.topMargin: 3
                                                    anchors.bottomMargin: 3
                                                    color: Qt.rgba(bridge.themeManager.successColor.r, bridge.themeManager.successColor.g, bridge.themeManager.successColor.b, 0.45)
                                                }
                                            }
                                        }
                                    }

                                    // Controller UNICO sopra le celle (z alto, FUORI dal Repeater →
                                    // sopravvive al rebuild durante il riordino live). Trascina l'etichetta
                                    // per spostare la colonna (reorder live), trascina il bordo destro per
                                    // ridimensionare, tasto destro = menu colonne.
                                    MouseArea {
                                        id: fsHdrCtlE
                                            anchors.fill: parent
                                            z: 100
                                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                                            hoverEnabled: true
                                            preventStealing: true
                                            property string grabId: ""
                                            property bool resizing: false
                                            property bool reordering: false
                                            property real pressSceneX: 0
                                            property int pressW: 0
                                            function cellAt(localX) {
                                                var p = mapToItem(fsHeaderRowE, localX, fsHeaderRowE.height / 2)
                                                return fsHeaderRowE.childAt(p.x, p.y)
                                            }
                                            // Colonna il cui BORDO destro è entro ±7px dal cursore (zona resize,
                                            // copre anche lo spacing fra colonne → facile da agganciare).
                                            function resizeTargetAt(localX) {
                                                var p = mapToItem(fsHeaderRowE, localX, 0)
                                                var kids = fsHeaderRowE.children
                                                for (var i = 0; i < kids.length; ++i) {
                                                    var c = kids[i]
                                                    if (!c || !c.col || mainWindow.fsColMeta(c.col.id).fill) continue
                                                    if (Math.abs(p.x - (c.x + c.width)) <= 7) return c.col.id
                                                }
                                                return ""
                                            }
                                            cursorShape: {
                                                if (resizing) return Qt.SplitHCursor
                                                if (reordering) return Qt.ClosedHandCursor
                                                return resizeTargetAt(mouseX) !== "" ? Qt.SplitHCursor : Qt.ArrowCursor
                                            }
                                            onPressed: function(m) {
                                                pressSceneX = mapToItem(null, m.x, m.y).x
                                                resizing = false; reordering = false
                                                var rid = resizeTargetAt(m.x)
                                                if (m.button === Qt.LeftButton && rid !== "") {
                                                    grabId = rid
                                                    resizing = true
                                                    pressW = mainWindow.fsColWidth(rid)
                                                } else {
                                                    var c = cellAt(m.x)
                                                    grabId = (c && c.col) ? c.col.id : ""
                                                }
                                            }
                                            onPositionChanged: function(m) {
                                                if (!pressed || grabId === "") return
                                                var sx = mapToItem(null, m.x, m.y).x
                                                if (resizing) {
                                                    mainWindow.fsSetColumnWidth(grabId, pressW + (sx - pressSceneX))
                                                    return
                                                }
                                                if (!(pressedButtons & Qt.LeftButton)) return
                                                if (!reordering && Math.abs(sx - pressSceneX) < 6) return
                                                reordering = true
                                                var c = cellAt(m.x)
                                                var overId = (c && c.col) ? c.col.id : ""
                                                if (overId !== "" && overId !== grabId)
                                                    mainWindow.fsMoveColumnToId(grabId, overId)
                                            }
                                            onReleased: function(m) {
                                                if (resizing) mainWindow.fsPersistWidths()
                                                else if (reordering) mainWindow.fsPersistOrder()
                                                grabId = ""; resizing = false; reordering = false
                                            }
                                            onCanceled: { grabId = ""; resizing = false; reordering = false }
                                            onClicked: function(m) {
                                                if (m.button === Qt.RightButton) {
                                                    var c = cellAt(m.x)
                                                    if (c && c.col) { fsHeaderMenuEmbedded.targetId = c.col.id; fsHeaderMenuEmbedded.popup() }
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
                                        model: !period1Detached
                                               ? ((bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : decodePanel.allDecodes)
                                               : null
                                        spacing: 1
                                        // 1.0.228 — cacheBuffer 3000 → 600 (allineato a DecodeList.qml).
                                        // Pre-1.0.228 con 3000px buffer e delegate complessi (RowLayout
                                        // + 9 Text con highlight checks) si istanziavano ~115 row out-of-
                                        // viewport, ognuna ricalcolata su decodeListVersion++ -> picco
                                        // CPU che contribuiva al "effetto molla" su scrolling tail-follow.
                                        cacheBuffer: 360
                                        reuseItems: true
                                        interactive: true
                                        verticalLayoutDirection: mainWindow.fsNewestFirst ? ListView.BottomToTop : ListView.TopToBottom
                                        property bool followTail: true
                                        property bool tailFollowPending: false
	                                        property bool tailFollowQueued: false
	                                        // 1.0.231 — counter pending decodes mentre user scrolla up.
	                                        // Permette al floating button "↓ N new" di sapere quanti
	                                        // decode sono arrivati dopo la perdita di tail-follow.
		                                        property int pendingNewDecodes: 0
		                                        function isNearTail() {
		                                            if (contentHeight <= height + 2) return true
		                                            if (mainWindow.fsNewestFirst) return contentY <= originY + 48
		                                            return contentY >= tailContentY() - 48
		                                        }
	                                        function updateFollowTail() {
	                                            if (tailFollowPending)
	                                                return
	                                            followTail = isNearTail()
	                                            // 1.0.231 — reset counter "↓ N new" quando torna a tail
	                                            if (followTail) evenPeriodList.pendingNewDecodes = 0
	                                        }
	                                        function tailContentY() {
	                                            if (mainWindow.fsNewestFirst) return originY
	                                            var bottom = originY + contentHeight - height
	                                            return Math.max(originY, bottom)
	                                        }
	                                        function finishTailFollow() {
	                                            var shouldSnap = tailFollowPending || followTail
	                                            tailFollowPending = false
	                                            if (shouldSnap) {
	                                                var targetY = tailContentY()
	                                                if (Math.abs(contentY - targetY) > 0.5)
	                                                    contentY = targetY
	                                            }
	                                            followTail = isNearTail()
	                                            if (followTail) evenPeriodList.pendingNewDecodes = 0
                                        }
                                        function forceTailFollow() {
    evenPeriodList.followTail = true
    evenPeriodList.tailFollowPending = true
    if (evenPeriodList.tailFollowQueued)
        return
    evenPeriodList.tailFollowQueued = true
    Qt.callLater(function() {
        evenPeriodList.tailFollowQueued = false
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
	                                            if (decodePanel.hasNativeBandActivityModel()) return
                                            // 1.0.231 — se user e' in scroll-back, no forced tail
                                            // ma incrementa counter per il floating "↓ N new" button.
	                                            if (!followTail) {
	                                                evenPeriodList.pendingNewDecodes++
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
	                                        // Keep decode-row motion on the GUI animation path. On Windows/D3D,
	                                        // first-use render-thread Animators can force a multi-second GPU sync.
	                                        // 1.0.255: !mainWindow.compactToggling -> disabilita displaced
	                                        // animations durante toggle compact (height change su tutti i
	                                        // delegate causa blink di 100ms).
	                                        add: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                                            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        addDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                                        }
	                                        moveDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                                        }
	                                        removeDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                                        }

                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                            interactive: true
                                            width: 8
                                        }

	                                        delegate: Rectangle {
	                                            id: fsRowEmbedded
	                                            // 1.0.155: separator meno invasivo — riga sottile, no label.
	                                            readonly property bool isPeriodSeparator: !!(modelData && modelData.isSeparator === true)
	                                            readonly property var entry: modelData || ({})
	                                            width: evenPeriodList.width
                                            // 1.0.229 — height adattiva via mainWindow.fullSpectrumRowHeight
                                            // (compact 14px / normal 26px). Toggle via toolbar o Ctrl+Shift+C.
                                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(mainWindow.fullSpectrumRowHeight * fs)
	                                            property var highlightFill: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightFill(modelData)
	                                            property var highlightBorder: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightBorder(modelData)
                                            property var userBgFill: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeUserBgFill(modelData)
	                                            // 1.0.205 — guard !modelData per evitare TypeError flood (~46/s) durante
	                                            // model swap transients che saturava il main thread via logger sincrono.
	                                            color: !modelData ? "transparent" :
	                                                   isPeriodSeparator ? "transparent" :
		                                                   userBgFill ? userBgFill :
                                                   highlightFill ? highlightFill :
			                                                   (entry.isCQ && bridge.decodeColorEnabled("colorCQ")) ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)) :
			                                                   decodePanel.isAtRxFrequency(entry.freq || "0", entry) ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(76/255, 175/255, 80/255, 0.2)) :
			                                                   index % 2 === 0 ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02)) : mainWindow.boostedDecodeBackgroundColor(Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05))
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
	                                                    if (entry.isTx) return
                                                    if (mouse.button === Qt.LeftButton) {
                                                        // Sinistro = imposta TX freq
                                                        if (!bridge.holdTxFreq)
	                                                            bridge.txFrequency = parseInt(entry.freq || "0")
                                                    } else if (mouse.button === Qt.RightButton) {
                                                        // Destro = imposta RX freq; Shift+Destro = QRZ.com (IU8LMC)
	                                                        if (mouse.modifiers & Qt.ShiftModifier)
	                                                            mainWindow.openQrzLookup(entry)
	                                                        else
	                                                            bridge.rxFrequency = parseInt(entry.freq || "0")
                                                    }
                                                }
                                                onDoubleClicked: function(mouse) {
                                                    if (parent.isPeriodSeparator) return
	                                                    if (!entry.isTx && mouse.button === Qt.LeftButton)
	                                                        decodePanel.handleDecodeDoubleClick(entry)
                                                }
                                                // IU8LMC: Show tooltip on hover
                                                onContainsMouseChanged: {
                                                    if (parent.isPeriodSeparator) { dxccTooltipVisible = false; return }
                                                    if (containsMouse) {
	                                                        dxccTooltipText = getDxccTooltipText(entry)
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
                                                spacing: 6

                                                Repeater {
                                                    model: mainWindow.fsColumnsForWidth(period1Panel.width)
                                                    delegate: Item {
                                                        id: fsCellE
                                                        readonly property var col: modelData
                                                        readonly property var meta: mainWindow.fsColMeta(col.id)
                                                        Layout.fillWidth: meta.fill
                                                        Layout.preferredWidth: meta.fill ? -1 : mainWindow.fsColWidthForPanel(col.id, period1Panel.width)
                                                        Layout.minimumWidth: meta.fill ? meta.minW : mainWindow.fsColWidthForPanel(col.id, period1Panel.width)
                                                        Layout.fillHeight: true
                                                        clip: col.id === "dxcc"
                                                        Text {
                                                            visible: fsCellE.col.id !== "dxcc"
                                                            anchors.fill: parent
                                                            text: mainWindow.fsCellText(fsRowEmbedded.entry, fsCellE.col.id)
                                                            color: mainWindow.fsCellColor(fsRowEmbedded.entry, fsCellE.col.id)
                                                            font.bold: mainWindow.fsCellBold(fsRowEmbedded.entry, fsCellE.col.id)
                                                            font.strikeout: fsCellE.col.id === "msg" ? decodePanel.decodeEntryStrikeout(fsRowEmbedded.entry) : false
                                                            font.family: mainWindow.decodedTextFontFamily
                                                            font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs)
                                                            horizontalAlignment: fsCellE.meta.align === "right" ? Text.AlignRight : Text.AlignLeft
                                                            verticalAlignment: Text.AlignVCenter
                                                            maximumLineCount: 1
                                                            elide: fsCellE.col.id === "msg" ? messageElideMode(fsRowEmbedded.entry.displayMessage || fsRowEmbedded.entry.message)
                                                                 : (fsCellE.col.id === "dxcc" ? Text.ElideRight : Text.ElideNone)
                                                            fontSizeMode: fsCellE.col.id === "dxcc" ? Text.HorizontalFit : Text.FixedSize
                                                            minimumPixelSize: fsCellE.col.id === "dxcc" ? Math.max(8, Math.round(mainWindow.decodedTextFontPixelSize * fs * 0.65)) : 0
                                                        }
                                                        Item {
                                                            visible: fsCellE.col.id === "dxcc"
                                                            anchors.fill: parent
                                                            Text {
                                                                anchors.fill: parent
                                                                anchors.rightMargin: fsRowEmbedded.entry.isLotw === true ? Math.max(9, Math.round(11 * fs)) : 0
                                                                text: mainWindow.fsCellText(fsRowEmbedded.entry, fsCellE.col.id)
                                                                color: mainWindow.fsCellColor(fsRowEmbedded.entry, fsCellE.col.id)
                                                                font.family: mainWindow.decodedTextFontFamily
                                                                font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs)
                                                                horizontalAlignment: Text.AlignRight
                                                                verticalAlignment: Text.AlignVCenter
                                                                maximumLineCount: 1
                                                                elide: Text.ElideRight
                                                                fontSizeMode: Text.HorizontalFit
                                                                minimumPixelSize: Math.max(8, Math.round(mainWindow.decodedTextFontPixelSize * fs * 0.65))
                                                            }
                                                            Rectangle {
                                                                visible: fsRowEmbedded.entry.isLotw === true
                                                                width: Math.max(5, Math.round(6 * fs))
                                                                height: width
                                                                radius: width / 2
                                                                anchors.right: parent.right
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                color: mainWindow.lotwMarkerColor()
                                                                border.color: mainWindow.boostedDecodeTextColor(textSecondary)
                                                                border.width: 1
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: qsTr("No decodes")
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
                                            ToolTip.text: qsTr("Go to the latest decode")
                                            ToolTip.delay: 500
                                        }
                                    }
                                }
                            }
                        }
                        }

                        // ========== SLOT 1 (default: RIGHT RX Frequency / Signal RX) ==========
                        Item {
                            id: colSlot1
                            property int targetPanelWidth: mainWindow.savedRxFreqPanelWidth
                            readonly property bool slotCollapsed: mainWindow.classicSlotCollapsed(1)
                            visible: !slotCollapsed   // 1.0.388 — vedi colSlot0
                            SplitView.fillWidth: mainWindow.classicDecodeFillSlot() === 1
                            SplitView.preferredWidth: slotCollapsed ? 0 : targetPanelWidth
                            SplitView.minimumWidth: slotCollapsed ? 0 : mainWindow.classicMinWidthForSlot(1)
                            onWidthChanged: {
                                if (!slotCollapsed && width >= 360 && Math.abs(targetPanelWidth - width) >= 1) {
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }

                        // ========== RIGHT: RX Frequency ==========
                        Rectangle {
                            id: rxFreqPanel
                            anchors.fill: parent
                            readonly property bool compactColumns: width < 450
                            readonly property bool compactHeader: width < 350
                            readonly property int utcColumnWidth: compactColumns ? 62 : 78
                            readonly property int dbColumnWidth: compactColumns ? 34 : 38
                            readonly property int dbDtGapWidth: compactColumns ? 4 : 6
                            readonly property int dtColumnWidth: compactColumns ? 42 : 48
                            readonly property int dtFreqGapWidth: mainWindow.signalRxShowFreqColumn ? (compactColumns ? 4 : 6) : 0
                            readonly property int freqColumnWidth: mainWindow.signalRxShowFreqColumn ? (compactColumns ? 42 : 46) : 0
                            readonly property int gapColumnWidth: compactColumns ? 8 : 12
                            readonly property int distanceColumnWidth: mainWindow.signalRxShowDistColumn && !compactColumns ? 56 : 0
                            readonly property int azColumnWidth: mainWindow.signalRxShowAzColumn && !compactColumns ? 42 : 0
                            // Stadio 1: riempie il suo slot-host (colSlot1 di default). La
                            // larghezza-valore è ora su colSlot1; qui resta solo il contenuto.
                            color: "transparent"

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
                                        text: qsTr("Drag the window here")
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
                                        running: rxFreqDockHighlighted && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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

                                        // Maniglia di drag colonna (Stadio 1)
                                        Loader {
                                            Layout.preferredWidth: 16
                                            Layout.preferredHeight: 16
                                            Layout.alignment: Qt.AlignVCenter
                                            sourceComponent: colDragHandleComponent
                                            onLoaded: if (item) item.panelId = "signalrx"
                                        }

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

	                                        // 1.0.253 — Compact mode toggle Signal RX
	                                        Rectangle {
	                                            Layout.preferredWidth: 34
	                                            Layout.preferredHeight: 18
	                                            radius: 4
	                                            color: rxCompactMA.containsMouse
	                                                ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
	                                                : (mainWindow.compactSignalRx
	                                                    ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
	                                                    : "transparent")
	                                            border.color: (rxCompactMA.containsMouse || mainWindow.compactSignalRx)
	                                                ? secondaryCyan
	                                                : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
	                                            border.width: 1
	                                            Text {
	                                                anchors.centerIn: parent
		                                                text: mainWindow.compactSignalRx ? "Full" : "Cmp"
	                                                font.pixelSize: mainWindow.compactSignalRx ? 10 : 9
	                                                font.bold: true
	                                                color: (rxCompactMA.containsMouse || mainWindow.compactSignalRx)
	                                                    ? secondaryCyan : textSecondary
	                                            }
	                                            MouseArea {
	                                                id: rxCompactMA
	                                                anchors.fill: parent
	                                                hoverEnabled: true
	                                                cursorShape: Qt.PointingHandCursor
	                                                onClicked: mainWindow.toggleCompactSignalRx()
	                                            }
	                                            ToolTip.visible: rxCompactMA.containsMouse
	                                            ToolTip.text: mainWindow.compactSignalRx
	                                                ? qsTr("Return to normal row height")
	                                                : qsTr("Compact rows (2x visible decodes)")
	                                            ToolTip.delay: 500
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
                                        Item { visible: rxFreqPanel.freqColumnWidth > 0; Layout.preferredWidth: rxFreqPanel.dtFreqGapWidth }
                                        Text { visible: rxFreqPanel.freqColumnWidth > 0; text: qsTr("Freq"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.freqColumnWidth }
                                        Item { Layout.preferredWidth: rxFreqPanel.gapColumnWidth }
                                        Text { text: qsTr("Message"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
                                        Text { visible: rxFreqPanel.distanceColumnWidth > 0; text: qsTr("Dist"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.distanceColumnWidth }
                                        Text { visible: rxFreqPanel.azColumnWidth > 0; text: qsTr("Az"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.azColumnWidth }
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
                                        cacheBuffer: 360  // 1.0.478 — meno delegate offscreen durante pile-up FT8/4/2
	                                        reuseItems: true
	                                        interactive: true
	                                        property bool followTail: true
	                                        property bool tailFollowPending: false
		                                        property bool tailFollowQueued: false
		                                        property int pendingNewDecodes: 0
		                                        function isNearTail() {
		                                            return contentHeight <= height + 2
		                                                || contentY >= tailContentY() - 48
		                                        }
	                                        function updateFollowTail() {
	                                            if (tailFollowPending)
	                                                return
	                                            followTail = isNearTail()
	                                            // 1.0.231 — reset counter "↓ N new" quando torna a tail
	                                            if (followTail) rxFrequencyList.pendingNewDecodes = 0
	                                        }
	                                        function tailContentY() {
	                                            var bottom = originY + contentHeight - height
	                                            return Math.max(originY, bottom)
	                                        }
	                                        function finishTailFollow() {
	                                            var shouldSnap = tailFollowPending || followTail
	                                            tailFollowPending = false
	                                            if (shouldSnap) {
	                                                var targetY = tailContentY()
	                                                if (Math.abs(contentY - targetY) > 0.5)
	                                                    contentY = targetY
	                                            }
	                                            followTail = isNearTail()
	                                            if (followTail) rxFrequencyList.pendingNewDecodes = 0
	                                        }
	                                        function forceTailFollow() {
    rxFrequencyList.followTail = true
    rxFrequencyList.tailFollowPending = true
    if (rxFrequencyList.tailFollowQueued)
        return
    rxFrequencyList.tailFollowQueued = true
    Qt.callLater(function() {
        rxFrequencyList.tailFollowQueued = false
        if (!rxFrequencyList)
            return
        var targetY = rxFrequencyList.tailContentY()
        rxFrequencyTailAnimation.stop()
        rxFrequencyList.tailFollowPending = true
        // 1.0.434 - come evenPeriodList (1.0.228): assegnazione diretta, niente
        // NumberAnimation. In FT2 i decode arrivano a raffica (piu' batch/periodo):
        // lo scroll animato 180-620ms si sovrapponeva ai nuovi decode -> "slot
        // machine" (righe che rimbalzano/saltano "nelle risposte"). Il tail-follow
        // non necessita animazione (l'addDisplaced YAnimator copre gia' i delegate).
        // NumberAnimation mantenuta nei floating detached (UX pop-out).
        rxFrequencyList.contentY = targetY
        rxFrequencyList.finishTailFollow()
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
                                            if (rxFreqDetached) return
                                            if (followTail || tailFollowPending)
                                                rxFrequencyTailSettleTimer.restart()
                                        }
                                        onHeightChanged: {
                                            if (rxFreqDetached) return
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
                                            if (rxFreqDetached) return
	                                            if (decodePanel.hasNativeRxDecodeModel()) return
                                            if (!followTail) {
                                                rxFrequencyList.pendingNewDecodes++
                                                return
                                            }
                                            forceTailFollow()
                                        }

                                        property int _ver: decodePanel.rxDecodeListVersion
                                        on_VerChanged: {
                                            if (rxFreqDetached) return
                                            if (!followTail) return
                                            forceTailFollow()
                                        }
                                        model: !rxFreqDetached
                                               ? ((bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : decodePanel.rxDecodes)
                                               : null
	                                        // Preserve the fade/slide without starting render-thread Animators.
	                                        add: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                                        }
	                                        addDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                                        }
	                                        moveDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                                        }
	                                        removeDisplaced: Transition {
	                                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
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
                                            // 1.0.253 — height adattiva compact mode Signal RX
                                            // 1.0.254 fix: rimosso `!hasEntry ? 0` clamp. Quando
                                            // modelData diventa transient-undefined durante shift-diff,
                                            // height collassa a 0 -> addDisplaced YAnimator chain crea
                                            // "blocchi neri mobili" durante scroll. Lasciare height
                                            // stabile e affidarsi al guard color in 6171.
                                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(mainWindow.signalRxRowHeight * fs)
                                            color: isPeriodSeparator ? "transparent" :
                                                   mainWindow.decodeUserBgFill(entry) ? mainWindow.decodeUserBgFill(entry) :
                                                   entry.isTx ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(241/255, 196/255, 15/255, 0.3)) :
                                                   entry.isMyCall ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(244/255, 67/255, 54/255, 0.3)) :
                                                   (entry.isCQ && bridge.decodeColorEnabled("colorCQ")) ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15)) :
                                                   index % 2 === 0 ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.08)) : mainWindow.boostedDecodeBackgroundColor(Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15))
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
	                                                        // Destro = imposta RX freq; Shift+Destro = QRZ.com (IU8LMC)
	                                                        if (mouse.modifiers & Qt.ShiftModifier)
	                                                            mainWindow.openQrzLookup(rxFrequencyDelegate.entry)
	                                                        else
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

		                                                Text { text: rxFrequencyDelegate.entry.formattedTime || decodePanel.formatUtcForDisplay(rxFrequencyDelegate.entry.time); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(rxFrequencyDelegate.entry.isTx ? "#f1c40f" : textSecondary); Layout.preferredWidth: rxFreqPanel.utcColumnWidth }
		                                                Text { text: rxFrequencyDelegate.entry.db || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(rxFrequencyDelegate.entry.snrColor || (rxFrequencyDelegate.entry.isTx ? "#f1c40f" : textSecondary)); font.bold: rxFrequencyDelegate.entry.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dbColumnWidth }
	                                                Item { Layout.preferredWidth: rxFreqPanel.dbDtGapWidth }
		                                                Text { text: rxFrequencyDelegate.entry.dt || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(rxFrequencyDelegate.entry.isTx ? "#f1c40f" : textSecondary); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dtColumnWidth }
	                                                Item { visible: rxFreqPanel.freqColumnWidth > 0; Layout.preferredWidth: rxFreqPanel.dtFreqGapWidth }
		                                                Text { visible: rxFreqPanel.freqColumnWidth > 0; text: rxFrequencyDelegate.entry.freq || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(rxFrequencyDelegate.entry.isTx ? "#f1c40f" : secondaryCyan); font.bold: rxFrequencyDelegate.entry.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.freqColumnWidth }
                                                Item { Layout.preferredWidth: rxFreqPanel.gapColumnWidth }
                                                Rectangle { property int dotSize: Math.max(5, Math.round(6 * fs)); visible: rxFrequencyDelegate.entry.isLotw === true; width: dotSize; height: dotSize; Layout.preferredWidth: dotSize; Layout.preferredHeight: dotSize; Layout.alignment: Qt.AlignVCenter; radius: dotSize / 2; color: mainWindow.lotwMarkerColor(); border.color: mainWindow.boostedDecodeTextColor(textSecondary); border.width: 1 }
                                                Text { text: rxFrequencyDelegate.entry.displayMessage || rxFrequencyDelegate.entry.message || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); font.bold: decodePanel.decodeEntryBold(rxFrequencyDelegate.entry); font.strikeout: decodePanel.decodeEntryStrikeout(rxFrequencyDelegate.entry); color: getDxccColor(rxFrequencyDelegate.entry); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: messageElideMode(rxFrequencyDelegate.entry.displayMessage || rxFrequencyDelegate.entry.message) }
		                                                Text { visible: rxFreqPanel.distanceColumnWidth > 0; text: decodePanel.distanceText(rxFrequencyDelegate.entry); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(textSecondary); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.distanceColumnWidth }
		                                                Text { visible: rxFreqPanel.azColumnWidth > 0; text: formatBearingDegrees(rxFrequencyDelegate.entry.dxBearing); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(secondaryCyan); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.azColumnWidth }
	                                            }
	                                        }

                                    }
                                }
                            }
                        }
                        }

                        // ========== SLOT 2 (default: Live Map) ==========
                        Item {
                            id: colSlot2
                            property int targetPanelWidth: mainWindow.savedLiveMapPanelWidth
                            readonly property bool slotCollapsed: mainWindow.classicSlotCollapsed(2)
                            visible: !slotCollapsed   // 1.0.388 — vedi colSlot0
                            SplitView.fillWidth: mainWindow.classicDecodeFillSlot() === 2
                            SplitView.preferredWidth: slotCollapsed ? 0 : targetPanelWidth
                            SplitView.minimumWidth: slotCollapsed ? 0 : mainWindow.classicMinWidthForSlot(2)
                            onWidthChanged: {
                                if (!slotCollapsed && width >= 280 && Math.abs(targetPanelWidth - width) >= 1) {
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }

                        Rectangle {
                            id: liveMapPanelHost
                            anchors.fill: parent
                            // Stadio 1: riempie il suo slot-host (colSlot2 di default). Il
                            // collasso quando la mappa è nascosta/staccata è gestito dallo slot
                            // (classicSlotCollapsed) -> segue il pannello in qualunque slot.
                            visible: mainWindow.liveMapPanelVisible && !mainWindow.liveMapDetached
                            color: "transparent"

                            Loader {
                                id: liveMapEmbeddedLoader
                                anchors.fill: parent
                                active: liveMapPanelHost.visible && mainWindow.startupLiveMapVisualReady
                                asynchronous: true
                                sourceComponent: liveMapEmbeddedComponent
                            }

                            // Maniglia di drag colonna (Stadio 1) — overlay sull'angolo
                            // alto-SINISTRO (i controlli della mappa stanno in alto a destra),
                            // sopra LiveMapPanel. Non tocca LiveMapPanel.qml.
                            Loader {
                                z: 50
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.leftMargin: 6
                                anchors.topMargin: 6
                                width: 16
                                height: 16
                                active: liveMapPanelHost.visible && mainWindow.startupLiveMapVisualReady
                                sourceComponent: colDragHandleComponent
                                onLoaded: if (item) item.panelId = "livemap"
                            }

                            Component {
                                id: liveMapEmbeddedComponent

                                LiveMapPanel {
                                    engine: bridge
                                    detachable: true
                                    detached: false
                                    onDetachRequested: mainWindow.detachLiveMapPanel()
                                }
                            }
                        }
                        }

                        // ========== SLOT 3 (DX Cluster) — 1.0.385 4ª colonna dedicata ==========
                        // Esiste SOLO quando il Cluster è dockato. Quando è spento o staccato
                        // (finestra flottante) lo slot va `visible:false`: 1.0.388 — con
                        // preferredWidth/minimumWidth=0 lo SplitView teneva comunque la
                        // larghezza trascinata a mano (colonna non spariva); `visible:false`
                        // lo esclude del tutto dal layout → niente colonna, niente handle.
                        Item {
                            id: colSlot3
                            property int targetPanelWidth: mainWindow.savedDxClusterColumnWidth
                            readonly property bool slotCollapsed: mainWindow.classicSlotCollapsed(3)
                            visible: !slotCollapsed
                            SplitView.fillWidth: mainWindow.classicDecodeFillSlot() === 3
                            SplitView.preferredWidth: slotCollapsed ? 0 : targetPanelWidth
                            SplitView.minimumWidth: slotCollapsed ? 0 : mainWindow.classicMinWidthForSlot(3)
                            onWidthChanged: {
                                if (!slotCollapsed && width >= 320 && Math.abs(targetPanelWidth - width) >= 1) {
                                    targetPanelWidth = Math.round(width)
                                    if (!mainWindow.windowStateRestoreInProgress)
                                        mainWindow.scheduleWindowStateSave()
                                }
                            }

                            // Pannello inline (Cluster dockato)
                            Rectangle {
                                id: dxClusterPanelHost
                                anchors.fill: parent
                                visible: mainWindow.dxClusterPanelVisible && !mainWindow.dxClusterDetached
                                color: "transparent"

                                DxClusterPanel {
                                    anchors.fill: parent
                                    embedded: true
                                    onCloseRequested: mainWindow.dxClusterPanelVisible = false
                                }

                                // Maniglia di drag colonna (riusa colDragHandleComponent)
                                Loader {
                                    z: 50
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.leftMargin: 6
                                    anchors.topMargin: 6
                                    width: 16
                                    height: 16
                                    active: dxClusterPanelHost.visible
                                    sourceComponent: colDragHandleComponent
                                    onLoaded: if (item) item.panelId = "dxcluster"
                                }

                                // Pulsante "Stacca" leggibile → ri-flotta il Cluster in finestra
                                // (1.0.388: era un'icona 16px poco leggibile, ora pill con testo)
                                Rectangle {
                                    z: 50
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.leftMargin: 28
                                    anchors.topMargin: 6
                                    height: 20
                                    width: dxcDetachLabel.implicitWidth + 16
                                    radius: 4
                                    color: dxcDetachMA.containsMouse ? secondaryCyan
                                                                     : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.92)
                                    border.color: secondaryCyan
                                    border.width: 1
                                    Text {
                                        id: dxcDetachLabel
                                        anchors.centerIn: parent
                                        text: "⤢ " + qsTr("Detach")
                                        font.pixelSize: 11
                                        font.bold: true
                                        color: dxcDetachMA.containsMouse ? bgDeep : textPrimary
                                    }
                                    MouseArea {
                                        id: dxcDetachMA
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: mainWindow.detachDxClusterPanel()
                                        ToolTip.visible: containsMouse
                                        ToolTip.text: qsTr("Detach the DX Cluster to a window")
                                    }
                                }
                            }

                        }
                    }

                    // ════════ Drag-layer pannelli interscambiabili (Stadio 1+2) ════════
                    // Dichiarato dentro decodePanel ma RE-PARENTATO a contentArea (parent:
                    // contentArea) così copre SIA le 3 colonne (decodePanel) SIA la TX area
                    // (txPanelContainer), entrambe figlie di contentArea -> targeting 2-D su
                    // 4 slot cross-container. NON gestito da alcuno SplitView (non diventa
                    // sezione/colonna). Ospita ghost + evidenziazione magnetica. NESSUN
                    // layer.enabled/FBO. Coordinate in spazio contentArea.
                    Item {
                        id: colDragLayer
                        parent: contentArea
                        anchors.fill: parent
                        z: 60
                        // Trasparente agli eventi quando non si sta trascinando: NON deve
                        // intercettare click/hover dei pannelli sottostanti.
                        visible: dragSlotIndex >= 0
                        enabled: false

                        // Indice (0..4) dello slot di PARTENZA del drag corrente; -1 = nessun drag.
                        property int dragSlotIndex: -1
                        // Indice (0..4) dello slot target sotto il puntatore.
                        property int dropSlotIndex: -1
                        // Stadio 3: numero di slot interscambiabili (3 colonne + TX area + Waterfall top).
                        readonly property int slotCount: 6   // 1.0.385 — +4ª colonna DX Cluster

                        // Rettangolo (in spazio contentArea) dello slot di indice idx.
                        // contentArea è l'antenato comune di colSlot0/1/2 (via decodePanel),
                        // di txSlot (via txPanelContainer) e del topSlot waterfallPanel (via
                        // mainVerticalSplit): mapToItem è valido per tutti e 5 gli slot.
                        function slotRect(idx) {
                            var slot = mainWindow.classicSlotForIndex(idx)
                            if (!slot || slot.width <= 0 || slot.height <= 0)
                                return null
                            var p = slot.mapToItem(contentArea, 0, 0)
                            return Qt.rect(p.x, p.y, slot.width, slot.height)
                        }

                        // Slot il cui CENTRO 2-D è più vicino al puntatore (spazio contentArea).
                        // Stadio 3: targeting in 2 dimensioni su 5 slot — la Waterfall (topSlot,
                        // in alto a tutta larghezza) + i 3 colSlot affiancati al centro + il
                        // txSlot largo-basso in basso. Ignora gli slot collassati (Live Map
                        // nascosta) e quelli senza rettangolo (pannello staccato -> host a 0).
                        function computeTargetSlot(panelX, panelY) {
                            var best = -1
                            var bestDist = 1e24
                            for (var i = 0; i < slotCount; ++i) {
                                if (mainWindow.classicSlotCollapsed(i))
                                    continue
                                var r = slotRect(i)
                                if (!r)
                                    continue
                                var midX = r.x + r.width / 2
                                var midY = r.y + r.height / 2
                                var dx = panelX - midX
                                var dy = panelY - midY
                                var d = dx * dx + dy * dy
                                if (d < bestDist) { bestDist = d; best = i }
                            }
                            return best
                        }

                        // ── Evidenziazione magnetica dello slot target ──
                        Rectangle {
                            id: colDropHighlight
                            visible: colDragLayer.dragSlotIndex >= 0 && colDragLayer.dropSlotIndex >= 0
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
                            border.color: secondaryCyan
                            border.width: 2
                            radius: 8
                            property var r: visible ? colDragLayer.slotRect(colDragLayer.dropSlotIndex) : null
                            x: r ? r.x : 0
                            y: r ? r.y : 0
                            width: r ? r.width : 0
                            height: r ? r.height : 0
                            Behavior on x { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }
                            Behavior on width { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }
                        }

                        // ── Ghost del pannello trascinato (proxy visuale) ──
                        Rectangle {
                            id: colDragGhost
                            visible: false
                            z: 70
                            radius: 8
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.92)
                            border.color: secondaryCyan
                            border.width: 1
                            opacity: 0.92
                            property string label: ""

                            function startFor(idx, panelX, panelY) {
                                var r = colDragLayer.slotRect(idx)
                                width = r ? Math.min(220, r.width) : 200
                                height = 40
                                label = colDragGhost.labelForId(mainWindow.classicIdInSlot(idx))
                                updateAt(panelX, panelY)
                                visible = true
                            }
                            function labelForId(panelId) {
                                if (panelId === "fullspectrum") return "Full Spectrum"
                                if (panelId === "signalrx")     return "Signal RX"
                                if (panelId === "livemap")      return "Live Map"
                                if (panelId === "txpanel")      return "TX Panel"
                                if (panelId === "waterfall")    return "Waterfall"
                                if (panelId === "dxcluster")    return "DX Cluster"
                                return panelId
                            }
                            function updateAt(panelX, panelY) {
                                var nx = panelX - width / 2
                                var ny = panelY - height / 2
                                if (nx < 0) nx = 0
                                if (nx > contentArea.width - width) nx = contentArea.width - width
                                if (ny < 0) ny = 0
                                if (ny > contentArea.height - height) ny = contentArea.height - height
                                x = nx
                                y = ny
                            }
                            function stop() {
                                visible = false
                                label = ""
                            }

                            Behavior on x { NumberAnimation { duration: 60; easing.type: Easing.OutQuad } }
                            Behavior on y { NumberAnimation { duration: 60; easing.type: Easing.OutQuad } }

                            Row {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: "⠿"
                                    font.pixelSize: 13
                                    color: secondaryCyan
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: colDragGhost.label
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    // ════════ Maniglia di drag riusabile per gli header dei 3 pannelli ════════
                    // Pattern World Clock (commit 8c6ce2b): piccola presa ⠿, z alto,
                    // SizeAllCursor; SOLO la presa avvia long-press 350ms + soglia 6px.
                    // È un elemento separato e piccolo: NON interferisce con l'header-drag
                    // del detach (che vive sull'header/finestra). Espone:
                    //   panelId  -> id logico del pannello cui appartiene ("fullspectrum"/…)
                    // Trova lo slot di partenza a runtime via classicSlotIndexOfId(panelId),
                    // così funziona qualunque sia l'ordine corrente della mappa.
                    Component {
                        id: colDragHandleComponent
                        Rectangle {
                            id: colHandle
                            property string panelId: ""
                            width: 16
                            height: 16
                            radius: 3
                            z: 40
                            color: colHandleMA.containsMouse || colHandleMA.armed
                                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                                   : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.12)
                            border.color: colHandleMA.containsMouse ? secondaryCyan : "transparent"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "⠿"
                                font.pixelSize: 12
                                color: colHandleMA.containsMouse ? secondaryCyan
                                                                 : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
                            }

                            MouseArea {
                                id: colHandleMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.SizeAllCursor
                                acceptedButtons: Qt.LeftButton
                                preventStealing: true

                                property bool armed: false
                                property int srcSlot: -1
                                property real pressX: 0
                                property real pressY: 0
                                property bool moved: false

                                function panelPt(mouse) {
                                    return mapToItem(contentArea, mouse.x, mouse.y)
                                }

                                Timer {
                                    id: colHandleHoldTimer
                                    interval: 350
                                    repeat: false
                                    onTriggered: {
                                        if (colHandleMA.pressedButtons & Qt.LeftButton) {
                                            colHandleMA.srcSlot = mainWindow.classicSlotIndexOfId(colHandle.panelId)
                                            if (colHandleMA.srcSlot < 0)
                                                return
                                            colHandleMA.armed = true
                                            colDragLayer.dragSlotIndex = colHandleMA.srcSlot
                                            colDragLayer.dropSlotIndex = colHandleMA.srcSlot
                                            colDragGhost.startFor(colHandleMA.srcSlot,
                                                                  colHandleMA.pressX, colHandleMA.pressY)
                                        }
                                    }
                                }

                                onPressed: function(mouse) {
                                    armed = false
                                    moved = false
                                    var p = panelPt(mouse)
                                    pressX = p.x
                                    pressY = p.y
                                    colHandleHoldTimer.start()
                                }
                                onPositionChanged: function(mouse) {
                                    var p = panelPt(mouse)
                                    if (Math.abs(p.x - pressX) > 6 || Math.abs(p.y - pressY) > 6)
                                        moved = true
                                    if (armed) {
                                        colDragGhost.updateAt(p.x, p.y)
                                        colDragLayer.dropSlotIndex = colDragLayer.computeTargetSlot(p.x, p.y)
                                    } else if (moved) {
                                        colHandleHoldTimer.stop()
                                    }
                                }
                                onReleased: function(mouse) {
                                    colHandleHoldTimer.stop()
                                    if (armed) {
                                        var p = panelPt(mouse)
                                        var target = colDragLayer.computeTargetSlot(p.x, p.y)
                                        colDragGhost.stop()
                                        var src = colDragLayer.dragSlotIndex
                                        colDragLayer.dragSlotIndex = -1
                                        colDragLayer.dropSlotIndex = -1
                                        armed = false
                                        if (target >= 0 && target !== src)
                                            mainWindow.swapClassicColumns(src, target)
                                    }
                                }
                                onCanceled: {
                                    colHandleHoldTimer.stop()
                                    if (armed) {
                                        colDragGhost.stop()
                                        colDragLayer.dragSlotIndex = -1
                                        colDragLayer.dropSlotIndex = -1
                                        armed = false
                                    }
                                }
                                ToolTip.visible: containsMouse && !armed
                                ToolTip.text: qsTr("Drag to swap the column")
                                ToolTip.delay: 500
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
	                            if (newHeight >= txPanelContainer.minHeight
	                                    && newHeight <= txPanelContainer.maxHeight) {
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
	                height: mainWindow.ft2LinkModeActive && !txPanelDetached
	                        ? Math.max(minHeight, Math.min(maxHeight, txPanelAutoHeight))
	                        : 160
	                color: "transparent"

	                readonly property int txPanelAutoHeight: Math.ceil((txPanelComponent ? txPanelComponent.implicitHeight : 92) + 2)
	                property int minHeight: mainWindow.ft2LinkModeActive ? 72 : 100
	                property int maxHeight: 350

                // Placeholder when detached - magnetic dock zone.
                // Stadio 2: mostrato solo quando la TX area (slot 3) ospita davvero il TX
                // (id "txpanel"); se il TX è stato spostato in una colonna, l'area TX ospita
                // un altro pannello e il placeholder non deve coprirlo. Il dock-back resta
                // possibile via la finestra flottante (zona = txPanelContainer) in ogni caso.
                Rectangle {
                    anchors.fill: parent
                    visible: txPanelDetached && mainWindow.classicIdInSlot(4) === "txpanel"
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
                            text: qsTr("Drag the window here")
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
                            running: txPanelDockHighlighted && bridge && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
                            loops: Animation.Infinite
                            OpacityAnimator { to: 0.4; duration: 300 }
                            OpacityAnimator { to: 1.0; duration: 300 }
                        }
                    }
                }

                // ════════ SLOT-HOST 4 (Stadio 2 — TX area interscambiabile) ════════
                // txSlot riempie l'area TX (txPanelContainer) ed ospita il pannello
                // assegnato allo slot 3 dalla mappa. Di DEFAULT contiene txPanelHostWrapper
                // (wrapper re-parentabile attorno all'istanza TxPanel). Lo SWAP cross-container
                // re-parenta txPanelHostWrapper in un colSlot (e una colonna in txSlot) via
                // applyClassicColumnOrder/swapClassicColumns. txSlot NON è figlio dello
                // SplitView -> lo SplitView resta a 3 figli. Placeholder detach + resize
                // handle restano figli DIRETTI di txPanelContainer (operano sull'AREA, non
                // sul pannello): detach/verticalResize del TX preservati in ogni ordine.
                Item {
                    id: txSlot
                    anchors.fill: parent

                    // Wrapper re-parentabile (panelId "txpanel"). anchors.fill -> assume la
                    // geometria del nuovo host (txSlot largo-basso, o un colSlot alto-stretto).
                    Item {
                        id: txPanelHostWrapper
                        anchors.fill: parent

                        // Actual TxPanel content
                        TxPanel {
	                            id: txPanelComponent
	                            anchors.fill: parent
	                            engine: bridge
	                            ft2LinkToolPanel: mainWindow.ft2LinkPanelDetached
	                                              ? ft2LinkFloatingLoader.item
	                                              : ft2LinkInlineLoader.item
                            showAsyncIcon: mainWindow.asyncIconVisible
                            handleLogPrompt: !txPanelDetached
                            visible: !txPanelDetached
                            onMamWindowRequested: mainWindow.openMamWindow()
                            onCallRequested: mainWindow.openCallDialog()
                            onFt2LinkAccessRequested: mainWindow.requestFt2LinkAccess()

                            // Maniglia di drag colonna (Stadio 2) — overlay sull'angolo
                            // alto-SINISTRO dell'header TX (il pulsante Pop è in alto a
                            // destra), sopra TxPanel. Non tocca TxPanel.qml. Viaggia col
                            // pannello quando re-parentato in un colSlot.
                            Loader {
                                z: 210
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.leftMargin: 6
                                anchors.topMargin: 5
                                width: 16
                                height: 16
                                active: !txPanelDetached
                                sourceComponent: colDragHandleComponent
                                onLoaded: if (item) item.panelId = "txpanel"
                            }

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
                                ToolTip.text: qsTr("Detach TX panel")
                                ToolTip.delay: 500
                            }
                        }
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
                }
            }
        }
        } // End contentScroll Flickable

        // DX-Pedition Fase 2a — alternative 3-column tactical workspace (opt-in).
        // Loader is inactive (zero cost) unless mainWindow.dxPeditionMode is ON, in
        // which case the classic header / contentScroll / StatusBar above collapse to 0.
        Loader {
            id: dxPeditionLoader
            Layout.fillWidth: true
            Layout.fillHeight: active
            Layout.preferredHeight: active ? -1 : 0
            visible: active
            active: mainWindow.dxPeditionMode
            asynchronous: true
            source: "components/DxPeditionWorkspace.qml"
            onLoaded: {
                item.bridge = bridge
                item.engine = bridge
                item.txPanelDetached = Qt.binding(function() { return mainWindow.txPanelDetached })
                // Let the workspace exit the mode / open Settings (its EXIT + SETUP
                // buttons) — the classic footer/menu is collapsed while DX-Ped is ON.
                item.requestExitDxPedition.connect(function() {
                    mainWindow.dxPeditionMode = false
                    bridge.setSetting("uiDxPeditionMode", false)
                })
                item.requestOpenSettings.connect(function() {
                    mainWindow.openSettingsDialog()
                })
                // 1.0.344 — LOG header + finestra MAM dalla DX-Pedition.
                item.requestOpenLog.connect(function() {
                    mainWindow.openLogWindow()
                })
                item.requestOpenMam.connect(function() {
                    mainWindow.openMamWindow()
                })
                // 1.0.345 — MACRO + CAT dalla DX-Pedition.
                item.requestOpenMacro.connect(function() {
                    mainWindow.openMacroDialog()
                })
                item.requestOpenCat.connect(function() {
                    if (bridge && bridge.openCatSettings)
                        bridge.openCatSettings()
                })
            }
        }

        // Status Bar
        StatusBar {
            Layout.fillWidth: true
            visible: !mainWindow.dxPeditionMode
            Layout.preferredHeight: visible ? implicitHeight : 0
            audioLevel: bridge ? bridge.audioLevel : 0.0
            signalLevel: bridge ? bridge.sMeter : 0.0
            monitoring: bridge ? bridge.monitoring : false
            transmitting: bridge ? bridge.transmitting : false
            pttPending: bridge ? (bridge.pttPending && !bridge.pttConfirmed) : false
            tuning: bridge ? bridge.tuning : false
            decoding: bridge ? bridge.decoding : false
            catStatus: bridge && bridge.catConnected ? "Connected" : "Disconnected"
        }
    }

    Dialog {
        id: resetLayoutConfirmDialog
        modal: true
        width: Math.max(360, Math.min(parent ? parent.width - 48 : 520, 520))
        implicitWidth: 480
        implicitHeight: 190
        anchors.centerIn: parent
        title: qsTr("Reset Layout")
        standardButtons: Dialog.Yes | Dialog.No

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: accentOrange
            border.width: 1
            radius: 8
        }

        contentItem: Item {
            implicitWidth: 440
            implicitHeight: 92

            Text {
                anchors.fill: parent
                text: qsTr("Bring all floating windows back into the main window\nand re-center Decodium on the primary monitor?\n\nThe saved coordinates will be cleared.")
                color: textPrimary
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }
        }

        onAccepted: {
            mainWindow.resetClassicColumnOrder()
            if (bridge)
                bridge.resetWindowLayout()
        }
    }

    // 1.0.498 — Offerta una-tantum della Modalità PC lento. Neutra (non dà del
    // "vecchio" al PC): la propone e basta, l'utente sceglie. Si mostra una sola
    // volta (flag LowEndModeOffered) e solo se la modalità non è già attiva.
    Timer {
        id: slowPcOfferTimer
        interval: 5000
        repeat: false
        running: !!bridge && !bridge.lowEndMode && !mainWindow.settingBool("LowEndModeOffered", false)
        onTriggered: {
            if (bridge && !bridge.lowEndMode && !mainWindow.settingBool("LowEndModeOffered", false))
                slowPcOfferDialog.open()
        }
    }

    Dialog {
        id: slowPcOfferDialog
        modal: true
        width: Math.max(380, Math.min(parent ? parent.width - 48 : 560, 560))
        implicitWidth: 520
        implicitHeight: 250
        anchors.centerIn: parent
        title: qsTr("Slow-PC mode")
        standardButtons: Dialog.Yes | Dialog.No

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: primaryBlue
            border.width: 1
            radius: 8
        }

        contentItem: Item {
            implicitWidth: 480
            implicitHeight: 160

            Text {
                anchors.fill: parent
                text: qsTr("If Decodium feels slow or the graphics freeze on this computer, you can turn on Slow-PC mode: it switches to OpenGL graphics (stable on older video cards), lightens CPU usage, and hides the heavy Live Map.\n\nTurn it on now? You can change this anytime in Settings.")
                color: textPrimary
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }
        }

        onAccepted: {
            if (bridge) {
                bridge.lowEndMode = true
                bridge.setSetting("LowEndModeOffered", true)
            }
        }
        onRejected: {
            if (bridge)
                bridge.setSetting("LowEndModeOffered", true)
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
                        text: qsTr("The legacy radio backend reported a problem.")
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
                text: rigErrorDetailsVisible ? qsTr("Hide details") : qsTr("Show details")
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
                text: qsTr("Configure radio")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    rigErrorDialog.close()
                    openSettingsTab(1)
                }
            }

            Button {
                text: qsTr("Retry")
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
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 36
                    font.bold: true
                    font.letterSpacing: 3
                    color: badgeColor
                }

                Text {
                    text: badgeSubText
                    font.family: decodiumMonoFontFamily
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
        // 1.0.308 (#7) — slide-in dall'alto: scende quando appare (più evidente del solo fade)
        anchors.topMargin: statusToastVisible ? 76 : 40
        anchors.rightMargin: 24
        width: Math.min(parent.width * 0.5, 600)
        implicitHeight: toastContent.implicitHeight + 28
        radius: 12
        // resta renderizzato durante il fade-out
        visible: opacity > 0.01
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.97)
        border.color: statusToastColor
        border.width: 2
        opacity: statusToastVisible ? 1.0 : 0.0

        // 1.0.308 (#7) — glow pulsante: attira l'occhio anche con la visione periferica
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.color: statusToastColor
            border.width: 3
            opacity: 0.0
            z: -1
            SequentialAnimation on opacity {
                running: statusToastVisible && (!bridge || bridge.uiQuality !== "Low")
                loops: Animation.Infinite
                NumberAnimation { from: 0.0; to: 0.65; duration: 600; easing.type: Easing.OutQuad }
                NumberAnimation { from: 0.65; to: 0.0; duration: 600; easing.type: Easing.InQuad }
            }
        }

        Row {
            id: toastContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 11

            Rectangle {
                width: 10; height: 10; radius: 5
                color: statusToastColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                width: parent.width - 21
                spacing: 5

                Text {
                    text: qsTr("Notifica")
                    font.pixelSize: 13
                    font.bold: true
                    color: statusToastColor
                }

                Text {
                    width: parent.width
                    text: statusToastText
                    wrapMode: Text.Wrap
                    font.pixelSize: 14
                    color: textPrimary
                }
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: statusToastVisible ? 220 : 300; easing.type: statusToastVisible ? Easing.OutQuad : Easing.InQuad }
        }
        Behavior on anchors.topMargin {
            NumberAnimation { duration: 260; easing.type: Easing.OutBack }
        }
    }

    Loader {
        id: logWindowLoader
        anchors.fill: parent
        active: false
        asynchronous: true
        source: "components/LogWindow.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: LogWindow")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    Loader {
        id: macroDialogLoader
        anchors.fill: parent
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
        anchors.fill: parent
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

    Loader {
        id: satelliteWindowLoader
        // A Popup/Dialog must keep its own dimensions.  Filling the main
        // window here forces the loaded SatelliteWindow to become a giant
        // transparent overlay and stretches every child layout with it.
        anchors.centerIn: parent
        active: false
        asynchronous: true
        source: "components/SatelliteWindow.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: SatelliteWindow")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    Connections {
        target: bridge
        ignoreUnknownSignals: true
        function onSatelliteTrackingWindowRequested() {
            openSatelliteWindow()
        }
    }

    // Setup is hosted by a real top-level window.  Popup coordinates are local
    // to Main.qml and cannot cross a monitor boundary on macOS; this host uses
    // desktop-global coordinates while preserving the existing themed dialog.
    Window {
        id: settingsFloatingWindow
        property int requestedTab: -1
        property bool desktopMoveActive: false
        // Su un portatile 1366x768 una finestra da 1500x900 non ci sta: usciva
        // dallo schermo, e con lei la colonna di destra delle impostazioni -
        // dove vive il campo della porta. Si parte dalla misura voluta, ma
        // senza mai arrivare a occupare tutto lo schermo.
        readonly property int availableScreenWidth: Screen.desktopAvailableWidth > 0
                                                    ? Screen.desktopAvailableWidth
                                                    : Screen.width
        readonly property int availableScreenHeight: Screen.desktopAvailableHeight > 0
                                                     ? Screen.desktopAvailableHeight
                                                     : Screen.height
        readonly property int preferredWidth: Math.max(minimumWidth,
                                                       Math.min(1500, Math.round(availableScreenWidth * 0.88)))
        readonly property int preferredHeight: Math.max(minimumHeight,
                                                        Math.min(900, Math.round(availableScreenHeight * 0.88)))
        width: preferredWidth
        height: preferredHeight
        minimumWidth: 800
        minimumHeight: 560
        visible: false
        // Setup must stay above the Decodium main window, but it must not stay
        // above unrelated desktop applications (for example XFCE's screenshot
        // tool).  The transient relationship gives the required application
        // stacking without a global always-on-top window flag.
        transientParent: mainWindow
        flags: Qt.Dialog | Qt.FramelessWindowHint
        title: qsTr("Settings - Decodium")
        color: "transparent"

        // The settings dialog is hosted in its own top-level Window.  Keep
        // the Material palette here as well as on mainWindow: without it,
        // buttons that do not provide a custom background fall back to the
        // platform's light button (white on the Darkcodium theme), making
        // their label and enabled/disabled state hard to read.
        Material.theme: bridge.themeManager.isLightTheme ? Material.Light : Material.Dark
        Material.accent: bridge.themeManager.primaryColor
        Material.primary: bridge.themeManager.secondaryColor
        Material.foreground: bridge.themeManager.textPrimary
        Material.background: bridge.themeManager.bgDeep

        x: mainWindow.x + Math.max(24, Math.round((mainWindow.width - width) / 2))
        y: mainWindow.y + Math.max(48, Math.round((mainWindow.height - height) / 2))

        function beginDesktopMove() {
            if (desktopMoveActive)
                return
            desktopMoveActive = true
            mainWindow.beginFloatingGeometryInteraction()
        }

        function finishDesktopMove() {
            if (!desktopMoveActive)
                return
            mainWindow.finishFloatingWindowDrag(settingsFloatingWindow)
            desktopMoveActive = false
            mainWindow.endFloatingGeometryInteraction()
        }

        function fitToCurrentScreen() {
            var target = mainWindow.geometryForWindowScreen(settingsFloatingWindow)
            if (!target)
                return

            mainWindow.fitWindowSizeToGeometry(settingsFloatingWindow, target)
            if (target.screen && settingsFloatingWindow.screen !== target.screen)
                settingsFloatingWindow.screen = target.screen
            settingsFloatingWindow.x = Math.max(
                        target.x,
                        Math.min(settingsFloatingWindow.x,
                                 target.x + Math.max(0, target.width - settingsFloatingWindow.width)))
            settingsFloatingWindow.y = Math.max(
                        target.y,
                        Math.min(settingsFloatingWindow.y,
                                 target.y + Math.max(0, target.height - settingsFloatingWindow.height)))
        }

        function showHostedWindow(tabIndex) {
            requestedTab = Number(tabIndex)
            if (!isFinite(requestedTab))
                requestedTab = -1
            settingsDialogLoader.active = true
            fitToCurrentScreen()
            show()
            raise()
            requestActivate()
            // Some window managers update Window.screen only after mapping the
            // native window. Re-apply the clamp on the next event-loop turn.
            Qt.callLater(settingsFloatingWindow.fitToCurrentScreen)
            if (settingsDialogLoader.item) {
                settingsDialogLoader.item.nativeHostWindow = settingsFloatingWindow
                if (requestedTab >= 0)
                    settingsDialogLoader.item.currentTab = Math.max(0, Math.min(13, Math.floor(requestedTab)))
                if (!settingsDialogLoader.item.visible)
                    settingsDialogLoader.item.open()
            }
        }

        function hideAfterDialogClosed() {
            hide()
            mainWindow.restoreTopmostPopoutsAfterSettings()
            mainWindow.scheduleWindowStateSave()
        }

        function hideHostedWindow() {
            if (settingsDialogLoader.item && settingsDialogLoader.item.visible) {
                settingsDialogLoader.item.close()
                return
            }
            hideAfterDialogClosed()
        }

        Component.onCompleted: mainWindow.restoreFloatingWindowState(
                                   settingsFloatingWindow,
                                   "settingsFloatingWindow",
                                   "",
                                   "")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onScreenChanged: Qt.callLater(settingsFloatingWindow.fitToCurrentScreen)
        onClosing: function(close) {
            if (!mainWindow.applicationClosing) {
                close.accepted = false
                hideHostedWindow()
                return
            }
            close.accepted = true
        }

        Loader {
            id: settingsDialogLoader
            anchors.fill: parent
            active: false
            asynchronous: true
            source: "components/SettingsDialog.qml"
            property var pendingAction: null
            onLoaded: {
                item.nativeHostWindow = settingsFloatingWindow
                console.log("Lazy component loaded: SettingsDialog tab=" + (item ? item.currentTab : -1))
                if (settingsFloatingWindow.requestedTab >= 0)
                    item.currentTab = Math.max(0, Math.min(13, Math.floor(settingsFloatingWindow.requestedTab)))
                if (settingsFloatingWindow.visible && !item.visible)
                    item.open()
                if (pendingAction) {
                    var action = pendingAction
                    pendingAction = null
                    action(item)
                }
            }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: settingsFloatingWindow
            maxWidth: 2400
            maxHeight: 1600
        }

        Shortcut {
            enabled: settingsFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: settingsFloatingWindow.hideHostedWindow()
        }
    }

    Connections {
        // The Settings dialog is lazily loaded, therefore connect through the
        // Loader item rather than coupling the dialog component to Main.qml.
        // Its Popup.closed signal also covers Escape, the header X and the
        // footer buttons.
        target: settingsDialogLoader.item
        ignoreUnknownSignals: true
        function onClosed() {
            settingsFloatingWindow.hideAfterDialogClosed()
        }
    }

    // Callsign Intelligence resta lazy: con l'impostazione predefinita OFF non
    // crea alcuna finestra quando cambia il DX call. Se l'utente abilita
    // l'apertura automatica, il servizio chiede questa finestra dopo aver
    // avviato lookup/cache e il contenuto segue il risultato in tempo reale.
    Loader {
        id: callsignLookupWindowLoader
        anchors.fill: parent
        active: false
        asynchronous: true
        source: "components/CallsignLookupWindow.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: CallsignLookupWindow")
            if (item)
                item.service = bridge ? bridge.callsignIntelligence : null
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    Connections {
        target: bridge && bridge.callsignIntelligence ? bridge.callsignIntelligence : null
        ignoreUnknownSignals: true
        function onLookupWindowRequested() {
            mainWindow.runWhenLoaded(callsignLookupWindowLoader, function(item) {
                if (item)
                    item.openForCall(bridge.callsignIntelligence.currentCall)
            })
        }
        function onLookupWindowCloseRequested() {
            if (callsignLookupWindowLoader.item)
                callsignLookupWindowLoader.item.close()
        }
    }

    Connections {
        target: settingsDialogLoader.item
        ignoreUnknownSignals: true
        function onFullScreenRequested() { mainWindow.toggleFullScreen() }
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
    // 1.0.233 — DevOverlay toggle (Sprint 2 Phase 7 perf roadmap).
    Shortcut {
        sequences: ["Ctrl+Shift+F"]
        context: Qt.ApplicationShortcut
        onActivated: {
            if (bridge) bridge.devOverlayActive = !bridge.devOverlayActive
        }
    }
    // 1.0.264 (fork-only) — Reset Layout (recupera finestre fuori monitor)
    Shortcut {
        sequence: "Ctrl+Shift+L"
        context: Qt.ApplicationShortcut
        onActivated: { mainWindow.resetClassicColumnOrder(); if (bridge) bridge.resetWindowLayout() }
    }
    // 1.0.268 (Phase 5.3) — apre Decode History Dialog
    Shortcut {
        sequence: "Ctrl+Shift+H"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.openHistoryDialog()
    }

    // 1.0.308 (#10) — scorciatoie operative: erano documentate nell'Info dialog (KEYBOARD
    // SHORTCUTS) ma NON implementate → il tester le premeva e "non funzionavano". Ora reali.
    Shortcut {
        sequence: "F1"
        context: Qt.ApplicationShortcut
        onActivated: { if (bridge) { bridge.monitoring ? bridge.stopMonitor() : bridge.startMonitor() } }
    }
    Shortcut {
        sequence: "F3"
        context: Qt.ApplicationShortcut
        onActivated: { if (bridge) bridge.autoSeq = !bridge.autoSeq }
    }
    Shortcut {
        sequence: "F4"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!bridge)
                return
            if (bridge.requestManualLogQso)
                bridge.requestManualLogQso()
            else
                bridge.promptLogQso()
        }
    }
    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: { if (bridge) bridge.halt() }
    }
    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.openSettingsDialog()
    }
    Shortcut {
        sequence: "Ctrl+L"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.openLogWindow()
    }
    Shortcut {
        sequence: "Ctrl+M"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.openMacroDialog()
    }

    // 1.0.233 — DevOverlay floating panel (async Loader, zero overhead
    // quando bridge.devOverlayActive == false).
    Loader {
        id: devOverlayLoader
        active: bridge && bridge.devOverlayActive
        asynchronous: true
        source: "components/DevOverlay.qml"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.rightMargin: 12
        z: 9999
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
            var title = prefixMatch ? prefixMatch[1].trim() : "Error"
            var summary = prefixMatch ? prefixMatch[2].trim() : String(msg)
            mainWindow.openWarningDialog(title, summary, "")
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
            mainWindow.openWarningDialog(title, summary, details)
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

    function warningLooksLikeCatFailure(title, summary, details) {
        var lower = (String(title) + " " + String(summary) + " " + String(details)).toLowerCase()
        return lower.indexOf("cat failure") >= 0
                || lower.indexOf("comunicazione cat") >= 0
                || (lower.indexOf("hamlib") >= 0
                    && (lower.indexOf("communication") >= 0
                        || lower.indexOf("timed out") >= 0
                        || lower.indexOf("bus error") >= 0))
    }

    function openWarningDialog(title, summary, details) {
        var safeDetails = details || ""
        if (warningLooksLikeCatFailure(title, summary, safeDetails)) {
            if (catFailureDialogShown)
                return
            catFailureDialogShown = true
        }
        warningDialogTitle = title || "Error"
        warningDialogSummary = summary || ""
        warningDialogDetails = safeDetails
        warningDialogDetailsVisible = false
        if (warningDialog.visible)
            warningDialog.close()
        warningDialog.open()
    }

    Dialog {
        id: warningDialog
        modal: true
        width: Math.max(420, Math.min(parent ? parent.width - 48 : 640, 760))
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
                        text: qsTr("Decodium reported a non-blocking problem.")
                        font.pixelSize: 11
                        color: textSecondary
                        elide: Text.ElideRight
                        width: Math.max(180, warningDialog.width - 96)
                    }
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: warningDialogSummary
                font.pixelSize: 15
                color: textPrimary
                wrapMode: Text.WordWrap
            }

            Button {
                visible: warningDialogDetails.length > 0
                text: warningDialogDetailsVisible ? qsTr("Hide details") : qsTr("Show details")
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

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: glassBorder
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                Button {
                    text: "OK"
                    Layout.preferredWidth: 128
                    Layout.preferredHeight: 40
                    onClicked: warningDialog.close()

                    contentItem: Text {
                        text: parent.text
                        color: textPrimary
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.22)
                                              : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
                        border.color: secondaryCyan
                        border.width: 1
                        radius: 8
                    }
                }
            }
        }
    }

    // Info Dialog
    Loader {
        id: infoDialogLoader
        anchors.fill: parent
        active: false
        asynchronous: true
        source: "components/InfoDialog.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: InfoDialog")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    // MAM Window - Multi-Answer Mode.  Use a desktop Window so its position is
    // global and it can be moved to any monitor on every supported platform.
    Window {
        id: mamFloatingWindow
        width: 700
        height: 450
        minimumWidth: 500
        minimumHeight: 360
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("Multi-Answer Mode - Decodium")
        color: "transparent"

        x: mainWindow.x + Math.max(24, Math.round((mainWindow.width - width) / 2))
        y: mainWindow.y + Math.max(48, Math.round((mainWindow.height - height) / 2))

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(mamFloatingWindow)
        }

        function showHostedWindow() {
            mamWindowLoader.active = true
            show()
            raise()
            requestActivate()
            if (mamWindowLoader.item) {
                mamWindowLoader.item.engine = bridge
                mamWindowLoader.item.nativeHostWindow = mamFloatingWindow
                if (!mamWindowLoader.item.visible)
                    mamWindowLoader.item.open()
            }
        }

        function hideAfterDialogClosed() {
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        function hideHostedWindow() {
            if (mamWindowLoader.item && mamWindowLoader.item.visible) {
                mamWindowLoader.item.close()
                return
            }
            hideAfterDialogClosed()
        }

        Component.onCompleted: mainWindow.restoreFloatingWindowState(
                                   mamFloatingWindow,
                                   "mamFloatingWindow",
                                   "",
                                   "")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onClosing: function(close) {
            if (!mainWindow.applicationClosing) {
                close.accepted = false
                hideHostedWindow()
                return
            }
            close.accepted = true
        }

        Loader {
            id: mamWindowLoader
            anchors.fill: parent
            active: false
            asynchronous: true
            source: "components/MamWindow.qml"
            property var pendingAction: null
            onLoaded: {
                item.engine = bridge
                item.nativeHostWindow = mamFloatingWindow
                console.log("Lazy component loaded: MamWindow")
                if (mamFloatingWindow.visible && !item.visible)
                    item.open()
                if (pendingAction) {
                    var action = pendingAction
                    pendingAction = null
                    action(item)
                }
            }
        }

        Connections {
            target: mamWindowLoader.item
            ignoreUnknownSignals: true
            function onClosed() { mamFloatingWindow.hideAfterDialogClosed() }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: mamFloatingWindow
            maxWidth: 1400
            maxHeight: 1000
        }

        Shortcut {
            enabled: mamFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: mamFloatingWindow.hideHostedWindow()
        }
    }

    // DECOMETER uses a real desktop window so its RF face can be placed on a
    // dedicated external display without changing the CAT/telemetry path.
    Window {
        id: decometerFloatingWindow
        property bool contentRequested: false
        property bool showPending: false
        readonly property real faceAspectRatio: 15 / 7
        readonly property int screenLimitedMaximumWidth: {
            if (!screen || !screen.availableGeometry)
                return maximumWidth
            var usableWidth = Math.max(minimumWidth,
                                       Number(screen.availableGeometry.width) - 24)
            var usableHeightAsWidth = Math.max(minimumWidth,
                                               (Number(screen.availableGeometry.height) - 24)
                                               * faceAspectRatio)
            return Math.round(Math.min(maximumWidth, usableWidth, usableHeightAsWidth))
        }
        width: 900
        height: 420
        minimumWidth: 450
        minimumHeight: 210
        maximumWidth: 1800
        maximumHeight: 840
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("DECØMETER - RF Meter - Decodium")
        color: "transparent"

        // Do not bind the desktop position to width/height.  A native move does
        // not necessarily remove a QML binding, so the old centred bindings
        // could move the instrument back whenever its proportional size was
        // normalised after a drag or a resize.
        x: 0
        y: 0

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(decometerFloatingWindow, faceAspectRatio)
            setProportionalWidth(width)
            mainWindow.finishFloatingWindowDrag(decometerFloatingWindow, faceAspectRatio)
        }

        function setProportionalWidth(requestedWidth) {
            var boundedWidth = Math.max(minimumWidth,
                                        Math.min(screenLimitedMaximumWidth,
                                                 Math.round(requestedWidth)))
            width = boundedWidth
            height = Math.round(boundedWidth / faceAspectRatio)
        }

        function normalizeProportionalSize() {
            var restoredWidth = Number(width)
            var restoredHeight = Number(height)
            if (!isFinite(restoredWidth) || restoredWidth <= 0
                    || !isFinite(restoredHeight) || restoredHeight <= 0) {
                setProportionalWidth(900)
                return
            }
            // Fit the old rectangle inside the 15:7 face.  This also migrates
            // the obsolete 924x444 host-with-gutter geometry without bars.
            var scale = Math.min(restoredWidth / 900, restoredHeight / 420)
            setProportionalWidth(900 * scale)
        }

        function presentHostedWindow() {
            normalizeProportionalSize()
            show()
            raise()
            requestActivate()
            if (decometerFloatingLoader.item)
                decometerFloatingLoader.item.activateHostedPanel()
        }

        function showHostedWindow() {
            // Build the relatively complex Canvas face while this native
            // window is still hidden.  Exposing the window first and then
            // attaching an asynchronously-created scene graph can race the
            // Qt 6.11 Metal render thread (the crash is in
            // QSGThreadedRenderLoop::update).  The Loader completes this
            // request from onLoaded.
            contentRequested = true
            if (!decometerFloatingLoader.item) {
                showPending = true
                return
            }
            showPending = false
            presentHostedWindow()
        }

        function hideHostedContent() {
            showPending = false
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        function hideHostedWindow() {
            hideHostedContent()
        }

        Component.onCompleted: {
            var restoredState = mainWindow.restoreFloatingWindowState(decometerFloatingWindow,
                                                                       "decometerFloatingWindow",
                                                                       "",
                                                                       "")
            normalizeProportionalSize()
            // A first launch has no saved geometry.  Centre it once, but leave
            // x/y as ordinary values afterwards so every edge and corner of the
            // active screen remains reachable.
            if (!isFinite(Number(restoredState.x)) || !isFinite(Number(restoredState.y))) {
                x = mainWindow.x + Math.max(24, Math.round((mainWindow.width - width) / 2))
                y = mainWindow.y + Math.max(48, Math.round((mainWindow.height - height) / 2))
                mainWindow.finishFloatingWindowDrag(decometerFloatingWindow, faceAspectRatio)
            }
        }
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: {
            mainWindow.scheduleWindowStateSave()
            if (decometerResizeHandles.nativeResizeActive)
                decometerResizeHandles.constrainNativeAspect()
        }
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onClosing: function(close) {
            if (!mainWindow.applicationClosing) {
                close.accepted = false
                hideHostedWindow()
                return
            }
            close.accepted = true
        }

        Loader {
            id: decometerFloatingLoader
            anchors.fill: parent
            active: decometerFloatingWindow.contentRequested
            asynchronous: true
            source: "components/DecometerWindow.qml"
            onLoaded: {
                item.nativeHostWindow = decometerFloatingWindow
                if (decometerFloatingWindow.showPending) {
                    decometerFloatingWindow.showPending = false
                    decometerFloatingWindow.presentHostedWindow()
                }
            }
        }

        ProportionalResizeHandles {
            id: decometerResizeHandles
            // The face is now a normal Loader item, so z-ordering is sufficient
            // and no cross-overlay reparenting is required.
            targetWindow: decometerFloatingWindow
            aspectRatio: decometerFloatingWindow.faceAspectRatio
            minWidth: decometerFloatingWindow.minimumWidth
            maxWidth: decometerFloatingWindow.screenLimitedMaximumWidth
        }

        Shortcut {
            enabled: decometerFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: decometerFloatingWindow.hideHostedWindow()
        }
    }

    // 1.0.262 — CALL Dialog (chiamata diretta a target callsign con retry/timeout)
    Loader {
        id: callDialogLoader
        anchors.fill: parent
        active: false
        asynchronous: true
        source: "components/CallDialog.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: CallDialog")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    // 1.0.268 (Phase 5.3) — Decode History Dialog (esplora DB SQLite)
    Loader {
        id: historyDialogLoader
        anchors.fill: parent
        active: false
        asynchronous: true
        source: "components/DecodeHistoryDialog.qml"
        property var pendingAction: null
        onLoaded: {
            console.log("Lazy component loaded: DecodeHistoryDialog")
            if (pendingAction) {
                var action = pendingAction
                pendingAction = null
                action(item)
            }
        }
    }

    // Auto-open MAM window when MAM mode is enabled
    Connections {
        target: bridge
        function onMultiAnswerModeChanged() {
            if (bridge.multiAnswerMode) {
                mainWindow.openMamWindow()
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
    Timer { id: statusToastHideTimer; interval: 5000; onTriggered: statusToastVisible = false }  // 1.0.308 (#7): più tempo per notarlo

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
            text: qsTr("SSTV - image radio... (BETA)")
            icon.source: ""
            enabled: bridge.sstvAvailable
            onTriggered: mainWindow.openSstvWorkspace()

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: parent.enabled ? secondaryCyan : textSecondary
                leftPadding: 10
            }
        }

        // RTTY: la finestra. Le bande e le frequenze NON stanno qui — RTTY e'
        // un modo come gli altri e si sceglie dal selettore dei modi, dove
        // sceglierlo commuta la radio. Un secondo elenco di bande in un menu
        // a parte sarebbe una seconda strada per fare la stessa cosa, e le due
        // prima o poi direbbero cose diverse.
        MenuItem {
            text: qsTr("Open the RTTY window...")
            icon.source: ""
            onTriggered: mainWindow.openRttyWindow()

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
            // 1.0.571 - DecoPort: pubblica la radio in rete e usa quelle degli
            // altri. La finestra e' un Loader: chi non la apre non paga nulla.
            text: qsTr("DecoPort - radio on the network...")
            icon.source: ""
            onTriggered: mainWindow.openDecoPortWindow()

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
            text: qsTr("About Decodium")
            icon.source: ""
            onTriggered: mainWindow.openInfoDialog(0)

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
            onTriggered: mainWindow.openInfoDialog(4)

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
            text: "📡 DECØMETER — " + qsTr("RF Meter...")
            onTriggered: mainWindow.openDecometerWindow()

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
            text: qsTr("MAM Window...")
            onTriggered: mainWindow.openMamWindow()

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
            text: "📂 " + qsTr("Open ALL.TXT folder")
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

        // IU8LMC: punto di ingresso all'assistenza (prima il dialog era irraggiungibile).
        MenuItem {
            text: "🛟 " + qsTr("Report a problem...")
            onTriggered: openBugReportDialog()
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12; color: textSecondary; leftPadding: 10
            }
        }

        // IU8LMC: era enabled:false ("Update checks disabled") perche' il checker
        // del bridge e' spento dalla 1.0.62. Ora usa DecodiumUpdater, che avvisa
        // davvero e sa scaricare e installare.
        MenuItem {
            enabled: !updater.busy
            text: "☁ " + (updater.busy ? qsTr("Checking...") : qsTr("Check for updates..."))
            onTriggered: {
                updater.check(false)   // manuale: rispondi anche se non c'e' nulla di nuovo
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
            text: "📂 " + qsTr("Esporta Cabrillo...")
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
		            text: (worldClock.showWorldClock ? "✓ " : "☐ ") + qsTr("Show clock")
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
		                text: uiLanguage === "lv" ? "✓ Latviešu" : "☐ Latviešu"
		                onTriggered: mainWindow.setUiLanguage("lv")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "lv" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "nl" ? "✓ Nederlands" : "☐ Nederlands"
		                onTriggered: mainWindow.setUiLanguage("nl")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "nl" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "ru" ? "✓ Русский" : "☐ Русский"
		                onTriggered: mainWindow.setUiLanguage("ru")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "ru" ? successGreen : textSecondary; leftPadding: 10 }
		            }

		            MenuItem {
		                text: uiLanguage === "ro" ? "✓ Română" : "☐ Română"
		                onTriggered: mainWindow.setUiLanguage("ro")
		                background: Rectangle { color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"; radius: 6 }
		                contentItem: Text { text: parent.text; font.pixelSize: 12; color: uiLanguage === "ro" ? successGreen : textSecondary; leftPadding: 10 }
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
            text: (waterfallPanelVisible ? "✓ " : "☐ ") + qsTr("Waterfall / Panadapter")
            onTriggered: waterfallPanelVisible = !waterfallPanelVisible
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: waterfallPanelVisible ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: (liveMapPanelVisible ? "✓ " : "☐ ") + qsTr("Live Map")
            onTriggered: liveMapPanelVisible = !liveMapPanelVisible
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
        title: qsTr("Export Cabrillo")
        anchors.centerIn: parent
        width: 400
        modal: true

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan; border.width: 1; radius: 10
        }

        contentItem: Column {
            spacing: 12; padding: 16
            Text { text: qsTr("Output file path:"); font.pixelSize: 12; color: textPrimary }
            DecoTextField {
                id: cabrilloPath
                width: 360
                text: (Qt.platform.os === "windows"
                       ? "C:/Users/IU8LMC/Documents/" : "~/")
                      + bridge.callsign + "_" + Qt.formatDate(new Date(), "yyyyMMdd") + ".cbr"
                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                color: textPrimary
                background: Rectangle {
                    color: Qt.rgba(textPrimary.r,textPrimary.g,textPrimary.b,0.07); border.color: glassBorder; radius: 4
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Export")
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
                text: qsTr("Watchdog Mode")
                font.pixelSize: 14
                font.bold: true
                color: textPrimary
            }

            Row {
                spacing: 10
                RadioButton {
                    id: wdOff
                    text: qsTr("Off")
                    checked: bridge.txWatchdogMode === 0
                    onClicked: bridge.txWatchdogMode = 0
                }
                RadioButton {
                    id: wdTime
                    text: qsTr("Time")
                    checked: bridge.txWatchdogMode === 1
                    onClicked: bridge.txWatchdogMode = 1
                }
                RadioButton {
                    id: wdCount
                    text: qsTr("Count")
                    checked: bridge.txWatchdogMode === 2
                    onClicked: bridge.txWatchdogMode = 2
                }
            }

            Row {
                spacing: 10
                visible: bridge.txWatchdogMode === 1
                Text { text: qsTr("Time (min):"); color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    from: 1; to: 999
                    value: bridge.txWatchdogTime
                    onValueChanged: if (bridge.txWatchdogTime !== value) bridge.txWatchdogTime = value
                }
            }

            Row {
                spacing: 10
                visible: bridge.txWatchdogMode === 2
                Text { text: qsTr("Max TX:"); color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    from: 1; to: 50
                    value: bridge.txWatchdogCount
                    onValueChanged: if (bridge.txWatchdogCount !== value) bridge.txWatchdogCount = value
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
                text: qsTr("Contest Type")
                font.pixelSize: 14
                font.bold: true
                color: textPrimary
            }

            DecoComboBox {
                id: contestTypeCombo
                width: 300
                model: bridge.contestTypeNames
                currentIndex: bridge.contestType
                onCurrentIndexChanged: bridge.contestType = currentIndex
            }

            // 1.0.441 - regola award selezionato (documenta lo scoring; nessun contatore)
            Text {
                text: qsTr("Rule: 1 point per worked station")
                font.pixelSize: 12
                color: secondaryCyan
                wrapMode: Text.WordWrap
                width: 300
                visible: bridge.contestTypeNames[bridge.contestType] === "Ft2.it Award 2026"
            }

            Text {
                text: qsTr("Exchange")
                font.pixelSize: 14
                font.bold: true
                color: textPrimary
                visible: bridge.contestType > 0
            }

            DecoTextField {
                width: 300
                text: bridge.contestExchange
                onTextChanged: bridge.contestExchange = text
                placeholderText: qsTr("Example: 599 001")
                visible: bridge.contestType > 0
            }

            Row {
                spacing: 10
                visible: bridge.contestType > 0
                Text { text: qsTr("Serial Number:"); color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
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
                text: qsTr("TRANSMISSION")
                font.pixelSize: 14
                font.bold: true
                color: accentOrange
            }
            Text { text: qsTr("F1 - F7: Select TX1 - TX7"); font.pixelSize: 12; color: textPrimary }
            Text { text: qsTr("F9: toggle RX-only 1st/2nd"); font.pixelSize: 12; color: textPrimary }
            Text { text: qsTr("Escape: Halt (immediate TX stop)"); font.pixelSize: 12; color: textPrimary }

            Rectangle { height: 1; width: parent.width - 40; color: glassBorder }

            Text {
                text: qsTr("CONTROLS (Ctrl+)")
                font.pixelSize: 14
                font.bold: true
                color: secondaryCyan
            }
            Text { text: qsTr("Ctrl+A: toggle Auto Sequence"); font.pixelSize: 12; color: textPrimary }
            Text { text: qsTr("Ctrl+G: Generate all TX messages"); font.pixelSize: 12; color: textPrimary }
            Text { text: qsTr("Ctrl+Z: toggle ZAP mode"); font.pixelSize: 12; color: textPrimary }

            Rectangle { height: 1; width: parent.width - 40; color: glassBorder }

            Text {
                text: qsTr("ACTIONS (Alt+)")
                font.pixelSize: 14
                font.bold: true
                color: successGreen
            }
            Text { text: qsTr("Alt+L: log the current QSO"); font.pixelSize: 12; color: textPrimary }
            Text { text: qsTr("Alt+M: clear the decode list (Monitor)"); font.pixelSize: 12; color: textPrimary }
            Text { text: qsTr("Alt+S: stop TX"); font.pixelSize: 12; color: textPrimary }
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
        title: qsTr("Waterfall - Decodium")
        color: "transparent"

        // 1.0.286 — RIMOSSO il DragHandler root: era figlio del Window (non
        // dell'header), quindi trascinava la finestra da QUALSIASI punto del corpo
        // (bug: perdendo il pollice di uno Slider partiva startSystemMove e la
        // finestra si muoveva). Il move-to-window resta SOLO sul MouseArea header
        // 'dragArea' (sotto), header-only come tutte le altre pop-out, e mantiene il
        // dock magnetico (che startSystemMove invece bypassava).

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
                if (!mainWindow.applicationClosing) {
                    mainWindow.waterfallPanelVisible = false
                    mainWindow.waterfallDetached = false
                    mainWindow.waterfallMinimized = false
                }
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

            // ===== RESIZE HANDLES =====
            // 1.0.288 — sostituiti gli 8 MouseArea inline (che facevano
            // waterfallWindow.width= da onMouseXChanged) con il componente
            // condiviso FloatingResizeHandles (z:1000, onPositionChanged), lo
            // stesso usato dalla Full Spectrum. Gli handle inline stavano SOTTO
            // l'header dragArea (dichiarati prima del ColumnLayout) e in frameless
            // ON (senza Qt.WindowStaysOnTopHint) perdevano il grab ai bordi →
            // resize bloccato + "barra superiore bloccata". FloatingResizeHandles
            // sta sopra l'header e funziona in entrambe le modalità frameless.
            FloatingResizeHandles {
                targetWindow: waterfallWindow
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
                            font.family: decodiumMonoFontFamily
                            color: textSecondary
                        }

                        Item { Layout.fillWidth: true }

                        // Frequency controls
                        RowLayout {
                            spacing: 8

                            Text {
                                text: qsTr("Zoom:")
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
                                      : "200-3200 Hz"
                                font.pixelSize: 10
                                font.family: decodiumMonoFontFamily
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
	                            ToolTip.text: qsTr("Minimize")
                            ToolTip.delay: 500
                        }

                    }
                }

                Loader {
                    id: waterfallDetachedLoader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: waterfallWindow.visible && mainWindow.waterfallPanelVisible
                    asynchronous: true
                    sourceComponent: waterfallDetachedComponent
                }

                Component {
                    id: waterfallDetachedComponent

                    Waterfall {
                        id: waterfallDisplayDetached
                        visible: mainWindow.waterfallPanelVisible && waterfallDetached
                        showControls: true
                        minFreq: 200
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
                            text: "Mode: " + bridge.mode + (mainWindow.dxpeditionModeLabel ? " · " + mainWindow.dxpeditionModeLabel : "")
                            font.pixelSize: 11
                            color: secondaryCyan
                        }

                        Text {
                            text: "Freq: " + (bridge.frequency / 1000000).toFixed(6) + " MHz"
                            font.pixelSize: 11
                            font.family: decodiumMonoFontFamily
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
                                running: bridge.monitoring && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"
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
        property bool contentRequested: false
        width: 960
        height: 720
        minimumWidth: 600
        minimumHeight: 400
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("QSO Log - Decodium")
        color: "transparent"

        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 50
        Component.onCompleted: mainWindow.restoreFloatingWindowState(logFloatingWindow, "logFloatingWindow", "logWindowDetached", "logWindowMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibleChanged: {
            if (visible) {
                contentRequested = true
                if (logNativePopupLoader.item && !logNativePopupLoader.item.visible)
                    logNativePopupLoader.item.open()
            }
        }

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(logFloatingWindow)
        }

        function showHostedWindow() {
            contentRequested = true
            show()
            raise()
            requestActivate()
            if (logNativePopupLoader.item && !logNativePopupLoader.item.visible)
                logNativePopupLoader.item.open()
        }

        function hideAfterDialogClosed() {
            logWindowDetached = false
            logWindowMinimized = false
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        function hideHostedWindow() {
            if (logNativePopupLoader.item && logNativePopupLoader.item.visible) {
                logNativePopupLoader.item.close()
                return
            }
            hideAfterDialogClosed()
        }

        function minimizeHostedWindow() {
            logWindowMinimized = true
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        onClosing: function(close) {
            if (!mainWindow.applicationClosing) {
                close.accepted = false
                hideHostedWindow()
                return
            }
            close.accepted = true
        }

        Rectangle {
            // Legacy duplicate shell retained inert during the transition to
            // the complete LogWindow popup hosted below.
            visible: false
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
                        z: 2
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.rightMargin: 88
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        property point pressGlobalPos: Qt.point(0, 0)
                        property point pressWindowPos: Qt.point(0, 0)
                        property bool nativeMoveActive: false
                        cursorShape: Qt.SizeAllCursor
                        onPressed: function(mouse) {
                            pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                            pressWindowPos = Qt.point(logFloatingWindow.x, logFloatingWindow.y)
                            nativeMoveActive = mainWindow.startNativeFloatingWindowMove(logFloatingWindow)
                            mouse.accepted = true
                        }
                        onPositionChanged: function(mouse) {
                            if (!pressed || nativeMoveActive)
                                return
                            mainWindow.dragFloatingWindowToGlobal(logFloatingWindow,
                                                                  pressWindowPos,
                                                                  pressGlobalPos,
                                                                  mapToGlobal(mouse.x, mouse.y))
                            mouse.accepted = true
                        }
                        onReleased: {
                            nativeMoveActive = false
                            mainWindow.finishFloatingWindowDrag(logFloatingWindow)
                        }
                        onCanceled: {
                            nativeMoveActive = false
                            mainWindow.finishFloatingWindowDrag(logFloatingWindow)
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        Text { text: "⋮⋮"; font.pixelSize: 14; color: textSecondary }
                        Text { text: qsTr("📋 QSO Log"); font.pixelSize: 16; font.bold: true; color: secondaryCyan }
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
                    active: false
                    asynchronous: true
                    sourceComponent: logContentComponent
                }
            }
        }

        Loader {
            id: logNativePopupLoader
            anchors.fill: parent
            active: logFloatingWindow.contentRequested
            asynchronous: true
            source: "components/LogWindow.qml"
            onLoaded: {
                item.nativeHostWindow = logFloatingWindow
                if (logFloatingWindow.visible && !item.visible)
                    item.open()
            }
        }

        Connections {
            target: logNativePopupLoader.item
            ignoreUnknownSignals: true
            function onClosed() { logFloatingWindow.hideAfterDialogClosed() }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: logFloatingWindow
            maxWidth: 2200
            maxHeight: 1400
        }

        Shortcut {
            enabled: logFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: logFloatingWindow.hideHostedWindow()
        }
    }

    // Log content component (shared)
    Component {
        id: logContentComponent
        LogWindowContent {
            refreshActive: logFloatingWindow.visible
        }
    }

    // ========== NATIVE ASTRONOMICAL DATA WINDOW ==========
    // The complete Astro dialog is hosted by a real top-level Window.  Its
    // coordinates are desktop-global, so the system move operation can cross
    // monitor boundaries and WindowState can restore the same display later.
    Window {
        id: astroFloatingWindow
        property bool contentRequested: false
        width: 680
        height: 780
        minimumWidth: 520
        minimumHeight: 520
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("Astronomical Data - Decodium")
        color: "transparent"

        x: mainWindow.x + Math.max(24, Math.round((mainWindow.width - width) / 2))
        y: mainWindow.y + Math.max(48, Math.round((mainWindow.height - height) / 2))

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(astroFloatingWindow)
        }

        function showHostedWindow() {
            contentRequested = true
            show()
            raise()
            requestActivate()
            if (astroFloatingLoader.item && !astroFloatingLoader.item.visible)
                astroFloatingLoader.item.open()
        }

        function hideHostedWindow() {
            astroWindowDetached = false
            astroWindowMinimized = false
            mainWindow.scheduleWindowStateSave()
            hide()
        }

        function minimizeHostedWindow() {
            astroWindowMinimized = true
            mainWindow.scheduleWindowStateSave()
            astroFloatingWindow.hide()
        }

        Component.onCompleted: mainWindow.restoreFloatingWindowState(
                                   astroFloatingWindow,
                                   "astroFloatingWindow",
                                   "astroWindowDetached",
                                   "astroWindowMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibleChanged: {
            if (visible) {
                contentRequested = true
                if (astroFloatingLoader.item && !astroFloatingLoader.item.visible)
                    astroFloatingLoader.item.open()
            }
        }

        onClosing: function(close) {
            if (!mainWindow.applicationClosing) {
                astroWindowDetached = false
                astroWindowMinimized = false
            }
            mainWindow.scheduleWindowStateSave()
            close.accepted = true
        }

        Loader {
            id: astroFloatingLoader
            anchors.fill: parent
            active: astroFloatingWindow.contentRequested
            asynchronous: true
            source: "components/AstroWindow.qml"
            onLoaded: {
                item.nativeHostWindow = astroFloatingWindow
                item.open()
            }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: astroFloatingWindow
            maxWidth: 1600
            maxHeight: 1400
        }

        Shortcut {
            enabled: astroFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: astroFloatingWindow.hideHostedWindow()
        }
    }

    // ========== NATIVE SATELLITE TRACKING WINDOW ==========
    Window {
        id: satelliteFloatingWindow
        property bool contentRequested: false
        width: 880
        height: 740
        minimumWidth: 640
        minimumHeight: 560
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("Satellite tracking - Decodium")
        color: "transparent"

        x: mainWindow.x + Math.max(24, Math.round((mainWindow.width - width) / 2))
        y: mainWindow.y + Math.max(48, Math.round((mainWindow.height - height) / 2))

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(satelliteFloatingWindow)
        }

        function showHostedWindow() {
            contentRequested = true
            show()
            raise()
            requestActivate()
            if (satelliteFloatingLoader.item && !satelliteFloatingLoader.item.visible)
                satelliteFloatingLoader.item.open()
        }

        function hideHostedWindow() {
            satelliteWindowDetached = false
            mainWindow.scheduleWindowStateSave()
            hide()
        }

        Component.onCompleted: mainWindow.restoreFloatingWindowState(
                                   satelliteFloatingWindow,
                                   "satelliteFloatingWindow",
                                   "satelliteWindowDetached",
                                   "")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibleChanged: {
            if (visible) {
                contentRequested = true
                if (satelliteFloatingLoader.item && !satelliteFloatingLoader.item.visible)
                    satelliteFloatingLoader.item.open()
            }
        }
        onClosing: function(close) {
            if (!mainWindow.applicationClosing)
                satelliteWindowDetached = false
            mainWindow.scheduleWindowStateSave()
            close.accepted = true
        }

        Loader {
            id: satelliteFloatingLoader
            anchors.fill: parent
            active: satelliteFloatingWindow.contentRequested
            asynchronous: true
            source: "components/SatelliteWindow.qml"
            onLoaded: {
                item.nativeHostWindow = satelliteFloatingWindow
                item.open()
            }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: satelliteFloatingWindow
            maxWidth: 1800
            maxHeight: 1400
        }

        Shortcut {
            enabled: satelliteFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: satelliteFloatingWindow.hideHostedWindow()
        }
    }

    // ========== DETACHABLE MACRO WINDOW ==========
    Window {
        id: macroFloatingWindow
        property bool contentRequested: false
        width: 700
        height: 600
        minimumWidth: 500
        minimumHeight: 400
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("Macro - Decodium")
        color: "transparent"

        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 150
        Component.onCompleted: mainWindow.restoreFloatingWindowState(macroFloatingWindow, "macroFloatingWindow", "macroDialogDetached", "macroDialogMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibleChanged: {
            if (visible) {
                contentRequested = true
                if (macroNativeDialogLoader.item && !macroNativeDialogLoader.item.visible)
                    macroNativeDialogLoader.item.open()
            }
        }

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(macroFloatingWindow)
        }

        function showHostedWindow() {
            contentRequested = true
            show()
            raise()
            requestActivate()
            if (macroNativeDialogLoader.item && !macroNativeDialogLoader.item.visible)
                macroNativeDialogLoader.item.open()
        }

        function hideAfterDialogClosed() {
            macroDialogDetached = false
            macroDialogMinimized = false
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        function hideHostedWindow() {
            if (macroNativeDialogLoader.item && macroNativeDialogLoader.item.visible) {
                macroNativeDialogLoader.item.close()
                return
            }
            hideAfterDialogClosed()
        }

        function minimizeHostedWindow() {
            macroDialogMinimized = true
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        onClosing: function(close) {
            if (!mainWindow.applicationClosing) {
                close.accepted = false
                hideHostedWindow()
                return
            }
            close.accepted = true
        }

        Rectangle {
            // The old reduced floating editor is kept inert; the complete
            // MacroDialog is hosted below so no functionality is lost.
            visible: false
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
                        id: macroFloatDragArea
                        z: 2
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.rightMargin: 88
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        property point pressGlobalPos: Qt.point(0, 0)
                        property point pressWindowPos: Qt.point(0, 0)
                        property bool nativeMoveActive: false
                        cursorShape: Qt.SizeAllCursor
                        onPressed: function(mouse) {
                            pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                            pressWindowPos = Qt.point(macroFloatingWindow.x, macroFloatingWindow.y)
                            nativeMoveActive = mainWindow.startNativeFloatingWindowMove(macroFloatingWindow)
                            mouse.accepted = true
                        }
                        onPositionChanged: function(mouse) {
                            if (!pressed)
                                return
                            if (nativeMoveActive)
                                return
                            mainWindow.dragFloatingWindowToGlobal(macroFloatingWindow,
                                                                  pressWindowPos,
                                                                  pressGlobalPos,
                                                                  mapToGlobal(mouse.x, mouse.y))
                            mouse.accepted = true
                        }
                        onReleased: {
                            nativeMoveActive = false
                            mainWindow.finishFloatingWindowDrag(macroFloatingWindow)
                        }
                        onCanceled: {
                            nativeMoveActive = false
                            mainWindow.finishFloatingWindowDrag(macroFloatingWindow)
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        Text { text: "⋮⋮"; font.pixelSize: 14; color: textSecondary }
                        Text { text: qsTr("⌨️ TX Macro Configuration"); font.pixelSize: 16; font.bold: true; color: secondaryCyan }
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
                    active: false
                    asynchronous: true
                    sourceComponent: macroContentComponent
                }
            }
        }

        Loader {
            id: macroNativeDialogLoader
            anchors.fill: parent
            active: macroFloatingWindow.contentRequested
            asynchronous: true
            source: "components/MacroDialog.qml"
            onLoaded: {
                item.nativeHostWindow = macroFloatingWindow
                if (macroFloatingWindow.visible && !item.visible)
                    item.open()
            }
        }

        Connections {
            target: macroNativeDialogLoader.item
            ignoreUnknownSignals: true
            function onClosed() { macroFloatingWindow.hideAfterDialogClosed() }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: macroFloatingWindow
            maxWidth: 1600
            maxHeight: 1200
        }

        Shortcut {
            enabled: macroFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: macroFloatingWindow.hideHostedWindow()
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
        title: qsTr("Rig Control - Decodium")
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
                        Text { text: qsTr("📻 Rig Control (CAT)"); font.pixelSize: 16; font.bold: true; color: secondaryCyan }
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
        title: qsTr("Live Map - Decodium")
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
            if (!mainWindow.applicationClosing) {
                mainWindow.liveMapPanelVisible = false
                mainWindow.liveMapDetached = false
                mainWindow.liveMapMinimized = false
                mainWindow.syncLiveMapFloatingVisibility(false)
            }
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
		        title: qsTr("Full Spectrum - Decodium")
	        color: "transparent"

	        // 1.0.286 — RIMOSSO il DragHandler root (stesso fix della Waterfall): era
	        // figlio del Window, trascinava la finestra da qualsiasi punto del corpo. Il
	        // move resta SOLO sull'header (MouseArea 'p1DragArea'), header-only come le
	        // altre pop-out, con dock magnetico (che startSystemMove bypassava).
	        readonly property bool compactColumns: width < 560
	        readonly property int utcColumnWidth: compactColumns ? 66 : 84
	        readonly property int dbColumnWidth: compactColumns ? 34 : 38
	        readonly property int dbDtGapWidth: compactColumns ? 4 : 6
	        readonly property int dtColumnWidth: compactColumns ? 42 : 48
	        readonly property int dtFreqGapWidth: compactColumns ? 6 : 8
	        readonly property int freqColumnWidth: compactColumns ? 42 : 46
	        readonly property int gapColumnWidth: compactColumns ? 8 : 12
	        readonly property int distanceColumnWidth: mainWindow.fullSpectrumShowDistColumn && !compactColumns ? 56 : 0
	        readonly property int dxccColumnWidth: compactColumns ? 108 : Math.min(300, Math.max(190, Math.round(width * 0.24)))
	        readonly property int azColumnWidth: mainWindow.fullSpectrumShowAzColumn ? (compactColumns ? 38 : 48) : 0

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
                var restoredState = mainWindow.restoreFloatingWindowState(period1FloatingWindow, "period1FloatingWindow", "period1Detached", "period1Minimized")
                if (mainWindow.ft2LinkModeActive) {
                    period1FloatingWindow.hide()
                    return
                }
                // 1.0.186 — Auto-detach Full Spectrum di default. Pasquale-pattern:
                // pop-out in Window separata -> render thread isolato -> niente stall
                // main-thread durante drain ListView / texture upload waterfall.
                // 1.0.201 — default ripristinato a true (1.0.197 upstream lo aveva
                // spento, causando regressione performance progressiva).
                // Disattivabile da Settings -> "Detach Full Spectrum".
                // Non sovrascrivere pero' una scelta utente gia' salvata:
                // se WindowState/period1FloatingWindow contiene "detached=false",
                // l'utente ha dockato/chiuso il pannello e al riavvio deve restare docked.
                var hasSavedDetachedChoice = restoredState && restoredState.detached !== undefined
                if (!mainWindow.period1Detached
                        && !hasSavedDetachedChoice
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
	                                text: decodePanel.displayedDecodeCount() +" " + qsTr("decodes")
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
			                                text: mainWindow.compactFullSpectrum ? "Full" : "Cmp"
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
		                                ? qsTr("Return to normal row height")
		                                : qsTr("Compact rows (2x visible decodes)")
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

                    Menu {
                        id: fsHeaderMenuFloating
                        property string targetId: ""
                        MenuItem { text: qsTr("◀  Move left"); enabled: mainWindow.fsCanMove(fsHeaderMenuFloating.targetId, -1); onTriggered: mainWindow.fsMoveColumn(fsHeaderMenuFloating.targetId, -1) }
                        MenuItem { text: qsTr("Move right  ▶"); enabled: mainWindow.fsCanMove(fsHeaderMenuFloating.targetId, 1); onTriggered: mainWindow.fsMoveColumn(fsHeaderMenuFloating.targetId, 1) }
                        MenuItem { text: qsTr("Hide this column"); enabled: mainWindow.fsColMeta(fsHeaderMenuFloating.targetId).canHide; onTriggered: mainWindow.fsSetColumnVisible(fsHeaderMenuFloating.targetId, false) }
                        MenuSeparator {}
                        MenuItem { text: (mainWindow.fsColVisible("utc")  ? "✓  " : "      ") + qsTr("UTC");     onTriggered: mainWindow.fsToggleColumnVisible("utc") }
                        MenuItem { text: (mainWindow.fsColVisible("db")   ? "✓  " : "      ") + qsTr("dB");      onTriggered: mainWindow.fsToggleColumnVisible("db") }
                        MenuItem { text: (mainWindow.fsColVisible("dt")   ? "✓  " : "      ") + qsTr("DT");      onTriggered: mainWindow.fsToggleColumnVisible("dt") }
                        MenuItem { text: (mainWindow.fsColVisible("freq") ? "✓  " : "      ") + qsTr("Freq");    onTriggered: mainWindow.fsToggleColumnVisible("freq") }
                        MenuItem { text: qsTr("✓  Message"); enabled: false }
                        MenuItem { text: (mainWindow.fsColVisible("dist") ? "✓  " : "      ") + qsTr("Dist");    onTriggered: mainWindow.fsToggleColumnVisible("dist") }
                        MenuItem { text: (mainWindow.fsColVisible("dxcc") ? "✓  " : "      ") + qsTr("DXCC");    enabled: mainWindow.showDxccInfo; onTriggered: mainWindow.fsToggleColumnVisible("dxcc") }
                        MenuItem { text: (mainWindow.fsColVisible("az")   ? "✓  " : "      ") + qsTr("Az");      enabled: mainWindow.showDxccInfo; onTriggered: mainWindow.fsToggleColumnVisible("az") }
                        MenuSeparator {}
                        MenuItem { text: (mainWindow.fsNewestFirst ? "✓  " : "      ") + qsTr("Newest on top"); onTriggered: mainWindow.fsToggleNewestFirst() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Reset columns"); onTriggered: mainWindow.fsResetColumns() }
                    }

                    RowLayout {
                        id: fsHeaderRowF
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Repeater {
                            model: mainWindow.fsColumnsForWidth(period1FloatingWindow.width)
                            delegate: Item {
                                id: fsHCellF
                                readonly property var col: modelData
                                readonly property var meta: mainWindow.fsColMeta(col.id)
                                Layout.fillWidth: meta.fill
                            Layout.preferredWidth: meta.fill ? -1 : mainWindow.fsColWidthForPanel(col.id, period1FloatingWindow.width)
                            Layout.minimumWidth: meta.fill ? meta.minW : mainWindow.fsColWidthForPanel(col.id, period1FloatingWindow.width)
                                Layout.fillHeight: true
                                Text {
                                    anchors.fill: parent
                                    anchors.rightMargin: fsHCellF.meta.fill ? 0 : 5
                                    text: fsHCellF.meta.label
                                    font.family: mainWindow.decodedTextFontFamily
                                    font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs)
                                    font.bold: true
                                    color: bridge.themeManager.successColor
                                    horizontalAlignment: fsHCellF.meta.align === "right" ? Text.AlignRight : Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                                // Linea divisoria/grip visibile sul bordo destro (zona di resize).
                                Rectangle {
                                    visible: !fsHCellF.meta.fill
                                    width: 1
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.topMargin: 3
                                    anchors.bottomMargin: 3
                                    color: Qt.rgba(bridge.themeManager.successColor.r, bridge.themeManager.successColor.g, bridge.themeManager.successColor.b, 0.45)
                                }
                            }
                        }
                    }

                    // Controller UNICO sopra le celle (floating): trascina l'etichetta per
                    // riordinare (live, fuori dal Repeater), il bordo destro per ridimensionare,
                    // tasto destro = menu colonne.
                    MouseArea {
                        id: fsHdrCtlF
                        anchors.fill: parent
                        z: 100
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        preventStealing: true
                        property string grabId: ""
                        property bool resizing: false
                        property bool reordering: false
                        property real pressSceneX: 0
                        property int pressW: 0
                        function cellAt(localX) {
                            var p = mapToItem(fsHeaderRowF, localX, fsHeaderRowF.height / 2)
                            return fsHeaderRowF.childAt(p.x, p.y)
                        }
                        function resizeTargetAt(localX) {
                            var p = mapToItem(fsHeaderRowF, localX, 0)
                            var kids = fsHeaderRowF.children
                            for (var i = 0; i < kids.length; ++i) {
                                var c = kids[i]
                                if (!c || !c.col || mainWindow.fsColMeta(c.col.id).fill) continue
                                if (Math.abs(p.x - (c.x + c.width)) <= 7) return c.col.id
                            }
                            return ""
                        }
                        cursorShape: {
                            if (resizing) return Qt.SplitHCursor
                            if (reordering) return Qt.ClosedHandCursor
                            return resizeTargetAt(mouseX) !== "" ? Qt.SplitHCursor : Qt.ArrowCursor
                        }
                        onPressed: function(m) {
                            pressSceneX = mapToItem(null, m.x, m.y).x
                            resizing = false; reordering = false
                            var rid = resizeTargetAt(m.x)
                            if (m.button === Qt.LeftButton && rid !== "") {
                                grabId = rid
                                resizing = true
                                pressW = mainWindow.fsColWidth(rid)
                            } else {
                                var c = cellAt(m.x)
                                grabId = (c && c.col) ? c.col.id : ""
                            }
                        }
                        onPositionChanged: function(m) {
                            if (!pressed || grabId === "") return
                            var sx = mapToItem(null, m.x, m.y).x
                            if (resizing) {
                                mainWindow.fsSetColumnWidth(grabId, pressW + (sx - pressSceneX))
                                return
                            }
                            if (!(pressedButtons & Qt.LeftButton)) return
                            if (!reordering && Math.abs(sx - pressSceneX) < 6) return
                            reordering = true
                            var c = cellAt(m.x)
                            var overId = (c && c.col) ? c.col.id : ""
                            if (overId !== "" && overId !== grabId)
                                mainWindow.fsMoveColumnToId(grabId, overId)
                        }
                        onReleased: function(m) {
                            if (resizing) mainWindow.fsPersistWidths()
                            else if (reordering) mainWindow.fsPersistOrder()
                            grabId = ""; resizing = false; reordering = false
                        }
                        onCanceled: { grabId = ""; resizing = false; reordering = false }
                        onClicked: function(m) {
                            if (m.button === Qt.RightButton) {
                                var c = cellAt(m.x)
                                if (c && c.col) { fsHeaderMenuFloating.targetId = c.col.id; fsHeaderMenuFloating.popup() }
                            }
                        }
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
                        model: period1Detached && period1FloatingWindow.visible
                               ? ((bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : decodePanel.allDecodes)
                               : null
                        cacheBuffer: 360  // 1.0.478 — meno delegate offscreen durante pile-up FT8/4/2
                        reuseItems: true
                        interactive: true
                        verticalLayoutDirection: mainWindow.fsNewestFirst ? ListView.BottomToTop : ListView.TopToBottom
                        property bool followTail: true
                        property bool tailFollowPending: false
	                        property bool tailFollowQueued: false
	                        // 1.0.231 — counter pending decodes (floating mode)
		                        property int pendingNewDecodes: 0
		                        function isNearTail() {
		                            if (contentHeight <= height + 2) return true
		                            if (mainWindow.fsNewestFirst) return contentY <= originY + 48
		                            return contentY >= tailContentY() - 48
		                        }
                        function updateFollowTail() {
                            if (tailFollowPending)
                                return
                            followTail = isNearTail()
                            if (followTail) period1FloatingList.pendingNewDecodes = 0
                        }
	                        function tailContentY() {
	                            if (mainWindow.fsNewestFirst) return originY
	                            var bottom = originY + contentHeight - height
	                            return Math.max(originY, bottom)
	                        }
	                        function finishTailFollow() {
	                            var shouldSnap = tailFollowPending || followTail
	                            tailFollowPending = false
	                            if (shouldSnap) {
	                                var targetY = tailContentY()
	                                if (Math.abs(contentY - targetY) > 0.5)
	                                    contentY = targetY
	                            }
	                            followTail = isNearTail()
	                            if (followTail) period1FloatingList.pendingNewDecodes = 0
                        }
                        function forceTailFollow() {
    period1FloatingList.followTail = true
    period1FloatingList.tailFollowPending = true
    if (period1FloatingList.tailFollowQueued)
        return
    period1FloatingList.tailFollowQueued = true
    Qt.callLater(function() {
        period1FloatingList.tailFollowQueued = false
        if (!period1FloatingList)
            return
        var targetY = period1FloatingList.tailContentY()
        period1FloatingTailAnimation.stop()
        period1FloatingList.tailFollowPending = true
        period1FloatingList.contentY = targetY
        period1FloatingList.finishTailFollow()
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
	                            if (decodePanel.hasNativeBandActivityModel()) return
                            // 1.0.231 — se user in scroll-back, counter ↓N
                            if (!followTail) {
                                period1FloatingList.pendingNewDecodes++
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
	                        // Preserve the fade/slide without starting render-thread Animators.
	                        add: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        addDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                        }
	                        moveDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                        }
	                        removeDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            interactive: true
                            width: 8
                        }

		                        delegate: Rectangle {
                            id: fsRowFloating
			                            width: parent ? parent.width : 100
		                            readonly property bool isPeriodSeparator: !!(modelData && modelData.isSeparator === true)
		                            readonly property var entry: modelData || ({})
		                            // 1.0.229 — height adattiva compact mode (vedi mainWindow.fullSpectrumRowHeight)
	                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(mainWindow.fullSpectrumRowHeight * fs)
		                            radius: 3
		                            property var highlightFill: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightFill(modelData)
		                            property var highlightBorder: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeHighlightBorder(modelData)
                                            property var userBgFill: (!modelData || isPeriodSeparator) ? null : mainWindow.decodeUserBgFill(modelData)
		                            // 1.0.205 — guard !modelData per evitare TypeError flood (~46/s) durante
		                            // model swap transients che saturava il main thread via logger sincrono.
		                            color: !modelData ? "transparent" :
		                                   isPeriodSeparator ? "transparent" :
			                                   userBgFill ? userBgFill :
                                                   highlightFill ? highlightFill :
				                                   entry.bgColorHex ? mainWindow.boostedDecodeBackgroundColor(entry.bgColorHex) :
				                                   (entry.isCQ && bridge.decodeColorEnabled("colorCQ")) ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15)) :
				                                   mainWindow.boostedDecodeBackgroundColor(Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05))
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
                                spacing: 6

                                Repeater {
                                    model: mainWindow.fsColumnsForWidth(period1FloatingWindow.width)
                                    delegate: Item {
                                        id: fsCellF
                                        readonly property var col: modelData
                                        readonly property var meta: mainWindow.fsColMeta(col.id)
                                        Layout.fillWidth: meta.fill
                                    Layout.preferredWidth: meta.fill ? -1 : mainWindow.fsColWidthForPanel(col.id, period1FloatingWindow.width)
                                    Layout.minimumWidth: meta.fill ? meta.minW : mainWindow.fsColWidthForPanel(col.id, period1FloatingWindow.width)
                                        Layout.fillHeight: true
                                        clip: col.id === "dxcc"
                                        Text {
                                            visible: fsCellF.col.id !== "dxcc"
                                            anchors.fill: parent
                                            text: mainWindow.fsCellText(fsRowFloating.entry, fsCellF.col.id)
                                            color: mainWindow.fsCellColor(fsRowFloating.entry, fsCellF.col.id)
                                            font.bold: mainWindow.fsCellBold(fsRowFloating.entry, fsCellF.col.id)
                                            font.strikeout: fsCellF.col.id === "msg" ? decodePanel.decodeEntryStrikeout(fsRowFloating.entry) : false
                                            font.family: mainWindow.decodedTextFontFamily
                                            font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs)
                                            horizontalAlignment: fsCellF.meta.align === "right" ? Text.AlignRight : Text.AlignLeft
                                            verticalAlignment: Text.AlignVCenter
                                            maximumLineCount: 1
                                            elide: fsCellF.col.id === "msg" ? messageElideMode(fsRowFloating.entry.displayMessage || fsRowFloating.entry.message)
                                                 : (fsCellF.col.id === "dxcc" ? Text.ElideRight : Text.ElideNone)
                                            fontSizeMode: fsCellF.col.id === "dxcc" ? Text.HorizontalFit : Text.FixedSize
                                            minimumPixelSize: fsCellF.col.id === "dxcc" ? Math.max(8, Math.round(mainWindow.decodedTextFontPixelSize * fs * 0.65)) : 0
                                        }
                                        Item {
                                            visible: fsCellF.col.id === "dxcc"
                                            anchors.fill: parent
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: fsRowFloating.entry.isLotw === true ? Math.max(9, Math.round(11 * fs)) : 0
                                                text: mainWindow.fsCellText(fsRowFloating.entry, fsCellF.col.id)
                                                color: mainWindow.fsCellColor(fsRowFloating.entry, fsCellF.col.id)
                                                font.family: mainWindow.decodedTextFontFamily
                                                font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs)
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                                maximumLineCount: 1
                                                elide: Text.ElideRight
                                                fontSizeMode: Text.HorizontalFit
                                                minimumPixelSize: Math.max(8, Math.round(mainWindow.decodedTextFontPixelSize * fs * 0.65))
                                            }
                                            Rectangle {
                                                visible: fsRowFloating.entry.isLotw === true
                                                width: Math.max(5, Math.round(6 * fs))
                                                height: width
                                                radius: width / 2
                                                anchors.right: parent.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: mainWindow.lotwMarkerColor()
                                                border.color: mainWindow.boostedDecodeTextColor(textSecondary)
                                                border.width: 1
                                            }
                                        }
                                    }
                                }
                            }

	                            MouseArea {
	                                enabled: !parent.isPeriodSeparator
	                                anchors.fill: parent
		                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                onClicked: (mouse) => { if (!parent.isPeriodSeparator && mouse.button === Qt.RightButton) mainWindow.openQrzLookup(entry) }
                                                onDoubleClicked: { if (!parent.isPeriodSeparator && !entry.isTx) decodePanel.handleDecodeDoubleClick(entry) }
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
        title: qsTr("Period 2 - Decodium")
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
	        title: qsTr("Signal RX - Decodium")
	        color: "transparent"
	        readonly property bool compactColumns: width < 520
	        readonly property int utcColumnWidth: compactColumns ? 66 : 84
	        readonly property int dbColumnWidth: compactColumns ? 34 : 38
	        readonly property int dbDtGapWidth: compactColumns ? 4 : 6
	        readonly property int dtColumnWidth: compactColumns ? 42 : 48
	        readonly property int dtFreqGapWidth: mainWindow.signalRxShowFreqColumn ? (compactColumns ? 4 : 6) : 0
	        readonly property int freqColumnWidth: mainWindow.signalRxShowFreqColumn ? (compactColumns ? 42 : 46) : 0
	        readonly property int gapColumnWidth: compactColumns ? 8 : 12
	        readonly property int distanceColumnWidth: mainWindow.signalRxShowDistColumn && !compactColumns ? 56 : 0
	        readonly property int azColumnWidth: mainWindow.signalRxShowAzColumn && !compactColumns ? 42 : 0

	        x: mainWindow.x + 300
        y: mainWindow.y + 250
        Component.onCompleted: {
            mainWindow.restoreFloatingWindowState(rxFreqFloatingWindow, "rxFreqFloatingWindow", "rxFreqDetached", "rxFreqMinimized")
            if (mainWindow.ft2LinkModeActive)
                rxFreqFloatingWindow.hide()
        }
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

	                            // 1.0.253 — Compact mode toggle Signal RX (floating window)
	                            Rectangle {
	                                Layout.preferredWidth: 50
	                                Layout.preferredHeight: 22
	                                radius: 4
	                                color: rxFloatCompactMA.containsMouse
	                                    ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
	                                    : (mainWindow.compactSignalRx
	                                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
	                                        : "transparent")
	                                border.color: (rxFloatCompactMA.containsMouse || mainWindow.compactSignalRx)
	                                    ? secondaryCyan
	                                    : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
	                                border.width: 1
	                                Text {
	                                    anchors.centerIn: parent
		                                    text: mainWindow.compactSignalRx ? "Full" : "Cmp"
	                                    font.pixelSize: mainWindow.compactSignalRx ? 11 : 10
	                                    font.bold: true
	                                    color: (rxFloatCompactMA.containsMouse || mainWindow.compactSignalRx)
	                                        ? secondaryCyan : textSecondary
	                                }
	                                MouseArea {
	                                    id: rxFloatCompactMA
	                                    anchors.fill: parent
	                                    hoverEnabled: true
	                                    cursorShape: Qt.PointingHandCursor
	                                    onClicked: mainWindow.toggleCompactSignalRx()
	                                }
	                                ToolTip.visible: rxFloatCompactMA.containsMouse
	                                ToolTip.text: mainWindow.compactSignalRx
	                                    ? qsTr("Return to normal row height")
	                                    : qsTr("Compact rows (2x visible decodes)")
	                                ToolTip.delay: 500
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
	                        Item { visible: rxFreqFloatingWindow.freqColumnWidth > 0; Layout.preferredWidth: rxFreqFloatingWindow.dtFreqGapWidth }
	                        Text { visible: rxFreqFloatingWindow.freqColumnWidth > 0; text: qsTr("Freq"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.freqColumnWidth }
	                        Item { Layout.preferredWidth: rxFreqFloatingWindow.gapColumnWidth }
	                        Text { text: qsTr("Message"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
	                        Text { visible: rxFreqFloatingWindow.distanceColumnWidth > 0; text: qsTr("Dist"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.distanceColumnWidth }
	                        Text { visible: rxFreqFloatingWindow.azColumnWidth > 0; text: qsTr("Az"); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextHeaderPixelSize * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.azColumnWidth }
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
                        cacheBuffer: 360  // 1.0.478 — meno delegate offscreen durante pile-up FT8/4/2
	                        reuseItems: true
	                        interactive: true
	                        property bool followTail: true
	                        property bool tailFollowPending: false
		                        property bool tailFollowQueued: false
		                        property int pendingNewDecodes: 0
		                        function isNearTail() {
		                            return contentHeight <= height + 2
		                                || contentY >= tailContentY() - 48
		                        }
                        function updateFollowTail() {
                            if (tailFollowPending)
                                return
                            followTail = isNearTail()
                            if (followTail) rxFrequencyFloatingList.pendingNewDecodes = 0
                        }
	                        function tailContentY() {
	                            var bottom = originY + contentHeight - height
	                            return Math.max(originY, bottom)
	                        }
	                        function finishTailFollow() {
	                            var shouldSnap = tailFollowPending || followTail
	                            tailFollowPending = false
	                            if (shouldSnap) {
	                                var targetY = tailContentY()
	                                if (Math.abs(contentY - targetY) > 0.5)
	                                    contentY = targetY
	                            }
	                            followTail = isNearTail()
	                            if (followTail) rxFrequencyFloatingList.pendingNewDecodes = 0
	                        }
                        function forceTailFollow() {
    rxFrequencyFloatingList.followTail = true
    rxFrequencyFloatingList.tailFollowPending = true
    if (rxFrequencyFloatingList.tailFollowQueued)
        return
    rxFrequencyFloatingList.tailFollowQueued = true
    Qt.callLater(function() {
        rxFrequencyFloatingList.tailFollowQueued = false
        if (!rxFrequencyFloatingList)
            return
        var targetY = rxFrequencyFloatingList.tailContentY()
        rxFrequencyFloatingTailAnimation.stop()
        rxFrequencyFloatingList.tailFollowPending = true
        rxFrequencyFloatingList.contentY = targetY
        rxFrequencyFloatingList.finishTailFollow()
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
	                            if (!rxFreqDetached) return
	                            if (followTail || tailFollowPending)
	                                rxFrequencyFloatingTailSettleTimer.restart()
	                        }
	                        onHeightChanged: {
	                            if (!rxFreqDetached) return
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
	                            if (!rxFreqDetached) return
	                            if (decodePanel.hasNativeRxDecodeModel()) return
	                            if (!followTail) {
	                                rxFrequencyFloatingList.pendingNewDecodes++
	                                return
	                            }
	                            forceTailFollow()
                        }
	                        property int _ver: decodePanel.rxDecodeListVersion
		                        on_VerChanged: {
	                            if (!rxFreqDetached) return
	                            if (!followTail) return
                            forceTailFollow()
                        }
                        model: rxFreqDetached && rxFreqFloatingWindow.visible
                               ? ((bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : decodePanel.rxDecodes)
                               : null
	                        // Preserve the fade/slide without starting render-thread Animators.
	                        add: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100; easing.type: Easing.OutQuad }
	                        }
	                        addDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                        }
	                        moveDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                        }
	                        removeDisplaced: Transition {
	                            enabled: bridge && bridge.smoothDecodeFlow && bridge.uiQuality !== "Low" && !mainWindow.compactToggling
	                            NumberAnimation { properties: "y"; duration: mainWindow.decodeRowSlideAnim ? 100 : 0; easing.type: Easing.OutQuad }
	                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            interactive: true
                            width: 8
                        }

	                        delegate: Rectangle {
	                            width: parent ? parent.width - 8 : 100
	                            readonly property bool isPeriodSeparator: !!(modelData && modelData.isSeparator === true)
	                            // 1.0.352 fix: guard !modelData via entry (era in period1FloatingWindow
	                            // dal 1.0.205, dimenticato in questo 4° delegate). Evita TypeError flood.
	                            readonly property var entry: modelData || ({})
	                            // 1.0.253 — height adattiva compact mode Signal RX
	                            height: isPeriodSeparator ? Math.round(4 * fs) : Math.round(mainWindow.signalRxRowHeight * fs)
	                            radius: 3
	                            // 1.0.254 fix: guard !modelData mancante (era in evenPeriodList
	                            // e period1FloatingList dal 1.0.205, dimenticato qui).
	                            // Durante shift-diff + scroll concorrenti, modelData diventa
	                            // undefined per 1-2 frame -> color="" -> Rectangle nero.
	                            color: !modelData ? "transparent" :
	                                   isPeriodSeparator ? "transparent" :
		                                   mainWindow.decodeUserBgFill(modelData) ? mainWindow.decodeUserBgFill(modelData) :
			                                   modelData.bgColorHex ? mainWindow.boostedDecodeBackgroundColor(modelData.bgColorHex) :
		                                   (modelData.isCQ && bridge.decodeColorEnabled("colorCQ")) ? mainWindow.boostedDecodeBackgroundColor(Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15)) : mainWindow.boostedDecodeBackgroundColor(Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.05))

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
		                                Text { text: entry.formattedTime || decodePanel.formatUtcForDisplay(entry.time); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(entry.isTx ? "#f1c40f" : textSecondary); Layout.preferredWidth: rxFreqFloatingWindow.utcColumnWidth }
		                                Text { text: entry.db || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(entry.snrColor || (entry.isTx ? "#f1c40f" : textSecondary)); font.bold: entry.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.dbColumnWidth }
	                                Item { Layout.preferredWidth: rxFreqFloatingWindow.dbDtGapWidth }
		                                Text { text: entry.dt || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(entry.isTx ? "#f1c40f" : textSecondary); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.dtColumnWidth }
	                                Item { visible: rxFreqFloatingWindow.freqColumnWidth > 0; Layout.preferredWidth: rxFreqFloatingWindow.dtFreqGapWidth }
			                                Text { visible: rxFreqFloatingWindow.freqColumnWidth > 0; text: entry.freq || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(entry.isTx ? "#f1c40f" : secondaryCyan); font.bold: entry.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.freqColumnWidth }
                                Item { Layout.preferredWidth: rxFreqFloatingWindow.gapColumnWidth }
                                Rectangle { property int dotSize: Math.max(5, Math.round(6 * fs)); visible: entry.isLotw === true; width: dotSize; height: dotSize; Layout.preferredWidth: dotSize; Layout.preferredHeight: dotSize; Layout.alignment: Qt.AlignVCenter; radius: dotSize / 2; color: mainWindow.lotwMarkerColor(); border.color: mainWindow.boostedDecodeTextColor(textSecondary); border.width: 1 }
                                Text { text: entry.displayMessage || entry.message || ""; font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); font.bold: decodePanel.decodeEntryBold(entry); font.strikeout: decodePanel.decodeEntryStrikeout(entry); color: getDxccColor(entry); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: messageElideMode(entry.displayMessage || entry.message) }
		                                Text { visible: rxFreqFloatingWindow.distanceColumnWidth > 0; text: decodePanel.distanceText(entry); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(textSecondary); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.distanceColumnWidth }
		                                Text { visible: rxFreqFloatingWindow.azColumnWidth > 0; text: formatBearingDegrees(entry.dxBearing); font.family: mainWindow.decodedTextFontFamily; font.pixelSize: Math.round(mainWindow.decodedTextFontPixelSize * fs); color: mainWindow.boostedDecodeTextColor(secondaryCyan); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqFloatingWindow.azColumnWidth }
                            }

	                            MouseArea {
	                                enabled: !parent.isPeriodSeparator
	                                anchors.fill: parent
	                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                onClicked: (mouse) => { if (!parent.isPeriodSeparator && mouse.button === Qt.RightButton) mainWindow.openQrzLookup(modelData) }
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
        title: qsTr("TX Panel - Decodium")
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
	                            ToolTip.text: qsTr("Dock TX panel")
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
	                    ft2LinkToolPanel: mainWindow.ft2LinkPanelDetached
	                                      ? ft2LinkFloatingLoader.item
	                                      : ft2LinkInlineLoader.item
                    showAsyncIcon: mainWindow.asyncIconVisible
                    handleLogPrompt: txPanelDetached
                    onMamWindowRequested: mainWindow.openMamWindow()
                    onCallRequested: mainWindow.openCallDialog()
                    onFt2LinkAccessRequested: mainWindow.requestFt2LinkAccess()
                }
            }
	        }
	    }

	    // ========== DETACHABLE FT2-LINK PANEL WINDOW ==========
	    Window {
	        id: ft2LinkFloatingWindow
	        width: 1180
	        height: 620
	        minimumWidth: 760
	        minimumHeight: 360
	        visible: mainWindow.ft2LinkModeActive && mainWindow.ft2LinkPanelDetached
	        title: qsTr("FT2-Link - Decodium")
	        color: bgDeep

	        x: mainWindow.x + 80
	        y: mainWindow.y + 120

	        onClosing: function(close) {
	            mainWindow.dockFt2LinkPanel()
	            close.accepted = true
	        }

	        Loader {
	            id: ft2LinkFloatingLoader
	            anchors.fill: parent
	            active: mainWindow.ft2LinkModeActive && mainWindow.ft2LinkPanelDetached
	            asynchronous: true
	            source: "../panels/FT2LinkPanel.qml"
	            onLoaded: {
	                if (item) {
	                    item.dragTarget = null
	                    item.toolTabsExternal = true
	                    item.poppedOut = true
	                }
	            }
	        }

	        Connections {
	            target: ft2LinkFloatingLoader.item
	            ignoreUnknownSignals: true
	            function onCloseRequested() {
	                mainWindow.dockFt2LinkPanel()
	            }
	            function onPopDockRequested() {
	                mainWindow.dockFt2LinkPanel()
	            }
	        }
	    }

	    // ===== GAP 3 — Pannelli floating draggabili =====

    // TimeSyncPanel — sotto il blocco Setup/REC/WAV, togglabile da menu
    Item {
        id: timeSyncOverlay
        readonly property int panelMargin: 12
        readonly property int topRailY: 10
        property bool userPositioned: mainWindow.settingBool("uiTimeSyncPanelUserPositioned", false)
        property real savedX: Number(mainWindow.safeBridgeSetting("uiTimeSyncPanelX", -1))
        property real savedY: Number(mainWindow.safeBridgeSetting("uiTimeSyncPanelY", -1))
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
            mainWindow.persistUiSetting("uiTimeSyncPanelUserPositioned", true)
            mainWindow.persistUiSetting("uiTimeSyncPanelX", savedX)
            mainWindow.persistUiSetting("uiTimeSyncPanelY", savedY)
        }
        function clampSavedPosition() {
            if (!userPositioned)
                return
            savedX = boundedX(savedX)
            savedY = boundedY(savedY)
            x = savedX
            y = savedY
            mainWindow.persistUiSetting("uiTimeSyncPanelX", savedX)
            mainWindow.persistUiSetting("uiTimeSyncPanelY", savedY)
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
            asynchronous: true
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

    // Active Stations is a desktop window rather than a Main.qml overlay.
    // Its x/y values are therefore global and remain valid on external screens.
    Window {
        id: activeStationsFloatingWindow
        width: 360
        height: 300
        minimumWidth: 300
        minimumHeight: 220
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("Active Stations - Decodium")
        color: "transparent"

        x: mainWindow.x + Math.max(12, mainWindow.width - width - 24)
        y: mainWindow.y + 120

        function finishDesktopMove() {
            mainWindow.finishFloatingWindowDrag(activeStationsFloatingWindow)
        }

        function showHostedWindow() {
            activeStationsPanelVisible = true
            activeStationsLoader.active = true
            show()
            raise()
            requestActivate()
        }

        function hideHostedWindow() {
            activeStationsPanelVisible = false
            hide()
            mainWindow.scheduleWindowStateSave()
        }

        Component.onCompleted: {
            mainWindow.restoreFloatingWindowState(activeStationsFloatingWindow,
                                                   "activeStationsFloatingWindow",
                                                   "",
                                                   "")
            if (activeStationsPanelVisible)
                Qt.callLater(showHostedWindow)
        }
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onClosing: function(close) {
            if (!mainWindow.applicationClosing)
                activeStationsPanelVisible = false
            mainWindow.scheduleWindowStateSave()
            close.accepted = true
        }

        Loader {
            id: activeStationsLoader
            anchors.fill: parent
            active: false
            asynchronous: true
            source: "../panels/ActiveStationsPanel.qml"
            onLoaded: item.nativeHostWindow = activeStationsFloatingWindow
        }

        Connections {
            target: activeStationsLoader.item
            ignoreUnknownSignals: true
            function onCloseRequested() { activeStationsFloatingWindow.hideHostedWindow() }
        }

        FloatingResizeHandles {
            z: 100
            targetWindow: activeStationsFloatingWindow
            maxWidth: 900
            maxHeight: 1000
        }

        Shortcut {
            enabled: activeStationsFloatingWindow.visible
            sequence: "Escape"
            context: Qt.WindowShortcut
            onActivated: activeStationsFloatingWindow.hideHostedWindow()
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
            asynchronous: true
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
            if (!bridge.foxMode)
                callerQueuePanelVisible = false
        }
    }

    // AstroPanel — C14: calcolatrice astronomica EME, angolo in basso a sinistra
    Item {
        id: astroPanelOverlay
        visible: astroPanelVisible
        z: 200
        property bool userPositioned: mainWindow.settingBool("uiAstroPanelUserPositioned", false)
        property real savedX: Number(mainWindow.safeBridgeSetting("uiAstroPanelX", -1))
        property real savedY: Number(mainWindow.safeBridgeSetting("uiAstroPanelY", -1))
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
            mainWindow.persistUiSetting("uiAstroPanelUserPositioned", true)
            mainWindow.persistUiSetting("uiAstroPanelX", savedX)
            mainWindow.persistUiSetting("uiAstroPanelY", savedY)
        }
        function clampSavedPosition() {
            if (!userPositioned)
                return
            savedX = boundedX(savedX)
            savedY = boundedY(savedY)
            x = savedX
            y = savedY
            mainWindow.persistUiSetting("uiAstroPanelX", savedX)
            mainWindow.persistUiSetting("uiAstroPanelY", savedY)
        }
        x: userPositioned ? boundedX(savedX) : 12
        y: userPositioned ? boundedY(savedY) : mainWindow.height - height - 180
        width: 320
        height: 230
        onWidthChanged: Qt.callLater(clampSavedPosition)
        onHeightChanged: Qt.callLater(clampSavedPosition)

        MouseArea {
            anchors.fill: parent
            drag.target: astroPanelOverlay
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0; drag.maximumX: mainWindow.width - astroPanelOverlay.width
            drag.minimumY: 0; drag.maximumY: mainWindow.height - 50
            onReleased: astroPanelOverlay.savePosition()
        }

        Loader {
            id: astroPanelLoader
            anchors.fill: parent
            active: astroPanelVisible
            asynchronous: true
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

    // 1.0.275 (fork-only) — DxClusterPanel ora in una Window floating separata,
    // spostabile ovunque sul monitor (anche fuori dalla mainWindow). Integrata
    // con il Reset Layout via property dxClusterDetached/Minimized e persistence
    // standard WindowState/dxClusterFloatingWindow.
    Window {
        id: dxClusterFloatingWindow
        title: qsTr("DX Cluster - Decodium")
        width: Math.max(500, Number(bridge.getSetting("uiDxClusterPanelWidth", 560)))
        height: Math.max(300, Number(bridge.getSetting("uiDxClusterPanelHeight", 360)))
        minimumWidth: 500
        minimumHeight: 300
        visible: mainWindow.dxClusterPanelVisible && mainWindow.dxClusterDetached   // 1.0.385 — solo se staccato
        flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
             | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint
             | Qt.WindowCloseButtonHint
        color: "#1a1a2e"

        x: mainWindow.x + Math.max(0, mainWindow.width - width - 60)
        y: mainWindow.y + 80

        Component.onCompleted: {
            mainWindow.restoreFloatingWindowState(
                dxClusterFloatingWindow, "dxClusterFloatingWindow", "dxClusterDetached", "dxClusterMinimized")
            // La geometria del DX Cluster va ripristinata, ma la finestra non
            // deve aprirsi da sola solo perche' e' floating/detached. La scelta
            // dell'utente e' `uiDxClusterPanelVisible`: se e' false o assente
            // resta chiusa anche quando esiste una vecchia WindowState salvata.
            if (!mainWindow.dxClusterPanelVisible || !mainWindow.dxClusterDetached) {
                dxClusterFloatingWindow.hide()   // 1.0.385 — dockato o spento → niente finestra
            } else if (!mainWindow.dxClusterMinimized) {
                dxClusterFloatingWindow.show()
                dxClusterFloatingWindow.raise()
            }
        }
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()
        onVisibilityChanged: function(visibility) {
            if (visibility === Window.Minimized) {
                mainWindow.dxClusterMinimized = true
            } else if (dxClusterFloatingWindow.visible) {
                mainWindow.dxClusterMinimized = false
            }
            mainWindow.scheduleWindowStateSave()
        }
        onClosing: function(close) {
            mainWindow.dxClusterPanelVisible = false
            close.accepted = true
        }

        Item {
            anchors.fill: parent

            // 1.0.386 — barra "Inserisci nel layout": aggancia il Cluster come 4ª colonna
            // accanto a Full Spectrum o a Signal RX (l'inserimento è esplicito; staccato
            // la colonna sparisce del tutto).
            Rectangle {
                id: dxcDockBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 30
                color: "#16213e"
                border.color: glassBorder
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Text {
                        text: qsTr("Insert into layout:")
                        color: textPrimary
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Repeater {
                        model: [
                            { ref: "fullspectrum", label: qsTr("◧ next to Full Spectrum") },
                            { ref: "signalrx",     label: qsTr("◧ next to Signal RX") }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            width: dxcBtnText.implicitWidth + 16
                            height: 20
                            radius: 3
                            anchors.verticalCenter: parent.verticalCenter
                            color: dxcDockBtnMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.30)
                                                              : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.14)
                            border.color: secondaryCyan
                            border.width: 1
                            Text {
                                id: dxcBtnText
                                anchors.centerIn: parent
                                text: modelData.label
                                color: textPrimary
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: dxcDockBtnMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: mainWindow.dockDxClusterNextTo(modelData.ref)
                            }
                        }
                    }
                }
            }

            DxClusterPanel {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: dxcDockBar.bottom
                anchors.bottom: parent.bottom
                onCloseRequested: mainWindow.dxClusterPanelVisible = false
                // positionCommitted non piu' usato: la Window OS gestisce drag/resize nativi
            }
        }
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

    // ── Proposta di rilevamento radio al primo avvio ─────────────────────────
    // Compare una sola volta, e solo a chi non ha ancora configurato il CAT:
    // e' proprio la persona che non sa quale porta scegliere. Chi ha gia' una
    // configurazione non viene disturbato. Il rilevamento e' passivo (nessuna
    // porta aperta), e nulla cambia finche' non si preme Applica.
    RigDetectResults {
        id: firstRunRigDetect
        onDismissed: bridge.setSetting("RigAutoDetectOffered", true)
    }

    Timer {
        id: firstRunRigDetectTimer
        interval: 8000          // dopo che le fasi di avvio si sono concluse
        running: true
        repeat: false
        onTriggered: {
            if (String(bridge.getSetting("RigAutoDetectOffered", false)) === "true")
                return
            // Il nominativo e' il segnale piu' affidabile di prima esecuzione:
            // chi ha gia' usato il programma ce l'ha, e non dipende da quando
            // il backend CAT finisce di caricare le proprie impostazioni.
            var callsign = String(bridge.callsign || "").trim()
            var cat = bridge.catManager
            var catPort = cat ? String(cat.serialPort || "").trim() : ""
            if (callsign.length > 0 && catPort.length > 0) {
                console.log("[RigDetect] gia' configurato (" + callsign + " su " + catPort
                            + "): nessuna proposta al primo avvio")
                return
            }
            console.log("[RigDetect] prima esecuzione: nominativo='" + callsign
                        + "' porta CAT='" + catPort + "'")

            var found = bridge.detectConnectedRigs()
            var worthShowing = false
            for (var i = 0; i < found.length; ++i) {
                if (found[i].confidence >= 60) {
                    worthShowing = true
                    break
                }
            }
            if (!worthShowing) {
                // Nulla di riconoscibile: si segna comunque, per non riproporsi
                // a ogni avvio a chi non ha una radio collegata via USB.
                bridge.setSetting("RigAutoDetectOffered", true)
                return
            }
            console.log("[RigDetect] proposta mostrata: " + found.length + " candidati, il primo e' "
                        + found[0].rigLabel + " su " + found[0].catPort)
            firstRunRigDetect.candidates = found
            firstRunRigDetect.introText =
                qsTr("Decodium found a radio connected to this computer. Applying the proposal sets the CAT port, the model and the audio devices, so you can start without hunting for the right COM port.")
            firstRunRigDetect.open()
        }
    }
}
