// DecoRTTY — the receive window.
//
// Decoded and transmitted text share one flow, the way a printed QSO would read.
// Uncertain characters are dimmed rather than hidden: on a weak signal a partly
// readable line is still worth something, but it must not look as authoritative
// as a solid copy.
import QtQuick
import QtQuick.Controls

import "."

GlassPanel {
    id: root

    property bool autoScroll: true
    property bool highlightCalls: true

    // Emesso quando un doppio clic ha registrato un collegamento.
    signal logged(string call)

    property string lastLogged: ""
    onLogged: (call) => { root.lastLogged = call; loggedTimer.restart() }
    Timer { id: loggedTimer; interval: 2500; onTriggered: root.lastLogged = "" }

    // Riconosce i nominativi nel testo ricevuto. Il RTTY arriva sporco, quindi
    // la regola resta larga: prefisso con almeno una cifra, poi lettere. Meglio
    // segnalarne uno in piu' che perdere quello giusto in mezzo a una riga.
    readonly property var callPattern:
        /([A-Z]{1,2}[0-9][A-Z]{1,4}|[0-9][A-Z]{1,2}[0-9][A-Z]{1,4})/g

    // Un colore QML convertito in stringa esce come "#aarrggbb", con l'alpha
    // davanti: l'HTML di Qt non lo riconosce e ignora l'attributo, quindi il
    // testo resta del colore normale e l'evidenziazione non si vede affatto.
    // Serve la forma a sei cifre.
    readonly property string callColour: {
        function hex(v) { return Math.round(v * 255).toString(16).padStart(2, "0") }
        return "#" + hex(Theme.accent.r) + hex(Theme.accent.g) + hex(Theme.accent.b)
    }

    function markCalls(line) {
        if (!highlightCalls)
            return line
        // Il testo va protetto prima di inserirci dentro del markup: un '&' o un
        // '<' arrivati dall'aria altrimenti mangiano il resto della riga.
        const safe = line.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        return safe.replace(callPattern, function (m) {
            return '<b><font color="' + callColour + '">' + m + '</font></b>'
        })
    }

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Item {
            width: parent.width
            height: 24

            PanelHeading {
                text: qsTr("RECEIVE")
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.lastLogged.length > 0
                width: loggedLabel.width + 18
                height: 20
                radius: 10
                color: Theme.success

                Text {
                    id: loggedLabel
                    anchors.centerIn: parent
                    text: root.lastLogged + " a log"
                    color: Theme.bgDeep
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            GlassButton {
                text: qsTr("Call")
                armed: root.highlightCalls
                accentColor: Theme.accent
                minimumWidth: 48
                implicitHeight: 22
                font.pixelSize: 10
                onClicked: root.highlightCalls = !root.highlightCalls
                ToolTip.visible: hovered
                ToolTip.delay: 700
                ToolTip.text: qsTr("Highlights callsigns. Click one to put it
in the HIS CALL field.")
            }

            GlassButton {
                text: qsTr("Follow")
                armed: root.autoScroll
                accentColor: Theme.secondary
                minimumWidth: 62
                implicitHeight: 22
                font.pixelSize: 10
                onClicked: {
                    root.autoScroll = !root.autoScroll
                    if (root.autoScroll)
                        view.positionViewAtEnd()
                }
            }

            GlassButton {
                text: qsTr("Copy")
                minimumWidth: 54
                implicitHeight: 22
                font.pixelSize: 10
                onClicked: {
                    clipboardHelper.text = receiveText.plainText()
                    clipboardHelper.selectAll()
                    clipboardHelper.copy()
                }
            }

            GlassButton {
                text: qsTr("Clear")
                accentColor: Theme.error
                minimumWidth: 54
                implicitHeight: 22
                font.pixelSize: 10
                onClicked: receiveText.clear()
            }
            }
        }

        ListView {
            id: view
            width: parent.width
            height: parent.height - 32
            clip: true
            model: receiveText
            spacing: 0
            cacheBuffer: 400

            // ── restare in coda ──────────────────────────────────────────
            //
            // positionViewAtEnd() da sola non basta. La si chiama nel momento in
            // cui la riga nasce, prima che il suo delegato sia stato misurato:
            // la vista si porta dove crede sia la fine, e l'ultima riga resta
            // mezza fuori dal bordo. Si rimanda a layout fatto e si completa
            // portando il contenuto a fondo per davvero.
            function stickToEnd() {
                if (!root.autoScroll || count === 0)
                    return
                positionViewAtEnd()
                contentY = Math.max(0, contentHeight - height)
            }

            // Il conteggio non cambia piu' quando la storia e' piena: ogni riga
            // nuova ne fa cadere una vecchia. Per questo si ascolta anche il
            // modello, che dice quando una riga e' nata davvero.
            onCountChanged: Qt.callLater(stickToEnd)
            // La finestra ridimensionata, o un pannello che si apre sopra,
            // cambiano l'altezza della vista: senza questo il testo resterebbe
            // fermo dov'era, cioe' fuori campo.
            onHeightChanged: Qt.callLater(stickToEnd)

            // L'operatore che scorre indietro per rileggere qualcosa comanda:
            // si smette di inseguire la coda finche' non ci torna da solo.
            onMovementEnded: root.autoScroll = atYEnd
            onFlickEnded: root.autoScroll = atYEnd

            Connections {
                target: receiveText
                function onLineAdded() { Qt.callLater(view.stickToEnd) }
            }

            delegate: Row {
                required property string lineText
                required property real   quality
                required property bool   corrected
                required property bool   transmitted

                width: view.width
                spacing: 6

                // Left rail: green for our own transmission, otherwise a
                // confidence bar.
                Rectangle {
                    width: 3
                    height: label.height
                    radius: 1.5
                    color: transmitted ? Theme.warning
                                       : Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                 Theme.accent.b, Math.max(0.15, quality))
                }

                Text {
                    id: label
                    width: parent.width - 12
                    text: transmitted ? lineText : root.markCalls(lineText)
                    textFormat: transmitted ? Text.PlainText : Text.StyledText
                    font.family: Theme.monoFamily
                    font.pixelSize: 14
                    wrapMode: Text.NoWrap
                    color: transmitted ? Theme.warning : Theme.textPrimary
                    // Confidence shows as opacity: a solid copy is bright, a
                    // marginal one recedes.
                    opacity: transmitted ? 0.95 : Math.max(0.42, Math.min(1.0, 0.42 + quality * 0.65))

                    // Un clic su una parola che sembra un nominativo lo porta nel
                    // campo del corrispondente; due clic lo mandano direttamente
                    // a log. In un contest sono i secondi che separano una
                    // risposta da una chiamata persa.
                    //
                    // La parola non si trova con positionAt(): su un testo
                    // formattato quella funzione conta anche i caratteri del
                    // markup, che a schermo non esistono, e l'indice che
                    // restituisce e' spostato di tutta la lunghezza dei tag
                    // colore inseriti prima del punto toccato. Il font e' a
                    // spaziatura fissa, quindi la colonna si ricava dividendo:
                    // e' esatta e non dipende da come il testo e' stato
                    // decorato.
                    function wordAt(px) {
                        const plain = lineText
                        if (!plain.length)
                            return ""
                        const charWidth = label.contentWidth / plain.length
                        if (charWidth <= 0)
                            return ""
                        let index = Math.floor(px / charWidth)
                        index = Math.max(0, Math.min(plain.length - 1, index))

                        let a = index, b = index
                        while (a > 0 && plain[a - 1] !== ' ') --a
                        while (b < plain.length && plain[b] !== ' ') ++b
                        const word = plain.substring(a, b).replace(/[^A-Z0-9\/]/g, "")
                        return (word.length >= 3 && /[0-9]/.test(word)) ? word : ""
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !transmitted && root.highlightCalls
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton

                        onClicked: (mouse) => {
                            const word = label.wordAt(mouse.x)
                            if (word.length)
                                macros.hisCall = word
                        }

                        onDoubleClicked: (mouse) => {
                            const word = label.wordAt(mouse.x)
                            if (!word.length)
                                return
                            macros.hisCall = word
                            // Registrato con i rapporti che stanno gia' nei
                            // campi in alto: e' quello che si vuole quando si
                            // lavora in fretta, e resta modificabile dal log.
                            if (qsoLog.addQso(word, macros.rstSent, "599", "", "",
                                              radio.frequencyMhz)) {
                                macros.incrementSerial()
                                root.logged(word)
                            }
                        }
                    }
                }
            }

            // Mentre si legge indietro il testo continua ad arrivare, e senza un
            // segno non si sa che sotto c'e' dell'altro. Un tocco qui riporta in
            // coda.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                width: backLabel.width + 22
                height: 22
                radius: 11
                visible: !root.autoScroll
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.9)

                Text {
                    id: backLabel
                    anchors.centerIn: parent
                    text: qsTr("jump to end ↓")
                    color: Theme.bgDeep
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.autoScroll = true
                        view.stickToEnd()
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 2.5
                    color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.5)
                }
            }
        }
    }

    // TextEdit is the only reliable route to the clipboard from QML without
    // pulling in a platform dependency.
    TextEdit {
        id: clipboardHelper
        visible: false
    }
}
