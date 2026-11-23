// Small "LoTW" tag shown in the decode lists next to a station that is in the
// LoTW user activity list (entry.isLotw). It replaces the 6 px dot used before,
// which was easy to miss. The colour follows the "LoTW marker" decode colour.
import QtQuick

Rectangle {
    id: badge

    property real sizeScale: 1.0
    property color accent: "#FFFFFF"
    property string label: "LoTW"

    implicitHeight: Math.max(10, Math.round(12 * sizeScale))
    implicitWidth: labelText.implicitWidth + Math.max(4, Math.round(6 * sizeScale))
    width: implicitWidth
    height: implicitHeight
    radius: Math.max(2, Math.round(3 * sizeScale))
    color: Qt.rgba(accent.r, accent.g, accent.b, 0.18)
    border.color: accent
    border.width: 1
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("LoTW user")

    Text {
        id: labelText
        anchors.centerIn: parent
        text: badge.label
        color: badge.accent
        font.pixelSize: Math.max(7, Math.round(8 * badge.sizeScale))
        font.bold: true
    }
}
