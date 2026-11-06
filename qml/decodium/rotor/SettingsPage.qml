import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: page

    spacing: RotorTheme.spacing

    SettingsPanel {
        Layout.fillHeight: true
        Layout.preferredWidth: 620
        Layout.maximumWidth: 760
        Layout.minimumWidth: 420
    }

    Flickable {
        id: rightColumn

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: 320
        contentWidth: width
        contentHeight: connection.height + RotorTheme.spacing + 260
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        GlassPanel {
            id: connection

            width: rightColumn.width
            height: 470
            title: qsTr("CONTROL BOX CONNECTION")

            GridLayout {
                anchors.fill: parent
                columns: 2
                rowSpacing: 9
                columnSpacing: 12

                Text {
                    text: qsTr("Rotator")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                RotorSwitch {
                    checked: rotor.enabled
                    textOn: qsTr("on")
                    textOff: qsTr("off")
                    onToggled: rotor.setSetting("enabled", checked)
                }

                Text {
                    text: qsTr("Simulator")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                RotorSwitch {
                    checked: rotor.simulate
                    textOn: qsTr("simulated rotator")
                    textOff: qsTr("real control box")
                    onToggled: rotor.setSetting("simulate", checked)
                }

                Text {
                    text: qsTr("Serial port")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    RotorCombo {
                        id: portCombo

                        Layout.fillWidth: true
                        enabled: !rotor.simulate
                        model: rotor.serialPorts
                        currentIndex: Math.max(0, rotor.serialPorts.indexOf(rotor.serialPort))
                        displayText: rotor.serialPort.length > 0 ? rotor.serialPort : qsTr("choose…")
                        onActivated: rotor.setSetting("port", currentText)
                    }

                    CommandButton {
                        text: "⟳"
                        onClicked: rotor.refreshSerialPorts()
                    }
                }

                Text {
                    text: qsTr("Model")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                RotorCombo {
                    id: modelCombo

                    Layout.fillWidth: true
                    model: rotor.modelChoices
                    textRole: "label"
                    currentIndex: {
                        for (let i = 0; i < rotor.modelChoices.length; ++i)
                            if (rotor.modelChoices[i].key === rotor.modelKey)
                                return i
                        return 0
                    }
                    onActivated: rotor.setSetting("model", rotor.modelChoices[currentIndex].key)
                }

                Text {
                    text: qsTr("Speed")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                Text {
                    text: qsTr("%1 baud, 8N1").arg(rotor.baudrate)
                    color: RotorTheme.textPrimary
                    font.pixelSize: RotorTheme.fontBody
                    font.family: RotorTheme.monoFamily
                }

                Text {
                    text: qsTr("Status")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                Text {
                    Layout.fillWidth: true
                    text: !rotor.enabled ? qsTr("off")
                          : rotor.connected ? qsTr("connected") + " · " + rotor.modelLabel
                          : (rotor.errorText.length > 0 ? rotor.errorText : qsTr("missing"))
                    color: rotor.connected ? RotorTheme.accent : RotorTheme.textPrimary
                    font.pixelSize: RotorTheme.fontBody
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: 1
                    color: RotorTheme.borderSoft
                }

                Text {
                    text: qsTr("Network access")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                RotorSwitch {
                    checked: rotor.networkEnabled
                    textOn: qsTr("phone, web and station software")
                    textOff: qsTr("closed")
                    onToggled: rotor.setSetting("network_enabled", checked)
                }

                Text {
                    text: qsTr("Token")
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                }

                FieldInput {
                    Layout.fillWidth: true
                    text: rotor.token
                    placeholderText: qsTr("empty = open access")
                    onEditingFinished: rotor.setSetting("token", text)
                }

                Item {
                    Layout.columnSpan: 2
                    Layout.fillHeight: true
                }

                Text {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: qsTr("Port, model and network apply immediately. The rotator stays off until you turn it on.")
                    color: RotorTheme.textDim
                    font.pixelSize: RotorTheme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        NetworkPanel {
            y: connection.height + RotorTheme.spacing
            width: rightColumn.width
            height: 260
        }
    }
}
