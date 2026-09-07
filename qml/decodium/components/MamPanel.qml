/* Decodium - Multi-Answer Mode Panel (incassato)
 * 1.0.345 — versione Item del MamWindow per il tab Cluster/MAM della DX-Pedition.
 * Stesso contenuto (Queue + Now) ma senza Dialog/header/drag: si incassa in un pannello.
 * MamWindow.qml resta separato per la finestra flottante classica.
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: mamPanel

    // Usa il bridge/engine globale come gli altri pannelli del workspace.
    property var engine: (typeof appEngine !== 'undefined' ? appEngine : null)

    // Colors (fallback identici a MamWindow)
    property color bgDeep: engine ? engine.themeManager.bgDeep : "#0b1220"
    property color accentGreen: engine ? engine.themeManager.accentColor : "#2ecc71"
    property color secondaryCyan: engine ? engine.themeManager.secondaryColor : "#00d8ff"
    property color warningOrange: engine ? engine.themeManager.warningColor : "#ff9800"
    property color errorRed: engine ? engine.themeManager.errorColor : "#f44336"
    property color textPrimary: engine ? engine.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: engine ? engine.themeManager.textSecondary : "#9db1c9"
    property color glassBorder: engine ? engine.themeManager.glassBorder : "#2a3950"

    readonly property bool mamQueueActive: engine && (engine.multiAnswerMode || engine.autoCqRepeat)
    readonly property var mamQueueEntries: mamPanel.mamQueueActive ? engine.callerQueue : []
    readonly property int mamQueueCount: mamPanel.mamQueueActive ? engine.callerQueueSize : 0
    readonly property string mamActiveCall: inferMamActiveCall()
    readonly property bool mamHasActiveCaller: engine
                                               && mamPanel.mamActiveCall.length > 0
                                               && (mamPanel.isDirectedTx(engine.currentTxMessage)
                                                   || (engine.currentTx >= 1 && engine.currentTx <= 5)
                                                   || (engine.qsoProgress >= 1 && engine.qsoProgress <= 5)
                                                   || (engine.txEnabled && !mamPanel.isCqMessage(engine.currentTxMessage)))
    readonly property int mamNowCount: mamPanel.mamHasActiveCaller ? 1 : 0

    function qsoProgressName(progress) {
        switch (progress) {
        case 1: return "CALLING"
        case 2: return "REPLYING"
        case 3: return "REPORT"
        case 4: return "ROGER_REPORT"
        case 5: return "SIGNOFF"
        case 6: return "IDLE_QSO"
        default: return "IDLE"
        }
    }

    // 1.0.364+ — etichetta leggibile dello stato di uno stream MAM dal suo
    // progress/currentTx (per la lista multi-stream).
    function mamStreamStage(progress, tx) {
        switch (tx) {
        case 2: return "TX2 rpt"
        case 3: return "TX3 R+rpt"
        case 4: return "TX4 RR73"
        case 5: return "TX5 73"
        case 6: return "CQ"
        default: return qsoProgressName(progress)
        }
    }

    function queueCall(entry) {
        if (!entry)
            return ""
        var parts = String(entry).split(/\s+/)
        return parts.length > 0 ? parts[0] : ""
    }

    function queueFreq(entry) {
        if (!entry)
            return ""
        var parts = String(entry).split(/\s+/)
        return parts.length > 1 ? parts[1] : ""
    }

    function queueSnr(entry) {
        if (!entry)
            return ""
        var parts = String(entry).split(/\s+/)
        return parts.length > 2 ? parts[2] : ""
    }

    function normalizeMamToken(token) {
        var value = String(token || "").trim().toUpperCase()
        value = value.replace(/[<>]/g, "")
        if (!value.length)
            return ""
        if (value === "CQ" || value === "QRZ" || value === "DE"
                || value === "73" || value === "RR73" || value === "RRR")
            return ""
        return value
    }

    function isCqMessage(message) {
        var text = String(message || "").trim().toUpperCase()
        return text.indexOf("CQ ") === 0 || text.indexOf("QRZ ") === 0
    }

    function isDirectedTx(message) {
        var text = String(message || "").trim()
        return text.length > 0 && !mamPanel.isCqMessage(text)
    }

    function inferMamActiveCall() {
        if (!engine)
            return ""

        var explicitCall = mamPanel.normalizeMamToken(engine.dxCall)
        if (explicitCall.length > 0)
            return explicitCall

        var message = String(engine.currentTxMessage || "").trim()
        if (!mamPanel.isDirectedTx(message))
            return ""

        var myCall = mamPanel.normalizeMamToken(engine.callsign)
        var parts = message.split(/\s+/)
        for (var i = 0; i < parts.length; ++i) {
            var candidate = mamPanel.normalizeMamToken(parts[i])
            if (!candidate.length)
                continue
            if (myCall.length > 0 && candidate === myCall)
                continue
            return candidate
        }

        return ""
    }

    component QueueMoveButton: Rectangle {
        id: queueMoveButton
        property string symbol: ""
        property string tip: ""
        property bool active: true
        signal triggered()

        implicitWidth: 20
        implicitHeight: 22
        radius: 4
        opacity: active ? 1.0 : 0.28
        color: active && moveMouse.containsMouse
               ? Qt.rgba(mamPanel.secondaryCyan.r, mamPanel.secondaryCyan.g, mamPanel.secondaryCyan.b, 0.24)
               : Qt.rgba(mamPanel.secondaryCyan.r, mamPanel.secondaryCyan.g, mamPanel.secondaryCyan.b, 0.08)
        border.width: 1
        border.color: active
                      ? Qt.rgba(mamPanel.secondaryCyan.r, mamPanel.secondaryCyan.g, mamPanel.secondaryCyan.b, 0.55)
                      : Qt.rgba(mamPanel.textSecondary.r, mamPanel.textSecondary.g, mamPanel.textSecondary.b, 0.25)

        Text {
            anchors.centerIn: parent
            text: queueMoveButton.symbol
            font.pixelSize: 11
            font.bold: true
            color: queueMoveButton.active ? mamPanel.textPrimary : mamPanel.textSecondary
        }

        MouseArea {
            id: moveMouse
            anchors.fill: parent
            enabled: queueMoveButton.active
            hoverEnabled: true
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: queueMoveButton.triggered()
        }

        ToolTip.visible: moveMouse.containsMouse && queueMoveButton.active
        ToolTip.text: queueMoveButton.tip
        ToolTip.delay: 500
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Queue
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(mamPanel.bgDeep.r, mamPanel.bgDeep.g, mamPanel.bgDeep.b, 0.3)
            radius: 6
            border.color: mamPanel.secondaryCyan

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Text {
                    text: "QUEUE"
                    font.pixelSize: 13
                    font.bold: true
                    color: mamPanel.secondaryCyan
                }

                Text {
                    text: mamPanel.mamQueueActive ? ("Count: " + mamPanel.mamQueueCount) : "Manual TX"
                    font.pixelSize: 11
                    color: mamPanel.textSecondary
                }

                ListView {
                    id: queueList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: mamPanel.mamQueueEntries
                    clip: true
                    spacing: 8
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height

                    delegate: Rectangle {
                        width: queueList.width - (queueScroll.visible ? queueScroll.width + 6 : 0)
                        height: 40
                        radius: 4
                        color: Qt.rgba(mamPanel.secondaryCyan.r, mamPanel.secondaryCyan.g, mamPanel.secondaryCyan.b, 0.08)

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 5

                            Text {
                                text: (index + 1) + "."
                                font.pixelSize: 11
                                font.bold: true
                                color: mamPanel.secondaryCyan
                            }

                            Text {
                                Layout.fillWidth: true
                                text: mamPanel.queueCall(modelData)
                                font.pixelSize: 11
                                font.bold: true
                                color: mamPanel.accentGreen
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: mamPanel.queueFreq(modelData).length > 0
                                text: mamPanel.queueFreq(modelData) + " Hz"
                                font.pixelSize: 11
                                color: mamPanel.textSecondary
                                Layout.preferredWidth: 54
                                horizontalAlignment: Text.AlignRight
                            }

                            Text {
                                visible: mamPanel.queueSnr(modelData).length > 0
                                text: mamPanel.queueSnr(modelData) + " dB"
                                font.pixelSize: 11
                                color: mamPanel.textSecondary
                                Layout.preferredWidth: 40
                                horizontalAlignment: Text.AlignRight
                            }

                            QueueMoveButton {
                                symbol: "⇧"
                                tip: "Move to top"
                                active: index > 0
                                onTriggered: if (mamPanel.engine) mamPanel.engine.moveCallerQueueItem(index, 0)
                            }

                            QueueMoveButton {
                                symbol: "↑"
                                tip: "Move up"
                                active: index > 0
                                onTriggered: if (mamPanel.engine) mamPanel.engine.moveCallerQueueItem(index, index - 1)
                            }

                            QueueMoveButton {
                                symbol: "↓"
                                tip: "Move down"
                                active: index < queueList.count - 1
                                onTriggered: if (mamPanel.engine) mamPanel.engine.moveCallerQueueItem(index, index + 1)
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: queueScroll
                        policy: ScrollBar.AsNeeded
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: mamPanel.mamQueueCount === 0
                        text: mamPanel.mamQueueActive ? "No queued callers" : "Queue inactive"
                        font.pixelSize: 12
                        color: mamPanel.textSecondary
                    }
                }
            }
        }

        // Now
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(mamPanel.bgDeep.r, mamPanel.bgDeep.g, mamPanel.bgDeep.b, 0.3)
            radius: 6
            border.color: mamPanel.warningOrange

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Text {
                    text: "NOW"
                    font.pixelSize: 13
                    font.bold: true
                    color: mamPanel.warningOrange
                }

                Text {
                    text: "Count: " + mamPanel.mamNowCount
                    font.pixelSize: 11
                    color: mamPanel.textSecondary
                }

                Column {
                    width: parent.width
                    spacing: 5
                    visible: mamPanel.mamHasActiveCaller

                    Text {
                        text: mamPanel.mamActiveCall
                        font.pixelSize: 14
                        font.bold: true
                        color: mamPanel.warningOrange
                    }

                    Text {
                        text: mamPanel.engine ? ("Stage: " + mamPanel.qsoProgressName(mamPanel.engine.qsoProgress)) : "Stage: IDLE"
                        font.pixelSize: 11
                        color: mamPanel.textSecondary
                    }

                    Text {
                        text: mamPanel.engine ? ("TX" + mamPanel.engine.currentTx + " — " + mamPanel.engine.currentTxMessage) : ""
                        font.pixelSize: 11
                        color: mamPanel.textPrimary
                        wrapMode: Text.WrapAnywhere
                        width: parent.width
                    }

                    Text {
                        text: mamPanel.engine ? ("Sent: " + (mamPanel.engine.reportSent || "--") + " | Rcvd: " + (mamPanel.engine.reportReceived || "--")) : ""
                        font.pixelSize: 11
                        color: mamPanel.textSecondary
                    }

                    Text {
                        text: mamPanel.engine ? ((mamPanel.engine.transmitting ? "Transmitting" : "Waiting") + " | TX " + (mamPanel.engine.txEnabled ? "enabled" : "disabled")) : ""
                        font.pixelSize: 11
                        color: mamPanel.engine && mamPanel.engine.transmitting ? mamPanel.warningOrange : mamPanel.accentGreen
                    }
                }

                Text {
                    visible: !mamPanel.mamHasActiveCaller && !(mamPanel.engine && mamPanel.engine.mamMultiStream)
                    text: qsTr("No active caller")
                    font.pixelSize: 12
                    color: mamPanel.textSecondary
                }

                // 1.0.364+ — MAM multi-stream (MSHV): QSO paralleli attivi.
                // Visibile solo col toggle multi-stream ON; la vista coda/NOW
                // seriale qui sopra resta intatta per il MAM seriale.
                Column {
                    width: parent.width
                    spacing: 4
                    visible: mamPanel.engine ? mamPanel.engine.mamMultiStream : false

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: mamPanel.glassBorder
                        visible: mamPanel.mamHasActiveCaller
                    }

                    Text {
                        text: qsTr("QSO attivi (multi-stream): ") + (mamPanel.engine ? mamPanel.engine.mamActiveSlotCount : 0)
                        font.pixelSize: 11
                        font.bold: true
                        color: mamPanel.secondaryCyan
                    }

                    // 1.0.365+ — con MAM multi-stream ON il doppio-click su una
                    // stazione decodificata la aggiunge a questa lista (la chiamo
                    // io da TX1) invece di spegnere il MAM.
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("Double-click a station to add it to the list.")
                        font.pixelSize: 10
                        font.italic: true
                        opacity: 0.7
                        color: mamPanel.secondaryCyan
                    }

                    Repeater {
                        model: mamPanel.engine ? mamPanel.engine.mamActiveSlots : []

                        Rectangle {
                            width: parent ? parent.width : 0
                            height: msRow.implicitHeight + 8
                            radius: 4
                            color: Qt.rgba(mamPanel.secondaryCyan.r, mamPanel.secondaryCyan.g, mamPanel.secondaryCyan.b, 0.08)

                            RowLayout {
                                id: msRow
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 6

                                Text {
                                    text: modelData.call
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: mamPanel.accentGreen
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.freq + " Hz"
                                    font.pixelSize: 11
                                    color: mamPanel.textSecondary
                                    Layout.preferredWidth: 54
                                    horizontalAlignment: Text.AlignRight
                                }

                                Text {
                                    text: mamPanel.mamStreamStage(modelData.progress, modelData.tx)
                                    font.pixelSize: 11
                                    color: mamPanel.warningOrange
                                    Layout.preferredWidth: 78
                                    horizontalAlignment: Text.AlignRight
                                }

                                Text {
                                    text: (modelData.snr === 127 ? "--" : modelData.snr) + " dB"
                                    font.pixelSize: 11
                                    color: mamPanel.textSecondary
                                    Layout.preferredWidth: 44
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }

                    Text {
                        visible: mamPanel.engine ? mamPanel.engine.mamActiveSlotCount === 0 : true
                        text: qsTr("No active stream")
                        font.pixelSize: 11
                        color: mamPanel.textSecondary
                    }
                }
            }
        }
    }
}
