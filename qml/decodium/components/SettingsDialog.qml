/* Decodium Qt6 - Complete Settings Dialog
 * All Decodium settings organized in tabs
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: settingsDialog
    title: "Decodium Settings"
    width: 1100
    height: 780
    modal: true
    closePolicy: Popup.CloseOnEscape

    // Dynamic theme colors from ThemeManager
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    // Standardized layout constants
    readonly property int outerMargin: 16
    readonly property int innerMargin: 16
    readonly property int groupSpacing: 12
    readonly property int rowSpacing: 12
    readonly property int colSpacing: 16
    readonly property int labelFontSize: 13
    readonly property int titleFontSize: 16
    readonly property int fieldHeight: 38

    background: Rectangle {
        color: bgMedium
        border.color: glassBorder
        border.width: 1
        radius: 12
    }

    header: Rectangle {
        height: 50
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
        radius: 12

        Text {
            anchors.centerIn: parent
            text: "Decodium Settings"
            font.pixelSize: 18
            font.bold: true
            color: secondaryCyan
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 12
            color: parent.color
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Tab bar
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            implicitHeight: 52

            background: Rectangle {
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
            }

            Repeater {
                model: ["Station", "Radio", "Audio", "Network", "Decoder", "Display", "Logging"]

                TabButton {
                    text: modelData
                    width: Math.max(140, Math.floor(tabBar.width / 7))
                    implicitHeight: 48
                    padding: 0
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 15
                        color: parent.checked ? secondaryCyan : textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.checked ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : "transparent"
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 2
                            color: parent.parent.checked ? secondaryCyan : "transparent"
                        }
                    }
                }
            }
        }

        // Tab content
        StackLayout {
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ========== STATION TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    // Station Info Group
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: stationGrid.implicitHeight + 40
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Station Information"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            GridLayout {
                                id: stationGrid
                                columns: 4
                                columnSpacing: 16
                                rowSpacing: 12

                                Text { text: "Callsign:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: callsignField
                                    text: bridge.callsign
                                    Layout.preferredWidth: 100
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }

                                Text { text: "Grid:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: gridField
                                    text: bridge.grid
                                    Layout.preferredWidth: 70
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }

                                Text { text: "Name:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: nameField
                                    Layout.preferredWidth: 120
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }

                                Text { text: "QTH:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: qthField
                                    Layout.preferredWidth: 120
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }

                                Text { text: "Rig:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: rigField
                                    Layout.preferredWidth: 120
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }

                                Text { text: "Antenna:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: antennaField
                                    Layout.preferredWidth: 120
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }

                                Text { text: "Power (W):"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: powerField
                                    text: "100"
                                    Layout.preferredWidth: 60
                                    font.pixelSize: 11
                                    validator: IntValidator { bottom: 1; top: 9999 }
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }
                        }
                    }

                    // Contest Group
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: contestSettingsColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: contestSettingsColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Contest Settings"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 16
                                Text { text: "Contest:"; color: textSecondary; font.pixelSize: 12 }
                                StyledComboBox {
                                    id: contestCombo
                                    model: ["None", "EU RSQ", "NA VHF", "EU VHF", "ARRL Field Day", "ARRL Digital", "WW Digi DX", "FT4 DX", "FT8 DX", "FT Roundup"]
                                    Layout.preferredWidth: 180
                                    font.pixelSize: 11
                                    popupMinWidth: 220
                                    textHorizontalAlignment: Text.AlignLeft
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "Exchange:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: exchangeField
                                    Layout.preferredWidth: 80
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }
                        }
                    }

                    // Startup Options
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: startupOptionsColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: startupOptionsColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Startup Options"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16
                                CheckBox {
                                    id: autoStartMonitorCheck
                                    text: "Auto-start monitor on startup"
                                    contentItem: Text {
                                        text: parent.text
                                        leftPadding: 8
                                        color: textPrimary
                                        font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ========== RADIO TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    // Refresh serial ports when tab becomes visible
                    Component.onCompleted: {
                        if (bridge.catManager) bridge.catManager.refreshPorts()
                    }

                    // CAT Control
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: 220
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: bridge.catConnected ? accentGreen : glassBorder
                        radius: 8

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            RowLayout {
                                Text { text: "CAT Control"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 12; height: 12; radius: 6
                                    color: bridge.catConnected ? accentGreen : "#666"
                                    border.color: Qt.darker(color, 1.3)
                                }
                                Text {
                                    text: bridge.catConnected ? "Connected" : "Disconnected"
                                    color: bridge.catConnected ? accentGreen : textSecondary
                                    font.pixelSize: 11
                                }
                            }

                            // Radio info when connected
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                                radius: 4
                                visible: bridge.catConnected
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 10; spacing: 20
                                    Text { text: bridge.catRigName || "---"; color: accentGreen; font.pixelSize: 13; font.bold: true }
                                    Text { text: bridge.catMode || "---"; color: secondaryCyan; font.pixelSize: 12 }
                                    Item { Layout.fillWidth: true }
                                }
                            }

                            // Open full CAT settings dialog
                            Button {
                                text: "Configure CAT / PTT…"
                                implicitHeight: 36
                                Layout.alignment: Qt.AlignHCenter
                                background: Rectangle {
                                    radius: 6
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: primaryBlue }
                                        GradientStop { position: 1.0; color: secondaryCyan }
                                    }
                                }
                                contentItem: Text {
                                    text: parent.text; color: textPrimary
                                    font.pixelSize: 12; font.bold: true
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: bridge.openCatSettings()
                            }
                        }
                    }
                }
            }

            // ========== AUDIO TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    // Input
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: audioInputColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: audioInputColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Audio Input"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Text { text: "Device:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 80 }
                                StyledComboBox {
                                    id: audioInputCombo
                                    property bool initialized: false
                                    model: bridge.audioInputDevices
                                    Connections {
                                        target: bridge
                                        function onAudioInputDevicesChanged() { audioInputRestoreTimer.start() }
                                    }
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                    popupMinWidth: 520
                                    textHorizontalAlignment: Text.AlignLeft
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }

                                    Timer {
                                        id: audioInputRestoreTimer
                                        interval: 150
                                        repeat: false
                                        onTriggered: {
                                            var savedDevice = bridge.audioInputDevice
                                            var idx = audioInputCombo.find(savedDevice)
                                            if (idx >= 0) audioInputCombo.currentIndex = idx
                                            audioInputCombo.initialized = true
                                        }
                                    }

                                    Component.onCompleted: { audioInputRestoreTimer.start() }

                                    onActivated: function(index) {
                                        var deviceName = textAt(index)
                                        if (deviceName !== "") {
                                            bridge.audioInputDevice = deviceName
                                        }
                                    }

                                    onCurrentIndexChanged: {
                                        if (initialized && currentIndex >= 0 && currentText !== "") {
                                            bridge.audioInputDevice = currentText
                                        }
                                    }
                                }
                                Button {
                                    implicitWidth: 36; implicitHeight: 36
                                    ToolTip.text: "Aggiorna dispositivi audio"; ToolTip.visible: hovered
                                    onClicked: bridge.refreshAudioDevices()
                                    background: Rectangle { color: hovered ? Qt.rgba(1,1,1,0.1) : "transparent"; border.color: glassBorder; radius: 4 }
                                    contentItem: Text { text: "↻"; color: secondaryCyan; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 6
                                columnSpacing: 16
                                rowSpacing: 10

                                Text { text: "Sample Rate:"; color: textSecondary; font.pixelSize: 12 }
                                StyledComboBox {
                                    model: ["44100", "48000", "96000"]
                                    currentIndex: 1
                                    Layout.preferredWidth: 130
                                    font.pixelSize: 12
                                    popupMinWidth: 150
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "Channel:"; color: textSecondary; font.pixelSize: 12 }
                                StyledComboBox {
                                    id: audioChannelCombo
                                    model: ["Left/Mono", "Right"]
                                    currentIndex: bridge.audioInputChannel === 1 ? 1 : 0
                                    Layout.preferredWidth: 150
                                    font.pixelSize: 12
                                    popupMinWidth: 170
                                    textHorizontalAlignment: Text.AlignLeft
                                    onActivated: bridge.audioInputChannel = currentIndex
                                    onCurrentIndexChanged: {
                                        if (currentIndex >= 0 && bridge.audioInputChannel !== currentIndex)
                                            bridge.audioInputChannel = currentIndex
                                    }
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "Bits:"; color: textSecondary; font.pixelSize: 12 }
                                StyledComboBox {
                                    model: ["16", "24", "32"]
                                    Layout.preferredWidth: 100
                                    font.pixelSize: 12
                                    popupMinWidth: 110
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }
                        }
                    }

                    // Output
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: audioOutputColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: audioOutputColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Audio Output"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Text { text: "Device:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 80 }
                                StyledComboBox {
                                    id: audioOutputCombo
                                    property bool initialized: false
                                    model: bridge.audioOutputDevices
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                    popupMinWidth: 520
                                    textHorizontalAlignment: Text.AlignLeft
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }

                                    Timer {
                                        id: audioOutputRestoreTimer
                                        interval: 150
                                        repeat: false
                                        onTriggered: {
                                            var savedDevice = bridge.audioOutputDevice
                                            var idx = audioOutputCombo.find(savedDevice)
                                            if (idx >= 0) audioOutputCombo.currentIndex = idx
                                            audioOutputCombo.initialized = true
                                        }
                                    }

                                    Component.onCompleted: { audioOutputRestoreTimer.start() }

                                    onActivated: {
                                        var deviceName = textAt(currentIndex)
                                        if (initialized && currentIndex >= 0 && deviceName !== "") {
                                            bridge.audioOutputDevice = deviceName
                                        }
                                    }

                                    onCurrentIndexChanged: {
                                        var deviceName = textAt(currentIndex)
                                        if (initialized && currentIndex >= 0 && deviceName !== "") {
                                            bridge.audioOutputDevice = deviceName
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Text { text: "TX Level:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 80 }
                                Slider { id: txLevel; from: 0; to: 100; value: 80; Layout.preferredWidth: 260 }
                                Text { text: Math.round(txLevel.value) + "%"; color: textPrimary; font.pixelSize: 11 }
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }

            // ========== NETWORK TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    // PSK Reporter
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: pskReporterColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: pskReporterColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            RowLayout {
                                Text { text: "PSK Reporter"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }
                                Item { Layout.fillWidth: true }
                            }

                            RowLayout {
                                spacing: 16
                                CheckBox {
                                    id: pskEnable
                                    text: "Abilita PSK Reporter"
                                    checked: bridge.pskReporterEnabled
                                    onCheckedChanged: bridge.pskReporterEnabled = checked
                                }
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: bridge.pskReporterConnected ? "#00ff00" : "#ff4444"
                                }
                                Text {
                                    text: bridge.pskReporterConnected ? "Connesso" : "Non connesso"
                                    color: bridge.pskReporterConnected ? "#00ff00" : textSecondary
                                    font.pixelSize: 11
                                }
                            }

                            // PSK Reporter Station Search
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.topMargin: 8
                                height: 1
                                color: glassBorder
                            }

                            RowLayout {
                                spacing: 12
                                Text { text: "Search Station:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: pskSearchCall
                                    Layout.preferredWidth: 120
                                    placeholderText: "Callsign..."
                                    font.pixelSize: 11
                                    font.capitalization: Font.AllUppercase
                                    background: Rectangle {
                                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                                        border.color: pskSearchCall.activeFocus ? secondaryCyan : glassBorder
                                        radius: 4
                                    }
                                    onAccepted: {
                                        if (text.trim().length > 0) {
                                            Qt.openUrlExternally("https://pskreporter.info/pskmap.html?callsign=" + text.trim().toUpperCase())
                                        }
                                    }
                                }
                                Rectangle {
                                    width: 70
                                    height: 28
                                    radius: 4
                                    color: pskSearchMouseArea.containsMouse ? Qt.rgba(0, 200/255, 255/255, 0.3) : Qt.rgba(0, 200/255, 255/255, 0.15)
                                    border.color: secondaryCyan
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Search"
                                        color: secondaryCyan
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: pskSearchMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (pskSearchCall.text.trim().length > 0) {
                                                Qt.openUrlExternally("https://pskreporter.info/pskmap.html?callsign=" + pskSearchCall.text.trim().toUpperCase())
                                            }
                                        }
                                    }
                                }
                                Text {
                                    text: "(Check if station is active on PSK Reporter)"
                                    color: textSecondary
                                    font.pixelSize: 11
                                    font.italic: true
                                }
                            }
                        }
                    }

                    // UDP Server (WSJT-X Compatible)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: udpServerColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: udpServerColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            RowLayout {
                                Text { text: "UDP Server (WSJT-X Compatible)"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }
                                Item { Layout.fillWidth: true }
                                CheckBox {
                                    id: udpEnable
                                    text: "Enable"
                                    checked: bridge.udpServer ? bridge.udpServer.enabled : false
                                    onCheckedChanged: if (bridge.udpServer) bridge.udpServer.enabled = checked
                                }
                            }

                            RowLayout {
                                spacing: 16
                                Text { text: "Port:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: udpPort
                                    text: bridge.udpServer ? bridge.udpServer.port : "2237"
                                    Layout.preferredWidth: 60
                                    enabled: !udpEnable.checked
                                    font.pixelSize: 11
                                    onEditingFinished: if (bridge.udpServer) bridge.udpServer.port = parseInt(text)
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "(JTAlert, GridTracker, Log4OM)"; color: textSecondary; font.pixelSize: 11 }
                            }

                            RowLayout {
                                spacing: 16
                                // Connection status indicator
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: bridge.udpServer && bridge.udpServer.connected ? "#00ff00" : "#ff4444"
                                }
                                Text {
                                    text: bridge.udpServer ? bridge.udpServer.status : "Not initialized"
                                    color: textSecondary
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    // Remote Server (TCP JSON API)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: remoteServerColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: remoteServerColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            RowLayout {
                                Text { text: "Remote Server (TCP JSON API)"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }
                                Item { Layout.fillWidth: true }
                                CheckBox {
                                    id: remoteServerEnable
                                    text: "Enable"
                                    checked: bridge.remoteServer ? bridge.remoteServer.running : false
                                    onCheckedChanged: {
                                        if (bridge.remoteServer) {
                                            if (checked) {
                                                bridge.remoteServer.start()
                                            } else {
                                                bridge.remoteServer.stop()
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                spacing: 16
                                Text { text: "Port:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: remoteServerPort
                                    text: bridge.remoteServer ? bridge.remoteServer.port : "8765"
                                    Layout.preferredWidth: 60
                                    enabled: !remoteServerEnable.checked
                                    font.pixelSize: 11
                                    onEditingFinished: if (bridge.remoteServer) bridge.remoteServer.port = parseInt(text)
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "(Remote control via TCP JSON)"; color: textSecondary; font.pixelSize: 11 }
                            }

                            RowLayout {
                                spacing: 16
                                CheckBox {
                                    id: remoteAuthEnable
                                    text: "Require Authentication"
                                    checked: bridge.remoteServer ? bridge.remoteServer.authEnabled : false
                                    onCheckedChanged: if (bridge.remoteServer) bridge.remoteServer.authEnabled = checked
                                }
                            }

                            RowLayout {
                                spacing: 16
                                Text { text: "Auth Token:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: remoteAuthToken
                                    Layout.preferredWidth: 200
                                    enabled: remoteAuthEnable.checked
                                    echoMode: TextInput.Password
                                    placeholderText: "Enter secret token..."
                                    font.pixelSize: 11
                                    onEditingFinished: if (bridge.remoteServer) bridge.remoteServer.setAuthToken(text)
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }

                            RowLayout {
                                spacing: 16
                                // Connection status indicator
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: bridge.remoteServer && bridge.remoteServer.running ? "#00ff00" : "#ff4444"
                                }
                                Text {
                                    text: bridge.remoteServer && bridge.remoteServer.running
                                          ? "Running - " + bridge.remoteServer.clientCount + " client(s) connected"
                                          : "Stopped"
                                    color: textSecondary
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }

            // ========== DECODER TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: decoderSettingsColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: decoderSettingsColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Decoder Settings"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 20
                                CheckBox { text: "Deep Search"; checked: true }
                                CheckBox { text: "AP Decoding"; checked: true }
                                CheckBox { text: "Single Pass" }
                            }

                            RowLayout {
                                spacing: 20
                                CheckBox {
                                    id: multiAnswerModeCheck
                                    text: "Multi-Answer Mode (MAM)"
                                    checked: true
                                    onCheckedChanged: {
                                        bridge.multiAnswerMode = checked
                                    }
                                }
                                Text {
                                    text: "(Enable all TX slots for multi-QSO)"
                                    color: textSecondary
                                    font.pixelSize: 11
                                }
                            }

                            RowLayout {
                                spacing: 12
                                Text { text: "Aggressive (FTD):"; color: textSecondary; font.pixelSize: 12 }
                                Slider { id: ftdSlider; from: 0; to: 10; value: 3; stepSize: 1; Layout.preferredWidth: 150 }
                                Text { text: ftdSlider.value.toFixed(0); color: textPrimary; font.pixelSize: 11 }
                            }

                            RowLayout {
                                spacing: 12
                                Text { text: "Deep Search Level:"; color: textSecondary; font.pixelSize: 12 }
                                Slider { id: deepSlider; from: 0; to: 5; value: 2; stepSize: 1; Layout.preferredWidth: 150 }
                                Text { text: deepSlider.value.toFixed(0); color: textPrimary; font.pixelSize: 11 }
                            }

                            // IU8LMC: FT Threads setting for CPU optimization
                            RowLayout {
                                spacing: 12
                                Text { text: "FT Threads:"; color: textSecondary; font.pixelSize: 12 }
                                Slider {
                                    id: ftThreadsSlider
                                    from: 1
                                    to: 6
                                    value: bridge.ftThreads
                                    stepSize: 1
                                    Layout.preferredWidth: 150
                                    onValueChanged: {
                                        if (bridge.ftThreads !== value) {
                                            bridge.ftThreads = value
                                        }
                                    }
                                }
                                Text { text: ftThreadsSlider.value.toFixed(0); color: textPrimary; font.pixelSize: 11 }
                                Text { text: "(1=low CPU, 6=fast)"; color: textSecondary; font.pixelSize: 10 }
                            }

                            RowLayout {
                                spacing: 12
                                Text { text: "RX Bandwidth:"; color: textSecondary; font.pixelSize: 12 }
                                SpinBox { from: 100; to: 2500; value: 200; stepSize: 50 }
                                Text { text: "-"; color: textSecondary }
                                SpinBox { from: 500; to: 5000; value: 2800; stepSize: 50 }
                                Text { text: "Hz"; color: textSecondary; font.pixelSize: 12 }
                            }

                            Text { text: "Raptor Features (FT2)"; color: secondaryCyan; font.bold: true; font.pixelSize: 14; Layout.topMargin: 10 }

                            RowLayout {
                                spacing: 20
                                CheckBox {
                                    text: "L2 Triggered Decoder"
                                    checked: bridge.asyncL2Enabled
                                    onCheckedChanged: bridge.asyncL2Enabled = checked
                                }
                                CheckBox {
                                    text: "Async Decode"
                                    checked: bridge.asyncDecodeEnabled
                                    onCheckedChanged: bridge.asyncDecodeEnabled = checked
                                }
                            }

                            RowLayout {
                                spacing: 20
                                CheckBox {
                                    text: "EMA Averaging"
                                    checked: bridge.emaEnabled
                                    onCheckedChanged: bridge.emaEnabled = checked
                                }
                                Text { text: "Alpha:"; color: textSecondary; font.pixelSize: 12 }
                                Slider {
                                    from: 0.1; to: 0.9; value: bridge.emaAlpha; stepSize: 0.05
                                    Layout.preferredWidth: 120
                                    onValueChanged: bridge.emaAlpha = value
                                    enabled: bridge.emaEnabled
                                }
                                Text { text: bridge.emaAlpha.toFixed(2); color: textPrimary; font.pixelSize: 11 }
                            }

                            RowLayout {
                                spacing: 20
                                CheckBox {
                                    text: "B4 Strikethrough"
                                    checked: bridge.showB4Strikethrough
                                    onCheckedChanged: bridge.b4Strikethrough = checked
                                }
                                CheckBox {
                                    text: "Filter B4"
                                    checked: bridge.filterNoB4
                                    onCheckedChanged: bridge.filterNoB4 = checked
                                }
                            }

                            Text { text: "DX Cluster"; color: secondaryCyan; font.bold: true; font.pixelSize: 14; Layout.topMargin: 10 }
                            RowLayout {
                                spacing: 12
                                Text { text: "Host:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    id: dxcHost
                                    text: bridge.dxCluster ? bridge.dxCluster.host : "dxc.va7dx.com"
                                    Layout.preferredWidth: 160
                                    color: textPrimary; font.pixelSize: 11
                                    onEditingFinished: if (bridge.dxCluster) bridge.dxCluster.host = text
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "Porta:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    text: bridge.dxCluster ? bridge.dxCluster.port.toString() : "7300"
                                    Layout.preferredWidth: 60
                                    color: textPrimary; font.pixelSize: 11
                                    onEditingFinished: if (bridge.dxCluster) bridge.dxCluster.port = parseInt(text) || 7300
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }
                            RowLayout {
                                spacing: 12
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: bridge.dxCluster && bridge.dxCluster.connected ? "#00ff00" : "#ff4444"
                                }
                                Text {
                                    text: bridge.dxCluster && bridge.dxCluster.connected ? "Connesso" : "Disconnesso"
                                    color: textSecondary; font.pixelSize: 11
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 80; height: 26; radius: 4
                                    color: dxcConnMouse.containsMouse ? Qt.rgba(0,200/255,1,0.3) : Qt.rgba(0,200/255,1,0.12)
                                    border.color: secondaryCyan
                                    MouseArea {
                                        id: dxcConnMouse; anchors.fill: parent; hoverEnabled: true
                                        onClicked: {
                                            if (bridge.dxCluster) {
                                                if (bridge.dxCluster.connected) bridge.dxCluster.disconnectCluster()
                                                else bridge.dxCluster.connectCluster()
                                            }
                                        }
                                    }
                                    Text {
                                        anchors.centerIn: parent; font.pixelSize: 11
                                        text: bridge.dxCluster && bridge.dxCluster.connected ? "Disconnetti" : "Connetti"
                                        color: secondaryCyan
                                    }
                                }
                            }

                            // ─── Cloudlog ───────────────────────────────────────────────────────
                            Text { text: "Cloudlog"; color: secondaryCyan; font.bold: true; font.pixelSize: 14; Layout.topMargin: 10 }
                            RowLayout {
                                spacing: 12
                                CheckBox {
                                    text: "Abilita"
                                    checked: bridge.cloudlogEnabled
                                    onCheckedChanged: bridge.cloudlogEnabled = checked
                                }
                            }
                            RowLayout {
                                spacing: 8
                                Text { text: "URL:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    Layout.preferredWidth: 200; font.pixelSize: 11
                                    text: bridge.cloudlogUrl
                                    placeholderText: "http://192.168.1.10/index.php"
                                    color: textPrimary
                                    onEditingFinished: bridge.cloudlogUrl = text
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "Key:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    Layout.preferredWidth: 140; font.pixelSize: 11
                                    text: bridge.cloudlogApiKey
                                    placeholderText: "API key..."
                                    echoMode: TextInput.Password
                                    color: textPrimary
                                    onEditingFinished: bridge.cloudlogApiKey = text
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Rectangle {
                                    width: 60; height: 26; radius: 4
                                    color: testClMouse.containsMouse ? Qt.rgba(0,200/255,1,0.3) : Qt.rgba(0,200/255,1,0.12)
                                    border.color: secondaryCyan
                                    MouseArea { id: testClMouse; anchors.fill: parent; hoverEnabled: true; onClicked: bridge.testCloudlogApi() }
                                    Text { anchors.centerIn: parent; text: "Test"; font.pixelSize: 11; color: secondaryCyan }
                                }
                            }

                            // ─── LotW + WSPR ────────────────────────────────────────────────────
                            RowLayout {
                                spacing: 20; Layout.topMargin: 8
                                CheckBox {
                                    text: "LotW lookup"
                                    checked: bridge.lotwEnabled
                                    onCheckedChanged: bridge.lotwEnabled = checked
                                }
                                Rectangle {
                                    width: 100; height: 26; radius: 4
                                    color: lotwUpdMouse.containsMouse ? Qt.rgba(0,200/255,1,0.3) : Qt.rgba(0,200/255,1,0.12)
                                    border.color: secondaryCyan
                                    visible: bridge.lotwEnabled
                                    MouseArea { id: lotwUpdMouse; anchors.fill: parent; hoverEnabled: true; onClicked: bridge.updateLotwUsers() }
                                    Text { anchors.centerIn: parent; text: bridge.lotwUpdating ? "Aggiornamento…" : "Aggiorna lista"; font.pixelSize: 10; color: secondaryCyan }
                                }
                                CheckBox {
                                    text: "Upload WSPR"
                                    checked: bridge.wsprUploadEnabled
                                    onCheckedChanged: bridge.wsprUploadEnabled = checked
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: modeOptionsColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: modeOptionsColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 6

                            Text { text: "Mode Options"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 16
                                CheckBox { text: "FT8 DXpedition Mode" }
                                CheckBox { text: "FT4 Contest Mode" }
                                CheckBox { text: "Q65 Multi-Decode" }
                            }
                        }
                    }
                }
            }

            // ========== DISPLAY TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: waterfallDisplayColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: waterfallDisplayColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Waterfall Display"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 16
                                Text { text: "Palette:"; color: textSecondary; font.pixelSize: 12 }
                                StyledComboBox {
                                    id: paletteSettingsCombo
                                    model: ["SDR Classic", "Raptor Green", "Grayscale", "SmartSDR", "Hot (SDR#)", "deskHPSDR"]
                                    Layout.preferredWidth: 150
                                    font.pixelSize: 11
                                    currentIndex: Math.max(0, bridge.uiPaletteIndex)
                                    onActivated: {
                                        bridge.uiPaletteIndex = currentIndex
                                        mainWindow.scheduleSave()
                                    }
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }

                            RowLayout {
                                spacing: 12
                                Text { text: "Brightness:"; color: textSecondary; font.pixelSize: 12 }
                                Slider { from: 0; to: 100; value: 50; Layout.preferredWidth: 150 }
                                Text { text: "Contrast:"; color: textSecondary; font.pixelSize: 12 }
                                Slider { from: 0; to: 100; value: 50; Layout.preferredWidth: 150 }
                            }

                            RowLayout {
                                CheckBox { text: "Frequency Scale"; checked: true }
                                CheckBox { text: "Time Markers"; checked: true }
                            }
                        }
                    }

                    // Theme Selection
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: themeSelectionColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: themeSelectionColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Color Theme"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 16
                                Text { text: "Theme:"; color: textSecondary; font.pixelSize: 12 }
                                StyledComboBox {
                                    id: themeCombo
                                    model: bridge.themeManager ? bridge.themeManager.availableThemes : ["Ocean Blue"]
                                    currentIndex: {
                                        if (bridge.themeManager) {
                                            var themes = bridge.themeManager.availableThemes
                                            return themes.indexOf(bridge.themeManager.currentTheme)
                                        }
                                        return 0
                                    }
                                    Layout.preferredWidth: 180
                                    font.pixelSize: 11
                                    background: Rectangle {
                                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                                        border.color: glassBorder
                                        radius: 4
                                    }
                                    contentItem: Text {
                                        text: themeCombo.displayText
                                        color: textPrimary
                                        font.pixelSize: 11
                                        leftPadding: 8
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onCurrentIndexChanged: {
                                        if (bridge.themeManager && currentIndex >= 0) {
                                            var themes = bridge.themeManager.availableThemes
                                            if (currentIndex < themes.length) {
                                                bridge.themeManager.currentTheme = themes[currentIndex]
                                            }
                                        }
                                    }
                                }

                                // Theme preview colors
                                Row {
                                    spacing: 4
                                    Rectangle {
                                        width: 20; height: 20; radius: 3
                                        color: bridge.themeManager ? bridge.themeManager.primaryColor : "#1a73e8"
                                        ToolTip.visible: ma1.containsMouse
                                        ToolTip.text: "Primary"
                                        MouseArea { id: ma1; anchors.fill: parent; hoverEnabled: true }
                                    }
                                    Rectangle {
                                        width: 20; height: 20; radius: 3
                                        color: bridge.themeManager ? bridge.themeManager.secondaryColor : "#00bcd4"
                                        ToolTip.visible: ma2.containsMouse
                                        ToolTip.text: "Secondary"
                                        MouseArea { id: ma2; anchors.fill: parent; hoverEnabled: true }
                                    }
                                    Rectangle {
                                        width: 20; height: 20; radius: 3
                                        color: bridge.themeManager ? bridge.themeManager.accentColor : "#00e676"
                                        ToolTip.visible: ma3.containsMouse
                                        ToolTip.text: "Accent"
                                        MouseArea { id: ma3; anchors.fill: parent; hoverEnabled: true }
                                    }
                                }
                            }

                            Text {
                                text: "Theme changes will be fully applied after restart"
                                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.5)
                                font.pixelSize: 11
                                font.italic: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: textColorsColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: textColorsColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Text Colors"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 20
                                RowLayout {
                                    Text { text: "CQ:"; color: textSecondary; font.pixelSize: 12 }
                                    Rectangle { width: 40; height: 20; radius: 3; color: "#00e676"; border.color: glassBorder }
                                }
                                RowLayout {
                                    Text { text: "My Call:"; color: textSecondary; font.pixelSize: 12 }
                                    Rectangle { width: 40; height: 20; radius: 3; color: "#ff6b6b"; border.color: glassBorder }
                                }
                                RowLayout {
                                    Text { text: "TX:"; color: textSecondary; font.pixelSize: 12 }
                                    Rectangle { width: 40; height: 20; radius: 3; color: "#f44336"; border.color: glassBorder }
                                }
                                RowLayout {
                                    Text { text: "Worked:"; color: textSecondary; font.pixelSize: 12 }
                                    Rectangle { width: 40; height: 20; radius: 3; color: "#888888"; border.color: glassBorder }
                                }
                            }
                        }
                    }
                }
            }

            // ========== LOGGING TAB ==========
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.parent.width - 20
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.preferredHeight: logSettingsColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: logSettingsColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "Log Settings"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 12
                                Text { text: "Log File:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    text: "decodium_log.adi"
                                    Layout.preferredWidth: 200
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Button {
                                    text: "Browse..."
                                    implicitWidth: 110
                                    implicitHeight: 34
                                    padding: 0
                                    background: Rectangle { color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1); border.color: glassBorder; radius: 4 }
                                    contentItem: Text {
                                        text: parent.text
                                        color: textPrimary
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            RowLayout {
                                spacing: 16
                                CheckBox { text: "Auto-log QSOs"; checked: true }
                                CheckBox { text: "Prompt Before Log" }
                                CheckBox { text: "Log TX Messages" }
                            }

                            RowLayout {
                                spacing: 16
                                Text { text: "Default RST Sent:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    text: "599"
                                    Layout.preferredWidth: 50
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                                Text { text: "RST Rcvd:"; color: textSecondary; font.pixelSize: 12 }
                                TextField {
                                    text: "599"
                                    Layout.preferredWidth: 50
                                    font.pixelSize: 11
                                    background: Rectangle { color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8); border.color: glassBorder; radius: 4 }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.margins: 16
                        Layout.topMargin: 0
                        Layout.preferredHeight: adifExportColumn.implicitHeight + 36
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 8

                        ColumnLayout {
                            id: adifExportColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text { text: "ADIF Export"; color: secondaryCyan; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                spacing: 12
                                Button {
                                    text: "Export All"
                                    implicitWidth: 130
                                    implicitHeight: 34
                                    padding: 0
                                    background: Rectangle { color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2); border.color: accentGreen; radius: 4 }
                                    contentItem: Text {
                                        text: parent.text
                                        color: accentGreen
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                                Button {
                                    text: "Export Selected"
                                    implicitWidth: 150
                                    implicitHeight: 34
                                    padding: 0
                                    background: Rectangle { color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1); border.color: glassBorder; radius: 4 }
                                    contentItem: Text {
                                        text: parent.text
                                        color: textPrimary
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                                Button {
                                    text: "Import ADIF"
                                    implicitWidth: 130
                                    implicitHeight: 34
                                    padding: 0
                                    background: Rectangle { color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1); border.color: glassBorder; radius: 4 }
                                    contentItem: Text {
                                        text: parent.text
                                        color: textPrimary
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 72
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.94)

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 12
            color: parent.color
        }

        Item {
            anchors.fill: parent
            anchors.margins: 14

            Flow {
            id: settingsFooterFlow
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(parent.width, 364)
            spacing: 12

            Button {
                text: "Applica"
                implicitWidth: 120
                implicitHeight: 42
                padding: 0
                onClicked: settingsDialog.saveSettings()
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.16)
                    border.color: secondaryCyan
                }
                contentItem: Text {
                    text: parent.text
                    color: secondaryCyan
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "Annulla"
                implicitWidth: 120
                implicitHeight: 42
                padding: 0
                onClicked: settingsDialog.reject()
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                    border.color: glassBorder
                }
                contentItem: Text {
                    text: parent.text
                    color: textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "OK"
                implicitWidth: 100
                implicitHeight: 42
                padding: 0
                onClicked: {
                    settingsDialog.saveSettings()
                    settingsDialog.accept()
                }
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.18)
                    border.color: accentGreen
                }
                contentItem: Text {
                    text: parent.text
                    color: accentGreen
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        }
    }

    onOpened: {
        loadSettings()
    }

    function loadSettings() {
        callsignField.text = bridge.callsign
        gridField.text     = bridge.grid
    }

    function saveSettings() {
        bridge.callsign = callsignField.text.trim().toUpperCase()
        bridge.grid     = gridField.text.trim().toUpperCase()
        bridge.saveSettings()
        console.log("Settings saved")
    }
}
