/* Decodium 4.0 - lazy Settings tab */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SettingsPageScroll {
    id: root
    property var dialog
    readonly property var bridge: dialog ? dialog.appBridge : null
    // Issue #66: Reporting must stay a label/control grid.  A four-column
    // layout can be measured against the outer Settings dialog instead of
    // this tab's actual viewport on KDE/XCB, leaving every second control
    // outside the clip region.  This page is deliberately two columns at all
    // sizes: it trades a little vertical space for reliable access to every
    // setting on fixed-size dialogs and every Qt platform plugin.
    readonly property int pageColumns: 2
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

    component UdpTrafficCheck: CheckBox {
        id: trafficCheck
        property var settingsHost
        property string settingKey
        property bool settingFallback: true
        checked: settingsHost ? settingsHost.boolSetting(settingKey, settingFallback) : settingFallback
        onToggled: {
            if (settingsHost)
                settingsHost.setBoolSettingIfChanged(settingKey, checked, settingFallback)
        }
        indicator: Rectangle {
            width: 18; height: 18; radius: 3
            color: trafficCheck.settingsHost
                   ? (trafficCheck.checked ? trafficCheck.settingsHost.primaryBlue : trafficCheck.settingsHost.bgMedium)
                   : "#101722"
            border.color: trafficCheck.settingsHost ? trafficCheck.settingsHost.glassBorder : "#334455"
            y: trafficCheck.height / 2 - height / 2
        }
        contentItem: Text {
            text: trafficCheck.text
            color: trafficCheck.settingsHost
                   ? (trafficCheck.enabled ? trafficCheck.settingsHost.textPrimary : trafficCheck.settingsHost.textDim)
                   : "#f2f5f7"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            leftPadding: 24
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

        // ── Servizi di Rete ──
        Text { text: qsTr("NETWORK SERVICES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("PSK Reporter:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.pskReporterEnabled
            onCheckedChanged: {
                bridge.pskReporterEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("TCP/IP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("PSKReporterTCPIP", false)
            onCheckedChanged: bridge.setSetting("PSKReporterTCPIP", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Query history:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: pskReporterTimeSpanCombo
            model: ["5 min", "10 min", "15 min", "20 min", "25 min", "30 min",
                    "35 min", "40 min", "45 min", "50 min", "55 min", "60 min"]
            currentIndex: Math.max(0, Math.min(11, Math.round(bridge.pskReporterTimeSpanMinutes / 5) - 1))
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); implicitHeight: controlHeight
            onActivated: {
                bridge.pskReporterTimeSpanMinutes = (currentIndex + 1) * 5
                dialog.scheduleSettingsPersist()
            }
            popup: SettingsComboPopup { combo: pskReporterTimeSpanCombo }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("PSK Reporter look-back period for callsign search and heard-by results.")
        }

        // ── DX Cluster ──
        Text { text: qsTr("DX CLUSTER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            id: dxClusterHostField
            text: bridge.dxCluster && bridge.dxCluster.host !== undefined ? String(bridge.dxCluster.host) : ""
            Layout.fillWidth: true
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            placeholderText: qsTr("dx.iz7auh.net")
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: if (bridge.dxCluster) bridge.dxCluster.host = text.trim()
        }
        Text { text: qsTr("Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: dxClusterPortSpin
            from: 1; to: 65535
            value: {
                var port = bridge.dxCluster && bridge.dxCluster.port !== undefined ? Number(bridge.dxCluster.port) : 8000
                return isFinite(port) ? port : 8000
            }
            editable: true
            implicitHeight: controlHeight
            Layout.fillWidth: true
            Layout.preferredWidth: portFieldMinWidth
            onValueChanged: if (bridge.dxCluster) bridge.dxCluster.port = value
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: dxClusterPortSpin.textFromValue(dxClusterPortSpin.value, dxClusterPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !dxClusterPortSpin.editable; validator: dxClusterPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        RowLayout {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 10

            Text {
                text: bridge.dxCluster && bridge.dxCluster.connected ? qsTr("Connected") : qsTr("Disconnected")
                color: bridge.dxCluster && bridge.dxCluster.connected ? accentGreen : textSecondary
                font.pixelSize: 12
            }

            Rectangle {
                width: 96; height: controlHeight; radius: 4
                color: dxClusterConnMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : bgMedium
                border.color: accentGreen
                Text { anchors.centerIn: parent; text: qsTr("Connect"); color: accentGreen; font.pixelSize: 12 }
                MouseArea {
                    id: dxClusterConnMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (!bridge.dxCluster) return
                        bridge.dxCluster.host = dxClusterHostField.text.trim()
                        bridge.dxCluster.port = dxClusterPortSpin.value
                        bridge.dxCluster.callsign = bridge.callsign
                        bridge.connectDxCluster()
                    }
                }
            }

            Rectangle {
                width: 110; height: controlHeight; radius: 4
                color: dxClusterDiscMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.2) : bgMedium
                border.color: "#f44336"
                Text { anchors.centerIn: parent; text: qsTr("Disconnect"); color: "#f44336"; font.pixelSize: 12 }
                MouseArea {
                    id: dxClusterDiscMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bridge.disconnectDxCluster()
                }
            }
        }

        Text { text: qsTr("Detail:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        Text {
            text: bridge.dxCluster && bridge.dxCluster.lastStatus ? bridge.dxCluster.lastStatus : qsTr("No message")
            color: textSecondary
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
        }

        // ── Cloudlog ──
        Text { text: qsTr("CLOUDLOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.cloudlogEnabled
            onCheckedChanged: {
                bridge.cloudlogEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { text: qsTr("API URL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.cloudlogUrl; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: Math.max(1, pageColumns - 1)
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.cloudlogUrl = text
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("API Key:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.cloudlogApiKey; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: Math.max(1, pageColumns - 1)
            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                bridge.cloudlogApiKey = text
                dialog.scheduleSettingsPersist()
            }
        }

        Text { text: qsTr("Station ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: cloudlogStIdSpin
            from: 0; to: 999; value: Number(bridge.getSetting("CloudLogStationID", 1)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("CloudLogStationID", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: cloudlogStIdSpin.textFromValue(cloudlogStIdSpin.value, cloudlogStIdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !cloudlogStIdSpin.editable; validator: cloudlogStIdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── QRZ Logbook ──
        Text { text: qsTr("QRZ LOGBOOK"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.qrzLogbookEnabled
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: {
                bridge.qrzLogbookEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Replace duplicates:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 130 }
        CheckBox {
            checked: bridge.qrzLogbookReplaceDuplicates
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: {
                bridge.qrzLogbookReplaceDuplicates = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("API Key:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.qrzLogbookApiKey; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: Math.max(1, pageColumns - 1)
            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
	                                bridge.qrzLogbookApiKey = text
	                                dialog.scheduleSettingsPersist()
	                                dialog.qrzLogbookTestStatus = ""
                dialog.qrzLogbookTestIsError = false
                dialog.qrzLogbookTestBusy = false
            }
        }

        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        Rectangle {
            width: 110; height: controlHeight; radius: 4
            opacity: dialog.qrzLogbookTestBusy ? 0.75 : 1
            color: qrzTestMA.containsMouse && !dialog.qrzLogbookTestBusy ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.2) : bgMedium
            border.color: dialog.qrzLogbookTestBusy ? textSecondary : secondaryCyan
            Text {
                anchors.centerIn: parent
                text: dialog.qrzLogbookTestBusy ? qsTr("Testing...") : qsTr("Test")
                color: dialog.qrzLogbookTestBusy ? textSecondary : secondaryCyan
                font.pixelSize: 12
            }
            MouseArea {
                id: qrzTestMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                enabled: !dialog.qrzLogbookTestBusy
                onClicked: {
                    dialog.qrzLogbookTestBusy = true
                    dialog.qrzLogbookTestIsError = false
                    dialog.qrzLogbookTestStatus = qsTr("Testing QRZ API key...")
                    bridge.testQrzLogbookApi()
                }
            }
        }
        Text {
            text: dialog.qrzLogbookTestStatus
            visible: text.length > 0
            color: dialog.qrzLogbookTestIsError ? "#ff5252" : (dialog.qrzLogbookTestBusy ? textSecondary : accentGreen)
            font.pixelSize: 12
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.columnSpan: 2
            Layout.preferredHeight: Math.max(controlHeight, implicitHeight)
        }

        // ── LotW ──
        Text { text: qsTr("LOTW"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("LotW Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.lotwEnabled
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: {
                bridge.lotwEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Password:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("Lotw_pwd", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
            Layout.minimumWidth: fieldMinWidth
            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("Lotw_pwd", text)
        }

        Text { text: qsTr("Non-QSL'd:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("NonQsl", false)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: bridge.setSetting("NonQsl", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Days Upload:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: lotwDaysSpin
            from: 0; to: 9999; value: Number(bridge.getSetting("LotWDaysSinceLastUpload", 365)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth
            onValueChanged: bridge.setSetting("LotWDaysSinceLastUpload", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: lotwDaysSpin.textFromValue(lotwDaysSpin.value, lotwDaysSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !lotwDaysSpin.editable; validator: lotwDaysSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        // ── Logging ──
        Text { text: qsTr("LOGGING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Prompt to Log:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: promptToLogCheck
            checked: boolSetting("PromptToLog", false)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onToggled: {
                if (!dialog.loggingChecksUpdating)
                    dialog.setLoggingMode(checked)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Auto Log:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: autoLogCheck
            checked: boolSetting("AutoLog", true)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onToggled: {
                if (!dialog.loggingChecksUpdating)
                    dialog.setLoggingMode(!checked)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            Component.onCompleted: Qt.callLater(function() { dialog.normalizeLoggingModeChecks() })
        }

        Text { text: qsTr("Log as RTTY:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("LogAsRTTY", false)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: bridge.setSetting("LogAsRTTY", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("4-digit Grids:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("Log4DigitGrids", false)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: bridge.setSetting("Log4DigitGrids", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Contest Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            enabled: !promptToLogCheck.checked
            opacity: enabled ? 1.0 : 0.45
            checked: bridge.getSetting("ContestingOnly", false)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: bridge.setSetting("ContestingOnly", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Spec Op Cmts:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("SpecOpInComments", false)
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            onCheckedChanged: bridge.setSetting("SpecOpInComments", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("dB in Cmts:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("dBReportsToComments", false)
            onCheckedChanged: bridge.setSetting("dBReportsToComments", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("ZZ00:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("ZZ00", false)
            onCheckedChanged: bridge.setSetting("ZZ00", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── Registrazione ──
        Text { text: qsTr("RECORDING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Record RX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.recordRxEnabled
            onCheckedChanged: {
                bridge.recordRxEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Record TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.recordTxEnabled
            onCheckedChanged: {
                bridge.recordTxEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("WSPR Upload:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.wsprUploadEnabled
            onCheckedChanged: {
                bridge.wsprUploadEnabled = checked
                dialog.scheduleSettingsPersist()
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── Remote Web Dashboard ──
        Text { text: qsTr("REMOTE WEB DASHBOARD (LAN)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("RemoteWebEnabled", false)
            onCheckedChanged: bridge.setSetting("RemoteWebEnabled", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("HTTP port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: remoteHttpPortSpin
            from: 1025; to: 65535; value: Number(bridge.getSetting("RemoteHttpPort", 19091)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("RemoteHttpPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: remoteHttpPortSpin.textFromValue(remoteHttpPortSpin.value, remoteHttpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !remoteHttpPortSpin.editable; validator: remoteHttpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("WS socket port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            readOnly: true
            text: String(bridge.remoteWebSocketPort())
            Layout.fillWidth: true
            Layout.preferredWidth: portFieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("WS bind:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            text: bridge.getSetting("RemoteWsBind", "0.0.0.0"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("RemoteWsBind", text)
        }
        Text { text: qsTr("Username:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            text: bridge.getSetting("RemoteUser", "admin"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("RemoteUser", text)
        }

        Text { text: qsTr("Access token:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            text: bridge.getSetting("RemoteToken", ""); Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
            placeholderText: qsTr("Required for LAN/WAN")
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("RemoteToken", text)
        }

        Text {
            text: qsTr("App restart required. For LAN/WAN, use a token of at least 12 characters.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.columnSpan: pageColumns
        }

        // ── UDP Server ──
        Text { text: qsTr("UDP SERVER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Client ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            id: udpClientIdField
            text: bridge.getSetting("UDPClientId", "WSJTX")
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            maximumLength: 64
            color: textPrimary
            font.pixelSize: controlFontSize
            inputMethodHints: Qt.ImhNoPredictiveText
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                var cleaned = String(text).trim()
                if (!cleaned.length)
                    cleaned = "WSJTX"
                if (cleaned !== text)
                    text = cleaned
                bridge.setSetting("UDPClientId", cleaned)
            }
        }
        Text { text: qsTr("Preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: udpClientIdPreset
            model: ["WSJTX", "Decodium"]
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            Component.onCompleted: currentIndex = Math.max(0, find(String(bridge.getSetting("UDPClientId", "WSJTX"))))
            onActivated: {
                udpClientIdField.text = currentText
                bridge.setSetting("UDPClientId", currentText)
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: udpClientIdPreset.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Server Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            text: bridge.getSetting("UDPServer", "127.0.0.1"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("UDPServer", text)
        }
        Text { text: qsTr("Server Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpPortSpin
            from: 1; to: 65535; value: Number(bridge.getSetting("UDPServerPort", 2237)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPServerPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpPortSpin.textFromValue(udpPortSpin.value, udpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpPortSpin.editable; validator: udpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Listen Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpListenSpin
            from: 0; to: 65535; value: Number(bridge.getSetting("UDPListenPort", 0)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPListenPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpListenSpin.textFromValue(udpListenSpin.value, udpListenSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpListenSpin.editable; validator: udpListenSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Multicast TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpTtlSpin
            from: 0; to: 255; value: Number(bridge.getSetting("UDPTTL", 1)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPTTL", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpTtlSpin.textFromValue(udpTtlSpin.value, udpTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpTtlSpin.editable; validator: udpTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Interface Used:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: udpInterfaceCombo
            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            Component.onCompleted: {
                var saved = bridge.udpInterfaceName()
                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
            }
            onActivated: bridge.setUdpInterfaceName(currentIndex <= 0 ? "" : currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: udpInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Primary traffic:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        Flow {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 16
            UdpTrafficCheck { settingsHost: root; text: qsTr("Decode"); settingKey: "UDPPrimarySendDecode" }
            UdpTrafficCheck { settingsHost: root; text: qsTr("Status"); settingKey: "UDPPrimarySendStatus" }
            UdpTrafficCheck {
                settingsHost: root; text: qsTr("QSO logged"); settingKey: "UDPPrimarySendQso"
                settingFallback: root.boolSetting("UDPPrimaryLoggedAdifEnabled", true)
            }
            UdpTrafficCheck { settingsHost: root; text: qsTr("WSPR"); settingKey: "UDPPrimarySendWspr" }
        }

        Text { text: qsTr("Secondary UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            id: udpSecondaryCheck
            checked: boolSetting("UDPSecondaryEnabled", true)
            onToggled: setBoolSettingIfChanged("UDPSecondaryEnabled", checked, true)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Secondary Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            text: bridge.getSetting("UDPSecondaryServer", bridge.getSetting("UDPServer", "127.0.0.1")); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("UDPSecondaryServer", text)
        }


        Text { text: qsTr("Secondary Client ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            id: udpSecondaryIdField
            text: bridge.getSetting("UDPSecondaryClientId", "Decodium")
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            maximumLength: 64
            color: textPrimary
            font.pixelSize: controlFontSize
            inputMethodHints: Qt.ImhNoPredictiveText
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                var cleaned = String(text).trim()
                if (!cleaned.length)
                    cleaned = "Decodium"
                if (cleaned !== text)
                    text = cleaned
                bridge.setSetting("UDPSecondaryClientId", cleaned)
            }
        }
        Text { text: qsTr("Preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: udpSecondaryIdPreset
            model: ["Decodium", "WSJTX"]
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            Component.onCompleted: currentIndex = Math.max(0, find(String(bridge.getSetting("UDPSecondaryClientId", "Decodium"))))
            onActivated: {
                udpSecondaryIdField.text = currentText
                bridge.setSetting("UDPSecondaryClientId", currentText)
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: udpSecondaryIdPreset.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Secondary Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpSecondaryPortSpin
            from: 1; to: 65535; value: Number(bridge.getSetting("UDPSecondaryServerPort", 2239)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPSecondaryServerPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpSecondaryPortSpin.textFromValue(udpSecondaryPortSpin.value, udpSecondaryPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpSecondaryPortSpin.editable; validator: udpSecondaryPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Secondary TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpSecondaryTtlSpin
            from: 0; to: 255; value: Number(bridge.getSetting("UDPSecondaryTTL", bridge.getSetting("UDPTTL", 1))); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPSecondaryTTL", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpSecondaryTtlSpin.textFromValue(udpSecondaryTtlSpin.value, udpSecondaryTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpSecondaryTtlSpin.editable; validator: udpSecondaryTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Secondary Interface:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: udpSecondaryInterfaceCombo
            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            Component.onCompleted: {
                var saved = String(bridge.getSetting("UDPSecondaryInterface", ""))
                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
            }
            onActivated: bridge.setSetting("UDPSecondaryInterface", currentIndex <= 0 ? "" : currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: udpSecondaryInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Secondary traffic:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        Flow {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 16
            UdpTrafficCheck { settingsHost: root; text: qsTr("Decode"); settingKey: "UDPSecondarySendDecode"; enabled: udpSecondaryCheck.checked }
            UdpTrafficCheck { settingsHost: root; text: qsTr("Status"); settingKey: "UDPSecondarySendStatus"; enabled: udpSecondaryCheck.checked }
            UdpTrafficCheck {
                settingsHost: root; text: qsTr("QSO logged"); settingKey: "UDPSecondarySendQso"
                settingFallback: root.boolSetting("UDPSecondaryLoggedAdifEnabled", true)
                enabled: udpSecondaryCheck.checked
            }
            UdpTrafficCheck { settingsHost: root; text: qsTr("WSPR"); settingKey: "UDPSecondarySendWspr"; enabled: udpSecondaryCheck.checked }
        }

        Text { text: qsTr("Tertiary UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            id: udpTertiaryCheck
            checked: boolSetting("UDPTertiaryEnabled", false)
            onToggled: setBoolSettingIfChanged("UDPTertiaryEnabled", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Tertiary Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            text: bridge.getSetting("UDPTertiaryServer", "127.0.0.1"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            enabled: udpTertiaryCheck.checked
            opacity: enabled ? 1.0 : 0.5
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("UDPTertiaryServer", text)
        }


        Text { text: qsTr("Tertiary Client ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            id: udpTertiaryIdField
            text: bridge.getSetting("UDPTertiaryClientId", "Decodium")
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            maximumLength: 64
            color: textPrimary
            font.pixelSize: controlFontSize
            inputMethodHints: Qt.ImhNoPredictiveText
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                var cleaned = String(text).trim()
                if (!cleaned.length)
                    cleaned = "Decodium"
                if (cleaned !== text)
                    text = cleaned
                bridge.setSetting("UDPTertiaryClientId", cleaned)
            }
        }
        Text { text: qsTr("Preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: udpTertiaryIdPreset
            model: ["Decodium", "WSJTX"]
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            Component.onCompleted: currentIndex = Math.max(0, find(String(bridge.getSetting("UDPTertiaryClientId", "Decodium"))))
            onActivated: {
                udpTertiaryIdField.text = currentText
                bridge.setSetting("UDPTertiaryClientId", currentText)
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: udpTertiaryIdPreset.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Tertiary Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpTertiaryPortSpin
            from: 1; to: 65535; value: Number(bridge.getSetting("UDPTertiaryServerPort", 2237)); editable: true
            enabled: udpTertiaryCheck.checked
            opacity: enabled ? 1.0 : 0.5
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPTertiaryServerPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpTertiaryPortSpin.textFromValue(udpTertiaryPortSpin.value, udpTertiaryPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpTertiaryPortSpin.editable; validator: udpTertiaryPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: udpTertiaryPortSpin.enabled }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Text { text: qsTr("Tertiary TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: udpTertiaryTtlSpin
            from: 0; to: 255; value: Number(bridge.getSetting("UDPTertiaryTTL", bridge.getSetting("UDPTTL", 1))); editable: true
            enabled: udpTertiaryCheck.checked
            opacity: enabled ? 1.0 : 0.5
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("UDPTertiaryTTL", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: udpTertiaryTtlSpin.textFromValue(udpTertiaryTtlSpin.value, udpTertiaryTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpTertiaryTtlSpin.editable; validator: udpTertiaryTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: udpTertiaryTtlSpin.enabled }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Tertiary Interface:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoComboBox {
            id: udpTertiaryInterfaceCombo
            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
            enabled: udpTertiaryCheck.checked
            opacity: enabled ? 1.0 : 0.5
            Layout.fillWidth: true
            Layout.minimumWidth: fieldMinWidth
            implicitHeight: controlHeight
            Component.onCompleted: {
                var saved = String(bridge.getSetting("UDPTertiaryInterface", ""))
                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
            }
            onActivated: bridge.setSetting("UDPTertiaryInterface", currentIndex <= 0 ? "" : currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: udpTertiaryInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Tertiary traffic:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        Flow {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 16
            UdpTrafficCheck { settingsHost: root; text: qsTr("Decode"); settingKey: "UDPTertiarySendDecode"; enabled: udpTertiaryCheck.checked }
            UdpTrafficCheck { settingsHost: root; text: qsTr("Status"); settingKey: "UDPTertiarySendStatus"; enabled: udpTertiaryCheck.checked }
            UdpTrafficCheck {
                settingsHost: root; text: qsTr("QSO logged"); settingKey: "UDPTertiarySendQso"
                settingFallback: root.boolSetting("UDPTertiaryLoggedAdifEnabled", true)
                enabled: udpTertiaryCheck.checked
            }
            UdpTrafficCheck { settingsHost: root; text: qsTr("WSPR"); settingKey: "UDPTertiarySendWspr"; enabled: udpTertiaryCheck.checked }
        }

        // ── N1MM Logger+ / HRD Logbook / EasyLog (ADIF UDP) ──
        Text { text: qsTr("N1MM / HRD LOGBOOK / EASYLOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text {
            text: qsTr("Use this ADIF UDP output for N1MM Logger+ or HRD Logbook QSO Forwarding. HRD normally listens on 127.0.0.1:2333; this is different from the primary WSJT-X UDP Server above.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
        }

        Item { Layout.fillWidth: true; Layout.preferredWidth: labelWidth }
        Button {
            id: hrdLogbookPresetButton
            text: qsTr("Use HRD Logbook preset")
            implicitHeight: controlHeight
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            onClicked: {
                // HRD's QSO Forwarding receives the same ADIF-over-UDP
                // stream configured as "N1MM Logger+ Broadcasts" in WSJT-X.
                bridge.setSetting("BroadcastToN1MM", true)
                bridge.setSetting("N1MMServer", "127.0.0.1")
                bridge.setSetting("N1MMServerPort", 2333)
                n1mmEnableCheck.checked = true
                n1mmServerField.text = "127.0.0.1"
                n1mmPortSpin.value = 2333
            }
            background: Rectangle {
                color: hrdLogbookPresetButton.hovered
                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
                       : bgMedium
                border.color: secondaryCyan
                radius: 4
            }
            contentItem: Text {
                text: hrdLogbookPresetButton.text
                color: secondaryCyan
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Enables the N1MM-compatible ADIF UDP output and sets 127.0.0.1:2333. In HRD Logbook, enable UDP Receive / WSJT-X QSO Forwarding on port 2333.")
        }

        Text { text: qsTr("Enable output:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            id: n1mmEnableCheck
            checked: boolSetting("BroadcastToN1MM", false)
            onToggled: setBoolSettingIfChanged("BroadcastToN1MM", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("UDP Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: n1mmPortSpin
            from: 1; to: 65535; value: Number(bridge.getSetting("N1MMServerPort", 2333)); editable: true
            enabled: n1mmEnableCheck.checked
            opacity: enabled ? 1.0 : 0.5
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("N1MMServerPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: n1mmPortSpin.textFromValue(n1mmPortSpin.value, n1mmPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !n1mmPortSpin.editable; validator: n1mmPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: n1mmPortSpin.enabled }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("UDP Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            id: n1mmServerField
            text: bridge.getSetting("N1MMServer", "127.0.0.1"); Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            enabled: n1mmEnableCheck.checked
            opacity: enabled ? 1.0 : 0.5
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("N1MMServer", text)
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: pageColumns; Layout.preferredHeight: 6 }

        Text { text: qsTr("Accept UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            // Default allineato con Configuration.cpp (true) per evitare
            // che il primo onCheckedChanged scriva `false` nel legacy INI
            // prima che Configuration abbia fatto write_settings.
            checked: bridge.getSetting("AcceptUDPRequests", true)
            onCheckedChanged: bridge.setSetting("AcceptUDPRequests", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Notify Request:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("NotifyOnRequest", false)
            onCheckedChanged: bridge.setSetting("NotifyOnRequest", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Restore Win:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("udpWindowRestore", false)
            onCheckedChanged: bridge.setSetting("udpWindowRestore", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── ADIF TCP ──
        Text { text: qsTr("ADIF TCP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Enable TCP ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        CheckBox {
            id: adifTcpCheck
            checked: boolSetting("ADIFTcpEnabled", false)
            onToggled: setBoolSettingIfChanged("ADIFTcpEnabled", checked, false)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("TCP Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        SpinBox {
            id: adifTcpPortSpin
            from: 1; to: 65535; value: Number(bridge.getSetting("ADIFTcpPort", 52001)); editable: true
            enabled: adifTcpCheck.checked
            opacity: enabled ? 1.0 : 0.5
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
            onValueChanged: bridge.setSetting("ADIFTcpPort", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: adifTcpPortSpin.textFromValue(adifTcpPortSpin.value, adifTcpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !adifTcpPortSpin.editable; validator: adifTcpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: adifTcpPortSpin.enabled }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("TCP Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
        DecoTextField {
            text: bridge.getSetting("ADIFTcpServer", "127.0.0.1"); Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
            enabled: adifTcpCheck.checked
            opacity: enabled ? 1.0 : 0.5
            color: textPrimary; font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: bridge.setSetting("ADIFTcpServer", text)
        }

        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            Layout.preferredHeight: 96
        }
    }
}
