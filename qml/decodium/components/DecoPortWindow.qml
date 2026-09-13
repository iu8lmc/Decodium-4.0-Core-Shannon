/* DecoPortWindow - la radio in rete: pubblicare la propria, usare quella di un
 * altro PC. Protocollo: doc/DECOPORT_PROTOCOL.md.
 *
 * Non c'e' niente da scegliere: il gateway trova da solo cosa e' attaccato, e
 * l'elenco delle radio in rete si riempie da solo con quello che si annuncia.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win

    readonly property var eng: (typeof appEngine !== 'undefined' ? appEngine : null)
    readonly property var gw: eng ? eng.decoPortGateway : null
    readonly property var disc: eng ? eng.decoPortDiscovery : null
    readonly property var lnk: eng ? eng.decoPortLink : null
    // Collegati e' una cosa, usarla al posto della propria scheda un'altra.
    readonly property bool useRemote: !!(eng && eng.decoPortUseRemote)
    readonly property bool monitorOn: !!(eng && eng.decoPortMonitor)

    // Il conto dei datagrammi arrivati al gateway. Si rilegge sia quando il
    // gateway segnala di aver contato qualcosa, sia a tempo: l'eta' dell'ultimo
    // pacchetto invecchia da sola anche quando non arriva piu' niente, ed e'
    // proprio il caso in cui la si vuole guardare.
    property var traf: null
    function rileggiTraffico() { win.traf = win.gw ? win.gw.traffico : null }
    Connections {
        target: win.gw
        function onTrafficoChanged() { win.rileggiTraffico() }
        function onRunningChanged()  { win.rileggiTraffico() }
    }
    Timer {
        interval: 2000
        repeat: true
        running: win.visible && !!(win.gw && win.gw.running)
        onTriggered: win.rileggiTraffico()
    }
    Component.onCompleted: win.rileggiTraffico()

    readonly property var tm: eng ? eng.themeManager : null
    readonly property color cBg:      tm ? tm.bgDeep        : "#0a0f14"
    readonly property color cPanel:   tm ? tm.panelColor    : "#111a22"
    readonly property color cBorder:  tm ? tm.borderColor   : "#22303c"
    readonly property color cText:    tm ? tm.textPrimary   : "#dbe4ea"
    readonly property color cDim:     tm ? tm.textSecondary : "#7d8b96"
    readonly property color cAccent:  tm ? tm.accentColor   : "#19d0ff"
    readonly property color cOk:      tm ? tm.successColor  : "#25d366"
    readonly property color cWarn:    tm ? tm.warningColor  : "#ffb84a"

    title: qsTr("DecoPort - radio on the network")
    width: 720
    height: 620
    minimumWidth: 520
    minimumHeight: 420
    color: cBg

    // hasDecoPortPassword() e' un metodo: in un binding non si aggiornerebbe da
    // solo, quindi lo stato lo teniamo qui e lo rileggiamo dopo ogni azione.
    property bool passwordSet: false
    property string passwordFeedback: ""

    function refreshPasswordState() {
        passwordSet = eng ? eng.hasDecoPortPassword() : false
    }

    // La scoperta si accende all'apertura e si spegne alla chiusura: nessun
    // socket in ascolto per chi questa finestra non la apre mai.
    onVisibleChanged: {
        if (visible)
            refreshPasswordState()
        if (!disc)
            return
        if (visible)
            disc.start()
        else
            disc.stop()
    }

    Component.onCompleted: refreshPasswordState()

    component Chip: Rectangle {
        id: chip
        property string label: ""
        property bool on: false
        property color tint: win.cAccent
        implicitWidth: chipTxt.implicitWidth + 16
        implicitHeight: 20
        radius: 10
        color: chip.on ? Qt.rgba(chip.tint.r, chip.tint.g, chip.tint.b, 0.18) : "transparent"
        border.width: 1
        border.color: chip.on ? chip.tint : win.cBorder
        Text {
            id: chipTxt
            anchors.centerIn: parent
            text: chip.label
            color: chip.on ? chip.tint : win.cDim
            font.pixelSize: 10
            font.bold: true
        }
    }

    component Btn: Rectangle {
        id: btn
        property string label: ""
        property bool danger: false
        property bool live: true
        signal clicked()
        implicitWidth: btnTxt.implicitWidth + 22
        implicitHeight: 26
        radius: 5
        opacity: btn.live ? 1.0 : 0.4
        color: btnMA.containsMouse && btn.live
               ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.16) : "transparent"
        border.width: 1
        border.color: btn.danger ? win.cWarn : win.cAccent
        Text {
            id: btnTxt
            anchors.centerIn: parent
            text: btn.label
            color: btn.danger ? win.cWarn : win.cAccent
            font.pixelSize: 11
            font.bold: true
        }
        MouseArea {
            id: btnMA
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: btn.live ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (btn.live) btn.clicked()
        }
    }

    component Section: Rectangle {
        id: sec
        property string title: ""
        default property alias content: secBody.data
        Layout.fillWidth: true
        implicitHeight: secCol.implicitHeight + 34
        radius: 8
        color: win.cPanel
        border.color: win.cBorder
        border.width: 1
        ColumnLayout {
            id: secCol
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 12
            spacing: 8
            Text {
                text: sec.title
                color: win.cAccent
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }
            ColumnLayout {
                id: secBody
                Layout.fillWidth: true
                spacing: 6
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 12

            // ── la password ────────────────────────────────────────────────
            Section {
                title: qsTr("SECURITY")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Chip {
                        label: win.passwordSet ? qsTr("PASSWORD SET") : qsTr("NO PASSWORD")
                        on: win.passwordSet
                        tint: win.passwordSet ? win.cOk : win.cWarn
                    }
                    Text {
                        Layout.fillWidth: true
                        text: win.passwordSet
                              ? qsTr("Every packet is signed. The same password is needed on the other computer.")
                              : qsTr("Without a password the radio is not published: the gateway refuses to start.")
                        color: win.cDim
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: pwField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: win.passwordSet
                                         ? qsTr("new password (at least 8 characters)")
                                         : qsTr("password (at least 8 characters)")
                        color: win.cText
                        font.pixelSize: 12
                        selectByMouse: true
                        onActiveFocusChanged: if (activeFocus) selectAll()
                        background: Rectangle {
                            color: "transparent"; radius: 4
                            border.color: win.cBorder; border.width: 1
                        }
                        onAccepted: applyBtn.clicked()
                    }
                    Btn {
                        id: applyBtn
                        label: win.passwordSet ? qsTr("CHANGE") : qsTr("SET")
                        live: pwField.text.length >= 8
                        onClicked: {
                            if (!win.eng || pwField.text.length < 8)
                                return
                            if (win.eng.setDecoPortPassword(pwField.text)) {
                                pwField.text = ""
                                win.refreshPasswordState()
                                // Il gateway ora firma con la chiave nuova: chi
                                // era collegato con la vecchia smette di essere
                                // riconosciuto, ed e' giusto cosi'.
                                win.passwordFeedback = qsTr("Password saved. Use the same one on the other computer; anyone connected with the old one is now shut out.")
                            } else {
                                win.passwordFeedback = qsTr("Password not accepted.")
                            }
                        }
                    }
                    Btn {
                        label: qsTr("REMOVE")
                        danger: true
                        live: win.passwordSet
                        onClicked: {
                            if (!win.eng)
                                return
                            win.eng.clearDecoPortPassword()
                            win.refreshPasswordState()
                            win.passwordFeedback = qsTr("Password removed and gateway stopped: the radio is no longer published.")
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: win.passwordFeedback
                    color: win.cDim
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("The password is not stored as you type it: it becomes a key, and the password is discarded. It cannot be read back from here.")
                    color: win.cDim
                    font.pixelSize: 10
                    font.italic: true
                    wrapMode: Text.WordWrap
                }
            }

            // ── la mia radio, pubblicata ────────────────────────────────────
            Section {
                title: qsTr("THIS RADIO - PUBLISHED ON THE NETWORK")

                Text {
                    Layout.fillWidth: true
                    text: win.gw ? win.gw.rigLabel : qsTr("unavailable")
                    color: win.cText
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: {
                        if (!win.gw)
                            return ""
                        var d = win.gw.detectedRadio()
                        if (!d || !d.catPort)
                            return ""
                        var parts = [qsTr("CAT %1").arg(d.catPort)]
                        if (d.baudRate > 0) parts.push(d.baudRate + " baud")
                        if (d.audioInput)  parts.push(qsTr("in: %1").arg(d.audioInput))
                        if (d.audioOutput) parts.push(qsTr("out: %1").arg(d.audioOutput))
                        return parts.join("  ·  ")
                    }
                    color: win.cDim
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: {
                        if (!win.gw) return ""
                        var d = win.gw.detectedRadio()
                        return (d && d.evidence) ? d.evidence : ""
                    }
                    color: win.cDim
                    font.pixelSize: 10
                    font.italic: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8
                    Btn {
                        label: (win.gw && win.gw.running) ? qsTr("STOP") : qsTr("PUBLISH")
                        danger: !!(win.gw && win.gw.running)
                        // Senza password non c'e' niente da pubblicare: il
                        // gateway rifiuterebbe comunque di partire.
                        live: win.passwordSet || !!(win.gw && win.gw.running)
                        onClicked: {
                            if (!win.eng) return
                            if (win.gw && win.gw.running)
                                win.eng.stopDecoPortGateway()
                            else
                                win.eng.startDecoPortGateway(5559)
                        }
                    }
                    Btn {
                        label: qsTr("RESCAN")
                        onClicked: if (win.gw) win.gw.refreshDetection()
                    }
                    Chip {
                        label: (win.gw && win.gw.running) ? qsTr("ON AIR ON THE LAN") : qsTr("NOT PUBLISHED")
                        on: !!(win.gw && win.gw.running)
                        tint: win.cOk
                    }
                    Chip {
                        label: qsTr("%1 client").arg(win.gw ? win.gw.clientCount : 0)
                        on: !!(win.gw && win.gw.clientCount > 0)
                    }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    text: win.gw ? win.gw.status : ""
                    color: win.cDim
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                // Il conto dei datagrammi. Distingue i due guasti che da fuori
                // si somigliano: non arriva niente, oppure arriva e lo
                // rifiutiamo. Compare solo a radio pubblicata, perche' prima
                // non c'e' niente da contare.
                Text {
                    id: contatoreTraffico
                    Layout.fillWidth: true
                    visible: !!(win.gw && win.gw.running)
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap

                    readonly property int ricevuti:  win.traf ? win.traf.ricevuti  : 0
                    readonly property int accettati: win.traf ? win.traf.accettati : 0
                    readonly property int eta:       win.traf ? win.traf.ultimoDaSecondi : -1

                    color: ricevuti === 0 ? win.cWarn
                                          : (accettati === 0 ? win.cWarn : win.cDim)

                    text: {
                        if (!win.traf)
                            return ""
                        if (ricevuti === 0)
                            return qsTr("No packet has arrived: the client is not reaching this machine. Check network, firewall and port %1.")
                                     .arg(win.gw ? win.gw.sessionPort : 0)
                        var da = win.traf.ultimoMittente
                        var quando = eta < 0 ? ""
                                   : (eta < 60 ? qsTr("%1 s ago").arg(eta)
                                               : qsTr("%1 min ago").arg(Math.floor(eta / 60)))
                        if (accettati === 0) {
                            // Arrivano ma li respingiamo tutti: qui il dettaglio
                            // e' la diagnosi, perche' ogni voce ha una causa sua.
                            var motivi = []
                            if (win.traf.firmaErrata > 0)
                                motivi.push(qsTr("%1 wrong password").arg(win.traf.firmaErrata))
                            if (win.traf.fuoriTempo > 0)
                                motivi.push(qsTr("%1 clock out of step").arg(win.traf.fuoriTempo))
                            if (win.traf.malformati > 0)
                                motivi.push(qsTr("%1 not DecoPort traffic").arg(win.traf.malformati))
                            if (win.traf.daBloccati > 0)
                                motivi.push(qsTr("%1 from a blocked sender").arg(win.traf.daBloccati))
                            return qsTr("%1 packets from %2 (%3), all rejected: %4")
                                     .arg(ricevuti).arg(da).arg(quando).arg(motivi.join(", "))
                        }
                        return qsTr("%1 packets received, %2 accepted - last from %3 (%4)")
                                 .arg(ricevuti).arg(accettati).arg(da).arg(quando)
                    }
                }
            }

            // ── quello che c'e' in rete ─────────────────────────────────────
            Section {
                title: qsTr("RADIOS FOUND ON THE NETWORK")

                Text {
                    Layout.fillWidth: true
                    visible: !win.disc || win.disc.radios.length === 0
                    text: qsTr("Listening. A gateway announces itself every two seconds; nothing to type.")
                    color: win.cDim
                    font.pixelSize: 11
                    font.italic: true
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: win.disc ? win.disc.radios : []
                    delegate: Rectangle {
                        id: radioRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 5
                        color: rowMA.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
                        border.width: 1
                        border.color: win.cBorder

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Text {
                                    text: radioRow.modelData.rigLabel
                                    color: win.cText
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: radioRow.modelData.host + ":" + radioRow.modelData.port
                                    color: win.cDim
                                    font.pixelSize: 10
                                }
                            }
                            Chip {
                                label: radioRow.modelData.catOnline ? qsTr("CAT") : qsTr("NO CAT")
                                on: radioRow.modelData.catOnline
                                tint: win.cOk
                            }
                            Btn {
                                label: qsTr("CONNECT")
                                onClicked: if (win.lnk)
                                    win.lnk.connectTo(radioRow.modelData.host, radioRow.modelData.port)
                            }
                        }
                        MouseArea {
                            id: rowMA
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            // ── la radio che sto usando ─────────────────────────────────────
            Section {
                title: qsTr("CONNECTED RADIO")

                Text {
                    Layout.fillWidth: true
                    text: win.lnk ? win.lnk.status : ""
                    color: (win.lnk && win.lnk.linked) ? win.cOk : win.cDim
                    font.pixelSize: 11
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: !!(win.lnk && win.lnk.linked)
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 6

                    Text { text: qsTr("Radio"); color: win.cDim; font.pixelSize: 11 }
                    Text {
                        text: win.lnk ? win.lnk.rigLabel : ""
                        color: win.cText; font.pixelSize: 12; font.bold: true
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }

                    Text { text: qsTr("Frequency"); color: win.cDim; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        TextField {
                            id: freqField
                            Layout.preferredWidth: 140
                            placeholderText: qsTr("Hz")
                            text: win.lnk ? String(Math.round(win.lnk.frequencyHz)) : ""
                            color: win.cText
                            font.pixelSize: 12
                            selectByMouse: true
                            // Il campo si seleziona tutto al fuoco: senza, un
                            // validator rende scomoda la digitazione.
                            onActiveFocusChanged: if (activeFocus) selectAll()
                            background: Rectangle {
                                color: "transparent"; radius: 4
                                border.color: win.cBorder; border.width: 1
                            }
                            onAccepted: if (win.lnk) win.lnk.tune(Number(text))
                        }
                        Btn {
                            label: qsTr("TUNE")
                            onClicked: if (win.lnk) win.lnk.tune(Number(freqField.text))
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Text { text: qsTr("Mode"); color: win.cDim; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        Text {
                            text: win.lnk ? win.lnk.modeName : ""
                            color: win.cText; font.pixelSize: 12; font.bold: true
                            Layout.preferredWidth: 60
                        }
                        Repeater {
                            model: ["USB", "LSB", "DIGU", "CW"]
                            delegate: Btn {
                                required property string modelData
                                label: modelData
                                onClicked: if (win.lnk) win.lnk.setModeName(modelData)
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Text { text: qsTr("Transmitter"); color: win.cDim; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        Chip {
                            label: (win.lnk && win.lnk.ptt) ? qsTr("KEYED") : qsTr("RECEIVING")
                            on: !!(win.lnk && win.lnk.ptt)
                            tint: win.cWarn
                        }
                        Text {
                            // Onesta' su cosa questo collegamento sa ancora fare.
                            text: qsTr("keying and transmit audio are not carried yet")
                            color: win.cDim
                            font.pixelSize: 10
                            font.italic: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }

                // ── usare davvero questa radio ──────────────────────────
                // Collegarsi mostra la radio; questo la mette al posto della
                // scheda audio locale, che e' il punto di tutto l'esercizio.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    visible: !!(win.lnk && win.lnk.linked)
                    implicitHeight: useCol.implicitHeight + 20
                    radius: 6
                    color: win.useRemote
                           ? Qt.rgba(win.cOk.r, win.cOk.g, win.cOk.b, 0.10) : "transparent"
                    border.width: 1
                    border.color: win.useRemote ? win.cOk : win.cBorder

                    ColumnLayout {
                        id: useCol
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        anchors.margins: 10
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Chip {
                                label: win.useRemote ? qsTr("IN USE") : qsTr("NOT IN USE")
                                on: win.useRemote
                                tint: win.cOk
                            }
                            Text {
                                Layout.fillWidth: true
                                text: win.useRemote
                                      ? qsTr("Decoding the remote radio — the local sound card is released")
                                      : qsTr("Decoding the local sound card")
                                color: win.cText
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Btn {
                                label: win.useRemote ? qsTr("STOP USING") : qsTr("USE THIS RADIO")
                                danger: win.useRemote
                                onClicked: if (win.eng)
                                    win.eng.setDecoPortUseRemote(!win.useRemote)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            visible: win.useRemote
                            Chip {
                                label: win.monitorOn ? qsTr("LISTENING") : qsTr("SILENT")
                                on: win.monitorOn
                                tint: win.cAccent
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Hear the remote radio on the speaker as well as decoding it")
                                color: win.cDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Btn {
                                label: win.monitorOn ? qsTr("MUTE") : qsTr("LISTEN")
                                onClicked: if (win.eng)
                                    win.eng.setDecoPortMonitor(!win.monitorOn)
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            // Una stringa sola: qsTr() con una concatenazione
                            // non finisce nei cataloghi di traduzione.
                            text: qsTr("Its audio goes into the decoder and its frequency becomes the one shown in the application, in both directions. Transmission stays local.")
                            color: win.cDim
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    visible: !!(win.lnk && win.lnk.linked)
                    Btn {
                        label: qsTr("DISCONNECT")
                        danger: true
                        onClicked: {
                            if (win.eng && win.useRemote)
                                win.eng.setDecoPortUseRemote(false)
                            if (win.lnk) win.lnk.disconnectFromGateway()
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
