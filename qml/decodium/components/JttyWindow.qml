/* JttyWindow - JTTY, il modo da tastiera di WSJT-X 3.2 (C++ nativo).
 *
 * Due elenchi come in WSJT-X: sopra il canale del QSO (RX +/- FTol), sotto
 * tutte le frequenze. I messaggi crescono frame dopo frame (1,888 s l'uno) e
 * si chiudono al frame con il bit di fine messaggio. Si trasmette quando si
 * vuole: il testo scritto mentre si e' gia' in aria si accoda senza buchi.
 *
 * Le frequenze RX/TX sono quelle dei marcatori del waterfall principale.
 * F1-F8 come in WSJT-X: %M mio nominativo, %H il suo, %E lo scambio,
 * %Q il prossimo in coda, %G il locatore.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win

    readonly property var eng: (typeof appEngine !== 'undefined' ? appEngine : null)
    readonly property var jt: (typeof jtty !== 'undefined' ? jtty : null)
    readonly property bool jttyOn: !!(jt && jt.active)
    readonly property bool txOn: !!(jt && jt.transmitting)
    readonly property string mono: (typeof decodiumMonoFontFamily !== 'undefined' ? decodiumMonoFontFamily : "monospace")

    readonly property var tm: eng ? eng.themeManager : null
    readonly property color cBg:      tm ? tm.bgDeep        : "#0a0f14"
    readonly property color cPanel:   tm ? tm.panelColor    : "#111a22"
    readonly property color cBorder:  tm ? tm.borderColor   : "#22303c"
    readonly property color cText:    tm ? tm.textPrimary   : "#dbe4ea"
    readonly property color cDim:     tm ? tm.textSecondary : "#7d8b96"
    readonly property color cAccent:  tm ? tm.accentColor   : "#19d0ff"
    readonly property color cOk:      tm ? tm.successColor  : "#25d366"
    readonly property color cWarn:    tm ? tm.warningColor  : "#ffb84a"
    readonly property color cTx:      "#ffd84a"

    title: qsTr("JTTY - keyboard mode")
    width: 900
    height: 700
    minimumWidth: 640
    minimumHeight: 480
    color: cBg

    // Ripete le anteprime dei tasti quando cambiano nominativi o numeri.
    property int previewTick: 0
    Connections {
        target: win.eng
        ignoreUnknownSignals: true
        function onDxCallChanged()   { win.previewTick++ }
        function onCallsignChanged() { win.previewTick++ }
        function onGridChanged()     { win.previewTick++ }
    }
    Connections {
        target: win.jt
        ignoreUnknownSignals: true
        function onSerialNumberChanged()    { win.previewTick++ }
        function onExchangeChanged()        { win.previewTick++ }
        function onExchangeProfileChanged() { win.previewTick++ }
        function onMacrosChanged()          { win.previewTick++ }
    }

    function sendTyped() {
        if (!jt) return
        var t = txField.text
        if (t.trim().length === 0) return
        if (jt.send(t)) txField.text = ""
    }

    function sendKey(k) {
        if (jt) jt.sendFunctionKey(k)
    }

    function shown(t) {
        return (jt && jt.lowerCase) ? String(t).toLowerCase() : t
    }

    Shortcut { sequence: "F1"; onActivated: win.sendKey(1) }
    Shortcut { sequence: "F2"; onActivated: win.sendKey(2) }
    Shortcut { sequence: "F3"; onActivated: win.sendKey(3) }
    Shortcut { sequence: "F4"; onActivated: win.sendKey(4) }
    Shortcut { sequence: "F5"; onActivated: win.sendKey(5) }
    Shortcut { sequence: "F6"; onActivated: win.sendKey(6) }
    Shortcut { sequence: "F7"; onActivated: win.sendKey(7) }
    Shortcut { sequence: "F8"; onActivated: win.sendKey(8) }
    Shortcut { sequence: "Esc"; onActivated: if (win.jt) win.jt.abort() }

    component Btn: Rectangle {
        id: btn
        property string label: ""
        property bool danger: false
        property bool live: true
        signal clicked()
        implicitWidth: btnTxt.implicitWidth + 22
        implicitHeight: 28
        radius: 5
        opacity: btn.live ? 1.0 : 0.4
        color: btnMA.containsMouse && btn.live
               ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.16) : "transparent"
        border.width: 1
        border.color: btn.danger ? win.cWarn : win.cAccent
        Text {
            id: btnTxt
            anchors.centerIn: parent
            text: btn.label
            color: btn.danger ? win.cWarn : win.cAccent
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

    component Label2: Text {
        color: win.cDim
        font.pixelSize: 11
    }

    // Un elenco di righe JTTY: ora UTC, frequenza audio, testo.
    component LinePane: Rectangle {
        id: pane
        property string heading: ""
        property var model: null
        property bool qsoList: false
        color: win.cPanel
        border.color: win.cBorder
        border.width: 1
        radius: 6
        clip: true

        Text {
            id: paneHead
            anchors { left: parent.left; top: parent.top; margins: 8 }
            text: pane.heading
            color: win.cAccent
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1.2
        }
        ListView {
            id: lv
            anchors { left: parent.left; right: parent.right; top: paneHead.bottom; bottom: parent.bottom; margins: 6 }
            model: pane.model
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            // Segue le righe nuove finche' l'operatore non risale a leggere.
            property bool follow: true
            onMovementEnded: follow = atYEnd
            onCountChanged: if (follow) Qt.callLater(positionViewAtEnd)
            delegate: Rectangle {
                required property int index
                required property string utc
                required property int frequency
                required property string text
                required property bool complete
                required property bool tx
                width: ListView.view.width
                height: lineTxt.implicitHeight + 4
                color: tx ? Qt.rgba(1.0, 0.85, 0.3, 0.12)
                          : (lineMA.containsMouse ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.08) : "transparent")
                Text {
                    id: lineTxt
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 4 }
                    wrapMode: Text.Wrap
                    font.family: win.mono
                    font.pixelSize: 13
                    color: tx ? win.cTx : (complete ? win.cText : win.cDim)
                    text: utc + "  " + String(frequency).padStart(4, " ") + "  "
                          + (tx ? "TX: " : "") + win.shown(parent.text)
                }
                MouseArea {
                    id: lineMA
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: if (win.jt) win.jt.pickLine(pane.qsoList, index)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // ── stato e parametri ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: topRow.implicitHeight + 16
            radius: 6
            color: win.cPanel
            border.color: win.cBorder
            RowLayout {
                id: topRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 10
                Rectangle {
                    implicitWidth: stTxt.implicitWidth + 16
                    implicitHeight: 22
                    radius: 11
                    color: win.txOn ? Qt.rgba(1, 0.3, 0.3, 0.25)
                                    : (win.jttyOn ? Qt.rgba(0.15, 0.8, 0.4, 0.18) : "transparent")
                    border.color: win.txOn ? "#ff5050" : (win.jttyOn ? win.cOk : win.cBorder)
                    Text {
                        id: stTxt
                        anchors.centerIn: parent
                        text: win.txOn ? qsTr("TX") : (win.jttyOn ? qsTr("RX") : qsTr("JTTY OFF"))
                        color: win.txOn ? "#ff7070" : (win.jttyOn ? win.cOk : win.cDim)
                        font.bold: true
                        font.pixelSize: 11
                    }
                }
                Label2 { text: qsTr("RX %1 Hz").arg(win.eng ? win.eng.rxFrequency : 0) }
                Label2 { text: qsTr("TX %1 Hz").arg(win.eng ? win.eng.txFrequency : 0) }
                Btn {
                    label: qsTr("TX=RX")
                    onClicked: if (win.eng) win.eng.txFrequency = win.eng.rxFrequency
                }
                Label2 { text: qsTr("FTol") }
                SpinBox {
                    from: 10; to: 500; stepSize: 10
                    value: win.jt ? win.jt.ftol : 50
                    editable: true
                    onValueModified: if (win.jt) win.jt.ftol = value
                    implicitWidth: 110
                }
                Label2 { text: qsTr("DX") }
                TextField {
                    implicitWidth: 110
                    text: win.eng ? win.eng.dxCall : ""
                    font.capitalization: Font.AllUppercase
                    onEditingFinished: if (win.eng) win.eng.dxCall = text.trim().toUpperCase()
                }
                Item { Layout.fillWidth: true }
                CheckBox {
                    checked: !!(win.jt && win.jt.lowerCase)
                    onToggled: if (win.jt) win.jt.lowerCase = checked
                }
                Label2 { text: qsTr("lower case") }
                Btn { label: qsTr("Clear"); onClicked: if (win.jt) win.jt.clearHistory() }
            }
        }

        // ── i due elenchi ─────────────────────────────────────────────────
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical
            handle: Rectangle {
                implicitHeight: 6
                color: SplitHandle.pressed || SplitHandle.hovered ? win.cAccent : win.cBorder
            }
            LinePane {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 120
                heading: qsTr("QSO FREQUENCY  (RX %1 ± %2 Hz)")
                         .arg(win.eng ? win.eng.rxFrequency : 0).arg(win.jt ? win.jt.ftol : 0)
                model: win.jt ? win.jt.qsoLines : null
                qsoList: true
            }
            LinePane {
                SplitView.preferredHeight: 220
                SplitView.minimumHeight: 90
                heading: qsTr("ALL FREQUENCIES  (click a line: RX there and pick the call)")
                model: win.jt ? win.jt.allLines : null
                qsoList: false
            }
        }

        // ── tasti funzione ───────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: macroCol.implicitHeight + 16
            radius: 6
            color: win.cPanel
            border.color: win.cBorder
            ColumnLayout {
                id: macroCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 6
                    rowSpacing: 6
                    Repeater {
                        model: 8
                        delegate: Rectangle {
                            id: keyBox
                            required property int index
                            readonly property int key: index + 1
                            readonly property string preview: {
                                win.previewTick
                                return win.jt ? win.jt.previewFunctionKey(key) : ""
                            }
                            readonly property bool invalid: preview.length > 0 && preview.charAt(0) === "!"
                            Layout.fillWidth: true
                            implicitHeight: 40
                            radius: 5
                            color: keyMA.containsMouse ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.14) : "transparent"
                            border.color: invalid ? win.cWarn : win.cBorder
                            Column {
                                anchors.fill: parent
                                anchors.margins: 4
                                Text {
                                    text: "F" + keyBox.key + "  " + (win.jt ? win.jt.macros[keyBox.index] : "")
                                    color: win.cAccent
                                    font.pixelSize: 10
                                    font.bold: true
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: keyBox.invalid ? keyBox.preview.substring(1) : win.shown(keyBox.preview)
                                    color: keyBox.invalid ? win.cWarn : win.cText
                                    font.family: win.mono
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }
                            MouseArea {
                                id: keyMA
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor
                                onClicked: function (mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        macroEditor.key = keyBox.key
                                        macroEditor.open()
                                    } else {
                                        win.sendKey(keyBox.key)
                                    }
                                }
                            }
                            ToolTip.visible: keyMA.containsMouse
                            ToolTip.delay: 600
                            ToolTip.text: qsTr("Click to send, right click to edit")
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label2 { text: qsTr("Contest") }
                    ComboBox {
                        implicitWidth: 150
                        model: [qsTr("None"), qsTr("Field Day"), qsTr("RTTY Roundup")]
                        currentIndex: win.jt ? win.jt.exchangeProfile : 0
                        onActivated: function (i) { if (win.jt) win.jt.exchangeProfile = i }
                    }
                    Label2 { text: qsTr("Exchange"); visible: !!(win.jt && win.jt.exchangeProfile > 0) }
                    TextField {
                        visible: !!(win.jt && win.jt.exchangeProfile > 0)
                        implicitWidth: 110
                        placeholderText: win.jt && win.jt.exchangeProfile === 1 ? "1D EMA" : "MA / DX"
                        text: win.jt ? win.jt.exchange : ""
                        onEditingFinished: if (win.jt) win.jt.exchange = text
                    }
                    Label2 { text: qsTr("Serial") }
                    SpinBox {
                        from: 0; to: 131071
                        editable: true
                        value: win.jt ? win.jt.serialNumber : 1
                        onValueModified: if (win.jt) win.jt.serialNumber = value
                        implicitWidth: 120
                    }
                    CheckBox {
                        checked: !!(win.jt && win.jt.autoLogOnTu)
                        onToggled: if (win.jt) win.jt.autoLogOnTu = checked
                    }
                    Label2 { text: qsTr("log on \"TU\"") }
                    Item { Layout.fillWidth: true }
                    Btn { label: qsTr("Log QSO"); onClicked: if (win.jt) win.jt.logQso() }
                }
            }
        }

        // ── trasmissione ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: txField
                Layout.fillWidth: true
                implicitHeight: 40
                font.family: win.mono
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                rightPadding: 10
                color: win.cText
                placeholderTextColor: win.cDim
                selectionColor: Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.35)
                selectedTextColor: win.cText
                background: Rectangle {
                    color: win.cBg
                    border.color: txField.activeFocus ? win.cAccent : win.cBorder
                    border.width: 1
                    radius: 5
                }
                maximumLength: 80
                // Material's floating placeholder is drawn above the custom
                // TextField background and overlaps the preceding panel.
                // Render the hint ourselves inside the editor instead.
                placeholderText: ""
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: txField.leftPadding
                    anchors.verticalCenter: parent.verticalCenter
                    visible: txField.text.length === 0 && !txField.activeFocus
                    color: win.cDim
                    font: txField.font
                    text: qsTr("Type a message and press Enter (Esc stops the transmission)")
                    elide: Text.ElideRight
                    width: Math.max(0, txField.width - txField.leftPadding - txField.rightPadding)
                }
                onAccepted: win.sendTyped()
                Component.onCompleted: forceActiveFocus()
            }
            Btn { label: qsTr("Send"); live: win.jttyOn && txField.text.trim().length > 0; onClicked: win.sendTyped() }
            Btn { label: qsTr("Stop TX"); danger: true; live: win.txOn; onClicked: if (win.jt) win.jt.abort() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 11
                color: win.jt && win.jt.lastError.length > 0 ? win.cWarn : (win.txOn ? win.cTx : win.cDim)
                text: {
                    if (!win.jt) return ""
                    if (win.jt.lastError.length > 0) return win.jt.lastError
                    if (win.txOn) return qsTr("Sending: %1").arg(win.shown(win.jt.sendingText))
                    if (!win.jttyOn) return qsTr("Select JTTY in the mode selector to receive and transmit.")
                    return txField.text.trim().length > 0 ? win.jt.describe(txField.text) : ""
                }
            }
        }
    }

    Popup {
        id: macroEditor
        property int key: 1
        anchors.centerIn: parent
        modal: true
        padding: 14
        onOpened: {
            macroField.text = win.jt ? win.jt.macros[key - 1] : ""
            macroField.forceActiveFocus()
        }
        background: Rectangle { color: win.cPanel; border.color: win.cAccent; radius: 8 }
        ColumnLayout {
            spacing: 8
            Text {
                text: qsTr("F%1 template").arg(macroEditor.key)
                color: win.cAccent
                font.bold: true
            }
            Text {
                text: qsTr("%M my call, %H and %Q the DX call, %E exchange, %G grid.\nThe default templates are sent as compact native frames.")
                color: win.cDim
                font.pixelSize: 11
            }
            TextField {
                id: macroField
                implicitWidth: 360
                font.family: win.mono
                onAccepted: saveBtn.clicked()
            }
            RowLayout {
                Btn {
                    id: saveBtn
                    label: qsTr("Save")
                    onClicked: { if (win.jt) win.jt.setMacro(macroEditor.key, macroField.text); macroEditor.close() }
                }
                Btn { label: qsTr("Defaults (all)"); onClicked: { if (win.jt) win.jt.resetMacros(); macroEditor.close() } }
                Btn { label: qsTr("Cancel"); onClicked: macroEditor.close() }
            }
        }
    }
}
