import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassPanel {
    id: panel

    title: qsTr("MEMORIES")

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: list

                model: rotor.presets
                spacing: 4
                reuseItems: true

                delegate: Rectangle {
                    id: row

                    required property var modelData

                    width: ListView.view.width
                    height: 40
                    radius: 8
                    color: hover.hovered ? RotorTheme.bgHeader : RotorTheme.bgElevated
                    border.color: RotorTheme.borderSoft
                    border.width: 1

                    HoverHandler { id: hover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 6
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            color: RotorTheme.textPrimary
                            font.pixelSize: RotorTheme.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            // Un asse assente arriva come null dal gateway e
                            // come undefined una volta convertito: valgono
                            // entrambi "questa memoria non ha elevazione".
                            readonly property bool hasElevation:
                                row.modelData.el !== null && row.modelData.el !== undefined

                            text: !hasElevation
                                  ? row.modelData.az.toFixed(1) + "°"
                                  : qsTr("%1° / %2°").arg(row.modelData.az.toFixed(1))
                                                     .arg(row.modelData.el.toFixed(1))
                            color: RotorTheme.textSecondary
                            font.pixelSize: RotorTheme.fontSmall
                            font.family: RotorTheme.monoFamily
                        }

                        CommandButton {
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 28
                            text: qsTr("GO")
                            kind: CommandButton.Kind.Primary
                            onClicked: rotor.recallPreset(row.modelData.name)
                        }

                        CommandButton {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 28
                            text: "×"
                            onClicked: rotor.deletePreset(row.modelData.name)
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: rotor.presets.length === 0
            text: qsTr("No memories: name the current heading and save it.")
            color: RotorTheme.textDim
            font.pixelSize: RotorTheme.fontSmall
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            FieldInput {
                id: nameField

                Layout.fillWidth: true
                placeholderText: qsTr("Memory name")
                onAccepted: panel.store()
            }

            CommandButton {
                Layout.preferredWidth: 110
                text: qsTr("SAVE HERE")
                enabled: rotor.hasPosition && nameField.text.trim().length > 0
                onClicked: panel.store()
            }
        }
    }

    function store() {
        if (nameField.text.trim().length === 0)
            return;
        rotor.savePresetHere(nameField.text);
        nameField.clear();
    }
}
