import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium 1.0

Rectangle {
    id: root
    required property var engine

    color: "transparent"

    property color bgDeep: engine ? engine.themeManager.bgDeep : "#0b1220"
    property color primaryBlue: engine ? engine.themeManager.primaryColor : "#3f7cff"
    property color secondaryCyan: engine ? engine.themeManager.secondaryColor : "#00d8ff"
    property color accentGreen: engine ? engine.themeManager.accentColor : "#2ecc71"
    property color textPrimary: engine ? engine.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: engine ? engine.themeManager.textSecondary : "#9db1c9"
    property color glassBorder: engine ? engine.themeManager.glassBorder : "#2a3950"

    function syncMapSettings() {
        if (!engine)
            return
        worldMap.setHomeGrid(engine.grid)
        worldMap.setGreylineEnabled(!!engine.getSetting("ShowGreyline", true))
        worldMap.setDistanceInMiles(!!engine.getSetting("Miles", false))
    }

    function syncTxState() {
        if (!engine)
            return
        worldMap.setTransmitState(!!(engine.transmitting || engine.tuning),
                                  engine.dxCall,
                                  engine.dxGrid,
                                  engine.mode)
    }

    function scheduleRebuild() {
        if (!engine)
            return
        rebuildTimer.restart()
    }

    Timer {
        id: rebuildTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (!root.engine)
                return
            worldMap.clearContacts()
            root.syncMapSettings()
            root.engine.replayWorldMapFeed()
            root.syncTxState()
        }
    }

    Component.onCompleted: scheduleRebuild()
    onVisibleChanged: if (visible) scheduleRebuild()

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.16)
            radius: 4
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: secondaryCyan
                    opacity: root.visible ? 1.0 : 0.5
                }

                Text {
                    text: "Live Map"
                    font.pixelSize: 14
                    font.bold: true
                    color: secondaryCyan
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: engine && engine.grid ? engine.grid : ""
                    font.pixelSize: 10
                    color: textSecondary
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
            border.width: 1
            radius: 4
            clip: true

            WorldMapItem {
                id: worldMap
                anchors.fill: parent
                anchors.margins: 2
                onContactClicked: function(call, grid) {
                    if (root.engine)
                        root.engine.processMapContactClick(call, grid)
                }
            }
        }
    }

    Connections {
        target: engine
        ignoreUnknownSignals: true

        function onGridChanged() {
            root.syncMapSettings()
            if (root.visible)
                root.scheduleRebuild()
        }
        function onDecodeListChanged() {
            if (root.visible)
                root.scheduleRebuild()
        }
        function onRxDecodeListChanged() {
            if (root.visible)
                root.scheduleRebuild()
        }
        function onTransmittingChanged() {
            root.syncTxState()
            if (root.visible)
                root.scheduleRebuild()
        }
        function onTuningChanged() {
            root.syncTxState()
            if (root.visible)
                root.scheduleRebuild()
        }
        function onDxCallChanged() {
            root.syncTxState()
            if (root.visible)
                root.scheduleRebuild()
        }
        function onDxGridChanged() {
            root.syncTxState()
            if (root.visible)
                root.scheduleRebuild()
        }
        function onModeChanged() {
            root.syncTxState()
            if (root.visible)
                root.scheduleRebuild()
        }
        function onSettingValueChanged(key, value) {
            if (key === "ShowGreyline" || key === "MapShowGreyline") {
                worldMap.setGreylineEnabled(!!value)
            } else if (key === "Miles") {
                worldMap.setDistanceInMiles(!!value)
            } else if (key === "WorldMapDisplayed" && root.visible) {
                root.scheduleRebuild()
            }
        }
        function onWorldMapResetRequested() {
            worldMap.clearContacts()
        }
        function onWorldMapContactAdded(call, sourceGrid, destinationGrid, role) {
            worldMap.addContact(call, sourceGrid, destinationGrid, role)
        }
        function onWorldMapContactAddedByLonLat(call, sourceLon, sourceLat, destinationGrid, role) {
            worldMap.addContactByLonLat(call, sourceLon, sourceLat, destinationGrid, role)
        }
        function onWorldMapContactDowngraded(call) {
            worldMap.downgradeContactToBand(call)
        }
    }
}
