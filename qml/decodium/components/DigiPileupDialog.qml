/* DigiPileup
 * One switch and three modes to work a pile-up of callers: streams kept the
 * right distance apart, an elastic transmit cycle, and a power budget.
 * Opened from the PILEUP button of the TX panel (right click) or its menu.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dlg
    width: 640
    height: 700
    modal: false
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

    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color warningOrange: bridge.themeManager.warningColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder
    readonly property color dialogSurface: bridge.themeManager.isGlassTheme
                                          && bridge.themeManager.glassTransparencySupported
                                          ? Qt.rgba(bridge.themeManager.panelColor.r,
                                                    bridge.themeManager.panelColor.g,
                                                    bridge.themeManager.panelColor.b, 0.58)
                                          : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)

    property var status: ({})
    function refreshStatus() {
        status = bridge.digiPileupStatus()
    }
    Timer {
        interval: 1000
        repeat: true
        running: dlg.visible
        triggeredOnStart: true
        onTriggered: dlg.refreshStatus()
    }

    background: Rectangle {
        color: dlg.dialogSurface
        border.color: dlg.secondaryCyan
        border.width: 2
        radius: 16
        layer.enabled: false
    }

    header: Rectangle {
        width: parent.width
        height: 50
        color: "transparent"

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                dlg.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                dlg.x += mouse.x - clickPos.x
                dlg.y += mouse.y - clickPos.y
                dlg.clampToParent()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16

            Text {
                text: "DigiPileup"
                font.pixelSize: 18
                font.bold: true
                color: dlg.secondaryCyan
            }
            Text {
                text: qsTr("experimental")
                font.pixelSize: 11
                color: dlg.warningOrange
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 14
                color: closeMA.containsMouse ? Qt.rgba(dlg.textPrimary.r, dlg.textPrimary.g, dlg.textPrimary.b, 0.2) : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "X"
                    font.pixelSize: 14
                    font.bold: true
                    color: dlg.textPrimary
                }
                MouseArea {
                    id: closeMA
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: dlg.close()
                }
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: dlg.glassBorder
        }
    }

    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14

            // ── Master switch ─────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                implicitHeight: masterCol.implicitHeight + 24
                radius: 10
                color: Qt.rgba(dlg.secondaryCyan.r, dlg.secondaryCyan.g, dlg.secondaryCyan.b, 0.08)
                border.color: bridge.digiPileupEnabled ? dlg.accentGreen : dlg.glassBorder
                border.width: bridge.digiPileupEnabled ? 2 : 1

                ColumnLayout {
                    id: masterCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Switch {
                            id: masterSwitch
                            checked: bridge.digiPileupEnabled
                            onToggled: bridge.digiPileupEnabled = checked
                        }
                        Text {
                            text: qsTr("Enable everything")
                            font.pixelSize: 15
                            font.bold: true
                            color: dlg.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: dlg.textSecondary
                        text: qsTr("Turns on multi-stream, the elastic cycle and the power budget together. It does not key the transmitter: enable TX and AutoCQ yourself (the DX-Pedition panel has an ARM button for that).")
                    }
                }
            }

            // ── Modes ─────────────────────────────────────────────────
            Text {
                text: qsTr("Modes")
                font.pixelSize: 14
                font.bold: true
                color: dlg.secondaryCyan
            }

            // Lanes
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Switch { checked: true; enabled: false }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: qsTr("Lanes (always on)"); font.pixelSize: 13; font.bold: true; color: dlg.textPrimary }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: dlg.textSecondary
                        text: qsTr("Simultaneous streams are kept the right distance apart for the mode, so they do not damage each other: FT2 170 Hz, FT4 85 Hz, FT8 50 Hz. A caller closer than that to a QSO in progress waits in the queue.")
                    }
                }
            }

            // Rhythm
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Switch {
                    checked: bridge.digiPileupElastic
                    onToggled: bridge.digiPileupElastic = checked
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: qsTr("Rhythm (elastic cycle, FT2)"); font.pixelSize: 13; font.bold: true; color: dlg.textPrimary }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: dlg.textSecondary
                        text: qsTr("Instead of transmitting every second slot (7.5 s), you transmit again as soon as every station you wrote to has answered, or is clearly not going to. The waiting time of each station is learned, like a network timeout. About 6 s per cycle is realistic. Needs FT2 and multi-stream.")
                    }
                }
            }

            // Budget
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Switch {
                    checked: bridge.digiPileupBudget
                    onToggled: bridge.digiPileupBudget = checked
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: qsTr("Power budget"); font.pixelSize: 13; font.bold: true; color: dlg.textPrimary }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: dlg.textSecondary
                        text: qsTr("The transmitter's peak power is shared between the streams: with N equal streams each one loses 20·log10(N) dB. Instead of a fixed number of equal streams, each station gets only the amplitude its own link needs, as many as fit; the others wait for the next frame. A weak station may get a whole frame to itself.")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text {
                    text: qsTr("Maximum simultaneous streams")
                    font.pixelSize: 12
                    color: dlg.textSecondary
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 1; to: 10
                    editable: false
                    enabled: bridge.mamMultiStream
                    value: bridge.mamMaxStreams
                    onValueModified: bridge.mamMaxStreams = value
                }
            }

            // ── Live status ───────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text {
                    text: qsTr("Max QSOs per call (0 = no limit)")
                    font.pixelSize: 12
                    color: dlg.textSecondary
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0; to: 3
                    editable: false
                    value: bridge.maxQsoPerCall
                    onValueModified: bridge.maxQsoPerCall = value
                }
            }

            Text {
                text: qsTr("Status")
                font.pixelSize: 14
                font.bold: true
                color: dlg.secondaryCyan
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: dlg.textPrimary
                text: {
                    var s = dlg.status
                    if (!s || s.mode === undefined) return ""
                    var cyc = s.lastCycleMs > 0 ? (s.lastCycleMs / 1000.0).toFixed(1) + " s" : "-"
                    return qsTr("Mode %1 · minimum distance between streams %2 Hz\nQSOs in progress %3 · callers waiting %4\nLast frame: %5 stream(s), %6 left for the next one · cycle %7\nElastic cycle running: %8")
                        .arg(s.mode).arg(s.gapHz).arg(s.activeSlots).arg(s.queued)
                        .arg(s.lastStreams).arg(s.lastDeferred).arg(cyc)
                        .arg(s.elasticRunning ? qsTr("yes") : qsTr("no"))
                }
            }

            // ── Info ──────────────────────────────────────────────────
            Text {
                text: qsTr("How to use it")
                font.pixelSize: 14
                font.bold: true
                color: dlg.secondaryCyan
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: dlg.textSecondary
                text: qsTr("<b>Station running the pile-up:</b> FT2 mode, CAT working as usual. Switch on \"Enable everything\", then enable TX and AutoCQ (or press ARM in the DX-Pedition panel). Do not raise the ALC: the sum of the streams is already normalised to the same peak as a single signal.<br><br><b>Stations calling:</b> nothing special. Use FT2 normally and double-click the CQ. There is no need to move above 1000 Hz: the answer comes on your own frequency. Do not enable the WSJT-X Fox/Hound special operation. If you call closer than 170 Hz to a QSO in progress you wait in the queue; moving to a free frequency gets you served sooner.<br><br><b>Limits:</b> experimental. Measured on simulations and on recorded signals, not yet on the air. The elastic cycle and the power budget work from the answers of the stations: a station that answers much later than usual may be missed in that cycle and will call again.")
            }

            Item { Layout.preferredHeight: 6 }
        }
    }
}
