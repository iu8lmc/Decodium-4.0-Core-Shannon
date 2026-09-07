// DecoRTTY — banda e modo, sopra il waterfall.
//
// Il posto giusto per cambiare banda e' accanto a quello che si guarda mentre
// si cerca qualcuno, non in un menu di impostazioni: si vede la banda vuota, si
// preme il pulsante accanto, e si guarda la successiva.
//
// Ogni pulsante porta la radio sul segmento dove il RTTY vive davvero — 14.080
// e non 14.000 — perche' al bordo dell'allocazione non c'e' nessuno.
//
// Quando la radio non si lascia comandare i pulsanti restano visibili ma spenti,
// e il perche' e' scritto nel suggerimento. E' l'unica soluzione onesta: farli
// sparire lascerebbe l'operatore a chiedersi dove sono finiti, e lasciarli
// accesi lo lascerebbe a premerli senza effetto.
import QtQuick
import QtQuick.Controls

import "."

Item {
    id: root

    // Chiamato quando la banda cambia davvero, per far ripartire il waterfall:
    // la coda di una banda sopra la testa di un'altra e' una figura che non e'
    // mai esistita in aria.

    // La radio si lascia comandare? Vuol dire che il CAT di Decodium e'
    // connesso; da una scheda audio non lo e' mai, perche' li' si ascolta
    // soltanto.
    readonly property bool live: radio.connected && radio.canControl
    // CAT backends use DATA/FSK names while this compact selector uses the
    // operator-facing DIGU/RTTY names. Compare their meanings, not spelling.
    readonly property string normalizedRadioMode: {
        const mode = radio.mode.trim().toUpperCase()
        if (mode === "DATA-U" || mode === "PKTUSB") return "DIGU"
        if (mode === "DATA-L" || mode === "PKTLSB") return "DIGL"
        if (mode === "FSK-R" || mode === "RTTYR") return "RTTY-U"
        if (mode === "FSK" || mode === "RTTY") return "RTTY-L"
        return mode
    }

    // Perche' e' spento, detto una volta sola e riusato da ogni suggerimento.
    readonly property string whyIdle: {
        if (!radio.connected)
            return qsTr("No radio connected.")
        return qsTr("Decodium's CAT is not connected, so the radio takes
no commands. Connect it in Decodium.")
    }

    // L'altezza segue il contenuto: quando la finestra si stringe i pulsanti
    // vanno a capo e la barra diventa piu' alta, invece di restare alta 24 con
    // meta' delle bande fuori dal bordo.
    implicitHeight: fila.implicitHeight

    // Bande e modi in una fila sola che va a capo. Prima erano due file
    // ancorate ai due bordi: a finestra stretta si venivano incontro e finivano
    // una sopra l'altra, con le scritte fuori. Cosi' si dispongono da se' su
    // quante righe servono, ed e' lo stesso modo in cui si comporta il resto di
    // Decodium quando lo spazio non basta.
    Flow {
        id: fila
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 3

        // I pulsanti delle bande stavano qui. Non ci sono piu': la banda si
        // sceglie da Decodium, dove sta il selettore dei modi che porta la
        // radio sul segmento RTTY. Sceglierla da due posti diversi vuol dire
        // che prima o poi i due dicono cose diverse.

        // Il modo in cui sta la radio adesso, anche quando non e' fra i nostri
        // quattro: se qualcuno l'ha messa in CW bisogna vederlo, altrimenti si
        // resta a chiedersi perche' non si copia niente.
        Text {
            // Niente ancoraggi qui dentro: in un Flow li mette il
            // posizionatore, e un anchor lo scavalca sovrapponendo gli
            // elementi invece di metterli in fila.
            height: 22
            verticalAlignment: Text.AlignVCenter
            text: radio.mode
            color: Theme.warning
            font.pixelSize: 10
            font.family: Theme.monoFamily
            rightPadding: 4
            visible: radio.connected && radio.mode !== ""
                     && radio.modes.indexOf(root.normalizedRadioMode) < 0
        }

        Repeater {
            model: radio.modes

            GlassButton {
                required property var modelData

                text: modelData === "DIGU" ? qsTr("DIGU · AFSK")
                      : modelData === "DIGL" ? qsTr("DIGL · AFSK")
                      : modelData.indexOf("RTTY") === 0 && !radio.requiresFullScaleTransmitAudio
                        ? modelData + qsTr(" · FSK")
                      : modelData
                enabled: root.live
                armed: root.normalizedRadioMode === modelData
                // DIGU e' quello giusto per l'AFSK: si distingue dagli altri.
                // I due modi RTTY veri si distinguono: sono quelli in cui
                // l'apparato stringe il filtro attorno ai toni.
                accentColor: modelData.indexOf("RTTY") === 0 ? Theme.success
                             : (modelData === "DIGU" ? Theme.secondary : Theme.textSecondary)
                minimumWidth: 46
                implicitHeight: 22
                font.pixelSize: 10
                // RadioHub owns radio-mode translation. In particular, QMX
                // exposes USB-audio FSK to Hamlib as PKTUSB/PKTLSB rather than
                // RTTY/RTTYR.
                onClicked: radio.setMode(modelData)
                ToolTip.visible: hovered
                ToolTip.delay: 700
                ToolTip.text: {
                    if (!root.live)
                        return root.whyIdle
                    if (modelData === "RTTY-U")
                        return radio.requiresFullScaleTransmitAudio
                               ? qsTr("QMX USB-audio FSK: Decodium selects its Digi/PKTUSB mode for transmission.")
                               : qsTr("True RTTY, upper sideband: the radio narrows
its filter around the tones. Best for copying — but
the radio waits for FSK keying, so transmitting from
here sends nothing.")
                    if (modelData === "RTTY-L")
                        return radio.requiresFullScaleTransmitAudio
                               ? qsTr("QMX reverse USB-audio FSK: Decodium selects its Digi/PKTLSB mode.")
                               : qsTr("True RTTY, lower sideband. Same as RTTY-U:
receive only, the radio expects FSK keying to
transmit.")
                    if (modelData === "DIGU")
                        return qsTr("DIGU — RTTY via audio/AFSK. Selects upper-sideband data mode (DATA-U / USB-D). Configure the radio to accept computer audio. Use this for audio-based RTTY transmission.")
                    if (modelData === "DIGL")
                        return qsTr("Data on the lower sideband. The tones come out\nreversed — REV puts them back.")
                    return qsTr("Voice sideband. RTTY is copied just the same,\nbut the radio's filter is wider than it needs.")
                }
            }
        }
    }
}
