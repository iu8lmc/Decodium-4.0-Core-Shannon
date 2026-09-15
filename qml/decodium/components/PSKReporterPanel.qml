/* PSKReporterPanel — DX-Pedition Mode "PSK Reporter · heard-by" (design 3.4)
 * Phase 3 (1.0.33x): chi nel mondo riceve il MIO segnale. Reads the GLOBAL
 * bridge: bridge.pskHeardByList (QVariantList di {call,grid,snr,freq,distKm,
 * dxcc}) + top stats pskHeardByCount / pskHeardByDxccCount / pskHeardByMaxKm.
 * Backend: bridge.fetchPskHeardBy() (async HTTP, rate-limit 60s interno).
 * On-demand fetch all'attivazione + Timer 300s SOLO quando visibile (anti-spam).
 * Classic workspace untouched (opt-in: vive solo dentro la vista DX-Pedition).
 *
 * Lesson 1.0.205: `!modelData` guard FIRST in every delegate clause.
 * Lesson Fase 2a: never shadow the global `bridge` (default-bound below).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)

    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:   tm ? tm.accentColor    : "#19ff88"
    readonly property color cBorder:   tm ? tm.borderColor    : "#1f2a22"
    readonly property color cText:     tm ? tm.textPrimary    : "#d6dcd8"
    readonly property color cTextDim:  tm ? tm.textSecondary  : "#6c7872"
    readonly property color cGrid:     tm ? tm.gridColor      : "#00d4b4"
    readonly property color cPanelHdr: tm ? tm.panelHeader    : "#0a0e0c"

    readonly property int rowH:  tm ? tm.densityRowHeight() : 22
    readonly property int fSize: tm ? tm.densityFontSize()  : 12

    // --- "last fetch" age (seconds) for the header meta, ticked by the timer ----
    property double lastFetchEpoch: 0
    property int lastAgeSec: 0
    property bool displayDistanceInMiles: root.bridge ? root._coerceBool(root.bridge.getSetting("Miles", false), false) : false

    function _doFetch() {
        if (root.bridge && typeof root.bridge.fetchPskHeardBy === "function") {
            root.bridge.fetchPskHeardBy()
            root.lastFetchEpoch = Date.now()
        }
    }

    function _coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return !!fallback
        if (typeof value === "boolean")
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return !!fallback
    }

    // Format a distance from stored km into the active user unit.
    function _fmtDistance(km) {
        if (km === undefined || km === null || km < 0) return "—"
        var value = displayDistanceInMiles ? Number(km) * 0.621371192 : Number(km)
        if (value >= 1000) return (value / 1000).toFixed(1) + "k"
        return Math.round(value).toString()
    }

    function _distanceUnit() {
        return displayDistanceInMiles ? "mi" : "km"
    }
    // Signed dB -> "-12" / "+03" / "—".
    function _fmtDb(snr) {
        if (snr === undefined || snr === null || snr === "") return "—"
        var n = Number(snr)
        if (isNaN(n)) return "—"
        return (n > 0 ? "+" : "") + n
    }

    // On-demand fetch when the panel first becomes active.
    Component.onCompleted: if (root.visible) root._doFetch()
    onVisibleChanged: if (root.visible && root.lastFetchEpoch === 0) root._doFetch()

    // 300s auto-refresh — ONLY while visible (do not spam the service / network).
    Timer {
        interval: 300000           // 5 min: heard-by changes slowly
        running: root.visible
        repeat: true
        onTriggered: root._doFetch()
    }
    // 1s tick just to age the "last Ns" label (cheap, visible-gated).
    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        onTriggered: {
            if (root.lastFetchEpoch > 0)
                root.lastAgeSec = Math.round((Date.now() - root.lastFetchEpoch) / 1000)
        }
    }

    Connections {
        target: root.bridge
        function onSettingValueChanged(key, value) {
            if (key === "Miles")
                root.displayDistanceInMiles = root._coerceBool(value, false)
        }
    }

    // ===========================================================================
    // Meta strip: "heard by N · N min · last Ns" (+ a subtle live dot when fetching).
    // ===========================================================================
    Rectangle {
        id: meta
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 22
        color: root.cPanelHdr
        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
            spacing: 8
            Text {
                text: {
                    var n = root.bridge ? Number(root.bridge.pskHeardByCount) : 0
                    var span = root.bridge ? Number(root.bridge.pskReporterTimeSpanMinutes) : 60
                    return "heard by " + n + " · " + span + " min"
                }
                color: root.cTextDim
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: !!(root.bridge && root.bridge.pskHeardByFetching)
                text: qsTr("updating…")
                color: root.cAccent
                font.pixelSize: 9; font.italic: true
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                visible: root.lastFetchEpoch > 0 && !(root.bridge && root.bridge.pskHeardByFetching)
                text: "last " + root.lastAgeSec + "s"
                color: root.cTextDim
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }
            // Manual refresh tap (respects the 60s backend rate-limit anyway).
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                width: 16; height: 16; radius: 3
                color: refreshMA.containsMouse ? Qt.rgba(root.cAccent.r, root.cAccent.g, root.cAccent.b, 0.18)
                                               : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "⟳"
                    color: root.cAccent
                    font.pixelSize: 12
                }
                MouseArea {
                    id: refreshMA
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root._doFetch()
                }
            }
        }
    }

    // ===========================================================================
    // Top stats strip: 3 columns (RX me / DXCC / km max) with large accent values.
    // ===========================================================================
    Rectangle {
        id: stats
        anchors { left: parent.left; right: parent.right; top: meta.bottom }
        height: 52
        color: "transparent"
        RowLayout {
            anchors.fill: parent
            spacing: 0
            Repeater {
                model: 3
                delegate: Item {
                    id: statCol
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Right divider line (skip the last column).
                    Rectangle {
                        visible: statCol.index < 2
                        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 8; bottomMargin: 8 }
                        width: 1
                        color: root.cBorder
                    }
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: {
                                if (!root.bridge) return "—"
                                if (statCol.index === 0) return String(Number(root.bridge.pskHeardByCount))
                                if (statCol.index === 1) return String(Number(root.bridge.pskHeardByDxccCount))
                                return root._fmtDistance(Number(root.bridge.pskHeardByMaxKm))
                            }
                            color: root.cAccent
                            font.pixelSize: 18; font.bold: true; font.family: decodiumMonoFontFamily
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: statCol.index === 0 ? "RX ME"
                                : statCol.index === 1 ? "DXCC" : root._distanceUnit().toUpperCase() + " MAX"
                            color: root.cTextDim
                            font.pixelSize: 9; font.letterSpacing: 1.0
                        }
                    }
                }
            }
        }
    }

    // Thin divider under stats.
    Rectangle {
        id: statsDiv
        anchors { left: parent.left; right: parent.right; top: stats.bottom }
        height: 1
        color: root.cBorder
    }

    // Column header strip: CALLSIGN | GRID | dB | DISTANCE
    Rectangle {
        id: colHdr
        anchors { left: parent.left; right: parent.right; top: statsDiv.bottom }
        height: 18
        color: Qt.rgba(root.cGrid.r, root.cGrid.g, root.cGrid.b, 0.10)
        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
            spacing: 0
            Text { text: "CALLSIGN"; color: root.cGrid; font.pixelSize: 9; font.bold: true; font.family: decodiumMonoFontFamily; Layout.fillWidth: true }
            Text { text: "GRID"; color: root.cGrid; font.pixelSize: 9; font.bold: true; font.family: decodiumMonoFontFamily; Layout.preferredWidth: 52 }
            Text { text: "dB"; color: root.cGrid; font.pixelSize: 9; font.bold: true; font.family: decodiumMonoFontFamily; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 34 }
            Text { text: "DISTANCE"; color: root.cGrid; font.pixelSize: 9; font.bold: true; font.family: decodiumMonoFontFamily; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 64 }
        }
    }

    // ===========================================================================
    // Reception report rows (one line per receiver).
    // ===========================================================================
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: colHdr.bottom; bottom: parent.bottom }
        anchors.margins: 2
        clip: true
        spacing: 1
        cacheBuffer: 400
        reuseItems: true
        model: (root.bridge && root.bridge.pskHeardByList) ? root.bridge.pskHeardByList : null

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 8 }

        delegate: Rectangle {
            id: del
            required property var modelData
            required property int index
            readonly property var entry: modelData || ({})
            width: list.width
            height: root.rowH
            radius: 2
            color: !modelData ? "transparent"
                 : (index % 2 === 0) ? Qt.rgba(root.cGrid.r, root.cGrid.g, root.cGrid.b, 0.05)
                                     : Qt.rgba(root.cGrid.r, root.cGrid.g, root.cGrid.b, 0.10)

            RowLayout {
                visible: !!del.modelData
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                spacing: 0
                Text {
                    text: !del.modelData ? "" : String(del.entry.call || "—")
                    color: root.cAccent
                    font.pixelSize: root.fSize; font.bold: true; font.family: decodiumMonoFontFamily
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: !del.modelData ? "" : String(del.entry.grid || "—")
                    color: root.cTextDim
                    font.pixelSize: root.fSize; font.family: decodiumMonoFontFamily
                    Layout.preferredWidth: 52
                }
                Text {
                    text: !del.modelData ? "" : root._fmtDb(del.entry.snr)
                    color: root.cText
                    font.pixelSize: root.fSize; font.family: decodiumMonoFontFamily
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: 34
                }
                Text {
                    text: {
                        if (!del.modelData) return ""
                        var km = Number(del.entry.distKm)
                        return (km < 0) ? "—" : root._fmtDistance(km) + " " + root._distanceUnit()
                    }
                    color: root.cTextDim
                    font.pixelSize: root.fSize - 2; font.family: decodiumMonoFontFamily
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: 64
                }
            }
        }
    }

    // Empty state.
    Text {
        anchors.centerIn: list
        visible: (!root.bridge) || Number(root.bridge.pskHeardByCount) === 0
        text: (root.bridge && root.bridge.pskHeardByFetching) ? "querying PSK Reporter…"
                                                              : "no reports yet"
        color: root.cTextDim
        font.pixelSize: 11; font.italic: true
    }
}
