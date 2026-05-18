import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium 1.0

Rectangle {
    id: root
    required property var engine

    color: "transparent"
    property bool detachable: false
    property bool detached: false
    signal detachRequested()

    property color bgDeep: engine ? engine.themeManager.bgDeep : "#0b1220"
    property color primaryBlue: engine ? engine.themeManager.primaryColor : "#3f7cff"
    property color secondaryCyan: engine ? engine.themeManager.secondaryColor : "#00d8ff"
    property color accentGreen: engine ? engine.themeManager.accentColor : "#2ecc71"
    property color textPrimary: engine ? engine.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: engine ? engine.themeManager.textSecondary : "#9db1c9"
    property color glassBorder: engine ? engine.themeManager.glassBorder : "#2a3950"
    property var worldMap: worldMapLoader.item
    property bool gpuLiveMapEnabled: engine ? !!engine.getSetting("LiveMapUseGpu", true) : true

    function syncMapSettings() {
        if (!engine || !worldMap)
            return
        worldMap.setHomeGrid(engine.grid)
        worldMap.setGreylineEnabled(!!engine.getSetting("ShowGreyline", true))
        worldMap.setDistanceInMiles(!!engine.getSetting("Miles", false))
    }

    function syncTxState() {
        if (!engine || !worldMap)
            return
        var txTargetCall = engine.currentTx === 6 ? "" : engine.dxCall
        var txTargetGrid = engine.currentTx === 6 ? "" : engine.dxGrid
        worldMap.setTransmitState(!!(engine.transmitting || engine.tuning),
                                  txTargetCall,
                                  txTargetGrid,
                                  engine.mode)
    }

    function scheduleRebuild() {
        if (!engine || !worldMap)
            return
        rebuildTimer.restart()
    }

    function initializeMap() {
        if (!worldMap)
            return
        worldMap.setActive(visible)
        root.syncMapSettings()
        root.syncTxState()
        if (visible)
            root.scheduleRebuild()
    }

    Timer {
        id: rebuildTimer
        // 1.0.209 — Debounce 1s. Era interval:0 (fires next tick) che con
        // 13 signal del bridge che invocavano scheduleRebuild() (decodeList,
        // rxDecodeList, transmitting, tuning, dxCall, dxGrid, currentTx,
        // txEnabled, qsoProgress, autoCqRepeat, mode, settingValue, grid)
        // significava clearContacts + replayWorldMapFeed (re-itera 500 entries
        // + addContact ognuno + paint) 2 volte/sec. Mappa mai stabile, "non
        // si vedeva" perche' sempre in rebuild. Ora rebuild totale solo
        // quando l'utente apre il pannello o cambia home grid; i nuovi
        // contact arrivano incrementali via onWorldMapContactAdded.
        interval: 1000
        repeat: false
        onTriggered: {
            if (!root.engine || !root.worldMap)
                return
            worldMap.clearContacts()
            root.syncMapSettings()
            root.engine.replayWorldMapFeed()
            root.syncTxState()
        }
    }

    Component.onCompleted: {
        // 1.0.213 — pausa l'animation timer del widget legacy quando il
        // pannello non e' visibile (riduce sprechi CPU ~50% in idle dietro
        // ad altri tab/pop-out chiusi).
        root.initializeMap()
    }
    onVisibleChanged: {
        if (worldMap)
            worldMap.setActive(visible)
        if (visible) scheduleRebuild()
    }

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

                // 1.0.223 — Toolbar zoom + greyline. Rifatti come Rectangle
                // inline (no Loader+Component) per evitare il bug 1.0.221 in
                // cui Layout.preferredWidth/Height era sul template Component
                // ma non veniva propagato al Loader -> bottoni 0x0 = invisibili
                // al click. Ora ogni bottone e' un Rectangle diretto figlio
                // del RowLayout, le Layout attached funzionano.
                Rectangle {
                    id: zoomOutBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: zoomOutMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: zoomOutMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "−"
                        font.pixelSize: 14
                        font.bold: true
                        color: zoomOutMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: zoomOutMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.zoomOut(1.4)
                    }
                    ToolTip.visible: zoomOutMa.containsMouse
                    ToolTip.text: qsTr("Zoom out")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: zoomInBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: zoomInMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: zoomInMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        font.pixelSize: 14
                        font.bold: true
                        color: zoomInMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: zoomInMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.zoomIn(1.4)
                    }
                    ToolTip.visible: zoomInMa.containsMouse
                    ToolTip.text: qsTr("Zoom in")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: resetBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: resetMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: resetMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "⌂"
                        font.pixelSize: 12
                        font.bold: true
                        color: resetMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.resetView()
                    }
                    ToolTip.visible: resetMa.containsMouse
                    ToolTip.text: qsTr("Reset view (auto-fit)")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: greylineBtn
                    property bool greylineOn: engine ? !!engine.getSetting("ShowGreyline", true) : true
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: greylineMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : (greylineOn ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18) : "transparent")
                    border.color: (greylineMa.containsMouse || greylineOn) ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "☼"
                        font.pixelSize: 12
                        font.bold: true
                        color: (greylineMa.containsMouse || greylineOn) ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: greylineMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            greylineBtn.greylineOn = !greylineBtn.greylineOn
                            if (root.engine)
                                root.engine.setSetting("ShowGreyline", greylineBtn.greylineOn)
                            if (root.worldMap)
                                root.worldMap.setGreylineEnabled(greylineBtn.greylineOn)
                        }
                    }
                    ToolTip.visible: greylineMa.containsMouse
                    ToolTip.text: qsTr("Toggle day/night greyline overlay")
                    ToolTip.delay: 500
                }

                Rectangle {
                    visible: root.detachable
                    Layout.preferredWidth: root.detached ? 42 : 34
                    Layout.preferredHeight: 18
                    radius: 4
                    color: liveMapDetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25) : "transparent"
                    border.color: liveMapDetachMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.detached ? "Dock" : "Pop"
                        font.pixelSize: 10
                        font.bold: true
                        color: liveMapDetachMA.containsMouse ? secondaryCyan : textSecondary
                    }

                    MouseArea {
                        id: liveMapDetachMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.detachRequested()
                    }

                    ToolTip.visible: liveMapDetachMA.containsMouse
                    ToolTip.text: root.detached ? qsTr("Reattach Live Map") : qsTr("Detach Live Map")
                    ToolTip.delay: 500
                }

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

            Loader {
                id: worldMapLoader
                anchors.fill: parent
                anchors.margins: 2
                sourceComponent: root.gpuLiveMapEnabled ? gpuWorldMapComponent : painterWorldMapComponent
                onLoaded: root.initializeMap()
                onStatusChanged: {
                    if (status === Loader.Error && root.gpuLiveMapEnabled) {
                        console.warn("Live Map GPU component failed to load; falling back to CPU WorldMapItem")
                        root.gpuLiveMapEnabled = false
                    }
                }

                Component {
                    id: painterWorldMapComponent
                    WorldMapItem {
                        onContactClicked: function(call, grid) {
                            if (root.engine)
                                root.engine.processMapContactClick(call, grid)
                        }
                    }
                }

                Component {
                    id: gpuWorldMapComponent
                    WorldMapGpuItem {
                        onContactClicked: function(call, grid) {
                            if (root.engine)
                                root.engine.processMapContactClick(call, grid)
                        }
                    }
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
        // 1.0.209 — RIMOSSO: onDecodeListChanged + onRxDecodeListChanged
        // triggeravano scheduleRebuild() = clearContacts + replayWorldMapFeed
        // a ogni decode (2/sec). I nuovi contact arrivano gia' incrementali
        // via onWorldMapContactAdded signal del bridge, no replay full
        // necessario.
        //
        // I signal TX/QSO sotto chiamano solo syncTxState() (cambio target
        // line lampeggiante, no rebuild): leggera e safe a ogni cambio.
        function onTransmittingChanged() {
            root.syncTxState()
        }
        function onTuningChanged() {
            root.syncTxState()
        }
        function onDxCallChanged() {
            root.syncTxState()
        }
        function onDxGridChanged() {
            root.syncTxState()
        }
        function onCurrentTxChanged() {
            root.syncTxState()
        }
        function onTxEnabledChanged() {
            root.syncTxState()
        }
        function onQsoProgressChanged() {
            root.syncTxState()
        }
        function onAutoCqRepeatChanged() {
            root.syncTxState()
        }
        function onModeChanged() {
            root.syncTxState()
        }
        function onSettingValueChanged(key, value) {
            if (key === "LiveMapUseGpu") {
                root.gpuLiveMapEnabled = !!value
                return
            }
            if (!worldMap)
                return
            if (key === "ShowGreyline" || key === "MapShowGreyline") {
                worldMap.setGreylineEnabled(!!value)
            } else if (key === "Miles") {
                worldMap.setDistanceInMiles(!!value)
            } else if (key === "WorldMapDisplayed" && root.visible) {
                root.scheduleRebuild()
            }
        }
        function onWorldMapResetRequested() {
            if (!root.visible || !worldMap)
                return
            worldMap.clearContacts()
        }
        function onWorldMapContactAdded(call, sourceGrid, destinationGrid, role) {
            if (!root.visible || !worldMap)
                return
            worldMap.addContact(call, sourceGrid, destinationGrid, role)
        }
        function onWorldMapContactAddedByLonLat(call, sourceLon, sourceLat, destinationGrid, role) {
            if (!root.visible || !worldMap)
                return
            worldMap.addContactByLonLat(call, sourceLon, sourceLat, destinationGrid, role)
        }
        function onWorldMapContactDowngraded(call) {
            if (!root.visible || !worldMap)
                return
            worldMap.downgradeContactToBand(call)
        }
    }
}
