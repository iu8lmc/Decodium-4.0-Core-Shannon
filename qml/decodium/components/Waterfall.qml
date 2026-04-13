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
    property bool useAether: bridge.getSetting("useAetherWaterfall", true)
    property int  minFreq: 200
    property int  maxFreq: 3000
    property int  spectrumHeight: 150

    // Altezza minima/massima del grafico spettro (regolabile tramite drag)
    readonly property int spectrumMinHeight: 60
    readonly property int spectrumMaxHeight: 500

    // Colors
    property color bgDeep:      bridge.themeManager.bgDeep
    property color bgPanel:     Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
    property color accentCyan:  bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSec:     Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
    property color borderColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)

    // Carica impostazioni panadapter da bridge (salvate via bridge.saveSettings)
    Component.onCompleted: {
        if (bridge.uiSpectrumHeight > 0) waterfallPanel.spectrumHeight = bridge.uiSpectrumHeight
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Toolbar controls ──────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: showControls ? 28 : 0
            visible: showControls
            color: bgPanel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 6

                // Palette
                Text { text: "Palette:"; color: textSec; font.pixelSize: 11 }
                ComboBox {
                    id: paletteCombo
                    Layout.preferredWidth: 106
                    model: waterfallDisplay.paletteNames
                    currentIndex: Math.max(0, bridge.uiPaletteIndex)
                    font.pixelSize: 11
                    onActivated: {
                        waterfallDisplay.paletteIndex = currentIndex
                        bridge.uiPaletteIndex = currentIndex
                        mainWindow.scheduleSave()
                    }
                    background: Rectangle { color: Qt.rgba(30/255,45/255,70/255,0.9); border.color: borderColor; radius: 2 }
                    contentItem: Text { text: paletteCombo.displayText; font.pixelSize: 11; color: textPrimary; verticalAlignment: Text.AlignVCenter; leftPadding: 4 }
                }

                // Auto Range
                CheckBox {
                    id: autoRangeCheck
                    checked: !(bridge && bridge.legacyBackendActive)
                    onCheckedChanged: waterfallDisplay.autoRange = checked
                    ToolTip.text: "Auto noise floor (IIR)"
                    ToolTip.visible: autoRangeCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 18; implicitHeight: 18; radius: 2
                        color: autoRangeCheck.checked ? "#00e676" : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: "#00e676"; border.width: 1
                        Text { anchors.centerIn: parent; text: "A"; color: "black"; font.pixelSize: 9; font.bold: true; visible: autoRangeCheck.checked }
                    }
                }
                Text { text: "Auto"; color: autoRangeCheck.checked ? "#00e676" : textSec; font.pixelSize: 11 }

                // Blanker toggle (AetherSDR-style)
                Text { text: "NB"; color: blankerCheck.checked ? "#ff9800" : textSec; font.pixelSize: 11; font.bold: true }
                CheckBox {
                    id: blankerCheck
                    checked: false
                    onCheckedChanged: waterfallDisplay.blankerEnabled = checked
                    ToolTip.text: "Waterfall Noise Blanker (AetherSDR)"
                    ToolTip.visible: blankerCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 18; implicitHeight: 18; radius: 2
                        color: blankerCheck.checked ? "#ff9800" : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: "#ff9800"; border.width: 1
                        Text { anchors.centerIn: parent; text: "B"; color: "black"; font.pixelSize: 9; font.bold: true; visible: blankerCheck.checked }
                    }
                }

                // AetherSDR toggle
                Rectangle {
                    width: aetherToggleText.width + 10; height: 18; radius: 3
                    color: waterfallPanel.useAether ? Qt.rgba(0,0.8,1,0.25) : Qt.rgba(0.3,0.3,0.3,0.2)
                    border.color: waterfallPanel.useAether ? "#00ccff" : "#555"
                    Text { id: aetherToggleText; anchors.centerIn: parent; text: "AetherSDR"; color: waterfallPanel.useAether ? "#00ccff" : textSec; font.pixelSize: 10; font.bold: true }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: { waterfallPanel.useAether = !waterfallPanel.useAether; bridge.setSetting("useAetherWaterfall", waterfallPanel.useAether) }
                    }
                    ToolTip.visible: hovered; ToolTip.text: "Alterna waterfall AetherSDR / SmartSDR"
                }

                // TX brackets toggle
                Text { text: "[ ]"; color: txBracketsCheck.checked ? accentGreen : textSec; font.pixelSize: 11; font.bold: true }
                CheckBox {
                    id: txBracketsCheck
                    checked: true
                    onCheckedChanged: waterfallDisplay.showTxBrackets = checked
                    indicator: Rectangle {
                        implicitWidth: 16; implicitHeight: 16; radius: 2
                        color: txBracketsCheck.checked ? accentGreen : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: accentGreen; border.width: 1
                        Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: txBracketsCheck.checked }
                    }
                }

                Item { Layout.fillWidth: true }

                // Peak Hold toggle
                CheckBox {
                    id: peakHoldCheck
                    checked: true
                    onCheckedChanged: waterfallDisplay.peakHold = checked
                    ToolTip.text: "Peak Hold"
                    ToolTip.visible: peakHoldCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 18; implicitHeight: 18; radius: 2
                        color: peakHoldCheck.checked ? "#ffcc00" : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: "#ffcc00"; border.width: 1
                        Text { anchors.centerIn: parent; text: "P"; color: "#262626"; font.pixelSize: 9; font.bold: true; visible: peakHoldCheck.checked }
                    }
                }
                Text { text: "Peak"; color: peakHoldCheck.checked ? "#ffcc00" : textSec; font.pixelSize: 11 }

                // Zoom
                Text { text: "Zoom"; color: textSec; font.pixelSize: 11 }
                Slider {
                    id: zoomSlider
                    Layout.preferredWidth: 70
                    from: 1; to: 8; value: bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0; stepSize: 0.5
                    onMoved: waterfallDisplay.zoomFactor = value
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
                Text { text: zoomSlider.value.toFixed(1)+"×"; color: accentCyan; font.pixelSize: 11; width: 28 }

                // dBm live noise floor
                Text {
                    text: waterfallDisplay.measuredFloor.toFixed(0) + "dBm"
                    color: "#00e676"
                    font.pixelSize: 11
                    ToolTip.text: "Noise floor misurato"
                    ToolTip.visible: nfLabel.containsMouse
                    ToolTip.delay: 400
                    MouseArea { id: nfLabel; anchors.fill: parent; hoverEnabled: true }
                }
            }
        }

        // ── Divisore draggabile tra spectrum e waterfall ──────────────────
        // Trascina per regolare l'altezza del grafico spettro
        Rectangle {
            id: spectrumDivider
            Layout.fillWidth: true
            Layout.preferredHeight: 6
            color: dividerMouse.containsMouse || dividerMouse.pressed
                   ? "#00E5FF" : Qt.rgba(1,1,1,0.08)

            // Linea centrale visibile
            Rectangle {
                anchors.centerIn: parent
                width: 40; height: 2; radius: 1
                color: "#00E5FF"; opacity: 0.6
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
                onMouseYChanged: {
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
            }

            Behavior on color { ColorAnimation { duration: 100 } }
        }

        // ── AetherSDR Waterfall ──────────────────────────────────────────
        AetherWaterfallItem {
            id: aetherWf
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: waterfallPanel.useAether

            rxFrequency: bridge.rxFrequency
            txFrequency: bridge.txFrequency
            autoBlack: autoRangeCheck.checked
            colorScheme: Math.max(0, bridge.uiPaletteIndex)

            onFrequencyClicked: function(freq) {
                waterfallPanel.frequencySelected(freq)
            }

            Rectangle {
                anchors.centerIn: parent
                width: startText2.width + 40; height: startText2.height + 20
                color: Qt.rgba(0, 0, 0, 0.85)
                border.color: "#00E5FF"; border.width: 1; radius: 6
                visible: !bridge.monitoring
                Text {
                    id: startText2
                    anchors.centerIn: parent
                    text: "AetherSDR Waterfall\nClicca MONITOR per avviare"
                    font.pixelSize: 12; color: "#B4B4B4"; horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // ── PanadapterItem — FlexRadio SmartSDR style ─────────────────────
        PanadapterItem {
            id: waterfallDisplay
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !waterfallPanel.useAether

            startFreq:      bridge.nfa
            bandwidth:      bridge.nfb - bridge.nfa
            rxFreq:         bridge.rxFrequency
            txFreq:         bridge.txFrequency
            running:        bridge.monitoring
            showTxBrackets: true
            spectrumHeight: waterfallPanel.spectrumHeight
            paletteIndex:   Math.max(0, bridge.uiPaletteIndex)
            autoRange:      autoRangeCheck.checked
            peakHold:       true
            zoomFactor:     bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0

            onPaletteIndexChanged: { bridge.uiPaletteIndex = paletteIndex; mainWindow.scheduleSave() }
            onZoomFactorChanged:   { bridge.uiZoomFactor   = zoomFactor;   mainWindow.scheduleSave() }

            onFrequencySelected: function(freq) {
                waterfallPanel.frequencySelected(freq)
            }
            onTxFrequencySelected: function(freq) {
                waterfallPanel.txFrequencySelected(freq)
            }

            Rectangle {
                anchors.centerIn: parent
                width: startText.width + 40; height: startText.height + 20
                color: Qt.rgba(0, 0, 0, 0.85)
                border.color: "#00E5FF"; border.width: 1; radius: 6
                visible: !bridge.monitoring
                Text {
                    id: startText
                    anchors.centerIn: parent
                    text: "Panadapter 4096-bin · SmartSDR style\nClicca MONITOR per avviare"
                    font.pixelSize: 12; color: "#B4B4B4"; horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // Sync Bridge → Waterfalls
    Connections {
        target: bridge

        function onPanadapterDataReady(dbValues, minDb, maxDb, freqMinHz, freqMaxHz) {
            if (waterfallPanel.useAether)
                aetherWf.addRow(dbValues, minDb, maxDb, freqMinHz, freqMaxHz)
            else
                waterfallDisplay.addSpectrumData(dbValues, minDb, maxDb, freqMinHz, freqMaxHz)
        }
        function onSpectrumDataReady(data) {
            if (!bridge.monitoring) return
            if (!waterfallPanel.useAether)
                waterfallDisplay.addSpectrumDataNorm(data)
        }
        function onRxFrequencyChanged() {
            waterfallDisplay.rxFreq = bridge.rxFrequency
            aetherWf.rxFrequency = bridge.rxFrequency
        }
        function onTxFrequencyChanged() {
            waterfallDisplay.txFreq = bridge.txFrequency
            aetherWf.txFrequency = bridge.txFrequency
        }
    }
}
