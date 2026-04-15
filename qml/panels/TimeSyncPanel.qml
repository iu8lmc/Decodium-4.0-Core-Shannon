import QtQuick
import QtQuick.Layouts

// TimeSyncPanel — adattato per bridge.* (bridge è il context object principale)
Rectangle {
    id: timeSyncPanel
    color: Qt.rgba(0, 0, 0, 0.7)
    border.color: Qt.rgba(0, 188, 212, 0.4)
    border.width: 1
    radius: 6

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
                text: expanded ? "▼" : "▶"
                font.pixelSize: 9
                color: "#90CAF9"
            }

            Text {
                text: "TIME SYNC"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
                color: "#B0BEC5"
            }

            Item { Layout.fillWidth: true }

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
                width: 7; height: 7; radius: 3.5
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

            MouseArea {
                Layout.fillWidth: true
                Layout.fillHeight: true
                onClicked: timeSyncPanel.expanded = !timeSyncPanel.expanded
            }
        }

        // Expanded content
        GridLayout {
            id: gridContent
            visible: expanded
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

    // Click header to expand
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: headerRow.implicitHeight + 8
        onClicked: timeSyncPanel.expanded = !timeSyncPanel.expanded
    }
}
