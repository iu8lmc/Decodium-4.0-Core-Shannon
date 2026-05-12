import QtQuick
import QtQuick.Layouts

// TimeSyncPanel — adattato per bridge.* (bridge è il context object principale)
Rectangle {
    id: timeSyncPanel
    color: Qt.rgba(0, 0, 0, 0.7)
    border.color: Qt.rgba(0, 188, 212, 0.4)
    border.width: 1
    radius: 6

    signal closeRequested()

    property var dragTarget: null
    property bool showCloseButton: false
    property bool expanded: false
    readonly property bool dtMetricsActive: {
        var mode = (bridge.mode || "").toString().toUpperCase()
        return mode === "FT4" || mode === "FT8"
    }
    implicitHeight: expanded
        ? headerRow.implicitHeight + 8 + gridContent.implicitHeight + 12
        : headerRow.implicitHeight + 8

    clip: true

    function correctedUtcNow() {
        var nowMs = Date.now()
        if (bridge.ntpSynced)
            nowMs += bridge.ntpOffsetMs
        return new Date(nowMs)
    }

    Behavior on implicitHeight { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

    Column {
        id: contentCol
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Header (always visible, clickable to expand)
        RowLayout {
            id: headerRow
            width: parent.width
            spacing: 6

            Text {
                text: timeSyncPanel.expanded ? "▼" : "▶"
                font.pixelSize: 9
                color: "#90CAF9"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: timeSyncPanel.expanded = !timeSyncPanel.expanded
                }
            }

            Text {
                text: "TIME SYNC"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
                color: "#B0BEC5"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: timeSyncPanel.dragTarget ? Qt.SizeAllCursor : Qt.PointingHandCursor
                    drag.target: timeSyncPanel.dragTarget
                    drag.axis: Drag.XAndYAxis
                    drag.minimumX: 0
                    drag.minimumY: 0
                    drag.maximumX: timeSyncPanel.dragTarget && timeSyncPanel.dragTarget.parent
                                   ? Math.max(0, timeSyncPanel.dragTarget.parent.width - timeSyncPanel.dragTarget.width)
                                   : 0
                    drag.maximumY: timeSyncPanel.dragTarget && timeSyncPanel.dragTarget.parent
                                   ? Math.max(0, timeSyncPanel.dragTarget.parent.height - timeSyncPanel.dragTarget.height)
                                   : 0
                    onClicked: timeSyncPanel.expanded = !timeSyncPanel.expanded
                    onReleased: {
                        if (timeSyncPanel.dragTarget && timeSyncPanel.dragTarget.savePosition)
                            timeSyncPanel.dragTarget.savePosition()
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 20

                MouseArea {
                    anchors.fill: parent
                    cursorShape: timeSyncPanel.dragTarget ? Qt.SizeAllCursor : Qt.PointingHandCursor
                    drag.target: timeSyncPanel.dragTarget
                    drag.axis: Drag.XAndYAxis
                    drag.minimumX: 0
                    drag.minimumY: 0
                    drag.maximumX: timeSyncPanel.dragTarget && timeSyncPanel.dragTarget.parent
                                   ? Math.max(0, timeSyncPanel.dragTarget.parent.width - timeSyncPanel.dragTarget.width)
                                   : 0
                    drag.maximumY: timeSyncPanel.dragTarget && timeSyncPanel.dragTarget.parent
                                   ? Math.max(0, timeSyncPanel.dragTarget.parent.height - timeSyncPanel.dragTarget.height)
                                   : 0
                    onClicked: timeSyncPanel.expanded = !timeSyncPanel.expanded
                    onReleased: {
                        if (timeSyncPanel.dragTarget && timeSyncPanel.dragTarget.savePosition)
                            timeSyncPanel.dragTarget.savePosition()
                    }
                }
            }

            // UTC Clock
            Text {
                id: utcClock
                font.family: "Monospace"
                font.pixelSize: 11
                font.bold: true
                color: "#00BCD4"
                text: Qt.formatDateTime(timeSyncPanel.correctedUtcNow(), "HH:mm:ss.z") + " UTC"
            }

            // NTP status dot
            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 3.5
                color: {
                    if (!bridge.ntpEnabled) return "#78909C"
                    if (!bridge.ntpSynced) return "#FF9800"
                    var offset = Math.abs(bridge.ntpOffsetMs)
                    if (offset < 20)  return "#4CAF50"
                    if (offset < 100) return "#FF9800"
                    return "#f44336"
                }
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                visible: timeSyncPanel.showCloseButton
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: 4
                color: closeMouse.containsMouse
                       ? Qt.rgba(244 / 255, 67 / 255, 54 / 255, 0.24)
                       : Qt.rgba(255, 255, 255, 0.04)
                border.color: closeMouse.containsMouse ? "#f44336" : Qt.rgba(255, 255, 255, 0.16)

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 10
                    font.bold: true
                    color: closeMouse.containsMouse ? "#f44336" : "#B0BEC5"
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: timeSyncPanel.closeRequested()
                }
            }
        }

        // Expanded content
        GridLayout {
            id: gridContent
            visible: timeSyncPanel.expanded
            columns: 2
            columnSpacing: 16
            rowSpacing: 4
            width: parent.width

            // NTP Offset
            Text {
                text: "NTP Offset:"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: {
                    if (!bridge.ntpEnabled) return "Disabled"
                    if (!bridge.ntpSynced) return "Syncing..."
                    return bridge.ntpOffsetMs.toFixed(1) + " ms"
                }
                font.family: "Monospace"
                font.pixelSize: 10
                color: {
                    if (!bridge.ntpEnabled) return "#78909C"
                    if (!bridge.ntpSynced) return "#FF9800"
                    var offset = Math.abs(bridge.ntpOffsetMs)
                    if (offset < 20)  return "#4CAF50"
                    if (offset < 100) return "#FF9800"
                    return "#f44336"
                }
            }

            // Soundcard Drift
            Text {
                text: "SC Drift:"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: bridge.soundcardDriftPpm.toFixed(1) + " ppm (" +
                      bridge.soundcardDriftMsPerPeriod.toFixed(1) + " ms/T)"
                font.family: "Monospace"
                font.pixelSize: 10
                color: {
                    var sev = bridge.driftSeverity
                    if (sev === "green")  return "#4CAF50"
                    if (sev === "yellow") return "#FF9800"
                    return "#f44336"
                }
            }

            // Average DT
            Text {
                text: "Avg DT:"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: {
                    if (!timeSyncPanel.dtMetricsActive) return "N/A"
                    if (bridge.timeSyncSampleCount <= 0) {
                        return bridge.decodeLatencyMs > 0 ? "No qualifying decodes" : "Waiting cycle"
                    }
                    var dt = bridge.avgDt
                    var status = Math.abs(dt) < 0.1 ? "LOCKED" :
                                 Math.abs(dt) < 0.3 ? "Converging" : "Adjusting"
                    return dt.toFixed(3) + " s (" + status + ")"
                }
                font.family: "Monospace"
                font.pixelSize: 10
                color: {
                    if (!timeSyncPanel.dtMetricsActive) return "#78909C"
                    if (bridge.timeSyncSampleCount <= 0) return "#78909C"
                    var dt = Math.abs(bridge.avgDt)
                    if (dt < 0.1) return "#4CAF50"
                    if (dt < 0.3) return "#FF9800"
                    return "#f44336"
                }
            }

            // Decode Latency
            Text {
                text: "Decode Latency:"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: {
                    if (!timeSyncPanel.dtMetricsActive) return "N/A"
                    if (bridge.decodeLatencyMs < 0) return "TIMEOUT"
                    if (bridge.decodeLatencyMs === 0) return "Waiting cycle"
                    return bridge.decodeLatencyMs.toFixed(0) + " ms"
                }
                font.family: "Monospace"
                font.pixelSize: 10
                color: {
                    if (!timeSyncPanel.dtMetricsActive) return "#78909C"
                    if (bridge.decodeLatencyMs < 0)    return "#f44336"
                    if (bridge.decodeLatencyMs === 0)  return "#78909C"
                    if (bridge.decodeLatencyMs < 3000) return "#4CAF50"
                    if (bridge.decodeLatencyMs < 8000) return "#FF9800"
                    return "#f44336"
                }
            }

            Text {
                text: "Settings:"
                font.family: "Monospace"
                font.pixelSize: 10
                color: "#78909C"
            }
            Rectangle {
                implicitWidth: 72
                implicitHeight: 20
                radius: 4
                color: ntpSettingsMouse.containsMouse
                       ? Qt.rgba(0, 188, 212, 0.18)
                       : Qt.rgba(255, 255, 255, 0.04)
                border.color: Qt.rgba(0, 188, 212, 0.45)

                Text {
                    anchors.centerIn: parent
                    text: "Open"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    color: "#B2EBF2"
                }

                MouseArea {
                    id: ntpSettingsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bridge.openTimeSyncSettings()
                }
            }
        }
    }

    // UTC clock update timer (100ms)
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: utcClock.text = Qt.formatDateTime(timeSyncPanel.correctedUtcNow(), "HH:mm:ss.z") + " UTC"
    }

}
