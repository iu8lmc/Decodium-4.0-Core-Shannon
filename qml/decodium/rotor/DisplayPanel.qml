import QtQuick
import QtQuick.Layouts

/*!
    Il display del control box: angolo a caratteri grandi, verso di rotazione,
    memoria corrispondente alla direzione attuale e stato in chiaro.
*/
GlassPanel {
    id: panel

    // Se l'antenna e' ferma sopra una memoria, quella memoria ha un nome: e'
    // piu' parlante di un numero, esattamente come sul frontalino.
    readonly property string standingOn: {
        if (!rotor.hasPosition)
            return "";
        const memories = rotor.presets;
        for (let i = 0; i < memories.length; ++i) {
            const gap = Math.abs(((memories[i].az - rotor.azimuth + 540) % 360) - 180);
            if (gap <= 3.0)
                return memories[i].name;
        }
        return "";
    }

    readonly property string activity: {
        if (!rotor.connected)
            return qsTr("control box missing");
        if (rotor.moving && rotor.azimuthTarget >= 0)
            return qsTr("rotating towards %1°").arg(rotor.azimuthTarget.toFixed(1));
        if (rotor.moving)
            return qsTr("rotating");
        return qsTr("stopped");
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: RotorTheme.spacing

            ColumnLayout {
                Layout.alignment: Qt.AlignBottom
                spacing: 6

                Rectangle {
                    Layout.preferredWidth: 168
                    Layout.preferredHeight: 40
                    radius: 8
                    color: RotorTheme.bgElevated
                    border.color: panel.standingOn.length > 0 ? RotorTheme.accent : RotorTheme.borderSoft
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        text: panel.standingOn.length > 0 ? panel.standingOn.toUpperCase()
                                                          : qsTr("FREE DIRECTION")
                        color: panel.standingOn.length > 0 ? RotorTheme.textPrimary : RotorTheme.textDim
                        font.pixelSize: RotorTheme.fontBody
                        font.bold: true
                        font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                Text {
                    Layout.preferredWidth: 168
                    text: panel.activity
                    color: !rotor.connected ? RotorTheme.danger
                         : rotor.moving ? RotorTheme.warning
                         : RotorTheme.textDim
                    font.pixelSize: RotorTheme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillWidth: true }

            BigReadout {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                label: qsTr("AZIMUTH")
                value: rotor.azimuth
                target: rotor.azimuthTarget
                valid: rotor.hasPosition
                moving: rotor.moving
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: RotorTheme.spacing

            RotationBar {
                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.alignment: Qt.AlignBottom
                sense: rotor.rotationSense
                moving: rotor.moving
            }

            BigReadout {
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                visible: rotor.hasElevation
                label: qsTr("ELEVATION")
                value: rotor.elevation
                target: rotor.elevationTarget
                valid: rotor.hasElevation
                moving: rotor.moving
                digitSize: 34
            }
        }
    }
}
