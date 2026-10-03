/* Esito del rilevamento automatico della radio — riquadro condiviso.
 *
 * Usato da tre punti: la sezione CAT delle impostazioni, la finestra Rig
 * Control e la proposta al primo avvio. Il rilevamento e' PASSIVO: legge solo
 * cio' che il sistema gia' sa (identita' USB delle porte, nomi delle schede
 * audio), non apre porte e non invia comandi, quindi e' innocuo anche a CAT
 * connesso e in trasmissione.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder:   bridge.themeManager.glassBorder
    property color bgDeep:        bridge.themeManager.bgDeep

    property var candidates: []
    // Testo che precede l'elenco: al primo avvio spiega perche' compare da solo.
    property string introText: ""
    // Emesso dopo che l'utente ha applicato una proposta, o ha chiuso senza
    // applicare: serve al primo avvio per non riproporsi piu'.
    signal dismissed()

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: 560
    height: 440
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onClosed: root.dismissed()

    // Esegue il rilevamento e mostra l'esito.
    function detectAndOpen(intro) {
        root.introText = intro || ""
        root.candidates = bridge.detectConnectedRigs()
        root.open()
    }

    // Applica un candidato. La radio si imposta per PRIMA: il backend, ricevuto
    // il nome, applica da solo i suoi valori predefiniti (velocita', indirizzo
    // CI-V), che poi affiniamo con quelli rilevati.
    function applyCandidate(candidate, withAudio) {
        var cat = bridge.catManager
        if (!candidate || !cat)
            return
        var token = String(candidate.rigToken || "")
        if (token.length > 0 && cat.rigList) {
            var list = cat.rigList || []
            var wanted = token.toLowerCase().replace(/[-_ ]/g, "")
            var best = ""
            for (var i = 0; i < list.length; ++i) {
                var normalized = String(list[i]).toLowerCase().replace(/[-_ ]/g, "")
                if (normalized.indexOf(wanted) < 0)
                    continue
                // A parita' di corrispondenza vince la voce piu' breve: fra
                // "FT-991" e "FT-991A" si sceglie quella che combacia davvero.
                if (best === "" || String(list[i]).length < best.length)
                    best = String(list[i])
            }
            if (best !== "")
                cat.rigName = best
        }
        if (candidate.catPort) {
            if (cat.refreshPorts)
                cat.refreshPorts()
            cat.serialPort = candidate.catPort
        }
        if (candidate.baudRate > 0)
            cat.baudRate = candidate.baudRate
        if (withAudio) {
            if (candidate.audioInput)
                bridge.audioInputDevice = candidate.audioInput
            if (candidate.audioOutput)
                bridge.audioOutputDevice = candidate.audioOutput
        }
    }

    background: Rectangle {
        color: root.bgDeep
        border.color: root.accentGreen
        border.width: 1
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("Detected radios")
            color: root.accentGreen
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            visible: root.introText.length > 0
            wrapMode: Text.WordWrap
            color: root.textPrimary
            font.pixelSize: 12
            text: root.introText
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.textSecondary
            font.pixelSize: 11
            text: root.candidates.length > 0
                  ? qsTr("Nothing has been changed yet. Check the proposal, then apply it.")
                  : qsTr("No serial device found. Is the radio on and the USB cable connected?")
        }

        ListView {
            id: candidateList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: root.candidates
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: card
                required property var modelData
                width: candidateList.width - 12
                height: cardColumn.implicitHeight + 20
                color: Qt.rgba(1, 1, 1, 0.05)
                border.color: card.modelData.confidence >= 70 ? root.accentGreen : root.glassBorder
                border.width: 1
                radius: 6

                ColumnLayout {
                    id: cardColumn
                    x: 10
                    y: 10
                    width: parent.width - 20
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: card.modelData.rigLabel
                            color: root.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: qsTr("confidence %1%").arg(card.modelData.confidence)
                            color: card.modelData.confidence >= 70 ? root.accentGreen : root.textSecondary
                            font.pixelSize: 11
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: root.textPrimary
                        font.pixelSize: 11
                        text: {
                            var line = qsTr("CAT port: %1").arg(card.modelData.catPort)
                            if (card.modelData.baudRate > 0)
                                line += qsTr(" — %1 baud").arg(card.modelData.baudRate)
                            var others = card.modelData.otherPorts || []
                            if (others.length > 0)
                                line += qsTr(" (other ports of the same device: %1)").arg(others.join(", "))
                            return line
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !!card.modelData.audioInput || !!card.modelData.audioOutput
                        wrapMode: Text.WordWrap
                        color: root.textPrimary
                        font.pixelSize: 11
                        text: qsTr("Audio: in %1 / out %2")
                                .arg(card.modelData.audioInput || qsTr("not found"))
                                .arg(card.modelData.audioOutput || qsTr("not found"))
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: root.textSecondary
                        font.pixelSize: 10
                        text: card.modelData.evidence
                    }

                    Button {
                        id: applyButton
                        Layout.alignment: Qt.AlignRight
                        implicitHeight: 30
                        text: qsTr("Apply")
                        onClicked: {
                            root.applyCandidate(card.modelData, audioCheck.checked)
                            root.close()
                        }
                        background: Rectangle {
                            color: applyButton.hovered
                                   ? Qt.rgba(root.accentGreen.r, root.accentGreen.g, root.accentGreen.b, 0.25)
                                   : Qt.rgba(1, 1, 1, 0.07)
                            border.color: root.accentGreen
                            radius: 4
                        }
                        contentItem: Text {
                            text: applyButton.text
                            color: root.accentGreen
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            CheckBox {
                id: audioCheck
                checked: true
                text: qsTr("Also set the radio's audio devices")
                contentItem: Text {
                    leftPadding: audioCheck.indicator.width + 6
                    text: audioCheck.text
                    color: root.textPrimary
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                }
                indicator: Rectangle {
                    implicitWidth: 16; implicitHeight: 16; radius: 3
                    x: audioCheck.leftPadding
                    y: (audioCheck.height - height) / 2
                    color: audioCheck.checked ? root.accentGreen : Qt.rgba(1, 1, 1, 0.07)
                    border.color: root.accentGreen
                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        color: root.bgDeep
                        font.pixelSize: 11
                        visible: audioCheck.checked
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                id: closeButton
                implicitHeight: 30
                text: qsTr("Close")
                onClicked: root.close()
                background: Rectangle {
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.07)
                    border.color: root.glassBorder
                    radius: 4
                }
                contentItem: Text {
                    text: closeButton.text
                    color: root.textPrimary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
