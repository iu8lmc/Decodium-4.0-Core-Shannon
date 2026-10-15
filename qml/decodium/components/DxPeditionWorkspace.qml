/* DxPeditionWorkspace — DX-Pedition Mode 3-column tactical layout
 * Phase 2a scaffold (1.0.330): layout shell + easy reused panels + placeholders.
 * Opt-in alternative workspace; loaded only when mainWindow.dxPeditionMode is ON.
 * Design source of truth: dxped_handoff/README.md (§2 layout, §3 components),
 *   dx-pedition-mode.html. Colours/metrics come ONLY from theme tokens (Fase 1).
 * Phase 2b/3 will replace the Full Spectrum / Signal RX / PSK / Log placeholders.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: workspace

    // Passed from Main.qml. Children read global `bridge` directly too, but we keep
    // explicit handles so this component is self-describing / testable in isolation.
    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)
    property var engine: (typeof appEngine !== 'undefined' ? appEngine : null)
    // Vero quando il pannello TX e' staccato in finestra propria: in quel caso
    // la conferma di log resta a quello staccato, non a questo.
    property bool txPanelDetached: false

    // Wired by Main.qml's Loader.onLoaded — let the user leave the mode or open
    // Settings from inside the workspace (the classic footer/menu is collapsed here,
    // so without these the user would be trapped). See Main.qml dxPeditionLoader.
    signal requestExitDxPedition()
    signal requestOpenSettings()
    signal requestOpenLog()    // 1.0.344 — pulsante LOG header → finestra QSO Log
    signal requestOpenMam()    // 1.0.344 — finestra MAM (Multi-Answer Mode)
    signal requestOpenMacro()  // 1.0.345 — Macro Dialog (editor messaggi)
    signal requestOpenCat()    // 1.0.345 — CAT / Rig Control dialog

    // --- Theme token shortcuts (Fase 1) — NO hardcoded hex anywhere below ---------
    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:     tm ? tm.accentColor   : "#19ff88"
    readonly property color cAccentDim:  tm ? tm.accentDim     : "#0fa55a"
    readonly property color cAccentDeep: tm ? tm.accentDeep    : "#052d1a"
    readonly property color cPanel:      tm ? tm.panelColor    : "#0d1310"
    readonly property color cPanelHdr:   tm ? tm.panelHeader   : "#0a0e0c"
    readonly property color cBorder:     tm ? tm.borderColor   : "#1f2a22"
    readonly property color cBg:         tm ? tm.bgDeep        : "#050706"
    readonly property color cText:       tm ? tm.textPrimary   : "#d6dcd8"
    readonly property color cTextDim:    tm ? tm.textSecondary : "#6c7872"
    readonly property color cPile:       tm ? tm.pileColor     : "#66e6ff"
    readonly property color cGrid:       tm ? tm.gridColor     : "#00d4b4"
    readonly property color cTx:         tm ? tm.txColor       : "#ff7a5c"
    readonly property color cRx:         tm ? tm.rxColor       : "#19ff88"
    readonly property color cWarn:       tm ? tm.warningColor  : "#ffb84a"
    readonly property color cHot:        tm ? tm.errorColor    : "#ff5466"

    // Density metrics (Fase 1 invokables) with safe fallbacks.
    readonly property int densPanelH: tm ? tm.densityPanelHeight() : 30
    readonly property int densRowH:   tm ? tm.densityRowHeight()   : 22
    readonly property int densFont:   tm ? tm.densityFontSize()    : 12

    // =========================================================================
    // 1.0.569 — pannelli STACCABILI e INTERSCAMBIABILI.
    // Tecnica (la stessa dello swap del layout classico): i figli degli SplitView
    // sono 7 SLOT fissi — non cambiano mai, quindi maniglie e dimensioni restano
    // stabili — mentre i pannelli veri vivono in un pool, creati UNA volta sola.
    // Scambiare due pannelli = scambiare due voci nella mappa e RE-PARENTARE:
    // il contenuto non viene mai ricreato, cosi' il Waterfall non perde il feed
    // PCM e il TxPanel non perde il suo popup di conferma log.
    // =========================================================================
    readonly property var panelKeys: ["cluster", "psk", "waterfall", "fullspectrum", "signalrx", "tx", "log"]
    readonly property string defaultPanelOrder: "cluster,psk,waterfall,fullspectrum,signalrx,tx,log"
    property string panelOrder: defaultPanelOrder
    property var detachedKeys: []
    // Pannelli CHIUSI: non stanno ne' nello slot ne' in finestra. Lo slot
    // collassa (visible:false, come vuole SplitView) invece di restare un buco,
    // e il pannello torna nel pool, quindi smette anche di consumare CPU.
    property var hiddenKeys: []
    property var slotItems: []
    property bool layoutReady: false
    // Le finestre dei pannelli staccati non vanno mostrate mentre il workspace
    // e' ancora in costruzione: il Loader di Main.qml e' asincrono e una Window
    // figlia mostrata durante l'incubazione riceve da Qt un `closing` spurio
    // appena la finestra madre viene agganciata alla scena — che verrebbe letto
    // come "riaggancia" e annullerebbe da solo lo stacco appena ripristinato.
    property bool floatsReady: false

    function orderList() {
        var raw = String(workspace.panelOrder || "").split(",")
        var out = []
        for (var i = 0; i < raw.length; ++i) {
            var k = raw[i].trim()
            if (k.length > 0 && workspace.panelKeys.indexOf(k) >= 0 && out.indexOf(k) < 0)
                out.push(k)
        }
        // Reintegra gli id mancanti alla loro posizione di default: una mappa
        // salvata da una versione con meno pannelli non deve farne sparire.
        var def = workspace.defaultPanelOrder.split(",")
        for (var j = 0; j < def.length; ++j) {
            if (out.indexOf(def[j]) < 0)
                out.splice(Math.min(j, out.length), 0, def[j])
        }
        return out
    }

    function panelKeyForSlot(index) {
        var o = orderList()
        return (index >= 0 && index < o.length) ? o[index] : ""
    }

    function isDetached(key) {
        return workspace.detachedKeys.indexOf(key) >= 0
    }

    function isHidden(key) {
        return workspace.hiddenKeys.indexOf(key) >= 0
    }

    // Quanti pannelli sono chiusi in questo momento. Serve a dirlo sul pulsante
    // PANELS: un pannello che sparisce senza lasciare traccia e' un pannello
    // perso, e l'operatore si ritrova a cercarlo.
    readonly property int closedPanelCount: workspace.hiddenKeys.length

    // Tutti i pannelli si staccano. La finestra staccata carica una PROPRIA
    // istanza (vedi floatSourceFor): il pannello agganciato resta dov'e', solo
    // nascosto. Lo scambio fra slot continua a spostare l'istanza viva, ma li'
    // la finestra e la scena sono le stesse, quindi e' sicuro.
    function panelDetachable(key) {
        return key.length > 0 && floatSourceFor(key).length > 0
    }

    function floatSourceFor(key) {
        switch (key) {
        case "cluster":      return "DxPedClusterPanel.qml"
        case "psk":          return "PSKReporterPanel.qml"
        case "waterfall":    return "Waterfall.qml"
        case "fullspectrum": return "FullSpectrumPanel.qml"
        case "signalrx":     return "SignalRxPanel.qml"
        case "tx":           return "DxPedTxPanel.qml"
        case "log":          return "LogQsoPanel.qml"
        }
        return ""
    }

    // Chiude il pannello ovunque si trovi: se era staccato la finestra sparisce,
    // se era agganciato lo slot collassa. Nessuno spazio vuoto lasciato dietro.
    function closePanel(key) {
        if (!key || isHidden(key))
            return
        workspace.detachedKeys = workspace.detachedKeys.filter(function(k) { return k !== key })
        var l = workspace.hiddenKeys.slice()
        l.push(key)
        workspace.hiddenKeys = l
        applyLayout()
        persistLayout()
    }

    function showPanel(key) {
        if (!key || !isHidden(key))
            return
        workspace.hiddenKeys = workspace.hiddenKeys.filter(function(k) { return k !== key })
        applyLayout()
        persistLayout()
    }

    function togglePanel(key) {
        if (isHidden(key))
            showPanel(key)
        else
            closePanel(key)
    }

    function panelTitle(key) {
        switch (key) {
        case "cluster":      return poolCluster.leftTab === 0 ? qsTr("Cluster") : qsTr("MAM")
        case "psk":          return qsTr("PSK Reporter")
        case "waterfall":    return qsTr("Waterfall")
        case "fullspectrum": return qsTr("Full Spectrum · Decode")
        case "signalrx":     return qsTr("Signal RX · QSO Lock")
        case "tx":           return qsTr("DX-Ped TX · Slot")
        case "log":          return qsTr("Log · QSO Entry")
        }
        return ""
    }

    function panelMeta(key) {
        switch (key) {
        case "cluster":      return "live feed"
        case "psk":          return "heard-by"
        case "waterfall":    return "panadapter"
        case "fullspectrum": return "live"
        case "signalrx":     return "live"
        case "tx":           return (workspace.bridge && workspace.bridge.mamMultiStream)
                                    ? (workspace.bridge.mamActiveSlotCount + "/" + workspace.bridge.mamMaxStreams)
                                    : "single"
        case "log":          return "live"
        }
        return ""
    }

    function panelLive(key) {
        switch (key) {
        case "cluster":
        case "psk":
        case "fullspectrum":
        case "signalrx":
            return true
        case "tx":
            return workspace.bridge ? workspace.bridge.mamMultiStream : false
        }
        return false
    }

    function panelItemFor(key) {
        switch (key) {
        case "cluster":      return poolCluster
        case "psk":          return poolPsk
        case "waterfall":    return poolWaterfall
        case "fullspectrum": return poolFullSpectrum
        case "signalrx":     return poolSignalRx
        case "tx":           return poolTx
        case "log":          return poolLog
        }
        return null
    }

    function reparentPanel(item, host) {
        if (!item || !host || item.parent === host)
            return
        // Sganciare gli anchor PRIMA del cambio di parent: un anchor che punta
        // al vecchio parent dopo il re-parent e' un errore a runtime.
        item.anchors.fill = undefined
        item.parent = host
        item.anchors.fill = host
    }

    function applyLayout() {
        if (!workspace.layoutReady)
            return
        var o = orderList()
        for (var i = 0; i < o.length; ++i) {
            var key = o[i]
            var item = panelItemFor(key)
            if (!item)
                continue
            // Il pannello agganciato vive SEMPRE nel suo slot, anche quando e'
            // chiuso o mostrato in finestra: in quei casi lo slot e' invisibile,
            // quindi non disegna e non consuma (isFrameConsumer guarda proprio
            // la visibilita'). L'unico spostamento e' fra slot della stessa
            // finestra: mai fra finestre diverse.
            var host = (i < workspace.slotItems.length && workspace.slotItems[i])
                     ? workspace.slotItems[i].body : null
            reparentPanel(item, host)
        }
    }

    function persistLayout() {
        if (!workspace.bridge)
            return
        workspace.bridge.setSetting("uiDxPedPanelOrder", orderList().join(","))
        workspace.bridge.setSetting("uiDxPedDetachedPanels", workspace.detachedKeys.join(","))
        workspace.bridge.setSetting("uiDxPedHiddenPanels", workspace.hiddenKeys.join(","))
    }

    // Posizione e dimensione di ogni finestra staccata, per riaprirla dove
    // l'utente l'aveva lasciata. Chiamata dalla finestra stessa quando smette
    // di essere spostata o ridimensionata.
    function persistFloatGeometry(key, x, y, w, h) {
        if (!workspace.bridge || !key)
            return
        workspace.bridge.setSetting("uiDxPedFloat_" + key, [x, y, w, h].join(","))
    }

    function restoreFloatGeometry(win) {
        if (!workspace.bridge || !win || !win.panelKey)
            return
        var raw = workspace.bridge.getSetting("uiDxPedFloat_" + win.panelKey, "")
        var parts = Array.isArray(raw) ? raw : String(raw || "").split(",")
        if (parts.length < 4)
            return
        var x = parseInt(parts[0]), y = parseInt(parts[1])
        var w = parseInt(parts[2]), h = parseInt(parts[3])
        if (isNaN(x) || isNaN(y) || isNaN(w) || isNaN(h) || w < 120 || h < 100)
            return
        win.x = x
        win.y = y
        win.width = w
        win.height = h
    }

    function swapSlots(a, b) {
        var o = orderList()
        if (a < 0 || b < 0 || a >= o.length || b >= o.length || a === b)
            return
        var tmp = o[a]
        o[a] = o[b]
        o[b] = tmp
        workspace.panelOrder = o.join(",")
        applyLayout()
        persistLayout()
    }

    function detachPanel(key) {
        if (!key || isDetached(key) || !panelDetachable(key))
            return
        var l = workspace.detachedKeys.slice()
        l.push(key)
        workspace.detachedKeys = l
        applyLayout()
        persistLayout()
    }

    function dockPanel(key) {
        if (!key || !isDetached(key))
            return
        workspace.detachedKeys = workspace.detachedKeys.filter(function(k) { return k !== key })
        applyLayout()
        persistLayout()
    }

    function resetPanelLayout() {
        var det = workspace.detachedKeys.slice()
        for (var i = 0; i < det.length; ++i)
            dockPanel(det[i])
        workspace.hiddenKeys = []
        workspace.panelOrder = workspace.defaultPanelOrder
        applyLayout()
        persistLayout()
    }

    // ---- trascinamento per lo scambio ---------------------------------------
    property bool dragActive: false
    property int  dragFromSlot: -1
    property int  dragTargetSlot: -1
    property real dragX: 0
    property real dragY: 0
    property string dragTitle: ""

    function beginDrag(index, pt) {
        workspace.dragFromSlot = index
        workspace.dragTargetSlot = index
        workspace.dragTitle = panelTitle(panelKeyForSlot(index))
        workspace.dragX = pt.x
        workspace.dragY = pt.y
        workspace.dragActive = true
    }

    function updateDrag(pt) {
        if (!workspace.dragActive)
            return
        workspace.dragX = pt.x
        workspace.dragY = pt.y
        workspace.dragTargetSlot = slotIndexAt(pt.x, pt.y)
    }

    function slotIndexAt(x, y) {
        for (var i = 0; i < workspace.slotItems.length; ++i) {
            var s = workspace.slotItems[i]
            if (!s || !s.visible || s.width <= 0 || s.height <= 0)
                continue
            var p = workspace.mapToItem(s, x, y)
            if (p.x >= 0 && p.y >= 0 && p.x <= s.width && p.y <= s.height)
                return i
        }
        return -1
    }

    function endDrag() {
        if (workspace.dragActive
            && workspace.dragTargetSlot >= 0
            && workspace.dragTargetSlot !== workspace.dragFromSlot) {
            swapSlots(workspace.dragFromSlot, workspace.dragTargetSlot)
        }
        cancelDrag()
    }

    function cancelDrag() {
        workspace.dragActive = false
        workspace.dragFromSlot = -1
        workspace.dragTargetSlot = -1
    }

    Component.onCompleted: {
        workspace.slotItems = [dxSlot0, dxSlot1, dxSlot2, dxSlot3, dxSlot4, dxSlot5, dxSlot6]
        if (workspace.bridge) {
            // getSetting puo' restituire una lista invece di una stringa quando il
            // valore salvato contiene virgole e non e' quotato: normalizziamo.
            function asCsv(v) {
                if (v === undefined || v === null) return ""
                return Array.isArray(v) ? v.join(",") : String(v)
            }
            var savedOrder = asCsv(workspace.bridge.getSetting("uiDxPedPanelOrder", ""))
            if (savedOrder.length > 0)
                workspace.panelOrder = savedOrder
            var savedDetached = asCsv(workspace.bridge.getSetting("uiDxPedDetachedPanels", ""))
            workspace.detachedKeys = savedDetached.length > 0
                ? savedDetached.split(",").filter(function(k) {
                      return workspace.panelKeys.indexOf(k) >= 0 && workspace.panelDetachable(k)
                  })
                : []
            var savedHidden = asCsv(workspace.bridge.getSetting("uiDxPedHiddenPanels", ""))
            workspace.hiddenKeys = savedHidden.length > 0
                ? savedHidden.split(",").filter(function(k) { return workspace.panelKeys.indexOf(k) >= 0 })
                : []
        }
        workspace.layoutReady = true
        applyLayout()
    }

    // Sblocca le finestre flottanti solo quando la finestra madre e' davvero
    // sullo schermo (vedi floatsReady): un ritardo fisso non basta, la main
    // window di Decodium compare dopo ~3.5 s e mostrarne prima una transiente
    // si prende il `closing` spurio del re-parent.
    readonly property var hostWindow: workspace.Window.window

    Timer {
        interval: 800
        repeat: false
        running: !workspace.floatsReady
                 && workspace.hostWindow !== null
                 && workspace.hostWindow !== undefined
                 && workspace.hostWindow.visible
        onTriggered: {
            workspace.floatsReady = true
            workspace.applyLayout()
        }
    }

    // Background fill so the classic chrome never shows through behind the shell.
    Rectangle { anchors.fill: parent; color: workspace.cBg }

    // ---------------------------------------------------------------------------
    // Slot fisso del workspace: cornice + header con il titolo del pannello che
    // ospita in QUEL momento, maniglia di scambio e pulsante stacca. Il corpo
    // nasce vuoto: il pannello vero ci viene riparentato dentro da applyLayout().
    // ---------------------------------------------------------------------------
    component DxSlot: Rectangle {
        id: slot
        property int slotIndex: -1
        readonly property string panelKey: workspace.panelKeyForSlot(slot.slotIndex)
        readonly property bool detached: workspace.isDetached(slot.panelKey)
        // Lo slot esiste solo se il pannello e' qui: chiuso o staccato collassa,
        // e SplitView ridistribuisce lo spazio agli altri.
        visible: slot.panelKey.length > 0
                 && !workspace.isHidden(slot.panelKey)
                 && !workspace.isDetached(slot.panelKey)
        readonly property bool dragSource: workspace.dragActive
                                        && workspace.dragFromSlot === slot.slotIndex
        readonly property bool dropTarget: workspace.dragActive
                                        && workspace.dragTargetSlot === slot.slotIndex
                                        && workspace.dragFromSlot !== slot.slotIndex
        property alias body: slotBody

        radius: 10
        color: workspace.cPanel
        border.color: slot.dropTarget ? workspace.cAccent : workspace.cBorder
        border.width: slot.dropTarget ? 2 : 1
        opacity: slot.dragSource ? 0.55 : 1.0
        clip: true

        Rectangle {
            id: slotHdr
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: workspace.densPanelH
            color: workspace.cPanelHdr
            radius: 10
            // Squadra gli angoli bassi.
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 10
                color: parent.color
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                spacing: 8

                // Maniglia: lo scambio si trascina SOLO da qui, mai dal corpo.
                Text {
                    text: "⠿"
                    color: (gripMA.containsMouse || slot.dragSource) ? workspace.cAccent : workspace.cTextDim
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip {
                        visible: gripMA.containsMouse && !workspace.dragActive
                        delay: 500
                        text: qsTr("Drag onto another panel to swap the two")
                    }
                    MouseArea {
                        id: gripMA
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        preventStealing: true
                        cursorShape: workspace.dragActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        onPressed: function(mouse) {
                            workspace.beginDrag(slot.slotIndex, mapToItem(workspace, mouse.x, mouse.y))
                        }
                        onPositionChanged: function(mouse) {
                            workspace.updateDrag(mapToItem(workspace, mouse.x, mouse.y))
                        }
                        onReleased: workspace.endDrag()
                        onCanceled: workspace.cancelDrag()
                    }
                }

                Rectangle {
                    visible: workspace.panelLive(slot.panelKey)
                    width: 8; height: 8; radius: 4
                    color: workspace.cAccent
                    SequentialAnimation on opacity {
                        running: workspace.panelLive(slot.panelKey)
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.25; duration: 800 }
                        NumberAnimation { from: 0.25; to: 1.0; duration: 800 }
                    }
                }

                Text {
                    text: workspace.panelTitle(slot.panelKey).toUpperCase()
                    color: workspace.cAccent
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.4
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: workspace.panelMeta(slot.panelKey)
                    visible: text.length > 0
                    color: workspace.cTextDim
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignVCenter
                }

                // Stacca in finestra propria (non per il Waterfall).
                Text {
                    text: "↗"
                    visible: workspace.panelDetachable(slot.panelKey)
                    color: detachMA.containsMouse ? workspace.cAccent : workspace.cTextDim
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip {
                        visible: detachMA.containsMouse
                        delay: 500
                        text: qsTr("Open this panel in a window of its own")
                    }
                    MouseArea {
                        id: detachMA
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: workspace.detachPanel(slot.panelKey)
                    }
                }

                // Chiude il pannello: lo slot collassa, si riapre da PANELS.
                Text {
                    text: "✕"
                    color: closeMA.containsMouse ? workspace.cHot : workspace.cTextDim
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip {
                        visible: closeMA.containsMouse
                        delay: 500
                        // Dire DOVE va a finire, non solo che si chiude.
                        text: qsTr("Close this panel - reopen it from PANELS in the top bar")
                    }
                    MouseArea {
                        id: closeMA
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: workspace.closePanel(slot.panelKey)
                    }
                }
            }
        }

        // Contenitore del pannello ospitato.
        Item {
            id: slotBody
            anchors { left: parent.left; right: parent.right; top: slotHdr.bottom; bottom: parent.bottom }
            anchors.margins: 1
        }

    }

    // ===========================================================================
    // Outer 4-row stack: header 60 / band 32 / main 1fr / status 28, gap 12, pad 12
    // ===========================================================================
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // -------- ROW 1 — HEADER (60px) ----------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            radius: 10
            color: workspace.cPanel
            border.color: workspace.cBorder
            border.width: 1

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 16

                // Brand block.
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 26; height: 26; radius: 6
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: workspace.cAccent }
                            GradientStop { position: 1.0; color: workspace.cAccentDim }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "DX"; color: workspace.cBg
                            font.pixelSize: 11; font.bold: true
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: qsTr("DECODIUM 4")
                            color: workspace.cText
                            font.pixelSize: 14; font.bold: true; font.letterSpacing: 1.4
                        }
                        Text {
                            text: qsTr("DX-PEDITION MODE")
                            color: workspace.cTextDim
                            font.pixelSize: 9; font.letterSpacing: 1.6
                        }
                    }
                }

                // Mode pill.
                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: modeLbl.implicitWidth + 16
                    implicitHeight: 20
                    radius: 4
                    color: workspace.cAccentDeep
                    border.color: workspace.cAccentDim
                    border.width: 1
                    Text {
                        id: modeLbl
                        anchors.centerIn: parent
                        text: (workspace.bridge && workspace.bridge.mode ? String(workspace.bridge.mode) : "FT8")
                        color: workspace.cAccent
                        font.pixelSize: 10; font.bold: true
                        font.family: decodiumMonoFontFamily
                    }
                }

                // Large frequency display.
                // Con lo scaling al 175% su una finestra da ~1130 px logici la
                // riga non ci stava e la barra dei comandi finiva OLTRE il bordo
                // destro: PANELS e compagni diventavano irraggiungibili, e con
                // loro il modo di riaprire un pannello chiuso. Adesso a cedere e'
                // la frequenza — si rimpicciolisce e al limite si tronca —
                // mentre orologio e comandi tengono la loro dimensione.
                Text {
                    id: freqDisplay
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    elide: Text.ElideRight
                    text: {
                        var hz = workspace.bridge && workspace.bridge.dialFrequency !== undefined
                                 ? Number(workspace.bridge.dialFrequency) : 0
                        if (!hz && workspace.bridge && workspace.bridge.frequency !== undefined)
                            hz = Number(workspace.bridge.frequency)
                        if (!hz) return "—.———.———"
                        var s = String(Math.round(hz))
                        // group thousands with dots: 18108000 -> 18.108.000
                        var out = ""; var c = 0
                        for (var i = s.length - 1; i >= 0; i--) {
                            out = s.charAt(i) + out
                            if (++c % 3 === 0 && i > 0) out = "." + out
                        }
                        return out
                    }
                    color: workspace.cAccent
                    // Tre scalini invece di una dimensione fissa: su una finestra
                    // stretta la frequenza resta leggibile e lascia passare i
                    // comandi.
                    font.pixelSize: workspace.width > 1500 ? 36
                                  : (workspace.width > 1150 ? 28 : 22)
                    font.bold: true
                    font.family: decodiumMonoFontFamily
                }

                // UTC clock.
                ColumnLayout {
                    spacing: 0
                    Layout.alignment: Qt.AlignVCenter
                    Text {
                        id: utcClock
                        property string t: "00:00:00"
                        text: t
                        color: workspace.cAccent
                        font.pixelSize: 22; font.bold: true; font.family: decodiumMonoFontFamily
                        Timer {
                            interval: 1000; running: workspace.visible; repeat: true; triggeredOnStart: true
                            onTriggered: {
                                var d = new Date()
                                function p(n) { return (n < 10 ? "0" : "") + n }
                                utcClock.t = p(d.getUTCHours()) + ":" + p(d.getUTCMinutes()) + ":" + p(d.getUTCSeconds())
                            }
                        }
                    }
                    Text {
                        text: qsTr("UTC · OPER IU8LMC")
                        color: workspace.cTextDim
                        font.pixelSize: 10; font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                    }
                }

                // Mini toolbar. SETUP opens Settings; others are placeholders (2b).
                RowLayout {
                    spacing: 6
                    Layout.alignment: Qt.AlignVCenter
                    Repeater {
                        model: ["PANELS", "SETUP", "LOG", "MAM", "MACRO", "CAT"]
                        delegate: Rectangle {
                            id: tbBtn
                            required property string modelData
                            readonly property bool isPanels: tbBtn.modelData === "PANELS"
                            readonly property bool flagged: tbBtn.isPanels
                                                         && workspace.closedPanelCount > 0
                            // Riferirsi al delegate per id e non tramite `parent`:
                            // dentro un delegate con proprieta' richieste quella
                            // catena non risolve, e i pulsanti restavano larghi
                            // 16 px con il testo vuoto — la barra c'era ma non si
                            // vedeva, e con essa spariva l'unico modo di riaprire
                            // un pannello chiuso.
                            implicitWidth: tbTxt.implicitWidth + 16
                            implicitHeight: 32
                            radius: 6
                            color: tbMA.containsMouse ? workspace.cAccentDeep : "transparent"
                            border.color: tbBtn.flagged ? workspace.cWarn : workspace.cBorder
                            border.width: 1
                            Text {
                                id: tbTxt
                                anchors.centerIn: parent
                                text: tbBtn.flagged
                                      ? tbBtn.modelData + "  " + workspace.closedPanelCount
                                      : tbBtn.modelData
                                color: tbBtn.flagged
                                       ? workspace.cWarn
                                       : (tbMA.containsMouse ? workspace.cAccent : workspace.cTextDim)
                                font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.0
                            }
                            ToolTip {
                                visible: tbMA.containsMouse && tbBtn.isPanels
                                delay: 400
                                text: workspace.closedPanelCount > 0
                                      ? qsTr("%1 closed panel(s). Open this to bring them back.")
                                            .arg(workspace.closedPanelCount)
                                      : qsTr("Shows where every panel is, and reopens the closed ones.")
                            }
                            MouseArea {
                                id: tbMA; anchors.fill: parent; hoverEnabled: true
                                // 1.0.344 — SETUP/LOG/MAM cablati; MACRO/CAT placeholder.
                                // 1.0.345 — tutti i pulsanti header cablati.
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (tbBtn.modelData === "PANELS")
                                        panelsPopup.open()
                                    else if (tbBtn.modelData === "SETUP")
                                        workspace.requestOpenSettings()
                                    else if (tbBtn.modelData === "LOG")
                                        workspace.requestOpenLog()
                                    else if (tbBtn.modelData === "MAM")
                                        workspace.requestOpenMam()
                                    else if (tbBtn.modelData === "MACRO")
                                        workspace.requestOpenMacro()
                                    else if (tbBtn.modelData === "CAT")
                                        workspace.requestOpenCat()
                                }
                            }
                        }
                    }

                    // EXIT — leave DX-Pedition Mode and return to the classic layout.
                    // Without this the user is trapped (classic footer/menu is hidden).
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: exitTxt.implicitWidth + 22
                        implicitHeight: 32
                        radius: 6
                        color: exitMA.containsMouse ? workspace.cHot : "transparent"
                        border.color: workspace.cHot
                        border.width: 1
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 5
                            Text {
                                text: "✕"
                                color: exitMA.containsMouse ? workspace.cBg : workspace.cHot
                                font.pixelSize: 12; font.bold: true
                            }
                            Text {
                                id: exitTxt
                                text: "EXIT"
                                color: exitMA.containsMouse ? workspace.cBg : workspace.cHot
                                font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.0
                            }
                        }
                        MouseArea {
                            id: exitMA; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: workspace.requestExitDxPedition()
                        }
                    }
                }
            }
        }

        // -------- ROW 2 — BAND STRIP (32px) ------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            radius: 8
            color: workspace.cPanel
            border.color: workspace.cBorder
            border.width: 1

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 6

                Text {
                    text: "BAND"
                    color: workspace.cTextDim
                    font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.6
                    Layout.alignment: Qt.AlignVCenter
                }

                Repeater {
                    model: ["160","80","60","40","30","20","17","15","12","10","6","4","2","70"]
                    delegate: Rectangle {
                        required property string modelData
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 34
                        implicitHeight: 24
                        radius: 4
                        property bool active: false   // wired to current band in 2b
                        color: active ? workspace.cAccentDeep
                                      : (bandMA.containsMouse ? workspace.cPanelHdr : "transparent")
                        border.color: active ? workspace.cAccentDim : workspace.cBorder
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: parent.active ? workspace.cAccent : workspace.cTextDim
                            font.pixelSize: 11; font.bold: true; font.family: decodiumMonoFontFamily
                        }
                        MouseArea {
                            id: bandMA; anchors.fill: parent; hoverEnabled: true
                            onClicked: {
                                // Placeholder: real band switch (CAT) wired in 2b.
                                if (workspace.bridge && typeof workspace.bridge.setBand === "function")
                                    workspace.bridge.setBand(parent.modelData)
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Two status chips (placeholder text — real cycle/seq state in 2b).
                Repeater {
                    model: ["● RX ACTIVE", "● AUTO-SEQ ON"]
                    delegate: Rectangle {
                        required property string modelData
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: chipTxt.implicitWidth + 14
                        implicitHeight: 22
                        radius: 4
                        color: workspace.cPanelHdr
                        border.color: workspace.cBorder
                        border.width: 1
                        Text {
                            id: chipTxt
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: workspace.cTextDim
                            font.pixelSize: 10; font.family: decodiumMonoFontFamily
                        }
                    }
                }
            }
        }

        // -------- ROW 3 — MAIN 3 COLUMNS (resizable) ---------------------------
        // 1.0.342 — SplitView annidati: orizzontale per le 3 colonne (resize
        // larghezza), verticale dentro ogni colonna (resize altezza).
        // 1.0.569 — i figli degli SplitView sono SLOT fissi: i pannelli ci
        // vengono riparentati dentro secondo panelOrder, quindi si scambiano
        // senza mai toccare la struttura (niente maniglie duplicate).
        SplitView {
            id: mainSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                color: SplitHandle.pressed ? workspace.cAccent
                     : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                Rectangle {
                    anchors.centerIn: parent
                    width: 2; height: 28; radius: 1
                    color: workspace.cBorder
                }
            }

            // ---- COL LEFT ------------------------------------------------------
            SplitView {
                id: colLeftSplit
                orientation: Qt.Vertical
                // Colonna senza piu' pannelli: sparisce, cosi' le altre due si
                // prendono tutta la larghezza invece di lasciare una striscia.
                visible: dxSlot0.visible || dxSlot1.visible
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 240
                handle: Rectangle {
                    implicitWidth: 8; implicitHeight: 8
                    color: SplitHandle.pressed ? workspace.cAccent
                         : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                    Rectangle { anchors.centerIn: parent; width: 28; height: 2; radius: 1; color: workspace.cBorder }
                }

                DxSlot {
                    id: dxSlot0
                    slotIndex: 0
                    SplitView.preferredHeight: 360
                    SplitView.minimumHeight: 120
                }
                DxSlot {
                    id: dxSlot1
                    slotIndex: 1
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 100
                }
            }

            // ---- COL CENTER ----------------------------------------------------
            SplitView {
                id: colCenterSplit
                orientation: Qt.Vertical
                visible: dxSlot2.visible || dxSlot3.visible
                SplitView.fillWidth: true
                SplitView.minimumWidth: 360
                handle: Rectangle {
                    implicitWidth: 8; implicitHeight: 8
                    color: SplitHandle.pressed ? workspace.cAccent
                         : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                    Rectangle { anchors.centerIn: parent; width: 28; height: 2; radius: 1; color: workspace.cBorder }
                }

                DxSlot {
                    id: dxSlot2
                    slotIndex: 2
                    SplitView.preferredHeight: 340
                    SplitView.minimumHeight: 160
                }
                DxSlot {
                    id: dxSlot3
                    slotIndex: 3
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 120
                }
            }

            // ---- COL RIGHT -----------------------------------------------------
            SplitView {
                id: colRightSplit
                orientation: Qt.Vertical
                visible: dxSlot4.visible || dxSlot5.visible || dxSlot6.visible
                SplitView.preferredWidth: 520
                SplitView.minimumWidth: 380
                handle: Rectangle {
                    implicitWidth: 8; implicitHeight: 8
                    color: SplitHandle.pressed ? workspace.cAccent
                         : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                    Rectangle { anchors.centerIn: parent; width: 28; height: 2; radius: 1; color: workspace.cBorder }
                }

                DxSlot {
                    id: dxSlot4
                    slotIndex: 4
                    SplitView.preferredHeight: 260
                    SplitView.minimumHeight: 140
                }
                DxSlot {
                    id: dxSlot5
                    slotIndex: 5
                    SplitView.preferredHeight: 420
                    SplitView.minimumHeight: 220
                }
                DxSlot {
                    id: dxSlot6
                    slotIndex: 6
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 100
                }
            }
        }

        // -------- ROW 4 — STATUS BAR (28px) ------------------------------------
        StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            audioLevel: workspace.bridge ? workspace.bridge.audioLevel : 0.0
            signalLevel: workspace.bridge ? workspace.bridge.sMeter : 0.0
            monitoring: workspace.bridge ? workspace.bridge.monitoring : false
            transmitting: workspace.bridge ? workspace.bridge.transmitting : false
            tuning: workspace.bridge ? workspace.bridge.tuning : false
            decoding: workspace.bridge ? workspace.bridge.decoding : false
            catStatus: (workspace.bridge && workspace.bridge.catConnected) ? "Connected" : "Disconnected"
        }
    }

    // =========================================================================
    // POOL — i pannelli veri. Creati una volta sola e riparentati negli slot (o
    // nelle finestre flottanti). NON sono figli di uno SplitView: metterceli
    // direttamente creerebbe maniglie fantasma, lezione dello swap classico.
    // =========================================================================
    Item {
        id: panelPool
        width: 0
        height: 0
        visible: false

        // Cluster + MAM a schede (estratto in DxPedClusterPanel.qml, cosi' la
        // finestra staccata puo' istanziarne uno suo).
        DxPedClusterPanel {
            id: poolCluster
            engine: workspace.engine
        }

        PSKReporterPanel { id: poolPsk }

        Waterfall {
            id: poolWaterfall
            showControls: true
        }

        FullSpectrumPanel { id: poolFullSpectrum }

        SignalRxPanel { id: poolSignalRx }

        DxPedTxPanel {
            id: poolTx
            engine: workspace.engine
            // In DX-Pedition il pannello TX di Main.qml e' dentro un contenitore
            // invisibile e la sua guardia lo esclude: se anche questo restasse a
            // false, la conferma di log non si aprirebbe piu' e il QSO
            // sembrerebbe non registrato.
            handleLogPrompt: !workspace.txPanelDetached
            // 1.0.344 — il pulsante MAM apre la finestra MAM (propagata a
            // Main.qml, che possiede l'istanza MamWindow).
            onMamWindowRequested: workspace.requestOpenMam()
        }

        LogQsoPanel { id: poolLog }
    }

    // ---- finestre dei pannelli staccati -------------------------------------
    // Una finestra STATICA per pannello, nascosta finche' il pannello e'
    // agganciato. Niente creazione/distruzione dinamica: cosi' non esiste la
    // corsa fra il re-parent del contenuto e la distruzione della finestra che
    // lo contiene, e il corpo di destinazione e' sempre pronto.
    component DxPedFloat: DxPedFloatWindow {
        bgColor: workspace.cPanel
        borderColor: workspace.cBorder
        accentColor: workspace.cAccent
        textDim: workspace.cTextDim
        panelTitle: workspace.panelTitle(panelKey)
        contentSource: workspace.floatSourceFor(panelKey)
        visible: workspace.floatsReady && workspace.isDetached(panelKey)
        onDockRequested: workspace.dockPanel(panelKey)
        onCloseRequested: workspace.closePanel(panelKey)
        onGeometrySettled: workspace.persistFloatGeometry(panelKey, x, y, width, height)
        Component.onCompleted: workspace.restoreFloatGeometry(this)
    }

    DxPedFloat { id: floatCluster;      panelKey: "cluster" }
    DxPedFloat { id: floatPsk;          panelKey: "psk" }
    DxPedFloat { id: floatWaterfall;    panelKey: "waterfall"; width: 900; height: 480 }
    DxPedFloat { id: floatFullSpectrum; panelKey: "fullspectrum"; width: 780; height: 420 }
    DxPedFloat { id: floatSignalRx;     panelKey: "signalrx" }
    DxPedFloat { id: floatTx;           panelKey: "tx"; width: 620; height: 620 }
    DxPedFloat { id: floatLog;          panelKey: "log" }

    // ---- menu PANELS: stato dei pannelli e riapertura di quelli chiusi ------
    Popup {
        id: panelsPopup
        parent: workspace
        x: workspace.width - width - 24
        y: 78
        width: 320
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: workspace.cPanel
            border.color: workspace.cAccentDim
            border.width: 1
            radius: 8
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            Text {
                text: qsTr("PANELS")
                color: workspace.cAccent
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }
            Text {
                text: qsTr("Click a panel to close or reopen it. A closed panel frees its slot instead of leaving a gap.")
                color: workspace.cTextDim
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.bottomMargin: 2
            }

            Repeater {
                model: workspace.panelKeys
                delegate: Rectangle {
                    id: panelRow
                    required property string modelData
                    readonly property bool closed: workspace.isHidden(panelRow.modelData)
                    readonly property bool floated: workspace.isDetached(panelRow.modelData)
                    Layout.fillWidth: true
                    implicitHeight: 26
                    radius: 4
                    color: rowMA.containsMouse ? workspace.cAccentDeep : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 8

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: panelRow.closed ? workspace.cBorder : workspace.cAccent
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Text {
                            text: workspace.panelTitle(panelRow.modelData)
                            color: panelRow.closed ? workspace.cTextDim : workspace.cText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Text {
                            text: panelRow.closed ? qsTr("closed")
                                                  : (panelRow.floated ? qsTr("window") : qsTr("docked"))
                            color: workspace.cTextDim
                            font.pixelSize: 9
                            font.letterSpacing: 0.8
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: rowMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: workspace.togglePanel(panelRow.modelData)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 2
                color: workspace.cBorder
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 24
                radius: 4
                color: resetMA.containsMouse ? workspace.cAccentDeep : "transparent"
                border.color: workspace.cBorder
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: qsTr("RESTORE DEFAULT LAYOUT")
                    color: workspace.cAccent
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                MouseArea {
                    id: resetMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        workspace.resetPanelLayout()
                        panelsPopup.close()
                    }
                }
            }
        }
    }

    // ---- strato di trascinamento: fantasma sopra a tutto --------------------
    Item {
        anchors.fill: parent
        z: 1000
        visible: workspace.dragActive

        Rectangle {
            x: workspace.dragX - width / 2
            y: workspace.dragY - height / 2
            width: ghostTxt.implicitWidth + 28
            height: 26
            radius: 6
            color: workspace.cAccentDeep
            border.color: workspace.cAccent
            border.width: 1
            opacity: 0.92
            Text {
                id: ghostTxt
                anchors.centerIn: parent
                text: workspace.dragTitle.toUpperCase()
                color: workspace.cAccent
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.2
            }
        }
    }

}
