/* Decodium 2.0 - Styled ComboBox Component
 * Glassmorphism dropdown with consistent styling
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    property color bgDeep: bridge.themeManager.bgDeep
    property color textPrimary: bridge.themeManager.textPrimary
    property color accentColor: bridge.themeManager.secondaryColor
    property color textColor: textPrimary
    property color bgColor: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
    property color borderColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
    property int itemHeight: 36

    height: 40
    font.pixelSize: 13

    background: Rectangle {
        color: control.bgColor
        border.color: control.pressed || control.popup.visible ? control.accentColor : control.borderColor
        border.width: control.pressed || control.popup.visible ? 2 : 1
        radius: 6
    }

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.accentColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        x: control.width - width - 8
        y: control.height / 2 - height / 2
        width: 10
        height: 6
        contextType: "2d"

        Connections {
            target: control
            function onPressedChanged() { indicator.requestPaint() }
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width / 2, height)
            ctx.closePath()
            ctx.fillStyle = control.accentColor
            ctx.fill()
        }
    }

    delegate: ItemDelegate {
        width: control.width
        height: control.itemHeight

        contentItem: Text {
            text: modelData !== undefined ? modelData : (model.text !== undefined ? model.text : "")
            color: highlighted ? "#ffffff" : control.textColor
            font.pixelSize: 12
            font.bold: highlighted
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: highlighted ? Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.4) : "transparent"
            radius: 4
        }

        highlighted: control.highlightedIndex === index
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 300)
        padding: 6

        background: Rectangle {
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
            border.color: control.accentColor
            border.width: 1
            radius: 6

            // Shadow effect using nested rectangle
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                z: -1
                radius: 10
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
            }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 2

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                }

                background: Rectangle {
                    implicitWidth: 4
                    color: "transparent"
                }
            }
        }
    }
}
