import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// QSYDialog — adattato per bridge.* (C15)
Dialog {
    id: qsyDialog
    title: "QSY"
    modal: true
    width: 340
    height: 380
    anchors.centerIn: parent

    // Deriva banda corrente dalla frequenza bridge
    property string currentMode: bridge.mode
    property string currentBand: {
        var f = bridge.frequency / 1000.0  // Hz → kHz
        if (f < 2000)   return "160m"
        if (f < 4500)   return "80m"
        if (f < 5900)   return "60m"
        if (f < 7500)   return "40m"
        if (f < 11000)  return "30m"
        if (f < 15000)  return "20m"
        if (f < 18500)  return "17m"
        if (f < 22000)  return "15m"
        if (f < 25500)  return "12m"
        if (f < 30000)  return "10m"
        if (f < 54000)  return "6m"
        if (f < 200000) return "2m"
        return "70cm"
    }

    readonly property var qsyFrequencies: ({
        "160m": [{"dial": 1840.0, "mode": "FT8"}, {"dial": 1840.0, "mode": "FT4"}],
        "80m":  [{"dial": 3573.0, "mode": "FT8"}, {"dial": 3575.0, "mode": "FT4"}, {"dial": 3568.0, "mode": "JT65"}],
        "60m":  [{"dial": 5357.0, "mode": "FT8"}],
        "40m":  [{"dial": 7074.0, "mode": "FT8"}, {"dial": 7047.5, "mode": "FT4"}, {"dial": 7076.0, "mode": "JT65"}],
        "30m":  [{"dial": 10136.0, "mode": "FT8"}, {"dial": 10140.0, "mode": "FT4"}],
        "20m":  [{"dial": 14074.0, "mode": "FT8"}, {"dial": 14080.0, "mode": "FT4"}, {"dial": 14076.0, "mode": "JT65"}],
        "17m":  [{"dial": 18100.0, "mode": "FT8"}, {"dial": 18104.0, "mode": "FT4"}],
        "15m":  [{"dial": 21074.0, "mode": "FT8"}, {"dial": 21140.0, "mode": "FT4"}, {"dial": 21076.0, "mode": "JT65"}],
        "12m":  [{"dial": 24915.0, "mode": "FT8"}, {"dial": 24919.0, "mode": "FT4"}],
        "10m":  [{"dial": 28074.0, "mode": "FT8"}, {"dial": 28180.0, "mode": "FT4"}],
        "6m":   [{"dial": 50313.0, "mode": "FT8"}, {"dial": 50318.0, "mode": "FT4"}],
        "2m":   [{"dial": 144174.0, "mode": "FT8"}, {"dial": 144170.0, "mode": "FT4"}],
        "70cm": [{"dial": 432065.0, "mode": "FT8"}, {"dial": 432065.0, "mode": "FT4"}]
    })

    background: Rectangle {
        color: Qt.rgba(12/255, 12/255, 22/255, 0.98)
        border.color: "#00BCD4"
        border.width: 1
        radius: 10
    }

    contentItem: ColumnLayout {
        spacing: 8

        // Header info
        Text {
            Layout.leftMargin: 12
            Layout.topMargin: 8
            text: "Banda corrente: " + currentBand + "  •  Modo: " + currentMode
            font.family: "Consolas"
            font.pixelSize: 11
            color: "#90A4AE"
        }

        Text {
            Layout.leftMargin: 12
            text: "FREQUENZE SUGGERITE"
            font.family: "Consolas"
            font.pixelSize: 10
            font.letterSpacing: 2
            color: "#546E7A"
        }

        ListView {
            id: freqList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            clip: true
            spacing: 2

            model: {
                var result = []
                var bands = Object.keys(qsyFrequencies)
                for (var b = 0; b < bands.length; b++) {
                    var band = bands[b]
                    var freqs = qsyFrequencies[band]
                    for (var f = 0; f < freqs.length; f++) {
                        result.push({
                            "band":      band,
                            "dial":      freqs[f].dial,
                            "mode":      freqs[f].mode,
                            "isCurrent": (band === currentBand && freqs[f].mode === currentMode)
                        })
                    }
                }
                return result
            }

            delegate: AbstractButton {
                width: freqList.width
                height: 32

                onClicked: {
                    // C15: usa bridge.qsyTo() che gestisce frequenza + modo + CAT
                    bridge.qsyTo(modelData.dial * 1000.0, modelData.mode)
                    qsyDialog.close()
                }

                background: Rectangle {
                    color: modelData.isCurrent
                           ? Qt.rgba(0, 0.74, 0.84, 0.15)
                           : (parent.hovered ? Qt.rgba(1,1,1,0.06) : Qt.rgba(1,1,1,0.02))
                    border.color: modelData.isCurrent ? "#00BCD4" : Qt.rgba(1,1,1,0.08)
                    border.width: modelData.isCurrent ? 1 : 0
                    radius: 5
                }

                contentItem: RowLayout {
                    spacing: 8

                    Text {
                        text: modelData.band
                        font.family: "Consolas"
                        font.pixelSize: 11
                        font.bold: true
                        color: modelData.isCurrent ? "#00BCD4" : "#ECEFF1"
                        Layout.preferredWidth: 46
                    }

                    Text {
                        text: modelData.dial.toFixed(1) + " kHz"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: "#00BCD4"
                        Layout.fillWidth: true
                    }

                    Text {
                        text: modelData.mode
                        font.family: "Consolas"
                        font.pixelSize: 10
                        font.bold: true
                        color: modelData.mode === currentMode ? "#69F0AE" : "#78909C"
                        Layout.preferredWidth: 36
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }

        // Pulsante chiudi
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 8
            text: "Chiudi"
            onClicked: qsyDialog.close()
            background: Rectangle {
                color: parent.hovered ? Qt.rgba(0, 0.74, 0.84, 0.2) : Qt.rgba(1,1,1,0.05)
                border.color: "#546E7A"; radius: 5
            }
            contentItem: Text {
                text: parent.text; color: "#90A4AE"
                font.family: "Consolas"; font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
