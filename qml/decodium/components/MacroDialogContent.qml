/* Macro Dialog Content - Floating version */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "transparent"

    property color bgDeep: bridge.themeManager.bgDeep
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        // Contest Mode Section
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.1)
            border.color: primaryBlue
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text { text: "Contest Mode"; font.pixelSize: 14; font.bold: true; color: primaryBlue }

                RowLayout {
                    spacing: 20
                    CheckBox {
                        id: contestCheck
                        text: "Enable Contest Mode"
                        checked: appEngine ? appEngine.inContestMode : false
                        onCheckedChanged: if (appEngine) appEngine.contestType = checked ? 1 : 0
                    }
                    Text { text: "Exchange:"; color: textSecondary; font.pixelSize: 11 }
                    TextField {
                        Layout.preferredWidth: 100
                        text: appEngine ? appEngine.contestExchange : ""
                        placeholderText: "e.g., 599 001"
                        onTextChanged: if (appEngine) appEngine.contestExchange = text
                        background: Rectangle {
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                            border.color: parent.focus ? secondaryCyan : glassBorder
                            radius: 4
                        }
                    }
                }
            }
        }

        // TX Macros Section
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.05)
            border.color: secondaryCyan
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text { text: "TX Macros"; font.pixelSize: 14; font.bold: true; color: secondaryCyan }

                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Repeater {
                        model: 6
                        delegate: RowLayout {
                            spacing: 8
                            Text {
                                text: "TX" + (index + 1) + ":"
                                font.pixelSize: 11
                                color: textSecondary
                                Layout.preferredWidth: 40
                            }
                            TextField {
                                Layout.preferredWidth: 220
                                text: appEngine && appEngine.macroManager ? appEngine.macroManager.getMacro(index) : ""
                                placeholderText: "Macro " + (index + 1)
                                font.pixelSize: 11
                                onTextChanged: if (appEngine && appEngine.macroManager) appEngine.macroManager.setMacro(index, text)
                                background: Rectangle {
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                                    border.color: parent.focus ? secondaryCyan : glassBorder
                                    radius: 4
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Macro variables help
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.05)
                    radius: 4

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text { text: "Available Variables:"; font.pixelSize: 10; font.bold: true; color: textSecondary }
                        Text {
                            text: "%CALL - Their callsign | %MYCALL - Your callsign | %GRID - Their grid"
                            font.pixelSize: 9; color: textSecondary; wrapMode: Text.Wrap
                        }
                        Text {
                            text: "%RST - Signal report | %EXCH - Contest exchange | %73 - 73 message"
                            font.pixelSize: 9; color: textSecondary; wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }
}
