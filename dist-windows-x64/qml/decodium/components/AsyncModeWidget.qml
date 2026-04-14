// AsyncModeWidget.qml — onda sinusoidale animata per FT2 Async mode
// Replica funzionale di widgets/asyncmodewidget.cpp per l'interfaccia QML
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    width:  90
    height: 50

    // Proprietà esposte
    property bool  running:      false
    property bool  transmitting: false
    property int   snr:          -99     // -99 = nessun segnale
    property bool  showWave:     true
    property bool  showMeter:    true
    property bool  hovered:      asyncHover.containsMouse

    // Fase animata (0 … 2π)
    property real _phase: 0.0

    NumberAnimation on _phase {
        id: phaseAnim
        from: 0
        to:   2 * Math.PI
        duration: 1000
        loops: Animation.Infinite
        running: root.running
    }

    // Colore corrente: rosso TX, verde ASYNC
    readonly property color _waveColor: root.transmitting ? "#ff4444" : "#00e676"

    // ── Sfondo ──────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color:        "#141422"
        border.color: "#333344"
        border.width: 1
        radius:       4
        clip:         true

        // ── Etichetta stato ─────────────────────────────────────────────────
        Text {
            id: stateLabel
            anchors.top:              parent.top
            anchors.topMargin:        2
            anchors.horizontalCenter: parent.horizontalCenter
            text:  root.running ? (root.transmitting ? "TX" : "ASYNC") : "FT2"
            color: root.running
                   ? (root.transmitting ? "#ff4444" : "#00e676")
                   : "#444444"
            font.family:    "Segoe UI"
            font.pixelSize: 8
            font.bold:      true
        }

        // ── Canvas onda sinusoidale ─────────────────────────────────────────
        Canvas {
            id: waveCanvas
            anchors.top:         stateLabel.bottom
            anchors.topMargin:   1
            anchors.left:        parent.left
            anchors.leftMargin:  2
            anchors.right:       parent.right
            anchors.rightMargin: 2
            height:              root.showMeter
                                 ? parent.height - stateLabel.height - 18
                                 : parent.height - stateLabel.height - 4
            visible: root.showWave && root.running

            onPaint: {
                var ctx    = getContext("2d")
                var w      = width
                var h      = height
                var midY   = h / 2
                var amp    = h * 0.42
                var color  = root.transmitting ? "#ff4444" : "#00e676"
                var phase  = root._phase

                ctx.clearRect(0, 0, w, h)

                // Riempimento sotto la curva (semitrasparente)
                ctx.beginPath()
                ctx.fillStyle = root.transmitting
                               ? "rgba(255,68,68,0.10)"
                               : "rgba(0,230,118,0.10)"
                for (var x = 1; x < w; x++) {
                    var t = x / w
                    var y = midY - amp * Math.sin(2 * Math.PI * 2.5 * t + phase)
                    if (x === 1) ctx.moveTo(x, y)
                    else         ctx.lineTo(x, y)
                }
                ctx.lineTo(w - 1, midY)
                ctx.lineTo(1,     midY)
                ctx.closePath()
                ctx.fill()

                // Curva sinusoidale
                ctx.beginPath()
                ctx.strokeStyle = color
                ctx.lineWidth   = 1.5
                for (var x2 = 1; x2 < w; x2++) {
                    var t2 = x2 / w
                    var y2 = midY - amp * Math.sin(2 * Math.PI * 2.5 * t2 + phase)
                    if (x2 === 1) ctx.moveTo(x2, y2)
                    else          ctx.lineTo(x2, y2)
                }
                ctx.stroke()
            }

            // Rideseña ogni volta che la fase cambia
            Connections {
                target: root
                function on_PhaseChanged() { waveCanvas.requestPaint() }
            }
            Connections {
                target: root
                function onTransmittingChanged() { waveCanvas.requestPaint() }
            }
        }

        // ── S-Meter ──────────────────────────────────────────────────────────
        Item {
            id: meterArea
            anchors.bottom:       parent.bottom
            anchors.bottomMargin: 2
            anchors.left:         parent.left
            anchors.leftMargin:   2
            anchors.right:        parent.right
            anchors.rightMargin:  2
            height:               14
            visible:              root.showMeter

            // Valore dB
            Text {
                id: dbLabel
                anchors.top:              parent.top
                width:                    parent.width
                horizontalAlignment:      Text.AlignHCenter
                text:  root.snr <= -99 ? "--- dB" : (root.snr + " dB")
                color: "white"
                font.family:    "Segoe UI"
                font.pixelSize: 6
            }

            // Barra del segnale (gradiante verde→giallo→rosso da sx a dx
            // ma "riempita" da sx: segnale debole = rosso, forte = verde)
            Rectangle {
                id: meterBg
                anchors.bottom: parent.bottom
                width:          parent.width
                height:         5
                color:          "#282828"
                radius:         2

                Rectangle {
                    id: meterBar
                    anchors.left:   parent.left
                    anchors.top:    parent.top
                    anchors.bottom: parent.bottom
                    radius:         2
                    width: {
                        // SNR_MIN=-28, SNR_MAX=10 → range 38 dB
                        var norm = Math.max(0, Math.min(1, (root.snr + 28) / 38))
                        return Math.max(2, norm * parent.width)
                    }
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0;  color: "#ff3333" }
                        GradientStop { position: 0.4;  color: "#ffcc00" }
                        GradientStop { position: 0.7;  color: "#66ff33" }
                        GradientStop { position: 1.0;  color: "#00ff88" }
                    }
                }
            }
        }
    }

    // Context menu via click destro (toggle onda/meter)
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: ctxMenu.popup()
    }

    Menu {
        id: ctxMenu
        MenuItem {
            text:      "Onda sinusoidale"
            checkable: true
            checked:   root.showWave
            onTriggered: root.showWave = !root.showWave
        }
        MenuItem {
            text:      "S-Meter"
            checkable: true
            checked:   root.showMeter
            onTriggered: root.showMeter = !root.showMeter
        }
    }

    MouseArea {
        id: asyncHover
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
}
