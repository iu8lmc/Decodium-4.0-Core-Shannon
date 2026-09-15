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
    property double cpuUsage: bridge ? bridge.processCpuUsage : 0.0    // Decodium process, normalized 0.0..1.0
    property double processGpuUsage: bridge ? bridge.processGpuUsage : -1.0
    readonly property string processGpuUsageSource: bridge ? String(bridge.processGpuUsageSource) : "unavailable"
    readonly property bool realGpuUsageAvailable: processGpuUsage >= 0.0
    readonly property string activeRhiBackend: bridge ? String(bridge.activeRhiBackend) : "unknown"
    readonly property bool softwareRenderer: activeRhiBackend.toLowerCase() === "software"
    readonly property bool panadapterGpuFftActive: bridge ? bridge.panadapterGpuFftActive : false
    readonly property string panadapterGpuFftBackend: bridge ? String(bridge.panadapterGpuFftBackend) : "CPU FFTW"
    property int gpuFrameCount: 0
    property int gpuFps: 0
    property double gpuActivity: 0.0
    readonly property bool linuxGpuCounterZeroWhileRendering:
        Qt.platform.os === "linux"
        && realGpuUsageAvailable
        && !softwareRenderer
        && processGpuUsage <= 0.0005
        && gpuFps > 1
    readonly property bool staleLinuxGpuCounter:
        Qt.platform.os === "linux"
        && (processGpuUsageSource === "drm-process-stale"
            || linuxGpuCounterZeroWhileRendering)
    readonly property bool trustedRealGpuUsageAvailable:
        realGpuUsageAvailable && !staleLinuxGpuCounter
    readonly property double estimatedGpuActivity: Math.max(0.0, Math.min(1.0, gpuActivity))
    readonly property double displayedGpuActivity: trustedRealGpuUsageAvailable
        ? Math.max(0.0, Math.min(1.0, processGpuUsage))
        : (softwareRenderer ? 0.0 : estimatedGpuActivity)
    readonly property double gpuLoadPercentValue: displayedGpuActivity * 100.0
    readonly property int gpuLoadPercent: Math.round(gpuLoadPercentValue)
    readonly property string gpuLoadText: trustedRealGpuUsageAvailable
        ? (gpuLoadPercentValue < 10.0 ? gpuLoadPercentValue.toFixed(1) + "%" : gpuLoadPercent.toFixed(0) + "%")
        : (softwareRenderer ? "CPU"
           : (staleLinuxGpuCounter
              ? (gpuFps > 0 ? gpuFps.toFixed(0) + "fps"
                 : (panadapterGpuFftActive ? "FFT" : "idle"))
              : (panadapterGpuFftActive ? "FFT"
                 : (gpuFps > 0 ? gpuFps.toFixed(0) + "fps" : "idle"))))
    readonly property string gpuLabelText: trustedRealGpuUsageAvailable
        ? "GPU:"
        : (softwareRenderer ? "RENDER:"
           : (!staleLinuxGpuCounter && panadapterGpuFftActive ? "GPU:" : "GPU ACT:"))
    readonly property bool gpuMonitorVisible: Qt.platform.os !== "osx"
    property double rigPowerWatts: bridge ? bridge.rigPowerWatts : 0.0
    property double rigSwr: bridge ? bridge.rigSwr : 0.0
    property double rigAlc: bridge ? bridge.rigAlc : 0.0  // 1.0.323 — ALC meter 0..100
    property bool rigAlcValid: bridge ? bridge.rigAlcValid : false
    property bool pwrAndSwrEnabled: bridge ? bridge.getSetting("PWRandSWR", false) : false
    property bool checkSwrEnabled: bridge ? bridge.getSetting("CheckSWR", false) : false
    readonly property bool rigTelemetryBackendActive: bridge && (bridge.catBackend === "hamlib" || bridge.catBackend === "tci")
    readonly property bool rigTelemetryVisible: pwrAndSwrEnabled && rigTelemetryBackendActive && catStatus === "Connected"
    readonly property color swrStatusColor: rigSwr >= 2.0 ? colorRed
        : rigSwr >= 1.5 ? colorYellow
        : textSecondary
    // 1.0.323 — ALC: verde se 0 < ALC ≤ 60, rosso se > 60 (over-ALC)
    readonly property color alcStatusColor: rigAlc > 60 ? colorRed : accentGreen

    readonly property bool hasLegacySignalLevel: signalLevel > 0.0
    // Prefer the legacy-calibrated S-meter when available; raw RMS is only a fallback.
    readonly property double scaledLevel: hasLegacySignalLevel
        ? Math.max(0.0, Math.min(1.0, signalLevel / 90.0))
        : (audioLevel > 0.0
            ? Math.max(0.0, Math.min(1.0, (20.0 * Math.log(audioLevel) / Math.LN10 + 60.0) / 60.0))
            : 0.0)
    property bool monitoring: false
    property bool transmitting: false
    property bool pttPending: false
    property bool tuning: false
    property bool decoding: false
    readonly property bool txVisualActive: transmitting || tuning
    readonly property bool ft2LinkMode: bridge && String(bridge.mode || "").toUpperCase() === "FT2-LINK"

    readonly property var themeManager: bridge && bridge.themeManager ? bridge.themeManager : null
    property color accentGreen: themeManager ? themeManager.accentColor : "#00FF88"
    property color secondaryCyan: themeManager ? themeManager.secondaryColor : "#00D4FF"
    property color textSecondary: themeManager ? themeManager.textSecondary : "#89B4D0"
    property color textPrimary: themeManager ? themeManager.textPrimary : "#E8F4FD"
    property color bgDeep: themeManager ? themeManager.bgDeep : "#111827"
    property color colorRed:    themeManager ? themeManager.ledRed       : "#f44336"
    property color colorYellow: themeManager ? themeManager.ledYellow    : "#FFEB3B"
    property color colorGreen:  themeManager ? themeManager.successColor : "#4CAF50"
    property color colorOrange: themeManager ? themeManager.warningColor : "#ff9800"
    readonly property bool compactFooter: width > 0 && width < 1800
    readonly property bool narrowFooter: width > 0 && width < 1450
    readonly property bool tightFooter: width > 0 && width < 1250
    readonly property int footerMargin: narrowFooter ? 6 : (compactFooter ? 8 : 12)
    readonly property int footerSpacing: narrowFooter ? 6 : (compactFooter ? 10 : 20)
    readonly property int footerSeparatorHeight: compactFooter ? 16 : 20
    readonly property int footerMetricBarWidth: narrowFooter ? 34 : (compactFooter ? 42 : 50)
    readonly property int footerMetricValueWidth: narrowFooter ? 28 : 34
    readonly property int footerButtonHeight: compactFooter ? 26 : 30
    readonly property bool showFooterVersion: width >= 1920
    readonly property bool showFooterFtThreads: !ft2LinkMode
    readonly property bool showFooterSignalDb: width >= 1320
    readonly property bool showFooterDxText: width >= 1550
    readonly property bool showFooterGpuMonitor: gpuMonitorVisible && width >= 1280

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            if (key === "PWRandSWR")
                pwrAndSwrEnabled = value
            else if (key === "CheckSWR")
                checkSwrEnabled = value
        }
    }

    Connections {
        target: statusBarComponent.gpuMonitorVisible ? statusBarComponent.Window.window : null
        function onFrameSwapped() {
            statusBarComponent.gpuFrameCount += 1
        }
    }

    Timer {
        interval: 1000
        running: statusBarComponent.gpuMonitorVisible
        repeat: true
        onTriggered: {
            var frames = statusBarComponent.gpuFrameCount
            statusBarComponent.gpuFrameCount = 0
            var activeFrames = frames <= 1 ? 0 : frames
            statusBarComponent.gpuFps = activeFrames
            var normalized = Math.min(1.0, activeFrames / 60.0)
            statusBarComponent.gpuActivity = 0.65 * statusBarComponent.gpuActivity + 0.35 * normalized
            if (activeFrames === 0 && statusBarComponent.gpuActivity < 0.02)
                statusBarComponent.gpuActivity = 0.0
        }
    }

    height: 36
    // 1.0.339: un Rectangle root ha implicitHeight=0; Main.qml lega
    // Layout.preferredHeight/minimumHeight a implicitHeight -> senza questo il
    // footer collassa a 0 in classico (qualsiasi tema). Ripristino fix.
    implicitHeight: 36
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
        anchors.leftMargin: footerMargin
        anchors.rightMargin: footerMargin
        spacing: footerSpacing

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
                width: narrowFooter ? 58 : (compactFooter ? 68 : 80)
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
                                    if (index < 3) return colorGreen  // Green S1-S3
                                    if (index < 6) return colorYellow  // Yellow S4-S6
                                    return colorRed  // Red S7-S9
                                }
                                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            }
                        }
                    }
                }
            }

            // dB value
            Text {
                visible: showFooterSignalDb
                text: {
                    if (hasLegacySignalLevel)
                        return signalLevel.toFixed(0) + "dB"
                    if (audioLevel > 0) {
                        var db = 20.0 * Math.log(audioLevel) / Math.LN10
                        return db.toFixed(0) + "dB"
                    }
                    return "-∞"
                }
                font.family: decodiumMonoFontFamily
                font.pixelSize: 10
                color: scaledLevel > 0.9 ? colorRed : textSecondary
                Layout.preferredWidth: 35
            }
        }

        // Separator
        Rectangle { width: 1; height: footerSeparatorHeight; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // Status indicators
        RowLayout {
            spacing: tightFooter ? 7 : 12

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
                width: pttPending ? 34 : 30
                height: 18
                radius: 9
                color: txVisualActive ? Qt.rgba(244/255, 67/255, 54/255, 0.4)
                       : (pttPending ? Qt.rgba(colorOrange.r, colorOrange.g, colorOrange.b, 0.28)
                                     : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1))
                border.color: txVisualActive ? colorRed
                              : (pttPending ? colorOrange : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2))

                Text {
                    anchors.centerIn: parent
                    text: pttPending ? "PTT" : "TX"
                    font.pixelSize: 9
                    font.bold: true
                    color: txVisualActive ? colorRed : (pttPending ? colorOrange : textSecondary)
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

                color: ledOn ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                border.color: ledOn ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                border.width: ledOn ? 2 : 1

                // Pulsing animation when decoding
                SequentialAnimation on opacity {
                    id: pulseAnimation
                    running: decLed.ledOn && !statusBarComponent.ft2LinkMode && statusBarComponent.visible
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
                    color: decLed.ledOn ? (themeManager && themeManager.isLightTheme ? bgDeep : "#ffffff") : textSecondary
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
                visible: showFooterFtThreads
                width: tightFooter ? 44 : (autoMode ? 58 : 40)
                height: 18
                radius: 9

                property int threadCount: bridge ? bridge.ftThreads : 1
                property bool autoMode: bridge ? bridge.ftThreadsAuto : false
                readonly property string displayValue: autoMode ? (tightFooter ? "A" : "AUTO") : threadCount.toString()
                readonly property string tooltipValue: autoMode ? "AUTO" : threadCount.toString()
                property bool isActive: threadCount > 1

                color: isActive ? Qt.rgba(255/255, 152/255, 0/255, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                border.color: isActive ? colorOrange : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                border.width: isActive ? 2 : 1

                // Glow effect when multiple threads active
                SequentialAnimation on opacity {
                    id: ftPulseAnimation
                    running: ftThreadsLed.isActive && statusBarComponent.decoding
                             && !statusBarComponent.ft2LinkMode
                             && statusBarComponent.visible
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
                        font.pixelSize: tightFooter ? 7 : 8
                        font.bold: true
                        color: ftThreadsLed.isActive ? colorOrange : textSecondary
                    }

                    Text {
                        text: ftThreadsLed.displayValue
                        font.pixelSize: tightFooter ? 8 : (ftThreadsLed.autoMode ? 8 : 9)
                        font.bold: true
                        font.family: decodiumMonoFontFamily
                        color: ftThreadsLed.isActive ? (themeManager && themeManager.isLightTheme ? bgDeep : "#ffffff") : textSecondary
                    }
                }

                // Click cycles 1→8→1; tooltip on hover
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        if (!bridge) return
                        if (mouse.button === Qt.RightButton) {
                            bridge.ftThreadsAuto = true
                        } else {
                            bridge.cycleFtThreads()
                        }
                    }

                    ToolTip {
                        visible: parent.containsMouse
                        delay: 500
                        text: qsTr("FT Decoder Threads: ")
                              + ftThreadsLed.tooltipValue
                              + "\nClick: cycle 1-8 - Right-click: AUTO"
                    }
                }
            }
        }

        // Separator
        Rectangle { width: 1; height: footerSeparatorHeight; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // CAT Status
        RowLayout {
            spacing: 4

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: catStatus === "Connected" ? accentGreen :
                       catStatus === "Connecting" ? colorOrange : colorRed
            }

            Text {
                text: "CAT: " + catStatus
                font.pixelSize: 10
                color: catStatus === "Connected" ? accentGreen : textSecondary
            }
        }

        // ── DecoPort ────────────────────────────────────────────────────────
        // Compare solo quando c'e' qualcosa da dire: radio pubblicata in rete,
        // oppure radio altrui in uso. Con DecoPort spento non occupa spazio.
        Rectangle {
            width: 1
            height: footerSeparatorHeight
            visible: decoPortRow.visible
            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
        }

        RowLayout {
            id: decoPortRow
            spacing: 4

            readonly property var gw: bridge ? bridge.decoPortGateway : null
            readonly property var lnk: bridge ? bridge.decoPortLink : null
            readonly property bool publishing: !!(gw && gw.running)
            readonly property bool usingRemote: !!(lnk && lnk.linked)
            readonly property int clients: gw ? gw.clientCount : 0

            visible: publishing || usingRemote

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: accentGreen
            }

            Text {
                // L'indirizzo, non un conteggio: e' quello che serve scrivere
                // sull'altra macchina, o sapere per capire quale radio si sta
                // usando. Il resto sta nel tooltip.
                text: {
                    if (decoPortRow.usingRemote)
                        return "DECOPORT: " + decoPortRow.lnk.peerAddress
                    var addr = decoPortRow.gw ? decoPortRow.gw.primaryAddress : ""
                    if (addr.length === 0)
                        return "DECOPORT: " + qsTr("no address")
                    return "DECOPORT: " + addr + ":" + decoPortRow.gw.sessionPort
                }
                font.pixelSize: 10
                color: accentGreen
                elide: Text.ElideRight
                Layout.maximumWidth: 220

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (typeof mainWindow !== 'undefined' && mainWindow.openDecoPortWindow)
                        mainWindow.openDecoPortWindow()
                    ToolTip {
                        visible: parent.containsMouse
                        delay: 500
                        text: {
                            if (decoPortRow.usingRemote) {
                                return decoPortRow.lnk.rigLabel + "\n"
                                     + decoPortRow.lnk.status + "\n"
                                     + qsTr("Click to open DecoPort")
                            }
                            var all = decoPortRow.gw ? decoPortRow.gw.addresses : []
                            return qsTr("This radio is published on the network, %1 client connected")
                                       .arg(decoPortRow.clients)
                                 + (all.length > 1 ? "\n" + qsTr("also reachable at: ")
                                                     + all.slice(1).join(", ") : "")
                                 + "\n" + qsTr("Click to open DecoPort")
                        }
                    }
                }
            }
        }

            Rectangle {
                width: 1
                height: footerSeparatorHeight
                visible: rigTelemetryVisible
                color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
        }

        RowLayout {
            spacing: 8
            visible: rigTelemetryVisible

            Text {
                text: "PWR:"
                font.pixelSize: 10
                color: textSecondary
            }

            Text {
                text: rigPowerWatts > 0.05 ? Math.round(rigPowerWatts).toString() + "W" : "--"
                font.family: decodiumMonoFontFamily
                font.pixelSize: 10
                color: rigPowerWatts > 0.05 ? accentGreen : textSecondary
                Layout.preferredWidth: 34
            }

            Rectangle {
                height: 18
                width: 78
                radius: 9
                color: rigSwr >= 2.0 ? Qt.rgba(244/255, 67/255, 54/255, 0.35)
                    : rigSwr >= 1.5 ? Qt.rgba(255/255, 235/255, 59/255, 0.28)
                    : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                border.color: rigSwr > 0 ? swrStatusColor : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)

                Text {
                    anchors.centerIn: parent
                    text: "SWR: " + (rigSwr > 0 ? rigSwr.toFixed(rigSwr < 10 ? 2 : 1) : "--")
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 10
                    font.bold: rigSwr >= 1.5
                    color: rigSwr >= 2.0 ? (themeManager && themeManager.isLightTheme ? bgDeep : "#ffffff") : swrStatusColor
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    ToolTip {
                        visible: parent.containsMouse
                        delay: 500
                        text: checkSwrEnabled
                            ? "SWR check active: TX/AutoCQ is blocked or interrupted when SWR > 2.5; Tune remains available for measurement"
                            : "Power/SWR telemetry from CAT"
                    }
                }
            }
        }

        // 1.0.323 — ALC display (visibile solo quando PWR/SWR telemetry è attivo e in TX)
        RowLayout {
            spacing: 4
            visible: rigTelemetryVisible

            Rectangle {
                height: 18
                width: 56
                radius: 9
                color: rigAlcValid && rigAlc > 60 ? Qt.rgba(244/255, 67/255, 54/255, 0.35)
                    : rigAlcValid ? Qt.rgba(76/255, 175/255, 80/255, 0.18)
                    : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                border.color: rigAlcValid ? alcStatusColor : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)

                Text {
                    anchors.centerIn: parent
                    text: rigAlcValid ? "ALC " + Math.round(rigAlc) : "ALC --"
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 10
                    font.bold: rigAlcValid && rigAlc > 60
                    color: rigAlcValid && rigAlc > 60 ? (themeManager && themeManager.isLightTheme ? bgDeep : "#ffffff") : alcStatusColor

                    ToolTip.visible: alcMouseArea.containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: rigAlcValid
                        ? qsTr("ALC meter 0..100\n>60 = excessive ALC (TX power too high)")
                        : qsTr("ALC is not reported by Hamlib for this rig/backend")
                }

                MouseArea {
                    id: alcMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }

        // Separator
        Rectangle { width: 1; height: footerSeparatorHeight; color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) }

        // PSK Reporter Status
        RowLayout {
            id: pskStatusRow
            spacing: 4
            property bool pskEnabled:   bridge ? bridge.pskReporterEnabled : false
            property bool pskConnected: bridge ? bridge.pskReporterConnected : false

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: pskStatusRow.pskConnected ? accentGreen :
                       pskStatusRow.pskEnabled ? colorOrange : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
            }

            Text {
                text: "PSK"
                font.pixelSize: 10
                color: pskStatusRow.pskConnected ? accentGreen :
                       pskStatusRow.pskEnabled ? colorOrange : textSecondary

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    ToolTip {
                        visible: parent.containsMouse
                        delay: 500
                        text: !pskStatusRow.pskEnabled
                              ? "PSK Reporter: disabled"
                              : (pskStatusRow.pskConnected
                                 ? "PSK Reporter: connected to report.pskreporter.info"
                                 : "PSK Reporter: recent HTTP errors")
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: compactFooter ? 0 : 1
            Layout.maximumWidth: compactFooter ? 8 : 16777215
        }

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
                width: footerMetricBarWidth
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
                    color: cpuUsage < 0.5 ? colorGreen :
                           cpuUsage < 0.8 ? colorOrange : colorRed
                }
            }

            Text {
                text: (cpuUsage * 100).toFixed(0) + "%"
                font.family: decodiumMonoFontFamily
                font.pixelSize: 10
                color: cpuUsage > 0.8 ? colorRed : textSecondary
                Layout.preferredWidth: footerMetricValueWidth
            }
        }

        // GPU/render activity monitor
        Item {
            id: gpuMonitor
            visible: showFooterGpuMonitor
            implicitWidth: visible ? gpuMonitorLayout.implicitWidth : 0
            implicitHeight: visible ? gpuMonitorLayout.implicitHeight : 0

            RowLayout {
                id: gpuMonitorLayout
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: gpuLabelText
                    font.pixelSize: 10
                    color: textSecondary
                }

                Rectangle {
                    width: footerMetricBarWidth
                    height: 12
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                    radius: 2
                    border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        width: Math.max(0, Math.min(parent.width - 4, (parent.width - 4) * displayedGpuActivity))
                        radius: 1
                        color: !trustedRealGpuUsageAvailable ? secondaryCyan
                               : displayedGpuActivity < 0.5 ? secondaryCyan
                               : displayedGpuActivity < 0.8 ? colorOrange : colorRed
                    }
                }

                Text {
                    text: gpuLoadText
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 10
                    color: trustedRealGpuUsageAvailable && displayedGpuActivity > 0.8 ? colorRed : textSecondary
                    Layout.preferredWidth: narrowFooter ? 36 : 42
                }
            }

            MouseArea {
                id: gpuMonitorMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton

                ToolTip {
                    visible: gpuMonitorMouse.containsMouse
                    delay: 500
                    text: trustedRealGpuUsageAvailable
                          ? (Qt.platform.os === "linux"
                             ? (processGpuUsageSource === "drm-device"
                                ? "Overall activity of the GPU used by Decodium\n"
                                : "Approximate GPU-engine usage for the Decodium process\n")
                             : "Real GPU usage for the Decodium process\n")
                            + gpuLoadText + " from system GPU counters\n"
                            + "Panadapter FFT: " + panadapterGpuFftBackend + "\n"
                            + gpuFps + " rendered frames/s"
                          : (softwareRenderer
                             ? "Qt Quick renderer: software (CPU)\n"
                               + "Panadapter FFT: " + panadapterGpuFftBackend + "\n"
                               + "No GPU utilisation is being reported"
                             : staleLinuxGpuCounter
                               ? "The Linux DRM counter is present but is not advancing reliably.\n"
                                 + "Showing Qt render activity instead of a false 0% value.\n"
                                 + "Panadapter FFT: " + panadapterGpuFftBackend + "\n"
                                 + gpuFps + " rendered frames/s (activity, not GPU load)"
                             : "Qt Quick renderer: " + activeRhiBackend + "\n"
                               + "Panadapter FFT: " + panadapterGpuFftBackend + "\n"
                               + "Per-process GPU utilisation counter unavailable\n"
                               + gpuFps + " rendered frames/s (activity, not GPU load)")
                }
            }
        }

        // Separator
        Rectangle {
            width: 1
            height: footerSeparatorHeight
            visible: showFooterVersion
            color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
        }

        // Version info
        Text {
            visible: showFooterVersion
            text: qsTr("Decodium 4.0")
            font.pixelSize: 10
            color: textSecondary
        }
    }

}
