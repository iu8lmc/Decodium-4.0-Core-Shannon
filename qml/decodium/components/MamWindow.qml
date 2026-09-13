/* Decodium - Multi-Answer Mode Window
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: mamWindow
    title: qsTr("Multi-Answer Mode")
    width: nativeHostWindow && parent ? Math.max(500, parent.width) : 700
    height: nativeHostWindow && parent ? Math.max(360, parent.height) : 450
    modal: false
    // 1.0.364+ — niente CloseOnPressOutside: la finestra MAM non si chiude piu' per
    // un click accidentale fuori. Resta chiudibile con Esc o con la X (in tema)
    // nell'header. Rimosso standardButtons (footer di stile default fuori-tema).
    closePolicy: Popup.CloseOnEscape
    property var engine: null
    property var nativeHostWindow: null
    property bool positionInitialized: false

    function clampToParent() {
        if (nativeHostWindow) return
        if (!parent) return
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        if (nativeHostWindow) {
            x = 0
            y = 0
            positionInitialized = true
            return
        }
        x = Math.max(0, Math.round((parent.width - width) / 2))
        y = Math.max(0, Math.round((parent.height - height) / 2))
        positionInitialized = true
    }

    function startNativeHostMove() {
        if (!nativeHostWindow || typeof nativeHostWindow.startSystemMove !== "function")
            return false
        try {
            return nativeHostWindow.startSystemMove()
        } catch (error) {
            console.log("MAM startSystemMove failed: " + error)
        }
        return false
    }

    function finishNativeHostMove() {
        if (nativeHostWindow && typeof nativeHostWindow.finishDesktopMove === "function")
            nativeHostWindow.finishDesktopMove()
    }

    function requestWindowClose() {
        if (nativeHostWindow && typeof nativeHostWindow.hideHostedWindow === "function") {
            nativeHostWindow.hideHostedWindow()
            return
        }
        mamWindow.close()
    }

    onAboutToShow: ensureInitialPosition()

    // Colors
    property color bgDeep: engine ? engine.themeManager.bgDeep : "#0b1220"
    property color bgPanel: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
    property color accentGreen: engine ? engine.themeManager.accentColor : "#2ecc71"
    property color secondaryCyan: engine ? engine.themeManager.secondaryColor : "#00d8ff"
    property color warningOrange: engine ? engine.themeManager.warningColor : "#ff9800"
    property color errorRed: engine ? engine.themeManager.errorColor : "#f44336"
    property color textPrimary: engine ? engine.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: engine ? engine.themeManager.textSecondary : "#9db1c9"
    property color glassBorder: engine ? engine.themeManager.glassBorder : "#2a3950"
    readonly property bool mamQueueActive: engine && (engine.multiAnswerMode || engine.autoCqRepeat)
    readonly property var mamQueueEntries: mamWindow.mamQueueActive ? engine.callerQueue : []
    readonly property int mamQueueCount: mamWindow.mamQueueActive ? engine.callerQueueSize : 0
    readonly property string mamActiveCall: inferMamActiveCall()
    readonly property bool mamHasActiveCaller: engine
                                               && mamWindow.mamActiveCall.length > 0
                                               && (mamWindow.isDirectedTx(engine.currentTxMessage)
                                                   || (engine.currentTx >= 1 && engine.currentTx <= 5)
                                                   || (engine.qsoProgress >= 1 && engine.qsoProgress <= 5)
                                                   || (engine.txEnabled && !mamWindow.isCqMessage(engine.currentTxMessage)))
    readonly property int mamNowCount: mamWindow.mamHasActiveCaller ? 1 : 0

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

    // 1.0.364+ - etichetta leggibile dello stato di uno stream MAM dal suo
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
        return text.length > 0 && !mamWindow.isCqMessage(text)
    }

    function inferMamActiveCall() {
        if (!engine)
            return ""

        var explicitCall = mamWindow.normalizeMamToken(engine.dxCall)
        if (explicitCall.length > 0)
            return explicitCall

        var message = String(engine.currentTxMessage || "").trim()
        if (!mamWindow.isDirectedTx(message))
            return ""

        var myCall = mamWindow.normalizeMamToken(engine.callsign)
        var parts = message.split(/\s+/)
        for (var i = 0; i < parts.length; ++i) {
            var candidate = mamWindow.normalizeMamToken(parts[i])
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
               ? Qt.rgba(mamWindow.secondaryCyan.r, mamWindow.secondaryCyan.g, mamWindow.secondaryCyan.b, 0.24)
               : Qt.rgba(mamWindow.secondaryCyan.r, mamWindow.secondaryCyan.g, mamWindow.secondaryCyan.b, 0.08)
        border.width: 1
        border.color: active
                      ? Qt.rgba(mamWindow.secondaryCyan.r, mamWindow.secondaryCyan.g, mamWindow.secondaryCyan.b, 0.55)
                      : Qt.rgba(mamWindow.textSecondary.r, mamWindow.textSecondary.g, mamWindow.textSecondary.b, 0.25)

        Text {
            anchors.centerIn: parent
            text: queueMoveButton.symbol
            font.pixelSize: 11
            font.bold: true
            color: queueMoveButton.active ? mamWindow.textPrimary : mamWindow.textSecondary
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

    background: Rectangle {
        color: bgPanel
        border.color: glassBorder
        border.width: 1
        radius: 8
    }

    header: Rectangle {
        height: 50
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
        radius: 8

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            property point pressGlobalPos: Qt.point(0, 0)
            property point pressWindowPos: Qt.point(0, 0)
            property bool nativeMoveActive: false
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                mamWindow.positionInitialized = true
                if (mamWindow.nativeHostWindow) {
                    pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    pressWindowPos = Qt.point(mamWindow.nativeHostWindow.x,
                                              mamWindow.nativeHostWindow.y)
                    nativeMoveActive = mamWindow.startNativeHostMove()
                }
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                if (mamWindow.nativeHostWindow) {
                    if (nativeMoveActive)
                        return
                    var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    mamWindow.nativeHostWindow.x = Math.round(
                                pressWindowPos.x + currentGlobalPos.x - pressGlobalPos.x)
                    mamWindow.nativeHostWindow.y = Math.round(
                                pressWindowPos.y + currentGlobalPos.y - pressGlobalPos.y)
                    return
                }
                mamWindow.x += mouse.x - clickPos.x
                mamWindow.y += mouse.y - clickPos.y
                mamWindow.clampToParent()
            }
            onReleased: {
                nativeMoveActive = false
                mamWindow.finishNativeHostMove()
            }
            onCanceled: {
                nativeMoveActive = false
                mamWindow.finishNativeHostMove()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "Multi-Answer Mode - " + (mamWindow.mamQueueActive ? ("Queue: " + mamWindow.mamQueueCount) : "Manual TX") + " | Now: " + mamWindow.mamNowCount
            font.pixelSize: 16
            font.bold: true
            color: warningOrange
        }

        // 1.0.364+ — X di chiusura in tema (sostituisce il footer standardButtons).
        // Sta sopra il MouseArea di drag (ultimo figlio dichiarato) e ha il suo
        // MouseArea che intercetta il click.
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 28
            height: 28
            radius: 6
            color: closeMamMA.containsMouse ? Qt.rgba(0.95, 0.26, 0.21, 0.30)
                                            : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
            border.color: closeMamMA.containsMouse ? errorRed : glassBorder
            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeMamMA.containsMouse ? errorRed : textPrimary
                font.pixelSize: 14
            }
            MouseArea {
                id: closeMamMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: mamWindow.requestWindowClose()
            }
        }
    }

    contentItem: Rectangle {
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            // Queue
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                radius: 6
                border.color: secondaryCyan

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    Text {
                        text: "QUEUE"
                        font.pixelSize: 14
                        font.bold: true
                        color: secondaryCyan
                    }

                    Text {
                        text: mamWindow.mamQueueActive ? ("Count: " + mamWindow.mamQueueCount) : "Manual TX"
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    ListView {
                        id: queueList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: mamWindow.mamQueueEntries
                        clip: true
                        spacing: 10
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: contentHeight > height

                        delegate: Rectangle {
                            width: queueList.width - (queueScroll.visible ? queueScroll.width + 6 : 0)
                            height: 42
                            radius: 4
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.08)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 7
                                spacing: 6

                                Text {
                                    text: (index + 1) + "."
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: secondaryCyan
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: mamWindow.queueCall(modelData)
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: accentGreen
                                    elide: Text.ElideRight
                                }

                                Text {
                                    visible: mamWindow.queueFreq(modelData).length > 0
                                    text: mamWindow.queueFreq(modelData) + " Hz"
                                    font.pixelSize: 11
                                    color: textSecondary
                                    Layout.preferredWidth: 58
                                    horizontalAlignment: Text.AlignRight
                                }

                                Text {
                                    visible: mamWindow.queueSnr(modelData).length > 0
                                    text: mamWindow.queueSnr(modelData) + " dB"
                                    font.pixelSize: 11
                                    color: textSecondary
                                    Layout.preferredWidth: 42
                                    horizontalAlignment: Text.AlignRight
                                }

                                QueueMoveButton {
                                    symbol: "⇧"
                                    tip: "Move to top"
                                    active: index > 0
                                    onTriggered: if (mamWindow.engine) mamWindow.engine.moveCallerQueueItem(index, 0)
                                }

                                QueueMoveButton {
                                    symbol: "↑"
                                    tip: "Move up"
                                    active: index > 0
                                    onTriggered: if (mamWindow.engine) mamWindow.engine.moveCallerQueueItem(index, index - 1)
                                }

                                QueueMoveButton {
                                    symbol: "↓"
                                    tip: "Move down"
                                    active: index < queueList.count - 1
                                    onTriggered: if (mamWindow.engine) mamWindow.engine.moveCallerQueueItem(index, index + 1)
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            id: queueScroll
                            policy: ScrollBar.AsNeeded
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: mamWindow.mamQueueCount === 0
                            text: mamWindow.mamQueueActive ? "No queued callers" : "Queue inactive"
                            font.pixelSize: 12
                            color: textSecondary
                        }
                    }
                }
            }

            // Now
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.3)
                radius: 6
                border.color: warningOrange

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    Text {
                        text: "NOW"
                        font.pixelSize: 14
                        font.bold: true
                        color: warningOrange
                    }

                    Text {
                        text: "Count: " + mamWindow.mamNowCount
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        visible: mamWindow.mamHasActiveCaller

                        Text {
                            text: mamWindow.mamActiveCall
                            font.pixelSize: 14
                            font.bold: true
                            color: warningOrange
                        }

                        Text {
                            text: mamWindow.engine ? ("Stage: " + mamWindow.qsoProgressName(mamWindow.engine.qsoProgress)) : "Stage: IDLE"
                            font.pixelSize: 11
                            color: textSecondary
                        }

                        Text {
                            text: mamWindow.engine ? ("TX" + mamWindow.engine.currentTx + " — " + mamWindow.engine.currentTxMessage) : ""
                            font.pixelSize: 11
                            color: textPrimary
                            wrapMode: Text.WrapAnywhere
                        }

                        Text {
                            text: mamWindow.engine ? ("Sent: " + (mamWindow.engine.reportSent || "--") + " | Rcvd: " + (mamWindow.engine.reportReceived || "--")) : ""
                            font.pixelSize: 11
                            color: textSecondary
                        }

                        Text {
                            text: mamWindow.engine ? ((mamWindow.engine.transmitting ? "Transmitting" : "Waiting") + " | TX " + (mamWindow.engine.txEnabled ? "enabled" : "disabled")) : ""
                            font.pixelSize: 11
                            color: mamWindow.engine && mamWindow.engine.transmitting ? warningOrange : accentGreen
                        }
                    }

                    Text {
                        visible: !mamWindow.mamHasActiveCaller && !(mamWindow.engine && mamWindow.engine.mamMultiStream)
                        text: qsTr("No active caller")
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    // 1.0.364+ - MAM multi-stream (MSHV): QSO paralleli attivi.
                    // Visibile solo col toggle multi-stream ON; la vista coda/NOW
                    // seriale resta intatta per il MAM seriale.
                    Column {
                        width: parent.width
                        spacing: 6
                        visible: mamWindow.engine ? mamWindow.engine.mamMultiStream : false

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: mamWindow.glassBorder
                            visible: mamWindow.mamHasActiveCaller
                        }

                        Text {
                            text: qsTr("QSO attivi (multi-stream): ") + (mamWindow.engine ? mamWindow.engine.mamActiveSlotCount : 0)
                            font.pixelSize: 12
                            font.bold: true
                            color: secondaryCyan
                        }

                        Repeater {
                            model: mamWindow.engine ? mamWindow.engine.mamActiveSlots : []

                            Rectangle {
                                width: parent ? parent.width : 0
                                height: msRow.implicitHeight + 8
                                radius: 4
                                color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.08)

                                RowLayout {
                                    id: msRow
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    Text {
                                        text: modelData.call
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: accentGreen
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: modelData.freq + " Hz"
                                        font.pixelSize: 11
                                        color: textSecondary
                                        Layout.preferredWidth: 58
                                        horizontalAlignment: Text.AlignRight
                                    }

                                    Text {
                                        text: mamWindow.mamStreamStage(modelData.progress, modelData.tx)
                                        font.pixelSize: 11
                                        color: warningOrange
                                        Layout.preferredWidth: 82
                                        horizontalAlignment: Text.AlignRight
                                    }

                                    Text {
                                        text: (modelData.snr === 127 ? "--" : modelData.snr) + " dB"
                                        font.pixelSize: 11
                                        color: textSecondary
                                        Layout.preferredWidth: 46
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }

                        Text {
                            visible: mamWindow.engine ? mamWindow.engine.mamActiveSlotCount === 0 : true
                            text: qsTr("No active stream")
                            font.pixelSize: 11
                            color: textSecondary
                        }
                    }
                }
            }
        }
    }
}
