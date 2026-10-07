import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassPanel {
    id: panel

    title: qsTr("STATION AND SAFETY")

    GridLayout {
        anchors.fill: parent
        columns: 2
        rowSpacing: 10
        columnSpacing: 14

        Text {
            text: qsTr("Callsign")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        Text {
            Layout.fillWidth: true
            text: rotor.callsign.length > 0 ? rotor.callsign : "-"
            color: RotorTheme.textPrimary
            font.pixelSize: RotorTheme.fontBody
            font.family: RotorTheme.monoFamily
        }

        Text {
            text: qsTr("QTH locator")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        Text {
            Layout.fillWidth: true
            text: rotor.locator.length > 0 ? rotor.locator : "-"
            color: RotorTheme.textPrimary
            font.pixelSize: RotorTheme.fontBody
            font.family: RotorTheme.monoFamily
        }

        Text {
            text: qsTr("Beamwidth: %1°").arg(rotor.beamwidth.toFixed(0))
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        Slider {
            id: beamSlider

            Layout.fillWidth: true
            from: 5
            to: 180
            stepSize: 1
            value: rotor.beamwidth
            onMoved: rotor.setSetting("beamwidth", value)

            background: Rectangle {
                x: beamSlider.leftPadding
                y: beamSlider.topPadding + beamSlider.availableHeight / 2 - height / 2
                width: beamSlider.availableWidth
                height: 4
                radius: 2
                color: RotorTheme.bgDeep

                Rectangle {
                    width: beamSlider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: RotorTheme.primary
                }
            }

            handle: Rectangle {
                x: beamSlider.leftPadding + beamSlider.visualPosition * (beamSlider.availableWidth - width)
                y: beamSlider.topPadding + beamSlider.availableHeight / 2 - height / 2
                width: 16
                height: 16
                radius: 8
                color: beamSlider.pressed ? RotorTheme.secondary : RotorTheme.primary
                border.color: RotorTheme.bgDeep
                border.width: 2
            }
        }

        Text {
            text: qsTr("Azimuth limits")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FieldInput {
                Layout.fillWidth: true
                text: rotor.azMin.toFixed(0)
                validator: DoubleValidator { bottom: -180; top: 720 }
                onEditingFinished: rotor.setLimit("az_min", parseFloat(text))
            }

            Text {
                text: "→"
                color: RotorTheme.textDim
            }

            FieldInput {
                Layout.fillWidth: true
                text: rotor.azMax.toFixed(0)
                validator: DoubleValidator { bottom: -180; top: 720 }
                onEditingFinished: rotor.setLimit("az_max", parseFloat(text))
            }
        }

        Text {
            text: qsTr("Park position")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        FieldInput {
            Layout.fillWidth: true
            text: rotor.parkAz.toFixed(0)
            validator: DoubleValidator { bottom: 0; top: 360 }
            onEditingFinished: rotor.setSetting("park_az", parseFloat(text))
        }

        Text {
            text: qsTr("Stop if the link drops")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        Switch {
            id: guardSwitch

            checked: rotor.stopOnClientLoss
            onToggled: rotor.setSetting("stop_on_client_loss", checked)

            indicator: Rectangle {
                implicitWidth: 44
                implicitHeight: 22
                x: guardSwitch.leftPadding
                y: guardSwitch.topPadding + guardSwitch.availableHeight / 2 - height / 2
                radius: 11
                color: guardSwitch.checked ? Qt.rgba(0.20, 0.83, 0.60, 0.30) : RotorTheme.bgDeep
                border.color: guardSwitch.checked ? RotorTheme.accent : RotorTheme.borderSoft
                border.width: 1

                Rectangle {
                    x: guardSwitch.checked ? parent.width - width - 3 : 3
                    y: 3
                    width: 16
                    height: 16
                    radius: 8
                    color: guardSwitch.checked ? RotorTheme.accent : RotorTheme.textDim

                    Behavior on x {
                        XAnimator { duration: 120 }
                    }
                }
            }

            contentItem: Text {
                text: guardSwitch.checked ? qsTr("on") : qsTr("off")
                color: RotorTheme.textSecondary
                font.pixelSize: RotorTheme.fontSmall
                leftPadding: guardSwitch.indicator.width + 10
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            text: qsTr("Stations heard by Decodium")
            color: RotorTheme.textSecondary
            font.pixelSize: RotorTheme.fontBody
        }

        Switch {
            id: spotsSwitch

            checked: rotor.spotsEnabled
            onToggled: rotor.setSetting("spots_enabled", checked)

            contentItem: Text {
                text: spotsSwitch.checked ? qsTr("on") : qsTr("off")
                color: RotorTheme.textSecondary
                font.pixelSize: RotorTheme.fontSmall
                leftPadding: spotsSwitch.indicator.width + 10
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            Layout.columnSpan: 2
            Layout.fillHeight: true
        }

        Text {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Callsign and locator are those set in Decodium. Changes are saved immediately.")
            color: RotorTheme.textDim
            font.pixelSize: RotorTheme.fontSmall
            wrapMode: Text.WordWrap
        }
    }
}
