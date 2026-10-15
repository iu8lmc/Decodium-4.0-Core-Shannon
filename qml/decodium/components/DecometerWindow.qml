/* Decodium Qt6 - DECOMETER RF Vector Meter
 * Strumento di misura RF: potenza diretta/riflessa, ROS, ALC.
 * Le grandezze provengono dalla telemetria CAT gia' letta dal rig
 * (RFPOWER_METER_WATTS / SWR / ALC); quanto non e' misurabile senza
 * un sensore vettoriale resta dichiarato come tale.
 * Disegno: Claude Design "DECOMETER RF Vector Meter".
 * By IU8LMC
 */

import QtQuick

Item {
    id: decometerWindow
    // The component is the content of Main.qml's real desktop Window.  Keeping
    // it as a plain Item avoids a second Dialog/Overlay scene-graph layer,
    // which can crash Qt 6.11's threaded Metal renderer when the host window is
    // exposed, and also removes the Material dialog's internal top padding.

    // The host owns desktop geometry; this item only fills its content area.
    property var nativeHostWindow: null

    readonly property int faceWidth: 900
    readonly property int faceHeight: 420

    // Il Loader ospite riempie la finestra (anchors.fill), quindi qui parent
    // e' l'area della finestra: niente anello di binding e coordinate x/y
    // utilizzabili per lo spostamento.
    readonly property real hostWidth: parent ? parent.width : faceWidth
    readonly property real hostHeight: parent ? parent.height : faceHeight

    // una sola cornice: la disegna il frontalino
    width: nativeHostWindow && parent
           ? parent.width
           : Math.min(faceWidth, Math.max(320, hostWidth - 24))
    height: nativeHostWindow && parent
            ? parent.height
            : Math.min(faceHeight, Math.max(220, hostHeight - 24))

    property bool placed: false

    function centerOnHost() {
        if (!parent) return
        x = nativeHostWindow ? 0 : Math.max(0, Math.round((parent.width - width) / 2))
        y = nativeHostWindow ? 0 : Math.max(0, Math.round((parent.height - height) / 2))
        placed = true
    }

    function clampToHost() {
        if (nativeHostWindow || !parent) return
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }

    function finishNativeHostMove() {
        if (nativeHostWindow && typeof nativeHostWindow.finishDesktopMove === "function")
            nativeHostWindow.finishDesktopMove()
    }

    function requestWindowClose() {
        if (nativeHostWindow && typeof nativeHostWindow.hideHostedWindow === "function") {
            nativeHostWindow.hideHostedWindow()
            return
        }
        visible = false
    }

    // ---- tavolozza dello strumento (fissa: e' un frontalino, non un tema) ----
    readonly property color colInk:      "#E8ECEF"
    readonly property color colCyan:     "#27C4D4"
    readonly property color colGreen:    "#46D67C"
    readonly property color colAmber:    "#FFB454"
    readonly property color colRed:      "#FF4A4A"
    readonly property color colMuted:    "#5B6670"
    readonly property color colLabel:    "#8A939C"
    readonly property color colDim:      "#4E5A63"
    readonly property color colEdge:     "#262D34"
    readonly property color colPanel:    "#14181D"

    // ---------------------------------------------------------------- misure
    // Sorgenti reali. Nessun valore viene inventato: se il rig non fornisce
    // il dato, il display resta muto.
    readonly property bool  catUp:     !!(typeof bridge !== "undefined" && bridge.catConnected)
    // La telemetria CAT e' disponibile anche durante TUNE, quando Decodium
    // mantiene transmitting=false. Inoltre il PTT puo' essere comandato
    // direttamente dalla radio o da un altro client: in quel caso la potenza
    // misurata e' l'indicazione autorevole che RF e' realmente presente.
    readonly property bool  appRfOn:   !!(typeof bridge !== "undefined"
                                         && (bridge.transmitting || bridge.tuning))
    readonly property real  rawFwd:    usingAmp ? amp.watts
                                       : ((typeof bridge !== "undefined" && bridge.rigPowerWatts > 0) ? bridge.rigPowerWatts : 0)
    readonly property real  rawSwr:    usingAmp ? Math.max(1, amp.swr)
                                       : ((typeof bridge !== "undefined" && bridge.rigSwr >= 1) ? bridge.rigSwr : 1)
    readonly property bool  txOn:      appRfOn || rawFwd > 0.05
    readonly property bool  swrValid:  usingAmp ? true
                                       : (catUp && (typeof bridge !== "undefined") && bridge.rigSwr >= 1)
    readonly property bool  pwrValid:  usingAmp ? true : (catUp && rawFwd > 0)
    readonly property real  rawAlc:    (typeof bridge !== "undefined") ? bridge.rigAlc : 0
    readonly property bool  alcValid:  catUp && (typeof bridge !== "undefined") && bridge.rigAlcValid

    // S-meter (1.0.567). Si legge dal gestore CAT, che lo tiene aggiornato e
    // dichiara da se' se il valore e' stato letto davvero: uno zero, su questa
    // scala, vorrebbe dire S9, cioe' un segnale pieno su una radio che
    // magari l'S-meter non ce l'ha nemmeno.
    //
    // Hamlib da' i dB rispetto a S9 e sei dB fanno un punto S: -54 e' S0,
    // 0 e' S9, sopra si contano i "piu'" a decine come sul frontalino.
    readonly property var   catManager: (typeof bridge !== "undefined" && bridge.hamlibCat)
                                        ? bridge.hamlibCat : null
    readonly property int   rawStrengthDb: catManager ? catManager.strengthDb : 0
    readonly property bool  strengthValid: !!(catManager && catManager.strengthValid) && !txOn
    readonly property int   sUnit:  Math.max(0, Math.min(9, Math.round(9 + rawStrengthDb / 6)))
    readonly property int   sOver:  rawStrengthDb > 0 ? Math.round(rawStrengthDb / 10) * 10 : 0
    readonly property string sTesto: sOver > 0 ? "S9+" + sOver : "S" + sUnit

    // La banda si ricava dalla frequenza invece di chiederla: e' un conto di
    // due righe, e non lega il frontalino a una proprieta' in piu' del ponte.
    readonly property string bandaCorrente: {
        if (typeof bridge === "undefined") return ""
        // bridge.frequency e' in HERTZ: senza dividere, il confronto con
        // una tabella in MHz non trova mai niente e la banda resta vuota.
        var mhz = Number(bridge.frequency) / 1e6
        if (!isFinite(mhz) || mhz <= 0) return ""
        var t = [[0.135,0.138,"2200m"], [0.472,0.479,"630m"], [1.8,2.0,"160m"],
                 [3.5,4.0,"80m"],   [5.3,5.4,"60m"],    [7.0,7.3,"40m"],
                 [10.1,10.15,"30m"],[14.0,14.35,"20m"], [18.06,18.17,"17m"],
                 [21.0,21.45,"15m"],[24.89,24.99,"12m"],[28.0,29.7,"10m"],
                 [50.0,54.0,"6m"],  [70.0,71.0,"4m"],   [144.0,148.0,"2m"],
                 [222.0,225.0,"1.25m"], [420.0,450.0,"70cm"]]
        for (var i = 0; i < t.length; ++i)
            if (mhz >= t[i][0] && mhz <= t[i][1]) return t[i][2]
        return ""
    }

    // La potenza CAT e' anche una sorgente attendibile dello stato RF. Alcuni
    // backend/apparati aggiornano correttamente i meter senza riflettere nello
    // stesso istante il PTT in bridge.transmitting (per esempio TUNE o PTT
    // azionato dalla radio). In quel caso non bisogna scartare misure valide.
    readonly property bool  rfActive:  txOn || rawFwd > 0.05

    // Il polling di potenza e ROS e' spento di serie: senza di esso nessun
    // apparato fornisce un metro, e uno strumento muto senza spiegazione manda
    // a cercare il guasto nel posto sbagliato.
    property bool telemetryPolling: false
    property bool telemetryEnableDeferred: false

    function refreshTelemetryPolling() {
        telemetryPolling = (typeof bridge !== "undefined")
                           && (!!bridge.getSetting("PWRandSWR", false)
                               || !!bridge.getSetting("CheckSWR", false))
    }

    function ensureTelemetryPolling() {
        refreshTelemetryPolling()
        if (!visible || telemetryPolling || typeof bridge === "undefined")
            return

        // Cambiare questa opzione riconnette la CAT per ricreare il backend
        // con i meter abilitati. Non farlo mai mentre la radio e' in aria.
        if (bridge.transmitting || bridge.tuning) {
            telemetryEnableDeferred = true
            return
        }

        telemetryEnableDeferred = false
        // Abilita soltanto la lettura dei meter. CheckSWR resta una scelta
        // esplicita dell'operatore e non viene attivato aprendo lo strumento.
        bridge.setSetting("PWRandSWR", true)
    }

    readonly property string statusLine: {
        if (!catUp)             return qsTr("NO CAT LINK")
        if (!telemetryPolling)  return qsTr("TELEMETRY OFF — ENABLE PWR/SWR")
        if (rfActive && !pwrValid && !swrValid) return qsTr("RIG REPORTS NO METER")
        // Mostrare 400 W senza dire da dove vengono sarebbe peggio che non
        // mostrarli: la sorgente va sempre dichiarata.
        if (usingAmp)
            return qsTr("AMP") + ": " + qsTr("amplifier")
        if (preferAmp && !ampReading)
            return qsTr("AMP SILENT - SHOWING EXCITER")
        var n = (typeof bridge !== "undefined" && bridge.catRigName.length)
                ? bridge.catRigName : qsTr("connected")
        return "CAT: " + n.toUpperCase()
    }
    readonly property color statusColor: (!catUp || !telemetryPolling) ? colAmber : colDim

    // ---------------------------------------------------- amplificatore
    // Sorgente indipendente dalla radio: se tace, la radio non se ne accorge.
    readonly property var  amp:        (typeof bridge !== "undefined") ? bridge.amplifier : null
    readonly property bool ampReading: !!(amp && amp.responding)
    // La scelta e' dell'operatore, ma senza dati non si mostra un quadrante
    // vuoto: si ricade sull'eccitatrice, dichiarandolo.
    property bool preferAmp: false
    readonly property bool usingAmp:   preferAmp && ampReading

    // coefficiente di riflessione: rho = (ROS-1)/(ROS+1)
    readonly property real  rho:       rawSwr > 1 ? (rawSwr - 1) / (rawSwr + 1) : 0

    // stato ballistico (aggiornato dal tick, non nei binding)
    property real vFwd: 0
    property real vRef: 0
    property real vSwr: 1
    property real pkFwdV: 0;  property real pkFwdT: 0
    property real pkRefV: 0;  property real pkRefT: 0
    property real pkSwrV: 0;  property real pkSwrT: 0
    property real pepW: 0
    property real avgW: 0
    property real txSeconds: 0

    // portate: 5 / 50 / 500 / 5000 W
    property int  rangeIdx: 1
    property bool autoRange: true
    property real rangeFrom: 50
    property real rangeTo: 50
    property real rangeT0: -9
    property real overT: 0
    readonly property var fsArr: [5, 50, 500, 5000]
    readonly property var fsMult: ["×0.1", "×1", "×10", "×100"]
    readonly property var fsWatt: ["5 W", "50 W", "500 W", "5 kW"]

    property int  screenIdx: 0
    readonly property int screenCount: 4

    // HOLD: le letture restano ferme dove sono. Ferma cio' che si VEDE, non
    // cio' che si misura — il ROS continua a essere letto e l'allarme continua
    // a valere, altrimenti sarebbe un modo per non accorgersi di un guasto
    // all'antenna proprio mentre si guarda lo strumento.
    property bool hold: false
    property real holdFwd: 0
    property real holdRef: 0
    property real holdSwr: 1
    property real holdPep: 0
    property real holdAvg: 0

    function prendiIstantanea() {
        holdFwd = pkFwdV
        holdRef = pkRefV
        holdSwr = vSwr
        holdPep = pepW
        holdAvg = avgW
    }

    // Con HOLD acceso si mostrano i valori del momento del fermo.
    readonly property real vFwdVista: hold ? holdFwd : pkFwdV
    readonly property real vRefVista: hold ? holdRef : pkRefV
    readonly property real vSwrVista: hold ? holdSwr : vSwr
    readonly property real pepVista:  hold ? holdPep : pepW
    readonly property real avgVista:  hold ? holdAvg : avgW

    // La potenza in dBm, come la mostra un misuratore da laboratorio:
    // 1 W = 30 dBm. Sotto il microwatt non si scrive un numero, perche'
    // sarebbe rumore del misuratore e non segnale.
    readonly property real fwdDbm: vFwdVista > 1e-6
                                   ? 10 * Math.log(vFwdVista * 1000) / Math.LN10 : -99

    property real clock: 0

    function nowS() { return clock }

    function setRange(i) {
        rangeFrom = fsArr[rangeIdx]
        rangeTo = fsArr[i]
        rangeT0 = clock
        rangeIdx = i
        autoRange = false
    }

    // portata effettiva con transizione morbida (0.35 s), come da disegno
    function effFs() {
        var d = clock - rangeT0
        if (d >= 0 && d < 0.35) {
            var e = d / 0.35
            var k = e * e * (3 - 2 * e)
            return rangeFrom + (rangeTo - rangeFrom) * k
        }
        return fsArr[rangeIdx]
    }

    function fmtW(v) { return v >= 100 ? v.toFixed(1) : v.toFixed(2) }

    // ---- grandezze derivate (fisica esatta, non stime) ----
    readonly property real returnLossDb: rho > 0.0005 ? -20 * Math.log(rho) / Math.LN10 : 99
    readonly property real mismatchLossDb: rho > 0.0005 ? -10 * Math.log(1 - rho * rho) / Math.LN10 : 0
    readonly property real netW: Math.max(0, vFwd - vRef)
    // con solo il modulo di Gamma la resistenza e' vincolata a un intervallo:
    // 50/ROS <= R <= 50*ROS. La reattanza richiede la fase, che il CAT non da'.
    readonly property real rMin: 50 / Math.max(1, rawSwr)
    readonly property real rMax: 50 * Math.max(1, rawSwr)

    function activateHostedPanel() {
        ensureTelemetryPolling()
        if (!placed) centerOnHost()
        else clampToHost()
        clock = 0
        txSeconds = 0
        pepW = 0
        if (typeof bridge !== "undefined")
            preferAmp = !!bridge.getSetting("DecometerPreferAmp", false)
        face.forceActiveFocus()
    }

    onNativeHostWindowChanged: {
        if (nativeHostWindow && nativeHostWindow.visible)
            activateHostedPanel()
    }

    Component.onCompleted: refreshTelemetryPolling()

    Connections {
        target: (typeof bridge !== "undefined") ? bridge : null
        ignoreUnknownSignals: true
        function onSettingValueChanged(key, value) {
            if (key === "PWRandSWR" || key === "CheckSWR")
                decometerWindow.refreshTelemetryPolling()
        }
        function onTransmittingChanged() {
            if (decometerWindow.telemetryEnableDeferred && !bridge.transmitting)
                decometerWindow.ensureTelemetryPolling()
        }
        function onTuningChanged() {
            if (decometerWindow.telemetryEnableDeferred && !bridge.tuning)
                decometerWindow.ensureTelemetryPolling()
        }
    }

    // riga di misura riusata dalle tre schermate del display
    component Readout: Item {
        property string tag: ""
        property string value: ""
        property string unit: ""
        property color tint: "#E8ECEF"
        property int valueSize: 17
        width: parent ? parent.width : 0
        height: valueSize + 4
        Text {
            id: tagText
            anchors.left: parent.left
            anchors.baseline: valText.baseline
            text: parent.tag
            width: 46
            font.pixelSize: 13; font.bold: true; font.family: "monospace"
            color: parent.tint
        }
        Text {
            id: unitText
            anchors.right: parent.right
            anchors.baseline: valText.baseline
            text: parent.unit
            font.pixelSize: 10; font.family: "monospace"
            color: parent.tint
        }
        Text {
            id: valText
            anchors.left: tagText.right
            anchors.right: unitText.left
            anchors.rightMargin: 8
            horizontalAlignment: Text.AlignRight
            text: parent.value
            font.pixelSize: parent.valueSize; font.bold: true; font.family: "monospace"
            color: parent.tint
        }
    }

    // ------------------------------------------------------------ ballistica
    // 25 Hz in trasmissione, 5 Hz a riposo: uno strumento non deve pesare
    // sui PC modesti quando non c'e' nulla da mostrare.
    // 25 Hz finche' c'e' qualcosa da mostrare (trasmissione in corso oppure
    // aghi e picchi ancora in discesa), 5 Hz a strumento fermo: la discesa
    // non deve andare a scatti, ma nemmeno pesare sui PC modesti.
    readonly property bool settling: vFwd > 0.01 || vRef > 0.001
                                     || pkFwdV > 0.01 || pkRefV > 0.001 || pkSwrV > 0.002

    Timer {
        id: engine
        interval: (decometerWindow.rfActive || decometerWindow.settling) ? 40 : 200
        running: decometerWindow.nativeHostWindow
                 ? decometerWindow.nativeHostWindow.visible
                 : decometerWindow.visible
        repeat: true
        onTriggered: decometerWindow.tick(interval / 1000)
    }

    function tick(dt) {
        clock += dt

        var fwdT = (rfActive && pwrValid) ? rawFwd : 0
        var swrT = (rfActive && swrValid) ? rawSwr : vSwr

        // attacco istantaneo, rilascio esponenziale (0.5 s potenza, 0.4 s ROS)
        function rel(cur, tgt, tau) {
            return tgt >= cur ? tgt : cur + (tgt - cur) * (1 - Math.exp(-dt / tau))
        }
        var refT = fwdT * rho * rho

        vFwd = rel(vFwd, fwdT, 0.5)
        vRef = rel(vRef, refT, 0.5)
        if (rfActive && swrValid)
            vSwr = rel(vSwr, swrT, 0.4)

        // ritenuta di picco 3 s, poi discesa con tau 0.9 s
        function peak(v, pv, pt) {
            if (v >= pv - 1e-9) return [v, clock]
            if (clock - pt > 3) return [pv + (0 - pv) * (1 - Math.exp(-dt / 0.9)), pt]
            return [pv, pt]
        }
        var p
        p = peak(vFwd, pkFwdV, pkFwdT); pkFwdV = p[0]; pkFwdT = p[1]
        p = peak(vRef, pkRefV, pkRefT); pkRefV = p[0]; pkRefT = p[1]
        var sF = Math.max(0, (vSwr - 1) / (vSwr + 1))
        p = peak(sF, pkSwrV, pkSwrT); pkSwrV = p[0]; pkSwrT = p[1]

        if (rfActive) {
            txSeconds += dt
            if (vFwd > pepW) pepW = vFwd
            avgW = avgW + (vFwd - avgW) * (1 - Math.exp(-dt / 3.0))
        }

        // cambio portata automatico: oltre il 95% del fondo scala per 0.5 s
        if (autoRange && vFwd > 0.95 * fsArr[rangeIdx] && rangeIdx < 3) {
            overT += dt
            if (overT > 0.5) { setRangeAuto(rangeIdx + 1); overT = 0 }
        } else if (autoRange && rangeIdx > 0 && pkFwdV < 0.35 * fsArr[rangeIdx - 1]) {
            overT -= dt
            if (overT < -2.5) { setRangeAuto(rangeIdx - 1); overT = 0 }
        } else {
            overT = 0
        }

        gauge.requestPaint()
    }

    function setRangeAuto(i) {
        rangeFrom = fsArr[rangeIdx]
        rangeTo = fsArr[i]
        rangeT0 = clock
        rangeIdx = i
    }

    Item {
        id: faceHolder
        anchors.fill: parent
        clip: true

        // il frontalino ha proporzioni fisse: si adatta scalando, non deformando
        readonly property real fit: Math.min(width / decometerWindow.faceWidth,
                                            height / decometerWindow.faceHeight)

        Item {
            id: face
            width: decometerWindow.faceWidth
            height: decometerWindow.faceHeight
            anchors.centerIn: parent
            scale: faceHolder.fit
            focus: true

            Keys.onLeftPressed:  decometerWindow.screenIdx = (decometerWindow.screenIdx + decometerWindow.screenCount - 1) % decometerWindow.screenCount
            Keys.onRightPressed: decometerWindow.screenIdx = (decometerWindow.screenIdx + 1) % decometerWindow.screenCount

            Rectangle {
                anchors.fill: parent
                radius: 10
                border.color: "#23292F"
                border.width: 1
                gradient: Gradient {
                    GradientStop { position: 0.0;  color: "#171B21" }
                    GradientStop { position: 0.55; color: "#12161B" }
                    GradientStop { position: 1.0;  color: "#101418" }
                }
            }

            // Presa per lo spostamento: dichiarata prima dei comandi, quindi
            // riceve il trascinamento solo dalle zone libere del frontalino -
            // pulsanti e chip restano cliccabili.
            MouseArea {
                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
                property real grabX: 0
                property real grabY: 0
                property point pressGlobalPos: Qt.point(0, 0)
                property point pressWindowPos: Qt.point(0, 0)
                property bool nativeMoveActive: false
                onPressed: function (mouse) {
                    grabX = mouse.x
                    grabY = mouse.y
                    decometerWindow.placed = true
                    if (decometerWindow.nativeHostWindow) {
                        pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                        pressWindowPos = Qt.point(decometerWindow.nativeHostWindow.x,
                                                  decometerWindow.nativeHostWindow.y)
                        // macOS reserves a visible title-bar-sized gutter for
                        // startSystemMove(), even on a frameless window.  That
                        // made this panel spring back before it could reach an
                        // edge.  Use the explicit global-coordinate move below
                        // so all screen corners remain reachable on every OS.
                        nativeMoveActive = false
                    }
                }
                onPositionChanged: function (mouse) {
                    if (!pressed) return
                    if (decometerWindow.nativeHostWindow) {
                        if (nativeMoveActive)
                            return
                        var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                        decometerWindow.nativeHostWindow.x = Math.round(
                                    pressWindowPos.x + currentGlobalPos.x - pressGlobalPos.x)
                        decometerWindow.nativeHostWindow.y = Math.round(
                                    pressWindowPos.y + currentGlobalPos.y - pressGlobalPos.y)
                        return
                    }
                    var s = faceHolder.fit
                    decometerWindow.x += (mouse.x - grabX) * s
                    decometerWindow.y += (mouse.y - grabY) * s
                    decometerWindow.clampToHost()
                }
                onReleased: {
                    nativeMoveActive = false
                    decometerWindow.finishNativeHostMove()
                }
                onCanceled: {
                    nativeMoveActive = false
                    decometerWindow.finishNativeHostMove()
                }
            }

            // ------------------------------------------------- scale ad arco
            Canvas {
                id: gauge
                x: 14; y: 6
                width: 640; height: 264
                renderStrategy: Canvas.Immediate

                onPaint: {
                    var g = getContext("2d")
                    g.clearRect(0, 0, 640, 264)

                    var cx = 320, cy = 640
                    var A0 = -Math.PI * 2 / 3, A1 = -Math.PI / 3
                    var dim = decometerWindow.txOn ? 1 : 0.3
                    var GREEN = decometerWindow.colGreen
                    var AMBER = decometerWindow.colAmber
                    var RED = decometerWindow.colRed

                    function ang(f) { return A0 + (A1 - A0) * f }
                    function tick(R, f, len) {
                        var a = ang(f)
                        g.beginPath()
                        g.moveTo(cx + R * Math.cos(a), cy + R * Math.sin(a))
                        g.lineTo(cx + (R + len) * Math.cos(a), cy + (R + len) * Math.sin(a))
                        g.stroke()
                    }
                    function lbl(R, f, s) {
                        var a = ang(f)
                        g.fillText(s, cx + R * Math.cos(a), cy + R * Math.sin(a) + 3)
                    }

                    g.textAlign = "center"
                    g.font = "9px monospace"
                    g.strokeStyle = "#3A424A"
                    g.lineWidth = 1
                    g.fillStyle = "#6A737C"

                    // fondo scala della portata attiva, in watt
                    var fs = decometerWindow.effFs()
                    var stepW = fs / 10
                    for (var i = 0; i <= 10; i++) {
                        tick(601, i / 10, 5)
                        var v = stepW * i
                        lbl(616, i / 10, v >= 1000 ? (v / 1000) + "k" : String(Math.round(v * 100) / 100))
                    }
                    // scala riflessa: fondo scala = 20% della diretta
                    for (var j = 0; j <= 5; j++) {
                        tick(536, j / 5, 5)
                        var vr = fs * 0.2 / 5 * j
                        lbl(550, j / 5, vr >= 1000 ? (vr / 1000) + "k" : String(Math.round(vr * 100) / 100))
                    }
                    var swrL = [[1, "1.0"], [1.25, "1.25"], [1.5, "1.5"], [2, "2"], [3, "3"], [5, "5"], [1e9, "∞"]]
                    for (var k = 0; k < swrL.length; k++) {
                        var s = swrL[k][0]
                        var f = Math.min(1, (s - 1) / (s + 1))
                        tick(476, f, 5)
                        lbl(490, f, swrL[k][1])
                    }
                    g.font = "8px monospace"
                    g.fillStyle = "#525C64"
                    var alcT = [[0.25, "25"], [0.5, "50"], [0.75, "75"]]
                    for (var q = 0; q < alcT.length; q++) {
                        tick(452, alcT[q][0], -4)
                        lbl(443, alcT[q][0], alcT[q][1])
                    }

                    g.font = "bold 9px sans-serif"
                    g.fillStyle = "#7A848D"
                    g.textAlign = "left"
                    g.fillText("FWD", 18, 88)
                    g.fillText("REF", 52, 150)
                    g.fillText("SWR", 87, 212)
                    g.font = "bold 8px sans-serif"
                    g.fillStyle = "#525C64"
                    g.fillText("ALC %", 126, 247)

                    function arc(R, len, n, litF, peakF, colFn) {
                        var step = (A1 - A0) / n
                        for (var i2 = 0; i2 < n; i2++) {
                            var ff = (i2 + 0.5) / n
                            var a2 = A0 + step * (i2 + 0.5)
                            var ca = Math.cos(a2), sa = Math.sin(a2)
                            g.beginPath()
                            g.moveTo(cx + R * ca, cy + R * sa)
                            g.lineTo(cx + (R + len) * ca, cy + (R + len) * sa)
                            if (ff <= litF) {
                                g.strokeStyle = colFn(ff)
                                g.lineWidth = 5
                                g.globalAlpha = dim
                                g.stroke()
                                if (dim === 1) {
                                    g.globalAlpha = 0.22
                                    g.lineWidth = 9
                                    g.stroke()
                                }
                            } else {
                                g.strokeStyle = decometerWindow.colInk
                                g.globalAlpha = 0.08
                                g.lineWidth = 5
                                g.stroke()
                            }
                            g.globalAlpha = 1
                        }
                        if (peakF > 0.012) {
                            var ip = Math.min(n - 1, Math.floor(peakF * n))
                            var ap = A0 + step * (ip + 0.5)
                            var cp = Math.cos(ap), sp = Math.sin(ap)
                            g.beginPath()
                            g.moveTo(cx + R * cp, cy + R * sp)
                            g.lineTo(cx + (R + len) * cp, cy + (R + len) * sp)
                            g.strokeStyle = colFn(Math.min(1, peakF))
                            g.lineWidth = 5
                            g.globalAlpha = Math.max(dim, 0.95)
                            g.stroke()
                            g.globalAlpha = 0.4
                            g.lineWidth = 11
                            g.stroke()
                            g.globalAlpha = 1
                        }
                    }

                    function pwCol(f) { return f < 0.7 ? GREEN : (f < 0.9 ? AMBER : RED) }
                    function swCol(f) { return f < 0.2 ? GREEN : (f < 0.5 ? AMBER : RED) }
                    function cl(v) { return Math.max(0, Math.min(1, v)) }

                    var sFnow = Math.max(0, (decometerWindow.vSwr - 1) / (decometerWindow.vSwr + 1))
                    arc(580, 20, 64, cl(decometerWindow.vFwd / fs), cl(decometerWindow.pkFwdV / fs), pwCol)
                    arc(520, 15, 46, cl(decometerWindow.vRef / (fs * 0.2)), cl(decometerWindow.pkRefV / (fs * 0.2)), pwCol)
                    // La scala del ROS comincia da 1.0, non da zero: li' non
                    // c'e' l'assenza di misura, c'e' l'adattamento perfetto.
                    // L'arco pero' e' pilotato dal coefficiente di riflessione,
                    // che a ROS 1.00 vale zero netto: nessuna tacca si accendeva,
                    // e la condizione migliore possibile finiva per somigliare a
                    // "nessuna lettura". Con una misura valida si accende sempre
                    // la prima tacca, che sulla scala e' proprio 1.0 - e la mezza
                    // tacca serve perche' un segmento si illumina quando il suo
                    // centro rientra nella frazione. Sugli altri due archi non si
                    // fa: li' lo zero e' davvero niente watt.
                    var nSwr = 34
                    arc(460, 15, nSwr,
                        decometerWindow.swrValid ? Math.max(0.5 / nSwr, cl(sFnow)) : 0,
                        decometerWindow.swrValid ? cl(decometerWindow.pkSwrV) : 0, swCol)

                    // arco ALC piu' interno: percentuale di compressione del rig
                    if (decometerWindow.alcValid) {
                        arc(430, 10, 24, cl(decometerWindow.rawAlc / 100), 0,
                            function (f) { return f < 0.6 ? GREEN : (f < 0.85 ? AMBER : RED) })
                    }
                }
            }

            Rectangle {
                x: 660; y: 20; width: 1; height: 380
                gradient: Gradient {
                    GradientStop { position: 0.0;  color: "transparent" }
                    GradientStop { position: 0.2;  color: "#262D34" }
                    GradientStop { position: 0.8;  color: "#262D34" }
                    GradientStop { position: 1.0;  color: "transparent" }
                }
            }

            // ------------------------------------------------------- portate
            Row {
                x: 170; y: 250; width: 430; height: 30
                spacing: 8

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("RANGE")
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    color: decometerWindow.colMuted
                    rightPadding: 4
                }

                Repeater {
                    model: 4
                    delegate: Rectangle {
                        id: rangeChip
                        required property int index
                        readonly property bool on: index === decometerWindow.rangeIdx
                        width: 62; height: 30; radius: 5
                        color: on ? Qt.rgba(0.153, 0.769, 0.831, 0.14) : decometerWindow.colPanel
                        border.width: 1
                        border.color: on ? decometerWindow.colCyan
                                         : (rangeMouse.containsMouse ? "#3A424A" : decometerWindow.colEdge)
                        Column {
                            anchors.centerIn: parent
                            spacing: 1
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: decometerWindow.fsMult[rangeChip.index]
                                font.pixelSize: 10; font.bold: true; font.family: "monospace"
                                color: rangeChip.on ? decometerWindow.colCyan : "#7A848D"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: decometerWindow.fsWatt[rangeChip.index]
                                font.pixelSize: 8; font.family: "monospace"
                                opacity: 0.75
                                color: rangeChip.on ? decometerWindow.colCyan : "#7A848D"
                            }
                        }
                        MouseArea {
                            id: rangeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: decometerWindow.setRange(rangeChip.index)
                        }
                    }
                }
            }

            // ------------------------------------------------ display numerico
            Rectangle {
                x: 170; y: 296; width: 430; height: 106
                radius: 6
                color: "#000000"
                border.width: 1
                border.color: decometerWindow.swrValid && decometerWindow.vSwr >= 3 ? decometerWindow.colRed : "#1E252C"

                Column {
                    anchors.fill: parent
                    anchors.topMargin: 7
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.bottomMargin: 8
                    spacing: 5
                    // a riposo il display si attenua, ma un avviso deve restare
                    // leggibile: e' l'unico posto dove si spiega il silenzio
                    opacity: (decometerWindow.txOn || !decometerWindow.catUp
                              || !decometerWindow.telemetryPolling) ? 1 : 0.45

                    Item {
                        width: parent.width; height: 11
                        Text {
                            anchors.left: parent.left
                            text: decometerWindow.statusLine
                            font.pixelSize: 9; font.letterSpacing: 1; font.family: "monospace"
                            color: decometerWindow.statusColor
                        }
                        Text {
                            anchors.right: parent.right
                            text: decometerWindow.txOn ? (decometerWindow.screenIdx === 2 ? "TX-AVG" : "TX-PK") : "RX"
                            font.pixelSize: 9; font.letterSpacing: 1; font.family: "monospace"
                            color: decometerWindow.txOn ? decometerWindow.colCyan : decometerWindow.colDim
                        }
                    }

                    Row {
                        width: parent.width
                        height: 72
                        spacing: 18

                        Column {
                            width: parent.width - 148
                            spacing: 2

                            // schermata 1 - potenza
                            Readout {
                                visible: decometerWindow.screenIdx === 0
                                tag: "FWD"; tint: decometerWindow.colCyan
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.vFwdVista) : "——"
                                unit: "W"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 0
                                tag: "REF"
                                value: decometerWindow.pwrValid && decometerWindow.swrValid
                                       ? decometerWindow.vRefVista.toFixed(3) : "——"
                                unit: "W"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 0
                                tag: "SWR"; tint: decometerWindow.colAmber; valueSize: 22
                                value: decometerWindow.swrValid ? decometerWindow.vSwrVista.toFixed(2) : "——"
                            }

                            // schermata 2 - adattamento
                            Readout {
                                visible: decometerWindow.screenIdx === 1
                                tag: "RL"; tint: decometerWindow.colCyan
                                value: decometerWindow.swrValid
                                       ? (decometerWindow.returnLossDb >= 99 ? "> 60" : decometerWindow.returnLossDb.toFixed(1))
                                       : "——"
                                unit: "dB"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 1
                                tag: "ML"
                                value: decometerWindow.swrValid ? decometerWindow.mismatchLossDb.toFixed(2) : "——"
                                unit: "dB"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 1
                                tag: "NET"; tint: decometerWindow.colGreen; valueSize: 22
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.netW) : "——"
                                unit: "W"
                            }

                            // schermata 3 - pilotaggio
                            Readout {
                                visible: decometerWindow.screenIdx === 2
                                tag: "ALC"; tint: decometerWindow.colAmber
                                value: decometerWindow.alcValid ? Math.round(decometerWindow.rawAlc) + "" : "——"
                                unit: "%"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 2
                                tag: "PEP"; tint: decometerWindow.colCyan
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.pepVista) : "——"
                                unit: "W"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 2
                                tag: "AVG"; valueSize: 22
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.avgVista) : "——"
                                unit: "W"
                            }

                            // schermata 4 - segnale ricevuto
                            // E' l'unica che parla di RICEZIONE: le altre tre
                            // guardano cosa esce, questa cosa entra. Per
                            // questo vale a trasmettitore fermo, al contrario
                            // di tutto il resto del frontalino.
                            Readout {
                                visible: decometerWindow.screenIdx === 3
                                tag: "S"; tint: decometerWindow.colGreen; valueSize: 22
                                value: decometerWindow.strengthValid ? decometerWindow.sTesto : "——"
                            }
                            // La frequenza in chiaro: la banda dice in quale
                            // porzione si sta operando, ma chi guarda un
                            // misuratore vuole leggere anche il numero, come
                            // sul display della radio.
                            Readout {
                                visible: decometerWindow.screenIdx === 3
                                tag: "FRQ"; tint: decometerWindow.colCyan; valueSize: 20
                                value: {
                                    if (typeof bridge === "undefined") return "——"
                                    // bridge.frequency e' in HERTZ (14074000 = 14,074 MHz)
                                    var hz = Number(bridge.frequency)
                                    return (isFinite(hz) && hz > 0) ? (hz / 1e6).toFixed(3) : "——"
                                }
                                unit: "MHz"
                            }
                            // La potenza in dBm accanto ai watt: stessa
                            // misura in un'altra unita', ed e' quella che
                            // legge chi lavora con strumenti da laboratorio.
                            Readout {
                                visible: decometerWindow.screenIdx === 3
                                tag: "PWR"
                                value: decometerWindow.pwrValid && decometerWindow.fwdDbm > -99
                                       ? decometerWindow.fwdDbm.toFixed(1) : "——"
                                unit: "dBm"
                            }
                        }

                        // colonna impedenza: dichiara cosa e' noto e cosa no
                        Item {
                            width: 130; height: parent.height
                            Rectangle { width: 1; height: parent.height; color: "#1A2228" }
                            Column {
                                x: 14
                                spacing: 3
                                Text {
                                    text: decometerWindow.screenIdx === 2 ? qsTr("TX TIME")
                                          : (decometerWindow.screenIdx === 3 ? qsTr("BAND") : qsTr("IMPEDANCE"))
                                    font.pixelSize: 9; font.letterSpacing: 1; font.family: "monospace"
                                    color: decometerWindow.colDim
                                    bottomPadding: 3
                                }
                                // Banda e frequenza: un misuratore che non dice DOVE
                                // si sta operando racconta meta' della cosa, ed e' la
                                // prima riga del frontalino di ogni wattmetro da tavolo.
                                Text {
                                    visible: decometerWindow.screenIdx === 3
                                    text: decometerWindow.bandaCorrente.length
                                          ? decometerWindow.bandaCorrente.toUpperCase() : "—"
                                    font.pixelSize: 20; font.bold: true; font.family: "monospace"
                                    color: decometerWindow.colCyan
                                }
                                Text {
                                    visible: decometerWindow.screenIdx === 3
                                    text: decometerWindow.strengthValid ? qsTr("RX SIGNAL")
                                          : (decometerWindow.txOn ? qsTr("TRANSMITTING") : qsTr("NO S-METER"))
                                    width: 116
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 8
                                    color: decometerWindow.colDim
                                }
                                Text {
                                    visible: decometerWindow.screenIdx !== 2 && decometerWindow.screenIdx !== 3
                                    text: decometerWindow.swrValid
                                          ? "|Γ| " + decometerWindow.rho.toFixed(3) : "|Γ| —"
                                    font.pixelSize: 13; font.family: "monospace"
                                    color: "#9FB3BC"
                                }
                                Text {
                                    visible: decometerWindow.screenIdx !== 2 && decometerWindow.screenIdx !== 3
                                    text: decometerWindow.swrValid
                                          ? "R " + decometerWindow.rMin.toFixed(1) + "–" + decometerWindow.rMax.toFixed(1)
                                          : "R —"
                                    font.pixelSize: 13; font.family: "monospace"
                                    color: "#9FB3BC"
                                }
                                Text {
                                    visible: decometerWindow.screenIdx !== 2
                                    text: qsTr("X: needs vector sensor")
                                    width: 116
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 8
                                    color: decometerWindow.colDim
                                }
                                Text {
                                    visible: decometerWindow.screenIdx === 2
                                    text: {
                                        var s = Math.floor(decometerWindow.txSeconds)
                                        return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + (s % 60)
                                    }
                                    font.pixelSize: 17; font.bold: true; font.family: "monospace"
                                    color: "#9FB3BC"
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------- spie
            Column {
                x: 684; y: 34
                spacing: 15

                Repeater {
                    model: [
                        { key: "swr",  label: qsTr("SWR ALARM") },
                        { key: "alc",  label: qsTr("ALC CLIP") },
                        { key: "pwr",  label: qsTr("PWR SENSE") },
                        { key: "cat",  label: qsTr("CAT LINK") }
                    ]
                    delegate: Row {
                        id: ledRow
                        required property var modelData
                        spacing: 11
                        readonly property bool lit: {
                            switch (ledRow.modelData.key) {
                            case "swr": return decometerWindow.swrValid && decometerWindow.vSwr >= 3
                                               && (Math.floor(decometerWindow.clock * 2.4) % 2 === 0)
                            case "alc": return decometerWindow.alcValid && decometerWindow.rawAlc >= 85
                            case "pwr": return decometerWindow.pwrValid
                            case "cat": return decometerWindow.catUp
                            }
                            return false
                        }
                        readonly property color hue: (ledRow.modelData.key === "swr" || ledRow.modelData.key === "alc")
                                                     ? decometerWindow.colRed : decometerWindow.colGreen
                        Rectangle {
                            width: 10; height: 10; radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: ledRow.lit ? ledRow.hue : "#20262C"
                            border.width: 1
                            border.color: Qt.rgba(0, 0, 0, 0.6)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: ledRow.modelData.label
                            font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.8
                            color: decometerWindow.colLabel
                        }
                    }
                }
            }

            // ------------------------------------------------ schermate e auto
            Column {
                x: 684; y: 196; width: 190
                spacing: 10

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "◀   " + qsTr("SCREEN") + "   ▶"
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    color: decometerWindow.colMuted
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10
                    Repeater {
                        model: ["◀", "▶"]
                        delegate: Rectangle {
                            id: navChip
                            required property int index
                            required property string modelData
                            width: 74; height: 32; radius: 5
                            border.width: 1
                            border.color: nav.containsMouse ? "#3A424A" : "#2A3138"
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#1C2127" }
                                GradientStop { position: 1.0; color: "#14181D" }
                            }
                            Text {
                                anchors.centerIn: parent
                                text: navChip.modelData
                                font.pixelSize: 11; font.bold: true
                                color: nav.containsMouse ? "#B9C2C9" : decometerWindow.colLabel
                            }
                            MouseArea {
                                id: nav
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var n = decometerWindow.screenCount
                                    decometerWindow.screenIdx =
                                        (decometerWindow.screenIdx + (navChip.index === 0 ? n - 1 : 1)) % n
                                }
                            }
                        }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6

                    Rectangle {
                        width: 76; height: 34; radius: 5
                        color: decometerWindow.autoRange ? Qt.rgba(0.153, 0.769, 0.831, 0.14) : "#181D22"
                        border.width: 1
                        border.color: decometerWindow.autoRange ? decometerWindow.colCyan
                                                                : (autoMouse.containsMouse ? "#3A424A" : "#2A3138")
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("AUTO")
                            font.pixelSize: 10; font.bold: true; font.letterSpacing: 2
                            color: decometerWindow.autoRange ? decometerWindow.colCyan : decometerWindow.colLabel
                        }
                        MouseArea {
                            id: autoMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: decometerWindow.autoRange = !decometerWindow.autoRange
                        }
                    }

                    // HOLD: comodo su un frontalino da tavolo per leggere con
                    // calma, e indispensabile quando lo strumento sta su un
                    // secondo schermo che non si guarda mentre si parla.
                    Rectangle {
                        width: 76; height: 34; radius: 5
                        color: decometerWindow.hold ? Qt.rgba(1.0, 0.706, 0.329, 0.16) : "#181D22"
                        border.width: 1
                        border.color: decometerWindow.hold ? decometerWindow.colAmber
                                                           : (holdMouse.containsMouse ? "#3A424A" : "#2A3138")
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("HOLD")
                            font.pixelSize: 10; font.bold: true; font.letterSpacing: 2
                            color: decometerWindow.hold ? decometerWindow.colAmber : decometerWindow.colLabel
                        }
                        MouseArea {
                            id: holdMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                // L'istantanea si prende ADESSO: da qui in
                                // avanti i valori sotto continuano a muoversi,
                                // ma quelli mostrati restano questi.
                                if (!decometerWindow.hold)
                                    decometerWindow.prendiIstantanea()
                                decometerWindow.hold = !decometerWindow.hold
                            }
                        }
                    }
                }

                // Sorgente della misura: eccitatrice o amplificatore. Compare
                // solo se un amplificatore e' configurato, per non offrire una
                // scelta che non esiste.
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6
                    visible: !!decometerWindow.amp && decometerWindow.amp.enabled
                    Repeater {
                        model: [{ useAmp: false, label: qsTr("EXC") },
                                { useAmp: true,  label: qsTr("AMP") }]
                        delegate: Rectangle {
                            id: srcChip
                            required property var modelData
                            readonly property bool on: decometerWindow.preferAmp === srcChip.modelData.useAmp
                            width: 76; height: 26; radius: 5
                            color: on ? Qt.rgba(0.153, 0.769, 0.831, 0.14) : "#181D22"
                            border.width: 1
                            border.color: on ? decometerWindow.colCyan
                                             : (srcMouse.containsMouse ? "#3A424A" : "#2A3138")
                            Text {
                                anchors.centerIn: parent
                                text: srcChip.modelData.label
                                font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.5
                                color: srcChip.on ? decometerWindow.colCyan : decometerWindow.colLabel
                            }
                            MouseArea {
                                id: srcMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    decometerWindow.preferAmp = srcChip.modelData.useAmp
                                    if (typeof bridge !== "undefined")
                                        bridge.setSetting("DecometerPreferAmp",
                                                          decometerWindow.preferAmp)
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------- marchio
            Column {
                x: 22
                y: decometerWindow.faceHeight - 58
                width: 136
                spacing: 3
                clip: true
                Text {
                    width: parent.width
                    textFormat: Text.StyledText
                    text: "DEC<font color=\"#27C4D4\">Ø</font>METER"
                    font.pixelSize: 12; font.bold: true; font.letterSpacing: 1.5
                    color: decometerWindow.colLabel
                }
                Text {
                    width: parent.width
                    text: qsTr("RF VECTOR METER") + "\n1.8–500 MHz"
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    font.pixelSize: 8; font.letterSpacing: 1.0
                    color: decometerWindow.colMuted
                }
            }

            Text {
                x: decometerWindow.faceWidth - 92
                y: decometerWindow.faceHeight - 24
                text: "DECODIUM"
                font.pixelSize: 8; font.bold: true; font.letterSpacing: 2.5
                color: decometerWindow.colDim
            }

            // chiusura: discreta, all'angolo del frontalino
            Rectangle {
                id: closeBtn
                x: decometerWindow.faceWidth - 32
                y: 10
                width: 22; height: 22; radius: 4
                color: closeMouse.containsMouse ? Qt.rgba(1, 0.29, 0.29, 0.18) : "transparent"
                border.width: 1
                border.color: closeMouse.containsMouse ? decometerWindow.colRed : "#2A3138"
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 11; font.bold: true
                    color: closeMouse.containsMouse ? decometerWindow.colRed : decometerWindow.colLabel
                }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: decometerWindow.requestWindowClose()
                }
            }
        }
    }
}
