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
    property int  minFreq: 200
    property int  maxFreq: 3000
    property int  spectrumHeight: 150
    property bool restoringSettings: false

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

    function applyManualContrast() {
        if (waterfallDisplay && !autoRangeCheck.checked) {
            waterfallDisplay.minDb = waterfallDisplay.measuredFloor - (150 - contrastSlider.value)
        }
    }

    // Colori preset per i callsign sul panadapter
    readonly property var labelColorPresets: [
        { name: "Auto",    color: "#00E6FF", custom: false },
        { name: "Ciano",   color: "#00E6FF", custom: true  },
        { name: "Bianco",  color: "#FFFFFF", custom: true  },
        { name: "Giallo",  color: "#FFE600", custom: true  },
        { name: "Verde",   color: "#00E664", custom: true  },
        { name: "Magenta", color: "#FF64FF", custom: true  },
        { name: "Arancio", color: "#FF9600", custom: true  }
    ]

    function loadPanadapterSettings() {
        restoringSettings = true
        if (bridge.uiSpectrumHeight > 0) waterfallPanel.spectrumHeight = bridge.uiSpectrumHeight
        paletteCombo.currentIndex = Math.max(0, bridge.getSetting("uiPaletteIndex", bridge.uiPaletteIndex))
        autoRangeCheck.checked = bridge.getSetting("uiWaterfallAutoRange", true)
        txBracketsCheck.checked = bridge.getSetting("uiWaterfallShowTxBrackets", true)
        peakHoldCheck.checked = bridge.getSetting("uiWaterfallPeakHold", true)
        blackSlider.value = bridge.getSetting("uiWaterfallBlackLevel", 15)
        gainSlider.value = bridge.getSetting("uiWaterfallColorGain", 50)
        contrastSlider.value = bridge.getSetting("uiWaterfallContrast", 80)
        speedSlider.value = bridge.getSetting("spectrumInterval", 20)
        zoomSlider.value = bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0

        // Callsign overlay
        labelFontSlider.value = bridge.getSetting("uiLabelFontSize", 8)
        labelSpacingSlider.value = bridge.getSetting("uiLabelSpacing", 2)
        labelBoldCheck.checked = bridge.getSetting("uiLabelBold", true)
        labelColorCombo.currentIndex = Math.max(0, Math.min(labelColorPresets.length - 1,
                                           bridge.getSetting("uiLabelColorPreset", 0)))

        waterfallDisplay.paletteIndex = paletteCombo.currentIndex
        waterfallDisplay.autoRange = autoRangeCheck.checked
        waterfallDisplay.showTxBrackets = txBracketsCheck.checked
        waterfallDisplay.peakHold = peakHoldCheck.checked
        waterfallDisplay.blackLevel = blackSlider.value
        waterfallDisplay.colorGain = gainSlider.value
        waterfallDisplay.zoomFactor = zoomSlider.value
        waterfallDisplay.labelFontSize = labelFontSlider.value
        waterfallDisplay.labelSpacing = labelSpacingSlider.value
        waterfallDisplay.labelBold = labelBoldCheck.checked
        var preset = labelColorPresets[labelColorCombo.currentIndex]
        waterfallDisplay.labelUseCustomColor = preset.custom
        waterfallDisplay.labelColor = preset.color
        applyManualContrast()
        restoringSettings = false
    }

    Component.onCompleted: Qt.callLater(loadPanadapterSettings)
    onVisibleChanged: if (visible) Qt.callLater(loadPanadapterSettings)

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
                Text { text: "Palette:"; color: textSec; font.pixelSize: 10 }
                ComboBox {
                    id: paletteCombo
                    Layout.preferredWidth: 106
                    model: waterfallDisplay.paletteNames
                    currentIndex: Math.max(0, bridge.uiPaletteIndex)
                    font.pixelSize: 10
                    onActivated: {
                        waterfallDisplay.paletteIndex = currentIndex
                        bridge.uiPaletteIndex = currentIndex
                        bridge.setSetting("uiPaletteIndex", currentIndex)
                        mainWindow.scheduleSave()
                    }
                    background: Rectangle { color: Qt.rgba(30/255,45/255,70/255,0.9); border.color: borderColor; radius: 2 }
                    contentItem: Text { text: paletteCombo.displayText; font.pixelSize: 10; color: textPrimary; verticalAlignment: Text.AlignVCenter; leftPadding: 4 }
                }

                // Auto Range
                CheckBox {
                    id: autoRangeCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.autoRange = checked
                        if (checked) {
                            waterfallDisplay.maxDb = waterfallDisplay.measuredFloor + 20
                        } else {
                            waterfallPanel.applyManualContrast()
                        }
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiWaterfallAutoRange", checked)
                        }
                    }
                    ToolTip.text: "Auto noise floor (IIR)"
                    ToolTip.visible: autoRangeCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: autoRangeCheck.checked ? "#00e676" : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: "#00e676"; border.width: 1
                        Text { anchors.centerIn: parent; text: "A"; color: "black"; font.pixelSize: 9; font.bold: true; visible: autoRangeCheck.checked }
                    }
                }
                Text { text: "Auto"; color: autoRangeCheck.checked ? "#00e676" : textSec; font.pixelSize: 10 }

                // TX brackets toggle
                Text { text: "[ ]"; color: txBracketsCheck.checked ? accentGreen : textSec; font.pixelSize: 10; font.bold: true }
                CheckBox {
                    id: txBracketsCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.showTxBrackets = checked
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiWaterfallShowTxBrackets", checked)
                        }
                    }
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
                    onCheckedChanged: {
                        waterfallDisplay.peakHold = checked
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiWaterfallPeakHold", checked)
                        }
                    }
                    ToolTip.text: "Peak Hold"
                    ToolTip.visible: peakHoldCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: peakHoldCheck.checked ? "#ffcc00" : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: "#ffcc00"; border.width: 1
                        Text { anchors.centerIn: parent; text: "P"; color: "#262626"; font.pixelSize: 9; font.bold: true; visible: peakHoldCheck.checked }
                    }
                }
                Text { text: "Peak"; color: peakHoldCheck.checked ? "#ffcc00" : textSec; font.pixelSize: 10 }

                // Zoom
                Text { text: "Zoom"; color: textSec; font.pixelSize: 10 }
                Slider {
                    id: zoomSlider
                    Layout.preferredWidth: 70
                    from: 1; to: 8; value: bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0; stepSize: 0.5
                    onValueChanged: {
                        waterfallDisplay.zoomFactor = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.uiZoomFactor = value
                            bridge.setSetting("uiZoomFactor", value)
                            mainWindow.scheduleSave()
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
                    color: "#00e676"
                    font.pixelSize: 10
                    ToolTip.text: "Noise floor misurato"
                    ToolTip.visible: nfLabel.containsMouse
                    ToolTip.delay: 400
                    MouseArea { id: nfLabel; anchors.fill: parent; hoverEnabled: true }
                }
            }
        }

        // ── Slider Black / Gain ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 22
            color: Qt.rgba(0,0,0,0.4); visible: showControls
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 6
                Text { text: "Black:"; color: "#5888AA"; font.pixelSize: 10 }
                Slider { id: blackSlider; Layout.preferredWidth: 80; from: 0; to: 100; value: 15; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.blackLevel = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiWaterfallBlackLevel", value)
                        }
                    }
                    background: Rectangle { x:blackSlider.leftPadding;y:blackSlider.topPadding+blackSlider.availableHeight/2-2;width:blackSlider.availableWidth;height:4;radius:2;color:"#1a2a3a" }
                    handle: Rectangle { x:blackSlider.leftPadding+blackSlider.visualPosition*(blackSlider.availableWidth-width);y:blackSlider.topPadding+blackSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:"#5888AA" }
                }
                Text { text: blackSlider.value.toFixed(0); color: "#5888AA"; font.pixelSize: 10; width: 20 }
                Rectangle { width:1;height:14;color:"#333" }
                Text { text: "Gain:"; color: "#88BBDD"; font.pixelSize: 10 }
                Slider { id: gainSlider; Layout.preferredWidth: 80; from: 0; to: 100; value: 50; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.colorGain = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiWaterfallColorGain", value)
                        }
                    }
                    background: Rectangle { x:gainSlider.leftPadding;y:gainSlider.topPadding+gainSlider.availableHeight/2-2;width:gainSlider.availableWidth;height:4;radius:2;color:"#1a2a3a" }
                    handle: Rectangle { x:gainSlider.leftPadding+gainSlider.visualPosition*(gainSlider.availableWidth-width);y:gainSlider.topPadding+gainSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:"#88BBDD" }
                }
                Text { text: gainSlider.value.toFixed(0); color: "#88BBDD"; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                Text { text: "Contrasto:"; color: "#AA88DD"; font.pixelSize: 10 }
                Slider { id: contrastSlider; Layout.preferredWidth: 70; from: 10; to: 150; value: 80; stepSize: 1
                    onValueChanged: {
                        waterfallPanel.applyManualContrast()
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiWaterfallContrast", value)
                        }
                    }
                    background: Rectangle { x:contrastSlider.leftPadding;y:contrastSlider.topPadding+contrastSlider.availableHeight/2-2;width:contrastSlider.availableWidth;height:4;radius:2;color:"#1a2a3a" }
                    handle: Rectangle { x:contrastSlider.leftPadding+contrastSlider.visualPosition*(contrastSlider.availableWidth-width);y:contrastSlider.topPadding+contrastSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:"#AA88DD" }
                }
                Text { text: contrastSlider.value.toFixed(0); color: "#AA88DD"; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                Text { text: "Vel:"; color: "#DD8866"; font.pixelSize: 10 }
                Slider { id: speedSlider; Layout.preferredWidth: 60; from: 10; to: 500; value: 20; stepSize: 5
                    onValueChanged: if (!waterfallPanel.restoringSettings) bridge.setSetting("spectrumInterval", value)
                    background: Rectangle { x:speedSlider.leftPadding;y:speedSlider.topPadding+speedSlider.availableHeight/2-2;width:speedSlider.availableWidth;height:4;radius:2;color:"#1a2a3a" }
                    handle: Rectangle { x:speedSlider.leftPadding+speedSlider.visualPosition*(speedSlider.availableWidth-width);y:speedSlider.topPadding+speedSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:"#DD8866" }
                }
                Text { text: speedSlider.value.toFixed(0)+"ms"; color: "#DD8866"; font.pixelSize: 10; width: 32 }

                Item { Layout.fillWidth: true }
            }
        }

        // ── Toolbar Callsign Labels (font/spaziatura/bold/colore) ─────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 22
            color: Qt.rgba(0,0,0,0.35); visible: showControls
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 6

                Text { text: "Calls:"; color: "#00E5FF"; font.pixelSize: 10; font.bold: true }

                // Font size
                Text { text: "Font"; color: textSec; font.pixelSize: 10 }
                Slider {
                    id: labelFontSlider
                    Layout.preferredWidth: 70
                    from: 6; to: 20; value: 8; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.labelFontSize = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiLabelFontSize", value)
                        }
                    }
                    background: Rectangle { x:labelFontSlider.leftPadding;y:labelFontSlider.topPadding+labelFontSlider.availableHeight/2-2;width:labelFontSlider.availableWidth;height:4;radius:2;color:"#1a2a3a"
                        Rectangle { width: labelFontSlider.visualPosition*parent.width; height: parent.height; radius: 2; color: "#00E5FF" }
                    }
                    handle: Rectangle { x:labelFontSlider.leftPadding+labelFontSlider.visualPosition*(labelFontSlider.availableWidth-width);y:labelFontSlider.topPadding+labelFontSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color: labelFontSlider.pressed ? accentGreen : "#00E5FF" }
                }
                Text { text: labelFontSlider.value.toFixed(0)+"px"; color: "#00E5FF"; font.pixelSize: 10; width: 26 }

                Rectangle { width:1;height:14;color:"#333" }

                // Spacing orizzontale
                Text { text: "Gap"; color: textSec; font.pixelSize: 10 }
                Slider {
                    id: labelSpacingSlider
                    Layout.preferredWidth: 60
                    from: 0; to: 20; value: 2; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.labelSpacing = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiLabelSpacing", value)
                        }
                    }
                    background: Rectangle { x:labelSpacingSlider.leftPadding;y:labelSpacingSlider.topPadding+labelSpacingSlider.availableHeight/2-2;width:labelSpacingSlider.availableWidth;height:4;radius:2;color:"#1a2a3a"
                        Rectangle { width: labelSpacingSlider.visualPosition*parent.width; height: parent.height; radius: 2; color: "#88DD88" }
                    }
                    handle: Rectangle { x:labelSpacingSlider.leftPadding+labelSpacingSlider.visualPosition*(labelSpacingSlider.availableWidth-width);y:labelSpacingSlider.topPadding+labelSpacingSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color: labelSpacingSlider.pressed ? accentGreen : "#88DD88" }
                }
                Text { text: labelSpacingSlider.value.toFixed(0); color: "#88DD88"; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                // Bold
                CheckBox {
                    id: labelBoldCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.labelBold = checked
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiLabelBold", checked)
                        }
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: labelBoldCheck.checked ? "#FFFFFF" : Qt.rgba(30/255,45/255,70/255,0.9)
                        border.color: "#FFFFFF"; border.width: 1
                        Text { anchors.centerIn: parent; text: "B"; color: "black"; font.pixelSize: 9; font.bold: true; visible: labelBoldCheck.checked }
                    }
                }
                Text { text: "Bold"; color: labelBoldCheck.checked ? "#FFFFFF" : textSec; font.pixelSize: 10 }

                Rectangle { width:1;height:14;color:"#333" }

                // Color preset
                Text { text: "Colore"; color: textSec; font.pixelSize: 10 }
                ComboBox {
                    id: labelColorCombo
                    Layout.preferredWidth: 86
                    font.pixelSize: 10
                    model: waterfallPanel.labelColorPresets.map(function(p){ return p.name })
                    currentIndex: 0
                    onActivated: {
                        var preset = waterfallPanel.labelColorPresets[currentIndex]
                        waterfallDisplay.labelUseCustomColor = preset.custom
                        waterfallDisplay.labelColor = preset.color
                        if (!waterfallPanel.restoringSettings) {
                            bridge.setSetting("uiLabelColorPreset", currentIndex)
                        }
                    }
                    background: Rectangle { color: Qt.rgba(30/255,45/255,70/255,0.9); border.color: borderColor; radius: 2 }
                    contentItem: Row {
                        spacing: 4
                        leftPadding: 4
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10; height: 10; radius: 2
                            color: waterfallPanel.labelColorPresets[labelColorCombo.currentIndex].color
                            border.color: "#555"; border.width: 1
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: labelColorCombo.displayText
                            font.pixelSize: 10; color: textPrimary
                        }
                    }
                }

                Item { Layout.fillWidth: true }
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

        // ── PanadapterItem — FlexRadio SmartSDR style ─────────────────────
        PanadapterItem {
            id: waterfallDisplay
            Layout.fillWidth: true
            Layout.fillHeight: true

            startFreq:      bridge.nfa
            bandwidth:      bridge.nfb - bridge.nfa
            rxFreq:         bridge.rxFrequency
            txFreq:         bridge.txFrequency
            running:        bridge.monitoring
            showTxBrackets: true
            spectrumHeight: waterfallPanel.spectrumHeight
            // Carica valori da Settings al primo avvio
            paletteIndex:   Math.max(0, bridge.uiPaletteIndex)
            autoRange:      autoRangeCheck.checked
            peakHold:       true
            zoomFactor:     bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0

            // Salva al bridge con debounce (2s dopo l'ultimo cambio)
            onPaletteIndexChanged: if (!waterfallPanel.restoringSettings) {
                bridge.uiPaletteIndex = paletteIndex
                bridge.setSetting("uiPaletteIndex", paletteIndex)
                mainWindow.scheduleSave()
            }
            onZoomFactorChanged: if (!waterfallPanel.restoringSettings) {
                bridge.uiZoomFactor = zoomFactor
                bridge.setSetting("uiZoomFactor", zoomFactor)
                mainWindow.scheduleSave()
            }
            onMeasuredFloorChanged: waterfallPanel.applyManualContrast()

            onFrequencySelected: function(freq) {
                waterfallPanel.frequencySelected(freq)        // RX
            }
            onTxFrequencySelected: function(freq) {
                waterfallPanel.txFrequencySelected(freq)      // TX
            }

            // Overlay "Start monitoring"
            Rectangle {
                anchors.centerIn: parent
                width: startText.width + 40; height: startText.height + 20
                color: Qt.rgba(0, 0, 0, 0.85)
                border.color: "#00E5FF"; border.width: 1; radius: 6
                visible: !bridge.monitoring && !bridge.transmitting && !bridge.tuning
                Text {
                    id: startText
                    anchors.centerIn: parent
                    text: "Panadapter 4096-bin · SmartSDR style\nClicca MONITOR per avviare"
                    font.pixelSize: 12
                    color: "#B4B4B4"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // Sync Bridge → PanadapterItem
    Connections {
        target: bridge

        // Alta risoluzione FFTW 4096 bin — include range frequenze esatto
        function onPanadapterDataReady(dbValues, minDb, maxDb, freqMinHz, freqMaxHz) {
            waterfallDisplay.addSpectrumData(dbValues, minDb, maxDb, freqMinHz, freqMaxHz)
        }
        // Fallback: valori normalizzati 0-1 dal legacy timer
        function onSpectrumDataReady(data) {
            if (!bridge.monitoring) return
            waterfallDisplay.addSpectrumDataNorm(data)
        }
        function onRxFrequencyChanged() { waterfallDisplay.rxFreq = bridge.rxFrequency }
        function onTxFrequencyChanged() { waterfallDisplay.txFreq = bridge.txFrequency }

        // Aggiorna i callsign decodificati sul grafico spettro
        function onDecodeListChanged() {
            var labels = []
            var seen = {}
            var list = bridge.decodeList
            // Prendi solo gli ultimi decode (ultimo periodo) — max 30
            var start = Math.max(0, list.length - 30)
            for (var i = start; i < list.length; ++i) {
                var d = list[i]
                if (d.isTx) continue
                var call = d.fromCall || ""
                var freq = parseInt(d.freq || "0")
                if (!call || freq <= 0) continue
                if (seen[call]) continue
                seen[call] = true
                labels.push({
                    call: call,
                    freq: freq,
                    snr: parseInt(d.db || "0"),
                    isCQ: d.isCQ || false,
                    isMyCall: d.isMyCall || false
                })
            }
            waterfallDisplay.setDecodeLabels(labels)
        }
    }
}
