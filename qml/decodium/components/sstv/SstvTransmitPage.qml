pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

SstvPage {
    id: root
    required property var engine
    readonly property var studio: root.engine ? root.engine.sstvStudio : null
    readonly property var txDiagnostics: root.engine
                                                 ? root.engine.sstvTxDiagnostics
                                                 : ({})
    readonly property bool compactLayout: width < 1200

    function overlay(kind, text) {
        return {
            "kind": kind,
            "text": text ? String(text).slice(0, 512) : "",
            "anchor": overlayAnchor.currentValue,
            "fontPixelSize": overlayFontSize.value,
            "margin": overlayMargin.value,
            "padding": 3,
            "foreground": "#ffffff",
            "background": "#99000000"
        }
    }

    function collectControls() {
        let overlays = []
        if (callsignOverlay.checked && root.engine.callsign)
            overlays.push(root.overlay("callsign", root.engine.callsign))
        if (locatorOverlay.checked && root.engine.grid)
            overlays.push(root.overlay("locator", root.engine.grid))
        if (utcOverlay.checked)
            overlays.push(root.overlay("utc", new Date().toISOString()))
        if (frequencyOverlay.checked && root.engine.frequency)
            overlays.push(root.overlay("frequency", root.engine.frequency + " Hz"))
        if (modeOverlay.checked && root.studio)
            overlays.push(root.overlay("mode", root.studio.modeName))
        if (customOverlay.checked && customOverlayText.text)
            overlays.push(root.overlay("custom", customOverlayText.text))
        if (reportOverlay.checked && reportOverlayText.text)
            overlays.push(root.overlay("report", reportOverlayText.text))
        if (watermarkOverlay.checked && watermarkOverlayText.text)
            overlays.push(root.overlay("watermark", watermarkOverlayText.text))
        return {
            "resizeMode": resizeMode.currentValue,
            "rotation": rotation.currentValue,
            "flipHorizontal": flipHorizontal.checked,
            "flipVertical": flipVertical.checked,
            "aspectLock": aspectLock.checked,
            "cropX": cropX.value / 100.0,
            "cropY": cropY.value / 100.0,
            "cropWidth": Math.min(cropWidth.value, 100 - cropX.value) / 100.0,
            "cropHeight": Math.min(cropHeight.value, 100 - cropY.value) / 100.0,
            "exposure": exposure.value,
            "brightness": brightness.value,
            "contrast": contrast.value,
            "gamma": gamma.value,
            "saturation": saturation.value,
            "whiteBalanceRed": whiteBalanceRed.value,
            "whiteBalanceGreen": whiteBalanceGreen.value,
            "whiteBalanceBlue": whiteBalanceBlue.value,
            "sharpness": sharpness.value,
            "grayscale": grayscale.checked,
            "dither": dither.checked,
            "background": backgroundColour.text,
            "borderWidth": borderEnabled.checked ? 2 : 0,
            "borderColor": "#ffffff",
            "overlays": overlays
        }
    }

    function preparePreview() {
        if (root.studio)
            root.studio.prepareImage(root.collectControls())
    }

    function selectComboValue(combo, value) {
        for (let index = 0; index < combo.count; ++index) {
            if (combo.valueAt(index) === value) {
                combo.currentIndex = index
                return
            }
        }
    }

    function applyTemplate() {
        if (!root.studio || !templateSelector.currentValue)
            return
        const definition = root.studio.templateDefinition(
                    templateSelector.currentValue)
        if (!definition || !definition.controls)
            return
        const values = definition.controls
        if (definition.modeId)
            root.studio.modeId = definition.modeId
        root.selectComboValue(resizeMode, values.resizeMode || "fit")
        root.selectComboValue(rotation, values.rotation || 0)
        flipHorizontal.checked = !!values.flipHorizontal
        flipVertical.checked = !!values.flipVertical
        aspectLock.checked = !!values.aspectLock
        cropX.value = Math.round((values.cropX || 0) * 100)
        cropY.value = Math.round((values.cropY || 0) * 100)
        cropWidth.value = Math.round((values.cropWidth || 1) * 100)
        cropHeight.value = Math.round((values.cropHeight || 1) * 100)
        exposure.value = values.exposure === undefined ? 0 : values.exposure
        brightness.value = values.brightness === undefined ? 0 : values.brightness
        contrast.value = values.contrast === undefined ? 1 : values.contrast
        gamma.value = values.gamma === undefined ? 1 : values.gamma
        saturation.value = values.saturation === undefined ? 1 : values.saturation
        whiteBalanceRed.value = values.whiteBalanceRed === undefined ? 1 : values.whiteBalanceRed
        whiteBalanceGreen.value = values.whiteBalanceGreen === undefined ? 1 : values.whiteBalanceGreen
        whiteBalanceBlue.value = values.whiteBalanceBlue === undefined ? 1 : values.whiteBalanceBlue
        sharpness.value = values.sharpness === undefined ? 0 : values.sharpness
        grayscale.checked = !!values.grayscale
        dither.checked = !!values.dither
        backgroundColour.text = values.background || "#000000"
        borderEnabled.checked = (values.borderWidth || 0) > 0

        callsignOverlay.checked = false
        locatorOverlay.checked = false
        utcOverlay.checked = false
        frequencyOverlay.checked = false
        modeOverlay.checked = false
        customOverlay.checked = false
        reportOverlay.checked = false
        watermarkOverlay.checked = false
        const savedOverlays = values.overlays || []
        for (let index = 0; index < savedOverlays.length; ++index) {
            const item = savedOverlays[index]
            if (item.kind === "callsign") callsignOverlay.checked = true
            else if (item.kind === "locator") locatorOverlay.checked = true
            else if (item.kind === "utc") utcOverlay.checked = true
            else if (item.kind === "frequency") frequencyOverlay.checked = true
            else if (item.kind === "mode") modeOverlay.checked = true
            else if (item.kind === "custom") {
                customOverlay.checked = true
                customOverlayText.text = item.text || ""
            } else if (item.kind === "report") {
                reportOverlay.checked = true
                reportOverlayText.text = item.text || ""
            } else if (item.kind === "watermark") {
                watermarkOverlay.checked = true
                watermarkOverlayText.text = item.text || ""
            }
            if (index === 0) {
                root.selectComboValue(overlayAnchor, item.anchor || "bottom-right")
                overlayFontSize.value = item.fontPixelSize || 18
                overlayMargin.value = item.margin === undefined ? 8 : item.margin
            }
        }
    }

    FileDialog {
        id: imagePicker
        title: qsTr("Load source image")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)"),
            qsTr("All files (*)")
        ]
        onAccepted: {
            if (root.studio)
                root.studio.loadSource(selectedFile)
        }
    }

    FileDialog {
        id: wavExporter
        title: qsTr("Export native SSTV audio")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        currentFolder: root.studio ? root.studio.wavExportFolder : ""
        nameFilters: [qsTr("WAV audio (*.wav)")]
        onAccepted: {
            if (root.studio) {
                root.studio.exportWav(
                            selectedFile,
                            wavSampleRate.currentValue,
                            wavMetadata.checked,
                            true,
                            fskIdEnabled.checked && root.engine.callsign
                            ? root.engine.callsign : "")
            }
        }
    }

    function openWavExporter() {
        if (!root.studio)
            return
        const suggestion = root.studio.suggestedWavUrl(
                    root.engine && root.engine.callsign
                    ? root.engine.callsign : "")
        if (suggestion && suggestion.toString().length > 0)
            wavExporter.selectedFile = suggestion
        wavExporter.open()
    }

    ScrollView {
        id: studioScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: studioScroll.availableWidth
            spacing: 9

            RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("TRANSMIT STUDIO")
                color: root.accentColor
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 1.2
            }
            Item { Layout.fillWidth: true }
            BusyIndicator {
                running: !!(root.studio && root.studio.busy)
                visible: running
                implicitWidth: 24
                implicitHeight: 24
            }
            Label {
                text: root.studio && root.studio.busy
                      ? qsTr("Working…") : qsTr("Ready")
                color: root.studio && root.studio.busy
                       ? root.accentColor : root.secondaryTextColor
                font.pixelSize: 10
            }
        }

            Item {
            id: previewArea
            Layout.fillWidth: true
            Layout.preferredHeight: 180

            Rectangle {
                x: 0
                y: 0
                width: Math.max(0, (previewArea.width - 10) / 2)
                height: previewArea.height
                color: "#05090d"
                border.color: root.borderColor
                border.width: 1
                radius: 8
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 8
                    source: root.studio ? root.studio.sourceImageSource : ""
                    asynchronous: true
                    cache: false
                    fillMode: Image.PreserveAspectFit
                    visible: root.studio && root.studio.sourceReady
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 5
                    visible: !root.studio || !root.studio.sourceReady
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("SOURCE IMAGE")
                        color: root.secondaryTextColor
                        font.bold: true
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Load, paste or drop an image here")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                    }
                }
                Label {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                    text: root.studio && root.studio.sourceName
                          ? root.studio.sourceName : ""
                    color: root.primaryTextColor
                    font.pixelSize: 10
                    padding: 4
                    background: Rectangle { color: "#aa05090d"; radius: 3 }
                }
                DropArea {
                    anchors.fill: parent
                    onDropped: function(drop) {
                        if (root.studio && drop.hasUrls && drop.urls.length > 0) {
                            root.studio.loadSource(drop.urls[0])
                            drop.acceptProposedAction()
                        }
                    }
                }
            }

            Rectangle {
                x: Math.max(0, (previewArea.width + 10) / 2)
                y: 0
                width: Math.max(0, (previewArea.width - 10) / 2)
                height: previewArea.height
                color: "#05090d"
                border.color: root.studio && root.studio.preparedReady
                              ? root.accentColor : root.borderColor
                border.width: 1
                radius: 8
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 8
                    source: root.studio ? root.studio.preparedImageSource : ""
                    asynchronous: true
                    cache: false
                    fillMode: Image.PreserveAspectFit
                    visible: root.studio && root.studio.preparedReady
                             && !root.studio.loopbackReady
                }
                Image {
                    anchors.fill: parent
                    anchors.margins: 8
                    source: root.studio ? root.studio.loopbackImageSource : ""
                    asynchronous: true
                    cache: false
                    fillMode: Image.PreserveAspectFit
                    visible: root.studio && root.studio.loopbackReady
                }
                Label {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 8
                    visible: root.studio && root.studio.loopbackReady
                    text: qsTr("LOOPBACK DECODE")
                    color: root.accentColor
                    font.pixelSize: 9
                    font.bold: true
                    padding: 3
                    background: Rectangle { color: "#cc05090d"; radius: 3 }
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 5
                    visible: !root.studio || !root.studio.preparedReady
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("EXACT TX PREVIEW")
                        color: root.secondaryTextColor
                        font.bold: true
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.studio
                              ? qsTr("%1 × %2 pixels")
                                    .arg(root.studio.outputSize.width)
                                    .arg(root.studio.outputSize.height)
                              : qsTr("Unavailable")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                    }
                }
            }
        }

            ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 4 : 8
                columnSpacing: 7
                rowSpacing: 5
                Label { text: qsTr("Mode"); color: root.secondaryTextColor }
                ComboBox {
                    id: modeSelector
                    objectName: "sstvModeSelector"
                    Layout.preferredWidth: 145
                    model: root.studio ? root.studio.modes : []
                    textRole: "name"
                    valueRole: "id"
                    enabled: !!(root.studio && !root.studio.busy)
                    onActivated: {
                        if (root.studio)
                            root.studio.modeId = currentValue
                    }
                }
                Label {
                    text: root.studio
                          ? qsTr("%1 × %2 · %3 s")
                                .arg(root.studio.outputSize.width)
                                .arg(root.studio.outputSize.height)
                                .arg(root.studio.estimatedDurationSeconds.toFixed(1))
                          : ""
                    color: root.secondaryTextColor
                    font.pixelSize: 10
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Load")
                    enabled: !!(root.studio && !root.studio.busy)
                    onClicked: imagePicker.open()
                }
                Button {
                    text: qsTr("Paste")
                    enabled: !!(root.studio && !root.studio.busy)
                    onClicked: root.studio.pasteSource()
                }
                Button {
                    text: qsTr("Pattern")
                    enabled: !!(root.studio && !root.studio.busy)
                    onClicked: root.studio.generateCalibrationPattern()
                }
                Button {
                    text: qsTr("Clear")
                    enabled: !!(root.studio && root.studio.sourceReady)
                    onClicked: root.studio.clearSource()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 4 : 9
                columnSpacing: 7
                rowSpacing: 5
                Label { text: qsTr("Geometry"); color: root.secondaryTextColor }
                ComboBox {
                    id: resizeMode
                    Layout.preferredWidth: 110
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        {"text": qsTr("Fit"), "value": "fit"},
                        {"text": qsTr("Fill + crop"), "value": "fill"},
                        {"text": qsTr("Stretch ⚠"), "value": "stretch"}
                    ]
                }
                ComboBox {
                    id: rotation
                    Layout.preferredWidth: 85
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        {"text": qsTr("0°"), "value": 0},
                        {"text": qsTr("90°"), "value": 90},
                        {"text": qsTr("180°"), "value": 180},
                        {"text": qsTr("270°"), "value": 270}
                    ]
                }
                CheckBox {
                    id: aspectLock
                    text: qsTr("Aspect lock")
                    checked: true
                    palette.windowText: root.primaryTextColor
                }
                CheckBox {
                    id: flipHorizontal
                    text: qsTr("Flip H")
                    palette.windowText: root.primaryTextColor
                }
                CheckBox {
                    id: flipVertical
                    text: qsTr("Flip V")
                    palette.windowText: root.primaryTextColor
                }
                CheckBox {
                    id: grayscale
                    text: qsTr("Grayscale")
                    palette.windowText: root.primaryTextColor
                }
                CheckBox {
                    id: dither
                    text: qsTr("Dither")
                    enabled: grayscale.checked
                    palette.windowText: enabled
                                        ? root.primaryTextColor
                                        : root.secondaryTextColor
                    onEnabledChanged: if (!enabled) checked = false
                }
                Item { Layout.fillWidth: true }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 5 : 10
                columnSpacing: 7
                rowSpacing: 5
                Label { text: qsTr("Crop %"); color: root.secondaryTextColor; Layout.preferredWidth: 62 }
                Label { text: qsTr("X"); color: root.secondaryTextColor }
                SpinBox { id: cropX; objectName: "sstvCropX"; from: 0; to: 99; value: 0; editable: true }
                Label { text: qsTr("Y"); color: root.secondaryTextColor }
                SpinBox { id: cropY; objectName: "sstvCropY"; from: 0; to: 99; value: 0; editable: true }
                Label { text: qsTr("Width"); color: root.secondaryTextColor }
                SpinBox { id: cropWidth; objectName: "sstvCropWidth"; from: 1; to: 100; value: 100; editable: true }
                Label { text: qsTr("Height"); color: root.secondaryTextColor }
                SpinBox { id: cropHeight; objectName: "sstvCropHeight"; from: 1; to: 100; value: 100; editable: true }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Normalised source crop; width/height are clamped at the image edge")
                    color: root.secondaryTextColor
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 2 : 5
                columnSpacing: 9
                rowSpacing: 5
                Label {
                    text: qsTr("Adjustments")
                    color: root.secondaryTextColor
                    Layout.preferredWidth: 62
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label { text: qsTr("Brightness"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: brightness; Layout.fillWidth: true; from: -0.5; to: 0.5; value: 0; stepSize: 0.01 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label { text: qsTr("Contrast"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: contrast; Layout.fillWidth: true; from: 0.25; to: 2.5; value: 1; stepSize: 0.05 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label { text: qsTr("Gamma"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: gamma; Layout.fillWidth: true; from: 0.25; to: 3; value: 1; stepSize: 0.05 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label { text: qsTr("Saturation"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: saturation; Layout.fillWidth: true; from: 0; to: 2.5; value: 1; stepSize: 0.05 }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 2 : 6
                columnSpacing: 9
                rowSpacing: 5
                Label {
                    text: qsTr("Colour/detail")
                    color: root.secondaryTextColor
                    Layout.preferredWidth: 62
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { text: qsTr("Exposure"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: exposure; objectName: "sstvExposure"; Layout.fillWidth: true; from: -4; to: 4; value: 0; stepSize: 0.05 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { text: qsTr("WB red"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: whiteBalanceRed; objectName: "sstvWhiteBalanceRed"; Layout.fillWidth: true; from: 0; to: 4; value: 1; stepSize: 0.05 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { text: qsTr("WB green"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: whiteBalanceGreen; objectName: "sstvWhiteBalanceGreen"; Layout.fillWidth: true; from: 0; to: 4; value: 1; stepSize: 0.05 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { text: qsTr("WB blue"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: whiteBalanceBlue; objectName: "sstvWhiteBalanceBlue"; Layout.fillWidth: true; from: 0; to: 4; value: 1; stepSize: 0.05 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { text: qsTr("Sharpness"); color: root.secondaryTextColor; font.pixelSize: 9 }
                    Slider { id: sharpness; objectName: "sstvSharpness"; Layout.fillWidth: true; from: 0; to: 2; value: 0; stepSize: 0.05 }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 5 : 15
                columnSpacing: 7
                rowSpacing: 5
                Label {
                    text: qsTr("Overlay")
                    color: root.secondaryTextColor
                    Layout.preferredWidth: 62
                }
                CheckBox {
                    id: callsignOverlay
                    objectName: "sstvOverlayCallsign"
                    text: qsTr("Callsign")
                    enabled: !!(root.engine && root.engine.callsign)
                    palette.windowText: enabled
                                        ? root.primaryTextColor
                                        : root.secondaryTextColor
                }
                CheckBox { id: locatorOverlay; objectName: "sstvOverlayGrid"; text: qsTr("Grid"); enabled: !!(root.engine && root.engine.grid); palette.windowText: enabled ? root.primaryTextColor : root.secondaryTextColor }
                CheckBox { id: utcOverlay; objectName: "sstvOverlayUtc"; text: qsTr("UTC"); palette.windowText: root.primaryTextColor }
                CheckBox { id: frequencyOverlay; objectName: "sstvOverlayFrequency"; text: qsTr("Frequency"); enabled: !!(root.engine && root.engine.frequency); palette.windowText: enabled ? root.primaryTextColor : root.secondaryTextColor }
                CheckBox { id: modeOverlay; objectName: "sstvOverlayMode"; text: qsTr("Mode"); palette.windowText: root.primaryTextColor }
                CheckBox { id: customOverlay; objectName: "sstvOverlayCustom"; text: qsTr("Custom"); palette.windowText: root.primaryTextColor }
                CheckBox { id: reportOverlay; objectName: "sstvOverlayReport"; text: qsTr("Report"); palette.windowText: root.primaryTextColor }
                CheckBox { id: watermarkOverlay; objectName: "sstvOverlayWatermark"; text: qsTr("Watermark"); palette.windowText: root.primaryTextColor }
                Item { Layout.fillWidth: true }
                ComboBox {
                    id: overlayAnchor
                    objectName: "sstvOverlayAnchor"
                    Layout.preferredWidth: 120
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        {"text": qsTr("Top left"), "value": "top-left"},
                        {"text": qsTr("Top centre"), "value": "top-centre"},
                        {"text": qsTr("Top right"), "value": "top-right"},
                        {"text": qsTr("Centre left"), "value": "centre-left"},
                        {"text": qsTr("Centre"), "value": "centre"},
                        {"text": qsTr("Centre right"), "value": "centre-right"},
                        {"text": qsTr("Bottom left"), "value": "bottom-left"},
                        {"text": qsTr("Bottom centre"), "value": "bottom-centre"},
                        {"text": qsTr("Bottom right"), "value": "bottom-right"}
                    ]
                    currentIndex: 8
                }
                Label { text: qsTr("Font"); color: root.secondaryTextColor }
                SpinBox { id: overlayFontSize; objectName: "sstvOverlayFontSize"; from: 6; to: 96; value: 18; editable: true }
                Label { text: qsTr("Margin"); color: root.secondaryTextColor }
                SpinBox { id: overlayMargin; objectName: "sstvOverlayMargin"; from: 0; to: 64; value: 8; editable: true }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 2 : 4
                columnSpacing: 7
                rowSpacing: 5
                Label { text: qsTr("Overlay text"); color: root.secondaryTextColor; Layout.preferredWidth: 62 }
                TextField { id: customOverlayText; objectName: "sstvCustomOverlayText"; Layout.fillWidth: true; maximumLength: 512; placeholderText: qsTr("Custom message") }
                TextField { id: reportOverlayText; objectName: "sstvReportOverlayText"; Layout.preferredWidth: 130; maximumLength: 512; placeholderText: qsTr("Signal report") }
                TextField { id: watermarkOverlayText; objectName: "sstvWatermarkOverlayText"; Layout.preferredWidth: 150; maximumLength: 512; placeholderText: qsTr("Watermark") }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 4 : 8
                columnSpacing: 7
                rowSpacing: 5
                Label {
                    text: qsTr("Frame/TX")
                    color: root.secondaryTextColor
                    Layout.preferredWidth: 62
                }
                CheckBox {
                    id: borderEnabled
                    text: qsTr("Border")
                    palette.windowText: root.primaryTextColor
                }
                Label { text: qsTr("Background"); color: root.secondaryTextColor }
                TextField {
                    id: backgroundColour
                    text: "#000000"
                    color: root.primaryTextColor
                    maximumLength: 9
                    Layout.preferredWidth: 90
                    Accessible.name: qsTr("Transparency background colour")
                }
                Item { Layout.fillWidth: true }
                CheckBox {
                    id: fskIdEnabled
                    text: qsTr("FSK ID")
                    enabled: !!(root.engine && root.engine.callsign
                                 && !root.engine.sstvTxActive)
                    palette.windowText: enabled
                                        ? root.primaryTextColor
                                        : root.secondaryTextColor
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Append Decodium's callsign as a native FSK identifier")
                }
                Button {
                    text: root.studio && root.studio.busy
                          ? qsTr("Cancel") : qsTr("Preview")
                    enabled: !!(root.studio && (root.studio.busy
                                                || root.studio.sourceReady))
                    onClicked: {
                        if (root.studio.busy)
                            root.studio.cancelWork()
                        else
                            root.preparePreview()
                    }
                }
                Button {
                    id: transmitButton
                    objectName: "sstvTransmitButton"
                    text: root.engine && root.engine.sstvTxActive
                          ? qsTr("Cancel TX") : qsTr("Transmit")
                    enabled: !!(root.engine
                                 && (root.engine.sstvTxActive
                                     || root.engine.sstvTxCanStart))
                    ToolTip.visible: hovered
                    ToolTip.text: root.engine && root.engine.sstvTxActive
                                  ? qsTr("Stop audio and release PTT safely")
                                  : qsTr("Start native TX through Decodium audio and CAT/PTT")
                    onClicked: {
                        if (root.engine.sstvTxActive) {
                            root.engine.cancelSstvTx()
                        } else {
                            root.engine.startSstvTx(
                                        fskIdEnabled.checked
                                        ? root.engine.callsign : "")
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 3 : 6
                columnSpacing: 7
                rowSpacing: 5
                Label { text: qsTr("Templates"); color: root.secondaryTextColor; Layout.preferredWidth: 62 }
                TextField {
                    id: templateName
                    objectName: "sstvTemplateName"
                    Layout.preferredWidth: 150
                    maximumLength: 64
                    placeholderText: qsTr("Named preset")
                }
                ComboBox {
                    id: templateSelector
                    objectName: "sstvTemplateSelector"
                    Layout.fillWidth: true
                    model: root.studio ? root.studio.templates : []
                    textRole: "name"
                    valueRole: "name"
                }
                Button {
                    objectName: "sstvTemplateSave"
                    text: qsTr("Save/update")
                    enabled: !!(root.studio && templateName.text.trim().length > 0)
                    onClicked: root.studio.saveTemplate(
                                   templateName.text, root.collectControls())
                }
                Button {
                    text: qsTr("Apply")
                    enabled: !!templateSelector.currentValue
                    onClicked: root.applyTemplate()
                }
                Button {
                    text: qsTr("Delete")
                    enabled: !!(root.studio && templateSelector.currentValue)
                    onClicked: root.studio.deleteTemplate(
                                   templateSelector.currentValue)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 7
                Label {
                    text: qsTr("WAV preview")
                    color: root.secondaryTextColor
                    Layout.preferredWidth: 72
                }
                ComboBox {
                    id: wavSampleRate
                    Layout.preferredWidth: 105
                    model: root.studio ? root.studio.wavSampleRates : []
                    currentIndex: 2
                    delegate: ItemDelegate {
                        required property var modelData
                        width: wavSampleRate.width
                        text: qsTr("%1 Hz").arg(modelData)
                    }
                }
                CheckBox {
                    id: wavMetadata
                    text: qsTr("JSON metadata")
                    checked: true
                    palette.windowText: root.primaryTextColor
                }
                Label {
                    Layout.fillWidth: true
                    text: root.studio && root.studio.wavExportPath
                          ? qsTr("Saved: %1").arg(root.studio.wavExportPath)
                          : (root.studio ? root.studio.wavExportWarning : "")
                    color: root.studio && root.studio.wavExportWarning
                           ? "#ffb454" : root.secondaryTextColor
                    elide: Text.ElideMiddle
                    font.pixelSize: 10
                }
                Button {
                    text: root.studio && root.studio.wavExportBusy
                          ? qsTr("Cancel WAV") : qsTr("Export WAV")
                    enabled: !!(root.studio
                                 && (root.studio.wavExportBusy
                                     || (root.studio.preparedReady
                                         && !root.studio.busy
                                         && root.engine
                                         && !root.engine.sstvTxActive)))
                    onClicked: {
                        if (root.studio.wavExportBusy)
                            root.studio.cancelWork()
                        else
                            root.openWavExporter()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Render the same native PCM source without keying PTT")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 7
                Label { text: qsTr("Loopback"); color: root.secondaryTextColor; Layout.preferredWidth: 62 }
                Button {
                    id: loopbackButton
                    objectName: "sstvLoopbackButton"
                    text: root.studio && root.studio.loopbackBusy
                          ? qsTr("Cancel loopback") : qsTr("Run TX→RX")
                    enabled: !!(root.studio
                                 && (root.studio.loopbackBusy
                                     || (root.studio.preparedReady
                                         && !root.studio.busy)))
                    onClicked: {
                        if (root.studio.loopbackBusy)
                            root.studio.cancelLoopback()
                        else
                            root.studio.startLoopback()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Encode native PCM and decode it with the real RX runtime; no radio or PTT")
                }
                ProgressBar {
                    Layout.preferredWidth: 150
                    from: 0; to: 1
                    value: root.studio ? root.studio.loopbackProgress : 0
                    visible: !!(root.studio && root.studio.loopbackBusy)
                    Accessible.name: qsTr("SSTV internal loopback progress")
                }
                Label {
                    Layout.fillWidth: true
                    text: root.studio
                          ? (root.studio.loopbackError
                             ? root.studio.loopbackError
                             : qsTr("State: %1 · %2%")
                                 .arg(root.studio.loopbackState)
                                 .arg(Math.round(root.studio.loopbackProgress * 100)))
                          : ""
                    color: root.studio && root.studio.loopbackError
                           ? "#ff6b6b" : root.secondaryTextColor
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Label {
                    visible: !!(root.studio && root.studio.loopbackReady)
                    text: root.studio
                          ? qsTr("%1 chunks · %2 ms")
                              .arg(root.studio.loopbackMetrics.chunks || 0)
                              .arg(root.studio.loopbackMetrics.elapsedMilliseconds || 0)
                          : ""
                    color: root.secondaryTextColor
                    font.pixelSize: 9
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 7
                Label { text: qsTr("Output/cal"); color: root.secondaryTextColor; Layout.preferredWidth: 62 }
                ComboBox {
                    id: calibrationTone
                    objectName: "sstvCalibrationTone"
                    Layout.preferredWidth: 150
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        {"text": qsTr("1200 Hz sync"), "value": "sync-1200"},
                        {"text": qsTr("1500 Hz black"), "value": "black-1500"},
                        {"text": qsTr("1900 Hz leader"), "value": "leader-1900"},
                        {"text": qsTr("2300 Hz white"), "value": "white-2300"}
                    ]
                }
                Button {
                    id: calibrationButton
                    objectName: "sstvCalibrationButton"
                    text: qsTr("Play short reference")
                    enabled: !!(root.engine && !root.engine.sstvTxActive
                                 && !root.engine.pttPending)
                    onClicked: root.engine.startSstvCalibrationTone(
                                   calibrationTone.currentValue)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Uses the normal audio, CAT/PTT, watchdog and cancellation path")
                }
                Label {
                    Layout.fillWidth: true
                    text: root.engine
                          ? qsTr("Audio: %1 · CAT: %2 · output −%3 dB")
                              .arg(root.engine.audioOutputDevice || qsTr("default"))
                              .arg(root.engine.catRigName || qsTr("none"))
                              .arg((root.engine.txOutputLevel / 10.0).toFixed(1))
                          : ""
                    color: root.secondaryTextColor
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
                Label {
                    text: root.engine
                          ? (root.engine.pttConfirmed
                             ? qsTr("PTT confirmed")
                             : (root.engine.pttPending
                                ? qsTr("PTT pending") : qsTr("PTT idle")))
                          : ""
                    color: root.engine && root.engine.pttConfirmed
                           ? "#63d68b" : root.secondaryTextColor
                    font.pixelSize: 10
                }
                Label {
                    text: qsTr("Peak %1 · headroom %2 dB · clips %3")
                          .arg(((root.txDiagnostics.pcmPeak || 0) * 100).toFixed(1) + "%")
                          .arg((root.txDiagnostics.headroomDb === undefined
                                ? -1.0 : root.txDiagnostics.headroomDb).toFixed(1))
                          .arg(root.txDiagnostics.clippedFrames || 0)
                    color: (root.txDiagnostics.clippedFrames || 0) > 0
                           ? "#ff6b6b" : root.secondaryTextColor
                    font.pixelSize: 10
                }
            }
        }

            RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: !!(root.engine && (root.engine.sstvTxActive
                                        || root.engine.sstvTxError
                                        || root.engine.sstvTxState))
                     || !!(root.studio && (root.studio.error
                                           || root.studio.warnings.length > 0))

            ProgressBar {
                Layout.preferredWidth: 150
                from: 0
                to: 1
                value: root.engine ? root.engine.sstvTxProgress : 0
                visible: !!(root.engine && root.engine.sstvTxActive)
                Accessible.name: qsTr("SSTV transmission progress")
            }
            Label {
                Layout.fillWidth: true
                text: root.engine && root.engine.sstvTxError
                      ? root.engine.sstvTxError
                      : (root.studio && root.studio.error
                         ? root.studio.error
                         : (root.studio && root.studio.warnings.length > 0
                            ? root.studio.warnings.join(" · ")
                            : (root.engine
                               ? qsTr("TX state: %1").arg(root.engine.sstvTxState)
                               : "")))
                color: (root.engine && root.engine.sstvTxError)
                       || (root.studio && root.studio.error)
                       ? "#ff6b6b" : root.secondaryTextColor
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            }
        }
    }
}
