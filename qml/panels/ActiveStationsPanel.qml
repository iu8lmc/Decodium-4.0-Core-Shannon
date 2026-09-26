import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// ActiveStationsPanel — usa bridge.activeStations (QObject* → ActiveStationsModel)
Rectangle {
    id: activePanel
    signal closeRequested()
    property var nativeHostWindow: null
    color: Qt.rgba(0, 0, 0, 0.75)
    border.color: Qt.rgba(0, 188, 212, 0.4)
    border.width: 1
    radius: 6
    implicitHeight: 280
    implicitWidth: 340

    function startNativeHostMove() {
        if (!nativeHostWindow || typeof nativeHostWindow.startSystemMove !== "function")
            return false
        try {
            return nativeHostWindow.startSystemMove()
        } catch (error) {
            console.log("Active Stations startSystemMove failed: " + error)
        }
        return false
    }

    function finishNativeHostMove() {
        if (nativeHostWindow && typeof nativeHostWindow.finishDesktopMove === "function")
            nativeHostWindow.finishDesktopMove()
    }

    function requestWindowClose() {
        if (nativeHostWindow && typeof nativeHostWindow.hideHostedWindow === "function") {
            nativeHostWindow.hideHostedWindow()
            return
        }
        closeRequested()
    }

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Header
        Item {
            width: parent.width
            height: 22

            MouseArea {
                id: activeStationsDragArea
                anchors.fill: parent
                anchors.rightMargin: 30
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                property point pressGlobalPos: Qt.point(0, 0)
                property point pressWindowPos: Qt.point(0, 0)
                property bool nativeMoveActive: false
                cursorShape: activePanel.nativeHostWindow ? Qt.SizeAllCursor : Qt.ArrowCursor
                onPressed: function(mouse) {
                    if (!activePanel.nativeHostWindow)
                        return
                    pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    pressWindowPos = Qt.point(activePanel.nativeHostWindow.x,
                                              activePanel.nativeHostWindow.y)
                    nativeMoveActive = activePanel.startNativeHostMove()
                    mouse.accepted = true
                }
                onPositionChanged: function(mouse) {
                    if (!pressed || !activePanel.nativeHostWindow || nativeMoveActive)
                        return
                    var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    activePanel.nativeHostWindow.x = Math.round(
                                pressWindowPos.x + currentGlobalPos.x - pressGlobalPos.x)
                    activePanel.nativeHostWindow.y = Math.round(
                                pressWindowPos.y + currentGlobalPos.y - pressGlobalPos.y)
                    mouse.accepted = true
                }
                onReleased: {
                    nativeMoveActive = false
                    activePanel.finishNativeHostMove()
                }
                onCanceled: {
                    nativeMoveActive = false
                    activePanel.finishNativeHostMove()
                }
            }

            RowLayout {
                anchors.fill: parent
                spacing: 6

                Text {
                    text: qsTr("ACTIVE STATIONS")
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 10
                    font.bold: true
                    color: "#00BCD4"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: (bridge.activeStations ? bridge.activeStations.count : 0) + " stn"
                    font.family: decodiumMonoFontFamily
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
                        font.family: decodiumMonoFontFamily
                        font.pixelSize: 10
                        color: "#00BCD4"
                    }

                    MouseArea {
                        id: closeMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: activePanel.requestWindowClose()
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
                    text: qsTr("CQ Only"); font.family: decodiumMonoFontFamily; font.pixelSize: 9
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
                    text: qsTr("Wanted"); font.family: decodiumMonoFontFamily; font.pixelSize: 9
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
                    text: qsTr("Clear"); font.family: decodiumMonoFontFamily; font.pixelSize: 9
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
            Text { text: qsTr("Call");  font.family: decodiumMonoFontFamily; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 78 }
            Text { text: qsTr("Freq");  font.family: decodiumMonoFontFamily; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 48 }
            Text { text: "SNR";   font.family: decodiumMonoFontFamily; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 33 }
            Text { text: qsTr("Grid");  font.family: decodiumMonoFontFamily; font.pixelSize: 9; color: "#546E7A"; Layout.preferredWidth: 48 }
            Text { text: "UTC";   font.family: decodiumMonoFontFamily; font.pixelSize: 9; color: "#546E7A"; Layout.fillWidth: true }
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
                        font.family: decodiumMonoFontFamily; font.pixelSize: 10; font.bold: true
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
                        font.family: decodiumMonoFontFamily; font.pixelSize: 10
                        color: "#B0BEC5"; Layout.preferredWidth: 48
                    }
                    Text {
                        text: model.snr
                        font.family: decodiumMonoFontFamily; font.pixelSize: 10
                        color: model.snr >= 0 ? "#4CAF50" : (model.snr >= -15 ? "#FF9800" : "#f44336")
                        Layout.preferredWidth: 33
                    }
                    Text {
                        text: model.grid || ""
                        font.family: decodiumMonoFontFamily; font.pixelSize: 10
                        color: model.isLotwUser ? "#64DD17" : "#B0BEC5"
                        Layout.preferredWidth: 48
                    }
                    Text {
                        text: model.lastUtc
                        font.family: decodiumMonoFontFamily; font.pixelSize: 10
                        color: "#546E7A"; Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: stnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    // IU8LMC: click destro -> apre la scheda del nominativo su QRZ.com
                    onClicked: (mouse) => {
                        if (mouse.button !== Qt.RightButton || !bridge.activeStations) return
                        var c = String(bridge.activeStations.callsignAt(index) || "").toUpperCase()
                        var segs = c.split("/"), best = ""
                        for (var j = 0; j < segs.length; ++j) if (segs[j].length > best.length) best = segs[j]
                        var call = (best.length ? best : c).replace(/[^A-Z0-9]/g, "")
                        if (call.length > 0) Qt.openUrlExternally("https://www.qrz.com/db/" + call)
                    }
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
                text: qsTr("No active stations")
                font.family: decodiumMonoFontFamily; font.pixelSize: 10
                color: "#546E7A"
            }
        }
    }
}
