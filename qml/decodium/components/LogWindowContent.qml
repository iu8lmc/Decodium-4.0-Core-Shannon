/* Decodium 2.5 - Log Window Content (floating version)
 * Full Glassmorphism with Edit Panel, Import/Export ADIF
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: logContent
    color: "transparent"

    // Dynamic theme colors from ThemeManager
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color accentOrange: bridge.themeManager.warningColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder
    property color glassOverlay: bridge.themeManager.glassOverlay

    property var qsoList: []
    property var stats: ({})
    property int selectedIndex: -1
    property var selectedQso: null
    property bool refreshActive: true
    property var logbookProfiles: []
    property var logbookNames: []
    property bool updatingLogbookCombo: false
    property bool adifImportNoticeVisible: false
    property string adifImportNotice: ""

    Component.onCompleted: if (refreshActive) {
        if (appEngine && appEngine.logManager && appEngine.logManager.warmLogCacheAsync)
            appEngine.logManager.warmLogCacheAsync()
        refreshLogbookProfiles()
        delayedInitialRefresh.restart()
    }
    onRefreshActiveChanged: if (refreshActive) {
        if (appEngine && appEngine.logManager && appEngine.logManager.warmLogCacheAsync)
            appEngine.logManager.warmLogCacheAsync()
        refreshLogbookProfiles()
        delayedInitialRefresh.restart()
    }

    Timer {
        id: delayedInitialRefresh
        interval: 180
        repeat: false
        onTriggered: if (logContent.refreshActive) refreshLog()
    }

    Timer {
        id: adifImportNoticeTimer
        interval: 12000
        repeat: false
        onTriggered: logContent.adifImportNoticeVisible = false
    }

    Connections {
        target: appEngine && appEngine.logManager ? appEngine.logManager : null
        function onQsoLogCacheChanged() {
            if (logContent.refreshActive) {
                refreshLogbookProfiles()
                refreshLog()
            }
        }
        function onActiveLogbookChanged() {
            refreshLogbookProfiles()
            clearSelection()
            if (logContent.refreshActive)
                refreshLog()
        }
        function onAdifImportFinished(success, imported, skipped, total, backupPath, message) {
            logContent.adifImportNotice = String(message || (success ? qsTr("ADIF import completed") : qsTr("ADIF import failed")))
            logContent.adifImportNoticeVisible = true
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

    function openExportAdifDialog() {
        var path = bridge.saveFileDialog(qsTr("Esporta file ADIF"),
                                         "",
                                         [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")])
        if (path.length > 0 && appEngine && appEngine.logManager)
            appEngine.logManager.exportToAdif(path)
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

    // Fallback auto-refresh
    Timer {
        interval: 3000
        running: logContent.refreshActive
        repeat: true
        onTriggered: refreshLog()
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
            width: 380
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: qsTr("You are about to delete the selected logbook.")
                color: accentOrange
                font.pixelSize: 13
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: "Nome: " + String(deleteLogbookDialog.profile.name || "Logbook")
                      + "\nPath: " + String(deleteLogbookDialog.profile.path || "")
                      + "\nQSO: " + String(deleteLogbookDialog.profile.qsoCount || 0)
                color: textPrimary
                font.pixelSize: 11
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
                font.pixelSize: 10
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        Rectangle {
            id: adifImportBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 38 : 0
            visible: (bridge && bridge.adifImportInProgress) || logContent.adifImportNoticeVisible
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
                          : logContent.adifImportNotice
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

        // Stats + Filter bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.06)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8
                spacing: 6

                Text { text: (stats.totalQsos || 0) + " QSOs"; font.pixelSize: 11; font.bold: true; font.family: decodiumMonoFontFamily; color: accentGreen }
                Rectangle { width: 1; height: 16; color: glassBorder }
                Text { text: (stats.uniqueCalls || 0) + " Calls"; font.pixelSize: 10; font.family: decodiumMonoFontFamily; color: textSecondary }

                Rectangle { width: 1; height: 16; color: glassBorder }

                StyledComboBox {
                    id: logbookCombo
                    model: logbookNames
                    Layout.preferredWidth: 116
                    height: 26
                    font.pixelSize: 9
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
                    width: 32; height: 24; radius: 3
                    color: newLogbookMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.28) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.10)
                    border.color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.55)
                    Text { anchors.centerIn: parent; text: qsTr("New"); font.pixelSize: 9; font.bold: true; color: accentGreen }
                    MouseArea { id: newLogbookMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: createLogbookDialog.open() }
                    ToolTip.visible: newLogbookMA.containsMouse; ToolTip.text: qsTr("Create a separate logbook"); ToolTip.delay: 500
                }

                Rectangle {
                    width: 34; height: 24; radius: 3
                    color: loadLogbookMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.28) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.10)
                    border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.55)
                    Text { anchors.centerIn: parent; text: qsTr("Load"); font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                    MouseArea { id: loadLogbookMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: logContent.openAddLogbookDialog() }
                    ToolTip.visible: loadLogbookMA.containsMouse; ToolTip.text: qsTr("Load/use an existing ADIF"); ToolTip.delay: 500
                }

                Rectangle {
                    width: 32; height: 24; radius: 3
                    color: backupLogbookMA.containsMouse ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.25) : Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.08)
                    border.color: Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.5)
                    Text { anchors.centerIn: parent; text: qsTr("Bkp"); font.pixelSize: 9; font.bold: true; color: accentOrange }
                    MouseArea { id: backupLogbookMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: if (appEngine && appEngine.logManager) appEngine.logManager.backupActiveLogbook() }
                    ToolTip.visible: backupLogbookMA.containsMouse; ToolTip.text: qsTr("Back up the active logbook"); ToolTip.delay: 500
                }

                Rectangle {
                    width: 32; height: 24; radius: 3
                    color: deleteLogbookMA.containsMouse ? Qt.rgba(255, 70, 90, 0.28) : Qt.rgba(255, 70, 90, 0.08)
                    border.color: Qt.rgba(255, 70, 90, 0.55)
                    opacity: logbookProfiles.length > 0 ? 1.0 : 0.45
                    Text { anchors.centerIn: parent; text: qsTr("Del"); font.pixelSize: 9; font.bold: true; color: "#ff465a" }
                    MouseArea {
                        id: deleteLogbookMA
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: logbookProfiles.length > 0
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: logContent.openDeleteLogbookDialog()
                    }
                    ToolTip.visible: deleteLogbookMA.containsMouse; ToolTip.text: qsTr("Delete selected logbook"); ToolTip.delay: 500
                }

                Rectangle {
                    Layout.preferredWidth: 110; height: 26; radius: 4
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                    border.color: searchField.focus ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                    border.width: searchField.focus ? 2 : 1
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 3; spacing: 3
                        Text { text: qsTr("\uD83D\uDD0D"); font.pixelSize: 10; color: textSecondary }
                        DecoTextField {
                            id: searchField; Layout.fillWidth: true
                            placeholderText: qsTr("Search...")
                            font.pixelSize: 10; font.family: decodiumMonoFontFamily
                            color: textPrimary; placeholderTextColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            onTextChanged: { clearSelection(); refreshLog() }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                StyledComboBox {
                    id: bandFilter
                    model: ["All", "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "4m", "2m", "1.25m", "70cm", "33cm", "23cm", "13cm", "9cm", "6cm", "3cm", "1.25cm"]
                    currentIndex: 0
                    Layout.preferredWidth: 75; height: 26; font.pixelSize: 9
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                StyledComboBox {
                    id: modeFilter
                    model: ["All", "FT8", "FT4", "FT2", "SFox", "JT65", "JT9", "Q65", "MSK144"]
                    currentIndex: 0
                    Layout.preferredWidth: 75; height: 26; font.pixelSize: 9
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                Rectangle {
                    id: clearFiltersButton
                    visible: logContent.hasActiveFilters()
                    Layout.preferredWidth: visible ? 24 : 0
                    height: 22
                    radius: 3
                    color: clearFiltersMA.containsMouse
                           ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.25)
                           : "transparent"
                    border.color: visible ? Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.55) : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("\u00D7")
                        font.pixelSize: 13
                        font.bold: true
                        color: accentOrange
                    }

                    MouseArea {
                        id: clearFiltersMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: logContent.resetFilters()
                    }

                    ToolTip.visible: clearFiltersMA.containsMouse
                    ToolTip.text: qsTr("Remove filters")
                    ToolTip.delay: 500
                }

                Item { Layout.fillWidth: true }

                // Import ADIF
                Rectangle {
                    width: 22; height: 22; radius: 3
                    color: !importFloatMA.enabled ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.04)
                           : importFloatMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: qsTr("\u2B07"); font.pixelSize: 11; color: importFloatMA.enabled && importFloatMA.containsMouse ? accentGreen : textSecondary }
                    MouseArea {
                        id: importFloatMA
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !(bridge && bridge.adifImportInProgress)
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: logContent.openImportAdifDialog()
                    }
                    ToolTip.visible: importFloatMA.containsMouse; ToolTip.text: qsTr("Import ADIF"); ToolTip.delay: 500
                }

                // Export ADIF
                Rectangle {
                    width: 22; height: 22; radius: 3
                    color: exportFloatMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: qsTr("\u2B06"); font.pixelSize: 11; color: exportFloatMA.containsMouse ? secondaryCyan : textSecondary }
                    MouseArea { id: exportFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: logContent.openExportAdifDialog() }
                    ToolTip.visible: exportFloatMA.containsMouse; ToolTip.text: qsTr("Export ADIF"); ToolTip.delay: 500
                }

                // Refresh
                Rectangle {
                    width: 22; height: 22; radius: 3
                    color: refreshFloatMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: qsTr("\u21BB"); font.pixelSize: 12; font.bold: true; color: refreshFloatMA.containsMouse ? accentGreen : textSecondary }
                    MouseArea { id: refreshFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: refreshLog() }
                    ToolTip.visible: refreshFloatMA.containsMouse; ToolTip.text: qsTr("Refresh"); ToolTip.delay: 500
                }
            }
        }

        // Column headers
        Rectangle {
            Layout.fillWidth: true; height: 20
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
            radius: 3

            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 0
                Text { text: qsTr("Date/Time"); font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 128 }
                Text { text: "Call"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 85 }
                Text { text: "Grid"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 50 }
                Text { text: "Band"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 50 }
                Text { text: "Mode"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 50 }
                Text { text: qsTr("Rpt"); font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 60 }
                Text { text: "Comment"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.fillWidth: true }
            }
        }

        // QSO List
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
            radius: 4; clip: true

            ListView {
                id: logListView
                anchors.fill: parent; anchors.margins: 3
                clip: true; model: qsoList; spacing: 1
                currentIndex: selectedIndex

                ScrollBar.vertical: ScrollBar {
                    active: true; policy: ScrollBar.AsNeeded
                    contentItem: Rectangle { implicitWidth: 4; radius: 2; color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) }
                    background: Rectangle { implicitWidth: 4; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.03); radius: 2 }
                }

                delegate: Rectangle {
                    width: logListView.width - 6; height: 24; radius: 3
                    color: index === selectedIndex
                           ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                           : logRowMA.containsMouse
                             ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
                             : (index % 2 === 0 ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05))
                    border.color: index === selectedIndex ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.5) : "transparent"
                    border.width: index === selectedIndex ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 100 } }

                    MouseArea {
                        id: logRowMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (selectedIndex === index) {
                                clearSelection()
                            } else {
                                selectedIndex = index
                                selectedQso = modelData
                                editCallF.text = modelData.call || ""
                                editGridF.text = modelData.grid || ""
                                editBandF.text = modelData.band || ""
                                editModeF.text = modelData.mode || ""
                                editSentF.text = modelData.reportSent || ""
                                editRcvdF.text = modelData.reportReceived || ""
                                editCommentF.text = modelData.comment || ""
                            }
                        }
                        onDoubleClicked: { if (appEngine) { appEngine.dxCall = modelData.call; appEngine.dxGrid = modelData.grid } }
                    }

                    // Same context action in the detached/floating log view.
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        enabled: String(modelData.call || "").trim().length > 0
                        onTapped: logCallContextMenu.popup()
                    }

                    Menu {
                        id: logCallContextMenu
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
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 0
                        Text { text: index === selectedIndex ? "\u25B6" : ""; font.pixelSize: 7; color: primaryBlue; Layout.preferredWidth: 10 }
                        Text { text: modelData.dateTime || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 10; color: textSecondary; Layout.preferredWidth: 118 }
                        Text { text: modelData.call || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 10; font.bold: true; color: accentGreen; Layout.preferredWidth: 85 }
                        Text { text: modelData.grid || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 10; color: secondaryCyan; Layout.preferredWidth: 50 }
                        Text { text: modelData.band || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 10; color: textPrimary; Layout.preferredWidth: 50 }
                        Text { text: modelData.mode || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 10; color: textPrimary; Layout.preferredWidth: 50 }
                        Text { text: (modelData.reportSent || "") + "/" + (modelData.reportReceived || ""); font.family: decodiumMonoFontFamily; font.pixelSize: 10; color: textSecondary; Layout.preferredWidth: 60 }
                        Text { text: modelData.comment || ""; font.family: decodiumMonoFontFamily; font.pixelSize: 10; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.45); Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }

                Text {
                    anchors.centerIn: parent; text: qsoList.length === 0 ? "No QSOs found" : ""
                    font.pixelSize: 12; font.bold: true; color: textSecondary; visible: qsoList.length === 0
                }
            }
        }

        // ======= EDIT PANEL (visible when a QSO is selected) =======
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: editPanelFloatContent.implicitHeight + 12
            visible: selectedIndex >= 0 && selectedQso !== null
            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.06)
            border.color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
            radius: 5

            ColumnLayout {
                id: editPanelFloatContent
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                // Row 1: fields
                RowLayout {
                    spacing: 6

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Call"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 112; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editCallF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            DecoTextField {
                                id: editCallF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily; font.bold: true
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#FFFFFF"; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Grid"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 70; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editGridF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            DecoTextField {
                                id: editGridF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#FFFFFF"; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Band"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 58; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editBandF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            DecoTextField {
                                id: editBandF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#FFFFFF"; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Mode"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 58; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editModeF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            DecoTextField {
                                id: editModeF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#FFFFFF"; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: qsTr("Sent"); font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 52; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editSentF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            DecoTextField {
                                id: editSentF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#FFFFFF"; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: qsTr("Rcvd"); font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 52; height: 30; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editRcvdF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            DecoTextField {
                                id: editRcvdF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#FFFFFF"; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Row 2: Comment + buttons
                RowLayout {
                    spacing: 6

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text { text: "Comment"; font.pixelSize: 8; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editCommentF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            DecoTextField {
                                id: editCommentF; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 10; font.family: decodiumMonoFontFamily
                                leftPadding: 6; rightPadding: 4; topPadding: 0; bottomPadding: 0
                                verticalAlignment: TextInput.AlignVCenter
                                color: textPrimary; background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignBottom
                        spacing: 4

                        // Save
                        Rectangle {
                            width: saveLabelF.width + 14; height: 22; radius: 3
                            color: saveFloatMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.4) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2)
                            border.color: accentGreen
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text { id: saveLabelF; anchors.centerIn: parent; text: qsTr("\u2713 Save"); font.pixelSize: 9; font.bold: true; color: accentGreen }
                            MouseArea {
                                id: saveFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (appEngine && appEngine.logManager && selectedQso) {
                                        var newData = {
                                            "call": editCallF.text, "grid": editGridF.text,
                                            "band": editBandF.text, "mode": editModeF.text,
                                            "reportSent": editSentF.text, "reportReceived": editRcvdF.text,
                                            "comment": editCommentF.text
                                        }
                                        appEngine.logManager.editQso(selectedQso.call, selectedQso.dateTime, newData)
                                        clearSelection()
                                        refreshLog()
                                    }
                                }
                            }
                        }

                        // Delete
                        Rectangle {
                            width: deleteLabelF.width + 14; height: 22; radius: 3
                            color: deleteFloatMA.containsMouse ? Qt.rgba(1, 0, 0, 0.3) : Qt.rgba(1, 0, 0, 0.1)
                            border.color: "#d32f2f"
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text { id: deleteLabelF; anchors.centerIn: parent; text: qsTr("\u2715 Delete"); font.pixelSize: 9; font.bold: true; color: "#d32f2f" }
                            MouseArea {
                                id: deleteFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (selectedQso) {
                                        deleteConfirmDialog.deleteCall = selectedQso.call
                                        deleteConfirmDialog.deleteDateTime = selectedQso.dateTime
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }

                        // Cancel
                        Rectangle {
                            width: cancelLabelF.width + 14; height: 22; radius: 3
                            color: cancelFloatMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                            border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text { id: cancelLabelF; anchors.centerIn: parent; text: qsTr("Cancel"); font.pixelSize: 9; color: textSecondary }
                            MouseArea { id: cancelFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: clearSelection() }
                        }
                    }
                }
            }
        }
    }
}
