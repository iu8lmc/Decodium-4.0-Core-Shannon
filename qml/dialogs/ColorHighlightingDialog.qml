import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ColorHighlightingDialog — adattato per bridge.* (B7)
Dialog {
    id: colorDialog
    title: "Color Highlighting"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 400
    anchors.centerIn: parent

    background: Rectangle {
        color: Qt.rgba(18/255, 18/255, 28/255, 0.98)
        border.color: "#00BCD4"
        border.width: 1
        radius: 10
    }

    // Modello colori — legge e scrive bridge.*
    property var colorModel: [
        { label: "CQ Calls",    prop: "colorCQ",       defaultColor: "#33FF33" },
        { label: "My Call",     prop: "colorMyCall",   defaultColor: "#FF5555" },
        { label: "DX Entity",   prop: "colorDXEntity", defaultColor: "#FFAA33" },
        { label: "73 / RR73",   prop: "color73",       defaultColor: "#5599FF" },
        { label: "B4 (Worked)", prop: "colorB4",       defaultColor: "#888888" }
    ]

    contentItem: Column {
        spacing: 10
        padding: 16

        Repeater {
            model: colorDialog.colorModel

            RowLayout {
                width: colorDialog.contentWidth - 32
                spacing: 12

                Text {
                    text: modelData.label
                    font.family: "Consolas"
                    font.pixelSize: 12
                    color: "#ECEFF1"
                    Layout.preferredWidth: 110
                }

                // Anteprima colore (click per modificare)
                Rectangle {
                    width: 36; height: 24; radius: 4
                    color: bridge[modelData.prop] || modelData.defaultColor
                    border.color: Qt.rgba(1,1,1,0.3)
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            colorPickerDialog.targetProp  = modelData.prop
                            colorPickerDialog.currentHex  = bridge[modelData.prop] || modelData.defaultColor
                            colorPickerInput.text         = colorPickerDialog.currentHex
                            colorPickerDialog.open()
                        }
                    }
                }

                // Etichetta colore corrente
                Text {
                    text: bridge[modelData.prop] || modelData.defaultColor
                    font.family: "Consolas"
                    font.pixelSize: 11
                    color: "#90A4AE"
                }

                Item { Layout.fillWidth: true }
            }
        }

        // B4 Strikethrough toggle
        RowLayout {
            spacing: 12
            width: colorDialog.contentWidth - 32

            Text {
                text: "B4 Strikethrough:"
                font.family: "Consolas"
                font.pixelSize: 12
                color: "#ECEFF1"
                Layout.preferredWidth: 110
            }

            Switch {
                checked: bridge.b4Strikethrough
                onToggled: bridge.b4Strikethrough = checked
            }
        }

        // Alert sounds toggle (B8)
        RowLayout {
            spacing: 12
            width: colorDialog.contentWidth - 32

            Text {
                text: "Alert Sounds:"
                font.family: "Consolas"
                font.pixelSize: 12
                color: "#ECEFF1"
                Layout.preferredWidth: 110
            }

            Switch {
                checked: bridge.alertSoundsEnabled
                onToggled: bridge.alertSoundsEnabled = checked
            }
        }
    }

    // Color picker sub-dialog
    Dialog {
        id: colorPickerDialog
        title: "Scegli Colore"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 280
        anchors.centerIn: parent

        property string targetProp: ""
        property string currentHex: "#FFFFFF"

        background: Rectangle {
            color: Qt.rgba(18/255, 18/255, 28/255, 0.98)
            border.color: "#00BCD4"
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 10
            padding: 12

            Text {
                text: "Colore esadecimale (#RRGGBB):"
                font.family: "Consolas"
                font.pixelSize: 11
                color: "#B0BEC5"
            }

            TextField {
                id: colorPickerInput
                width: 240
                text: colorPickerDialog.currentHex
                placeholderText: "#RRGGBB"
                font.family: "Consolas"
                font.pixelSize: 13
                background: Rectangle {
                    color: Qt.rgba(1,1,1,0.08)
                    border.color: "#546E7A"
                    radius: 4
                }
                color: "#ECEFF1"
            }

            // Anteprima
            Rectangle {
                width: 240; height: 32; radius: 4
                color: colorPickerInput.text.match(/^#[0-9A-Fa-f]{6}$/)
                       ? colorPickerInput.text : "transparent"
                border.color: Qt.rgba(1,1,1,0.2)
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: colorPickerInput.text.match(/^#[0-9A-Fa-f]{6}$/) ? "" : "colore non valido"
                    color: "#f44336"; font.pixelSize: 10
                }
            }
        }

        onAccepted: {
            if (colorPickerInput.text.match(/^#[0-9A-Fa-f]{6}$/) && targetProp !== "") {
                bridge[targetProp] = colorPickerInput.text
                bridge.saveSettings()
            }
        }
    }

    onAccepted: bridge.saveSettings()
}
