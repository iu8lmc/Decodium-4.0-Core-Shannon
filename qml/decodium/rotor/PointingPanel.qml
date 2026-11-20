import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FieldInput {
                id: azField

                Layout.fillWidth: true
                placeholderText: qsTr("Azimuth °")
                validator: DoubleValidator { bottom: 0; top: 360; decimals: 1 }
                onAccepted: panel.send()
            }

            FieldInput {
                id: elField

                Layout.fillWidth: true
                visible: rotor.hasElevation
                placeholderText: qsTr("Elevation °")
                validator: DoubleValidator { bottom: 0; top: 180; decimals: 1 }
                onAccepted: panel.send()
            }

            CommandButton {
                Layout.preferredWidth: 92
                text: qsTr("AIM")
                kind: CommandButton.Kind.Primary
                onClicked: panel.send()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 8
            columnSpacing: 6

            Repeater {
                model: [
                    { name: qsTr("N"), deg: 0 }, { name: qsTr("NE"), deg: 45 },
                    { name: qsTr("E"), deg: 90 }, { name: qsTr("SE"), deg: 135 },
                    { name: qsTr("S"), deg: 180 }, { name: qsTr("SW"), deg: 225 },
                    { name: qsTr("W"), deg: 270 }, { name: qsTr("NW"), deg: 315 }
                ]

                CommandButton {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    text: modelData.name
                    onClicked: rotor.gotoAzimuth(modelData.deg)
                }
            }
        }
    }

    function send() {
        const az = parseFloat(azField.text.replace(",", "."));
        const el = parseFloat(elField.text.replace(",", "."));
        rotor.gotoPosition(isNaN(az) ? -1 : az, isNaN(el) ? -1 : el);
    }
}
