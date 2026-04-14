/* Decodium Qt6 - Glass Button Component */
import QtQuick
import QtQuick.Controls

Button {
    id: glassButton

    property color textPrimary: bridge.themeManager.textPrimary
    property color glassColor: bridge.themeManager.glassOverlay
    property color borderColor: bridge.themeManager.glassBorder
    property color hoverColor: Qt.rgba(bridge.themeManager.secondaryColor.r, bridge.themeManager.secondaryColor.g, bridge.themeManager.secondaryColor.b, 0.3)
    property color textColor: textPrimary

    contentItem: Text {
        text: glassButton.text
        font.pixelSize: 13
        color: textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        color: glassButton.hovered ? hoverColor : glassColor
        border.color: borderColor
        radius: 8

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }
}
