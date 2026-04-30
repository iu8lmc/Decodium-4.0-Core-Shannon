import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ColorHighlightingDialog — palette WSJT-X (B7)
Dialog {
    id: colorDialog
    title: "Color Highlighting"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 460
    height: 620
    anchors.centerIn: parent

    background: Rectangle {
        color: Qt.rgba(18/255, 18/255, 28/255, 0.98)
        border.color: "#00BCD4"
        border.width: 1
        radius: 10
    }

    // Modello colori — ordine WSJT-X (priorità decrescente, salvo CQ/B4 in coda)
    property var colorModel: [
        { label: "Messaggio Trasmesso",   prop: "colorTxMessage",        defaultColor: "#FFFF00" },
        { label: "Mio Nominativo",        prop: "colorMyCall",           defaultColor: "#FF5555" },
        { label: "Nuovo DXCC in Banda",   prop: "colorNewDxccBand",      defaultColor: "#F8AAD0" },
        { label: "Nuovo DXCC",            prop: "colorNewDxcc",          defaultColor: "#FF00FF" },
        { label: "Nuovo Continente Banda",prop: "colorNewContinentBand", defaultColor: "#F5B7C7" },
        { label: "Nuovo Continente",      prop: "colorNewContinent",     defaultColor: "#E91E63" },
        { label: "Nuova Zona CQ Banda",   prop: "colorNewCqZoneBand",    defaultColor: "#F5DDA0" },
        { label: "Nuova Zona CQ",         prop: "colorNewCqZone",        defaultColor: "#F0A030" },
        { label: "Nuova Zona ITU Banda",  prop: "colorNewItuZoneBand",   defaultColor: "#D4E89F" },
        { label: "Nuova Zona ITU",        prop: "colorNewItuZone",       defaultColor: "#9ACD32" },
        { label: "Nuova Griglia Banda",   prop: "colorNewGridBand",      defaultColor: "#FFCAA0" },
        { label: "Nuova Griglia",         prop: "colorNewGrid",          defaultColor: "#FF8C00" },
        { label: "Nuovo Nominativo Banda",prop: "colorNewCallBand",      defaultColor: "#B5E8E8" },
        { label: "Nuovo Nominativo",      prop: "colorNewCall",          defaultColor: "#00E0E0" },
        { label: "Utente LoTW",           prop: "colorLotwUser",         defaultColor: "#FFFFFF" },
        { label: "CQ nel messaggio",      prop: "colorCQ",               defaultColor: "#33FF33" },
        { label: "DX Entity",             prop: "colorDXEntity",         defaultColor: "#FFAA33" },
        { label: "73 / RR73",             prop: "color73",               defaultColor: "#5599FF" },
        { label: "B4 (Worked)",           prop: "colorB4",               defaultColor: "#888888" }
    ]

    contentItem: Flickable {
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: contentColumn
            spacing: 6
            padding: 16
            width: parent.width

            Repeater {
                model: colorDialog.colorModel

                RowLayout {
                    width: contentColumn.width - 32
                    spacing: 12

                    Text {
                        text: modelData.label
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: "#ECEFF1"
                        Layout.preferredWidth: 180
                        elide: Text.ElideRight
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
                width: contentColumn.width - 32

                Text {
                    text: "B4 Strikethrough:"
                    font.family: "Consolas"
                    font.pixelSize: 12
                    color: "#ECEFF1"
                    Layout.preferredWidth: 180
                }

                Switch {
                    checked: bridge.b4Strikethrough
                    onToggled: bridge.b4Strikethrough = checked
                }
            }

            // Alert sounds toggle (B8)
            RowLayout {
                spacing: 12
                width: contentColumn.width - 32

                Text {
                    text: "Alert Sounds:"
                    font.family: "Consolas"
                    font.pixelSize: 12
                    color: "#ECEFF1"
                    Layout.preferredWidth: 180
                }

                Switch {
                    checked: bridge.alertSoundsEnabled
                    onToggled: bridge.alertSoundsEnabled = checked
                }
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
