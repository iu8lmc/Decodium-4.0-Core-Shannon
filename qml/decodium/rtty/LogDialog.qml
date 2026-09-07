// DecoRTTY — registro dei collegamenti.
//
// Il collegamento si registra dai campi che sono già compilati in alto, così
// annotarlo costa un tasto e non una trascrizione. Il file ADIF si riscrive a
// ogni riga: un programma che si chiude male non porta via la serata.
import QtQuick
import QtQuick.Controls

import "."

Dialog {
    id: root

    modal: true
    visible: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    // Mai piu' grande della finestra che lo ospita. Con una misura fissa un
    // dialogo piu' alto della finestra usciva dai bordi, e quello che si vedeva
    // intorno non era piu' RTTY: era il velo dell'overlay steso sopra Decodium.
    // Si misura sull'overlay, che copre l'area della finestra ed e' lo stesso
    // su cui il dialogo si centra. Non su Window.window: un Dialog non deriva
    // da Item e quella proprieta' li' non vale.
    width: Math.min(720, Overlay.overlay ? Overlay.overlay.width - 24 : 720)
    height: Math.min(520, Overlay.overlay ? Overlay.overlay.height - 24 : 520)
    padding: 0

    background: GlassPanel { tintOpacity: 0.97 }

    // Il contenuto scorre. Con un'altezza fissa quello che non entrava
    // spariva e basta: su uno schermo con la scalatura di Windows al 150% i
    // punti disponibili sono molti meno di quelli che il conto sulla carta
    // suggerisce, e la parte in fondo di questa finestra non si vedeva —
    // senza nemmeno una barra a dire che c'era dell'altro.
    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Column {
        width: parent.width
        spacing: 10

        Item { width: 1; height: 6 }

        Row {
            x: 16
            width: parent.width - 32
            spacing: 10

            PanelHeading {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("LOG")
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%n contact(s)", "", qsoLog.count)
                color: Theme.textSecondary
                font.pixelSize: 11
            }
            Item { width: parent.width - 420; height: 1 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsoLog.logPath
                color: Theme.textSecondary
                font.pixelSize: 9
                elide: Text.ElideLeft
                width: 260
            }
        }

        // ── riga di registrazione ───────────────────────────────────────
        Rectangle {
            x: 12
            width: parent.width - 24
            height: 58
            radius: 8
            color: Theme.glassOverlay
            border.color: Theme.glassBorder

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                QsoField {
                    label: "CALL"
                    width: 110
                    text: macros.hisCall
                    highlight: true
                    onEdited: (value) => macros.hisCall = value
                }
                QsoField {
                    label: qsTr("RST SENT")
                    width: 60
                    text: macros.rstSent
                    onEdited: (value) => macros.rstSent = value
                }
                QsoField {
                    id: rstRcvd
                    label: qsTr("RST RCVD")
                    width: 60
                    text: "599"
                }
                QsoField { id: opName; label: qsTr("NAME"); width: 100 }
                QsoField { id: opQth;  label: "QTH";  width: 110 }

                GlassButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Log it")
                    accentColor: Theme.success
                    minimumWidth: 92
                    implicitHeight: 30
                    enabled: macros.hisCall.length > 0
                    onClicked: {
                        // Il collegamento entra nel log di Decodium, non in un
                        // archivio di questa finestra: di la' ci sono il libro
                        // attivo, l'ADIF e gli instradamenti verso QRZ, eQSL,
                        // HRDLog, Club Log e PSK Reporter. La lista qui sotto
                        // resta come promemoria della sessione — comoda per
                        // vedere a colpo d'occhio chi si e' gia' lavorato — ma
                        // il collegamento vero e' quello registrato di la'.
                        var registrato = bridge
                            ? bridge.registraQsoRtty(macros.hisCall, macros.rstSent,
                                                     rstRcvd.text, opName.text,
                                                     opQth.text, "")
                            : false
                        if (registrato)
                            qsoLog.addQso(macros.hisCall, macros.rstSent, rstRcvd.text,
                                          opName.text, opQth.text, radio.frequencyMhz)
                        if (registrato) {
                            // Pronti per il prossimo: il numero progressivo avanza
                            // e i campi del corrispondente si svuotano.
                            macros.incrementSerial()
                            macros.hisCall = ""
                            opName.text = ""
                            opQth.text = ""
                        }
                    }
                }
            }
        }

        // ── avviso duplicato ────────────────────────────────────────────
        Rectangle {
            x: 12
            width: parent.width - 24
            height: visible ? 26 : 0
            visible: macros.hisCall.length > 2 && qsoLog.timesWorked(macros.hisCall) > 0
            radius: 6
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.18)
            border.color: Theme.warning

            Text {
                anchors.centerIn: parent
                text: qsTr("Worked before: %1 — last on %2").arg(macros.hisCall)
                                                      .arg(qsoLog.lastWorked(macros.hisCall))
                color: Theme.warning
                font.pixelSize: 11
            }
        }

        // ── elenco ──────────────────────────────────────────────────────
        ListView {
            id: view
            x: 12
            width: parent.width - 24
            height: root.height - 230
            clip: true
            model: qsoLog
            spacing: 2
            verticalLayoutDirection: ListView.BottomToTop   // l'ultimo in cima

            delegate: Rectangle {
                required property int index
                required property date start
                required property string call
                required property string rstSent
                required property string rstReceived
                required property string name
                required property real frequencyMhz

                width: view.width
                height: 26
                radius: 4
                color: index % 2 ? "transparent" : Qt.rgba(1, 1, 1, 0.03)

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14

                    Text {
                        text: Qt.formatDateTime(start, "dd MMM hh:mm") + "Z"
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.family: Theme.monoFamily
                        width: 110
                    }
                    Text {
                        text: call
                        color: Theme.accent
                        font.pixelSize: 12
                        font.family: Theme.monoFamily
                        font.bold: true
                        width: 100
                    }
                    Text {
                        text: frequencyMhz > 0 ? frequencyMhz.toFixed(3) : "—"
                        color: Theme.secondary
                        font.pixelSize: 11
                        font.family: Theme.monoFamily
                        width: 80
                    }
                    Text {
                        text: rstSent + " / " + rstReceived
                        color: Theme.textPrimary
                        font.pixelSize: 11
                        font.family: Theme.monoFamily
                        width: 80
                    }
                    Text {
                        text: name
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }
        }

        Row {
            x: 16
            spacing: 8

            GlassButton {
                text: qsTr("Export ADIF")
                minimumWidth: 116
                accentColor: Theme.primary
                onClicked: qsoLog.exportAdif()
            }
            GlassButton {
                text: qsTr("Close")
                minimumWidth: 84
                onClicked: root.close()
            }
        }
    }
    }
}
