import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium

Rectangle {
    id: msgAvgPanel
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    visible: false

    property var avgModel: ListModel {}
    property int avgCount: 0

    function addAveragedResult(utc, snr, dt, freq, message, nAvg) {
        avgModel.insert(0, {
            utc: utc,
            snr: snr,
            dt: dt,
            freq: freq,
            message: message,
            nAvg: nAvg
        });
        avgCount++;
        if (avgModel.count > 200) avgModel.remove(avgModel.count - 1);
    }

    function clear() {
        avgModel.clear();
        avgCount = 0;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingSmall

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "MESSAGE AVERAGING"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTiny
                font.bold: true
                font.letterSpacing: 2
                color: Theme.accent
            }

            Item { Layout.fillWidth: true }

            Text {
                text: avgCount + " avg"
                font.family: Theme.digitalFont
                font.pixelSize: Theme.fontTiny
                color: Theme.textSecondary
            }

            AbstractButton {
                implicitWidth: 40; implicitHeight: 18
                onClicked: clear()
                contentItem: Text {
                    text: "Clear"
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                    color: Theme.alertRed
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent" }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        // Column headers
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Text { text: "UTC"; font.pixelSize: Theme.fontTiny; color: Theme.textDim; Layout.preferredWidth: 50 }
            Text { text: "SNR"; font.pixelSize: Theme.fontTiny; color: Theme.textDim; Layout.preferredWidth: 35 }
            Text { text: "DT"; font.pixelSize: Theme.fontTiny; color: Theme.textDim; Layout.preferredWidth: 40 }
            Text { text: "Freq"; font.pixelSize: Theme.fontTiny; color: Theme.textDim; Layout.preferredWidth: 50 }
            Text { text: "Avg"; font.pixelSize: Theme.fontTiny; color: Theme.textDim; Layout.preferredWidth: 25 }
            Text { text: "Message"; font.pixelSize: Theme.fontTiny; color: Theme.textDim; Layout.fillWidth: true }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ListView {
            id: avgList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: avgModel

            delegate: Rectangle {
                width: avgList.width
                height: 20
                color: avgMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    spacing: 4

                    Text {
                        text: model.utc
                        font.family: Theme.digitalFont
                        font.pixelSize: Theme.fontTiny
                        color: Theme.textSecondary
                        Layout.preferredWidth: 50
                    }
                    Text {
                        text: model.snr
                        font.family: Theme.digitalFont
                        font.pixelSize: Theme.fontTiny
                        color: model.snr >= 0 ? "#4CAF50" : (model.snr >= -15 ? "#FF9800" : Theme.alertRed)
                        Layout.preferredWidth: 35
                    }
                    Text {
                        text: model.dt.toFixed(1)
                        font.family: Theme.digitalFont
                        font.pixelSize: Theme.fontTiny
                        color: Theme.textSecondary
                        Layout.preferredWidth: 40
                    }
                    Text {
                        text: model.freq
                        font.family: Theme.digitalFont
                        font.pixelSize: Theme.fontTiny
                        color: Theme.textSecondary
                        Layout.preferredWidth: 50
                    }
                    Text {
                        text: model.nAvg
                        font.family: Theme.digitalFont
                        font.pixelSize: Theme.fontTiny
                        color: Theme.accent
                        Layout.preferredWidth: 25
                    }
                    Text {
                        text: model.message
                        font.family: Theme.digitalFont
                        font.pixelSize: Theme.fontTiny
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: avgMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            Text {
                anchors.centerIn: parent
                visible: avgModel.count === 0
                text: "No averaged results yet"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTiny
                color: Theme.textDim
            }
        }
    }
}
