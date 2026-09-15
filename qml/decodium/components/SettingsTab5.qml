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

        // ── Remote Web Server (PWA per iPad/mobile) ──
        Text { text: qsTr("REMOTE WEB SERVER (iPad / mobile PWA)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enable Web Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
        CheckBox {
            id: webServerToggle
            checked: bridge.webServerRunning()
            onCheckedChanged: {
                if (checked) {
                    var port = parseInt(webServerPortField.text) || 8080
                    bridge.startWebServer(port)
                    bridge.setSetting("WebServerEnabled", true)
                    bridge.setSetting("WebServerPort", port)
                } else {
                    bridge.stopWebServer()
                    bridge.setSetting("WebServerEnabled", false)
                }
                webServerUrlLabel.text = bridge.webServerUrl() || qsTr("(not active)")
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Porta TCP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
        DecoTextField {
            id: webServerPortField
            text: String(bridge.getSetting("WebServerPort", 8080))
            Layout.preferredWidth: 80
            leftPadding: 8
            rightPadding: 8
            validator: IntValidator { bottom: 1024; top: 65535 }
            color: textPrimary
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 3 }
            onEditingFinished: bridge.setSetting("WebServerPort", parseInt(text) || 8080)
        }

        Text { text: qsTr("URL accesso:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
        Text {
            id: webServerUrlLabel
            text: bridge.webServerUrl() || qsTr("(not active)")
            color: bridge.webServerRunning() ? accentGreen : textSecondary
            font.pixelSize: 12
            font.family: decodiumMonoFontFamily
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.fillWidth: true
        }

        Text { text: qsTr(""); Layout.preferredWidth: 160 }
        Button {
            // PWA remote: apre l'URL locale autenticato in browser.
            text: qsTr("📱 Open Remote for iPad")
            enabled: bridge.webServerRunning()
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            onClicked: {
                var url = bridge.webServerQrUrl()
                if (url) Qt.openUrlExternally(url)
            }
            background: Rectangle {
                color: parent.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                                     : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15)
                border.color: primaryBlue
                border.width: 1
                radius: 4
            }
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? textPrimary : textSecondary
                font.pixelSize: 12; font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // ── Decode list display (Decodium 3-style) ──
        Text { text: qsTr("DECODE LIST DISPLAY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 12 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Colored period separator:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
        CheckBox {
            // 1.0.149: bind diretto al Q_INVOKABLE C++ invece che
            // alla QSettings raw — cosi' il toggle aggiorna anche
            // m_decodeShowPeriodSeparator a runtime (era solo
            // letto al boot via loadSettings).
            checked: bridge.decodeShowPeriodSeparator()
            onCheckedChanged: {
                bridge.setDecodeShowPeriodSeparator(checked)
                bridge.setSetting("decodeShowPeriodSeparator", checked)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Newest first:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
        CheckBox {
            checked: bridge.getSetting("decodeNewestFirst", false)
            onCheckedChanged: bridge.setSetting("decodeNewestFirst", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        // ── Parametri Decodifica ──
        Text { text: qsTr("DECODE PARAMETERS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Decode Depth:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: decodeDepthCombo
            model: [qsTr("Fast"),qsTr("Normal"),qsTr("Deep")]; Layout.fillWidth: true; Layout.minimumWidth: comboFieldMinWidth; Layout.preferredWidth: comboFieldMinWidth; implicitHeight: controlHeight
            currentIndex: Math.max(0, Math.min(count - 1, bridge.ndepth - 1))
            onActivated: {
                bridge.ndepth = currentIndex + 1
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: decodeDepthCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; rightPadding: 42; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { text: qsTr("Low Freq (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: nfaSpin
            from: 0; to: 5000; value: bridge.nfa; editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: {
                bridge.nfa = value
                dialog.scheduleSettingsPersist()
            }
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: nfaSpin.textFromValue(nfaSpin.value, nfaSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !nfaSpin.editable; validator: nfaSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("High Freq (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: nfbSpin
            from: 0; to: 5000; value: bridge.nfb; editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: {
                bridge.nfb = value
                dialog.scheduleSettingsPersist()
            }
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: nfbSpin.textFromValue(nfbSpin.value, nfbSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !nfbSpin.editable; validator: nfbSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("RX Bandwidth:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: rxBwSpin
            from: 100; to: 5000; value: Number(bridge.getSetting("RXBandwidth", 2500)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
            onValueChanged: bridge.setSetting("RXBandwidth", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: rxBwSpin.textFromValue(rxBwSpin.value, rxBwSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !rxBwSpin.editable; validator: rxBwSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Decode at 52s:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            checked: bridge.getSetting("DecodeAt52s", false)
            onCheckedChanged: bridge.setSetting("DecodeAt52s", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Single Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            checked: bridge.singleDecode
            onToggled: {
                bridge.singleDecode = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── JT65 VHF/UHF ──
        Text { text: qsTr("JT65 VHF/UHF"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Erasure Patterns:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: erasurePatSpin
            from: 0; to: 99999; value: Number(bridge.getSetting("RandomErasurePatterns", 7)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("RandomErasurePatterns", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: erasurePatSpin.textFromValue(erasurePatSpin.value, erasurePatSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !erasurePatSpin.editable; validator: erasurePatSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Aggressive:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: aggressiveSpin
            from: 0; to: 10; value: Number(bridge.getSetting("AggressiveLevel", 0)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("AggressiveLevel", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: aggressiveSpin.textFromValue(aggressiveSpin.value, aggressiveSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !aggressiveSpin.editable; validator: aggressiveSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Two-Pass:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("TwoPassDecoding", false)
            onCheckedChanged: bridge.setSetting("TwoPassDecoding", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── Sidelobe Control ──
        Text { text: qsTr("SIDELOBE CONTROL"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Sidelobe Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: sidelobeCombo
            model: [qsTr("Low Sidelobes"),qsTr("Max Sensitivity")]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: Number(bridge.getSetting("SidelobeMode", 0))
            onActivated: bridge.setSetting("SidelobeMode", currentIndex)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: sidelobeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Degrade S/N:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: degradeSnSpin
            from: 0; to: 100; value: Number(bridge.getSetting("DegradeSN", 0)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("DegradeSN", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: degradeSnSpin.textFromValue(degradeSnSpin.value, degradeSnSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !degradeSnSpin.editable; validator: degradeSnSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        // ── Filtri Decodifica ──
        Text { text: qsTr("DECODE FILTERS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("CQ Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.filterCqOnly
            onCheckedChanged: {
                bridge.filterCqOnly = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        // 1.0.383 — livello di inclusione del filtro CQ (attivo solo con "CQ Only" ON).
        Text { text: qsTr("CQ filter:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: cqFilterLevelCombo
            enabled: bridge.filterCqOnly
            opacity: enabled ? 1.0 : 0.5
            model: ["CQ", "CQ/73", "CQ/73/RR73", "CQ/73/RR73/RRR"]
            Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: bridge ? bridge.cqFilterLevel : 0
            onActivated: {
                if (bridge && bridge.cqFilterLevel !== currentIndex) {
                    bridge.cqFilterLevel = currentIndex
                    dialog.scheduleSettingsPersist()
                }
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: cqFilterLevelCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: cqFilterLevelCombo }
        }
        Text { text: qsTr("My Call Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.filterMyCallOnly
            onCheckedChanged: {
                bridge.filterMyCallOnly = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Zap:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.zapEnabled
            onCheckedChanged: {
                bridge.zapEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Deep Search:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.deepSearchEnabled
            onCheckedChanged: {
                bridge.deepSearchEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("AP Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.ft8ApEnabled
            onCheckedChanged: {
                bridge.ft8ApEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Avg Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.avgDecodeEnabled
            onCheckedChanged: {
                bridge.avgDecodeEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        // 1.0.299 — deep decode-list-only durante TX (recupera stazioni terze in QSO)
        Text {
            text: qsTr("Deep decode of last RX slot during TX (list only):")
            color: textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.fillWidth: true
            Layout.preferredWidth: 0
        }
        CheckBox {
            checked: bridge.ft8DeepDecodeInTx
            onCheckedChanged: {
                bridge.ft8DeepDecodeInTx = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("While operating/in QSO in FT8, ALSO launch the deep decode depth-4 (decode-list only) in addition to the fast depth-2 that drives TX.\n\nRecovers third-party stations that the fast pass would miss during operation, WITHOUT touching timing or QSO closure (it's pure decode-list, not auto-seq).\n\nCosts extra CPU during QSOs. Default: OFF.")
        }
    }
}
