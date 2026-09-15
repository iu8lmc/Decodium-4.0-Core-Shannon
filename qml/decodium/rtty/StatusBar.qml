// DecoRTTY — the bottom rail: everything about how well the link is working.
import QtQuick

import "."

Rectangle {
    id: root

    height: 26
    color: Qt.rgba(Theme.bgMedium.r, Theme.bgMedium.g, Theme.bgMedium.b, 0.92)

    // Una sintonia che si sposta da sola, senza dirlo, si legge come un guasto.
    // Il messaggio resta il tempo di accorgersene e poi si toglie di mezzo.
    property string notice: ""

    Connections {
        target: rtty
        function onAutoTuned(message) {
            root.notice = message
            noticeTimer.restart()
        }
    }

    Timer {
        id: noticeTimer
        interval: 6000
        onTriggered: root.notice = ""
    }

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.glassBorder
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18

        StatusItem {
            label: "LOCK"
            value: rtty.locked ? "yes" : "no"
            valueColor: rtty.locked ? Theme.accent : Theme.textSecondary
        }
        StatusItem {
            label: "TONE"
            value: rtty.toneSeparationDb.toFixed(1) + " dB"
            valueColor: rtty.toneSeparationDb >= rtty.squelchDb ? Theme.accent : Theme.textSecondary
        }
        StatusItem { label: "S/N"; value: rtty.snrDb.toFixed(1) + " dB" }
        StatusItem {
            label: "AFC"
            value: (rtty.afcOffsetHz >= 0 ? "+" : "") + rtty.afcOffsetHz.toFixed(1) + " Hz"
            valueColor: Math.abs(rtty.afcOffsetHz) > 40 ? Theme.warning : Theme.textPrimary
        }
        StatusItem { label: "BAUD"; value: rtty.measuredBaud.toFixed(2) }
        StatusItem { label: "FEC"; value: rtty.correctionDepth > 0 ? rtty.correctionDepth + " ch" : "off" }
    }

    Text {
        anchors.centerIn: parent
        text: root.notice
        color: Theme.success
        font.pixelSize: 11
        opacity: root.notice.length ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        // La radio e' sempre quella di Decodium: non c'e' piu' un'altra
        // sorgente da cui distinguerla, e dirlo una volta basta.
        text: qsTr("Decodium's radio") + "   ·   v" + appVersion
        color: Theme.textSecondary
        font.pixelSize: 10
    }
}
