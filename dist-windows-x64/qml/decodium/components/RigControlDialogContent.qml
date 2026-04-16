/* Rig Control Dialog Content - CAT Configuration
 * COM port, baud rate, PTT method for Kenwood TS-590S
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "transparent"

    property color primaryBlue:    bridge.themeManager.primaryColor
    property color secondaryCyan:  bridge.themeManager.secondaryColor
    property color accentGreen:    bridge.themeManager.accentColor
    property color textPrimary:    bridge.themeManager.textPrimary
    property color textSecondary:  bridge.themeManager.textSecondary
    property color glassBorder:    bridge.themeManager.glassBorder
    property color errorRed:       "#f44336"

    property var cat: bridge.catManager

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        // ── Status bar ──────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 48
            radius: 8
            color: cat && cat.connected
                   ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15)
                   : Qt.rgba(errorRed.r,    errorRed.g,    errorRed.b,    0.12)
            border.color: cat && cat.connected ? accentGreen : errorRed

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Rectangle {
                    width: 14; height: 14; radius: 7
                    color: cat && cat.connected ? accentGreen : errorRed
                }
                Text {
                    text: cat && cat.connected
                          ? "Connected — " + (cat.rigName || "Kenwood")
                          : "Not connected"
                    font.pixelSize: 13; font.bold: true
                    color: cat && cat.connected ? accentGreen : errorRed
                }
                Item { Layout.fillWidth: true }
                // Frequency + mode when connected
                Text {
                    visible: cat && cat.connected && cat.frequency > 0
                    text: cat ? (cat.frequency / 1e6).toFixed(6) + " MHz  " + cat.mode : ""
                    font.pixelSize: 12; font.family: "Monospace"
                    color: secondaryCyan
                }
            }
        }

        // ── Configuration grid ──────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 10
            columnSpacing: 12

            // Row 1 — Radio model
            Text { text: "Radio"; color: textSecondary; font.pixelSize: 12 }
            ComboBox {
                id: rigCombo
                Layout.fillWidth: true
                implicitHeight: 34
                model: cat ? cat.rigList : []
                Component.onCompleted: {
                    if (!cat) return
                    var idx = find(cat.rigName)
                    if (idx >= 0) currentIndex = idx
                }
                onActivated: if (cat) cat.rigName = currentText
                background: Rectangle { color: Qt.rgba(1,1,1,0.05); border.color: glassBorder; radius: 4 }
                contentItem: Text { leftPadding: 8; text: rigCombo.displayText; color: textPrimary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
            }

            // Row 2 — COM port
            Text { text: "Serial port"; color: textSecondary; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                ComboBox {
                    id: portCombo
                    Layout.fillWidth: true
                    implicitHeight: 34
                    model: cat ? cat.portList : []
                    Component.onCompleted: {
                        if (!cat) return
                        var idx = find(cat.serialPort)
                        if (idx >= 0) currentIndex = idx
                    }
                    onActivated: if (cat) cat.serialPort = currentText
                    background: Rectangle { color: Qt.rgba(1,1,1,0.05); border.color: glassBorder; radius: 4 }
                    contentItem: Text { leftPadding: 8; text: portCombo.displayText; color: textPrimary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    implicitWidth: 32; implicitHeight: 34
                    text: "↻"
                    font.pixelSize: 16
                    background: Rectangle { color: Qt.rgba(1,1,1,0.07); border.color: glassBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: secondaryCyan; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: if (cat) { cat.refreshPorts(); var idx = portCombo.find(cat.serialPort); if (idx >= 0) portCombo.currentIndex = idx }
                    ToolTip.visible: hovered; ToolTip.text: "Refresh ports"; ToolTip.delay: 500
                }
            }

            // Row 3 — Baud rate
            Text { text: "Baud rate"; color: textSecondary; font.pixelSize: 12 }
            ComboBox {
                id: baudCombo
                Layout.fillWidth: true
                implicitHeight: 34
                model: cat ? cat.baudList : []
                Component.onCompleted: {
                    if (!cat) return
                    var idx = find(cat.baudRate.toString())
                    if (idx >= 0) currentIndex = idx
                }
                onActivated: if (cat) cat.baudRate = parseInt(currentText)
                background: Rectangle { color: Qt.rgba(1,1,1,0.05); border.color: glassBorder; radius: 4 }
                contentItem: Text { leftPadding: 8; text: baudCombo.displayText; color: textPrimary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
            }

            // Row 4 — PTT method
            Text { text: "PTT method"; color: textSecondary; font.pixelSize: 12 }
            ComboBox {
                id: pttCombo
                Layout.fillWidth: true
                implicitHeight: 34
                model: ["CAT", "DTR", "RTS"]
                Component.onCompleted: {
                    if (!cat) return
                    var idx = find(cat.pttMethod)
                    if (idx >= 0) currentIndex = idx
                }
                onActivated: if (cat) cat.pttMethod = currentText
                background: Rectangle { color: Qt.rgba(1,1,1,0.05); border.color: glassBorder; radius: 4 }
                contentItem: Text { leftPadding: 8; text: pttCombo.displayText; color: textPrimary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
            }
        }

        // ── Auto-options ─────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            CheckBox {
                text: "Auto-connect on startup"
                font.pixelSize: 11
                checked: cat ? cat.catAutoConnect : false
                onCheckedChanged: if (cat) cat.catAutoConnect = checked
                contentItem: Text { leftPadding: 26; text: parent.text; color: textSecondary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                indicator: Rectangle {
                    implicitWidth: 18; implicitHeight: 18; radius: 3
                    x: parent.leftPadding; y: (parent.height - height) / 2
                    border.color: glassBorder
                    color: parent.checked ? accentGreen : Qt.rgba(1,1,1,0.05)
                    Text { visible: parent.parent.checked; text: "✓"; anchors.centerIn: parent; color: "#000"; font.pixelSize: 12; font.bold: true }
                }
            }

            CheckBox {
                text: "Auto-start audio"
                font.pixelSize: 11
                checked: cat ? cat.audioAutoStart : false
                onCheckedChanged: if (cat) cat.audioAutoStart = checked
                contentItem: Text { leftPadding: 26; text: parent.text; color: textSecondary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                indicator: Rectangle {
                    implicitWidth: 18; implicitHeight: 18; radius: 3
                    x: parent.leftPadding; y: (parent.height - height) / 2
                    border.color: glassBorder
                    color: parent.checked ? accentGreen : Qt.rgba(1,1,1,0.05)
                    Text { visible: parent.parent.checked; text: "✓"; anchors.centerIn: parent; color: "#000"; font.pixelSize: 12; font.bold: true }
                }
            }
        }

        // ── Note TS-590S ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 36
            radius: 6
            color: Qt.rgba(1,0.8,0,0.07)
            border.color: Qt.rgba(1,0.8,0,0.3)
            Text {
                anchors.centerIn: parent
                text: "TS-590S USB: Menu 60 COM = USB, Menu 62 BAUD = 115200 (default)"
                font.pixelSize: 10; font.italic: true
                color: Qt.rgba(1,0.85,0.2,0.8)
            }
        }

        Item { Layout.fillHeight: true }

        // ── Connect / Disconnect buttons ─────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true
                implicitHeight: 42
                text: "Save"
                background: Rectangle { radius: 6; color: Qt.rgba(1,1,1,0.07); border.color: glassBorder }
                contentItem: Text { text: parent.text; font.pixelSize: 13; font.bold: true; color: textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: if (cat) cat.saveSettings()
            }

            Button {
                Layout.fillWidth: true
                implicitHeight: 42
                enabled: cat && !cat.connected
                text: "Connect"
                background: Rectangle {
                    radius: 6
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: parent.parent.enabled ? primaryBlue : Qt.rgba(0.3,0.3,0.3,0.5) }
                        GradientStop { position: 1.0; color: parent.parent.enabled ? secondaryCyan : Qt.rgba(0.3,0.3,0.3,0.5) }
                    }
                }
                contentItem: Text { text: parent.text; font.pixelSize: 13; font.bold: true; color: textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: if (cat) cat.connectRig()
            }

            Button {
                Layout.fillWidth: true
                implicitHeight: 42
                enabled: cat && cat.connected
                text: "Disconnect"
                background: Rectangle {
                    radius: 6
                    color: parent.enabled ? Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.25) : Qt.rgba(0.3,0.3,0.3,0.3)
                    border.color: parent.enabled ? errorRed : glassBorder
                }
                contentItem: Text { text: parent.text; font.pixelSize: 13; font.bold: true; color: parent.enabled ? errorRed : textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: if (cat) cat.disconnectRig()
            }
        }
    }
}
