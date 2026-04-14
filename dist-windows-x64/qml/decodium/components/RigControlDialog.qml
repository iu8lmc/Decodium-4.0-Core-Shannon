/* Decodium 4.0 Core Shannon — CAT Settings Dialog
 * Backend "native": DecodiumCatManager (QSerialPort, 15 radio)  [default]
 * Backend "hamlib":  DecodiumTransceiverManager (Hamlib, 300+ radio)
 * bridge.catManager → QObject* al backend attivo (duck-typing QML)
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: rigDialog
    title: "CAT — Impostazioni Transceiver"
    modal: true
    width: 760
    height: 720
    closePolicy: Popup.CloseOnEscape
    property bool positionInitialized: false

    function clampToParent() {
        if (!parent) return
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        x = Math.max(0, Math.round((parent.width - width) / 2))
        y = Math.max(0, Math.round((parent.height - height) / 2))
        positionInitialized = true
    }

    onAboutToShow: ensureInitialPosition()

    property color bgDeep:        bridge.themeManager.bgDeep
    property color bgMedium:      bridge.themeManager.bgMedium
    property color bgLight:       bridge.themeManager.bgLight
    property color primaryBlue:   bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder:   bridge.themeManager.glassBorder
    readonly property int controlHeight: 38
    readonly property int controlFontSize: 13

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan; border.width: 2; radius: 12
    }

    header: Rectangle {
        height: 76
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                rigDialog.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                rigDialog.x += mouse.x - clickPos.x
                rigDialog.y += mouse.y - clickPos.y
                rigDialog.clampToParent()
            }
        }

        RowLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 10
            ColumnLayout {
                spacing: 2

                Text {
                    text: "CAT — Controllo Transceiver"
                    font.pixelSize: 16
                    font.bold: true
                    color: secondaryCyan
                }

                Text {
                    text: bridge.catConnected
                          ? "Connesso • " + (bridge.frequency / 1e6).toFixed(6) + " MHz • " + bridge.catMode
                            + (bridge.catManager.split ? " • SPLIT" : "")
                          : "Disconnesso"
                    font.pixelSize: 12
                    font.bold: true
                    color: bridge.catConnected ? accentGreen : "#f44336"
                }
            }
            Item { Layout.fillWidth: true }
            // LED connessione
            Rectangle {
                width: 12; height: 12; radius: 6
                color: bridge.catConnected ? accentGreen : "#f44336"
                SequentialAnimation on opacity {
                    running: bridge.catConnected; loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 600 }
                    NumberAnimation { to: 1.0; duration: 600 }
                }
            }
            Text {
                text: bridge.catConnected ? bridge.catRigName : "Non connesso"
                font.pixelSize: 13
                color: bridge.catConnected ? accentGreen : "#f44336"
            }
            // Chiudi
            Rectangle {
                width: 34; height: 34; radius: 6
                color: closeMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.3) : Qt.rgba(1,1,1,0.1)
                border.color: closeMA.containsMouse ? "#f44336" : glassBorder
                Text { anchors.centerIn: parent; text: "✕"; color: closeMA.containsMouse ? "#f44336" : textPrimary; font.pixelSize: 14 }
                MouseArea { id: closeMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: rigDialog.close() }
            }
        }
    }

    contentItem: Item {
        implicitWidth: contentLayout.implicitWidth
        implicitHeight: contentLayout.implicitHeight + 36

        ColumnLayout {
        id: contentLayout
        spacing: 12
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        anchors.topMargin: 24

        // ── Status bar ─────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; height: 42
            color: bridge.catConnected ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.12)
                                       : Qt.rgba(0.95, 0.26, 0.21, 0.12)
            border.color: bridge.catConnected ? accentGreen : "#f44336"
            radius: 8
            RowLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 12
                Rectangle { width: 12; height: 12; radius: 6; color: bridge.catConnected ? accentGreen : "#f44336" }
                Text {
                    text: bridge.catConnected ? "CAT attivo" : "CAT non connesso"
                    font.pixelSize: 12; font.bold: true
                    color: bridge.catConnected ? accentGreen : "#f44336"
                }
                Item { Layout.fillWidth: true }
            }
        }

        // ── Selettore backend CAT ──────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 8

            Flow {
                Layout.fillWidth: true
                width: parent.width
                spacing: 8

                Text {
                    width: 72
                    height: 34
                    text: "Backend:"
                    color: textSecondary
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }

                Repeater {
                    model: [["native","Nativo (QSerialPort)"],["omnirig","OmniRig"],["hamlib","Hamlib (300+ radio)"]]
                    delegate: Rectangle {
                        property string bk: modelData[0]
                        property bool active: bridge.catBackend === bk
                        width: 170
                        height: 34
                        radius: 6
                        color: active ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : "transparent"
                        border.color: active ? primaryBlue : glassBorder

                        Text {
                            anchors.centerIn: parent
                            text: modelData[1]
                            color: active ? primaryBlue : textSecondary
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!bridge.catManager.connected)
                                    bridge.catBackend = bk
                            }
                        }
                    }
                }
            }

            Text {
                text: "Il backend si cambia solo a radio scollegata."
                color: textSecondary
                font.pixelSize: 10
                opacity: 0.6
            }
        }

        // ── Tab bar: Principale / Avanzate ────────────────────────────────
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            background: Rectangle { color: "transparent" }
            TabButton {
                text: "Principale"
                implicitHeight: 42
                background: Rectangle {
                    color: tabBar.currentIndex === 0 ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.2) : "transparent"
                    border.color: tabBar.currentIndex === 0 ? secondaryCyan : "transparent"
                    radius: 6
                }
                contentItem: Text { text: parent.text; color: tabBar.currentIndex === 0 ? secondaryCyan : textSecondary; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            TabButton {
                text: "Avanzate"
                implicitHeight: 42
                background: Rectangle {
                    color: tabBar.currentIndex === 1 ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.2) : "transparent"
                    border.color: tabBar.currentIndex === 1 ? secondaryCyan : "transparent"
                    radius: 6
                }
                contentItem: Text { text: parent.text; color: tabBar.currentIndex === 1 ? secondaryCyan : textSecondary; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // ── StackLayout tabs ──────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // ═══════════ TAB 0 — PRINCIPALE ═══════════
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }

                    // Radio model
                    Text { text: "Radio:"; color: textSecondary; font.pixelSize: 12 }
                    // ── Selezione rig: campo testo + pulsante "Scegli…" ──────
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4

                        TextField {
                            id: rigNameField
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            leftPadding: 8
                            text: bridge.catManager.rigName
                            color: textPrimary; font.pixelSize: controlFontSize
                            placeholderText: "Nome rig…"
                            background: Rectangle { color: bgMedium; border.color: activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: bridge.catManager.setRigName(text.trim())
                        }

                        Button {
                            implicitWidth: 88; implicitHeight: controlHeight
                            padding: 0
                            text: "Scegli…"
                            onClicked: rigPickerDialog.open()
                            background: Rectangle { color: parent.hovered ? bgMedium : "transparent"; border.color: secondaryCyan; radius: 4 }
                            contentItem: Text { text: parent.text; color: secondaryCyan; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }

                    // ── Dialog separato per la selezione del rig ─────────────
                    Dialog {
                        id: rigPickerDialog
                        title: "Seleziona Transceiver"
                        modal: true
                        width: 460; height: 500
                        anchors.centerIn: parent
                        closePolicy: Popup.CloseOnEscape

                        background: Rectangle { color: Qt.rgba(bgDeep.r,bgDeep.g,bgDeep.b,0.98); border.color: secondaryCyan; border.width: 2; radius: 10 }
                        header: Rectangle {
                            height: 44; color: "transparent"
                            Text { anchors.centerIn: parent; text: "⚙  Scegli Transceiver (" + rigPickerList.count + " disponibili)"; color: secondaryCyan; font.pixelSize: 13; font.bold: true }
                        }

                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 8; spacing: 6

                            TextField {
                                id: rigFilterField
                                Layout.fillWidth: true; implicitHeight: 32
                                placeholderText: "Filtra per nome (es. Icom, Yaesu, FT-991…)"
                                leftPadding: 8; color: textPrimary; font.pixelSize: 11
                                background: Rectangle { color: bgMedium; border.color: activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            }

                            ListView {
                                id: rigPickerList
                                Layout.fillWidth: true; Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                                model: {
                                    var src = bridge.catManager ? bridge.catManager.rigList : []
                                    var q = rigFilterField.text.toLowerCase()
                                    if (!q) return src
                                    return src.filter(function(r){ return r.toLowerCase().indexOf(q) >= 0 })
                                }
                                delegate: ItemDelegate {
                                    width: rigPickerList.width; height: 30
                                    highlighted: modelData === bridge.catManager.rigName
                                    contentItem: Text {
                                        text: modelData; color: highlighted ? secondaryCyan : textPrimary
                                        font.pixelSize: 11; leftPadding: 8
                                        verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                                    }
                                    background: Rectangle { color: highlighted ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.15) : (hovered ? bgMedium : bgDeep) }
                                    onClicked: {
                                        rigNameField.text = modelData
                                        bridge.catManager.setRigName(modelData)
                                        rigPickerDialog.close()
                                    }
                                }
                            }

                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Annulla"; implicitHeight: 32; implicitWidth: 100
                                onClicked: rigPickerDialog.close()
                                background: Rectangle { color: "transparent"; border.color: glassBorder; radius: 4 }
                                contentItem: Text { text: parent.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                    }

                    // Porta seriale (visibile se portType=serial/usb)
                    Text {
                        text: "Porta:"; color: textSecondary; font.pixelSize: 12
                        visible: bridge.catManager.portType !== "network" && bridge.catManager.portType !== "tci"
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        visible: bridge.catManager.portType !== "network" && bridge.catManager.portType !== "tci"
                        ComboBox {
                            id: portCombo
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            editable: true
                            model: bridge.catManager.portList
                            Component.onCompleted: {
                                var idx = find(bridge.catManager.serialPort)
                                if (idx >= 0) currentIndex = idx
                                else editText = bridge.catManager.serialPort
                            }
                            contentItem: TextField {
                                leftPadding: 8; text: portCombo.editText; color: textPrimary; font.pixelSize: controlFontSize
                                background: Rectangle { color: "transparent" }
                                onTextEdited: portCombo.editText = text
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            delegate: ItemDelegate {
                                width: portCombo.width
                                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { color: hovered ? bgMedium : bgDeep }
                            }
                        }
                        Button {
                            implicitWidth: 36; implicitHeight: 36
                            padding: 0
                            ToolTip.text: "Aggiorna porte"; ToolTip.visible: hovered
                            onClicked: bridge.catManager.refreshPorts()
                            background: Rectangle { color: parent.hovered ? bgMedium : "transparent"; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: "↻"; color: secondaryCyan; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }

                    // Baud rate (solo seriale)
                    Text {
                        text: "Baud:"; color: textSecondary; font.pixelSize: 12
                        visible: bridge.catManager.portType === "serial" || bridge.catManager.portType === "usb"
                    }
                    ComboBox {
                        id: baudCombo
                        Layout.fillWidth: true; implicitHeight: controlHeight
                        visible: bridge.catManager.portType === "serial" || bridge.catManager.portType === "usb"
                        model: bridge.catManager.baudList
                        Component.onCompleted: {
                            var idx = find(bridge.catManager.baudRate.toString())
                            currentIndex = idx >= 0 ? idx : find("57600")
                        }
                        contentItem: Text { leftPadding: 8; text: baudCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; verticalAlignment: Text.AlignVCenter; height: baudCombo.height }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        delegate: ItemDelegate {
                            width: baudCombo.width
                            contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: hovered ? bgMedium : bgDeep }
                        }
                    }

                    // Porta di rete (HRD / DXLab / rigctld)
                    Text {
                        text: "Host:Port:"; color: textSecondary; font.pixelSize: 12
                        visible: bridge.catManager.portType === "network"
                    }
                    TextField {
                        id: networkField
                        Layout.fillWidth: true; implicitHeight: controlHeight
                        visible: bridge.catManager.portType === "network"
                        text: bridge.catManager.networkPort
                        placeholderText: "localhost:4532"
                        color: textPrimary; font.pixelSize: controlFontSize
                        onTextChanged: bridge.catManager.networkPort = text
                        background: Rectangle { color: bgMedium; border.color: activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }

                    // Porta TCI
                    Text {
                        text: "TCI:"; color: textSecondary; font.pixelSize: 12
                        visible: bridge.catManager.portType === "tci"
                    }
                    TextField {
                        id: tciField
                        Layout.fillWidth: true; implicitHeight: controlHeight
                        visible: bridge.catManager.portType === "tci"
                        text: bridge.catManager.tciPort
                        placeholderText: "localhost:50001"
                        color: textPrimary; font.pixelSize: controlFontSize
                        onTextChanged: bridge.catManager.tciPort = text
                        background: Rectangle { color: bgMedium; border.color: activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                    }

                    // PTT method
                    Text { text: "PTT:"; color: textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: pttCombo
                        Layout.fillWidth: true; implicitHeight: controlHeight
                        model: bridge.catManager.pttMethodList
                        Component.onCompleted: { var i = find(bridge.catManager.pttMethod); currentIndex = i>=0?i:0 }
                        contentItem: Text { leftPadding: 8; text: pttCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; verticalAlignment: Text.AlignVCenter; height: pttCombo.height }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        delegate: ItemDelegate {
                            width: pttCombo.width
                            contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: hovered ? bgMedium : bgDeep }
                        }
                    }

                    // Split
                    Text { text: "Split:"; color: textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: splitCombo
                        Layout.fillWidth: true; implicitHeight: controlHeight
                        model: bridge.catManager.splitModeList
                        Component.onCompleted: { var i = find(bridge.catManager.splitMode); currentIndex = i>=0?i:0 }
                        contentItem: Text { leftPadding: 8; text: splitCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; verticalAlignment: Text.AlignVCenter; height: splitCombo.height }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        delegate: ItemDelegate {
                            width: splitCombo.width
                            contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: hovered ? bgMedium : bgDeep }
                        }
                    }

                    // Auto-connect e auto-start
                    Text { text: "Auto:"; color: textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 16
                        CheckBox {
                            text: "Connetti all'avvio"
                            checked: bridge.catManager.catAutoConnect
                            onCheckedChanged: bridge.catManager.catAutoConnect = checked
                            contentItem: Text { text: parent.text; leftPadding: 4; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                        }
                        CheckBox {
                            text: "Avvia monitor"
                            checked: bridge.catManager.audioAutoStart
                            onCheckedChanged: bridge.catManager.audioAutoStart = checked
                            contentItem: Text { text: parent.text; leftPadding: 4; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
            }

            // ═══════════ TAB 1 — AVANZATE ═══════════
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                GridLayout {
                    width: parent.width
                    columns: 2; columnSpacing: 12; rowSpacing: 10
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }

                    // PTT port
                    Text { text: "PTT port:"; color: textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        ComboBox {
                            id: pttPortCombo; Layout.fillWidth: true; implicitHeight: controlHeight; editable: true
                            model: ["CAT"].concat(bridge.catManager.portList)
                            Component.onCompleted: {
                                var idx = find(bridge.catManager.pttPort)
                                currentIndex = idx >= 0 ? idx : 0
                                editText = bridge.catManager.pttPort
                            }
                            contentItem: TextField {
                                leftPadding: 8; text: pttPortCombo.editText
                                color: textPrimary; font.pixelSize: controlFontSize
                                background: Rectangle { color: "transparent" }
                                onTextEdited: pttPortCombo.editText = text
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            delegate: ItemDelegate {
                                width: pttPortCombo.width
                                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { color: hovered ? bgMedium : bgDeep }
                            }
                        }
                    }

                    // Poll interval
                    Text { text: "Poll (s):"; color: textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: pollSpin; from: 1; to: 10; value: bridge.catManager.pollInterval; implicitHeight: controlHeight
                        contentItem: Text { text: pollSpin.value; color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                    }

                    // Data bits
                    Text { text: "Data bits:"; color: textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: dataCombo; Layout.fillWidth: true; implicitHeight: controlHeight
                        model: ["8","7"]
                        Component.onCompleted: { var i = find(bridge.catManager.dataBits); currentIndex = i>=0?i:0 }
                        contentItem: Text { leftPadding: 8; text: dataCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; verticalAlignment: Text.AlignVCenter; height: dataCombo.height }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        delegate: ItemDelegate {
                            width: dataCombo.width
                            contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: hovered ? bgMedium : bgDeep }
                        }
                    }

                    // Stop bits
                    Text { text: "Stop bits:"; color: textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: stopCombo; Layout.fillWidth: true; implicitHeight: controlHeight
                        model: ["1","2"]
                        Component.onCompleted: { var i = find(bridge.catManager.stopBits); currentIndex = i>=0?i:0 }
                        contentItem: Text { leftPadding: 8; text: stopCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; verticalAlignment: Text.AlignVCenter; height: stopCombo.height }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        delegate: ItemDelegate {
                            width: stopCombo.width
                            contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: hovered ? bgMedium : bgDeep }
                        }
                    }

                    // Handshake
                    Text { text: "Handshake:"; color: textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: hsCombo; Layout.fillWidth: true; implicitHeight: controlHeight
                        model: ["none","xonxoff","hardware"]
                        Component.onCompleted: { var i = find(bridge.catManager.handshake); currentIndex = i>=0?i:0 }
                        contentItem: Text { leftPadding: 8; text: hsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; verticalAlignment: Text.AlignVCenter; height: hsCombo.height }
                        background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        delegate: ItemDelegate {
                            width: hsCombo.width
                            contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: hovered ? bgMedium : bgDeep }
                        }
                    }

                    // DTR / RTS force
                    Text { text: "DTR:"; color: textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        CheckBox { text: "Force"; checked: bridge.catManager.forceDtr; onCheckedChanged: bridge.catManager.forceDtr = checked; contentItem: Text { text: parent.text; leftPadding: 4; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter } }
                        CheckBox { text: "High";  checked: bridge.catManager.dtrHigh;  onCheckedChanged: bridge.catManager.dtrHigh  = checked; contentItem: Text { text: parent.text; leftPadding: 4; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter } }
                    }

                    Text { text: "RTS:"; color: textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        CheckBox { text: "Force"; checked: bridge.catManager.forceRts; onCheckedChanged: bridge.catManager.forceRts = checked; contentItem: Text { text: parent.text; leftPadding: 4; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter } }
                        CheckBox { text: "High";  checked: bridge.catManager.rtsHigh;  onCheckedChanged: bridge.catManager.rtsHigh  = checked; contentItem: Text { text: parent.text; leftPadding: 4; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter } }
                    }
                }
            }
        }

        // ── Errori CAT (scrollabile, max 60px) ───────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 60
            visible: catErrorText.text.length > 0
            color: Qt.rgba(0.95, 0.26, 0.21, 0.1)
            border.color: "#f44336"; radius: 4
            ScrollView {
                anchors.fill: parent; anchors.margins: 4
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                Text {
                    id: catErrorText
                    width: parent.width
                    wrapMode: Text.Wrap; color: "#ff6b6b"; font.pixelSize: 10
                    text: ""
                }
            }
        }
        Connections {
            target: bridge
            function onErrorMessage(msg) {
                // mostra solo la prima riga significativa + dettaglio ridotto
                var lines = msg.split("\n").filter(function(l){ return l.trim().length > 0 })
                catErrorText.text = lines.length > 0 ? lines[0] + (lines.length > 1 ? "\n…(" + (lines.length-1) + " righe)" : "") : msg
            }
        }

        // ── Bottoni ───────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            implicitHeight: actionFlow.height

            Flow {
                id: actionFlow
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, 436)
                spacing: 12

            Button {
                implicitWidth: 150; implicitHeight: 42
                padding: 0
                text: "▶  Connetti"
                enabled: !bridge.catConnected
                onClicked: {
                    bridge.catManager.rigName     = rigNameField.text.trim()
                    bridge.catManager.serialPort  = portCombo.editText.trim()
                    bridge.catManager.baudRate    = parseInt(baudCombo.currentText)
                    bridge.catManager.pttMethod   = pttCombo.currentText
                    bridge.catManager.splitMode   = splitCombo.currentText
                    bridge.catManager.pttPort     = pttPortCombo.editText.trim()
                    bridge.catManager.pollInterval = pollSpin.value
                    bridge.catManager.dataBits    = dataCombo.currentText
                    bridge.catManager.stopBits    = stopCombo.currentText
                    bridge.catManager.handshake   = hsCombo.currentText
                    bridge.catManager.saveSettings()
                    bridge.catManager.connectRig()
                }
                background: Rectangle {
                    radius: 8
                    color: parent.enabled ? Qt.rgba(0,1,0.53,0.15) : Qt.rgba(1,1,1,0.05)
                    border.color: parent.enabled ? accentGreen : glassBorder
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: 12; font.bold: true
                    color: parent.enabled ? accentGreen : textSecondary
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                implicitWidth: 150; implicitHeight: 42
                padding: 0
                text: "■  Disconnetti"
                enabled: bridge.catConnected
                onClicked: bridge.catManager.disconnectRig()
                background: Rectangle {
                    radius: 8
                    color: parent.enabled ? Qt.rgba(0.95,0.26,0.21,0.15) : Qt.rgba(1,1,1,0.05)
                    border.color: parent.enabled ? "#f44336" : glassBorder
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: 12; font.bold: true
                    color: parent.enabled ? "#f44336" : textSecondary
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                implicitWidth: 100; implicitHeight: 42
                padding: 0
                text: "Chiudi"
                onClicked: rigDialog.close()
                background: Rectangle { radius: 8; color: Qt.rgba(1,1,1,0.07); border.color: glassBorder }
                contentItem: Text {
                    text: parent.text; font.pixelSize: 12; color: textSecondary
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }
        }
    }
}
}
