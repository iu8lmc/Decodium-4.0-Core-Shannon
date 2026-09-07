/* Decodium Qt6 - Decode Window
 * Split view: Band Activity + RX Frequency
 * Like Decodium
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
// import Qt.labs.settings 1.1  // non disponibile in questa build Qt

Window {
    id: decodeWindow
    title: "Decodium - Decoded Messages - " + appEngine.mode
    minimumWidth: 800
    minimumHeight: 400
    visible: true
    color: bgDeep

    // Carica posizione/dimensioni da bridge
    Component.onCompleted: {
        if (bridge.uiDecodeWinWidth  > 0) decodeWindow.width  = bridge.uiDecodeWinWidth
        if (bridge.uiDecodeWinHeight > 0) decodeWindow.height = bridge.uiDecodeWinHeight
        if (bridge.uiDecodeWinX     !== 0) decodeWindow.x = bridge.uiDecodeWinX
        if (bridge.uiDecodeWinY     !== 0) decodeWindow.y = bridge.uiDecodeWinY
    }
    // Salva al bridge (bridge.saveSettings() chiamato al close di Main)
    onXChanged:      { bridge.uiDecodeWinX      = x;      mainWindow.scheduleSave() }
    onYChanged:      { bridge.uiDecodeWinY      = y;      mainWindow.scheduleSave() }
    onWidthChanged:  { bridge.uiDecodeWinWidth  = width;  mainWindow.scheduleSave() }
    onHeightChanged: { bridge.uiDecodeWinHeight = height; mainWindow.scheduleSave() }

    flags: Qt.Window | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint

    // Dynamic theme colors from ThemeManager
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder
    property int decodeColorBoost: Math.max(0, Math.min(100, Number(bridge.getSetting("uiDecodeColorBoost", 35))))
    property int decodeColorRevision: 0
    property bool showDxccInfo: bridge.getSetting("ShowDXCC", true)
    property bool showTxMessagesInRx: bridge.getSetting("TXMessagesToRX", true)
    property bool displayDistanceInMiles: coerceBool(bridge.getSetting("Miles", false), false)
    readonly property real leftPanelWidth: width * 0.5
    readonly property bool compactBandColumns: leftPanelWidth < 460
    readonly property int bandUtcWidth: compactBandColumns ? 66 : 86
    readonly property int bandDbWidth: compactBandColumns ? 34 : 40
    readonly property int bandDbDtGapWidth: compactBandColumns ? 4 : 6
    readonly property int bandDtWidth: compactBandColumns ? 42 : 50
    readonly property int bandDtFreqGapWidth: compactBandColumns ? 6 : 8
    readonly property int bandFreqWidth: compactBandColumns ? 42 : 50
    readonly property bool bandShowWsprDrift: bridge && bridge.mode === "WSPR"
    readonly property int bandDriftWidth: bandShowWsprDrift ? (compactBandColumns ? 36 : 42) : 0
    readonly property int bandGapWidth: compactBandColumns ? 8 : 12
    readonly property int bandDxccWidth: showDxccInfo ? (compactBandColumns ? 92 : 132) : 0
    readonly property int bandAzWidth: showDxccInfo ? (compactBandColumns ? 42 : 52) : 0
    readonly property int bandMessageMinWidth: compactBandColumns ? 72 : 140
    readonly property real rightPanelWidth: width * 0.5
    readonly property bool compactRxColumns: rightPanelWidth < 450
    readonly property bool compactRxHeader: rightPanelWidth < 340
    readonly property int rxUtcWidth: compactRxColumns ? 62 : 78
    readonly property int rxDbWidth: compactRxColumns ? 34 : 40
    readonly property int rxDbDtGapWidth: compactRxColumns ? 4 : 6
    readonly property int rxDtWidth: compactRxColumns ? 42 : 50
    readonly property int rxGapWidth: compactRxColumns ? 8 : 12
    readonly property int rxDistanceWidth: compactRxColumns ? 0 : 50
    property int decodeListVersion: 0
    property int rxDecodeListVersion: 0
    property bool bandActivitySnapshotPending: false
    property bool rxSnapshotPending: false
    property var bandActivityModel: (bridge && bridge.bandActivityModel) ? [] : filteredDecodeEntries(appEngine.decodeList)
    property bool highlight73: bridge.getSetting("Highlight73", true)
    property bool highlightOrange: bridge.getSetting("HighlightOrange", false)
    property bool highlightBlue: bridge.getSetting("HighlightBlue", false)
    // Decodium 3-style: separatore vuoto tra periodi + ordine inverso
    property bool decodeShowPeriodSeparator: bridge.getSetting("decodeShowPeriodSeparator", true)
    property bool decodeNewestFirst: bridge.getSetting("decodeNewestFirst", false)
    onDecodeShowPeriodSeparatorChanged: {
        if (!decodeWindow.hasNativeBandActivityModel())
            bandActivityModel = filteredDecodeEntries(appEngine.decodeList)
        if (!decodeWindow.hasNativeRxDecodeModel())
            rxDecodeModel = currentRxDecodes()
    }
    onDecodeNewestFirstChanged: {
        if (!decodeWindow.hasNativeBandActivityModel())
            bandActivityModel = filteredDecodeEntries(appEngine.decodeList)
        if (!decodeWindow.hasNativeRxDecodeModel())
            rxDecodeModel = currentRxDecodes()
    }
    function refreshDecodeColors() {
        decodeColorRevision = (decodeColorRevision + 1) % 1000000
    }
	    Connections {
	        target: bridge
	        function onSettingValueChanged(key, value) {
	            if (key === "decodeShowPeriodSeparator")
	                decodeShowPeriodSeparator = value
	            else if (key === "decodeNewestFirst")
	                decodeNewestFirst = value
		            else if (key === "uiDecodeColorBoost") {
		                decodeColorBoost = Math.max(0, Math.min(100, Number(value)))
                        decodeWindow.refreshDecodeColors()
                    }
		        }
                function onDecodeColorEnabledChanged(prop, enabled) {
                    decodeWindow.refreshDecodeColors()
                }
                function onDecodeColorBgChanged() { decodeWindow.refreshDecodeColors() }
                function onColorCQChanged() { decodeWindow.refreshDecodeColors() }
                function onColorMyCallChanged() { decodeWindow.refreshDecodeColors() }
                function onColorDXEntityChanged() { decodeWindow.refreshDecodeColors() }
                function onColor73Changed() { decodeWindow.refreshDecodeColors() }
                function onColorB4Changed() { decodeWindow.refreshDecodeColors() }
                function onColorDecodeTextChanged() { decodeWindow.refreshDecodeColors() }
                function onColorTxMessageChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewDxccChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewDxccBandChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewContinentChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewContinentBandChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewCqZoneChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewCqZoneBandChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewItuZoneChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewItuZoneBandChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewGridChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewGridBandChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewCallChanged() { decodeWindow.refreshDecodeColors() }
                function onColorNewCallBandChanged() { decodeWindow.refreshDecodeColors() }
                function onColorLotwUserChanged() { decodeWindow.refreshDecodeColors() }
                function onDecodeColorBoldChanged(prop, bold) { decodeWindow.refreshDecodeColors() }
		    }
    property bool hideTelemetryOnlyDecodes: Qt.platform.os === "windows"
    property string highlightOrangeCallsigns: bridge.getSetting("HighlightOrangeCallsigns", "")
    property string highlightBlueCallsigns: bridge.getSetting("HighlightBlueCallsigns", "")
    // Signal RX model: property-backed come bandActivityModel — evita il
    // reset del layout ListView che si verifica con function-call model
    // (new array reference ad ogni invocazione → contentY azzerato).
    property var rxDecodeModel: (bridge && bridge.rxDecodeModel) ? [] : currentRxDecodes()
    property var clearedRxDecodeKeys: ({})

    function hasNativeBandActivityModel() {
        return bridge && bridge.bandActivityModel
    }

    function hasNativeRxDecodeModel() {
        return bridge && bridge.rxDecodeModel
    }

    function bandActivityCount() {
        void(decodeWindow.decodeListVersion)
        if (decodeWindow.hasNativeBandActivityModel())
            return bridge.bandActivityModel.count()
        return decodeWindow.bandActivityModel.length
    }

    function signalRxCount() {
        void(decodeWindow.rxDecodeListVersion)
        if (decodeWindow.hasNativeRxDecodeModel())
            return bridge.rxDecodeModel.count()
        return decodeWindow.rxDecodeModel.length
    }

    function queueDecodeSnapshotUiCommit(bandPending, rxPending) {
        bandActivitySnapshotPending = bandActivitySnapshotPending || bandPending
        rxSnapshotPending = rxSnapshotPending || rxPending
        decodeSnapshotUiCommitTimer.restart()
    }

    Timer {
        id: decodeSnapshotUiCommitTimer
        interval: 60
        repeat: false
        onTriggered: {
            if (decodeWindow.bandActivitySnapshotPending) {
                decodeWindow.bandActivitySnapshotPending = false
                decodeWindow.decodeListVersion++
                if (bandActivityList)
                    bandActivityList.followTailAfterModelUpdate()
            }
            if (decodeWindow.rxSnapshotPending) {
                decodeWindow.rxSnapshotPending = false
                decodeWindow.rxDecodeListVersion++
                if (rxFrequencyList)
                    rxFrequencyList.followTailAfterModelUpdate()
            }
        }
    }

    Connections {
        target: (bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : null
        ignoreUnknownSignals: true
        function onSnapshotApplied() {
            decodeWindow.queueDecodeSnapshotUiCommit(true, false)
        }
    }

    Connections {
        target: (bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : null
        ignoreUnknownSignals: true
        function onSnapshotApplied() {
            decodeWindow.queueDecodeSnapshotUiCommit(false, true)
        }
    }

	    Connections {
	        target: appEngine
	        function onDecodeListChanged() {
            if (!decodeWindow.hasNativeBandActivityModel()) {
                decodeWindow.bandActivityModel = filteredDecodeEntries(appEngine.decodeList)
                decodeWindow.decodeListVersion++
                if (bandActivityList)
                    bandActivityList.followTailAfterModelUpdate()
            }
            if (!decodeWindow.hasNativeRxDecodeModel()) {
                decodeWindow.rxDecodeModel = currentRxDecodes()
                decodeWindow.rxDecodeListVersion++
                if (rxFrequencyList)
                    rxFrequencyList.followTailAfterModelUpdate()
            }
        }
        function onRxDecodeListChanged() {
            if (!decodeWindow.hasNativeRxDecodeModel()) {
                decodeWindow.rxDecodeModel = currentRxDecodes()
	            decodeWindow.rxDecodeListVersion++
	            if (rxFrequencyList)
	                rxFrequencyList.followTailAfterModelUpdate()
	        }
	        }
	        function onDxCallChanged() {
	            decodeWindow.clearedRxDecodeKeys = ({})
	            if (!decodeWindow.hasNativeRxDecodeModel()) {
	                decodeWindow.rxDecodeModel = currentRxDecodes()
	                decodeWindow.rxDecodeListVersion++
	                if (rxFrequencyList)
	                    rxFrequencyList.followTailAfterModelUpdate()
	            }
	        }
	        function onRxFrequencyChanged() {
	            decodeWindow.clearedRxDecodeKeys = ({})
	            if (!decodeWindow.hasNativeRxDecodeModel()) {
	                decodeWindow.rxDecodeModel = currentRxDecodes()
	                decodeWindow.rxDecodeListVersion++
	                if (rxFrequencyList)
	                    rxFrequencyList.followTailAfterModelUpdate()
	            }
	        }
	    }

    // Refresh rxDecodeModel anche quando cambia il filtro Tx2QSO/TXMessagesToRX
    onShowTxMessagesInRxChanged: {
        if (decodeWindow.hasNativeRxDecodeModel())
            return
        rxDecodeModel = currentRxDecodes()
        rxDecodeListVersion++
        if (rxFrequencyList)
            rxFrequencyList.followTailAfterModelUpdate()
    }
    Component.onCompleted: {
        if (!decodeWindow.hasNativeRxDecodeModel())
            rxDecodeModel = currentRxDecodes()
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            if (key === "ShowDXCC" || key === "DXCCEntity")
                decodeWindow.showDxccInfo = !!value
            else if (key === "TXMessagesToRX" || key === "Tx2QSO")
                decodeWindow.showTxMessagesInRx = !!value
            else if (key === "Highlight73")
                decodeWindow.highlight73 = !!value
            else if (key === "HighlightOrange")
                decodeWindow.highlightOrange = !!value
            else if (key === "HighlightBlue")
                decodeWindow.highlightBlue = !!value
            else if (key === "HighlightOrangeCallsigns" || key === "OrangeCallsigns")
                decodeWindow.highlightOrangeCallsigns = String(value || "")
            else if (key === "HighlightBlueCallsigns" || key === "BlueCallsigns")
                decodeWindow.highlightBlueCallsigns = String(value || "")
            else if (key === "Miles")
                decodeWindow.displayDistanceInMiles = decodeWindow.coerceBool(value, false)
        }
    }

    function coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return !!fallback
        if (typeof value === "boolean")
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return !!fallback
    }

    function formatDistanceText(distanceKm, withSpace) {
        var km = Number(distanceKm)
        if (!isFinite(km) || km <= 0)
            return ""
        var value = displayDistanceInMiles ? km * 0.621371192 : km
        return Math.round(value) + (withSpace ? " " : "") + (displayDistanceInMiles ? "mi" : "km")
    }

    // Shannon-compatible color scheme
    function isSignoffMessage(message) {
        var words = String(message || "").toUpperCase().replace(/[<>;,]/g, " ").split(/\s+/)
        for (var i = 0; i < words.length; ++i) {
            if (words[i] === "73" || words[i] === "RR73" || words[i] === "RRR")
                return true
        }
        return false
    }

    function highlightListMatches(message, listText) {
        var wanted = String(listText || "").toUpperCase().split(/[,\s;]+/)
        var messageText = " " + String(message || "").toUpperCase().replace(/[<>;,]/g, " ") + " "
        for (var i = 0; i < wanted.length; ++i) {
            var token = wanted[i].trim()
            if (token.length > 0 && messageText.indexOf(" " + token + " ") !== -1)
                return true
        }
        return false
    }

    function customHighlightColor(modelData) {
        var message = modelData.message || ""
        if (highlightOrange && highlightListMatches(message, highlightOrangeCallsigns))
            return "#E14B00"
        if (highlightBlue && highlightListMatches(message, highlightBlueCallsigns))
            return "#0064FF"
        return ""
    }

    function effectiveDecodeColor(prop) {
        decodeWindow.decodeColorRevision
        return bridge.effectiveDecodeColor(prop)
    }

    // 1.0.416 — sfondo riga scelto dall'utente per categoria (opt-in). null se non abilitato.
    function decodeUserBgFill(modelData) {
        decodeWindow.decodeColorRevision
        if (!modelData) return null
        var hex = bridge.decodeHighlightUserBg(modelData)
        return (hex && hex.length > 0) ? Qt.color(hex) : null
    }

    function wsjtxHighlightHex(modelData) {
        decodeWindow.decodeColorRevision
        var hex = bridge.decodeHighlightBg(modelData)
        if (!hex || hex.length === 0) return ""
        return hex
    }

    function wsjtxRowHighlightHex(modelData) {
        var hex = wsjtxHighlightHex(modelData)
        if (hex.length === 0 || !modelData)
            return ""

        // New DXCC/grid/call/zone classes can legitimately match most rows.
        // Render those as text colors only; reserve row fills for strong events.
        if (modelData.isTx === true || modelData.isMyCall === true)
            return hex
        return ""
    }

    function wsjtxTextHighlightColor(modelData) {
        var hex = wsjtxHighlightHex(modelData)
        if (hex.length === 0 || !modelData)
            return ""
        if (modelData.isTx === true || modelData.isMyCall === true)
            return ""
        return hex
    }

    function decodeClamp01(value) {
        return Math.max(0, Math.min(1, Number(value)))
    }

    function decodeColorObject(value) {
        if (value === undefined || value === null)
            return null
        if (typeof value === "object" && value.r !== undefined)
            return value
        var text = String(value)
        if (text.length === 0)
            return null
        try {
            return Qt.color(text)
        } catch (e) {
            return null
        }
    }

    function boostedDecodeTextColor(value) {
        var boost = Math.max(0, Math.min(100, Number(decodeWindow.decodeColorBoost))) / 100.0
        if (boost <= 0)
            return value
        var c = decodeColorObject(value)
        if (!c || c.a <= 0)
            return value
        var lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        var sat = 0.85 * boost
        var r = decodeClamp01(lum + (c.r - lum) * (1.0 + sat))
        var g = decodeClamp01(lum + (c.g - lum) * (1.0 + sat))
        var b = decodeClamp01(lum + (c.b - lum) * (1.0 + sat))
        var boostedLum = 0.2126 * r + 0.7152 * g + 0.0722 * b
        var targetLum = lum < 0.55
                ? lum + (0.68 - lum) * 0.75 * boost
                : lum + (0.95 - lum) * 0.35 * boost
        if (boostedLum < targetLum) {
            var mix = Math.min(0.75, (targetLum - boostedLum) / Math.max(0.001, 1.0 - boostedLum))
            r = r + (1.0 - r) * mix
            g = g + (1.0 - g) * mix
            b = b + (1.0 - b) * mix
        }
        return Qt.rgba(decodeClamp01(r), decodeClamp01(g), decodeClamp01(b), c.a)
    }

    function boostedDecodeBackgroundColor(value) {
        var boost = Math.max(0, Math.min(100, Number(decodeWindow.decodeColorBoost))) / 100.0
        if (boost <= 0)
            return value
        var c = decodeColorObject(value)
        if (!c || c.a <= 0)
            return value
        var lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        var sat = 0.55 * boost
        var r = decodeClamp01(c.r + (c.r - lum) * sat)
        var g = decodeClamp01(c.g + (c.g - lum) * sat)
        var b = decodeClamp01(c.b + (c.b - lum) * sat)
        var alphaLift = c.a < 0.12 ? 0.18 * boost : 0.28 * boost
        var alphaCap = c.a < 0.12 ? 0.32 : 0.72
        return Qt.rgba(r, g, b, Math.min(alphaCap, c.a + alphaLift))
    }

    function readableTextOnHighlight(hex) {
        var c = Qt.color(hex)
        var luminance = (0.299 * c.r) + (0.587 * c.g) + (0.114 * c.b)
        return luminance > 0.55 ? "#000000" : "#FFFFFF"
    }

    // WSJT-X background cascade for strong events only.
    // Restituisce il fill traslucido per il delegate, oppure null per fallback.
    function wsjtxBgColor(modelData) {
        var hex = wsjtxRowHighlightHex(modelData)
        if (hex.length === 0) return null
        var c = Qt.color(hex)
        return boostedDecodeBackgroundColor(Qt.rgba(c.r, c.g, c.b, 0.35))
    }
    function wsjtxBorderColor(modelData) {
        var hex = wsjtxRowHighlightHex(modelData)
        if (hex.length === 0) return null
        var c = Qt.color(hex)
        return boostedDecodeTextColor(Qt.rgba(c.r, c.g, c.b, 0.85))
    }

    // Shannon-compatible coloring (priorità DecodeHighlightingModel)
    // 1.0.141: difesa contro "stazioni fantasma in rosso". A volte una row
    // riceveva isMyCall=true ma il messaggio shown non contiene davvero il
    // nostro callsign (causa ipotizzata: ghost decode marginal-SNR oppure
    // race tra model update). Verifica testuale prima di applicare il colore.
    function rowReallyIsMyCall(modelData) {
        if (!modelData || modelData.isMyCall !== true) return false
        var myCall = String((bridge && bridge.callsign) || "").toUpperCase().trim()
        if (myCall.length === 0) return true   // niente callsign configurato: trust the flag
        var msg = String(modelData.message || modelData.displayMessage || "").toUpperCase()
        // Word-boundary friendly: lo cerco con spazi attorno; il messaggio FT
        // ha solo spazi come separatori, niente punteggiatura.
        var padded = " " + msg.replace(/[<>;,]/g, " ").replace(/\//g, " ") + " "
        return padded.indexOf(" " + myCall + " ") >= 0
    }

    function getDxccColor(modelData) {
        var rowHex = wsjtxRowHighlightHex(modelData)
        if (rowHex.length > 0)
            return readableTextOnHighlight(rowHex)

        var customColor = customHighlightColor(modelData)
        if (customColor !== "") return boostedDecodeTextColor(customColor)

        var textHighlight = wsjtxTextHighlightColor(modelData)
        if (textHighlight.length > 0)
            return boostedDecodeTextColor(textHighlight)

        return boostedDecodeTextColor(effectiveDecodeColor(decodeTextColorProp(modelData)))
    }

    function decodeEntryBold(modelData) {
        decodeWindow.decodeColorRevision
        if (!modelData)
            return false
        if (customHighlightColor(modelData) !== "")
            return false
        return !!(bridge.decodeColorEnabled(decodeTextColorProp(modelData)) &&
                  bridge.decodeColorBold(decodeTextColorProp(modelData)))
    }

    function decodeColorCategoryEnabled(prop) {
        decodeWindow.decodeColorRevision
        return !!(bridge && bridge.decodeColorEnabled(prop))
    }

    function decodeTextColorProp(modelData) {
        if (!modelData)
            return "colorDecodeText"
        if (modelData.isTx === true && decodeColorCategoryEnabled("colorTxMessage")) return "colorTxMessage"
        if (rowReallyIsMyCall(modelData) && decodeColorCategoryEnabled("colorMyCall")) return "colorMyCall"
        if (highlight73 && isSignoffMessage(modelData.message) && decodeColorCategoryEnabled("color73")) return "color73"
        if ((modelData.isB4 === true || modelData.dxIsWorked === true) && decodeColorCategoryEnabled("colorB4")) return "colorB4"
        if (modelData.isCQ === true && decodeColorCategoryEnabled("colorCQ")) return "colorCQ"
        if (modelData.dxIsNewDxccBand === true && decodeColorCategoryEnabled("colorNewDxccBand")) return "colorNewDxccBand"
        if (modelData.dxIsNewDxcc === true && decodeColorCategoryEnabled("colorNewDxcc")) return "colorNewDxcc"
        if (modelData.dxIsNewContinentBand === true && decodeColorCategoryEnabled("colorNewContinentBand")) return "colorNewContinentBand"
        if (modelData.dxIsNewContinent === true && decodeColorCategoryEnabled("colorNewContinent")) return "colorNewContinent"
        if (modelData.dxIsNewCqZoneBand === true && decodeColorCategoryEnabled("colorNewCqZoneBand")) return "colorNewCqZoneBand"
        if (modelData.dxIsNewCqZone === true && decodeColorCategoryEnabled("colorNewCqZone")) return "colorNewCqZone"
        if (modelData.dxIsNewItuZoneBand === true && decodeColorCategoryEnabled("colorNewItuZoneBand")) return "colorNewItuZoneBand"
        if (modelData.dxIsNewItuZone === true && decodeColorCategoryEnabled("colorNewItuZone")) return "colorNewItuZone"
        if (modelData.dxIsNewGridBand === true && decodeColorCategoryEnabled("colorNewGridBand")) return "colorNewGridBand"
        if (modelData.dxIsNewGrid === true && decodeColorCategoryEnabled("colorNewGrid")) return "colorNewGrid"
        if (modelData.dxIsNewCallBand === true && decodeColorCategoryEnabled("colorNewCallBand")) return "colorNewCallBand"
        if (modelData.dxIsNewCall === true && decodeColorCategoryEnabled("colorNewCall")) return "colorNewCall"
        if ((modelData.dxIsMostWanted === true || modelData.dxIsNewCountry === true || modelData.dxIsNewBand === true)
                && decodeColorCategoryEnabled("colorDXEntity"))
            return "colorDXEntity"
        return "colorDecodeText"
    }

    function lotwMarkerColor() {
        return boostedDecodeTextColor(effectiveDecodeColor("colorLotwUser"))
    }

    // IU8LMC: Function to build tooltip text
    function getDxccTooltip(modelData) {
        if (!modelData.dxCountry) return ""
        var lines = []

        // Header: Callsign - Country (Continent)
        var header = (modelData.dxCallsign || "") + " - " + dxccDisplayText(modelData)
        if (modelData.dxContinent) header += " (" + modelData.dxContinent + ")"
        lines.push(header)

        // Prefix info
        if (modelData.dxPrefix) {
            lines.push("Prefix: " + modelData.dxPrefix)
        }

        // Status line
        if (modelData.dxIsMostWanted && !modelData.dxIsWorked) {
            lines.push("🔥 MOST WANTED - NEW!")
        } else if (modelData.dxIsNewCountry) {
            lines.push("★ NEW COUNTRY!")
        } else if (modelData.dxIsNewBand) {
            lines.push("✓ Worked - NEW BAND!")
        } else if (modelData.dxIsWorked) {
            lines.push("✓ Worked")
        }

        return lines.join("\n")
    }

    function usStateLabel(modelData) {
        if (!bridge || !bridge.showUsState || !modelData || !modelData.usState)
            return ""
        return String(modelData.usState).trim().toUpperCase()
    }

    function dxccDisplayText(modelData) {
        if (!modelData)
            return ""
        var country = modelData.dxCountry ? String(modelData.dxCountry) : ""
        var state = usStateLabel(modelData)
        if (country.length > 0 && state.length > 0)
            return country + " · " + state
        return country.length > 0 ? country : state
    }

	    function formatBearingDegrees(value) {
	        return value !== undefined && value >= 0 ? Math.round(value) + "°" : ""
	    }

	    function normalizedCallToken(token) {
	        return String(token || "").toUpperCase().replace(/[<>;,]/g, "").trim()
	    }

	    function isTelemetryHexToken(token) {
	        var text = normalizedCallToken(token)
	        return text.length >= 7
	            && text.length <= 18
	            && /[A-F]/.test(text)
	            && /[0-9]/.test(text)
	            && /^[0-9A-F]+$/.test(text)
	    }

	    function isTelemetryOnlyDecodeMessage(message) {
	        var parts = String(message || "").split(/\s+/).filter(function(part) {
	            return String(part || "").trim().length > 0
	        })
	        return parts.length === 1 && isTelemetryHexToken(parts[0])
	    }

	    function looksLikeCallsignTokenValue(token) {
	        var text = normalizedCallToken(token)
	        if (text.length === 0)
	            return false
	        if (text === "CQ" || text === "DX" || text === "QRZ" || text === "DE" || text === "TEST")
	            return false
	        if (isTelemetryHexToken(text))
	            return false
	        var hasLetter = /[A-Z]/.test(text)
	        var hasDigit = /[0-9]/.test(text)
	        return hasLetter && hasDigit && /^[A-Z0-9/-]+$/.test(text)
	    }

	    function callsignBase(call) {
	        var text = normalizedCallToken(call)
	        if (text.length === 0)
	            return ""
	        var parts = text.split("/")
	        var best = ""
	        for (var i = 0; i < parts.length; ++i) {
	            var part = normalizedCallToken(parts[i])
	            if (looksLikeCallsignTokenValue(part) && part.length > best.length)
	                best = part
	        }
	        return best.length > 0 ? best : text
	    }

	    function messageContainsCallBase(message, base) {
	        var wanted = String(base || "").trim().toUpperCase()
	        if (wanted.length === 0)
	            return false
	        var parts = String(message || "").split(/\s+/)
	        for (var i = 0; i < parts.length; ++i) {
	            if (callsignBase(parts[i]) === wanted)
	                return true
	        }
	        return false
	    }

	    function shouldDisplayDecodeEntry(item) {
	        if (!item)
	            return false
	        // 1.0.226 — ghost filter via bridge per chiusura fallback. Pre-1.0.226
	        // QML filtrava SOLO telemetry-only, lasciando passare ghost (myCall +
	        // partner sintatticamente non-valido o terzo token corrupted) quando
	        // bridge.rxDecodeModel era momentaneamente null (boot/Loader race).
	        if (bridge && bridge.entryLooksLikeGhost && bridge.entryLooksLikeGhost(item))
	            return false
	        if (!decodeWindow.hideTelemetryOnlyDecodes)
	            return true
	        return !isTelemetryOnlyDecodeMessage(item.displayMessage || item.message)
	    }

	    function filteredDecodeEntries(list) {
	        var filtered = []
	        if (!list)
	            return filtered
	        for (var i = 0; i < list.length; ++i) {
	            var item = list[i]
	            if (shouldDisplayDecodeEntry(item))
	                filtered.push(item)
	        }
	        // Decodium 3-style: ordine inverso (più recente in alto)
	        if (decodeNewestFirst)
	            filtered.reverse()
	        // Decodium 3-style: riga vuota tra periodi diversi
	        console.warn("[BAND-FILTER] decodeShowPeriodSeparator=" + decodeShowPeriodSeparator
	                    + " filtered.length=" + filtered.length)
	        if (decodeShowPeriodSeparator && filtered.length > 1) {
	            var withSep = []
	            var prevPeriod = ""
	            var prevTs = 0
	            var sepCount = 0
	            for (var j = 0; j < filtered.length; ++j) {
	                var it = filtered[j]
	                var t = String(it.time || it.utc || "")
	                var ts = Number(it.timestamp || 0)
	                var newPeriod = false
	                if (t.length > 0) {
	                    if (prevPeriod && t !== prevPeriod) newPeriod = true
	                } else {
	                    if (prevTs > 0 && ts > 0 && (ts - prevTs) > 1500) newPeriod = true
	                }
	                if (newPeriod) {
	                    withSep.push({ isSeparator: true, time: t })
	                    sepCount++
	                }
	                if (t.length > 0) prevPeriod = t
	                if (ts > 0) prevTs = ts
	                withSep.push(it)
	            }
	            console.warn("[BAND-FILTER] inserted " + sepCount + " separators, total rows=" + withSep.length)
	            return withSep
	        }
	        return filtered
	    }

	    // Shannon: RX Frequency window ±200Hz (sbFtol default) + messaggi diretti a noi
	    property int rxBandwidth: 200  // Shannon sbFtol default 200Hz

    // Shannon isAtRxFrequency: dentro finestra ±200Hz OR messaggio per noi
    function isAtRxFrequency(freq, md) {
        var f = parseInt(freq)
        var inWindow = Math.abs(f - appEngine.rxFrequency) <= rxBandwidth
	        var relevant = md && (md.isMyCall || md.isTx)
	        return inWindow || relevant
	    }

	    function currentQsoPartnerBase() {
	        return callsignBase(appEngine.dxCall || "")
	    }

	    function rxEntryBelongsToCurrentQso(item) {
	        if (!item)
	            return false

	        var activeBase = currentQsoPartnerBase()
	        if (item.isTx === true)
	            return true

	        var myBase = callsignBase(appEngine.callsign || "")
	        var message = item.message || ""
	        var myMatch = item.isMyCall === true
	            || messageContainsCallBase(message, myBase)
	        if (myMatch)
	            return true

	        if (activeBase.length === 0)
	            return isAtRxFrequency(item.freq || "0", item)

	        var activeMatch = messageContainsCallBase(message, activeBase)
	            || callsignBase(item.fromCall || "") === activeBase
	            || callsignBase(item.dxCallsign || "") === activeBase
	        return activeMatch
	    }

	    function rxSortSeconds(item) {
	        var digits = String((item && (item.utc || item.time)) || "").replace(/[^0-9]/g, "")
	        if (digits.length >= 6)
	            digits = digits.substring(0, 6)
	        else if (digits.length === 4)
	            digits = digits + "00"
	        else
	            return -1

	        var hh = parseInt(digits.substring(0, 2))
	        var mm = parseInt(digits.substring(2, 4))
	        var ss = parseInt(digits.substring(4, 6))
	        if (isNaN(hh) || isNaN(mm) || isNaN(ss)
	                || hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59)
	            return -1
	        return hh * 3600 + mm * 60 + ss
	    }

	    function rxSortKey(item) {
	        var seconds = rxSortSeconds(item)
	        if (seconds < 0)
	            return 9007199254740991
	        var now = new Date()
	        var nowSeconds = now.getUTCHours() * 3600 + now.getUTCMinutes() * 60 + now.getUTCSeconds()
	        var delta = seconds - nowSeconds
	        if (delta > 60)
	            seconds -= 86400
	        else if (delta < -86340)
	            seconds += 86400
	        return seconds * 1000
	    }

	    function sortedRxDecodes(items) {
	        var indexed = []
	        for (var i = 0; i < items.length; ++i)
	            indexed.push({ item: items[i], order: i, key: rxSortKey(items[i]) })
	        indexed.sort(function(a, b) {
	            if (a.key !== b.key)
	                return a.key - b.key
	            return a.order - b.order
	        })
	        var out = []
	        for (var j = 0; j < indexed.length; ++j)
	            out.push(indexed[j].item)
	        return out
	    }

	    function rxEntryKey(item) {
	        var key = [
	            item.utc || item.time || "",
	            item.freq || "",
	            item.dt || "",
	            item.snr || "",
	            item.message || "",
	            item.isTx === true ? "tx" : "rx"
	        ].join("|")
	        if (String(item.utc || item.time || "").trim().length === 0 && Number(item.timestamp || 0) > 0)
	            key += "|ts=" + String(item.timestamp)
	        return key
	    }

	    function clearSignalRxDecodes() {
	        if (decodeWindow.hasNativeRxDecodeModel()) {
	            decodeWindow.clearedRxDecodeKeys = ({})
	        } else {
	            var hidden = {}
	            for (var i = 0; i < decodeWindow.rxDecodeModel.length; ++i) {
	                var item = decodeWindow.rxDecodeModel[i]
	                if (!item || item.isSeparator === true)
	                    continue
	                hidden[decodeWindow.rxEntryKey(item)] = true
	            }
	            decodeWindow.clearedRxDecodeKeys = hidden
	            decodeWindow.rxDecodeModel = []
	        }
	        decodeWindow.rxDecodeListVersion++
	        appEngine.clearRxDecodes()
	        if (rxFrequencyList)
	            rxFrequencyList.forceTailFollow()
	    }

	    function currentRxDecodes() {
	        var merged = []
	        var seen = {}
	        function appendIfNeeded(item, allowTx) {
	            if (!item)
	                return
	            if (!allowTx && item.isTx === true)
	                return
	            if (!decodeWindow.showTxMessagesInRx && item.isTx)
	                return
	            if (!shouldDisplayDecodeEntry(item))
	                return
	            if (!rxEntryBelongsToCurrentQso(item))
	                return
	            var key = decodeWindow.rxEntryKey(item)
	            if (decodeWindow.clearedRxDecodeKeys[key])
	                return
	            if (seen[key])
	                return
	            seen[key] = true
	            merged.push(item)
	        }
	        if (appEngine.rxDecodeList) {
            for (var j = 0; j < appEngine.rxDecodeList.length; j++) {
                appendIfNeeded(appEngine.rxDecodeList[j], true)
	            }
        }
        var sorted = sortedRxDecodes(merged)
        // Decodium 3-style: ordine inverso (più recente in alto)
        if (decodeNewestFirst)
            sorted.reverse()
        // Decodium 3-style: riga vuota tra periodi diversi
        if (decodeShowPeriodSeparator && sorted.length > 1) {
            var withSep = []
            var prevPeriod = ""
            var prevTs = 0
            for (var n = 0; n < sorted.length; ++n) {
                var it = sorted[n]
                var t = String(it.time || it.utc || "")
                var ts = Number(it.timestamp || 0)
                var newPeriod = false
                if (t.length > 0) {
                    if (prevPeriod && t !== prevPeriod) newPeriod = true
                } else {
                    if (prevTs > 0 && ts > 0 && (ts - prevTs) > 1500) newPeriod = true
                }
                if (newPeriod) withSep.push({ isSeparator: true, time: t })
                if (t.length > 0) prevPeriod = t
                if (ts > 0) prevTs = ts
                withSep.push(it)
            }
            return withSep
        }
        return sorted
    }

    function formatUtcForDisplay(timeStr) {
        var digits = String(timeStr || "").replace(/[^0-9]/g, "")
        if (digits.length >= 6)
            return digits.substring(0, 2) + ":" + digits.substring(2, 4) + ":" + digits.substring(4, 6)
        if (digits.length === 4)
            return digits.substring(0, 2) + ":" + digits.substring(2, 4)
        return timeStr || ""
    }

    // IU8LMC: Custom tooltip properties
    property string tooltipText: ""
    property bool tooltipVisible: false
    property real tooltipX: 0
    property real tooltipY: 0
    property string debugInfo: "Hover a decode to see country"  // Debug display

    // IU8LMC: Custom tooltip component
    Rectangle {
        id: customTooltip
        visible: tooltipVisible && tooltipText !== ""
        x: Math.min(tooltipX + 15, decodeWindow.width - width - 10)
        y: Math.max(tooltipY - height - 5, 10)
        z: 1000
        width: tooltipLabel.width + 16
        height: tooltipLabel.height + 10
        color: (bridge && bridge.themeManager) ? Qt.rgba(bridge.themeManager.bgDeep.r, bridge.themeManager.bgDeep.g, bridge.themeManager.bgDeep.b, 0.94) : "#e0202020"
        border.color: (bridge && bridge.themeManager) ? bridge.themeManager.glassBorder : "#606060"
        border.width: 1
        radius: 4

        Text {
            id: tooltipLabel
            anchors.centerIn: parent
            text: tooltipText
            color: textPrimary
            font.pixelSize: 11
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ── Pannelli Band Activity + RX Frequency — SEMPRE esattamente 50/50 ─
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: splitContainer

            // Divisore 1px sempre al centro esatto
            Rectangle {
                x: parent.width / 2
                y: 0; width: 1; height: parent.height
                color: Qt.rgba(1,1,1,0.12)
                z: 10
            }

            // ========== LEFT PANEL: Band Activity — 50% esatto ============
            Rectangle {
                x: 0; y: 0
                width: parent.width / 2 - 1
                height: parent.height
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    // Band Activity Header
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                        radius: 6

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 12

                            Text {
                                text: qsTr("Full Spectrum")
                                font.pixelSize: 13
                                font.bold: true
                                color: secondaryCyan
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: decodeWindow.bandActivityCount() + " " + qsTr("decodes")
                                font.pixelSize: 11
                                color: textSecondary
                            }

                            Button {
                                text: "Clear"
                                flat: true
                                implicitHeight: 24
                                implicitWidth: 50
                                onClicked: appEngine.clearDecodes()

                                contentItem: Text {
                                    text: parent.text
                                    color: textSecondary
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(255,255,255,0.1) : "transparent"
                                    radius: 4
                                }
                            }
                        }
                    }

                    // Band Activity Column headers
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
                        radius: 3

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 0

                            Text {
                                text: "UTC"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                Layout.preferredWidth: decodeWindow.bandUtcWidth
                            }
                            Text {
                                text: "dB"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.bandDbWidth
                            }
                            Item { Layout.preferredWidth: decodeWindow.bandDbDtGapWidth }
                            Text {
                                text: "DT"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.bandDtWidth
                            }
                            Item { Layout.preferredWidth: decodeWindow.bandDtFreqGapWidth }
                            Text {
                                text: qsTr("Freq")
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.bandFreqWidth
                            }
                            Item {
                                visible: decodeWindow.bandShowWsprDrift
                                Layout.preferredWidth: decodeWindow.bandDriftWidth
                                Layout.fillHeight: true
                                Text {
                                    anchors.fill: parent
                                    text: qsTr("Drift")
                                    font.family: decodiumMonoFontFamily
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: secondaryCyan
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Item { Layout.preferredWidth: decodeWindow.bandGapWidth }
                            Text {
                                text: "Message"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                Layout.fillWidth: true
                            }
                            Item {
                                visible: decodeWindow.showDxccInfo
                                Layout.preferredWidth: decodeWindow.bandDxccWidth
                                Layout.fillHeight: true
                                Text {
                                    anchors.fill: parent
                                    text: "DXCC"
                                    font.family: decodiumMonoFontFamily
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: secondaryCyan
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Item {
                                visible: decodeWindow.showDxccInfo
                                Layout.preferredWidth: decodeWindow.bandAzWidth
                                Layout.fillHeight: true
                                Text {
                                    anchors.fill: parent
                                    text: qsTr("Az")
                                    font.family: decodiumMonoFontFamily
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: secondaryCyan
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    // Band Activity List
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                        radius: 6
                        clip: true

                        ListView {
                            id: bandActivityList
                            anchors.fill: parent
                            anchors.margins: 4
	                            clip: true
	                            // 1.0.143 fase 2: model nativo C++ (QAbstractListModel)
	                            // invece di array JS filtrato. Update incrementali
	                            // (beginInsertRows/dataChanged) eliminano scatti.
	                            model: (bridge && bridge.bandActivityModel) ? bridge.bandActivityModel : decodeWindow.bandActivityModel
	                            spacing: 1
	                            // 1.0.140: ridotto da 3000 (pre-rendering ~115 row con
	                            // 7+ binding ternari ciascuna = costo CPU/GPU significativo
	                            // ad ogni model change). 600 = ~23 row buffer, smooth ma
	                            // 5× meno overhead. Su PC vecchi user-reported -10% CPU.
	                            cacheBuffer: 360
	                            reuseItems: true
	                            property bool followTail: true
                            property bool tailFollowPending: false
	                            property bool tailFollowQueued: false
	                            function isNearTail() {
	                                return contentHeight <= height + 2
	                                      || contentY >= Math.max(0, contentHeight - height - 48)
	                            }
                            function updateFollowTail() {
                                if (tailFollowPending)
                                    return
                                followTail = isNearTail()
                            }
                            function tailContentY() {
                                return Math.max(0, contentHeight - height)
                            }
                            function finishTailFollow() {
                                tailFollowPending = false
                                followTail = isNearTail()
                            }
                            function shouldSnapTailFollow() {
                                return appEngine && appEngine.transmitting
                            }
                            function followTailAfterModelUpdate() {
                                if (followTail || isNearTail())
                                    forceTailFollow()
                            }
                            function forceTailFollow() {
    followTail = true
    tailFollowPending = true
    if (tailFollowQueued)
        return
    tailFollowQueued = true
    Qt.callLater(function() {
        tailFollowQueued = false
        if (!bandActivityList)
            return
        var targetY = bandActivityList.tailContentY()
        bandActivityTailAnimation.stop()
        bandActivityList.tailFollowPending = true
        bandActivityList.contentY = targetY
        bandActivityList.finishTailFollow()
    })
}
NumberAnimation {
    id: bandActivityTailAnimation
                                target: bandActivityList
                                property: "contentY"
	                                duration: 160
	                                easing.type: Easing.OutCubic
	                                onStopped: bandActivityList.finishTailFollow()
	                            }
	                            Timer {
	                                id: bandActivityTailSettleTimer
	                                interval: 32
	                                repeat: false
	                                onTriggered: {
	                                    if (bandActivityList.followTail || bandActivityList.tailFollowPending)
	                                        bandActivityList.forceTailFollow()
	                                }
	                            }
                            Component.onCompleted: Qt.callLater(function() {
                                positionViewAtEnd()
                                updateFollowTail()
                            })
                            onContentYChanged: updateFollowTail()
	                            onContentHeightChanged: {
	                                if (followTail || tailFollowPending) {
	                                    if (shouldSnapTailFollow()) {
	                                        bandActivityTailAnimation.stop()
	                                        contentY = tailContentY()
	                                        finishTailFollow()
	                                    } else {
	                                        bandActivityTailSettleTimer.restart()
	                                    }
	                                }
	                            }
	                            onHeightChanged: {
	                                if (followTail || tailFollowPending)
	                                    forceTailFollow()
	                                else
	                                    updateFollowTail()
	                            }
	                            onDraggingChanged: {
	                                if (dragging) {
	                                    followTail = false
	                                    tailFollowPending = false
	                                    bandActivityTailAnimation.stop()
	                                }
	                            }
                            onCountChanged: {
                                if (decodeWindow.hasNativeBandActivityModel()) return
                                if (!followTail) return
                                forceTailFollow()
                            }
	                            // 1.0.125: rimosse animazioni add/addDisplaced/moveDisplaced/
	                            // removeDisplaced. Quando arrivano 20+ righe in un colpo
	                            // (slot FT8 finale o burst FT2 async), 20 fade-in opacity +
	                            // 20 y-animation 270ms simultanei creavano flicker e jitter.
	                            // Le righe ora appaiono istantanee nella loro posizione finale;
	                            // solo il tail-follow (sopra) anima lo scroll, in 160ms.

                            // 1.0.179 — Smooth Decode Flow opt-in. Lo scheduler C++ spalma
                            // il rilascio a chunk 1-2 row con interval 80-200ms, quindi le
                            // transitions add/addDisplaced sono SAFE (no piu' 20 fade
                            // simultanei). Attive solo se bridge.smoothDecodeFlow === true.
                            // NO move/remove transitions: causerebbero jitter su clear/sort.
                            add: Transition {
                                enabled: bridge ? bridge.smoothDecodeFlow : false
                                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0
                                                  duration: 100; easing.type: Easing.OutQuad }
                            }
                            addDisplaced: Transition {
                                enabled: bridge ? bridge.smoothDecodeFlow : false
                                NumberAnimation { properties: "y"; duration: 100
                                                  easing.type: Easing.OutQuad }
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                interactive: true
                                width: 8
                            }

                            delegate: Rectangle {
                                // 1.0.150: defensive — Qt6 con QAbstractListModel multi-role espone i ruoli
// via attached `model.X` (garantito); `modelData` puo' essere il primo ruolo
// (string time) invece dell'oggetto intero. Controlla entrambi.
// 1.0.153: per QAbstractListModel multi-role, i ruoli QML sono esposti
// come context property scoped (es. `isSeparator`, `time`) — accesso
// diretto senza prefisso model./modelData.
readonly property bool isPeriodSeparator: {
    // Provo tutti i 3 pattern Qt6 di accesso al ruolo:
    if (typeof isSeparator !== "undefined" && isSeparator) return true
    if (model && model.isSeparator) return true
    if (modelData && typeof modelData === "object" && modelData.isSeparator) return true
    return false
}
Component.onCompleted: {
    if (!bridge || !bridge.qmlDebugLog) return
    var scopedSep = (typeof isSeparator !== "undefined") ? isSeparator : "ctx-undef"
    var modelSep = (typeof model !== "undefined" && model !== null) ? model.isSeparator : "model-undef"
    var mdType = typeof modelData
    var mdSep = (typeof modelData === "object" && modelData) ? modelData.isSeparator : "md-not-obj"
    if (index < 3 || isPeriodSeparator || scopedSep === true || modelSep === true) {
        bridge.qmlDebugLog("SEPDBG idx=" + index + " isPeriodSeparator=" + isPeriodSeparator
            + " scoped.isSep=" + scopedSep + " model.isSep=" + modelSep
            + " mdType=" + mdType + " md.isSep=" + mdSep)
    }
}
                                width: bandActivityList.width
                                height: isPeriodSeparator ? 18 : 26
                                // Cascata WSJT-X prioritaria; fallback ai vecchi tinte/zebra.
                                color: {
                                    if (isPeriodSeparator) return Qt.rgba(1, 0.3, 0.3, 0.35)  // ROSSO chiaro evidente
                                    var ubg = decodeWindow.decodeUserBgFill(modelData)
                                    if (ubg) return ubg
                                    var wsx = decodeWindow.wsjtxBgColor(modelData)
                                    if (wsx) return wsx
                                    // 1.0.134: match DxCall — oro (Band Activity). Solo flag
                                    // pre-calcolato C++, niente funzione QML (no regressione).
	                                    if (modelData.matchesDxCall === true)
	                                        return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(1, 0.84, 0, 0.30))
                                    // 1.0.141: difesa contro stazioni "fantasma" rosse
	                                    if (decodeWindow.rowReallyIsMyCall(modelData))
	                                        return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(244/255, 67/255, 54/255, 0.25))
	                                    if (modelData.isCQ && bridge.decodeColorEnabled("colorCQ"))     return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12))
	                                    if (isAtRxFrequency(modelData.freq, modelData))
	                                        return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2))
	                                    return index % 2 === 0
	                                           ? decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02))
	                                           : decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05))
                                }
                                border.color: {
                                    if (isPeriodSeparator) return "transparent"
                                    var wsx = decodeWindow.wsjtxBorderColor(modelData)
                                    return wsx ? wsx : "transparent"
                                }
                                border.width: (!isPeriodSeparator && decodeWindow.wsjtxBgColor(modelData)) ? 1 : 0
                                radius: 2

                                // Linea visibile centrata nel separatore
                                Rectangle {
                                    visible: parent.isPeriodSeparator
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    height: 3
                                    color: "#ff3030"
                                }
                                Text {
                                    visible: parent.isPeriodSeparator
                                    anchors.centerIn: parent
                                    text: qsTr("── PERIOD ──")
                                    color: "#ff8080"
                                    font.pixelSize: 10
                                    font.bold: true
                                }

                                MouseArea {
                                    id: bandDelegateMouseArea
                                    enabled: !parent.isPeriodSeparator
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: (mouse) => {
                                        if (parent.isPeriodSeparator) return
                                        if (mouse.button === Qt.RightButton) {
                                            openQrzLookup(modelData)   // IU8LMC: click destro -> QRZ.com
                                            return
                                        }
                                        appEngine.selectDecode(index)
                                        // Set RX frequency to this decode's frequency
                                        appEngine.rxFrequency = parseInt(modelData.freq)
                                    }
                                    onDoubleClicked: {
                                        // Set TX and RX frequency to decode frequency
                                        if (!appEngine.holdTxFreq)
                                            appEngine.txFrequency = parseInt(modelData.freq)
                                        appEngine.rxFrequency = parseInt(modelData.freq)
                                        console.log("Double-click: calling " + extractCall(modelData.message) + " at " + modelData.freq + " Hz")
                                        // Process decode via HvTxW::DecListTextAll (Decodium path)
                                        appEngine.processDecodeDoubleClick(
                                            modelData.message,
                                            modelData.time,
                                            modelData.db,
                                            parseInt(modelData.freq)
                                        )
                                        // Set DX info for local state
                                        appEngine.setDxFromDecode(
                                            extractCall(modelData.message),
                                            extractGrid(modelData.message),
                                            modelData.db
                                        )
                                    }

                                    // IU8LMC: Custom tooltip on hover
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            // Debug: show in UI
                                            debugInfo = "Call: " + (modelData.dxCallsign || "none") + " | Country: " + (modelData.dxCountry || "none")
                                            if (modelData.dxCountry && modelData.dxCountry !== "") {
                                                tooltipText = getDxccTooltip(modelData)
                                                var pos = bandDelegateMouseArea.mapToGlobal(mouseX, mouseY)
                                                tooltipX = pos.x - decodeWindow.x + 10
                                                tooltipY = pos.y - decodeWindow.y - 30
                                                tooltipVisible = true
                                            }
                                        } else {
                                            tooltipVisible = false
                                        }
                                    }
                                    onPositionChanged: {
                                        if (containsMouse && tooltipVisible) {
                                            var pos = bandDelegateMouseArea.mapToGlobal(mouseX, mouseY)
                                            tooltipX = pos.x - decodeWindow.x + 10
                                            tooltipY = pos.y - decodeWindow.y - 30
                                        }
                                    }
                                }

                                RowLayout {
                                    visible: !parent.isPeriodSeparator
                                    anchors.fill: parent
                                    anchors.leftMargin: 4
                                    anchors.rightMargin: 4
                                    spacing: 0

                                    Text {
                                        text: modelData.formattedTime || formatUtcForDisplay(modelData.time)
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                        Layout.preferredWidth: decodeWindow.bandUtcWidth
                                    }

                                    Text {
                                        text: modelData.db
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(parseInt(modelData.db) > -5 ? accentGreen :
	                                               parseInt(modelData.db) > -15 ? secondaryCyan :
	                                               parseInt(modelData.db) > -20 ? textSecondary : "#888")
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.bandDbWidth
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.bandDbDtGapWidth }

                                    Text {
                                        text: modelData.dt
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.bandDtWidth
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.bandDtFreqGapWidth }

                                    Text {
                                        text: modelData.freq
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(modelData.isTx ? "#f1c40f" : secondaryCyan)
                                        font.bold: modelData.isTx === true
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.bandFreqWidth
                                    }
                                    Item {
                                        visible: decodeWindow.bandShowWsprDrift
                                        Layout.preferredWidth: decodeWindow.bandDriftWidth
                                        Layout.fillHeight: true
                                        Text {
                                            anchors.fill: parent
                                            text: modelData.mode === "WSPR" ? (modelData.drift || "0") : ""
                                            font.family: decodiumMonoFontFamily
                                            font.pixelSize: 11
                                            color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                            horizontalAlignment: Text.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.bandGapWidth }

                                    // Messaggio con coloring Shannon-compatible
                                    Rectangle {
                                        visible: modelData.isLotw === true
                                        Layout.preferredWidth: 6
                                        Layout.preferredHeight: 6
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 3
                                        color: decodeWindow.lotwMarkerColor()
                                        border.color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                        border.width: 1
                                    }
                                    Text {
                                        id: bandMsgText
                                        text: modelData.displayMessage || modelData.message
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
                                        // 1.0.144: precomputed in C++ (enrichDecodeEntry)
                                        font.bold: decodeWindow.decodeEntryBold(modelData)
                                        // Shannon strikethrough per B4
                                        font.strikeout: modelData.isB4 && bridge.b4Strikethrough
                                        color: getDxccColor(modelData)
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: decodeWindow.bandMessageMinWidth
                                        elide: decodeWindow.messageElideMode(modelData.displayMessage || modelData.message)
                                    }

                                    Item {
                                        visible: decodeWindow.showDxccInfo
                                        Layout.preferredWidth: decodeWindow.bandDxccWidth
                                        Layout.fillHeight: true
                                        Text {
                                            anchors.fill: parent
                                            anchors.rightMargin: modelData.isLotw === true ? 11 : 0
                                            text: decodeWindow.dxccDisplayText(modelData)
                                            font.family: decodiumMonoFontFamily
                                            font.pixelSize: 11
	                                            color: decodeWindow.boostedDecodeTextColor((modelData.dxCountry || modelData.usState) && decodeWindow.decodeColorCategoryEnabled("colorDXEntity") ? decodeWindow.effectiveDecodeColor("colorDXEntity") : textSecondary)
                                            horizontalAlignment: Text.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideNone
                                            fontSizeMode: Text.HorizontalFit
                                            minimumPixelSize: 8
                                            maximumLineCount: 1
                                        }
                                        Rectangle {
                                            visible: modelData.isLotw === true
                                            width: 6
                                            height: 6
                                            radius: 3
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: decodeWindow.lotwMarkerColor()
                                            border.color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                            border.width: 1
                                        }
                                    }

                                    Item {
                                        visible: decodeWindow.showDxccInfo
                                        Layout.preferredWidth: decodeWindow.bandAzWidth
                                        Layout.fillHeight: true
                                        Text {
                                            anchors.fill: parent
                                            text: formatBearingDegrees(modelData.dxBearing)
                                            font.family: decodiumMonoFontFamily
                                            font.pixelSize: 11
	                                            color: decodeWindow.boostedDecodeTextColor(secondaryCyan)
                                            horizontalAlignment: Text.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }

                        // Empty state
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("No decoded messages\nClick Monitor to start")
                            font.pixelSize: 12
                            color: textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            visible: decodeWindow.bandActivityCount() === 0
                        }
                    }
                }
            }

            // ========== RIGHT PANEL: RX Frequency — 50% esatto ============
            Rectangle {
                x: parent.width / 2; y: 0
                width: parent.width / 2
                height: parent.height
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    // RX Frequency Header
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                        radius: 6

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8

                            Text {
                                text: qsTr("Signal RX")
                                font.pixelSize: decodeWindow.compactRxHeader ? 12 : 13
                                font.bold: true
                                color: primaryBlue
                            }

                            Item { Layout.fillWidth: true }

                            // RX Frequency count
                            Text {
                                text: {
                                    return decodeWindow.signalRxCount() + " msgs"
                                }
                                font.pixelSize: 11
                                color: textSecondary
                                visible: !decodeWindow.compactRxHeader
                            }

                            Button {
                                text: "Clear"
                                flat: true
                                implicitHeight: 24
                                implicitWidth: 50
                                onClicked: decodeWindow.clearSignalRxDecodes()

                                contentItem: Text {
                                    text: parent.text
                                    color: textSecondary
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(255,255,255,0.1) : "transparent"
                                    radius: 4
                                }
                            }
                        }
                    }

                    // RX Frequency Column headers
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
                        radius: 3

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 0

                            Text {
                                text: "UTC"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                Layout.preferredWidth: decodeWindow.rxUtcWidth
                            }
                            Text {
                                text: "dB"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.rxDbWidth
                            }
                            Item { Layout.preferredWidth: decodeWindow.rxDbDtGapWidth }
                            Text {
                                text: "DT"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.rxDtWidth
                            }
                            Item { Layout.preferredWidth: decodeWindow.rxGapWidth }
                            Text {
                                text: "Message"
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // RX Frequency List (filtered)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                        border.color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                        border.width: 1
                        radius: 6
                        clip: true

                        ListView {
                            id: rxFrequencyList
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
	                            spacing: 1
	                            interactive: true
	                            // 1.0.140: ridotto da 3000 — vedi commento bandActivityList
	                            cacheBuffer: 360
	                            reuseItems: true
	                            // Pattern identico a bandActivityList: model property-backed
                            // (decodeWindow.rxDecodeModel) + followTail/isNearTail/updateFollowTail
                            // basato su contentY/contentHeight. Così Signal RX si comporta
                            // esattamente come Full Spectrum.
                            // 1.0.143 fase 2: vedi commento bandActivityList sopra.
                            model: (bridge && bridge.rxDecodeModel) ? bridge.rxDecodeModel : decodeWindow.rxDecodeModel
                            property bool followTail: true
                            property bool tailFollowPending: false
	                            property bool tailFollowQueued: false
	                            function isNearTail() {
	                                return contentHeight <= height + 2
	                                      || contentY >= Math.max(0, contentHeight - height - 48)
	                            }
                            function updateFollowTail() {
                                if (tailFollowPending)
                                    return
                                followTail = isNearTail()
                            }
                            function tailContentY() {
                                return Math.max(0, contentHeight - height)
                            }
                            function finishTailFollow() {
                                tailFollowPending = false
                                followTail = isNearTail()
                            }
                            function shouldSnapTailFollow() {
                                return appEngine && appEngine.transmitting
                            }
                            function followTailAfterModelUpdate() {
                                if (followTail || isNearTail())
                                    forceTailFollow()
                            }
                            function forceTailFollow() {
    followTail = true
    tailFollowPending = true
    if (tailFollowQueued)
        return
    tailFollowQueued = true
    Qt.callLater(function() {
        tailFollowQueued = false
        if (!rxFrequencyList)
            return
        var targetY = rxFrequencyList.tailContentY()
        rxFrequencyTailAnimation.stop()
        rxFrequencyList.tailFollowPending = true
        rxFrequencyList.contentY = targetY
        rxFrequencyList.finishTailFollow()
    })
}
NumberAnimation {
    id: rxFrequencyTailAnimation
                                target: rxFrequencyList
                                property: "contentY"
	                                duration: 160
	                                easing.type: Easing.OutCubic
	                                onStopped: rxFrequencyList.finishTailFollow()
	                            }
	                            Timer {
	                                id: rxFrequencyTailSettleTimer
	                                interval: 32
	                                repeat: false
	                                onTriggered: {
	                                    if (rxFrequencyList.followTail || rxFrequencyList.tailFollowPending)
	                                        rxFrequencyList.forceTailFollow()
	                                }
	                            }
                            Component.onCompleted: Qt.callLater(function() {
                                positionViewAtEnd()
                                updateFollowTail()
                            })
                            onContentYChanged: updateFollowTail()
	                            onContentHeightChanged: {
	                                if (followTail || tailFollowPending) {
	                                    if (shouldSnapTailFollow()) {
	                                        rxFrequencyTailAnimation.stop()
	                                        contentY = tailContentY()
	                                        finishTailFollow()
	                                    } else {
	                                        rxFrequencyTailSettleTimer.restart()
	                                    }
	                                }
	                            }
	                            onHeightChanged: {
	                                if (followTail || tailFollowPending)
	                                    forceTailFollow()
	                                else
	                                    updateFollowTail()
	                            }
	                            onDraggingChanged: {
	                                if (dragging) {
	                                    followTail = false
	                                    tailFollowPending = false
	                                    rxFrequencyTailAnimation.stop()
	                                }
	                            }
                            onCountChanged: {
                                if (decodeWindow.hasNativeRxDecodeModel()) return
                                if (!followTail) return
                                forceTailFollow()
                            }
	                            // 1.0.125: rimosse animazioni add/addDisplaced/moveDisplaced/
	                            // removeDisplaced (stesso motivo di bandActivityList).

                            // 1.0.179 — Smooth Decode Flow opt-in. Lo scheduler C++ spalma
                            // il rilascio a chunk 1-2 row con interval 80-200ms, quindi le
                            // transitions add/addDisplaced sono SAFE (no piu' 20 fade
                            // simultanei). Attive solo se bridge.smoothDecodeFlow === true.
                            // NO move/remove transitions: causerebbero jitter su clear/sort.
                            add: Transition {
                                enabled: bridge ? bridge.smoothDecodeFlow : false
                                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0
                                                  duration: 100; easing.type: Easing.OutQuad }
                            }
                            addDisplaced: Transition {
                                enabled: bridge ? bridge.smoothDecodeFlow : false
                                NumberAnimation { properties: "y"; duration: 100
                                                  easing.type: Easing.OutQuad }
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                interactive: true
                                width: 8
                            }

                            delegate: Rectangle {
                                // 1.0.150: defensive — Qt6 con QAbstractListModel multi-role espone i ruoli
// via attached `model.X` (garantito); `modelData` puo' essere il primo ruolo
// (string time) invece dell'oggetto intero. Controlla entrambi.
// 1.0.153: per QAbstractListModel multi-role, i ruoli QML sono esposti
// come context property scoped (es. `isSeparator`, `time`) — accesso
// diretto senza prefisso model./modelData.
readonly property bool isPeriodSeparator: {
    // Provo tutti i 3 pattern Qt6 di accesso al ruolo:
    if (typeof isSeparator !== "undefined" && isSeparator) return true
    if (model && model.isSeparator) return true
    if (modelData && typeof modelData === "object" && modelData.isSeparator) return true
    return false
}
Component.onCompleted: {
    if (!bridge || !bridge.qmlDebugLog) return
    var scopedSep = (typeof isSeparator !== "undefined") ? isSeparator : "ctx-undef"
    var modelSep = (typeof model !== "undefined" && model !== null) ? model.isSeparator : "model-undef"
    var mdType = typeof modelData
    var mdSep = (typeof modelData === "object" && modelData) ? modelData.isSeparator : "md-not-obj"
    if (index < 3 || isPeriodSeparator || scopedSep === true || modelSep === true) {
        bridge.qmlDebugLog("SEPDBG idx=" + index + " isPeriodSeparator=" + isPeriodSeparator
            + " scoped.isSep=" + scopedSep + " model.isSep=" + modelSep
            + " mdType=" + mdType + " md.isSep=" + mdSep)
    }
}
                                width: rxFrequencyList.width - 12
                                height: isPeriodSeparator ? 18 : 24
                                color: {
                                    if (isPeriodSeparator) return Qt.rgba(1, 0.3, 0.3, 0.35)
                                    var wsx = decodeWindow.wsjtxBgColor(modelData)
                                    if (wsx) return wsx
                                    // 1.0.134: match DxCall — oro (Signal RX). Solo flag
                                    // pre-calcolato C++, niente funzione QML.
	                                    if (modelData.matchesDxCall === true)
	                                        return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(1, 0.84, 0, 0.30))
                                    // 1.0.141: difesa contro stazioni "fantasma" rosse (Signal RX)
	                                    if (decodeWindow.rowReallyIsMyCall(modelData))
	                                        return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(244/255, 67/255, 54/255, 0.3))
	                                    if (modelData.isCQ && bridge.decodeColorEnabled("colorCQ"))     return decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15))
	                                    return index % 2 === 0
	                                           ? decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.05))
	                                           : decodeWindow.boostedDecodeBackgroundColor(Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.1))
                                }
                                border.color: {
                                    if (isPeriodSeparator) return "transparent"
                                    var wsx = decodeWindow.wsjtxBorderColor(modelData)
                                    return wsx ? wsx : "transparent"
                                }
                                border.width: (!isPeriodSeparator && decodeWindow.wsjtxBgColor(modelData)) ? 1 : 0
                                radius: 2

                                Rectangle {
                                    visible: parent.isPeriodSeparator
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    height: 3
                                    color: "#ff3030"
                                }
                                Text {
                                    visible: parent.isPeriodSeparator
                                    anchors.centerIn: parent
                                    text: qsTr("── PERIOD ──")
                                    color: "#ff8080"
                                    font.pixelSize: 10
                                    font.bold: true
                                }

                                MouseArea {
                                    id: rxDelegateMouseArea
                                    enabled: !parent.isPeriodSeparator
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: (mouse) => {
                                        if (parent.isPeriodSeparator) return
                                        if (mouse.button === Qt.RightButton)
                                            openQrzLookup(modelData)   // IU8LMC: click destro -> QRZ.com
                                    }
                                    onDoubleClicked: {
                                        if (parent.isPeriodSeparator) return
                                        // Set TX and RX frequency to decode frequency
                                        if (!appEngine.holdTxFreq)
                                            appEngine.txFrequency = parseInt(modelData.freq)
                                        appEngine.rxFrequency = parseInt(modelData.freq)
                                        console.log("Double-click RX: calling " + extractCall(modelData.message) + " at " + modelData.freq + " Hz")
                                        // Process decode via HvTxW::DecListTextAll (Decodium path)
                                        appEngine.processDecodeDoubleClick(
                                            modelData.message,
                                            modelData.time,
                                            modelData.db,
                                            parseInt(modelData.freq)
                                        )
                                        appEngine.setDxFromDecode(
                                            extractCall(modelData.message),
                                            extractGrid(modelData.message),
                                            modelData.db
                                        )
                                    }

                                    // IU8LMC: Custom tooltip on hover
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            console.log("HOVER RX - dxCountry:", modelData.dxCountry, "call:", modelData.dxCallsign)
                                            if (modelData.dxCountry && modelData.dxCountry !== "") {
                                                tooltipText = getDxccTooltip(modelData)
                                                var pos = rxDelegateMouseArea.mapToGlobal(mouseX, mouseY)
                                                tooltipX = pos.x - decodeWindow.x + 10
                                                tooltipY = pos.y - decodeWindow.y - 30
                                                tooltipVisible = true
                                            }
                                        } else {
                                            tooltipVisible = false
                                        }
                                    }
                                    onPositionChanged: {
                                        if (containsMouse && tooltipVisible) {
                                            var pos = rxDelegateMouseArea.mapToGlobal(mouseX, mouseY)
                                            tooltipX = pos.x - decodeWindow.x + 10
                                            tooltipY = pos.y - decodeWindow.y - 30
                                        }
                                    }
                                }

                                RowLayout {
                                    visible: !parent.isPeriodSeparator
                                    anchors.fill: parent
                                    anchors.leftMargin: 4
                                    anchors.rightMargin: 4
                                    spacing: 0

                                    Text {
                                        text: modelData.formattedTime || formatUtcForDisplay(modelData.time)
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                        Layout.preferredWidth: decodeWindow.rxUtcWidth
                                    }

                                    Text {
                                        text: modelData.db
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(parseInt(modelData.db) > -5 ? accentGreen :
	                                               parseInt(modelData.db) > -15 ? secondaryCyan :
	                                               parseInt(modelData.db) > -20 ? textSecondary : "#888")
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.rxDbWidth
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.rxDbDtGapWidth }

                                    Text {
                                        text: modelData.dt
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
	                                        color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.rxDtWidth
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.rxGapWidth }

                                    // Messaggio RX con coloring + strikethrough B4
                                    Rectangle {
                                        visible: modelData.isLotw === true
                                        Layout.preferredWidth: 6
                                        Layout.preferredHeight: 6
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 3
                                        color: decodeWindow.lotwMarkerColor()
                                        border.color: decodeWindow.boostedDecodeTextColor(textSecondary)
                                        border.width: 1
                                    }
                                    Rectangle {
                                        visible: decodeWindow.usStateLabel(modelData).length > 0
                                        Layout.preferredWidth: 26
                                        Layout.preferredHeight: 16
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 4
                                        color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.16)
                                        border.color: decodeWindow.boostedDecodeTextColor(secondaryCyan)
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: decodeWindow.usStateLabel(modelData)
                                            font.family: decodiumMonoFontFamily
                                            font.pixelSize: 9
                                            font.bold: true
                                            color: decodeWindow.boostedDecodeTextColor(secondaryCyan)
                                        }
                                    }
                                    Text {
                                        id: rxMsgText
                                        text: modelData.displayMessage || modelData.message
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 11
                                        // 1.0.144: precomputed in C++ (enrichDecodeEntry)
                                        font.bold: decodeWindow.decodeEntryBold(modelData)
                                        font.strikeout: modelData.isB4 && bridge.b4Strikethrough
                                        color: getDxccColor(modelData)
                                        Layout.fillWidth: true
                                        elide: decodeWindow.messageElideMode(modelData.displayMessage || modelData.message)
                                    }

                                    Text {
                                        visible: !decodeWindow.compactRxColumns
                                        text: modelData.dxDistance > 0 ?
                                              decodeWindow.formatDistanceText(modelData.dxDistance, false) : ""
                                        font.pixelSize: 9
	                                        color: decodeWindow.boostedDecodeTextColor((bridge && bridge.themeManager) ? bridge.themeManager.textSecondary : "#666688")
                                        Layout.preferredWidth: decodeWindow.rxDistanceWidth
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }

                    }
                }
            }
        }

        // Status bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 20

                Text {
                    text: appEngine.monitoring ? "● Monitoring" : "○ Stopped"
                    font.pixelSize: 11
                    color: appEngine.monitoring ? accentGreen : textSecondary
                }

                Text {
                    text: "Mode: " + appEngine.mode
                    font.pixelSize: 11
                    color: textSecondary
                }

                Text {
                    text: debugInfo  // IU8LMC: Debug display for DXCC hover
                    font.pixelSize: 10
                    color: secondaryCyan
                }

                Text {
                    text: "RX: " + appEngine.rxFrequency + " Hz"
                    font.pixelSize: 11
                    color: primaryBlue
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: appEngine.decoding ? "Decoding..." : "Ready"
                    font.pixelSize: 11
                    color: appEngine.decoding ? secondaryCyan : textSecondary
                }
            }
        }
    }

    // Helper functions to extract callsign and grid from message
    function extractCall(message) {
        // FT8 message formats:
        // CQ DX_CALL GRID           -> extract DX_CALL
        // MY_CALL DX_CALL REPORT    -> extract DX_CALL (DX is calling me)
        // DX_CALL MY_CALL REPORT    -> extract DX_CALL (I should call DX)
        var parts = message.split(" ")
        if (parts.length < 2) return ""

        // Skip CQ prefix
        var startIdx = 0
        if (parts[0] === "CQ" || parts[0].startsWith("CQ_")) {
            startIdx = 1
            // Handle CQ DX, CQ NA, etc. (2-letter modifiers)
            if (parts.length > 2 && parts[1].length <= 3 && !/[0-9]/.test(parts[1])) {
                startIdx = 2
            }
        }

        if (startIdx >= parts.length) return ""

        var call1 = parts[startIdx] || ""
        var call2 = parts[startIdx + 1] || ""

        // For CQ messages, return the CQ caller
        if (startIdx > 0) {
            return call1
        }

        // For non-CQ messages, find which call is not mine
        var myCall = appEngine.callsign.toUpperCase()
        if (call1.toUpperCase() === myCall) {
            return call2  // DX is calling me, return DX call
        } else {
            return call1  // I should call this station
        }
    }

    function extractGrid(message) {
        var parts = String(message || "").toUpperCase().replace(/[<>;,]/g, " ").split(/\s+/)
        for (var i = parts.length - 1; i >= 0; --i) {
            var token = parts[i].trim()
            if (token === "RR73" || token === "RRR" || token === "73")
                continue
            if (/^[A-R]{2}[0-9]{2}([A-X]{2})?$/.test(token))
                return token
        }
        return ""
    }

    // IU8LMC: click destro su un decode -> apre la scheda del nominativo su QRZ.com nel browser.
    // Usa la call base (i portable/prefix risolvono sulla scheda dell'operatore).
    function openQrzLookup(modelData) {
        if (!modelData)
            return
        var call = callsignBase(String(modelData.dxCallsign || ""))
        if (call.length === 0)
            call = callsignBase(String(extractCall(modelData.message || "")))
        if (call.length === 0)
            return
        Qt.openUrlExternally("https://www.qrz.com/db/" + call.toUpperCase())
    }

    function messageElideMode(message) {
        var myCall = String((appEngine && appEngine.callsign) || (bridge && bridge.callsign) || "").trim().toUpperCase()
        if (!myCall.length)
            return Text.ElideRight

        var normalized = " " + String(message || "").toUpperCase().replace(/[<>;,]/g, " ") + " "
        return normalized.indexOf(" " + myCall + " ") >= 0
            ? Text.ElideMiddle
            : Text.ElideRight
    }
}
