import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: field

    implicitHeight: 38
    color: RotorTheme.textPrimary
    font.pixelSize: RotorTheme.fontBody
    font.family: RotorTheme.monoFamily
    placeholderTextColor: RotorTheme.textDim
    selectionColor: Qt.rgba(0.22, 0.74, 0.97, 0.35)
    selectedTextColor: RotorTheme.textPrimary
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: 8
        color: RotorTheme.bgDeep
        border.color: field.activeFocus ? RotorTheme.primary : RotorTheme.borderSoft
        border.width: 1
    }
}
