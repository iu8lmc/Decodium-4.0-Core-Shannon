/* Decodium 4.0 - lazy Settings tab */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SettingsPageScroll {
    id: settingsTab2

    property var dialog
    readonly property var bridge: dialog ? dialog.appBridge : null
    readonly property bool compactSettingsLayout: dialog ? dialog.compactSettingsLayout : false
    readonly property bool narrowSettingsLayout: dialog ? dialog.narrowSettingsLayout : false
    readonly property int pageColumns: compactSettingsLayout ? 2 : 4
    readonly property int labelWidth: dialog ? dialog.labelWidth : 120
    readonly property int fieldMinWidth: dialog ? dialog.fieldMinWidth : 180
    readonly property int wideFieldMinWidth: dialog ? dialog.wideFieldMinWidth : 260
    readonly property int portFieldMinWidth: dialog ? dialog.portFieldMinWidth : 140
    readonly property int numericFieldMinWidth: dialog ? dialog.numericFieldMinWidth : 120
    readonly property int comboFieldMinWidth: dialog ? dialog.comboFieldMinWidth : 180
    readonly property int frequencyPageMinWidth: dialog ? dialog.frequencyPageMinWidth : 760
    readonly property int scrollLeftMargin: dialog ? dialog.scrollLeftMargin : 10
    readonly property int scrollTopMargin: dialog ? dialog.scrollTopMargin : 10
    readonly property int scrollRightMargin: dialog ? dialog.scrollRightMargin : 12
    readonly property int scrollBottomMargin: dialog ? dialog.scrollBottomMargin : 96
    pageLeftMargin: scrollLeftMargin
    pageTopMargin: scrollTopMargin
    pageRightMargin: scrollRightMargin
    pageBottomMargin: scrollBottomMargin
    minimumContentWidth: dialog ? dialog.settingsPageMinimumContentWidth(pageColumns) : 0
    readonly property color bgDeep: dialog ? dialog.bgDeep : "#080b12"
    readonly property color bgMedium: dialog ? dialog.bgMedium : "#101722"
    readonly property color bgLight: dialog ? dialog.bgLight : "#1a2433"
    readonly property color bgDark: dialog ? dialog.bgDark : "#080b12"
    readonly property color primaryBlue: dialog ? dialog.primaryBlue : "#3ba7ff"
    readonly property color secondaryCyan: dialog ? dialog.secondaryCyan : "#00d9ff"
    readonly property color accentGreen: dialog ? dialog.accentGreen : "#00f08b"
    readonly property color textPrimary: dialog ? dialog.textPrimary : "#f2f5f7"
    readonly property color textSecondary: dialog ? dialog.textSecondary : "#a7b2c0"
    readonly property color textDim: dialog ? dialog.textDim : "#667180"
    readonly property color glassBorder: dialog ? dialog.glassBorder : "#334455"
    readonly property int controlHeight: dialog ? dialog.controlHeight : 32
    readonly property int controlFontSize: dialog ? dialog.controlFontSize : 12
    readonly property int controlVerticalPadding: dialog ? dialog.controlVerticalPadding : 0
    readonly property int spinTextSidePadding: dialog ? dialog.spinTextSidePadding : 52

    function boolSetting(key, fallback) {
        return dialog ? dialog.boolSetting(key, fallback) : !!fallback
    }

    function setBoolSettingIfChanged(key, value, fallback) {
        if (dialog)
            dialog.setBoolSettingIfChanged(key, value, fallback)
    }

    component RtlCheckBox: CheckBox {
        id: rtlCheckControl
        required property var theme
        implicitWidth: 26
        implicitHeight: theme.controlHeight
        hoverEnabled: true
        opacity: 1.0

        indicator: Rectangle {
            implicitWidth: 22
            implicitHeight: 22
            x: 2
            y: (rtlCheckControl.height - height) / 2
            radius: 4
            opacity: 1.0
            color: rtlCheckControl.checked
                   ? (rtlCheckControl.enabled
                      ? rtlCheckControl.theme.primaryBlue
                      : Qt.darker(rtlCheckControl.theme.primaryBlue, 1.45))
                   : (rtlCheckControl.hovered && rtlCheckControl.enabled
                      ? Qt.lighter(rtlCheckControl.theme.bgLight, 1.65)
                      : (rtlCheckControl.enabled
                         ? Qt.lighter(rtlCheckControl.theme.bgMedium, 1.55)
                         : Qt.lighter(rtlCheckControl.theme.bgMedium, 1.25)))
            border.width: 2
            border.color: !rtlCheckControl.enabled
                          ? Qt.lighter(rtlCheckControl.theme.glassBorder, 1.45)
                          : (rtlCheckControl.checked
                             ? rtlCheckControl.theme.secondaryCyan
                             : Qt.lighter(rtlCheckControl.theme.glassBorder, 1.9))

            Text {
                anchors.centerIn: parent
                text: rtlCheckControl.checked ? "✓" : ""
                color: rtlCheckControl.enabled
                       ? rtlCheckControl.theme.textPrimary
                       : rtlCheckControl.theme.textSecondary
                font.pixelSize: 15
                font.bold: true
            }
        }

        contentItem: Item {
            implicitWidth: 0
            implicitHeight: 0
        }
    }

    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    GridLayout {
        width: Math.max(0, parent.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        columns: pageColumns; columnSpacing: 10; rowSpacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.rightMargin: dialog.scrollRightMargin
        anchors.topMargin: dialog.scrollTopMargin

        // ── Dispositivi Audio ──
        Text { text: qsTr("AUDIO DEVICES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 4 }
        Item { Layout.fillWidth: true }
        Rectangle {
            Layout.preferredWidth: 110
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignRight
            radius: 6
            color: audioRefreshMA.containsMouse ? bgMedium : "transparent"
            border.color: glassBorder
            Text {
                anchors.centerIn: parent
                text: qsTr("↻  Refresh")
                color: secondaryCyan
                font.pixelSize: 11
                font.bold: true
            }
            MouseArea {
                id: audioRefreshMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: dialog.refreshAudioDevices()
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Input Device:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: audioInDevCombo
            model: bridge.audioInputDevices
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            currentIndex: dialog.stringListIndexOf(bridge.audioInputDevices, bridge.audioInputDevice)
            onActivated: {
                bridge.audioInputDevice = currentText
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: audioInDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.width: Math.max(audioInDevCombo.width, 560)
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            hoverEnabled: true
            ToolTip.visible: hovered && Qt.platform.os === "linux"
            ToolTip.delay: 600
            ToolTip.text: qsTr("Linux: entries marked 'Pulse/PipeWire monitor' capture the audio playing on that PipeWire/PulseAudio sink, useful for WebSDR/KiwiSDR browser audio. Selecting a Pulse/PipeWire source requires pactl and changes the current user's default capture source.")
        }
        Text { text: qsTr("Input Channel:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: audioInChCombo
            model: [qsTr("Mono"),qsTr("Left"),qsTr("Right"),qsTr("Both")]; Layout.fillWidth: true; implicitHeight: controlHeight
            Layout.minimumWidth: fieldMinWidth
            currentIndex: bridge.audioInputChannel
            onActivated: {
                bridge.audioInputChannel = currentIndex
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: audioInChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Item { Layout.columnSpan: 2 }

        Text { text: qsTr("Output Device:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: audioOutDevCombo
            model: bridge.audioOutputDevices
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            currentIndex: dialog.stringListIndexOf(bridge.audioOutputDevices, bridge.audioOutputDevice)
            onActivated: {
                bridge.audioOutputDevice = currentText
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: audioOutDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.width: Math.max(audioOutDevCombo.width, 560)
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Output Channel:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: audioOutChCombo
            model: [qsTr("Mono"),qsTr("Left"),qsTr("Right"),qsTr("Both")]; Layout.fillWidth: true; implicitHeight: controlHeight
            Layout.minimumWidth: fieldMinWidth
            currentIndex: bridge.audioOutputChannel
            onActivated: {
                bridge.audioOutputChannel = currentIndex
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: audioOutChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Item { Layout.columnSpan: 2 }

        // ── RTL-SDR ──
        Text { text: qsTr("RTL-SDR RECEIVER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 10 }
        Item { Layout.fillWidth: true }
        Rectangle {
            Layout.preferredWidth: 110
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignRight
            radius: 6
            color: rtlRefreshMouse.containsMouse ? bgMedium : "transparent"
            border.color: glassBorder
            Text { anchors.centerIn: parent; text: qsTr("↻  Refresh"); color: secondaryCyan; font.pixelSize: 11; font.bold: true }
            MouseArea {
                id: rtlRefreshMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: bridge.refreshRtlSdrDevices()
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Use RTL-SDR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        RtlCheckBox {
            id: rtlEnabledCheck
            theme: settingsTab2
            checked: bridge.getSetting("RtlSdrEnabled", false)
            enabled: bridge.rtlSdrSupported
            Layout.preferredHeight: controlHeight
            onClicked: {
                bridge.setSetting("RtlSdrEnabled", checked)
                dialog.scheduleSettingsPersist()
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Receives directly from an RTL-SDR. This mode is receive-only: Tune and TX are disabled.")
        }
        Text {
            text: bridge.rtlSdrSupported
                  ? qsTr("Experimental function under development")
                    + " — " + qsTr("No external audio cable is required. RX only.")
                  : qsTr("RTL-SDR support is not included in this build.")
            color: bridge.rtlSdrSupported ? "#ffb74d" : "#ff6b6b"
            font.pixelSize: 11
            font.bold: bridge.rtlSdrSupported
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        Text { text: qsTr("Receiver:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: rtlDeviceCombo
            model: bridge.rtlSdrDevices
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            currentIndex: Math.max(0, Math.min(bridge.rtlSdrDevices.length - 1, bridge.getSetting("RtlSdrDeviceIndex", 0)))
            onActivated: {
                bridge.setSetting("RtlSdrDeviceIndex", currentIndex)
                Qt.callLater(rtlModeCombo.normalizeModeForReceiver)
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlDeviceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.width: Math.max(rtlDeviceCombo.width, 500)
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Input mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: rtlModeCombo
            property bool directSamplingAvailable: bridge.rtlSdrDirectSamplingAvailable(rtlDeviceCombo.currentIndex)
            function normalizeModeForReceiver() {
                if (!directSamplingAvailable
                        && bridge.getSetting("RtlSdrMode", "sdr") === "direct") {
                    bridge.setSetting("RtlSdrMode", "sdr")
                    currentIndex = 0
                    dialog.scheduleSettingsPersist()
                }
            }
            model: [qsTr("SDR Radio"), qsTr("Direct Sampling")]
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
                     && bridge.rtlSdrDevices.length > 0
            Layout.fillWidth: true
            implicitHeight: controlHeight
            currentIndex: bridge.getSetting("RtlSdrMode", "sdr") === "direct"
                          && directSamplingAvailable ? 1 : 0
            onActivated: {
                if (currentIndex === 1 && !directSamplingAvailable) {
                    currentIndex = 0
                    return
                }
                bridge.setSetting("RtlSdrMode", currentIndex === 1 ? "direct" : "sdr")
                dialog.scheduleSettingsPersist()
            }
            onDirectSamplingAvailableChanged: normalizeModeForReceiver()
            Component.onCompleted: normalizeModeForReceiver()
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlModeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate {
                enabled: index === 0 || rtlModeCombo.directSamplingAvailable
                opacity: enabled ? 1.0 : 0.45
                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            ToolTip.visible: hovered
            ToolTip.text: !directSamplingAvailable
                          ? qsTr("Uses the tuner path. RTL-SDR Blog V4 handles its HF upconverter automatically.")
                          : currentIndex === 1
                          ? qsTr("Uses the RTL2832 direct-sampling Q ADC for HF reception from 500 kHz to 24 MHz.")
                          : qsTr("Uses the tuner path. RTL-SDR Blog V4 handles its HF upconverter automatically.")
        }
        Text {
            visible: bridge.rtlSdrDevices.length > 0
                     && !rtlModeCombo.directSamplingAvailable
            text: qsTr("Uses the tuner path. RTL-SDR Blog V4 handles its HF upconverter automatically.")
            color: secondaryCyan
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
        }
        Text { text: qsTr("Demodulator:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: rtlDemodCombo
            model: [qsTr("Weak signal / FT8 audio"), qsTr("Wide FM broadcast"),
                    qsTr("Narrow FM"), qsTr("AM"), qsTr("USB"),
                    qsTr("LSB"), qsTr("CW")]
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.fillWidth: true
            implicitHeight: controlHeight
            property var demodKeys: ["weak", "wfm", "nfm", "am", "usb", "lsb", "cw"]
            property bool ssbMode: currentIndex === 4 || currentIndex === 5
            currentIndex: Math.max(0, demodKeys.indexOf(bridge.getSetting("RtlSdrDemodulator", "weak")))
            onActivated: {
                bridge.setSetting("RtlSdrDemodulator", demodKeys[currentIndex])
                // Wide FM needs RF bandwidth beyond the old FT8
                // input rates; use a known 48 kHz audio divisor.
                var currentRate = bridge.getSetting("RtlSdrSampleRate", 240000)
                if (currentIndex === 1 && currentRate < 960000)
                    bridge.setSetting("RtlSdrSampleRate", 960000)
                else if (currentIndex !== 1 && currentRate > 288000)
                    bridge.setSetting("RtlSdrSampleRate", 240000)
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlDemodCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            ToolTip.visible: hovered
            ToolTip.text: currentIndex === 0
                          ? qsTr("Sends decoder PCM only to Decodium's weak-signal modes; the RF panadapter remains IQ-based.")
                          : qsTr("Demodulates receive audio separately. TX and Tune remain disabled with RTL-SDR.")
        }
        Text { text: qsTr("Listen to receiver audio:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        RtlCheckBox {
            id: rtlReceiverAudioCheck
            theme: settingsTab2
            checked: bridge.getSetting("RtlSdrAudioEnabled", rtlDemodCombo.currentIndex !== 0)
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlDemodCombo.currentIndex !== 0
            Layout.preferredHeight: controlHeight
            onClicked: {
                bridge.setSetting("RtlSdrAudioEnabled", checked)
                dialog.scheduleSettingsPersist()
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Plays demodulated mono audio through the dedicated receiver output below. It is asynchronous and never feeds the FT8 decoder.")
        }
        // Keep the IF controls in their own two-column form.  The
        // surrounding audio grid has four columns; mixing its spare
        // columns with three-column spans caused labels and controls
        // to wrap independently on wide displays.
        Item {
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
            implicitHeight: rtlIfSettingsGrid.implicitHeight

            GridLayout {
                id: rtlIfSettingsGrid
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                columns: 2
                columnSpacing: 10
                rowSpacing: 8

                Text { text: qsTr("Follow dial frequency:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                RtlCheckBox {
                    id: rtlFollowDialCheck
                    theme: settingsTab2
                    checked: bridge.getSetting("RtlSdrFollowDial", true)
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
                    Layout.preferredHeight: controlHeight
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    onClicked: {
                        bridge.setSetting("RtlSdrFollowDial", checked)
                        dialog.scheduleSettingsPersist()
                    }
                }

                Text { text: qsTr("Use receiver IF output:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                RtlCheckBox {
                    id: rtlIfEnabledCheck
                    theme: settingsTab2
                    checked: bridge.getSetting("RtlSdrIfEnabled", false)
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
                    Layout.preferredHeight: controlHeight
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    onClicked: {
                        bridge.setSetting("RtlSdrIfEnabled", checked)
                        dialog.scheduleSettingsPersist()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Keeps the radio dial frequency for display, logging and decoding while the RTL-SDR is tuned to the receiver's fixed IF output.")
                }

                Text { text: qsTr("IF frequency (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                DecoTextField {
                    id: rtlIfFrequencyField
                    text: String(bridge.getSetting("RtlSdrIfFrequencyHz", 8830000))
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlIfEnabledCheck.checked
                    Layout.fillWidth: true
                    implicitHeight: controlHeight
                    validator: IntValidator { bottom: 100000; top: 1766000000 }
                    color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
                    background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    onEditingFinished: {
                        bridge.setSetting("RtlSdrIfFrequencyHz", Number(text))
                        dialog.scheduleSettingsPersist()
                    }
                }

                Text { text: qsTr("IF sideband:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                DecoComboBox {
                    id: rtlIfSidebandCombo
                    property var sidebandKeys: ["auto", "usb", "lsb"]
                    model: [qsTr("Automatic"), qsTr("USB"), qsTr("LSB")]
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlIfEnabledCheck.checked
                    Layout.fillWidth: true
                    implicitHeight: controlHeight
                    currentIndex: Math.max(0, sidebandKeys.indexOf(bridge.getSetting("RtlSdrIfSideband", "auto")))
                    onActivated: {
                        bridge.setSetting("RtlSdrIfSideband", sidebandKeys[currentIndex])
                        dialog.scheduleSettingsPersist()
                    }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    contentItem: Text { text: rtlIfSidebandCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                    delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                        background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                    popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Automatic uses LSB only with the LSB demodulator; weak-signal modes such as FT8 use USB.")
                }

                Text { text: qsTr("USB shift (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                DecoTextField {
                    id: rtlIfUsbShiftField
                    text: String(bridge.getSetting("RtlSdrIfUsbShiftHz", 1500))
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlIfEnabledCheck.checked
                    Layout.fillWidth: true
                    implicitHeight: controlHeight
                    validator: IntValidator { bottom: -500000; top: 500000 }
                    color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
                    background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    onEditingFinished: {
                        bridge.setSetting("RtlSdrIfUsbShiftHz", Number(text))
                        dialog.scheduleSettingsPersist()
                    }
                }

                Text { text: qsTr("LSB shift (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                DecoTextField {
                    id: rtlIfLsbShiftField
                    text: String(bridge.getSetting("RtlSdrIfLsbShiftHz", -1500))
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlIfEnabledCheck.checked
                    Layout.fillWidth: true
                    implicitHeight: controlHeight
                    validator: IntValidator { bottom: -500000; top: 500000 }
                    color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
                    background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    onEditingFinished: {
                        bridge.setSetting("RtlSdrIfLsbShiftHz", Number(text))
                        dialog.scheduleSettingsPersist()
                    }
                }

                Text { text: qsTr("Invert IF spectrum:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                RtlCheckBox {
                    id: rtlIfSpectrumInvertedCheck
                    theme: settingsTab2
                    checked: bridge.getSetting("RtlSdrIfSpectrumInverted", false)
                    enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlIfEnabledCheck.checked
                    Layout.preferredHeight: controlHeight
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    onClicked: {
                        bridge.setSetting("RtlSdrIfSpectrumInverted", checked)
                        dialog.scheduleSettingsPersist()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Enable this when signals move in the opposite direction on the panadapter because the receiver's IF mixer reverses the spectrum.")
                }

                Text {
                    visible: rtlIfEnabledCheck.checked
                    text: qsTr("IF mode: Decodium keeps the radio dial frequency on screen and tunes the RTL-SDR to IF plus the selected USB/LSB shift.")
                    color: secondaryCyan
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
            }
        }

        Text { text: qsTr("Receiver speaker output:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: rtlAudioOutputCombo
            model: {
                var outputs = [qsTr("System default")]
                for (var i = 0; i < bridge.audioOutputDevices.length; ++i)
                    outputs.push(bridge.audioOutputDevices[i])
                return outputs
            }
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
                     && rtlDemodCombo.currentIndex !== 0
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            currentIndex: {
                var wanted = bridge.getSetting("RtlSdrAudioOutputDevice", "")
                if (!wanted)
                    return 0
                for (var i = 0; i < bridge.audioOutputDevices.length; ++i) {
                    if (bridge.audioOutputDevices[i] === wanted)
                        return i + 1
                }
                return 0
            }
            onActivated: {
                bridge.setSetting("RtlSdrAudioOutputDevice",
                                  currentIndex === 0 ? "" : currentText)
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlAudioOutputCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.width: Math.max(rtlAudioOutputCombo.width, 560)
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("System default normally selects the computer speakers and is independent from Decodium's TX audio output.")
        }

        Text { text: qsTr("RF frequency (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            id: rtlFrequencyField
            text: String(bridge.getSetting("RtlSdrFrequencyHz", Math.round(bridge.frequency)))
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && !rtlFollowDialCheck.checked
            Layout.fillWidth: true
            implicitHeight: controlHeight
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            validator: IntValidator {
                bottom: rtlModeCombo.currentIndex === 1 ? 500000 : 100000
                top: rtlModeCombo.currentIndex === 1 ? 24000000 : 1766000000
            }
            color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                bridge.setSetting("RtlSdrFrequencyHz", Number(text))
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("Sample rate:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: rtlSampleRateCombo
            model: rtlDemodCombo.currentIndex === 1 ? ["960000", "1200000"] : ["240000", "288000"]
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
                     && rtlModeCombo.currentIndex !== 1
            Layout.fillWidth: true
            implicitHeight: controlHeight
            currentIndex: {
                var wanted = bridge.getSetting("RtlSdrSampleRate", rtlDemodCombo.currentIndex === 1 ? 960000 : 240000)
                var index = model.indexOf(String(wanted))
                return index >= 0 ? index : 0
            }
            onActivated: {
                bridge.setSetting("RtlSdrSampleRate", Number(currentText))
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlSampleRateCombo.displayText + " sps"; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData + " sps"; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text {
            text: rtlDemodCombo.currentIndex === 1
                  ? qsTr("RF spectrum: centre ±480 kHz or more; receiver audio is Wide FM at 48 kHz.")
                  : qsTr("RF spectrum is derived from complex IQ, independent from decoder audio.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
        }
        Text { text: qsTr("PPM correction:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            id: rtlPpmField
            text: String(bridge.getSetting("RtlSdrPpmCorrection", 0))
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.fillWidth: true
            implicitHeight: controlHeight
            validator: IntValidator { bottom: -500; top: 500 }
            color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                bridge.setSetting("RtlSdrPpmCorrection", Number(text))
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("Tuner AGC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        RtlCheckBox {
            id: rtlAgcCheck
            theme: settingsTab2
            checked: bridge.getSetting("RtlSdrGainTenthsDb", -1) < 0
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.preferredHeight: controlHeight
            onClicked: {
                bridge.setSetting("RtlSdrGainTenthsDb", checked ? -1 : 200)
                dialog.scheduleSettingsPersist()
            }
        }
        Text { text: qsTr("Manual gain (dB):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            id: rtlGainField
            text: (bridge.getSetting("RtlSdrGainTenthsDb", -1) < 0) ? "" : String(bridge.getSetting("RtlSdrGainTenthsDb", -1) / 10.0)
            placeholderText: qsTr("Automatic")
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
                     && rtlModeCombo.currentIndex !== 1 && !rtlAgcCheck.checked
            Layout.fillWidth: true
            implicitHeight: controlHeight
            validator: DoubleValidator { bottom: 0; top: 50; decimals: 1 }
            color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                bridge.setSetting("RtlSdrGainTenthsDb", Math.round(Number(text) * 10))
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("Digital AGC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        RtlCheckBox {
            theme: settingsTab2
            checked: bridge.getSetting("RtlSdrDigitalAgc", false)
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.preferredHeight: controlHeight
            onClicked: {
                bridge.setSetting("RtlSdrDigitalAgc", checked)
                dialog.scheduleSettingsPersist()
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Usually leave this disabled. It can raise the noise floor on RTL-SDR Blog V4 receivers.")
        }
        Item { Layout.columnSpan: 2; Layout.fillWidth: true; implicitHeight: 1 }

        Text { text: qsTr("Bias tee:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        RtlCheckBox {
            theme: settingsTab2
            checked: bridge.getSetting("RtlSdrBiasTee", false)
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.preferredHeight: controlHeight
            onClicked: {
                bridge.setSetting("RtlSdrBiasTee", checked)
                dialog.scheduleSettingsPersist()
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Supplies power to an external active antenna or LNA only when required by that equipment.")
        }
        Text { text: qsTr("Audio gain:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: compactSettingsLayout ? 132 : 172; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        Slider {
            id: rtlAudioGainSlider
            from: 0.1; to: 4.0; stepSize: 0.1; live: true
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked
            Layout.fillWidth: true
            Binding on value { value: bridge.getSetting("RtlSdrAudioGain", 1.0); when: !rtlAudioGainSlider.pressed }
            onPressedChanged: {
                if (!pressed) {
                    bridge.setSetting("RtlSdrAudioGain", value)
                    dialog.scheduleSettingsPersist()
                }
            }
        }

        Text {
            visible: rtlDemodCombo.ssbMode
            text: qsTr("SSB voice bandwidth:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: labelWidth
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
        }
        DecoComboBox {
            id: rtlSsbBandwidthCombo
            visible: rtlDemodCombo.ssbMode
            property var bandwidthValues: [1800, 2400, 3000, 3500, 4000]
            model: ["1.8 kHz", "2.4 kHz", "3.0 kHz", "3.5 kHz", "4.0 kHz"]
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlDemodCombo.ssbMode
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            implicitHeight: controlHeight
            currentIndex: Math.max(0, bandwidthValues.indexOf(
                Number(bridge.getSetting("RtlSdrSsbVoiceBandwidthHz", 3500))))
            onActivated: {
                bridge.setSetting("RtlSdrSsbVoiceBandwidthHz", bandwidthValues[currentIndex])
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlSsbBandwidthCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Post-demodulation audio filter only. It does not change the radio dial or RTL-SDR tuning.")
        }

        Text {
            visible: rtlDemodCombo.ssbMode
            text: qsTr("SSB audio AGC:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: labelWidth
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
        }
        DecoComboBox {
            id: rtlSsbAgcCombo
            visible: rtlDemodCombo.ssbMode
            property var agcValues: ["off", "slow", "medium"]
            model: [qsTr("Off"), qsTr("Slow"), qsTr("Medium")]
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlDemodCombo.ssbMode
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            implicitHeight: controlHeight
            currentIndex: Math.max(0, agcValues.indexOf(
                String(bridge.getSetting("RtlSdrSsbAgcMode", "slow")).toLowerCase()))
            onActivated: {
                bridge.setSetting("RtlSdrSsbAgcMode", agcValues[currentIndex])
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlSsbAgcCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Adjusts listening level after SSB demodulation. Off preserves the raw receiver level.")
        }

        Text {
            visible: rtlDemodCombo.ssbMode
            text: qsTr("SSB notch (Hz):")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: labelWidth
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
        }
        DecoTextField {
            id: rtlSsbNotchField
            visible: rtlDemodCombo.ssbMode
            text: String(bridge.getSetting("RtlSdrSsbNotchFrequencyHz", 0))
            placeholderText: qsTr("0 = Off")
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlDemodCombo.ssbMode
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            implicitHeight: controlHeight
            validator: IntValidator { bottom: 0; top: 4800 }
            color: textPrimary
            font.pixelSize: controlFontSize
            leftPadding: 8
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                bridge.setSetting("RtlSdrSsbNotchFrequencyHz", Number(text))
                dialog.scheduleSettingsPersist()
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Set the audible whistle frequency to remove a persistent carrier. Use 0 to disable.")
        }

        Text {
            visible: rtlDemodCombo.ssbMode
            text: qsTr("SSB noise reduction:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: labelWidth
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
        }
        DecoComboBox {
            id: rtlSsbNoiseReductionCombo
            visible: rtlDemodCombo.ssbMode
            property var reductionValues: ["off", "light", "medium"]
            model: [qsTr("Off"), qsTr("Light"), qsTr("Medium")]
            enabled: bridge.rtlSdrSupported && rtlEnabledCheck.checked && rtlDemodCombo.ssbMode
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            implicitHeight: controlHeight
            currentIndex: Math.max(0, reductionValues.indexOf(
                String(bridge.getSetting("RtlSdrSsbNoiseReduction", "off")).toLowerCase()))
            onActivated: {
                bridge.setSetting("RtlSdrSsbNoiseReduction", reductionValues[currentIndex])
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: rtlSsbNoiseReductionCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Adaptive speech gate for background noise in pauses. It never affects digital-mode decoder audio.")
        }

        Text {
            visible: rtlDemodCombo.ssbMode
            text: qsTr("These controls apply only to RTL-SDR USB/LSB listening after demodulation; they never alter FT8 or the radio tuning reference.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
        }

        Text {
            text: bridge.rtlSdrStatus
            color: bridge.rtlSdrRunning ? secondaryCyan : textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
            Layout.topMargin: 2
        }

        // ── Livelli ──
        Text { text: qsTr("LEVELS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("RX Input Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
	                        RowLayout {
	                            Layout.fillWidth: true
	                            Layout.columnSpan: Math.max(1, pageColumns - 1)
	                            spacing: 8
	                            Slider {
	                                id: setupRxInputLevelSlider
	                                from: 0; to: 100; live: true; stepSize: 1
	                                Layout.fillWidth: true
	                                Binding on value { value: bridge.rxInputLevel; when: !setupRxInputLevelSlider.pressed }
		                                onMoved: {
		                                    bridge.rxInputLevel = value
		                                    dialog.scheduleSettingsPersist()
		                                }
		                                onPressedChanged: {
		                                    if (!pressed && Math.abs(bridge.rxInputLevel - value) >= 0.5) {
		                                        bridge.rxInputLevel = value
		                                        dialog.scheduleSettingsPersist()
		                                    }
		                                }
	                            }
	                            Text {
	                                text: Math.round(bridge.rxInputLevel)
	                                color: secondaryCyan
	                                font.pixelSize: 11
	                                font.family: decodiumMonoFontFamily
	                                Layout.preferredWidth: 28
	                                horizontalAlignment: Text.AlignRight
	                            }
	                            Rectangle {
	                                Layout.preferredWidth: 48
	                                Layout.preferredHeight: 22
	                                radius: 4
	                                color: bridge.autoRxInputLevel
	                                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
	                                       : bgMedium
	                                border.color: bridge.autoRxInputLevel ? secondaryCyan : glassBorder
	                                Text {
	                                    anchors.centerIn: parent
	                                    text: "AUTO"
	                                    color: bridge.autoRxInputLevel ? secondaryCyan : textSecondary
	                                    font.pixelSize: 10
	                                    font.bold: true
	                                }
	                                MouseArea {
	                                    id: setupRxAutoMouse
	                                    anchors.fill: parent
	                                    hoverEnabled: true
	                                    cursorShape: Qt.PointingHandCursor
		                                    onClicked: {
		                                        bridge.autoRxInputLevel = !bridge.autoRxInputLevel
		                                        dialog.scheduleSettingsPersist()
		                                    }
	                                }
	                                ToolTip.visible: setupRxAutoMouse.containsMouse
	                                ToolTip.text: bridge.autoRxInputLevel ? qsTr("Auto RX level active")
	                                                                       : qsTr("Auto RX level disabled")
	                            }
	                        }

        Text { text: qsTr("TX Output Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        Slider {
            id: setupTxOutputLevelSlider
            from: 450; to: 0; live: true; stepSize: 1
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            Binding on value { value: bridge.txOutputLevel; when: !setupTxOutputLevelSlider.pressed }
	                            onMoved: {
	                                bridge.txOutputLevel = value
	                                dialog.scheduleSettingsPersist()
	                            }
	                            onPressedChanged: {
	                                if (!pressed && Math.abs(bridge.txOutputLevel - value) >= 0.5) {
	                                    bridge.txOutputLevel = value
	                                    dialog.scheduleSettingsPersist()
	                                }
	                            }
        }

        // ── Directory ──
        Text { text: qsTr("DIRECTORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Save Directory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            id: saveDirectoryField
            text: bridge.getSetting("SaveDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: Math.max(1, pageColumns - 1)
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            readOnly: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("SaveDirectory", text)
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: dialog.openDirectoryPicker("SaveDirectory", saveDirectoryField.text)
            }
        }

        Text { text: qsTr("AzEl Directory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            id: azElDirectoryField
            text: bridge.getSetting("AzElDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: Math.max(1, pageColumns - 1)
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            readOnly: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("AzElDirectory", text)
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: dialog.openDirectoryPicker("AzElDirectory", azElDirectoryField.text)
            }
        }

        // ── Power Memory ──
        Text { text: qsTr("POWER MEMORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Band TX Memory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("PowerBandTXMemory", false)
            onCheckedChanged: bridge.setSetting("PowerBandTXMemory", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Band Tune Mem:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("PowerBandTuneMemory", false)
            onCheckedChanged: bridge.setSetting("PowerBandTuneMemory", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
    }
}
