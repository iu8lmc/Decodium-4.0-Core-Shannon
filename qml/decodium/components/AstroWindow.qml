/* Decodium Qt6 - Astronomical Data Window
 * Moon/Sun data and EME information
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: astroWindow
    title: "Astronomical Data"
    modal: false
    width: 500
    height: 550
    closePolicy: Popup.CloseOnEscape

    // Colors
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color moonColor: "#ffd54f"
    property color sunColor: "#ff9800"
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan
        border.width: 2
        radius: 12
    }

    header: Rectangle {
        height: 50
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                text: "🌙 Astronomical Data"
                font.pixelSize: 18
                font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            // Minimize button
            Rectangle {
                width: 28
                height: 28
                radius: 4
                color: astroMinMA.containsMouse ? Qt.rgba(255/255, 193/255, 7/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                border.color: astroMinMA.containsMouse ? "#ffc107" : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: "−"
                    font.pixelSize: 18
                    font.bold: true
                    color: astroMinMA.containsMouse ? "#ffc107" : textPrimary
                }

                MouseArea {
                    id: astroMinMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        astroWindowMinimized = true
                        astroWindow.close()
                    }
                }

                ToolTip.visible: astroMinMA.containsMouse
                ToolTip.text: "Riduci a icona"
                ToolTip.delay: 500
            }

            // Close button
            Rectangle {
                width: 28
                height: 28
                radius: 4
                color: astroCloseMA.containsMouse ? Qt.rgba(244/255, 67/255, 54/255, 0.3) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1)
                border.color: astroCloseMA.containsMouse ? "#f44336" : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 12
                    font.bold: true
                    color: astroCloseMA.containsMouse ? "#f44336" : textPrimary
                }

                MouseArea {
                    id: astroCloseMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: astroWindow.close()
                }

                ToolTip.visible: astroCloseMA.containsMouse
                ToolTip.text: "Chiudi"
                ToolTip.delay: 500
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Location display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15)
            border.color: primaryBlue
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 20

                Text {
                    text: "Location:"
                    font.pixelSize: 12
                    color: textSecondary
                }
                Text {
                    text: appEngine.astroManager ? appEngine.astroManager.gridLocator : "----"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Consolas"
                    color: primaryBlue
                }
                Text {
                    text: appEngine.astroManager ?
                          "(" + appEngine.astroManager.latitude.toFixed(2) + ", " +
                          appEngine.astroManager.longitude.toFixed(2) + ")" : ""
                    font.pixelSize: 11
                    color: textSecondary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: Qt.formatDateTime(new Date(), "yyyy-MM-dd hh:mm") + " UTC"
                    font.pixelSize: 11
                    font.family: "Consolas"
                    color: textSecondary
                }
            }
        }

        // Moon data
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            color: Qt.rgba(255/255, 213/255, 79/255, 0.1)
            border.color: moonColor
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Text {
                        text: "Moon"
                        font.pixelSize: 14
                        font.bold: true
                        color: moonColor
                    }
                    Text {
                        text: appEngine.astroManager ? "(" + appEngine.astroManager.moonPhase + ")" : ""
                        font.pixelSize: 11
                        color: textSecondary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.moonIllumination.toFixed(0) + "% illuminated" : ""
                        font.pixelSize: 11
                        color: moonColor
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 20
                    rowSpacing: 6

                    Text { text: "Azimuth:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.moonAzimuth.toFixed(1) + "°" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                    Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.moonElevation.toFixed(1) + "°" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: appEngine.astroManager && appEngine.astroManager.moonElevation > 0 ? accentGreen : "#f44336"
                    }

                    Text { text: "Distance:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? (appEngine.astroManager.moonDistance / 1000).toFixed(0) + " Mm" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                    Text { text: "Doppler:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.moonDoppler.toFixed(0) + " Hz" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }

                    Text { text: "Rise:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.moonRise + " UTC" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                    Text { text: "Set:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.moonSet + " UTC" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                }
            }
        }

        // Sun data
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: Qt.rgba(255/255, 152/255, 0/255, 0.1)
            border.color: sunColor
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "Sun"
                    font.pixelSize: 14
                    font.bold: true
                    color: sunColor
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 20
                    rowSpacing: 6

                    Text { text: "Azimuth:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.sunAzimuth.toFixed(1) + "°" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                    Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.sunElevation.toFixed(1) + "°" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: appEngine.astroManager && appEngine.astroManager.sunElevation > 0 ? sunColor : textSecondary
                    }

                    Text { text: "Sunrise:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.sunrise + " UTC" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                    Text { text: "Sunset:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.sunset + " UTC" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                }
            }
        }

        // EME Data
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.1)
            border.color: accentGreen
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Text {
                        text: "EME (Earth-Moon-Earth)"
                        font.pixelSize: 14
                        font.bold: true
                        color: accentGreen
                    }
                    Item { Layout.fillWidth: true }

                    // EME condition indicator
                    Row {
                        spacing: 2
                        Repeater {
                            model: 5
                            Rectangle {
                                width: 12
                                height: 12
                                radius: 2
                                color: appEngine.astroManager && index < appEngine.astroManager.emeCondition ?
                                       accentGreen : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                            }
                        }
                    }
                    Text {
                        text: appEngine.astroManager ? "Condition: " + appEngine.astroManager.emeCondition + "/5" : ""
                        font.pixelSize: 10
                        color: textSecondary
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 20
                    rowSpacing: 6

                    Text { text: "Status:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager && appEngine.astroManager.emePossible ? "POSSIBLE" : "NOT POSSIBLE"
                        font.pixelSize: 12
                        font.bold: true
                        color: appEngine.astroManager && appEngine.astroManager.emePossible ? accentGreen : "#f44336"
                    }
                    Text { text: "Path Loss:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.emePathLoss.toFixed(1) + " dB" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }

                    Text { text: "Doppler:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? (appEngine.astroManager.emeDoppler >= 0 ? "+" : "") +
                              appEngine.astroManager.emeDoppler.toFixed(0) + " Hz" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                    }
                    Text { text: "Frequency:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ? appEngine.astroManager.frequency.toFixed(3) + " MHz" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: secondaryCyan
                    }

                    Text { text: "Window:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine.astroManager ?
                              appEngine.astroManager.emeWindowStart + " - " +
                              appEngine.astroManager.emeWindowEnd + " UTC" : "---"
                        font.family: "Consolas"
                        font.pixelSize: 12
                        color: textPrimary
                        Layout.columnSpan: 3
                    }
                }
            }
        }

        // Refresh button
        RowLayout {
            Layout.alignment: Qt.AlignHCenter

            Button {
                text: "Refresh"
                implicitWidth: 100
                implicitHeight: 32

                background: Rectangle {
                    radius: 6
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: primaryBlue }
                        GradientStop { position: 1.0; color: secondaryCyan }
                    }
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 12
                    font.bold: true
                    color: textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }

                onClicked: {
                    if (appEngine.astroManager) {
                        appEngine.astroManager.update()
                    }
                }
            }
        }
    }

    // Auto-refresh timer
    Timer {
        interval: 60000
        running: astroWindow.visible
        repeat: true
        onTriggered: {
            if (appEngine.astroManager) {
                appEngine.astroManager.update()
            }
        }
    }

    // Update location from appEngine grid on open
    onOpened: {
        if (appEngine.astroManager && appEngine.grid) {
            appEngine.astroManager.setLocationFromGrid(appEngine.grid)
            appEngine.astroManager.frequency = appEngine.frequency / 1000000.0  // Hz to MHz
        }
    }
}
