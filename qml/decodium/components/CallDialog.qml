// 1.0.262 — CALL Dialog (fork-only iu8lmc)
// Pannello per chiamata diretta verso uno specifico callsign con limite target-missing/timeout,
// + sezione AutoCQ generico (max cycles, pausa fra cicli).
//
// Apertura via callButton in TxPanel.qml. Bridge backend:
//   - bridge.targetCallActive       (bool, read-only)
//   - bridge.targetCallSign         (QString, settable)
//   - bridge.targetCallMaxRetries   (int)
//   - bridge.targetCallTimeoutS     (int)
//   - bridge.targetCallPeriod       (CALL UI int: 0=1st, 1=2nd, 2=alterna)
//   - bridge.targetCallPauseS       (int)
//   - bridge.targetCallRetryCount   (int, read-only)
//   - bridge.startTargetCall()      / stopTargetCall()
//
// AutoCQ generico (reuse esistente):
//   - bridge.autoCqMaxCycles        (0 = infinito)
//   - bridge.autoCqPauseSec
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: callDialog
    title: qsTr("Chiamate (CALL)")
    readonly property int screenAvailableWidth: (Screen.desktopAvailableWidth > 0
                                                  ? Screen.desktopAvailableWidth
                                                  : (Screen.width > 0 ? Screen.width : 1024))
    readonly property int screenAvailableHeight: (Screen.desktopAvailableHeight > 0
                                                   ? Screen.desktopAvailableHeight
                                                   : (Screen.height > 0 ? Screen.height : 768))
    width: Math.min(560, Math.max(440, Math.round(screenAvailableWidth * 0.52)))
    height: Math.min(720, Math.max(360, Math.round(screenAvailableHeight * 0.86)))
    minimumWidth: Math.min(420, width)
    minimumHeight: Math.min(320, height)
    // Use a normal native window on Windows too. Qt.Dialog with only a close
    // hint can produce a non-movable tool/dialog frame on some Windows themes.
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    color: "#1a1a2e"

    readonly property color cAccent:     "#3a9dff"  // primaryBlue
    readonly property color cGreen:      "#3acb6c"  // successGreen
    readonly property color cOrange:     "#ff9b3a"  // warningOrange
    readonly property color cText:       "#e8edf2"  // textPrimary
    readonly property color cMuted:      "#a5afc4"
    readonly property color cBorder:     "#4a5372"
    readonly property color cFieldBg:    "#30354f"
    readonly property color cFieldBgFocus: "#384463"

    component StyledSpinBox : SpinBox {
        id: control
        editable: true
        Layout.preferredWidth: 110
        Layout.preferredHeight: 36
        contentItem: TextInput {
            selectByMouse: true
            onActiveFocusChanged: if (activeFocus) selectAll()
            z: 2
            text: control.textFromValue(control.value, control.locale)
            color: callDialog.cText
            selectionColor: callDialog.cAccent
            selectedTextColor: callDialog.cText
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !control.editable
            validator: control.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            font.family: decodiumMonoFontFamily
            font.pixelSize: 14
        }
        up.indicator: Rectangle {
            x: control.width - width
            width: 34
            height: control.height
            color: control.enabled
                   ? (control.up.pressed ? Qt.alpha(callDialog.cAccent, 0.36) : Qt.alpha(callDialog.cAccent, 0.18))
                   : Qt.alpha(callDialog.cText, 0.05)
            border.color: callDialog.cBorder
            radius: 6
            Text {
                anchors.centerIn: parent
                text: "+"
                color: control.enabled ? callDialog.cText : callDialog.cMuted
                font.pixelSize: 18
                font.bold: true
            }
        }
        down.indicator: Rectangle {
            x: 0
            width: 34
            height: control.height
            color: control.enabled
                   ? (control.down.pressed ? Qt.alpha(callDialog.cAccent, 0.36) : Qt.alpha(callDialog.cAccent, 0.18))
                   : Qt.alpha(callDialog.cText, 0.05)
            border.color: callDialog.cBorder
            radius: 6
            Text {
                anchors.centerIn: parent
                text: "-"
                color: control.enabled ? callDialog.cText : callDialog.cMuted
                font.pixelSize: 18
                font.bold: true
            }
        }
        background: Rectangle {
            color: control.enabled
                   ? (control.activeFocus ? callDialog.cFieldBgFocus : callDialog.cFieldBg)
                   : Qt.alpha(callDialog.cText, 0.06)
            border.color: control.activeFocus ? callDialog.cAccent : callDialog.cBorder
            border.width: control.activeFocus ? 2 : 1
            radius: 6
        }
    }

    component DecoCheck : CheckBox {
        id: dc
        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            x: 0
            y: (dc.height - height) / 2
            radius: 4
            color: dc.checked ? Qt.alpha(callDialog.cAccent, 0.28) : callDialog.cFieldBg
            border.color: !dc.enabled ? Qt.alpha(callDialog.cText, 0.15)
                          : (dc.checked ? callDialog.cAccent : callDialog.cBorder)
            border.width: 2
            Text {
                anchors.centerIn: parent
                text: dc.checked ? "✓" : ""
                color: callDialog.cText
                font.pixelSize: 12
                font.bold: true
            }
        }
        contentItem: Text {
            text: dc.text
            color: dc.enabled ? callDialog.cText : callDialog.cMuted
            font.pixelSize: 12
            leftPadding: 24
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        anchors.fill: parent
        color: callDialog.color

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            ScrollView {
                id: callScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: Math.max(0, callScroll.availableWidth - 2)
                    spacing: 12

                    // ====== HEADER ======
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "📞  " + qsTr("Direct call")
                            color: callDialog.cText
                            font.pixelSize: 18
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            visible: bridge && bridge.targetCallActive
                            readonly property bool armed: bridge && bridge.targetCallArmedWaiting
                            width: stateLabel.implicitWidth + 16
                            height: stateLabel.implicitHeight + 6
                            radius: 4
                            color: Qt.alpha(armed ? callDialog.cOrange : callDialog.cGreen, 0.25)
                            border.color: armed ? callDialog.cOrange : callDialog.cGreen
                            border.width: 1
                            Text {
                                id: stateLabel
                                anchors.centerIn: parent
                                text: parent.armed ? qsTr("⏳ ARMED") : qsTr("ACTIVE")
                                color: parent.armed ? callDialog.cOrange : callDialog.cGreen
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: callDialog.cBorder }

            // ====== TARGET CALLSIGN ======
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: qsTr("Target callsign"); color: callDialog.cMuted; font.pixelSize: 12 }
                DecoTextField {
                    id: targetField
                    Layout.fillWidth: true
                    text: bridge ? bridge.targetCallSign : ""
                    placeholderText: qsTr("es. F4CQS")
                    placeholderTextColor: "#7d89a5"
                    color: callDialog.cText
                    font.pixelSize: 18
                    font.bold: true
                    font.family: decodiumMonoFontFamily
                    selectByMouse: true
                    background: Rectangle {
                        color: targetField.activeFocus ? callDialog.cFieldBgFocus : callDialog.cFieldBg
                        border.color: targetField.activeFocus ? callDialog.cAccent : callDialog.cBorder
                        border.width: targetField.activeFocus ? 2 : 1
                        radius: 6
                    }
                    onEditingFinished: if (bridge) bridge.targetCallSign = text
                    enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                }
            }

            // ====== TARGET MISSING + TIMEOUT ======
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8

                Text { text: qsTr("Max unanswered calls"); color: callDialog.cMuted; font.pixelSize: 12 }
                Text { text: qsTr("Total timeout (s)"); color: callDialog.cMuted; font.pixelSize: 12 }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    StyledSpinBox {
                        id: retriesSpin
                        from: 0; to: 999
                        value: bridge ? bridge.targetCallMaxRetries : 10
                        enabled: !infiniteCheck.checked && (!bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled))
                        onValueModified: if (bridge) bridge.targetCallMaxRetries = value
                        Layout.preferredWidth: 100
                    }
                    CheckBox {
                        id: infiniteCheck
                        text: qsTr("∞")
                        checked: bridge && bridge.targetCallMaxRetries === 0
                        enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                        onToggled: {
                            if (!bridge) return
                            if (checked) bridge.targetCallMaxRetries = 0
                            else bridge.targetCallMaxRetries = retriesSpin.value > 0 ? retriesSpin.value : 10
                        }
                        indicator: Rectangle {
                            implicitWidth: 18
                            implicitHeight: 18
                            x: 0
                            y: (infiniteCheck.height - height) / 2
                            radius: 4
                            color: infiniteCheck.checked ? Qt.alpha(callDialog.cAccent, 0.28) : callDialog.cFieldBg
                            border.color: infiniteCheck.checked ? callDialog.cAccent : callDialog.cBorder
                            border.width: 2
                            Text {
                                anchors.centerIn: parent
                                text: infiniteCheck.checked ? "✓" : ""
                                color: callDialog.cText
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        contentItem: Text {
                            text: infiniteCheck.text
                            color: callDialog.cText
                            font.bold: true
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                StyledSpinBox {
                    id: timeoutSpin
                    from: 10; to: 600
                    value: bridge ? bridge.targetCallTimeoutS : 90
                    enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                    Layout.preferredWidth: 120
                    onValueModified: if (bridge) bridge.targetCallTimeoutS = value
                }
            }

            // ====== PERIODO FT8/FT4 ======
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: qsTr("Periodo FT8/FT4"); color: callDialog.cMuted; font.pixelSize: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    RadioButton {
                        id: periodFirst
                        text: qsTr("1st (:00/:30)")
                        checked: bridge && bridge.targetCallPeriod === 0
                        enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                        onToggled: if (checked && bridge) bridge.targetCallPeriod = 0
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: 0
                            y: (periodFirst.height - height) / 2
                            radius: 10
                            color: callDialog.cFieldBg
                            border.color: periodFirst.checked ? callDialog.cAccent : callDialog.cBorder
                            border.width: 2
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: periodFirst.checked ? callDialog.cAccent : "transparent"
                            }
                        }
                        contentItem: Text {
                            text: periodFirst.text
                            color: callDialog.cText
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    RadioButton {
                        id: periodSecond
                        text: qsTr("2nd (:15/:45)")
                        checked: bridge && bridge.targetCallPeriod === 1
                        enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                        onToggled: if (checked && bridge) bridge.targetCallPeriod = 1
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: 0
                            y: (periodSecond.height - height) / 2
                            radius: 10
                            color: callDialog.cFieldBg
                            border.color: periodSecond.checked ? callDialog.cAccent : callDialog.cBorder
                            border.width: 2
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: periodSecond.checked ? callDialog.cAccent : "transparent"
                            }
                        }
                        contentItem: Text {
                            text: periodSecond.text
                            color: callDialog.cText
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    RadioButton {
                        id: periodAlt
                        text: qsTr("Alterna")
                        checked: bridge && bridge.targetCallPeriod === 2
                        enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                        onToggled: if (checked && bridge) bridge.targetCallPeriod = 2
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: 0
                            y: (periodAlt.height - height) / 2
                            radius: 10
                            color: callDialog.cFieldBg
                            border.color: periodAlt.checked ? "#ff2f7b" : callDialog.cBorder
                            border.width: 2
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: periodAlt.checked ? "#ff2f7b" : "transparent"
                            }
                        }
                        contentItem: Text {
                            text: periodAlt.text
                            color: callDialog.cText
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // ====== PAUSA FRA CICLI ======
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: qsTr("Pausa fra cicli (s)")
                    color: callDialog.cMuted
                    font.pixelSize: 12
                    Layout.preferredWidth: 180
                }
                StyledSpinBox {
                    id: pauseSpin
                    from: 0; to: 300
                    value: bridge ? bridge.targetCallPauseS : 0
                    enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                    Layout.preferredWidth: 100
                    onValueModified: if (bridge) bridge.targetCallPauseS = value
                }
            }

            // ====== DX-WATCH ARMATO (1.0.408) ======
            Rectangle { Layout.fillWidth: true; height: 1; color: callDialog.cBorder }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                DecoCheck {
                    id: armedCheck
                    Layout.fillWidth: true
                    text: qsTr("DX-watch armed — doesn't call immediately: waits for the target to be decoded")
                    checked: bridge && bridge.armedWatchEnabled
                    enabled: !bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled)
                    onToggled: if (bridge) bridge.armedWatchEnabled = checked
                }
                DecoCheck {
                    id: reArmCheck
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    text: qsTr("Re-arm: if the target disappears, go back to listening (max 3 times, then manual Halt)")
                    checked: bridge && bridge.armedReArm
                    enabled: armedCheck.checked && (!bridge || (!bridge.targetCallActive && !bridge.autoCallEnabled))
                    opacity: enabled ? 1.0 : 0.5
                    onToggled: if (bridge) bridge.armedReArm = checked
                }
            }

            // ====== TELEMETRIA RUNTIME ======
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: telemetryCol.implicitHeight + 16
                color: Qt.alpha(callDialog.cAccent, 0.08)
                border.color: Qt.alpha(callDialog.cAccent, 0.3)
                border.width: 1
                radius: 4
                visible: bridge && bridge.targetCallActive

                ColumnLayout {
                    id: telemetryCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    Text {
                        readonly property bool armed: bridge && bridge.targetCallArmedWaiting
                        text: armed
                              ? qsTr("⏳ Listening: waiting for %1 to be decoded…").arg(bridge ? bridge.targetCallSign : "")
                              : qsTr("Status: calling %1").arg(bridge ? bridge.targetCallSign : "")
                        color: armed ? callDialog.cOrange : callDialog.cAccent
                        font.bold: true
                        font.pixelSize: 13
                    }
                    Text {
                        visible: !(bridge && bridge.targetCallArmedWaiting)
                        text: bridge ? qsTr("Unanswered %1 / %2").arg(bridge.targetCallRetryCount).arg(
                                  bridge.targetCallMaxRetries === 0 ? qsTr("∞") : bridge.targetCallMaxRetries) : ""
                        color: callDialog.cText
                        font.pixelSize: 12
                    }
                }
            }

            Item { Layout.preferredHeight: 2 } // spacer

            // ====== AUTO CALL ======
            Rectangle { Layout.fillWidth: true; height: 1; color: callDialog.cBorder }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                Text {
                    text: qsTr("Auto Call")
                    color: callDialog.cText
                    font.pixelSize: 16
                    font.bold: true
                }
                DecoCheck {
                    id: autoCallCheck
                    Layout.fillWidth: true
                    text: qsTr("Automatically call eligible CQ stations without selecting a target")
                    checked: bridge && bridge.autoCallEnabled
                    enabled: bridge && (!bridge.targetCallActive || bridge.autoCallEnabled)
                    onToggled: if (bridge) bridge.autoCallEnabled = checked
                }
                Button {
                    id: autoCallStartButton
                    property bool running: bridge && bridge.autoCallEnabled
                    Layout.fillWidth: true
                    text: running
                          ? qsTr("■ Stop Auto Call")
                          : qsTr("▶ Start Auto Call")
                    enabled: bridge && (!bridge.targetCallActive || bridge.autoCallEnabled)
                    onClicked: if (bridge) bridge.autoCallEnabled = !bridge.autoCallEnabled
                    background: Rectangle {
                        radius: 6
                        color: parent.running
                               ? (parent.down ? Qt.darker(callDialog.cGreen, 1.12) : callDialog.cGreen)
                               : (parent.enabled
                                  ? (parent.down ? Qt.alpha(callDialog.cGreen, 0.38)
                                                 : (parent.hovered ? Qt.alpha(callDialog.cGreen, 0.32)
                                                                   : Qt.alpha(callDialog.cGreen, 0.24)))
                                  : Qt.alpha(callDialog.cText, 0.06))
                        border.color: parent.running || parent.enabled ? callDialog.cGreen : callDialog.cBorder
                        border.width: parent.enabled ? 2 : 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.running ? "#08131a" : (parent.enabled ? callDialog.cGreen : callDialog.cMuted)
                        font.pixelSize: 13
                        font.bold: parent.enabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Answers eligible CQ stations, skips calls already worked on this band, and stops at the session limit.")
                    color: callDialog.cMuted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: bridge && bridge.autoCallEnabled
                          ? qsTr("Armed: waits for a valid CQ decode; TX starts on the next legal slot.")
                          : qsTr("Idle: press Start Auto Call to arm automatic CQ replies.")
                    color: bridge && bridge.autoCallEnabled ? callDialog.cGreen : callDialog.cMuted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 6
                    Text { text: qsTr("Max QSOs this session (0=∞)"); color: callDialog.cMuted; font.pixelSize: 12 }
                    StyledSpinBox {
                        from: 0; to: 999
                        value: bridge ? bridge.autoCallMaxQsos : 5
                        enabled: bridge && !bridge.autoCallEnabled
                        Layout.preferredWidth: 110
                        onValueModified: if (bridge) bridge.autoCallMaxQsos = value
                    }
                    Text { text: qsTr("Candidate priority"); color: callDialog.cMuted; font.pixelSize: 12 }
                    ComboBox {
                        id: autoCallPriorityCombo
                        model: [qsTr("Last decoded"), qsTr("Strongest signal"), qsTr("Furthest grid")]
                        currentIndex: bridge ? bridge.autoCallPriority : 0
                        enabled: bridge && !bridge.autoCallEnabled
                        Layout.fillWidth: true
                        onActivated: if (bridge) bridge.autoCallPriority = currentIndex
                        contentItem: Text {
                            leftPadding: 14
                            rightPadding: 34
                            text: autoCallPriorityCombo.displayText
                            color: autoCallPriorityCombo.enabled ? callDialog.cText : callDialog.cMuted
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        indicator: Text {
                            x: autoCallPriorityCombo.width - width - 12
                            y: (autoCallPriorityCombo.height - height) / 2
                            text: "▾"
                            color: autoCallPriorityCombo.enabled ? callDialog.cText : callDialog.cMuted
                            font.pixelSize: 18
                        }
                        background: Rectangle {
                            radius: 6
                            color: autoCallPriorityCombo.enabled
                                   ? (autoCallPriorityCombo.activeFocus
                                      ? callDialog.cFieldBgFocus : callDialog.cFieldBg)
                                   : Qt.alpha(callDialog.cText, 0.08)
                            border.color: autoCallPriorityCombo.activeFocus
                                          ? callDialog.cAccent : callDialog.cBorder
                            border.width: autoCallPriorityCombo.activeFocus ? 2 : 1
                        }
                        delegate: ItemDelegate {
                            width: autoCallPriorityCombo.width
                            contentItem: Text {
                                leftPadding: 12
                                text: modelData
                                color: parent.highlighted ? callDialog.cText : callDialog.cMuted
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.highlighted ? callDialog.cAccent : callDialog.cFieldBg
                            }
                        }
                        popup: Popup {
                            y: autoCallPriorityCombo.height + 2
                            width: autoCallPriorityCombo.width
                            padding: 2
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: autoCallPriorityCombo.popup.visible ? autoCallPriorityCombo.delegateModel : null
                            }
                            background: Rectangle {
                                color: callDialog.cFieldBg
                                border.color: callDialog.cAccent
                                border.width: 1
                                radius: 6
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: bridge ? qsTr("QSOs completed: %1 / %2").arg(bridge.autoCallQsoCount).arg(
                                  bridge.autoCallMaxQsos === 0 ? qsTr("∞") : bridge.autoCallMaxQsos) : ""
                        color: callDialog.cMuted
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Reset count")
                        enabled: bridge && bridge.autoCallQsoCount > 0
                        onClicked: if (bridge) bridge.resetAutoCallQsoCount()
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 36
                        background: Rectangle {
                            radius: 18
                            color: parent.enabled
                                   ? (parent.down ? Qt.alpha(callDialog.cText, 0.24)
                                                  : Qt.alpha(callDialog.cText, 0.12))
                                   : Qt.alpha(callDialog.cText, 0.06)
                            border.color: parent.enabled ? callDialog.cBorder : Qt.alpha(callDialog.cBorder, 0.6)
                            border.width: 1
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? callDialog.cText : callDialog.cMuted
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // ====== SEZIONE AUTOCQ GENERICO (riusa esistente) ======
            Rectangle { Layout.fillWidth: true; height: 1; color: callDialog.cBorder }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: qsTr("Generic AutoCQ (ACQ button)")
                    color: callDialog.cMuted
                    font.pixelSize: 11
                    font.italic: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: qsTr("Max CQ calls (0=∞)")
                        color: callDialog.cMuted
                        font.pixelSize: 12
                        Layout.preferredWidth: 180
                    }
                    StyledSpinBox {
                        from: 0; to: 999
                        value: bridge ? bridge.autoCqMaxCycles : 0
                        Layout.preferredWidth: 100
                        onValueModified: if (bridge) bridge.autoCqMaxCycles = value
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: qsTr("Pausa fra cicli CQ (s)")
                        color: callDialog.cMuted
                        font.pixelSize: 12
                        Layout.preferredWidth: 180
                    }
                    StyledSpinBox {
                        from: 0; to: 300
                        value: bridge ? bridge.autoCqPauseSec : 0
                        Layout.preferredWidth: 100
                        onValueModified: if (bridge) bridge.autoCqPauseSec = value
                    }
                }
            }

            }
            }

            // ====== ACTION BUTTONS ======
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 8

                Button {
                    text: qsTr("Close")
                    Layout.preferredWidth: 90
                    onClicked: callDialog.close()
                    background: Rectangle {
                        radius: 6
                        color: parent.down ? Qt.alpha(callDialog.cText, 0.20)
                                           : (parent.hovered ? Qt.alpha(callDialog.cText, 0.15)
                                                             : Qt.alpha(callDialog.cText, 0.10))
                        border.color: callDialog.cBorder
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: callDialog.cText
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Stop")
                    Layout.preferredWidth: 90
                    enabled: bridge !== null
                    onClicked: {
                        if (!bridge) return
                        if (bridge.targetCallActive)
                            bridge.stopTargetCall()
                        if (bridge.autoCallEnabled)
                            bridge.autoCallEnabled = false
                        bridge.haltWithReason("call-dialog-stop")
                    }
                    background: Rectangle {
                        radius: 6
                        color: parent.enabled
                               ? (parent.down ? Qt.alpha(callDialog.cOrange, 0.36)
                                              : (parent.hovered ? Qt.alpha(callDialog.cOrange, 0.30)
                                                                : Qt.alpha(callDialog.cOrange, 0.22)))
                               : Qt.alpha(callDialog.cText, 0.06)
                        border.color: parent.enabled ? callDialog.cOrange : callDialog.cBorder
                        border.width: parent.enabled ? 2 : 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? callDialog.cOrange : callDialog.cMuted
                        font.pixelSize: 13
                        font.bold: parent.enabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: qsTr("▶ Direct Call")
                    Layout.preferredWidth: 125
                    enabled: bridge && !bridge.targetCallActive && !bridge.autoCallEnabled && targetField.text.trim().length > 2
                    onClicked: {
                        if (!bridge) return
                        bridge.targetCallSign = targetField.text
                        bridge.startTargetCall()
                    }
                    background: Rectangle {
                        radius: 6
                        color: parent.enabled
                               ? (parent.down ? Qt.alpha(callDialog.cGreen, 0.38)
                                              : (parent.hovered ? Qt.alpha(callDialog.cGreen, 0.32)
                                                                : Qt.alpha(callDialog.cGreen, 0.24)))
                               : Qt.alpha(callDialog.cText, 0.06)
                        border.color: parent.enabled ? callDialog.cGreen : callDialog.cBorder
                        border.width: parent.enabled ? 2 : 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? callDialog.cGreen : callDialog.cMuted
                        font.pixelSize: 13
                        font.bold: parent.enabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
