/* LogQsoPanel — DX-Pedition Mode "Log · QSO Entry" compact form (design §3.8)
 * Phase 2b (1.0.331): compact, read-mostly QSO entry card. Mirrors the data
 * the native log prompt uses (bridge.pendingLogQsoPreview()) and offers a
 * single "LOG QSO" action (bridge.logQso()). Refreshed whenever the DX call /
 * grid / TX message change so it tracks the live QSO without polling.
 *
 * We intentionally do NOT embed LogWindowContent here: that is the full
 * logbook browser (qsoList/stats/profiles/ADIF) — heavier than this panel and
 * wrong semantics for the tactical layout. The compact card uses only
 * bridge-reachable invokables/properties, so it is safe in an external
 * component (no Main.qml local ids).
 *
 * Lesson Fase 2a: never shadow the global `bridge` (default-bound below).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)

    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:   tm ? tm.accentColor    : "#19ff88"
    readonly property color cBorder:   tm ? tm.borderColor    : "#1f2a22"
    readonly property color cText:     tm ? tm.textPrimary    : "#d6dcd8"
    readonly property color cTextDim:  tm ? tm.textSecondary  : "#6c7872"
    readonly property color cCyan:     tm ? tm.secondaryColor : "#66e6ff"
    readonly property color cGreen:    tm ? tm.successColor    : "#3ad17a"
    readonly property color cPanelHdr: tm ? tm.panelHeader     : "#0a0e0c"

    // Snapshot of the pending QSO (call/grid/sent/rcvd/freq/mode/comment).
    property var qso: ({})
    function refresh() {
        if (root.bridge && typeof root.bridge.pendingLogQsoPreview === "function")
            root.qso = root.bridge.pendingLogQsoPreview() || ({})
        else
            root.qso = ({})
    }
    Component.onCompleted: refresh()

    // Track live QSO state cheaply (no timer): refresh on the signals that
    // actually change the loggable content.
    Connections {
        target: root.bridge
        ignoreUnknownSignals: true
        function onDxCallChanged() { root.refresh() }
        function onDxGridChanged() { root.refresh() }
        function onCurrentTxMessageChanged() { root.refresh() }
        function onTransmittingChanged() { root.refresh() }
    }

    readonly property string qCall: qso && qso.call ? String(qso.call).trim() : ""
    readonly property string qGrid: qso && qso.grid ? String(qso.grid).trim() : ""
    readonly property string qSent: qso && qso.sent ? String(qso.sent).trim() : ""
    readonly property string qRcvd: qso && qso.rcvd ? String(qso.rcvd).trim() : ""
    readonly property bool   canLog: qCall.length > 0

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 8

        // Big DX call.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Text {
                text: "CALL"
                color: root.cTextDim
                font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.6
            }
            Text {
                text: root.qCall.length > 0 ? root.qCall : "—"
                color: root.cAccent
                font.pixelSize: 26; font.bold: true; font.family: decodiumMonoFontFamily
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // GRID / RST sent / RST rcvd mini-fields.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [
                    { k: "GRID", v: root.qGrid },
                    { k: "RST S", v: root.qSent },
                    { k: "RST R", v: root.qRcvd }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 6
                    color: root.cPanelHdr
                    border.color: root.cBorder
                    border.width: 1
                    ColumnLayout {
                        anchors { fill: parent; margins: 6 }
                        spacing: 0
                        Text {
                            text: !modelData ? "" : modelData.k
                            color: root.cTextDim
                            font.pixelSize: 8; font.bold: true; font.letterSpacing: 1.2
                        }
                        Text {
                            text: !modelData ? "—" : (modelData.v && modelData.v.length > 0 ? modelData.v : "—")
                            color: root.cText
                            font.pixelSize: 14; font.bold: true; font.family: decodiumMonoFontFamily
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // NEXT TX line.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: 6
            color: root.cPanelHdr
            border.color: root.cBorder
            border.width: 1
            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                spacing: 8
                Text {
                    text: qsTr("NEXT TX")
                    color: root.cTextDim
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.4
                }
                Text {
                    text: {
                        var m = (root.bridge && root.bridge.currentTxMessage)
                                ? String(root.bridge.currentTxMessage).trim() : ""
                        return m.length > 0 ? m : "—"
                    }
                    color: root.cCyan
                    font.pixelSize: 13; font.bold: true; font.family: decodiumMonoFontFamily
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }

        // LOG QSO button (green) -> native bridge.logQso().
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            radius: 8
            enabled: root.canLog
            opacity: root.canLog ? 1.0 : 0.45
            color: logMA.pressed ? Qt.darker(root.cGreen, 1.2)
                   : logMA.containsMouse ? Qt.lighter(root.cGreen, 1.1)
                   : root.cGreen
            Text {
                anchors.centerIn: parent
                text: root.canLog ? "LOG QSO" : "NO QSO TO LOG"
                color: "#04140b"
                font.pixelSize: 13; font.bold: true; font.letterSpacing: 1.4
            }
            MouseArea {
                id: logMA
                anchors.fill: parent
                hoverEnabled: true
                enabled: root.canLog
                cursorShape: root.canLog ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (!root.canLog || !root.bridge) return
                    if (typeof root.bridge.logQso === "function")
                        root.bridge.logQso()
                    root.refresh()
                }
            }
        }
    }
}
