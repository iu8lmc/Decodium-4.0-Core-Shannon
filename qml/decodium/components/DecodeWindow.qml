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
    readonly property real leftPanelWidth: width * 0.5
    readonly property bool compactBandColumns: leftPanelWidth < 460
    readonly property int bandUtcWidth: compactBandColumns ? 48 : 60
    readonly property int bandDbWidth: compactBandColumns ? 28 : 35
    readonly property int bandDtWidth: compactBandColumns ? 34 : 45
    readonly property int bandFreqWidth: compactBandColumns ? 42 : 50
    readonly property int bandGapWidth: compactBandColumns ? 6 : 10
    readonly property int bandDxccWidth: compactBandColumns ? 92 : 132
    readonly property int bandAzWidth: compactBandColumns ? 42 : 52
    readonly property int bandMessageMinWidth: compactBandColumns ? 72 : 140
    readonly property real rightPanelWidth: width * 0.5
    readonly property bool compactRxColumns: rightPanelWidth < 420
    readonly property bool compactRxHeader: rightPanelWidth < 340
    readonly property int rxUtcWidth: compactRxColumns ? 58 : 72
    readonly property int rxDbWidth: compactRxColumns ? 28 : 35
    readonly property int rxDtWidth: compactRxColumns ? 36 : 45
    readonly property int rxGapWidth: compactRxColumns ? 6 : 10
    readonly property int rxDistanceWidth: compactRxColumns ? 0 : 50
    readonly property int rxHeaderBadgeWidth: compactRxHeader ? 62 : 70
    property int decodeListVersion: 0

    // Shannon-compatible color scheme
    readonly property color colorB4:          "#606060"   // Grigio — già lavorato (B4)
    readonly property color colorWorked:      "#606060"   // alias B4
    readonly property color colorNewBand:     "#FFD700"   // Oro — nuovo su questa banda
    readonly property color colorNewCountry:  "#00BB00"   // Verde — nuovo paese
    readonly property color colorMostWanted:  "#FF00FF"   // Magenta — most wanted
    readonly property color colorMyCall:      "#FF4444"   // Rosso — nostro callsign
    readonly property color colorTx:          "#FF8C00"   // Arancione — TX
    readonly property color colorLotw:        "#44BBFF"   // Azzurro — utente LotW

    // Shannon-compatible coloring (priorità DecodeHighlightingModel)
    function getDxccColor(modelData) {
        if (modelData.isTx)     return colorTx
        if (modelData.isMyCall) return colorMyCall
        if (modelData.isB4 || modelData.dxIsWorked) return colorB4
        if (modelData.isLotw)   return colorLotw
        if (modelData.dxIsMostWanted) return colorMostWanted
        if (modelData.dxIsNewCountry) return colorNewCountry
        if (modelData.dxIsNewBand)    return colorNewBand
        if (modelData.isCQ)     return accentGreen
        return textPrimary
    }

    // IU8LMC: Function to build tooltip text
    function getDxccTooltip(modelData) {
        if (!modelData.dxCountry) return ""
        var lines = []

        // Header: Callsign - Country (Continent)
        var header = (modelData.dxCallsign || "") + " - " + modelData.dxCountry
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

    function formatBearingDegrees(value) {
        return value !== undefined && value >= 0 ? Math.round(value) + "°" : ""
    }

    // Shannon: RX Frequency window ±200Hz (sbFtol default) + messaggi diretti a noi
    property int rxBandwidth: 200  // Shannon sbFtol default 200Hz

    Connections {
        target: appEngine
        function onDecodeListChanged() {
            decodeWindow.decodeListVersion++
        }
        function onRxDecodeListChanged() {
            decodeWindow.decodeListVersion++
        }
    }

    // Shannon isAtRxFrequency: dentro finestra ±200Hz OR messaggio per noi
    function isAtRxFrequency(freq, md) {
        var f = parseInt(freq)
        var inWindow = Math.abs(f - appEngine.rxFrequency) <= rxBandwidth
        var relevant = md && (md.isMyCall || md.isTx)
        return inWindow || relevant
    }
    function currentRxDecodes() {
        var merged = []
        function utcSortValue(timeStr) {
            var digits = String(timeStr || "").replace(/[^0-9]/g, "")
            if (digits.length >= 6)
                return parseInt(digits.substring(0, 6))
            if (digits.length === 4)
                return parseInt(digits + "00")
            return -1
        }
        if (appEngine.rxDecodeList) {
            for (var j = 0; j < appEngine.rxDecodeList.length; j++) {
                if (appEngine.rxDecodeList[j])
                    merged.push(appEngine.rxDecodeList[j])
            }
        }
        merged.sort(function(a, b) {
            var ta = utcSortValue(a.time)
            var tb = utcSortValue(b.time)
            if (ta !== tb)
                return ta - tb
            var fa = parseInt(a.freq || "0")
            var fb = parseInt(b.freq || "0")
            if (fa !== fb)
                return fa - fb
            return String(a.message || "").localeCompare(String(b.message || ""))
        })
        return merged
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
        color: "#e0202020"
        border.color: "#606060"
        border.width: 1
        radius: 4

        Text {
            id: tooltipLabel
            anchors.centerIn: parent
            text: tooltipText
            color: textPrimary
            font.pixelSize: 11
            font.family: "Segoe UI"
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
                                text: "Band Activity"
                                font.pixelSize: 13
                                font.bold: true
                                color: secondaryCyan
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: appEngine.decodeList.length + " decodes"
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
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                Layout.preferredWidth: decodeWindow.bandUtcWidth
                            }
                            Text {
                                text: "dB"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.bandDbWidth
                            }
                            Text {
                                text: "DT"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.bandDtWidth
                            }
                            Text {
                                text: "Freq"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.bandFreqWidth
                            }
                            Item { Layout.preferredWidth: decodeWindow.bandGapWidth }
                            Text {
                                text: "Message"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: secondaryCyan
                                Layout.fillWidth: true
                            }
                            Item {
                                Layout.preferredWidth: decodeWindow.bandDxccWidth
                                Layout.fillHeight: true
                                Text {
                                    anchors.fill: parent
                                    text: "DXCC"
                                    font.family: "Monospace"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: secondaryCyan
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Item {
                                Layout.preferredWidth: decodeWindow.bandAzWidth
                                Layout.fillHeight: true
                                Text {
                                    anchors.fill: parent
                                    text: "Az"
                                    font.family: "Monospace"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: secondaryCyan
                                    horizontalAlignment: Text.AlignHCenter
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
                            model: appEngine.decodeList
                            spacing: 1
                            property bool followTail: true
                            function updateFollowTail() {
                                followTail = contentHeight <= height + 2
                                          || contentY >= Math.max(0, contentHeight - height - 8)
                            }
                            Component.onCompleted: Qt.callLater(function() {
                                positionViewAtEnd()
                                updateFollowTail()
                            })
                            onContentYChanged: updateFollowTail()
                            onHeightChanged: updateFollowTail()
                            onCountChanged: {
                                if (followTail) {
                                    Qt.callLater(function() {
                                        positionViewAtEnd()
                                        updateFollowTail()
                                    })
                                }
                            }

                            ScrollBar.vertical: ScrollBar {
                                active: true
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                width: bandActivityList.width - 12
                                height: 26
                                color: modelData.isMyCall ?
                                       Qt.rgba(244/255, 67/255, 54/255, 0.25) :
                                       modelData.isCQ ?
                                       Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12) :
                                       isAtRxFrequency(modelData.freq, modelData) ?
                                       Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2) :
                                       index % 2 === 0 ?
                                       Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.02) :
                                       Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                                radius: 2

                                MouseArea {
                                    id: bandDelegateMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        appEngine.selectDecode(index)
                                        // Set RX frequency to this decode's frequency
                                        appEngine.rxFrequency = parseInt(modelData.freq)
                                    }
                                    onDoubleClicked: {
                                        // Set TX and RX frequency to decode frequency
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
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 0

                                    Text {
                                        text: formatUtcForDisplay(modelData.time)
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: textSecondary
                                        Layout.preferredWidth: decodeWindow.bandUtcWidth
                                    }

                                    Text {
                                        text: modelData.db
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: parseInt(modelData.db) > -5 ? accentGreen :
                                               parseInt(modelData.db) > -15 ? secondaryCyan :
                                               parseInt(modelData.db) > -20 ? textSecondary : "#888"
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.bandDbWidth
                                    }

                                    Text {
                                        text: modelData.dt
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: textSecondary
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.bandDtWidth
                                    }

                                    Text {
                                        text: modelData.freq
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: isAtRxFrequency(modelData.freq, modelData) ? primaryBlue : secondaryCyan
                                        font.bold: isAtRxFrequency(modelData.freq, modelData)
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.bandFreqWidth
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.bandGapWidth }

                                    // Messaggio con coloring Shannon-compatible
                                    Text {
                                        id: bandMsgText
                                        text: modelData.message
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        font.bold: modelData.isTx || modelData.isCQ || modelData.isMyCall ||
                                                   modelData.dxIsNewCountry || modelData.dxIsMostWanted
                                        // Shannon strikethrough per B4
                                        font.strikeout: modelData.isB4 && bridge.b4Strikethrough
                                        color: getDxccColor(modelData)
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: decodeWindow.bandMessageMinWidth
                                        elide: Text.ElideRight
                                    }

                                    Item {
                                        Layout.preferredWidth: decodeWindow.bandDxccWidth
                                        Layout.fillHeight: true
                                        Text {
                                            anchors.fill: parent
                                            text: modelData.dxCountry || ""
                                            font.family: "Monospace"
                                            font.pixelSize: 11
                                            color: modelData.dxIsNewCountry ? colorNewCountry :
                                                   modelData.dxIsMostWanted ? colorMostWanted : textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Item {
                                        Layout.preferredWidth: decodeWindow.bandAzWidth
                                        Layout.fillHeight: true
                                        Text {
                                            anchors.fill: parent
                                            text: formatBearingDegrees(modelData.dxBearing)
                                            font.family: "Monospace"
                                            font.pixelSize: 11
                                            color: secondaryCyan
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }

                        // Empty state
                        Text {
                            anchors.centerIn: parent
                            text: "No decoded messages\nClick Monitor to start"
                            font.pixelSize: 12
                            color: textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            visible: appEngine.decodeList.length === 0
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
                                text: decodeWindow.compactRxHeader ? "RX Freq" : "RX Frequency"
                                font.pixelSize: decodeWindow.compactRxHeader ? 12 : 13
                                font.bold: true
                                color: primaryBlue
                            }

                            Rectangle {
                                Layout.preferredWidth: decodeWindow.rxHeaderBadgeWidth
                                Layout.preferredHeight: 22
                                color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                                radius: 4
                                border.color: primaryBlue

                                Text {
                                    anchors.centerIn: parent
                                    text: appEngine.rxFrequency + " Hz"
                                    font.family: "Monospace"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: primaryBlue
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // RX Frequency count
                            Text {
                                text: {
                                    void(decodeWindow.decodeListVersion)
                                    return currentRxDecodes().length + " msgs"
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
                                onClicked: appEngine.clearRxDecodes()

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
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                Layout.preferredWidth: decodeWindow.rxUtcWidth
                            }
                            Text {
                                text: "dB"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.rxDbWidth
                            }
                            Text {
                                text: "DT"
                                font.family: "Monospace"
                                font.pixelSize: 10
                                font.bold: true
                                color: primaryBlue
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: decodeWindow.rxDtWidth
                            }
                            Item { Layout.preferredWidth: decodeWindow.rxGapWidth }
                            Text {
                                text: "Message"
                                font.family: "Monospace"
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
                            property int _ver: decodeWindow.decodeListVersion

                            // Filter model to only show messages at RX frequency
                            model: {
                                void(_ver)
                                return currentRxDecodes()
                            }

                            ScrollBar.vertical: ScrollBar {
                                active: true
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                width: rxFrequencyList.width - 12
                                height: 24
                                color: modelData.isMyCall ?
                                       Qt.rgba(244/255, 67/255, 54/255, 0.3) :
                                       modelData.isCQ ?
                                       Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15) :
                                       index % 2 === 0 ?
                                       Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.05) :
                                       Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.1)
                                radius: 2

                                MouseArea {
                                    id: rxDelegateMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onDoubleClicked: {
                                        // Set TX and RX frequency to decode frequency
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
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 0

                                    Text {
                                        text: formatUtcForDisplay(modelData.time)
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: textSecondary
                                        Layout.preferredWidth: decodeWindow.rxUtcWidth
                                    }

                                    Text {
                                        text: modelData.db
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: parseInt(modelData.db) > -5 ? accentGreen :
                                               parseInt(modelData.db) > -15 ? secondaryCyan :
                                               parseInt(modelData.db) > -20 ? textSecondary : "#888"
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.rxDbWidth
                                    }

                                    Text {
                                        text: modelData.dt
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        color: textSecondary
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: decodeWindow.rxDtWidth
                                    }

                                    Item { Layout.preferredWidth: decodeWindow.rxGapWidth }

                                    // Messaggio RX con coloring + strikethrough B4
                                    Text {
                                        id: rxMsgText
                                        text: modelData.message
                                        font.family: "Monospace"
                                        font.pixelSize: 11
                                        font.bold: modelData.isTx || modelData.isCQ || modelData.isMyCall ||
                                                   modelData.dxIsNewCountry || modelData.dxIsMostWanted
                                        font.strikeout: modelData.isB4 && bridge.b4Strikethrough
                                        color: getDxccColor(modelData)
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    // Distanza
                                    Text {
                                        visible: modelData.dxDistance > 0 && !decodeWindow.compactRxColumns
                                        text: modelData.dxDistance > 0 ?
                                              Math.round(modelData.dxDistance) + "km" : ""
                                        font.pixelSize: 9
                                        color: "#666688"
                                        Layout.preferredWidth: decodeWindow.rxDistanceWidth
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }

                        // Empty state for RX Frequency
                        Text {
                            anchors.centerIn: parent
                            text: "No messages at " + appEngine.rxFrequency + " Hz\nClick on Band Activity to select frequency"
                            font.pixelSize: 12
                            color: textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            visible: rxFrequencyList.count === 0
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
        // Look for 4-character grid square
        var gridRegex = /\b([A-R]{2}[0-9]{2})\b/i
        var match = message.match(gridRegex)
        return match ? match[1].toUpperCase() : ""
    }
}
