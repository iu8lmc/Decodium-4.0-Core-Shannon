/* Decodium Qt6 - Glass Panel Component */
import QtQuick

Rectangle {
    id: glassPanel

    property color bgDeep: bridge.themeManager.bgDeep
    property color glassColor: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
    property color textPrimary: bridge.themeManager.textPrimary
    property color borderColor: bridge.themeManager.glassBorder
    property int borderRadius: 14

    color: glassColor
    border.color: borderColor
    border.width: 1
    radius: borderRadius
}
