/* Decodium - Multi-Answer Mode Window
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: mamWindow
    title: "Multi-Answer Mode"
    width: 700
    height: 450
    modal: false
    standardButtons: Dialog.Close
    property var engine: null
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
    readonly property var mamQueueEntries: engine ? engine.callerQueue : []
    readonly property int mamQueueCount: engine ? engine.callerQueueSize : 0
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
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                mamWindow.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                mamWindow.x += mouse.x - clickPos.x
                mamWindow.y += mouse.y - clickPos.y
                mamWindow.clampToParent()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "Multi-Answer Mode - Queue: " + mamWindow.mamQueueCount + " | Now: " + mamWindow.mamNowCount
            font.pixelSize: 16
            font.bold: true
            color: warningOrange
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
                        text: "Count: " + mamWindow.mamQueueCount
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
                            height: 38
                            radius: 4
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.08)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8

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
                                }

                                Text {
                                    visible: mamWindow.queueSnr(modelData).length > 0
                                    text: mamWindow.queueSnr(modelData) + " dB"
                                    font.pixelSize: 11
                                    color: textSecondary
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
                            text: "No queued callers"
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
                        visible: !mamWindow.mamHasActiveCaller
                        text: "No active caller"
                        font.pixelSize: 12
                        color: textSecondary
                    }
                }
            }
        }
    }
}
