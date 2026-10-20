/* Decodium 4.0 Core Gallager — Splash Screen
 * Schermata di avvio con logo, versione e animazione di caricamento.
 * Si chiude automaticamente dopo splashDuration ms oppure al click.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: splashRoot

    property int splashDuration: 10000   // 1.0.310: resta 10s (o "Avvia" per proseguire)
    property color bgDeep:        "#0a0e1a"
    property color primaryBlue:   "#1a73e8"
    property color secondaryCyan: "#00bcd4"
    property color accentGreen:   "#00e676"
    property color textPrimary:   "#e8eaf6"
    property color textSecondary: "#8899aa"

    signal finished()

    color: bgDeep
    z:     9999

    // 1.0.310: niente più click-anywhere — si prosegue col pulsante "Avvia"
    // (così la splash resta i 10s pieni senza chiusure accidentali)

    // Timer auto-close
    Timer {
        id: autoClose
        interval: splashDuration
        running: splashRoot.visible
        repeat: false
        onTriggered: splashRoot.close()
    }

    function close() {
        fadeOut.start()
    }

    NumberAnimation {
        id: fadeOut
        target: splashRoot
        property: "opacity"
        from: 1.0; to: 0.0
        duration: 400
        easing.type: Easing.InQuad
        onFinished: splashRoot.finished()
    }

    // ── Contenuto centrale ──────────────────────────────────────────────────
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0

        // Icona / logo circolare
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 140; height: 140; radius: 70
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1a73e8" }
                GradientStop { position: 1.0; color: "#00bcd4" }
            }

            // Bordo luminoso pulsante
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.color: Qt.rgba(0, 0.74, 0.83, 0.6)
                border.width: 3

                SequentialAnimation on opacity {
                    running: splashRoot.visible; loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 900 }
                    NumberAnimation { to: 1.0; duration: 900 }
                }
            }

            // Prova a caricare l'icona; fallback sulla lettera "D"
            Image {
                id: iconImg
                anchors.centerIn: parent
                width: 96; height: 96
                source: "qrc:/icon_128x128.png"
                fillMode: Image.PreserveAspectFit
                smooth: true; mipmap: true
                visible: status === Image.Ready
            }

            Text {
                anchors.centerIn: parent
                text: "D"
                font.pixelSize: 80; font.bold: true
                color: "white"
                visible: iconImg.status !== Image.Ready
            }
        }

        Item { height: 28 }

        // Nome applicazione
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "DECODIUM"
            font.pixelSize: 48; font.bold: true; font.letterSpacing: 6
            color: textPrimary
        }

        Item { height: 6 }

        // Versione e codename
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Rectangle {
                width: 44; height: 22; radius: 11
                color: Qt.rgba(0, 0.74, 0.83, 0.25)
                border.color: secondaryCyan; border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "4.0"
                    font.pixelSize: 12; font.bold: true
                    color: secondaryCyan
                }
            }

            Text {
                text: qsTr("Core Gallager")
                font.pixelSize: 16; font.italic: true
                color: secondaryCyan
            }
        }

        Item { height: 14 }

        // Tagline
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Advanced Digital Mode Decoder for Amateur Radio")
            font.pixelSize: 13
            color: textSecondary
        }

        Item { height: 50 }

        // Barra di caricamento
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 320; height: 3; radius: 2
            color: Qt.rgba(1, 1, 1, 0.08)

            Rectangle {
                id: progressBar
                height: parent.height; radius: parent.radius
                color: secondaryCyan
                width: 0

                NumberAnimation on width {
                    from: 0; to: 320
                    duration: splashRoot.splashDuration - 200
                    easing.type: Easing.InOutQuad
                    running: splashRoot.visible
                }

                // Riflesso scorrevole
                Rectangle {
                    width: 60; height: parent.height
                    radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: Qt.rgba(1,1,1,0.5) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                    NumberAnimation on x {
                        from: -60; to: 320
                        duration: splashRoot.splashDuration - 200
                        easing.type: Easing.InOutQuad
                        running: splashRoot.visible
                    }
                }
            }
        }

        Item { height: 12 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Loading…")
            font.pixelSize: 11
            color: Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.6)
        }

        Item { height: 26 }

        // 1.0.310: pulsanti — Offrimi un caffè + Avvia (prosegui subito)
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 14

            // ☕ Buy me a coffee → buymeacoffee.com/iu8lmc
            Rectangle {
                width: 200; height: 38; radius: 8
                color: bmacMA.containsMouse ? Qt.rgba(1, 0.86, 0.25, 0.32) : Qt.rgba(1, 0.86, 0.25, 0.16)
                border.color: "#FFD740"; border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    anchors.centerIn: parent
                    text: qsTr("☕  Buy me a coffee")
                    font.pixelSize: 13; font.bold: true
                    color: "#FFE082"
                }
                MouseArea {
                    id: bmacMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.openUrlExternally("https://buymeacoffee.com/iu8lmc")
                }
            }

            // ▶ Avvia → chiude la splash e prosegue
            Rectangle {
                width: 140; height: 38; radius: 8
                color: startMA.containsMouse ? Qt.rgba(0, 0.74, 0.83, 0.45) : Qt.rgba(0, 0.74, 0.83, 0.26)
                border.color: secondaryCyan; border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Start  ▶")
                    font.pixelSize: 13; font.bold: true
                    color: textPrimary
                }
                MouseArea {
                    id: startMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: splashRoot.close()
                }
            }
        }
    }

    // ── Credits in basso ────────────────────────────────────────────────────
    ColumnLayout {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 24 }
        spacing: 4

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Named after Robert G. Gallager · Father of LDPC Codes")
            font.pixelSize: 10; font.italic: true
            color: Qt.rgba(0.53, 0.6, 0.67, 0.7)
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("© 2024-2026 IU8LMC  ·  GPL Open Source  ·  Based on WSJT-X by K1JT et al. & WSJT-X by K1JT")
            font.pixelSize: 10
            color: Qt.rgba(0.53, 0.6, 0.67, 0.5)
        }
    }

    // Entrata con fade-in
    opacity: 0
    NumberAnimation on opacity {
        from: 0; to: 1
        duration: 350
        easing.type: Easing.OutQuad
        running: splashRoot.visible
    }
}
