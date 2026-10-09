import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: bugDialog
    title: qsTr("Report a problem")
    modal: true
    width: 700
    height: 580
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton

    Material.theme: Material.Dark
    Material.accent: Material.Orange

    property color bgDeep: "#1a1a2e"
    property color accentOrange: "#ff7814"

    // IU8LMC — Autodiagnosi. Lo storico dice che la grande maggioranza delle
    // segnalazioni NON sono bug (versione vecchia, orologio fuori sync, MAM
    // acceso in FT2, banda chiusa, thread che affamano l'audio): le cause note
    // vanno mostrate PRIMA che l'utente scriva, non dopo.
    property var selfCheckFindings: []

    function refreshSelfCheck() {
        selfCheckFindings = (bridge && bridge.runSelfCheck) ? bridge.runSelfCheck() : []
    }

    // IU8LMC — La segnalazione va al forum community.ft2.it/groups (non piu' GitHub),
    // nella lingua dell'utente (il testo e' gia' tradotto via qsTr) e nella categoria
    // giusta. Caso A: se l'autodiagnosi trova una causa nota, la categoria arriva da
    // li'; altrimenti "other". Caso B: l'utente descrive e conferma.
    // Categorie del forum (valori del form: cat/rx/tx/ui/audio/install/lang/other).
    property var categoryModel: [
        { value: "cat",     label: qsTr("CAT / HRD") },
        { value: "rx",      label: qsTr("Reception / decoding") },
        { value: "tx",      label: qsTr("Transmission") },
        { value: "ui",      label: qsTr("Interface") },
        { value: "audio",   label: qsTr("Audio & peripherals") },
        { value: "install", label: qsTr("Installation & updates") },
        { value: "lang",    label: qsTr("Translations") },
        { value: "other",   label: qsTr("General / Other") }
    ]

    // Categoria suggerita dall'autodiagnosi (la causa nota piu' grave). L'utente
    // puo' comunque cambiarla dal menu prima di inviare.
    function reportCategory() {
        if (selfCheckFindings.length > 0 && selfCheckFindings[0].category)
            return selfCheckFindings[0].category
        return "other"
    }

    // Posiziona il menu sulla categoria suggerita dalla diagnostica.
    function applySuggestedCategory() {
        var v = reportCategory()
        for (var i = 0; i < categoryModel.length; ++i) {
            if (categoryModel[i].value === v) { categoryCombo.currentIndex = i; return }
        }
        categoryCombo.currentIndex = categoryModel.length - 1   // "other"
    }

    function reportTitle() {
        var d = descriptionField.text.trim()
        if (d.length > 0)
            return d.split("\n")[0].substring(0, 100)
        // Caso A senza descrizione: titolo dal problema/modo.
        var mode = (bridge && bridge.mode) ? bridge.mode : ""
        return qsTr("Problem in %1").arg(mode || "Decodium")
    }

    function reportBody() {
        var parts = []
        if (descriptionField.text.trim().length > 0)
            parts.push(descriptionField.text.trim())
        // Cause note gia' rilevate: aiutano chi legge sul forum.
        if (selfCheckFindings.length > 0) {
            parts.push("")
            parts.push(qsTr("Automatic self-check:"))
            for (var i = 0; i < selfCheckFindings.length; ++i)
                parts.push("- " + selfCheckFindings[i].title + ": " + selfCheckFindings[i].detail)
        }
        // Contesto diagnostico (versione, OS, audio, rig, ultimi errori).
        if (bridge.diagnostics)
            parts.push("\n" + bridge.diagnostics.buildReport(""))
        return parts.join("\n")
    }

    Component.onCompleted: {
        if (bridge.diagnostics) bridge.diagnostics.analyzeErrors()
        refreshSelfCheck()
    }
    onOpened: {
        if (bridge.diagnostics) bridge.diagnostics.analyzeErrors()
        refreshSelfCheck()          // stato fresco a ogni apertura
        applySuggestedCategory()    // pre-seleziona la categoria dalla diagnostica
    }

    background: Rectangle {
        color: bgDeep
        radius: 12
        border.color: accentOrange
        border.width: 1

        // Keep this dialog compatible with the Linux Qt 6.4 AppImage runtime.
        // QtQuick.Effects/MultiEffect is only available from Qt 6.5.
        layer.enabled: false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Sottotitolo: spiega dove va la segnalazione.
        Label {
            Layout.fillWidth: true
            text: qsTr("Your report is posted on the community forum, in your language, so others can help.")
            color: "#8b949e"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // ── IU8LMC: Autodiagnosi ─────────────────────────────────────────
        // Cause note rilevate dallo STATO (non dal testo del log): versione,
        // orologio, MAM in FT2, banda morta, thread. Ogni regola nasce da un
        // caso reale diagnosticato sul campo. Vedi DecodiumSelfCheck.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: selfCheckColumn.implicitHeight + 20
            visible: bugDialog.selfCheckFindings.length > 0
            color: "#2a1f14"
            radius: 6
            border.color: accentOrange
            border.width: 1

            ColumnLayout {
                id: selfCheckColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Found %n likely cause(s) — please check these before reporting:",
                               "", bugDialog.selfCheckFindings.length)
                    color: accentOrange
                    font.pixelSize: 13
                    font.bold: true
                    wrapMode: Text.Wrap
                }

                Repeater {
                    model: bugDialog.selfCheckFindings

                    delegate: ColumnLayout {
                        required property var modelData   // nudo non e' in scope nei binding
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: (modelData.severity === 2 ? "⛔  " : "⚠  ") + modelData.title
                            color: modelData.severity === 2 ? "#ff6b6b" : "#ffd166"
                            font.pixelSize: 13
                            font.bold: true
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.detail
                            color: "#c9d1d9"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "→  " + modelData.action
                            color: "#aaddaa"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        // Nessuna causa nota: allora la segnalazione ha senso davvero.
        Label {
            Layout.fillWidth: true
            visible: bugDialog.selfCheckFindings.length === 0
            text: qsTr("Self-check found no known cause — please describe the problem below.")
            color: "#aaddaa"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // Categoria: suggerita dalla diagnostica, modificabile dall'utente.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label {
                text: qsTr("Category:")
                color: "white"
                font.pixelSize: 14
            }
            ComboBox {
                id: categoryCombo
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                model: bugDialog.categoryModel
                Material.foreground: "white"
            }
        }

        // User description
        Label {
            text: qsTr("Describe the problem:")
            color: "white"
            font.pixelSize: 14
        }
        // Contenitore + placeholder MANUALE: la floating-label Material della
        // TextArea si sovrapponeva al testo dentro la ScrollView (bug di rendering).
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: "#2a2a3e"
            radius: 4
            clip: true

            TextArea {
                id: descriptionField
                anchors.fill: parent
                color: "white"
                font.pixelSize: 13
                wrapMode: TextArea.Wrap
                selectByMouse: true
                leftPadding: 8; topPadding: 8; rightPadding: 8; bottomPadding: 8
                background: null
            }
            Text {
                anchors.fill: parent
                anchors.margins: 8
                visible: descriptionField.text.length === 0 && !descriptionField.activeFocus
                text: qsTr("What happens, since when, and what you already tried. Paste any error messages too.")
                color: "#6b7280"
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
        }

        // Diagnostic preview
        Label {
            text: qsTr("Diagnostic info (auto-collected):")
            color: "#888"
            font.pixelSize: 12
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: diagnosticPreview
                text: bridge.diagnostics ? bridge.diagnostics.systemInfo
                      + "\n\n--- Audio ---\n" + bridge.diagnostics.audioState
                      + "\n\n--- Rig ---\n" + bridge.diagnostics.rigState
                      + (bridge.diagnostics.errorCount > 0
                         ? "\n\n--- QML Errors ---\n" + bridge.diagnostics.qmlErrors
                         : "")
                      : qsTr("Diagnostics not available")
                readOnly: true
                color: "#999"
                font.pixelSize: 11
                font.family: decodiumMonoFontFamily
                wrapMode: TextArea.Wrap
                background: Rectangle { color: "#1e1e2e"; radius: 4 }
            }
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: qsTr("Copy Report")
                Material.background: "#333"
                onClicked: {
                    var report = bridge.diagnostics.buildReport(descriptionField.text)
                    bridge.copyToClipboard(report)
                    copyLabel.visible = true
                    copyTimer.restart()
                }
            }
            Label {
                id: copyLabel
                text: qsTr("Copied!")
                color: accentOrange
                visible: false
                Timer { id: copyTimer; interval: 2000; onTriggered: copyLabel.visible = false }
            }

            // IU8LMC: carica l'ultimo log diagnostico, lo intitola col nominativo
            // e lo pubblica nel gruppo Generale.
            Button {
                text: community.busy ? qsTr("Sending...") : qsTr("Upload last diagnostic log")
                enabled: !community.busy
                Material.background: "#2a4a6a"
                onClicked: {
                    var call = (bridge && bridge.callsign && bridge.callsign.trim().length > 0)
                               ? bridge.callsign.trim().toUpperCase() : "SWL"
                    var stamp = Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm")
                    var title = qsTr("Diagnostic log — %1 — %2").arg(call).arg(stamp)
                    // Il forum limita il body a 8000 caratteri: prendo la coda
                    // recente del log (~6500) + un'intestazione breve, per stare
                    // sotto il limite senza tagliare le righe piu' recenti.
                    var tail = community.diagnosticLogTail(6500)
                    if (tail.length === 0) {
                        communityResult.isError = true
                        communityResult.text = qsTr("The diagnostic log could not be read.")
                        return
                    }
                    var sys = bridge.diagnostics ? bridge.diagnostics.systemInfo : ""
                    var body = qsTr("Diagnostic log uploaded from Decodium.") + "\n"
                             + sys.substring(0, 700) + "\n"
                             + "=== decodium_diagnostic.log (tail) ===\n" + tail
                    // Gruppo Generale = "other".
                    community.postProblem("other", title, call, body)
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: community.busy ? qsTr("Posting...") : qsTr("Post to community")
                enabled: descriptionField.text.trim().length > 10 && !community.busy
                Material.background: accentOrange
                onClicked: {
                    var cat = categoryCombo.currentValue || bugDialog.reportCategory()
                    var ttl = bugDialog.reportTitle()
                    if (community.alreadyReported(cat, ttl)) {
                        communityResult.text = qsTr("You already reported this recently. "
                            + "Check the community for replies: %1").arg("https://groups.ft2.it")
                        communityResult.isError = true
                        return
                    }
                    community.postProblem(cat, ttl,
                        (bridge && bridge.callsign) ? bridge.callsign : "",
                        bugDialog.reportBody())
                }
            }

            Button {
                text: qsTr("Close")
                Material.background: "#444"
                onClicked: bugDialog.close()
            }
        }

        // Esito invio al forum
        Label {
            id: communityResult
            property bool isError: false
            visible: text !== ""
            color: isError ? "#ff8844" : "#44ff44"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Connections {
            target: community
            function onPosted(url) {
                communityResult.isError = false
                // Mostro l'indirizzo breve groups.ft2.it (quello che usi tu); il
                // topic appena creato e' in cima alla lista dei gruppi.
                communityResult.text = qsTr("Posted! Open the groups: %1").arg("https://groups.ft2.it")
            }
            function onFailed(message) {
                communityResult.isError = true
                communityResult.text = message
            }
        }
    }

    Connections {
        target: bridge.diagnostics
        function onSubmitFinished(success, url) {
            if (success) {
                descriptionField.text = ""
            }
        }
    }
}
