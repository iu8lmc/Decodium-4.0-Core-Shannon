// DecoRTTY — the surface every panel is built on.
import QtQuick

Rectangle {
    id: root

    property color tint: Theme.bgDeep
    property real  tintOpacity: 0.72

    color: Qt.rgba(tint.r, tint.g, tint.b, tintOpacity)
    border.color: Theme.glassBorder
    border.width: 1
    radius: 14
}
