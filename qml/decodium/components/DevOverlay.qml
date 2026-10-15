/* Decodium 4.0 - DevOverlay (Sprint 2 Phase 7 perf roadmap)
 * Based on WSJT-X by K1JT et al.
 * By IU8LMC
 *
 * Floating panel con metriche live (frame time, decode rate, RHI backend,
 * delegate count). Toggle: Ctrl+Shift+F. Update rate 4 Hz (250 ms).
 * Zero PII nel snapshot: solo numeri/metriche pure.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: devOverlay
    width: 280
    height: 320

    property var target: typeof bridge !== "undefined" ? bridge : null
    property real appStartMs: Date.now()
    // 1.0.244 fix: Date.now() non e' osservabile in QML -> uptimeSec era
    // calcolato solo al primo eval del binding (subito dopo apertura).
    // _liveTick e' aggiornato da Timer 1s -> forza re-eval ogni secondo.
    property real _liveTick: 0
    readonly property real uptimeSec: {
        var _ = _liveTick  // dependency capture
        return (Date.now() - appStartMs) / 1000.0
    }
    readonly property bool warmingUp: uptimeSec < 5
    // 1.0.243 fix: era hardcoded "1.0.233" -> mostrava versione obsoleta nel
    // snapshot. Usa bridge.version() (Q_INVOKABLE, FORK_RELEASE_VERSION live).
    readonly property string forkVersion: (target && target.version)
        ? target.version() : "?"

    Timer {
        id: liveTickTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: _liveTick = Date.now()
    }

    function fmt(v, digits) {
        if (v === undefined || v === null || isNaN(v)) return "--"
        return Number(v).toFixed(digits === undefined ? 1 : digits)
    }

    function modelCount(m) {
        // 1.0.236 fix: DecodeListModel espone count come Q_INVOKABLE method
        // (non property), quindi va chiamato con parentesi. Vecchio binding
        // ritornava la function reference -> stampava "function() { [native code] }".
        if (!m) return -1
        try {
            if (typeof m.count === "function") return m.count()
            if (typeof m.count === "number") return m.count
        } catch (e) {}
        return -1
    }

    function buildSnapshot() {
        var t = target
        var lines = []
        lines.push("Decodium DevOverlay snapshot v" + forkVersion)
        lines.push("Generated: " + (new Date()).toISOString())
        lines.push("Uptime: " + fmt(uptimeSec, 0) + " s")
        lines.push("")
        lines.push("[Graphics]")
        lines.push("  RHI backend       : " + (t ? t.activeRhiBackend : "n/a"))
        lines.push("")
        lines.push("[Frame time (ms)]")
        var ftAvail = (t && t.lastFrameTimeMs > 0)
        if (ftAvail) {
            lines.push("  last              : " + fmt(t.lastFrameTimeMs, 2))
            lines.push("  mean (32 samples) : " + fmt(t.meanFrameTimeMs, 2))
            lines.push("  p99               : " + fmt(t.p99FrameTimeMs, 2))
        } else {
            lines.push("  (n/a - Qt 6.11 RHI on-demand pipeline doesn't")
            lines.push("   emit frameSwapped/afterRendering after startup)")
        }
        lines.push("")
        lines.push("[Decode rate (Hz)]")
        lines.push("  received          : " + fmt(t ? t.decodeRateReceivedHz : 0, 1))
        lines.push("  committed         : " + fmt(t ? t.decodeRateCommittedHz : 0, 1))
        lines.push("")
        lines.push("[Delegate counters]")
        var dl = (t && t.decodeList) ? t.decodeList.length : 0
        lines.push("  decodeList size   : " + dl)
        var bm = t ? modelCount(t.bandActivityModel) : -1
        var rm = t ? modelCount(t.rxDecodeModel) : -1
        lines.push("  bandActivityModel : " + (bm < 0 ? "n/a" : bm))
        lines.push("  rxDecodeModel     : " + (rm < 0 ? "n/a" : rm))
        lines.push("")
        lines.push("[Cumulative counters (debug)]")
        // 1.0.245: incrementati PRIMA del gate m_devOverlayActive.
        // Se totalFrameSamples > 0 ma mean=0 -> bug downstream QTimer/emit.
        // Se totalFrameSamples == 0 -> connect frameSwapped non funziona.
        lines.push("  totalFrameSamples       : " + (t ? t.totalFrameSamples : 0))
        lines.push("  totalDecodesReceived    : " + (t ? t.totalDecodesReceived : 0))
        lines.push("  totalDecodesCommitted   : " + (t ? t.totalDecodesCommitted : 0))
        lines.push("  devOverlayActive (gate) : " + (t ? t.devOverlayActive : false))
        // 1.0.244: hint quando metrics non popolate. warmingUp = uptime<5s.
        if (warmingUp) {
            lines.push("")
            lines.push("(Note: still warming up uptime=" + fmt(uptimeSec, 0) +
                       "s -- wait at least 10s after Ctrl+Shift+F")
            lines.push(" before clicking Copy diagnostics for meaningful samples.)")
        }
        // Snapshot deliberatamente privo di callsign/grid/freq specifiche (no PII).
        return lines.join("\n")
    }

    // Body panel
    Rectangle {
        id: panel
        anchors.fill: parent
        color: "#CC1A1A2A"
        border.color: "#5DD9C1"
        border.width: 1
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            // Header (drag handle)
            Rectangle {
                id: header
                Layout.fillWidth: true
                height: 22
                color: "#1F2A3A"
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 4
                    spacing: 4
                    Text {
                        text: qsTr("Decodium DevOverlay")
                        color: "#5DD9C1"
                        font.pixelSize: 11
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "x"
                        color: "#FF6464"
                        font.pixelSize: 12
                        font.bold: true
                        rightPadding: 4
                        leftPadding: 4
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (target) target.devOverlayActive = false
                                devOverlay.visible = false
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: 24
                    cursorShape: Qt.SizeAllCursor
                    property real pressX: 0
                    property real pressY: 0
                    onPressed: function(mouse) {
                        pressX = mouse.x
                        pressY = mouse.y
                    }
                    onPositionChanged: function(mouse) {
                        if (pressed) {
                            devOverlay.x += (mouse.x - pressX)
                            devOverlay.y += (mouse.y - pressY)
                        }
                    }
                }
            }

            // Table-like metrics
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 2

                Text { text: "RHI:"; color: "#9AB8C4"; font.pixelSize: 10 }
                Text {
                    text: target ? target.activeRhiBackend : "n/a"
                    color: "#E0E0E0"; font.pixelSize: 10; font.bold: true
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Frame (ms):"); color: "#9AB8C4"; font.pixelSize: 10 }
                Text {
                    // 1.0.252: Qt 6.11 RHI on-demand stoppa frame signals dopo
                    // ~4s startup. Frame timing inaffidabile su Decodium pipeline.
                    text: warmingUp
                          ? "warming up... " + fmt(uptimeSec, 0) + "/5s"
                          : (target && target.lastFrameTimeMs > 0
                             ? (fmt(target.lastFrameTimeMs, 2)
                                + "  mean " + fmt(target.meanFrameTimeMs, 2)
                                + "  p99 " + fmt(target.p99FrameTimeMs, 2))
                             : "n/a (Qt RHI on-demand)")
                    color: warmingUp ? "#7088A0"
                                     : (target && target.lastFrameTimeMs > 0
                                        ? "#E0E0E0" : "#7088A0")
                    font.pixelSize: 10
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Decode Hz:"); color: "#9AB8C4"; font.pixelSize: 10 }
                Text {
                    text: warmingUp
                          ? "warming up..."
                          : ("rx " + fmt(target ? target.decodeRateReceivedHz : 0, 1)
                             + "  ok " + fmt(target ? target.decodeRateCommittedHz : 0, 1))
                    color: warmingUp ? "#7088A0" : "#E0E0E0"
                    font.pixelSize: 10
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Delegates:"); color: "#9AB8C4"; font.pixelSize: 10 }
                Text {
                    // Force binding re-eval ad ogni perfMetricsChanged (250ms tick).
                    property real _tick: target ? target.lastFrameTimeMs : 0
                    text: {
                        // 1.0.236 fix: DecodeListModel.count() e' Q_INVOKABLE method,
                        // non property -- chiamare con parentesi via modelCount().
                        var _ = _tick  // dependency capture per binding re-eval
                        var dl = (target && target.decodeList) ? target.decodeList.length : 0
                        var bm = target ? modelCount(target.bandActivityModel) : -1
                        var rm = target ? modelCount(target.rxDecodeModel) : -1
                        return "list " + dl + "  band " + (bm < 0 ? "?" : bm)
                               + "  rx " + (rm < 0 ? "?" : rm)
                    }
                    color: "#E0E0E0"; font.pixelSize: 10
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Uptime:"); color: "#9AB8C4"; font.pixelSize: 10 }
                Text {
                    // 1.0.244 fix: binding rilegge ad ogni Timer 1s tick.
                    text: fmt(uptimeSec, 0) + " s"
                    color: "#E0E0E0"; font.pixelSize: 10
                    Layout.fillWidth: true
                }
            }

            // Sparkline frame time (32 samples)
            Rectangle {
                id: sparkBox
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#0E1620"
                border.color: "#2A3848"
                border.width: 1
                radius: 4

                Text {
                    text: qsTr("Frame time 5s")
                    color: "#7088A0"
                    font.pixelSize: 9
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 4
                }

                Canvas {
                    id: spark
                    anchors.fill: parent
                    anchors.margins: 4
                    anchors.topMargin: 14

                    function paintSparkline() {
                        var ctx = getContext("2d")
                        if (!ctx) return
                        ctx.reset()
                        var samples = (target && target.frameTimeSamples)
                                      ? target.frameTimeSamples : []
                        if (!samples || samples.length === 0) return
                        var w = width
                        var h = height
                        var maxV = 1.0
                        for (var i = 0; i < samples.length; ++i) {
                            if (samples[i] > maxV) maxV = samples[i]
                        }
                        // Clamp scale per leggibilita' (target 16.6 ms = 60Hz)
                        if (maxV < 20) maxV = 20
                        var n = samples.length
                        var dx = n > 1 ? w / (n - 1) : 0
                        ctx.strokeStyle = "#5DD9C1"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        for (var j = 0; j < n; ++j) {
                            var v = samples[j]
                            var y = h - (v / maxV) * h
                            if (j === 0) ctx.moveTo(0, y)
                            else ctx.lineTo(j * dx, y)
                        }
                        ctx.stroke()
                        // 16.6 ms reference line (60Hz target)
                        ctx.strokeStyle = "#444A55"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        var yRef = h - (16.6 / maxV) * h
                        ctx.moveTo(0, yRef)
                        ctx.lineTo(w, yRef)
                        ctx.stroke()
                    }

                    onPaint: paintSparkline()
                }

                Connections {
                    target: devOverlay.target
                    function onPerfMetricsChanged() { spark.requestPaint() }
                }
            }

            // Buttons row
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    text: qsTr("Copy diagnostics")
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    onClicked: {
                        var snap = devOverlay.buildSnapshot()
                        // Helper Q_INVOKABLE su bridge (Qt 6: no Qt.application.clipboard).
                        if (target && target.copyToClipboard) {
                            target.copyToClipboard(snap)
                        } else if (target && target.qmlDebugLog) {
                            target.qmlDebugLog("[DevOverlay] snapshot:\n" + snap)
                        }
                        copyFeedback.opacity = 1
                        copyFeedbackTimer.restart()
                    }
                }

                Button {
                    text: qsTr("Close")
                    Layout.preferredWidth: 64
                    font.pixelSize: 10
                    onClicked: {
                        if (target) target.devOverlayActive = false
                        devOverlay.visible = false
                    }
                }
            }

            Text {
                id: copyFeedback
                text: qsTr("Copied to clipboard")
                color: "#5DD9C1"
                font.pixelSize: 9
                opacity: 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
            Timer {
                id: copyFeedbackTimer
                interval: 1500
                onTriggered: copyFeedback.opacity = 0
            }
        }
    }
}
