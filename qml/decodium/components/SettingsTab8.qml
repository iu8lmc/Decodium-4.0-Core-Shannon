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
    id: colorsSettingsScroll
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    GridLayout {
        id: colorsSettingsGrid
        width: Math.max(0, parent.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        columns: pageColumns; columnSpacing: 10; rowSpacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.rightMargin: dialog.scrollRightMargin
        anchors.topMargin: dialog.scrollTopMargin

        // ── Colori Decodifica ──
        Text { text: qsTr("DECODE COLORS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Repeater {
            model: dialog.decodeColorModel
            delegate: RowLayout {
                id: decodeColorRow
                Layout.columnSpan: pageColumns
                Layout.fillWidth: true
                spacing: 10
                property string targetProp: modelData.prop
                property string defaultColor: modelData.defaultColor
                property string currentColor: bridge[targetProp] || defaultColor
                property bool colorEnabled: bridge.decodeColorEnabled(targetProp)
                property bool boldEnabled: bridge.decodeColorBold(targetProp)
                property string shownColor: colorEnabled ? currentColor : bridge.decodeColorFallback
                // 1.0.416 — sfondo riga per categoria (opt-in, default OFF)
                property bool bgEnabled: bridge.decodeColorBgEnabled(targetProp)
                property string bgColor: {
                    var v = bridge.decodeColorBgValue(targetProp)
                    return (v && v.length > 0) ? v : "#202830"
                }

                Text {
                    text: modelData.label + ":"
                    color: textSecondary
                    font.pixelSize: 12
                    Layout.preferredWidth: 210
                    elide: Text.ElideRight
                }

                CheckBox {
                    id: decodeColorEnabledCheck
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: controlHeight
                    checked: decodeColorRow.colorEnabled
                    onClicked: {
                        decodeColorRow.colorEnabled = checked
                        bridge.setDecodeColorEnabled(decodeColorRow.targetProp, checked)
                        decodeColorInput.text = decodeColorRow.shownColor
                    }
                    indicator: Rectangle {
                        width: 18
                        height: 18
                        radius: 3
                        color: parent.checked ? primaryBlue : bgMedium
                        border.color: glassBorder
                        y: parent.height / 2 - height / 2
                    }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Use this specific decode color. When OFF, this category uses the shared default color.")
                }

                Rectangle {
                    width: 60
                    height: 24
                    radius: 4
                    color: dialog.validHexColor(decodeColorRow.shownColor) ? decodeColorRow.shownColor : bridge.decodeColorFallback
                    opacity: decodeColorRow.colorEnabled ? 1.0 : 0.55
                    border.color: glassBorder
                    MouseArea {
                        anchors.fill: parent
                        enabled: decodeColorRow.colorEnabled
                        cursorShape: Qt.PointingHandCursor
                        onClicked: decodeColorPresetPop.open()
                    }
                    Popup {
                        id: decodeColorPresetPop
                        width: 232
                        height: 88
                        background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                        Flow {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4
                            Repeater {
                                model: dialog.presetColors.concat([decodeColorRow.defaultColor])
                                delegate: Rectangle {
                                    width: 20
                                    height: 20
                                    radius: 3
                                    color: modelData
                                    border.color: glassBorder
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            dialog.setDecodeHighlightColor(decodeColorRow.targetProp, modelData)
                                            decodeColorInput.text = dialog.normalizedHexColor(modelData)
                                            decodeColorPresetPop.close()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                DecoTextField {
                    id: decodeColorInput
                    text: decodeColorRow.shownColor
                    enabled: decodeColorRow.colorEnabled
                    opacity: enabled ? 1.0 : 0.55
                    selectByMouse: true
                    leftPadding: 8
                    rightPadding: 8
                    implicitHeight: controlHeight
                    Layout.preferredWidth: 110
                    color: dialog.validHexColor(text) ? textPrimary : "#ff5555"
                    font.pixelSize: controlFontSize
                    onActiveFocusChanged: {
                        if (!activeFocus)
                            text = decodeColorRow.shownColor
                    }
                    onAccepted: {
                        if (dialog.setDecodeHighlightColor(decodeColorRow.targetProp, text))
                            text = dialog.normalizedHexColor(text)
                    }
                    background: Rectangle {
                        color: bgMedium
                        border.color: decodeColorInput.activeFocus ? secondaryCyan : glassBorder
                        radius: 4
                    }
                }

                Button {
                    text: qsTr("Reset")
                    Layout.preferredWidth: 72
                    implicitHeight: controlHeight
                    enabled: decodeColorRow.colorEnabled
                    opacity: enabled ? 1.0 : 0.55
                    onClicked: {
                        dialog.setDecodeHighlightColor(decodeColorRow.targetProp, decodeColorRow.defaultColor)
                        decodeColorInput.text = decodeColorRow.defaultColor
                    }
                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.24) : bgMedium
                        border.color: glassBorder
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textPrimary
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text { text: qsTr("Bold") + ":"; color: textSecondary; font.pixelSize: 11; Layout.leftMargin: 6 }
                CheckBox {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: controlHeight
                    checked: decodeColorRow.boldEnabled
                    enabled: decodeColorRow.colorEnabled
                    opacity: enabled ? 1.0 : 0.55
                    onClicked: {
                        decodeColorRow.boldEnabled = checked
                        bridge.setDecodeColorBold(decodeColorRow.targetProp, checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height / 2 - height / 2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                // ── 1.0.416: colore di SFONDO riga (per categoria) ──
                Text { text: qsTr("BG:"); color: textSecondary; font.pixelSize: 11; Layout.leftMargin: 8 }
                CheckBox {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: controlHeight
                    checked: decodeColorRow.bgEnabled
                    onClicked: {
                        decodeColorRow.bgEnabled = checked
                        bridge.setDecodeColorBgEnabled(decodeColorRow.targetProp, checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height / 2 - height / 2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Colors the row BACKGROUND (in addition to the text) for this category. OFF = no custom background.")
                }
                Rectangle {
                    width: 60
                    height: 24
                    radius: 4
                    color: dialog.validHexColor(decodeColorRow.bgColor) ? decodeColorRow.bgColor : "#202830"
                    opacity: decodeColorRow.bgEnabled ? 1.0 : 0.55
                    border.color: glassBorder
                    MouseArea {
                        anchors.fill: parent
                        enabled: decodeColorRow.bgEnabled
                        cursorShape: Qt.PointingHandCursor
                        onClicked: decodeBgPresetPop.open()
                    }
                    Popup {
                        id: decodeBgPresetPop
                        width: 232
                        height: 88
                        background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                        Flow {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4
                            Repeater {
                                model: dialog.presetColors
                                delegate: Rectangle {
                                    width: 20
                                    height: 20
                                    radius: 3
                                    color: modelData
                                    border.color: glassBorder
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var nz = dialog.normalizedHexColor(modelData)
                                            bridge.setDecodeColorBg(decodeColorRow.targetProp, nz)
                                            decodeColorRow.bgColor = nz
                                            decodeBgInput.text = nz
                                            decodeBgPresetPop.close()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                DecoTextField {
                    id: decodeBgInput
                    text: decodeColorRow.bgColor
                    enabled: decodeColorRow.bgEnabled
                    opacity: enabled ? 1.0 : 0.55
                    selectByMouse: true
                    leftPadding: 8
                    rightPadding: 8
                    implicitHeight: controlHeight
                    Layout.preferredWidth: 90
                    color: dialog.validHexColor(text) ? textPrimary : "#ff5555"
                    font.pixelSize: controlFontSize
                    onActiveFocusChanged: { if (!activeFocus) text = decodeColorRow.bgColor }
                    onAccepted: {
                        var nz = dialog.normalizedHexColor(text)
                        if (dialog.validHexColor(nz)) {
                            bridge.setDecodeColorBg(decodeColorRow.targetProp, nz)
                            decodeColorRow.bgColor = nz
                            text = nz
                        }
                    }
                    background: Rectangle { color: bgMedium; border.color: decodeBgInput.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                }

                Item { Layout.fillWidth: true }
            }
        }
        Text { text: qsTr("B4 Strikethrough:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.b4Strikethrough
            // Persist only a real user action. onCheckedChanged also fires
            // while the binding is initialised and used to perform a second,
            // potentially stale write through the generic settings path.
            onToggled: bridge.b4Strikethrough = checked
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text {
            text: qsTr("Decode Boost:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 10
            Slider {
                id: decodeColorBoostSlider
                from: 0
                to: 100
                stepSize: 1
                value: Math.max(0, Math.min(100, Number(bridge.getSetting("uiDecodeColorBoost", 35))))
                Layout.fillWidth: true
                onValueChanged: bridge.setSetting("uiDecodeColorBoost", Math.round(value))
            }
            Text {
                text: Math.round(decodeColorBoostSlider.value) + "%"
                color: textPrimary
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 44
            }
        }
        Text {
            text: qsTr("Visual contrast only; it does not change decoder sensitivity.")
            color: textDim
            font.pixelSize: 10
            Layout.columnSpan: pageColumns
            Layout.leftMargin: 110
        }

	                        // ── Highlighting ──
	                        Text { text: qsTr("HIGHLIGHTING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Highlight 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("Highlight73", true)
            onCheckedChanged: bridge.setSetting("Highlight73", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { text: qsTr("HL Orange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("HighlightOrange", false)
            onCheckedChanged: bridge.setSetting("HighlightOrange", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Orange Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("HighlightOrangeCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("HighlightOrangeCallsigns", text.toUpperCase())
        }

        Text { text: qsTr("HL Blue:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("HighlightBlue", false)
            onCheckedChanged: bridge.setSetting("HighlightBlue", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Blue Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("HighlightBlueCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("HighlightBlueCallsigns", text.toUpperCase())
        }

        // ── Colori Interfaccia (sfondo + testo) — #6, stile v3 ──
        Text { text: qsTr("COLORI INTERFACCIA (sfondo + testo)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        RowLayout {
            Layout.columnSpan: pageColumns; Layout.fillWidth: true; spacing: 10
            Text { text: qsTr("Usa colori personalizzati:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 210; elide: Text.ElideRight }
            CheckBox {
                checked: bridge.themeManager.customColorsEnabled
                onToggled: bridge.themeManager.customColorsEnabled = checked
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: ""; leftPadding: 24 }
            }
            Text { text: qsTr("(overrides theme background and text)"); color: textSecondary; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
        }

        RowLayout {
            Layout.columnSpan: pageColumns; Layout.fillWidth: true; spacing: 10
            enabled: bridge.themeManager.customColorsEnabled
            opacity: enabled ? 1.0 : 0.4
            Text { text: qsTr("Background:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 210; elide: Text.ElideRight }
            Rectangle { width: 60; height: 24; radius: 4; border.color: glassBorder
                color: dialog.validHexColor(bridge.themeManager.customBgColor) ? bridge.themeManager.customBgColor : bgDeep }
            DecoTextField {
                id: customBgField
                Layout.preferredWidth: 120; implicitHeight: 28; leftPadding: 8
                text: bridge.themeManager.customBgColor
                placeholderText: "#0A0F1A"
                color: dialog.validHexColor(text) ? textPrimary : "#ff5555"
                font.pixelSize: 12; selectByMouse: true
                onEditingFinished: if (dialog.validHexColor(text)) bridge.themeManager.customBgColor = text
                background: Rectangle { color: bgMedium; border.color: customBgField.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.columnSpan: pageColumns; Layout.fillWidth: true; spacing: 10
            enabled: bridge.themeManager.customColorsEnabled
            opacity: enabled ? 1.0 : 0.4
            Text { text: qsTr("Text:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 210; elide: Text.ElideRight }
            Rectangle { width: 60; height: 24; radius: 4; border.color: glassBorder
                color: dialog.validHexColor(bridge.themeManager.customTextColor) ? bridge.themeManager.customTextColor : textPrimary }
            DecoTextField {
                id: customTextField
                Layout.preferredWidth: 120; implicitHeight: 28; leftPadding: 8
                text: bridge.themeManager.customTextColor
                placeholderText: "#E8F4FD"
                color: dialog.validHexColor(text) ? textPrimary : "#ff5555"
                font.pixelSize: 12; selectByMouse: true
                onEditingFinished: if (dialog.validHexColor(text)) bridge.themeManager.customTextColor = text
                background: Rectangle { color: bgMedium; border.color: customTextField.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            }
            Item { Layout.fillWidth: true }
        }

        // ── Spettro ──
        Text { text: qsTr("SPECTRUM"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Palette:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: paletteCombo
            model: ["SDR Classic","Raptor Green","Grayscale","SmartSDR","Hot (SDR#)","deskHPSDR","Aether Default","Aether BlueGreen","Aether Fire","Aether Plasma","FlexRadio"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: Math.max(1, pageColumns - 1)
            currentIndex: Math.max(0, bridge.uiPaletteIndex)
            onActivated: {
                bridge.uiPaletteIndex = currentIndex
                bridge.setSetting("uiPaletteIndex", currentIndex)
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: paletteCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Black Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        Slider {
            from: 0; to: 100; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallBlackLevel", 15)); Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            onValueChanged: bridge.setSetting("uiWaterfallBlackLevel", value)
        }

        Text { text: qsTr("Color Gain:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        Slider {
            from: 0; to: 100; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallColorGain", 50)); Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            onValueChanged: bridge.setSetting("uiWaterfallColorGain", value)
        }

        Text { text: qsTr("Contrast:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        Slider {
            from: 10; to: 150; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallContrast", 80)); Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            onValueChanged: bridge.setSetting("uiWaterfallContrast", value)
        }

    }
}
