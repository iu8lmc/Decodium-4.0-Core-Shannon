/* Decodium Qt6 - Band Selector Component
 * Displays band buttons for quick band switching
 * Integrates with BandManager for automatic frequency changes
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: bandSelector

    // Required property - reference to the app engine
    required property var engine

    // Convenience alias
    property var bandMgr: engine ? engine.bandManager : null

    // Style properties
    property color glassBg: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.85)
    property color glassBorder: bridge.themeManager.glassBorder
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary

    // Common HF bands for FT8/FT4 operation
    property var hfBands: [
        { index: 3,  lambda: "160M", name: "1.8 MHz" },
        { index: 4,  lambda: "80M",  name: "3.5 MHz" },
        { index: 5,  lambda: "60M",  name: "5 MHz" },
        { index: 6,  lambda: "40M",  name: "7 MHz" },
        { index: 7,  lambda: "30M",  name: "10 MHz" },
        { index: 8,  lambda: "20M",  name: "14 MHz" },
        { index: 9,  lambda: "17M",  name: "18 MHz" },
        { index: 10, lambda: "15M",  name: "21 MHz" },
        { index: 11, lambda: "12M",  name: "24 MHz" },
        { index: 12, lambda: "10M",  name: "28 MHz" },
        { index: 14, lambda: "6M",   name: "50 MHz" }
    ]

    // VHF bands for meteor scatter modes
    property var vhfBands: [
        { index: 16, lambda: "4M",   name: "70 MHz" },
        { index: 17, lambda: "2M",   name: "144 MHz" },
        { index: 19, lambda: "70CM", name: "432 MHz" }
    ]

    implicitHeight: contentColumn.height

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        // Current band display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2)
            border.color: primaryBlue
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Text {
                    text: "Band:"
                    color: textSecondary
                    font.pixelSize: 12
                }

                Text {
                    text: bandMgr ? bandMgr.currentBandLambda : "--"
                    color: primaryBlue
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    text: bandMgr ? "(" + bandMgr.currentBandName + ")" : ""
                    color: textSecondary
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Dial:"
                    color: textSecondary
                    font.pixelSize: 12
                }

                Text {
                    text: bandMgr ? formatFrequency(bandMgr.dialFrequency) : "--"
                    color: accentGreen
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Consolas"
                }
            }
        }

        // HF Bands row
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "HF:"
                color: textSecondary
                font.pixelSize: 10
                Layout.preferredWidth: 24
            }

            Repeater {
                model: hfBands

                BandButton {
                    bandIndex: modelData.index
                    bandLambda: modelData.lambda
                    bandName: modelData.name
                    onClicked: {
                        if (bandMgr) {
                            bandMgr.changeBand(bandIndex)
                        }
                    }
                }
            }
        }

        // VHF Bands row (optional - show in collapsed state or when needed)
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: engine && (engine.mode === "MSK144" || engine.mode === "JTMS" || engine.mode === "FSK441")

            Text {
                text: "VHF:"
                color: textSecondary
                font.pixelSize: 10
                Layout.preferredWidth: 24
            }

            Repeater {
                model: vhfBands

                BandButton {
                    bandIndex: modelData.index
                    bandLambda: modelData.lambda
                    bandName: modelData.name
                    onClicked: {
                        if (bandMgr) {
                            bandMgr.changeBand(bandIndex)
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    // Band Button component
    component BandButton: Button {
        id: bandBtn
        property int bandIndex: 0
        property string bandLambda: ""
        property string bandName: ""

        // Use internal bandIndex for selection - more reliable binding
        readonly property bool isSelected: bandMgr && bandMgr.currentBandIndex === bandBtn.bandIndex

        Layout.preferredWidth: 42
        Layout.preferredHeight: 28

        background: Rectangle {
            color: bandBtn.isSelected ?
                   Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.5) :
                   (bandBtn.hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1))
            border.color: bandBtn.isSelected ? primaryBlue : (bandBtn.hovered ? secondaryCyan : glassBorder)
            border.width: bandBtn.isSelected ? 2 : 1
            radius: 4

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }

            // Glow effect when selected
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                visible: bandBtn.isSelected
                color: "transparent"
                border.color: primaryBlue
                border.width: 1
                opacity: 0.5

                SequentialAnimation on opacity {
                    running: bandBtn.isSelected
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 800 }
                    NumberAnimation { to: 0.7; duration: 800 }
                }
            }
        }

        contentItem: Text {
            text: bandBtn.bandLambda
            color: bandBtn.isSelected ? "#ffffff" : (bandBtn.hovered ? secondaryCyan : textPrimary)
            font.pixelSize: 10
            font.bold: bandBtn.isSelected
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        ToolTip.visible: hovered
        ToolTip.text: bandBtn.bandName
        ToolTip.delay: 500
    }

    // Helper function to format frequency
    function formatFrequency(freqHz) {
        if (freqHz < 1000000) {
            return (freqHz / 1000).toFixed(1) + " kHz"
        } else if (freqHz < 1000000000) {
            return (freqHz / 1000000).toFixed(3) + " MHz"
        } else {
            return (freqHz / 1000000000).toFixed(3) + " GHz"
        }
    }
}
