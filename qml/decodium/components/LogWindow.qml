/* Decodium 2.5 - QSO Log Window
 * Full Glassmorphism with Edit Panel, Import/Export ADIF
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Popup {
    id: logWindow
    property var nativeHostWindow: null
    width: nativeHostWindow && parent ? Math.max(600, parent.width) : 960
    height: nativeHostWindow && parent ? Math.max(400, parent.height) : 720
    modal: false
    closePolicy: Popup.CloseOnEscape
    padding: 0
    property bool positionInitialized: false

    function clampToParent() {
        if (nativeHostWindow) return
        if (!parent) return
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        if (nativeHostWindow) {
            x = 0
            y = 0
            positionInitialized = true
            return
        }
        x = Math.max(0, Math.round((parent.width - width) / 2))
        y = Math.max(0, Math.round((parent.height - height) / 2))
        positionInitialized = true
    }

    function startNativeHostMove() {
        if (!nativeHostWindow || typeof nativeHostWindow.startSystemMove !== "function")
            return false
        try {
            return nativeHostWindow.startSystemMove()
        } catch (error) {
            console.log("Log startSystemMove failed: " + error)
        }
        return false
    }

    function finishNativeHostMove() {
        if (nativeHostWindow && typeof nativeHostWindow.finishDesktopMove === "function")
            nativeHostWindow.finishDesktopMove()
    }

    function requestWindowClose() {
        if (nativeHostWindow && typeof nativeHostWindow.hideHostedWindow === "function") {
            nativeHostWindow.hideHostedWindow()
            return
        }
        logWindow.close()
    }

    // Dynamic theme colors from ThemeManager
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color bgLight: bridge.themeManager.bgLight
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color accentOrange: bridge.themeManager.warningColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder
    property color glassOverlay: bridge.themeManager.glassOverlay

    // Layout constants
    readonly property int outerMargin: 10
    readonly property int innerMargin: 6

    property var qsoList: []
    property var stats: ({})
    property int selectedIndex: -1
    property var selectedQso: null
    property var logbookProfiles: []
    property var logbookNames: []
    property bool updatingLogbookCombo: false
    property bool adifImportNoticeVisible: false
    property string adifImportNotice: ""
    property bool displayDistanceInMiles: bridge ? coerceBool(bridge.getSetting("Miles", false), false) : false

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

    function formatDistanceText(distanceKm, withSpace) {
        var km = Number(distanceKm)
        if (!isFinite(km) || km <= 0)
            return ""
        var value = displayDistanceInMiles ? km * 0.621371192 : km
        return Math.round(value) + (withSpace ? " " : "") + (displayDistanceInMiles ? "mi" : "km")
    }

    onAboutToShow: {
        ensureInitialPosition()
        if (appEngine && appEngine.logManager && appEngine.logManager.warmLogCacheAsync)
            appEngine.logManager.warmLogCacheAsync()
        refreshLogbookProfiles()
        delayedInitialRefresh.restart()
    }

    Timer {
        id: delayedInitialRefresh
        interval: 180
        repeat: false
        onTriggered: if (logWindow.visible) refreshLog()
    }

    Timer {
        id: adifImportNoticeTimer
        interval: 12000
        repeat: false
        onTriggered: logWindow.adifImportNoticeVisible = false
    }

    Connections {
        target: appEngine && appEngine.logManager ? appEngine.logManager : null
        function onQsoLogCacheChanged() {
            if (logWindow.visible) {
                refreshLogbookProfiles()
                refreshLog()
            }
        }
        function onActiveLogbookChanged() {
            refreshLogbookProfiles()
            clearSelection()
            if (logWindow.visible)
                refreshLog()
        }
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            if (key === "Miles")
                logWindow.displayDistanceInMiles = logWindow.coerceBool(value, false)
        }
        function onAdifImportFinished(success, imported, skipped, total, backupPath, message) {
            logWindow.adifImportNotice = String(message || (success ? qsTr("ADIF import completed") : qsTr("ADIF import failed")))
            logWindow.adifImportNoticeVisible = true
            adifImportNoticeTimer.restart()
        }
    }

    function refreshLogbookProfiles() {
        if (!(appEngine && appEngine.logManager && appEngine.logManager.logbookProfiles))
            return
        var profiles = appEngine.logManager.logbookProfiles()
        var names = []
        var active = -1
        for (var i = 0; i < profiles.length; ++i) {
            var p = profiles[i] || ({})
            names.push(String(p.name || "Logbook") + "  " + String(p.qsoCount || 0))
            if (p.active)
                active = i
        }
        updatingLogbookCombo = true
        logbookProfiles = profiles
        logbookNames = names
        if (active >= 0 && logbookCombo)
            logbookCombo.currentIndex = active
        updatingLogbookCombo = false
    }

    function refreshLog() {
        if (appEngine && appEngine.logManager) {
            qsoList = appEngine.logManager.searchQsos(
                searchField.text,
                bandFilter.currentText === "All" ? "" : bandFilter.currentText,
                modeFilter.currentText === "All" ? "" : modeFilter.currentText, "", "")
            stats = statsFromRows(qsoList)
        }
    }

    function hasActiveFilters() {
        var band = bandFilter.currentText || ""
        var mode = modeFilter.currentText || ""
        return searchField.text.length > 0
               || (band.length > 0 && band !== "All")
               || (mode.length > 0 && mode !== "All")
    }

    function resetFilters() {
        searchField.text = ""
        bandFilter.currentIndex = 0
        modeFilter.currentIndex = 0
        clearSelection()
        refreshLog()
    }

    function fileUrlToLocalPath(fileUrl) {
        var path = String(fileUrl || "")
        if (path.indexOf("file://localhost/") === 0)
            path = path.substring(16)
        else if (path.indexOf("file://") === 0)
            path = path.substring(7)
        path = decodeURIComponent(path)
        if (Qt.platform.os === "windows" && path.length > 2 && path.charAt(0) === "/" && path.charAt(2) === ":")
            path = path.substring(1)
        return path
    }

    function openImportAdifDialog() {
        var path = bridge.openFileDialog(qsTr("Importa file ADIF"),
                                         "",
                                         [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")])
        if (path.length > 0 && appEngine && appEngine.logManager) {
            if (appEngine.logManager.importFromAdifAsync)
                appEngine.logManager.importFromAdifAsync(path)
            else
                appEngine.logManager.importFromAdif(path)
            clearSelection()
        }
    }

    function openAddLogbookDialog() {
        var path = bridge.openFileDialog(qsTr("Carica logbook ADIF"),
                                         "",
                                         [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")])
        if (path.length > 0 && appEngine && appEngine.logManager) {
            appEngine.logManager.addLogbook(path, "")
            clearSelection()
            refreshLogbookProfiles()
            refreshLog()
        }
    }

    function selectedLogbookProfile() {
        if (!logbookProfiles || logbookProfiles.length === 0)
            return ({})
        var idx = logbookCombo ? logbookCombo.currentIndex : -1
        if (idx < 0 || idx >= logbookProfiles.length) {
            for (var i = 0; i < logbookProfiles.length; ++i) {
                var candidate = logbookProfiles[i] || ({})
                if (candidate.active)
                    return candidate
            }
            return logbookProfiles[0] || ({})
        }
        return logbookProfiles[idx] || ({})
    }

    function openDeleteLogbookDialog() {
        var profile = selectedLogbookProfile()
        if (!profile.path)
            return
        deleteLogbookDialog.profile = profile
        deleteLogbookFileCheck.checked = true
        deleteLogbookDialog.open()
    }

    function openExportAdifDialog() {
        var path = bridge.saveFileDialog(qsTr("Esporta file ADIF"),
                                         "",
                                         [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")])
        if (path.length > 0 && appEngine && appEngine.logManager)
            appEngine.logManager.exportToAdif(path)
    }

    function statsFromRows(rows) {
        var calls = ({})
        var grids = ({})
        var maxDistance = 0
        var farthestCall = ""
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            var call = String(row.call || "").toUpperCase()
            var grid = String(row.grid || "").toUpperCase()
            var distance = Number(row.distanceKm || 0)
            if (call.length > 0)
                calls[call] = true
            if (grid.length > 0)
                grids[grid] = true
            if (distance > maxDistance) {
                maxDistance = distance
                farthestCall = call
            }
        }
        return {
            totalQsos: rows.length,
            uniqueCalls: Object.keys(calls).length,
            uniqueGrids: Object.keys(grids).length,
            maxDistance: maxDistance,
            farthestCall: farthestCall
        }
    }

    function clearSelection() {
        selectedIndex = -1
        selectedQso = null
    }

    // Fallback auto-refresh: catches auto-sequence QSOs that bypass MshvBridge
    Timer {
        interval: 15000
        running: logWindow.visible && logWindow.activeFocus
        repeat: true
        onTriggered: refreshLog()
    }

    Overlay.modal: Rectangle { color: "transparent" }

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan
        border.width: 2
        radius: 10

        // Inner glow
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 8
            color: "transparent"
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.08)
            border.width: 1
        }

        // Bottom accent line
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.6
            height: 2
            radius: 1
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
        }
    }

    Dialog {
        id: createLogbookDialog
        title: qsTr("Nuovo logbook")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            width: 300
            spacing: 8
            Label {
                text: qsTr("Nome operatore / callsign")
                color: textPrimary
            }
            DecoTextField {
                id: newLogbookNameField
                Layout.fillWidth: true
                placeholderText: qsTr("e.g. 9H1SR or AMICO")
                selectByMouse: true
                color: textPrimary
            }
            CheckBox {
                id: newLogbookBackupCheck
                checked: true
                text: qsTr("Backup log attuale")
            }
        }

        onOpened: {
            newLogbookNameField.text = appEngine ? String(appEngine.callsign || "") : ""
            newLogbookNameField.forceActiveFocus()
            newLogbookNameField.selectAll()
        }
        onAccepted: {
            if (appEngine && appEngine.logManager) {
                appEngine.logManager.createLogbook(newLogbookNameField.text, newLogbookBackupCheck.checked)
                clearSelection()
                refreshLogbookProfiles()
                refreshLog()
            }
        }
    }

    Dialog {
        id: deleteLogbookDialog
        title: qsTr("Delete logbook")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        property var profile: ({})

        ColumnLayout {
            width: 430
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: qsTr("You are about to delete the selected logbook.")
                color: accentOrange
                font.pixelSize: 14
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: "Nome: " + String(deleteLogbookDialog.profile.name || "Logbook")
                      + "\nPath: " + String(deleteLogbookDialog.profile.path || "")
                      + "\nQSO: " + String(deleteLogbookDialog.profile.qsoCount || 0)
                color: textPrimary
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
            }
            CheckBox {
                id: deleteLogbookFileCheck
                checked: true
                text: qsTr("Also delete the ADIF file from disk")
            }
            Label {
                Layout.fillWidth: true
                text: deleteLogbookFileCheck.checked
                      ? qsTr("Destructive operation: the .adi file will be deleted. If this is the last logbook, Decodium will create a new empty logbook.")
                      : qsTr("The .adi file will remain on disk; only the association with Decodium will be removed.")
                color: textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        onAccepted: {
            if (appEngine && appEngine.logManager && deleteLogbookDialog.profile.path) {
                if (appEngine.logManager.deleteLogbook(deleteLogbookDialog.profile.path, deleteLogbookFileCheck.checked)) {
                    clearSelection()
                    refreshLogbookProfiles()
                    refreshLog()
                }
            }
        }
    }

    // Delete confirmation dialog
    Dialog {
        id: deleteConfirmDialog
        title: qsTr("Confirm deletion")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        property string deleteCall: ""
        property string deleteDateTime: ""

        Label {
            text: qsTr("Delete the QSO with %1?").arg(deleteConfirmDialog.deleteCall)
            font.pixelSize: 13
        }

        onAccepted: {
            if (appEngine && appEngine.logManager) {
                appEngine.logManager.deleteQso(deleteCall, deleteDateTime)
                clearSelection()
                refreshLog()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ======= HEADER BAR =======
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
            radius: 10

            MouseArea {
                anchors.fill: parent
                property point clickPos: Qt.point(0, 0)
                property point pressGlobalPos: Qt.point(0, 0)
                property point pressWindowPos: Qt.point(0, 0)
                property bool nativeMoveActive: false
                cursorShape: Qt.SizeAllCursor
                onPressed: function(mouse) {
                    clickPos = Qt.point(mouse.x, mouse.y)
                    logWindow.positionInitialized = true
                    if (logWindow.nativeHostWindow) {
                        pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                        pressWindowPos = Qt.point(logWindow.nativeHostWindow.x,
                                                  logWindow.nativeHostWindow.y)
                        nativeMoveActive = logWindow.startNativeHostMove()
                    }
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    if (logWindow.nativeHostWindow) {
                        if (nativeMoveActive)
                            return
                        var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                        logWindow.nativeHostWindow.x = Math.round(
                                    pressWindowPos.x + currentGlobalPos.x - pressGlobalPos.x)
                        logWindow.nativeHostWindow.y = Math.round(
                                    pressWindowPos.y + currentGlobalPos.y - pressGlobalPos.y)
                        return
                    }
                    logWindow.x += mouse.x - clickPos.x
                    logWindow.y += mouse.y - clickPos.y
                    logWindow.clampToParent()
                }
                onReleased: {
                    nativeMoveActive = false
                    logWindow.finishNativeHostMove()
                }
                onCanceled: {
                    nativeMoveActive = false
                    logWindow.finishNativeHostMove()
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 10
                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 10

                // Status dot (pulsing)
                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: secondaryCyan

                    SequentialAnimation on opacity {
                        running: logWindow.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.4; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }

                Text {
                    text: qsTr("QSO Log")
                    font.pixelSize: 15
                    font.bold: true
                    font.letterSpacing: 0.5
                    color: secondaryCyan
                }

                Rectangle { width: 1; height: 18; color: glassBorder }

                // Inline stats
                Text {
                    text: (stats.totalQsos || 0) + " QSOs"
                    font.pixelSize: 12; font.bold: true; font.family: decodiumMonoFontFamily
                    color: accentGreen
                }
                Text {
                    text: (stats.uniqueCalls || 0) + " Calls"
                    font.pixelSize: 11; font.family: decodiumMonoFontFamily
                    color: textSecondary
                }
                Text {
                    text: (stats.uniqueGrids || 0) + " Grids"
                    font.pixelSize: 11; font.family: decodiumMonoFontFamily
                    color: textSecondary
                }

                Item { Layout.fillWidth: true }

                // Minimize
                Rectangle {
                    width: 26; height: 26; radius: 4
                    color: logMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : "transparent"
                    border.color: logMinMA.containsMouse ? "#ffc107" : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent; text: qsTr("\u2212")
                        font.pixelSize: 16; font.bold: true
                        color: logMinMA.containsMouse ? "#ffc107" : textPrimary
                    }
                    MouseArea {
                        id: logMinMA; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            logWindowMinimized = true
                            if (logWindow.nativeHostWindow
                                    && typeof logWindow.nativeHostWindow.minimizeHostedWindow === "function")
                                logWindow.nativeHostWindow.minimizeHostedWindow()
                            else
                                logWindow.close()
                        }
                    }
                    ToolTip.visible: logMinMA.containsMouse
                    ToolTip.text: qsTr("Minimize"); ToolTip.delay: 500
                }

                // Close
                Rectangle {
                    width: 26; height: 26; radius: 4
                    color: logCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                    border.color: logCloseMA.containsMouse ? "#f44336" : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent; text: qsTr("\u2715")
                        font.pixelSize: 11; font.bold: true
                        color: logCloseMA.containsMouse ? "#f44336" : textPrimary
                    }
                    MouseArea {
                        id: logCloseMA; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: logWindow.requestWindowClose()
                    }
                    ToolTip.visible: logCloseMA.containsMouse
                    ToolTip.text: qsTr("Close"); ToolTip.delay: 500
                }
            }
        }

        Rectangle {
            id: adifImportBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 38 : 0
            Layout.leftMargin: outerMargin
            Layout.rightMargin: outerMargin
            visible: (bridge && bridge.adifImportInProgress) || logWindow.adifImportNoticeVisible
            radius: 5
            color: bridge && bridge.adifImportInProgress
                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
                   : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)
            border.color: bridge && bridge.adifImportInProgress ? secondaryCyan : accentGreen

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: bridge && bridge.adifImportInProgress
                          ? String(bridge.adifImportStatus || qsTr("Importing ADIF..."))
                          : logWindow.adifImportNotice
                    color: textPrimary
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
                ProgressBar {
                    visible: bridge && bridge.adifImportInProgress
                    Layout.preferredWidth: 110
                    from: 0; to: 100
                    value: bridge ? bridge.adifImportProgress : 0
                }
                Text {
                    visible: bridge && bridge.adifImportInProgress
                    text: (bridge ? bridge.adifImportProgress : 0) + "%"
                    color: secondaryCyan
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        // ======= FILTER BAR =======
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.margins: outerMargin
            Layout.bottomMargin: 0
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.06)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8
                spacing: 6

                StyledComboBox {
                    id: logbookCombo
                    model: logbookNames
                    Layout.preferredWidth: 150
                    height: 28
                    font.pixelSize: 10
                    onActivated: function(index) {
                        if (updatingLogbookCombo || !(appEngine && appEngine.logManager))
                            return
                        var profile = logbookProfiles[index] || ({})
                        if (profile.path)
                            appEngine.logManager.switchLogbook(profile.path)
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: appEngine && appEngine.logManager
                                  ? "Active logbook: " + appEngine.logManager.activeLogbookPath
                                  : "Logbook"
                    ToolTip.delay: 500
                }

                Rectangle {
                    width: 46; height: 28; radius: 4
                    color: newLogbookMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.28) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.10)
                    border.color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.55)
                    Text { anchors.centerIn: parent; text: qsTr("New"); font.pixelSize: 10; font.bold: true; color: accentGreen }
                    MouseArea { id: newLogbookMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: createLogbookDialog.open() }
                    ToolTip.visible: newLogbookMA.containsMouse; ToolTip.text: qsTr("Create a separate logbook"); ToolTip.delay: 500
                }

                Rectangle {
                    width: 48; height: 28; radius: 4
                    color: loadLogbookMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.28) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.10)
                    border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.55)
                    Text { anchors.centerIn: parent; text: qsTr("Load"); font.pixelSize: 10; font.bold: true; color: secondaryCyan }
                    MouseArea { id: loadLogbookMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: logWindow.openAddLogbookDialog() }
                    ToolTip.visible: loadLogbookMA.containsMouse; ToolTip.text: qsTr("Load/use an existing ADIF as a logbook"); ToolTip.delay: 500
                }

                Rectangle {
                    width: 44; height: 28; radius: 4
                    color: backupLogbookMA.containsMouse ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.25) : Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.08)
                    border.color: Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.5)
                    Text { anchors.centerIn: parent; text: qsTr("Bkp"); font.pixelSize: 10; font.bold: true; color: accentOrange }
                    MouseArea { id: backupLogbookMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: if (appEngine && appEngine.logManager) appEngine.logManager.backupActiveLogbook() }
                    ToolTip.visible: backupLogbookMA.containsMouse; ToolTip.text: qsTr("Back up the active logbook"); ToolTip.delay: 500
                }

                Rectangle {
                    width: 42; height: 28; radius: 4
                    color: deleteLogbookMA.containsMouse ? Qt.rgba(255, 70, 90, 0.28) : Qt.rgba(255, 70, 90, 0.08)
                    border.color: Qt.rgba(255, 70, 90, 0.55)
                    opacity: logbookProfiles.length > 0 ? 1.0 : 0.45
                    Text { anchors.centerIn: parent; text: qsTr("Del"); font.pixelSize: 10; font.bold: true; color: "#ff465a" }
                    MouseArea {
                        id: deleteLogbookMA
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: logbookProfiles.length > 0
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: logWindow.openDeleteLogbookDialog()
                    }
                    ToolTip.visible: deleteLogbookMA.containsMouse; ToolTip.text: qsTr("Delete selected logbook"); ToolTip.delay: 500
                }

                // Search field
                Rectangle {
                    Layout.preferredWidth: 150; height: 28; radius: 4
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                    border.color: searchField.focus ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                    border.width: searchField.focus ? 2 : 1
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 4
                        Text { text: qsTr("\uD83D\uDD0D"); font.pixelSize: 11; color: textSecondary }
                        DecoTextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search callsign, locator...")
                            font.pixelSize: 11; font.family: decodiumMonoFontFamily
                            color: textPrimary
                            placeholderTextColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            onTextChanged: { clearSelection(); refreshLog() }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                StyledComboBox {
                    id: bandFilter
                    model: ["All", "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "4m", "2m", "1.25m", "70cm", "33cm", "23cm", "13cm", "9cm", "6cm", "3cm", "1.25cm"]
                    currentIndex: 0
                    Layout.preferredWidth: 85; height: 28
                    font.pixelSize: 10
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                StyledComboBox {
                    id: modeFilter
                    model: ["All", "FT8", "FT4", "FT2", "SFox", "JT65", "JT9", "Q65", "MSK144", "MSK40", "FSK441"]
                    currentIndex: 0
                    Layout.preferredWidth: 85; height: 28
                    font.pixelSize: 10
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                Rectangle {
                    id: clearFiltersButton
                    visible: logWindow.hasActiveFilters()
                    Layout.preferredWidth: visible ? clearFiltersLabel.width + 18 : 0
                    height: 28
                    radius: 4
                    color: clearFiltersMA.containsMouse
                           ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.25)
                           : Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.10)
                    border.color: Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.55)
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        id: clearFiltersLabel
                        anchors.centerIn: parent
                        text: qsTr("Clear")
                        font.pixelSize: 10
                        font.bold: true
                        color: accentOrange
                    }

                    MouseArea {
                        id: clearFiltersMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: logWindow.resetFilters()
                    }

                    ToolTip.visible: clearFiltersMA.containsMouse
                    ToolTip.text: qsTr("Remove search, band, and mode filters")
                    ToolTip.delay: 500
                }

                Item { Layout.fillWidth: true }

                // Import ADIF
                Rectangle {
                    width: importLabel.width + 16; height: 28; radius: 4
                    color: !importMA.enabled ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.04)
                           : importMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)
                    border.color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.5)
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text { id: importLabel; anchors.centerIn: parent; text: qsTr("Import ADIF"); font.pixelSize: 10; font.bold: true; color: importMA.enabled ? accentGreen : textSecondary }
                    MouseArea {
                        id: importMA; anchors.fill: parent; hoverEnabled: true
                        enabled: !(bridge && bridge.adifImportInProgress)
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: logWindow.openImportAdifDialog()
                    }
                    ToolTip.visible: importMA.containsMouse; ToolTip.text: qsTr("Import ADIF file"); ToolTip.delay: 500
                }

                // Export ADIF
                Rectangle {
                    width: adifLabel.width + 16; height: 28; radius: 4
                    color: exportMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
                    border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5)
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text { id: adifLabel; anchors.centerIn: parent; text: qsTr("Esporta ADIF"); font.pixelSize: 10; font.bold: true; color: secondaryCyan }
                    MouseArea {
                        id: exportMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: logWindow.openExportAdifDialog()
                    }
                    ToolTip.visible: exportMA.containsMouse; ToolTip.text: qsTr("Export ADIF"); ToolTip.delay: 500
                }

                // Refresh
                Rectangle {
                    width: 28; height: 28; radius: 4
                    color: refreshMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text { anchors.centerIn: parent; text: qsTr("\u21BB"); font.pixelSize: 14; font.bold: true; color: refreshMA.containsMouse ? accentGreen : textSecondary }
                    MouseArea {
                        id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: refreshLog()
                    }
                    ToolTip.visible: refreshMA.containsMouse; ToolTip.text: qsTr("Refresh"); ToolTip.delay: 500
                }
            }
        }

        // ======= COLUMN HEADERS =======
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: outerMargin; Layout.rightMargin: outerMargin
            Layout.topMargin: innerMargin
            height: 22
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
            radius: 3

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10; anchors.rightMargin: 10
                spacing: 0

                Text { text: qsTr("Date/Time"); font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 148 }
                Text { text: "Call"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 100 }
                Text { text: "Grid"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 60 }
                Text { text: "Band"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 55 }
                Text { text: "Mode"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 55 }
                Text { text: "Sent"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                Text { text: "Rcvd"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                Text { text: qsTr("Dist"); font.pixelSize: 10; font.bold: true; color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 60 }
                Text { text: "Comment"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.fillWidth: true }
            }
        }

        // ======= QSO LIST =======
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: outerMargin; Layout.rightMargin: outerMargin
            Layout.topMargin: 3
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
            radius: 4
            clip: true

            ListView {
                id: qsoListView
                anchors.fill: parent
                anchors.margins: 3
                clip: true
                model: qsoList
                spacing: 1
                currentIndex: selectedIndex

                ScrollBar.vertical: ScrollBar {
                    active: true
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle { implicitWidth: 4; radius: 2; color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) }
                    background: Rectangle { implicitWidth: 4; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.03); radius: 2 }
                }

                delegate: Rectangle {
                    width: qsoListView.width - 8
                    height: 26
                    radius: 3
                    color: index === selectedIndex
                           ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                           : qsoRowMA.containsMouse
                             ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
                             : (index % 2 === 0 ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05))
                    border.color: index === selectedIndex ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.5) : "transparent"
                    border.width: index === selectedIndex ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 100 } }

                    MouseArea {
                        id: qsoRowMA; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (selectedIndex === index) {
                                clearSelection()
                            } else {
                                selectedIndex = index
                                selectedQso = modelData
                                // Populate edit fields
                                editCall.text = modelData.call || ""
                                editGrid.text = modelData.grid || ""
                                editBand.text = modelData.band || ""
                                editMode.text = modelData.mode || ""
                                editSent.text = modelData.reportSent || ""
                                editRcvd.text = modelData.reportReceived || ""
                                editComment.text = modelData.comment || ""
                            }
                        }
                        onDoubleClicked: {
                            if (appEngine) { appEngine.dxCall = modelData.call; appEngine.dxGrid = modelData.grid }
                        }
                    }

                    // Keep the usual left-click selection intact while offering
                    // a quick clipboard action for a logged station.
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        enabled: String(modelData.call || "").trim().length > 0
                        onTapped: qsoCallContextMenu.popup()
                    }

                    Menu {
                        id: qsoCallContextMenu
                        MenuItem {
                            text: qsTr("Copy Callsign")
                            height: 32
                            onTriggered: {
                                if (bridge)
                                    bridge.copyToClipboard(String(modelData.call || "").trim())
                            }
                            contentItem: Text {
                                text: parent.text
                                color: textPrimary
                                font.pixelSize: 12
                                leftPadding: 10
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.highlighted
                                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                                       : "transparent"
                            }
                        }
                        background: Rectangle {
                            implicitWidth: 150
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
                            border.color: glassBorder
                            radius: 6
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 10
                        spacing: 0

                        // Selection indicator
                        Text {
                            text: index === selectedIndex ? "\u25B6" : ""
                            font.pixelSize: 8; color: primaryBlue
                            Layout.preferredWidth: 12
                        }
                        Text { text: modelData.dateTime || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: textSecondary; Layout.preferredWidth: 136 }
                        Text { text: modelData.call || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; font.bold: true; color: accentGreen; Layout.preferredWidth: 100 }
                        Text { text: modelData.grid || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: secondaryCyan; Layout.preferredWidth: 60 }
                        Text { text: modelData.band || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: textPrimary; Layout.preferredWidth: 55 }
                        Text { text: modelData.mode || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: textPrimary; Layout.preferredWidth: 55 }
                        Text { text: modelData.reportSent || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                        Text { text: modelData.reportReceived || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                        Text { text: logWindow.formatDistanceText(modelData.distance || 0, true); font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: accentOrange; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 60 }
                        Text { text: modelData.comment || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 11; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.45); Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }

                // Empty state
                Column {
                    anchors.centerIn: parent; spacing: 6
                    visible: qsoList.length === 0
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("No QSOs found"); font.pixelSize: 13; font.bold: true; color: textSecondary }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: searchField.text.length > 0 ? "Try a different search" : "Start making contacts!"; font.pixelSize: 10; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.35) }
                }
            }
        }

        // ======= EDIT PANEL (visible when a QSO is selected) =======
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: editPanelContent.implicitHeight + 16
            Layout.leftMargin: outerMargin; Layout.rightMargin: outerMargin
            Layout.topMargin: innerMargin
            visible: selectedIndex >= 0 && selectedQso !== null
            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.06)
            border.color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
            radius: 6

            ColumnLayout {
                id: editPanelContent
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                // Row 1: Call, Grid, Band, Mode
                RowLayout {
                    spacing: 10

                    // Call
                    ColumnLayout {
                        spacing: 2
                        Text { text: "Call"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 132; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editCall.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editCall; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily; font.bold: true
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    // Grid
                    ColumnLayout {
                        spacing: 2
                        Text { text: "Grid"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 78; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editGrid.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editGrid; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    // Band
                    ColumnLayout {
                        spacing: 2
                        Text { text: "Band"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 66; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editBand.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editBand; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    // Mode
                    ColumnLayout {
                        spacing: 2
                        Text { text: "Mode"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 66; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editMode.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editMode; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    // Sent
                    ColumnLayout {
                        spacing: 2
                        Text { text: "Sent"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 56; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editSent.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editSent; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    // Rcvd
                    ColumnLayout {
                        spacing: 2
                        Text { text: "Rcvd"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 56; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editRcvd.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editRcvd; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Row 2: Comment + buttons
                RowLayout {
                    spacing: 10

                    // Comment
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "Comment"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            Layout.fillWidth: true; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editComment.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editComment; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    // Action buttons
                    RowLayout {
                        Layout.alignment: Qt.AlignBottom
                        spacing: 6

                        // Save button
                        Rectangle {
                            width: saveLabel.width + 20; height: 26; radius: 4
                            color: saveMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.4) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2)
                            border.color: accentGreen
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text { id: saveLabel; anchors.centerIn: parent; text: qsTr("\u2713 Save"); font.pixelSize: 10; font.bold: true; color: accentGreen }
                            MouseArea {
                                id: saveMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (appEngine && appEngine.logManager && selectedQso) {
                                        var newData = {
                                            "call": editCall.text,
                                            "grid": editGrid.text,
                                            "band": editBand.text,
                                            "mode": editMode.text,
                                            "reportSent": editSent.text,
                                            "reportReceived": editRcvd.text,
                                            "comment": editComment.text
                                        }
                                        appEngine.logManager.editQso(selectedQso.call, selectedQso.dateTime, newData)
                                        clearSelection()
                                        refreshLog()
                                    }
                                }
                            }
                        }

                        // Delete button
                        Rectangle {
                            width: deleteLabel.width + 20; height: 26; radius: 4
                            color: deleteMA.containsMouse ? Qt.rgba(1, 0, 0, 0.3) : Qt.rgba(1, 0, 0, 0.1)
                            border.color: "#d32f2f"
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text { id: deleteLabel; anchors.centerIn: parent; text: qsTr("\u2715 Delete"); font.pixelSize: 10; font.bold: true; color: "#d32f2f" }
                            MouseArea {
                                id: deleteMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (selectedQso) {
                                        deleteConfirmDialog.deleteCall = selectedQso.call
                                        deleteConfirmDialog.deleteDateTime = selectedQso.dateTime
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }

                        // Cancel button
                        Rectangle {
                            width: cancelLabel.width + 20; height: 26; radius: 4
                            color: cancelMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                            border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text { id: cancelLabel; anchors.centerIn: parent; text: qsTr("Cancel"); font.pixelSize: 10; color: textSecondary }
                            MouseArea {
                                id: cancelMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: clearSelection()
                            }
                        }
                    }
                }
            }
        }

        // ======= STATS BAR =======
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.margins: outerMargin
            Layout.topMargin: innerMargin
            color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.06)
            border.color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2)
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14; anchors.rightMargin: 14
                spacing: 20

                Column {
                    spacing: 0
                    Text { text: "TOTAL"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.totalQsos || "0"; font.pixelSize: 20; font.bold: true; color: accentGreen; font.family: decodiumMonoFontFamily }
                }

                Rectangle { width: 1; height: 30; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

                Column {
                    spacing: 0
                    Text { text: "CALLS"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.uniqueCalls || "0"; font.pixelSize: 20; font.bold: true; color: secondaryCyan; font.family: decodiumMonoFontFamily }
                }

                Column {
                    spacing: 0
                    Text { text: "GRIDS"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.uniqueGrids || "0"; font.pixelSize: 20; font.bold: true; color: secondaryCyan; font.family: decodiumMonoFontFamily }
                }

                Rectangle { width: 1; height: 30; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

                Column {
                    spacing: 0
                    Text { text: qsTr("MAX DIST"); font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: logWindow.formatDistanceText(stats.maxDistance || 0, true) || "0 " + (logWindow.displayDistanceInMiles ? "mi" : "km"); font.pixelSize: 16; font.bold: true; color: accentOrange; font.family: decodiumMonoFontFamily }
                }

                Column {
                    spacing: 0
                    Text { text: "FARTHEST"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.farthestCall || "-"; font.pixelSize: 16; font.bold: true; color: accentOrange; font.family: decodiumMonoFontFamily }
                }

                Item { Layout.fillWidth: true }

                Column {
                    spacing: 0
                    Text { text: "SHOWING"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: qsoList.length + " QSOs"; font.pixelSize: 14; font.bold: true; color: textPrimary; font.family: decodiumMonoFontFamily }
                }
            }
        }
    }
}
