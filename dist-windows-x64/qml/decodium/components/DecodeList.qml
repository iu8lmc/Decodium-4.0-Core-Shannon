/* Decodium Qt6 - Decode List Component */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: decodeListComponent

    property var model: []
    property color accentGreen: bridge.themeManager.accentColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color errorRed: bridge.themeManager.errorColor

    signal decodeSelected(int index)
    signal decodeDoubleClicked(int index)

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: decodeListComponent.model
        spacing: 3
        // Chronological order: oldest at top, newest at bottom (like WSJT-X)
        verticalLayoutDirection: ListView.TopToBottom

        // Auto-scroll to newest decode at the bottom
        onCountChanged: {
            if (count > 0 && !listView.moving) {
                Qt.callLater(function() { listView.positionViewAtEnd() })
            }
        }

        delegate: Rectangle {
            width: listView.width
            height: 32
            // TX messages in red, CQ with grid in green-tinted, others neutral
            color: {
                if (modelData.isTx)  return Qt.rgba(244/255, 67/255, 54/255, 0.2)
                if (modelData.isCQ)  return Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)
                if (modelData.isMyCall) return Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.1)
                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.04)
            }
            border.color: {
                if (modelData.isTx)     return errorRed
                if (modelData.isCQ)     return Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.3)
                if (modelData.isMyCall) return Qt.rgba(errorRed.r, errorRed.g, errorRed.b, 0.4)
                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
            }
            border.width: (modelData.isTx || modelData.isMyCall) ? 2 : 1
            radius: 5

            MouseArea {
                anchors.fill: parent
                onClicked: decodeListComponent.decodeSelected(index)
                onDoubleClicked: decodeListComponent.decodeDoubleClicked(index)
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                // Tempo UTC
                Text {
                    text: modelData.time || ""
                    font.family: "Monospace"
                    font.pixelSize: 11
                    color: modelData.isTx ? errorRed : textSecondary
                    Layout.preferredWidth: 52
                }

                // SNR / TX indicator
                Text {
                    text: modelData.isTx ? ">>>TX" : (modelData.db + "dB")
                    font.family: "Monospace"
                    font.pixelSize: 11
                    font.bold: modelData.isTx
                    color: {
                        if (modelData.isTx) return errorRed
                        var db = parseInt(modelData.db)
                        if (db >= 0)   return "#69F0AE"
                        if (db >= -10) return accentGreen
                        if (db >= -15) return textSecondary
                        return Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.7)
                    }
                    Layout.preferredWidth: 46
                }

                // Frequenza audio
                Text {
                    text: modelData.freq + "Hz"
                    font.family: "Monospace"
                    font.pixelSize: 11
                    color: modelData.isTx ? errorRed : secondaryCyan
                    Layout.preferredWidth: 54
                }

                // GAP 5 — C13: Distanza (km) se disponibile
                Text {
                    visible: !modelData.isTx && modelData.dxDistance !== undefined && modelData.dxDistance > 0
                    text: modelData.dxDistance !== undefined && modelData.dxDistance > 0
                          ? Math.round(modelData.dxDistance) + "km"
                          : ""
                    font.family: "Monospace"
                    font.pixelSize: 10
                    color: "#80CBC4"  // teal chiaro
                    Layout.preferredWidth: 52
                    horizontalAlignment: Text.AlignRight
                }

                // Testo messaggio
                Text {
                    text: modelData.message || ""
                    font.family: "Monospace"
                    font.pixelSize: 12
                    font.bold: modelData.isCQ || modelData.isTx
                    // B7 — C13: applica colori dinamici
                    color: {
                        if (modelData.isTx)     return errorRed
                        if (modelData.isMyCall) return bridge.colorMyCall
                        if (modelData.isCQ)     return bridge.colorCQ
                        if (modelData.isB4)     return bridge.colorB4
                        return textPrimary
                    }
                    font.strikeout: modelData.isB4 !== undefined && modelData.isB4 && bridge.showB4Strikethrough
                    opacity: modelData.isB4 !== undefined && modelData.isB4 ? 0.55 : 1.0
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }
}
