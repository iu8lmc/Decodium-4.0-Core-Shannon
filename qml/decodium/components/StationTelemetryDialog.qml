import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// IU8LMC — Popup "info stazione + meteo" ricevuta via FT8/FT4 (tipo 0.5,
// opt-in su entrambi i lati). Il tipo di messaggio non porta il nominativo:
// likelyCall e' una correlazione temporale col QSO appena loggato, non una
// certezza, e va mostrata come tale.
Dialog {
    id: telemetryDialog
    title: qsTr("Station info received")
    modal: true
    width: 420
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton

    Material.theme: Material.Dark
    Material.accent: Material.Cyan

    property color bgDeep: "#1a1a2e"
    property color accentCyan: "#00d9ff"
    property color textSecondary: "#9aa6b8"

    // Impostata da chi apre il dialog prima di chiamare open().
    property var fields: ({})

    background: Rectangle {
        color: telemetryDialog.bgDeep
        radius: 12
        border.color: telemetryDialog.accentCyan
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: telemetryDialog.fields.likelyCall
                ? qsTr("Presumably from %1").arg(telemetryDialog.fields.likelyCall)
                : qsTr("Sender unknown — this message type carries no callsign")
            color: telemetryDialog.accentCyan
            font.pixelSize: 16
            font.bold: true
            wrapMode: Text.Wrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 6

            Label { text: "🌡 " + qsTr("Temperature:"); color: telemetryDialog.textSecondary }
            Label {
                text: (telemetryDialog.fields.tempC === undefined || telemetryDialog.fields.tempC === null)
                    ? qsTr("unknown") : qsTr("%1 °C").arg(telemetryDialog.fields.tempC)
                color: "white"
            }
            Label { text: "💨 " + qsTr("Wind:"); color: telemetryDialog.textSecondary }
            Label { text: qsTr("%1 km/h %2").arg(telemetryDialog.fields.windSpeedKmh || 0).arg(telemetryDialog.fields.windDirLabel || ""); color: "white" }
            Label { text: "☁ " + qsTr("Sky:"); color: telemetryDialog.textSecondary }
            Label { text: telemetryDialog.fields.sky || "?"; color: "white" }
            Label { text: "📍 " + qsTr("Locator:"); color: telemetryDialog.textSecondary }
            Label { text: telemetryDialog.fields.grid4 || "?"; color: "white" }
            Label { text: "🔌 " + qsTr("Power:"); color: telemetryDialog.textSecondary }
            Label { text: qsTr("%1 W").arg(telemetryDialog.fields.powerWatts || 0); color: "white" }
            Label { text: "📻 " + qsTr("Radio:"); color: telemetryDialog.textSecondary }
            Label { text: telemetryDialog.fields.radioModel || "?"; color: "white"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "📡 " + qsTr("Antenna:"); color: telemetryDialog.textSecondary }
            Label { text: telemetryDialog.fields.antennaType || "?"; color: "white"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }

        Button {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Close")
            onClicked: telemetryDialog.close()
        }
    }
}
