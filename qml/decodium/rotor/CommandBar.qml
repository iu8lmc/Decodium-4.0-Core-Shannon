import QtQuick
import QtQuick.Layouts

/*!
    I comandi che servono con l'antenna in movimento: passi a destra e a
    sinistra, STOP al centro sotto il pollice, riposo e memorie.
*/
GlassPanel {
    id: bar

    signal presetsRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [-10, -1]

                CommandButton {
                    required property int modelData

                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 48
                    text: (modelData === -10 ? "◀◀ " : "◀ ") + modelData + "°"
                    enabled: rotor.hasPosition
                    onClicked: rotor.jog(modelData)
                }
            }

            CommandButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("STOP")
                kind: CommandButton.Kind.Danger
                font.pixelSize: 20
                onClicked: rotor.stop(false)
            }

            Repeater {
                model: [1, 10]

                CommandButton {
                    required property int modelData

                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 48
                    text: "+" + modelData + (modelData === 10 ? " ▶▶" : " ▶")
                    enabled: rotor.hasPosition
                    onClicked: rotor.jog(modelData)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            CommandButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                text: qsTr("PARK %1°").arg(rotor.parkAz.toFixed(0))
                onClicked: rotor.park()
            }

            CommandButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                text: qsTr("MEMORIES…")
                onClicked: bar.presetsRequested()
            }
        }
    }
}
