/* Decodium Qt6 - Header Bar Component */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: headerBarComponent

    property string currentMode: "FT8"
    property double frequency: 14074000.0
    property bool monitoring: false
    property bool autoSeq: false
    property bool catConnected: false
    property double catFrequency: 0
    property string catMode: ""
    property bool transmitting: false

    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color glassBorder: bridge.themeManager.glassBorder
    property color glassOverlay: bridge.themeManager.glassOverlay
    property color textPrimary: bridge.themeManager.textPrimary
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color colorRed: bridge.themeManager.ledRed

    signal modeChanged(string mode)
    signal startMonitor()
    signal stopMonitor()
    signal toggleAutoSeq()
    signal tune()
    signal halt()

    readonly property var supportedModes: bridge ? bridge.availableModes() : ["FT8", "FT2", "FT4", "Q65", "MSK144", "JT65", "JT9", "JT4", "FST4", "FST4W", "WSPR"]

    height: 60
    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 16

        // Logo with icon
        RowLayout {
            spacing: 8

            // Decodium icon
            Image {
                source: "qrc:/icon_128x128.png"
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            // Decodium text
            Text {
                text: "Decodium"
                font.pixelSize: 24
                font.bold: true
                color: secondaryCyan
            }

            // Version badge
            Rectangle {
                width: 36
                height: 18
                radius: 9
                color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
                border.color: secondaryCyan
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "4.0"
                    font.pixelSize: 10
                    font.bold: true
                    color: secondaryCyan
                }
            }

            // Codename badge
            Text {
                text: "Core Shannon"
                font.pixelSize: 10
                font.italic: true
                color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.7)
                leftPadding: 2
            }
        }

        // Mode selector
        StyledComboBox {
            id: modeSelector
            model: supportedModes
            currentIndex: model.indexOf(currentMode)
            Layout.preferredWidth: 120

            background: Rectangle {
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                border.color: glassBorder
                radius: 8
            }

            onCurrentTextChanged: headerBarComponent.modeChanged(currentText)
        }

        // Radio Frequency Display (from CAT or manual)
        Rectangle {
            Layout.preferredWidth: catConnected ? 280 : 180
            Layout.preferredHeight: 44
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
            border.color: catConnected ? accentGreen : glassBorder
            border.width: catConnected ? 2 : 1
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                // CAT connection indicator
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: catConnected ? accentGreen : "#666"
                    visible: true

                    SequentialAnimation on opacity {
                        running: catConnected
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 1000 }
                        NumberAnimation { to: 1.0; duration: 1000 }
                    }
                }

                // Frequency
                Text {
                    text: {
                        var freq = catConnected ? catFrequency : frequency
                        return (freq / 1000000).toFixed(6)
                    }
                    font.pixelSize: 20
                    font.family: "Monospace"
                    font.bold: true
                    color: transmitting ? colorRed : accentGreen
                    Layout.fillWidth: true
                }

                Text {
                    text: "MHz"
                    font.pixelSize: 12
                    color: textPrimary
                    opacity: 0.7
                }

                // Mode from radio (when CAT connected)
                Rectangle {
                    visible: catConnected && catMode !== ""
                    width: 60
                    height: 24
                    radius: 4
                    color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
                    border.color: secondaryCyan

                    Text {
                        anchors.centerIn: parent
                        text: catMode
                        font.pixelSize: 10
                        font.bold: true
                        color: secondaryCyan
                    }
                }
            }

            // TX indicator overlay
            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "transparent"
                border.color: colorRed
                border.width: 3
                visible: transmitting
                opacity: transmitting ? 1 : 0

                SequentialAnimation on opacity {
                    running: transmitting
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 300 }
                    NumberAnimation { to: 1.0; duration: 300 }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // PSK Reporter Station Search
        RowLayout {
            spacing: 4

            Rectangle {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 32
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                border.color: pskSearchInput.activeFocus ? secondaryCyan : glassBorder
                radius: 6

                TextField {
                    id: pskSearchInput
                    anchors.fill: parent
                    anchors.margins: 2
                    placeholderText: (activeFocus || text.length > 0) ? "" : "Call..."
                    font.pixelSize: 11
                    font.capitalization: Font.AllUppercase
                    color: textPrimary
                    placeholderTextColor: textSecondary
                    verticalAlignment: TextInput.AlignVCenter
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 0
                    bottomPadding: 0
                    background: Rectangle { color: "transparent" }
                    onAccepted: {
                        if (text.trim().length > 0 && appEngine) {
                            appEngine.searchPskReporter(text.trim().toUpperCase())
                        }
                    }
                }
            }

            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: pskSearchBtn.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
                border.color: secondaryCyan

                Text {
                    anchors.centerIn: parent
                    text: "\uD83D\uDD0D"  // magnifying glass emoji
                    font.pixelSize: 14
                }

                MouseArea {
                    id: pskSearchBtn
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (pskSearchInput.text.trim().length > 0 && appEngine) {
                            appEngine.searchPskReporter(pskSearchInput.text.trim().toUpperCase())
                        }
                    }
                }

                ToolTip.visible: pskSearchBtn.containsMouse
                ToolTip.text: "Search station on PSK Reporter"
                ToolTip.delay: 500
            }
        }

        // Quick actions
        RowLayout {
            spacing: 10

            Button {
                text: monitoring ? "Stop" : "Monitor"
                highlighted: monitoring
                onClicked: monitoring ? headerBarComponent.stopMonitor() : headerBarComponent.startMonitor()

                background: Rectangle {
                    color: monitoring ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : glassOverlay
                    border.color: monitoring ? accentGreen : glassBorder
                    radius: 8
                }
            }

            Button {
                text: "Auto Seq"
                highlighted: autoSeq
                onClicked: headerBarComponent.toggleAutoSeq()

                background: Rectangle {
                    color: autoSeq ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : glassOverlay
                    border.color: autoSeq ? secondaryCyan : glassBorder
                    radius: 8
                }
            }

            Button {
                text: "Tune"
                onClicked: headerBarComponent.tune()

                background: Rectangle {
                    color: glassOverlay
                    border.color: glassBorder
                    radius: 8
                }
            }

            Button {
                text: "Halt"
                onClicked: headerBarComponent.halt()

                background: Rectangle {
                    color: Qt.rgba(244/255, 67/255, 54/255, 0.2)
                    border.color: colorRed
                    radius: 8
                }
            }
            // Raptor: Async Decode indicator
            Rectangle {
                width: 24; height: 24; radius: 12
                color: bridge.asyncDecodeEnabled ? Qt.alpha(secondaryCyan, 0.6) : Qt.rgba(0.2, 0.2, 0.2, 0.5)
                border.color: bridge.asyncDecodeEnabled ? secondaryCyan : glassBorder
                visible: bridge.mode === "FT2"
                Text { anchors.centerIn: parent; text: "A"; color: "white"; font.pixelSize: 10; font.bold: true }
                ToolTip { id: asyncTip; visible: asyncMA.containsMouse; text: "Async Decode " + (bridge.asyncDecodeEnabled ? "ON" : "OFF") }
                MouseArea { id: asyncMA; anchors.fill: parent; hoverEnabled: true; onClicked: bridge.asyncDecodeEnabled = !bridge.asyncDecodeEnabled }
            }

            // Raptor: DX Cluster Spot button
            Button {
                text: "Spot"
                visible: bridge.dxCluster && bridge.dxCluster.enabled && bridge.dxCluster.connected
                onClicked: {
                    if (bridge.dxCluster && bridge.dxCall.length > 0) {
                        bridge.dxCluster.submitSpotVerified(bridge.dxCall, bridge.frequency / 1000.0, bridge.mode + " Decodium")
                    }
                }
                background: Rectangle {
                    color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2)
                    border.color: accentGreen
                    radius: 8
                }
            }
        }

        // World Clock with Analog Display
        Item {
            id: worldClock
            Layout.preferredWidth: 180
            Layout.preferredHeight: 44

            // Timezone data
            property var timezones: [
                { name: "UTC", zoneId: "UTC" },
                { name: "London", zoneId: "Europe/London" },
                { name: "Rome", zoneId: "Europe/Rome" },
                { name: "Moscow", zoneId: "Europe/Moscow" },
                { name: "Dubai", zoneId: "Asia/Dubai" },
                { name: "Tokyo", zoneId: "Asia/Tokyo" },
                { name: "Sydney", zoneId: "Australia/Sydney" },
                { name: "New York", zoneId: "America/New_York" },
                { name: "Los Angeles", zoneId: "America/Los_Angeles" }
            ]
            property int selectedTz: 0
            property int hours: 0
            property int minutes: 0
            property int seconds: 0

            Timer {
                interval: 1000
                running: true
                repeat: true
                onTriggered: worldClock.updateTime()
            }

            Component.onCompleted: updateTime()

            function updateTime() {
                if (selectedTz < 0 || selectedTz >= timezones.length) {
                    selectedTz = 0
                }

                var snapshot = bridge.worldClockSnapshot(timezones[selectedTz].zoneId)
                hours = snapshot.hours
                minutes = snapshot.minutes
                seconds = snapshot.seconds
            }

            function nextTimezone() {
                selectedTz = (selectedTz + 1) % timezones.length
                updateTime()
            }

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                border.color: clockMA.containsMouse ? secondaryCyan : glassBorder
                border.width: clockMA.containsMouse ? 2 : 1
                radius: 8

                Row {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 6

                    // Analog Clock Face
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 18
                        color: bgMedium
                        border.color: secondaryCyan
                        border.width: 1

                        // Clock face markers
                        Repeater {
                            model: 12
                            Rectangle {
                                width: index % 3 === 0 ? 2 : 1
                                height: index % 3 === 0 ? 4 : 2
                                color: textSecondary
                                x: 18 + 14 * Math.sin(index * 30 * Math.PI / 180) - width/2
                                y: 18 - 14 * Math.cos(index * 30 * Math.PI / 180) - height/2
                            }
                        }

                        // Hour hand
                        Rectangle {
                            width: 2
                            height: 10
                            color: textPrimary
                            x: 17
                            y: 10
                            transformOrigin: Item.Bottom
                            rotation: (worldClock.hours % 12 + worldClock.minutes / 60) * 30
                        }

                        // Minute hand
                        Rectangle {
                            width: 1.5
                            height: 13
                            color: secondaryCyan
                            x: 17.25
                            y: 7
                            transformOrigin: Item.Bottom
                            rotation: worldClock.minutes * 6
                        }

                        // Second hand
                        Rectangle {
                            width: 1
                            height: 15
                            color: accentGreen
                            x: 17.5
                            y: 5
                            transformOrigin: Item.Bottom
                            rotation: worldClock.seconds * 6
                        }

                        // Center dot
                        Rectangle {
                            width: 4
                            height: 4
                            radius: 2
                            color: accentGreen
                            x: 16
                            y: 16
                        }
                    }

                    // Digital time and timezone selector
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            font.pixelSize: 14
                            font.family: "Monospace"
                            font.bold: true
                            color: textPrimary
                            text: ("0" + worldClock.hours).slice(-2) + ":" +
                                  ("0" + worldClock.minutes).slice(-2) + ":" +
                                  ("0" + worldClock.seconds).slice(-2)
                        }

                        Text {
                            text: worldClock.timezones[worldClock.selectedTz].name + " ▼"
                            font.pixelSize: 10
                            color: secondaryCyan
                        }
                    }
                }
            }

            // MouseArea on top of everything
            MouseArea {
                id: clockMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    console.log("Clock clicked!")
                    worldClock.nextTimezone()
                }
            }
        }
    }
}
