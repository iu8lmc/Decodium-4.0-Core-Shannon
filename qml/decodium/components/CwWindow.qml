/* CwWindow - il modulo CW di Decodium: decodificatore, macro e manipolatore.
 *
 * Viene da DecoDXLog. Il testo parte come audio (sidetone) dal percorso di
 * trasmissione di Decodium, e per questo funziona anche con una radio remota
 * (DecoPort, Decolink); in alternativa da un manipolatore su una porta seriale
 * (DTR/RTS) o da un K1EL WinKeyer. Il decodificatore ascolta l'audio della
 * radio, locale o remota.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win

    readonly property var eng: (typeof appEngine !== 'undefined' ? appEngine : null)
    readonly property var cw: (typeof cwModule !== 'undefined' ? cwModule : null)
    readonly property var tm: eng ? eng.themeManager : null
    readonly property color cBg:     tm ? tm.bgDeep        : "#0a0f14"
    readonly property color cPanel:  tm ? tm.panelColor    : "#111a22"
    readonly property color cBorder: tm ? tm.borderColor   : "#22303c"
    readonly property color cText:   tm ? tm.textPrimary   : "#dbe4ea"
    readonly property color cDim:    tm ? tm.textSecondary : "#7d8b96"
    readonly property color cAccent: tm ? tm.accentColor   : "#19d0ff"
    readonly property color cOk:     tm ? tm.successColor  : "#25d366"
    readonly property color cWarn:   tm ? tm.warningColor  : "#ffb84a"
    readonly property color cBad:    "#ff5c5c"
    readonly property string mono:   (typeof decodiumMonoFontFamily !== 'undefined') ? decodiumMonoFontFamily : "monospace"

    readonly property bool canSend: !!(cw && cw.canSend)
    readonly property bool remote: !!(cw && cw.remoteRadio)

    title: qsTr("CW") + (eng && eng.catConnected ? " - " + eng.catRigName : "")
    width: 920
    height: 760
    minimumWidth: 640
    minimumHeight: 520
    color: cBg

    // Le proprieta' Material si attaccano all'albero di UNA finestra: questa e'
    // una finestra a se', e senza queste righe ogni ComboBox e Slider resta col
    // tema chiaro di serie.
    Material.theme: (tm && tm.isLightTheme) ? Material.Light : Material.Dark
    Material.accent: cAccent
    Material.foreground: cText
    Material.background: cBg

    // Il decodificatore riceve l'audio solo mentre la finestra e' aperta.
    onVisibleChanged: if (eng) eng.cwInAscolto = visible
    Component.onDestruction: if (eng) eng.cwInAscolto = false

    function ctx() {
        return {
            call: callField.text.trim(),
            rst: rstField.text.trim(),
            nr: nrField.text.trim(),
            exch: exchField.text.trim(),
            name: nameField.text.trim()
        }
    }
    function say(text, level) {
        statusText.text = text
        statusText.color = level === "warning" ? cWarn : cDim
        statusTimer.restart()
    }

    Connections {
        target: win.cw
        function onMessage(text, level) { win.say(text, level) }
    }
    Connections {
        target: win.eng
        function onDxCallChanged() { if (win.eng) callField.text = win.eng.dxCall }
    }
    Timer { id: statusTimer; interval: 7000; onTriggered: statusText.text = "" }

    component Lbl: Text {
        color: win.cDim
        font.pixelSize: 11
    }
    component Btn: Rectangle {
        id: btn
        property string label: ""
        property bool danger: false
        property bool live: true
        property bool armed: false
        signal clicked()
        implicitWidth: btnTxt.implicitWidth + 22
        implicitHeight: 28
        radius: 5
        opacity: btn.live ? 1.0 : 0.4
        readonly property color tone: btn.danger ? win.cBad : win.cAccent
        color: btn.armed ? Qt.rgba(tone.r, tone.g, tone.b, 0.30)
                         : (btnMA.containsMouse && btn.live ? Qt.rgba(tone.r, tone.g, tone.b, 0.16) : "transparent")
        border.width: 1
        border.color: btn.tone
        Text {
            id: btnTxt
            anchors.centerIn: parent
            text: btn.label
            color: btn.tone
            font.pixelSize: 11
            font.bold: true
        }
        MouseArea {
            id: btnMA
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: btn.live ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (btn.live) btn.clicked()
        }
    }
    component Field: TextField {
        color: win.cText
        placeholderTextColor: win.cDim
        selectByMouse: true
        font.pixelSize: 12
        font.family: win.mono
        background: Rectangle {
            radius: 4
            color: "transparent"
            border.width: 1
            border.color: parent.activeFocus ? win.cAccent : win.cBorder
        }
    }

    // ── Le macro si scrivono qui ──────────────────────────────────────────
    Popup {
        id: macroEditor
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        width: Math.min(win.width - 40, 760)
        height: Math.min(win.height - 60, 560)
        padding: 14
        background: Rectangle { color: win.cPanel; border.color: win.cBorder; radius: 8 }
        onOpened: macroList.model = win.cw ? win.cw.macros : []

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Text {
                text: qsTr("MACROS")
                color: win.cAccent
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.cDim
                font.pixelSize: 11
                text: qsTr("Holes filled when sent: {MYCALL} {CALL} {RST} {NR} {EXCH} {NAME}. Anything left empty is dropped.")
            }
            ListView {
                id: macroList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    width: macroList.width - 12
                    spacing: 6
                    Field {
                        Layout.preferredWidth: 130
                        text: modelData.label
                        onEditingFinished: win.cw.setMacro(index, text, modelData.text)
                    }
                    Field {
                        Layout.fillWidth: true
                        text: modelData.text
                        onEditingFinished: win.cw.setMacro(index, modelData.label, text)
                    }
                    Btn {
                        label: "×"
                        danger: true
                        live: macroList.count > 1
                        onClicked: win.cw.removeMacro(index)
                    }
                }
            }
            RowLayout {
                spacing: 8
                Btn { label: qsTr("Add a key"); onClicked: win.cw.addMacro() }
                Btn { label: qsTr("Restore the defaults"); danger: true; onClicked: win.cw.resetMacros() }
                Item { Layout.fillWidth: true }
                Btn { label: qsTr("Close"); onClicked: macroEditor.close() }
            }
        }
    }
    Connections {
        target: win.cw
        function onMacrosChanged() { if (macroEditor.visible) macroList.model = win.cw.macros }
    }

    // ── Come si manda il CW ───────────────────────────────────────────────
    Popup {
        id: setup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        width: Math.min(win.width - 40, 560)
        padding: 16
        background: Rectangle { color: win.cPanel; border.color: win.cBorder; radius: 8 }

        ColumnLayout {
            width: parent.width
            spacing: 10
            Text {
                text: qsTr("CW TRANSMIT")
                color: win.cAccent
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Lbl { text: qsTr("Send CW as") }
                ComboBox {
                    id: backendBox
                    Layout.fillWidth: true
                    readonly property var keys: ["audio", "serial", "winkeyer"]
                    model: [qsTr("Audio tone (works with a remote radio)"),
                            qsTr("Serial keyer (DTR / RTS)"),
                            qsTr("K1EL WinKeyer")]
                    currentIndex: Math.max(0, keys.indexOf(win.cw ? win.cw.txBackend : "audio"))
                    onActivated: (i) => win.cw.txBackend = keys[i]
                }
                Lbl { text: qsTr("Port"); visible: backendBox.currentIndex > 0 }
                RowLayout {
                    visible: backendBox.currentIndex > 0
                    Layout.fillWidth: true
                    ComboBox {
                        id: portBox
                        Layout.fillWidth: true
                        editable: true
                        model: win.cw ? win.cw.availablePorts() : []
                        function refresh() { model = win.cw ? win.cw.availablePorts() : [] }
                        onAccepted: apply(editText)
                        onActivated: apply(currentText)
                        function apply(p) {
                            if (backendBox.currentIndex === 1) win.cw.keyerPort = p
                            else win.cw.winKeyerPort = p
                        }
                        Component.onCompleted: editText = backendBox.currentIndex === 1
                                                          ? win.cw.keyerPort : win.cw.winKeyerPort
                    }
                    Btn { label: qsTr("Refresh"); onClicked: portBox.refresh() }
                }
                Lbl { text: qsTr("Key line"); visible: backendBox.currentIndex === 1 }
                ComboBox {
                    visible: backendBox.currentIndex === 1
                    model: ["DTR", "RTS"]
                    currentIndex: win.cw && win.cw.keyerLine === "RTS" ? 1 : 0
                    onActivated: (i) => win.cw.keyerLine = model[i]
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.remote ? win.cWarn : win.cDim
                font.pixelSize: 11
                text: win.remote
                      ? qsTr("A remote radio is in use: the CW goes out as an audio tone through the link, whatever is chosen here.")
                      : qsTr("The audio tone needs the radio in USB or DATA-U. A serial keyer or a WinKeyer keys the radio's own key input and needs the radio in CW.")
            }
            RowLayout {
                spacing: 8
                Btn { label: qsTr("Test (V)"); live: win.canSend; onClicked: win.cw.testKeyer() }
                Text {
                    color: win.cDim
                    font.pixelSize: 11
                    visible: !!win.cw && win.cw.winKeyerVersion > 0
                    text: qsTr("WinKeyer version %1").arg(win.cw ? win.cw.winKeyerVersion : 0)
                }
                Item { Layout.fillWidth: true }
                Btn { label: qsTr("Close"); onClicked: setup.close() }
            }
        }
    }

    // ── La finestra ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                width: 10; height: 10; radius: 5
                color: win.canSend ? win.cOk : win.cDim
            }
            Text {
                color: win.cText
                font.pixelSize: 12
                font.family: win.mono
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: (win.eng && win.eng.catConnected
                       ? (win.eng.frequency / 1e6).toFixed(6) + " MHz  " + win.eng.catMode
                       : qsTr("no CAT")) + "   ·   "
                      + (win.cw ? (win.cw.effectiveBackend === "audio" ? qsTr("audio tone")
                                   : win.cw.effectiveBackend === "serial" ? qsTr("serial keyer") : "WinKeyer") : "")
                      + (win.remote ? "   ·   " + qsTr("remote radio") : "")
            }
            Btn { label: qsTr("Stop"); danger: true; armed: !!win.cw && win.cw.sending; onClicked: win.cw.stop() }
            Btn { label: qsTr("Macros…"); onClicked: macroEditor.open() }
            Btn { label: qsTr("Setup…"); onClicked: setup.open() }
        }

        Text {
            Layout.fillWidth: true
            visible: !win.canSend
            wrapMode: Text.Wrap
            color: win.cWarn
            font.pixelSize: 12
            text: !win.cw ? ""
                  : (win.cw.effectiveBackend === "audio"
                     ? qsTr("CW transmit is not ready: it needs the radio connected and a transmit audio output. The decoder works anyway, it only needs the radio audio.")
                     : qsTr("The keyer is not open: choose its port in Setup. The decoder works anyway."))
        }

        // ── Con chi si parla ────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Lbl { text: qsTr("Call") }
            Field { id: callField; Layout.preferredWidth: 120; text: win.eng ? win.eng.dxCall : ""; font.capitalization: Font.AllUppercase }
            Lbl { text: qsTr("RST") }
            Field { id: rstField; Layout.preferredWidth: 56; text: "599" }
            Lbl { text: qsTr("Rcvd") }
            Field { id: rcvdField; Layout.preferredWidth: 56; text: "599" }
            Lbl { text: qsTr("Nr") }
            Field { id: nrField; Layout.preferredWidth: 64 }
            Lbl { text: qsTr("Exch") }
            Field { id: exchField; Layout.preferredWidth: 90; font.capitalization: Font.AllUppercase }
            Lbl { text: qsTr("Name") }
            Field { id: nameField; Layout.fillWidth: true }
            Btn {
                label: qsTr("Log QSO")
                live: !!win.eng && callField.text.trim().length > 0
                onClicked: {
                    const ok = win.eng.registraQsoCw(callField.text, rstField.text, rcvdField.text,
                                                     nameField.text, "", "")
                    win.say(ok ? qsTr("QSO logged") : qsTr("Nothing to log: enter the call"),
                            ok ? "info" : "warning")
                }
            }
        }

        // ── Le macro: tasti tutti uguali, tasto destro per cambiarli ────
        GridLayout {
            Layout.fillWidth: true
            columns: win.width > 860 ? 6 : 4
            columnSpacing: 6
            rowSpacing: 6
            Repeater {
                id: macroKeys
                model: win.cw ? win.cw.macros : []
                delegate: Item {
                    id: keyItem
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    implicitHeight: 30
                    readonly property bool active: !!win.cw && win.cw.activeMacroIndex === index
                    Rectangle {
                        anchors.fill: parent
                        radius: 5
                        opacity: win.canSend ? 1.0 : 0.45
                        color: keyItem.active ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.30)
                               : (keyMA.pressed ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.22)
                                  : (keyMA.containsMouse ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.12) : "transparent"))
                        border.width: 1
                        border.color: keyItem.active ? win.cAccent : win.cBorder
                        Text {
                            anchors.fill: parent
                            anchors.margins: 4
                            text: keyItem.modelData.label.trim().length > 0 ? keyItem.modelData.label : "F" + (keyItem.index + 1)
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: keyItem.active ? win.cAccent : win.cText
                            font.family: win.mono
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                    MouseArea {
                        id: keyMA
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) macroEditor.open()
                            else if (win.canSend) win.cw.sendMacro(keyItem.index, win.ctx())
                        }
                    }
                    ToolTip.visible: keyMA.containsMouse
                    ToolTip.delay: 600
                    ToolTip.text: keyItem.modelData.text + "\n" + qsTr("Right click: change it")
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Lbl { text: qsTr("Speed") }
            Slider {
                Layout.fillWidth: true
                from: 5
                to: 60
                stepSize: 1
                value: win.cw ? win.cw.wpm : 20
                onMoved: win.cw.wpm = Math.round(value)
            }
            Text {
                text: qsTr("%1 wpm").arg(win.cw ? win.cw.wpm : 20)
                color: win.cText
                font.family: win.mono
                font.pixelSize: 12
                font.bold: true
            }
        }

        // ── Scrivere a mano quello che non sta in una macro ─────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Field {
                id: freeText
                Layout.fillWidth: true
                font.capitalization: Font.AllUppercase
                placeholderText: qsTr("write here and press Enter: it goes out in CW")
                enabled: win.canSend
                onAccepted: { win.cw.sendText(text, win.ctx()); text = "" }
            }
            Btn {
                label: qsTr("Send")
                live: win.canSend && freeText.text.trim().length > 0
                onClicked: { win.cw.sendText(freeText.text, win.ctx()); freeText.text = "" }
            }
        }

        // ── Il decodificatore ──────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Switch {
                text: qsTr("Decoder")
                checked: win.cw ? win.cw.decoderOn : true
                onToggled: win.cw.decoderOn = checked
            }
            ColumnLayout {
                spacing: 1
                Lbl { text: qsTr("Decoder tone") }
                ComboBox {
                    id: toneBox
                    implicitWidth: 120
                    readonly property var choices: [0, 400, 500, 550, 570, 575, 600, 610, 615, 620, 625, 630, 650, 700, 800, 1000]
                    model: choices.map(hz => hz === 0 ? qsTr("Auto") : hz + " Hz")
                    currentIndex: win.cw ? choices.indexOf(win.cw.decoderToneLock) : 0
                    onActivated: (row) => win.cw.decoderToneLock = choices[row]
                }
            }
            ColumnLayout {
                spacing: 1
                Lbl { text: qsTr("Decoder speed") }
                ComboBox {
                    id: speedBox
                    implicitWidth: 116
                    readonly property var choices: [0, 10, 12, 15, 16, 17, 18, 19, 20, 22, 25, 27, 30, 35, 40, 45, 50]
                    model: choices.map(w => w === 0 ? qsTr("Auto") : w + " WPM")
                    currentIndex: win.cw ? choices.indexOf(win.cw.decoderSpeedLock) : 0
                    onActivated: (row) => win.cw.decoderSpeedLock = choices[row]
                }
            }
            Text {
                Layout.fillWidth: true
                readonly property var info: win.cw ? win.cw.decoderScope : null
                readonly property bool readingNow: !!info && info.reading === true
                visible: !!win.cw && win.cw.decoderOn
                elide: Text.ElideRight
                text: {
                    if (!info) return ""
                    const speed = win.cw.decoderSpeedLock > 0 ? qsTr("fixed %1 WPM").arg(win.cw.decoderSpeedLock)
                                                              : qsTr("%1 WPM").arg(Math.round(info.wpm || 0))
                    if (win.cw.decoderToneLock > 0)
                        return readingNow ? qsTr("fixed %1 Hz · %2").arg(win.cw.decoderToneLock).arg(speed)
                                          : qsTr("fixed %1 Hz · noise").arg(win.cw.decoderToneLock)
                    return readingNow ? qsTr("auto · %1 · %2 Hz").arg(speed).arg(Math.round(info.pitch || 0))
                                      : qsTr("listening…")
                }
                color: readingNow ? win.cOk : win.cWarn
                font.family: win.mono
                font.pixelSize: 11
            }
            Btn { label: qsTr("Clear"); onClicked: win.cw.clearDecoder() }
        }

        // Il grafico come quello di ggmorse: sopra i segni come li sta
        // leggendo, sotto il segnale filtrato sul tono con la soglia.
        Rectangle {
            id: scope
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            visible: !!win.cw && win.cw.decoderOn
            color: Qt.darker(win.cPanel, 1.4)
            border.color: win.cBorder
            radius: 4
            clip: true

            readonly property var info: win.cw ? win.cw.decoderScope : null
            readonly property var trace: info && info.signal ? info.signal : []
            readonly property real level: info && info.level !== undefined ? info.level : 0
            readonly property bool reading: info ? info.reading === true : false
            onInfoChanged: plot.requestPaint()

            Canvas {
                id: plot
                anchors.fill: parent
                anchors.margins: 4
                anchors.bottomMargin: 18
                renderStrategy: Canvas.Cooperative
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const sig = scope.trace
                    const n = sig.length
                    if (n < 2 || width <= 0)
                        return
                    const keyH = 12
                    const top = keyH + 6
                    const h = height - top
                    const dx = width / (n - 1)
                    ctx.fillStyle = win.cOk
                    ctx.globalAlpha = scope.reading ? 0.9 : 0.25
                    let start = -1
                    for (let i = 0; i <= n; ++i) {
                        const on = i < n && sig[i] > scope.level
                        if (on && start < 0) start = i
                        else if (!on && start >= 0) {
                            ctx.fillRect(start * dx, 1, Math.max(1.5, (i - start) * dx), keyH - 2)
                            start = -1
                        }
                    }
                    ctx.globalAlpha = 1
                    ctx.beginPath()
                    ctx.moveTo(0, top + h)
                    for (let i = 0; i < n; ++i) ctx.lineTo(i * dx, top + h - sig[i] * h)
                    ctx.lineTo(width, top + h)
                    ctx.closePath()
                    ctx.fillStyle = Qt.rgba(0.95, 0.55, 0.16, 0.25)
                    ctx.fill()
                    ctx.beginPath()
                    for (let i = 0; i < n; ++i) {
                        const y = top + h - sig[i] * h
                        if (i === 0) ctx.moveTo(0, y); else ctx.lineTo(i * dx, y)
                    }
                    ctx.strokeStyle = "#f28c28"
                    ctx.lineWidth = 1.2
                    ctx.stroke()
                    const ly = Math.round(top + h - scope.level * h) + 0.5
                    ctx.setLineDash([4, 3])
                    ctx.strokeStyle = win.cDim
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.moveTo(0, ly)
                    ctx.lineTo(width, ly)
                    ctx.stroke()
                }
            }
            Text {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 6
                anchors.bottomMargin: 3
                text: scope.trace.length === 0 ? qsTr("waiting for audio…")
                      : qsTr("F: %1 Hz · S: %2 WPM · C: %3")
                          .arg(Number(scope.info.pitch || 0).toFixed(1))
                          .arg(Math.round(scope.info.wpm || 0))
                          .arg(Number(scope.info.cost || 0).toFixed(3))
                color: scope.reading ? win.cText : win.cDim
                font.family: win.mono
                font.pixelSize: 10
            }
            Text {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 6
                anchors.bottomMargin: 3
                visible: scope.trace.length > 0
                text: scope.reading ? qsTr("reading") : qsTr("noise")
                color: scope.reading ? win.cOk : win.cDim
                font.pixelSize: 10
                font.bold: scope.reading
            }
        }

        // Il testo decodificato scorre in una misura sua, come in una telescrivente.
        ScrollView {
            id: decodedScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 80
            clip: true
            contentWidth: availableWidth
            TextArea {
                id: decoded
                width: decodedScroll.availableWidth
                readOnly: true
                wrapMode: TextArea.WrapAnywhere
                text: win.cw ? win.cw.decoderText : ""
                color: win.cText
                font.family: win.mono
                font.pixelSize: 14
                background: Rectangle { color: Qt.darker(win.cPanel, 1.2); border.color: win.cBorder; radius: 4 }
                onTextChanged: cursorPosition = length
            }
        }

        Text {
            id: statusText
            Layout.fillWidth: true
            color: win.cDim
            font.pixelSize: 11
            elide: Text.ElideRight
            text: ""
        }
    }

    // I tasti funzione, quando la finestra ha il fuoco; Esc ferma tutto.
    Repeater {
        model: Math.min(12, win.cw ? win.cw.macros.length : 0)
        Item {
            required property int index
            Shortcut {
                sequence: "F" + (index + 1)
                enabled: win.active && win.canSend && !macroEditor.visible && !setup.visible
                onActivated: win.cw.sendMacro(index, win.ctx())
            }
        }
    }
    Shortcut { sequence: "Esc"; enabled: win.active && !macroEditor.visible && !setup.visible; onActivated: win.cw.stop() }
}
