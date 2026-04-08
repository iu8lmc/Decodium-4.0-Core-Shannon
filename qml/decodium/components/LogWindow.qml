/* Decodium 2.5 - QSO Log Window
 * Full Glassmorphism with Edit Panel, Import/Export ADIF
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Popup {
    id: logWindow
    width: 960
    height: 720
    modal: false
    closePolicy: Popup.CloseOnEscape
    padding: 0
    anchors.centerIn: parent ? parent : undefined

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

    onOpened: refreshLog()

    function refreshLog() {
        if (appEngine && appEngine.logManager) {
            qsoList = appEngine.logManager.searchQsos(
                searchField.text,
                bandFilter.currentText === "All" ? "" : bandFilter.currentText,
                modeFilter.currentText === "All" ? "" : modeFilter.currentText, "", "")
            stats = appEngine.logManager.getQsoStats()
        }
    }

    function clearSelection() {
        selectedIndex = -1
        selectedQso = null
    }

    // Real-time update: refresh log when a new QSO is logged (via LogManager signal)
    Connections {
        target: appEngine && appEngine.logManager ? appEngine.logManager : null
        function onQsoLogged(call, mode, band) { if (logWindow.visible) refreshLog() }
    }

    // Real-time update: refresh log when DecodiumBridge signals a QSO logged
    Connections {
        target: bridge
        function onQsoLogged(call, grid, report) { if (logWindow.visible) refreshLog() }
    }

    // Fallback auto-refresh: catches auto-sequence QSOs that bypass MshvBridge
    Timer {
        interval: 3000
        running: logWindow.visible
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

    // Import ADIF FileDialog
    FileDialog {
        id: importFileDialog
        title: "Importa file ADIF"
        nameFilters: ["ADIF files (*.adi *.adif)", "All files (*)"]
        onAccepted: {
            if (appEngine && appEngine.logManager) {
                var path = selectedFile.toString().replace("file:///", "")
                var count = appEngine.logManager.importFromAdif(path)
                clearSelection()
                refreshLog()
            }
        }
    }

    // Export ADIF FileDialog
    FileDialog {
        id: exportFileDialog
        title: "Esporta file ADIF"
        nameFilters: ["ADIF files (*.adi *.adif)", "All files (*)"]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (appEngine && appEngine.logManager) {
                var path = selectedFile.toString().replace("file:///", "")
                appEngine.logManager.exportToAdif(path)
            }
        }
    }

    // Delete confirmation dialog
    Dialog {
        id: deleteConfirmDialog
        title: "Conferma eliminazione"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        property string deleteCall: ""
        property string deleteDateTime: ""

        Label {
            text: "Eliminare il QSO con " + deleteConfirmDialog.deleteCall + "?"
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
                    text: "QSO Log"
                    font.pixelSize: 15
                    font.bold: true
                    font.letterSpacing: 0.5
                    color: secondaryCyan
                }

                Rectangle { width: 1; height: 18; color: glassBorder }

                // Inline stats
                Text {
                    text: (stats.totalQsos || 0) + " QSOs"
                    font.pixelSize: 12; font.bold: true; font.family: "Consolas"
                    color: accentGreen
                }
                Text {
                    text: (stats.uniqueCalls || 0) + " Calls"
                    font.pixelSize: 11; font.family: "Consolas"
                    color: textSecondary
                }
                Text {
                    text: (stats.uniqueGrids || 0) + " Grids"
                    font.pixelSize: 11; font.family: "Consolas"
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
                        anchors.centerIn: parent; text: "\u2212"
                        font.pixelSize: 16; font.bold: true
                        color: logMinMA.containsMouse ? "#ffc107" : textPrimary
                    }
                    MouseArea {
                        id: logMinMA; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { logWindowMinimized = true; logWindow.close() }
                    }
                    ToolTip.visible: logMinMA.containsMouse
                    ToolTip.text: "Riduci a icona"; ToolTip.delay: 500
                }

                // Close
                Rectangle {
                    width: 26; height: 26; radius: 4
                    color: logCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                    border.color: logCloseMA.containsMouse ? "#f44336" : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent; text: "\u2715"
                        font.pixelSize: 11; font.bold: true
                        color: logCloseMA.containsMouse ? "#f44336" : textPrimary
                    }
                    MouseArea {
                        id: logCloseMA; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: logWindow.close()
                    }
                    ToolTip.visible: logCloseMA.containsMouse
                    ToolTip.text: "Chiudi"; ToolTip.delay: 500
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

                // Search field
                Rectangle {
                    Layout.preferredWidth: 200; height: 28; radius: 4
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                    border.color: searchField.focus ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                    border.width: searchField.focus ? 2 : 1
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 4
                        Text { text: "\uD83D\uDD0D"; font.pixelSize: 11; color: textSecondary }
                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "Search call, grid..."
                            font.pixelSize: 11; font.family: "Consolas"
                            color: textPrimary
                            placeholderTextColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            onTextChanged: { clearSelection(); refreshLog() }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                StyledComboBox {
                    id: bandFilter
                    model: ["All", "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m", "70cm"]
                    Layout.preferredWidth: 85; height: 28
                    font.pixelSize: 10
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                StyledComboBox {
                    id: modeFilter
                    model: ["All", "FT8", "FT4", "SFox", "JT65", "JT9", "Q65", "MSK144", "MSK40", "FSK441"]
                    Layout.preferredWidth: 85; height: 28
                    font.pixelSize: 10
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                Item { Layout.fillWidth: true }

                // Import ADIF
                Rectangle {
                    width: importLabel.width + 16; height: 28; radius: 4
                    color: importMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)
                    border.color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.5)
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text { id: importLabel; anchors.centerIn: parent; text: "Import ADIF"; font.pixelSize: 10; font.bold: true; color: accentGreen }
                    MouseArea {
                        id: importMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: importFileDialog.open()
                    }
                    ToolTip.visible: importMA.containsMouse; ToolTip.text: "Importa file ADIF"; ToolTip.delay: 500
                }

                // Export ADIF
                Rectangle {
                    width: adifLabel.width + 16; height: 28; radius: 4
                    color: exportMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
                    border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5)
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text { id: adifLabel; anchors.centerIn: parent; text: "Export ADIF"; font.pixelSize: 10; font.bold: true; color: secondaryCyan }
                    MouseArea {
                        id: exportMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: exportFileDialog.open()
                    }
                    ToolTip.visible: exportMA.containsMouse; ToolTip.text: "Esporta ADIF"; ToolTip.delay: 500
                }

                // Refresh
                Rectangle {
                    width: 28; height: 28; radius: 4
                    color: refreshMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text { anchors.centerIn: parent; text: "\u21BB"; font.pixelSize: 14; font.bold: true; color: refreshMA.containsMouse ? accentGreen : textSecondary }
                    MouseArea {
                        id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: refreshLog()
                    }
                    ToolTip.visible: refreshMA.containsMouse; ToolTip.text: "Aggiorna"; ToolTip.delay: 500
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

                Text { text: "Date/Time"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 130 }
                Text { text: "Call"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 100 }
                Text { text: "Grid"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 60 }
                Text { text: "Band"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 55 }
                Text { text: "Mode"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 55 }
                Text { text: "Sent"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                Text { text: "Rcvd"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                Text { text: "Dist"; font.pixelSize: 10; font.bold: true; color: secondaryCyan; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 60 }
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
                        Text { text: modelData.dateTime || ""; font.family: "Consolas"; font.pixelSize: 11; color: textSecondary; Layout.preferredWidth: 118 }
                        Text { text: modelData.call || ""; font.family: "Consolas"; font.pixelSize: 11; font.bold: true; color: accentGreen; Layout.preferredWidth: 100 }
                        Text { text: modelData.grid || ""; font.family: "Consolas"; font.pixelSize: 11; color: secondaryCyan; Layout.preferredWidth: 60 }
                        Text { text: modelData.band || ""; font.family: "Consolas"; font.pixelSize: 11; color: textPrimary; Layout.preferredWidth: 55 }
                        Text { text: modelData.mode || ""; font.family: "Consolas"; font.pixelSize: 11; color: textPrimary; Layout.preferredWidth: 55 }
                        Text { text: modelData.reportSent || ""; font.family: "Consolas"; font.pixelSize: 11; color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                        Text { text: modelData.reportReceived || ""; font.family: "Consolas"; font.pixelSize: 11; color: textSecondary; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 45 }
                        Text { text: (modelData.distance || 0) > 0 ? modelData.distance + " km" : ""; font.family: "Consolas"; font.pixelSize: 11; color: accentOrange; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 60 }
                        Text { text: modelData.comment || ""; font.family: "Consolas"; font.pixelSize: 11; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.45); Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }

                // Empty state
                Column {
                    anchors.centerIn: parent; spacing: 6
                    visible: qsoList.length === 0
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "No QSOs found"; font.pixelSize: 13; font.bold: true; color: textSecondary }
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
                            width: 120; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editCall.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editCall; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"; font.bold: true
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
                            width: 70; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editGrid.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editGrid; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"
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
                            width: 60; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editBand.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editBand; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"
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
                            width: 60; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editMode.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editMode; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"
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
                            width: 50; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editSent.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editSent; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"
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
                            width: 50; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editRcvd.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editRcvd; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"
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
                            Layout.fillWidth: true; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editComment.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField {
                                id: editComment; anchors.fill: parent; anchors.margins: 2
                                font.pixelSize: 11; font.family: "Consolas"
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

                            Text { id: saveLabel; anchors.centerIn: parent; text: "\u2713 Salva"; font.pixelSize: 10; font.bold: true; color: accentGreen }
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

                            Text { id: deleteLabel; anchors.centerIn: parent; text: "\u2715 Elimina"; font.pixelSize: 10; font.bold: true; color: "#d32f2f" }
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

                            Text { id: cancelLabel; anchors.centerIn: parent; text: "Annulla"; font.pixelSize: 10; color: textSecondary }
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
                    Text { text: stats.totalQsos || "0"; font.pixelSize: 20; font.bold: true; color: accentGreen; font.family: "Consolas" }
                }

                Rectangle { width: 1; height: 30; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

                Column {
                    spacing: 0
                    Text { text: "CALLS"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.uniqueCalls || "0"; font.pixelSize: 20; font.bold: true; color: secondaryCyan; font.family: "Consolas" }
                }

                Column {
                    spacing: 0
                    Text { text: "GRIDS"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.uniqueGrids || "0"; font.pixelSize: 20; font.bold: true; color: secondaryCyan; font.family: "Consolas" }
                }

                Rectangle { width: 1; height: 30; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

                Column {
                    spacing: 0
                    Text { text: "MAX DIST"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: (stats.maxDistance || 0) + " km"; font.pixelSize: 16; font.bold: true; color: accentOrange; font.family: "Consolas" }
                }

                Column {
                    spacing: 0
                    Text { text: "FARTHEST"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: stats.farthestCall || "-"; font.pixelSize: 16; font.bold: true; color: accentOrange; font.family: "Consolas" }
                }

                Item { Layout.fillWidth: true }

                Column {
                    spacing: 0
                    Text { text: "SHOWING"; font.pixelSize: 8; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5); font.letterSpacing: 1; font.bold: true }
                    Text { text: qsoList.length + " QSOs"; font.pixelSize: 14; font.bold: true; color: textPrimary; font.family: "Consolas" }
                }
            }
        }
    }
}
