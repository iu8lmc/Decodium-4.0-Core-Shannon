import QtQuick

/*!
    Lettura d'angolo a caratteri grandi: dall'altro capo dello shack si deve
    capire dov'e' puntata l'antenna senza avvicinarsi al monitor.
*/
Item {
    id: readout

    property string label: ""
    property real value: 0
    property real target: -1
    property bool valid: false
    property bool moving: false
    property int digitSize: 78

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    Column {
        id: column

        anchors.right: parent.right
        spacing: 0

        Text {
            anchors.right: parent.right
            text: readout.label
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontSmall
            font.bold: true
            font.letterSpacing: 2.0
        }

        Row {
            anchors.right: parent.right
            spacing: 0

            Text {
                text: readout.valid ? Math.round(readout.value) : "---"
                color: readout.moving ? RotorTheme.warning : RotorTheme.textPrimary
                font.pixelSize: readout.digitSize
                font.family: RotorTheme.monoFamily
                font.bold: true
            }

            Text {
                anchors.top: parent.top
                anchors.topMargin: readout.digitSize * 0.06
                text: "°"
                color: readout.moving ? RotorTheme.warning : RotorTheme.textPrimary
                font.pixelSize: readout.digitSize * 0.62
                font.family: RotorTheme.monoFamily
                font.bold: true
            }
        }

        Text {
            anchors.right: parent.right
            text: readout.target >= 0
                  ? qsTr("towards %1°").arg(readout.target.toFixed(1))
                  : (readout.valid ? qsTr("stable position") : qsTr("no reading"))
            color: readout.target >= 0 ? RotorTheme.primary : RotorTheme.textDim
            font.pixelSize: RotorTheme.fontBody
            font.family: RotorTheme.monoFamily
        }
    }
}
