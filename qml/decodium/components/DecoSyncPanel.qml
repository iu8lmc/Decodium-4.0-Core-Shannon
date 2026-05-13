/* DecoSyncTime monitoring panel — fase 5.
 *
 * Mostra in tempo reale:
 *  - Stato globale (synced/locked, offset corrente, drift stimato)
 *  - Stato dei 3 source paralleli: NTP UDP, HTTPS Date, SelfCal (network amateur)
 *  - Plot rolling dell'offset Kalman (ultimi 60 secondi)
 *  - Pulsante "Sync Now" per forzare ricalibrazione
 *
 * Aggiornato a 1Hz via Timer. Sample storage rolling per il plot.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Qt.rgba(0.06, 0.07, 0.10, 0.98)
    border.color: Qt.rgba(0.2, 0.55, 0.85, 0.55)
    border.width: 1
    radius: 6

    readonly property color textPrimary:    "#e8edf2"
    readonly property color textSecondary:  "#9aa4af"
    readonly property color accentBlue:     "#3a8edc"
    readonly property color accentGreen:    "#46c46e"
    readonly property color accentAmber:    "#f5a623"
    readonly property color accentRed:      "#e04545"

    // Rolling buffer per il plot dell'offset.
    property var offsetHistory: []
    readonly property int maxHistoryPoints: 120

    function pushSample() {
        if (!bridge.decoSyncTime) return
        var off = bridge.decoSyncTime.kalmanOffsetMs()
        offsetHistory.push(off)
        if (offsetHistory.length > maxHistoryPoints)
            offsetHistory.shift()
        offsetCanvas.requestPaint()
    }

    Timer {
        id: refreshTimer
        interval: 1000
        running: root.visible
        repeat: true
        onTriggered: {
            root.pushSample()
            statusLabel.text = root.buildStatusText()
        }
    }

    function buildStatusText() {
        if (!bridge.decoSyncTime) return "DecoSyncTime non disponibile"
        var ds = bridge.decoSyncTime
        var parts = []
        parts.push("Lock: " + (ds.kalmanHasLock() ? "SI" : "NO"))
        parts.push("Offset: " + ds.kalmanOffsetMs().toFixed(1) + " ms")
        parts.push("Drift: " + (ds.kalmanDriftMsPerSec() * 1000).toFixed(2) + " ms/s")
        return parts.join(" │ ")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            text: "DecoSyncTime Monitor"
            color: accentBlue
            font.pixelSize: 14
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(accentBlue.r, accentBlue.g, accentBlue.b, 0.3)
        }

        Text {
            id: statusLabel
            text: root.buildStatusText()
            color: textPrimary
            font.pixelSize: 11
            font.family: "Consolas, Monaco, monospace"
            Layout.fillWidth: true
        }

        // ── Plot offset Kalman ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: Qt.rgba(0.03, 0.04, 0.06, 0.95)
            border.color: Qt.rgba(0.18, 0.22, 0.28, 0.7)
            border.width: 1
            radius: 3

            Canvas {
                id: offsetCanvas
                anchors.fill: parent
                anchors.margins: 4
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.fillStyle = "rgba(8, 10, 14, 1)"
                    ctx.fillRect(0, 0, width, height)
                    var n = offsetHistory.length
                    if (n < 2) {
                        ctx.fillStyle = "rgba(150, 165, 180, 0.4)"
                        ctx.font = "10px Consolas"
                        ctx.fillText("Acquisizione dati…", width / 2 - 50, height / 2)
                        return
                    }
                    // Range
                    var minV = offsetHistory[0], maxV = offsetHistory[0]
                    for (var i = 1; i < n; ++i) {
                        if (offsetHistory[i] < minV) minV = offsetHistory[i]
                        if (offsetHistory[i] > maxV) maxV = offsetHistory[i]
                    }
                    var span = Math.max(2.0, maxV - minV)
                    var pad = span * 0.1
                    minV -= pad; maxV += pad
                    var spanFull = maxV - minV
                    // Grid orizzontale zero
                    if (minV < 0 && maxV > 0) {
                        var y0 = height - ((0 - minV) / spanFull) * height
                        ctx.strokeStyle = "rgba(120, 130, 145, 0.35)"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(0, y0)
                        ctx.lineTo(width, y0)
                        ctx.stroke()
                    }
                    // Linea offset
                    ctx.strokeStyle = "rgba(58, 142, 220, 0.9)"
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    for (var j = 0; j < n; ++j) {
                        var x = (j / (maxHistoryPoints - 1)) * width
                        var y = height - ((offsetHistory[j] - minV) / spanFull) * height
                        if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                    }
                    ctx.stroke()
                    // Min/max labels
                    ctx.fillStyle = "rgba(154, 164, 175, 0.85)"
                    ctx.font = "9px Consolas"
                    ctx.fillText(maxV.toFixed(1) + " ms", 4, 10)
                    ctx.fillText(minV.toFixed(1) + " ms", 4, height - 4)
                }
            }
        }

        // ── Source status rows ──
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 4
            columnSpacing: 12

            Text { text: "Source"; color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { text: "Stato"; color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { text: "Offset"; color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { text: "Servers"; color: textSecondary; font.pixelSize: 10; font.bold: true }

            // NTP — via legacyNtpClient signals propagated as decoSyncTime
            Text { text: "NTP UDP"; color: textPrimary; font.pixelSize: 11 }
            Text {
                text: bridge.ntpSynced ? "SYNC" : "—"
                color: bridge.ntpSynced ? accentGreen : textSecondary
                font.pixelSize: 11; font.bold: bridge.ntpSynced
            }
            Text {
                text: bridge.ntpSynced ? bridge.ntpOffsetMs.toFixed(1) + " ms" : "—"
                color: textPrimary; font.pixelSize: 11
            }
            Text { text: "8 pool"; color: textSecondary; font.pixelSize: 11 }

            // HTTPS
            Text { text: "HTTPS Date"; color: textPrimary; font.pixelSize: 11 }
            Text {
                text: (bridge.decoSyncTime && bridge.decoSyncTime.httpsSynced()) ? "SYNC" : "—"
                color: (bridge.decoSyncTime && bridge.decoSyncTime.httpsSynced()) ? accentGreen : textSecondary
                font.pixelSize: 11
                font.bold: bridge.decoSyncTime && bridge.decoSyncTime.httpsSynced()
            }
            Text {
                text: (bridge.decoSyncTime && bridge.decoSyncTime.httpsSynced())
                      ? bridge.decoSyncTime.httpsOffsetMs().toFixed(1) + " ms" : "—"
                color: textPrimary; font.pixelSize: 11
            }
            Text {
                text: bridge.decoSyncTime ? bridge.decoSyncTime.httpsLastCount() + "/5" : "—"
                color: textSecondary; font.pixelSize: 11
            }

            // SelfCal
            Text { text: "SelfCal FT"; color: textPrimary; font.pixelSize: 11 }
            Text {
                text: (bridge.decoSyncTime && bridge.decoSyncTime.selfCalPendingCount() > 0)
                      ? "buffering" : "idle"
                color: accentAmber; font.pixelSize: 11
            }
            Text {
                text: bridge.decoSyncTime
                      ? bridge.decoSyncTime.selfCalLastOffsetMs().toFixed(1) + " ms" : "—"
                color: textPrimary; font.pixelSize: 11
            }
            Text {
                text: bridge.decoSyncTime
                      ? bridge.decoSyncTime.selfCalPendingCount() + " pend." : "—"
                color: textSecondary; font.pixelSize: 11
            }
        }

        Item { Layout.fillHeight: true }

        // ── Sync Now button ──
        Button {
            text: "Sync Now"
            Layout.alignment: Qt.AlignHCenter
            onClicked: if (bridge.decoSyncTime) bridge.decoSyncTime.syncNow()
            background: Rectangle {
                color: parent.hovered ? Qt.rgba(0.23, 0.55, 0.85, 0.35) : Qt.rgba(0.10, 0.20, 0.30, 0.85)
                border.color: accentBlue
                border.width: 1
                radius: 4
            }
            contentItem: Text {
                text: parent.text
                color: textPrimary
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
