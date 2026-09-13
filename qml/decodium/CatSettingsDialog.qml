import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Dialog {
    id: catDialog
    title: qsTr("CAT - Radio Settings")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 440
    height: 360

    // Colors injected from parent (Main.qml properties), with theme-aware fallback
    property color bgColor:      (bridge && bridge.themeManager) ? bridge.themeManager.bgMedium      : "#111827"
    property color bgLight:      (bridge && bridge.themeManager) ? bridge.themeManager.bgLight       : "#1E2D42"
    property color textColor:    (bridge && bridge.themeManager) ? bridge.themeManager.textPrimary   : "#E8F4FD"
    property color textSec:      (bridge && bridge.themeManager) ? bridge.themeManager.textSecondary : "#89B4D0"
    property color accent:       (bridge && bridge.themeManager) ? bridge.themeManager.primaryColor  : "#4A90E2"
    property color accentGreen:  (bridge && bridge.themeManager) ? bridge.themeManager.accentColor   : "#00FF88"

    background: Rectangle {
        color: bgColor
        border.color: accent
        border.width: 1
        radius: 8
    }

    header: Rectangle {
        color: bgLight
        height: 40
        radius: 8
        Label {
            anchors.centerIn: parent
            text: qsTr("⚙ CAT — Radio Settings")
            color: textColor
            font.pixelSize: 14
            font.bold: true
        }
    }

    contentItem: ColumnLayout {
        spacing: 12
        anchors.margins: 16

        // Rig model
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Radio:"; color: textSec; Layout.preferredWidth: 100 }
            DecoComboBox {
                id: rigCombo
                Layout.fillWidth: true
                model: bridge.catManager.rigList
                property string filterText: ""
                property var filteredRigList: {
                    var src = bridge.catManager ? bridge.catManager.rigList : []
                    var q = filterText.trim().toLowerCase()
                    if (q.length === 0)
                        return src

                    var terms = q.split(/\s+/)
                    var out = []
                    for (var i = 0; i < src.length; ++i) {
                        var name = String(src[i])
                        var haystack = name.toLowerCase()
                        var match = true
                        for (var t = 0; t < terms.length; ++t) {
                            if (terms[t].length > 0 && haystack.indexOf(terms[t]) < 0) {
                                match = false
                                break
                            }
                        }
                        if (match)
                            out.push(name)
                    }
                    return out
                }
                function chooseRig(name) {
                    var idx = model.indexOf(name)
                    if (idx >= 0)
                        currentIndex = idx
                    bridge.catManager.rigName = name
                    rigComboPopup.close()
                }
                currentIndex: model.indexOf(bridge.catManager.rigName)
                contentItem: Text {
                    leftPadding: 8
                    text: rigCombo.displayText
                    color: textColor
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: bgLight; border.color: accent; border.width: 1; radius: 4
                }
                popup: Popup {
                    id: rigComboPopup
                    width: rigCombo.width
                    height: Math.min(360, 48 + Math.max(34, rigComboPopupList.contentHeight))
                    focus: true
                    onOpened: {
                        rigCombo.filterText = ""
                        rigSearchField.forceActiveFocus()
                    }
                    contentItem: Column {
                        width: rigComboPopup.width
                        spacing: 6

                        DecoTextField {
                            id: rigSearchField
                            x: 8
                            width: parent.width - 16
                            height: 34
                            placeholderText: qsTr("Search radio...")
                            text: rigCombo.filterText
                            selectByMouse: true
                            color: textColor
                            placeholderTextColor: textSec
                            font.pixelSize: 12
                            leftPadding: 10
                            rightPadding: 10
                            onTextChanged: rigCombo.filterText = text
                            background: Rectangle {
                                color: bgLight
                                border.color: accent
                                border.width: 1
                                radius: 4
                            }
                        }

                        ListView {
                            id: rigComboPopupList
                            x: 8
                            width: parent.width - 16
                            height: rigComboPopup.height - rigSearchField.height - 22
                            clip: true
                            model: rigCombo.filteredRigList
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick
                            interactive: true
                            focus: true
                            delegate: ItemDelegate {
                                width: rigComboPopupList.width
                                height: 32
                                highlighted: modelData === bridge.catManager.rigName
                                contentItem: Text {
                                    text: modelData
                                    color: parent.highlighted ? accent : textColor
                                    font.pixelSize: 11
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                                background: Rectangle {
                                    color: hovered || parent.highlighted ? bgLight : bgColor
                                }
                                onClicked: rigCombo.chooseRig(modelData)
                            }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                        }
                    }
                    background: Rectangle { color: bgColor; border.color: accent; radius: 4 }
                }
            }
        }

        // Serial port
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Serial port:"); color: textSec; Layout.preferredWidth: 100 }
            DecoComboBox {
                id: portCombo
                Layout.fillWidth: true
                model: bridge.catManager.portList
                editable: true
                currentIndex: model.indexOf(bridge.catManager.serialPort)
                editText: bridge.catManager.serialPort
                onActivated: if (bridge.catManager.serialPort !== editText)
                                 bridge.catManager.serialPort = editText
                onEditTextChanged: if (bridge.catManager.serialPort !== editText)
                                       bridge.catManager.serialPort = editText
                contentItem: DecoTextField {
                    leftPadding: 8
                    text: portCombo.editText
                    color: textColor
                    font.pixelSize: 12
                    background: Rectangle { color: "transparent" }
                    // 1.0.352 fix: onTextEdited (solo input utente) invece di onTextChanged,
                    // che scattava anche sull'update del binding text: portCombo.editText
                    // rompendolo. Allineato a RigControlDialog.
                    onTextEdited: portCombo.editText = text
                }
                background: Rectangle {
                    color: bgLight; border.color: accent; border.width: 1; radius: 4
                }
                delegate: ItemDelegate {
                    width: portCombo.width
                    contentItem: Text { text: modelData; color: textColor; font.pixelSize: 12 }
                    background: Rectangle { color: hovered ? bgLight : bgColor }
                }
                popup: Popup {
                    width: portCombo.width
                    height: Math.min(contentItem.implicitHeight, 200)
                    contentItem: ListView {
                        clip: true
                        model: portCombo.delegateModel
                        ScrollBar.vertical: ScrollBar {}
                    }
                    background: Rectangle { color: bgColor; border.color: accent; radius: 4 }
                }
                Button {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28; height: 28
                    text: "↻"
                    ToolTip.text: qsTr("Refresh ports")
                    ToolTip.delay: 500
                    ToolTip.visible: hovered
                    onClicked: bridge.catManager.refreshPorts()
                    background: Rectangle { color: "transparent" }
                    contentItem: Text { text: "↻"; color: accent; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter }
                }
            }
        }

        // Baud rate
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Speed (baud):"); color: textSec; Layout.preferredWidth: 100 }
            DecoComboBox {
                id: baudCombo
                Layout.fillWidth: true
                model: bridge.catManager.baudList
                currentIndex: model.indexOf(String(bridge.catManager.baudRate).trim())
                onActivated: {
                    var v = parseInt(currentText)
                    if (!isNaN(v) && bridge.catManager.baudRate !== v)
                        bridge.catManager.baudRate = v
                }
                contentItem: Text {
                    leftPadding: 8
                    text: baudCombo.currentIndex >= 0 ? baudCombo.displayText : String(bridge.catManager.baudRate).trim()
                    color: textColor
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: bgLight; border.color: accent; border.width: 1; radius: 4
                }
                delegate: ItemDelegate {
                    width: baudCombo.width
                    contentItem: Text { text: modelData; color: textColor; font.pixelSize: 12 }
                    background: Rectangle { color: hovered ? bgLight : bgColor }
                }
            }
        }

        // PTT method
        RowLayout {
            Layout.fillWidth: true
            Label { text: "PTT:"; color: textSec; Layout.preferredWidth: 100 }
            DecoComboBox {
                id: pttCombo
                Layout.fillWidth: true
                model: ["CAT","DTR","RTS","VOX"]
                currentIndex: model.indexOf(bridge.catManager.pttMethod)
                onActivated: if (bridge.catManager.pttMethod !== currentText)
                                 bridge.catManager.pttMethod = currentText
                contentItem: Text {
                    leftPadding: 8
                    text: pttCombo.displayText
                    color: textColor
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: bgLight; border.color: accent; border.width: 1; radius: 4
                }
                delegate: ItemDelegate {
                    width: pttCombo.width
                    contentItem: Text { text: modelData; color: textColor; font.pixelSize: 12 }
                    background: Rectangle { color: hovered ? bgLight : bgColor }
                }
            }
        }

        // Auto-connect / Auto-start
        RowLayout {
            Layout.fillWidth: true
            spacing: 20
            CheckBox {
                id: autoConnectCheck
                text: qsTr("Auto-connect CAT on startup")
                checked: bridge.catManager.catAutoConnect
                onCheckedChanged: if (bridge.catManager.catAutoConnect !== checked)
                                      bridge.catManager.catAutoConnect = checked
                contentItem: Text {
                    leftPadding: autoConnectCheck.indicator.width + 6
                    text: autoConnectCheck.text
                    color: textColor
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }
                indicator: Rectangle {
                    implicitWidth: 18; implicitHeight: 18; radius: 3
                    x: autoConnectCheck.leftPadding
                    y: (autoConnectCheck.height - height) / 2
                    color: autoConnectCheck.checked ? accent : bgLight
                    border.color: accent
                    Text {
                        anchors.centerIn: parent
                        text: "✓"; color: "white"; font.pixelSize: 12
                        visible: autoConnectCheck.checked
                    }
                }
            }
            CheckBox {
                id: autoStartCheck
                text: qsTr("Start audio on connect")
                checked: bridge.catManager.audioAutoStart
                onCheckedChanged: if (bridge.catManager.audioAutoStart !== checked)
                                      bridge.catManager.audioAutoStart = checked
                contentItem: Text {
                    leftPadding: autoStartCheck.indicator.width + 6
                    text: autoStartCheck.text
                    color: textColor
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }
                indicator: Rectangle {
                    implicitWidth: 18; implicitHeight: 18; radius: 3
                    x: autoStartCheck.leftPadding
                    y: (autoStartCheck.height - height) / 2
                    color: autoStartCheck.checked ? accentGreen : bgLight
                    border.color: accentGreen
                    Text {
                        anchors.centerIn: parent
                        text: "✓"; color: "black"; font.pixelSize: 12
                        visible: autoStartCheck.checked
                    }
                }
            }
        }

        // Status row
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                width: 10; height: 10; radius: 5
                color: bridge.catConnected ? accentGreen : "#f44336"
            }
            Label {
                text: bridge.catConnected
                      ? "Connesso: " + bridge.catRigName + "  " + (bridge.frequency / 1e6).toFixed(6) + " MHz"
                      : qsTr("Not connected")
                color: bridge.catConnected ? accentGreen : "#f44336"
                font.pixelSize: 11
            }
        }

        // Connect/Disconnect buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 12
            Button {
                text: bridge.catConnected ? qsTr("Disconnect") : qsTr("Connect")
                enabled: !bridge.catConnected
                onClicked: {
                    bridge.catManager.rigName        = rigCombo.currentText
                    bridge.catManager.serialPort     = portCombo.editText
                    bridge.catManager.baudRate       = parseInt(baudCombo.currentText)
                    bridge.catManager.pttMethod      = pttCombo.currentText
                    bridge.catManager.catAutoConnect = autoConnectCheck.checked
                    bridge.catManager.audioAutoStart = autoStartCheck.checked
                    bridge.catManager.saveSettings()
                    bridge.catManager.connectRig()
                }
                background: Rectangle {
                    color: parent.enabled ? "#1A4A1A" : "#222"
                    border.color: parent.enabled ? accentGreen : "#444"
                    radius: 6
                }
                contentItem: Text { text: parent.text; color: parent.enabled ? accentGreen : "#666"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 12 }
            }
            Button {
                text: qsTr("Disconnect")
                enabled: bridge.catConnected
                onClicked: bridge.catManager.disconnectRig()
                background: Rectangle {
                    color: parent.enabled ? "#4A1A1A" : "#222"
                    border.color: parent.enabled ? "#f44336" : "#444"
                    radius: 6
                }
                contentItem: Text { text: parent.text; color: parent.enabled ? "#f44336" : "#666"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 12 }
            }
        }
    }

    onAccepted: {
        bridge.catManager.rigName        = rigCombo.currentText
        bridge.catManager.serialPort     = portCombo.editText
        bridge.catManager.baudRate       = parseInt(baudCombo.currentText)
        bridge.catManager.pttMethod      = pttCombo.currentText
        bridge.catManager.catAutoConnect = autoConnectCheck.checked
        bridge.catManager.audioAutoStart = autoStartCheck.checked
        bridge.catManager.saveSettings()
    }
}
