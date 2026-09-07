// DecoRTTY — il righello della radio, da zero a cinquanta megahertz.
//
// Tutta la copertura in una striscia sola: si vede dove si è, si vede quanto
// manca al pezzo di spettro che interessa, e ci si va puntandolo. I segmenti
// amatoriali sono colorati perché fra 0 e 50 MHz la maggior parte della strada
// è roba d'altri — emittenti, utility, servizi — e senza quei riferimenti un
// righello lungo cinquanta megahertz non dice niente.
//
// Cinquanta megahertz in duemila punti fanno venticinque kilohertz per punto:
// il trascinamento serve ad arrivare nel quartiere giusto, non all'indirizzo.
// L'indirizzo lo dà la rotella, che muove di un kilohertz per scatto — cento
// hertz con Maiusc, dieci kilohertz con Ctrl.
//
// E la seriale va protetta. Un trascinamento genera decine di posizioni al
// secondo, e l'FT-991A a 38400 baud con la conferma di ritorno ne regge una
// manciata: senza freno la coda dei comandi cresce e la radio insegue il dito
// con secondi di ritardo, arrivando dove il dito non è più. Si manda una
// posizione ogni centoventi millisecondi, più quella del rilascio — che è
// l'unica che conta davvero.
import QtQuick
import QtQuick.Controls

import "."

Item {
    id: root

    property real minMhz: 0
    property real maxMhz: 50

    readonly property bool live: radio.connected && radio.canControl

    // Dove stiamo portando la radio, mentre la radio ci arriva. Zero quando non
    // si sta comandando niente e vale quello che dice l'apparato.
    //
    // Serve perche' la risposta del CAT non e' immediata: senza, il cursore
    // tornerebbe indietro a ogni movimento — si trascina, il disegno salta alla
    // vecchia frequenza, e si insegue il proprio ritardo.
    property real steering: 0
    readonly property real shownMhz: steering > 0 ? steering : radio.frequencyMhz

    implicitHeight: 34

    function clampMhz(mhz) {
        // Sotto i trenta kilohertz nessun ricevitore di questa famiglia sente
        // niente, e un comando di sintonia a zero non vuol dire nulla.
        return Math.max(0.030, Math.min(root.maxMhz, mhz))
    }

    function mhzAt(x) {
        return root.minMhz + (x / width) * (root.maxMhz - root.minMhz)
    }

    function xOf(mhz) {
        return (mhz - root.minMhz) / (root.maxMhz - root.minMhz) * width
    }

    onShownMhzChanged: ruler.requestPaint()

    // Il freno sulla seriale.
    Timer {
        id: throttle
        interval: 120
        repeat: true
        running: area.pressed && root.live
        onTriggered: {
            if (root.steering > 0 && root.steering !== root.lastSent) {
                radio.setFrequencyMhz(root.steering)
                root.lastSent = root.steering
            }
        }
    }
    property real lastSent: 0

    // Passata la mano, si torna a credere all'apparato.
    Timer {
        id: settle
        interval: 1500
        onTriggered: root.steering = 0
    }

    Canvas {
        id: ruler
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            const h = height
            const barTop = 2
            const barH = h - 15          // sotto restano le cifre

            // Il fondo: la strada intera, quasi tutta di qualcun altro.
            ctx.fillStyle = Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.85)
            ctx.fillRect(0, barTop, width, barH)

            // I segmenti amatoriali.
            const bands = radio.bands
            const here = radio.currentBand
            for (let i = 0; i < bands.length; ++i) {
                const x0 = root.xOf(bands[i].low)
                const x1 = root.xOf(bands[i].high)
                const w = Math.max(3, x1 - x0)   // i 160 metri sono un'unghia
                const mine = (i === here)
                ctx.fillStyle = mine
                    ? Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.75)
                    : Qt.rgba(Theme.secondary.r, Theme.secondary.g, Theme.secondary.b, 0.30)
                ctx.fillRect(x0, barTop, w, barH)

                // Il nome, solo dove ci sta davvero.
                if (w > 26) {
                    ctx.fillStyle = mine ? Theme.bgDeep : Theme.textSecondary
                    ctx.font = "9px sans-serif"
                    ctx.textAlign = "center"
                    ctx.fillText(bands[i].name.replace(" m", ""), x0 + w / 2, barTop + barH - 3)
                }
            }

            // Le tacche dei megahertz: fitte ogni uno, marcate ogni cinque.
            for (let mhz = 0; mhz <= root.maxMhz; ++mhz) {
                const x = root.xOf(mhz)
                const big = (mhz % 5) === 0
                ctx.strokeStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                                          Theme.textSecondary.b, big ? 0.55 : 0.22)
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(Math.round(x) + 0.5, barTop)
                ctx.lineTo(Math.round(x) + 0.5, barTop + (big ? 6 : 3))
                ctx.stroke()

                if (big) {
                    ctx.fillStyle = Theme.textSecondary
                    ctx.font = "9px sans-serif"
                    ctx.textAlign = mhz === 0 ? "left"
                                  : (mhz >= root.maxMhz ? "right" : "center")
                    ctx.fillText(mhz, x, h - 3)
                }
            }

            // Dove siamo. Una lama sottile, e un triangolo che la trova da
            // lontano: su cinquanta megahertz una riga sola sparisce.
            if (radio.connected && root.shownMhz > 0) {
                const x = root.xOf(root.shownMhz)
                ctx.strokeStyle = Theme.accent
                ctx.lineWidth = 1.5
                ctx.beginPath()
                ctx.moveTo(x, barTop)
                ctx.lineTo(x, barTop + barH)
                ctx.stroke()

                ctx.fillStyle = Theme.accent
                ctx.beginPath()
                ctx.moveTo(x - 4, barTop)
                ctx.lineTo(x + 4, barTop)
                ctx.lineTo(x, barTop + 5)
                ctx.closePath()
                ctx.fill()
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.live
        hoverEnabled: true
        cursorShape: root.live ? Qt.PointingHandCursor : Qt.ArrowCursor

        property real hoverMhz: 0

        onPositionChanged: (mouse) => {
            hoverMhz = root.mhzAt(mouse.x)
            if (pressed) {
                root.steering = root.clampMhz(hoverMhz)
                settle.stop()
            }
        }
        onPressed: (mouse) => {
            root.steering = root.clampMhz(root.mhzAt(mouse.x))
            settle.stop()
        }
        onReleased: {
            // L'ultima posizione e' l'unica che l'operatore aveva in mente.
            if (root.steering > 0) {
                radio.setFrequencyMhz(root.steering)
                root.lastSent = root.steering
            }
            settle.restart()
        }
        onExited: hoverMhz = 0

        onWheel: (wheel) => {
            const step = (wheel.modifiers & Qt.ControlModifier) ? 0.010
                       : (wheel.modifiers & Qt.ShiftModifier)   ? 0.0001
                                                                : 0.001
            const base = root.steering > 0 ? root.steering : radio.frequencyMhz
            root.steering = root.clampMhz(base + (wheel.angleDelta.y > 0 ? step : -step))
            radio.setFrequencyMhz(root.steering)
            root.lastSent = root.steering
            settle.restart()
        }
    }

    // La frequenza sotto il dito, mentre ci si passa sopra. Senza, puntare a
    // occhio su una scala lunga cinquanta megahertz e' tirare a indovinare.
    Rectangle {
        visible: area.containsMouse && area.hoverMhz > 0
        x: Math.max(0, Math.min(root.width - width, root.xOf(area.hoverMhz) - width / 2))
        y: -3
        width: hint.implicitWidth + 10
        height: hint.implicitHeight + 4
        radius: 3
        color: Theme.bgDeep
        border.color: Theme.glassBorder
        border.width: 1

        Text {
            id: hint
            anchors.centerIn: parent
            text: area.hoverMhz.toFixed(3) + " MHz"
            color: Theme.accent
            font.pixelSize: 10
            font.family: Theme.monoFamily
        }
    }

    ToolTip.visible: area.containsMouse && !area.pressed && root.live
    ToolTip.delay: 900
    ToolTip.text: qsTr("Drag to move the radio; the wheel tunes by 1 kHz —\n100 Hz with Shift, 10 kHz with Ctrl.")
}
