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
    width:         460
    height:        320

    // ── header ──────────────────────────────────────────────────────────────
    Rectangle {
        id: header
        width: parent.width; height: 34
        color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.12)
        radius: 8
        // bottom corners piatti
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: parent.color }

        MouseArea {
            anchors.fill: parent
            drag.target: root
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0
            drag.maximumX: root.parent ? root.parent.width - root.width : root.x
            drag.minimumY: 0
            drag.maximumY: root.parent ? root.parent.height - 50 : root.y
            hoverEnabled: true
            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: root.z = 250
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 6 }
            spacing: 6

            // stato connessione
            Rectangle {
                width: 8; height: 8; radius: 4
                color: bridge.dxCluster && bridge.dxCluster.connected ? "#00e676" : "#ef5350"
            }
            Text {
                text: "DX Cluster"
                color: secondaryCyan; font.bold: true; font.pixelSize: 12
            }
            Text {
                text: bridge.dxCluster && bridge.dxCluster.connected
                      ? "● " + bridge.dxCluster.host
                      : "disconnesso"
                color: textSecondary; font.pixelSize: 10
            }
            Item { Layout.fillWidth: true }

            // contatore spot
            Text {
                text: (bridge.dxCluster && bridge.dxCluster.spots ? bridge.dxCluster.spots.length : 0) + " spot"
                color: textSecondary; font.pixelSize: 10
            }

            // pulsante Connetti / Disconnetti
            Rectangle {
                width: 72; height: 22; radius: 4
                color: connMouse.containsMouse
                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                       : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
                border.color: secondaryCyan; border.width: 1
                MouseArea {
                    id: connMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (!bridge.dxCluster) return
                        if (bridge.dxCluster.connected)
                            bridge.disconnectDxCluster()
                        else
                            bridge.connectDxCluster()
                    }
                }
                Text {
                    anchors.centerIn: parent
                    text: bridge.dxCluster && bridge.dxCluster.connected ? "Disconnetti" : "Connetti"
                    color: secondaryCyan; font.pixelSize: 10
                }
            }

            // pulsante chiudi
            Rectangle {
                width: 22; height: 22; radius: 4
                color: closeMouse.containsMouse ? Qt.rgba(1, 0.2, 0.2, 0.4) : "transparent"
                MouseArea {
                    id: closeMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
                Text { anchors.centerIn: parent; text: "✕"; color: textSecondary; font.pixelSize: 11 }
            }
        }
    }

    // ── colonne header tabella ───────────────────────────────────────────────
    Rectangle {
        id: colHeader
        anchors.top: header.bottom
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
            var all = bridge.dxCluster ? bridge.dxCluster.spots : []
            if (root.modeFilter === "") return all
            return all.filter(function(s) { return s["mode"] === root.modeFilter })
        }

        // auto-scroll verso il basso quando arrivano nuovi spot
        onCountChanged: if (count > 0) positionViewAtEnd()

        delegate: Rectangle {
            width:  spotList.width
            height: 22
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
                        color: textPrimary; font.pixelSize: 9; font.bold: true
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
}
