import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: root

    required property var engine
    property var mapLayers: null
    property var operations: null
    property var externalOverlays: null
    property color backgroundColor: "#0b1220"
    property color borderColor: "#2a3950"
    property color primaryColor: "#3f7cff"
    property color accentColor: "#2ecc71"
    property color textColor: "#e5eefc"
    property color mutedColor: "#9db1c9"

    function expose(window) {
        window.show()
        window.raise()
        window.requestActivate()
    }
    function openRoster() { expose(rosterWindow) }
    function openStatistics() { expose(statisticsWindow) }
    function openConditions() { expose(conditionsWindow) }
    function ageText(seconds) {
        var value = Number(seconds)
        if (!isFinite(value) || value < 0)
            return qsTr("no sample")
        if (value < 60)
            return qsTr("%1s old").arg(Math.round(value))
        if (value < 3600)
            return qsTr("%1m old").arg(Math.floor(value / 60))
        return qsTr("%1h old").arg((value / 3600).toFixed(1))
    }
    function stateColor(state) {
        if (state === "current") return root.accentColor
        if (state === "stale") return "#f6c344"
        if (state === "error") return "#e35d6a"
        return root.mutedColor
    }
    function rosterGridLabel(row) {
        if (!row || !row.grid)
            return ""
        return String(row.grid) + (row.gridMarker ? " " + row.gridMarker : "")
    }

    Window {
        id: rosterWindow
        width: 620
        height: 680
        minimumWidth: 420
        minimumHeight: 360
        visible: false
        title: qsTr("Map Roster - Decodium")
        color: root.backgroundColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            Text {
                text: qsTr("LIVE CALL ROSTER")
                color: root.primaryColor
                font.pixelSize: 13
                font.bold: true
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Filter call, grid or DXCC")
                font.pixelSize: 10
                onTextEdited: {
                    if (root.mapLayers)
                        root.mapLayers.rosterTextFilter = text
                }
            }
            ListView {
                id: detachedRosterList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                // A hidden detached window must not retain a second live
                // roster delegate tree during RX.
                model: rosterWindow.visible && root.mapLayers
                       ? root.mapLayers.roster : []
                reuseItems: true
                cacheBuffer: 320
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: detachedRosterList.width
                    height: 48
                    radius: 3
                    color: index % 2 ? "#101a28" : "#0d2430"
                    border.width: modelData.wanted ? 1 : 0
                    border.color: modelData.confirmed
                        ? root.accentColor : "#f6c344"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        Text {
                            Layout.preferredWidth: 110
                            text: modelData.call || ""
                            color: root.textColor
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.preferredWidth: 142
                            text: root.rosterGridLabel(modelData)
                                  + (modelData.gridOrigin ? " · " + modelData.gridOrigin : "")
                            color: root.mutedColor
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%1  %2  %3 dB  %4  %5")
                                .arg(modelData.band || "—")
                                .arg(modelData.mode || "—")
                                .arg(modelData.snr || 0)
                                .arg(modelData.dxcc || "")
                                .arg(Number(modelData.sourceCount || 1) > 1
                                     ? "✓ " + (modelData.sourceSummary || "")
                                     : (modelData.sourceSummary
                                        || modelData.source || ""))
                            color: root.mutedColor
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                        Button {
                            text: qsTr("CALL")
                            font.pixelSize: 9
                            enabled: !!modelData.call
                            onClicked: root.engine.processMapRosterCall(
                                modelData.call, modelData.grid || "")
                        }
                    }
                }
            }
        }
    }

    Window {
        id: statisticsWindow
        width: 620
        height: 720
        minimumWidth: 420
        minimumHeight: 360
        visible: false
        title: qsTr("Map Statistics - Decodium")
        color: root.backgroundColor
        MapStatisticsPanel {
            anchors.fill: parent
            anchors.margins: 10
            operations: root.operations
            mapLayers: root.mapLayers
            legacyStatistics: root.mapLayers ? root.mapLayers.statistics : ({})
            borderColor: root.borderColor
            primaryColor: root.primaryColor
            accentColor: root.accentColor
            textColor: root.textColor
            mutedColor: root.mutedColor
        }
    }

    Window {
        id: conditionsWindow
        width: 620
        height: 600
        minimumWidth: 420
        minimumHeight: 320
        visible: false
        title: qsTr("Radio Conditions - Decodium")
        color: root.backgroundColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("EXTERNAL MAP CONDITIONS")
                    color: root.primaryColor
                    font.pixelSize: 13
                    font.bold: true
                }
                Button {
                    text: qsTr("Refresh")
                    font.pixelSize: 9
                    enabled: root.externalOverlays
                    onClicked: root.externalOverlays.refreshAll()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 74
                radius: 3
                color: "#101a28"
                border.width: 1
                border.color: root.borderColor
                visible: root.externalOverlays
                Column {
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 3
                    Text {
                        text: root.externalOverlays
                              && root.externalOverlays.moonDataAvailable
                            ? qsTr("Moon: Az %1°  El %2°  Distance %3 km")
                                  .arg(root.externalOverlays.moonAzimuth.toFixed(1))
                                  .arg(root.externalOverlays.moonElevation.toFixed(1))
                                  .arg(Math.round(root.externalOverlays.moonDistanceKm))
                            : qsTr("Moon data unavailable")
                        color: "#dbe7ff"
                        font.pixelSize: 10
                    }
                    Text {
                        text: root.externalOverlays
                              && root.externalOverlays.moonDataAvailable
                            ? qsTr("Sublunar point %1°, %2°  Illumination %3%")
                                  .arg(root.externalOverlays.moonSublunarLatitude.toFixed(2))
                                  .arg(root.externalOverlays.moonSublunarLongitude.toFixed(2))
                                  .arg(root.externalOverlays.moonIllumination.toFixed(1))
                            : ""
                        color: root.mutedColor
                        font.pixelSize: 9
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                radius: 3
                color: "#0d2430"
                border.width: 1
                border.color: root.borderColor
                visible: root.externalOverlays
                Column {
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 3
                    Text {
                        text: qsTr("PROPAGATION FORECAST · age / validity / decay")
                        color: root.primaryColor
                        font.pixelSize: 9
                        font.bold: true
                    }
                    ListView {
                        width: parent.width
                        height: 48
                        orientation: ListView.Horizontal
                        spacing: 5
                        clip: true
                        model: root.externalOverlays
                            ? root.externalOverlays.temporalLegend : []
                        delegate: Rectangle {
                            required property var modelData
                            width: 112
                            height: 44
                            radius: 3
                            color: "#101a28"
                            border.width: 1
                            border.color: root.stateColor(modelData.state)
                            Column {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 1
                                Text {
                                    text: modelData.label || modelData.layerId
                                    color: root.stateColor(modelData.state)
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Text {
                                    text: root.ageText(modelData.ageSeconds)
                                          + " · " + (modelData.stateText || "")
                                    color: root.mutedColor
                                    font.pixelSize: 8
                                }
                                Text {
                                    text: modelData.validUntilText
                                          ? qsTr("valid to %1").arg(modelData.validUntilText)
                                          : qsTr("validity unavailable")
                                    color: root.mutedColor
                                    font.pixelSize: 7
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
            ListView {
                id: providerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                model: root.externalOverlays
                    ? root.externalOverlays.providerStatus : []
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: providerList.width
                    height: 68
                    radius: 3
                    color: index % 2 ? "#101a28" : "#0d2430"
                    border.width: 1
                    border.color: modelData.error ? "#e35d6a" : root.borderColor
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: modelData.label || modelData.id
                                color: modelData.enabled
                                    ? root.textColor : root.mutedColor
                                font.pixelSize: 10
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.error
                                      || qsTr("%1 items  ·  %2")
                                           .arg(modelData.itemCount || 0)
                                           .arg(modelData.attribution || "")
                                color: modelData.error
                                    ? "#e35d6a" : root.mutedColor
                                font.pixelSize: 8
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.ageText(modelData.ageSeconds)
                                      + "  ·  " + (modelData.stateText || "")
                                      + (modelData.validUntilText
                                         ? qsTr("  · valid to %1").arg(modelData.validUntilText)
                                         : "")
                                color: root.stateColor(modelData.state)
                                font.pixelSize: 7
                                elide: Text.ElideRight
                            }
                        }
                        BusyIndicator {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            running: !!modelData.loading
                            visible: running
                        }
                        Button {
                            text: qsTr("Refresh")
                            font.pixelSize: 8
                            onClicked: root.externalOverlays.refreshLayer(
                                modelData.id)
                        }
                    }
                }
            }
        }
    }
}
