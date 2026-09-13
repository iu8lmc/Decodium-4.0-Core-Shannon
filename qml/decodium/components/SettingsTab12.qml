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
        id: uiButtonsGrid
        width: Math.max(0, parent.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        columns: 2; columnSpacing: 28; rowSpacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.rightMargin: dialog.scrollRightMargin
        anchors.topMargin: dialog.scrollTopMargin

        readonly property var toolbarButtons: [
            { label: qsTr("Monitor (MON / STOP)"),  key: "uiBtnMonitorVisible" },
            { label: qsTr("Setup (⚙)"),         key: "uiBtnSetupVisible" },
            { label: "REC",                          key: "uiBtnRecVisible" },
            { label: "WAV",                          key: "uiBtnWavVisible" },
            { label: "Log",                          key: "uiBtnLogVisible" },
            { label: "Macro",                        key: "uiBtnMacroVisible" },
            { label: "Astro",                        key: "uiBtnAstroVisible" },
            { label: qsTr("Layout (window reset)"),  key: "uiBtnFooterResetVisible" },
            { label: qsTr("History (decode history)"), key: "uiBtnFooterHistoryVisible" },
            { label: "CAT",                          key: "uiBtnCatVisible" },
            { label: qsTr("DX-Pedition (workspace)"), key: "uiBtnDxPedVisible" },
            { label: qsTr("Async FT2 (A)"),          key: "uiAsyncIconVisible" },
            { label: "PSK Reporter",                 key: "uiPskReporterToolbarVisible" },
            { label: qsTr("DX Cluster (toolbar)"),   key: "uiDxClusterToolbarVisible" },
            { label: qsTr("World Clock"),            key: "uiWorldClockVisible" }
        ]

        Text {
            text: qsTr("Show or hide UI buttons as you prefer. Changes are immediate and saved automatically.")
            color: textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap
            Layout.columnSpan: 2; Layout.fillWidth: true; Layout.bottomMargin: 4
        }

        // ── Toolbar in alto ──
        Text { text: qsTr("TOP TOOLBAR"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 2; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Repeater {
            model: uiButtonsGrid.toolbarButtons
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: modelData.label; color: textPrimary; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                CheckBox {
                    checked: dialog.boolSetting(modelData.key, true)
                    onToggled: dialog.setBoolSettingIfChanged(modelData.key, checked, true)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
            }
        }

        // ── Ordine pulsanti toolbar (drag&drop) ──
        Text { text: qsTr("TOOLBAR BUTTON ORDER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 14 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 2; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text {
            text: qsTr("Drag the top toolbar buttons (long-press) to reorder them. Use the button below to restore the default order.")
            color: textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap
            Layout.columnSpan: 2; Layout.fillWidth: true; Layout.topMargin: 2
        }

        Rectangle {
            Layout.columnSpan: 2
            Layout.topMargin: 4
            implicitWidth: resetOrderLabel.implicitWidth + 28
            implicitHeight: 30
            radius: 4
            color: resetOrderMA.containsMouse ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25) : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.12)
            border.color: primaryBlue
            border.width: 1

            Text {
                id: resetOrderLabel
                anchors.centerIn: parent
                text: qsTr("Restore default button order")
                color: textPrimary
                font.pixelSize: 12
            }

            MouseArea {
                id: resetOrderMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                // Reset the toolbar only.  The clock position is
                // independent and must not be cleared as a side effect.
                onClicked: {
                    if (bridge)
                        bridge.setSetting("uiToolbarOrder", "")
                }
            }
        }

        // ── Ordine pulsanti TX panel (drag&drop) ──
        Rectangle {
            Layout.columnSpan: 2
            Layout.topMargin: 6
            implicitWidth: resetTxOrderLabel.implicitWidth + 28
            implicitHeight: 30
            radius: 4
            color: resetTxOrderMA.containsMouse ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25) : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.12)
            border.color: primaryBlue
            border.width: 1

            Text {
                id: resetTxOrderLabel
                anchors.centerIn: parent
                text: qsTr("Restore default TX panel order")
                color: textPrimary
                font.pixelSize: 12
            }

            MouseArea {
                id: resetTxOrderMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                // Reset via il canale setting canonico: TxPanel.onSettingValueChanged
                // ricostruisce uiTxPanelOrder dal default quando il valore è vuoto.
                onClicked: if (bridge) bridge.setSetting("uiTxPanelOrder", "")
            }
        }

        Item { Layout.fillWidth: true; Layout.columnSpan: 2; Layout.fillHeight: true }
    }
}
