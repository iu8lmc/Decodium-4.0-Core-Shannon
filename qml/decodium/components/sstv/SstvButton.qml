// Plain scene-graph primitives avoid style-specific cached/effect backgrounds
// which can appear as stale tiles until hover repaints them (issue #86).
import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control
    // The SSTV components also load without the application bridge (tests).
    property real scaleFactor: (typeof bridge !== "undefined" && bridge && bridge.settings
                                && bridge.settings.uiScale) ? bridge.settings.uiScale : 1
    implicitWidth: Math.max(80 * scaleFactor, (implicitContentWidth + leftPadding + rightPadding) * scaleFactor)
    implicitHeight: Math.max(40 * scaleFactor, (implicitContentHeight + topPadding + bottomPadding) * scaleFactor)
    padding: 10 * scaleFactor
    horizontalPadding: 16 * scaleFactor
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
        radius: 6 * scaleFactor
        color: control.down || control.checked ? control.palette.highlight
             : control.hovered ? Qt.lighter(control.palette.button, 1.18)
             : control.palette.button
        opacity: control.enabled ? 1.0 : 0.45
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? control.palette.highlight : control.palette.mid
    }
}
