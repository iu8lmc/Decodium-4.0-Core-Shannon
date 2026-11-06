import QtQuick

Rectangle {
    id: panel

    property alias title: heading.text
    default property alias content: body.data

    color: RotorTheme.bgPanel
    border.color: RotorTheme.border
    border.width: 1
    radius: RotorTheme.radius

    Rectangle {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: heading.text.length > 0 ? 34 : 0
        visible: height > 0
        color: RotorTheme.bgHeader
        radius: RotorTheme.radius

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: RotorTheme.radius
            color: RotorTheme.bgHeader
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: RotorTheme.borderSoft
        }

        Text {
            id: heading

            anchors.left: parent.left
            anchors.leftMargin: RotorTheme.padding
            anchors.verticalCenter: parent.verticalCenter
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontSmall
            font.letterSpacing: 1.4
            font.bold: true
            text: ""
        }
    }

    Item {
        id: body

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: RotorTheme.padding
    }
}
