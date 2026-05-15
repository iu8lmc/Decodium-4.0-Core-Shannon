// 1.0.195 — QSY Quick Picker: dialog rapido per QSY su Working Frequencies preset.
// Mostra una griglia/lista filtrata dalla tabella `workingFrequencyRows` del bridge.
// Click → bridge.qsyToWorkingFrequency(index) → setFrequency + setMode in sequenza.
//
// Filtri:
//   - "Preferred only" (default ON): mostra solo i preset con preferred=true.
//   - Filter per mode (FT8/FT4/FT2/CW/SSB...): ComboBox.
//   - Filter per band (40m/20m/15m/...): ComboBox.
//
// UX: apertura via Shortcut F2 o pulsante (TBD posizione).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: qsyPickerDialog
    title: qsTr("QSY Quick Picker")
    width: 580
    height: 540
    minimumWidth: 420
    minimumHeight: 360
    flags: Qt.Dialog | Qt.WindowCloseButtonHint
    color: "#1a1a2e"

    property var allRows: []
    property bool preferredOnly: true
    property string modeFilter: ""
    property string bandFilter: ""

    function reload() {
        if (!bridge) { allRows = []; return }
        allRows = bridge.workingFrequencyRows()
    }

    function filteredRows() {
        var out = []
        for (var i = 0; i < allRows.length; ++i) {
            var r = allRows[i]
            if (preferredOnly && !r.preferred) continue
            if (modeFilter && modeFilter !== "Tutti" && r.mode !== modeFilter) continue
            if (bandFilter && bandFilter !== "Tutte" && r.band !== bandFilter) continue
            out.push(r)
        }
        return out
    }

    function uniqueModes() {
        var set = {}; var arr = ["Tutti"]
        for (var i = 0; i < allRows.length; ++i) {
            var m = allRows[i].mode
            if (m && !set[m]) { set[m] = true; arr.push(m) }
        }
        return arr
    }

    function uniqueBands() {
        var set = {}; var arr = ["Tutte"]
        for (var i = 0; i < allRows.length; ++i) {
            var b = allRows[i].band
            if (b && b !== "OOB" && !set[b]) { set[b] = true; arr.push(b) }
        }
        return arr
    }

    onVisibleChanged: if (visible) reload()

    Rectangle {
        anchors.fill: parent
        color: "#1a1a2e"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: qsTr("QSY a preset Working Frequencies")
                    color: "#ff7814"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("%1 preset · %2 visibili")
                        .arg(qsyPickerDialog.allRows.length)
                        .arg(qsyPickerDialog.filteredRows().length)
                    color: "#aaaacc"
                    font.pixelSize: 11
                }
            }

            // Filtri
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                CheckBox {
                    text: qsTr("Solo Preferred")
                    checked: qsyPickerDialog.preferredOnly
                    onCheckedChanged: qsyPickerDialog.preferredOnly = checked
                    contentItem: Text { text: parent.text; color: "#cccccc"; leftPadding: 22; verticalAlignment: Text.AlignVCenter }
                    indicator: Rectangle { width: 16; height: 16; radius: 3; color: parent.checked ? "#ff7814" : "#2a2a3e"; border.color: "#505070"; y: parent.height/2 - height/2 }
                }
                Text { text: qsTr("Modo:"); color: "#aaaacc"; font.pixelSize: 12 }
                ComboBox {
                    id: modeCombo
                    model: qsyPickerDialog.uniqueModes()
                    currentIndex: 0
                    onActivated: qsyPickerDialog.modeFilter = currentText
                    implicitWidth: 100
                }
                Text { text: qsTr("Banda:"); color: "#aaaacc"; font.pixelSize: 12 }
                ComboBox {
                    id: bandCombo
                    model: qsyPickerDialog.uniqueBands()
                    currentIndex: 0
                    onActivated: qsyPickerDialog.bandFilter = currentText
                    implicitWidth: 90
                }
                Item { Layout.fillWidth: true }
            }

            // Lista preset
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#11111d"
                border.color: "#2a2a3e"
                border.width: 1
                radius: 4

                ListView {
                    id: presetList
                    anchors.fill: parent
                    anchors.margins: 4
                    model: qsyPickerDialog.filteredRows()
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        width: presetList.width
                        height: 40
                        color: hoverArea.containsMouse ? "#2a2a3e" : "#1a1a2e"
                        border.color: modelData.preferred ? "#ff7814" : "#3a3a4e"
                        border.width: modelData.preferred ? 2 : 1
                        radius: 3

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12
                            Text {
                                text: modelData.band || "OOB"
                                color: "#aaaaff"
                                font.bold: true
                                font.pixelSize: 13
                                Layout.preferredWidth: 50
                            }
                            Text {
                                text: modelData.mode || "—"
                                color: "#88ccff"
                                font.pixelSize: 12
                                Layout.preferredWidth: 60
                            }
                            Text {
                                text: modelData.frequencyMHz + " MHz"
                                color: "#ffffff"
                                font.family: Qt.platform.os === "osx" ? "Menlo" : (Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono")
                                font.pixelSize: 13
                                Layout.preferredWidth: 130
                            }
                            Text {
                                text: modelData.description || ""
                                color: "#aaaacc"
                                font.pixelSize: 11
                                font.italic: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.region || ""
                                color: "#666688"
                                font.pixelSize: 10
                                Layout.preferredWidth: 50
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (bridge) bridge.qsyToWorkingFrequency(modelData.index)
                                qsyPickerDialog.close()
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                }
            }

            // Footer
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Click su un preset per fare QSY · F2 per riaprire")
                    color: "#666688"
                    font.pixelSize: 10
                    font.italic: true
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Chiudi")
                    onClicked: qsyPickerDialog.close()
                    contentItem: Text { text: parent.text; color: "#ffffff"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: "#2a2a3e"; border.color: "#505070"; radius: 4 }
                }
            }
        }
    }
}
