/* Decodium - Queue Panel
 * Manages waiting stations queue and active TX slots
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: queuePanel
    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
    border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
    radius: 8

    // Dynamic theme colors from ThemeManager
    property color bgDeep: bridge.themeManager.bgDeep
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    // Queue data model
    property var queueModel: ListModel {
        id: queueListModel
    }

    // Active slots model (max 5 slots)
    property var slotsModel: ListModel {
        id: activeSlotModel
    }

    // Max slots setting
    property int maxSlots: 5

    // Reference to app engine
    property var engine: null

    // Add station to queue
    function addToQueue(call, snr, grid, freq) {
        // Check if already in queue
        for (var i = 0; i < queueListModel.count; i++) {
            if (queueListModel.get(i).call === call) {
                // Update existing entry
                queueListModel.set(i, {call: call, snr: snr, grid: grid, freq: freq, time: Qt.formatTime(new Date(), "hh:mm:ss")})
                return
            }
        }
        // Add new entry
        queueListModel.append({call: call, snr: snr, grid: grid, freq: freq, time: Qt.formatTime(new Date(), "hh:mm:ss")})
    }

    // Move from queue to active slot
    function activateFromQueue(index) {
        if (activeSlotModel.count >= maxSlots) return
        if (index < 0 || index >= queueListModel.count) return

        var item = queueListModel.get(index)
        activeSlotModel.append({
            call: item.call,
            snr: item.snr,
            grid: item.grid,
            freq: item.freq,
            status: "TX1",
            progress: 0
        })
        queueListModel.remove(index)

        // Set as DX call in engine
        if (engine) {
            engine.dxCall = item.call
            engine.dxGrid = item.grid || ""
        }
    }

    // Remove from active slot
    function removeFromSlot(index) {
        if (index >= 0 && index < activeSlotModel.count) {
            activeSlotModel.remove(index)
        }
    }

    // Clear all queue
    function clearQueue() {
        queueListModel.clear()
    }

    // Clear all slots
    function clearSlots() {
        activeSlotModel.clear()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "QUEUE"
                font.pixelSize: 12
                font.bold: true
                color: secondaryCyan
            }

            Rectangle {
                width: 24
                height: 18
                radius: 9
                color: queueListModel.count > 0 ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : Qt.rgba(100/255, 100/255, 100/255, 0.3)
                border.color: queueListModel.count > 0 ? accentGreen : "#666"

                Text {
                    anchors.centerIn: parent
                    text: queueListModel.count
                    font.pixelSize: 10
                    font.bold: true
                    color: queueListModel.count > 0 ? accentGreen : "#888"
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "SLOTS"
                font.pixelSize: 12
                font.bold: true
                color: "#ff9800"
            }

            Rectangle {
                width: 40
                height: 18
                radius: 9
                color: Qt.rgba(255/255, 152/255, 0, 0.3)
                border.color: "#ff9800"

                Text {
                    anchors.centerIn: parent
                    text: activeSlotModel.count + "/" + maxSlots
                    font.pixelSize: 10
                    font.bold: true
                    color: "#ff9800"
                }
            }
        }

        // Content row with Queue and Slots
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Queue List
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                radius: 4
                border.color: glassBorder

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 2

                    // Queue header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text { text: "Call"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 70 }
                        Text { text: "SNR"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 35 }
                        Text { text: "Grid"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 45 }
                        Text { text: "Freq"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 45 }
                        Item { Layout.fillWidth: true }
                    }

                    // Queue list
                    ListView {
                        id: queueListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: queueListModel
                        clip: true
                        spacing: 1

                        delegate: Rectangle {
                            width: queueListView.width
                            height: 22
                            color: mouseArea.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                            radius: 2

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onDoubleClicked: activateFromQueue(index)
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 4

                                Text {
                                    text: model.call
                                    font.pixelSize: 10
                                    font.bold: true
                                    font.family: "Consolas"
                                    color: accentGreen
                                    Layout.preferredWidth: 70
                                }
                                Text {
                                    text: model.snr
                                    font.pixelSize: 10
                                    font.family: "Consolas"
                                    color: parseInt(model.snr) >= 0 ? accentGreen : "#f44336"
                                    Layout.preferredWidth: 35
                                }
                                Text {
                                    text: model.grid || "-"
                                    font.pixelSize: 10
                                    font.family: "Consolas"
                                    color: textSecondary
                                    Layout.preferredWidth: 45
                                }
                                Text {
                                    text: model.freq
                                    font.pixelSize: 10
                                    font.family: "Consolas"
                                    color: secondaryCyan
                                    Layout.preferredWidth: 45
                                }
                                Item { Layout.fillWidth: true }

                                // Add to slot button
                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 2
                                    color: addBtnArea.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : "transparent"
                                    border.color: accentGreen

                                    Text {
                                        anchors.centerIn: parent
                                        text: "+"
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: accentGreen
                                    }

                                    MouseArea {
                                        id: addBtnArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: activateFromQueue(index)
                                    }
                                }
                            }
                        }

                        // Empty state
                        Text {
                            anchors.centerIn: parent
                            text: "Queue empty\nDouble-click decode to add"
                            font.pixelSize: 10
                            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            horizontalAlignment: Text.AlignHCenter
                            visible: queueListModel.count === 0
                        }
                    }

                    // Clear queue button
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        text: "Clear Queue"
                        font.pixelSize: 9
                        enabled: queueListModel.count > 0
                        onClicked: clearQueue()

                        background: Rectangle {
                            color: parent.enabled ? Qt.rgba(244/255, 67/255, 54/255, 0.2) : Qt.rgba(100/255, 100/255, 100/255, 0.1)
                            border.color: parent.enabled ? "#f44336" : "#666"
                            radius: 3
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: parent.enabled ? "#f44336" : "#666"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // Active Slots
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                radius: 4
                border.color: glassBorder

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 2

                    // Slots header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text { text: "Call"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 70 }
                        Text { text: "Status"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 40 }
                        Text { text: "SNR"; font.pixelSize: 9; font.bold: true; color: textSecondary; Layout.preferredWidth: 35 }
                        Item { Layout.fillWidth: true }
                    }

                    // Slots list
                    ListView {
                        id: slotsListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: activeSlotModel
                        clip: true
                        spacing: 1

                        delegate: Rectangle {
                            width: slotsListView.width
                            height: 24
                            color: Qt.rgba(255/255, 152/255, 0, 0.15)
                            radius: 2
                            border.color: "#ff9800"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 4

                                Text {
                                    text: model.call
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.family: "Consolas"
                                    color: "#ff9800"
                                    Layout.preferredWidth: 70
                                }
                                Rectangle {
                                    width: 35
                                    height: 16
                                    radius: 2
                                    color: model.status === "TX1" ? Qt.rgba(244/255, 67/255, 54/255, 0.5) :
                                           model.status === "TX2" ? Qt.rgba(255/255, 152/255, 0, 0.5) :
                                           model.status === "TX3" ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.5) :
                                           Qt.rgba(100/255, 100/255, 100/255, 0.5)

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.status
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: textPrimary
                                    }
                                }
                                Text {
                                    text: model.snr
                                    font.pixelSize: 10
                                    font.family: "Consolas"
                                    color: parseInt(model.snr) >= 0 ? accentGreen : "#f44336"
                                    Layout.preferredWidth: 35
                                }
                                Item { Layout.fillWidth: true }

                                // Remove button
                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 2
                                    color: removeBtnArea.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : "transparent"
                                    border.color: "#f44336"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "×"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#f44336"
                                    }

                                    MouseArea {
                                        id: removeBtnArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: removeFromSlot(index)
                                    }
                                }
                            }
                        }

                        // Empty state
                        Text {
                            anchors.centerIn: parent
                            text: "No active QSOs\nAdd from queue"
                            font.pixelSize: 10
                            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                            horizontalAlignment: Text.AlignHCenter
                            visible: activeSlotModel.count === 0
                        }
                    }

                    // Slots control row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "Max:"
                            font.pixelSize: 9
                            color: textSecondary
                        }

                        SpinBox {
                            id: maxSlotsSpin
                            from: 1
                            to: 5
                            value: maxSlots
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 22
                            font.pixelSize: 9
                            onValueChanged: maxSlots = value
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            Layout.preferredHeight: 22
                            text: "Clear"
                            font.pixelSize: 9
                            enabled: activeSlotModel.count > 0
                            onClicked: clearSlots()

                            background: Rectangle {
                                color: parent.enabled ? Qt.rgba(244/255, 67/255, 54/255, 0.2) : Qt.rgba(100/255, 100/255, 100/255, 0.1)
                                border.color: parent.enabled ? "#f44336" : "#666"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: parent.enabled ? "#f44336" : "#666"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }
        }
    }
}
