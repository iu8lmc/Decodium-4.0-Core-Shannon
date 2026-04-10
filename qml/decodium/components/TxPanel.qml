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
    property color successGreen: bridge.themeManager.accentColor
    property color warningOrange: bridge.themeManager.warningColor
    property color errorRed: bridge.themeManager.errorColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color bgDeep: bridge.themeManager.bgDeep
    readonly property var supportedModes: engine ? engine.availableModes() : ["FT8", "FT2", "FT4", "Q65", "MSK144", "JT65", "JT9", "JT4", "FST4", "FST4W", "WSPR"]

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

    component ToolbarButtonContent: Item {
        required property string label
        property string glyph: ""
        property color foreground: "white"
        property int glyphSize: 14
        property int labelSize: 10
        property bool boldLabel: false

        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: glyph.length > 0 ? 4 : 0

            Text {
                visible: glyph.length > 0
                text: glyph
                color: foreground
                font.pixelSize: glyphSize
                font.family: "Consolas"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: label
                color: foreground
                font.pixelSize: labelSize
                font.family: "Consolas"
                font.bold: boldLabel
                anchors.verticalCenter: parent.verticalCenter
            }
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
            ColumnLayout {
                id: topControlsRow
                Layout.fillWidth: true
                spacing: 4

                Item {
                    Layout.fillWidth: true
                    Layout.rightMargin: 36
                    implicitHeight: topControlsFlow.height

                    Flow {
                        id: topControlsFlow
                        width: parent.width
                        spacing: 4

                        Rectangle {
                            width: Math.min(440, topControlsFlow.width)
                            height: 28
                            radius: 4
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                            border.color: glassBorder

                            Flickable {
                                anchors.fill: parent
                                anchors.margins: 1
                                contentWidth: bandButtonsRow.width
                                contentHeight: bandButtonsRow.height
                                flickableDirection: Flickable.HorizontalFlick
                                boundsBehavior: Flickable.StopAtBounds
                                interactive: contentWidth > width
                                clip: true

                                Row {
                                    id: bandButtonsRow
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
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                hoverEnabled: true
                                                onClicked: {
                                                    if (engine && engine.bandManager)
                                                        engine.bandManager.changeBandByLambda(modelData + "M")
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 136
                            height: 36
                            radius: 6
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
                            border.color: secondaryCyan
                            border.width: 1

                            StyledComboBox {
                                id: modeSelector
                                anchors.fill: parent
                                anchors.margins: 1
                                model: txPanel.supportedModes
                                currentIndex: {
                                    var mode = engine ? engine.mode : "FT8"
                                    var idx = model.indexOf(mode)
                                    return idx >= 0 ? idx : 0
                                }
                                onCurrentTextChanged: if (engine && currentText) engine.mode = currentText
                                font.family: "Consolas"
                                font.pixelSize: 14
                                itemHeight: 40
                                popupMinWidth: 176
                                textHorizontalAlignment: Text.AlignHCenter
                                leftPadding: 8
                                rightPadding: 24
                                topPadding: 6
                                bottomPadding: 6
                                bgColor: "transparent"
                                borderColor: "transparent"
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: autoSqBtn.checked ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: autoSqBtn.checked ? primaryBlue : glassBorder
                            border.width: autoSqBtn.checked ? 2 : 1

                            Button {
                                id: autoSqBtn
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.autoSeq : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.autoSeq = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "ASQ"
                                    glyph: "\u26A1"
                                    foreground: autoSqBtn.checked ? primaryBlue : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: autoSqBtn.checked
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Auto Sequence"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: mamBtn.checked ? Qt.rgba(255/255, 152/255, 0, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: mamBtn.checked ? warningOrange : glassBorder
                            border.width: mamBtn.checked ? 2 : 1

                            Button {
                                id: mamBtn
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.multiAnswerMode : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.multiAnswerMode = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "MAM"
                                    glyph: "\u21C6"
                                    foreground: mamBtn.checked ? warningOrange : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: mamBtn.checked
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Multi-Answer Mode (right-click=window)"
                                ToolTip.delay: 500
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: txPanel.mamWindowRequested()
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: deepBtn.checked ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: deepBtn.checked ? accentGreen : glassBorder
                            border.width: deepBtn.checked ? 2 : 1

                            Button {
                                id: deepBtn
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.deepSearchEnabled : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.deepSearchEnabled = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "DEEP"
                                    glyph: "\u25CE"
                                    foreground: deepBtn.checked ? accentGreen : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: deepBtn.checked
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Deep Search"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: apBtn.checked ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: apBtn.checked ? secondaryCyan : glassBorder
                            border.width: apBtn.checked ? 2 : 1

                            Button {
                                id: apBtn
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.avgDecodeEnabled : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.avgDecodeEnabled = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "AP"
                                    glyph: "\u25C6"
                                    foreground: apBtn.checked ? secondaryCyan : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: apBtn.checked
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "A-Priori Decoding"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: swlBtn.checked ? Qt.rgba(156/255, 39/255, 176/255, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: swlBtn.checked ? "#9c27b0" : glassBorder
                            border.width: swlBtn.checked ? 2 : 1

                            Button {
                                id: swlBtn
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.swlMode : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.swlMode = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "SWL"
                                    glyph: "\u2609"
                                    foreground: swlBtn.checked ? "#9c27b0" : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: swlBtn.checked
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "SWL Mode (Listen Only)"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: autoSeqBtn2.checked ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: autoSeqBtn2.checked ? primaryBlue : glassBorder
                            border.width: autoSeqBtn2.checked ? 2 : 1

                            Button {
                                id: autoSeqBtn2
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.autoSeq : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.autoSeq = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "SEQ"
                                    glyph: "\u21BB"
                                    foreground: autoSeqBtn2.checked ? primaryBlue : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: autoSeqBtn2.checked
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Auto Sequence"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 78
                            height: 36
                            radius: 6
                            color: txEnableBtn.checked ? Qt.rgba(244/255, 67/255, 54/255, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: txEnableBtn.checked ? errorRed : glassBorder
                            border.width: txEnableBtn.checked ? 2 : 1

                            Button {
                                id: txEnableBtn
                                anchors.fill: parent
                                checkable: true
                                checked: engine ? engine.txEnabled : false
                                padding: 0
                                onCheckedChanged: if (engine) engine.txEnabled = checked
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "TX"
                                    glyph: "\u25B2"
                                    foreground: txEnableBtn.checked ? errorRed : textSecondary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: true
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Enable TX"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 82
                            height: 36
                            radius: 6
                            color: engine && engine.autoCqRepeat ? Qt.alpha(successGreen, 0.3) : Qt.alpha(textPrimary, 0.05)
                            border.color: engine && engine.autoCqRepeat ? successGreen : Qt.alpha(textPrimary, 0.2)
                            border.width: engine && engine.autoCqRepeat ? 2 : 1

                            Button {
                                id: autoCqButton
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "ACQ"
                                    glyph: "⟳"
                                    foreground: engine && engine.autoCqRepeat ? successGreen : textPrimary
                                    glyphSize: 16
                                    labelSize: 11
                                    boldLabel: engine && engine.autoCqRepeat
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Auto CQ Repeat\n(Chiama CQ automaticamente fino a risposta)"
                                ToolTip.delay: 500

                                onClicked: {
                                    if (engine) {
                                        if (!engine.autoSeq && !engine.autoCqRepeat)
                                            engine.autoSeq = true
                                        engine.autoCqRepeat = !engine.autoCqRepeat
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 84
                            height: 36
                            radius: 6
                            visible: engine && engine.mode !== "FT2"
                            color: engine && engine.txPeriod === 1 ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.28)
                                                                   : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: engine && engine.txPeriod === 1 ? primaryBlue : glassBorder
                            border.width: engine && engine.txPeriod === 1 ? 2 : 1

                            Button {
                                id: txPhaseButton
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: engine && engine.txPeriod === 1 ? "1ST" : "EVEN"
                                    glyph: engine && engine.txPeriod === 1 ? "\u2460" : "\u2461"
                                    foreground: engine && engine.txPeriod === 1 ? primaryBlue : textPrimary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: true
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Tx even/1st\n(come Decodium 3)"
                                ToolTip.delay: 500
                                onClicked: if (engine) engine.txPeriod = engine.txPeriod === 1 ? 0 : 1
                            }
                        }

                        Rectangle {
                            width: 92
                            height: 36
                            radius: 6
                            visible: engine && engine.mode !== "FT2"
                            color: engine && engine.alt12Enabled ? Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.22)
                                                                 : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            border.color: engine && engine.alt12Enabled ? warningOrange : glassBorder
                            border.width: engine && engine.alt12Enabled ? 2 : 1

                            Button {
                                id: alt12Button
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "ALT 1/2"
                                    glyph: "\u21C4"
                                    foreground: engine && engine.alt12Enabled ? warningOrange : textPrimary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: engine && engine.alt12Enabled
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Auto CQ: alterna le fasi Tx/Rx dopo CQ ripetuti senza risposta"
                                ToolTip.delay: 500
                                onClicked: if (engine) engine.alt12Enabled = !engine.alt12Enabled
                            }
                        }

                        Rectangle {
                            width: 86
                            height: 36
                            radius: 6
                            color: engine && engine.startFromTx2 ? Qt.alpha(warningOrange, 0.3) : Qt.alpha(textPrimary, 0.05)
                            border.color: engine && engine.startFromTx2 ? warningOrange : Qt.alpha(textPrimary, 0.2)
                            border.width: engine && engine.startFromTx2 ? 2 : 1

                            Button {
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "Q.Call"
                                    glyph: "⚡"
                                    foreground: engine && engine.startFromTx2 ? warningOrange : textPrimary
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: engine && engine.startFromTx2
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Quick Call\n(Doppio click su decode abilita TX automaticamente\ncome Shannon: 'Double-click on call sets Tx enable')"
                                ToolTip.delay: 500
                                onClicked: if (engine) engine.startFromTx2 = !engine.startFromTx2
                            }
                        }

                        Rectangle {
                            width: 84
                            height: 36
                            radius: 6
                            color: tuneButton.isTuning ? Qt.alpha(warningOrange, 0.5) : Qt.alpha(warningOrange, 0.2)
                            border.color: warningOrange
                            border.width: tuneButton.isTuning ? 2 : 1

                            Button {
                                id: tuneButton
                                anchors.fill: parent
                                property bool isTuning: engine && engine.tuning
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: tuneButton.isTuning ? "STOP" : "TUNE"
                                    glyph: "\u266B"
                                    foreground: warningOrange
                                    glyphSize: 15
                                    labelSize: 10
                                    boldLabel: true
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

                        Rectangle {
                            width: 84
                            height: 36
                            radius: 6
                            color: clearTxButton.hovered ? Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.24)
                                                         : Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.12)
                            border.color: warningOrange
                            border.width: 1

                            Button {
                                id: clearTxButton
                                anchors.fill: parent
                                padding: 0
                                enabled: engine !== null
                                onClicked: if (engine) engine.clearTxMessages()
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "CLEAR"
                                    foreground: warningOrange
                                    labelSize: 11
                                    boldLabel: true
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Svuota DX, report e TX1-TX5"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 84
                            height: 36
                            radius: 6
                            color: haltButton.hovered || (engine && engine.transmitting)
                                   ? Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.92)
                                   : Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.78)
                            border.color: Qt.lighter(errorRed, 1.2)
                            border.width: engine && engine.transmitting ? 2 : 1

                            Button {
                                id: haltButton
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "HALT"
                                    glyph: "\u25A0"
                                    foreground: "#FFF8F6"
                                    glyphSize: 14
                                    labelSize: 11
                                    boldLabel: true
                                }
                                onClicked: if (engine) engine.halt()
                                ToolTip.visible: hovered
                                ToolTip.text: "Halt TX"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 56
                            height: 36
                            radius: 6
                            visible: engine && engine.mode === "FT2"
                            color: engine && engine.asyncTxEnabled ? Qt.alpha(secondaryCyan, 0.3) : Qt.rgba(0.15, 0.15, 0.15, 0.6)
                            border.color: engine && engine.asyncTxEnabled ? secondaryCyan : glassBorder
                            border.width: 1

                            Button {
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "ATX"
                                    foreground: engine && engine.asyncTxEnabled ? secondaryCyan : textSecondary
                                    labelSize: 10
                                    boldLabel: true
                                }
                                onClicked: if (engine) engine.asyncTxEnabled = !engine.asyncTxEnabled
                                ToolTip.visible: hovered
                                ToolTip.text: "Async TX (FT2)"
                                ToolTip.delay: 500
                            }
                        }

                        AsyncModeWidget {
                            id: asyncModeVis
                            width: 90
                            height: 50
                            visible: engine && engine.mode === "FT2"
                            running: engine ? (engine.asyncTxEnabled || engine.asyncDecodeEnabled) : false
                            transmitting: engine ? engine.transmitting : false
                            snr: engine ? engine.asyncSnrDb : -99
                            ToolTip.visible: hovered
                            ToolTip.text: "FT2 Async Mode — onda sinusoidale: verde=RX, rosso=TX"
                            ToolTip.delay: 400
                        }

                        Rectangle {
                            width: 56
                            height: 36
                            radius: 6
                            visible: engine && engine.mode === "FT2"
                            color: engine && engine.dualCarrierEnabled ? Qt.alpha(warningOrange, 0.3) : Qt.rgba(0.15, 0.15, 0.15, 0.6)
                            border.color: engine && engine.dualCarrierEnabled ? warningOrange : glassBorder
                            border.width: 1

                            Button {
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "2xC"
                                    foreground: engine && engine.dualCarrierEnabled ? warningOrange : textSecondary
                                    labelSize: 10
                                    boldLabel: true
                                }
                                onClicked: if (engine) engine.dualCarrierEnabled = !engine.dualCarrierEnabled
                                ToolTip.visible: hovered
                                ToolTip.text: "Dual Carrier (-3dB/carrier)"
                                ToolTip.delay: 500
                            }
                        }

                        Rectangle {
                            width: 56
                            height: 36
                            radius: 6
                            visible: engine && engine.mode === "FT2"
                            color: engine && engine.manualTxMode ? Qt.alpha(errorRed, 0.4) : Qt.rgba(0.15, 0.15, 0.15, 0.6)
                            border.color: engine && engine.manualTxMode ? errorRed : glassBorder
                            border.width: 1

                            Button {
                                anchors.fill: parent
                                padding: 0
                                background: Rectangle { color: "transparent" }
                                contentItem: ToolbarButtonContent {
                                    label: "PTT"
                                    foreground: engine && engine.manualTxMode ? errorRed : textSecondary
                                    labelSize: 10
                                    boldLabel: true
                                }
                                onClicked: {
                                    if (engine) {
                                        if (!engine.manualTxMode) engine.manualTxMode = true
                                        else engine.triggerManualTx()
                                    }
                                }
                                onPressAndHold: if (engine) engine.manualTxMode = false
                                ToolTip.visible: hovered
                                ToolTip.text: "Manual TX: Click=transmit, Long press=disable"
                                ToolTip.delay: 500
                            }
                        }
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

                // TX6 - CQ
                TxButton {
                    txNum: 6
                    label: "TX6"
                    message: engine && engine.txMessages.length > 5 ? engine.txMessages[5] : "-- --"
                    isSelected: engine && engine.currentTx === 6
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 6
                    isCQ: true
                    onClicked: if (engine) engine.sendTx(6)
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

        contentItem: Item {
            Column {
                anchors.centerIn: parent
                width: parent.width - 10
                spacing: 2

                Text {
                    width: parent.width
                    text: label
                    color: {
                        if (isTransmitting) return errorRed
                        if (isSelected) return primaryBlue
                        if (isCQ) return accentGreen
                        return textSecondary
                    }
                    font.pixelSize: 10
                    font.bold: isSelected || isTransmitting
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
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
}
