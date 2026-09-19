import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// IU8LMC — Avviso di aggiornamento con conferma esplicita.
//
// Niente parte da solo: l'app avvisa, l'utente decide. "Aggiorna ora" scarica e
// installa il pacchetto appropriato; "Piu' tardi" richiude e basta; "Salta
// questa versione" silenzia SOLO questa versione, non le successive. Su Linux
// l'AppImage corrente viene sostituita atomicamente quando possibile.
//
// 1.0.644 — due difetti visibili nella segnalazione del 19/09/2026:
//   1) il contenuto usciva DAL riquadro e finiva sopra la lista delle
//      decodifiche. Causa: la ColumnLayout era ancorata al genitore, e una
//      colonna ancorata non ha dimensione implicita, quindi il Dialog restava
//      alto quanto il solo titolo mentre i figli disegnavano piu' in basso.
//      Ora il contenuto sta in contentItem/header/footer e le dimensioni le
//      decide il layout, con un tetto legato alla finestra.
//   2) le note di rilascio si leggevano in Markdown grezzo ("## English (UK)",
//      "**v1.0.639**"): ora sono renderizzate.
// I colori arrivano dal tema come nelle altre finestre, invece di essere fissi.
Dialog {
    id: updateDialog
    modal: true
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton
    padding: 0

    property var theme: bridge && bridge.themeManager ? bridge.themeManager : null
    property color bgDeep: theme ? theme.bgDeep : "#1a1a2e"
    property color bgMedium: theme ? theme.bgMedium : "#2a2a3e"
    property color primaryBlue: theme ? theme.primaryColor : "#4a9eff"
    property color secondaryCyan: theme ? theme.secondaryColor : "#00d9ff"
    property color accentOrange: theme ? theme.warningColor : "#ff7814"
    property color textPrimary: theme ? theme.textPrimary : "#e6edf3"
    property color textSecondary: theme ? theme.textSecondary : "#8b949e"
    property color glassBorder: theme ? theme.glassBorder : Qt.rgba(1, 1, 1, 0.12)

    Material.theme: theme && theme.isLightTheme ? Material.Light : Material.Dark
    Material.accent: primaryBlue
    Material.primary: secondaryCyan

    readonly property int spazioLarghezza: parent ? parent.width : 900
    readonly property int spazioAltezza: parent ? parent.height : 700
    width: Math.min(680, spazioLarghezza - 48)
    // L'altezza la chiede il contenuto, ma non oltre la finestra che lo ospita.
    height: Math.min(implicitHeight, spazioAltezza - 48)

    background: Rectangle {
        color: bgDeep
        radius: 12
        border.color: glassBorder
        border.width: 1
    }

    header: Rectangle {
        implicitHeight: 52
        color: "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: glassBorder
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 16
            spacing: 10

            Text {
                text: "⬆"
                color: accentOrange
                font.pixelSize: 16
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Update available")
                color: textPrimary
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }

            // Versione nuova in evidenza: e' l'informazione che si cerca per prima.
            Rectangle {
                Layout.preferredHeight: 22
                Layout.preferredWidth: versionLabel.implicitWidth + 18
                radius: 4
                color: Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.16)
                border.color: accentOrange
                border.width: 1

                Label {
                    id: versionLabel
                    anchors.centerIn: parent
                    text: updater.latestVersion
                    color: accentOrange
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 10

        // I margini stanno qui e non su padding del Dialog: header e footer
        // devono toccare i bordi, il contenuto no.
        Item { Layout.preferredHeight: 6 }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("You are running %1.").arg(updater.currentVersion)
                    color: textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: updater.releaseRepository.length > 0
                    text: qsTr("Source: %1").arg(updater.releaseRepository)
                    color: textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            text: qsTr("What's new:")
            color: secondaryCyan
            font.pixelSize: 12
            font.bold: true
            visible: updater.releaseNotes.length > 0
        }

        // Note di rilascio: Markdown reso leggibile, non piu' i cancelletti e
        // gli asterischi del testo grezzo.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.minimumHeight: 120
            Layout.preferredHeight: 260
            visible: updater.releaseNotes.length > 0
            color: bgMedium
            radius: 6
            border.color: glassBorder
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    text: updater.releaseNotes
                    textFormat: TextEdit.MarkdownText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: Text.Wrap
                    color: textPrimary
                    font.pixelSize: 12
                    background: null
                }
            }
        }

        // Avanzamento del download: senza, un utente con linea lenta pensa che
        // si sia piantato e chiude l'app a meta' scaricamento.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            visible: updater.busy
            spacing: 4

            ProgressBar {
                Layout.fillWidth: true
                indeterminate: updater.progress < 0
                from: 0; to: 100
                value: updater.progress
            }
            Label {
                Layout.fillWidth: true
                text: updater.statusText
                color: textSecondary
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            visible: !updater.busy
            text: {
                if (Qt.platform.os === "linux") {
                    if (updater.appImageRuntime) {
                        return qsTr("The current AppImage will be replaced safely and Decodium "
                                    + "will restart. Your settings and QSO log are kept.")
                    }
                    return qsTr("The new AppImage will be saved to your Downloads folder. "
                                + "Launch it manually to complete the update.")
                }
                if (Qt.platform.os === "windows") {
                    return qsTr("Decodium will close and the installer will start. Your settings "
                                + "and QSO log are kept.")
                }
                return qsTr("The update package will be downloaded and opened. Your settings "
                            + "and QSO log are kept.")
            }
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            visible: !updater.busy && updater.statusText.length > 0
            text: updater.statusText
            color: textPrimary
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        Item { Layout.preferredHeight: 4 }
    }

    footer: Rectangle {
        implicitHeight: 56
        color: "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: glassBorder
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            Button {
                text: qsTr("Skip this version")
                enabled: !updater.busy
                flat: true
                font.pixelSize: 12
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? textSecondary : Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.4)
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: { updater.skipThisVersion(); updateDialog.close() }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Later")
                enabled: !updater.busy
                flat: true
                font.pixelSize: 12
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? textPrimary : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.4)
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updateDialog.close()
            }

            Button {
                id: updateButton
                text: Qt.platform.os === "linux"
                      ? qsTr("Update AppImage")
                      : (Qt.platform.os === "windows"
                         ? qsTr("Update now") : qsTr("Download update"))
                enabled: !updater.busy
                font.pixelSize: 12
                font.bold: true
                Layout.preferredHeight: 34
                background: Rectangle {
                    radius: 6
                    color: updateButton.enabled
                           ? (updateButton.down ? Qt.darker(accentOrange, 1.2)
                                                : (updateButton.hovered ? Qt.lighter(accentOrange, 1.1) : accentOrange))
                           : Qt.rgba(accentOrange.r, accentOrange.g, accentOrange.b, 0.35)
                }
                contentItem: Text {
                    text: updateButton.text
                    color: "#12121c"
                    font: updateButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 10
                    rightPadding: 10
                }
                onClicked: updater.downloadAndInstall()
            }
        }
    }
}
