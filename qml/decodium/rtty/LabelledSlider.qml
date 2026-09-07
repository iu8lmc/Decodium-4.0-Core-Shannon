// DecoRTTY — a slider that shows its value where you are already looking.
import QtQuick
import QtQuick.Controls

Column {
    id: root

    property string label: ""
    property string suffix: ""
    property real   from: 0
    property real   to: 100
    property real   stepSize: 1
    property real   value: 0
    property int    decimals: 0

    signal moved(real newValue)

    spacing: 2

    Item {
        width: parent.width
        height: 15

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            // L'etichetta cede il posto al valore, non viceversa: il numero e'
            // quello che si legge mentre si gira la manopola.
            width: Math.max(0, parent.width - readout.width - 8)
            elide: Text.ElideRight
            text: root.label
            color: Theme.textSecondary
            font.pixelSize: 11
        }
        Text {
            id: readout
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.value.toFixed(root.decimals) + root.suffix
            color: Theme.textPrimary
            font.pixelSize: 11
            font.family: Theme.monoFamily
        }
    }

    Slider {
        id: slider
        width: parent.width
        height: 18
        from: root.from
        to: root.to
        stepSize: root.stepSize
        value: root.value

        onMoved: {
            root.moved(slider.value)
            // Il legame si rompe appena il dito muove il cursore: da quel punto
            // il pallino vive per conto suo e non torna piu' a raccontare quel
            // che pensa il decodificatore. Se il motore limita il valore — o lo
            // rifiuta — il numero dice una cosa e il pallino un'altra. Si
            // riannoda subito.
            value = Qt.binding(function() { return root.value })
        }

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 3
            radius: 1.5
            color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.9)

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 1.5
                color: Theme.primary
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 12
            height: 12
            radius: 6
            color: slider.pressed ? Theme.secondary : Theme.textPrimary
            border.color: Theme.primary
            border.width: 1
        }
    }
}
