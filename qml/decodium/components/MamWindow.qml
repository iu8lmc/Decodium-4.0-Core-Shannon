/* Decodium - Multi-Answer Mode Window
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: mamWindow
    title: "Multi-Answer Mode"
    width: 700
    height: 450
    modal: false
    standardButtons: Dialog.Close
    property bool positionInitialized: false

    function clampToParent() {
        if (!parent) return
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        x = Math.max(0, Math.round((parent.width - width) / 2))
        y = Math.max(0, Math.round((parent.height - height) / 2))
        positionInitialized = true
    }

    onOpened: ensureInitialPosition()

    // Colors
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgPanel: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
    property color accentGreen: bridge.themeManager.accentColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color warningOrange: bridge.themeManager.warningColor
    property color errorRed: bridge.themeManager.errorColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    background: Rectangle {
        color: bgPanel
        border.color: glassBorder
        border.width: 1
        radius: 8
    }

    header: Rectangle {
        height: 50
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
        radius: 8

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                mamWindow.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                mamWindow.x += mouse.x - clickPos.x
                mamWindow.y += mouse.y - clickPos.y
                mamWindow.clampToParent()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "Multi-Answer Mode - Queue: " + (appEngine ? appEngine.mamQueueCount : 0) + " | Now: " + (appEngine ? appEngine.mamNowCount : 0)
            font.pixelSize: 16
            font.bold: true
            color: warningOrange
        }
    }

    contentItem: Rectangle {
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            // Queue
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                radius: 6
                border.color: secondaryCyan

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    Text {
                        text: "QUEUE"
                        font.pixelSize: 14
                        font.bold: true
                        color: secondaryCyan
                    }

                    Text {
                        text: appEngine ? "Count: " + appEngine.mamQueueCount : "N/A"
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    Repeater {
                        model: appEngine ? appEngine.mamQueueData : []
                        delegate: Text {
                            text: modelData.call + " " + modelData.distance
                            font.pixelSize: 11
                            color: accentGreen
                        }
                    }
                }
            }

            // Now
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                radius: 6
                border.color: warningOrange

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    Text {
                        text: "NOW"
                        font.pixelSize: 14
                        font.bold: true
                        color: warningOrange
                    }

                    Text {
                        text: appEngine ? "Count: " + appEngine.mamNowCount : "N/A"
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    Repeater {
                        model: appEngine ? appEngine.mamNowData : []
                        delegate: Text {
                            text: modelData.call + " TX:" + modelData.txDb + " RX:" + modelData.rxDb
                            font.pixelSize: 11
                            color: warningOrange
                        }
                    }
                }
            }
        }
    }
}
