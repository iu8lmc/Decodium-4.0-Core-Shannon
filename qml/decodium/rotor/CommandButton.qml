import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    enum Kind { Normal, Primary, Danger }

    property int kind: CommandButton.Kind.Normal

    readonly property color tint: kind === CommandButton.Kind.Primary ? RotorTheme.primary
                                : kind === CommandButton.Kind.Danger ? RotorTheme.danger
                                : RotorTheme.textSecondary

    implicitHeight: 38
    font.pixelSize: RotorTheme.fontBody
    font.bold: kind !== CommandButton.Kind.Normal

    background: Rectangle {
        radius: 8
        color: control.kind === CommandButton.Kind.Normal
               ? (control.down ? RotorTheme.bgHeader : RotorTheme.bgElevated)
               : Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.down ? 0.42 : 0.20)
        border.color: control.hovered || control.down ? control.tint : RotorTheme.borderSoft
        border.width: 1
        opacity: control.enabled ? 1 : 0.4
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.kind === CommandButton.Kind.Normal ? RotorTheme.textPrimary : control.tint
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.5
    }
}
