import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// ActiveStationsPanel — usa bridge.activeStations (QObject* → ActiveStationsModel)
Rectangle {
    id: activePanel
    color: Qt.rgba(0, 0, 0, 0.75)
    border.color: Qt.rgba(0, 188, 212, 0.4)
    border.width: 1
    radius: 6
    implicitHeight: 280
    implicitWidth: 340

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Header
        RowLayout {
            width: parent.width
            spacing: 6

            Text {
                text: "ACTIVE STATIONS"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
                color: "#00BCD4"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: (bridge.activeStations ? bridge.activeStations.count : 0) + " stn"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#B0BEC5"
            }

            Rectangle {
                width: 22
                height: 22
                radius: 4
                color: closeMA.containsMouse ? Qt.rgba(1, 1, 1, 0.10) : "transparent"
                border.color: Qt.rgba(0, 188, 212, 0.3)

                Text {
                    anchors.centerIn: parent
                    text: "X"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    color: "#00BCD4"
                }

                MouseArea {
                    id: closeMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var root = activePanel.parent
                        while (root) {
                            if (root.hasOwnProperty("visible")) {
                                root.visible = false
                            }
                            if (root.hasOwnProperty("activeStationsPanelVisible")) {
                                root.activeStationsPanelVisible = false
                                break
                            }
                            root = root.parent
                        }
                    }
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(0, 188, 212, 0.3) }

        // Filter buttons
        RowLayout {
            width: parent.width
            spacing: 4

            AbstractButton {
                implicitWidth: 58; implicitHeight: 18
                property bool active: bridge.activeStations ? bridge.activeStations.filterCqOnly : false
                onClicked: { if (bridge.activeStations) bridge.activeStations.filterCqOnly = !bridge.activeStations.filterCqOnly }
                background: Rectangle {
                    color: parent.active ? Qt.rgba(0, 0.74, 0.84, 0.2) : "transparent"
                    border.color: parent.active ? "#00BCD4" : "#546E7A"
                    border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: "CQ Only"; font.family: "Monospace"; font.pixelSize: 9
                    color: parent.active ? "#00BCD4" : "#78909C"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            AbstractButton {
                implicitWidth: 58; implicitHeight: 18
                property bool active: bridge.activeStations ? bridge.activeStations.filterWantedOnly : false
                onClicked: { if (bridge.activeStations) bridge.activeStations.filterWantedOnly = !bridge.activeStations.filterWantedOnly }
                background: Rectangle {
                    color: parent.active ? Qt.rgba(0, 0.74, 0.84, 0.2) : "transparent"
                    border.color: parent.active ? "#00BCD4" : "#546E7A"
                    border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: "Wanted"; font.family: "Monospace"; font.pixelSize: 9
                    color: parent.active ? "#00BCD4" : "#78909C"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillWidth: true }

            AbstractButton {
                implicitWidth: 36; implicitHeight: 18
                onClicked: { if (bridge.activeStations) bridge.activeStations.clear() }
                contentItem: Text {
                    text: "Clear"; font.family: "Monospace"; font.pixelSize: 9
                    color: "#f44336"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent" }
            }
        }

        // Column headers
        RowLayout {
            width: parent.width
            spacing: 4
            Text { text: "Call";  font.family: "Monospace"; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 78 }
            Text { text: "Freq";  font.family: "Monospace"; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 48 }
            Text { text: "SNR";   font.family: "Monospace"; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 33 }
            Text { text: "Grid";  font.family: "Monospace"; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 48 }
            Text { text: "UTC";   font.family: "Monospace"; font.pixelSize: 9; color: "#546E7A"; Layout.fillWidth: true }
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(84/255, 110/255, 122/255, 0.5) }

        // Station list
        ListView {
            id: stationList
            width: parent.width
            height: parent.height - 94
            clip: true
            model: bridge.activeStations

            delegate: Rectangle {
                width: stationList.width
                readonly property bool passesCqFilter: !bridge.activeStations || !bridge.activeStations.filterCqOnly || model.isCq
                readonly property bool passesWantedFilter: !bridge.activeStations || !bridge.activeStations.filterWantedOnly || model.isWanted
                visible: passesCqFilter && passesWantedFilter
                height: visible ? 20 : 0
                color: stnMouse.containsMouse ? Qt.rgba(0, 0.74, 0.84, 0.12) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    spacing: 4

                    Text {
                        text: model.callsign
                        font.family: "Monospace"; font.pixelSize: 10; font.bold: true
                        color: {
                            if (model.isNewDxcc) return "#FF4081"
                            if (model.isNewGrid) return "#7C4DFF"
                            if (model.isWanted)  return "#FFAB00"
                            if (model.age === 0) return "#00BCD4"
                            return "#ECEFF1"
                        }
                        Layout.preferredWidth: 78; elide: Text.ElideRight
                    }
                    Text {
                        text: model.frequency
                        font.family: "Monospace"; font.pixelSize: 10
                        color: "#B0BEC5"; Layout.preferredWidth: 48
                    }
                    Text {
                        text: model.snr
                        font.family: "Monospace"; font.pixelSize: 10
                        color: model.snr >= 0 ? "#4CAF50" : (model.snr >= -15 ? "#FF9800" : "#f44336")
                        Layout.preferredWidth: 33
                    }
                    Text {
                        text: model.grid || ""
                        font.family: "Monospace"; font.pixelSize: 10
                        color: model.isLotwUser ? "#64DD17" : "#B0BEC5"
                        Layout.preferredWidth: 48
                    }
                    Text {
                        text: model.lastUtc
                        font.family: "Monospace"; font.pixelSize: 10
                        color: "#546E7A"; Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: stnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: {
                        // Use bridge to handle double-click on a callsign
                        if (bridge.activeStations) {
                            var call = bridge.activeStations.callsignAt(index)
                            var freq = bridge.activeStations.frequencyAt(index)
                            var msg = "CQ " + call + " " + (model.grid || "")
                            bridge.processDecodeDoubleClick(msg, model.lastUtc, String(model.snr), freq)
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: !bridge.activeStations || bridge.activeStations.count === 0
                text: "No active stations"
                font.family: "Monospace"; font.pixelSize: 10
                color: "#546E7A"
            }
        }
    }
}
