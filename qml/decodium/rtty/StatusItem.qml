// DecoRTTY — one label/value pair on the status rail.
import QtQuick

Row {
    property string label: ""
    property string value: ""
    property color  valueColor: Theme.textPrimary

    spacing: 5

    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: label
        color: Theme.textSecondary
        font.pixelSize: 9
        font.bold: true
        font.letterSpacing: 0.8
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: value
        color: valueColor
        font.pixelSize: 11
        font.family: Theme.monoFamily
    }
}
