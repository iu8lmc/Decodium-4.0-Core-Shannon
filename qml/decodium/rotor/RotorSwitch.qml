import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: control

    property string textOn: qsTr("on")
    property string textOff: qsTr("off")

    indicator: Rectangle {
        implicitWidth: 44
        implicitHeight: 22
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        radius: 11
        color: control.checked ? Qt.rgba(0.20, 0.83, 0.60, 0.30) : RotorTheme.bgDeep
        border.color: control.checked ? RotorTheme.accent : RotorTheme.borderSoft
        border.width: 1

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 16
            height: 16
            radius: 8
            color: control.checked ? RotorTheme.accent : RotorTheme.textDim

            Behavior on x {
                XAnimator { duration: 120 }
            }
        }
    }

    contentItem: Text {
        text: control.checked ? control.textOn : control.textOff
        color: RotorTheme.textSecondary
        font.pixelSize: RotorTheme.fontSmall
        leftPadding: control.indicator.width + 10
        verticalAlignment: Text.AlignVCenter
    }
}
