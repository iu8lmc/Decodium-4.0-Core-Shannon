/* Decodium Qt6 - Status Bar Component
 * Includes S-meter, CPU monitor, and status indicators
 * By IU8LMC
 */
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: statusBarComponent

    property string catStatus: "Disconnected"
    property string pttStatus: "Ready"
    property double audioLevel: 0.0  // raw RMS 0.0..1.0 da DecodiumAudioSink
    property double signalLevel: 0.0 // legacy S-meter in dB circa 0..90
    property double cpuUsage: 0.0    // 0.0 to 1.0

    // Scala logaritmica: -60 dBFS → 0.0, 0 dBFS → 1.0
    // Mappa valori RMS tipici (0.001..0.3) su tutto il range S-meter
    readonly property double scaledLevel: audioLevel > 0.0
        ? Math.max(0.0, Math.min(1.0, (20.0 * Math.log(audioLevel) / Math.LN10 + 60.0) / 60.0))
        : Math.max(0.0, Math.min(1.0, signalLevel / 90.0))
    property bool monitoring: false
    property bool transmitting: false
    property bool decoding: false

    onDecodingChanged: console.log("StatusBar: decoding =", decoding)

    readonly property var themeManager: bridge && bridge.themeManager ? bridge.themeManager : null
    property color accentGreen: themeManager ? themeManager.accentColor : "#4CAF50"
    property color secondaryCyan: themeManager ? themeManager.secondaryColor : "#00D4FF"
    property color textSecondary: themeManager ? themeManager.textSecondary : "#89B4D0"
    property color textPrimary: themeManager ? themeManager.textPrimary : "#E8F4FD"
    property color bgDeep: themeManager ? themeManager.bgDeep : "#111827"

    height: 36
    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)

    // Top border line
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 20

        // S-Meter Display
        RowLayout {
            spacing: 6

            Text {
                text: "S:"
                font.pixelSize: 11
                font.bold: true
                color: textSecondary
            }

            // S-Meter bar
            Rectangle {
                width: 80
                height: 16
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                radius: 3
                border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)

                // S-Meter segments
                Row {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 1

                    Repeater {
                        model: 9  // S1-S9

                        Rectangle {
                            width: (parent.width - 8) / 9
                            height: parent.height
                            radius: 1
                            color: {
                                var threshold = (index + 1) / 9.0
                                if (scaledLevel >= threshold) {
                                    if (index < 3) return "#4CAF50"  // Green S1-S3
                                    if (index < 6) return "#FFEB3B"  // Yellow S4-S6
                                    return "#f44336"  // Red S7-S9
                                }
                                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            }
                        }
                    }
                }
            }

            // dB value
            Text {
                text: {
                    if (audioLevel > 0) {
                        var db = 20.0 * Math.log(audioLevel) / Math.LN10
                        return db.toFixed(0) + "dB"
                    }
                    if (signalLevel > 0)
                        return signalLevel.toFixed(0) + "dB"
                    return "-∞"
                }
                font.family: "Consolas"
                font.pixelSize: 10
                color: scaledLevel > 0.9 ? "#f44336" : textSecondary
                Layout.preferredWidth: 35
            }
        }

        // Separator
        Rectangle { width: 1; height: 20; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // Status indicators
        RowLayout {
            spacing: 12

            // MON indicator
            Rectangle {
                width: 36
                height: 18
                radius: 9
                color: monitoring ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                border.color: monitoring ? accentGreen : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)

                Text {
                    anchors.centerIn: parent
                    text: "MON"
                    font.pixelSize: 9
                    font.bold: true
                    color: monitoring ? accentGreen : textSecondary
                }
            }

            // TX indicator
            Rectangle {
                width: 30
                height: 18
                radius: 9
                color: transmitting ? Qt.rgba(244/255, 67/255, 54/255, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                border.color: transmitting ? "#f44336" : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)

                Text {
                    anchors.centerIn: parent
                    text: "TX"
                    font.pixelSize: 9
                    font.bold: true
                    color: transmitting ? "#f44336" : textSecondary
                }
            }

            // DEC indicator - click to test
            Rectangle {
                id: decLed
                width: 34
                height: 18
                radius: 9

                property bool ledOn: statusBarComponent.decoding || testDecoding
                property bool testDecoding: false

                onLedOnChanged: console.log("DEC LED ledOn changed to:", ledOn)

                color: ledOn ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                border.color: ledOn ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                border.width: ledOn ? 2 : 1

                // Pulsing animation when decoding
                SequentialAnimation on opacity {
                    id: pulseAnimation
                    running: decLed.ledOn
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.6; duration: 300 }
                    NumberAnimation { to: 1.0; duration: 300 }
                    onRunningChanged: if (!running) decLed.opacity = 1.0
                }

                Text {
                    anchors.centerIn: parent
                    text: "DEC"
                    font.pixelSize: 9
                    font.bold: true
                    color: decLed.ledOn ? "#ffffff" : textSecondary
                }

                // Click to test LED
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        decLed.testDecoding = true
                        testTimer.start()
                        console.log("DEC LED TEST - decoding property:", decoding)
                    }
                }

                Timer {
                    id: testTimer
                    interval: 2000
                    onTriggered: decLed.testDecoding = false
                }
            }

            // FT Threads indicator - shows active decoder threads
            Rectangle {
                id: ftThreadsLed
                width: 40
                height: 18
                radius: 9

                property int threadCount: bridge ? bridge.ftThreads : 1
                property bool isActive: threadCount > 1

                color: isActive ? Qt.rgba(255/255, 152/255, 0/255, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                border.color: isActive ? "#ff9800" : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                border.width: isActive ? 2 : 1

                // Glow effect when multiple threads active
                SequentialAnimation on opacity {
                    id: ftPulseAnimation
                    running: ftThreadsLed.isActive && statusBarComponent.decoding
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.7; duration: 400 }
                    NumberAnimation { to: 1.0; duration: 400 }
                    onRunningChanged: if (!running) ftThreadsLed.opacity = 1.0
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        text: "FT"
                        font.pixelSize: 8
                        font.bold: true
                        color: ftThreadsLed.isActive ? "#ff9800" : textSecondary
                    }

                    Text {
                        text: ftThreadsLed.threadCount.toString()
                        font.pixelSize: 9
                        font.bold: true
                        font.family: "Consolas"
                        color: ftThreadsLed.isActive ? "#ffffff" : textSecondary
                    }
                }

                // Tooltip on hover
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    ToolTip {
                        visible: parent.containsMouse
                        delay: 500
                        text: "FT Decoder Threads: " + ftThreadsLed.threadCount
                    }
                }
            }
        }

        // Separator
        Rectangle { width: 1; height: 20; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // CAT Status
        RowLayout {
            spacing: 4

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: catStatus === "Connected" ? accentGreen :
                       catStatus === "Connecting" ? "#ff9800" : "#f44336"
            }

            Text {
                text: "CAT: " + catStatus
                font.pixelSize: 10
                color: catStatus === "Connected" ? accentGreen : textSecondary
            }
        }

        // Separator
        Rectangle { width: 1; height: 20; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // PSK Reporter Status
        RowLayout {
            spacing: 4
            property bool pskEnabled:   bridge ? bridge.pskReporterEnabled : false
            property bool pskConnected: bridge ? bridge.pskReporterConnected : false

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: parent.pskConnected ? accentGreen :
                       parent.pskEnabled ? "#ff9800" : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
            }

            Text {
                text: "PSK"
                font.pixelSize: 10
                color: parent.pskConnected ? accentGreen :
                       parent.pskEnabled ? "#ff9800" : textSecondary
            }
        }

        Item { Layout.fillWidth: true }

        // CPU Monitor
        RowLayout {
            spacing: 6

            Text {
                text: "CPU:"
                font.pixelSize: 10
                color: textSecondary
            }

            // CPU bar
            Rectangle {
                width: 50
                height: 12
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                radius: 2
                border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    width: Math.max(0, Math.min(parent.width - 4, (parent.width - 4) * cpuUsage))
                    radius: 1
                    color: cpuUsage < 0.5 ? "#4CAF50" :
                           cpuUsage < 0.8 ? "#ff9800" : "#f44336"
                }
            }

            Text {
                text: (cpuUsage * 100).toFixed(0) + "%"
                font.family: "Consolas"
                font.pixelSize: 10
                color: cpuUsage > 0.8 ? "#f44336" : textSecondary
                Layout.preferredWidth: 30
            }
        }

        // Separator
        Rectangle { width: 1; height: 20; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // Version info
        Text {
            text: "Decodium 4.0"
            font.pixelSize: 10
            color: textSecondary
        }
    }

    // CPU usage timer (simulated for now)
    Timer {
        interval: 2000
        running: true
        repeat: true
        onTriggered: {
            // Simulate CPU usage - in real implementation this would come from a C++ backend
            cpuUsage = 0.1 + Math.random() * 0.3
        }
    }
}
