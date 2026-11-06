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
    id: frequenciesScrollView
    clip: true
    // SettingsDialog owns the persistence operations, while this lazily-loaded
    // page owns the editor controls. Export the controls explicitly: QML ids
    // are component-local and cannot otherwise be resolved by SettingsDialog.
    property alias calibrationSlopeFieldControl: frequencySlopeField
    property alias calibrationInterceptFieldControl: frequencyInterceptField
    property alias workingFrequencyRegionControl: frequencyRegionCombo
    property alias workingFrequencyModeControl: frequencyModeCombo
    property alias workingFrequencyMHzControl: frequencyMHzField
    property alias workingFrequencyPreferredControl: frequencyPreferredCheck
    property alias workingFrequencyDescriptionControl: frequencyDescriptionField
    property alias workingFrequencyStartControl: frequencyStartField
    property alias workingFrequencyEndControl: frequencyEndField
    property alias stationFrequencyBandControl: stationBandCombo
    property alias stationFrequencyOffsetControl: stationOffsetField
    property alias stationFrequencyAntennaControl: stationAntennaField
    readonly property int pageContentWidth: dialog.frequencyPageMinWidth
    minimumContentWidth: pageContentWidth + dialog.scrollLeftMargin + dialog.scrollRightMargin
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    ColumnLayout {
        id: frequenciesContent
        width: Math.max(dialog.frequencyPageMinWidth,
                        parent.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.topMargin: dialog.scrollTopMargin
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("FREQUENCY CALIBRATION")
                color: secondaryCyan
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                id: refreshFrequencyButton
                text: qsTr("Refresh")
                implicitHeight: controlHeight
                Layout.preferredWidth: 94
                onClicked: dialog.refreshFrequencySettings()
                background: Rectangle {
                    color: refreshFrequencyButton.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.24) : bgMedium
                    border.color: glassBorder
                    radius: 4
                }
                contentItem: Text {
                    text: refreshFrequencyButton.text
                    color: textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        GridLayout {
            Layout.fillWidth: true
            columns: compactSettingsLayout ? 3 : 6
            columnSpacing: 10
            rowSpacing: 8

            Text { text: qsTr("Slope:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 80; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
            DecoTextField {
                id: frequencySlopeField
                text: Number(bridge.frequencyCalibrationSlopePpm()).toFixed(5)
                Layout.preferredWidth: 130
                implicitHeight: controlHeight
                leftPadding: 8
                color: textPrimary
                font.pixelSize: controlFontSize
                horizontalAlignment: TextInput.AlignRight
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: DoubleValidator { bottom: -200; top: 200; decimals: 5; notation: DoubleValidator.StandardNotation }
                topPadding: controlVerticalPadding
                bottomPadding: controlVerticalPadding
                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                onEditingFinished: text = dialog.commitFrequencySlope(text)
            }
            Text { text: qsTr("ppm"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 44; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }

            Text { text: qsTr("Intercept:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 88; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
            DecoTextField {
                id: frequencyInterceptField
                text: Number(bridge.frequencyCalibrationInterceptHz()).toFixed(2)
                Layout.preferredWidth: 130
                implicitHeight: controlHeight
                leftPadding: 8
                color: textPrimary
                font.pixelSize: controlFontSize
                horizontalAlignment: TextInput.AlignRight
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: DoubleValidator { bottom: -1000; top: 1000; decimals: 2; notation: DoubleValidator.StandardNotation }
                topPadding: controlVerticalPadding
                bottomPadding: controlVerticalPadding
                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                onEditingFinished: text = dialog.commitFrequencyIntercept(text)
            }
            Text { text: qsTr("Hz"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 22; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
            // 1.0.192 — Reset button per Frequency Calibration (slope=0, intercept=0)
            Button {
                id: frequencyCalibrationResetButton
                text: qsTr("Reset")
                implicitHeight: controlHeight
                Layout.fillWidth: true
                onClicked: {
                    if (!bridge) return
                    bridge.setFrequencyCalibrationSlopePpm(0.0)
                    bridge.setFrequencyCalibrationInterceptHz(0.0)
                    frequencySlopeField.text = Number(0).toFixed(5)
                    frequencyInterceptField.text = Number(0).toFixed(2)
                }
                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text { text: parent.text; color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("Reset calibration (slope=0, intercept=0). The frequency is written to the rig without correction (fast path).")
            }
        }

        // 1.0.193 — Live preview della correzione su frequenze tipiche FT8
        // (banda 20m 14.074 MHz, banda 10m 28.074 MHz). Aggiornato a ogni
        // edit dei TextField slope/intercept tramite property binding.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 12
            Text {
                text: qsTr("Preview correzione:")
                color: textSecondary
                font.pixelSize: 11
                font.italic: true
            }
            Text {
                id: frequencyCalibrationPreview
                readonly property double slope: parseFloat(frequencySlopeField.text) || 0.0
                readonly property double intercept: parseFloat(frequencyInterceptField.text) || 0.0
                readonly property double delta14: 14074000.0 * slope * 1e-6 + intercept
                readonly property double delta28: 28074000.0 * slope * 1e-6 + intercept
                text: qsTr("14.074 MHz → %1 Hz · 28.074 MHz → %2 Hz")
                          .arg(delta14.toFixed(2))
                          .arg(delta28.toFixed(2))
                color: (Math.abs(delta14) > 50 || Math.abs(delta28) > 100)
                       ? "#ff8844" : secondaryCyan
                font.pixelSize: 11
                font.family: mainWindow.decodedTextFontFamily
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            Text {
                text: qsTr("WORKING FREQUENCIES")
                color: secondaryCyan
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                id: loadWorkingFrequenciesButton
                text: qsTr("Load")
                implicitHeight: controlHeight
                Layout.preferredWidth: 78
                onClicked: dialog.openWorkingFrequenciesLoadDialog(false)
                background: Rectangle { color: loadWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text { text: loadWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                id: mergeWorkingFrequenciesButton
                text: qsTr("Merge")
                implicitHeight: controlHeight
                Layout.preferredWidth: 78
                onClicked: dialog.openWorkingFrequenciesLoadDialog(true)
                background: Rectangle { color: mergeWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text { text: mergeWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                id: saveWorkingFrequenciesButton
                text: qsTr("Save as")
                implicitHeight: controlHeight
                Layout.preferredWidth: 88
                onClicked: dialog.openWorkingFrequenciesSaveDialog()
                background: Rectangle { color: saveWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text { text: saveWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                id: resetWorkingFrequenciesButton
                text: qsTr("Defaults")
                implicitHeight: controlHeight
                Layout.preferredWidth: 104
                onClicked: {
                    bridge.resetWorkingFrequenciesToDefaults()
                    dialog.clearWorkingFrequencyEditor()
                    dialog.refreshFrequencySettings()
                }
                background: Rectangle { color: resetWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text { text: resetWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.44)
            border.color: glassBorder
            radius: 6
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: qsTr("Region:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 58; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoComboBox {
                        id: frequencyRegionCombo
                        model: dialog.frequencyRegionOptions
                        Layout.preferredWidth: 132
                        implicitHeight: controlHeight
                    }
                    Text { text: qsTr("Mode:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 44; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoComboBox {
                        id: frequencyModeCombo
                        model: dialog.frequencyModeOptions
                        Layout.preferredWidth: 124
                        implicitHeight: controlHeight
                        Component.onCompleted: dialog.setComboText(frequencyModeCombo, bridge.mode || "FT8")
                    }
                    Text { text: qsTr("Freq MHz:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 68; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoTextField {
                        id: frequencyMHzField
                        placeholderText: "14.074000"
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        horizontalAlignment: TextInput.AlignRight
                        leftPadding: 8
                        rightPadding: 8
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        implicitHeight: controlHeight
                        background: Rectangle {
                            color: bgMedium
                            border.color: parent.activeFocus ? secondaryCyan
                                                               : (parent.text.length > 0 && !dialog.workingFrequencyEditorHasValidFrequency() ? "#ff7777" : glassBorder)
                            radius: 4
                        }
                    }
                    CheckBox {
                        id: frequencyPreferredCheck
                        text: qsTr("Pref")
                        Layout.preferredWidth: 82
                        implicitHeight: controlHeight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: qsTr("Description:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 94; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoTextField {
                        id: frequencyDescriptionField
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        Layout.fillWidth: true
                        implicitHeight: controlHeight
                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: qsTr("Start:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 46; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoTextField {
                        id: frequencyStartField
                        placeholderText: "yyyy-MM-dd HH:mm"
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        Layout.fillWidth: true
                        Layout.minimumWidth: 170
                        implicitHeight: controlHeight
                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }
                    Text { text: qsTr("End:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 38; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoTextField {
                        id: frequencyEndField
                        placeholderText: "yyyy-MM-dd HH:mm"
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        Layout.fillWidth: true
                        Layout.minimumWidth: 170
                        implicitHeight: controlHeight
                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    Button {
                        id: addWorkingFrequencyButton
                        text: qsTr("Add")
                        enabled: dialog.workingFrequencyEditorHasValidFrequency()
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 86
                        onClicked: dialog.addWorkingFrequencyFromEditor()
                        background: Rectangle { color: addWorkingFrequencyButton.enabled && addWorkingFrequencyButton.hovered ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.18) : bgMedium; border.color: addWorkingFrequencyButton.enabled ? accentGreen : glassBorder; radius: 4 }
                        contentItem: Text { text: addWorkingFrequencyButton.text; color: addWorkingFrequencyButton.enabled ? accentGreen : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        id: updateWorkingFrequencyButton
                        text: qsTr("Update")
                        enabled: dialog.selectedWorkingFrequencyIndex >= 0
                                 && dialog.workingFrequencyEditorHasValidFrequency()
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 96
                        onClicked: dialog.updateWorkingFrequencyFromEditor()
                        background: Rectangle { color: updateWorkingFrequencyButton.enabled && updateWorkingFrequencyButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: updateWorkingFrequencyButton.enabled ? primaryBlue : glassBorder; radius: 4 }
                        contentItem: Text { text: updateWorkingFrequencyButton.text; color: updateWorkingFrequencyButton.enabled ? primaryBlue : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        id: deleteWorkingFrequencyButton
                        text: qsTr("Delete")
                        enabled: dialog.selectedWorkingFrequencyIndex >= 0
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 96
                        onClicked: dialog.deleteSelectedWorkingFrequency()
                        background: Rectangle { color: deleteWorkingFrequencyButton.enabled && deleteWorkingFrequencyButton.hovered ? Qt.rgba(1,0.2,0.2,0.16) : bgMedium; border.color: deleteWorkingFrequencyButton.enabled ? "#ff5b5b" : glassBorder; radius: 4 }
                        contentItem: Text { text: deleteWorkingFrequencyButton.text; color: deleteWorkingFrequencyButton.enabled ? "#ff7777" : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        id: clearWorkingFrequencyButton
                        text: qsTr("New")
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 86
                        onClicked: dialog.newWorkingFrequencyEditor()
                        background: Rectangle { color: clearWorkingFrequencyButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.16) : bgMedium; border.color: glassBorder; radius: 4 }
                        contentItem: Text { text: clearWorkingFrequencyButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(420, Math.max(260, dialog.height * 0.40))
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.62)
            border.color: glassBorder
            radius: 6
            clip: true

            Flickable {
                id: frequencyTableFlick
                anchors.fill: parent
                contentWidth: Math.max(width, 1120)
                contentHeight: frequencyTableColumn.height
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: frequencyTableColumn
                    width: frequencyTableFlick.contentWidth
                    Rectangle {
                        width: parent.width
                        height: 30
                        color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18)
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Text { text: qsTr("IARU Region"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: qsTr("Mode"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 80; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: qsTr("Frequency"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 210; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: qsTr("Pref"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 56; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            Text { text: qsTr("Description"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 250; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: qsTr("Start Date/Time"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: qsTr("End Date/Time"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                    Repeater {
                        id: frequencySettingsList
                        model: dialog.workingFrequencyRows
                        delegate: Rectangle {
                            id: frequencySettingsRow
                            width: frequencyTableColumn.width
                            height: 30
                            color: dialog.selectedWorkingFrequencyIndex === Number(row.index)
                                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.22)
                                   : (index % 2 === 0 ? Qt.rgba(1,1,1,0.035) : Qt.rgba(1,1,1,0.015))
                            property var row: modelData
                            MouseArea {
                                anchors.fill: parent
                                onClicked: dialog.selectWorkingFrequencyRow(frequencySettingsRow.row)
                            }
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8
                                Text { text: frequencySettingsRow.row.region || ""; color: textPrimary; font.pixelSize: 11; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                Text { text: frequencySettingsRow.row.mode || ""; color: textPrimary; font.pixelSize: 11; width: 80; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                Text { text: frequencySettingsRow.row.frequency || ""; color: textPrimary; font.pixelSize: 11; width: 210; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                CheckBox {
                                    id: preferredFrequencyCheck
                                    checked: !!frequencySettingsRow.row.preferred
                                    width: 56
                                    height: 30
                                    onClicked: {
                                        bridge.setWorkingFrequencyPreferred(Number(frequencySettingsRow.row.index), checked)
                                        dialog.refreshFrequencySettings()
                                    }
                                    indicator: Rectangle { width: 14; height: 14; radius: 3; color: preferredFrequencyCheck.checked ? primaryBlue : bgMedium; border.color: glassBorder; x: preferredFrequencyCheck.width / 2 - width / 2; y: preferredFrequencyCheck.height / 2 - height / 2 }
                                    contentItem: Text { text: "" }
                                }
                                Text { text: frequencySettingsRow.row.description || ""; color: textPrimary; font.pixelSize: 11; width: 250; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                Text { text: frequencySettingsRow.row.startTime || ""; color: textSecondary; font.pixelSize: 11; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                Text { text: frequencySettingsRow.row.endTime || ""; color: textSecondary; font.pixelSize: 11; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 3
            Text {
                text: qsTr("STATION INFORMATION")
                color: secondaryCyan
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("Band offset is the transverter/station frequency offset for that band; use 0.000000 when unused.")
                color: textSecondary
                font.pixelSize: 10
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 106
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.44)
            border.color: glassBorder
            radius: 6
            clip: true
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: qsTr("Band:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 46; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoComboBox {
                        id: stationBandCombo
                        model: dialog.frequencyBandOptions
                        Layout.preferredWidth: 132
                        implicitHeight: controlHeight
                        Component.onCompleted: dialog.setComboText(stationBandCombo, "20m")
                    }
                    Text { text: qsTr("Offset MHz:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 82; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoTextField {
                        id: stationOffsetField
                        text: "0.000000"
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        horizontalAlignment: TextInput.AlignRight
                        leftPadding: 8
                        rightPadding: 8
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: RegularExpressionValidator {
                            // Accept pasted units and typographic minus signs;
                            // the backend normalises and validates the value.
                            regularExpression: /^\s*[-+\u2212\u2012\u2013\u2014\uFE63\uFF0D]?(?:\d+(?:[.,]\d*)?|[.,]\d+)\s*(?:MHz|Hz)?\s*$/i
                        }
                        onTextEdited: {
                            dialog.stationFrequencyEditorStatus = ""
                            dialog.stationFrequencyEditorError = false
                        }
                        Layout.preferredWidth: 146
                        implicitHeight: controlHeight
                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }
                    Text { text: qsTr("Antenna:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 70; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                    DecoTextField {
                        id: stationAntennaField
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        Layout.fillWidth: true
                        Layout.minimumWidth: 160
                        implicitHeight: controlHeight
                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: dialog.stationFrequencyEditorStatus
                        color: dialog.stationFrequencyEditorError ? "#ff7777" : accentGreen
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Button {
                        id: addStationFrequencyButton
                        text: qsTr("Add")
                        enabled: String(stationOffsetField.text || "").trim().length > 0
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 86
                        onClicked: dialog.addStationFrequencyFromEditor()
                        background: Rectangle { color: addStationFrequencyButton.hovered ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.18) : bgMedium; border.color: accentGreen; radius: 4 }
                        contentItem: Text { text: addStationFrequencyButton.text; color: accentGreen; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        id: updateStationFrequencyButton
                        text: qsTr("Update")
                        enabled: dialog.selectedStationFrequencyIndex >= 0
                                 && String(stationOffsetField.text || "").trim().length > 0
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 96
                        onClicked: dialog.updateStationFrequencyFromEditor()
                        background: Rectangle { color: updateStationFrequencyButton.enabled && updateStationFrequencyButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: updateStationFrequencyButton.enabled ? primaryBlue : glassBorder; radius: 4 }
                        contentItem: Text { text: updateStationFrequencyButton.text; color: updateStationFrequencyButton.enabled ? primaryBlue : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        id: deleteStationFrequencyButton
                        text: qsTr("Delete")
                        enabled: dialog.selectedStationFrequencyIndex >= 0
                        implicitHeight: controlHeight
                        Layout.preferredWidth: 96
                        onClicked: dialog.deleteSelectedStationFrequency()
                        background: Rectangle { color: deleteStationFrequencyButton.enabled && deleteStationFrequencyButton.hovered ? Qt.rgba(1,0.2,0.2,0.16) : bgMedium; border.color: deleteStationFrequencyButton.enabled ? "#ff5b5b" : glassBorder; radius: 4 }
                        contentItem: Text { text: deleteStationFrequencyButton.text; color: deleteStationFrequencyButton.enabled ? "#ff7777" : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 210
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.62)
            border.color: glassBorder
            radius: 6
            clip: true
            Column {
                anchors.fill: parent
                Rectangle {
                    width: parent.width
                    height: 30
                    color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18)
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8
                        Text { text: qsTr("Band"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: qsTr("Offset"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 160; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        Text { text: qsTr("Antenna Description"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: parent.width - 310; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                    }
                }
                ListView {
                    id: stationSettingsList
                    width: parent.width
                    height: parent.height - 30
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: dialog.stationFrequencyRows
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: Rectangle {
                        id: stationSettingsRow
                        width: stationSettingsList.width
                        height: 30
                        color: dialog.selectedStationFrequencyIndex === Number(row.index)
                               ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.22)
                               : (index % 2 === 0 ? Qt.rgba(1,1,1,0.035) : Qt.rgba(1,1,1,0.015))
                        property var row: modelData
                        MouseArea {
                            anchors.fill: parent
                            onClicked: dialog.selectStationFrequencyRow(stationSettingsRow.row)
                        }
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Text { text: stationSettingsRow.row.band || ""; color: textPrimary; font.pixelSize: 11; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: stationSettingsRow.row.offset || ""; color: textPrimary; font.pixelSize: 11; width: 160; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            Text { text: stationSettingsRow.row.antenna || ""; color: textPrimary; font.pixelSize: 11; width: parent.width - 310; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
        }

        Component.onCompleted: dialog.refreshFrequencySettings()
    }
}
