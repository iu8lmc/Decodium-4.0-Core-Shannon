import QtQuick
import QtQuick.Layouts

/*!
    Le prime sei memorie a portata di pollice, come i tasti diretti di un
    control box: nome sopra, angolo sotto, un tocco e il rotore parte.
    Le celle libere aprono l'elenco completo delle memorie.
*/
GridLayout {
    id: grid

    property var entries: rotor.presets

    signal manageRequested()

    columns: 3
    rowSpacing: 8
    columnSpacing: 8

    Repeater {
        model: 6

        Rectangle {
            id: cell

            required property int index

            readonly property var entry: grid.entries.length > index ? grid.entries[index] : null
            readonly property bool filled: entry !== null

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 48
            radius: 8
            border.color: filled ? RotorTheme.primary : RotorTheme.borderSoft
            border.width: 1

            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: !cell.filled ? RotorTheme.bgElevated
                         : RotorTheme.dark ? "#1D4E77" : "#7FB2E0"
                }

                GradientStop {
                    position: 1.0
                    color: !cell.filled ? RotorTheme.bgElevated
                         : RotorTheme.dark ? "#0B2A45" : "#2C6FAF"
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 0
                visible: cell.filled

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: cell.filled ? cell.entry.name : ""
                    color: "#F2F7FC"
                    font.pixelSize: RotorTheme.fontBody
                    font.bold: true
                    elide: Text.ElideRight
                    width: cell.width - 12
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: cell.filled ? Math.round(cell.entry.az) + "°" : ""
                    color: "#CFE4F7"
                    font.pixelSize: 18
                    font.family: RotorTheme.monoFamily
                    font.bold: true
                }
            }

            Text {
                anchors.centerIn: parent
                visible: !cell.filled
                text: "+"
                color: RotorTheme.textDim
                font.pixelSize: 22
                font.bold: true
            }

            HoverHandler {
                id: hover

                cursorShape: Qt.PointingHandCursor
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "#FFFFFF"
                opacity: hover.hovered ? 0.10 : 0
                visible: opacity > 0
            }

            TapHandler {
                onTapped: {
                    if (cell.filled)
                        rotor.recallPreset(cell.entry.name);
                    else
                        grid.manageRequested();
                }
            }
        }
    }
}
