/* Waterfall — Decodium3 High-Resolution Panadapter
 * Uses PanadapterItem (QQuickItem + QSGNode, FFTW 4096-bin, SmartSDR style)
 * By IU8LMC — 2025
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// import Qt.labs.settings 1.1  // non disponibile in questa build Qt
import Decodium 1.0

Item {
    id: waterfallPanel

    signal frequencySelected(int freq)       // Destro = RX
    signal txFrequencySelected(int freq)     // Sinistro = TX

    property bool showControls: true
    property bool controlsExpanded: true
    property int  minFreq: 200
    property int  maxFreq: 3200
    property int  spectrumHeight: 150
    property bool restoringSettings: false
    property bool syncingPaletteChoice: false
    property bool showDecodeCallsigns: true
    property var spectrumDecodeLabels: []
    property bool dxClusterRefreshPending: false
    readonly property bool ft2LinkMode: bridge && String(bridge.mode || "").toUpperCase() === "FT2-LINK"

    // Altezza minima/massima del grafico spettro (regolabile tramite drag)
    // 1.0.288 — vincoli rilassati: spettro e cascata ridimensionabili quasi liberamente
    // (decidere quale aprire di più). Spettro 40..(altezza-24), cascata ≥24.
    readonly property bool controlsVisible: showControls && controlsExpanded
    readonly property int spectrumMinHeight: 40
    // IU8LMC FIX — era un 4000 FISSO: il drag poteva portare spectrumHeight molto oltre
    // l'altezza reale del pannello (es. 2808 salvato con pannello alto 152). Il render
    // (PanadapterItem.spectrumHeight, sotto) clampa a height-waterfallMinHeight, quindi il
    // valore restava fuori scala e trascinare la barra non cambiava NULLA -> "barra bloccata".
    // Ora il tetto del drag coincide col tetto del render: si auto-ripara al primo drag.
    // Finche' il layout non e' pronto (height<=0) resta permissivo (4000) per non
    // clampare a 40 durante il restore delle impostazioni (onSettingValueChanged).
    readonly property int spectrumMaxHeight: waterfallDisplay.height > 0
                                             ? Math.max(spectrumMinHeight,
                                                        waterfallDisplay.height - waterfallMinHeight)
                                             : 4000
    readonly property int waterfallMinHeight: controlsVisible ? 24 : 12

    // Colors
    property color bgDeep:      bridge.themeManager.bgDeep
    property color bgPanel:     Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
    property color accentCyan:  bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSec:     Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
    property color borderColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
    // Theme-aware toolbar palette (was hardcoded dark before).
    property color wfText:        bridge.themeManager.isLightTheme ? bridge.themeManager.textPrimary    : "#FFFFFF"
    property color wfToolbarBg:   bridge.themeManager.isLightTheme ? bridge.themeManager.panelHeader    : Qt.rgba(0,0,0,0.75)
    property color wfTrack:       bridge.themeManager.isLightTheme ? bridge.themeManager.borderSoft     : "#1a2a3a"
    property color wfFrame:       bridge.themeManager.isLightTheme ? bridge.themeManager.borderColor    : "#3a5470"
    property color wfYellow:      bridge.themeManager.isLightTheme ? bridge.themeManager.warningColor   : "#ffcc00"
    property color wfBlue:        bridge.themeManager.isLightTheme ? bridge.themeManager.primaryColor   : "#88BBDD"
    property color wfPurple:      bridge.themeManager.isLightTheme ? bridge.themeManager.ledMagenta     : "#AA88DD"
    property color wfSlate:       bridge.themeManager.isLightTheme ? bridge.themeManager.textSecondary  : "#5888AA"

    readonly property var labelColorPresets: [
        { name: "Auto",    color: "#00E6FF", custom: false },
        { name: "Cyan",    color: "#00E6FF", custom: true  },
        { name: "White",   color: "#FFFFFF", custom: true  },
        { name: "Yellow",  color: "#FFE600", custom: true  },
        { name: "Green",   color: "#00E664", custom: true  },
        { name: "Magenta", color: "#FF64FF", custom: true  },
        { name: "Orange",  color: "#FF9600", custom: true  }
    ]

    function applyManualContrast() {
        if (waterfallDisplay && !autoRangeCheck.checked) {
            waterfallDisplay.minDb = waterfallDisplay.measuredFloor - (150 - contrastSlider.value)
        }
    }

    function coerceBool(value, fallback) {
        if (value === true || value === false)
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off" || text.length === 0)
            return false
        return !!fallback
    }

    function boolSetting(key, fallback) {
        return coerceBool(bridge.getSetting(key, fallback), fallback)
    }

    function clampNumber(value, minValue, maxValue, fallback) {
        var n = Number(value)
        if (!isFinite(n))
            n = fallback
        return Math.max(minValue, Math.min(maxValue, n))
    }

    function clampIndex(value, count, fallback) {
        return Math.round(clampNumber(value, 0, Math.max(0, count - 1), fallback))
    }

    function clampPaletteIndex(value) {
        var count = 0
        if (paletteCombo && paletteCombo.count !== undefined)
            count = paletteCombo.count
        if ((!count || count < 1) && waterfallDisplay && waterfallDisplay.paletteNames)
            count = waterfallDisplay.paletteNames.length
        return clampIndex(value, Math.max(1, count), 0)
    }

    function scheduleGraphSave() {
        if (mainWindow && mainWindow.scheduleSave)
            mainWindow.scheduleSave()
    }

    function persistGraphSetting(key, value) {
        bridge.setSetting(key, value)
        scheduleGraphSave()
    }

    function isValidClickableAudioFreq(freq) {
        return freq >= 100 && freq <= 5000
    }

    function setPaletteIndex(index, persist) {
        if (syncingPaletteChoice)
            return
        syncingPaletteChoice = true

        var next = clampPaletteIndex(index)
        if (paletteCombo.currentIndex !== next)
            paletteCombo.currentIndex = next
        if (!bridge.themeManager.isLightTheme && waterfallDisplay.paletteIndex !== next)
            waterfallDisplay.paletteIndex = next
        if (bridge.uiPaletteIndex !== next)
            bridge.uiPaletteIndex = next

        if (persist === undefined)
            persist = !waterfallPanel.restoringSettings
        if (persist)
            persistGraphSetting("uiPaletteIndex", next)

        syncingPaletteChoice = false
    }

    function loadPanadapterSettings() {
        restoringSettings = true
        waterfallPanel.controlsExpanded = boolSetting("uiWaterfallControlsExpanded", true)
        if (bridge.uiSpectrumHeight > 0) waterfallPanel.spectrumHeight = bridge.uiSpectrumHeight
        waterfallPanel.setPaletteIndex(bridge.getSetting("uiPaletteIndex", bridge.uiPaletteIndex), false)
        autoRangeCheck.checked = boolSetting("uiWaterfallAutoRange", true)
        txBracketsCheck.checked = boolSetting("uiWaterfallShowTxBrackets", true)
        peakHoldCheck.checked = boolSetting("uiWaterfallPeakHold", true)
        blackSlider.value = bridge.getSetting("uiWaterfallBlackLevel", 15)
        gainSlider.value = bridge.getSetting("uiWaterfallColorGain", 50)
        contrastSlider.value = bridge.getSetting("uiWaterfallContrast", 80)
        speedSlider.value = bridge.getSetting("spectrumInterval", 20)
        zoomSlider.value = bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0
        labelFontSlider.value = bridge.getSetting("uiLabelFontSize", 8)
        labelSpacingSlider.value = bridge.getSetting("uiLabelSpacing", 2)
        labelBoldCheck.checked = boolSetting("uiLabelBold", true)
        labelColorCombo.currentIndex = Math.max(0, Math.min(labelColorPresets.length - 1,
                                           bridge.getSetting("uiLabelColorPreset", 0)))
        waterfallPanel.setShowDecodeCallsigns(boolSetting("uiWaterfallShowCallsigns", true), false)
        dxClusterCheck.checked = boolSetting("uiWaterfallShowDxCluster", false)
        waterfallDisplay.showDxClusterSpots = dxClusterCheck.checked
        // Spento per impostazione predefinita: costa vertici e su macchine
        // modeste si deve poter accendere solo di proposito.
        spectrum3dToggle.checked = boolSetting("uiSpectrum3d", false)
        waterfallDisplay.spectrum3d = spectrum3dToggle.checked
        traces3dSlider.value = bridge.getSetting("uiSpectrum3dTraces", 28)
        floor3dSlider.value = bridge.getSetting("uiSpectrum3dFloorDepth", 6)
        waterfallDisplay.spectrum3dTraces = traces3dSlider.value
        waterfallDisplay.spectrum3dFloorDepth = floor3dSlider.value
        noiseCutSlider.value = bridge.getSetting("uiNoiseFloorPercentile", 10)
        waterfallDisplay.noiseFloorPercentile = noiseCutSlider.value

        // In light theme la palette è forzata a 11 (mockup pastello). Non sovrascrivere col valore Settings.
        waterfallDisplay.autoRange = autoRangeCheck.checked
        waterfallDisplay.showTxBrackets = txBracketsCheck.checked
        waterfallDisplay.peakHold = peakHoldCheck.checked
        waterfallDisplay.blackLevel = blackSlider.value
        waterfallDisplay.colorGain = gainSlider.value
        waterfallDisplay.contrastLevel = contrastSlider.value
        waterfallDisplay.zoomFactor = zoomSlider.value
        waterfallDisplay.labelFontSize = labelFontSlider.value
        waterfallDisplay.labelSpacing = labelSpacingSlider.value
        waterfallDisplay.labelBold = labelBoldCheck.checked
        var preset = labelColorPresets[labelColorCombo.currentIndex]
        waterfallDisplay.labelUseCustomColor = preset.custom
        waterfallDisplay.labelColor = preset.color
        waterfallPanel.applyManualContrast()
        restoringSettings = false
    }

    function setControlsExpanded(expanded) {
        if (waterfallPanel.controlsExpanded === expanded)
            return
        waterfallPanel.controlsExpanded = expanded
        waterfallPanel.persistGraphSetting("uiWaterfallControlsExpanded", expanded)
    }

    function clearDecodeLabels() {
        decodeLabelSourceRefreshTimer.stop()
        spectrumDecodeLabels = []
        waterfallDisplay.setDecodeLabels([])
    }

    function setShowDecodeCallsigns(enabled, persist) {
        var next = coerceBool(enabled, true)
        var changed = waterfallPanel.showDecodeCallsigns !== next
        waterfallPanel.showDecodeCallsigns = next
        if (!next) {
            clearDecodeLabels()
        } else if (changed) {
            refreshDecodeLabels()
        }
        if (persist === undefined)
            persist = !waterfallPanel.restoringSettings
        if (persist) {
            waterfallPanel.persistGraphSetting("uiWaterfallShowCallsigns", next)
        }
    }

    function txSignalBandwidthHz(modeName) {
        var mode = String(modeName || bridge.mode || "").trim().toUpperCase()
        if (mode === "FT2-LINK" || mode === "FT2LINK")
            return 2300
        if (mode === "FT4")
            return 90
        if (mode === "FT8" || mode === "FT2")
            return 50
        return 0
    }

    Timer {
        id: decodeLabelSourceRefreshTimer
        interval: 160
        repeat: false
        onTriggered: waterfallPanel.refreshDecodeLabelsNow()
    }

    function refreshDecodeLabels(delayMs) {
        if (!waterfallPanel.visible)
            return
        if (!waterfallPanel.showDecodeCallsigns) {
            clearDecodeLabels()
            return
        }
        decodeLabelSourceRefreshTimer.interval = delayMs === undefined ? 160 : Math.max(0, delayMs)
        if (!decodeLabelSourceRefreshTimer.running)
            decodeLabelSourceRefreshTimer.start()
        else if (decodeLabelSourceRefreshTimer.interval === 0)
            decodeLabelSourceRefreshTimer.restart()
    }

    function refreshDecodeLabelsNow() {
        if (!waterfallPanel.visible)
            return
        if (!waterfallPanel.showDecodeCallsigns) {
            clearDecodeLabels()
            return
        }

        var labels = []
        var seen = {}
        var nativeModel = (bridge && bridge.bandActivityModel)
                ? bridge.bandActivityModel : null
        var list = nativeModel ? null : bridge.decodeList
        var listCount = nativeModel ? nativeModel.count() : list.length
        // Prendi solo gli ultimi decode (ultimo periodo) — max 30.
        // Scorri al contrario: se la stessa stazione compare piu' volte,
        // il waterfall deve mostrare il valore SNR del decode piu' recente,
        // cioe' quello che l'operatore vede in cima al Full Spectrum.
        var start = Math.max(0, listCount - 30)
        for (var i = listCount - 1; i >= start; --i) {
            var d = nativeModel ? nativeModel.entry(i) : list[i]
            if (!d || d.isSeparator === true)
                continue
            if (d.isTx)
                continue
            var call = d.fromCall || ""
            var freq = parseInt(d.freq || "0")
            if (!call || !waterfallPanel.isValidClickableAudioFreq(freq))
                continue
            if (seen[call])
                continue
            seen[call] = true
            // 1.0.138: legge il hex pre-calcolato in enrichDecodeEntry
            // (era: bridge.decodeHighlightBg(d) per ogni call ad ogni
            //  decodeListChanged → 150-600 chiamate/sec, alto CPU/GPU).
            var hex = d.highlightBg || ""
            labels.push({
                call: call,
                freq: freq,
                snr: parseInt(d.db || "0"),
                isCQ: d.isCQ || false,
                isMyCall: d.isMyCall || false,
                color: hex
            })
        }
        waterfallPanel.spectrumDecodeLabels = labels
        waterfallDisplay.setDecodeLabels([])
    }

    Timer {
        id: dxClusterRefreshTimer
        interval: 250
        repeat: false
        onTriggered: {
            if ((bridge.transmitting || bridge.tuning) && waterfallPanel.dxClusterRefreshPending) {
                interval = 1000
                restart()
                return
            }
            waterfallPanel.dxClusterRefreshPending = false
            waterfallPanel.refreshDxClusterSpots()
        }
    }

    function scheduleDxClusterRefresh(delayMs) {
        if (!dxClusterCheck.checked) {
            waterfallPanel.dxClusterRefreshPending = false
            dxClusterRefreshTimer.stop()
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        waterfallPanel.dxClusterRefreshPending = true
        var ms = (delayMs === undefined) ? 250 : delayMs
        dxClusterRefreshTimer.interval = (bridge.transmitting || bridge.tuning) ? 1000 : Math.max(0, ms)
        if (!dxClusterRefreshTimer.running)
            dxClusterRefreshTimer.start()
        else if (!(bridge.transmitting || bridge.tuning))
            dxClusterRefreshTimer.restart()
    }

    // Filtra gli spot del DX cluster per la dial corrente e li passa al
    // PanadapterItem. Ogni voce: { call, freq } con freq in audio Hz.
    // bridge.frequency è la dial in Hz. Spot.frequency è in kHz.
    // Se la dial non è nota (CAT scollegato), inferiamo dalla mediana
    // degli spot intorno a una banda plausibile per non bloccare la feature.
    function refreshDxClusterSpots() {
        if (!waterfallPanel.visible)
            return
        if (!dxClusterCheck.checked) {
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        if (bridge.transmitting || bridge.tuning) {
            scheduleDxClusterRefresh(1000)
            return
        }
        var spots = bridge.dxClusterSpots || []
        var dialHz = Number(bridge.frequency) || 0
        var fmin = waterfallPanel.minFreq
        var fmax = waterfallPanel.maxFreq
        console.log("[Waterfall][DxCluster] refresh: spots=" + spots.length
                    + " dialHz=" + dialHz + " fmin=" + fmin + " fmax=" + fmax
                    + " toggle=" + dxClusterCheck.checked)
        if (!spots || spots.length === 0) {
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        // Fallback: se dialHz=0 o molto piccola, prova a usare la freq media
        // degli spot 20m (14000-14400 kHz) come dial pseudo per non perdere
        // visualizzazione totale.
        if (dialHz <= 1000) {
            // Conta spot per banda 20m e usa 14074 come default FT8 dial.
            var has20m = false
            for (var k = 0; k < spots.length; ++k) {
                var fk = Number(spots[k].frequency || 0)
                if (fk >= 14000 && fk <= 14400) { has20m = true; break }
            }
            if (has20m) {
                dialHz = 14074000
                console.log("[Waterfall][DxCluster] fallback dial=14074000Hz (20m FT8)")
            }
        }
        if (dialHz <= 0) {
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        var out = []
        var seen = {}
        var totalRange = 0
        for (var i = spots.length - 1; i >= 0; --i) {
            var s = spots[i]
            if (!s) continue
            var call = (s.dxCall || "").toString().toUpperCase()
            if (!call) continue
            if (seen[call]) continue
            var fkhz = Number(s.frequency || 0)
            if (!fkhz) continue
            var audio = Math.round(fkhz * 1000 - dialHz)
            if (audio < fmin || audio > fmax) continue
            // Skippa spot troppo vicini alla dial (audio<100Hz): non utili
            // come target TX e renderebbero TUNE inaudibile se cliccati.
            if (audio < 100) continue
            seen[call] = true
            out.push({ call: call, freq: audio })
            totalRange++
            if (out.length >= 40) break
        }
        console.log("[Waterfall][DxCluster] inRange=" + totalRange + " sentToWaterfall=" + out.length)
        waterfallDisplay.setDxClusterSpots(out)
    }

    Component.onCompleted: Qt.callLater(loadPanadapterSettings)
    onVisibleChanged: if (visible) Qt.callLater(loadPanadapterSettings)

    // Aggiorna gli spot cluster sul waterfall quando arrivano nuovi spot
    // o quando cambia la dial (cambia anche il filtro audio offset).
    Connections {
        target: bridge
        function onDxClusterSpotsChanged() { waterfallPanel.scheduleDxClusterRefresh(250) }
    }
    Connections {
        target: bridge
        function onFrequencyChanged() {
            if (dxClusterCheck.checked) waterfallPanel.scheduleDxClusterRefresh(120)
        }
        function onTransmittingChanged() {
            if (!bridge.transmitting && waterfallPanel.dxClusterRefreshPending)
                waterfallPanel.scheduleDxClusterRefresh(0)
        }
        function onTuningChanged() {
            if (!bridge.tuning && waterfallPanel.dxClusterRefreshPending)
                waterfallPanel.scheduleDxClusterRefresh(0)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            // L'altezza segue i comandi: quando vanno a capo la barra cresce,
            // invece di restare alta 46 con le righe in piu' tagliate via.
            Layout.preferredHeight: waterfallPanel.controlsVisible
                                    ? Math.max(46, filaComandiWf.implicitHeight + 8) : 0
            Layout.bottomMargin: waterfallPanel.controlsVisible ? 6 : 0
            color: wfToolbarBg; visible: waterfallPanel.controlsVisible
            border.color: wfFrame; border.width: 1
            clip: true

            // I comandi vanno a capo quando non ci stanno. Dove il pannello e'
            // largo — la finestra principale — restano su una riga sola come
            // sempre: un Flow che ha spazio si comporta come una fila. Dove e'
            // stretto, come nella finestra RTTY, si dispongono su quante righe
            // servono invece di farsi tagliare a meta' dal bordo.
            //
            // Gli elementi qui dentro hanno larghezze e altezze proprie e non
            // piu' quelle del layout a riga: in un Flow le proprieta' Layout.*
            // non hanno effetto, e lasciarle avrebbe fatto collassare i comandi
            // alle loro dimensioni naturali.
            Flow {
                id: filaComandiWf
                x: 6
                y: 3
                width: parent.width - 12
                spacing: 6

                Text {
                    text: qsTr("Calls:")
                    color: accentCyan
                    font.pixelSize: 10
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }
                CheckBox {
                    id: showCallsCheck
                    width: 18
                    height: 18
                    leftPadding: 0
                    rightPadding: 0
                    topPadding: 0
                    bottomPadding: 0
                    checked: waterfallPanel.showDecodeCallsigns
                    onClicked: waterfallPanel.setShowDecodeCallsigns(checked)
                    ToolTip.text: qsTr("Show decoded callsigns on the waterfall")
                    ToolTip.visible: showCallsCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        x: Math.round((showCallsCheck.width - width) / 2)
                        y: Math.round((showCallsCheck.height - height) / 2)
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: showCallsCheck.checked ? accentCyan : wfToolbarBg
                        border.color: accentCyan; border.width: 1
                        Text { anchors.centerIn: parent; text: "C"; color: "black"; font.pixelSize: 9; font.bold: true; visible: showCallsCheck.checked }
                    }
                    contentItem: Item { implicitWidth: 0; implicitHeight: 0 }
                }

                Text { text: qsTr("Font"); color: wfText; font.pixelSize: 10 }
                Slider {
                    id: labelFontSlider
                    width: 70
                    from: 6; to: 20; value: 8; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.labelFontSize = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelFontSize", value)
                        }
                    }
                    background: Rectangle { x:labelFontSlider.leftPadding; y:labelFontSlider.topPadding+labelFontSlider.availableHeight/2-2; width:labelFontSlider.availableWidth; height:4; radius:2; color:wfTrack
                        Rectangle { width: labelFontSlider.visualPosition*parent.width; height: parent.height; radius: 2; color: accentCyan }
                    }
                    handle: Rectangle { x:labelFontSlider.leftPadding+labelFontSlider.visualPosition*(labelFontSlider.availableWidth-width); y:labelFontSlider.topPadding+labelFontSlider.availableHeight/2-height/2; width:10; height:10; radius:5; color: labelFontSlider.pressed ? accentGreen : accentCyan }
                }
                Text { text: labelFontSlider.value.toFixed(0)+"px"; color: accentCyan; font.pixelSize: 10; width: 26 }

                Rectangle { width:1; height:14; color:"#333" }

                Text { text: qsTr("Gap"); color: wfText; font.pixelSize: 10 }
                Slider {
                    id: labelSpacingSlider
                    width: 60
                    from: 0; to: 20; value: 2; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.labelSpacing = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelSpacing", value)
                        }
                    }
                    background: Rectangle { x:labelSpacingSlider.leftPadding; y:labelSpacingSlider.topPadding+labelSpacingSlider.availableHeight/2-2; width:labelSpacingSlider.availableWidth; height:4; radius:2; color:wfTrack
                        Rectangle { width: labelSpacingSlider.visualPosition*parent.width; height: parent.height; radius: 2; color: accentGreen }
                    }
                    handle: Rectangle { x:labelSpacingSlider.leftPadding+labelSpacingSlider.visualPosition*(labelSpacingSlider.availableWidth-width); y:labelSpacingSlider.topPadding+labelSpacingSlider.availableHeight/2-height/2; width:10; height:10; radius:5; color: labelSpacingSlider.pressed ? accentGreen : accentGreen }
                }
                Text { text: labelSpacingSlider.value.toFixed(0); color: accentGreen; font.pixelSize: 10; width: 20 }

                Rectangle { width:1; height:14; color:"#333" }

                CheckBox {
                    id: labelBoldCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.labelBold = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelBold", checked)
                        }
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: labelBoldCheck.checked ? "#FFFFFF" : wfToolbarBg
                        border.color: "#FFFFFF"; border.width: 1
                        Text { anchors.centerIn: parent; text: "B"; color: "black"; font.pixelSize: 9; font.bold: true; visible: labelBoldCheck.checked }
                    }
                }
                Text { text: qsTr("Bold"); color: labelBoldCheck.checked ? "#FFFFFF" : textSec; font.pixelSize: 10 }

                Rectangle { width:1; height:14; color:"#333" }

                Text { text: qsTr("Color"); color: wfText; font.pixelSize: 10 }
                DecoComboBox {
                    id: labelColorCombo
                    width: 122
                    font.pixelSize: 10
                    model: waterfallPanel.labelColorPresets.map(function(p){ return p.name })
                    currentIndex: 0
                    onActivated: {
                        var preset = waterfallPanel.labelColorPresets[currentIndex]
                        waterfallDisplay.labelUseCustomColor = preset.custom
                        waterfallDisplay.labelColor = preset.color
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelColorPreset", currentIndex)
                        }
                    }
                    background: Rectangle { color: wfToolbarBg; border.color: borderColor; radius: 2 }
                    contentItem: Item {
                        implicitWidth: 88
                        implicitHeight: 22

                        Rectangle {
                            id: labelColorSwatch
                            x: 6
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10; height: 10; radius: 2
                            color: waterfallPanel.labelColorPresets[labelColorCombo.currentIndex].color
                            border.color: "#555"; border.width: 1
                        }
                        Text {
                            anchors.left: labelColorSwatch.right
                            anchors.leftMargin: 6
                            anchors.right: parent.right
                            anchors.rightMargin: 24
                            anchors.verticalCenter: parent.verticalCenter
                            text: labelColorCombo.displayText
                            font.pixelSize: 10; color: textPrimary
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    delegate: ItemDelegate {
                        id: colorDelegate
                        required property int index

                        width: ListView.view ? ListView.view.width : 196
                        height: 38
                        leftPadding: 12
                        rightPadding: 12
                        highlighted: labelColorCombo.highlightedIndex === index

                        contentItem: RowLayout {
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 13
                                Layout.preferredHeight: 13
                                Layout.alignment: Qt.AlignVCenter
                                radius: 3
                                color: waterfallPanel.labelColorPresets[colorDelegate.index].color
                                border.color: Qt.rgba(1, 1, 1, 0.55)
                                border.width: 1
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: waterfallPanel.labelColorPresets[colorDelegate.index].name
                                color: "#ffffff"
                                font.pixelSize: 12
                                font.bold: colorDelegate.highlighted || labelColorCombo.currentIndex === colorDelegate.index
                                elide: Text.ElideNone
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        background: Rectangle {
                            color: colorDelegate.highlighted || labelColorCombo.currentIndex === colorDelegate.index
                                   ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.42)
                                   : "transparent"
                        }
                    }
                    popup: Popup {
                        y: labelColorCombo.height + 2
                        width: Math.max(labelColorCombo.width, 196)
                        height: Math.min(contentItem.implicitHeight + 8, 310)
                        padding: 4
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: labelColorCombo.popup.visible ? labelColorCombo.delegateModel : null
                            currentIndex: labelColorCombo.highlightedIndex
                            highlightMoveDuration: 0
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { }
                        }

                        background: Rectangle {
                            color: "#07111f"
                            border.color: accentCyan
                            border.width: 1
                            radius: 4
                        }
                    }
                }

                Rectangle { width:1; height:14; color:"#333" }

                // ── Palette / Auto / [] / Peak / Zoom / dBm ──
                // Unita alla barra Calls per avere un'unica riga superiore (feedback IK8OLM)
                Text { text: qsTr("Palette:"); color: wfText; font.pixelSize: 10 }
                DecoComboBox {
                    id: paletteCombo
                    width: 142
                    model: waterfallDisplay.paletteNames
                    currentIndex: 0
                    font.pixelSize: 10
                    onActivated: {
                        waterfallPanel.setPaletteIndex(currentIndex, true)
                    }
                    background: Rectangle { color: wfToolbarBg; border.color: borderColor; radius: 2 }
                    contentItem: Text {
                        text: paletteCombo.displayText
                        font.pixelSize: 10
                        color: textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                        rightPadding: 24
                        elide: Text.ElideRight
                    }
                    delegate: ItemDelegate {
                        id: paletteDelegate
                        required property int index
                        required property var model

                        width: ListView.view ? ListView.view.width : 214
                        height: 38
                        leftPadding: 12
                        rightPadding: 12
                        highlighted: paletteCombo.highlightedIndex === index

                        contentItem: Text {
                            text: paletteCombo.optionText(paletteDelegate.model)
                            color: "#ffffff"
                            font.pixelSize: 12
                            font.bold: paletteDelegate.highlighted || paletteCombo.currentIndex === paletteDelegate.index
                            elide: Text.ElideNone
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: paletteDelegate.highlighted || paletteCombo.currentIndex === paletteDelegate.index
                                   ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.42)
                                   : "transparent"
                        }
                    }
                    popup: Popup {
                        y: paletteCombo.height + 2
                        width: Math.max(paletteCombo.width, 214)
                        height: Math.min(contentItem.implicitHeight + 8, 360)
                        padding: 4
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: paletteCombo.popup.visible ? paletteCombo.delegateModel : null
                            currentIndex: paletteCombo.highlightedIndex
                            highlightMoveDuration: 0
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { }
                        }

                        background: Rectangle {
                            color: "#07111f"
                            border.color: accentCyan
                            border.width: 1
                            radius: 4
                        }
                    }
                }

                // Auto Range
                CheckBox {
                    id: autoRangeCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.autoRange = checked
                        if (!checked) {
                            waterfallPanel.applyManualContrast()
                        }
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallAutoRange", checked)
                        }
                    }
                    ToolTip.text: qsTr("Automatic noise threshold (IIR)")
                    ToolTip.visible: autoRangeCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: autoRangeCheck.checked ? accentGreen : wfToolbarBg
                        border.color: accentGreen; border.width: 1
                        Text { anchors.centerIn: parent; text: "A"; color: "black"; font.pixelSize: 9; font.bold: true; visible: autoRangeCheck.checked }
                    }
                }
                Text { text: "Auto"; color: autoRangeCheck.checked ? accentGreen : textSec; font.pixelSize: 10 }

                // Quanto taglia la soglia automatica. Compare solo quando la
                // soglia e' accesa: a filtro spento non regola nulla.
                Text {
                    text: qsTr("Cut:"); color: accentGreen; font.pixelSize: 10
                    visible: autoRangeCheck.checked
                }
                Slider {
                    id: noiseCutSlider
                    visible: autoRangeCheck.checked
                    from: 5; to: 40; stepSize: 1
                    width: 70; height: 18
                    value: 10
                    onMoved: {
                        waterfallDisplay.noiseFloorPercentile = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiNoiseFloorPercentile", value)
                        }
                    }
                    ToolTip.text: qsTr("How much of the spectrum the automatic threshold calls noise and cuts away. 10 is the historical setting: raise it to clean up an empty band, lower it if weak signals disappear.")
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    background: Rectangle { x:noiseCutSlider.leftPadding;y:noiseCutSlider.topPadding+noiseCutSlider.availableHeight/2-2;width:noiseCutSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:noiseCutSlider.leftPadding+noiseCutSlider.visualPosition*(noiseCutSlider.availableWidth-width);y:noiseCutSlider.topPadding+noiseCutSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:accentGreen }
                }
                Text {
                    text: noiseCutSlider.value.toFixed(0) + "%"
                    color: accentGreen; font.pixelSize: 10; width: 26
                    visible: autoRangeCheck.checked
                }

                // Spettro 3D a tracce impilate. La cascata sotto resta invariata:
                // cambia solo come viene disegnato lo spettro sopra di essa.
                //
                // UN SOLO bersaglio, non casella + etichetta separate: l'etichetta
                // non era cliccabile e il quadratino finiva a ridosso di quello di
                // "Auto", quindi si mancava il bersaglio o si premeva l'altro.
                // Qui tutto il rettangolo e' cliccabile e lo stato si legge a colpo
                // d'occhio: pieno = acceso, vuoto = spento.
                Rectangle {
                    id: spectrum3dToggle
                    property bool checked: false
                    implicitWidth: 42
                    implicitHeight: 18
                    radius: 3
                    color: checked ? accentCyan : (spectrum3dMA.containsMouse ? Qt.rgba(1,1,1,0.10) : wfToolbarBg)
                    border.color: accentCyan
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "3D"
                        color: spectrum3dToggle.checked ? "black" : accentCyan
                        font.pixelSize: 10
                        font.bold: true
                    }

                    ToolTip.text: spectrum3dToggle.checked
                                  ? qsTr("Stacked-trace 3D spectrum: on. Click to go back to the 2D trace.")
                                  : qsTr("Stacked-trace 3D spectrum: shows the history of the band receding into the distance. It costs more to draw, so leave it off on modest machines.")
                    ToolTip.visible: spectrum3dMA.containsMouse
                    ToolTip.delay: 400

                    MouseArea {
                        id: spectrum3dMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            spectrum3dToggle.checked = !spectrum3dToggle.checked
                            waterfallDisplay.spectrum3d = spectrum3dToggle.checked
                            if (!waterfallPanel.restoringSettings) {
                                waterfallPanel.persistGraphSetting("uiSpectrum3d", spectrum3dToggle.checked)
                            }
                        }
                    }
                }

                // Regolazioni del 3D: compaiono solo quando serve, per non
                // affollare la barra a chi il 3D non lo usa.
                Text {
                    text: qsTr("Traces:"); color: accentCyan; font.pixelSize: 10
                    visible: spectrum3dToggle.checked
                }
                Slider {
                    id: traces3dSlider
                    visible: spectrum3dToggle.checked
                    width: 70
                    from: 8; to: 96; value: 28; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.spectrum3dTraces = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiSpectrum3dTraces", value)
                        }
                    }
                    ToolTip.text: qsTr("How many history traces are drawn. Fewer traces separate the ridges; more of them show a longer history.")
                    ToolTip.visible: traces3dSlider.hovered
                    ToolTip.delay: 400
                    background: Rectangle { x:traces3dSlider.leftPadding;y:traces3dSlider.topPadding+traces3dSlider.availableHeight/2-2;width:traces3dSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:traces3dSlider.leftPadding+traces3dSlider.visualPosition*(traces3dSlider.availableWidth-width);y:traces3dSlider.topPadding+traces3dSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:accentCyan }
                }
                Text {
                    text: traces3dSlider.value.toFixed(0); color: accentCyan; font.pixelSize: 10; width: 18
                    visible: spectrum3dToggle.checked
                }
                Text {
                    text: qsTr("Floor:"); color: accentCyan; font.pixelSize: 10
                    visible: spectrum3dToggle.checked
                }
                Slider {
                    id: floor3dSlider
                    visible: spectrum3dToggle.checked
                    width: 70
                    from: 0; to: 30; value: 6; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.spectrum3dFloorDepth = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiSpectrum3dFloorDepth", value)
                        }
                    }
                    ToolTip.text: qsTr("How far above the minimum the ridges start. Raise it to flatten the noise and leave only the signals standing.")
                    ToolTip.visible: floor3dSlider.hovered
                    ToolTip.delay: 400
                    background: Rectangle { x:floor3dSlider.leftPadding;y:floor3dSlider.topPadding+floor3dSlider.availableHeight/2-2;width:floor3dSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:floor3dSlider.leftPadding+floor3dSlider.visualPosition*(floor3dSlider.availableWidth-width);y:floor3dSlider.topPadding+floor3dSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:accentCyan }
                }
                Text {
                    text: floor3dSlider.value.toFixed(0) + " dB"; color: accentCyan; font.pixelSize: 10; width: 32
                    visible: spectrum3dToggle.checked
                }

                // TX brackets toggle
                Text { text: "[ ]"; color: txBracketsCheck.checked ? accentGreen : textSec; font.pixelSize: 10; font.bold: true }
                CheckBox {
                    id: txBracketsCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.showTxBrackets = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallShowTxBrackets", checked)
                        }
                    }
                    indicator: Rectangle {
                        implicitWidth: 16; implicitHeight: 16; radius: 2
                        color: txBracketsCheck.checked ? accentGreen : wfToolbarBg
                        border.color: accentGreen; border.width: 1
                        Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: txBracketsCheck.checked }
                    }
                }

                // DX Cluster spots toggle (giallo, click = chiama stazione)
                CheckBox {
                    id: dxClusterCheck
                    checked: false
                    onCheckedChanged: {
                        waterfallDisplay.showDxClusterSpots = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallShowDxCluster", checked)
                        }
                        if (checked) {
                            waterfallPanel.scheduleDxClusterRefresh(0)
                        }
                    }
                    ToolTip.text: qsTr("Show DX Cluster spots on the waterfall (click to call)")
                    ToolTip.visible: dxClusterCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: dxClusterCheck.checked ? "#FFC800" : wfToolbarBg
                        border.color: "#FFC800"; border.width: 1
                        Text { anchors.centerIn: parent; text: "DX"; color: "black"; font.pixelSize: 7; font.bold: true; visible: dxClusterCheck.checked }
                    }
                }
                Text { text: qsTr("Cluster"); color: dxClusterCheck.checked ? "#FFC800" : textSec; font.pixelSize: 10 }

                // Lo spaziatore che spingeva a destra il pulsante di chiusura non
                // serve piu': in una fila che va a capo l'ultimo elemento sta dove
                // arriva, e uno spaziatore elastico lo spingerebbe da solo su una
                // riga tutta sua.

                Rectangle {
                    id: collapseControlsButton
                    width: 112
                    height: 28
                    radius: 6
                    color: collapseControlsMA.containsMouse
                           ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.26)
                           : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.86)
                    border.color: accentGreen
                    border.width: collapseControlsMA.containsMouse ? 2 : 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: qsTr("Hide")
                            color: accentGreen
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: qsTr("\u25B4")
                            color: accentGreen
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: collapseControlsMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: waterfallPanel.setControlsExpanded(false)
                    }

                    ToolTip.visible: collapseControlsMA.containsMouse
                    ToolTip.delay: 450
                    ToolTip.text: qsTr("Hide waterfall controls")
                }

                // Peak Hold toggle
                CheckBox {
                    id: peakHoldCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.peakHold = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallPeakHold", checked)
                        }
                    }
                    ToolTip.text: qsTr("Peak Hold: keeps spectrum peaks visible")
                    ToolTip.visible: peakHoldCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: peakHoldCheck.checked ? wfYellow : wfToolbarBg
                        border.color: wfYellow; border.width: 1
                        Text { anchors.centerIn: parent; text: "P"; color: "#262626"; font.pixelSize: 9; font.bold: true; visible: peakHoldCheck.checked }
                    }
                }
                Text { text: qsTr("Peak"); color: peakHoldCheck.checked ? wfYellow : textSec; font.pixelSize: 10 }

                // Zoom
                Text { text: qsTr("Zoom"); color: wfText; font.pixelSize: 10 }
                Slider {
                    id: zoomSlider
                    width: 70
                    from: 1; to: 8; value: bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0; stepSize: 0.5
                    onValueChanged: {
                        waterfallDisplay.zoomFactor = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.uiZoomFactor = value
                            waterfallPanel.persistGraphSetting("uiZoomFactor", value)
                        }
                    }
                    background: Rectangle {
                        x: zoomSlider.leftPadding; y: zoomSlider.topPadding + zoomSlider.availableHeight/2 - 2
                        width: zoomSlider.availableWidth; height: 4; radius: 2
                        color: Qt.rgba(textPrimary.r,textPrimary.g,textPrimary.b,0.15)
                        Rectangle { width: zoomSlider.visualPosition * parent.width; height: parent.height; radius: 2; color: accentCyan }
                    }
                    handle: Rectangle {
                        x: zoomSlider.leftPadding + zoomSlider.visualPosition*(zoomSlider.availableWidth - width)
                        y: zoomSlider.topPadding + zoomSlider.availableHeight/2 - height/2
                        width: 10; height: 10; radius: 5
                        color: zoomSlider.pressed ? accentGreen : accentCyan
                    }
                }
                Text { text: zoomSlider.value.toFixed(1)+"×"; color: accentCyan; font.pixelSize: 10; width: 28 }

                // dBm live noise floor
                Text {
                    text: waterfallDisplay.measuredFloor.toFixed(0) + "dBm"
                    color: accentGreen
                    font.pixelSize: 10
                    ToolTip.text: qsTr("Measured noise threshold")
                    ToolTip.visible: nfLabel.containsMouse
                    ToolTip.delay: 400
                    MouseArea { id: nfLabel; anchors.fill: parent; hoverEnabled: true }
                }
            }
        }

        // ── Slider Black / Gain ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: waterfallPanel.controlsVisible ? 22 : 0
            color: wfToolbarBg; visible: waterfallPanel.controlsVisible
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 6
                Text { text: qsTr("Black:"); color: wfSlate; font.pixelSize: 10 }
                Slider { id: blackSlider; Layout.preferredWidth: 80; from: 0; to: 100; value: 15; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.blackLevel = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallBlackLevel", value)
                        }
                    }
                    background: Rectangle { x:blackSlider.leftPadding;y:blackSlider.topPadding+blackSlider.availableHeight/2-2;width:blackSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:blackSlider.leftPadding+blackSlider.visualPosition*(blackSlider.availableWidth-width);y:blackSlider.topPadding+blackSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:wfSlate }
                }
                Text { text: blackSlider.value.toFixed(0); color: wfSlate; font.pixelSize: 10; width: 20 }
                Rectangle { width:1;height:14;color:"#333" }
                Text { text: qsTr("Gain:"); color: wfBlue; font.pixelSize: 10 }
                Slider { id: gainSlider; Layout.preferredWidth: 80; from: 0; to: 100; value: 50; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.colorGain = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallColorGain", value)
                        }
                    }
                    background: Rectangle { x:gainSlider.leftPadding;y:gainSlider.topPadding+gainSlider.availableHeight/2-2;width:gainSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:gainSlider.leftPadding+gainSlider.visualPosition*(gainSlider.availableWidth-width);y:gainSlider.topPadding+gainSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:wfBlue }
                }
                Text { text: gainSlider.value.toFixed(0); color: wfBlue; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                Text { text: qsTr("Contrasto:"); color: wfPurple; font.pixelSize: 10 }
                Slider { id: contrastSlider; Layout.preferredWidth: 70; from: 10; to: 150; value: 80; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.contrastLevel = value
                        waterfallPanel.applyManualContrast()
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallContrast", value)
                        }
                    }
                    background: Rectangle { x:contrastSlider.leftPadding;y:contrastSlider.topPadding+contrastSlider.availableHeight/2-2;width:contrastSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:contrastSlider.leftPadding+contrastSlider.visualPosition*(contrastSlider.availableWidth-width);y:contrastSlider.topPadding+contrastSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:wfPurple }
                }
                Text { text: contrastSlider.value.toFixed(0); color: wfPurple; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                Text { text: qsTr("Vel:"); color: "#DD8866"; font.pixelSize: 10 }
                Slider { id: speedSlider; Layout.preferredWidth: 60; from: 10; to: 500; value: 20; stepSize: 5
                    onValueChanged: if (!waterfallPanel.restoringSettings) waterfallPanel.persistGraphSetting("spectrumInterval", value)
                    background: Rectangle { x:speedSlider.leftPadding;y:speedSlider.topPadding+speedSlider.availableHeight/2-2;width:speedSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:speedSlider.leftPadding+speedSlider.visualPosition*(speedSlider.availableWidth-width);y:speedSlider.topPadding+speedSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:"#DD8866" }
                }
                Text { text: speedSlider.value.toFixed(0)+"ms"; color: "#DD8866"; font.pixelSize: 10; width: 32 }

                Item { Layout.fillWidth: true }
            }
        }

        // ── Divisore draggabile tra spectrum e waterfall ──────────────────
        // Trascina per regolare l'altezza del grafico spettro
        Rectangle {
            id: spectrumDivider
            Layout.fillWidth: true
            Layout.preferredHeight: waterfallPanel.controlsVisible ? 12 : 0
            visible: waterfallPanel.controlsVisible
            color: dividerMouse.containsMouse || dividerMouse.pressed
                   ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.30) : Qt.rgba(1,1,1,0.06)

            // 1.0.288 — grip più evidente: trascina questa barra per regolare quanto
            // spazio dare allo spettro (in alto) vs la cascata (in basso).
            Rectangle {
                anchors.centerIn: parent
                width: 100; height: 4; radius: 2
                color: accentCyan
                opacity: dividerMouse.containsMouse || dividerMouse.pressed ? 1.0 : 0.75
            }

            MouseArea {
                id: dividerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor
                property int dragStartY: 0
                property int dragStartH: 0

                onPressed: function(mouse) {
                    dragStartY = mouse.y + spectrumDivider.mapToItem(waterfallPanel, 0, 0).y
                    dragStartH = waterfallPanel.spectrumHeight
                }
                // 1.0.288 — BUG FIX: era onMouseYChanged con `mouse.y`, ma in quell'handler
                // il parametro `mouse` non esiste (è property-change) → TypeError → il drag
                // non aggiornava mai spectrumHeight (split mai funzionante). Uso onPositionChanged
                // (che riceve `mouse`) e mouseY come fallback.
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var dy = (mouse.y + spectrumDivider.mapToItem(waterfallPanel, 0, 0).y) - dragStartY
                        var newH = dragStartH + dy
                        var newVal = Math.round(Math.max(waterfallPanel.spectrumMinHeight,
                                             Math.min(waterfallPanel.spectrumMaxHeight, newH)))
                        waterfallPanel.spectrumHeight = newVal
                        bridge.uiSpectrumHeight = newVal
                        mainWindow.scheduleSave()
                    }
                }
                onReleased: {
                    waterfallPanel.persistGraphSetting("uiSpectrumHeight", waterfallPanel.spectrumHeight)
                }
            }

            Behavior on color { ColorAnimation { duration: bridge.lowCpuModeEnabled ? 0 : 100 } }
        }

        // ── PanadapterItem — FlexRadio SmartSDR style ─────────────────────
        PanadapterItem {
            id: waterfallDisplay
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Decoder audio uses the familiar 200..3200 Hz axis. When the
            // RTL-SDR RF path is active the incoming bins are absolute RF,
            // therefore the viewport must follow the tuner centre and its
            // real sample-rate span instead of clipping them to the FT8 axis.
            startFreq:      bridge.rtlSdrRfView
                            ? Math.round(bridge.rtlSdrRfCenterFrequency
                                         - bridge.rtlSdrRfSampleRate / 2)
                            : waterfallPanel.minFreq
            bandwidth:      bridge.rtlSdrRfView
                            ? bridge.rtlSdrRfSampleRate
                            : waterfallPanel.maxFreq - waterfallPanel.minFreq
            rxFreq:         bridge.rxFrequency
            txFreq:         bridge.txFrequency
            running:        bridge.monitoring
            externalSpectrumActive: bridge.rtlSdrRfView
            showTxBrackets: true
            spectrumHeight: Math.max(waterfallPanel.spectrumMinHeight,
                                     Math.min(waterfallPanel.spectrumHeight,
                                              Math.max(waterfallPanel.spectrumMinHeight,
                                                       waterfallDisplay.height - waterfallPanel.waterfallMinHeight)))
            // In FT2-Link evitiamo il render loop pieno, ma 500 ms rende il
            // panadapter visibilmente scattoso. Manteniamo un refresh fluido in
            // profilo normale e lasciamo il taglio piu' forte solo a Low CPU.
            throttleActive: bridge.lowCpuModeEnabled || waterfallPanel.ft2LinkMode
            throttleIntervalMs: waterfallPanel.ft2LinkMode
                                ? (bridge.lowCpuModeEnabled ? 200 : 80)
                                : (bridge.lowCpuModeEnabled ? 250 : 100)
            // Carica valori da Settings al primo avvio.
            paletteIndex:   0
            contrastLevel:  contrastSlider.value

            // In light theme forza la palette pastello chiara (indice 11) per coerenza visiva col mockup.
            // Binding esplicito: ha priorità sopra qualsiasi assegnazione procedurale finché when=true.
            Binding {
                target: waterfallDisplay
                property: "paletteIndex"
                value: 11
                when: bridge.themeManager.isLightTheme
                restoreMode: Binding.RestoreBindingOrValue
            }
            Component.onCompleted: {
                console.log("[Waterfall] theme=" + bridge.themeManager.currentTheme
                    + " isLight=" + bridge.themeManager.isLightTheme
                    + " paletteIndex=" + waterfallDisplay.paletteIndex)
            }
            Connections {
                target: bridge.themeManager
                function onPaletteChanged() {
                    console.log("[Waterfall] paletteChanged → theme=" + bridge.themeManager.currentTheme
                        + " isLight=" + bridge.themeManager.isLightTheme
                        + " paletteIndex=" + waterfallDisplay.paletteIndex)
                }
            }
            autoRange:      autoRangeCheck.checked
            peakHold:       true
            zoomFactor:     bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0

            readonly property real rxFilterHz: 300
            readonly property real txSignalBandwidthHz: waterfallPanel.txSignalBandwidthHz(bridge.mode)
            readonly property real viewRangeHz: Math.max(1, bandwidth) / Math.max(0.001, zoomFactor)
            readonly property real viewStartHz: startFreq + Math.max(1, bandwidth) * 0.5 + panHz - viewRangeHz * 0.5
            readonly property real rxFilterLeftX: Math.max(0, Math.min(width, freqToPixel(rxFreq - rxFilterHz * 0.5)))
            readonly property real rxFilterRightX: Math.max(0, Math.min(width, freqToPixel(rxFreq + rxFilterHz * 0.5)))
            readonly property real txSignalLeftX: freqToPixel(txFreq - txSignalBandwidthHz * 0.5)
            readonly property real txSignalRightX: freqToPixel(txFreq + txSignalBandwidthHz * 0.5)
            readonly property bool txSignalGuideVisible: showTxBrackets
                                                           && txSignalBandwidthHz > 0
                                                           && txSignalRightX >= 0
                                                           && txSignalLeftX <= width

            function freqToPixel(freq) {
                return (Number(freq) - viewStartHz) * width / viewRangeHz
            }

            // Salva al bridge con debounce (2s dopo l'ultimo cambio)
            // Non persistere la palette light auto-attivata col tema (resta una scelta del tema, non utente).
            onPaletteIndexChanged: {
                if (!waterfallPanel.restoringSettings
                    && !waterfallPanel.syncingPaletteChoice
                    && !bridge.themeManager.isLightTheme) {
                    waterfallPanel.setPaletteIndex(paletteIndex, true)
                }
            }
            onZoomFactorChanged: if (!waterfallPanel.restoringSettings) {
                bridge.uiZoomFactor = zoomFactor
                waterfallPanel.persistGraphSetting("uiZoomFactor", zoomFactor)
            }
            onMeasuredFloorChanged: waterfallPanel.applyManualContrast()
            onGpuFftActivated: function(backend) {
                bridge.setGpuPanadapterFftActive(true, backend)
            }
            onGpuFftUnavailable: function(reason) {
                bridge.setGpuPanadapterFftAvailable(false, reason)
            }

            onFrequencySelected: function(freq) {
                waterfallPanel.frequencySelected(freq)        // RX
            }
            onTxFrequencySelected: function(freq) {
                waterfallPanel.txFrequencySelected(freq)      // TX
            }
            onDxClusterSpotClicked: function(call, audioFreqHz) {
                console.log("[Waterfall] DX cluster click → engage", call, "@", audioFreqHz, "Hz")
                if (!waterfallPanel.isValidClickableAudioFreq(audioFreqHz))
                    return
                bridge.engageDxClusterSpot(call, audioFreqHz)
            }
            onDecodeLabelClicked: function(call, audioFreqHz) {
                console.log("[Waterfall] decode label click → engage", call, "@", audioFreqHz, "Hz")
                if (!waterfallPanel.isValidClickableAudioFreq(audioFreqHz))
                    return
                bridge.engageDxClusterSpot(call, audioFreqHz)
            }

            Item {
                id: spectrumGpuOverlay
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.max(0, Math.min(waterfallDisplay.spectrumHeight, waterfallDisplay.height))
                z: 10
                clip: true
                visible: waterfallDisplay.spectrumGpuOverlayAvailable && height > 0
                readonly property bool cppOverlayEnabled: waterfallDisplay.spectrumGpuOverlayAvailable
                readonly property bool nativeDecodeLabelsEnabled: true

                readonly property string fixedFontFamily: decodiumMonoFontFamily
                readonly property real viewStartHz: waterfallDisplay.viewStartHz
                readonly property real viewRangeHz: waterfallDisplay.viewRangeHz
                readonly property real dbRange: Math.max(1, waterfallDisplay.maxDb - waterfallDisplay.minDb)
                readonly property int freqStep: viewRangeHz > 3000 ? 500 : (viewRangeHz > 1000 ? 200 : 100)
                readonly property bool txVisible: {
                    var txX = xForFreq(waterfallDisplay.txFreq)
                    return txX >= 0 && txX < width && waterfallDisplay.txFreq !== waterfallDisplay.rxFreq
                }

                function xForFreq(freq) {
                    return (Number(freq) - viewStartHz) * width / viewRangeHz
                }

                function clamp(value, minValue, maxValue) {
                    return Math.max(minValue, Math.min(maxValue, value))
                }

                function markerBoxX(markerX, boxWidth) {
                    var boxX = markerX + 8
                    if (boxX + boxWidth > width - 2)
                        boxX = markerX - boxWidth - 8
                    return clamp(boxX, 2, Math.max(2, width - boxWidth - 2))
                }

                function markerBoxY(centerY, boxHeight) {
                    var minY = boxHeight / 2 + 3
                    var maxY = Math.max(minY, height - boxHeight / 2 - 22)
                    return clamp(centerY, minY, maxY) - boxHeight / 2
                }

                function frequencyGridModel() {
                    var out = []
                    var first = (Math.floor(viewStartHz / freqStep) + 1) * freqStep
                    var end = viewStartHz + viewRangeHz
                    for (var f = first; f < end; f += freqStep) {
                        var x = xForFreq(f)
                        if (x < 0 || x >= width)
                            continue
                        out.push({
                            x: Math.round(x),
                            label: f >= 1000 ? (f / 1000).toFixed(1) + "k" : String(Math.round(f))
                        })
                    }
                    return out
                }

                function tickModel() {
                    var out = []
                    var first = Math.floor(viewStartHz / 500) * 500 + 500
                    var end = viewStartHz + viewRangeHz
                    for (var f = first; f < end; f += 500) {
                        var x = xForFreq(f)
                        if (x < 0 || x >= width)
                            continue
                        out.push({ x: Math.round(x), label: String(Math.round(f)) })
                    }
                    return out
                }

                function decodeColor(label) {
                    if (waterfallDisplay.labelUseCustomColor)
                        return waterfallDisplay.labelColor
                    // 1.0.366+ fix regressione (dal 1.0.332): rispetta il colore
                    // highlight precalcolato (DXCC/new/worked-before, campo `color`
                    // = highlightBg dal modello). Dal 1.0.332 il rendering era passato
                    // al Repeater QML che ignorava questo campo -> i call uscivano solo
                    // coi 3 colori fissi. Ora torna l'highlight WSJT-X-like.
                    if (label.color && String(label.color).length > 0)
                        return label.color
                    if (label.isMyCall)
                        return "#ff5050"
                    if (label.isCQ)
                        return "#00e664"
                    return "#00c8ff"
                }

                property var cachedFrequencyGridModel: []
                property var cachedTickModel: []

                Timer {
                    id: staticOverlayRefreshTimer
                    interval: 120
                    repeat: false
                    onTriggered: {
                        if (spectrumGpuOverlay.cppOverlayEnabled)
                            return
                        spectrumGpuOverlay.cachedFrequencyGridModel =
                            spectrumGpuOverlay.frequencyGridModel()
                        spectrumGpuOverlay.cachedTickModel =
                            spectrumGpuOverlay.tickModel()
                    }
                }

                function scheduleStaticOverlayRefresh() {
                    if (cppOverlayEnabled)
                        return
                    if (!staticOverlayRefreshTimer.running)
                        staticOverlayRefreshTimer.start()
                }

                // 1.0.175 — Cache del label model con throttle 4 Hz.
                // Su banda affollata (FT2 async 5-10 decode/sec) il sort+pack
                // di decodeLabelModel() veniva chiamato troppo spesso, in
                // competizione col paint del waterfall → label che saltano
                // frame e si "accavallano". Cachiamo il risultato e
                // refreshiamo al massimo 4 volte al secondo.
                property var cachedDecodeLabelModel: []

                Timer {
                    id: decodeLabelRefreshTimer
                    interval: 250
                    repeat: false
                    onTriggered: {
                        if (spectrumGpuOverlay.cppOverlayEnabled && !spectrumGpuOverlay.nativeDecodeLabelsEnabled)
                            return
                        spectrumGpuOverlay.cachedDecodeLabelModel =
                            spectrumGpuOverlay.decodeLabelModel()
                    }
                }

                Connections {
                    target: waterfallPanel
                    function onSpectrumDecodeLabelsChanged() {
                        if (spectrumGpuOverlay.cppOverlayEnabled && !spectrumGpuOverlay.nativeDecodeLabelsEnabled)
                            return
                        if (!waterfallPanel.showDecodeCallsigns) {
                            decodeLabelRefreshTimer.stop()
                            spectrumGpuOverlay.cachedDecodeLabelModel = []
                            return
                        }
                        if (!decodeLabelRefreshTimer.running)
                            decodeLabelRefreshTimer.start()
                    }
                    function onShowDecodeCallsignsChanged() {
                        if (spectrumGpuOverlay.cppOverlayEnabled && !spectrumGpuOverlay.nativeDecodeLabelsEnabled)
                            return
                        if (!waterfallPanel.showDecodeCallsigns) {
                            decodeLabelRefreshTimer.stop()
                            spectrumGpuOverlay.cachedDecodeLabelModel = []
                            return
                        }
                        if (!decodeLabelRefreshTimer.running)
                            decodeLabelRefreshTimer.start()
                    }
                }

                onWidthChanged:  {
                    if (!cppOverlayEnabled || nativeDecodeLabelsEnabled) {
                        if (!decodeLabelRefreshTimer.running) decodeLabelRefreshTimer.start()
                    }
                    if (!cppOverlayEnabled) {
                        scheduleStaticOverlayRefresh()
                    }
                }
                onHeightChanged: {
                    if (!cppOverlayEnabled || nativeDecodeLabelsEnabled) {
                        if (!decodeLabelRefreshTimer.running) decodeLabelRefreshTimer.start()
                    }
                    if (!cppOverlayEnabled) {
                        scheduleStaticOverlayRefresh()
                    }
                }
                onViewStartHzChanged: scheduleStaticOverlayRefresh()
                onViewRangeHzChanged: scheduleStaticOverlayRefresh()

                Component.onCompleted: {
                    if (cppOverlayEnabled) {
                        cachedFrequencyGridModel = []
                        cachedTickModel = []
                        cachedDecodeLabelModel = nativeDecodeLabelsEnabled ? decodeLabelModel() : []
                    } else {
                        cachedFrequencyGridModel = frequencyGridModel()
                        cachedTickModel = tickModel()
                        cachedDecodeLabelModel = decodeLabelModel()
                    }
                }

                function decodeLabelModel() {
                    if (!waterfallPanel.showDecodeCallsigns)
                        return []

                    var source = waterfallPanel.spectrumDecodeLabels || []
                    var items = []
                    var i
                    for (i = 0; i < source.length; ++i) {
                        var d = source[i]
                        var call = d.call || ""
                        var freq = Number(d.freq || 0)
                        if (!call || !waterfallPanel.isValidClickableAudioFreq(freq))
                            continue
                        var x = xForFreq(freq)
                        if (x < 0 || x >= width)
                            continue
                        var text = call + " " + Number(d.snr || 0)
                        var effectiveLabelFontSize = Math.max(10, waterfallDisplay.labelFontSize)
                        items.push({
                            x: Math.round(x),
                            text: text,
                            color: decodeColor(d),
                            call: call,
                            freq: freq,
                            widthHint: Math.max(24, text.length * Math.max(6, effectiveLabelFontSize * 0.68))
                        })
                    }

                    items.sort(function(a, b) { return a.x - b.x })

                    var rowHeight = Math.max(12, Math.max(10, waterfallDisplay.labelFontSize) + 5)
                    var maxRows = Math.max(1, Math.floor(Math.max(1, height - 24) / rowHeight))
                    var rowRight = []
                    for (i = 0; i < maxRows; ++i)
                        rowRight.push(-1000000)

                    var laidOut = []
                    for (i = 0; i < items.length; ++i) {
                        var it = items[i]
                        var textX = clamp(it.x + 2, 2, Math.max(2, width - it.widthHint - 2))
                        var row = -1
                        for (var r = 0; r < maxRows; ++r) {
                            if (textX > rowRight[r] + waterfallDisplay.labelSpacing) {
                                row = r
                                break
                            }
                        }
                        if (row < 0)
                            continue
                        laidOut.push({
                            x: it.x,
                            textX: textX,
                            y: 2 + row * rowHeight,
                            text: it.text,
                            color: it.color,
                            call: it.call,
                            freq: it.freq,
                            widthHint: it.widthHint,
                            rowHeight: rowHeight
                        })
                        rowRight[row] = textX + it.widthHint
                    }
                    return laidOut
                }

                Rectangle {
                    x: waterfallDisplay.rxFilterLeftX
                    y: 0
                    width: Math.max(0, waterfallDisplay.rxFilterRightX - waterfallDisplay.rxFilterLeftX)
                    height: parent.height
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && width > 0
                    color: Qt.rgba(0.31, 0.43, 0.47, 0.09)
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? 0 : 6
                    Rectangle {
                        width: spectrumGpuOverlay.width
                        height: 1
                        x: 0
                        y: Math.round(spectrumGpuOverlay.height - 1 - (index / 5) * (spectrumGpuOverlay.height - 16))
                        color: "#262626"
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? 0 : 6
                    Text {
                        readonly property real norm: index / 5
                        x: 2
                        y: Math.max(0, Math.round(spectrumGpuOverlay.height - 1 - norm * (spectrumGpuOverlay.height - 16)) - height)
                        text: String(Math.round(waterfallDisplay.minDb + norm * spectrumGpuOverlay.dbRange))
                        color: "#a0a0a0"
                        font.family: spectrumGpuOverlay.fixedFontFamily
                        font.pixelSize: 8
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedFrequencyGridModel
                    Rectangle {
                        x: modelData.x
                        y: 0
                        width: 1
                        height: Math.max(0, spectrumGpuOverlay.height - 16)
                        color: "#282828"
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedFrequencyGridModel
                    Text {
                        x: spectrumGpuOverlay.clamp(modelData.x - 18, 2, Math.max(2, spectrumGpuOverlay.width - width - 2))
                        y: Math.max(0, spectrumGpuOverlay.height - height - 1)
                        text: modelData.label
                        color: "#dcdcdc"
                        font.family: spectrumGpuOverlay.fixedFontFamily
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedTickModel
                    Rectangle {
                        x: modelData.x
                        y: Math.max(0, spectrumGpuOverlay.height - 18)
                        width: 1
                        height: 12
                        color: "#ffe600"
                        opacity: 0.70
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedTickModel
                    Text {
                        x: spectrumGpuOverlay.clamp(modelData.x - 20, 2, Math.max(2, spectrumGpuOverlay.width - width - 2))
                        y: Math.max(0, spectrumGpuOverlay.height - 36)
                        text: modelData.label
                        color: "#dcd200"
                        font.family: spectrumGpuOverlay.fixedFontFamily
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.nativeDecodeLabelsEnabled
                           ? spectrumGpuOverlay.cachedDecodeLabelModel
                           : (spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedDecodeLabelModel)
                    Item {
                        width: spectrumGpuOverlay.width
                        height: spectrumGpuOverlay.height
                        Rectangle {
                            x: modelData.x
                            y: 0
                            width: 1
                            height: Math.max(0, spectrumGpuOverlay.height - 20)
                            color: modelData.color
                            opacity: 0.52
                        }
                        Rectangle {
                            x: Math.round(modelData.textX - 2)
                            y: Math.round(modelData.y - 1)
                            width: Math.round(modelData.widthHint + 4)
                            height: Math.round(modelData.rowHeight)
                            color: "#000000"
                            opacity: 0.48
                            radius: 0
                        }
                        Text {
                            x: Math.round(modelData.textX)
                            y: Math.round(modelData.y)
                            text: modelData.text
                            color: modelData.color
                            font.family: spectrumGpuOverlay.fixedFontFamily
                            font.pixelSize: Math.max(10, waterfallDisplay.labelFontSize)
                            font.bold: waterfallDisplay.labelBold
                            font.hintingPreference: Font.PreferFullHinting
                            renderType: Text.QtRendering
                        }
                        MouseArea {
                            x: Math.round(modelData.textX - 3)
                            y: Math.round(modelData.y - 2)
                            width: Math.round(modelData.widthHint + 6)
                            height: Math.round(modelData.rowHeight + 2)
                            acceptedButtons: Qt.LeftButton
                            propagateComposedEvents: true
                            cursorShape: Qt.PointingHandCursor
                            // Click sul NOMINATIVO = chiama quella stazione. Fino alla
                            // 1.0.337 serviva Ctrl+click e il click semplice si limitava
                            // a spostare la frequenza TX, cosa che non si indovina: chi
                            // vede un call sul waterfall si aspetta di poterlo chiamare
                            // cliccandolo. Per la sola frequenza restano il click sul
                            // waterfall fuori dall'etichetta e Ctrl+click sull'etichetta,
                            // che non viene accettato qui e passa all'item C++.
                            onPressed: function(mouse) { mouse.accepted = !(mouse.modifiers & Qt.ControlModifier) }
                            onClicked: function(mouse) {
                                if (mouse.modifiers & Qt.ControlModifier)
                                    return
                                if (waterfallPanel.isValidClickableAudioFreq(modelData.freq))
                                    bridge.engageDxClusterSpot(modelData.call, modelData.freq)
                            }
                        }
                    }
                }

                Item {
                    id: rxMarkerLine
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.rxFreq)
                    x: Math.round(markerX)
                    y: 0
                    width: 1
                    height: parent.height
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && markerX >= 0 && markerX < spectrumGpuOverlay.width
                    Rectangle { x: -3; width: 7; height: parent.height; color: "#00e5ff"; opacity: 0.27 }
                    Rectangle { x: -1; width: 3; height: parent.height; color: "#00e5ff"; opacity: 0.94 }
                    Rectangle { x: 0; width: 1; height: parent.height; color: "#b4ffff" }
                }

                Item {
                    id: txMarkerLine
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.txFreq)
                    x: Math.round(markerX)
                    y: 0
                    width: 1
                    height: parent.height
                    // 1.0.288 — marker TX magenta nascosto: l'UNICO indicatore TX è la
                    // linea rossa animata (txCarrierActiveBar), visibile solo in trasmissione.
                    visible: false
                    Rectangle { x: -3; width: 7; height: parent.height; color: "#ff00ff"; opacity: 0.27 }
                    Rectangle { x: -1; width: 3; height: parent.height; color: "#ff00ff"; opacity: 0.94 }
                    Rectangle { x: 0; width: 1; height: parent.height; color: "#ffc8ff" }
                }

                // I due riferimenti dell'RTTY: mark e space, alle frequenze in cui
                // il decodificatore li aspetta. Sono fissi — non seguono il
                // segnale, e' il segnale che va portato sopra di loro, girando
                // la sintonia finche' le due portanti ci si appoggiano. E' il
                // modo in cui si accorda l'RTTY in ogni programma che lo fa, e
                // senza questi due riferimenti il waterfall mostra dove sono i
                // toni ma non dove dovrebbero essere.
                //
                // Compaiono solo in RTTY: negli altri modi sarebbero due righe
                // in mezzo allo spettro senza alcun significato.
                Repeater {
                    id: rttyToneMarkers
                    model: {
                        if (typeof rtty === 'undefined' || !rtty) return []
                        if (!bridge || String(bridge.mode).toUpperCase() !== "RTTY") return []
                        var mark = Number(rtty.markHz)
                        var shift = Number(rtty.shiftHz)
                        if (!(mark > 0) || !(shift > 0)) return []
                        // Il piu' basso e' lo space: nella convenzione di questo
                        // decodificatore il mark sta in alto e lo shift si conta
                        // verso il basso. REV scambia il significato dei due
                        // toni ma non le loro frequenze, quindi le righe non si
                        // spostano — ed e' giusto cosi': si accorda sulle stesse
                        // due righe in un senso e nell'altro.
                        return [{ hz: mark - shift, nome: "S" },
                                { hz: mark,         nome: "M" }]
                    }

                    delegate: Item {
                        required property var modelData
                        readonly property real markerX: spectrumGpuOverlay.xForFreq(modelData.hz)
                        x: Math.round(markerX)
                        y: 0
                        width: 1
                        height: parent.height
                        visible: markerX >= 0 && markerX < spectrumGpuOverlay.width
                        z: 40

                        // Un alone largo e tenue piu' una riga sottile e netta:
                        // la riga dice dove, l'alone la rende visibile anche
                        // sopra il giallo acceso di una portante forte.
                        Rectangle { x: -3; width: 7; height: parent.height; color: "#ffb000"; opacity: 0.22 }
                        Rectangle { x: 0;  width: 1; height: parent.height; color: "#ffd166"; opacity: 0.9 }

                        // La lettera in cima, piccola: M e S si riconoscono a
                        // colpo d'occhio e non rubano spazio allo spettro.
                        Text {
                            x: 3
                            y: 2
                            text: parent.modelData.nome
                            color: "#ffd166"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                // 1.0.365+ (fork) - MAM multi-stream: marker per ogni slot QSO
                // attivo (modello Fox/hunter). Una linea verticale arancione +
                // etichetta col call alla freq audio dello slot, mappata con la
                // stessa xForFreq() di RX/TX/decode. Gated da bridge.mamMultiStream:
                // con il MAM OFF mamActiveSlots e' vuoto e nulla viene disegnato.
                Repeater {
                    id: mamSlotMarkers
                    model: bridge && bridge.mamMultiStream ? bridge.mamActiveSlots : []
                    delegate: Item {
                        readonly property real slotFreq: Number(modelData.freq)
                        readonly property string slotCall: String(modelData.call)
                        readonly property bool slotTx: modelData.tx !== undefined && Number(modelData.tx) > 0
                        readonly property real markerX: spectrumGpuOverlay.xForFreq(slotFreq)
                        x: 0
                        y: 0
                        width: spectrumGpuOverlay.width
                        height: spectrumGpuOverlay.height
                        z: 40
                        visible: bridge && bridge.mamMultiStream
                                 && markerX >= 0 && markerX < spectrumGpuOverlay.width
                        // Linea verticale arancione (distinta dal RX ciano e dal TX rosso).
                        Rectangle {
                            x: Math.round(markerX) - 1
                            y: 0
                            width: 2
                            height: parent.height
                            color: "#ffa000"
                            opacity: 0.92
                        }
                        // Etichetta col call dello slot, ancorata in alto.
                        Rectangle {
                            id: mamSlotLabelBox
                            readonly property real boxW: mamSlotLabelText.implicitWidth + 8
                            x: spectrumGpuOverlay.markerBoxX(markerX, boxW)
                            y: 2
                            width: boxW
                            height: mamSlotLabelText.implicitHeight + 4
                            color: "#331a00"
                            border.width: 1
                            border.color: "#ffa000"
                            opacity: 0.92
                            radius: 2
                            Text {
                                id: mamSlotLabelText
                                anchors.centerIn: parent
                                text: slotCall + (slotTx ? " TX" : "")
                                color: "#ffd27f"
                                font.family: spectrumGpuOverlay.fixedFontFamily
                                font.pixelSize: Math.max(9, waterfallDisplay.labelFontSize - 1)
                                font.bold: true
                                renderType: Text.QtRendering
                            }
                        }
                    }
                }

                Item {
                    id: txSignalWidthGuide
                    z: -1
                    anchors.fill: parent
                    // 1.0.288 — guida banda TX magenta nascosta: l'unico indicatore TX è
                    // la linea rossa animata (txCarrierActiveBar), visibile solo in TX.
                    visible: false

                    readonly property real leftX: waterfallDisplay.txSignalLeftX
                    readonly property real rightX: waterfallDisplay.txSignalRightX
                    readonly property real clampedLeftX: spectrumGpuOverlay.clamp(leftX, 0, spectrumGpuOverlay.width)
                    readonly property real clampedRightX: spectrumGpuOverlay.clamp(rightX, 0, spectrumGpuOverlay.width)

                    Rectangle {
                        x: txSignalWidthGuide.clampedLeftX
                        y: 0
                        width: Math.max(1, txSignalWidthGuide.clampedRightX - txSignalWidthGuide.clampedLeftX)
                        height: parent.height
                        color: Qt.rgba(1.0, 0.0, 1.0, 0.055)
                    }
                    Rectangle {
                        x: Math.round(txSignalWidthGuide.leftX) - 1
                        y: 0
                        width: 2
                        height: parent.height
                        visible: txSignalWidthGuide.leftX >= 0 && txSignalWidthGuide.leftX <= spectrumGpuOverlay.width
                        color: Qt.rgba(1.0, 0.40, 1.0, 0.82)
                    }
                    Rectangle {
                        x: Math.round(txSignalWidthGuide.rightX) - 1
                        y: 0
                        width: 2
                        height: parent.height
                        visible: txSignalWidthGuide.rightX >= 0 && txSignalWidthGuide.rightX <= spectrumGpuOverlay.width
                        color: Qt.rgba(1.0, 0.40, 1.0, 0.82)
                    }
                }

                // 1.0.269 (fork-only) — TX CARRIER ACTIVE: barra rossa larga ~50Hz
                // (banda della portante FT8) che si accende SOLO quando bridge.transmitting=true
                // o bridge.tuning=true. Indica visivamente che la portante e' on-air.
                // 1.0.270: z=50 per disegnare SOPRA rxMarkerLabel/txMarkerLabel (z=0)
                // che venivano dichiarati dopo e coprivano la banda rossa.
                Item {
                    id: txCarrierActiveBar
                    z: 50
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.txFreq)
                    readonly property real carrierBwHz: Math.max(50, waterfallDisplay.txSignalBandwidthHz)
                    readonly property real barWidthPx: Math.max(8, carrierBwHz * spectrumGpuOverlay.width / Math.max(1, spectrumGpuOverlay.viewRangeHz))
                    x: Math.round(markerX - barWidthPx / 2)
                    y: 0
                    width: barWidthPx
                    height: parent.height
                    visible: bridge && (bridge.transmitting || bridge.tuning)
                             && markerX >= 0 && markerX < spectrumGpuOverlay.width
                    // 1.0.288 — pulsazione animata: "linea rossa animata" che indica la TX in corso.
                    SequentialAnimation on opacity {
                        running: txCarrierActiveBar.visible
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.45; duration: 400; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 0.45; to: 1.0; duration: 400; easing.type: Easing.InOutSine }
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(1.0, 0.12, 0.12, 0.32)
                        border.color: Qt.rgba(1.0, 0.20, 0.20, 0.95)
                        border.width: 2
                    }
                    Rectangle {
                        x: parent.width / 2 - 1
                        width: 2
                        height: parent.height
                        color: Qt.rgba(1.0, 0.25, 0.25, 0.98)
                    }
                    // Etichetta "TX ON-AIR" sopra
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        width: txOnAirText.implicitWidth + 10
                        height: txOnAirText.implicitHeight + 4
                        radius: 3
                        color: Qt.rgba(0.85, 0.05, 0.05, 0.95)
                        Text {
                            id: txOnAirText
                            anchors.centerIn: parent
                            text: bridge && bridge.tuning ? "TUNE" : "TX ON-AIR"
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                Item {
                    id: rxMarkerLabel
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.rxFreq)
                    readonly property real centerY: spectrumGpuOverlay.height / 2 - (spectrumGpuOverlay.txVisible ? 12 : 0)
                    width: rxMarkerText.implicitWidth + 12
                    height: rxMarkerText.implicitHeight + 6
                    x: spectrumGpuOverlay.markerBoxX(markerX, width)
                    y: spectrumGpuOverlay.markerBoxY(centerY, height)
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && markerX >= 0 && markerX < spectrumGpuOverlay.width
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Qt.rgba(0, 0, 0, 0.70)
                        border.color: "#00e5ff"
                        border.width: 1
                    }
                    Text {
                        id: rxMarkerText
                        anchors.centerIn: parent
                        text: "RX " + Math.round(waterfallDisplay.rxFreq)
                        color: "#00e5ff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Item {
                    id: txMarkerLabel
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.txFreq)
                    readonly property real centerY: spectrumGpuOverlay.height / 2 + 12
                    width: txMarkerText.implicitWidth + 12
                    height: txMarkerText.implicitHeight + 6
                    x: spectrumGpuOverlay.markerBoxX(markerX, width)
                    y: spectrumGpuOverlay.markerBoxY(centerY, height)
                    // 1.0.288 — etichetta TX nascosta: unico indicatore TX = linea rossa animata in TX.
                    visible: false
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Qt.rgba(0, 0, 0, 0.70)
                        border.color: "#ff00ff"
                        border.width: 1
                    }
                    Text {
                        id: txMarkerText
                        anchors.centerIn: parent
                        text: "TX " + Math.round(waterfallDisplay.txFreq)
                        color: "#ff00ff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 4
                    anchors.bottomMargin: 2
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && waterfallDisplay.autoRange
                    text: "NF:" + Math.round(waterfallDisplay.measuredFloor) + "dB"
                    color: "#646464"
                    font.family: spectrumGpuOverlay.fixedFontFamily
                    font.pixelSize: 8
                }
            }

            Rectangle {
                id: rxFilterWaterfallOverlay
                x: waterfallDisplay.rxFilterLeftX
                y: waterfallDisplay.spectrumHeight
                z: 2
                width: Math.max(0, waterfallDisplay.rxFilterRightX - waterfallDisplay.rxFilterLeftX)
                height: Math.max(0, waterfallDisplay.height - waterfallDisplay.spectrumHeight)
                visible: (bridge.monitoring || bridge.transmitting || bridge.tuning) && width > 0 && height > 0
                color: Qt.rgba(0.31, 0.43, 0.47, 0.10)
                border.color: Qt.rgba(0.55, 0.80, 0.90, 0.16)
                border.width: 1
            }

            // 1.0.288 — Linea TX ROSSA animata NELLA CASCATA (al centro della portante TX),
            // visibile SOLO in trasmissione/tune. Sostituisce la vecchia guida magenta
            // (nascosta come nello spettro). Solo visuale, niente mouse.
            Item {
                id: txSignalWaterfallGuide
                x: 0
                y: waterfallDisplay.spectrumHeight
                z: 3
                width: waterfallDisplay.width
                height: Math.max(0, waterfallDisplay.height - waterfallDisplay.spectrumHeight)
                readonly property real markerX: waterfallDisplay.freqToPixel(waterfallDisplay.txFreq)
                visible: bridge && (bridge.transmitting || bridge.tuning)
                         && height > 0 && markerX >= 0 && markerX < width

                // 1.0.288 — onda rossa che si propaga lateralmente dal centro e svanisce
                // (effetto evanescente "come se la riga si propagasse"), in loop durante il TX.
                Rectangle {
                    id: txCascadeWave
                    y: 0
                    height: parent.height
                    radius: 3
                    color: "#ff3030"
                    x: txSignalWaterfallGuide.markerX - width / 2
                    ParallelAnimation {
                        running: txSignalWaterfallGuide.visible
                        loops: Animation.Infinite
                        NumberAnimation { target: txCascadeWave; property: "width"; from: 3; to: 54; duration: 1200; easing.type: Easing.OutQuad }
                        NumberAnimation { target: txCascadeWave; property: "opacity"; from: 0.55; to: 0.0; duration: 1200; easing.type: Easing.OutQuad }
                    }
                }

                // Linea centrale rossa (portante TX)
                Rectangle { x: Math.round(txSignalWaterfallGuide.markerX) - 4; width: 9; height: parent.height; color: "#ff1414"; opacity: 0.22 }
                Rectangle { x: Math.round(txSignalWaterfallGuide.markerX) - 1; width: 3; height: parent.height; color: "#ff2020"; opacity: 0.95 }
                Rectangle { x: Math.round(txSignalWaterfallGuide.markerX);     width: 1; height: parent.height; color: "#ffd0d0" }

                // Marca "TX" in cima alla linea
                Rectangle {
                    y: 2
                    width: txCascadeBadge.implicitWidth + 10
                    height: txCascadeBadge.implicitHeight + 4
                    radius: 3
                    x: Math.max(0, Math.min(parent.width - width,
                               Math.round(txSignalWaterfallGuide.markerX) - width / 2))
                    color: Qt.rgba(0.85, 0.05, 0.05, 0.95)
                    border.color: "#ffffff"
                    border.width: 1
                    Text {
                        id: txCascadeBadge
                        anchors.centerIn: parent
                        text: "TX"
                        color: "#ffffff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            // Overlay "Start monitoring"
            Rectangle {
                anchors.centerIn: parent
                width: startText.width + 40; height: startText.height + 20
                color: Qt.rgba(0, 0, 0, 0.85)
                border.color: accentCyan; border.width: 1; radius: 6
                visible: !bridge.monitoring && !bridge.transmitting && !bridge.tuning
                Text {
                    id: startText
                    anchors.centerIn: parent
                    text: qsTr("4096-bin panadapter · SmartSDR style\nClick MONITOR to start")
                    font.pixelSize: 12
                    color: "#B4B4B4"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
	        }
	    }

    Rectangle {
        id: waterfallControlsToggle
        visible: waterfallPanel.showControls && !waterfallPanel.controlsExpanded
        // Posizione libera (mobile): -1 = non impostato \u2192 default top-right come prima.
        property real userX: -1
        property real userY: -1
        x: userX >= 0 ? Math.max(0, Math.min(userX, parent.width - width)) : (parent.width - width - 72)
        y: userY >= 0 ? Math.max(0, Math.min(userY, parent.height - height)) : 8
        width: Math.min(158, Math.max(134, parent.width - 88))
        height: 30
        z: 200
        radius: 7
        color: toggleMouse.containsMouse
               ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.30)
               : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.90)
        border.color: accentCyan
        border.width: toggleMouse.containsMouse ? 2 : 1

        // Ripristina posizione salvata all'avvio (stesso helper bridge.getSetting delle altre graph-settings)
        Component.onCompleted: {
            var sx = bridge.getSetting("uiWfControlsToggleX", -1)
            var sy = bridge.getSetting("uiWfControlsToggleY", -1)
            if (sx >= 0) userX = waterfallPanel.clampNumber(sx, 0, Math.max(0, parent.width - width), -1)
            if (sy >= 0) userY = waterfallPanel.clampNumber(sy, 0, Math.max(0, parent.height - height), -1)
        }

        Row {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 9
            spacing: 7
            Text {
                text: qsTr("\u2630")
                color: accentCyan
                font.pixelSize: 14
                font.bold: true
            }
            Text {
                text: qsTr("Show controls")
                color: accentCyan
                font.pixelSize: 11
                font.bold: true
            }
        }

        MouseArea {
            id: toggleMouse
            anchors.fill: parent
            anchors.leftMargin: 20
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: waterfallPanel.setControlsExpanded(!waterfallPanel.controlsExpanded)
        }

        // Maniglia di trascinamento (pattern World Clock / MamWindow): solo questa muove il pulsante.
        Rectangle {
            id: wfToggleHandle
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 3
            width: 16
            height: 24
            radius: 4
            color: wfToggleHandleMA.containsMouse
                   ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.25)
                   : "transparent"
            Text {
                anchors.centerIn: parent
                text: qsTr("\u283f")
                color: accentCyan
                font.pixelSize: 12
            }
            MouseArea {
                id: wfToggleHandleMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeAllCursor
                property point clickPos: Qt.point(0, 0)
                onPressed: function(mouse) {
                    clickPos = Qt.point(mouse.x, mouse.y)
                    if (waterfallControlsToggle.userX < 0) waterfallControlsToggle.userX = waterfallControlsToggle.x
                    if (waterfallControlsToggle.userY < 0) waterfallControlsToggle.userY = waterfallControlsToggle.y
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    var nx = waterfallControlsToggle.userX + (mouse.x - clickPos.x)
                    var ny = waterfallControlsToggle.userY + (mouse.y - clickPos.y)
                    var p = waterfallControlsToggle.parent
                    waterfallControlsToggle.userX = Math.max(0, Math.min(nx, p.width - waterfallControlsToggle.width))
                    waterfallControlsToggle.userY = Math.max(0, Math.min(ny, p.height - waterfallControlsToggle.height))
                }
                onReleased: {
                    waterfallPanel.persistGraphSetting("uiWfControlsToggleX", Math.round(waterfallControlsToggle.userX))
                    waterfallPanel.persistGraphSetting("uiWfControlsToggleY", Math.round(waterfallControlsToggle.userY))
                }
            }
        }

        ToolTip.visible: toggleMouse.containsMouse
        ToolTip.delay: 450
        ToolTip.text: qsTr("Show waterfall controls")
    }

    // Sync Bridge → PanadapterItem
    Connections {
        target: bridge

        // Alta risoluzione FFTW 4096 bin — include range frequenze esatto
        function onPanadapterDataReady(dbValues, minDb, maxDb, freqMinHz, freqMaxHz) {
            if (!waterfallPanel.visible) return
            waterfallDisplay.addSpectrumData(dbValues, minDb, maxDb, freqMinHz, freqMaxHz)
        }
        // Fallback: valori normalizzati 0-1 dal legacy timer
        function onSpectrumDataReady(data) {
            if (!waterfallPanel.visible) return
            if (!bridge.monitoring) return
            waterfallDisplay.addSpectrumDataNorm(data)
        }
        function syncRtlSdrRfMarkers() {
            waterfallDisplay.rxFreq = bridge.rtlSdrRfView
                    ? bridge.rtlSdrRfSelectedFrequency : bridge.rxFrequency
            waterfallDisplay.txFreq = bridge.rtlSdrRfView
                    ? bridge.rtlSdrRfSelectedFrequency : bridge.txFrequency
        }
        function onRxFrequencyChanged() { syncRtlSdrRfMarkers() }
        function onTxFrequencyChanged() { syncRtlSdrRfMarkers() }
        function onRtlSdrRfViewChanged() { syncRtlSdrRfMarkers() }
        function onRtlSdrRfCenterFrequencyChanged() { syncRtlSdrRfMarkers() }
        function onRtlSdrRfSelectedFrequencyChanged() { syncRtlSdrRfMarkers() }
        function onSettingValueChanged(key, value) {
            waterfallPanel.restoringSettings = true
            if (key === "uiPaletteIndex") {
                waterfallPanel.setPaletteIndex(value, false)
            } else if (key === "uiWaterfallControlsExpanded") {
                waterfallPanel.controlsExpanded = waterfallPanel.coerceBool(value, true)
            } else if (key === "uiSpectrumHeight") {
                waterfallPanel.spectrumHeight = Math.round(waterfallPanel.clampNumber(value, waterfallPanel.spectrumMinHeight, waterfallPanel.spectrumMaxHeight, waterfallPanel.spectrumHeight))
            } else if (key === "uiWaterfallAutoRange") {
                autoRangeCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.autoRange = autoRangeCheck.checked
            } else if (key === "uiWaterfallShowTxBrackets") {
                txBracketsCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.showTxBrackets = txBracketsCheck.checked
            } else if (key === "uiWaterfallShowDxCluster") {
                dxClusterCheck.checked = waterfallPanel.coerceBool(value, false)
                waterfallDisplay.showDxClusterSpots = dxClusterCheck.checked
            } else if (key === "uiWaterfallPeakHold") {
                peakHoldCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.peakHold = peakHoldCheck.checked
            } else if (key === "uiWaterfallBlackLevel") {
                blackSlider.value = waterfallPanel.clampNumber(value, blackSlider.from, blackSlider.to, blackSlider.value)
                waterfallDisplay.blackLevel = blackSlider.value
            } else if (key === "uiWaterfallColorGain") {
                gainSlider.value = waterfallPanel.clampNumber(value, gainSlider.from, gainSlider.to, gainSlider.value)
                waterfallDisplay.colorGain = gainSlider.value
            } else if (key === "uiWaterfallContrast") {
                contrastSlider.value = waterfallPanel.clampNumber(value, contrastSlider.from, contrastSlider.to, contrastSlider.value)
                waterfallDisplay.contrastLevel = contrastSlider.value
                waterfallPanel.applyManualContrast()
            } else if (key === "spectrumInterval") {
                speedSlider.value = waterfallPanel.clampNumber(value, speedSlider.from, speedSlider.to, speedSlider.value)
            } else if (key === "uiZoomFactor") {
                zoomSlider.value = waterfallPanel.clampNumber(value, zoomSlider.from, zoomSlider.to, zoomSlider.value)
                waterfallDisplay.zoomFactor = zoomSlider.value
            } else if (key === "uiLabelFontSize") {
                labelFontSlider.value = waterfallPanel.clampNumber(value, labelFontSlider.from, labelFontSlider.to, labelFontSlider.value)
                waterfallDisplay.labelFontSize = labelFontSlider.value
            } else if (key === "uiLabelSpacing") {
                labelSpacingSlider.value = waterfallPanel.clampNumber(value, labelSpacingSlider.from, labelSpacingSlider.to, labelSpacingSlider.value)
                waterfallDisplay.labelSpacing = labelSpacingSlider.value
            } else if (key === "uiLabelBold") {
                labelBoldCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.labelBold = labelBoldCheck.checked
            } else if (key === "uiLabelColorPreset") {
                labelColorCombo.currentIndex = waterfallPanel.clampIndex(value, waterfallPanel.labelColorPresets.length, labelColorCombo.currentIndex)
                var preset = waterfallPanel.labelColorPresets[labelColorCombo.currentIndex]
                waterfallDisplay.labelUseCustomColor = preset.custom
                waterfallDisplay.labelColor = preset.color
            } else if (key === "uiWaterfallShowCallsigns") {
                waterfallPanel.setShowDecodeCallsigns(waterfallPanel.coerceBool(value, true), false)
            } else if (key === "uiWfControlsToggleX") {
                var tx = Number(value)
                if (isFinite(tx) && tx >= 0)
                    waterfallControlsToggle.userX = waterfallPanel.clampNumber(tx, 0, Math.max(0, waterfallControlsToggle.parent.width - waterfallControlsToggle.width), waterfallControlsToggle.userX)
            } else if (key === "uiWfControlsToggleY") {
                var ty = Number(value)
                if (isFinite(ty) && ty >= 0)
                    waterfallControlsToggle.userY = waterfallPanel.clampNumber(ty, 0, Math.max(0, waterfallControlsToggle.parent.height - waterfallControlsToggle.height), waterfallControlsToggle.userY)
            } else {
                waterfallPanel.restoringSettings = false
                return
            }
            waterfallPanel.restoringSettings = false
        }

        // Aggiorna i callsign decodificati sul grafico spettro
        function onDecodeListChanged() {
            if (!bridge.bandActivityModel)
                waterfallPanel.refreshDecodeLabels()
        }
    }

    Connections {
        target: (bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : null
        ignoreUnknownSignals: true
        function onSnapshotApplied() {
            waterfallPanel.refreshDecodeLabels()
        }
    }
}
