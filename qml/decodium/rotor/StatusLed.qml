import QtQuick

Row {
    id: led

    property color colour: RotorTheme.textDim
    property alias label: caption.text
    property bool blinking: false

    spacing: 6

    Rectangle {
        id: lamp

        width: 10
        height: 10
        radius: 5
        anchors.verticalCenter: parent.verticalCenter
        color: led.colour

        Rectangle {
            anchors.centerIn: parent
            width: 18
            height: 18
            radius: 9
            color: "transparent"
            border.color: led.colour
            border.width: 1
            opacity: 0.35
        }

        SequentialAnimation on opacity {
            running: led.blinking && led.visible
            loops: Animation.Infinite
            alwaysRunToEnd: true

            OpacityAnimator { from: 1.0; to: 0.25; duration: 500 }
            OpacityAnimator { from: 0.25; to: 1.0; duration: 500 }
        }
    }

    Text {
        id: caption

        anchors.verticalCenter: parent.verticalCenter
        color: RotorTheme.textSecondary
        font.pixelSize: RotorTheme.fontSmall
        font.letterSpacing: 0.6
    }
}
