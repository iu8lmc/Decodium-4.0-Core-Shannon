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

        // ── Dettagli Stazione ──
        Text { text: qsTr("STATION DETAILS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text {
            text: qsTr("My Call:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            HoverHandler { id: myCallInfoHover }
            ToolTip.visible: myCallInfoHover.hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("The active callsign transmitted by Decodium and written as STATION_CALLSIGN in ADIF.")
        }
        DecoTextField {
            text: bridge.callsign; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.callsign = text
                dialog.scheduleSettingsPersist()
            }
        }
        Text { text: qsTr("My Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            text: bridge.grid; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.grid = text
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("Auto Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        CheckBox {
            checked: bridge.getSetting("AutoGrid", false)
            onCheckedChanged: bridge.setSetting("AutoGrid", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("IARU Region:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            model: ["1","2","3"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: Number(bridge.getSetting("Region", 0))
            onActivated: bridge.setSetting("Region", currentIndex)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
        }

        Text { text: qsTr("Type 2 Msg Gen:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            model: [qsTr("Full"),qsTr("Type 1 prefix"),qsTr("Type 2 prefix")]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: Number(bridge.getSetting("Type2MsgGen", 0))
            onActivated: bridge.setSetting("Type2MsgGen", currentIndex)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
        }
        Text {
            text: qsTr("Op Call:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            HoverHandler { id: opCallInfoHover }
            ToolTip.visible: opCallInfoHover.hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Optional legacy operator-callsign setting. It never replaces My Call for transmission or STATION_CALLSIGN in ADIF.")
        }
        DecoTextField {
            text: bridge.getSetting("OpCall", ""); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            placeholderText: qsTr("Optional operator callsign")
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("OpCall", text)
        }

        // ── Info Stazione ──
        Text { text: qsTr("STATION INFO"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text {
            text: qsTr("Station label:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            HoverHandler { id: stationLabelInfoHover }
            ToolTip.visible: stationLabelInfoHover.hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Optional descriptive label for this station or operating setup. It is not a callsign or a radio model.")
        }
        DecoTextField {
            text: bridge.stationName; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            placeholderText: qsTr("Optional - e.g. Home station")
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.stationName = text
                dialog.scheduleSettingsPersist()
            }
        }
        Text { text: qsTr("QTH:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            text: bridge.stationQth; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.stationQth = text
                dialog.scheduleSettingsPersist()
            }
        }

        Text {
            text: qsTr("Rig / radio:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            HoverHandler { id: rigInfoHover }
            ToolTip.visible: rigInfoHover.hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Optional transceiver or radio description, for example Icom IC-7100. If left empty, Decodium uses the CAT rig name when available for PSK Reporter metadata.")
        }
        DecoTextField {
            text: bridge.stationRigInfo; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            placeholderText: qsTr("Optional - e.g. Icom IC-7100")
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.stationRigInfo = text
                dialog.scheduleSettingsPersist()
            }
        }
        Text { text: qsTr("Antenna:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoTextField {
            text: bridge.stationAntenna; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.stationAntenna = text
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("Power (W):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        SpinBox {
            id: stPowerSpin
            from: 0; to: 9999; value: bridge.stationPowerWatts; editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth
            onValueChanged: {
                bridge.stationPowerWatts = value
                dialog.scheduleSettingsPersist()
            }
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: stPowerSpin.textFromValue(stPowerSpin.value, stPowerSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !stPowerSpin.editable; validator: stPowerSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        // ── Telemetria stazione+meteo (FT8/FT4 tipo 0.5, opt-in) ──
        Text { text: qsTr("STATION + WEATHER TELEMETRY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }
        Text {
            text: qsTr("Sends one extra FT8/FT4 message right after a QSO is logged, with your radio, antenna, power, locator and current weather packed into a compact code (no free text, no callsign field in this message type). Off by default.")
            color: textSecondary; font.pixelSize: 11; wrapMode: Text.WordWrap
            Layout.columnSpan: pageColumns; Layout.fillWidth: true; Layout.bottomMargin: 4
        }

        Text { text: qsTr("Radio model:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            model: bridge.stationRadioModelNames(); Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: Number(bridge.getSetting("RadioModelId", 0))
            onActivated: bridge.setSetting("RadioModelId", currentIndex)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
        }
        Text { text: qsTr("Antenna type:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            model: bridge.stationAntennaTypeNames(); Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: Number(bridge.getSetting("AntennaTypeId", 0))
            onActivated: bridge.setSetting("AntennaTypeId", currentIndex)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
        }

        Text { text: qsTr("Auto weather:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        RowLayout {
            spacing: 10
            CheckBox {
                id: weatherApiCheck
                checked: bridge.getSetting("WeatherApiEnable", false)
                onCheckedChanged: { bridge.setSetting("WeatherApiEnable", checked); if (checked) bridge.fetchWeatherForGrid() }
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: ""; leftPadding: 24 }
            }
            Text {
                property var preview: bridge.currentWeatherPreview()
                text: weatherApiCheck.checked
                    ? (preview.available
                        ? qsTr("Now: %1°C, wind %2 km/h %3, %4").arg(preview.tempC).arg(preview.windKmh).arg(preview.windDirLabel).arg(preview.sky)
                        : qsTr("Fetching…"))
                    : qsTr("Off — temperature/wind sent as “unknown”")
                color: textSecondary; font.pixelSize: 11
                Timer { interval: 15000; running: weatherApiCheck.checked; repeat: true; onTriggered: parent.preview = bridge.currentWeatherPreview() }
            }
        }

        Text { text: qsTr("After each QSO:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        CheckBox {
            checked: bridge.getSetting("SendStationTelemetry", false)
            onCheckedChanged: bridge.setSetting("SendStationTelemetry", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: qsTr("Send station + weather info"); leftPadding: 24; color: textSecondary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; height: 18 }
        }
        Text { text: qsTr("On receive:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        CheckBox {
            checked: bridge.getSetting("ShowTelemetryPopup", false)
            onCheckedChanged: bridge.setSetting("ShowTelemetryPopup", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: qsTr("Show popup with correspondent's info"); leftPadding: 24; color: textSecondary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; height: 18 }
        }
    }
}
