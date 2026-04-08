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
    implicitHeight: expanded
        ? headerRow.implicitHeight + 8 + gridContent.implicitHeight + 12
        : headerRow.implicitHeight + 8

    clip: true

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
                font.family: "Consolas"
                font.pixelSize: 10
                font.bold: true
                color: "#B0BEC5"
            }

            Item { Layout.fillWidth: true }

            // UTC Clock
            Text {
                id: utcClock
                font.family: "Consolas"
                font.pixelSize: 11
                font.bold: true
                color: "#00BCD4"
                text: Qt.formatDateTime(new Date(), "HH:mm:ss.z") + " UTC"
            }

            // NTP status dot
            Rectangle {
                width: 7; height: 7; radius: 3.5
                color: {
                    if (!bridge.ntpSynced) return "#f44336"
                    var offset = Math.abs(bridge.ntpOffsetMs)
                    if (offset < 20)  return "#4CAF50"
                    if (offset < 100) return "#FF9800"
                    return "#f44336"
                }
                Layout.alignment: Qt.AlignVCenter
            }

            MouseArea {
                anchors.fill: parent
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
                font.family: "Consolas"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: {
                    if (!bridge.ntpSynced) return "Not synced"
                    return bridge.ntpOffsetMs.toFixed(1) + " ms"
                }
                font.family: "Consolas"
                font.pixelSize: 10
                color: {
                    if (!bridge.ntpSynced) return "#f44336"
                    var offset = Math.abs(bridge.ntpOffsetMs)
                    if (offset < 20)  return "#4CAF50"
                    if (offset < 100) return "#FF9800"
                    return "#f44336"
                }
            }

            // Soundcard Drift
            Text {
                text: "SC Drift:"
                font.family: "Consolas"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: bridge.soundcardDriftPpm.toFixed(1) + " ppm (" +
                      bridge.soundcardDriftMsPerPeriod.toFixed(1) + " ms/T)"
                font.family: "Consolas"
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
                font.family: "Consolas"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: {
                    var dt = bridge.avgDt
                    var status = Math.abs(dt) < 0.1 ? "LOCKED" :
                                 Math.abs(dt) < 0.3 ? "Converging" : "Adjusting"
                    return dt.toFixed(3) + " s (" + status + ")"
                }
                font.family: "Consolas"
                font.pixelSize: 10
                color: {
                    var dt = Math.abs(bridge.avgDt)
                    if (dt < 0.1) return "#4CAF50"
                    if (dt < 0.3) return "#FF9800"
                    return "#f44336"
                }
            }

            // Decode Latency
            Text {
                text: "Decode Latency:"
                font.family: "Consolas"
                font.pixelSize: 10
                color: "#78909C"
            }
            Text {
                text: {
                    if (bridge.decodeLatencyMs < 0) return "TIMEOUT"
                    return bridge.decodeLatencyMs.toFixed(0) + " ms"
                }
                font.family: "Consolas"
                font.pixelSize: 10
                color: {
                    if (bridge.decodeLatencyMs < 0)    return "#f44336"
                    if (bridge.decodeLatencyMs < 3000) return "#4CAF50"
                    if (bridge.decodeLatencyMs < 8000) return "#FF9800"
                    return "#f44336"
                }
            }
        }
    }

    // UTC clock update timer (100ms)
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: utcClock.text = Qt.formatDateTime(new Date(), "HH:mm:ss.z") + " UTC"
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
