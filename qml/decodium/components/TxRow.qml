/* TX Row Component - Single TX message row for Decodium-style TX panel
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: txRow
    height: 24
    implicitHeight: 24
    Layout.preferredHeight: 24

    // Required properties
    property int txIndex: 0
    property bool isSelected: false
    property bool isTransmitting: false
    property string message: ""
    property bool isEditable: txIndex === 6  // Only TX7 is editable

    // Signals
    signal selectClicked()
    signal txClicked()
    signal messageEdited(string newMessage)

    // Colors
    property color txSelectedBg: "#550000"
    property color txActiveBg: "#AA0000"
    property color textColor: bridge.themeManager.textPrimary
    property color borderColor: bridge.themeManager.glassBorder
    property color bgDeep: bridge.themeManager.bgDeep

    // Background for selection/transmission
    Rectangle {
        anchors.fill: parent
        color: isTransmitting ? txActiveBg : (isSelected ? txSelectedBg : "transparent")
        radius: 3
        border.width: isSelected ? 1 : 0
        border.color: isSelected ? "#880000" : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 4

        // TX message field
        TextField {
            id: messageField
            Layout.fillWidth: true
            Layout.preferredHeight: 20

            text: message
            readOnly: !isEditable
            font.family: "Monospace"
            font.pixelSize: 11
            color: textColor
            selectByMouse: true

            background: Rectangle {
                color: isSelected ? Qt.rgba(85/255, 0, 0, 0.5) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                border.color: isSelected ? "#880000" : borderColor
                border.width: 1
                radius: 3
            }

            onTextChanged: {
                if (isEditable && text !== message) {
                    messageEdited(text)
                }
            }
        }

        // Radio button for selection
        RadioButton {
            id: selectRadio
            checked: isSelected

            indicator: Rectangle {
                implicitWidth: 16
                implicitHeight: 16
                radius: 8
                x: selectRadio.leftPadding
                y: parent.height / 2 - height / 2
                color: "transparent"
                border.color: isSelected ? "#ff0000" : "#888888"
                border.width: 2

                Rectangle {
                    width: 8
                    height: 8
                    x: 4
                    y: 4
                    radius: 4
                    color: isSelected ? "#ff0000" : "transparent"
                }
            }

            onClicked: selectClicked()
        }

        // TX button
        Button {
            id: txButton
            Layout.preferredWidth: 40
            Layout.preferredHeight: 20

            background: Rectangle {
                color: isTransmitting ? "#ff0000" : (isSelected ? "#880000" : "#444444")
                border.color: isTransmitting ? "#ff4444" : (isSelected ? "#aa0000" : "#666666")
                border.width: 1
                radius: 4
            }

            contentItem: Text {
                text: "Tx" + (txIndex + 1)
                color: textColor
                font.bold: isSelected || isTransmitting
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: txClicked()
        }
    }
}
