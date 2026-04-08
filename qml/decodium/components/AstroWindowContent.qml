/* Astro Window Content - Floating version */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "transparent"

    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color moonColor: "#ffd54f"
    property color sunColor: "#ff9800"
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
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

                Text { text: "Location:"; font.pixelSize: 12; color: textSecondary }
                Text {
                    text: appEngine && appEngine.astroManager ? appEngine.astroManager.gridLocator : "----"
                    font.pixelSize: 14; font.bold: true; font.family: "Consolas"; color: primaryBlue
                }
                Text { text: "Lat:"; font.pixelSize: 11; color: textSecondary }
                Text {
                    text: appEngine && appEngine.astroManager ? appEngine.astroManager.latitude.toFixed(2) + "°" : "--"
                    font.pixelSize: 11; font.family: "Consolas"; color: textPrimary
                }
                Text { text: "Lon:"; font.pixelSize: 11; color: textSecondary }
                Text {
                    text: appEngine && appEngine.astroManager ? appEngine.astroManager.longitude.toFixed(2) + "°" : "--"
                    font.pixelSize: 11; font.family: "Consolas"; color: textPrimary
                }
            }
        }

        // Moon and Sun side by side
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // Moon Section
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                color: Qt.rgba(255/255, 213/255, 79/255, 0.1)
                border.color: moonColor
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Text { text: "🌙"; font.pixelSize: 24 }
                        Text { text: "Moon"; font.pixelSize: 16; font.bold: true; color: moonColor }
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 6

                        Text { text: "Azimuth:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.moonAzimuth.toFixed(1) + "°" : "--"
                            color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                        }
                        Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.moonElevation.toFixed(1) + "°" : "--"
                            color: appEngine && appEngine.astroManager && appEngine.astroManager.moonElevation > 0 ? accentGreen : "#f44336"
                            font.pixelSize: 12; font.family: "Consolas"
                        }
                        Text { text: "Distance:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? (appEngine.astroManager.moonDistance / 1000).toFixed(0) + " km" : "--"
                            color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                        }
                        Text { text: "Phase:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.moonPhase.toFixed(0) + "%" : "--"
                            color: moonColor; font.pixelSize: 12; font.family: "Consolas"
                        }
                    }
                }
            }

            // Sun Section
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                color: Qt.rgba(255/255, 152/255, 0/255, 0.1)
                border.color: sunColor
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Text { text: "☀️"; font.pixelSize: 24 }
                        Text { text: "Sun"; font.pixelSize: 16; font.bold: true; color: sunColor }
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 6

                        Text { text: "Azimuth:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.sunAzimuth.toFixed(1) + "°" : "--"
                            color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                        }
                        Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.sunElevation.toFixed(1) + "°" : "--"
                            color: appEngine && appEngine.astroManager && appEngine.astroManager.sunElevation > 0 ? sunColor : "#666"
                            font.pixelSize: 12; font.family: "Consolas"
                        }
                        Text { text: "Sunrise:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.sunrise : "--:--"
                            color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                        }
                        Text { text: "Sunset:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: appEngine && appEngine.astroManager ? appEngine.astroManager.sunset : "--:--"
                            color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                        }
                    }
                }
            }
        }

        // EME Info
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.1)
            border.color: accentGreen
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Text { text: "EME (Earth-Moon-Earth)"; font.pixelSize: 14; font.bold: true; color: accentGreen }
                RowLayout {
                    spacing: 30
                    Text { text: "Path Loss:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine && appEngine.astroManager ? appEngine.astroManager.emePathLoss.toFixed(1) + " dB" : "--"
                        color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                    }
                    Text { text: "Degradation:"; color: textSecondary; font.pixelSize: 11 }
                    Text {
                        text: appEngine && appEngine.astroManager ? appEngine.astroManager.emeDegradation.toFixed(1) + " dB" : "--"
                        color: textPrimary; font.pixelSize: 12; font.family: "Consolas"
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
