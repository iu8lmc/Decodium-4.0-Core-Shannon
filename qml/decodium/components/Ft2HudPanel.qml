// Ft2HudPanel.qml — Heads-up display per il sequencer FT2 single-pipeline.
//
// Mostra in un solo pannello tutto il contesto necessario nei 3.75s di slot:
//   - Stato corrente dell'engine (Idle / CallingCq / AwaitingReport / ...)
//   - DX call attuale in caratteri grandi
//   - Barra countdown del periodo (ricavata da bridge.periodProgress)
//   - Anteprima del prossimo messaggio TX con pulsante CANCEL
//   - Caller queue interattiva (click engage, ✕ skip)
//
// Tutto bindato alle Q_PROPERTY del Bridge; nessuna logica locale.
// Visibile solo quando bridge.mode === "FT2".

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: hud

    color: Qt.rgba(0, 0, 0, 0.82)
    border.color: Qt.rgba(0, 230, 118, 0.55)
    border.width: 1
    radius: 8

    implicitWidth: 320
    implicitHeight: headerRow.height + bodyCol.height + 18

    signal closeRequested()

    // ----- Theming (con fallback se themeManager non è inizializzato) -----
    property color accent:        bridge.themeManager ? bridge.themeManager.accentColor    : "#00e676"
    property color cyan:          bridge.themeManager ? bridge.themeManager.secondaryColor : "#00bcd4"
    property color textPrimary:   bridge.themeManager ? bridge.themeManager.textPrimary    : "#ECEFF1"
    property color textSecondary: bridge.themeManager ? bridge.themeManager.textSecondary  : "#90A4AE"
    property color warning:       bridge.themeManager ? bridge.themeManager.warningColor   : "#FF9800"
    property color danger:        bridge.themeManager ? bridge.themeManager.ledRed         : "#F44336"

    // ----- Engine state derivati -----
    readonly property string engineState: bridge.ft2State || "—"
    readonly property var    callers:     bridge.ft2Callers || []
    readonly property string dx:          bridge.dxCall || ""
    readonly property int    txNum:       bridge.currentTx
    readonly property string nextTx:      bridge.currentTxMessage || ""
    readonly property int    progress:    bridge.periodProgress
    readonly property int    progressMax: 15  // FT2 = 15 tick (3.75s @ 4Hz)

    // ----- Slot phase (0..1) — usata da bar e ring countdown -----
    readonly property real   slotPhase: progressMax > 0 ? Math.min(1, progress / progressMax) : 0
    readonly property real   slotRemainingS: Math.max(0, 3.75 * (1 - slotPhase))

    // ----- Colore stato (semaforo) -----
    readonly property color stateColor: {
        switch (engineState) {
        case "Idle":            return textSecondary
        case "CallingCq":       return cyan
        case "ReplyingTx1":
        case "AwaitingReport":
        case "AwaitingRRR":
        case "AwaitingSignoff": return accent
        case "Closing":         return warning
        default:                return textSecondary
        }
    }

    // ====== Header ======
    RowLayout {
        id: headerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 8
        height: 22

        Text {
            text: "FT2 HUD"
            font.family: "Monospace"
            font.pixelSize: 11
            font.bold: true
            color: accent
        }

        // Pallino stato
        Rectangle {
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: 4
            color: stateColor
        }

        Text {
            text: engineState
            font.family: "Monospace"
            font.pixelSize: 11
            color: stateColor
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.preferredWidth: 38
            Layout.preferredHeight: 18
            radius: 3
            color: txNum > 0 ? Qt.rgba(warning.r, warning.g, warning.b, 0.25) : "transparent"
            border.color: txNum > 0 ? warning : Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.4)
            Text {
                anchors.centerIn: parent
                text: txNum > 0 ? ("TX" + txNum) : "—"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
                color: txNum > 0 ? warning : textSecondary
            }
        }

        // LED ON-AIR: rosso pulsante quando trasmettiamo, grigio in RX.
        // Permette di leggere lo stato TX/RX a colpo d'occhio senza guardare
        // la barra di stato del backend.
        Rectangle {
            Layout.preferredWidth: 12
            Layout.preferredHeight: 12
            radius: 6
            color: bridge.transmitting ? danger : Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.20)
            border.color: bridge.transmitting ? danger : Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.45)
            border.width: 1
            SequentialAnimation on opacity {
                running: bridge.transmitting
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.45; duration: 380; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.45; to: 1.0; duration: 380; easing.type: Easing.InOutSine }
            }
        }

        // Close panel button
        Rectangle {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            radius: 2
            color: closeMa.containsMouse ? Qt.rgba(1,1,1,0.1) : "transparent"
            Text {
                anchors.centerIn: parent
                text: "✕"
                font.pixelSize: 10
                color: textSecondary
            }
            MouseArea {
                id: closeMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: hud.closeRequested()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerRow.bottom
        anchors.topMargin: 4
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        height: 1
        color: Qt.rgba(0, 230, 118, 0.25)
    }

    // ====== Body ======
    ColumnLayout {
        id: bodyCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerRow.bottom
        anchors.topMargin: 12
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        // ----- DX call grande + countdown -----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "DX"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    color: textSecondary
                }
                Text {
                    text: hud.dx === "" ? "—" : hud.dx
                    font.family: "Monospace"
                    font.pixelSize: 22
                    font.bold: true
                    color: hud.dx === "" ? textSecondary : accent
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: hud.slotRemainingS.toFixed(1) + "s"
                    font.family: "Monospace"
                    font.pixelSize: 11
                    color: textSecondary
                }
            }

            // Slot countdown bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: Qt.rgba(255,255,255,0.08)

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * (1 - hud.slotPhase)
                    radius: 2
                    color: hud.slotPhase > 0.85 ? danger
                         : hud.slotPhase > 0.6  ? warning
                         : accent
                    Behavior on width { NumberAnimation { duration: 200 } }
                }
            }
        }

        // ----- Next TX preview + cancel -----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            radius: 4
            color: Qt.rgba(255, 152, 0, 0.10)
            border.color: Qt.rgba(255, 152, 0, 0.45)
            visible: hud.nextTx !== ""

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 4
                spacing: 6

                Text {
                    text: "TX▶"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    color: warning
                }
                Text {
                    text: hud.nextTx
                    font.family: "Monospace"
                    font.pixelSize: 11
                    color: textPrimary
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 20
                    radius: 3
                    color: cancelMa.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.45)
                                                   : Qt.rgba(244/255, 67/255, 54/255, 0.18)
                    border.color: danger
                    Text {
                        anchors.centerIn: parent
                        text: "CANCEL"
                        font.family: "Monospace"
                        font.pixelSize: 9
                        font.bold: true
                        color: danger
                    }
                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bridge.ft2CancelNextTx()
                    }
                }
            }
        }

        // ----- TX latency badge -----
        // Microsecondi tra arrivo decode e emit txMessageRequested.
        // Verde < 50ms = ottimo, giallo 50-100, arancio 100-200, rosso > 200ms
        // (oltre 200ms in FT2 il primo TX dello slot rischia di essere troncato).
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: bridge.ft2AvgTxLatencyUs > 0

            Text {
                text: "LAT"
                font.family: "Monospace"
                font.pixelSize: 9
                font.bold: true
                color: textSecondary
            }
            Rectangle {
                id: latBar
                Layout.fillWidth: true
                Layout.preferredHeight: 18
                radius: 3
                clip: true
                readonly property real avgMs: bridge.ft2AvgTxLatencyUs / 1000.0
                readonly property real maxMs: bridge.ft2MaxTxLatencyUs / 1000.0
                readonly property color tone:
                    avgMs > 200 ? danger
                  : avgMs > 100 ? warning
                  : avgMs >  50 ? Qt.rgba(1, 1, 0.4, 1)
                  : accent
                color: Qt.rgba(tone.r, tone.g, tone.b, 0.10)
                border.color: tone

                // Fill gauge: larghezza proporzionale a avgMs (saturazione a 200ms,
                // soglia oltre cui il primo TX dello slot rischia di essere troncato).
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * Math.min(latBar.avgMs / 200.0, 1.0)
                    radius: 3
                    color: Qt.rgba(latBar.tone.r, latBar.tone.g, latBar.tone.b, 0.40)
                    Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 4
                    spacing: 8

                    Text {
                        text: "avg " + latBar.avgMs.toFixed(1) + "ms"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                        color: latBar.tone
                    }
                    Text {
                        text: "max " + latBar.maxMs.toFixed(1) + "ms"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        color: textSecondary
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 14
                        radius: 2
                        color: latResetMa.containsMouse ? Qt.rgba(1,1,1,0.15) : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: "↺"
                            font.pixelSize: 10
                            color: textSecondary
                        }
                        MouseArea {
                            id: latResetMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: bridge.ft2ResetLatencyStats()
                        }
                    }
                }
            }
        }

        // ----- Pipeline segment telemetry (Tier 1) -----
        // Mostra le tre tappe pre-engine/post-engine misurate dal bridge:
        //   DEC = stage7 native decoder (durata Fortran-style C++ call)
        //   QUE = cross-thread queue delay (worker emit → bridge slot entry)
        //   ENC = genStdMsgs encoder (TX1..TX6 message format)
        // La latenza dispatch interna engine resta nel badge LAT sopra.
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: bridge.ft2AvgDecoderUs > 0 || bridge.ft2AvgQueueUs > 0 || bridge.ft2AvgEncoderUs > 0

            Text {
                text: "PIPE"
                font.family: "Monospace"
                font.pixelSize: 9
                font.bold: true
                color: textSecondary
            }
            Repeater {
                model: [
                    { label: "DEC", avgUs: bridge.ft2AvgDecoderUs, maxUs: bridge.ft2MaxDecoderUs, sat: 200.0 },
                    { label: "QUE", avgUs: bridge.ft2AvgQueueUs,   maxUs: bridge.ft2MaxQueueUs,   sat:  20.0 },
                    { label: "ENC", avgUs: bridge.ft2AvgEncoderUs, maxUs: bridge.ft2MaxEncoderUs, sat:  20.0 }
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    radius: 3
                    clip: true
                    readonly property real avgMs: modelData.avgUs / 1000.0
                    readonly property real maxMs: modelData.maxUs / 1000.0
                    readonly property real sat:   modelData.sat
                    readonly property color tone:
                        avgMs > sat        ? danger
                      : avgMs > sat * 0.5  ? warning
                      : avgMs > sat * 0.25 ? Qt.rgba(1, 1, 0.4, 1)
                      : accent
                    color: Qt.rgba(tone.r, tone.g, tone.b, 0.10)
                    border.color: tone

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * Math.min(parent.avgMs / parent.sat, 1.0)
                        radius: 3
                        color: Qt.rgba(parent.tone.r, parent.tone.g, parent.tone.b, 0.40)
                        Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label + " " + parent.avgMs.toFixed(1)
                        font.family: "Monospace"
                        font.pixelSize: 9
                        font.bold: true
                        color: parent.tone
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: "↑" + parent.maxMs.toFixed(1)
                        font.family: "Monospace"
                        font.pixelSize: 8
                        color: textSecondary
                    }
                }
            }
        }

        // ----- Caller queue header -----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "CALLERS"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
                color: cyan
            }
            Rectangle {
                Layout.preferredWidth: 22; Layout.preferredHeight: 14
                radius: 7
                color: Qt.rgba(0, 188, 212, 0.25)
                Text {
                    anchors.centerIn: parent
                    text: hud.callers.length
                    font.family: "Monospace"
                    font.pixelSize: 9
                    font.bold: true
                    color: cyan
                }
            }
            Item { Layout.fillWidth: true }
            // Bottone CLEAR: scarica tutta la queue in un colpo (utile dopo
            // un pile-up se l'operatore vuole tornare al CQ pulito).
            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 14
                radius: 2
                visible: hud.callers.length > 0
                color: clearAllMa.containsMouse ? Qt.rgba(danger.r, danger.g, danger.b, 0.25)
                                                : Qt.rgba(danger.r, danger.g, danger.b, 0.10)
                border.color: Qt.rgba(danger.r, danger.g, danger.b, 0.55)
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "CLEAR"
                    font.family: "Monospace"
                    font.pixelSize: 8
                    font.bold: true
                    color: danger
                }
                MouseArea {
                    id: clearAllMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bridge.ft2ClearQueue()
                }
            }
            Text {
                text: "click=engage  ✕=skip"
                font.family: "Monospace"
                font.pixelSize: 8
                color: textSecondary
            }
        }

        // ----- Caller list -----
        ListView {
            id: callerList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(120, contentHeight)
            clip: true
            spacing: 2
            model: hud.callers

            delegate: Rectangle {
                width: callerList.width
                height: 22
                radius: 3
                property bool isHovered: rowMa.containsMouse
                color: isHovered ? Qt.rgba(0, 230, 118, 0.18)
                     : (modelData.direct ? Qt.rgba(0, 230, 118, 0.08) : "transparent")
                border.color: modelData.direct ? Qt.rgba(0, 230, 118, 0.4) : "transparent"
                border.width: 1

                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    onClicked: bridge.ft2PromoteCaller(modelData.call)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.rightMargin: 4
                    spacing: 6

                    Text {
                        text: (index + 1) + "."
                        font.family: "Monospace"
                        font.pixelSize: 9
                        color: textSecondary
                        Layout.preferredWidth: 16
                    }
                    Text {
                        text: modelData.call
                        font.family: "Monospace"
                        font.pixelSize: 11
                        font.bold: true
                        color: index === 0 ? accent : textPrimary
                        Layout.preferredWidth: 76
                    }
                    Text {
                        text: (modelData.snr >= 0 ? "+" : "") + modelData.snr
                        font.family: "Monospace"
                        font.pixelSize: 10
                        color: modelData.snr >= -10 ? accent
                             : modelData.snr >= -18 ? warning
                             : danger
                        Layout.preferredWidth: 32
                        horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: modelData.grid !== "" ? modelData.grid : "----"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        color: textSecondary
                        Layout.preferredWidth: 38
                    }
                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        radius: 2
                        color: skipMa.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.35) : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            font.pixelSize: 10
                            color: skipMa.containsMouse ? danger : textSecondary
                        }
                        MouseArea {
                            id: skipMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: bridge.ft2SkipCaller(modelData.call)
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: callerList.count === 0
                text: "Nessun caller in coda"
                font.family: "Monospace"
                font.pixelSize: 10
                color: textSecondary
            }
        }
    }
}
