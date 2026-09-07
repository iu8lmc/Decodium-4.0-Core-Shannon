// DecoRTTY — decoder controls.
//
// The presets cover what is actually on the air; the individual controls are
// there for the rest. Everything takes effect immediately, because tuning a
// marginal signal is an interactive business.
//
// La colonna e' stretta e corta, e questi comandi non ci stanno tutti: scorrono.
// Ma uno scorrimento che non si vede non esiste — chi guarda il pannello legge
// un elenco tagliato a meta' e conclude che il programma e' rotto. Per questo la
// barra sta sempre in vista invece di comparire al passaggio del mouse, e i
// comandi sono stretti quanto basta perche' ne resti fuori il meno possibile.
import QtQuick
import QtQuick.Controls

import "."

GlassPanel {
    id: root

    ScrollView {
        id: scroller
        anchors.fill: parent
        anchors.margins: 8
        anchors.rightMargin: 4        // la barra occupa il resto
        clip: true
        contentWidth: availableWidth

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
            width: 7
            contentItem: Rectangle {
                implicitWidth: 7
                radius: 3.5
                color: parent.pressed ? Theme.accent
                                      : Qt.rgba(Theme.primary.r, Theme.primary.g,
                                                Theme.primary.b, 0.65)
            }
            background: Rectangle {
                implicitWidth: 7
                radius: 3.5
                color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.6)
            }
        }

        Column {
            id: controls
            width: root.width - 27
            spacing: 7

            // ── standard presets ────────────────────────────────────────────
            Grid {
                width: parent.width
                columns: 2
                spacing: 4

                Repeater {
                    model: [{ label: "45.45 / 170", baud: 45.45, shift: 170 },
                            { label: "50 / 170",    baud: 50.0,  shift: 170 },
                            { label: "75 / 170",    baud: 75.0,  shift: 170 },
                            { label: "45.45 / 850", baud: 45.45, shift: 850 }]
                    delegate: GlassButton {
                        required property var modelData
                        text: modelData.label
                        minimumWidth: (controls.width - 4) / 2
                        implicitHeight: 24
                        font.pixelSize: 10
                        armed: Math.abs(rtty.baud - modelData.baud) < 0.1
                               && Math.abs(rtty.shiftHz - modelData.shift) < 1
                        onClicked: { rtty.baud = modelData.baud; rtty.shiftHz = modelData.shift }
                    }
                }
            }

            LabelledSlider {
                width: parent.width
                label: "Mark"
                suffix: " Hz"
                from: 500; to: 3000; stepSize: 1
                value: rtty.markHz
                onMoved: (v) => rtty.markHz = v
            }

            LabelledSlider {
                width: parent.width
                label: "Shift"
                suffix: " Hz"
                from: 20; to: 1000; stepSize: 1
                value: rtty.shiftHz
                onMoved: (v) => rtty.shiftHz = v
            }

            LabelledSlider {
                width: parent.width
                label: "Squelch"
                suffix: " dB"
                from: 0; to: 20; stepSize: 0.5
                decimals: 1
                value: rtty.squelchDb
                onMoved: (v) => rtty.squelchDb = v
            }

            // ── error correction depth ──────────────────────────────────────
            Column {
                width: parent.width
                spacing: 3

                Item {
                    width: parent.width
                    height: 15

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, parent.width - depth.width - 8)
                        elide: Text.ElideRight
                        text: qsTr("Correction")
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                    Text {
                        id: depth
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: rtty.correctionDepth === 0
                              ? "off"
                              : rtty.correctionDepth + " ch · " +
                                (rtty.correctionDepth / rtty.baud * 7.5).toFixed(1) + " s"
                        color: Theme.textPrimary
                        font.pixelSize: 11
                        font.family: Theme.monoFamily
                    }
                }

                Row {
                    spacing: 4

                    GlassButton {
                        text: qsTr("Off")
                        minimumWidth: (controls.width - 8) / 3
                        implicitHeight: 23
                        font.pixelSize: 10
                        armed: rtty.correctionDepth === 0
                        onClicked: rtty.correctionDepth = 0
                    }
                    GlassButton {
                        text: qsTr("Normal")
                        minimumWidth: (controls.width - 8) / 3
                        implicitHeight: 23
                        font.pixelSize: 10
                        armed: rtty.correctionDepth > 0 && rtty.correctionDepth <= 6
                        onClicked: rtty.correctionDepth = 4
                    }
                    GlassButton {
                        text: qsTr("Deep")
                        minimumWidth: (controls.width - 8) / 3
                        implicitHeight: 23
                        font.pixelSize: 10
                        armed: rtty.correctionDepth > 6
                        onClicked: rtty.correctionDepth = 10
                    }
                }
            }

            // I quattro comandi dell'aggancio al segnale. Stavano nel pannello
            // waterfall di DecoRTTY, che qui e' stato sostituito da quello di
            // Decodium: sono rimasti fuori nel passaggio, ed erano proprio
            // quelli che si toccano di continuo mentre si cerca di copiare.
            Row {
                width: parent.width
                spacing: 4

                GlassButton {
                    text: "REV"
                    minimumWidth: (controls.width - 12) / 4
                    implicitHeight: 24
                    font.pixelSize: 10
                    armed: rtty.reverse
                    accentColor: Theme.warning
                    onClicked: rtty.reverse = !rtty.reverse
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Swaps the two tones. When the other station is on the
opposite sideband everything arrives as gibberish, and
this is the one button that fixes it.")
                }

                GlassButton {
                    text: "AFC"
                    minimumWidth: (controls.width - 12) / 4
                    implicitHeight: 24
                    font.pixelSize: 10
                    armed: rtty.afcEnabled
                    accentColor: Theme.secondary
                    onClicked: rtty.afcEnabled = !rtty.afcEnabled
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Follows the drift of the signal already locked,
within a few tens of hertz.")
                }

                GlassButton {
                    text: "AUTO"
                    minimumWidth: (controls.width - 12) / 4
                    implicitHeight: 24
                    font.pixelSize: 10
                    armed: rtty.autoTuneEnabled
                    accentColor: Theme.success
                    onClicked: rtty.autoTuneEnabled = !rtty.autoTuneEnabled
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Searches by itself whenever nothing is being
copied, and tries the polarity too. While the
decoder is locked it touches nothing.")
                }

                GlassButton {
                    text: qsTr("CENTRE")
                    minimumWidth: (controls.width - 12) / 4
                    implicitHeight: 24
                    font.pixelSize: 10
                    accentColor: Theme.primary
                    onClicked: rtty.centreOnSignal()
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Finds the tone pair in the band and moves the
tuning onto it. Once, now.")
                }
            }

            Row {
                width: parent.width
                spacing: 4

                GlassButton {
                    text: "USOS"
                    minimumWidth: (controls.width - 4) / 2
                    implicitHeight: 24
                    font.pixelSize: 10
                    armed: rtty.unshiftOnSpace
                    accentColor: Theme.secondary
                    onClicked: rtty.unshiftOnSpace = !rtty.unshiftOnSpace
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Unshift on space — recovers a lost FIGS/LTRS, but\nbreaks long figure groups such as serial numbers.")
                }

                GlassButton {
                    text: qsTr("Set radio")
                    minimumWidth: (controls.width - 4) / 2
                    implicitHeight: 24
                    font.pixelSize: 10
                    enabled: radio.connected
                    accentColor: Theme.success
                    onClicked: radio.applyRttyProfile(rtty.markHz, rtty.shiftHz)
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Puts the radio into DIGU and narrows its filter\naround the tones.")
                }
            }
        }
    }
}
