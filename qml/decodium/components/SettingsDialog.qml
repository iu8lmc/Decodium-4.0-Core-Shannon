/* Decodium 4.0 — Impostazioni Generali
 * Sostituisce il dialogo impostazioni legacy WSJT-X.
 * Tutte le modifiche sono LIVE (bind diretto alle proprieta bridge).
 * Layout orizzontale: sidebar + StackLayout con GridLayout 4 colonne.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: settingsDialog
    title: "Impostazioni"
    modal: true
    width: 960
    height: 700
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

    function openTab(index) {
        tabStack.currentIndex = index
        open()
    }

    // ── Theme colors ─────────────────────────────────────────────────────
    property color bgDeep:        bridge.themeManager.bgDeep
    property color bgMedium:      bridge.themeManager.bgMedium
    property color bgLight:       bridge.themeManager.bgLight
    property color primaryBlue:   bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder:   bridge.themeManager.glassBorder
    readonly property int controlHeight: 32
    readonly property int controlFontSize: 12

    // ── Preset colors for color pickers ──────────────────────────────────
    readonly property var presetColors: [
        "#ff0000","#ff6600","#ffcc00","#33cc33","#00ccff","#0066ff",
        "#9933ff","#ff33cc","#ffffff","#cccccc","#666666","#000000"
    ]

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan; border.width: 2; radius: 12
    }

    // ── Draggable header ─────────────────────────────────────────────────
    header: Rectangle {
        height: 56
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                settingsDialog.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                settingsDialog.x += mouse.x - clickPos.x
                settingsDialog.y += mouse.y - clickPos.y
                settingsDialog.clampToParent()
            }
        }

        RowLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 10

            Text {
                text: "Impostazioni"
                font.pixelSize: 16; font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: 34; height: 34; radius: 6
                color: closeMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.3) : Qt.rgba(1,1,1,0.1)
                border.color: closeMA.containsMouse ? "#f44336" : glassBorder
                Text { anchors.centerIn: parent; text: "\u2715"; color: closeMA.containsMouse ? "#f44336" : textPrimary; font.pixelSize: 14 }
                MouseArea { id: closeMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.close() }
            }
        }
    }

    // ── Content ──────────────────────────────────────────────────────────
    contentItem: Item {
        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ── Sidebar ──────────────────────────────────────────────
            Rectangle {
                Layout.preferredWidth: 130
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)

                Column {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 2

                    Repeater {
                        model: ["Stazione","Radio","Audio","TX","Display","Decodifica","Reporting","Colori","Avanzate","Alerts","Filtri"]
                        delegate: Rectangle {
                            width: parent.width; height: 36; radius: 6
                            color: tabStack.currentIndex === index ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (tabMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent")
                            border.color: tabStack.currentIndex === index ? primaryBlue : "transparent"
                            Text { anchors.centerIn: parent; text: modelData; color: tabStack.currentIndex === index ? primaryBlue : textSecondary; font.pixelSize: 12 }
                            MouseArea { id: tabMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: tabStack.currentIndex = index }
                        }
                    }
                }
            }

            // Vertical separator
            Rectangle { Layout.fillHeight: true; width: 1; color: glassBorder }

            // ── StackLayout ──────────────────────────────────────────
            StackLayout {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                // ═══════════ TAB 0 — STAZIONE ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Dettagli Stazione ──
                        Text { text: "DETTAGLI STAZIONE"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "My Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.callsign; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.callsign = text
                        }
                        Text { text: "My Grid:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.grid; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.grid = text
                        }

                        Text { text: "Auto Grid:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AutoGrid", false)
                            onCheckedChanged: bridge.setSetting("AutoGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "IARU Region:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            model: ["1","2","3"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("Region", 0))
                            onActivated: bridge.setSetting("Region", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Type 2 Msg Gen:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            model: ["Full","Type 1 prefix","Type 2 prefix"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("Type2MsgGen", 0))
                            onActivated: bridge.setSetting("Type2MsgGen", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Op Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OpCall", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OpCall", text)
                        }

                        // ── Info Stazione ──
                        Text { text: "INFO STAZIONE"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Nome Stazione:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationName; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationName = text
                        }
                        Text { text: "QTH:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationQth; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationQth = text
                        }

                        Text { text: "Rig Info:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationRigInfo; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationRigInfo = text
                        }
                        Text { text: "Antenna:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationAntenna; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationAntenna = text
                        }

                        Text { text: "Potenza (W):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: stPowerSpin
                            from: 0; to: 9999; value: bridge.stationPowerWatts; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.stationPowerWatts = value
                            contentItem: TextInput { text: stPowerSpin.textFromValue(stPowerSpin.value, stPowerSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !stPowerSpin.editable; validator: stPowerSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 1 — RADIO ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Backend CAT ──
                        Text { text: "BACKEND CAT"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Backend:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Row {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 6
                            Repeater {
                                model: [["native","Nativo (15 radio)"],["hamlib","Hamlib (300+ radio)"],["omnirig","OmniRig"]]
                                delegate: Rectangle {
                                    property string bk: modelData[0]
                                    property bool active: bridge.catBackend === bk
                                    width: 170; height: 30; radius: 6
                                    color: active ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (bkMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent")
                                    border.color: active ? primaryBlue : glassBorder
                                    Text { anchors.centerIn: parent; text: modelData[1]; color: active ? primaryBlue : textSecondary; font.pixelSize: 11 }
                                    MouseArea { id: bkMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: { if (!bridge.catConnected) bridge.catBackend = bk }
                                    }
                                }
                            }
                        }

                        // ── Stato connessione ──
                        Text { text: "Stato:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Row {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 8
                            Rectangle { width: 12; height: 12; radius: 6; color: bridge.catConnected ? accentGreen : "#f44336"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: bridge.catConnected ? "Connesso — " + bridge.catRigName + " — " + bridge.catMode : "Disconnesso"; color: bridge.catConnected ? accentGreen : "#f44336"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Item { width: 20; height: 1 }
                            Rectangle {
                                width: 100; height: 28; radius: 6
                                color: connMA.containsMouse ? (bridge.catConnected ? Qt.rgba(0.95,0.26,0.21,0.2) : Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.2)) : "transparent"
                                border.color: bridge.catConnected ? "#f44336" : accentGreen
                                Text { anchors.centerIn: parent; text: bridge.catConnected ? "Disconnetti" : "Connetti"; color: bridge.catConnected ? "#f44336" : accentGreen; font.pixelSize: 11 }
                                MouseArea { id: connMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.catConnected ? bridge.catManager.disconnectRig() : bridge.catManager.connectRig()
                                }
                            }
                            Rectangle {
                                width: 28; height: 28; radius: 6
                                color: refreshMA.containsMouse ? bgMedium : "transparent"
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: "↻"; color: secondaryCyan; font.pixelSize: 16 }
                                MouseArea { id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.catManager.refreshPorts()
                                }
                            }
                        }

                        // ── Controllo CAT ──
                        Text { text: "CONTROLLO CAT"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Rig:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: rigCombo
                            model: bridge.catManager ? bridge.catManager.rigList : []; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            Component.onCompleted: { if (bridge.catManager) { var i = find(bridge.catManager.rigName); if (i >= 0) currentIndex = i } }
                            onActivated: { if (bridge.catManager) bridge.catManager.rigName = currentText }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: rigCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Serial Port:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: serialPortCombo
                            model: bridge.catManager ? bridge.catManager.portList : []; Layout.fillWidth: true; implicitHeight: controlHeight
                            Component.onCompleted: { if (bridge.catManager) { var i = find(bridge.catManager.serialPort); if (i >= 0) currentIndex = i } }
                            onActivated: { if (bridge.catManager) bridge.catManager.serialPort = currentText }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: serialPortCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Baud Rate:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: baudCombo
                            model: bridge.catManager && bridge.catManager.baudList ? bridge.catManager.baudList : ["4800","9600","19200","38400","57600","115200"]
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            Component.onCompleted: { if (bridge.catManager) { var i = find(String(bridge.catManager.baudRate)); if (i >= 0) currentIndex = i } }
                            onActivated: { if (bridge.catManager) bridge.catManager.baudRate = parseInt(currentText) }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: baudCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "PTT Method:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: pttCombo
                            model: bridge.catManager && bridge.catManager.pttMethodList ? bridge.catManager.pttMethodList : ["CAT","DTR","RTS","VOX"]
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            Component.onCompleted: { if (bridge.catManager) { var i = find(bridge.catManager.pttMethod); if (i >= 0) currentIndex = i } }
                            onActivated: { if (bridge.catManager) bridge.catManager.pttMethod = currentText }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: pttCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Poll Interval (s):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: pollSpin
                            from: 1; to: 99; value: bridge.catManager ? bridge.catManager.pollInterval : 3; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: { if (bridge.catManager) bridge.catManager.pollInterval = value }
                            contentItem: TextInput { text: pollSpin.textFromValue(pollSpin.value, pollSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !pollSpin.editable; validator: pollSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Parametri Seriali ──
                        Text { text: "PARAMETRI SERIALI"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Data Bits:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: dataBitsCombo
                            model: ["Default","7","8"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("CATDataBits", 0))
                            onActivated: bridge.setSetting("CATDataBits", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: dataBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Stop Bits:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: stopBitsCombo
                            model: ["Default","1","2"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("CATStopBits", 0))
                            onActivated: bridge.setSetting("CATStopBits", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: stopBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Handshake:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: handshakeCombo
                            model: ["Default","None","XON/XOFF","Hardware"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("CATHandshake", 0))
                            onActivated: bridge.setSetting("CATHandshake", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: handshakeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: "Force DTR:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: forceDtrCombo
                            model: ["Default","Off","On"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("CATForceDTR", 0))
                            onActivated: bridge.setSetting("CATForceDTR", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: forceDtrCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Force RTS:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: forceRtsCombo
                            model: ["Default","Off","On"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("CATForceRTS", 0))
                            onActivated: bridge.setSetting("CATForceRTS", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: forceRtsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        // ── Operazione Split ──
                        Text { text: "OPERAZIONE SPLIT"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Split:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: splitCombo
                            model: ["None","Fake It","Rig"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("SplitMode", 0))
                            onActivated: bridge.setSetting("SplitMode", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: splitCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: modeCombo
                            model: ["USB","Data/Pkt","None"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("CATMode", 0))
                            onActivated: bridge.setSetting("CATMode", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: modeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "TX Audio Src:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: txAudioSrcCombo
                            model: ["Rear/Data","Front/Mic"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("TXAudioSource", 0))
                            onActivated: bridge.setSetting("TXAudioSource", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: txAudioSrcCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Diagnostica ──
                        Text { text: "DIAGNOSTICA"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Check SWR:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("CheckSWR", false)
                            onCheckedChanged: bridge.setSetting("CheckSWR", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "PWR and SWR:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PWRandSWR", false)
                            onCheckedChanged: bridge.setSetting("PWRandSWR", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 10
                            Rectangle {
                                width: 100; height: controlHeight; radius: 4
                                color: catConnMA.containsMouse ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.3) : bgMedium
                                border.color: accentGreen
                                Text { anchors.centerIn: parent; text: "Connect"; color: accentGreen; font.pixelSize: 12 }
                                MouseArea { id: catConnMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (bridge.nativeCat) bridge.nativeCat.connectRig() } }
                            }
                            Rectangle {
                                width: 100; height: controlHeight; radius: 4
                                color: catDiscMA.containsMouse ? Qt.rgba(1,0.3,0.3,0.3) : bgMedium
                                border.color: "#f44336"
                                Text { anchors.centerIn: parent; text: "Disconnect"; color: "#f44336"; font.pixelSize: 12 }
                                MouseArea { id: catDiscMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (bridge.nativeCat) bridge.nativeCat.disconnectRig() } }
                            }
                        }
                    }
                }

                // ═══════════ TAB 2 — AUDIO ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Dispositivi Audio ──
                        Text { text: "DISPOSITIVI AUDIO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Input Device:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: audioInDevCombo
                            model: bridge.audioInputDevices; Layout.fillWidth: true; implicitHeight: controlHeight
                            Component.onCompleted: { var i = find(bridge.audioInputDevice); if (i >= 0) currentIndex = i }
                            onActivated: bridge.audioInputDevice = currentText
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioInDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Input Channel:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: audioInChCombo
                            model: ["Mono","Left","Right"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: bridge.audioInputChannel
                            onActivated: bridge.audioInputChannel = currentIndex
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioInChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Output Device:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: audioOutDevCombo
                            model: bridge.audioOutputDevices; Layout.fillWidth: true; implicitHeight: controlHeight
                            Component.onCompleted: { var i = find(bridge.audioOutputDevice); if (i >= 0) currentIndex = i }
                            onActivated: bridge.audioOutputDevice = currentText
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioOutDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Output Channel:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: audioOutChCombo
                            model: ["Mono","Left","Right"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: bridge.audioOutputChannel
                            onActivated: bridge.audioOutputChannel = currentIndex
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioOutChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        // ── Livelli ──
                        Text { text: "LIVELLI"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "RX Input Level:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 0; to: 100; value: bridge.rxInputLevel; Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.rxInputLevel = value
                        }

                        Text { text: "TX Output Level:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 0; to: 450; value: Number(bridge.getSetting("TXOutputLevel", 100)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("TXOutputLevel", value)
                        }

                        // ── Directory ──
                        Text { text: "DIRECTORY"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Save Directory:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("SaveDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("SaveDirectory", text)
                        }

                        Text { text: "AzEl Directory:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("AzElDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AzElDirectory", text)
                        }

                        // ── Power Memory ──
                        Text { text: "POWER MEMORY"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Band TX Memory:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PowerBandTXMemory", false)
                            onCheckedChanged: bridge.setSetting("PowerBandTXMemory", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Band Tune Mem:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PowerBandTuneMemory", false)
                            onCheckedChanged: bridge.setSetting("PowerBandTuneMemory", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 3 — TX ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Frequenza e Timing ──
                        Text { text: "FREQUENZA E TIMING"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "TX Frequency:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: txFreqSpin
                            from: 0; to: 5000; value: Number(bridge.getSetting("TXFrequency", 1500)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("TXFrequency", value)
                            contentItem: TextInput { text: txFreqSpin.textFromValue(txFreqSpin.value, txFreqSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !txFreqSpin.editable; validator: txFreqSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "TX Period:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Text { text: bridge.txPeriod !== undefined ? bridge.txPeriod + " s" : "15 s"; color: textPrimary; font.pixelSize: controlFontSize; Layout.fillWidth: true }

                        Text { text: "TX Delay (s):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: txDelaySpin
                            from: 0; to: 30; value: Number(bridge.getSetting("TXDelay", 2)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("TXDelay", value)
                            contentItem: TextInput { text: txDelaySpin.textFromValue(txDelaySpin.value, txDelaySpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !txDelaySpin.editable; validator: txDelaySpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Allow TX QSY:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AllowTXQSY", false)
                            onCheckedChanged: bridge.setSetting("AllowTXQSY", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Sequenza Automatica ──
                        Text { text: "SEQUENZA AUTOMATICA"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Auto Sequence:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.autoSeq
                            onCheckedChanged: bridge.autoSeq = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Start from TX2:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.startFromTx2
                            onCheckedChanged: bridge.startFromTx2 = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Send RR73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.sendRR73
                            onCheckedChanged: bridge.sendRR73 = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Quick QSO:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.quickQsoEnabled
                            onCheckedChanged: bridge.quickQsoEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Confirm 73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.confirm73
                            onCheckedChanged: bridge.confirm73 = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Disable TX 73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DisableTXon73", false)
                            onCheckedChanged: bridge.setSetting("DisableTXon73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Repeat TX 73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("RepeatTXuntil73", false)
                            onCheckedChanged: bridge.setSetting("RepeatTXuntil73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Watchdog ──
                        Text { text: "WATCHDOG"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "TX Watchdog (min):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: txWdSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("TXWatchdog", 6)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("TXWatchdog", value)
                            contentItem: TextInput { text: txWdSpin.textFromValue(txWdSpin.value, txWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !txWdSpin.editable; validator: txWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Tune Watchdog:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            CheckBox {
                                id: tuneWdCheck
                                checked: bridge.getSetting("TuneWatchdogEnabled", false)
                                onCheckedChanged: bridge.setSetting("TuneWatchdogEnabled", checked)
                                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                contentItem: Text { text: ""; leftPadding: 24 }
                            }
                            SpinBox {
                                id: tuneWdSpin
                                from: 1; to: 120; value: Number(bridge.getSetting("TuneWatchdogMin", 5)); editable: true; enabled: tuneWdCheck.checked
                                implicitHeight: controlHeight; Layout.fillWidth: true
                                onValueChanged: bridge.setSetting("TuneWatchdogMin", value)
                                contentItem: TextInput { text: tuneWdSpin.textFromValue(tuneWdSpin.value, tuneWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !tuneWdSpin.editable; validator: tuneWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            }
                        }

                        // ── CW ID ──
                        Text { text: "CW ID"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "CW ID after 73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("CWIDafter73", false)
                            onCheckedChanged: bridge.setSetting("CWIDafter73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "CW ID Interval:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: cwIdIntSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("CWIDInterval", 10)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("CWIDInterval", value)
                            contentItem: TextInput { text: cwIdIntSpin.textFromValue(cwIdIntSpin.value, cwIdIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !cwIdIntSpin.editable; validator: cwIdIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Tone Spacing ──
                        Text { text: "TONE SPACING"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "2x Tone Spacing:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("x2ToneSpacing", false)
                            onCheckedChanged: bridge.setSetting("x2ToneSpacing", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "4x Tone Spacing:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("x4ToneSpacing", false)
                            onCheckedChanged: bridge.setSetting("x4ToneSpacing", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Alt F1-F6 Bind:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlternateF1F6Bindings", false)
                            onCheckedChanged: bridge.setSetting("AlternateF1F6Bindings", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 4 — DISPLAY ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Font ──
                        Text { text: "FONT"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Font:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("Font", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Font", text)
                        }
                        Text { text: "Decoded Font:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("DecodedTextFont", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("DecodedTextFont", text)
                        }

                        // ── Decodifiche ──
                        Text { text: "DECODIFICHE"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Show DXCC:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowDXCC", false)
                            onCheckedChanged: bridge.setSetting("ShowDXCC", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Country Names:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowCountryNames", false)
                            onCheckedChanged: bridge.setSetting("ShowCountryNames", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Principal Prefix:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PrincipalPrefix", false)
                            onCheckedChanged: bridge.setSetting("PrincipalPrefix", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "HL DX Grid:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightDXGrid", false)
                            onCheckedChanged: bridge.setSetting("HighlightDXGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "HL DX Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightDXCall", false)
                            onCheckedChanged: bridge.setSetting("HighlightDXCall", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Blank Line:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("InsertBlankLine", false)
                            onCheckedChanged: bridge.setSetting("InsertBlankLine", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Detailed Blank:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DetailedBlank", false)
                            onCheckedChanged: bridge.setSetting("DetailedBlank", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "From Top:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DecodesFromTop", false)
                            onCheckedChanged: bridge.setSetting("DecodesFromTop", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "TX Msg to RX:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TXMessagesToRX", false)
                            onCheckedChanged: bridge.setSetting("TXMessagesToRX", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Mappa e Distanza ──
                        Text { text: "MAPPA E DISTANZA"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Show Distance:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowDistance", false)
                            onCheckedChanged: bridge.setSetting("ShowDistance", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Show Azimuth:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowAzimuth", false)
                            onCheckedChanged: bridge.setSetting("ShowAzimuth", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Miles:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Miles", false)
                            onCheckedChanged: bridge.setSetting("Miles", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Greyline:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowGreyline", false)
                            onCheckedChanged: bridge.setSetting("ShowGreyline", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Map All Msgs:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapAllMessages", false)
                            onCheckedChanged: bridge.setSetting("MapAllMessages", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Grid to State:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapGridToState", false)
                            onCheckedChanged: bridge.setSetting("MapGridToState", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Click TX:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapSingleClickTX", false)
                            onCheckedChanged: bridge.setSetting("MapSingleClickTX", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Allineamento ──
                        Text { text: "ALLINEAMENTO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Align:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Align", false)
                            onCheckedChanged: bridge.setSetting("Align", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Align Steps:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: alignStepsSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AlignSteps", value)
                            contentItem: TextInput { text: alignStepsSpin.textFromValue(alignStepsSpin.value, alignStepsSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !alignStepsSpin.editable; validator: alignStepsSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Align Steps 2:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: alignSteps2Spin
                            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps2", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AlignSteps2", value)
                            contentItem: TextInput { text: alignSteps2Spin.textFromValue(alignSteps2Spin.value, alignSteps2Spin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !alignSteps2Spin.editable; validator: alignSteps2Spin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 5 — DECODIFICA ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Parametri Decodifica ──
                        Text { text: "PARAMETRI DECODIFICA"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Decode Depth:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: decodeDepthCombo
                            model: ["Fast","Normal","Deep"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: bridge.ndepth - 1
                            onActivated: bridge.ndepth = currentIndex + 1
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: decodeDepthCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: "Low Freq (Hz):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: nfaSpin
                            from: 0; to: 5000; value: bridge.nfa; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.nfa = value
                            contentItem: TextInput { text: nfaSpin.textFromValue(nfaSpin.value, nfaSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !nfaSpin.editable; validator: nfaSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "High Freq (Hz):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: nfbSpin
                            from: 0; to: 5000; value: bridge.nfb; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.nfb = value
                            contentItem: TextInput { text: nfbSpin.textFromValue(nfbSpin.value, nfbSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !nfbSpin.editable; validator: nfbSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "RX Bandwidth:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: rxBwSpin
                            from: 100; to: 5000; value: Number(bridge.getSetting("RXBandwidth", 2500)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("RXBandwidth", value)
                            contentItem: TextInput { text: rxBwSpin.textFromValue(rxBwSpin.value, rxBwSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !rxBwSpin.editable; validator: rxBwSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Decode at 52s:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DecodeAt52s", false)
                            onCheckedChanged: bridge.setSetting("DecodeAt52s", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Single Decode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("SingleDecode", false)
                            onCheckedChanged: bridge.setSetting("SingleDecode", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── JT65 VHF/UHF ──
                        Text { text: "JT65 VHF/UHF"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Erasure Patterns:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: erasurePatSpin
                            from: 0; to: 99999; value: Number(bridge.getSetting("RandomErasurePatterns", 7)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("RandomErasurePatterns", value)
                            contentItem: TextInput { text: erasurePatSpin.textFromValue(erasurePatSpin.value, erasurePatSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !erasurePatSpin.editable; validator: erasurePatSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Aggressive:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: aggressiveSpin
                            from: 0; to: 10; value: Number(bridge.getSetting("AggressiveLevel", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AggressiveLevel", value)
                            contentItem: TextInput { text: aggressiveSpin.textFromValue(aggressiveSpin.value, aggressiveSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !aggressiveSpin.editable; validator: aggressiveSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Two-Pass:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TwoPassDecoding", false)
                            onCheckedChanged: bridge.setSetting("TwoPassDecoding", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Sidelobe Control ──
                        Text { text: "SIDELOBE CONTROL"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Sidelobe Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: sidelobeCombo
                            model: ["Low Sidelobes","Max Sensitivity"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("SidelobeMode", 0))
                            onActivated: bridge.setSetting("SidelobeMode", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: sidelobeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "Degrade S/N:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: degradeSnSpin
                            from: 0; to: 100; value: Number(bridge.getSetting("DegradeSN", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("DegradeSN", value)
                            contentItem: TextInput { text: degradeSnSpin.textFromValue(degradeSnSpin.value, degradeSnSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !degradeSnSpin.editable; validator: degradeSnSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Filtri Decodifica ──
                        Text { text: "FILTRI DECODIFICA"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "CQ Only:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.filterCqOnly
                            onCheckedChanged: bridge.filterCqOnly = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "My Call Only:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.filterMyCallOnly
                            onCheckedChanged: bridge.filterMyCallOnly = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Zap:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.zapEnabled
                            onCheckedChanged: bridge.zapEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Deep Search:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.deepSearchEnabled
                            onCheckedChanged: bridge.deepSearchEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Avg Decode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.avgDecodeEnabled
                            onCheckedChanged: bridge.avgDecodeEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 6 — REPORTING ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Servizi di Rete ──
                        Text { text: "SERVIZI DI RETE"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "PSK Reporter:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.pskReporterEnabled
                            onCheckedChanged: bridge.pskReporterEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "TCP/IP:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PSKRtcpip", false)
                            onCheckedChanged: bridge.setSetting("PSKRtcpip", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Cloudlog ──
                        Text { text: "CLOUDLOG"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.cloudlogEnabled
                            onCheckedChanged: bridge.cloudlogEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: "API URL:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.cloudlogUrl; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.cloudlogUrl = text
                        }

                        Text { text: "API Key:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.cloudlogApiKey; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.cloudlogApiKey = text
                        }

                        Text { text: "Station ID:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: cloudlogStIdSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("CloudlogStationID", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("CloudlogStationID", value)
                            contentItem: TextInput { text: cloudlogStIdSpin.textFromValue(cloudlogStIdSpin.value, cloudlogStIdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !cloudlogStIdSpin.editable; validator: cloudlogStIdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── LotW ──
                        Text { text: "LOTW"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "LotW Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.lotwEnabled
                            onCheckedChanged: bridge.lotwEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Password:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("LoTWPassword", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("LoTWPassword", text)
                        }

                        Text { text: "Non-QSL'd:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LoTWNonQSL", false)
                            onCheckedChanged: bridge.setSetting("LoTWNonQSL", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Days Upload:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: lotwDaysSpin
                            from: 0; to: 9999; value: Number(bridge.getSetting("LoTWDaysSinceUpload", 365)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("LoTWDaysSinceUpload", value)
                            contentItem: TextInput { text: lotwDaysSpin.textFromValue(lotwDaysSpin.value, lotwDaysSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !lotwDaysSpin.editable; validator: lotwDaysSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Logging ──
                        Text { text: "LOGGING"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Prompt to Log:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PromptToLog", true)
                            onCheckedChanged: bridge.setSetting("PromptToLog", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Auto Log:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AutoLog", false)
                            onCheckedChanged: bridge.setSetting("AutoLog", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Direct Log QSO:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.directLogQso
                            onCheckedChanged: bridge.directLogQso = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Log as RTTY:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LogAsRTTY", false)
                            onCheckedChanged: bridge.setSetting("LogAsRTTY", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "4-digit Grids:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Log4DigitGrids", false)
                            onCheckedChanged: bridge.setSetting("Log4DigitGrids", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Contest Only:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ContestingOnly", false)
                            onCheckedChanged: bridge.setSetting("ContestingOnly", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Spec Op Cmts:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("SpecOpInComments", false)
                            onCheckedChanged: bridge.setSetting("SpecOpInComments", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "dB in Cmts:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("dBReportsToComments", false)
                            onCheckedChanged: bridge.setSetting("dBReportsToComments", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "ZZ00:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ZZ00", false)
                            onCheckedChanged: bridge.setSetting("ZZ00", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Registrazione ──
                        Text { text: "REGISTRAZIONE"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Record RX:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.recordRxEnabled
                            onCheckedChanged: bridge.recordRxEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Record TX:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.recordTxEnabled
                            onCheckedChanged: bridge.recordTxEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "WSPR Upload:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.wsprUploadEnabled
                            onCheckedChanged: bridge.wsprUploadEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── UDP Server ──
                        Text { text: "UDP SERVER"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Server Name:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("UDPServerName", "127.0.0.1"); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("UDPServerName", text)
                        }
                        Text { text: "Server Port:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: udpPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPServerPort", 2237)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("UDPServerPort", value)
                            contentItem: TextInput { text: udpPortSpin.textFromValue(udpPortSpin.value, udpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpPortSpin.editable; validator: udpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Listen Port:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: udpListenSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPListenPort", 2237)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("UDPListenPort", value)
                            contentItem: TextInput { text: udpListenSpin.textFromValue(udpListenSpin.value, udpListenSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpListenSpin.editable; validator: udpListenSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "TTL:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: udpTtlSpin
                            from: 0; to: 255; value: Number(bridge.getSetting("UDPTTL", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("UDPTTL", value)
                            contentItem: TextInput { text: udpTtlSpin.textFromValue(udpTtlSpin.value, udpTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpTtlSpin.editable; validator: udpTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Accept UDP:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AcceptUDPRequests", false)
                            onCheckedChanged: bridge.setSetting("AcceptUDPRequests", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Notify Request:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("NotifyOnRequest", false)
                            onCheckedChanged: bridge.setSetting("NotifyOnRequest", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Restore Win:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("RestoreWindow", false)
                            onCheckedChanged: bridge.setSetting("RestoreWindow", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 7 — COLORI ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Colori Decodifica ──
                        Text { text: "COLORI DECODIFICA"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        // Color CQ
                        Text { text: "Color CQ:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorCqRect
                            width: 60; height: 24; radius: 4; color: bridge.colorCQ; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorCqPop.open() }
                            Popup {
                                id: colorCqPop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorCQ = modelData; colorCqPop.close() } }
                                    } }
                                }
                            }
                        }
                        // Color My Call
                        Text { text: "Color My Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorMyCallRect
                            width: 60; height: 24; radius: 4; color: bridge.colorMyCall; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorMyCallPop.open() }
                            Popup {
                                id: colorMyCallPop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorMyCall = modelData; colorMyCallPop.close() } }
                                    } }
                                }
                            }
                        }

                        // Color DX Entity
                        Text { text: "Color DX Entity:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorDxEntRect
                            width: 60; height: 24; radius: 4; color: bridge.colorDXEntity; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorDxEntPop.open() }
                            Popup {
                                id: colorDxEntPop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorDXEntity = modelData; colorDxEntPop.close() } }
                                    } }
                                }
                            }
                        }
                        // Color 73
                        Text { text: "Color 73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: color73Rect
                            width: 60; height: 24; radius: 4; color: bridge.color73; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: color73Pop.open() }
                            Popup {
                                id: color73Pop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.color73 = modelData; color73Pop.close() } }
                                    } }
                                }
                            }
                        }

                        // Color B4
                        Text { text: "Color B4:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorB4Rect
                            width: 60; height: 24; radius: 4; color: bridge.colorB4; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorB4Pop.open() }
                            Popup {
                                id: colorB4Pop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorB4 = modelData; colorB4Pop.close() } }
                                    } }
                                }
                            }
                        }
                        Text { text: "B4 Strikethrough:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.b4Strikethrough
                            onCheckedChanged: bridge.b4Strikethrough = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Highlighting ──
                        Text { text: "HIGHLIGHTING"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Only Sought:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("OnlyFieldsSought", false)
                            onCheckedChanged: bridge.setSetting("OnlyFieldsSought", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "WAE Entities:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("IncludeWAEEntities", false)
                            onCheckedChanged: bridge.setSetting("IncludeWAEEntities", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Highlight 73:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Highlight73", false)
                            onCheckedChanged: bridge.setSetting("Highlight73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "HL by Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightByMode", false)
                            onCheckedChanged: bridge.setSetting("HighlightByMode", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "HL Orange:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightOrange", false)
                            onCheckedChanged: bridge.setSetting("HighlightOrange", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Orange Calls:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OrangeCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OrangeCallsigns", text)
                        }

                        Text { text: "HL Blue:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightBlue", false)
                            onCheckedChanged: bridge.setSetting("HighlightBlue", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Blue Calls:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("BlueCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("BlueCallsigns", text)
                        }

                        // ── Spettro ──
                        Text { text: "SPETTRO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Palette:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: paletteCombo
                            model: ["Default","Blue","Green","AFMHot","LinRad","CubeYF"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            currentIndex: Number(bridge.getSetting("SpectrumPalette", 0))
                            onActivated: bridge.setSetting("SpectrumPalette", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: paletteCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "Ref Level:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: -40; to: 40; value: Number(bridge.getSetting("SpectrumRefLevel", 0)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("SpectrumRefLevel", value)
                        }

                        Text { text: "Dynamic Range:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 10; to: 120; value: Number(bridge.getSetting("SpectrumDynRange", 60)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("SpectrumDynRange", value)
                        }

                        // ── Download Dati ──
                        Text { text: "DOWNLOAD DATI"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 10
                            Rectangle {
                                width: 140; height: controlHeight; radius: 4
                                color: dlCtyMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: "Download CTY.dat"; color: primaryBlue; font.pixelSize: 12 }
                                MouseArea { id: dlCtyMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.setSetting("DownloadCTY", true) }
                            }
                            Rectangle {
                                width: 160; height: controlHeight; radius: 4
                                color: dlCall3MA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: "Download CALL3.TXT"; color: primaryBlue; font.pixelSize: 12 }
                                MouseArea { id: dlCall3MA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.setSetting("DownloadCALL3", true) }
                            }
                        }
                    }
                }

                // ═══════════ TAB 8 — AVANZATE ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Avvio ──
                        Text { text: "AVVIO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Monitor OFF:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MonitorOFF", false)
                            onCheckedChanged: bridge.setSetting("MonitorOFF", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Monitor Last:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MonitorLastUsed", false)
                            onCheckedChanged: bridge.setSetting("MonitorLastUsed", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Auto Astro:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AutoAstroWindow", false)
                            onCheckedChanged: bridge.setSetting("AutoAstroWindow", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "kHz no k:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("kHzWithoutk", false)
                            onCheckedChanged: bridge.setSetting("kHzWithoutk", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Progress Red:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ProgressBarRed", false)
                            onCheckedChanged: bridge.setSetting("ProgressBarRed", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "High DPI:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighDPI", false)
                            onCheckedChanged: bridge.setSetting("HighDPI", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Larger Tab:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LargerTabWidget", false)
                            onCheckedChanged: bridge.setSetting("LargerTabWidget", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Comportamento ──
                        Text { text: "COMPORTAMENTO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Quick Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("QuickCall", false)
                            onCheckedChanged: bridge.setSetting("QuickCall", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Force Call 1st:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ForceCall1st", false)
                            onCheckedChanged: bridge.setSetting("ForceCall1st", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "VHF/UHF:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.vhfUhfFeatures
                            onCheckedChanged: bridge.vhfUhfFeatures = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Wait Features:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("EnableWaitFeatures", false)
                            onCheckedChanged: bridge.setSetting("EnableWaitFeatures", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Erase Band Act:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("EraseBandActivity", false)
                            onCheckedChanged: bridge.setSetting("EraseBandActivity", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Clear DX Grid:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ClearDXGrid", false)
                            onCheckedChanged: bridge.setSetting("ClearDXGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Clear DX Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ClearDXCall", false)
                            onCheckedChanged: bridge.setSetting("ClearDXCall", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "RX>TX after QSO:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("RXtoTXafterQSO", false)
                            onCheckedChanged: bridge.setSetting("RXtoTXafterQSO", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Alt Erase Btn:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlternateEraseButton", false)
                            onCheckedChanged: bridge.setSetting("AlternateEraseButton", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "No Btn Color:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DisableButtonColoring", false)
                            onCheckedChanged: bridge.setSetting("DisableButtonColoring", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Modo Operativo ──
                        Text { text: "MODO OPERATIVO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Split Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.splitMode
                            onCheckedChanged: bridge.splitMode = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "SWL Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.swlMode
                            onCheckedChanged: bridge.swlMode = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Fox Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.foxMode
                            onCheckedChanged: bridge.foxMode = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Hound Mode:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.houndMode
                            onCheckedChanged: bridge.houndMode = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "SuperFox:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("SuperFox", false)
                            onCheckedChanged: bridge.setSetting("SuperFox", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Show OTP:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowOTP", false)
                            onCheckedChanged: bridge.setSetting("ShowOTP", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Contest ──
                        Text { text: "CONTEST"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Activity:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: contestCombo
                            model: ["None","ARRL Digi","WW DIGI","Field Day","RTTY Roundup","EU VHF","NA VHF","Fox","Hound","Q65 Pileup"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            currentIndex: Number(bridge.getSetting("SelectedActivity", 0))
                            onActivated: bridge.setSetting("SelectedActivity", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: contestCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: "FD Exchange:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("FieldDayExchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("FieldDayExchange", text)
                        }
                        Text { text: "RTTY Exchange:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("RTTYExchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RTTYExchange", text)
                        }

                        Text { text: "Contest Name:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("ContestName", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("ContestName", text)
                        }
                        Text { text: "Indiv Name:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("IndividualContestName", false)
                            onCheckedChanged: bridge.setSetting("IndividualContestName", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "NCCC Sprint:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("NCCCSprint", false)
                            onCheckedChanged: bridge.setSetting("NCCCSprint", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Clock Compensation ──
                        Text { text: "CLOCK COMPENSATION"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "DT Synced (ms):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: dtSyncSpin
                            from: 0; to: 9999; value: Number(bridge.getSetting("DTClampSynced", 500)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("DTClampSynced", value)
                            contentItem: TextInput { text: dtSyncSpin.textFromValue(dtSyncSpin.value, dtSyncSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !dtSyncSpin.editable; validator: dtSyncSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "DT Unsync (ms):"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: dtUnsyncSpin
                            from: 0; to: 9999; value: Number(bridge.getSetting("DTClampUnsynced", 1000)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("DTClampUnsynced", value)
                            contentItem: TextInput { text: dtUnsyncSpin.textFromValue(dtUnsyncSpin.value, dtUnsyncSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !dtUnsyncSpin.editable; validator: dtUnsyncSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── OTP ──
                        Text { text: "OTP"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "OTP Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("OTPEnabled", false)
                            onCheckedChanged: bridge.setSetting("OTPEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "OTP Seed:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OTPSeed", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OTPSeed", text)
                        }

                        Text { text: "OTP Interval:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: otpIntSpin
                            from: 1; to: 3600; value: Number(bridge.getSetting("OTPInterval", 30)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("OTPInterval", value)
                            contentItem: TextInput { text: otpIntSpin.textFromValue(otpIntSpin.value, otpIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !otpIntSpin.editable; validator: otpIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: "OTP URL:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OTPURL", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OTPURL", text)
                        }
                    }
                }

                // ═══════════ TAB 9 — ALERTS ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Audio Alerts ──
                        Text { text: "AUDIO ALERTS"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Alerts Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertSoundsEnabled
                            onCheckedChanged: bridge.alertSoundsEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // Alert grid
                        Text { text: "CQ in Msg:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertCQInMsg", false)
                            onCheckedChanged: bridge.setSetting("AlertCQInMsg", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "My Call:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertMyCall", false)
                            onCheckedChanged: bridge.setSetting("AlertMyCall", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "New DXCC:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewDXCC", false)
                            onCheckedChanged: bridge.setSetting("AlertNewDXCC", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "New DXCC Band:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewDXCCBand", false)
                            onCheckedChanged: bridge.setSetting("AlertNewDXCCBand", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "New Grid:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewGrid", false)
                            onCheckedChanged: bridge.setSetting("AlertNewGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "New Grid Band:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewGridBand", false)
                            onCheckedChanged: bridge.setSetting("AlertNewGridBand", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "New Continent:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewContinent", false)
                            onCheckedChanged: bridge.setSetting("AlertNewContinent", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "New Cont Band:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewContinentBand", false)
                            onCheckedChanged: bridge.setSetting("AlertNewContinentBand", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "New CQ Zone:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewCQZone", false)
                            onCheckedChanged: bridge.setSetting("AlertNewCQZone", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "CQ Zone Band:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewCQZoneBand", false)
                            onCheckedChanged: bridge.setSetting("AlertNewCQZoneBand", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "New ITU Zone:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewITUZone", false)
                            onCheckedChanged: bridge.setSetting("AlertNewITUZone", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "ITU Zone Band:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertNewITUZoneBand", false)
                            onCheckedChanged: bridge.setSetting("AlertNewITUZoneBand", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "DX Call/Grid:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertDXCallGrid", false)
                            onCheckedChanged: bridge.setSetting("AlertDXCallGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "QSY Message:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlertQSYMessage", false)
                            onCheckedChanged: bridge.setSetting("AlertQSYMessage", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 10 — FILTRI ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Blacklist ──
                        Text { text: "BLACKLIST"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("BlacklistEnabled", false)
                            onCheckedChanged: bridge.setSetting("BlacklistEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // Blacklist 1-12 (2 per row)
                        Text { text: "Blacklist 1:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist1", text) }
                        Text { text: "Blacklist 2:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist2", text) }

                        Text { text: "Blacklist 3:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist3", text) }
                        Text { text: "Blacklist 4:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist4", text) }

                        Text { text: "Blacklist 5:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist5", text) }
                        Text { text: "Blacklist 6:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist6", text) }

                        Text { text: "Blacklist 7:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist7", text) }
                        Text { text: "Blacklist 8:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist8", text) }

                        Text { text: "Blacklist 9:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist9", text) }
                        Text { text: "Blacklist 10:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist10", text) }

                        Text { text: "Blacklist 11:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist11", text) }
                        Text { text: "Blacklist 12:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist12", text) }

                        // ── Whitelist ──
                        Text { text: "WHITELIST"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("WhitelistEnabled", false)
                            onCheckedChanged: bridge.setSetting("WhitelistEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: "Whitelist 1:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist1", text) }
                        Text { text: "Whitelist 2:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist2", text) }

                        Text { text: "Whitelist 3:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist3", text) }
                        Text { text: "Whitelist 4:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist4", text) }

                        Text { text: "Whitelist 5:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist5", text) }
                        Text { text: "Whitelist 6:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist6", text) }

                        Text { text: "Whitelist 7:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist7", text) }
                        Text { text: "Whitelist 8:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist8", text) }

                        Text { text: "Whitelist 9:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist9", text) }
                        Text { text: "Whitelist 10:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist10", text) }

                        Text { text: "Whitelist 11:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist11", text) }
                        Text { text: "Whitelist 12:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist12", text) }

                        // ── Always Pass ──
                        Text { text: "ALWAYS PASS"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Enabled:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlwaysPassEnabled", false)
                            onCheckedChanged: bridge.setSetting("AlwaysPassEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: "Always Pass 1:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass1", text) }
                        Text { text: "Always Pass 2:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass2", text) }

                        Text { text: "Always Pass 3:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass3", text) }
                        Text { text: "Always Pass 4:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass4", text) }

                        Text { text: "Always Pass 5:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass5", text) }
                        Text { text: "Always Pass 6:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass6", text) }

                        Text { text: "Always Pass 7:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass7", text) }
                        Text { text: "Always Pass 8:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass8", text) }

                        Text { text: "Always Pass 9:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass9", text) }
                        Text { text: "Always Pass 10:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass10", text) }

                        Text { text: "Always Pass 11:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass11", text) }
                        Text { text: "Always Pass 12:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("AlwaysPass12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AlwaysPass12", text) }

                        // ── Territory ──
                        Text { text: "TERRITORY"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Territory 1:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory1", text) }
                        Text { text: "Territory 2:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory2", text) }

                        Text { text: "Territory 3:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory3", text) }
                        Text { text: "Territory 4:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory4", text) }

                        // ── Opzioni Filtro ──
                        Text { text: "OPZIONI FILTRO"; color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: "Wait & Pounce:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("FiltersWaitPounceOnly", false)
                            onCheckedChanged: bridge.setSetting("FiltersWaitPounceOnly", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: "Calling Only:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("FiltersCallingOnly", false)
                            onCheckedChanged: bridge.setSetting("FiltersCallingOnly", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: "Worked Today+:"; color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("QuickFilterWorked", false)
                            onCheckedChanged: bridge.setSetting("QuickFilterWorked", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

            } // StackLayout
        } // RowLayout
    } // contentItem
}
