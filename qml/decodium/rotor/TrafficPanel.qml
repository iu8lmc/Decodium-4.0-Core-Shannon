import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassPanel {
    id: panel

    title: qsTr("SERIAL TRAFFIC")

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: qsTr("%1 @ %2 8N1").arg(rotor.port).arg(rotor.baudrate)
                color: RotorTheme.textSecondary
                font.pixelSize: RotorTheme.fontSmall
                font.family: RotorTheme.monoFamily
            }

            Item { Layout.fillWidth: true }

            StatusLed {
                colour: RotorTheme.accent
                label: qsTr("%1 frame").arg(rotor.traffic.length)
            }
        }

        ListView {
            id: log

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: rotor.traffic
            clip: true
            spacing: 1
            reuseItems: true
            onCountChanged: positionViewAtEnd()

            delegate: Row {
                id: entry

                required property var modelData

                readonly property bool outgoing: modelData.dir === "tx"

                spacing: 10

                Text {
                    text: entry.outgoing ? "TX" : "RX"
                    color: entry.outgoing ? RotorTheme.warning : RotorTheme.accent
                    font.pixelSize: RotorTheme.fontSmall
                    font.family: RotorTheme.monoFamily
                    font.bold: true
                }

                Text {
                    text: entry.modelData.frame
                    color: RotorTheme.textPrimary
                    font.pixelSize: RotorTheme.fontSmall
                    font.family: RotorTheme.monoFamily
                }

                Text {
                    text: entry.modelData.hex
                    color: RotorTheme.textDim
                    font.pixelSize: RotorTheme.fontSmall
                    font.family: RotorTheme.monoFamily
                }
            }
        }
    }
}
