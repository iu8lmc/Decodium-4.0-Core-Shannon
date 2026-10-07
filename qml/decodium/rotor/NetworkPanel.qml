import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: panel

    title: qsTr("NETWORK LINKS")

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Addresses to use on the phone and station software:")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontSmall
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: rotor.endpoints

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: 10

                StatusLed {
                    colour: modelData.active ? RotorTheme.accent : RotorTheme.danger
                    label: modelData.role
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: modelData.address
                    color: RotorTheme.textPrimary
                    font.pixelSize: RotorTheme.fontBody
                    font.family: RotorTheme.monoFamily
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: RotorTheme.borderSoft
        }

        Text {
            Layout.fillWidth: true
            text: rotor.tokenRequired
                  ? qsTr("Token-protected access: clients must present it.")
                  : qsTr("Open access on the local network. For use away from home go through a VPN; do not open ports on the router.")
            color: rotor.tokenRequired ? RotorTheme.accent : RotorTheme.warning
            font.pixelSize: RotorTheme.fontSmall
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }
    }
}
