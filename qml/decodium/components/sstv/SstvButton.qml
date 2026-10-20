// Plain scene-graph primitives avoid style-specific cached/effect backgrounds
// which can appear as stale tiles until hover repaints them (issue #86).
import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control
    implicitWidth: Math.max(80, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(40, implicitContentHeight + topPadding + bottomPadding)
    padding: 10
    horizontalPadding: 16
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.palette.buttonText
        opacity: control.enabled ? 1.0 : 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        objectName: "sstvPlainButtonBackground"
        radius: 6
        color: control.down || control.checked ? control.palette.highlight
             : control.hovered ? Qt.lighter(control.palette.button, 1.18)
             : control.palette.button
        opacity: control.enabled ? 1.0 : 0.45
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? control.palette.highlight : control.palette.mid
    }
}
