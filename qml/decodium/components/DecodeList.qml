/* Decodium Qt6 - Decode List Component */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: decodeListComponent

    property var model: []
    property color accentGreen: bridge.themeManager.accentColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color errorRed: bridge.themeManager.errorColor
    property bool highlight73: bridge.getSetting("Highlight73", true)
    property bool highlightOrange: bridge.getSetting("HighlightOrange", false)
    property bool highlightBlue: bridge.getSetting("HighlightBlue", false)
    property string highlightOrangeCallsigns: bridge.getSetting("HighlightOrangeCallsigns", "")
    property string highlightBlueCallsigns: bridge.getSetting("HighlightBlueCallsigns", "")

    signal decodeSelected(int index)
    signal decodeDoubleClicked(int index)

    function messageElideMode(message) {
        var myCall = String((bridge && bridge.callsign) || "").trim().toUpperCase()
        if (!myCall.length)
            return Text.ElideRight

        var normalized = " " + String(message || "").toUpperCase().replace(/[<>;,]/g, " ") + " "
        return normalized.indexOf(" " + myCall + " ") >= 0
            ? Text.ElideMiddle
            : Text.ElideRight
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

    function customHighlightColor(modelData) {
        var message = modelData.message || ""
        if (highlightOrange && highlightListMatches(message, highlightOrangeCallsigns))
            return "#E14B00"
        if (highlightBlue && highlightListMatches(message, highlightBlueCallsigns))
            return "#0064FF"
        return ""
    }

    // Background color from the WSJT-X-style cascade in C++ (bridge.decodeHighlightBg).
    // Returned hex is converted to a translucent fill so message text stays readable on dark theme.
    // Higher alpha (0.55) so the WSJT-X palette differences are obvious on the dark theme.
    function wsjtxBgColor(modelData) {
        var hex = bridge.decodeHighlightBg(modelData)
        if (!hex || hex.length === 0) return null
        var c = Qt.color(hex)
        return Qt.rgba(c.r, c.g, c.b, 0.55)
    }
    function wsjtxBorderColor(modelData) {
        var hex = bridge.decodeHighlightBg(modelData)
        if (!hex || hex.length === 0) return null
        var c = Qt.color(hex)
        return Qt.rgba(c.r, c.g, c.b, 0.95)
    }

    function decodeTextColor(modelData) {
        var customColor = customHighlightColor(modelData)
        // Quando la cascata WSJT-X assegna uno sfondo colorato, il testo va forzato
        // su nero altrimenti il testo chiaro sparisce sul rosa/cream/cyan pallido.
        var hl = bridge.decodeHighlightBg(modelData)
        if (hl && hl.length > 0)
            return "#000000"
        if (modelData.isTx)
            return errorRed
        if (modelData.isMyCall)
            return bridge.colorMyCall
        if (customColor !== "")
            return customColor
        if (highlight73 && isSignoffMessage(modelData.message))
            return bridge.color73
        if (modelData.isCQ)
            return bridge.colorCQ
        if (modelData.isB4)
            return bridge.colorB4
        if (modelData.dxCountry && String(modelData.dxCountry).length > 0)
            return bridge.colorDXEntity
        return textPrimary
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            if (key === "Highlight73")
                decodeListComponent.highlight73 = !!value
            else if (key === "HighlightOrange")
                decodeListComponent.highlightOrange = !!value
            else if (key === "HighlightBlue")
                decodeListComponent.highlightBlue = !!value
            else if (key === "HighlightOrangeCallsigns" || key === "OrangeCallsigns")
                decodeListComponent.highlightOrangeCallsigns = String(value || "")
            else if (key === "HighlightBlueCallsigns" || key === "BlueCallsigns")
                decodeListComponent.highlightBlueCallsigns = String(value || "")
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: decodeListComponent.model
        spacing: 3
        // Chronological order: oldest at top, newest at bottom (like WSJT-X)
        verticalLayoutDirection: ListView.TopToBottom

        // Auto-scroll to newest decode at the bottom
        onCountChanged: {
            if (count > 0 && !listView.moving) {
                Qt.callLater(function() { listView.positionViewAtEnd() })
            }
        }

        delegate: Rectangle {
            width: listView.width
            height: 32
            // WSJT-X palette first (TX > MyCall > NewDxccBand > … > CQ via bridge.decodeHighlightBg),
            // fallback ai tinte storiche se la cascata non assegna nulla.
            color: {
                var wsx = decodeListComponent.wsjtxBgColor(modelData)
                if (wsx) return wsx
                if (modelData.isTx)  return Qt.rgba(244/255, 67/255, 54/255, 0.2)
                if (modelData.isCQ)  return Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)
                if (modelData.isMyCall) return Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.1)
                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.04)
            }
            border.color: {
                var wsx = decodeListComponent.wsjtxBorderColor(modelData)
                if (wsx) return wsx
                if (modelData.isTx)     return errorRed
                if (modelData.isCQ)     return Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3)
                if (modelData.isMyCall) return Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.4)
                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
            }
            border.width: (modelData.isTx || modelData.isMyCall) ? 2 : 1
            radius: 5

            MouseArea {
                anchors.fill: parent
                onClicked: decodeListComponent.decodeSelected(index)
                onDoubleClicked: decodeListComponent.decodeDoubleClicked(index)
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                // Tempo UTC
                Text {
                    text: modelData.time || ""
                    font.family: "Monospace"
                    font.pixelSize: 11
                    color: modelData.isTx ? errorRed : textSecondary
                    Layout.preferredWidth: 52
                }

                // SNR / TX indicator
                Text {
                    text: modelData.isTx ? ">>>TX" : (modelData.db + "dB")
                    font.family: "Monospace"
                    font.pixelSize: 11
                    font.bold: modelData.isTx
                    color: {
                        if (modelData.isTx) return errorRed
                        var db = parseInt(modelData.db)
                        if (db >= 0)   return (bridge && bridge.themeManager) ? bridge.themeManager.successColor : "#69F0AE"
                        if (db >= -10) return accentGreen
                        if (db >= -15) return textSecondary
                        return Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.7)
                    }
                    Layout.preferredWidth: 46
                }

                // Frequenza audio
                Text {
                    text: modelData.freq + "Hz"
                    font.family: "Monospace"
                    font.pixelSize: 11
                    color: modelData.isTx ? errorRed : secondaryCyan
                    Layout.preferredWidth: 54
                }

                // GAP 5 — C13: Distanza (km) se disponibile
                Text {
                    visible: !modelData.isTx && modelData.dxDistance !== undefined && modelData.dxDistance > 0
                    text: modelData.dxDistance !== undefined && modelData.dxDistance > 0
                          ? Math.round(modelData.dxDistance) + "km"
                          : ""
                    font.family: "Monospace"
                    font.pixelSize: 10
                    color: (bridge && bridge.themeManager) ? bridge.themeManager.secondaryColor : "#80CBC4"
                    Layout.preferredWidth: 52
                    horizontalAlignment: Text.AlignRight
                }

                // Testo messaggio
                Text {
                    text: modelData.message || ""
                    font.family: "Monospace"
                    font.pixelSize: 12
                    font.bold: modelData.isCQ || modelData.isTx
                    // B7 — C13: applica colori dinamici
                    color: decodeListComponent.decodeTextColor(modelData)
                    font.strikeout: modelData.isB4 !== undefined && modelData.isB4 && bridge.showB4Strikethrough
                    opacity: modelData.isB4 !== undefined && modelData.isB4 ? 0.55 : 1.0
                    Layout.fillWidth: true
                    elide: decodeListComponent.messageElideMode(modelData.message)
                }
            }
        }
    }
}
