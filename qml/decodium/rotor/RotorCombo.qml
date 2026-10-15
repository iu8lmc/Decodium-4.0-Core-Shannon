import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control

    implicitHeight: 32
    font.pixelSize: RotorTheme.fontBody

    contentItem: Text {
        leftPadding: 10
        rightPadding: control.indicator.width + 6
        text: control.displayText
        color: RotorTheme.textPrimary
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 8
        color: RotorTheme.bgDeep
        border.color: control.activeFocus ? RotorTheme.primary : RotorTheme.borderSoft
        border.width: 1
    }

    delegate: ItemDelegate {
        required property var modelData
        required property int index

        width: control.width
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole ? (modelData[control.textRole] !== undefined ? modelData[control.textRole] : modelData) : modelData
            color: RotorTheme.textPrimary
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: highlighted ? Qt.rgba(0.22, 0.74, 0.97, 0.18) : RotorTheme.bgDeep
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 260)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }

        background: Rectangle {
            color: RotorTheme.bgDeep
            border.color: RotorTheme.borderSoft
            radius: 8
        }
    }
}
