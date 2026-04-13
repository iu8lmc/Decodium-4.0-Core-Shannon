/* Decodium — DX Cluster Panel
 * Mostra gli spot dal DX Cluster TCP in tempo reale.
 * Doppio-click su uno spot → QSY alla frequenza.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    signal closeRequested()

    property color bgDeep:        bridge.themeManager.bgDeep
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder:   bridge.themeManager.glassBorder
    property color warningOrange: bridge.themeManager.warningColor

    color:         Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.93)
    border.color:  glassBorder
    border.width:  1
    radius:        8
    width:         580
    height:        480

    // ── header ──────────────────────────────────────────────────────────────
    Rectangle {
        id: header
        width: parent.width; height: 40
        color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
        radius: 8
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: parent.color }

        // Zona drag solo nella parte sinistra del titolo
        MouseArea {
            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
            width: parent.width * 0.4
            drag.target: root
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0
            drag.maximumX: root.parent ? root.parent.width - root.width : root.x
            drag.minimumY: 0
            drag.maximumY: root.parent ? root.parent.height - 50 : root.y
            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: root.z = 9001
        }

        // Titolo (a sinistra)
        Row {
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: 10
            spacing: 6
            Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter
                color: bridge.dxClusterConnected ? "#00e676" : "#ef5350" }
            Text { text: "DX Cluster"; color: secondaryCyan; font.bold: true; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
            Text { text: bridge.dxClusterConnected ? bridge.dxClusterHost : ""; color: textSecondary; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
        }

        // Pulsanti a destra (z alto per essere sopra tutto)
        Row {
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.rightMargin: 6
            spacing: 6
            z: 10

            // Pulsante CONNETTI grande
            Rectangle {
                width: 90; height: 28; radius: 6
                color: connBtnArea.pressed ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.5)
                     : connBtnArea.containsMouse ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.35)
                     : Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.15)
                border.color: bridge.dxClusterConnected ? "#f44336" : accentGreen
                border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: bridge.dxClusterConnected ? "Disconnetti" : "Connetti"
                    color: bridge.dxClusterConnected ? "#f44336" : accentGreen
                    font.pixelSize: 11; font.bold: true
                }
                MouseArea {
                    id: connBtnArea; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (bridge.dxClusterConnected) {
                            bridge.disconnectDxCluster()
                        } else {
                            var h = hostField.text ? hostField.text.trim() : "dx.iz7auh.net"
                            var p = portField.text ? parseInt(portField.text) : 8000
                            if (!h) h = "dx.iz7auh.net"
                            if (!p || p <= 0) p = 8000
                            bridge.connectDxCluster(h, p)
                        }
                    }
                }
            }

            // Pulsante chiudi
            Rectangle {
                width: 28; height: 28; radius: 6
                color: closeArea.containsMouse ? Qt.rgba(1, 0.2, 0.2, 0.4) : "transparent"
                border.color: closeArea.containsMouse ? "#f44336" : glassBorder
                Text { anchors.centerIn: parent; text: "✕"; color: closeArea.containsMouse ? "#f44336" : textSecondary; font.pixelSize: 13 }
                MouseArea {
                    id: closeArea; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }
        }
    }

    // ── barra connessione (host + port + connetti) ────────────────────────
    Rectangle {
        id: connBar
        anchors.top: header.bottom
        width: parent.width; height: 36
        color: Qt.rgba(0, 0, 0, 0.3)

        RowLayout {
            anchors.fill: parent; anchors.margins: 4; spacing: 4

            Text { text: "Host:"; color: textSecondary; font.pixelSize: 10 }
            TextField {
                id: hostField
                Layout.fillWidth: true; implicitHeight: 28; leftPadding: 6; font.pixelSize: 12
                color: textPrimary; placeholderText: "dx.iz7auh.net"
                text: bridge.dxClusterHost || "dx.iz7auh.net"
                background: Rectangle { color: Qt.rgba(0,0,0,0.4); border.color: glassBorder; radius: 3 }
                onEditingFinished: bridge.dxClusterHost = text
            }
            Text { text: ":"; color: textSecondary; font.pixelSize: 10 }
            TextField {
                id: portField
                implicitWidth: 60; implicitHeight: 28; leftPadding: 6; font.pixelSize: 12
                color: textPrimary; placeholderText: "8000"
                text: String(bridge.dxClusterPort || 8000)
                background: Rectangle { color: Qt.rgba(0,0,0,0.4); border.color: glassBorder; radius: 3 }
                validator: IntValidator { bottom: 1; top: 65535 }
                onEditingFinished: bridge.dxClusterPort = parseInt(text)
            }

            // Pulsante connetti rimosso — usa quello nell'header
        }
    }

    // ── colonne header tabella ───────────────────────────────────────────────
    Rectangle {
        id: colHeader
        anchors.top: connBar.bottom
        width: parent.width; height: 22
        color: Qt.rgba(0, 0, 0, 0.25)

        Row {
            anchors { fill: parent; leftMargin: 6 }
            spacing: 0
            Text { width: 46;  text: "Ora";      color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { width: 76;  text: "DX Call";  color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { width: 70;  text: "Freq";     color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { width: 44;  text: "Banda";    color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { width: 48;  text: "Modo";     color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { width: 70;  text: "Spotter";  color: textSecondary; font.pixelSize: 10; font.bold: true }
            Text { Layout.fillWidth: true; text: "Info"; color: textSecondary; font.pixelSize: 10; font.bold: true }
        }
    }

    // ── lista spot ───────────────────────────────────────────────────────────
    ListView {
        id: spotList
        anchors {
            top:    colHeader.bottom
            left:   parent.left
            right:  parent.right
            bottom: filterRow.top
            margins: 1
        }
        clip: true
        model: {
            var all = bridge.dxClusterSpots || []
            if (root.modeFilter === "") return all
            return all.filter(function(s) { return s["mode"] === root.modeFilter })
        }

        // auto-scroll verso il basso quando arrivano nuovi spot
        onCountChanged: if (count > 0) positionViewAtEnd()

        delegate: Rectangle {
            width:  spotList.width
            height: 28
            color: {
                if (rowMouse.containsMouse)
                    return Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
                // evidenzia modi digitali
                var md = modelData["mode"] || ""
                if (md === "FT8" || md === "FT4" || md === "FT2")
                    return Qt.rgba(0, 0.9, 0.5, 0.07)
                return index % 2 === 0 ? Qt.rgba(0,0,0,0.05) : Qt.rgba(0,0,0,0)
            }
            radius: 2

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                // doppio-click → QSY alla frequenza dello spot
                onDoubleClicked: {
                    var freqKhz = modelData["frequency"] || 0
                    if (freqKhz > 0) {
                        var mode = modelData["mode"] || ""
                        bridge.qsyTo(freqKhz * 1000.0, mode)
                    }
                }

                // singolo click → pre-seleziona callsign
                onClicked: {
                    var call = modelData["dxCall"] || ""
                    if (call.length > 0) bridge.dxCall = call
                }
            }

            Row {
                anchors { fill: parent; leftMargin: 6 }
                spacing: 0

                Text {
                    width: 46
                    text: modelData["time"] || ""
                    color: textSecondary; font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Text {
                    width: 76
                    text: modelData["dxCall"] || ""
                    color: accentGreen; font.pixelSize: 11; font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: 70
                    text: {
                        var f = modelData["frequency"] || 0
                        return f > 0 ? f.toFixed(1) + " kHz" : ""
                    }
                    color: textPrimary; font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Text {
                    width: 44
                    text: modelData["band"] || ""
                    color: secondaryCyan; font.pixelSize: 10
                }
                Rectangle {
                    width: 48; height: 18; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: {
                        var md = modelData["mode"] || ""
                        if (md === "FT8") return Qt.rgba(0, 0.7, 1, 0.25)
                        if (md === "FT4") return Qt.rgba(0, 1, 0.6, 0.2)
                        if (md === "FT2") return Qt.rgba(1, 0.6, 0, 0.22)
                        if (md === "CW")  return Qt.rgba(1, 1, 0, 0.2)
                        if (md === "SSB") return Qt.rgba(0.8, 0.2, 0.8, 0.2)
                        return Qt.rgba(0.5, 0.5, 0.5, 0.15)
                    }
                    Text {
                        anchors.centerIn: parent
                        text: modelData["mode"] || "?"
                        color: textPrimary; font.pixelSize: 10; font.bold: true
                    }
                }
                Text {
                    width: 70
                    text: modelData["spotter"] || ""
                    color: textSecondary; font.pixelSize: 10
                    leftPadding: 4
                    elide: Text.ElideRight
                }
                Text {
                    width: root.width - 358
                    text: modelData["comment"] || ""
                    color: textSecondary; font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            // tooltip callsign su hover
            ToolTip {
                visible: rowMouse.containsMouse
                delay: 500
                text: {
                    var c = modelData["dxCall"] || ""
                    var f = modelData["frequency"] || 0
                    var sp = modelData["spotter"] || ""
                    return c + "  —  " + f.toFixed(1) + " kHz\nSpotter: " + sp +
                           "\n↵ click = imposta DX call\n↵↵ doppio-click = QSY"
                }
                contentItem: Text {
                    text: parent.text; color: "#ffffff"; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                background: Rectangle {
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
                    border.color: secondaryCyan; radius: 4
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle { radius: 2; color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) }
        }
    }

    // ── barra filtro in basso ─────────────────────────────────────────────────
    Rectangle {
        id: filterRow
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 34
        color: Qt.rgba(0, 0, 0, 0.2)
        radius: 8
        // top corners piatti
        Rectangle { anchors.top: parent.top; width: parent.width; height: 8; color: parent.color }

        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            spacing: 8

            Text { text: "Filtro:"; color: textSecondary; font.pixelSize: 10 }

            // filtro per modo
            Repeater {
                model: ["FT8", "FT4", "FT2", "CW", "SSB"]
                delegate: Rectangle {
                    width: 34; height: 20; radius: 3
                    property bool active: root.modeFilter === modelData
                    color: active
                           ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.4)
                           : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.1)
                    border.color: secondaryCyan; border.width: active ? 1 : 0
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.modeFilter = (root.modeFilter === modelData ? "" : modelData)
                    }
                    Text { anchors.centerIn: parent; text: modelData; color: textPrimary; font.pixelSize: 9 }
                }
            }

            Item { Layout.fillWidth: true }

            // pulsante svuota lista
            Rectangle {
                width: 60; height: 22; radius: 4
                color: clearMouse.containsMouse ? Qt.rgba(1,0.3,0.3,0.3) : Qt.rgba(0.5,0.5,0.5,0.1)
                border.color: Qt.rgba(1,0.4,0.4,0.5); border.width: 1
                MouseArea {
                    id: clearMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (bridge.dxCluster) bridge.dxCluster.clearSpots()
                }
                Text { anchors.centerIn: parent; text: "Clear"; color: textSecondary; font.pixelSize: 10 }
            }
        }
    }

    property string modeFilter: ""

    // ── Terminal Telnet ──────────────────────────────────────────────────
    property bool terminalVisible: false
    property var terminalLines: []

    Connections {
        target: bridge.dxCluster
        function onStatusUpdate(msg) {
            var lines = root.terminalLines.slice()
            lines.push(msg)
            if (lines.length > 200) lines = lines.slice(lines.length - 200)
            root.terminalLines = lines
        }
    }

    Rectangle {
        visible: root.terminalVisible
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: terminalInput.top
        anchors.top: filterRow.bottom
        anchors.margins: 4
        color: Qt.rgba(0, 0, 0, 0.85)
        border.color: secondaryCyan; border.width: 1; radius: 4

        Flickable {
            id: termFlick
            anchors.fill: parent; anchors.margins: 4
            contentHeight: termText.implicitHeight
            clip: true
            flickableDirection: Flickable.VerticalFlick

            Text {
                id: termText
                width: parent.width
                text: root.terminalLines.join("\n")
                color: "#00ff88"; font.family: "Consolas"; font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
            }

            onContentHeightChanged: {
                if (contentHeight > height)
                    contentY = contentHeight - height
            }
        }
    }

    // Campo input comandi telnet
    Rectangle {
        id: terminalInput
        visible: root.terminalVisible
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom; anchors.margins: 4
        height: 30; radius: 4
        color: Qt.rgba(0, 0, 0, 0.7); border.color: secondaryCyan; border.width: 1

        RowLayout {
            anchors.fill: parent; anchors.margins: 2; spacing: 4

            Text { text: ">"; color: secondaryCyan; font.family: "Consolas"; font.pixelSize: 13; font.bold: true; Layout.leftMargin: 4 }

            TextInput {
                id: cmdInput
                Layout.fillWidth: true; Layout.fillHeight: true
                color: "#00ff88"; font.family: "Consolas"; font.pixelSize: 12
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                onAccepted: {
                    if (text.trim().length > 0 && bridge.dxCluster) {
                        bridge.dxCluster.sendCommand(text.trim())
                        var lines = root.terminalLines.slice()
                        lines.push("> " + text.trim())
                        root.terminalLines = lines
                        text = ""
                    }
                }
            }

            Rectangle {
                width: 50; height: 22; radius: 4
                color: termBtnMA.containsMouse ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) : "transparent"
                border.color: secondaryCyan; border.width: 1
                Text { anchors.centerIn: parent; text: "Invia"; color: secondaryCyan; font.pixelSize: 10 }
                MouseArea { id: termBtnMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: cmdInput.accepted()
                }
            }
        }
    }

    // Pulsante toggle terminale nell'header
    Rectangle {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 40; anchors.topMargin: 6
        width: 24; height: 24; radius: 4
        color: root.terminalVisible ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) : Qt.rgba(1,1,1,0.05)
        border.color: root.terminalVisible ? secondaryCyan : glassBorder
        Text { anchors.centerIn: parent; text: ">_"; color: root.terminalVisible ? secondaryCyan : textSecondary; font.pixelSize: 10; font.bold: true }
        MouseArea {
            id: termToggleMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: root.terminalVisible = !root.terminalVisible
        }
        ToolTip.visible: termToggleMA.containsMouse
        ToolTip.text: "Terminal Telnet"
    }

    // ── Handle di ridimensionamento (angolo basso-destro) ────────────────────
    MouseArea {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        width: 18; height: 18
        cursorShape: Qt.SizeFDiagCursor
        property point startPos
        property real startW; property real startH
        onPressed: function(mouse) { startPos = Qt.point(mouse.x, mouse.y); startW = root.width; startH = root.height }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            var dx = mouse.x - startPos.x; var dy = mouse.y - startPos.y
            root.width  = Math.max(400, startW + dx)
            root.height = Math.max(250, startH + dy)
        }
        Rectangle {
            anchors.fill: parent; color: "transparent"
            Text { anchors.centerIn: parent; text: "⤡"; color: textSecondary; font.pixelSize: 12; opacity: 0.5 }
        }
    }
}
