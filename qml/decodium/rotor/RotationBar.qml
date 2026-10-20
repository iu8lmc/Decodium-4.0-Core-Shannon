import QtQuick
import QtQuick.Layouts

/*!
    Indicatore di rotazione: i segmenti scorrono nel verso in cui il rotore sta
    davvero girando, le frecce dicono CCW o CW e il cerchietto si accende
    quando l'antenna e' ferma.
*/
Item {
    id: bar

    property int sense: 0             // -1 antiorario, 0 fermo, +1 orario
    property bool moving: false

    readonly property int segments: 7

    implicitHeight: 22
    implicitWidth: row.implicitWidth

    QtObject {
        id: internal

        property int step: 0
    }

    Timer {
        interval: 110
        repeat: true
        running: bar.moving && bar.visible
        onTriggered: internal.step = (internal.step + 1) % bar.segments
    }

    RowLayout {
        id: row

        anchors.fill: parent
        spacing: 12

        Row {
            spacing: 3

            Repeater {
                model: bar.segments

                Rectangle {
                    required property int index

                    readonly property int slot: bar.sense < 0
                                                ? (index + internal.step) % bar.segments
                                                : (bar.segments + index - internal.step) % bar.segments

                    width: 5
                    height: 18
                    radius: 1
                    color: bar.moving && slot < 3 ? RotorTheme.warning : RotorTheme.textDim
                    opacity: bar.moving ? (slot < 3 ? 1.0 : 0.30) : 0.30
                }
            }
        }

        Text {
            text: "◀ CCW"
            color: bar.moving && bar.sense < 0 ? RotorTheme.warning : RotorTheme.textDim
            font.pixelSize: RotorTheme.fontSmall
            font.bold: true
            font.letterSpacing: 1.0
        }

        Text {
            text: "CW ▶"
            color: bar.moving && bar.sense > 0 ? RotorTheme.warning : RotorTheme.textDim
            font.pixelSize: RotorTheme.fontSmall
            font.bold: true
            font.letterSpacing: 1.0
        }

        Text {
            text: "(●)"
            color: bar.moving ? RotorTheme.textDim : RotorTheme.accent
            font.pixelSize: RotorTheme.fontSmall
            font.bold: true
        }

        Item { Layout.fillWidth: true }
    }
}
