/* Decodium 2.5 - Log Window Content (floating version)
 * Full Glassmorphism with Edit Panel, Import/Export ADIF
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

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

    Component.onCompleted: refreshLog()

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

    // Fallback auto-refresh
    Timer {
        interval: 3000
        running: true
        repeat: true
        onTriggered: refreshLog()
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Stats + Filter bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.06)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8
                spacing: 6

                Text { text: (stats.totalQsos || 0) + " QSOs"; font.pixelSize: 11; font.bold: true; font.family: "Monospace"; color: accentGreen }
                Rectangle { width: 1; height: 16; color: glassBorder }
                Text { text: (stats.uniqueCalls || 0) + " Calls"; font.pixelSize: 10; font.family: "Monospace"; color: textSecondary }

                Rectangle { width: 1; height: 16; color: glassBorder }

                Rectangle {
                    Layout.preferredWidth: 130; height: 26; radius: 4
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                    border.color: searchField.focus ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                    border.width: searchField.focus ? 2 : 1
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 3; spacing: 3
                        Text { text: "\uD83D\uDD0D"; font.pixelSize: 10; color: textSecondary }
                        TextField {
                            id: searchField; Layout.fillWidth: true
                            placeholderText: "Search..."
                            font.pixelSize: 10; font.family: "Monospace"
                            color: textPrimary; placeholderTextColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            onTextChanged: { clearSelection(); refreshLog() }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                StyledComboBox {
                    id: bandFilter
                    model: ["All", "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"]
                    Layout.preferredWidth: 75; height: 26; font.pixelSize: 9
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                StyledComboBox {
                    id: modeFilter
                    model: ["All", "FT8", "FT4", "SFox", "JT65", "JT9", "Q65", "MSK144"]
                    Layout.preferredWidth: 75; height: 26; font.pixelSize: 9
                    onCurrentTextChanged: { clearSelection(); refreshLog() }
                }

                Item { Layout.fillWidth: true }

                // Import ADIF
                Rectangle {
                    width: 22; height: 22; radius: 3
                    color: importFloatMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: "\u2B07"; font.pixelSize: 11; color: importFloatMA.containsMouse ? accentGreen : textSecondary }
                    MouseArea { id: importFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: importFileDialog.open() }
                    ToolTip.visible: importFloatMA.containsMouse; ToolTip.text: "Import ADIF"; ToolTip.delay: 500
                }

                // Export ADIF
                Rectangle {
                    width: 22; height: 22; radius: 3
                    color: exportFloatMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: "\u2B06"; font.pixelSize: 11; color: exportFloatMA.containsMouse ? secondaryCyan : textSecondary }
                    MouseArea { id: exportFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: exportFileDialog.open() }
                    ToolTip.visible: exportFloatMA.containsMouse; ToolTip.text: "Export ADIF"; ToolTip.delay: 500
                }

                // Refresh
                Rectangle {
                    width: 22; height: 22; radius: 3
                    color: refreshFloatMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: "\u21BB"; font.pixelSize: 12; font.bold: true; color: refreshFloatMA.containsMouse ? accentGreen : textSecondary }
                    MouseArea { id: refreshFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: refreshLog() }
                    ToolTip.visible: refreshFloatMA.containsMouse; ToolTip.text: "Aggiorna"; ToolTip.delay: 500
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
                Text { text: "Date/Time"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 115 }
                Text { text: "Call"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 85 }
                Text { text: "Grid"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 50 }
                Text { text: "Band"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 50 }
                Text { text: "Mode"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 50 }
                Text { text: "Rpt"; font.pixelSize: 9; font.bold: true; color: secondaryCyan; Layout.preferredWidth: 60 }
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

                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 0
                        Text { text: index === selectedIndex ? "\u25B6" : ""; font.pixelSize: 7; color: primaryBlue; Layout.preferredWidth: 10 }
                        Text { text: modelData.dateTime || ""; font.family: "Monospace"; font.pixelSize: 10; color: textSecondary; Layout.preferredWidth: 105 }
                        Text { text: modelData.call || ""; font.family: "Monospace"; font.pixelSize: 10; font.bold: true; color: accentGreen; Layout.preferredWidth: 85 }
                        Text { text: modelData.grid || ""; font.family: "Monospace"; font.pixelSize: 10; color: secondaryCyan; Layout.preferredWidth: 50 }
                        Text { text: modelData.band || ""; font.family: "Monospace"; font.pixelSize: 10; color: textPrimary; Layout.preferredWidth: 50 }
                        Text { text: modelData.mode || ""; font.family: "Monospace"; font.pixelSize: 10; color: textPrimary; Layout.preferredWidth: 50 }
                        Text { text: (modelData.reportSent || "") + "/" + (modelData.reportReceived || ""); font.family: "Monospace"; font.pixelSize: 10; color: textSecondary; Layout.preferredWidth: 60 }
                        Text { text: modelData.comment || ""; font.family: "Monospace"; font.pixelSize: 10; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.45); Layout.fillWidth: true; elide: Text.ElideRight }
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
                            width: 100; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editCallF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            TextField { id: editCallF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 12; font.family: "Monospace"; font.bold: true; color: "#FFFFFF"; background: Rectangle { color: "transparent" } }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Grid"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 60; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editGridF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            TextField { id: editGridF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 12; font.family: "Monospace"; color: "#FFFFFF"; background: Rectangle { color: "transparent" } }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Band"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 50; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editBandF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            TextField { id: editBandF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 12; font.family: "Monospace"; color: "#FFFFFF"; background: Rectangle { color: "transparent" } }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Mode"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 50; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editModeF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            TextField { id: editModeF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 12; font.family: "Monospace"; color: "#FFFFFF"; background: Rectangle { color: "transparent" } }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Sent"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 45; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editSentF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            TextField { id: editSentF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 12; font.family: "Monospace"; color: "#FFFFFF"; background: Rectangle { color: "transparent" } }
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text { text: "Rcvd"; font.pixelSize: 9; font.bold: true; color: secondaryCyan }
                        Rectangle {
                            width: 45; height: 26; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                            border.color: editRcvdF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.25)
                            TextField { id: editRcvdF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 12; font.family: "Monospace"; color: "#FFFFFF"; background: Rectangle { color: "transparent" } }
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
                            Layout.fillWidth: true; height: 22; radius: 3
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: editCommentF.focus ? primaryBlue : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                            TextField { id: editCommentF; anchors.fill: parent; anchors.margins: 2; font.pixelSize: 10; font.family: "Monospace"; color: textPrimary; background: Rectangle { color: "transparent" } }
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
                            Text { id: saveLabelF; anchors.centerIn: parent; text: "\u2713 Salva"; font.pixelSize: 9; font.bold: true; color: accentGreen }
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
                            Text { id: deleteLabelF; anchors.centerIn: parent; text: "\u2715 Elimina"; font.pixelSize: 9; font.bold: true; color: "#d32f2f" }
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
                            Text { id: cancelLabelF; anchors.centerIn: parent; text: "Annulla"; font.pixelSize: 9; color: textSecondary }
                            MouseArea { id: cancelFloatMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: clearSelection() }
                        }
                    }
                }
            }
        }
    }
}
