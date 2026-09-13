// DecoRTTY — title bar: identity, radio state, frequency and the QSO fields.
import QtQuick
import QtQuick.Controls

import "."

Rectangle {
    id: root

    signal settingsRequested()
    signal logRequested()

    height: 50
    color: Qt.rgba(Theme.bgMedium.r, Theme.bgMedium.g, Theme.bgMedium.b, 0.92)

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.glassBorder
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                text: "DecoRTTY"
                color: Theme.textPrimary
                font.pixelSize: 17
                font.bold: true
                font.letterSpacing: 0.5
            }
            Text {
                text: qsTr("RTTY")
                color: Theme.secondary
                font.pixelSize: 9
                font.letterSpacing: 1.2
            }
        }

        Rectangle { width: 1; height: 34; color: Theme.glassBorder
                    anchors.verticalCenter: parent.verticalCenter }

        // ── radio state ─────────────────────────────────────────────────
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 9; height: 9; radius: 4.5
                color: radio.connected ? Theme.accent : Theme.textSecondary
                opacity: radio.connected ? 1 : 0.4

            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                Text {
                    text: radio.connected ? radio.radioName : "No radio"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }
                Text {
                    text: radio.statusText
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    width: 210
                }
            }
        }

        Rectangle { width: 1; height: 34; color: Theme.glassBorder
                    anchors.verticalCenter: parent.verticalCenter }

        // ── frequency and mode ──────────────────────────────────────────
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                text: radio.connected ? radio.frequencyMhz.toFixed(6) + " MHz" : "—.——————"
                color: radio.connected ? Theme.secondary : Theme.textSecondary
                font.pixelSize: 18
                font.family: Theme.monoFamily
            }
            Text {
                text: (radio.mode.length > 0 ? radio.mode : "—")
                      + "   S " + (radio.signalStrengthDbm > -200 ? radio.signalStrengthDbm + " dBm" : "—")
                color: Theme.textSecondary
                font.pixelSize: 10
                font.family: Theme.monoFamily
            }
        }
    }

    // ── QSO fields ──────────────────────────────────────────────────────
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        QsoField {
            label: "HIS CALL"
            width: 118
            text: macros.hisCall
            onEdited: (value) => macros.hisCall = value
            highlight: true
        }

        QsoField {
            label: "RST"
            width: 58
            text: macros.rstSent
            onEdited: (value) => macros.rstSent = value
        }

        QsoField {
            label: "NR"
            width: 58
            text: String(macros.serialNumber)
            onEdited: (value) => macros.serialNumber = parseInt(value) || 1
        }

        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsoLog.count > 0 ? "Log " + qsoLog.count : "Log"
            accentColor: Theme.accent
            minimumWidth: 72
            onClicked: root.logRequested()
        }

        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Setup")
            minimumWidth: 68
            onClicked: root.settingsRequested()
        }
    }
}
