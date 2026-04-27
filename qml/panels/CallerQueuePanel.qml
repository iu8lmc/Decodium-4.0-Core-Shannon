import QtQuick
import QtQuick.Layouts

// CallerQueuePanel — Fox mode caller queue, adattato per bridge.*
Rectangle {
    id: callerQueuePanel
    color: Qt.rgba(0, 0, 0, 0.75)
    border.color: Qt.rgba(255, 152, 0, 0.5)
    border.width: 1
    radius: 6
    visible: bridge.foxMode
    implicitWidth: 240
    implicitHeight: Math.min(contentHeight + 50, 230)

    signal closeRequested()

    property int contentHeight: queueList.contentHeight

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Header
        RowLayout {
            width: parent.width
            spacing: 6

            Text {
                text: "FOX CALLER QUEUE"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
                color: "#FF9800"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: bridge.callerQueueSize + "/20"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#B0BEC5"
            }

            // Close panel button
            Rectangle {
                width: 18; height: 18; radius: 2
                color: mouseAreaClear.containsMouse ? Qt.rgba(1,1,1,0.1) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "✕"; font.pixelSize: 10; color: "#78909C"
                }

                MouseArea {
                    id: mouseAreaClear
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: callerQueuePanel.closeRequested()
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(255, 152, 0, 0.3) }

        // Queue list
        ListView {
            id: queueList
            width: parent.width
            height: Math.min(contentHeight, 170)
            clip: true
            model: bridge.callerQueue

            delegate: Rectangle {
                width: queueList.width
                height: 22
                color: delegateMouse.containsMouse ? Qt.rgba(1, 0.6, 0, 0.1) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    spacing: 6

                    Text {
                        text: (index + 1) + "."
                        font.family: "Monospace"; font.pixelSize: 10
                        color: "#546E7A"; Layout.preferredWidth: 18
                    }

                    Text {
                        text: modelData
                        font.family: "Monospace"; font.pixelSize: 11; font.bold: true
                        color: index === 0 ? "#FF9800" : "#ECEFF1"
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "✕"
                        font.pixelSize: 10; color: "#546E7A"
                        visible: delegateMouse.containsMouse

                        MouseArea {
                            anchors.fill: parent
                            onClicked: bridge.dequeueStation(modelData)
                        }
                    }
                }

                MouseArea {
                    id: delegateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            Text {
                anchors.centerIn: parent
                visible: queueList.count === 0
                text: "No callers in queue"
                font.family: "Monospace"; font.pixelSize: 10
                color: "#546E7A"
            }
        }
    }
}
