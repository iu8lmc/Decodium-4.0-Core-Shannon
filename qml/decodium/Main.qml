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
    minimumWidth: 1200
    minimumHeight: 700
    visible: true
    title: "Decodium 4.0 — " + bridge.mode + " — " + bridge.callsign
    property bool windowStateRestoreInProgress: true

    // Carica posizione/dimensioni finestre dal bridge
    Component.onCompleted: {
        var state = bridge.loadWindowState("mainWindow")
        if (state.width !== undefined && state.width > 0) width = state.width
        if (state.height !== undefined && state.height > 0) height = state.height
        if (state.x !== undefined) x = state.x
        if (state.y !== undefined) y = state.y
        Qt.callLater(function() {
            windowStateRestoreInProgress = false
        })
    }

    // Altezza pannello waterfall — caricata da bridge.uiWaterfallHeight
    property int  waterfallPanelHeight: bridge.uiWaterfallHeight > 0 ? bridge.uiWaterfallHeight : 350

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
    // Funzione helper chiamabile da qualsiasi parte del QML
    function scheduleSave() { saveTimer.restart() }
    function scheduleWindowStateSave() {
        if (!windowStateRestoreInProgress) {
            windowStateSaveTimer.restart()
        }
    }

    function restoreFloatingWindowState(windowRef, key, detachedPropName, minimizedPropName) {
        var state = bridge.loadWindowState(key)
        if (state.width !== undefined && state.width > 0) windowRef.width = state.width
        if (state.height !== undefined && state.height > 0) windowRef.height = state.height
        if (state.x !== undefined) windowRef.x = state.x
        if (state.y !== undefined) windowRef.y = state.y

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

    function persistWindowLayouts() {
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
    }

    // Salva impostazioni bridge al close
    onClosing: function(close) {
        saveTimer.stop()
        windowStateSaveTimer.stop()
        persistWindowLayouts()
        bridge.saveSettings()
        console.log("Main window closing - shutting down application")
        // Close all floating windows
        if (waterfallWindow) waterfallWindow.close()
        if (logWindow) logWindow.close()
        if (astroWindow) astroWindow.close()
        if (macroDialog) macroDialog.close()
        if (rigControlDialog) rigControlDialog.close()
        if (txPanelFloatingWindow) txPanelFloatingWindow.close()
        if (period1FloatingWindow) period1FloatingWindow.close()
        if (period2FloatingWindow) period2FloatingWindow.close()
        if (rxFreqFloatingWindow) rxFreqFloatingWindow.close()
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
    property bool period2Detached: false
    property bool period2Minimized: false
    property bool period2DockHighlighted: false
    property bool rxFreqDetached: false
    property bool rxFreqMinimized: false
    property bool rxFreqDockHighlighted: false

    // TxPanel detached and minimized states
    property bool txPanelDetached: false
    property bool txPanelMinimized: false
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

    // === GAP 3 — Nuovi pannelli (A3, B9, A4, C14) ===
    property bool timeSyncPanelVisible:       false
    property bool activeStationsPanelVisible: false
    property bool callerQueuePanelVisible:    false
    property bool astroPanelVisible:          false
    property bool dxClusterPanelVisible:      false

    // === Dialoghi lazy-loaded ===
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

    property var legacySetupSections: [
        { title: "Generale",   tabIndex: 0, description: "Stazione, display e comportamento" },
        { title: "Radio",      tabIndex: 1, description: "CAT, PTT e parametri radio" },
        { title: "Audio",      tabIndex: 2, description: "Schede audio, canali e livelli" },
        { title: "Macro TX",   tabIndex: 3, description: "Messaggi predefiniti e testo libero" },
        { title: "Reporting",  tabIndex: 4, description: "PSK Reporter, logging e servizi" },
        { title: "Frequenze",  tabIndex: 5, description: "Bande, frequenze e allocazioni" },
        { title: "Colori",     tabIndex: 6, description: "Highlight, palette e aspetto decode" },
        { title: "Avanzate",   tabIndex: 7, description: "Opzioni estese e comportamento avanzato" },
        { title: "Allarmi",    tabIndex: 8, description: "Alert, notifiche e suoni" },
        { title: "Filtri",     tabIndex: 9, description: "Whitelist, blacklist e filtri decode" }
    ]
    property string rigErrorDialogTitle: ""
    property string rigErrorSummary: ""
    property string rigErrorDetails: ""
    property bool rigErrorDetailsVisible: false
    property string warningDialogTitle: ""
    property string warningDialogSummary: ""
    property string warningDialogDetails: ""
    property bool warningDialogDetailsVisible: false

    function openSetupMenu(anchorItem) {
        const point = anchorItem.mapToItem(Overlay.overlay, 0, anchorItem.height + 8)
        setupMenuPopup.x = Math.max(16, Math.min(point.x, mainWindow.width - setupMenuPopup.width - 16))
        setupMenuPopup.y = Math.max(16, point.y)
        setupMenuPopup.open()
    }

    // WAV file open dialog - single file
    FileDialog {
        id: wavOpenDialog
        title: "Apri file WAV per decodifica"
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
        title: "Seleziona cartella con file WAV"
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

    // IU8LMC: DXCC color scheme (JTDX-style)
    readonly property color colorWorked: "#808080"       // Gray - already worked
    readonly property color colorNewBand: "#FFD700"      // Gold - new on this band
    readonly property color colorNewCountry: "#00FF00"   // Bright green - new country
    readonly property color colorMostWanted: "#FF00FF"   // Magenta - most wanted

    // IU8LMC: Tooltip properties
    property string dxccTooltipText: ""
    property bool dxccTooltipVisible: false
    property real dxccTooltipX: 0
    property real dxccTooltipY: 0

    // Shannon-compatible color function (allineato a DecodeWindow.qml)
    function getDxccColor(modelData) {
        if (modelData.isTx)     return "#FF8C00"
        if (modelData.isMyCall) return "#FF4444"
        if (modelData.isB4 === true || modelData.dxIsWorked === true) return "#606060"
        if (modelData.isLotw === true) return "#44BBFF"
        if (modelData.dxIsMostWanted === true) return colorMostWanted
        if (modelData.dxIsNewCountry === true) return colorNewCountry
        if (modelData.dxIsNewBand === true) return colorNewBand
        if (modelData.isCQ) return accentGreen
        return textPrimary
    }

    function formatBearingDegrees(value) {
        return value !== undefined && value >= 0 ? Math.round(value) + "°" : ""
    }

    // IU8LMC: Function to build tooltip text
    function getDxccTooltipText(modelData) {
        if (!modelData.dxCountry) return ""
        var lines = []
        var header = modelData.dxCallsign + " - " + modelData.dxCountry
        if (modelData.dxContinent) header += " (" + modelData.dxContinent + ")"
        lines.push(header)
        // Bearing and distance to DX station
        if (modelData.dxBearing !== undefined && modelData.dxBearing >= 0) {
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

    // Dock zones positions
    property rect waterfallDockZone: Qt.rect(8, 64, 450, contentArea.height - 108)

    // Magnetic snap threshold
    readonly property int snapThreshold: 80

    Material.theme: bridge.themeManager.isLightTheme ? Material.Light : Material.Dark
    Material.accent: secondaryCyan
    Material.primary: primaryBlue

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

    // Main background gradient
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: bgDeep }
            GradientStop { position: 0.5; color: bgMedium }
            GradientStop { position: 1.0; color: bgDeep }
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
            border.color: bridge.pskSearchFound ? accentGreen : "#f44336"
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
                    color: bridge.pskSearchFound ? accentGreen : "#f44336"
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
                    color: bridge.pskSearchFound ? accentGreen : "#f44336"
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
                        running: bridge.pskSearching
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
    Connections {
        target: bridge
        function onPskSearchingChanged() {
            if (!bridge.pskSearching && bridge.pskSearchCallsign !== "") {
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
                color: freqInputPopup.isTx ? "#f44336" : accentGreen
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
                    font.family: "Consolas"
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
                                    running: bridge.catConnected
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.4; duration: 800 }
                                    NumberAnimation { to: 1.0; duration: 800 }
                                }
                            }

                            // Frequency display - syncs with radio or band selection
                            Text {
                                id: frequencyDisplay
                                // bridge.frequency is synced with both CAT and BandManager
                                text: (bridge.frequency / 1000000).toFixed(6)
                                font.pixelSize: Math.round(26 * fs)
                                font.family: "Consolas"
                                font.bold: true
                                color: bridge.transmitting ? "#ff6b6b" : accentGreen
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
                                color: bridge.transmitting ? "#f44336" : textSecondary
                            }
                            // TX frequency - click to edit
                            Rectangle {
                                width: txFreqText.width + 6; height: 14; radius: 2
                                color: txFreqMouseArea.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1) : "transparent"
                                Text {
                                    id: txFreqText; anchors.centerIn: parent
                                    text: bridge.txFrequency + " Hz"
                                    font.pixelSize: 10; font.family: "Consolas"
                                    color: bridge.txFrequency === bridge.rxFrequency ? accentGreen : "#f44336"
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
                                    font.pixelSize: 10; font.family: "Consolas"
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
                                border.color: bridge.txFrequency === bridge.rxFrequency ? accentGreen : "#f44336"

                                Text {
                                    anchors.centerIn: parent
                                    text: "TX=RX"
                                    font.pixelSize: 8
                                    font.bold: true
                                    color: bridge.txFrequency === bridge.rxFrequency ? accentGreen : "#f44336"
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
                            color: (bridge.ledCoherentAveraging || bridge.ledNeuralSync || bridge.ledTurboFeedback) ? accentGreen : "#555"
                        }

                        // LED 1: Coherent Averaging (Blue)
                        Rectangle {
                            id: ledCoherent
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.ledCoherentAveraging ? "#2196F3" : "#333"
                            border.color: bridge.ledCoherentAveraging ? "#64B5F6" : "#444"
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: bridge.ledCoherentAveraging
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.5; duration: 400 }
                                NumberAnimation { to: 1.0; duration: 400 }
                            }

                            ToolTip.visible: maCoherent.containsMouse
                            ToolTip.text: "Coherent Avg: " + (bridge.ledCoherentAveraging ? bridge.coherentCount + " signals" : "OFF")

                            MouseArea {
                                id: maCoherent
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // LED 2: Neural Sync (Purple)
                        Rectangle {
                            id: ledNeural
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.ledNeuralSync ? "#9C27B0" : "#333"
                            border.color: bridge.ledNeuralSync ? "#CE93D8" : "#444"
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: bridge.ledNeuralSync
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.5; duration: 300 }
                                NumberAnimation { to: 1.0; duration: 300 }
                            }

                            ToolTip.visible: maNeural.containsMouse
                            ToolTip.text: "Neural Sync: " + (bridge.ledNeuralSync ? (bridge.neuralScore * 100).toFixed(0) + "%" : "OFF")

                            MouseArea {
                                id: maNeural
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // LED 3: Turbo Feedback (Orange)
                        Rectangle {
                            id: ledTurbo
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.ledTurboFeedback ? "#FF9800" : "#333"
                            border.color: bridge.ledTurboFeedback ? "#FFCC80" : "#444"
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: bridge.ledTurboFeedback
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.5; duration: 350 }
                                NumberAnimation { to: 1.0; duration: 350 }
                            }

                            ToolTip.visible: maTurbo.containsMouse
                            ToolTip.text: "Turbo Feedback: " + (bridge.ledTurboFeedback ? bridge.turboIterations + " iter" : "OFF")

                            MouseArea {
                                id: maTurbo
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        // Separator
                        Rectangle {
                            width: 1
                            height: 8
                            color: "#404060"
                        }

                        // Time Sync LED (Green when synced)
                        Rectangle {
                            id: ledTimeSync
                            width: 8
                            height: 8
                            radius: 4
                            color: accentGreen
                            border.color: "#81C784"
                            border.width: 1
                        }

                        // UTC Time display
                        Text {
                            text: bridge.utcTime
                            font.pixelSize: 8
                            font.family: "Consolas"
                            font.bold: true
                            color: accentGreen
                        }
                    }

                    // TX indicator border
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.color: "#f44336"
                        border.width: 3
                        visible: bridge.transmitting
                        opacity: 0

                        SequentialAnimation on opacity {
                            running: bridge.transmitting
                            loops: Animation.Infinite
                            NumberAnimation { to: 1.0; duration: 250 }
                            NumberAnimation { to: 0.3; duration: 250 }
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
                                        onMoved: bridge.rxInputLevel = value
                                        Binding on value { value: bridge.rxInputLevel; when: !rxSliderHeader.pressed }
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
                                    Text { text: Math.round(bridge.rxInputLevel); color: secondaryCyan; font.pixelSize: 8; font.family: "Consolas"; Layout.preferredWidth: 18 }
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
                                        text: bridge.txOutputLevel < 450 ? ("-" + ((450 - bridge.txOutputLevel) / 10).toFixed(1)) : "0.0"
                                        color: accentGreen
                                        font.pixelSize: 8
                                        font.family: "Consolas"
                                        Layout.preferredWidth: 28
                                    }
                                }
                            }
                        }

                        // Row 2: LVL + Monitor (bottom)
                        RowLayout {
                            spacing: 2

                            // Audio Level Meter
                            Rectangle {
                                Layout.preferredWidth: 60
                                Layout.preferredHeight: 16
                                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                                border.color: glassBorder
                                radius: 2

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 2

                                    Text {
                                        text: "LVL"
                                        font.pixelSize: 7
                                        font.bold: true
                                        color: textSecondary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Rectangle {
                                        width: 30
                                        height: 10
                                        color: bgDeep
                                        border.color: glassBorder
                                        radius: 1
                                        anchors.verticalCenter: parent.verticalCenter

                                        Rectangle {
                                            x: 1
                                            y: 1
                                            // Gain x5 per rendere visibile il segnale tipico da radio USB (~0.05-0.15)
                                            property real displayLevel: Math.min(1.0, bridge.audioLevel * 5.0)
                                            width: Math.max(2, (parent.width - 2) * displayLevel)
                                            height: parent.height - 2
                                            radius: 1
                                            color: displayLevel > 0.8 ? "#f44336" : displayLevel > 0.5 ? accentGreen : secondaryCyan
                                            Behavior on width { NumberAnimation { duration: 50 } }
                                        }
                                    }
                                }
                            }

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
                                    ToolTip.text: "Seleziona modo di decodifica"
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
                                            running: bridge.monitoring
                                            loops: Animation.Infinite
                                            NumberAnimation { to: 0.4; duration: 500 }
                                            NumberAnimation { to: 1.0; duration: 500 }
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
                                onClicked: openSetupMenu(settingsButton)
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
                                   (bridge.wavManager && bridge.wavManager.recording ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent")
                            border.color: bridge.wavManager && bridge.wavManager.recording ? "#f44336" : "transparent"
                            border.width: bridge.wavManager && bridge.wavManager.recording ? 1 : 0

                            Row {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: bridge.wavManager && bridge.wavManager.recording ? "●" : "○"
                                    font.pixelSize: 12
                                    color: bridge.wavManager && bridge.wavManager.recording ? "#f44336" : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "REC"
                                    font.pixelSize: 9
                                    color: bridge.wavManager && bridge.wavManager.recording ? "#f44336" : textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: recMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.wavManager.recording ? bridge.wavManager.stopRecording() : bridge.wavManager.startRecording()
                            }

                            ToolTip.visible: recMA.containsMouse
                            ToolTip.text: bridge.wavManager && bridge.wavManager.recording ?
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
                            ToolTip.text: "Click: apri WAV singolo\nClick destro: batch cartella"
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
                                onClicked: logWindow.open()
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
                                onClicked: macroDialog.open()
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
                                onClicked: astroWindow.open()
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
                                    color: bridge.catConnected ? accentGreen : "#f44336"
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
                                onClicked: bridge.openCatSettings()
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
                    width: 252
                    height: 80
                    readonly property int analogClockWidth: 60
                    readonly property int cardMargins: 10
                    readonly property int rowSpacing: 12
                    readonly property int infoColumnWidth: Math.max(116, width - (cardMargins * 2) - analogClockWidth - rowSpacing - 4)

                    property var timezones: [
                        { name: "UTC", offset: 0 },
                        { name: "London", offset: 0 },
                        { name: "Rome", offset: 1 },
                        { name: "Moscow", offset: 3 },
                        { name: "Dubai", offset: 4 },
                        { name: "Tokyo", offset: 9 },
                        { name: "Sydney", offset: 11 },
                        { name: "New York", offset: -5 },
                        { name: "Los Angeles", offset: -8 }
                    ]
                    property int selectedTz: 0
                    property int hours: 0
                    property int minutes: 0
                    property int seconds: 0
                    property string dateStr: ""

                    Timer {
                        interval: 1000
                        running: true
                        repeat: true
                        onTriggered: worldClock.updateTime()
                    }

                    Component.onCompleted: updateTime()

                    function updateTime() {
                        var now = new Date()
                        var utc = now.getTime() + (now.getTimezoneOffset() * 60000)
                        var tzTime = new Date(utc + (timezones[selectedTz].offset * 3600000))
                        hours = tzTime.getHours()
                        minutes = tzTime.getMinutes()
                        seconds = tzTime.getSeconds()
                        var day = ("0" + tzTime.getDate()).slice(-2)
                        var month = ("0" + (tzTime.getMonth() + 1)).slice(-2)
                        var year = tzTime.getFullYear()
                        dateStr = day + "/" + month + "/" + year
                    }

                    function nextTimezone() {
                        selectedTz = (selectedTz + 1) % timezones.length
                        updateTime()
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
                            anchors.left: parent.left
                            anchors.leftMargin: worldClock.cardMargins
                            anchors.verticalCenter: parent.verticalCenter
                            width: worldClock.analogClockWidth
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

                        Text {
                            id: worldClockTimeText
                            anchors.left: analogClockFace.right
                            anchors.leftMargin: worldClock.rowSpacing
                            anchors.right: parent.right
                            anchors.rightMargin: worldClock.cardMargins
                            anchors.top: parent.top
                            anchors.topMargin: 2
                            font.pixelSize: 22
                            minimumPixelSize: 17
                            fontSizeMode: Text.Fit
                            font.family: "Consolas"
                            font.bold: true
                            color: textPrimary
                            elide: Text.ElideRight
                            text: ("0" + worldClock.hours).slice(-2) + ":" +
                                  ("0" + worldClock.minutes).slice(-2) + ":" +
                                  ("0" + worldClock.seconds).slice(-2)
                        }

                        Text {
                            id: worldClockDateText
                            anchors.left: worldClockTimeText.left
                            anchors.right: worldClockTimeText.right
                            anchors.top: worldClockTimeText.bottom
                            anchors.topMargin: 2
                            text: worldClock.dateStr
                            font.pixelSize: 11
                            font.family: "Consolas"
                            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.7)
                            elide: Text.ElideRight
                        }

                        StyledComboBox {
                            id: timezoneCombo
                            anchors.left: worldClockTimeText.left
                            anchors.right: worldClockTimeText.right
                            anchors.top: worldClockDateText.bottom
                            anchors.topMargin: 2
                            height: 32
                            font.pixelSize: 11
                            itemHeight: 34
                            popupMinWidth: width
                            textHorizontalAlignment: Text.AlignLeft
                            topPadding: 4
                            bottomPadding: 4
                            leftPadding: 12
                            rightPadding: 32
                            model: worldClock.timezones.map(function(t) { return t.name })
                            currentIndex: Math.max(0, worldClock.selectedTz)
                            accentColor: secondaryCyan
                            textColor: textPrimary
                            bgColor: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.92)
                            borderColor: glassBorder
                            onActivated: {
                                worldClock.selectedTz = currentIndex
                                worldClock.updateTime()
                            }
                            onCurrentIndexChanged: {
                                if (currentIndex >= 0 && worldClock.selectedTz !== currentIndex) {
                                    worldClock.selectedTz = currentIndex
                                    worldClock.updateTime()
                                }
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
                    ToolTip.text: "Ripristina Waterfall"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: waterfallMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
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
                            logWindow.open()
                        }
                    }

                    ToolTip.visible: logRestoreMA.containsMouse
                    ToolTip.text: "Ripristina QSO Log"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: logWindowMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
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
                            astroWindow.open()
                        }
                    }

                    ToolTip.visible: astroRestoreMA.containsMouse
                    ToolTip.text: "Ripristina Astronomical Data"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: astroWindowMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }

                // DX Cluster toggle button — sempre visibile nella toolbar
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
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
                        spacing: 4
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "📡"
                            font.pixelSize: 24
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
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dxClusterPanelVisible = !dxClusterPanelVisible
                    }

                    ToolTip.visible: dxcBtnMA.containsMouse
                    ToolTip.text: "DX Cluster — apri pannello spot"
                    ToolTip.delay: 400

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
                            macroDialog.open()
                        }
                    }

                    ToolTip.visible: macroRestoreMA.containsMouse
                    ToolTip.text: "Ripristina Macro Configuration"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: macroDialogMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
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
                            rigControlDialog.open()
                        }
                    }

                    ToolTip.visible: rigRestoreMA.containsMouse
                    ToolTip.text: "Ripristina Rig Control"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: rigControlMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }

                // Period 1 restore button
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
                            text: "1️⃣"
                            font.pixelSize: 24
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Period 1"
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
                            period1Minimized = false
                            period1FloatingWindow.show()
                        }
                    }

                    ToolTip.visible: p1RestoreMA.containsMouse
                    ToolTip.text: "Ripristina Period 1"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: period1Minimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
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
                    ToolTip.text: "Ripristina Period 2"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: period2Minimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
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
                            text: "RX Freq"
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
                            rxFreqMinimized = false
                            rxFreqFloatingWindow.show()
                        }
                    }

                    ToolTip.visible: rxRestoreMA.containsMouse
                    ToolTip.text: "Ripristina RX Frequency"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: rxFreqMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }

                // TX Panel restore button
                Rectangle {
                    width: 74
                    height: 74
                    radius: 8
                    visible: txPanelMinimized
                    color: txRestoreMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: txRestoreMA.containsMouse ? "#f44336" : glassBorder
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
                            color: "#f44336"
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
                    ToolTip.text: "Ripristina TX Panel"
                    ToolTip.delay: 500

                    SequentialAnimation on opacity {
                        running: txPanelMinimized
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.7; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }

                // PSK Reporter Search
                Rectangle {
                    width: 160
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
                                    placeholderText: "Callsign..."
                                    font.pixelSize: 11
                                    font.capitalization: Font.AllUppercase
                                    font.family: "Consolas"
                                    color: textPrimary
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

        // Content area for dockable panels
        Item {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            Layout.topMargin: 0

            // Main vertical split: Waterfall on top, Decode panels below
            SplitView {
                id: mainVerticalSplit
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: contentArea.height - txPanelContainer.height - (queuePanelComponent.visible ? 152 : 12)
                orientation: Qt.Vertical

                // Magnetic snap points for waterfall height
                property var snapPoints: [150, 200, 250, 300, 350, 400]
                property int snapThreshold: 20  // Pixels to trigger snap

                // Vertical drag handle with magnetic snap indicator
                handle: Rectangle {
                    id: splitHandle
                    implicitWidth: 10
                    implicitHeight: 14
                    color: SplitHandle.hovered || SplitHandle.pressed ? "#00e6e6" : "#505070"

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
                        width: 80
                        height: 6
                        radius: 3
                        color: parent.nearSnapPoint ? "#00ffff" : parent.color

                        // Glow effect when near snap
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -3
                            radius: 6
                            color: "transparent"
                            border.color: parent.parent.nearSnapPoint ? "#00ffff" : "transparent"
                            border.width: 2
                            opacity: 0.5
                        }
                    }

                    // Snap on release
                    Connections {
                        target: SplitHandle
                        function onPressedChanged() {
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
                    SplitView.minimumHeight: waterfallDetached ? 40 : 200
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
                                text: waterfallPanel.isDockHighlighted ? "🧲 Rilascia qui!" : "🌊 Waterfall Detached"
                                color: waterfallPanel.isDockHighlighted ? secondaryCyan : textSecondary
                                font.pixelSize: waterfallPanel.isDockHighlighted ? 16 : 12
                                font.bold: waterfallPanel.isDockHighlighted
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Trascina la finestra qui"
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
                                running: waterfallPanel.isDockHighlighted
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.4; duration: 300 }
                                NumberAnimation { to: 1.0; duration: 300 }
                            }
                        }
                    }

                    // Embedded waterfall content
                    Rectangle {
                        anchors.fill: parent
                        visible: !waterfallDetached
                        color: "transparent"

                        // Header - DRAGGABLE for magnetic detach
                        Rectangle {
                            id: waterfallHeader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 28
                            color: embeddedDragArea.isDragging ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                            radius: 8

                            Behavior on color { ColorAnimation { duration: 100 } }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: parent.radius
                                color: parent.color
                            }

                            // Drag area for magnetic detach
                            MouseArea {
                                id: embeddedDragArea
                                anchors.fill: parent
                                property bool isDragging: false
                                property point startPos: Qt.point(0, 0)
                                property point dragOffset: Qt.point(0, 0)

                                cursorShape: Qt.SizeAllCursor

                                onPressed: function(mouse) {
                                    startPos = mapToGlobal(mouse.x, mouse.y)
                                    isDragging = false
                                }

                                onPositionChanged: function(mouse) {
                                    if (pressed) {
                                        var currentPos = mapToGlobal(mouse.x, mouse.y)
                                        var deltaX = currentPos.x - startPos.x
                                        var deltaY = currentPos.y - startPos.y
                                        var distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY)

                                        // Start detach if dragged more than 50 pixels
                                        if (distance > 50 && !isDragging) {
                                            isDragging = true
                                            waterfallDetached = true
                                            // Position window at cursor
                                            waterfallWindow.x = currentPos.x - 100
                                            waterfallWindow.y = currentPos.y - 20
                                            waterfallWindow.show()
                                        }
                                    }
                                }

                                onReleased: {
                                    isDragging = false
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 8

                                Text {
                                    text: "⋮⋮⋮"
                                    font.pixelSize: 14
                                    color: secondaryCyan
                                }

                                Text {
                                    text: "Waterfall"
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: secondaryCyan
                                }

                                Text {
                                    text: "— trascina per sganciare"
                                    font.pixelSize: 10
                                    color: textSecondary
                                    opacity: 0.6
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }

                        Waterfall {
                            id: waterfallDisplayEmbedded
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: waterfallHeader.bottom
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            visible: !waterfallDetached
                            showControls: true
                            minFreq: 200
                            maxFreq: 3000
                            spectrumHeight: 150

                            onFrequencySelected: function(freq) {
                                bridge.rxFrequency = freq      // tasto destro = RX
                            }
                            onTxFrequencySelected: function(freq) {
                                bridge.txFrequency = freq      // tasto sinistro = TX
                            }
                        }
                    }
                }

                // ========== BOTTOM: Decode Panels (Period1 | Period2 | RX Freq) ==========
                Rectangle {
                    id: decodePanel
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 150
                    color: "transparent"

                    // Current active period tracking
                    property bool isCurrentPeriodEven: true
                    property int currentSecond: 0
                    property real periodLength: bridge.mode === "FT2" ? 3.75 : (bridge.mode === "FT4" ? 7.5 : 15)  // FT2=3.75s, FT4=7.5s, FT8=15s

                    // IU8LMC: Reactive property for all decodes (Band Activity)
                    property var allDecodes: []
                    property int decodeListVersion: 0

                    // Update decode list when decodeList changes
                    Connections {
                        target: bridge
                        function onDecodeListChanged() {
                            decodePanel.decodeListVersion++
                            decodePanel.allDecodes = bridge.decodeList
                        }
                    }

                    Timer {
                        id: periodTimer
                        interval: 200  // Update 5 times per second for smooth indicator
                        running: true
                        repeat: true
                        onTriggered: {
                            var now = new Date()
                            decodePanel.currentSecond = now.getSeconds()
                            var pLen = decodePanel.periodLength
                            // Calculate period index (0-based)
                            var periodIndex = Math.floor(decodePanel.currentSecond / pLen)
                            // Even period: period index 0, 2, 4, 6...
                            decodePanel.isCurrentPeriodEven = (periodIndex % 2) === 0
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
                        var relevant = md && (md.isMyCall || md.isTx)
                        return inWindow || relevant
                    }
                    function currentRxDecodes() {
                        var merged = []
                        var seen = {}
                        function utcSortValue(timeStr) {
                            var digits = String(timeStr || "").replace(/[^0-9]/g, "")
                            if (digits.length >= 6)
                                return parseInt(digits.substring(0, 6))
                            if (digits.length === 4)
                                return parseInt(digits + "00")
                            return -1
                        }
                        function appendUnique(item) {
                            if (!item)
                                return
                            var key = (item.time || "") + "|" +
                                      (item.freq || "") + "|" +
                                      (item.message || "") + "|" +
                                      (item.isTx ? "1" : "0")
                            if (seen[key])
                                return
                            seen[key] = true
                            merged.push(item)
                        }
                        if (bridge.rxDecodeList) {
                            for (var j = 0; j < bridge.rxDecodeList.length; j++) {
                                appendUnique(bridge.rxDecodeList[j])
                            }
                        }
                        for (var i = 0; i < bridge.decodeList.length; i++) {
                            var item = bridge.decodeList[i]
                            if (decodePanel.isAtRxFrequency(item.freq, item)) {
                                appendUnique(item)
                            }
                        }
                        merged.sort(function(a, b) {
                            var ta = utcSortValue(a.time)
                            var tb = utcSortValue(b.time)
                            if (ta !== tb)
                                return ta - tb
                            var fa = parseInt(a.freq || "0")
                            var fb = parseInt(b.freq || "0")
                            if (fa !== fb)
                                return fa - fb
                            return String(a.message || "").localeCompare(String(b.message || ""))
                        })
                        return merged
                    }

                    function formatUtcForDisplay(timeStr) {
                        var digits = String(timeStr || "").replace(/[^0-9]/g, "")
                        if (digits.length >= 6)
                            return digits.substring(0, 2) + ":" + digits.substring(2, 4) + ":" + digits.substring(4, 6)
                        if (digits.length === 4)
                            return digits.substring(0, 2) + ":" + digits.substring(2, 4)
                        return timeStr || ""
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
                        for (var i = 0; i < bridge.decodeList.length; i++) {
                            var d = bridge.decodeList[i]
                            if (!d.isTx && isEvenPeriod(d.time)) {
                                filtered.push(d)
                            }
                        }
                        return filtered
                    }

                    function getOddPeriodDecodes() {
                        var filtered = []
                        for (var i = 0; i < bridge.decodeList.length; i++) {
                            var d = bridge.decodeList[i]
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
                        border.color: glassBorder
                        border.width: 1

                        property real periodLen: decodePanel.periodLength
                        property real txDuration: bridge.mode === "FT2" ? 2.87 : (bridge.mode === "FT4" ? 5.04 : 12.64)
                        property real progress: 0.0
                        property real secInPeriod: 0.0
                        property bool isTxPhase: false
                        property bool isEvenPeriod: true
                        property string periodLabel: ""

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
                                // isTxPhase: true quando siamo nel NOSTRO periodo TX
                                // txPeriod=0 → TX nei periodi pari, txPeriod=1 → TX nei dispari
                                var isOurTxPeriod = bridge ? ((bridge.txPeriod === 0) === timingBar.isEvenPeriod) : false
                                timingBar.isTxPhase = isOurTxPeriod && timingBar.secInPeriod < timingBar.txDuration
                                timingBar.periodLabel = isOurTxPeriod ? "TX" : "RX"
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
                            color: Qt.rgba(1, 1, 1, 0.08)

                            // TX zone marker
                            Rectangle {
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
                                color: timingBar.isTxPhase ? "#f44336" : accentGreen
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
                                color: "#ffffff"
                                opacity: 0.9
                            }

                            // TX/RX boundary marker
                            Rectangle {
                                x: parent.width * (timingBar.txDuration / timingBar.periodLen) - 1
                                y: -4
                                width: 2
                                height: parent.height + 8
                                color: Qt.rgba(1, 1, 1, 0.3)
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
                            font.family: "Consolas"
                            color: timingBar.isTxPhase ? "#f44336" : "#4CAF50"
                        }

                        // Mode + phase label (left of bar)
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 26
                            anchors.verticalCenter: parent.verticalCenter
                            text: timingBar.isTxPhase ? "TX" : "RX"
                            font.pixelSize: 10
                            font.bold: true
                            font.family: "Consolas"
                            color: timingBar.isTxPhase ? "#f44336" : accentGreen
                        }

                        // Time counter (right)
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: timingBar.secInPeriod.toFixed(1) + " / " + timingBar.periodLen.toFixed(1) + "s"
                            font.pixelSize: 10
                            font.family: "Consolas"
                            color: textSecondary
                        }
                    }

                    SplitView {
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
                        }

                        // ========== LEFT: Band Activity ==========
                        Rectangle {
                            id: period1Panel
                            SplitView.preferredWidth: 400
                            SplitView.minimumWidth: 360
                            readonly property bool compactColumns: width < 540
                            readonly property int utcColumnWidth: compactColumns ? 48 : 55
                            readonly property int dbColumnWidth: compactColumns ? 26 : 30
                            readonly property int dtColumnWidth: compactColumns ? 32 : 35
                            readonly property int freqColumnWidth: compactColumns ? 42 : 45
                            readonly property int gapColumnWidth: compactColumns ? 4 : 6
                            readonly property int dxccColumnWidth: compactColumns ? 96 : 132
                            readonly property int azColumnWidth: compactColumns ? 42 : 52
                            readonly property int messageMinWidth: compactColumns ? 72 : 140
                            Component.onCompleted: {
                                // Dopo il layout, porta il separatore al 50%
                                Qt.callLater(function() {
                                    if (parent && parent.width > 0)
                                        period1Panel.SplitView.preferredWidth = parent.width * 0.5
                                })
                            }
                            color: "transparent"

                            // Placeholder when detached - magnetic dock zone
                            Rectangle {
                                anchors.fill: parent
                                visible: period1Detached
                                color: period1DockHighlighted ? Qt.rgba(76/255, 175/255, 80/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                                radius: 8
                                border.color: period1DockHighlighted ? "#4CAF50" : glassBorder
                                border.width: period1DockHighlighted ? 3 : 1

                                Behavior on color { ColorAnimation { duration: 100 } }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: period1DockHighlighted ? "🧲 Rilascia qui!" : "1️⃣ Period 1 Detached"
                                        color: period1DockHighlighted ? "#4CAF50" : textSecondary
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
                                }

                                // Pulsing border when highlighted
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    radius: 8
                                    border.color: "#4CAF50"
                                    border.width: 4
                                    visible: period1DockHighlighted
                                    opacity: 0.8

                                    SequentialAnimation on opacity {
                                        running: period1DockHighlighted
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 0.4; duration: 300 }
                                        NumberAnimation { to: 1.0; duration: 300 }
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
                                    border.color: decodePanel.isCurrentPeriodEven ? "#4CAF50" : "transparent"
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
                                            color: "#4CAF50"

                                            SequentialAnimation on opacity {
                                                running: decodePanel.isCurrentPeriodEven
                                                loops: Animation.Infinite
                                                NumberAnimation { to: 0.3; duration: 500 }
                                                NumberAnimation { to: 1.0; duration: 500 }
                                            }

                                            // Reset opacity when not active
                                            opacity: decodePanel.isCurrentPeriodEven ? 1.0 : 0.5
                                        }

                                        Text {
                                            text: "Band Activity"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#4CAF50"
                                        }

                                        // ACTIVE badge when current period
                                        Rectangle {
                                            visible: decodePanel.isCurrentPeriodEven
                                            width: 50
                                            height: 16
                                            radius: 8
                                            color: Qt.rgba(76/255, 175/255, 80/255, 0.4)
                                            border.color: "#4CAF50"

                                            Text {
                                                anchors.centerIn: parent
                                                text: "ACTIVE"
                                                font.pixelSize: 9
                                                font.bold: true
                                                color: "#4CAF50"
                                            }

                                            SequentialAnimation on opacity {
                                                running: decodePanel.isCurrentPeriodEven
                                                loops: Animation.Infinite
                                                NumberAnimation { to: 0.6; duration: 600 }
                                                NumberAnimation { to: 1.0; duration: 600 }
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: decodePanel.allDecodes.length + " decodes"
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

                                        // Detach button
                                        Rectangle {
                                            width: 20
                                            height: 20
                                            radius: 4
                                            color: p1DetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
                                            border.color: p1DetachMA.containsMouse ? secondaryCyan : "transparent"

                                            Text {
                                                anchors.centerIn: parent
                                                text: "⇗"
                                                font.pixelSize: 12
                                                color: p1DetachMA.containsMouse ? secondaryCyan : textSecondary
                                            }

                                            MouseArea {
                                                id: p1DetachMA
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    period1Detached = true
                                                    period1FloatingWindow.show()
                                                }
                                            }

                                            ToolTip.visible: p1DetachMA.containsMouse
                                            ToolTip.text: "Sgancia Period 1"
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
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 0

                                        Text { text: "UTC"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: "#4CAF50"; Layout.preferredWidth: period1Panel.utcColumnWidth }
                                        Text { text: "dB"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: "#4CAF50"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dbColumnWidth }
                                        Text { text: "DT"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: "#4CAF50"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dtColumnWidth }
                                        Text { text: "Freq"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: "#4CAF50"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.freqColumnWidth }
                                        Item { Layout.preferredWidth: period1Panel.gapColumnWidth }
                                        Text { text: "Message"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: "#4CAF50"; Layout.fillWidth: true }
                                        Item {
                                            Layout.preferredWidth: period1Panel.dxccColumnWidth
                                            Layout.fillHeight: true
                                            Text {
                                                anchors.fill: parent
                                                text: "DXCC"
                                                font.family: "Consolas"
                                                font.pixelSize: Math.round(11 * fs)
                                                font.bold: true
                                                color: "#4CAF50"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                        Item {
                                            Layout.preferredWidth: period1Panel.azColumnWidth
                                            Layout.fillHeight: true
                                            Text {
                                                anchors.fill: parent
                                                text: "Az"
                                                font.family: "Consolas"
                                                font.pixelSize: Math.round(11 * fs)
                                                font.bold: true
                                                color: "#4CAF50"
                                                horizontalAlignment: Text.AlignHCenter
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
                                        model: decodePanel.allDecodes
                                        spacing: 1
                                        onCountChanged: positionViewAtEnd()

                                        ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }

                                        delegate: Rectangle {
                                            width: evenPeriodList.width - 8
                                            height: Math.round(26 * fs)
                                            color: modelData.isTx ? Qt.rgba(241/255, 196/255, 15/255, 0.25) :
                                                   modelData.isMyCall ? Qt.rgba(244/255, 67/255, 54/255, 0.25) :
                                                   modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12) :
                                                   decodePanel.isAtRxFrequency(modelData.freq || "0", modelData) ? Qt.rgba(76/255, 175/255, 80/255, 0.2) :
                                                   index % 2 === 0 ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                                            radius: 2

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                onClicked: function(mouse) {
                                                    if (modelData.isTx) return
                                                    if (mouse.button === Qt.LeftButton) {
                                                        // Sinistro = imposta TX freq
                                                        bridge.txFrequency = parseInt(modelData.freq || "0")
                                                    } else if (mouse.button === Qt.RightButton) {
                                                        // Destro = imposta RX freq
                                                        bridge.rxFrequency = parseInt(modelData.freq || "0")
                                                    }
                                                }
                                                onDoubleClicked: function(mouse) {
                                                    if (!modelData.isTx && mouse.button === Qt.LeftButton)
                                                        decodePanel.handleDecodeDoubleClick(modelData)
                                                }
                                                // IU8LMC: Show tooltip on hover
                                                onContainsMouseChanged: {
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
                                                anchors.fill: parent
                                                anchors.leftMargin: 6
                                                anchors.rightMargin: 6
                                                spacing: 0

                                                Text { text: decodePanel.formatUtcForDisplay(modelData.time); font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; Layout.preferredWidth: period1Panel.utcColumnWidth }
                                                Text { text: modelData.db || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : parseInt(modelData.db || "0") > -5 ? accentGreen : parseInt(modelData.db || "0") > -15 ? secondaryCyan : textSecondary; font.bold: modelData.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dbColumnWidth }
                                                Text { text: modelData.dt || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.dtColumnWidth }
                                                Text { text: modelData.freq || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : decodePanel.isAtRxFrequency(modelData.freq || "0", modelData) ? "#4CAF50" : secondaryCyan; font.bold: modelData.isTx || decodePanel.isAtRxFrequency(modelData.freq || "0", modelData); horizontalAlignment: Text.AlignRight; Layout.preferredWidth: period1Panel.freqColumnWidth }
                                                Item { Layout.preferredWidth: period1Panel.gapColumnWidth }
                                                Text { text: modelData.message || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); font.bold: modelData.isTx || modelData.isCQ || modelData.isMyCall || (modelData.dxIsNewCountry === true) || (modelData.dxIsMostWanted === true); color: modelData.isTx ? "#f1c40f" : getDxccColor(modelData); Layout.fillWidth: true; Layout.minimumWidth: period1Panel.messageMinWidth; elide: Text.ElideRight }
                                                Item {
                                                    Layout.preferredWidth: period1Panel.dxccColumnWidth
                                                    Layout.fillHeight: true
                                                    Text {
                                                        anchors.fill: parent
                                                        text: modelData.dxCountry || ""
                                                        font.family: "Consolas"
                                                        font.pixelSize: Math.round(11 * fs)
                                                        color: modelData.dxIsNewCountry ? colorNewCountry : modelData.dxIsMostWanted ? colorMostWanted : textSecondary
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                        elide: Text.ElideRight
                                                    }
                                                }
                                                Item {
                                                    Layout.preferredWidth: period1Panel.azColumnWidth
                                                    Layout.fillHeight: true
                                                    Text {
                                                        anchors.fill: parent
                                                        text: formatBearingDegrees(modelData.dxBearing)
                                                        font.family: "Consolas"
                                                        font.pixelSize: Math.round(11 * fs)
                                                        color: secondaryCyan
                                                        horizontalAlignment: Text.AlignHCenter
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
                                    }
                                }
                            }
                        }

                        // ========== RIGHT: RX Frequency ==========
                        Rectangle {
                            id: rxFreqPanel
                            SplitView.fillWidth: true
                            SplitView.minimumWidth: 260
                            readonly property bool compactColumns: width < 430
                            readonly property bool compactHeader: width < 350
                            readonly property int utcColumnWidth: compactColumns ? 58 : 72
                            readonly property int dbColumnWidth: compactColumns ? 26 : 30
                            readonly property int dtColumnWidth: compactColumns ? 30 : 35
                            readonly property int gapColumnWidth: compactColumns ? 4 : 6
                            readonly property int headerBadgeWidth: compactHeader ? 62 : 70
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
                                        text: rxFreqDockHighlighted ? "🧲 Rilascia qui!" : "📡 RX Freq Detached"
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
                                        running: rxFreqDockHighlighted
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 0.4; duration: 300 }
                                        NumberAnimation { to: 1.0; duration: 300 }
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
                                            text: rxFreqPanel.compactHeader ? "RX Freq" : "RX Frequency"
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
                                                font.family: "Consolas"
                                                font.pixelSize: 10
                                                font.bold: true
                                                color: primaryBlue
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: {
                                                return decodePanel.currentRxDecodes().length + " msgs"
                                            }
                                            font.pixelSize: 10
                                            color: textSecondary
                                            visible: !rxFreqPanel.compactHeader
                                        }

                                        // Detach button
                                        Rectangle {
                                            width: 20
                                            height: 20
                                            radius: 4
                                            color: rxDetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
                                            border.color: rxDetachMA.containsMouse ? secondaryCyan : "transparent"

                                            Text {
                                                anchors.centerIn: parent
                                                text: "⇗"
                                                font.pixelSize: 12
                                                color: rxDetachMA.containsMouse ? secondaryCyan : textSecondary
                                            }

                                            MouseArea {
                                                id: rxDetachMA
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    rxFreqDetached = true
                                                    rxFreqFloatingWindow.show()
                                                }
                                            }

                                            ToolTip.visible: rxDetachMA.containsMouse
                                            ToolTip.text: "Sgancia RX Frequency"
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
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 0

                                        Text { text: "UTC"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: primaryBlue; Layout.preferredWidth: rxFreqPanel.utcColumnWidth }
                                        Text { text: "dB"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dbColumnWidth }
                                        Text { text: "DT"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: primaryBlue; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dtColumnWidth }
                                        Item { Layout.preferredWidth: rxFreqPanel.gapColumnWidth }
                                        Text { text: "Message"; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); font.bold: true; color: primaryBlue; Layout.fillWidth: true }
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
                                        onCountChanged: positionViewAtEnd()

                                        model: {
                                            return decodePanel.currentRxDecodes()
                                        }

                                        ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }

                                        delegate: Rectangle {
                                            width: rxFrequencyList.width - 8
                                            height: Math.round(26 * fs)
                                            color: modelData.isTx ? Qt.rgba(241/255, 196/255, 15/255, 0.3) :
                                                   modelData.isMyCall ? Qt.rgba(244/255, 67/255, 54/255, 0.3) :
                                                   modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) :
                                                   index % 2 === 0 ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.08) : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15)
                                            radius: 2

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                onClicked: function(mouse) {
                                                    if (modelData.isTx) return
                                                    if (mouse.button === Qt.LeftButton) {
                                                        // Sinistro = imposta TX freq
                                                        bridge.txFrequency = parseInt(modelData.freq || "0")
                                                    } else if (mouse.button === Qt.RightButton) {
                                                        // Destro = imposta RX freq
                                                        bridge.rxFrequency = parseInt(modelData.freq || "0")
                                                    }
                                                }
                                                onDoubleClicked: function(mouse) {
                                                    if (!modelData.isTx && mouse.button === Qt.LeftButton)
                                                        decodePanel.handleDecodeDoubleClick(modelData)
                                                }
                                                // IU8LMC: Show DXCC tooltip on hover
                                                onContainsMouseChanged: {
                                                    if (containsMouse && modelData.dxCountry && modelData.dxCountry !== "") {
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
                                                    if (containsMouse && dxccTooltipVisible) {
                                                        var pos = mapToGlobal(mouseX, mouseY)
                                                        dxccTooltipX = pos.x - mainWindow.x
                                                        dxccTooltipY = pos.y - mainWindow.y
                                                    }
                                                }
                                            }

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 6
                                                anchors.rightMargin: 6
                                                spacing: 0

                                                Text { text: decodePanel.formatUtcForDisplay(modelData.time); font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; Layout.preferredWidth: rxFreqPanel.utcColumnWidth }
                                                Text { text: modelData.db || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : parseInt(modelData.db || "0") > -5 ? accentGreen : parseInt(modelData.db || "0") > -15 ? secondaryCyan : textSecondary; font.bold: modelData.isTx === true; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dbColumnWidth }
                                                Text { text: modelData.dt || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); color: modelData.isTx ? "#f1c40f" : textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: rxFreqPanel.dtColumnWidth }
                                                Item { Layout.preferredWidth: rxFreqPanel.gapColumnWidth }
                                                Text { text: modelData.message || ""; font.family: "Consolas"; font.pixelSize: Math.round(12 * fs); font.bold: modelData.isTx || modelData.isCQ || modelData.isMyCall || (modelData.dxIsNewCountry === true) || (modelData.dxIsMostWanted === true); color: modelData.isTx ? "#f1c40f" : getDxccColor(modelData); Layout.fillWidth: true; elide: Text.ElideRight }
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
                    }
                }
            } // End mainVerticalSplit (Waterfall top, Decode panels bottom)

            // Resize handle between decode panels and TX panel
            Rectangle {
                id: verticalResizeHandle
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: queuePanelComponent.visible ? queuePanelComponent.top : txPanelContainer.top
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

            // Queue Panel (between decode panels and TX panel)
            QueuePanel {
                id: queuePanelComponent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: txPanelContainer.top
                anchors.bottomMargin: 4
                height: 140
                engine: bridge
                visible: bridge.multiAnswerMode
            }

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
                    border.color: txPanelDockHighlighted ? "#f44336" : glassBorder
                    border.width: txPanelDockHighlighted ? 3 : 1

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: txPanelDockHighlighted ? "🧲 Rilascia qui!" : "📻 TX Panel Detached"
                            color: txPanelDockHighlighted ? "#f44336" : textSecondary
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
                        border.color: "#f44336"
                        border.width: 4
                        visible: txPanelDockHighlighted
                        opacity: 0.8

                        SequentialAnimation on opacity {
                            running: txPanelDockHighlighted
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.4; duration: 300 }
                            NumberAnimation { to: 1.0; duration: 300 }
                        }
                    }
                }

                // Actual TxPanel content
                TxPanel {
                    id: txPanelComponent
                    anchors.fill: parent
                    engine: bridge
                    visible: !txPanelDetached
                    onMamWindowRequested: mamWindow.open()

                    // Detach button overlay at top-right
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        width: 24
                        height: 24
                        radius: 4
                        z: 200
                        color: txDetachMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: txDetachMA.containsMouse ? "#f44336" : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.2)

                        Text {
                            anchors.centerIn: parent
                            text: "⇗"
                            font.pixelSize: 12
                            color: txDetachMA.containsMouse ? "#f44336" : textSecondary
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
                        ToolTip.text: "Sgancia TX Panel"
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
                color: "#f0202020"
                border.color: "#808080"
                border.width: 1
                radius: 4

                Text {
                    id: dxccTooltipLabel
                    anchors.centerIn: parent
                    text: dxccTooltipText
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.family: "Segoe UI"
                }
            }
        }

        // Status Bar
        StatusBar {
            Layout.fillWidth: true
            audioLevel: bridge ? bridge.audioLevel : 0.0
            signalLevel: bridge ? bridge.sMeter : 0.0
            monitoring: bridge ? bridge.monitoring : false
            transmitting: bridge ? bridge.transmitting : false
            decoding: bridge ? bridge.decoding : false
            catStatus: bridge && bridge.catConnected ? "Connected" : "Disconnected"
        }
    }

    // Settings Dialog
    SettingsDialog {
        id: settingsDialog
    }

    Popup {
        id: setupMenuPopup
        parent: Overlay.overlay
        width: 520
        modal: false
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 1
            radius: 14
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 56
                Layout.preferredHeight: implicitHeight
                color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.95)
                radius: 14

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Setup"
                    font.pixelSize: 18
                    font.bold: true
                    color: secondaryCyan
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Preferenze Decodium 3"
                    font.pixelSize: 11
                    color: textSecondary
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.topMargin: 14
                Layout.bottomMargin: 14
                columns: 2
                rowSpacing: 10
                columnSpacing: 10

                Repeater {
                    model: legacySetupSections

                    delegate: Button {
                        id: setupEntryButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 58

                        contentItem: Column {
                            spacing: 2
                            anchors.centerIn: parent

                            Text {
                                text: modelData.title
                                font.pixelSize: 14
                                font.bold: true
                                color: textPrimary
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                text: modelData.description
                                font.pixelSize: 10
                                color: textSecondary
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        background: Rectangle {
                            color: setupEntryButton.down
                                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.26)
                                   : (setupEntryButton.hovered ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.16)
                                                     : Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.92))
                            border.color: setupEntryButton.hovered ? secondaryCyan : glassBorder
                            border.width: 1
                            radius: 10
                        }

                        onClicked: {
                            setupMenuPopup.close()
                            bridge.openSetupSettings(modelData.tabIndex)
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                Layout.preferredHeight: implicitHeight
                color: glassBorder
            }

            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: 48

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Il setup storico resta disponibile completo. Time Sync / NTP apre il pannello legacy con server, sync immediato e diagnostica."
                        font.pixelSize: 11
                        color: textSecondary
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: "Apri Generale"
                        onClicked: {
                            setupMenuPopup.close()
                            bridge.openSetupSettings(0)
                        }

                        contentItem: Text {
                            text: parent.text
                            color: secondaryCyan
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.14)
                            border.color: secondaryCyan
                            border.width: 1
                            radius: 8
                        }
                    }

                    Button {
                        text: "Time Sync / NTP"
                        onClicked: {
                            setupMenuPopup.close()
                            bridge.openTimeSyncSettings()
                        }

                        contentItem: Text {
                            text: parent.text
                            color: warningOrange
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.12)
                            border.color: warningOrange
                            border.width: 1
                            radius: 8
                        }
                    }
                }
            }
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
                        text: "Il backend radio legacy ha segnalato un problema."
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
                text: rigErrorDetailsVisible ? "Nascondi dettagli" : "Mostra dettagli"
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
                text: "Configura radio"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    rigErrorDialog.close()
                    bridge.openSetupSettings(1)
                }
            }

            Button {
                text: "Riprova"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    rigErrorDialog.close()
                    bridge.retryRigConnection()
                }
            }

            Button {
                text: "Chiudi"
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
                    font.family: "Consolas"
                    font.pixelSize: 36
                    font.bold: true
                    font.letterSpacing: 3
                    color: badgeColor
                }

                Text {
                    text: badgeSubText
                    font.family: "Consolas"
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

    // Log Window
    LogWindow {
        id: logWindow
    }

    // Macro Dialog
    MacroDialog {
        id: macroDialog
    }

    // Astro Window
    AstroWindow {
        id: astroWindow
    }

    // Rig Control Dialog
    RigControlDialog {
        id: rigControlDialog
    }

    // Apri CAT dialog quando bridge.openCatSettings() viene chiamato
    Connections {
        target: bridge
        function onErrorMessage(msg) {
            warningDialogTitle = "Errore"
            warningDialogSummary = msg
            warningDialogDetails = ""
            warningDialogDetailsVisible = false
            warningDialog.open()
        }
        function onWarningRaised(title, summary, details) {
            warningDialogTitle = title
            warningDialogSummary = summary
            warningDialogDetails = details
            warningDialogDetailsVisible = false
            warningDialog.open()
        }
        function onSetupSettingsRequested(tabIndex) {
            settingsDialog.openTab(tabIndex)
        }
        function onTimeSyncSettingsRequested() {
            timeSyncPanelVisible = true
        }
        function onCatSettingsRequested() {
            rigControlDialog.open()
        }
        function onRigErrorRaised(title, summary, details) {
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
        width: 560
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        title: warningDialogTitle

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
                        text: warningDialogTitle.length > 0 ? warningDialogTitle : "Errore"
                        font.pixelSize: 18
                        font.bold: true
                        color: accentOrange
                    }

                    Text {
                        text: "Decodium ha segnalato un problema non bloccante."
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
                text: warningDialogSummary
                font.pixelSize: 15
                color: textPrimary
                wrapMode: Text.WordWrap
            }

            Button {
                visible: warningDialogDetails.length > 0
                text: warningDialogDetailsVisible ? "Nascondi dettagli" : "Mostra dettagli"
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

            Button {
                text: "OK"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: warningDialog.close()
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

    // Main Menu (Hamburger)
    Menu {
        id: mainMenu
        x: 60
        y: 90
        background: Rectangle {
            implicitWidth: 230
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan
            border.width: 1
            radius: 10
        }

        MenuItem {
            text: "About Decodium"
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
            text: "Storia del Programma"
            onTriggered: { infoDialog.currentTab = 1; infoDialog.open() }

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
            text: "Guida Rapida"
            onTriggered: { infoDialog.currentTab = 3; infoDialog.open() }

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

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        MenuItem {
            text: bridge.themeManager.isLightTheme ? "☀ Stellar Light" : "☽ Ocean Blue"
            onTriggered: {
                if (bridge.themeManager.isLightTheme)
                    bridge.themeManager.applyThemeByName("Ocean Blue")
                else
                    bridge.themeManager.applyThemeByName("Stellar Light")
            }

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                font.bold: true
                color: secondaryCyan
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        MenuItem {
            text: "Contatta Sviluppatore"
            onTriggered: Qt.openUrlExternally("mailto:iu8lmc@gmail.com")

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: secondaryCyan
                leftPadding: 10
            }
        }

        MenuItem {
            text: "Invia Feedback"
            onTriggered: { infoDialog.currentTab = 2; infoDialog.open() }

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

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        MenuItem {
            text: "PSK Reporter"
            onTriggered: Qt.openUrlExternally("https://pskreporter.info")

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
            text: "QRZ.com"
            onTriggered: Qt.openUrlExternally("https://www.qrz.com")

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
            text: "Link Utili..."
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
            text: "⌨ Scorciatoie Tastiera"
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
            text: bridge.swlMode ? "✓ SWL Mode (Solo RX)" : "☐ SWL Mode (Solo RX)"
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
            text: bridge.multiAnswerMode ? "✓ Multi-Answer Mode" : "☐ Multi-Answer Mode"
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
            text: "MAM Window..."
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
            text: "📂 Apri Cartella ALL.TXT"
            onTriggered: Qt.openUrlExternally("file:///" + bridge.logAllTxtPath)

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
            text: bridge.txWatchdogMode > 0 ? "✓ TX Watchdog" : "☐ TX Watchdog"
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
            text: bridge.splitMode ? "✓ Split Mode" : "☐ Split Mode"
            onTriggered: bridge.splitMode = !bridge.splitMode

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.splitMode ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: bridge.contestType > 0 ? "✓ Contest Mode" : "☐ Contest Mode"
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
            text: bridge.filterCqOnly ? "✓ Solo CQ" : "☐ Solo CQ"
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
            text: bridge.filterMyCallOnly ? "✓ Solo My Call" : "☐ Solo My Call"
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

        MenuItem {
            text: bridge.zapEnabled ? "✓ ZAP Mode" : "☐ ZAP Mode"
            onTriggered: bridge.zapEnabled = !bridge.zapEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.zapEnabled ? accentOrange : textSecondary
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
            text: bridge.deepSearchEnabled ? "✓ Deep Search" : "☐ Deep Search"
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
            text: bridge.avgDecodeEnabled ? "✓ Avg Decode" : "☐ Avg Decode"
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
            text: bridge.vhfUhfFeatures ? "✓ VHF/UHF Features" : "☐ VHF/UHF Features"
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
            text: bridge.recordRxEnabled ? "✓ Registra RX" : "☐ Registra RX"
            onTriggered: bridge.recordRxEnabled = !bridge.recordRxEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.recordRxEnabled ? "#f44336" : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: bridge.recordTxEnabled ? "✓ Registra TX" : "☐ Registra TX"
            onTriggered: bridge.recordTxEnabled = !bridge.recordTxEnabled

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.recordTxEnabled ? "#f44336" : textSecondary
                leftPadding: 10
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: glassBorder
            }
        }

        // ===== AUTO SEQUENCE OPTIONS =====
        MenuItem {
            text: bridge.confirm73 ? "✓ Conferma 73" : "☐ Conferma 73"
            onTriggered: bridge.confirm73 = !bridge.confirm73

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.confirm73 ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: bridge.startFromTx2 ? "✓ Start da TX2" : "☐ Start da TX2"
            onTriggered: bridge.startFromTx2 = !bridge.startFromTx2

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.startFromTx2 ? successGreen : textSecondary
                leftPadding: 10
            }
        }

        MenuItem {
            text: bridge.directLogQso ? "✓ Log QSO Diretto" : "☐ Log QSO Diretto"
            onTriggered: bridge.directLogQso = !bridge.directLogQso

            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: bridge.directLogQso ? successGreen : textSecondary
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
            text: bridge.alertOnCq ? "✓ Alert su CQ" : "☐ Alert su CQ"
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
            text: "🎨 Color Highlighting..."
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
            text: "📡 QSY..."
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
            text: bridge.updateAvailable ? "🆕 Aggiorna Decodium v" + bridge.latestVersion : "☁ Controlla Aggiornamenti"
            onTriggered: {
                if (bridge.updateAvailable)
                    Qt.openUrlExternally("https://github.com/IU8LMC/decodium/releases/latest")
                else
                    bridge.checkForUpdates()
            }
            background: Rectangle {
                color: parent.highlighted ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                radius: 6
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: 12
                color: bridge.updateAvailable ? "#FF9800" : textSecondary; leftPadding: 10
            }
        }

        MenuItem {
            text: "📂 Esporta Cabrillo..."
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
            text: bridge.ctyDatUpdating ? "⏳ cty.dat download..." : "🌍 Aggiorna cty.dat"
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
            text: timeSyncPanelVisible ? "✓ Time Sync Panel" : "☐ Time Sync Panel"
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
            text: activeStationsPanelVisible ? "✓ Active Stations" : "☐ Active Stations"
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
            text: bridge.foxMode ? "✓ Fox Mode (Caller Queue)" : "☐ Fox Mode (Caller Queue)"
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
            text: astroPanelVisible ? "✓ Astro / EME" : "☐ Astro / EME"
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
            text: dxClusterPanelVisible ? "✓ DX Cluster" : "☐ DX Cluster"
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
            text: bridge.alertOnMyCall ? "✓ Alert su My Call" : "☐ Alert su My Call"
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
        title: "Esporta Cabrillo"
        anchors.centerIn: parent
        width: 400
        modal: true

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: secondaryCyan; border.width: 1; radius: 10
        }

        contentItem: Column {
            spacing: 12; padding: 16
            Text { text: "Percorso file output:"; font.pixelSize: 12; color: textPrimary }
            TextField {
                id: cabrilloPath
                width: 360
                text: (Qt.platform.os === "windows"
                       ? "C:/Users/IU8LMC/Documents/" : "~/")
                      + bridge.callsign + "_" + Qt.formatDate(new Date(), "yyyyMMdd") + ".cbr"
                font.family: "Consolas"; font.pixelSize: 11
                color: textPrimary
                background: Rectangle {
                    color: Qt.rgba(1,1,1,0.07); border.color: glassBorder; radius: 4
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "Esporta"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    if (bridge.exportCabrillo(cabrilloPath.text))
                        cabrilloDlg.close()
                }
            }
            Button {
                text: "Annulla"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }

    // ===== TX WATCHDOG DIALOG =====
    Dialog {
        id: txWatchdogDialog
        title: "TX Watchdog"
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
                text: "Modalità Watchdog"
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
                    text: "Tempo"
                    checked: bridge.txWatchdogMode === 1
                    onClicked: bridge.txWatchdogMode = 1
                }
                RadioButton {
                    id: wdCount
                    text: "Conteggio"
                    checked: bridge.txWatchdogMode === 2
                    onClicked: bridge.txWatchdogMode = 2
                }
            }

            Row {
                spacing: 10
                visible: bridge.txWatchdogMode === 1
                Text { text: "Tempo (min):"; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
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
            Button { text: "Chiudi"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
    }

    // ===== CONTEST MODE DIALOG =====
    Dialog {
        id: contestDialog
        title: "Contest Mode"
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
                text: "Tipo Contest"
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
                placeholderText: "Es: 599 001"
                visible: bridge.contestType > 0
            }

            Row {
                spacing: 10
                visible: bridge.contestType > 0
                Text { text: "Numero Seriale:"; color: textPrimary; anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    from: 1; to: 9999
                    value: bridge.contestNumber
                    onValueChanged: bridge.contestNumber = value
                }
            }
        }

        footer: DialogButtonBox {
            Button { text: "Chiudi"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
    }

    // ===== KEYBOARD SHORTCUTS DIALOG =====
    Dialog {
        id: keyboardShortcutsDialog
        title: "Scorciatoie Tastiera"
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
                text: "TRASMISSIONE"
                font.pixelSize: 14
                font.bold: true
                color: accentOrange
            }
            Text { text: "F1 - F7: Seleziona TX1 - TX7"; font.pixelSize: 12; color: textPrimary }
            Text { text: "F9: Toggle RX Only First/Second"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Escape: Halt (Stop TX immediato)"; font.pixelSize: 12; color: textPrimary }

            Rectangle { height: 1; width: parent.width - 40; color: glassBorder }

            Text {
                text: "CONTROLLI (Ctrl+)"
                font.pixelSize: 14
                font.bold: true
                color: secondaryCyan
            }
            Text { text: "Ctrl+A: Toggle Auto Sequence"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Ctrl+G: Genera tutti i messaggi TX"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Ctrl+Z: Toggle ZAP mode"; font.pixelSize: 12; color: textPrimary }

            Rectangle { height: 1; width: parent.width - 40; color: glassBorder }

            Text {
                text: "AZIONI (Alt+)"
                font.pixelSize: 14
                font.bold: true
                color: successGreen
            }
            Text { text: "Alt+L: Log QSO corrente"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Alt+M: Clear lista decode (Monitor)"; font.pixelSize: 12; color: textPrimary }
            Text { text: "Alt+S: Stop TX"; font.pixelSize: 12; color: textPrimary }
        }

        footer: DialogButtonBox {
            Button {
                text: "Chiudi"
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
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "Waterfall - Decodium"
        color: "transparent"

        // Position to right of main window initially
        x: mainWindow.x + mainWindow.width + 20
        y: mainWindow.y + 50
        Component.onCompleted: mainWindow.restoreFloatingWindowState(waterfallWindow, "waterfallWindow", "waterfallDetached", "waterfallMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        // Handle window close
        onClosing: function(close) {
            waterfallDetached = false
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
                            font.family: "Consolas"
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
                                        if (waterfallDisplayDetached.maxFreq < 5000) {
                                            waterfallDisplayDetached.maxFreq += 500
                                        }
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
                                        if (waterfallDisplayDetached.maxFreq > 1000) {
                                            waterfallDisplayDetached.maxFreq -= 500
                                        }
                                    }
                                }
                            }

                            Text {
                                text: waterfallDisplayDetached.minFreq + "-" + waterfallDisplayDetached.maxFreq + " Hz"
                                font.pixelSize: 10
                                font.family: "Consolas"
                                color: textSecondary
                            }
                        }

                        // Info text
                        Text {
                            text: "— trascina sulla finestra principale per agganciare"
                            font.pixelSize: 10
                            color: textSecondary
                            opacity: 0.6
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
                            ToolTip.text: "Riduci a icona"
                            ToolTip.delay: 500
                        }

                        // Close button
                        Rectangle {
                            width: 28
                            height: 24
                            radius: 4
                            color: closeMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                            border.color: closeMA.containsMouse ? "#f44336" : glassBorder

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                font.pixelSize: 12
                                font.bold: true
                                color: closeMA.containsMouse ? "#f44336" : textPrimary
                            }

                            MouseArea {
                                id: closeMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    waterfallDetached = false
                                    waterfallMinimized = false
                                    waterfallWindow.close()
                                }
                            }

                            ToolTip.visible: closeMA.containsMouse
                            ToolTip.text: "Chiudi e riaggancia"
                            ToolTip.delay: 500
                        }
                    }
                }

                // Waterfall content
                Waterfall {
                    id: waterfallDisplayDetached
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: waterfallDetached
                    showControls: true
                    minFreq: 200
                    maxFreq: 3000
                    spectrumHeight: 150

                    onFrequencySelected: function(freq) {
                        bridge.rxFrequency = freq              // tasto destro = RX
                    }
                    onTxFrequencySelected: function(freq) {
                        bridge.txFrequency = freq              // tasto sinistro = TX
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
                            font.family: "Consolas"
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
                                running: bridge.monitoring
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.4; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 }
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
                            border.color: logFloatCloseMA.containsMouse ? "#f44336" : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: logFloatCloseMA.containsMouse ? "#f44336" : textPrimary }
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
                    active: logWindowDetached
                    sourceComponent: logContentComponent
                }
            }
        }
    }

    // Log content component (shared)
    Component {
        id: logContentComponent
        LogWindowContent { }
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
                            border.color: astroFloatCloseMA.containsMouse ? "#f44336" : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: astroFloatCloseMA.containsMouse ? "#f44336" : textPrimary }
                            MouseArea { id: astroFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { astroWindowDetached = false; astroWindowMinimized = false; astroFloatingWindow.close() }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: astroWindowDetached
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
                            border.color: macroFloatCloseMA.containsMouse ? "#f44336" : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: macroFloatCloseMA.containsMouse ? "#f44336" : textPrimary }
                            MouseArea { id: macroFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { macroDialogDetached = false; macroDialogMinimized = false; macroFloatingWindow.close() }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: macroDialogDetached
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
                            border.color: rigFloatCloseMA.containsMouse ? "#f44336" : glassBorder
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 12; font.bold: true; color: rigFloatCloseMA.containsMouse ? "#f44336" : textPrimary }
                            MouseArea { id: rigFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { rigControlDetached = false; rigControlMinimized = false; rigFloatingWindow.close() }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: rigControlDetached
                    sourceComponent: rigContentComponent
                }
            }
        }
    }

    Component {
        id: rigContentComponent
        RigControlDialogContent { }
    }

    // ========== DETACHABLE PERIOD 1 WINDOW ==========
    Window {
        id: period1FloatingWindow
        width: 500
        height: 400
        minimumWidth: 350
        minimumHeight: 250
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "Period 1 - Decodium"
        color: "transparent"

        x: mainWindow.x + 100
        y: mainWindow.y + 150
        Component.onCompleted: mainWindow.restoreFloatingWindowState(period1FloatingWindow, "period1FloatingWindow", "period1Detached", "period1Minimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            period1Detached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: "#4CAF50"
            border.width: 2

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
                                period1DockHighlighted = false
                                period1Detached = false
                                period1FloatingWindow.close()
                            }
                            hasMovedAway = false
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text { text: "⋮⋮"; font.pixelSize: 12; color: "#4CAF50" }
                        Rectangle { width: 10; height: 10; radius: 5; color: "#4CAF50" }
                        Text { text: "Period 1 (0,30s)"; font.pixelSize: 14; font.bold: true; color: "#4CAF50" }
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: p1FloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 16; font.bold: true; color: p1FloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: p1FloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { period1Minimized = true; period1FloatingWindow.hide() }
                            }
                        }

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: p1FloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 11; font.bold: true; color: p1FloatCloseMA.containsMouse ? "#f44336" : textPrimary }
                            MouseArea { id: p1FloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { period1Detached = false; period1Minimized = false; period1FloatingWindow.close() }
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
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        spacing: 1
                        model: decodePanel.allDecodes
                        ScrollBar.vertical: ScrollBar { active: true }

                        delegate: Rectangle {
                            width: parent ? parent.width - 8 : 100
                            height: Math.round(24 * fs)
                            radius: 3
                            color: modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.05)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 6
                                Text { text: decodePanel.formatUtcForDisplay(modelData.time); font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); color: textSecondary; Layout.preferredWidth: 64 }
                                Text { text: modelData.db || ""; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); color: parseInt(modelData.db || "0") > -10 ? accentGreen : textSecondary; Layout.preferredWidth: 28 }
                                Text { text: modelData.message || ""; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); color: getDxccColor(modelData); Layout.fillWidth: true; elide: Text.ElideRight }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onDoubleClicked: { if (!modelData.isTx) decodePanel.handleDecodeDoubleClick(modelData) }
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
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "RX Frequency - Decodium"
        color: "transparent"

        x: mainWindow.x + 300
        y: mainWindow.y + 250
        Component.onCompleted: mainWindow.restoreFloatingWindowState(rxFreqFloatingWindow, "rxFreqFloatingWindow", "rxFreqDetached", "rxFreqMinimized")
        onXChanged: mainWindow.scheduleWindowStateSave()
        onYChanged: mainWindow.scheduleWindowStateSave()
        onWidthChanged: mainWindow.scheduleWindowStateSave()
        onHeightChanged: mainWindow.scheduleWindowStateSave()

        onClosing: function(close) {
            rxFreqDetached = false
            close.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            radius: 10
            border.color: primaryBlue
            border.width: 2

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
                                rxFreqDockHighlighted = false
                                rxFreqDetached = false
                                rxFreqFloatingWindow.close()
                            }
                            hasMovedAway = false
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text { text: "⋮⋮"; font.pixelSize: 12; color: primaryBlue }
                        Rectangle { width: 10; height: 10; radius: 5; color: primaryBlue }
                        Text { text: "RX Frequency"; font.pixelSize: 14; font.bold: true; color: primaryBlue }

                        Rectangle {
                            width: 70
                            height: 20
                            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                            radius: 4
                            border.color: primaryBlue

                            Text {
                                anchors.centerIn: parent
                                text: bridge.rxFrequency + " Hz"
                                font.family: "Consolas"
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: rxFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 16; font.bold: true; color: rxFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: rxFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { rxFreqMinimized = true; rxFreqFloatingWindow.hide() }
                            }
                        }

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: rxFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 11; font.bold: true; color: rxFloatCloseMA.containsMouse ? "#f44336" : textPrimary }
                            MouseArea { id: rxFloatCloseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { rxFreqDetached = false; rxFreqMinimized = false; rxFreqFloatingWindow.close() }
                            }
                        }
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
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        spacing: 1
                        model: {
                            return decodePanel.currentRxDecodes()
                        }
                        ScrollBar.vertical: ScrollBar { active: true }

                        delegate: Rectangle {
                            width: parent ? parent.width - 8 : 100
                            height: Math.round(24 * fs)
                            radius: 3
                            color: modelData.isCQ ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.05)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 6
                                Text { text: decodePanel.formatUtcForDisplay(modelData.time); font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); color: textSecondary; Layout.preferredWidth: 64 }
                                Text { text: modelData.db || ""; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); color: parseInt(modelData.db || "0") > -10 ? accentGreen : textSecondary; Layout.preferredWidth: 28 }
                                Text { text: modelData.message || ""; font.family: "Consolas"; font.pixelSize: Math.round(11 * fs); color: getDxccColor(modelData); Layout.fillWidth: true; elide: Text.ElideRight }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onDoubleClicked: { if (!modelData.isTx) decodePanel.handleDecodeDoubleClick(modelData) }
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
            border.color: "#f44336"
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

                        Text { text: "⋮⋮"; font.pixelSize: 12; color: "#f44336" }
                        Rectangle { width: 10; height: 10; radius: 5; color: "#f44336" }
                        Text { text: "TX Panel"; font.pixelSize: 14; font.bold: true; color: "#f44336" }
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: txFloatMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 16; font.bold: true; color: txFloatMinMA.containsMouse ? "#ffc107" : textPrimary }
                            MouseArea { id: txFloatMinMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { txPanelMinimized = true; txPanelFloatingWindow.hide() }
                            }
                        }

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: txFloatCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 11; font.bold: true; color: txFloatCloseMA.containsMouse ? "#f44336" : textPrimary }
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
                    onMamWindowRequested: mamWindow.open()
                }
            }
        }
    }

    // ===== GAP 3 — Pannelli floating draggabili =====

    // TimeSyncPanel — angolo in alto a destra, togglabile da menu
    Item {
        id: timeSyncOverlay
        visible: timeSyncPanelVisible
        z: 200
        x: mainWindow.width - width - 12
        y: 100
        width: 360
        height: timeSyncLoader.item ? timeSyncLoader.item.implicitHeight : 28

        MouseArea {
            anchors.fill: parent
            drag.target: timeSyncOverlay
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0; drag.maximumX: mainWindow.width - timeSyncOverlay.width
            drag.minimumY: 0; drag.maximumY: mainWindow.height - 28
        }

        Loader {
            id: timeSyncLoader
            anchors.fill: parent
            active: timeSyncPanelVisible
            source: "../panels/TimeSyncPanel.qml"
        }
    }

    // ActiveStationsPanel — posizione sotto TimeSyncPanel
    Item {
        id: activeStationsOverlay
        visible: activeStationsPanelVisible
        z: 200
        x: mainWindow.width - width - 12
        y: timeSyncPanelVisible ? 100 + timeSyncOverlay.height + 8 : 100
        width: 360
        height: 280

        MouseArea {
            anchors.fill: parent
            drag.target: activeStationsOverlay
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0; drag.maximumX: mainWindow.width - activeStationsOverlay.width
            drag.minimumY: 0; drag.maximumY: mainWindow.height - activeStationsOverlay.height
        }

        Loader {
            id: activeStationsLoader
            anchors.fill: parent
            active: activeStationsPanelVisible
            source: "../panels/ActiveStationsPanel.qml"
        }
    }

    // CallerQueuePanel — visibile solo in Fox mode, angolo in basso a destra
    Item {
        id: callerQueueOverlay
        visible: bridge.foxMode
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
            active: bridge.foxMode
            source: "../panels/CallerQueuePanel.qml"
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
        z: 210
        x: mainWindow.width - width - 12
        y: 60
        onCloseRequested: dxClusterPanelVisible = false
    }

    // ── Splash Screen Decodium 4.0 Core Shannon ─────────────────────────────
    SplashScreen {
        id: splash
        anchors.fill: parent
        visible: true
        onFinished: splash.visible = false
    }
}
