import QtQuick
import QtQuick.Controls

Popup {
    id: comboPopup

    property var combo: null
    property int minPopupWidth: 220
    property int maxPopupHeight: 360
    readonly property var comboOrigin: combo && parent ? combo.mapToItem(parent, 0, 0) : Qt.point(0, 0)
    readonly property real wantedHeight: Math.min(maxPopupHeight, comboPopupList.contentHeight + padding * 2)
    readonly property real spaceBelow: parent && combo ? parent.height - comboOrigin.y - combo.height - 8 : maxPopupHeight
    readonly property real spaceAbove: parent && combo ? comboOrigin.y - 8 : 0
    readonly property bool openAbove: wantedHeight > spaceBelow && spaceAbove > spaceBelow

    parent: Overlay.overlay
    modal: false
    focus: true
    padding: 6
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.min(Math.max(combo ? combo.width : 0, minPopupWidth), Math.max(80, parent.width - 16))
                  : Math.max(combo ? combo.width : 0, minPopupWidth)
    height: Math.max(44, Math.min(wantedHeight, Math.max(44, openAbove ? spaceAbove : spaceBelow)))
    x: parent ? Math.max(8, Math.min(comboOrigin.x, parent.width - width - 8)) : 0
    y: parent
       ? (openAbove
          ? Math.max(8, comboOrigin.y - height - 2)
          : Math.min(comboOrigin.y + (combo ? combo.height : 0) + 2, parent.height - height - 8))
       : 0

    onOpened: comboPopupList.forceActiveFocus()

    background: Rectangle {
        color: "#080b12"
        border.color: "#334455"
        radius: 4
    }

    contentItem: ListView {
        id: comboPopupList
        anchors.fill: parent
        clip: true
        model: comboPopup.visible && combo ? combo.delegateModel : null
        currentIndex: combo ? combo.highlightedIndex : -1
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: true
        focus: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        function clampContentY(value) {
            return Math.max(0, Math.min(Math.max(0, contentHeight - height), value))
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: function(wheel) {
                var pixelDelta = wheel.pixelDelta ? wheel.pixelDelta.y : 0
                var angleDelta = wheel.angleDelta ? wheel.angleDelta.y : 0
                var step = pixelDelta !== 0 ? pixelDelta : angleDelta / 120 * 48
                if (step === 0)
                    return
                comboPopupList.contentY = comboPopupList.clampContentY(comboPopupList.contentY - step)
                wheel.accepted = true
            }
        }
    }
}
