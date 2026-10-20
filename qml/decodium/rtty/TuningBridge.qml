// DecoRTTY — il ponte di sintonia.
//
// Due colonne, mark e space, e una traversa che le unisce. Le colonne salgono
// con il livello del rispettivo tono; la traversa sta dritta quando i due si
// equivalgono e pende dalla parte del piu' forte.
//
// Si legge senza pensarci, ed e' il motivo per cui in ogni programma per RTTY
// c'e' qualcosa di simile: girando la sintonia si guarda una cosa sola — se la
// traversa e' orizzontale e le colonne sono alte, il segnale e' a posto. Un
// numero direbbe la stessa cosa e si leggerebbe piu' lentamente.
//
// L'ellisse incrociata del TuningScope dice un'altra cosa ancora: la' si vede
// lo scarto in frequenza, qui l'equilibrio fra i due toni. Servono entrambe e
// non si sostituiscono.
import QtQuick

import "."

Item {
    id: root

    implicitHeight: 46

    // I due livelli, lisciati. I valori istantanei ballano di continuo — sono
    // l'uscita di due rivelatori, non una misura — e un ponte che sfarfalla e'
    // illeggibile proprio mentre lo si guarda per accordare.
    property real livelloMark: 0
    property real livelloSpace: 0
    // Quanto pesa il valore nuovo: un quinto. Segue una manopola girata a mano
    // senza inseguire ogni singolo bit.
    readonly property real inerzia: 0.2

    // Il piu' alto visto di recente, per dare una scala alle colonne. Cala
    // lentamente da se': senza, una scarica atmosferica schiaccerebbe il
    // grafico per il resto della serata.
    property real riferimento: 0.05

    Connections {
        target: (typeof rtty !== 'undefined') ? rtty : null
        function onScopePointReady(mark, space) {
            var m = Math.abs(mark)
            var s = Math.abs(space)
            root.livelloMark  = root.livelloMark  * (1 - root.inerzia) + m * root.inerzia
            root.livelloSpace = root.livelloSpace * (1 - root.inerzia) + s * root.inerzia
            var piu = Math.max(root.livelloMark, root.livelloSpace)
            root.riferimento = piu > root.riferimento
                               ? piu
                               : root.riferimento * 0.999 + piu * 0.001
            ponte.requestPaint()
        }
    }

    Canvas {
        id: ponte
        anchors.fill: parent
        anchors.margins: 2

        onPaint: {
            var g = getContext("2d")
            g.reset()
            g.clearRect(0, 0, width, height)

            var rif = Math.max(0.02, root.riferimento)
            var hm = Math.min(1, root.livelloMark / rif)
            var hs = Math.min(1, root.livelloSpace / rif)

            var base = height - 4          // dove poggiano le colonne
            var cima = 6                   // il punto piu' alto raggiungibile
            var corsa = base - cima
            var largh = 14
            var xs = width * 0.34          // space a sinistra
            var xm = width * 0.66          // mark a destra

            var ys = base - corsa * hs
            var ym = base - corsa * hm

            // La linea di terra: da' un appoggio alle colonne, altrimenti
            // sembrano sospese quando sono basse.
            g.strokeStyle = "#2a3742"
            g.lineWidth = 1
            g.beginPath()
            g.moveTo(4, base + 0.5)
            g.lineTo(width - 4, base + 0.5)
            g.stroke()

            // La traversa. E' lei che si guarda: dritta vuol dire due toni
            // equilibrati, storta vuol dire che uno arriva piu' dell'altro —
            // sintonia spostata, o una banda laterale sbagliata.
            var pendenza = Math.abs(ys - ym)
            var dritta = pendenza < corsa * 0.12 && (hm > 0.25 || hs > 0.25)
            g.strokeStyle = dritta ? "#00ff88" : "#ffb000"
            g.lineWidth = 3
            g.beginPath()
            g.moveTo(xs, ys)
            g.lineTo(xm, ym)
            g.stroke()

            // Le due colonne.
            function colonna(x, y, colore) {
                g.fillStyle = colore
                g.fillRect(x - largh / 2, y, largh, base - y)
                g.fillStyle = "#ffffff"
                g.globalAlpha = 0.75
                g.fillRect(x - largh / 2, y, largh, 2)
                g.globalAlpha = 1
            }
            colonna(xs, ys, "#4a90e2")
            colonna(xm, ym, "#00d4ff")

            // Le due lettere, sotto le rispettive colonne.
            g.fillStyle = "#89b4d0"
            g.font = "bold 9px sans-serif"
            g.textAlign = "center"
            g.fillText("S", xs, height - 0.5)
            g.fillText("M", xm, height - 0.5)
        }
    }

    // Quando non arriva niente il ponte si spegne da se', invece di restare
    // congelato sull'ultimo segnale sentito: un grafico fermo che sembra vivo
    // e' peggio di uno vuoto.
    Timer {
        interval: 250
        repeat: true
        running: root.visible
        onTriggered: {
            root.livelloMark  *= 0.82
            root.livelloSpace *= 0.82
            ponte.requestPaint()
        }
    }
}
