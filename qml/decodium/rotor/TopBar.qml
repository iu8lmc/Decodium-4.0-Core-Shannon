import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: bar

    implicitHeight: 58
    color: RotorTheme.bgHeader

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: RotorTheme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: RotorTheme.padding
        anchors.rightMargin: RotorTheme.padding
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: 8
            color: Qt.rgba(0.22, 0.74, 0.97, 0.16)
            border.color: RotorTheme.primary
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "◈"
                color: RotorTheme.primary
                font.pixelSize: 18
            }
        }

        ColumnLayout {
            spacing: 0

            Text {
                text: "DecoRotor"
                color: RotorTheme.textPrimary
                font.pixelSize: RotorTheme.fontTitle
                font.bold: true
                font.letterSpacing: 0.6
            }

            Text {
                text: rotor.callsign.length > 0
                      ? qsTr("%1 · %2").arg(rotor.callsign).arg(rotor.locator)
                      : rotor.locator
                color: RotorTheme.textDim
                font.pixelSize: RotorTheme.fontSmall
            }
        }

        Item { Layout.fillWidth: true }

        StatusLed {
            label: rotor.connected ? qsTr("CONTROL BOX %1").arg(rotor.port)
                                   : qsTr("CONTROL BOX MISSING")
            colour: rotor.connected ? RotorTheme.accent : RotorTheme.danger
            blinking: !rotor.connected
        }

        StatusLed {
            label: rotor.moving ? qsTr("ROTATING") : qsTr("STOPPED")
            colour: rotor.moving ? RotorTheme.warning : RotorTheme.textDim
            blinking: rotor.moving
        }

        StatusLed {
            label: qsTr("%n client", "", rotor.clients)
            colour: rotor.clients > 0 ? RotorTheme.primary : RotorTheme.textDim
        }

        Text {
            text: rotor.modelLabel
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontSmall
        }

        Rectangle {
            id: lamp

            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: 8
            color: light.hovered ? RotorTheme.bgElevated : "transparent"
            border.color: RotorTheme.borderSoft
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: RotorTheme.dark ? "☾" : "☀"
                color: RotorTheme.textSecondary
                font.pixelSize: 16
            }

            HoverHandler {
                id: light

                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                onTapped: rotor.setSetting("dark_theme", !rotor.darkTheme)
            }

            ToolTip.visible: light.hovered
            ToolTip.text: RotorTheme.dark ? qsTr("Switch to the light dial")
                                     : qsTr("Switch to the night dial")
        }
    }
}
