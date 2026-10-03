/* SignalRxPanel — DX-Pedition Mode "Signal RX · QSO Lock" (design §3.7)
 * Phase 2b (1.0.331): large DX-call lock header + decode list filtered on the
 * RX frequency. Reads bridge.rxDecodeModel (the SAME model the classic inline
 * rxFreqPanel uses) via the GLOBAL bridge context. Classic inline untouched.
 *
 * Lesson 1.0.205: `!modelData` guard FIRST in every delegate clause.
 * Lesson Fase 2a: never shadow the global `bridge` (default-bound below).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)

    // IU8LMC: apre la scheda del nominativo su QRZ.com nel browser (call base).
    function openQrzLookup(entry) {
        if (!entry) return
        var raw = String(entry.dxCallsign || "")
        if (raw.length === 0) {
            var parts = String(entry.message || "").split(/\s+/)
            for (var i = 0; i < parts.length; ++i) {
                var t = parts[i].replace(/[^A-Za-z0-9\/]/g, "")
                if (/[A-Za-z]/.test(t) && /[0-9]/.test(t)) { raw = t; break }
            }
        }
        var segs = String(raw).split("/"), best = ""
        for (var j = 0; j < segs.length; ++j) if (segs[j].length > best.length) best = segs[j]
        var call = (best.length ? best : raw).toUpperCase().replace(/[^A-Z0-9]/g, "")
        if (call.length > 0) Qt.openUrlExternally("https://www.qrz.com/db/" + call)
    }

    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:   tm ? tm.accentColor    : "#19ff88"
    readonly property color cBorder:   tm ? tm.borderColor    : "#1f2a22"
    readonly property color cText:     tm ? tm.textPrimary    : "#d6dcd8"
    readonly property color cTextDim:  tm ? tm.textSecondary  : "#6c7872"
    readonly property color cBlue:     tm ? tm.primaryColor   : "#3aa0ff"
    readonly property color cCyan:     tm ? tm.secondaryColor : "#66e6ff"
    readonly property color cTx:       tm ? tm.txColor        : "#ff7a5c"
    readonly property color cLotw:     root.bridge ? root.bridge.effectiveDecodeColor("colorLotwUser") : "#ffffff"
    readonly property color cCq:       root.bridge ? root.bridge.effectiveDecodeColor("colorCQ") : root.cAccent

    readonly property int rowH: tm ? tm.densityRowHeight() : 22
    readonly property int fSize: tm ? tm.densityFontSize() : 12
    property int decodeColorRevision: 0
    property bool highlight73: root.bridge ? root.bridge.getSetting("Highlight73", true) : true
    property bool highlightOrange: root.bridge ? root.bridge.getSetting("HighlightOrange", false) : false
    property bool highlightBlue: root.bridge ? root.bridge.getSetting("HighlightBlue", false) : false
    property string highlightOrangeCallsigns: root.bridge ? root.bridge.getSetting("HighlightOrangeCallsigns", "") : ""
    property string highlightBlueCallsigns: root.bridge ? root.bridge.getSetting("HighlightBlueCallsigns", "") : ""
    property int decodeColorBoost: root.bridge ? Math.max(0, Math.min(100, Number(root.bridge.getSetting("uiDecodeColorBoost", 35)))) : 35

    readonly property bool compact: width < 420
    readonly property int wUtc:  compact ? 58 : 76
    readonly property int wDb:   34
    readonly property int wDt:   compact ? 40 : 46
    readonly property int gap:   8

    function usStateLabel(entry) {
        if (!root.bridge || !root.bridge.showUsState || !entry || !entry.usState)
            return ""
        return String(entry.usState).trim().toUpperCase()
    }

    function refreshDecodeColors() {
        decodeColorRevision = (decodeColorRevision + 1) % 1000000
    }

    Connections {
        target: root.bridge
        function onSettingValueChanged(key, value) {
            if (key === "Highlight73")
                root.highlight73 = !!value
            else if (key === "HighlightOrange")
                root.highlightOrange = !!value
            else if (key === "HighlightBlue")
                root.highlightBlue = !!value
            else if (key === "HighlightOrangeCallsigns" || key === "OrangeCallsigns")
                root.highlightOrangeCallsigns = String(value || "")
            else if (key === "HighlightBlueCallsigns" || key === "BlueCallsigns")
                root.highlightBlueCallsigns = String(value || "")
            else if (key === "uiDecodeColorBoost")
                root.decodeColorBoost = Math.max(0, Math.min(100, Number(value)))
            else
                return
            root.refreshDecodeColors()
        }
        function onColor73Changed() { root.refreshDecodeColors() }
        function onColorCQChanged() { root.refreshDecodeColors() }
        function onColorMyCallChanged() { root.refreshDecodeColors() }
        function onColorDXEntityChanged() { root.refreshDecodeColors() }
        function onColorB4Changed() { root.refreshDecodeColors() }
        function onColorTxMessageChanged() { root.refreshDecodeColors() }
        function onColorDecodeTextChanged() { root.refreshDecodeColors() }
        function onColorNewDxccChanged() { root.refreshDecodeColors() }
        function onColorNewDxccBandChanged() { root.refreshDecodeColors() }
        function onColorNewContinentChanged() { root.refreshDecodeColors() }
        function onColorNewContinentBandChanged() { root.refreshDecodeColors() }
        function onColorNewCqZoneChanged() { root.refreshDecodeColors() }
        function onColorNewCqZoneBandChanged() { root.refreshDecodeColors() }
        function onColorNewItuZoneChanged() { root.refreshDecodeColors() }
        function onColorNewItuZoneBandChanged() { root.refreshDecodeColors() }
        function onColorNewGridChanged() { root.refreshDecodeColors() }
        function onColorNewGridBandChanged() { root.refreshDecodeColors() }
        function onColorNewCallChanged() { root.refreshDecodeColors() }
        function onColorNewCallBandChanged() { root.refreshDecodeColors() }
        function onDecodeColorEnabledChanged(prop, enabled) { root.refreshDecodeColors() }
        function onDecodeColorBoldChanged(prop, bold) { root.refreshDecodeColors() }
    }

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

    function decodeMessageColorProp(entry) {
        if (!entry)
            return "colorDecodeText"
        if (entry.isTx === true && decodeColorCategoryEnabled("colorTxMessage")) return "colorTxMessage"
        if (entry.isMyCall === true && decodeColorCategoryEnabled("colorMyCall")) return "colorMyCall"
        if (root.highlight73 && root.bridge && isSignoffMessage(entry.displayMessage || entry.message || "") && decodeColorCategoryEnabled("color73")) return "color73"
        if ((entry.isB4 === true || entry.dxIsWorked === true) && decodeColorCategoryEnabled("colorB4")) return "colorB4"
        if (entry.isCQ === true && decodeColorCategoryEnabled("colorCQ")) return "colorCQ"
        if (entry.dxIsNewDxccBand === true && decodeColorCategoryEnabled("colorNewDxccBand")) return "colorNewDxccBand"
        if (entry.dxIsNewDxcc === true && decodeColorCategoryEnabled("colorNewDxcc")) return "colorNewDxcc"
        if (entry.dxIsNewContinentBand === true && decodeColorCategoryEnabled("colorNewContinentBand")) return "colorNewContinentBand"
        if (entry.dxIsNewContinent === true && decodeColorCategoryEnabled("colorNewContinent")) return "colorNewContinent"
        if (entry.dxIsNewCqZoneBand === true && decodeColorCategoryEnabled("colorNewCqZoneBand")) return "colorNewCqZoneBand"
        if (entry.dxIsNewCqZone === true && decodeColorCategoryEnabled("colorNewCqZone")) return "colorNewCqZone"
        if (entry.dxIsNewItuZoneBand === true && decodeColorCategoryEnabled("colorNewItuZoneBand")) return "colorNewItuZoneBand"
        if (entry.dxIsNewItuZone === true && decodeColorCategoryEnabled("colorNewItuZone")) return "colorNewItuZone"
        if (entry.dxIsNewGridBand === true && decodeColorCategoryEnabled("colorNewGridBand")) return "colorNewGridBand"
        if (entry.dxIsNewGrid === true && decodeColorCategoryEnabled("colorNewGrid")) return "colorNewGrid"
        if (entry.dxIsNewCallBand === true && decodeColorCategoryEnabled("colorNewCallBand")) return "colorNewCallBand"
        if (entry.dxIsNewCall === true && decodeColorCategoryEnabled("colorNewCall")) return "colorNewCall"
        if ((entry.dxIsMostWanted === true || entry.dxIsNewCountry === true || entry.dxIsNewBand === true)
                && decodeColorCategoryEnabled("colorDXEntity"))
            return "colorDXEntity"
        return "colorDecodeText"
    }

    function decodeColorCategoryEnabled(prop) {
        root.decodeColorRevision
        return !!(root.bridge && root.bridge.decodeColorEnabled(prop))
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
        root.decodeColorRevision
        var boost = Math.max(0, Math.min(100, Number(root.decodeColorBoost))) / 100.0
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
        root.decodeColorRevision
        var boost = Math.max(0, Math.min(100, Number(root.decodeColorBoost))) / 100.0
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

    function decodeMessageColor(entry) {
        root.decodeColorRevision
        if (!entry)
            return root.cText
        var message = entry.displayMessage || entry.message || ""
        if (root.highlightOrange && highlightListMatches(message, root.highlightOrangeCallsigns))
            return boostedDecodeTextColor("#E14B00")
        if (root.highlightBlue && highlightListMatches(message, root.highlightBlueCallsigns))
            return boostedDecodeTextColor("#0064FF")
        return root.bridge ? boostedDecodeTextColor(root.bridge.effectiveDecodeColor(decodeMessageColorProp(entry))) : root.cText
    }

    function decodeMessageBold(entry) {
        root.decodeColorRevision
        if (!entry || !root.bridge)
            return false
        var message = entry.displayMessage || entry.message || ""
        if ((root.highlightOrange && highlightListMatches(message, root.highlightOrangeCallsigns)) ||
            (root.highlightBlue && highlightListMatches(message, root.highlightBlueCallsigns)))
            return false
        var prop = decodeMessageColorProp(entry)
        return root.bridge.decodeColorEnabled(prop) && root.bridge.decodeColorBold(prop)
    }

    // --- QSO lock banner: large DX call (accent) -------------------------------
    Rectangle {
        id: lockBanner
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 48
        color: Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.10)
        RowLayout {
            anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
            spacing: 12
            ColumnLayout {
                spacing: 0
                Text {
                    text: qsTr("QSO LOCK")
                    color: root.cTextDim
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.6
                }
                Text {
                    text: {
                        var c = (root.bridge && root.bridge.dxCall) ? String(root.bridge.dxCall).trim() : ""
                        return c.length > 0 ? c : "—"
                    }
                    color: root.cAccent
                    font.pixelSize: 24; font.bold: true; font.family: decodiumMonoFontFamily
                }
            }
            Item { Layout.fillWidth: true }
            ColumnLayout {
                spacing: 0
                Layout.alignment: Qt.AlignVCenter
                Text {
                    text: "GRID"
                    color: root.cTextDim
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.4
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
                Text {
                    text: {
                        var g = (root.bridge && root.bridge.dxGrid) ? String(root.bridge.dxGrid).trim() : ""
                        return g.length > 0 ? g : "—"
                    }
                    color: root.cCyan
                    font.pixelSize: 16; font.bold: true; font.family: decodiumMonoFontFamily
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }
    }

    // Column header strip.
    Rectangle {
        id: colHdr
        anchors { left: parent.left; right: parent.right; top: lockBanner.bottom }
        height: 20
        color: Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.18)
        RowLayout {
            anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
            spacing: 0
            Text { text: "UTC"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: decodiumMonoFontFamily; Layout.preferredWidth: root.wUtc }
            Text { text: "dB"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: decodiumMonoFontFamily; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDb }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "DT"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: decodiumMonoFontFamily; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDt }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "MESSAGE"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: decodiumMonoFontFamily; Layout.fillWidth: true }
        }
    }

    // RX-filtered decode list.
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: colHdr.bottom; bottom: parent.bottom }
        anchors.margins: 2
        clip: true
        spacing: 1
        cacheBuffer: 600
        reuseItems: true
        model: (root.bridge && root.bridge.rxDecodeModel) ? root.bridge.rxDecodeModel : null

        property bool followTail: true
        function snapTail() {
            if (followTail) positionViewAtEnd()
        }
        Connections {
            target: (root.bridge && root.bridge.rxDecodeModel)
                    ? root.bridge.rxDecodeModel : null
            ignoreUnknownSignals: true
            function onSnapshotApplied() { Qt.callLater(list.snapTail) }
        }
        onDraggingChanged: if (dragging) followTail = false
        onContentYChanged: {
            followTail = (contentHeight <= height + 2) || (contentY >= originY + contentHeight - height - 48)
        }
        Component.onCompleted: Qt.callLater(positionViewAtEnd)

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 8 }

        delegate: Rectangle {
            id: del
            readonly property var entry: modelData || ({})
            readonly property bool isSep: !!(modelData && modelData.isSeparator === true)
            width: list.width
            height: isSep ? 4 : root.rowH
            color: !modelData ? "transparent" :
                   isSep ? "transparent" :
                   (root.bridge && root.bridge.decodeHighlightUserBg(entry).length > 0) ? root.boostedDecodeBackgroundColor(root.bridge.decodeHighlightUserBg(entry)) :
                   entry.isTx ? Qt.rgba(0.95, 0.77, 0.06, 0.28) :
                   entry.isMyCall ? Qt.rgba(0.96, 0.26, 0.21, 0.28) :
                   (entry.isCQ && root.bridge && root.bridge.decodeColorEnabled("colorCQ")) ? root.boostedDecodeBackgroundColor(Qt.rgba(root.cCq.r, root.cCq.g, root.cCq.b, 0.14)) :
                   (index % 2 === 0) ? Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.07)
                                     : Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.13)
            radius: 2

            Rectangle {
                visible: del.isSep
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: 12; anchors.rightMargin: 12
                height: 1
                color: Qt.rgba(0.85, 0.25, 0.25, 0.5)
            }

            MouseArea {
                enabled: !!modelData && !del.isSep
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) {
                    if (!modelData || del.isSep || del.entry.isTx) return
                    if (!root.bridge) return
                    if (mouse.button === Qt.LeftButton) {
                        if (!root.bridge.holdTxFreq)
                            root.bridge.txFrequency = parseInt(del.entry.freq || "0")
                    } else if (mouse.button === Qt.RightButton) {
                        // Destro = imposta RX freq; Shift+Destro = QRZ.com (IU8LMC)
                        if (mouse.modifiers & Qt.ShiftModifier)
                            root.openQrzLookup(del.entry)
                        else
                            root.bridge.rxFrequency = parseInt(del.entry.freq || "0")
                    }
                }
                onDoubleClicked: function(mouse) {
                    if (!modelData || del.isSep || del.entry.isTx) return
                    if (!root.bridge || mouse.button !== Qt.LeftButton) return
                    if (!del.entry.message) return
                    root.bridge.processDecodeDoubleClick(
                        del.entry.message || "",
                        del.entry.time || "",
                        del.entry.db || "",
                        parseInt(del.entry.freq || "0"))
                }
            }

            RowLayout {
                visible: !del.isSep
                anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                spacing: 0
                Text {
                    text: !modelData ? "" : (del.entry.formattedTime || del.entry.time || "")
                    color: !modelData ? root.cTextDim : (del.entry.isTx ? "#f1c40f" : root.cTextDim)
                    font.pixelSize: root.fSize; font.family: decodiumMonoFontFamily
                    Layout.preferredWidth: root.wUtc
                }
                Text {
                    text: !modelData ? "" : (del.entry.db || "")
                    color: !modelData ? root.cTextDim : (del.entry.snrColor || (del.entry.isTx ? "#f1c40f" : root.cTextDim))
                    font.pixelSize: root.fSize; font.family: decodiumMonoFontFamily
                    font.bold: !!modelData && del.entry.isTx === true
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: root.wDb
                }
                Item { Layout.preferredWidth: root.gap }
                Text {
                    text: !modelData ? "" : (del.entry.dt || "")
                    color: !modelData ? root.cTextDim : (del.entry.isTx ? "#f1c40f" : root.cTextDim)
                    font.pixelSize: root.fSize; font.family: decodiumMonoFontFamily
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: root.wDt
                }
                Item { Layout.preferredWidth: root.gap }
                Rectangle {
                    visible: !!modelData && del.entry.isLotw === true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    Layout.alignment: Qt.AlignVCenter
                    radius: 3
                    color: root.cLotw
                    border.color: root.cTextDim
                    border.width: 1
                }
                Rectangle {
                    visible: root.usStateLabel(del.entry).length > 0
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 16
                    Layout.alignment: Qt.AlignVCenter
                    radius: 4
                    color: Qt.rgba(root.cCyan.r, root.cCyan.g, root.cCyan.b, 0.16)
                    border.color: root.cCyan
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: root.usStateLabel(del.entry)
                        color: root.cCyan
                        font.pixelSize: 9
                        font.bold: true
                        font.family: decodiumMonoFontFamily
                    }
                }
                Text {
                    text: !modelData ? "" : (del.entry.displayMessage || del.entry.message || "")
                    color: root.decodeMessageColor(del.entry)
                    font.pixelSize: root.fSize; font.family: decodiumMonoFontFamily
                    font.bold: root.decodeMessageBold(del.entry)
                    font.strikeout: !!modelData
                                        && (del.entry.isB4 === true || del.entry.dxIsWorked === true)
                                        && !!root.bridge
                                        && root.bridge.b4Strikethrough
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: qsTr("No RX-frequency decodes")
            color: root.cTextDim
            font.pixelSize: 12; font.italic: true
        }
    }
}
