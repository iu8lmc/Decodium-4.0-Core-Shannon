import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    readonly property var bridge: appEngine
    Material.theme: Material.Dark
    Material.accent: "#44d4df"
    Material.primary: "#182632"
    title: qsTr("SSB — PC microphone")
    width: 1200; height: 820; minimumWidth: 760; minimumHeight: 560
    color: "#101923"
    palette.windowText: "#eaf1f8"
    palette.text: "#eaf1f8"
    palette.buttonText: "#eaf1f8"
    palette.button: "#263747"
    palette.base: "#182632"
    palette.highlight: "#17a6bb"
    property var voice: ssbModule
    property var controls: bridge.ssbControls
    property bool ready: bridge.mode === "SSB" && bridge.catConnected
    readonly property bool directCatPowerOnAvailable: !!(bridge.hamlibCat
        && (bridge.hamlibCat.portType === "serial" || bridge.hamlibCat.portType === "usb")
        && String(bridge.hamlibCat.serialPort || "").trim().length > 0
        && ["default", "auto", "predefinito"].indexOf(String(bridge.hamlibCat.serialPort || "").trim().toLowerCase()) < 0
        && !bridge.hamlibCat.connecting && !bridge.decoPortUseRemote)
    property string radioActionStatus: ""
    property bool syncingRadioMode: false
    // Slot per cui e' stato chiesto "AI help": la risposta dell'assistente ne sostituisce il testo.
    property int pendingSlot: -1
    property string lastApplied: ""
    Connections {
        target: assistant
        function onChanged() {
            if (win.pendingSlot >= 0 && !assistant.busy && assistant.lastReply.length > 0
                    && assistant.lastReply !== win.lastApplied) {
                win.lastApplied = assistant.lastReply
                var m = voiceKeyer.messages[win.pendingSlot]
                if (m) voiceKeyer.setSlotText(win.pendingSlot, m.label, assistant.lastReply)
                win.pendingSlot = -1
            }
        }
    }
    function syncRadioModeFromCat() {
        syncingRadioMode = true
        var mode = (bridge.catMode || "").toString().toUpperCase()
        if (mode === "LSB" || mode === "DIGL" || mode === "DATA-L" || mode === "PKTLSB") {
            side.currentIndex = 1
            dataInput.checked = mode !== "LSB"
        } else if (mode === "USB" || mode === "DIGU" || mode === "DATA-U" || mode === "PKTUSB") {
            side.currentIndex = 0
            dataInput.checked = mode !== "USB"
        }
        syncingRadioMode = false
    }
    function updateRadioMode() { bridge.ssbSetRadioMode(dataInput.checked ? (side.currentIndex ? "DIGL" : "DIGU") : (side.currentIndex ? "LSB" : "USB")) }
    onClosing: voice.stop()
    onActiveChanged: if (!active) voice.stop()
    Component.onDestruction: voice.stop()
    onVisibleChanged: if (visible) {
        syncRadioModeFromCat()
        bridge.ssbRefreshControls()
        if (bridge.hamlibCat) bridge.hamlibCat.requestAtuState()
    }
    Component.onCompleted: {
        syncRadioModeFromCat()
        if (bridge.hamlibCat) bridge.hamlibCat.requestAtuState()
    }
    Shortcut { sequence: "Escape"; onActivated: { voice.stop(); voiceKeyer.stop() } }

    // Barra spaziatrice = PTT: si trasmette finche' e' premuta. Con il cursore in
    // un campo di testo (frequenza) la barra scrive uno spazio e basta; con un
    // elenco aperto non si ruba il fuoco.
    readonly property bool popupOpen: side.popup.visible || mic.popup.visible
                                      || agcBox.popup.visible || rxAgc.popup.visible
    function isTextEntry(item) { return !!item && ("cursorPosition" in item) }
    onActiveFocusItemChanged: Qt.callLater(function () {
        var f = win.activeFocusItem
        if (!win.active || f === keyCatcher || win.isTextEntry(f) || win.popupOpen)
            return
        keyCatcher.forceActiveFocus()
    })
    Item {
        id: keyCatcher
        focus: true
        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Space && !event.isAutoRepeat) {
                event.accepted = true
                if (win.ready && controls.busy !== true && mic.currentIndex >= 0 && !voice.transmitting)
                    voice.start()
            }
        }
        Keys.onReleased: function (event) {
            if (event.key === Qt.Key_Space && !event.isAutoRepeat) {
                event.accepted = true
                voice.stop()
            }
        }
    }

    // Altri comandi di ricezione che la radio espone via CAT: compaiono solo
    // quelli che dichiara di saper leggere e scrivere.
    readonly property var extraLevels: [
        { key: "af",      label: qsTr("AF gain %"),            kind: "pct" },
        { key: "sql",     label: qsTr("Squelch %"),            kind: "pct" },
        { key: "nr",      label: qsTr("Noise reduction %"),    kind: "pct" },
        { key: "nb",      label: qsTr("Noise blanker %"),      kind: "pct" },
        { key: "comp",    label: qsTr("Speech compressor %"),  kind: "pct" },
        { key: "voxgain", label: qsTr("VOX gain %"),           kind: "pct" },
        { key: "ifshift", label: qsTr("IF shift Hz"),          kind: "int" },
        { key: "notchf",  label: qsTr("Notch Hz"),             kind: "int" },
        { key: "preamp",  label: qsTr("Preamp dB"),            kind: "list" },
        { key: "att",     label: qsTr("Attenuator dB"),        kind: "list" }
    ]
    readonly property var extraFuncs: [
        { key: "nbOn",   label: qsTr("Noise blanker") },
        { key: "nrOn",   label: qsTr("Noise reduction") },
        { key: "anfOn",  label: qsTr("Auto notch") },
        { key: "compOn", label: qsTr("Speech compressor") },
        { key: "voxOn",  label: qsTr("VOX") },
        { key: "lockOn", label: qsTr("Dial lock") }
    ]
    Dialog {
        id: powerOffDialog
        title: qsTr("Power off radio?")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        anchors.centerIn: parent
        Label {
            text: qsTr("This will switch off the radio connected through Hamlib CAT. The CAT session is kept open so you can try Power On afterward.")
            wrapMode: Text.WordWrap
            width: 340
        }
        onAccepted: if (bridge.hamlibCat) bridge.hamlibCat.requestRadioPowerOff()
    }
    Connections {
        target: bridge
        function onCatConnectedChanged() {
            if (!bridge.catConnected) voice.stop()
            if (win.visible) {
                syncRadioModeFromCat(); bridge.ssbRefreshControls()
                if (bridge.catConnected && bridge.hamlibCat) bridge.hamlibCat.requestAtuState()
            }
        }
        function onCatModeChanged() { syncRadioModeFromCat() }
        function onModeChanged() { if (bridge.mode !== "SSB") voice.stop() }
        function onDecoPortUseRemoteChanged() { voice.stop(); bridge.ssbRefreshControls() }
    }
    Connections {
        target: bridge.hamlibCat
        function onRadioActionCompleted(action, ok, error) {
            win.radioActionStatus = ok
                ? (action.toLowerCase().indexOf("atu off") >= 0 ? qsTr("Radio ATU disabled")
                   : action.toLowerCase().indexOf("atu") >= 0 ? qsTr("Radio ATU command sent")
                   : action.toLowerCase().indexOf("on") >= 0 ? qsTr("Radio power-on command sent")
                   : qsTr("Radio power-off command sent"))
                : (error || qsTr("Radio command failed"))
        }
    }
    footer: Frame {
        background: Rectangle { color: "#101923" }
        padding: 14
        ColumnLayout {
            anchors.fill: parent
        Button {
            Layout.fillWidth: true; Layout.preferredHeight: 64
            text: voice.transmitting ? qsTr("TRANSMITTING — release to receive") : qsTr("HOLD TO TALK • PTT")
            font.bold: true; font.pixelSize: 20
            enabled: ready && controls.busy !== true && mic.currentIndex>=0
            onPressed: voice.start()
            onReleased: voice.stop()
            onCanceled: voice.stop()
            background: Rectangle { radius: 8; color: voice.transmitting ? "#b53238" : "#087f92"; opacity: parent.enabled ? 1 : .45 }
        }
        Label { text: voice.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
        Label { text: qsTr("Hold the space bar to talk (Esc stops)."); Layout.fillWidth: true; color: "#6f8294"; font.pixelSize: 12 }
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 14; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "SSB"; font.pixelSize: 26; font.bold: true; color: "#44d4df" }
            Label { text: bridge.decoPortUseRemote ? qsTr("REMOTE RADIO") : qsTr("LOCAL RADIO"); color: "#a9bac9" }
            TextField {
                id: dial
                Layout.preferredWidth: 190; font.pixelSize: 20
                // Consolas is not installed by default on macOS/Linux and
                // makes Qt rebuild its font-family aliases at startup.
                font.family: (typeof decodiumMonoFontFamily !== "undefined" && decodiumMonoFontFamily)
                             ? decodiumMonoFontFamily : "monospace"
                text: (bridge.frequency / 1000000).toFixed(6)
                enabled: ready && controls.busy !== true && !voice.transmitting
                onAccepted: { bridge.cwTuneFrequency(Number(text.replace(",","."))*1000000); focus=false }
                ToolTip.visible: hovered; ToolTip.text: qsTr("Frequency in MHz. Press Enter to tune.")
            }
            Label { text: "MHz" }
            Button { text: "−100 Hz"; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.cwTuneFrequency(bridge.frequency-100) }
            Button { text: "+100 Hz"; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.cwTuneFrequency(bridge.frequency+100) }
            ComboBox { id: side; model: ["USB", "LSB"]; Layout.preferredWidth: 90; enabled: ready && controls.busy !== true && !voice.transmitting; onActivated: win.updateRadioMode() }
            CheckBox { id: dataInput; text: qsTr("USB/data audio input"); checked: true; enabled: ready && controls.busy !== true && !voice.transmitting; onToggled: if(win.ready && !win.syncingRadioMode) win.updateRadioMode() }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Activate SSB"); visible: bridge.mode !== "SSB"; onClicked: bridge.mode="SSB" }
            Label { text: bridge.catConnected ? bridge.catMode : qsTr("CAT disconnected"); color: bridge.catConnected ? "#77e7a9" : "#ffa971" }
        }
        Label {
            text: qsTr("TX uses the radio audio output selected in Settings. USB/LSB requires the radio to accept PC audio; use data input when needed.")
            Layout.fillWidth: true; elide: Text.ElideRight; color: "#6f8294"; font.pixelSize: 11
        }
        SplitView {
            id: split
            Layout.fillWidth: true; Layout.fillHeight: true
            orientation: Qt.Horizontal
            ScrollView {
                id: leftPane
                SplitView.preferredWidth: 680; SplitView.minimumWidth: 380
                contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: leftPane.availableWidth - 10; spacing: 12
            GroupBox {
                title: qsTr("Voice keyer • preregistered messages"); Layout.fillWidth: true
                background: Rectangle { radius: 10; color: "#2a182632"; border.color: "#33506a"; border.width: 1 }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        text: qsTr("Hold Record while speaking, release to save. Play sends the message on SSB; Preview plays it only on this computer. Esc stops.")
                        wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Voice speed"); color: "#a9bac9" }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.6; to: 1.6; stepSize: 0.05; value: voiceKeyer.speed
                            onMoved: voiceKeyer.speed = value
                        }
                        Label { text: voiceKeyer.speed.toFixed(2) + "x"; Layout.preferredWidth: 46; color: "#eaf1f8" }
                    }
                    GridLayout {
                        id: slotGrid
                        Layout.fillWidth: true
                        columns: Math.max(1, Math.floor(width / 470))
                        columnSpacing: 10; rowSpacing: 10
                        Repeater {
                            model: voiceKeyer.messages
                            delegate: Rectangle {
                                id: card
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                implicitHeight: cardCol.implicitHeight + 20
                                radius: 10
                                color: "#33182632"
                                border.color: card.modelData.hasAudio ? "#2f8f6a" : "#2d4256"
                                border.width: 1
                                ColumnLayout {
                                    id: cardCol
                                    anchors.fill: parent; anchors.margins: 10
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: "F" + (card.modelData.index + 1); color: "#44d4df"; font.bold: true; Layout.preferredWidth: 26 }
                                        TextField {
                                            id: keyLabel
                                            Layout.preferredWidth: 110
                                            text: card.modelData.label; maximumLength: 12
                                            onEditingFinished: voiceKeyer.setSlotText(card.modelData.index, text, keyText.text)
                                        }
                                        ComboBox {
                                            Layout.preferredWidth: 72
                                            model: ["it", "en"]
                                            currentIndex: card.modelData.lang === "en" ? 1 : 0
                                            onActivated: voiceKeyer.setSlotLanguage(card.modelData.index, currentText)
                                        }
                                        TextField {
                                            id: keyText
                                            Layout.fillWidth: true
                                            text: card.modelData.text; placeholderText: qsTr("text of the message")
                                            onEditingFinished: voiceKeyer.setSlotText(card.modelData.index, keyLabel.text, text)
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Button {
                                            text: qsTr("Record")
                                            enabled: !voiceKeyer.playing && !voiceKeyer.synthesizing && !(voiceKeyer.recording && !pressed)
                                            onPressed: voiceKeyer.startRecording(card.modelData.index)
                                            onReleased: voiceKeyer.stopRecording()
                                            onCanceled: voiceKeyer.stopRecording()
                                        }
                                        Button {
                                            text: qsTr("Preview")
                                            enabled: card.modelData.text.length > 0 && packs.voiceInstalled
                                                     && !voiceKeyer.playing && !voiceKeyer.recording && !voiceKeyer.synthesizing
                                            onClicked: voiceKeyer.preview(card.modelData.text, card.modelData.lang)
                                        }
                                        Button {
                                            text: qsTr("AI help")
                                            enabled: packs.assistantInstalled && !assistant.busy && card.modelData.text.length > 0
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Ask the assistant to rewrite this text so it reads well on the air")
                                            onClicked: {
                                                win.pendingSlot = card.modelData.index
                                                assistant.ask(qsTr("Rewrite this message so that a radio operator reads it aloud: short, clear, in the same language, no abbreviations that are hard to pronounce. Reply with the message only: ") + card.modelData.text)
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: card.modelData.hasAudio ? qsTr("recorded") : qsTr("text only")
                                            color: card.modelData.hasAudio ? "#77e7a9" : "#6f8294"
                                        }
                                        Button {
                                            text: qsTr("Clear")
                                            enabled: card.modelData.hasAudio && !voiceKeyer.playing && !voiceKeyer.recording
                                            onClicked: voiceKeyer.clearSlot(card.modelData.index)
                                        }
                                        Button {
                                            text: qsTr("Play")
                                            highlighted: true
                                            enabled: (card.modelData.hasAudio || (card.modelData.text.length > 0 && packs.voiceInstalled))
                                                     && ready && controls.busy !== true
                                                     && !voiceKeyer.playing && !voiceKeyer.recording && !voiceKeyer.synthesizing && !voice.transmitting
                                            onClicked: voiceKeyer.play(card.modelData.index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        BusyIndicator { running: voiceKeyer.synthesizing; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                        Label { text: voiceKeyer.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: qsTr("Hold to listen")
                            enabled: !voiceKeyer.playing && !voiceKeyer.recording && !speech.working && packs.speechInstalled
                            onPressed: speech.startListening(keyLangBox.currentText)
                            onReleased: speech.stopListening()
                            onCanceled: speech.stopListening()
                        }
                        ComboBox {
                            id: keyLangBox
                            Layout.preferredWidth: 72
                            model: ["it", "en"]
                        }
                        TextField {
                            Layout.fillWidth: true
                            readOnly: true
                            text: speech.transcript
                            placeholderText: qsTr("What you said appears here")
                        }
                        Button {
                            text: qsTr("Clear"); enabled: speech.transcript.length > 0
                            onClicked: speech.clearTranscript()
                        }
                    }
                    Label { text: speech.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                }
            }
            GroupBox {
                title: qsTr("Assistant • helps you write and read your messages"); Layout.fillWidth: true
                // Pannello trasparente: lascia vedere lo sfondo, la risposta e' sempre leggibile.
                background: Rectangle {
                    radius: 12; color: "#22182632"; border.color: "#4d8aa3"; border.width: 1
                }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        visible: !packs.assistantInstalled
                        text: qsTr("Install the Assistant pack (in the right column) to chat here. Nothing leaves this computer.")
                        wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#ffa971"
                    }
                    Label {
                        visible: packs.assistantInstalled
                        text: qsTr("Ask for a CQ call, a signal report, a short reply. Press AI help on a message to have it rewritten for the voice.")
                        wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9"
                    }
                    TextArea {
                        Layout.fillWidth: true; Layout.preferredHeight: 190
                        readOnly: true; wrapMode: TextArea.Wrap
                        text: assistant.transcript
                        placeholderText: qsTr("The conversation appears here")
                        color: "#eaf1f8"
                        background: Rectangle { radius: 8; color: "#18101923"; border.color: "#2d4256" }
                        onTextChanged: cursorPosition = length
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: askField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Ask something…")
                            enabled: packs.assistantInstalled && !assistant.busy
                            onAccepted: if (text.length > 0) { assistant.ask(text); text = "" }
                        }
                        Button {
                            text: qsTr("Ask"); highlighted: true
                            enabled: packs.assistantInstalled && !assistant.busy && askField.text.length > 0
                            onClicked: { assistant.ask(askField.text); askField.text = "" }
                        }
                        Button { text: qsTr("Stop"); visible: assistant.busy; onClicked: assistant.cancel() }
                        Button { text: qsTr("Clear"); enabled: !assistant.busy; onClicked: assistant.clear() }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Button {
                            text: qsTr("Preview reply")
                            enabled: assistant.lastReply.length > 0 && packs.voiceInstalled
                                     && !voiceKeyer.playing && !voiceKeyer.synthesizing && !assistant.busy
                            onClicked: voiceKeyer.preview(assistant.lastReply, keyLangBox.currentText)
                        }
                        Button {
                            // Trasmette: la voce esce in aria, con il PTT. Per questo il nome lo dice.
                            text: qsTr("Send reply by voice (transmits)")
                            enabled: assistant.lastReply.length > 0 && packs.voiceInstalled && ready
                                     && !voiceKeyer.playing && !voiceKeyer.synthesizing && !assistant.busy && controls.busy !== true
                            onClicked: voiceKeyer.sayText(assistant.lastReply, keyLangBox.currentText)
                        }
                        BusyIndicator { running: assistant.busy; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                        Label { text: assistant.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                    }
                }
            }
                }
            }
            ScrollView {
                id: rightPane
                SplitView.fillWidth: true; SplitView.minimumWidth: 340
                contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: rightPane.availableWidth - 10; spacing: 12
            GroupBox {
                title: qsTr("Optional packs • install only what you use"); Layout.fillWidth: true
                background: Rectangle { radius: 10; color: "#2a182632"; border.color: "#33506a"; border.width: 1 }
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        text: qsTr("Install only what you need. Untick Use to switch a pack off without deleting it; Remove frees its disk space. Nothing goes to the internet during use, and no account is needed.")
                        wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9"
                    }
                    Repeater {
                        model: [
                            { key: 0, name: qsTr("Voice (Italian and English)"), size: "150 MB" },
                            { key: 1, name: qsTr("Speech recognition"), size: "490 MB" },
                            { key: 2, name: qsTr("Assistant (local language model)"), size: "1.1 GB" }
                        ]
                        delegate: RowLayout {
                            id: packRow
                            required property var modelData
                            readonly property bool isOn: packRow.modelData.key === 0 ? packs.voiceInstalled
                                : packRow.modelData.key === 1 ? packs.speechInstalled : packs.assistantInstalled
                            Layout.fillWidth: true
                            Label { text: packRow.modelData.name; Layout.fillWidth: true }
                            Label { text: packRow.modelData.size; color: "#6f8294"; Layout.preferredWidth: 70 }
                            Label {
                                text: packRow.isOn ? qsTr("installed") : qsTr("not installed")
                                color: packRow.isOn ? "#77e7a9" : "#a9bac9"
                                Layout.preferredWidth: 110
                            }
                            CheckBox {
                                // Selezione: un pacchetto installato si puo' spegnere senza cancellarlo.
                                text: qsTr("Use")
                                visible: packRow.isOn
                                checked: packs.isEnabled(packRow.modelData.key)
                                onToggled: packs.setEnabled(packRow.modelData.key, checked)
                            }
                            Button {
                                text: qsTr("Install")
                                visible: !packRow.isOn
                                enabled: !packs.busy
                                onClicked: packs.install(packRow.modelData.key)
                            }
                            Button {
                                text: qsTr("Remove")
                                visible: packRow.isOn
                                enabled: !packs.busy && !voiceKeyer.playing && !voiceKeyer.recording && !assistant.busy
                                onClicked: packs.remove(packRow.modelData.key)
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: packs.busy
                        ProgressBar { Layout.fillWidth: true; from: 0; to: 1; value: packs.progress }
                        Button { text: qsTr("Cancel"); onClicked: packs.cancel() }
                    }
                    Label { text: packs.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                }
            }
            GroupBox {
                title: qsTr("PC microphone • voice processing"); Layout.fillWidth: true
                background: Rectangle { radius: 10; color: "#2a182632"; border.color: "#33506a"; border.width: 1 }
                ColumnLayout {
                    anchors.fill: parent
                    ComboBox {
                        id: mic; Layout.fillWidth: true; model: voice.microphones; textRole: "name"; valueRole: "id"
                        enabled: !voice.transmitting
                        currentIndex: { for(var i=0;i<voice.microphones.length;i++)if(voice.microphones[i].id===voice.microphone)return i;return -1 }
                        displayText: currentIndex < 0 ? qsTr("Select PC microphone…") : currentText
                        onActivated: voice.microphone=currentValue
                    }
                    RowLayout {
                        Label { text: qsTr("Mic gain") }
                        Slider { Layout.fillWidth: true; from: -20; to: 30; stepSize: 1; value: voice.gainDb; onMoved: voice.gainDb=value }
                        Label { text: voice.gainDb.toFixed(0)+" dB"; Layout.preferredWidth: 50 }
                        CheckBox { text: qsTr("Automatic gain"); checked: voice.automatic; onToggled: voice.automatic=checked }
                    }
                    RowLayout {
                        Label { text: qsTr("Voice filter") }
                        SpinBox { from: 80; to: 500; stepSize: 20; value: voice.lowCut; onValueModified: voice.lowCut=value }
                        Label { text: "–" }
                        SpinBox { from: 1800; to: 3500; stepSize: 100; value: voice.highCut; onValueModified: voice.highCut=value }
                        Label { text: qsTr("Hz • limiter active"); Layout.fillWidth: true }
                    }
                    RowLayout {
                        Label { text: qsTr("Voice level") }
                        ProgressBar { from: 0; to: 1; value: voice.micPeak; Layout.fillWidth: true }
                        Label { text: voice.micPeak>.001 ? (20*Math.log(voice.micPeak)/Math.LN10).toFixed(0)+" dBFS" : "—"; Layout.preferredWidth: 65 }
                    }
                }
            }
            GroupBox {
                title: qsTr("Radio • CAT controls"); Layout.fillWidth: true
                background: Rectangle { radius: 10; color: "#2a182632"; border.color: "#33506a"; border.width: 1 }
                GridLayout {
                    anchors.fill: parent; columns: 4; columnSpacing: 12
                    Label { text: qsTr("RF power %") }
                    SpinBox { from: 0; to: 100; value: controls.power === undefined ? 0 : Math.round(controls.power); enabled: ready && controls.busy !== true && !voice.transmitting && controls.powerSupported === true; onValueModified: bridge.ssbSetControl("power",value) }
                    Label { text: qsTr("RX filter Hz") }
                    SpinBox { from: 300; to: 4000; stepSize: 100; value: controls.filter === undefined ? 2400 : controls.filter; enabled: ready && controls.busy !== true && !voice.transmitting && controls.filterSupported === true; onValueModified: bridge.ssbSetControl("filter",value) }
                    Label { text: qsTr("Radio AGC") }
                    DecoComboBox {
                        id: agcBox
                        model: controls.agcModes || []; textRole: "name"; valueRole: "value"
                        enabled: ready && controls.busy !== true && !voice.transmitting && controls.agcSupported === true
                        currentIndex: { var a=controls.agcModes || [];for(var i=0;i<a.length;i++)if(a[i].value===controls.agc)return i;return -1 }
                        onActivated: {
                            // Read the numeric value from the selected model row.
                            // This avoids relying on the style ComboBox's currentValue
                            // property, which can briefly expose the row index while
                            // a refreshed AGC model is being installed.
                            var a = controls.agcModes || [];
                            var row = a[currentIndex];
                            var agcValue = row && row.value !== undefined ? Number(row.value) : Number(currentValue);
                            bridge.ssbSetControl("agc", agcValue);
                        }
                    }
                    Label { text: qsTr("RF gain %") }
                    SpinBox { from: 0; to: 100; value: controls.rf === undefined ? 0 : Math.round(controls.rf); enabled: ready && controls.busy !== true && !voice.transmitting && controls.rfSupported === true; onValueModified: bridge.ssbSetControl("rf",value) }
                    Label { text: qsTr("Radio mic gain %") }
                    SpinBox { from: 0; to: 100; value: controls.mic === undefined ? 0 : Math.round(controls.mic); enabled: ready && controls.busy !== true && !voice.transmitting && controls.micSupported === true; onValueModified: bridge.ssbSetControl("mic",value) }
                    Button { text: qsTr("Read radio"); Layout.columnSpan: 2; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: { bridge.ssbRefreshControls(); if (bridge.hamlibCat) bridge.hamlibCat.requestAtuState() } }
                    Label {
                        text: qsTr("ATU state: ") + (bridge.hamlibCat && bridge.hamlibCat.connected
                              ? (bridge.hamlibCat.atuStateValid
                                 ? (bridge.hamlibCat.atuEnabled ? qsTr("ON") : qsTr("OFF"))
                                 : qsTr("unknown"))
                              : qsTr("—"))
                        Layout.columnSpan: 2
                    }
                    Button {
                        text: qsTr("Read ATU state")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        onClicked: bridge.hamlibCat.requestAtuState()
                    }
                    Button {
                        text: qsTr("ATU Tune")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Ask the radio to start its internal antenna tuner.")
                        onClicked: bridge.hamlibCat.requestAtuTune()
                    }
                    Button {
                        text: qsTr("ATU OFF")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Disable the radio's automatic antenna tuner.")
                        onClicked: bridge.hamlibCat.requestAtuOff()
                    }
                    Button {
                        text: qsTr("Power on radio")
                        Layout.columnSpan: 2
                        enabled: bridge.mode === "SSB" && controls.busy !== true && !voice.transmitting
                                 && win.directCatPowerOnAvailable
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Ask a supported radio to power on through direct Hamlib CAT. The selected CAT port must remain available while the radio is in standby.")
                        onClicked: bridge.hamlibCat.requestRadioPowerOn()
                    }
                    Button {
                        text: qsTr("Power off radio")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        onClicked: powerOffDialog.open()
                    }
                    Label {
                        text: win.radioActionStatus
                        visible: text.length > 0
                        color: text.indexOf(qsTr("failed")) >= 0 ? "#ffa971" : "#77e7a9"
                        Layout.columnSpan: 4
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Label { text: qsTr("AGC OFF enables manual reception gain using RF gain. Unavailable controls are disabled."); Layout.columnSpan: 4; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                    Label { text: controls.error || ""; visible: text.length>0; color: "#ffa971"; Layout.columnSpan: 4; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            GroupBox {
                title: qsTr("Receive • radio settings (read and set by CAT)"); Layout.fillWidth: true
                background: Rectangle { radius: 10; color: "#2a182632"; border.color: "#33506a"; border.width: 1 }
                visible: ready && bridge.catConnected
                GridLayout {
                    anchors.fill: parent; columns: 2; columnSpacing: 16; rowSpacing: 4
                    Repeater {
                        model: win.extraLevels
                        delegate: RowLayout {
                            id: lvl
                            required property var modelData
                            readonly property bool canEdit: win.ready && controls.busy !== true && !voice.transmitting
                            readonly property var cur: controls[modelData.key]
                            visible: controls[modelData.key + "Supported"] === true
                            Layout.columnSpan: 2; Layout.fillWidth: true
                            Label { text: lvl.modelData.label; Layout.preferredWidth: 170 }
                            Slider {
                                visible: lvl.modelData.kind === "pct"; Layout.fillWidth: true
                                from: 0; to: 100; stepSize: 1
                                value: lvl.cur === undefined ? 0 : lvl.cur
                                enabled: lvl.canEdit
                                onPressedChanged: if (!pressed) bridge.ssbSetControl(lvl.modelData.key, Math.round(value))
                            }
                            Label {
                                visible: lvl.modelData.kind === "pct"; Layout.preferredWidth: 50
                                text: Math.round(lvl.cur === undefined ? 0 : lvl.cur) + " %"
                            }
                            SpinBox {
                                visible: lvl.modelData.kind === "int"
                                from: controls[lvl.modelData.key + "Min"] === undefined ? -1200 : controls[lvl.modelData.key + "Min"]
                                to: controls[lvl.modelData.key + "Max"] === undefined ? 1200 : controls[lvl.modelData.key + "Max"]
                                stepSize: 50
                                value: lvl.cur === undefined ? 0 : lvl.cur
                                enabled: lvl.canEdit
                                onValueModified: bridge.ssbSetControl(lvl.modelData.key, value)
                            }
                            ComboBox {
                                visible: lvl.modelData.kind === "list"
                                model: controls[lvl.modelData.key + "Values"] || []
                                enabled: lvl.canEdit
                                currentIndex: { var a = controls[lvl.modelData.key + "Values"] || []; for (var i = 0; i < a.length; i++) if (a[i] === lvl.cur) return i; return -1 }
                                displayText: currentIndex < 0 ? "—" : (model[currentIndex] === 0 ? qsTr("Off") : model[currentIndex] + " dB")
                                onActivated: { var a = controls[lvl.modelData.key + "Values"] || []; bridge.ssbSetControl(lvl.modelData.key, Number(a[currentIndex])) }
                            }
                            Item { Layout.fillWidth: true; visible: lvl.modelData.kind !== "pct" }
                        }
                    }
                    Repeater {
                        model: win.extraFuncs
                        delegate: CheckBox {
                            id: fn
                            required property var modelData
                            text: modelData.label
                            visible: controls[modelData.key + "Supported"] === true
                            checked: controls[modelData.key] === true
                            enabled: win.ready && controls.busy !== true && !voice.transmitting
                            onToggled: {
                                bridge.ssbSetControl(modelData.key, checked ? 1 : 0)
                                checked = Qt.binding(function () { return controls[fn.modelData.key] === true })
                            }
                        }
                    }
                }
            }
            RowLayout {
                Label { text: "ALC: "+(bridge.rigAlcValid ? Math.round(bridge.rigAlc)+" %" : "—"); font.pixelSize: 18 }
                Label { text: qsTr("RF: ")+(bridge.rigPowerWatts > 0 ? bridge.rigPowerWatts.toFixed(1)+" W" : "—"); Layout.fillWidth: true; font.pixelSize: 18 }
                Label { text: qsTr("SWR: ")+(bridge.rigSwr >= 1 ? bridge.rigSwr.toFixed(1) : "—"); font.pixelSize: 18 }
            }
            RowLayout {
                CheckBox { text: qsTr("Listen RX"); checked: bridge.decoPortMonitor; enabled: !voice.transmitting; onToggled: { bridge.decoPortMonitor=checked; if(checked)bridge.startRx() } }
                Slider { from: 0; to: 100; stepSize: 1; value: bridge.remoteRxVolume; onMoved: bridge.remoteRxVolume=value; Layout.fillWidth: true }
                Label { text: bridge.remoteRxVolume+" %" }
            }
            RowLayout {
                Label { text: qsTr("Listen AGC") }
                ComboBox {
                    id: rxAgc
                    model: [qsTr("Off"), qsTr("Auto"), qsTr("Fast"), qsTr("Medium"), qsTr("Slow"), qsTr("Long")]
                    currentIndex: bridge.ssbRxAgcMode
                    onActivated: bridge.ssbRxAgcMode = currentIndex
                }
                Label { text: qsTr("Threshold"); visible: bridge.ssbRxAgcMode > 1 }
                Slider {
                    visible: bridge.ssbRxAgcMode > 1; Layout.fillWidth: true
                    from: -120; to: -20; stepSize: 1; value: bridge.ssbRxAgcThreshold
                    onMoved: bridge.ssbRxAgcThreshold = value
                }
                Label { visible: bridge.ssbRxAgcMode > 1; text: bridge.ssbRxAgcThreshold + " dBFS"; Layout.preferredWidth: 80 }
                Item { Layout.fillWidth: true; visible: bridge.ssbRxAgcMode <= 1 }
                Label { visible: bridge.ssbRxAgcMode > 0; text: qsTr("Gain %1 dB").arg(bridge.ssbRxAgcGainDb.toFixed(0)); Layout.preferredWidth: 90 }
            }
            Label {
                text: qsTr("Software AGC on the listening audio only (from DECODIUM SDR). Auto follows the noise floor by itself. It does not touch the decoder or the radio's AGC.")
                wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9"
            }
            ComboBox {
                Layout.fillWidth: true; model: bridge.cwMonitorOutputs(); textRole: "label"; valueRole: "id"
                enabled: !voice.transmitting
                currentIndex: { for(var i=0;i<model.length;i++)if(model[i].id===bridge.remoteRxDevice)return i;return -1 }
                onActivated: bridge.remoteRxDevice=currentValue
            }

                }
            }
        }
    }
}
