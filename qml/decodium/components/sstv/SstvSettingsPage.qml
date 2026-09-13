pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SstvPage {
    id: root
    required property var engine
    readonly property var sharing: root.engine ? root.engine.sstvShare : null
    readonly property var digital: root.engine ? root.engine.sstvDigital : null
    readonly property var gallery: root.engine ? root.engine.sstvGallery : null
    readonly property var rxStats: root.engine
                                           ? root.engine.sstvRxDiagnostics : ({})
    readonly property var rxControls: root.engine && root.engine.sstvRxControls
                                      ? root.engine.sstvRxControls : ({})
    readonly property string rxSourceType: root.engine && root.engine.sstvRxSource
                                           ? String(root.engine.sstvRxSource)
                                           : qsTr("Unavailable")
    readonly property string rxSourceDevice: root.engine && root.engine.sstvRxSourceDevice
                                             ? String(root.engine.sstvRxSourceDevice) : ""
    readonly property string rxSourceDisplay: root.rxSourceDevice.length > 0
                                              ? qsTr("%1 — %2").arg(root.rxSourceType)
                                                    .arg(root.rxSourceDevice)
                                              : root.rxSourceType

    function updateRxControl(name, value) {
        if (!root.engine)
            return false
        const update = ({})
        update[name] = value
        return root.engine.updateSstvRxControls(update)
    }

    function correctionIndex(value) {
        const index = ["off", "auto", "manual"].indexOf(String(value || "auto"))
        return index >= 0 ? index : 1
    }

    function syncDigitalProfile() {
        if (!root.digital) {
            digitalProfile.currentIndex = -1
            return
        }
        const values = root.digital.profiles || []
        for (let index = 0; index < values.length; ++index) {
            if (values[index].id === root.digital.selectedProfileId) {
                digitalProfile.currentIndex = index
                return
            }
        }
        digitalProfile.currentIndex = -1
    }

    function updateRetentionField(name, value) {
        if (!root.gallery)
            return false
        const current = root.gallery.retentionSettings || ({})
        const updated = {
            "automaticEnabled": Boolean(current.automaticEnabled),
            "maximumAgeDays": Number(current.maximumAgeDays || 0),
            "imageQuotaBytes": Number(current.imageQuotaBytes || 0),
            "thumbnailQuotaBytes": Number(current.thumbnailQuotaBytes || 0),
            "rawAudioQuotaBytes": Number(current.rawAudioQuotaBytes || 0),
            "sharedPolicy": Number(current.sharedPolicy || 0),
            "maximumDeletesPerRun": Number(current.maximumDeletesPerRun || 100)
        }
        updated[name] = value
        return root.gallery.updateRetentionSettings(updated) !== 0
    }

    Component.onCompleted: syncDigitalProfile()

    Connections {
        target: root.digital
        enabled: root.digital !== null
        function onSelectedProfileChanged() {
            root.syncDigitalProfile()
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label { text: qsTr("SETTINGS"); color: root.accentColor; font.pixelSize: 15; font.bold: true; font.letterSpacing: 1.2 }
            Label { Layout.fillWidth: true; text: qsTr("SSTV uses safe defaults and does not change existing Decodium audio, CAT or radio profiles."); color: root.secondaryTextColor; wrapMode: Text.WordWrap }
            Rectangle {
                objectName: "sstvSettingsRxDefaults"
                Layout.fillWidth: true
                Layout.preferredHeight: receiverDefaults.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: receiverDefaults
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Analog receiver defaults")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width >= 1000 ? 6 : 2
                        columnSpacing: 10
                        rowSpacing: 7

                        Label { text: qsTr("AFC mode"); color: root.secondaryTextColor }
                        ComboBox {
                            objectName: "sstvSettingsRxAfcMode"
                            model: [qsTr("Off"), qsTr("Automatic"), qsTr("Manual")]
                            currentIndex: root.correctionIndex(root.rxControls.afcMode)
                            Accessible.name: qsTr("Default SSTV AFC mode")
                            onActivated: root.updateRxControl("afcMode",
                                                              ["off", "auto", "manual"][currentIndex])
                        }
                        Label { text: qsTr("Manual AFC (Hz)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvSettingsRxManualAfc"
                            from: -150
                            to: 150
                            editable: true
                            value: Math.round(Number(root.rxControls.manualFrequencyCorrectionHz || 0))
                            enabled: String(root.rxControls.afcMode || "auto") === "manual"
                            onValueModified: root.updateRxControl("manualFrequencyCorrectionHz", value)
                        }
                        Label { text: qsTr("Slant mode"); color: root.secondaryTextColor }
                        ComboBox {
                            objectName: "sstvSettingsRxSlantMode"
                            model: [qsTr("Off"), qsTr("Automatic"), qsTr("Manual")]
                            currentIndex: root.correctionIndex(root.rxControls.slantMode)
                            Accessible.name: qsTr("Default SSTV slant mode")
                            onActivated: root.updateRxControl("slantMode",
                                                              ["off", "auto", "manual"][currentIndex])
                        }

                        Label { text: qsTr("Manual slant (ppm)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvSettingsRxManualSlant"
                            from: -5000
                            to: 5000
                            editable: true
                            value: Math.round(Number(root.rxControls.manualClockErrorPpm || 0))
                            enabled: String(root.rxControls.slantMode || "auto") === "manual"
                            onValueModified: root.updateRxControl("manualClockErrorPpm", value)
                        }
                        Label { text: qsTr("Replay buffer (seconds)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvSettingsRxReplayRetention"
                            from: 5
                            to: 600
                            editable: true
                            value: Number(root.rxControls.replayRetentionSeconds || 180)
                            onValueModified: root.updateRxControl("replayRetentionSeconds", value)
                        }
                        Switch {
                            objectName: "sstvSettingsRxNoVis"
                            text: qsTr("Receive without VIS")
                            checked: Boolean(root.rxControls.receiveWithoutVis)
                            Accessible.description: qsTr("Fail closed when canonical timing observations are ambiguous")
                            onToggled: root.updateRxControl("receiveWithoutVis", checked)
                        }
                        Switch {
                            objectName: "sstvSettingsRxTimingFallback"
                            text: qsTr("Canonical timing fallback")
                            checked: Boolean(root.rxControls.timingFallbackEnabled)
                            onToggled: root.updateRxControl("timingFallbackEnabled", checked)
                        }
                        Switch {
                            objectName: "sstvSettingsRxRetainRaw"
                            text: qsTr("Retain diagnostic audio")
                            checked: Boolean(root.rxControls.retainRawAudio)
                            onToggled: root.updateRxControl("retainRawAudio", checked)
                        }
                        Switch {
                            objectName: "sstvSettingsRxScope"
                            text: qsTr("Enable bounded live scope")
                            checked: Boolean(root.rxControls.diagnosticScopeEnabled)
                            onToggled: root.updateRxControl("diagnosticScopeEnabled", checked)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Retained audio is hard-bounded and raw WAV export or re-decode runs outside the GUI and audio callback threads.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: storageSettings.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: storageSettings
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Images and storage")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: root.engine && root.engine.sstvStorageReady
                                  ? qsTr("Decodium storage ready")
                                  : qsTr("Storage unavailable")
                            color: root.engine && root.engine.sstvStorageReady
                                   ? root.successColor : root.warningColor
                            font.pixelSize: 10
                        }
                    }
                    Switch {
                        objectName: "sstvSettingsRxAutoSave"
                        text: qsTr("Automatically save received SSTV images as lossless PNG")
                        checked: !!(root.engine
                                     && root.engine.sstvRxAutoSaveEnabled)
                        enabled: !!(root.engine && root.engine.sstvStorageReady)
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: text
                        Accessible.description: qsTr("Enabling this option accepts Decodium's application-data storage location for automatic SSTV saves")
                        onToggled: {
                            if (root.engine
                                    && checked !== root.engine.sstvRxAutoSaveEnabled) {
                                root.engine.sstvRxAutoSaveEnabled = checked
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Automatic saving is opt-in. PNG encoding, sidecar integrity checks and SQLite indexing run on Decodium's storage worker, outside the GUI and audio threads.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: retentionSettingsLayout.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: retentionSettingsLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Gallery retention and quotas")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: root.gallery && root.gallery.retentionBusy
                                  ? qsTr("Storage worker busy")
                                  : qsTr("Preview before deletion")
                            color: root.gallery && root.gallery.retentionBusy
                                   ? root.warningColor : root.successColor
                            font.pixelSize: 10
                        }
                    }
                    Switch {
                        id: retentionAutomatic
                        objectName: "sstvRetentionAutomaticEnabled"
                        text: qsTr("Enable automatic Gallery retention")
                        checked: Boolean(root.gallery
                                         && root.gallery.retentionSettings.automaticEnabled)
                        enabled: Boolean(root.gallery
                                         && !root.gallery.retentionBusy)
                        Accessible.name: text
                        Accessible.description: qsTr("Destructive automatic retention is off by default and uses the same protected, journalled planner as manual retention")
                        onToggled: {
                            if (root.gallery
                                    && checked !== Boolean(root.gallery.retentionSettings.automaticEnabled)) {
                                root.updateRetentionField("automaticEnabled", checked)
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Automatic deletion remains disabled until this switch is explicitly enabled. Manual retention always starts with a non-destructive preview and strong confirmation. Favourites and QSO-linked rows are never candidates.")
                        color: root.warningColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        objectName: "sstvRetentionSettingsGrid"
                        Layout.fillWidth: true
                        columns: width >= 900 ? 4 : 2
                        columnSpacing: 10
                        rowSpacing: 7
                        enabled: Boolean(root.gallery
                                         && !root.gallery.retentionBusy)

                        Label { text: qsTr("Maximum age (days, 0=off)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvRetentionMaximumAgeDays"
                            from: 0; to: 36500; editable: true
                            value: root.gallery
                                   ? Number(root.gallery.retentionSettings.maximumAgeDays || 0) : 0
                            onValueModified: root.updateRetentionField("maximumAgeDays", value)
                        }
                        Label { text: qsTr("Batch limit"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvRetentionBatchLimit"
                            from: 1; to: 500; editable: true
                            value: root.gallery
                                   ? Number(root.gallery.retentionSettings.maximumDeletesPerRun || 100) : 100
                            onValueModified: root.updateRetentionField("maximumDeletesPerRun", value)
                        }

                        Label { text: qsTr("Image quota (MiB, 0=off)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvRetentionImageQuotaMiB"
                            from: 0; to: 1048576; editable: true
                            value: root.gallery
                                   ? Math.round(Number(root.gallery.retentionSettings.imageQuotaBytes || 0) / 1048576) : 0
                            onValueModified: root.updateRetentionField("imageQuotaBytes", value * 1048576)
                        }
                        Label { text: qsTr("Thumbnail quota (MiB, 0=off)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvRetentionThumbnailQuotaMiB"
                            from: 0; to: 1048576; editable: true
                            value: root.gallery
                                   ? Math.round(Number(root.gallery.retentionSettings.thumbnailQuotaBytes || 0) / 1048576) : 0
                            onValueModified: root.updateRetentionField("thumbnailQuotaBytes", value * 1048576)
                        }

                        Label { text: qsTr("Raw audio quota (MiB, 0=off)"); color: root.secondaryTextColor }
                        SpinBox {
                            objectName: "sstvRetentionRawAudioQuotaMiB"
                            from: 0; to: 1048576; editable: true
                            value: root.gallery
                                   ? Math.round(Number(root.gallery.retentionSettings.rawAudioQuotaBytes || 0) / 1048576) : 0
                            onValueModified: root.updateRetentionField("rawAudioQuotaBytes", value * 1048576)
                        }
                        Label { text: qsTr("Shared files"); color: root.secondaryTextColor }
                        ComboBox {
                            objectName: "sstvRetentionSharedPolicy"
                            textRole: "text"
                            valueRole: "value"
                            model: [
                                {"text": qsTr("Protect (recommended)"), "value": 0},
                                {"text": qsTr("Allow completed uploads"), "value": 1}
                            ]
                            currentIndex: root.gallery
                                          && Number(root.gallery.retentionSettings.sharedPolicy || 0) === 1 ? 1 : 0
                            onActivated: root.updateRetentionField("sharedPolicy", currentValue)
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: txTimingSettings.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: txTimingSettings
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Transmission timing")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: root.engine && root.engine.sstvTxActive
                                  ? qsTr("Locked during TX")
                                  : qsTr("Ready to adjust")
                            color: root.engine && root.engine.sstvTxActive
                                   ? root.warningColor : root.successColor
                            font.pixelSize: 10
                        }
                    }

                    GridLayout {
                        objectName: "sstvTxTimingGrid"
                        Layout.fillWidth: true
                        columns: width >= 900 ? 4 : 2
                        columnSpacing: 10
                        rowSpacing: 7
                        enabled: !!(root.engine && !root.engine.sstvTxActive)

                        Label { text: qsTr("CAT/PTT lead"); color: root.secondaryTextColor }
                        SpinBox {
                            id: pttLead
                            objectName: "sstvPttLeadMsControl"
                            from: 0
                            to: 5000
                            stepSize: 50
                            editable: true
                            value: root.engine ? root.engine.sstvPttLeadMs : 200
                            Accessible.name: qsTr("SSTV CAT PTT lead milliseconds")
                            onValueModified: if (root.engine) root.engine.sstvPttLeadMs = value
                        }
                        Label { text: qsTr("CAT/PTT tail"); color: root.secondaryTextColor }
                        SpinBox {
                            id: pttTail
                            objectName: "sstvPttTailMsControl"
                            from: 0
                            to: 5000
                            stepSize: 50
                            editable: true
                            value: root.engine ? root.engine.sstvPttTailMs : 500
                            Accessible.name: qsTr("SSTV CAT PTT tail milliseconds")
                            onValueModified: if (root.engine) root.engine.sstvPttTailMs = value
                        }

                        Label { text: qsTr("PTT release retry"); color: root.secondaryTextColor }
                        SpinBox {
                            id: pttRetry
                            objectName: "sstvPttRetryMsControl"
                            from: 100
                            to: 2000
                            stepSize: 100
                            editable: true
                            value: root.engine ? root.engine.sstvPttReleaseRetryMs : 500
                            Accessible.name: qsTr("SSTV PTT release retry milliseconds")
                            onValueModified: if (root.engine) root.engine.sstvPttReleaseRetryMs = value
                        }
                        Item { Layout.columnSpan: 2; Layout.fillWidth: true }

                        Label { text: qsTr("VOX pre-key"); color: root.secondaryTextColor }
                        SpinBox {
                            id: voxPreKey
                            objectName: "sstvVoxPreKeyMsControl"
                            from: 100
                            to: 4000
                            stepSize: 50
                            editable: true
                            value: root.engine ? root.engine.sstvVoxPreKeyMs : 750
                            Accessible.name: qsTr("SSTV VOX pre-key milliseconds")
                            onValueModified: if (root.engine) root.engine.sstvVoxPreKeyMs = value
                        }
                        Label { text: qsTr("VOX hang"); color: root.secondaryTextColor }
                        SpinBox {
                            id: voxHang
                            objectName: "sstvVoxHangMsControl"
                            from: 100
                            to: 5000
                            stepSize: 50
                            editable: true
                            value: root.engine ? root.engine.sstvVoxHangMs : 500
                            Accessible.name: qsTr("SSTV VOX hang milliseconds")
                            onValueModified: if (root.engine) root.engine.sstvVoxHangMs = value
                        }

                        Label { text: qsTr("VOX tone"); color: root.secondaryTextColor }
                        SpinBox {
                            id: voxTone
                            objectName: "sstvVoxToneHzControl"
                            from: 300
                            to: 3000
                            stepSize: 25
                            editable: true
                            value: root.engine
                                   ? Math.round(root.engine.sstvVoxToneFrequencyHz)
                                   : 1900
                            Accessible.name: qsTr("SSTV VOX tone hertz")
                            onValueModified: if (root.engine) root.engine.sstvVoxToneFrequencyHz = value
                        }
                        Label { text: qsTr("VOX level (%)"); color: root.secondaryTextColor }
                        SpinBox {
                            id: voxLevel
                            objectName: "sstvVoxLevelControl"
                            from: 5
                            to: 100
                            stepSize: 5
                            editable: true
                            value: root.engine
                                   ? Math.round(root.engine.sstvVoxToneLevel * 100.0)
                                   : 50
                            Accessible.name: qsTr("SSTV VOX tone level percent")
                            onValueModified: if (root.engine) root.engine.sstvVoxToneLevel = value / 100.0
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("CAT/PTT lead and tail are silent timing barriers. In VOX mode Decodium instead emits a bounded pre-key tone before the SSTV header and a hang tone after the payload; WAV exports remain protocol-only.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: receptionSettings.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: receptionSettings
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    Label {
                        text: qsTr("Reception")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Decodium audio source")
                            color: root.secondaryTextColor
                        }
                        Label {
                            objectName: "sstvSettingsRxSource"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: root.rxSourceDisplay
                            color: root.primaryTextColor
                            font.bold: true
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Native decoder lifecycle")
                            color: root.secondaryTextColor
                        }
                        Label {
                            objectName: "sstvSettingsRxState"
                            text: root.engine ? root.engine.sstvRxState
                                              : qsTr("Unavailable")
                            color: root.engine && root.engine.sstvRxActive
                                   ? root.successColor : root.primaryTextColor
                            font.bold: true
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("SSTV listens only to Decodium's selected mono PCM route. It never opens a second audio-capture device and remains inactive until reception or replay is started.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: sharingSettings.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: sharingSettings
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Remote sharing")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: root.sharing && root.sharing.configured
                                  ? qsTr("Provider configured")
                                  : qsTr("No provider configured")
                            color: root.sharing && root.sharing.configured
                                   ? root.successColor : root.secondaryTextColor
                            font.pixelSize: 10
                        }
                    }
                    Switch {
                        objectName: "sstvSettingsSharingEnabled"
                        text: qsTr("Enable opt-in remote image sharing")
                        checked: !!(root.sharing && root.sharing.enabled)
                        enabled: !!(root.sharing && root.sharing.ready)
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: text
                        Accessible.description: qsTr("This enables the native sharing client only; no provider, upload or incoming download is selected automatically")
                        onToggled: {
                            if (root.sharing
                                    && checked !== root.sharing.enabled) {
                                root.sharing.setEnabled(checked)
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.sharing
                              ? (root.sharing.secureStorageAvailable
                                 ? qsTr("Credential storage is available. Uploads, incoming downloads and public sharing remain off until explicitly requested on the Remote Sharing page.")
                                 : qsTr("Secure credential storage is unavailable; authenticated sharing fails closed."))
                              : qsTr("Remote sharing is unavailable in this build.")
                        color: root.sharing && root.sharing.secureStorageAvailable
                               ? root.secondaryTextColor : root.warningColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: digitalSettings.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: digitalSettings
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    Label {
                        text: qsTr("Digital SSTV / HAMDRM")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Named profile")
                            color: root.secondaryTextColor
                        }
                        ComboBox {
                            id: digitalProfile
                            objectName: "sstvSettingsHamDrmProfile"
                            Layout.fillWidth: true
                            model: root.digital ? root.digital.profiles : []
                            textRole: "displayName"
                            valueRole: "id"
                            enabled: !!(root.digital && !root.digital.busy)
                            onActivated: {
                                if (root.digital && currentValue)
                                    root.digital.selectedProfileId = currentValue
                            }
                            Accessible.name: qsTr("HAMDRM profile")
                        }
                    }
                    Label {
                        objectName: "sstvSettingsHamDrmCapability"
                        Layout.fillWidth: true
                        text: root.digital
                              ? root.digital.capabilityMessage
                              : qsTr("HAMDRM is unavailable in this build.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: diagnosticsSettings.implicitHeight + 24
                color: Qt.rgba(0, 0, 0, 0.10)
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: diagnosticsSettings
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    Label {
                        text: qsTr("Runtime diagnostics")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    GridLayout {
                        objectName: "sstvSettingsDiagnostics"
                        Layout.fillWidth: true
                        columns: width >= 900 ? 4 : 2
                        columnSpacing: 10
                        rowSpacing: 4
                        Label { text: qsTr("Queued samples"); color: root.secondaryTextColor }
                        Label { text: String(root.rxStats.queuedSamples || 0); color: root.primaryTextColor }
                        Label { text: qsTr("Dropped samples"); color: root.secondaryTextColor }
                        Label { text: String(root.rxStats.droppedSamples || 0); color: root.primaryTextColor }
                        Label { text: qsTr("DSP chunks"); color: root.secondaryTextColor }
                        Label { text: String(root.rxStats.chunksProcessed || 0); color: root.primaryTextColor }
                        Label { text: qsTr("Failures"); color: root.secondaryTextColor }
                        Label { text: String(root.rxStats.processingFailures || 0); color: root.primaryTextColor }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The Diagnostics page exposes the complete bounded runtime snapshot. Image pixels, raw audio, credentials and signed URLs are not included in these counters.")
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
