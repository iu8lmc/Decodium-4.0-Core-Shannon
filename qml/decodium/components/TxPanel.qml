/* Decodium Qt6 - TX Panel Component
 * TX panel delegates to bridge (which uses HvTxW for auto-sequencing)
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: txPanel

    // Required property - reference to the app engine
    required property var engine

    // Convenience aliases
    property var log: engine ? engine.logManager : null

    // Signal for MAM window request
    signal mamWindowRequested()

    // Style properties
    property color glassBg: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.85)
    property color glassBorder: bridge.themeManager.glassBorder
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color warningOrange: bridge.themeManager.warningColor
    property color errorRed: bridge.themeManager.errorColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color bgDeep: bridge.themeManager.bgDeep

    // State colors based on QSO progress (bridge::QSOProgress enum)
    // 0=IDLE, 1=CALLING_CQ, 2=REPLYING, 3=REPORT, 4=ROGER_REPORT, 5=SIGNOFF, 6=IDLE_QSO
    property color stateColor: {
        if (!engine) return textSecondary
        switch (engine.qsoProgress) {
            case 0: return textSecondary      // IDLE
            case 1: return accentGreen        // CALLING_CQ
            case 2: return primaryBlue        // REPLYING
            case 3: return secondaryCyan      // REPORT
            case 4: return warningOrange      // ROGER_REPORT
            case 5: return warningOrange      // SIGNOFF
            case 6: return accentGreen        // IDLE_QSO (QSO completed)
            default: return textSecondary
        }
    }

    property string stateText: {
        if (!engine) return "Not Ready"
        switch (engine.qsoProgress) {
            case 0: return "Idle"
            case 1: return "Calling CQ"
            case 2: return "Replying"
            case 3: return "Sending Report"
            case 4: return "Roger Report"
            case 5: return "Sending 73"
            case 6: return "QSO Done"
            default: return "Unknown"
        }
    }

    Rectangle {
        anchors.fill: parent
        color: glassBg
        border.color: engine && engine.transmitting ? errorRed : glassBorder
        border.width: engine && engine.transmitting ? 2 : 1
        radius: 12

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 3


            // ===== Band + Mode toggles row =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                // Band selector (compact)
                Rectangle {
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: 440
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: glassBorder
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        spacing: 1

                        Repeater {
                            model: ["160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6"]

                            Rectangle {
                                id: bandRect
                                width: 38
                                height: 26
                                radius: 3

                                readonly property bool isSelected: engine && engine.bandManager &&
                                                                   engine.bandManager.currentBandLambda === (modelData + "M")

                                color: isSelected ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: isSelected ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                                border.width: isSelected ? 2 : 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 10
                                    font.bold: bandRect.isSelected
                                    color: bandRect.isSelected ? "#ffffff" : textPrimary
                                }

                                MouseArea {
                                    id: bandMa
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (engine && engine.bandManager) {
                                            engine.bandManager.changeBandByLambda(modelData + "M")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Mode selector
                Rectangle {
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 36
                    color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
                    border.color: secondaryCyan
                    border.width: 1
                    radius: 6

                    ComboBox {
                        id: modeSelector
                        anchors.fill: parent
                        model: ["FT8", "FT4", "FT2", "Q65A", "Q65B", "Q65C", "Q65D", "Q65E", "JT65A", "JT65B", "JT65C", "MSK144"]
                        currentIndex: {
                            var mode = engine ? engine.mode : "FT8"
                            var idx = model.indexOf(mode)
                            return idx >= 0 ? idx : 0
                        }
                        onCurrentTextChanged: if (engine && currentText) engine.mode = currentText

                        background: Rectangle { color: "transparent" }
                        contentItem: Text {
                            text: modeSelector.displayText
                            color: secondaryCyan
                            font.pixelSize: 13
                            font.bold: true
                            font.family: "Consolas"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                        indicator: Item {}

                        popup: Popup {
                            y: modeSelector.height
                            width: modeSelector.width
                            implicitHeight: contentItem.implicitHeight + 2
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: modeSelector.popup.visible ? modeSelector.delegateModel : null
                                currentIndex: modeSelector.highlightedIndex
                            }

                            background: Rectangle {
                                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                                border.color: secondaryCyan
                                radius: 4
                            }
                        }
                    }
                }

                // AutoSQ toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: autoSqBtn.checked ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: autoSqBtn.checked ? primaryBlue : glassBorder
                    border.width: autoSqBtn.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: autoSqBtn
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.autoSeq : false
                        onCheckedChanged: if (engine) engine.autoSeq = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u26A1"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: autoSqBtn.checked ? primaryBlue : textSecondary
                            }
                            Text {
                                text: "ASQ"
                                color: autoSqBtn.checked ? primaryBlue : textSecondary
                                font.pixelSize: 10
                                font.bold: autoSqBtn.checked
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Auto Sequence"
                        ToolTip.delay: 500
                    }
                }

                // MAM toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: mamBtn.checked ? Qt.rgba(255/255, 152/255, 0, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: mamBtn.checked ? warningOrange : glassBorder
                    border.width: mamBtn.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: mamBtn
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.multiAnswerMode : false
                        onCheckedChanged: if (engine) engine.multiAnswerMode = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u21C6"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: mamBtn.checked ? warningOrange : textSecondary
                            }
                            Text {
                                text: "MAM"
                                color: mamBtn.checked ? warningOrange : textSecondary
                                font.pixelSize: 10
                                font.bold: mamBtn.checked
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Multi-Answer Mode (right-click=window)"
                        ToolTip.delay: 500
                    }

                    // Right-click to open MAM window
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: txPanel.mamWindowRequested()
                    }
                }

                // Deep toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: deepBtn.checked ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: deepBtn.checked ? accentGreen : glassBorder
                    border.width: deepBtn.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: deepBtn
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.deepSearchEnabled : false
                        onCheckedChanged: if (engine) engine.deepSearchEnabled = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u25CE"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: deepBtn.checked ? accentGreen : textSecondary
                            }
                            Text {
                                text: "DEEP"
                                color: deepBtn.checked ? accentGreen : textSecondary
                                font.pixelSize: 10
                                font.bold: deepBtn.checked
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Deep Search"
                        ToolTip.delay: 500
                    }
                }

                // AP toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: apBtn.checked ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: apBtn.checked ? secondaryCyan : glassBorder
                    border.width: apBtn.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: apBtn
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.avgDecodeEnabled : false
                        onCheckedChanged: if (engine) engine.avgDecodeEnabled = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u25C6"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: apBtn.checked ? secondaryCyan : textSecondary
                            }
                            Text {
                                text: "AP"
                                color: apBtn.checked ? secondaryCyan : textSecondary
                                font.pixelSize: 10
                                font.bold: apBtn.checked
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "A-Priori Decoding"
                        ToolTip.delay: 500
                    }
                }

                // SWL toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: swlBtn.checked ? Qt.rgba(156/255, 39/255, 176/255, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: swlBtn.checked ? "#9c27b0" : glassBorder
                    border.width: swlBtn.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: swlBtn
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.swlMode : false
                        onCheckedChanged: if (engine) engine.swlMode = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u2609"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: swlBtn.checked ? "#9c27b0" : textSecondary
                            }
                            Text {
                                text: "SWL"
                                color: swlBtn.checked ? "#9c27b0" : textSecondary
                                font.pixelSize: 10
                                font.bold: swlBtn.checked
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "SWL Mode (Listen Only)"
                        ToolTip.delay: 500
                    }
                }

                // AutoSeq toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: autoSeqBtn2.checked ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: autoSeqBtn2.checked ? primaryBlue : glassBorder
                    border.width: autoSeqBtn2.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: autoSeqBtn2
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.autoSeq : false
                        onCheckedChanged: if (engine) engine.autoSeq = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u21BB"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: autoSeqBtn2.checked ? primaryBlue : textSecondary
                            }
                            Text {
                                text: "SEQ"
                                color: autoSeqBtn2.checked ? primaryBlue : textSecondary
                                font.pixelSize: 10
                                font.bold: autoSeqBtn2.checked
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Auto Sequence"
                        ToolTip.delay: 500
                    }
                }

                // TX Enable toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: txEnableBtn.checked ? Qt.rgba(244/255, 67/255, 54/255, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                    border.color: txEnableBtn.checked ? errorRed : glassBorder
                    border.width: txEnableBtn.checked ? 2 : 1
                    radius: 6

                    Button {
                        id: txEnableBtn
                        anchors.fill: parent
                        checkable: true
                        checked: engine ? engine.txEnabled : false
                        onCheckedChanged: if (engine) engine.txEnabled = checked
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u25B2"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: txEnableBtn.checked ? errorRed : textSecondary
                            }
                            Text {
                                text: "TX"
                                color: txEnableBtn.checked ? errorRed : textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Enable TX"
                        ToolTip.delay: 500
                    }
                }

                // Auto CQ Repeat toggle
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: engine && engine.autoCqRepeat ? Qt.alpha(successGreen, 0.3) : Qt.alpha(textPrimary, 0.05)
                    border.color: engine && engine.autoCqRepeat ? successGreen : Qt.alpha(textPrimary, 0.2)
                    border.width: engine && engine.autoCqRepeat ? 2 : 1
                    radius: 6

                    Button {
                        id: autoCqButton
                        anchors.fill: parent
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "⟳"  // Unicode circular arrow
                                font.pixelSize: 16
                                color: engine && engine.autoCqRepeat ? successGreen : textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "ACQ"
                                font.pixelSize: 11
                                color: engine && engine.autoCqRepeat ? successGreen : textPrimary
                                font.bold: engine && engine.autoCqRepeat
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Auto CQ Repeat\n(Chiama CQ automaticamente fino a risposta)"
                        ToolTip.delay: 500

                        onClicked: {
                            if (engine) {
                                // Auto CQ requires Auto Seq to be enabled
                                if (!engine.autoSeq && !engine.autoCqRepeat) {
                                    console.log("Auto CQ richiede Auto Seq attivo")
                                    // Auto-enable Auto Seq when enabling Auto CQ
                                    engine.autoSeq = true
                                }
                                engine.autoCqRepeat = !engine.autoCqRepeat
                            }
                        }
                    }
                }

                // Quick Call toggle (come Shannon "Double-click on call sets Tx enable")
                // Abilita TX automaticamente al doppio click senza premere "Enable TX"
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: engine && engine.startFromTx2 ? Qt.alpha(warningOrange, 0.3) : Qt.alpha(textPrimary, 0.05)
                    border.color: engine && engine.startFromTx2 ? warningOrange : Qt.alpha(textPrimary, 0.2)
                    border.width: engine && engine.startFromTx2 ? 2 : 1
                    radius: 6

                    Button {
                        anchors.fill: parent
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "⚡"
                                font.pixelSize: 14
                                color: engine && engine.startFromTx2 ? warningOrange : textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "Q.Call"
                                font.pixelSize: 10
                                color: engine && engine.startFromTx2 ? warningOrange : textPrimary
                                font.bold: engine && engine.startFromTx2
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Quick Call\n(Doppio click su decode abilita TX automaticamente\ncome Shannon: 'Double-click on call sets Tx enable')"
                        ToolTip.delay: 500
                        onClicked: { if (engine) engine.startFromTx2 = !engine.startFromTx2 }
                    }
                }

                // TUNE button
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: tuneButton.isTuning ? Qt.alpha(warningOrange, 0.5) : Qt.alpha(warningOrange, 0.2)
                    border.color: warningOrange
                    border.width: tuneButton.isTuning ? 2 : 1
                    radius: 6

                    Button {
                        id: tuneButton
                        anchors.fill: parent
                        property bool isTuning: engine && engine.tuning
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u266B"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: warningOrange
                            }
                            Text {
                                text: tuneButton.isTuning ? "Stop" : "TUNE"
                                color: warningOrange
                                font.pixelSize: 10
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        onClicked: {
                            if (engine) {
                                if (engine.tuning) engine.stopTune()
                                else engine.startTune()
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Tune"
                        ToolTip.delay: 500
                    }
                }

                // HALT button
                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 36
                    color: Qt.alpha(errorRed, 0.3)
                    border.color: errorRed
                    border.width: engine && engine.transmitting ? 2 : 1
                    radius: 6

                    Button {
                        anchors.fill: parent
                        background: Rectangle { color: "transparent" }
                        contentItem: Row {
                            spacing: 3
                            anchors.centerIn: parent
                            Text {
                                text: "\u25A0"
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: errorRed
                            }
                            Text {
                                text: "HALT"
                                color: errorRed
                                font.pixelSize: 10
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        onClicked: if (engine) engine.halt()
                        ToolTip.visible: hovered
                        ToolTip.text: "Halt TX"
                        ToolTip.delay: 500
                    }
                }

                // Raptor: Async TX (FT2 only)
                Rectangle {
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    color: engine && engine.asyncTxEnabled ? Qt.alpha(secondaryCyan, 0.3) : Qt.rgba(0.15, 0.15, 0.15, 0.6)
                    border.color: engine && engine.asyncTxEnabled ? secondaryCyan : glassBorder
                    border.width: 1
                    radius: 6
                    visible: engine && engine.mode === "FT2"
                    Button {
                        anchors.fill: parent
                        background: Rectangle { color: "transparent" }
                        contentItem: Text { text: "ATX"; color: engine && engine.asyncTxEnabled ? secondaryCyan : textSecondary; font.pixelSize: 9; font.bold: true; anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter }
                        onClicked: if (engine) engine.asyncTxEnabled = !engine.asyncTxEnabled
                        ToolTip.visible: hovered; ToolTip.text: "Async TX (FT2)"; ToolTip.delay: 500
                    }
                }

                // AsyncModeWidget — onda sinusoidale animata (sempre visibile in FT2)
                AsyncModeWidget {
                    id: asyncModeVis
                    Layout.preferredWidth:  90
                    Layout.preferredHeight: 50
                    Layout.alignment:       Qt.AlignVCenter
                    visible:      engine && engine.mode === "FT2"
                    running:      engine ? (engine.asyncTxEnabled || engine.asyncDecodeEnabled) : false
                    transmitting: engine ? engine.transmitting : false
                    snr:          engine ? engine.asyncSnrDb   : -99
                    ToolTip.visible: hovered
                    ToolTip.text:    "FT2 Async Mode — onda sinusoidale: verde=RX, rosso=TX"
                    ToolTip.delay:   400
                }

                // Raptor: Dual Carrier (FT2 only)
                Rectangle {
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    color: engine && engine.dualCarrierEnabled ? Qt.alpha(warningOrange, 0.3) : Qt.rgba(0.15, 0.15, 0.15, 0.6)
                    border.color: engine && engine.dualCarrierEnabled ? warningOrange : glassBorder
                    border.width: 1
                    radius: 6
                    visible: engine && engine.mode === "FT2"
                    Button {
                        anchors.fill: parent
                        background: Rectangle { color: "transparent" }
                        contentItem: Text { text: "2xC"; color: engine && engine.dualCarrierEnabled ? warningOrange : textSecondary; font.pixelSize: 9; font.bold: true; anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter }
                        onClicked: if (engine) engine.dualCarrierEnabled = !engine.dualCarrierEnabled
                        ToolTip.visible: hovered; ToolTip.text: "Dual Carrier (-3dB/carrier)"; ToolTip.delay: 500
                    }
                }

                // Raptor: Manual TX PTT
                Rectangle {
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    color: engine && engine.manualTxMode ? Qt.alpha(errorRed, 0.4) : Qt.rgba(0.15, 0.15, 0.15, 0.6)
                    border.color: engine && engine.manualTxMode ? errorRed : glassBorder
                    border.width: 1
                    radius: 6
                    visible: engine && engine.mode === "FT2"
                    Button {
                        anchors.fill: parent
                        background: Rectangle { color: "transparent" }
                        contentItem: Text { text: "PTT"; color: engine && engine.manualTxMode ? errorRed : textSecondary; font.pixelSize: 10; font.bold: true; anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter }
                        onClicked: {
                            if (engine) {
                                if (!engine.manualTxMode) engine.manualTxMode = true
                                else engine.triggerManualTx()
                            }
                        }
                        onPressAndHold: if (engine) engine.manualTxMode = false
                        ToolTip.visible: hovered; ToolTip.text: "Manual TX: Click=transmit, Long press=disable"; ToolTip.delay: 500
                    }
                }

                Item { Layout.fillWidth: true }

                // RX Frequency
                Rectangle {
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 36
                    color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.15)
                    border.color: accentGreen
                    border.width: 1
                    radius: 6

                    Row {
                        anchors.centerIn: parent
                        spacing: 3
                        Text {
                            text: "\u25BC"
                            color: accentGreen
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: engine ? engine.rxFrequency : "0"
                            color: accentGreen
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Consolas"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    ToolTip.visible: rxFreqMouse.containsMouse
                    ToolTip.text: "RX Frequency"
                    ToolTip.delay: 500

                    MouseArea {
                        id: rxFreqMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                // TX Frequency
                Rectangle {
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 36
                    color: Qt.rgba(244/255, 67/255, 54/255, 0.15)
                    border.color: errorRed
                    border.width: 1
                    radius: 6

                    Row {
                        anchors.centerIn: parent
                        spacing: 3
                        Text {
                            text: "\u25B2"
                            color: errorRed
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: engine ? engine.txFrequency : "0"
                            color: errorRed
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Consolas"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    ToolTip.visible: txFreqMouse.containsMouse
                    ToolTip.text: "TX Frequency"
                    ToolTip.delay: 500

                    MouseArea {
                        id: txFreqMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

            }

            // DX Station info row
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                // QSO State indicator
                Rectangle {
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 32
                    color: Qt.alpha(stateColor, 0.2)
                    border.color: stateColor
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: stateText
                        color: stateColor
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Text {
                    text: "DX:"
                    color: secondaryCyan
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: dxCallField
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 32
                    text: engine ? engine.dxCall : ""
                    placeholderText: "Call"
                    font.pixelSize: 12
                    font.family: "Consolas"
                    color: textPrimary

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: dxCallField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.dxCall) {
                            engine.dxCall = text.toUpperCase()
                        }
                    }
                }

                Text {
                    text: "Grid:"
                    color: secondaryCyan
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: dxGridField
                    Layout.preferredWidth: 65
                    Layout.preferredHeight: 32
                    text: engine ? engine.dxGrid : ""
                    placeholderText: "Grid"
                    font.pixelSize: 12
                    font.family: "Consolas"
                    color: textPrimary

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: dxGridField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.dxGrid) {
                            engine.dxGrid = text.toUpperCase()
                        }
                    }
                }

                Text {
                    text: "S:"
                    color: secondaryCyan
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: rptSentField
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 32
                    text: engine ? engine.reportSent : "-10"
                    font.pixelSize: 12
                    font.family: "Consolas"
                    color: textPrimary

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: rptSentField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.reportSent) {
                            engine.reportSent = text
                        }
                    }
                }

                Text {
                    text: "R:"
                    color: secondaryCyan
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: rptRcvdField
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 32
                    text: engine ? engine.reportReceived : ""
                    placeholderText: "--"
                    font.pixelSize: 11
                    font.family: "Consolas"
                    color: accentGreen

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: rptRcvdField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.reportReceived) {
                            engine.reportReceived = text
                        }
                    }
                }

                // Next/TX Message display
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: engine && engine.transmitting ?
                           Qt.alpha(errorRed, 0.2) :
                           Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                    border.color: engine && engine.transmitting ? errorRed : glassBorder
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        Text {
                            text: engine && engine.transmitting ? "TX:" : "Next:"
                            color: engine && engine.transmitting ? errorRed : textSecondary
                            font.pixelSize: 10
                            font.bold: engine && engine.transmitting
                        }

                        Text {
                            Layout.fillWidth: true
                            text: engine ? engine.currentTxMessage : ""
                            color: engine && engine.transmitting ? errorRed : textPrimary
                            font.family: "Consolas"
                            font.pixelSize: 11
                            font.bold: engine && engine.transmitting
                            elide: Text.ElideRight
                        }

                        Text {
                            text: log ? "QSOs:" + log.qsoCount : ""
                            color: textSecondary
                            font.pixelSize: 9
                        }
                    }
                }

                // Log QSO button
                Button {
                    id: logQsoBtn
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 32
                    enabled: engine && engine.dxCall.length > 0

                    background: Rectangle {
                        color: logQsoBtn.enabled ?
                               Qt.alpha(accentGreen, 0.3) :
                               Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                        border.color: logQsoBtn.enabled ? accentGreen : glassBorder
                        radius: 4
                    }

                    contentItem: Row {
                        spacing: 4
                        anchors.centerIn: parent
                        Text {
                            text: "\u270E"
                            color: logQsoBtn.enabled ? accentGreen : textSecondary
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "LOG"
                            color: logQsoBtn.enabled ? accentGreen : textSecondary
                            font.pixelSize: 11
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    onClicked: if (engine) engine.logQso()
                }
            }

            // TX Buttons row
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                // TX1 - Answer CQ (Grid)
                TxButton {
                    txNum: 1
                    label: "TX1"
                    message: engine && engine.txMessages.length > 0 ? engine.txMessages[0] : "-- --"
                    isSelected: engine && engine.currentTx === 1
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 1
                    onClicked: if (engine) engine.sendTx(1)
                }

                // TX2 - Send Report
                TxButton {
                    txNum: 2
                    label: "TX2"
                    message: engine && engine.txMessages.length > 1 ? engine.txMessages[1] : "-- --"
                    isSelected: engine && engine.currentTx === 2
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 2
                    onClicked: if (engine) engine.sendTx(2)
                }

                // TX3 - R+Report
                TxButton {
                    txNum: 3
                    label: "TX3"
                    message: engine && engine.txMessages.length > 2 ? engine.txMessages[2] : "-- --"
                    isSelected: engine && engine.currentTx === 3
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 3
                    onClicked: if (engine) engine.sendTx(3)
                }

                // TX4 - RR73
                TxButton {
                    txNum: 4
                    label: "TX4"
                    message: engine && engine.txMessages.length > 3 ? engine.txMessages[3] : "-- --"
                    isSelected: engine && engine.currentTx === 4
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 4
                    onClicked: if (engine) engine.sendTx(4)
                }

                // TX5 - 73
                TxButton {
                    txNum: 5
                    label: "TX5"
                    message: engine && engine.txMessages.length > 4 ? engine.txMessages[4] : "-- --"
                    isSelected: engine && engine.currentTx === 5
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 5
                    onClicked: if (engine) engine.sendTx(5)
                }

            }

        }
    }

    // TX Button component
    component TxButton: Button {
        property int txNum: 1
        property string label: "TX1"
        property string message: ""
        property bool isSelected: false
        property bool isTransmitting: false
        property bool isCQ: false

        Layout.fillWidth: true
        Layout.preferredHeight: 50

        background: Rectangle {
            color: {
                if (isTransmitting) return Qt.alpha(errorRed, 0.4)
                if (isSelected) return Qt.alpha(primaryBlue, 0.3)
                if (isCQ) return Qt.alpha(accentGreen, 0.2)
                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
            }
            border.color: {
                if (isTransmitting) return errorRed
                if (isSelected) return primaryBlue
                if (isCQ) return accentGreen
                return glassBorder
            }
            border.width: isSelected || isTransmitting ? 2 : 1
            radius: 6

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        contentItem: ColumnLayout {
            spacing: 2

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: label
                color: {
                    if (isTransmitting) return errorRed
                    if (isSelected) return primaryBlue
                    if (isCQ) return accentGreen
                    return textSecondary
                }
                font.pixelSize: 10
                font.bold: isSelected || isTransmitting
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: message
                color: isTransmitting ? errorRed : textPrimary
                font.family: "Consolas"
                font.pixelSize: 9
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
