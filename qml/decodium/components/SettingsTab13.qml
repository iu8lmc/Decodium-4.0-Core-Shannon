/* Decodium 4.0 - lazy Settings tab */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SettingsPageScroll {
    property var dialog
    readonly property var bridge: dialog ? dialog.appBridge : null
    readonly property bool compactSettingsLayout: dialog ? dialog.compactSettingsLayout : false
    readonly property bool narrowSettingsLayout: dialog ? dialog.narrowSettingsLayout : false
    readonly property int pageColumns: compactSettingsLayout ? 2 : 4
    readonly property int labelWidth: dialog ? dialog.labelWidth : 120
    readonly property int fieldMinWidth: dialog ? dialog.fieldMinWidth : 180
    readonly property int wideFieldMinWidth: dialog ? dialog.wideFieldMinWidth : 260
    readonly property int portFieldMinWidth: dialog ? dialog.portFieldMinWidth : 140
    readonly property int numericFieldMinWidth: dialog ? dialog.numericFieldMinWidth : 120
    readonly property int comboFieldMinWidth: dialog ? dialog.comboFieldMinWidth : 180
    readonly property int frequencyPageMinWidth: dialog ? dialog.frequencyPageMinWidth : 760
    readonly property int scrollLeftMargin: dialog ? dialog.scrollLeftMargin : 10
    readonly property int scrollTopMargin: dialog ? dialog.scrollTopMargin : 10
    readonly property int scrollRightMargin: dialog ? dialog.scrollRightMargin : 12
    readonly property int scrollBottomMargin: dialog ? dialog.scrollBottomMargin : 96
    pageLeftMargin: scrollLeftMargin
    pageTopMargin: scrollTopMargin
    pageRightMargin: scrollRightMargin
    pageBottomMargin: scrollBottomMargin
    minimumContentWidth: dialog ? dialog.settingsPageMinimumContentWidth(pageColumns) : 0
    readonly property color bgDeep: dialog ? dialog.bgDeep : "#080b12"
    readonly property color bgMedium: dialog ? dialog.bgMedium : "#101722"
    readonly property color bgLight: dialog ? dialog.bgLight : "#1a2433"
    readonly property color bgDark: dialog ? dialog.bgDark : "#080b12"
    readonly property color primaryBlue: dialog ? dialog.primaryBlue : "#3ba7ff"
    readonly property color secondaryCyan: dialog ? dialog.secondaryCyan : "#00d9ff"
    readonly property color accentGreen: dialog ? dialog.accentGreen : "#00f08b"
    readonly property color textPrimary: dialog ? dialog.textPrimary : "#f2f5f7"
    readonly property color textSecondary: dialog ? dialog.textSecondary : "#a7b2c0"
    readonly property color textDim: dialog ? dialog.textDim : "#667180"
    readonly property color glassBorder: dialog ? dialog.glassBorder : "#334455"
    readonly property int controlHeight: dialog ? dialog.controlHeight : 32
    readonly property int controlFontSize: dialog ? dialog.controlFontSize : 12
    readonly property int controlVerticalPadding: dialog ? dialog.controlVerticalPadding : 0
    readonly property int spinTextSidePadding: dialog ? dialog.spinTextSidePadding : 52

    function boolSetting(key, fallback) {
        return dialog ? dialog.boolSetting(key, fallback) : !!fallback
    }

    function setBoolSettingIfChanged(key, value, fallback) {
        if (dialog)
            dialog.setBoolSettingIfChanged(key, value, fallback)
    }
    id: callsignSettingsPage
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    ColumnLayout {
        width: Math.max(0, callsignSettingsPage.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.rightMargin: dialog.scrollRightMargin
        spacing: 10

        Text { text: qsTr("CALLSIGN INTELLIGENCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }
        Text {
            text: qsTr("Lookup locale con fallback DXCC, cache SQLite e provider aggiornabili. Le credenziali eQSL e Club Log sono salvate nel portachiavi tramite il canale secure settings.")
            color: textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 6

            Text { text: qsTr("Apertura automatica all'avvio QSO"); color: textSecondary }
            CheckBox {
                checked: dialog.callsignService ? dialog.callsignService.autoOpenOnQsoStart : false
                onToggled: if (dialog.callsignService) dialog.callsignService.autoOpenOnQsoStart = checked
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: qsTr("Apri il pannello lookup"); color: textPrimary; leftPadding: 24; verticalAlignment: Text.AlignVCenter }
            }
            Text { text: qsTr("Chiusura automatica dopo logging"); color: textSecondary }
            CheckBox {
                checked: dialog.callsignService ? dialog.callsignService.autoCloseAfterLogging : false
                onToggled: if (dialog.callsignService) dialog.callsignService.autoCloseAfterLogging = checked
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: qsTr("Chiudi il pannello dopo il QSO"); color: textPrimary; leftPadding: 24; verticalAlignment: Text.AlignVCenter }
            }
            Text { text: qsTr("Arricchimento campi mancanti"); color: textSecondary }
            CheckBox {
                checked: dialog.callsignService ? dialog.callsignService.enrichMissingFields : false
                onToggled: if (dialog.callsignService) dialog.callsignService.enrichMissingFields = checked
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: qsTr("Grid, nome e QTH nel prossimo log"); color: textPrimary; leftPadding: 24; verticalAlignment: Text.AlignVCenter }
            }
            Text { text: qsTr("Durata cache (minuti)"); color: textSecondary }
            SpinBox {
                from: 5; to: 10080; stepSize: 5
                value: dialog.callsignService ? dialog.callsignService.cacheTtlMinutes : 1440
                onValueModified: if (dialog.callsignService) dialog.callsignService.cacheTtlMinutes = value
            }
        }

        Text { text: qsTr("eQSL INBOX — CONFERME RICEVUTE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; Layout.topMargin: 8 }
        Text {
            text: qsTr("Scarica l'InBox/Archivio eQSL in ADIF e sincronizza le conferme nel logbook attivo. Il nome utente predefinito è il callsign della stazione.")
            color: textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        Text {
            text: qsTr("QRZ.com usa la chiave API già configurata nella sezione QRZ Logbook. Il pulsante Aggiorna scarica solo le conferme; in alternativa puoi importare qui un file ADI esportato da QRZ. Download, paginazione e sincronizzazione del logbook avvengono in background e lo stato resta visibile nella scheda.")
            color: textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 6
            Text { text: qsTr("Username/callsign eQSL"); color: textSecondary }
            TextField {
                Layout.fillWidth: true
                text: dialog.callsignService ? dialog.callsignService.eqslUsername : ""
                placeholderText: qsTr("Callsign eQSL o nickname account")
                onEditingFinished: if (dialog.callsignService) dialog.callsignService.eqslUsername = text.trim()
            }
            Text { text: qsTr("Password eQSL"); color: textSecondary }
            TextField {
                Layout.fillWidth: true
                text: dialog.callsignService ? dialog.callsignService.eqslPassword : ""
                echoMode: TextInput.Password
                onEditingFinished: if (dialog.callsignService) dialog.callsignService.eqslPassword = text.trim()
            }
        }

        Text { text: qsTr("LoTW — CONFERME RICEVUTE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; Layout.topMargin: 8 }
        Text {
            text: qsTr("Scarica le QSL LoTW ricevute e sincronizzale nel logbook. La password viene riutilizzata dalla sezione Reporting → LoTW; il login LoTW può essere diverso dal callsign operativo.")
            color: textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 6
            Text { text: qsTr("Username LoTW"); color: textSecondary }
            TextField {
                Layout.fillWidth: true
                text: dialog.callsignService ? dialog.callsignService.lotwUsername : ""
                placeholderText: qsTr("Callsign o username LoTW")
                onEditingFinished: if (dialog.callsignService) dialog.callsignService.lotwUsername = text.trim()
            }
        }

        Text { text: qsTr("CLUB LOG OQRS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; Layout.topMargin: 8 }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 6
            Text { text: qsTr("API key"); color: textSecondary }
            TextField {
                Layout.fillWidth: true
                text: dialog.callsignService ? dialog.callsignService.clubLogApiKey : ""
                echoMode: TextInput.Password
                onEditingFinished: if (dialog.callsignService) dialog.callsignService.clubLogApiKey = text.trim()
            }
            Text { text: qsTr("Email"); color: textSecondary }
            TextField {
                Layout.fillWidth: true
                text: dialog.callsignService ? dialog.callsignService.clubLogEmail : ""
                onEditingFinished: if (dialog.callsignService) dialog.callsignService.clubLogEmail = text.trim()
            }
            Text { text: qsTr("Application password"); color: textSecondary }
            TextField {
                Layout.fillWidth: true
                text: dialog.callsignService ? dialog.callsignService.clubLogApplicationPassword : ""
                echoMode: TextInput.Password
                onEditingFinished: if (dialog.callsignService) dialog.callsignService.clubLogApplicationPassword = text.trim()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Clear global lookup cache")
                enabled: dialog.callsignService
                onClicked: dialog.clearCallsignCacheDialogRef.open()
            }
            Item { Layout.fillWidth: true }
            Text {
                text: dialog.callsignService ? dialog.callsignService.status : ""
                color: textSecondary; elide: Text.ElideRight; Layout.fillWidth: true
            }
        }

        Text { text: qsTr("LOCAL DATABASES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; Layout.topMargin: 8 }
        Text {
            text: dialog.callsignService ? qsTr("SQLite: %1").arg(dialog.callsignService.databasePath) : ""
            color: textSecondary
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 2
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        Repeater {
            model: dialog.callsignDatabaseEntries()
            delegate: Rectangle {
                id: dbDelegate
                required property var modelData
                property bool confirmedProvider: modelData.id === "lotw_confirmed"
                                                || modelData.id === "qrz_confirmed"
                property bool confirmationSource: confirmedProvider || modelData.managedFile === true
                Layout.fillWidth: true
                Layout.preferredHeight: modelData.managedFile
                                        ? (modelData.error ? 150 : 126)
                                        : modelData.updateable
                                        ? (modelData.error ? 196 : 176)
                                          + ((modelData.id === "clublog_oqrs" || confirmedProvider) ? 22 : 0)
                                        : 68
                Layout.minimumHeight: modelData.managedFile
                                     ? (modelData.error ? 150 : 126)
                                     : modelData.updateable
                                     ? (modelData.error ? 196 : 176)
                                       + ((modelData.id === "clublog_oqrs" || confirmedProvider) ? 22 : 0)
                                     : 68
                color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.55)
                border.color: glassBorder
                radius: 5

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: dialog.callsignDatabaseLabel(modelData)
                            color: textPrimary
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Button {
                            text: qsTr("Update")
                            visible: modelData.updateable
                            enabled: dialog.callsignService
                                     && !dialog.callsignService.lookupPending
                                     && !dialog.callsignDatabaseUpdating(modelData.id)
                            Layout.preferredWidth: 100
                            Layout.minimumWidth: 96
                            implicitHeight: dialog.controlHeight
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? textPrimary : textSecondary
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                            }
                            onClicked: dialog.refreshCallsignDatabase(modelData.id)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: Number(modelData.rowCount || 0) < 0
                                  ? qsTr("Updating...")
                                  : dbDelegate.confirmationSource && modelData.rowCount > 0
                                  ? qsTr("%1 confirmations in last sync").arg(modelData.rowCount)
                                  : dbDelegate.confirmationSource
                                  ? qsTr("No confirmations")
                                  : modelData.rowCount > 0
                                  ? qsTr("%1 record").arg(modelData.rowCount)
                                  : qsTr("No records")
                            color: textSecondary
                            font.pixelSize: 11
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: modelData.status || qsTr("Never updated")
                                color: modelData.error ? "#ff7676" : secondaryCyan
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                                Layout.fillWidth: true
                            }
                            Text {
                                visible: (modelData.id === "clublog_oqrs" || modelData.id === "lotw_confirmed" || modelData.id === "qrz_confirmed" || modelData.managedFile === true)
                                         && Number(modelData.updatedAt || 0) > 0
                                text: qsTr("Last update: %1").arg(
                                          Qt.formatDateTime(new Date(Number(modelData.updatedAt)), "yyyy-MM-dd HH:mm"))
                                color: textSecondary
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                    RowLayout {
                        visible: modelData.managedFile === true
                        Layout.fillWidth: true
                        Text {
                            text: modelData.localPath
                                  ? qsTr("File: %1").arg(modelData.localPath)
                                  : qsTr("File not found")
                            color: textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 2
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        visible: modelData.updateable && modelData.managedFile !== true
                        Layout.fillWidth: true
                        TextField {
                            id: localDatabasePathField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: dialog.controlHeight
                            implicitHeight: dialog.controlHeight
                            placeholderText: dbDelegate.confirmedProvider
                                             ? qsTr("Optional ADI file to merge into the logbook")
                                             : qsTr("Optional local file path")
                            text: modelData.localPath || ""
                        }
                    }
                    RowLayout {
                        visible: modelData.updateable && modelData.managedFile !== true
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Button {
                            text: qsTr("Choose")
                            Layout.preferredWidth: 92
                            Layout.minimumWidth: 88
                            implicitHeight: dialog.controlHeight
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? textPrimary : textSecondary
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                            }
                            onClicked: {
                                var selected = bridge.openFileDialog(
                                    dbDelegate.confirmedProvider ? qsTr("Import confirmations ADI")
                                                                  : qsTr("Import callsign database"),
                                    "",
                                    dbDelegate.confirmedProvider
                                        ? [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")]
                                        : [qsTr("Databases and CSV (*)")])
                                if (selected && selected.length > 0)
                                    localDatabasePathField.text = selected
                            }
                        }
                        Button {
                            text: dbDelegate.confirmedProvider ? qsTr("Import ADI") : qsTr("Import")
                            enabled: dialog.callsignService && localDatabasePathField.text.trim().length > 0
                            Layout.preferredWidth: 92
                            Layout.minimumWidth: 88
                            implicitHeight: dialog.controlHeight
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? textPrimary : textSecondary
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                            }
                            onClicked: dialog.callsignService.importDatabase(modelData.id, localDatabasePathField.text.trim())
                        }
                    }
                    Text {
                        visible: dbDelegate.confirmedProvider
                        text: qsTr("Manual recovery only: importing an ADI merges its confirmations into the active logbook, just like Update. It does not advance the online LoTW download cursor.")
                        color: textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        visible: !!modelData.error
                        text: modelData.error || ""
                        color: "#ff7676"
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Item { Layout.fillWidth: true; Layout.fillHeight: true; implicitHeight: 16 }
    }
}
