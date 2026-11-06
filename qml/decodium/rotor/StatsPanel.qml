import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: panel

    title: qsTr("OPERATION")

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Repeater {
            model: [
                { caption: qsTr("Frames sent"), value: rotor.txFrames.toString(), tint: RotorTheme.warning },
                { caption: qsTr("Frames received"), value: rotor.rxFrames.toString(), tint: RotorTheme.accent },
                { caption: qsTr("Lost replies"), value: rotor.errorCount.toString(),
                  tint: rotor.errorCount > 0 ? RotorTheme.danger : RotorTheme.textSecondary },
                { caption: qsTr("Reconnections"), value: rotor.reconnects.toString(),
                  tint: rotor.reconnects > 0 ? RotorTheme.warning : RotorTheme.textSecondary },
                { caption: qsTr("Up for"), value: rotor.uptimeText, tint: RotorTheme.primary },
                { caption: qsTr("Connected clients"), value: rotor.clients.toString(), tint: RotorTheme.primary }
            ]

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: modelData.caption
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                    elide: Text.ElideRight
                }

                Text {
                    text: modelData.value
                    color: modelData.tint
                    font.pixelSize: 18
                    font.family: RotorTheme.monoFamily
                    font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: RotorTheme.borderSoft
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("Control box")
                color: RotorTheme.textSecondary
                font.pixelSize: RotorTheme.fontBody
            }

            Text {
                text: rotor.modelLabel
                color: RotorTheme.textPrimary
                font.pixelSize: RotorTheme.fontSmall
                elide: Text.ElideRight
                Layout.maximumWidth: 220
            }
        }

        Item { Layout.fillHeight: true }
    }
}
