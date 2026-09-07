/* Decodium 4.0 - lazy Settings tab */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SettingsPageScroll {
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

        // ── Frequenza e Timing ──
        Text { text: qsTr("FREQUENCY AND TIMING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("TX Frequency:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: txFreqSpin
            from: 0; to: 5000; value: bridge.txFrequency; editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: {
                if (bridge.txFrequency !== value)
                    bridge.txFrequency = value
                bridge.setSetting("txFrequency", value)
            }
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: txFreqSpin.textFromValue(txFreqSpin.value, txFreqSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txFreqSpin.editable; validator: txFreqSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("TX Slot:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: txSlotCombo
            model: [qsTr("Second (:15/:45)"), qsTr("First (:00/:30)")]
            currentIndex: bridge.txPeriod === 1 ? 1 : 0
            Layout.fillWidth: true; Layout.minimumWidth: comboFieldMinWidth; Layout.preferredWidth: comboFieldMinWidth; implicitHeight: controlHeight
            onActivated: {
                bridge.txPeriod = currentIndex === 1 ? 1 : 0
                bridge.setSetting("txPeriod", bridge.txPeriod)
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: txSlotCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; rightPadding: 42; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("TX Delay (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: txDelaySpin
            from: 0; to: 5; stepSize: 1; value: Math.round(Number(bridge.getSetting("TxDelay", 0.2)) * 10); editable: true
            textFromValue: function(value, locale) { return Number(value / 10).toLocaleString(locale, "f", 1) }
            valueFromText: function(text, locale) {
                var parsed = Number.fromLocaleString(locale, text)
                return isNaN(parsed) ? 0 : Math.round(parsed * 10)
            }
            validator: DoubleValidator { bottom: 0.0; top: 0.5; decimals: 1; notation: DoubleValidator.StandardNotation }
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: bridge.setSetting("TxDelay", value / 10)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: txDelaySpin.textFromValue(txDelaySpin.value, txDelaySpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txDelaySpin.editable; validator: txDelaySpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Allow TX QSY:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            checked: bridge.getSetting("TxQSYAllowed", false)
            onCheckedChanged: bridge.setSetting("TxQSYAllowed", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        // ── Ready profiles (1.0.384) ──
        // Apply a coherent set of FT2/decode toggles as a group.
        // Selettore rapido equivalente anche in toolbar (accanto a Setup).
        Text { text: qsTr("READY PROFILES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }
        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            implicitHeight: readyProfilesColumn.implicitHeight
            ColumnLayout {
                id: readyProfilesColumn
                width: parent.width
                spacing: 8
                Repeater {
                    model: [
                        { pid: "balanced", name: qsTr("Balanced (daily QSO) - default"),
                          desc: qsTr("Conservative ON · full decode AutoCQ ON · close strong partners ON · adaptive decode ON · AP cache rescue ON · skip end-slot OFF · MAM OFF · partner memory ON · TX2 resend ON · smooth flow ON · caller retries 5.") },
                        { pid: "weak", name: qsTr("Weak-signal / DX hunting"),
                          desc: qsTr("Like Balanced, but: caller retries 7 · adaptive decode OFF (maximum sensitivity) · AP cache rescue ON (accepts some false positives) · skip end-slot OFF (do not lose late decodes).") },
                        { pid: "contest", name: qsTr("Contest / high density"),
                          desc: qsTr("close strong partners ON · skip end-slot ON (minimum latency) · MAM multi-stream ON (2 streams, experimental) · full decode AutoCQ ON · caller retries 3 · partner memory ON · conservative OFF.") },
                        { pid: "cpu", name: qsTr("CPU-limited (Decodium Console / mini PC)"),
                          desc: qsTr("adaptive decode ON · MAM OFF · full decode AutoCQ OFF · smooth flow ON · rest at defaults. Watchdogs unchanged.") }
                    ]
                    delegate: Rectangle {
                        id: profileEntry
                        required property var modelData
                        readonly property bool isActive: bridge && bridge.activeReadyProfile === modelData.pid
                        Layout.fillWidth: true
                        implicitHeight: profileEntryCol.implicitHeight + 16
                        radius: 6
                        color: isActive ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18) : bgMedium
                        border.color: isActive ? primaryBlue : glassBorder
                        border.width: 1
                        ColumnLayout {
                            id: profileEntryCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 8
                            spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text { text: modelData.name; color: textPrimary; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                Text { text: qsTr("● active"); color: primaryBlue; font.pixelSize: 11; font.bold: true; visible: profileEntry.isActive }
                            }
                            Text { text: modelData.desc; color: textSecondary; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (bridge) bridge.applyReadyProfile(modelData.pid)
                        }
                    }
                }
            }
        }

        // ── Sequenza Automatica ──
        Text { text: qsTr("AUTO SEQUENCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            implicitHeight: autoSequenceGrid.implicitHeight

            GridLayout {
                id: autoSequenceGrid
                width: parent.width
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                property int checkWidth: 34
                property int valueWidth: 110
                property real labelWidth: Math.max(240, width - valueWidth - columnSpacing)

                Text {
                    text: qsTr("Auto Sequence:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge.autoSeq
                    onCheckedChanged: {
                        bridge.autoSeq = checked
                        bridge.setSetting("autoSeq", checked)
                        bridge.setSetting("AutoSeq", checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text {
                    text: qsTr("Send RR73:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge.sendRR73
                    onCheckedChanged: {
                        bridge.sendRR73 = checked
                        bridge.setSetting("sendRR73", checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text {
                    text: qsTr("Quick QSO:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge.quickQsoEnabled
                    onCheckedChanged: {
                        bridge.quickQsoEnabled = checked
                        bridge.setSetting("quickQsoEnabled", checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                // 1.0.304 (#9) — resume-on-reply: riprende il QSO se il partner
                // torna a rispondere entro 2 min dall'Halt. Opt-in, default OFF.
                Text {
                    text: qsTr("Resume QSO on partner reply:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.resumeQsoOnReply : false
                    onCheckedChanged: {
                        if (bridge) {
                            bridge.resumeQsoOnReply = checked
                            dialog.scheduleSettingsPersist()
                        }
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("If you Halt during an active QSO and that same station sends a direct reply to your callsign within 2 minutes, Decodium can resume that QSO.\n\nApplies only to FT8/FT4/FT2 and only to the saved QSO state.\n\nDefault: OFF (= Halt fully stops the sequence by default).")
                }
                Text {
                    text: qsTr("Disable TX after 73:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge.getSetting("73TxDisable", true)
                    onCheckedChanged: bridge.setSetting("73TxDisable", checked)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text {
                    text: qsTr("MSK/Q65 TX until 73:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge.getSetting("RepeatTx", false)
                    onCheckedChanged: bridge.setSetting("RepeatTx", checked)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                // ══════════ FT2 UTILITY ══════════
                Text { text: qsTr("FT2 UTILITY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 10 }
                Rectangle { Layout.fillWidth: true; Layout.columnSpan: 2; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                // 1.0.311 — FT2: ripetizioni del 73/RR73 finale regolabili (era fisso 8)
                Text {
                    text: qsTr("FT2: signoff retries (73/RR73):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                SpinBox {
                    id: ft2SignoffCapSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 1; to: 8; editable: true
                    value: bridge ? bridge.ft2SignoffRetryCap : 4
                    onValueChanged: if (bridge && bridge.ft2SignoffRetryCap !== value) bridge.setFt2SignoffRetryCap(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: ft2SignoffCapSpin.textFromValue(ft2SignoffCapSpin.value, ft2SignoffCapSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft2SignoffCapSpin.editable; validator: ft2SignoffCapSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("How many times to repeat the final 73/RR73 in FT2 waiting for the partner's ack before logging and closing.\n\nDefault: 4 (~28s).\n\nLower = closes earlier (less 'stuck' on the same station).\nHigher = more patient with weak/QSB partners.\n\nDoesn't affect FT8/FT4.")
                }

                // 1.0.315 — FT4: ripetizioni del 73/RR73 finale regolabili
                Text {
                    text: qsTr("FT4: signoff retries (73/RR73):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                SpinBox {
                    id: ft4SignoffCapSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 1; to: 8; editable: true
                    value: bridge ? bridge.ft4SignoffRetryCap : 4
                    onValueChanged: if (bridge && bridge.ft4SignoffRetryCap !== value) bridge.setFt4SignoffRetryCap(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: ft4SignoffCapSpin.textFromValue(ft4SignoffCapSpin.value, ft4SignoffCapSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft4SignoffCapSpin.editable; validator: ft4SignoffCapSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("How many times to repeat the final 73/RR73 in FT4.\n\nDefault: 4 (~30s).\n\nIncrease to 6-8 for weak/QSB partners (replaces the former automatic weak/conservative extras).\n\nDoesn't affect FT2/FT8.")
                }

                // 1.0.315 — FT8: ripetizioni del 73/RR73 finale regolabili
                Text {
                    text: qsTr("FT8: signoff retries (73/RR73):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                SpinBox {
                    id: ft8SignoffCapSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 1; to: 8; editable: true
                    value: bridge ? bridge.ft8SignoffRetryCap : 3
                    onValueChanged: if (bridge && bridge.ft8SignoffRetryCap !== value) bridge.setFt8SignoffRetryCap(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: ft8SignoffCapSpin.textFromValue(ft8SignoffCapSpin.value, ft8SignoffCapSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft8SignoffCapSpin.editable; validator: ft8SignoffCapSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("How many times to repeat the final 73/RR73 in FT8.\n\nDefault: 3 (~45s).\n\nIncrease to 6-8 for weak/QSB partners (replaces the former automatic weak/conservative extras).\n\nDoesn't affect FT2/FT4.")
                }

                // 1.0.437 - opt-in: extra signoff retries per partner debole (FTX)
                Text {
                    text: qsTr("Weak-partner signoff boost (FT2/4/8):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ftxWeakBoostCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ftxWeakSignoffBoost : false
                    onCheckedChanged: if (bridge && bridge.ftxWeakSignoffBoost !== checked) bridge.setFtxWeakSignoffBoost(checked)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("When ON, automatically grants extra final 73/RR73 retries when the active partner is weak (SNR at or below the threshold below), giving fragile QSOs more chances to close.\n\nDefault OFF = unchanged behavior. Applies to FT2/FT4/FT8, always clamped to max 8 and still bounded by the TX watchdog.")
                }
                Text {
                    text: qsTr("  weak SNR threshold (dB):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                    enabled: ftxWeakBoostCheck.checked
                }
                SpinBox {
                    id: ftxWeakSnrSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: -30; to: -5; editable: true
                    enabled: ftxWeakBoostCheck.checked
                    value: bridge ? bridge.ftxWeakSnrThreshold : -15
                    onValueChanged: if (bridge && bridge.ftxWeakSnrThreshold !== value) bridge.setFtxWeakSnrThreshold(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: ftxWeakSnrSpin.textFromValue(ftxWeakSnrSpin.value, ftxWeakSnrSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ftxWeakSnrSpin.editable; validator: ftxWeakSnrSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("A partner whose SNR is at or below this value is treated as 'weak' and receives the extra signoff retries.\n\nDefault: -15 dB.")
                }
                Text {
                    text: qsTr("  extra signoff retries:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                    enabled: ftxWeakBoostCheck.checked
                }
                SpinBox {
                    id: ftxWeakBonusSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 1; to: 6; editable: true
                    enabled: ftxWeakBoostCheck.checked
                    value: bridge ? bridge.ftxWeakSignoffBonus : 3
                    onValueChanged: if (bridge && bridge.ftxWeakSignoffBonus !== value) bridge.setFtxWeakSignoffBonus(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: ftxWeakBonusSpin.textFromValue(ftxWeakBonusSpin.value, ftxWeakBonusSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ftxWeakBonusSpin.editable; validator: ftxWeakBonusSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("How many extra final 73/RR73 retries to add on top of the per-mode cap for weak partners.\n\nDefault: +3 (capped so the total never exceeds 8).")
                }

                // 1.0.446 - opt-in: guard ri-aggancio RRR post-log (caso 9H1SR "troppe richiamate")
                Text {
                    text: qsTr("Post-log RRR re-engage guard (FT2):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2ReengageGuardCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2PostLogReengageGuard : false
                    onCheckedChanged: if (bridge && bridge.ft2PostLogReengageGuard !== checked) bridge.setFt2PostLogReengageGuard(checked)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("When ON, after a QSO is logged ('partner left') stop re-sending RR73 to a partner that keeps calling you with R+report because they did not copy your signoff (the 9H1SR too-many-calls case).\n\nA few courtesy repeats are still allowed (see max), then suppressed within the 30s cooldown. Default OFF. FT2 only.")
                }
                Text {
                    text: qsTr("  courtesy RRR max:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                    enabled: ft2ReengageGuardCheck.checked
                }
                SpinBox {
                    id: ft2ReengageMaxSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 0; to: 5; editable: true
                    enabled: ft2ReengageGuardCheck.checked
                    value: bridge ? bridge.ft2PostLogReengageMax : 1
                    onValueChanged: if (bridge && bridge.ft2PostLogReengageMax !== value) bridge.setFt2PostLogReengageMax(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: ft2ReengageMaxSpin.textFromValue(ft2ReengageMaxSpin.value, ft2ReengageMaxSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft2ReengageMaxSpin.editable; validator: ft2ReengageMaxSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("How many courtesy RR73 to still send to a just-logged partner before suppressing further re-engagements.\n\n0 = suppress immediately. Default: 1.")
                }

                // 1.0.314 — opt-in: TX immediato al click (stile 1.0.283)
                Text {
                    text: qsTr("Immediate TX on click (1.0.283 style):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ftxImmediateClickCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ftxImmediateClickTx : false
                    onCheckedChanged: {
                        if (bridge && bridge.ftxImmediateClickTx !== checked)
                            bridge.setFtxImmediateClickTx(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Restores the 'TX starts IMMEDIATELY on double-click' behaviour of 1.0.283.\n\n• FT2: relaxes the period-gate (TX1 from click bypasses waiting for the next slot)\n• FT8/FT4: raises the clickable window cap to d3CapMs (~11s on FT8, 5.6s on FT4) = real 1.0.283 behaviour\n\nDefault: OFF (= safe upstream behaviour).\n\nEnable if it bothers you to wait 1 cycle after the click.")
                }

                // 1.0.371 - opt-in: logga RR73 (TX4) anche se il partner sparisce (FT2 async AutoCQ)
                Text {
                    text: qsTr("Log RR73 even if partner leaves (FT2):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2LogRr73OnPartnerLeftCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2LogRr73OnPartnerLeft : false
                    onCheckedChanged: {
                        if (bridge && bridge.ft2LogRr73OnPartnerLeft !== checked)
                            bridge.setFt2LogRr73OnPartnerLeft(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("FT2 + async AutoCQ: when WE close with RR73 (TX4) after the partner R+report and the partner then disappears, log the QSO anyway (at the signoff cap) instead of leaving it unlogged.\n\nMatches TX5/73 and sync mode behaviour.\n\nDefault: OFF.")
                }

                // 1.0.317 — opt-in: FT8 fast sequence (grace ridotta + late-decode accept)
                Text {
                    text: qsTr("FT8: fast sequences (WSJT-X/JTDX style):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft8FastSequenceCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft8FastSequence : false
                    onCheckedChanged: {
                        if (bridge && bridge.ft8FastSequence !== checked)
                            bridge.setFt8FastSequence(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Reduces FT8 sequence waits for users who prefer WSJT-X/JTDX-style reactivity.\n\nTwo changes:\n  (1) Boundary grace 1200ms → 400ms = TX starts ~800ms earlier after the slot boundary\n  (2) onFt8DecodeReady accepts late decodes within d3CapMs (~11s) instead of dropping the slot = no more '15s extra after the partner's reply'\n\nSAFETY: under CPU pressure the pre-existing clamp forces grace ≥900ms (safety > reactivity on loaded PCs).\n\nDefault: OFF (= conservative upstream behaviour, max decode reliability).")
                }

                // 1.0.367 — opt-in: finestra TX FT2 async conservativa (default ON = calmo/stabile)
                Text {
                    text: qsTr("FT2: conservative TX window (no truncated frames):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2ConservativeTimingCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2ConservativeTiming : true
                    onCheckedChanged: {
                        if (bridge && bridge.ft2ConservativeTiming !== checked)
                            bridge.setFt2ConservativeTiming(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Controls how late in a slot the async FT2 TX may start.\n\n• ON (default): the TX starts only if the FULL payload (~2520ms) still fits — window ~18% of the slot. If it would arrive late, the TX is deferred to the next slot instead of sending a TRUNCATED frame the partner can't decode. Calm, Decodium-3.0-style stability.\n• OFF: FIX B (1.0.353) behaviour — window up to ~76% of the slot (only ~700ms of useful payload required). More reactive but can transmit truncated frames on a late reply.\n\nEnable OFF only if you want maximum reactivity and accept occasional non-decodable late TX.")
                }

                // 1.0.321 — opt-in: FT2 manual one-shot disarm (Salvatore 1.0.300 latch fix)
                Text {
                    text: qsTr("FT2: manual one-shot disarm (1.0.300+):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2ManualOneShotCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2ManualOneShotEnabled : false
                    onCheckedChanged: {
                        if (bridge && bridge.ft2ManualOneShotEnabled !== checked)
                            bridge.setFt2ManualOneShotEnabled(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("When ON (upstream 1.0.300+ behaviour): after a manual TX1-TX3 in FT2 the TX is disarmed and re-armed ONLY when a partner decode arrives. Avoids TX1 looping forever on double-click, but on WEAK partners that don't decode in the first RX period the QSO is lost (= 'TX1 stops without completing').\n\nWhen OFF (default on this fork, pre-1.0.300): TX1 keeps repeating until 'Caller Retries' is reached — better for weak-signal QSOs (Pasquale's case).\n\nEnable only if you double-click stations that consistently reply on the first attempt.")
                }

                // 1.0.321 — Caller retries (era Q_PROPERTY non esposta in UI)
                Text {
                    text: qsTr("Caller retries (max TX repeats per step):")
                    color: textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.columnSpan: 2
                    Layout.preferredHeight: Math.max(controlHeight, implicitHeight)
                }
                SpinBox {
                    id: maxCallerRetriesSpin
                    Layout.columnSpan: 2
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 1; to: 99; editable: true
                    value: bridge ? bridge.maxCallerRetries : 10
                    onValueChanged: if (bridge && bridge.maxCallerRetries !== value) bridge.setMaxCallerRetries(value)
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: maxCallerRetriesSpin.textFromValue(maxCallerRetriesSpin.value, maxCallerRetriesSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !maxCallerRetriesSpin.editable; validator: maxCallerRetriesSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    // 1.0.493 — segnale visivo: col watchdog owner (hard cap OFF) questo numero è ignorato
                    opacity: (bridge && bridge.txWatchdogMode !== 0 && !bridge.callerRetriesAlwaysHard) ? 0.5 : 1.0
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Maximum times the same TX step (TX1/TX2/TX3) repeats before halting if the partner doesn't reply.\n\nDefault: 10.\n\nFT2 (slot 3.75s): 10 retries ≈ 38s of calling.\nFT8 (slot 15s): 10 retries ≈ 150s.\n\nLower (4-6) = less time wasted on stations that don't reply.\nHigher (15-20) = patience for weak DX / marginal propagation.\n\nNote: with 'FT2 manual one-shot disarm' OFF (default) this is what stops TX1 from looping forever.")
                }
                // 1.0.493 — avviso: watchdog owner del limite → il cap qui sopra è ignorato
                Text {
                    visible: bridge && bridge.txWatchdogMode !== 0 && !bridge.callerRetriesAlwaysHard
                    text: qsTr("⚠ TX Watchdog is active and 'hard cap' below is OFF: the Caller-retries limit above is IGNORED until the watchdog timeout.")
                    color: "#e6a23c"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.columnSpan: pageColumns
                    Layout.fillWidth: true
                }

                // 1.0.446 - P1-5 opt-in: cap Caller retries duro anche con TX watchdog ON
                Text {
                    text: qsTr("Caller retries hard cap (even with watchdog):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: callerRetriesAlwaysHardCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.callerRetriesAlwaysHard : true
                    onCheckedChanged: {
                        if (bridge && bridge.callerRetriesAlwaysHard !== checked)
                            bridge.setCallerRetriesAlwaysHard(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("When ON (default), the 'Caller retries' cap on TX1/TX2 halts the call even if the TX Watchdog is enabled — the number you set is a real hard limit.\n\nWhen OFF (upstream 1.0.438 behaviour): the TX Watchdog takes priority and ignores the Caller-retries cap until its own timeout, so a call can repeat for the whole watchdog duration (default 6 min).")
                }

                // 1.0.447 - Fondamenta Fase 1: censimento transizioni di stato FT2 (diagnostico, solo-log)
                Text {
                    text: qsTr("FT2 state transition census (log only):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2TransitionCensusCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2TransitionCensus : false
                    onCheckedChanged: {
                        if (bridge && bridge.ft2TransitionCensus !== checked)
                            bridge.setFt2TransitionCensus(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Diagnostic only (off by default): logs every FT2 QSO state transition (from/to/progress) to the diagnostic log, to empirically map the real sequencer state machine. No behaviour change at all - it only writes log lines. Used to design future deterministic-transition guards safely.")
                }

                // 1.0.447 - Leva#6-A: gate smart-TX adattivi all'occupazione canale (sperimentale)
                Text {
                    text: qsTr("Adaptive async TX timing (experimental):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2AdaptiveTxGatesCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2AdaptiveTxGates : false
                    onCheckedChanged: {
                        if (bridge && bridge.ft2AdaptiveTxGates !== checked)
                            bridge.setFt2AdaptiveTxGates(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Experimental, off by default. Makes the FT2 async-TX timing gates (RMS-quiet, decode-quiet, anti-collision jitter) adapt to channel occupancy: a bit more reactive when the channel is clear, more conservative when it is crowded. With this OFF the timing is byte-identical to the standard behaviour. It never transmits before hearing the partner.")
                }

                // Conservative FT2 (weak-signal mode) — opt-in tuning
                // anti-QSB: ghost filter rilassato, retry cap esteso SNR-
                // adattivo, same-step wait piu' permissivo per partner
                // marginali. Default OFF: comportamento standard FT2.
                Text {
                    text: qsTr("Conservative FT2 (weak-signal mode):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2ConservativeCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2Conservative : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2Conservative(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Anti-QSB tuning:\n  • Ghost filter -24 dB instead of -22\n  • Retry cap extended SNR-adaptive (+2..+4 extra)\n  • Same-step wait relaxed for weak partners\n\nDefault: OFF — enable it if you have weak DX partners or marginal propagation.")
                }

                // 1.0.289 — FT2 #1: piena profondità decode durante AutoCQ
                Text {
                    text: qsTr("FT2: full decode in AutoCQ:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2FullDecodeCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2FullDecodeInAutoCq : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2FullDecodeInAutoCq(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("While calling CQ (AutoCQ), keeps the decode depth at full (OSD + 4th subtraction pass + weak-signal averaging) instead of reducing it to 2.\n\nHelps you hear weak responders. Reduces automatically under CPU pressure anyway.\n\nDefault: OFF.")
                }

                // 1.0.289 — FT2 #3: chiusura rapida partner forti
                Text {
                    text: qsTr("FT2: close strong partners earlier:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2QuickGiveUpCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2QuickGiveUpStrong : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2QuickGiveUpStrong(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("If a STRONG partner (SNR > 0 dB) doesn't send the final 73, reduces RR73 repetitions from 8 to 4 (~15s instead of 30s) before logging and returning to CQ.\n\nWeak partners keep the extra anti-QSB repetitions.\n\nDefault: OFF.")
                }

                // 1.0.292 — FT2: decode adattivo (dedup re-decode async in solo-ascolto)
                Text {
                    text: qsTr("FT2: adaptive decode (CPU saver):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2AdaptiveDecodeCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2AdaptiveDecode : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2AdaptiveDecode(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("In LISTEN-ONLY mode (not calling CQ nor in a QSO), thins async re-decode from 100ms to ~350ms: doesn't re-decode 95%-overlapping audio → saves CPU and reduces the peaks that may lower decode depth.\n\nWhen waiting for a reply (AutoCQ/QSO) it stays at full cadence. Loses no decodes.\n\nUseful mainly on modest PCs.\n\nDefault: OFF.")
                }

                // Sprint2-1 — FT2: narrow async decode (fast pass attorno a nfqso)
                Text {
                    text: qsTr("FT2: narrow reply decode (experimental):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2NarrowAsyncDecodeCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2NarrowAsyncDecode : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2NarrowAsyncDecode(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("When WAITING FOR A REPLY (AutoCQ or active QSO), decodes a narrow window around your RX frequency (±150 Hz) instead of the whole band, with a full-band pass every 4th cycle.\n\nThe reply is decoded earlier in the slot (less CPU per attempt), so TX can react in the same slot instead of the next one. Band activity is still scanned 1 cycle out of 4.\n\nDefault: OFF.")
                }

                // 1.0.293/294 — FT2: AP hashed-callsign cache (Phase 1: display-only rescue)
                Text {
                    text: qsTr("FT2: AP cache rescue (experimental):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2ApHashCacheCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2ApHashCache : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2ApHashCache(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Experimental FT2 AP cache: stores callsigns seen in-band as hashes (TTL 30 min) and may rescue borderline FT2 decodes when a decoded callsign is already in the cache.\n\nSafety gate: AP-cache-rescued rows are shown/audited, but they do not drive AutoSeq, AutoCQ, or automatic TX. They are also not used to seed the AP cache again.\n\nDefault: OFF.")
                }

                // 1.0.355 — FT2: salta decode ridondante di fine-slot
                Text {
                    text: qsTr("FT2: skip redundant end-slot decode (reduces lock-in latency):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2AsyncSkipRedundantSyncDecodeCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2AsyncSkipRedundantSyncDecode : false
                    onToggled: {
                        if (bridge) {
                            bridge.ft2AsyncSkipRedundantSyncDecode = checked
                            dialog.scheduleSettingsPersist()
                        }
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("FT2 async only: when the asynchronous decode (incremental every 100 ms) has ALREADY decoded a slot, skip the end-of-slot synchronous decode pass for that slot.\n\nBenefit: removes contention (~1.8 s after TX) on the same worker, so the partner reply is picked up faster.\n\nCost: for slots already covered by async you lose the full end-of-slot weak-averaging pass, which can recover weak/marginal stations. Slots where async returned EMPTY still keep the sync decode.\n\nDefault: OFF.")
                }

                // 1.0.364+ — MAM multi-stream (MSHV): risponde a più
                // chiamanti nello stesso slot su frequenze diverse.
                // Opzione AGGIUNTIVA del MAM seriale. Default OFF.
                Text {
                    text: qsTr("FT2/FT8 MAM multi-stream (MSHV, sperimentale):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: mamMultiStreamCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.mamMultiStream : false
                    onToggled: {
                        if (bridge) {
                            bridge.mamMultiStream = checked
                            dialog.scheduleSettingsPersist()
                        }
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("MSHV multi-stream mode: in a single period it replies to MULTIPLE callers at the same time, each on ITS own audio frequency (like a DX-pedition station).\n\nThis is an ADDITIONAL MAM option: MAM (Multi-Answer Mode) or AutoCQ must be active before it can run. With this OFF, MAM remains serial, one caller at a time, as before.\n\nEXPERIMENTAL. Default: OFF.")
                }

                // 1.0.364+ — MAM multi-stream: numero massimo di stream simultanei
                Text {
                    text: qsTr("MAM multi-stream: max stream simultanei (1-10):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                SpinBox {
                    id: mamMaxStreamsSpin
                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                    Layout.alignment: Qt.AlignLeft
                    implicitHeight: controlHeight
                    from: 1; to: 10; editable: true
                    enabled: bridge ? bridge.mamMultiStream : false
                    opacity: enabled ? 1.0 : 0.4
                    value: bridge ? bridge.mamMaxStreams : 3
                    onValueModified: {
                        if (bridge && bridge.mamMaxStreams !== value) {
                            bridge.mamMaxStreams = value
                            dialog.scheduleSettingsPersist()
                        }
                    }
                    contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: mamMaxStreamsSpin.textFromValue(mamMaxStreamsSpin.value, mamMaxStreamsSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !mamMaxStreamsSpin.editable; validator: mamMaxStreamsSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("How many parallel QSOs MAM multi-stream can run at the same time, each on its own frequency.\n\nRange 1-10. Default: 3. With 1 you answer one caller at a time, but still on THEIR frequency (MSHV style).\n\nHigher values split the TX power across the streams (about 1/sqrt(N) each) and need more CPU to generate the overlapping audio. Enabled only when MAM multi-stream is active.")
                }

                // 1.0.187 — FT2 Weak-Signal Pack F v2: partner-memory cache (30s)
                Text {
                    text: qsTr("FT2 partner-memory (anti-QSB):")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2PartnerMemoryCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2PartnerMemoryEnabled : false
                    enabled: bridge ? bridge.ft2Conservative : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2PartnerMemoryEnabled(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2; opacity: parent.enabled ? 1.0 : 0.4 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Caches partner state (callsign + TX num + qsoProgress + SNR) for 30 seconds: if the partner disappears for QSB and reappears within 30s, restores the qsoProgress instead of restarting from TX1.\n\nRequires Conservative FT2 active.\n\nDefault: OFF (opt-in after the 1.0.186 revert — strict gate + [FT2WS-F] log). Automatically disabled if Conservative is OFF.")
                }

                // 1.0.187 — FT2 Weak-Signal Pack G: TX2 re-send forzato pre-fallback
                Text {
                    text: qsTr("FT2 TX2 re-send on stall:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: ft2Tx2ResendCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.ft2Tx2ResendOnStall : true
                    enabled: bridge ? bridge.ft2Conservative : false
                    onCheckedChanged: {
                        if (bridge) bridge.setFt2Tx2ResendOnStall(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2; opacity: parent.enabled ? 1.0 : 0.4 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("If you're in TX3 (R+report) and the partner doesn't reply for 2 periods (~7.5s), re-sends TX2 (signal report) once before leaving the QSO.\n\nHelps with weak partners that didn't ack the first time. Capped to 1 re-send per QSO (no loops).\n\nRequires Conservative FT2 active. Default: ON under Conservative.")
                }

                // Smooth decode flow (streaming progressivo FT8/FT4)
                // — spalma i decode dal batch a streaming continuo
                // stile WSJT-X live. Auto-fallback se UI stall.
                // Default ON; disattiva se vedi rallentamenti.
                Text {
                    text: qsTr("Smooth decode flow:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                    Layout.preferredHeight: controlHeight
                }
                CheckBox {
                    id: smoothDecodeFlowCheck
                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                    Layout.preferredHeight: controlHeight
                    checked: bridge ? bridge.smoothDecodeFlow : true
                    onCheckedChanged: {
                        if (bridge) bridge.setSmoothDecodeFlow(checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Spreads FT8/FT4 decodes from the final end-of-period batch into continuous streaming with animated fade (~100 ms per row). FT2 async is unchanged because it already streams. Default: ON; auto-fallback if UI stalls are detected on modest PCs. Disable for legacy batch behavior.")
                }
            }
        }

        // ── Watchdog ──
        Text { text: qsTr("WATCHDOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("TX Watchdog Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: txWdModeCombo
            model: [qsTr("Off"), qsTr("Time"), qsTr("Count")]
            currentIndex: bridge ? bridge.txWatchdogMode : 0
            implicitHeight: controlHeight
            Layout.fillWidth: true
            Layout.minimumWidth: numericFieldMinWidth
            Layout.preferredWidth: numericFieldMinWidth
            onActivated: {
                if (bridge && bridge.txWatchdogMode !== currentIndex) {
                    bridge.txWatchdogMode = currentIndex
                    dialog.scheduleSettingsPersist()
                }
            }
            contentItem: Text {
                text: parent.displayText
                color: textPrimary
                font.pixelSize: controlFontSize
                leftPadding: 8
                verticalAlignment: Text.AlignVCenter
            }
        }
        Text { text: qsTr("TX Watchdog Time (min):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: txWdSpin
            from: 1; to: 999; value: bridge.txWatchdogTime; editable: true
            enabled: bridge.txWatchdogMode === 1
            property bool completed: false
            function applyWatchdog() {
                if (bridge && bridge.txWatchdogMode === 1 && bridge.txWatchdogTime !== value) {
                    bridge.txWatchdogTime = value
                    dialog.scheduleSettingsPersist()
                }
            }
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: if (completed) applyWatchdog()
            Component.onCompleted: completed = true
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: txWdSpin.textFromValue(txWdSpin.value, txWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txWdSpin.editable; validator: txWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("TX Watchdog Count:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: txWdCountSpin
            from: 1; to: 50; value: bridge.txWatchdogCount; editable: true
            enabled: bridge.txWatchdogMode === 2
            property bool completed: false
            function applyWatchdog() {
                if (bridge && bridge.txWatchdogMode === 2 && bridge.txWatchdogCount !== value) {
                    bridge.txWatchdogCount = value
                    dialog.scheduleSettingsPersist()
                }
            }
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: if (completed) applyWatchdog()
            Component.onCompleted: completed = true
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: txWdCountSpin.textFromValue(txWdCountSpin.value, txWdCountSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txWdCountSpin.editable; validator: txWdCountSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        // 1.0.446 - P0-3 opt-in: logga il QSO se il watchdog scatta a scambio completato
        Text { text: qsTr("Log QSO at watchdog timeout:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            id: txWdLogOnCloseCheck
            implicitHeight: controlHeight
            checked: bridge ? bridge.txWatchdogLogOnClose : false
            onCheckedChanged: if (bridge && bridge.txWatchdogLogOnClose !== checked) bridge.setTxWatchdogLogOnClose(checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("When ON, if the TX watchdog fires while a QSO has already completed the two-way report exchange (both reports exchanged, progress >= ROGER_REPORT), the QSO is logged instead of abandoned.\n\nDefault OFF = 1.0.445 behavior (only a deferred snapshot, recovered only if the partner re-sends 73; in a manual QSO it is lost).")
        }
        Text { text: qsTr("Tune Watchdog (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth + 44; Layout.preferredWidth: numericFieldMinWidth + 44; spacing: 6
            CheckBox {
                id: tuneWdCheck
                checked: bridge.getSetting("TuneWatchdog", true)
                onCheckedChanged: bridge.setSetting("TuneWatchdog", checked)
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: ""; leftPadding: 24 }
            }
            SpinBox {
                id: tuneWdSpin
                from: 0; to: 300; value: Number(bridge.getSetting("TuneWatchdogTime", 90)); editable: true; enabled: tuneWdCheck.checked
                implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                onValueChanged: bridge.setSetting("TuneWatchdogTime", value)
                contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: tuneWdSpin.textFromValue(tuneWdSpin.value, tuneWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !tuneWdSpin.editable; validator: tuneWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            }
        }

        // ── CW ID ──
        Text { text: qsTr("CW ID"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("CW ID after 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("After73", false)
            onCheckedChanged: bridge.setSetting("After73", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("CW ID Interval (min):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: cwIdIntSpin
            from: 0; to: 999; value: Number(bridge.getSetting("IDint", 0)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("IDint", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: cwIdIntSpin.textFromValue(cwIdIntSpin.value, cwIdIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !cwIdIntSpin.editable; validator: cwIdIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        // ── Tone Spacing ──
        Text { text: qsTr("TONE SPACING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("2x Tone Spacing:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: x2ToneSpacingCheck
            checked: bridge.getSetting("x2ToneSpacing", false)
            onCheckedChanged: {
                if (checked) {
                    x4ToneSpacingCheck.checked = false
                    bridge.setSetting("x4ToneSpacing", false)
                }
                bridge.setSetting("x2ToneSpacing", checked)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("4x Tone Spacing:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: x4ToneSpacingCheck
            checked: bridge.getSetting("x4ToneSpacing", false)
            onCheckedChanged: {
                if (checked) {
                    x2ToneSpacingCheck.checked = false
                    bridge.setSetting("x2ToneSpacing", false)
                }
                bridge.setSetting("x4ToneSpacing", checked)
            }
            Component.onCompleted: {
                if (checked && x2ToneSpacingCheck.checked) {
                    x2ToneSpacingCheck.checked = false
                    bridge.setSetting("x2ToneSpacing", false)
                }
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Alt F1-F6 Bind:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("AlternateBindings", false)
            onCheckedChanged: bridge.setSetting("AlternateBindings", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
    }
}
