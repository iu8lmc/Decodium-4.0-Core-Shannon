/* Decodium Qt6 - Macro Configuration Dialog
 * TX Macro editing and contest mode settings
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: macroDialog
    title: "Macro Settings"
    modal: true
    width: 700
    height: 600
    closePolicy: Popup.CloseOnEscape
    property bool positionInitialized: false

    function clampToParent() {
        if (!parent) return
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        x = Math.max(0, Math.round((parent.width - width) / 2))
        y = Math.max(0, Math.round((parent.height - height) / 2))
        positionInitialized = true
    }

    onAboutToShow: ensureInitialPosition()

    // Colors
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color bgLight: bridge.themeManager.bgLight
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan
        border.width: 2
        radius: 12
    }

    header: Rectangle {
        height: 50
        color: "transparent"

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                macroDialog.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                macroDialog.x += mouse.x - clickPos.x
                macroDialog.y += mouse.y - clickPos.y
                macroDialog.clampToParent()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                text: "⌨️ TX Macro Configuration"
                font.pixelSize: 18
                font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            // Minimize button
            Rectangle {
                width: 28
                height: 28
                radius: 4
                color: macroMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                border.color: macroMinMA.containsMouse ? "#ffc107" : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: "−"
                    font.pixelSize: 18
                    font.bold: true
                    color: macroMinMA.containsMouse ? "#ffc107" : textPrimary
                }

                MouseArea {
                    id: macroMinMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        macroDialogMinimized = true
                        macroDialog.close()
                    }
                }

                ToolTip.visible: macroMinMA.containsMouse
                ToolTip.text: "Riduci a icona"
                ToolTip.delay: 500
            }

            // Close button
            Rectangle {
                width: 28
                height: 28
                radius: 4
                color: macroCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                border.color: macroCloseMA.containsMouse ? "#f44336" : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 12
                    font.bold: true
                    color: macroCloseMA.containsMouse ? "#f44336" : textPrimary
                }

                MouseArea {
                    id: macroCloseMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: macroDialog.close()
                }

                ToolTip.visible: macroCloseMA.containsMouse
                ToolTip.text: "Chiudi"
                ToolTip.delay: 500
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 16

        // Contest Mode Section
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.1)
            border.color: primaryBlue
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "Contest Mode"
                    font.pixelSize: 14
                    font.bold: true
                    color: primaryBlue
                }

                RowLayout {
                    spacing: 12

                    Text {
                        text: "Contest:"
                        font.pixelSize: 12
                        color: textSecondary
                        Layout.preferredWidth: 80
                    }

                    StyledComboBox {
                        id: contestCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        model: appEngine.macroManager ? appEngine.macroManager.contestList : []
                        currentIndex: appEngine.macroManager ? appEngine.macroManager.contestId : 0

                        background: Rectangle {
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                            border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                            radius: 4
                        }

                        contentItem: Text {
                            text: contestCombo.displayText
                            color: textPrimary
                            font.pixelSize: 12
                            leftPadding: 8
                            verticalAlignment: Text.AlignVCenter
                        }

                        onCurrentIndexChanged: {
                            if (appEngine.macroManager) {
                                appEngine.macroManager.contestId = currentIndex
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 12
                    visible: appEngine.macroManager && appEngine.macroManager.contestMode

                    Text {
                        text: "Serial #:"
                        font.pixelSize: 12
                        color: textSecondary
                        Layout.preferredWidth: 80
                    }

                    SpinBox {
                        id: serialSpinBox
                        from: 1
                        to: appEngine.macroManager ? appEngine.macroManager.getSerialNumberMax() : 7999
                        value: appEngine.macroManager ? appEngine.macroManager.serialNumber : 1
                        Layout.preferredWidth: 120

                        background: Rectangle {
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                            border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                            radius: 4
                        }

                        contentItem: TextInput {
                            text: serialSpinBox.value.toString().padStart(4, '0')
                            font.family: "Monospace"
                            font.pixelSize: 14
                            color: accentGreen
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            readOnly: !serialSpinBox.editable
                            validator: serialSpinBox.validator
                        }

                        onValueChanged: {
                            if (appEngine.macroManager) {
                                appEngine.macroManager.serialNumber = value
                            }
                        }
                    }

                    Text {
                        text: "Exchange:"
                        font.pixelSize: 12
                        color: textSecondary
                        Layout.leftMargin: 20
                    }

                    TextField {
                        id: exchangeField
                        text: appEngine.macroManager ? appEngine.macroManager.exchange : ""
                        Layout.preferredWidth: 120
                        font.pixelSize: 12
                        color: textPrimary

                        background: Rectangle {
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                            border.color: exchangeField.activeFocus ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                            radius: 4
                        }

                        onTextChanged: {
                            if (appEngine.macroManager) {
                                appEngine.macroManager.exchange = text.toUpperCase()
                            }
                        }
                    }
                }
            }
        }

        // Macro Templates Section
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.2)
            border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Text {
                        text: "TX Macro Templates"
                        font.pixelSize: 14
                        font.bold: true
                        color: textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Reset to Default"
                        implicitHeight: 28
                        background: Rectangle {
                            color: parent.hovered ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: textSecondary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }
                        onClicked: {
                            if (appEngine.macroManager) {
                                appEngine.macroManager.resetMacrosToDefault()
                                macroRepeater.model = null  // Force refresh
                                macroRepeater.model = 7
                            }
                        }
                    }
                }

                // Help text
                Text {
                    text: "Codes: %M=MyCall  %T=TheirCall  %R=Report  %N=Serial#  %G4=Grid4  %G6=Grid6  %E=Exchange"
                    font.pixelSize: 10
                    color: textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 6

                        Repeater {
                            id: macroRepeater
                            model: 7

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    width: 50
                                    height: 32
                                    radius: 4
                                    color: {
                                        if (index === 5) return Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3)  // TX6 = CQ (green)
                                        if (index === 1 || index === 2) return Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)  // TX2/TX3 = Report (cyan)
                                        return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                    }
                                    border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)

                                    Text {
                                        anchors.centerIn: parent
                                        text: "TX" + (index + 1)
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: textPrimary
                                    }
                                }

                                TextField {
                                    id: macroField
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    text: appEngine.macroManager ? appEngine.macroManager.getMacroTemplate(index) : ""
                                    font.family: "Monospace"
                                    font.pixelSize: 12
                                    color: textPrimary

                                    background: Rectangle {
                                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.4)
                                        border.color: macroField.activeFocus ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
                                        radius: 4
                                    }

                                    onTextChanged: {
                                        if (appEngine.macroManager && text !== appEngine.macroManager.getMacroTemplate(index)) {
                                            appEngine.macroManager.setMacroTemplate(index, text)
                                        }
                                    }
                                }

                                // Preview button
                                Button {
                                    implicitWidth: 60
                                    implicitHeight: 32
                                    text: "Preview"

                                    background: Rectangle {
                                        color: parent.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                        radius: 4
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        color: textSecondary
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    onClicked: {
                                        if (appEngine.macroManager) {
                                            var preview = appEngine.macroManager.generateContestTx(
                                                index + 1,
                                                appEngine.callsign,
                                                appEngine.grid,
                                                appEngine.dxCall || "W1ABC",
                                                "",
                                                appEngine.reportSent || "-10",
                                                appEngine.txFrequency
                                            )
                                            previewText.text = "Preview: " + preview
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Preview area
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                    border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                    radius: 4

                    Text {
                        id: previewText
                        anchors.centerIn: parent
                        text: "Click 'Preview' to see expanded macro"
                        font.family: "Monospace"
                        font.pixelSize: 12
                        color: accentGreen
                    }
                }
            }
        }

        // Footer buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Item { Layout.fillWidth: true }

            Button {
                text: "Close"
                implicitWidth: 100
                implicitHeight: 36

                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.4) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                    border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 13
                    color: textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }

                onClicked: macroDialog.close()
            }
        }
    }
}
