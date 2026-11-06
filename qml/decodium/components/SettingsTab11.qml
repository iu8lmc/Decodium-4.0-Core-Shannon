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

        // ── Blacklist ──
        Text { text: qsTr("BLACKLIST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("Blacklisted", false)
            onToggled: dialog.setBoolSettingIfChanged("Blacklisted", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // Blacklist 1-12 (2 per row)
        Text { text: qsTr("Blacklist 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist1", text.toUpperCase()) }
        Text { text: qsTr("Blacklist 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist2", text.toUpperCase()) }

        Text { text: qsTr("Blacklist 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist3", text.toUpperCase()) }
        Text { text: qsTr("Blacklist 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist4", text.toUpperCase()) }

        Text { text: qsTr("Blacklist 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist5", text.toUpperCase()) }
        Text { text: qsTr("Blacklist 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist6", text.toUpperCase()) }

        Text { text: qsTr("Blacklist 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist7", text.toUpperCase()) }
        Text { text: qsTr("Blacklist 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist8", text.toUpperCase()) }

        Text { text: qsTr("Blacklist 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist9", text.toUpperCase()) }
        Text { text: qsTr("Blacklist 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist10", text.toUpperCase()) }

        Text { text: qsTr("Blacklist 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist11", text.toUpperCase()) }
        Text { text: qsTr("Blacklist 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Blacklist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Blacklist12", text.toUpperCase()) }

        // ── Whitelist ──
        Text { text: qsTr("WHITELIST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("Whitelisted", false)
            onToggled: dialog.setBoolSettingIfChanged("Whitelisted", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { text: qsTr("Whitelist 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist1", text.toUpperCase()) }
        Text { text: qsTr("Whitelist 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist2", text.toUpperCase()) }

        Text { text: qsTr("Whitelist 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist3", text.toUpperCase()) }
        Text { text: qsTr("Whitelist 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist4", text.toUpperCase()) }

        Text { text: qsTr("Whitelist 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist5", text.toUpperCase()) }
        Text { text: qsTr("Whitelist 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist6", text.toUpperCase()) }

        Text { text: qsTr("Whitelist 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist7", text.toUpperCase()) }
        Text { text: qsTr("Whitelist 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist8", text.toUpperCase()) }

        Text { text: qsTr("Whitelist 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist9", text.toUpperCase()) }
        Text { text: qsTr("Whitelist 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist10", text.toUpperCase()) }

        Text { text: qsTr("Whitelist 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist11", text.toUpperCase()) }
        Text { text: qsTr("Whitelist 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Whitelist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Whitelist12", text.toUpperCase()) }

        // ── Always Pass ──
        Text { text: qsTr("ALWAYS PASS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("AlwaysPass", false)
            onToggled: dialog.setBoolSettingIfChanged("AlwaysPass", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { text: qsTr("Always Pass 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass1", text.toUpperCase()) }
        Text { text: qsTr("Always Pass 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass2", text.toUpperCase()) }

        Text { text: qsTr("Always Pass 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass3", text.toUpperCase()) }
        Text { text: qsTr("Always Pass 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass4", text.toUpperCase()) }

        Text { text: qsTr("Always Pass 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass5", text.toUpperCase()) }
        Text { text: qsTr("Always Pass 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass6", text.toUpperCase()) }

        Text { text: qsTr("Always Pass 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass7", text.toUpperCase()) }
        Text { text: qsTr("Always Pass 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass8", text.toUpperCase()) }

        Text { text: qsTr("Always Pass 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass9", text.toUpperCase()) }
        Text { text: qsTr("Always Pass 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass10", text.toUpperCase()) }

        Text { text: qsTr("Always Pass 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass11", text.toUpperCase()) }
        Text { text: qsTr("Always Pass 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField { text: bridge.getSetting("Pass12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Pass12", text.toUpperCase()) }

        // ── Territory ──
        Text { text: qsTr("EXCLUDE TERRITORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Europe:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
        CheckBox {
            checked: dialog.territorySettingMatches("Territory1", "EU", ["EUROPE", "EUROPA"])
            onToggled: dialog.setTerritoryExcluded("Territory1", "EU", checked)
            Component.onCompleted: dialog.normalizeTerritorySetting("Territory1", "EU", ["EUROPE", "EUROPA"])
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Africa:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
        CheckBox {
            checked: dialog.territorySettingMatches("Territory2", "AF", ["AFRICA"])
            onToggled: dialog.setTerritoryExcluded("Territory2", "AF", checked)
            Component.onCompleted: dialog.normalizeTerritorySetting("Territory2", "AF", ["AFRICA"])
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Oceania:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
        CheckBox {
            checked: dialog.territorySettingMatches("Territory3", "OC", ["OCEANIA"])
            onToggled: dialog.setTerritoryExcluded("Territory3", "OC", checked)
            Component.onCompleted: dialog.normalizeTerritorySetting("Territory3", "OC", ["OCEANIA"])
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Asia:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
        CheckBox {
            checked: dialog.territorySettingMatches("Territory4", "AS", ["ASIA"])
            onToggled: dialog.setTerritoryExcluded("Territory4", "AS", checked)
            Component.onCompleted: dialog.normalizeTerritorySetting("Territory4", "AS", ["ASIA"])
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("North America:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
        CheckBox {
            checked: dialog.territorySettingMatches("Territory5", "NA", ["NORTH AMERICA", "N. AMERICA", "N AMERICA"])
            onToggled: dialog.setTerritoryExcluded("Territory5", "NA", checked)
            Component.onCompleted: dialog.normalizeTerritorySetting("Territory5", "NA", ["NORTH AMERICA", "N. AMERICA", "N AMERICA"])
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("South America:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
        CheckBox {
            checked: dialog.territorySettingMatches("Territory6", "SA", ["SOUTH AMERICA", "S. AMERICA", "S AMERICA"])
            onToggled: dialog.setTerritoryExcluded("Territory6", "SA", checked)
            Component.onCompleted: dialog.normalizeTerritorySetting("Territory6", "SA", ["SOUTH AMERICA", "S. AMERICA", "S AMERICA"])
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        // ── Opzioni Filtro ──
        Text { text: qsTr("FILTER OPTIONS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Hide") + " " + qsTr("Worked on Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }
        CheckBox {
            checked: dialog.boolSetting("FiltersHideWorkedBand", false)
            onToggled: dialog.setBoolSettingIfChanged("FiltersHideWorkedBand", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Hide stations already worked on the current band.")
        }

        Text { text: qsTr("Hide") + " " + qsTr("Worked Today:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }
        CheckBox {
            checked: dialog.boolSetting("FiltersHideWorkedToday", false)
            onToggled: dialog.setBoolSettingIfChanged("FiltersHideWorkedToday", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Hide stations already logged today in UTC.")
        }

        Text { text: qsTr("Hide") + " " + qsTr("Worked Yesterday Too:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }
        CheckBox {
            enabled: dialog.boolSetting("FiltersHideWorkedToday", false)
            opacity: enabled ? 1.0 : 0.45
            checked: dialog.boolSetting("FiltersWorkedTodayIncludesYesterday", false)
            onToggled: dialog.setBoolSettingIfChanged("FiltersWorkedTodayIncludesYesterday", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Widen Worked Today to cover today and yesterday in UTC.")
        }

        Text { text: qsTr("Hide") + " " + qsTr("Worked Ever:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }
        CheckBox {
            checked: dialog.boolSetting("FiltersHideWorkedEver", false)
            onToggled: dialog.setBoolSettingIfChanged("FiltersHideWorkedEver", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Hide every station already present in the log, on any band and any date.")
        }

        Text { text: qsTr("Wait & Pounce:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.waitPounceActive
            onToggled: {
                bridge.waitPounceActive = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Wait & Pounce listens for filtered CQ decodes, but it only starts a reply when TX/CQ is already armed by the operator.")
        }
        Text { text: qsTr("W&P Filters Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("FiltersForWaitAndPounceOnly", false)
            onToggled: dialog.setBoolSettingIfChanged("FiltersForWaitAndPounceOnly", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Calling Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("FiltersForWord2", false)
            onToggled: dialog.setBoolSettingIfChanged("FiltersForWord2", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
    }
}
