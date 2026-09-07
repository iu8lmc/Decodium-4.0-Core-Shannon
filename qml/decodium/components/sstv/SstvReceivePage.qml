pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

SstvPage {
    id: root

    required property var engine
    property string feedback: ""
    property string imageViewMode: "fit"
    property real imageZoom: 1.0

    readonly property var stats: engine && engine.sstvRxDiagnostics
                                 ? engine.sstvRxDiagnostics : ({})
    readonly property var controls: engine && engine.sstvRxControls
                                    ? engine.sstvRxControls : ({})
    readonly property var modeChoices: engine && engine.sstvRxModeChoices
                                       ? engine.sstvRxModeChoices : []
    readonly property bool imageAvailable: !!(stats && stats.imageAvailable
                                               && engine && engine.sstvRxImageSource)
    readonly property real imageCoverage: Math.max(0.0, Math.min(1.0,
                                                   Number(stats && stats.imageCoverage || 0)))
    readonly property bool replayActive: !!(engine && engine.sstvWavReplayActive)
    readonly property real replayProgress: Math.max(0.0, Math.min(1.0,
                                                    Number(engine && engine.sstvWavReplayProgress || 0)))
    readonly property bool audioJobBusy: !!(engine && engine.sstvRxAudioJobBusy)
    readonly property string sourceType: engine && engine.sstvRxSource
                                         ? String(engine.sstvRxSource)
                                         : qsTr("Unavailable")
    readonly property string sourceDevice: engine && engine.sstvRxSourceDevice
                                           ? String(engine.sstvRxSourceDevice) : ""
    readonly property string sourceDisplay: sourceDevice.length > 0
                                            ? qsTr("%1 — %2").arg(sourceType).arg(sourceDevice)
                                            : sourceType

    function applyControl(name, value) {
        if (!root.engine)
            return false
        const update = ({})
        update[name] = value
        return root.applyControls(update)
    }

    function applyControls(update) {
        if (!root.engine)
            return false
        const accepted = root.engine.updateSstvRxControls(update)
        if (!accepted)
            root.feedback = qsTr("Receiver control was rejected")
        return accepted
    }

    function controlIndex(value, values) {
        const index = values.indexOf(String(value || ""))
        return index >= 0 ? index : 0
    }

    function modeIndex(modeId) {
        const wanted = String(modeId || "")
        for (let index = 0; index < root.modeChoices.length; ++index) {
            if (String(root.modeChoices[index].id) === wanted)
                return index
        }
        return root.modeChoices.length > 0 ? 0 : -1
    }

    function selectedModeId(combo) {
        if (!combo || combo.currentIndex < 0
                || combo.currentIndex >= root.modeChoices.length)
            return ""
        return String(root.modeChoices[combo.currentIndex].id || "")
    }

    function textValue(key, fallback) {
        if (!stats || stats[key] === undefined || stats[key] === null || stats[key] === "")
            return fallback
        return String(stats[key])
    }

    function saveStateText(state) {
        switch (state) {
        case "saving": return qsTr("Saving…")
        case "saved": return qsTr("Saved in Gallery")
        case "error": return qsTr("Save failed")
        case "unavailable": return qsTr("Storage unavailable")
        default: return qsTr("Ready to save")
        }
    }

    FileDialog {
        id: wavPicker
        title: qsTr("Replay an SSTV WAV recording")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("WAV audio (*.wav *.wave)"),
            qsTr("All files (*)")
        ]
        onAccepted: {
            if (!root.engine)
                return
            root.feedback = root.engine.startSstvWavReplay(selectedFile)
                    ? qsTr("Native WAV replay started")
                    : qsTr("WAV replay could not start")
        }
    }

    component Metric: Rectangle {
        id: metric
        property string label: ""
        property string value: "--"
        property color valueColor: root.primaryTextColor
        Layout.fillWidth: true
        Layout.preferredHeight: 58
        color: Qt.rgba(0, 0, 0, 0.10)
        border.color: root.borderColor
        border.width: 1
        radius: 7

        Column {
            anchors.fill: parent
            anchors.margins: 9
            spacing: 3
            Label {
                text: metric.label
                color: root.secondaryTextColor
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.8
            }
            Label {
                width: parent.width
                text: metric.value
                color: metric.valueColor
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        text: qsTr("RECEIVE")
                        color: root.accentColor
                        font.pixelSize: 15
                        font.bold: true
                        font.letterSpacing: 1.2
                    }
                    Label {
                        text: qsTr("Streaming decoder on Decodium's active mono PCM path")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                    }
                }

                Button {
                    text: root.engine && root.engine.sstvRxActive ? qsTr("Stop monitor") : qsTr("Start monitor")
                    enabled: !!(root.engine && root.engine.sstvAvailable
                                 && !root.replayActive)
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: text
                    onClicked: {
                        if (!root.engine)
                            return
                        if (root.engine.sstvRxActive) {
                            root.engine.stopSstvRx()
                            root.feedback = qsTr("SSTV reception stopped")
                        } else {
                            root.feedback = root.engine.startSstvRx()
                                    ? qsTr("SSTV reception started")
                                    : qsTr("SSTV reception could not start")
                        }
                    }
                }
                Button {
                    text: qsTr("Reset")
                    enabled: !!(root.engine && root.engine.sstvRxActive
                                 && !root.replayActive)
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: qsTr("Reset SSTV decoder")
                    onClicked: {
                        if (root.engine && root.engine.resetSstvRx())
                            root.feedback = qsTr("Decoder state reset")
                    }
                }
                Button {
                    objectName: "sstvRxAbortFrame"
                    text: qsTr("Abort frame")
                    enabled: !!(root.engine && root.engine.sstvRxActive
                                 && !root.replayActive)
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: qsTr("Abort the current SSTV frame")
                    onClicked: {
                        if (root.engine && root.engine.abortSstvRxFrame())
                            root.feedback = qsTr("Current frame aborted")
                    }
                }
                Button {
                    objectName: "sstvReplayWavOpen"
                    text: qsTr("Replay WAV…")
                    enabled: !!(root.engine && root.engine.sstvAvailable
                                 && !root.replayActive)
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: qsTr("Replay an SSTV WAV file")
                    onClicked: wavPicker.open()
                }
                Button {
                    objectName: "sstvReplayWavCancel"
                    text: qsTr("Cancel replay")
                    visible: root.replayActive
                    enabled: root.replayActive
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: text
                    onClicked: {
                        if (root.engine) {
                            root.engine.cancelSstvWavReplay()
                            root.feedback = qsTr("Cancelling WAV replay")
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Switch {
                    id: autoSaveSwitch
                    objectName: "sstvRxAutoSave"
                    text: qsTr("Auto-save received images")
                    checked: !!(root.engine
                                 && root.engine.sstvRxAutoSaveEnabled)
                    enabled: !!(root.engine && root.engine.sstvStorageReady)
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: text
                    Accessible.description: qsTr("Save each completed or safely recovered partial SSTV image as lossless PNG in the Decodium Gallery")
                    onToggled: {
                        if (root.engine
                                && checked !== root.engine.sstvRxAutoSaveEnabled) {
                            root.engine.sstvRxAutoSaveEnabled = checked
                        }
                    }
                }

                Button {
                    objectName: "sstvRxSaveImage"
                    text: root.stats && root.stats.imageComplete
                          ? qsTr("Save image") : qsTr("Save partial")
                    enabled: !!(root.engine && root.engine.sstvStorageReady
                                 && root.imageAvailable
                                 && root.engine.sstvRxSaveState !== "saving")
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: text
                    Accessible.description: qsTr("Save the current coherent native decoder snapshot in the Decodium Gallery")
                    onClicked: {
                        if (!root.engine)
                            return
                        root.feedback = root.engine.saveSstvRxImage()
                                ? qsTr("SSTV image queued for lossless storage")
                                : (root.engine.sstvRxSaveError
                                   || qsTr("This SSTV snapshot is already saved"))
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: root.engine
                          ? root.saveStateText(root.engine.sstvRxSaveState)
                          : root.saveStateText("unavailable")
                    color: root.engine && root.engine.sstvRxSaveState === "saved"
                           ? root.successColor
                           : root.engine && (root.engine.sstvRxSaveState === "error"
                                             || root.engine.sstvRxSaveState === "unavailable")
                             ? root.warningColor : root.secondaryTextColor
                    font.pixelSize: 10
                    font.bold: root.engine
                               && root.engine.sstvRxSaveState === "saving"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: !!(root.engine
                            && (root.replayActive
                                || root.engine.sstvWavReplayFileName
                                || root.engine.sstvWavReplayError))
                spacing: 8
                Label {
                    Layout.preferredWidth: 170
                    text: root.engine
                          ? qsTr("WAV · %1").arg(root.engine.sstvWavReplayState)
                          : ""
                    color: root.replayActive ? root.accentColor
                                             : root.secondaryTextColor
                    font.pixelSize: 10
                    font.bold: root.replayActive
                    elide: Text.ElideRight
                }
                ProgressBar {
                    objectName: "sstvReplayWavProgress"
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: root.replayProgress
                    Accessible.name: qsTr("SSTV WAV replay progress")
                    Accessible.description: qsTr("%1 percent replayed")
                                                .arg((root.replayProgress * 100).toFixed(0))
                }
                Label {
                    Layout.maximumWidth: 230
                    text: root.engine ? root.engine.sstvWavReplayFileName : ""
                    color: root.secondaryTextColor
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
                Label {
                    text: qsTr("%1%").arg((root.replayProgress * 100).toFixed(0))
                    color: root.primaryTextColor
                    font.pixelSize: 10
                }
            }

            Rectangle {
                objectName: "sstvRxCorrectionControls"
                Layout.fillWidth: true
                Layout.preferredHeight: rxControlLayout.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: rxControlLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Native receiver controls")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: qsTr("AFC %1 Hz · slant %2 ppm")
                                  .arg(Number(root.stats && root.stats.afcCorrectionHz || 0).toFixed(1))
                                  .arg(Number(root.stats && root.stats.slantAppliedPpm || 0).toFixed(1))
                            color: root.secondaryTextColor
                            font.pixelSize: 10
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.width >= 940 ? 6 : 4
                        columnSpacing: 8
                        rowSpacing: 7

                        Label { text: qsTr("Mode"); color: root.secondaryTextColor }
                        ComboBox {
                            id: modeControl
                            objectName: "sstvRxModeControl"
                            Layout.preferredWidth: 135
                            model: [qsTr("Automatic"), qsTr("Manual")]
                            currentIndex: String(root.controls.modeControl || "auto") === "manual" ? 1 : 0
                            Accessible.name: qsTr("SSTV receive mode selection")
                            onActivated: root.applyControl("modeControl",
                                                           currentIndex === 1 ? "manual" : "auto")
                        }
                        Label { text: qsTr("Mode profile"); color: root.secondaryTextColor }
                        ComboBox {
                            id: manualMode
                            objectName: "sstvRxManualMode"
                            Layout.preferredWidth: 180
                            model: root.modeChoices
                            textRole: "name"
                            currentIndex: root.modeIndex(root.controls.modeControl === "manual"
                                                        ? root.controls.manualMode
                                                        : (root.controls.lockedMode
                                                           || root.controls.manualMode))
                            Accessible.name: qsTr("Manual or locked SSTV mode")
                            onActivated: {
                                const selected = root.selectedModeId(manualMode)
                                if (selected.length === 0)
                                    return
                                root.applyControl("manualMode", selected)
                                if (root.controls.modeLockEnabled)
                                    root.applyControl("lockedMode", selected)
                            }
                        }
                        Switch {
                            id: modeLock
                            objectName: "sstvRxModeLock"
                            text: qsTr("Lock")
                            checked: Boolean(root.controls.modeLockEnabled)
                            enabled: root.modeChoices.length > 0
                            Accessible.name: qsTr("Lock receive mode")
                            onToggled: {
                                const update = ({modeLockEnabled: checked})
                                if (checked) {
                                    const selected = root.selectedModeId(manualMode)
                                    if (selected.length === 0)
                                        return
                                    update.lockedMode = selected
                                }
                                root.applyControls(update)
                            }
                        }
                        Switch {
                            objectName: "sstvRxNoVis"
                            text: qsTr("Receive without VIS")
                            checked: Boolean(root.controls.receiveWithoutVis)
                            Accessible.name: text
                            Accessible.description: qsTr("Use bounded canonical timing fallback and reject ambiguous modes")
                            onToggled: root.applyControl("receiveWithoutVis", checked)
                        }

                        Label { text: qsTr("AFC"); color: root.secondaryTextColor }
                        ComboBox {
                            id: afcMode
                            objectName: "sstvRxAfcMode"
                            Layout.preferredWidth: 135
                            model: [qsTr("Off"), qsTr("Automatic"), qsTr("Manual")]
                            currentIndex: root.controlIndex(root.controls.afcMode,
                                                           ["off", "auto", "manual"])
                            Accessible.name: qsTr("SSTV automatic frequency control mode")
                            onActivated: root.applyControl("afcMode",
                                                           ["off", "auto", "manual"][currentIndex])
                        }
                        Label { text: qsTr("Correction (Hz)"); color: root.secondaryTextColor }
                        SpinBox {
                            id: manualAfc
                            objectName: "sstvRxManualAfcHz"
                            Layout.preferredWidth: 120
                            from: -150
                            to: 150
                            editable: true
                            value: Math.round(Number(root.controls.manualFrequencyCorrectionHz || 0))
                            enabled: String(root.controls.afcMode || "auto") === "manual"
                            Accessible.name: qsTr("Manual SSTV frequency correction in hertz")
                            onValueModified: root.applyControl("manualFrequencyCorrectionHz", value)
                        }
                        Button {
                            objectName: "sstvRxAfcReset"
                            text: qsTr("Reset AFC")
                            Accessible.name: text
                            onClicked: {
                                if (root.engine) {
                                    root.engine.resetSstvRxAfc()
                                    root.feedback = qsTr("AFC estimator reset")
                                }
                            }
                        }
                        Label {
                            text: qsTr("Measured %1 Hz · confidence %2%")
                                  .arg(Number(root.stats && root.stats.afcMeasuredOffsetHz || 0).toFixed(1))
                                  .arg((Number(root.stats && root.stats.afcConfidence || 0) * 100).toFixed(0))
                            color: root.secondaryTextColor
                            font.pixelSize: 10
                        }

                        Label { text: qsTr("Slant"); color: root.secondaryTextColor }
                        ComboBox {
                            id: slantMode
                            objectName: "sstvRxSlantMode"
                            Layout.preferredWidth: 135
                            model: [qsTr("Off"), qsTr("Automatic"), qsTr("Manual")]
                            currentIndex: root.controlIndex(root.controls.slantMode,
                                                           ["off", "auto", "manual"])
                            Accessible.name: qsTr("SSTV slant correction mode")
                            onActivated: root.applyControl("slantMode",
                                                           ["off", "auto", "manual"][currentIndex])
                        }
                        Label { text: qsTr("Clock (ppm)"); color: root.secondaryTextColor }
                        SpinBox {
                            id: manualSlant
                            objectName: "sstvRxManualSlantPpm"
                            Layout.preferredWidth: 120
                            from: -5000
                            to: 5000
                            editable: true
                            value: Math.round(Number(root.controls.manualClockErrorPpm || 0))
                            enabled: String(root.controls.slantMode || "auto") === "manual"
                            Accessible.name: qsTr("Manual SSTV clock error in parts per million")
                            onValueModified: root.applyControl("manualClockErrorPpm", value)
                        }
                        Button {
                            objectName: "sstvRxSlantReset"
                            text: qsTr("Reset slant")
                            Accessible.name: text
                            onClicked: {
                                if (root.engine) {
                                    root.engine.resetSstvRxSlant()
                                    root.feedback = qsTr("Slant estimator reset")
                                }
                            }
                        }
                        Label {
                            text: qsTr("Measured %1 ppm · confidence %2%")
                                  .arg(Number(root.stats && root.stats.slantMeasuredPpm || 0).toFixed(1))
                                  .arg((Number(root.stats && root.stats.slantConfidence || 0) * 100).toFixed(0))
                            color: root.secondaryTextColor
                            font.pixelSize: 10
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Switch {
                            objectName: "sstvRxTimingFallback"
                            text: qsTr("Canonical timing fallback")
                            checked: Boolean(root.controls.timingFallbackEnabled)
                            Accessible.name: text
                            onToggled: root.applyControl("timingFallbackEnabled", checked)
                        }
                        Label {
                            text: qsTr("Retain")
                            color: root.secondaryTextColor
                        }
                        SpinBox {
                            objectName: "sstvRxReplayRetention"
                            from: 5
                            to: 600
                            editable: true
                            value: Number(root.controls.replayRetentionSeconds || 180)
                            Accessible.name: qsTr("Retained receiver audio in seconds")
                            onValueModified: root.applyControl("replayRetentionSeconds", value)
                        }
                        Label { text: qsTr("seconds"); color: root.secondaryTextColor }
                        Switch {
                            objectName: "sstvRxRetainRaw"
                            text: qsTr("Retain diagnostic audio")
                            checked: Boolean(root.controls.retainRawAudio)
                            Accessible.name: text
                            onToggled: root.applyControl("retainRawAudio", checked)
                        }
                        Switch {
                            objectName: "sstvRxScopeEnabled"
                            text: qsTr("Live diagnostic scope")
                            checked: Boolean(root.controls.diagnosticScopeEnabled)
                            Accessible.name: text
                            onToggled: root.applyControl("diagnosticScopeEnabled", checked)
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            objectName: "sstvRxRedecodeRecent"
                            text: qsTr("Re-decode recent")
                            enabled: !!(root.engine && !root.audioJobBusy
                                         && !root.replayActive
                                         && Number(root.stats.replayRetainedSamples || 0) > 0)
                            Accessible.name: qsTr("Re-decode retained SSTV audio with the selected corrections")
                            onClicked: {
                                if (!root.engine)
                                    return
                                const selectedMode = String(root.controls.modeControl || "auto") === "manual"
                                                   ? root.selectedModeId(manualMode) : "auto"
                                root.feedback = root.engine.redecodeRecentSstv({
                                    "mode": selectedMode,
                                    "afcMode": String(root.controls.afcMode || "auto"),
                                    "frequencyCorrectionHz": Number(root.controls.manualFrequencyCorrectionHz || 0),
                                    "slantMode": String(root.controls.slantMode || "auto"),
                                    "clockErrorPpm": Number(root.controls.manualClockErrorPpm || 0)
                                }) ? qsTr("Preparing retained audio re-decode")
                                   : qsTr("Retained audio re-decode could not start")
                            }
                        }
                        Button {
                            objectName: "sstvRxSaveRawAudio"
                            text: qsTr("Save raw audio")
                            enabled: !!(root.engine && root.engine.sstvStorageReady
                                         && !root.audioJobBusy
                                         && Number(root.stats.replayRetainedSamples || 0) > 0)
                            Accessible.name: qsTr("Save the retained SSTV acquisition as a diagnostic WAV")
                            onClicked: {
                                if (root.engine)
                                    root.feedback = root.engine.saveSstvRxRawAudio()
                                            ? qsTr("Saving retained diagnostic WAV")
                                            : qsTr("Diagnostic WAV could not be saved")
                            }
                        }
                        Button {
                            objectName: "sstvRxCancelAudioJob"
                            text: qsTr("Cancel audio job")
                            visible: root.audioJobBusy
                            enabled: root.audioJobBusy
                            Accessible.name: text
                            onClicked: root.engine.cancelSstvRxAudioJob()
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.engine
                                  ? qsTr("Audio job: %1").arg(root.engine.sstvRxAudioJobState)
                                  : ""
                            color: root.audioJobBusy ? root.accentColor
                                                     : root.secondaryTextColor
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(250, Math.min(390, root.height * 0.52))
                color: "#05090d"
                border.color: root.engine && (root.engine.sstvRxActive
                                               || root.replayActive)
                              ? root.accentColor : root.borderColor
                border.width: 1
                radius: 8
                clip: true

                Canvas {
                    anchors.fill: parent
                    opacity: 0.28
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = root.borderColor
                        ctx.lineWidth = 1
                        for (var x = 0; x < width; x += 24) {
                            ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                        }
                        for (var y = 0; y < height; y += 24) {
                            ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                        }
                    }
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                }

                Flickable {
                    id: imageViewport
                    objectName: "sstvRxImageViewport"
                    anchors.fill: parent
                    anchors.margins: 10
                    anchors.topMargin: 46
                    visible: root.imageAvailable
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: receivedImage.width
                    contentHeight: receivedImage.height

                    Image {
                        id: receivedImage
                        objectName: "sstvRxReceivedImage"
                        width: root.imageViewMode === "fit"
                               ? imageViewport.width
                               : Math.max(1, Number(root.stats.imageWidth || implicitWidth || 1))
                                 * root.imageZoom
                        height: root.imageViewMode === "fit"
                                ? imageViewport.height
                                : Math.max(1, Number(root.stats.imageHeight || implicitHeight || 1))
                                  * root.imageZoom
                        source: root.engine ? root.engine.sstvRxImageSource : ""
                        fillMode: root.imageViewMode === "fit"
                                  ? Image.PreserveAspectFit : Image.Stretch
                        asynchronous: true
                        cache: false
                        smooth: root.imageViewMode !== "pixel"
                        mipmap: root.imageViewMode !== "pixel"
                        Accessible.name: qsTr("Progressive received SSTV image")
                    }
                }

                Row {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 9
                    spacing: 5
                    visible: root.imageAvailable
                    Button {
                        objectName: "sstvRxImageFit"
                        text: qsTr("Fit")
                        checked: root.imageViewMode === "fit"
                        checkable: true
                        Accessible.name: qsTr("Fit SSTV image to view")
                        onClicked: root.imageViewMode = "fit"
                    }
                    Button {
                        objectName: "sstvRxImagePixel"
                        text: qsTr("1:1 pixels")
                        checked: root.imageViewMode === "pixel"
                        checkable: true
                        Accessible.name: qsTr("Show SSTV image at native pixel size")
                        onClicked: {
                            root.imageViewMode = "pixel"
                            root.imageZoom = 1.0
                        }
                    }
                    Button {
                        objectName: "sstvRxImageZoomOut"
                        text: qsTr("−")
                        enabled: root.imageViewMode !== "fit" && root.imageZoom > 0.25
                        Accessible.name: qsTr("Zoom SSTV image out")
                        onClicked: {
                            root.imageViewMode = "zoom"
                            root.imageZoom = Math.max(0.25, root.imageZoom / 1.25)
                        }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("%1%").arg((root.imageZoom * 100).toFixed(0))
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                    }
                    Button {
                        objectName: "sstvRxImageZoomIn"
                        text: qsTr("+")
                        enabled: root.imageViewMode !== "fit" && root.imageZoom < 8.0
                        Accessible.name: qsTr("Zoom SSTV image in")
                        onClicked: {
                            root.imageViewMode = "zoom"
                            root.imageZoom = Math.min(8.0, root.imageZoom * 1.25)
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 430)
                    spacing: 8
                    visible: !root.imageAvailable
                    Label {
                        width: parent.width
                        text: root.replayActive
                              ? qsTr("Decoding the WAV recording")
                              : root.engine && root.engine.sstvRxActive
                                ? qsTr("Waiting for an SSTV frame")
                              : qsTr("Start reception to display a progressive image")
                        color: root.primaryTextColor
                        font.pixelSize: 17
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        width: parent.width
                        text: qsTr("The image surface is updated from bounded native scan-line events; audio and pixel arrays are never processed in QML.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    id: sourceBadge
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    width: Math.min(sourceLabel.implicitWidth + 18,
                                    Math.max(0, parent.width - 20))
                    implicitHeight: 24
                    radius: 12
                    color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.12)
                    border.color: root.borderColor
                    Label {
                        id: sourceLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 9
                        anchors.rightMargin: 9
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Source: %1").arg(root.sourceDisplay)
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    ToolTip.visible: sourceMouse.containsMouse && root.sourceDevice.length > 0
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Audio device: %1").arg(root.sourceDevice)
                    MouseArea {
                        id: sourceMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    visible: root.imageAvailable
                    implicitWidth: imageProgressLabel.implicitWidth + 18
                    implicitHeight: 24
                    radius: 12
                    color: Qt.rgba(0, 0, 0, 0.72)
                    border.color: root.stats && root.stats.imageComplete
                                  ? root.successColor : root.borderColor
                    Label {
                        id: imageProgressLabel
                        anchors.centerIn: parent
                        text: root.stats && root.stats.imageComplete
                              ? qsTr("Complete · %1").arg(root.textValue("imageMode", qsTr("SSTV")))
                              : qsTr("%1 · %2% · %3/%4 lines")
                                  .arg(root.textValue("imageMode", qsTr("SSTV")))
                                  .arg((root.imageCoverage * 100).toFixed(0))
                                  .arg(root.textValue("imageLinesPublished", "0"))
                                  .arg(root.textValue("imageHeight", "--"))
                        color: root.stats && root.stats.imageComplete
                               ? root.successColor : root.primaryTextColor
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                ProgressBar {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.bottomMargin: 2
                    visible: root.imageAvailable && !(root.stats && root.stats.imageComplete)
                    from: 0
                    to: 1
                    value: root.imageCoverage
                    Accessible.name: qsTr("SSTV image reception progress")
                    Accessible.description: qsTr("%1 percent received").arg((root.imageCoverage * 100).toFixed(0))
                }
            }

            Rectangle {
                objectName: "sstvRxDiagnosticScope"
                Layout.fillWidth: true
                Layout.preferredHeight: 135
                visible: Boolean(root.controls.diagnosticScopeEnabled)
                color: "#05090d"
                border.color: root.borderColor
                border.width: 1
                radius: 7
                clip: true

                Canvas {
                    id: scopeCanvas
                    anchors.fill: parent
                    anchors.margins: 8
                    property int snapshotRevision: Number(root.stats && root.stats.revision || 0)
                    onSnapshotRevisionChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const context = getContext("2d")
                        context.clearRect(0, 0, width, height)
                        context.strokeStyle = root.borderColor
                        context.lineWidth = 1
                        for (let row = 0; row <= 4; ++row) {
                            const y = row * height / 4
                            context.beginPath()
                            context.moveTo(0, y)
                            context.lineTo(width, y)
                            context.stroke()
                        }
                        const points = root.stats && root.stats.scope
                                     ? root.stats.scope : []
                        if (points.length < 2)
                            return
                        context.strokeStyle = root.accentColor
                        context.lineWidth = 1.5
                        context.beginPath()
                        for (let index = 0; index < points.length; ++index) {
                            const x = index * width / Math.max(1, points.length - 1)
                            const frequency = Number(points[index].frequencyHz || 0)
                            const normalized = Math.max(0, Math.min(1,
                                                       (frequency - 900) / 1700))
                            const y = height - normalized * height
                            if (index === 0)
                                context.moveTo(x, y)
                            else
                                context.lineTo(x, y)
                        }
                        context.stroke()
                    }
                    Accessible.name: qsTr("Bounded live SSTV frequency scope")
                }

                Label {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 10
                    text: qsTr("LIVE SCOPE · 900–2600 Hz · %1 points")
                          .arg(root.stats && root.stats.scope
                               ? root.stats.scope.length : 0)
                    color: root.secondaryTextColor
                    font.pixelSize: 9
                    font.bold: true
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.width >= 850 ? 4 : 2
                columnSpacing: 8
                rowSpacing: 8

                Metric {
                    label: qsTr("DETECTED MODE")
                    value: root.engine && root.engine.sstvDetectedMode
                           ? root.engine.sstvDetectedMode : qsTr("Searching")
                    valueColor: root.engine && root.engine.sstvDetectedMode
                                ? root.successColor : root.secondaryTextColor
                }
                Metric {
                    label: qsTr("VIS")
                    value: root.stats && root.stats.visAvailable
                           ? (qsTr("%1 · %2").arg(root.textValue("visPrimary", "--"))
                                             .arg(root.stats.visValid ? qsTr("valid") : qsTr("rejected")))
                           : qsTr("Not detected")
                    valueColor: root.stats && root.stats.visValid ? root.successColor : root.secondaryTextColor
                }
                Metric {
                    label: qsTr("RAW VIS BITS")
                    value: root.textValue("visRawBits", qsTr("Not detected"))
                }
                Metric {
                    label: qsTr("DECODED VIS")
                    value: root.stats && root.stats.visMappedMode
                           ? root.stats.visMappedMode
                           : root.textValue("visPrimary", qsTr("Not mapped"))
                    valueColor: root.stats && root.stats.visValid
                                ? root.successColor : root.secondaryTextColor
                }
                Metric {
                    label: qsTr("FSK ID")
                    value: root.stats && root.stats.fskIdValid
                           ? root.textValue("fskId", qsTr("Valid"))
                           : qsTr("Not detected")
                    valueColor: root.stats && root.stats.fskIdValid
                                ? root.successColor : root.secondaryTextColor
                }
                Metric {
                    label: qsTr("TONE ESTIMATE")
                    value: root.stats && Number(root.stats.frequencyObservations) > 0
                           ? qsTr("%1 Hz").arg(Number(root.stats.lastFrequencyHz).toFixed(1))
                           : "--"
                }
                Metric {
                    label: qsTr("CONFIDENCE")
                    value: root.stats && Number(root.stats.frequencyObservations) > 0
                           ? qsTr("%1%").arg((Number(root.stats.lastFrequencyConfidence) * 100).toFixed(0))
                           : "--"
                }
                Metric {
                    label: qsTr("AFC OFFSET / APPLIED")
                    value: qsTr("%1 / %2 Hz")
                           .arg(Number(root.stats && root.stats.afcMeasuredOffsetHz || 0).toFixed(1))
                           .arg(Number(root.stats && root.stats.afcCorrectionHz || 0).toFixed(1))
                    valueColor: Number(root.stats && root.stats.afcConfidence || 0) >= 0.6
                                ? root.successColor : root.primaryTextColor
                }
                Metric {
                    label: qsTr("AFC CONFIDENCE")
                    value: qsTr("%1% · %2 refs")
                           .arg((Number(root.stats && root.stats.afcConfidence || 0) * 100).toFixed(0))
                           .arg(root.textValue("afcAcceptedReferences", "0"))
                }
                Metric {
                    label: qsTr("SLANT MEASURED / APPLIED")
                    value: qsTr("%1 / %2 ppm")
                           .arg(Number(root.stats && root.stats.slantMeasuredPpm || 0).toFixed(1))
                           .arg(Number(root.stats && root.stats.slantAppliedPpm || 0).toFixed(1))
                }
                Metric {
                    label: qsTr("SYNC / CURRENT LINE")
                    value: qsTr("%1 · line %2")
                           .arg(root.stats && root.stats.syncLocked
                                ? qsTr("locked") : qsTr("searching"))
                           .arg(root.textValue("currentLine", "0"))
                    valueColor: root.stats && root.stats.syncLocked
                                ? root.successColor : root.secondaryTextColor
                }
                Metric {
                    label: qsTr("SIGNAL / SNR")
                    value: qsTr("%1 RMS · %2 dB")
                           .arg(Number(root.stats && root.stats.signalRms || 0).toFixed(3))
                           .arg(Number(root.stats && root.stats.signalSnrDb || 0).toFixed(1))
                }
                Metric {
                    label: qsTr("START UTC")
                    value: root.textValue("startUtc", qsTr("Waiting"))
                }
                Metric {
                    label: qsTr("RF FREQUENCY")
                    value: root.stats && Number(root.stats.rfFrequencyHz) > 0
                           ? qsTr("%1 Hz").arg(Number(root.stats.rfFrequencyHz).toFixed(0))
                           : "--"
                }
                Metric {
                    label: qsTr("IMAGE")
                    value: root.imageAvailable
                           ? qsTr("%1 × %2").arg(root.textValue("imageWidth", "--"))
                                             .arg(root.textValue("imageHeight", "--"))
                           : qsTr("Waiting")
                    valueColor: root.stats && root.stats.imageComplete
                                ? root.successColor : root.primaryTextColor
                }
                Metric {
                    label: qsTr("IMAGE COVERAGE")
                    value: root.imageAvailable
                           ? qsTr("%1%").arg((root.imageCoverage * 100).toFixed(1))
                           : "--"
                    valueColor: root.stats && root.stats.imagePartial
                                ? root.warningColor : root.primaryTextColor
                }
                Metric {
                    label: qsTr("INPUT CHUNKS")
                    value: root.textValue("chunksProcessed", "0")
                }
                Metric {
                    label: qsTr("RESAMPLED SAMPLES")
                    value: root.textValue("samplesResampled", "0")
                }
                Metric {
                    label: qsTr("RETAINED AUDIO")
                    value: qsTr("%1 / %2 samples")
                           .arg(root.textValue("replayRetainedSamples", "0"))
                           .arg(root.textValue("replayCapacitySamples", "0"))
                }
                Metric {
                    label: qsTr("DSP BLOCK AVG / MAX")
                    value: qsTr("%1 / %2 ms")
                           .arg((Number(root.stats && root.stats.dspAverageBlockNs || 0) / 1000000).toFixed(2))
                           .arg((Number(root.stats && root.stats.dspMaximumBlockNs || 0) / 1000000).toFixed(2))
                }
                Metric {
                    label: qsTr("PROGRESSIVE UPDATE RATE")
                    value: qsTr("%1 updates/s")
                           .arg(Number(root.stats && root.stats.progressiveUpdatesPerSecond || 0).toFixed(1))
                }
                Metric {
                    label: qsTr("DISCONTINUITIES")
                    value: root.textValue("discontinuities", "0")
                    valueColor: Number(root.stats.discontinuities || 0) > 0
                                ? root.warningColor : root.primaryTextColor
                }
                Metric {
                    label: qsTr("BOUNDED QUEUE DROPS")
                    value: root.textValue("droppedChunks", "0")
                    valueColor: Number(root.stats.droppedChunks || 0) > 0
                                ? root.warningColor : root.primaryTextColor
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.feedback.length > 0
                         || !!(root.stats && root.stats.lastError)
                         || !!(root.engine && root.engine.sstvWavReplayError)
                         || !!(root.engine && root.engine.sstvRxSaveError)
                         || !!(root.engine && root.engine.sstvRxAudioJobError)
                text: root.engine && root.engine.sstvWavReplayError
                      ? root.engine.sstvWavReplayError
                      : root.engine && root.engine.sstvRxSaveError
                        ? root.engine.sstvRxSaveError
                      : root.engine && root.engine.sstvRxAudioJobError
                        ? root.engine.sstvRxAudioJobError
                      : root.stats && root.stats.lastError
                        ? root.stats.lastError : root.feedback
                color: (root.engine && root.engine.sstvWavReplayError)
                       || (root.engine && root.engine.sstvRxSaveError)
                       || (root.engine && root.engine.sstvRxAudioJobError)
                       || (root.stats && root.stats.lastError)
                       ? root.warningColor : root.secondaryTextColor
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
