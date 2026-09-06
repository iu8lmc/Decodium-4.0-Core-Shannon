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

        // ── Avvio ──
        Text { text: qsTr("STARTUP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            implicitHeight: advancedStartupGrid.implicitHeight

            GridLayout {
                id: advancedStartupGrid
                width: parent.width
                columns: pageColumns
                columnSpacing: 14
                rowSpacing: 10
                property int checkWidth: 34
                property real labelWidth: Math.max(190, (width - (checkWidth * 2) - (columnSpacing * 3)) / 2)

                Text { text: qsTr("Monitor OFF:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    enabled: false
                    checked: false
                    Component.onCompleted: dialog.setBoolSettingIfChanged("MonitorOFF", false, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; opacity: parent.enabled ? 1.0 : 0.55; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Monitor Last:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("MonitorLastUsed", false)
                    onToggled: dialog.setBoolSettingIfChanged("MonitorLastUsed", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Auto Astro:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.getSetting("AutoAstroWindow", false)
                    onCheckedChanged: bridge.setSetting("AutoAstroWindow", checked)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("kHz no k:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("kHzWithoutK", false)
                    onToggled: dialog.setBoolSettingIfChanged("kHzWithoutK", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Progress Red:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("ProgressBarRed", true)
                    onToggled: dialog.setBoolSettingIfChanged("ProgressBarRed", checked, true)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("High DPI:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("HighDPI", true)
                    onToggled: dialog.setBoolSettingIfChanged("HighDPI", checked, true)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Larger Tab:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.getSetting("LargerTabWidget", false)
                    onCheckedChanged: bridge.setSetting("LargerTabWidget", checked)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Direct Visual:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("DirectVisualAudioCaptureUnsafe", false)
                    onToggled: dialog.setBoolSettingIfChanged("DirectVisualAudioCaptureUnsafe", checked, false)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Fast visual panadapter. In legacy mode it may open a second audio capture; in normal mode it only increases the visual refresh rate. Default: OFF.")
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text {
                    visible: Qt.platform.os === "linux"
                    text: qsTr("OpenGL GPU FFT:")
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: advancedStartupGrid.labelWidth
                    Layout.preferredHeight: visible ? controlHeight : 0
                }
                CheckBox {
                    visible: Qt.platform.os === "linux"
                    Layout.preferredWidth: advancedStartupGrid.checkWidth
                    Layout.preferredHeight: visible ? controlHeight : 0
                    checked: bridge.getSetting("OpenGlGpuPanadapterFft", false)
                    onToggled: bridge.setSetting("OpenGlGpuPanadapterFft", checked)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("After restart, offloads the visual panadapter FFT to OpenGL compute on supported Linux drivers. It does not move FT decoding to the GPU. A failed or stalled GPU path falls back automatically to asynchronous CPU FFT. Default: OFF.")
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text {
                    visible: Qt.platform.os === "linux"
                    text: qsTr("Visual FFT only; restart required; safe CPU fallback remains active.")
                    color: textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                // 1.0.497 — Modalità PC lento (master): un solo interruttore per hardware vecchio
                Text { text: qsTr("Slow-PC mode:"); color: textSecondary; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.lowEndMode
                    onToggled: {
                        bridge.lowEndMode = checked
                        dialog.scheduleSettingsPersist()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("One switch for old/slow PCs. Enables: conservative D3D11 graphics on Windows (needs restart), Low CPU mode, max 4 FT threads, normal process priority, CPU decode profile, and hides Live Map / Full Spectrum by default. Default: OFF.")
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Low CPU:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.lowCpuModeEnabled
                    onToggled: {
                        bridge.lowCpuModeEnabled = checked
                        dialog.scheduleSettingsPersist()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Profile for slow PCs: maximum 2 FT threads, slower waterfall, reduced early/deep decoding. Default: OFF.")
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text {
                    text: qsTr("Reduces FT threads, waterfall refresh, and QML rendering during monitor/TX.")
                    color: textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }
            }
        }

        // ── Aggiornamenti dati ──
        Text { text: qsTr("DATA UPDATES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("LotW Users:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: 120; Layout.preferredHeight: controlHeight }
        Text {
            text: bridge.lotwUpdating ? qsTr("Updating...")
                  : (bridge.lotwUserCount > 0 ? qsTr("%1 users").arg(bridge.lotwUserCount)
                                              : qsTr("Not loaded"))
            color: bridge.lotwUserCount > 0 ? accentGreen : textSecondary
            font.pixelSize: 11
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.preferredHeight: controlHeight
        }
        Button {
            id: forceLotwUpdateButton
            readonly property bool busy: bridge ? bridge.lotwUpdating : false
            text: busy ? qsTr("Updating...") : qsTr("Force Update")
            implicitHeight: controlHeight
            Layout.minimumWidth: 170
            Layout.preferredWidth: Math.max(170, implicitWidth + 12)
            onClicked: {
                if (!busy)
                    bridge.forceUpdateLotwUsers()
            }
            background: Rectangle {
                radius: 4
                color: forceLotwUpdateButton.busy
                       ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18)
                       : (forceLotwUpdateButton.down || forceLotwUpdateButton.hovered
                          ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.24)
                          : bgMedium)
                border.color: forceLotwUpdateButton.busy ? secondaryCyan : glassBorder
            }
            contentItem: Item {
                implicitWidth: lotwUpdateContent.implicitWidth
                implicitHeight: controlHeight

                Row {
                    id: lotwUpdateContent
                    anchors.centerIn: parent
                    spacing: 7

                    BusyIndicator {
                        visible: forceLotwUpdateButton.busy
                        running: visible
                        width: 16
                        height: 16
                        Material.accent: secondaryCyan
                    }
                    Text {
                        height: 16
                        text: forceLotwUpdateButton.text
                        color: forceLotwUpdateButton.busy ? secondaryCyan : textPrimary
                        font.pixelSize: controlFontSize
                        font.bold: forceLotwUpdateButton.busy
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Item { Layout.fillWidth: true; Layout.preferredHeight: controlHeight }

        Text { text: qsTr("US States:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: 120; Layout.preferredHeight: controlHeight }
        Text {
            text: bridge.usStateDataUpdating ? qsTr("Updating...")
                  : (bridge.usStateDataReady ? qsTr("%1 calls, %2 locators").arg(bridge.usStateGridCount).arg(bridge.usStateLocatorCount)
                                             : qsTr("Not loaded"))
            color: bridge.usStateDataReady ? accentGreen : textSecondary
            font.pixelSize: 11
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.preferredHeight: controlHeight
        }
        Button {
            id: forceUsStateUpdateButton
            readonly property bool busy: bridge ? bridge.usStateDataUpdating : false
            text: busy ? qsTr("Updating...") : qsTr("Force Update")
            implicitHeight: controlHeight
            Layout.minimumWidth: 170
            Layout.preferredWidth: Math.max(170, implicitWidth + 12)
            onClicked: {
                if (!busy)
                    bridge.updateUsStateData()
            }
            background: Rectangle {
                radius: 4
                color: forceUsStateUpdateButton.busy
                       ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18)
                       : (forceUsStateUpdateButton.down || forceUsStateUpdateButton.hovered
                          ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.24)
                          : bgMedium)
                border.color: forceUsStateUpdateButton.busy ? secondaryCyan : glassBorder
            }
            contentItem: Item {
                implicitWidth: usStateUpdateContent.implicitWidth
                implicitHeight: controlHeight

                Row {
                    id: usStateUpdateContent
                    anchors.centerIn: parent
                    spacing: 7

                    BusyIndicator {
                        visible: forceUsStateUpdateButton.busy
                        running: visible
                        width: 16
                        height: 16
                        Material.accent: secondaryCyan
                    }
                    Text {
                        height: 16
                        text: forceUsStateUpdateButton.text
                        color: forceUsStateUpdateButton.busy ? secondaryCyan : textPrimary
                        font.pixelSize: controlFontSize
                        font.bold: forceUsStateUpdateButton.busy
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Item { Layout.fillWidth: true; Layout.preferredHeight: controlHeight }

        // ── Comportamento ──
        Text { text: qsTr("BEHAVIOR"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            implicitHeight: advancedBehaviorGrid.implicitHeight

            GridLayout {
                id: advancedBehaviorGrid
                width: parent.width
                columns: pageColumns
                columnSpacing: 14
                rowSpacing: 10
                property int checkWidth: 34
                property real labelWidth: Math.max(190, (width - (checkWidth * 2) - (columnSpacing * 3)) / 2)

                Text { text: qsTr("Quick Call:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("QuickCall", true)
                    onToggled: dialog.setBoolSettingIfChanged("QuickCall", checked, true)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Force Call 1st:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("ForceCallFirst", false)
                    onToggled: dialog.setBoolSettingIfChanged("ForceCallFirst", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("VHF/UHF:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.vhfUhfFeatures
                    onToggled: {
                        bridge.vhfUhfFeatures = checked
                        dialog.scheduleSettingsPersist()
                        bridge.setSetting("VHFUHF", checked)
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Wait Features:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("WaitFeaturesEnabled", true)
                    onToggled: dialog.setBoolSettingIfChanged("WaitFeaturesEnabled", checked, true)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Erase Band Act:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("erase_BandActivity", false)
                    onToggled: dialog.setBoolSettingIfChanged("erase_BandActivity", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Clear DX Grid:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("clear_DXgrid", false)
                    onToggled: dialog.setBoolSettingIfChanged("clear_DXgrid", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Clear DX Call:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("clear_DXcall", false)
                    onToggled: dialog.setBoolSettingIfChanged("clear_DXcall", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("RX>TX after QSO:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("set_RXtoTX", false)
                    onToggled: dialog.setBoolSettingIfChanged("set_RXtoTX", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }

                Text { text: qsTr("Alt Erase Btn:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("AlternateEraseButtonBehavior", true)
                    onToggled: dialog.setBoolSettingIfChanged("AlternateEraseButtonBehavior", checked, true)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("No Btn Color:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("TxWarningDisabled", false)
                    onToggled: dialog.setBoolSettingIfChanged("TxWarningDisabled", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
            }
        }

        // ── Modo Operativo ──
        Text { text: qsTr("OPERATING MODE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            implicitHeight: advancedOperatingGrid.implicitHeight

            GridLayout {
                id: advancedOperatingGrid
                width: parent.width
                columns: pageColumns
                columnSpacing: 14
                rowSpacing: 10
                property int checkWidth: 34
                property real labelWidth: Math.max(190, (width - (checkWidth * 2) - (columnSpacing * 3)) / 2)

                Text { text: qsTr("Fox Mode:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.foxMode
                    onToggled: {
                        bridge.foxMode = checked
                        dialog.scheduleSettingsPersist()
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Hound Mode:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: bridge.houndMode
                    onToggled: {
                        bridge.houndMode = checked
                        dialog.scheduleSettingsPersist()
                    }
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("SuperFox:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("SuperFox", true)
                    onToggled: dialog.setBoolSettingIfChanged("SuperFox", checked, true)
                    enabled: !bridge.transmitting && !bridge.tuning
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("To work a SuperFox DXpedition, select FT8 and enable Hound Mode as well. This option alone does not enable SuperHound reception.")
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
                Text { text: qsTr("Show OTP:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                CheckBox {
                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                    checked: dialog.boolSetting("ShowOTP", false)
                    onToggled: dialog.setBoolSettingIfChanged("ShowOTP", checked, false)
                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                    contentItem: Text { text: ""; leftPadding: 24 }
                }
            }
        }

        // ── Contest ──
        Text { text: qsTr("CONTEST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Activity:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: contestCombo
            model: [qsTr("None"),"NA VHF","EU VHF",qsTr("Field Day"),"RTTY Roundup","WW DIGI",qsTr("Fox"),qsTr("Hound"),"ARRL Digi","Q65 Pileup"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: Math.max(1, pageColumns - 1)
            currentIndex: Math.max(0, Math.min(model.length - 1, bridge.specialOperationActivity))
            onActivated: {
                bridge.specialOperationActivity = currentIndex
                dialog.scheduleSettingsPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: contestCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("FD Exchange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("Field_Day_Exchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Field_Day_Exchange", text.toUpperCase())
        }
        Text { text: qsTr("RTTY Exchange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("RTTY_Exchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("RTTY_Exchange", text.toUpperCase())
        }

        Text { text: qsTr("Contest Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("Contest_Name", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Contest_Name", text.toUpperCase())
        }
        Text { text: qsTr("Indiv Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("Individual_Contest_Name", false)
            onToggled: dialog.setBoolSettingIfChanged("Individual_Contest_Name", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("NCCC Sprint:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("NCCC_Sprint", false)
            onToggled: dialog.setBoolSettingIfChanged("NCCC_Sprint", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── NTP Time Sync ──
        Text { text: qsTr("NTP TIME SYNC"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enable NTP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.ntpEnabled
            onClicked: bridge.setSetting("NTPEnabled", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text {
            text: bridge.ntpEnabled
                  ? (bridge.ntpSynced ? "Synced" : "Syncing / waiting reply")
                  : "Disabled"
            color: bridge.ntpEnabled ? (bridge.ntpSynced ? accentGreen : "#FF9800") : textSecondary
            font.pixelSize: 12
            Layout.columnSpan: 2
            verticalAlignment: Text.AlignVCenter
        }

        Text { text: qsTr("Custom Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: ntpServerField
            text: bridge.getSetting("NTPCustomServer", "")
            Layout.fillWidth: true
            Layout.columnSpan: 2
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            placeholderText: qsTr("Empty = automatic public servers")
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: bridge.setSetting("NTPCustomServer", text.trim())
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: controlHeight
            radius: 4
            color: ntpSyncNowMouse.containsMouse && bridge.ntpEnabled
                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25)
                   : bgMedium
            border.color: bridge.ntpEnabled ? secondaryCyan : glassBorder
            opacity: bridge.ntpEnabled ? 1.0 : 0.55
            Text {
                anchors.centerIn: parent
                text: qsTr("Sync Now")
                color: bridge.ntpEnabled ? textPrimary : textSecondary
                font.pixelSize: 12
                font.bold: bridge.ntpEnabled
            }
            MouseArea {
                id: ntpSyncNowMouse
                anchors.fill: parent
                enabled: bridge.ntpEnabled
                hoverEnabled: true
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: bridge.syncNtpNow()
            }
        }

        Text {
            text: qsTr("Leave the server empty to automatically use pool.ntp.org, Apple, Cloudflare, and Google.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.columnSpan: pageColumns
        }
        Text { text: qsTr("RF self-calibration:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("DecoSyncSelfCalEnabled", false)
            onClicked: bridge.setSetting("DecoSyncSelfCalEnabled", checked)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Use received decode DT values only as a secondary time-sync hint after NTP/HTTPS is already locked. Default: OFF.")
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text {
            text: qsTr("Secondary hint only; it cannot create the first time lock.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.columnSpan: 2
        }

        // ── ADV Decoding ──
        Text { text: qsTr("ADV DECODING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Auto Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        ColumnLayout {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 2
            Switch {
                text: qsTr("AUTO - enable the 3 technologies when needed")
                checked: bridge.advAutoModeEnabled
                onToggled: {
                    bridge.advAutoModeEnabled = checked
                    dialog.scheduleSettingsPersist()
                }
                contentItem: Text {
                    text: parent.text; color: textPrimary; font.pixelSize: 12; font.bold: true
                    leftPadding: parent.indicator.width + 8
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Text {
                text: qsTr("When ON, the 3 features below are managed automatically. Trigger: Neural+Turbo when decodes < 2/slot for 4 slots. Coherent when Q65 SNR < -22 dB.")
                color: "#888"; font.pixelSize: 10
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                leftPadding: 8
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                visible: bridge.advAutoModeEnabled
                Text { text: qsTr("Live state:"); color: "#888"; font.pixelSize: 10 }
                Rectangle { width: 8; height: 8; radius: 4; color: bridge.advNeuralSyncActive ? "#0f0" : "#444" }
                Text { text: qsTr("Neural"); color: bridge.advNeuralSyncActive ? "#0f0" : "#666"; font.pixelSize: 10 }
                Rectangle { width: 8; height: 8; radius: 4; color: bridge.advTurboFeedbackActive ? "#0f0" : "#444" }
                Text { text: qsTr("Turbo"); color: bridge.advTurboFeedbackActive ? "#0f0" : "#666"; font.pixelSize: 10 }
                Rectangle { width: 8; height: 8; radius: 4; color: bridge.advCoherentAvgActive ? "#0f0" : "#444" }
                Text { text: qsTr("Coherent"); color: bridge.advCoherentAvgActive ? "#0f0" : "#666"; font.pixelSize: 10 }
            }
        }

        Text { text: qsTr("Coherent Avg:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0 }
        ColumnLayout {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 2
            opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0
            Switch {
                text: qsTr("Coherent Average (Q65/JT65)")
                checked: bridge.coherentAvgEnabled
                onToggled: {
                    bridge.coherentAvgEnabled = checked
                    dialog.scheduleSettingsPersist()
                }
                enabled: !bridge.advAutoModeEnabled
                contentItem: Text {
                    text: parent.text; color: textPrimary; font.pixelSize: 12
                    leftPadding: parent.indicator.width + 8
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Text {
                text: qsTr("Accumulates multi-slot averaging for Q65/JT65 decodes (+1-3 dB)")
                color: "#888"; font.pixelSize: 10
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                leftPadding: 8
            }
        }

        Text { text: qsTr("Neural Sync:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0 }
        ColumnLayout {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 2
            opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0
            Switch {
                text: qsTr("Neural Sync (FT8 OSD decoder)")
                checked: bridge.neuralSyncEnabled
                onToggled: {
                    bridge.neuralSyncEnabled = checked
                    dialog.scheduleSettingsPersist()
                }
                enabled: !bridge.advAutoModeEnabled
                contentItem: Text {
                    text: parent.text; color: textPrimary; font.pixelSize: 12
                    leftPadding: parent.indicator.width + 8
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Text {
                text: qsTr("Forces OSD-aware FT8 decoding (+2-3 dB on borderline signals)")
                color: "#888"; font.pixelSize: 10
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                leftPadding: 8
            }
        }

        Text { text: qsTr("Turbo Feedback:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0 }
        ColumnLayout {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 2
            opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0
            Switch {
                text: qsTr("Turbo Feedback (extended LDPC iterations)")
                checked: bridge.turboFeedbackEnabled
                onToggled: {
                    bridge.turboFeedbackEnabled = checked
                    dialog.scheduleSettingsPersist()
                }
                enabled: !bridge.advAutoModeEnabled
                contentItem: Text {
                    text: parent.text; color: textPrimary; font.pixelSize: 12
                    leftPadding: parent.indicator.width + 8
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Text {
                text: qsTr("Extended LDPC iterations for marginal decode recovery")
                color: "#888"; font.pixelSize: 10
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                leftPadding: 8
            }
        }

        // ── OTP ──
        Text { text: qsTr("OTP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("OTP Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("OTPEnabled", false)
            onCheckedChanged: bridge.setSetting("OTPEnabled", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("OTP Seed:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("OTPSeed", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("OTPSeed", text)
        }

        Text { text: qsTr("OTP Interval:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: otpIntSpin
            from: 1; to: 3600; value: Number(bridge.getSetting("OTPinterval", 1)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("OTPinterval", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: otpIntSpin.textFromValue(otpIntSpin.value, otpIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !otpIntSpin.editable; validator: otpIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("OTP URL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("OTPUrl", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("OTPUrl", text)
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: pageColumns; Layout.preferredHeight: 80 }
    }
}
