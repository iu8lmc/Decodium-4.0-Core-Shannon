/* Decodium Qt6 - Astronomical Data Window
 * Moon/Sun data and EME information
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "../../panels"

Dialog {
    id: astroWindow
    title: "Astronomical Data"
    modal: false
    width: Math.max(480, Math.min(620, (parent ? parent.width : 620) - 48))
    height: Math.max(430, Math.min(720, (parent ? parent.height : 720) - 48))
    padding: 16
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

    function displayPropagationValue(value, placeholder) {
        if (value === undefined || value === null) return placeholder || "---"
        const text = String(value)
        return text.length > 0 ? text : (placeholder || "---")
    }

    onOpened: {
        ensureInitialPosition()
        if (astroWindow.astroManager && bridge.grid) {
            astroWindow.astroManager.setLocationFromGrid(bridge.grid)
            astroWindow.astroManager.frequency = bridge.frequency / 1000000.0
            astroWindow.astroManager.update()
        }
        if (astroWindow.propagationManager) {
            astroWindow.propagationManager.refresh()
        }
    }

    property var astroManager: (typeof appEngine !== "undefined" && appEngine) ? appEngine.astroManager : null
    property bool hasAstroManager: astroManager !== null && astroManager !== undefined
    property var propagationManager: (typeof appEngine !== "undefined" && appEngine) ? appEngine.propagationManager : null
    property bool hasPropagationManager: propagationManager !== null && propagationManager !== undefined
    property bool hasGrid: !!(bridge && bridge.grid && bridge.grid.length >= 4)
    property string fallbackGrid: hasGrid ? bridge.grid.toUpperCase() : "----"
    property real fallbackLatitude: hasGrid ? bridge.latFromGrid(bridge.grid) : 0.0
    property real fallbackLongitude: hasGrid ? bridge.lonFromGrid(bridge.grid) : 0.0

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

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                astroWindow.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                astroWindow.x += mouse.x - clickPos.x
                astroWindow.y += mouse.y - clickPos.y
                astroWindow.clampToParent()
            }
        }

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

            Rectangle {
                width: 28
                height: 28
                radius: 4
                color: astroMinMA.containsMouse
                       ? Qt.rgba(255 / 255, 193 / 255, 7 / 255, 0.3)
                       : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
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

            Rectangle {
                width: 28
                height: 28
                radius: 4
                color: astroCloseMA.containsMouse
                       ? Qt.rgba(244 / 255, 67 / 255, 54 / 255, 0.3)
                       : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
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

    contentItem: ScrollView {
        id: astroScroll
        clip: true
        rightPadding: 12
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: Math.max(astroScroll.availableWidth, 0)
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15)
                border.color: primaryBlue
                radius: 8

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12

                    Text {
                        text: "Location:"
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    Text {
                        text: astroWindow.hasAstroManager ? astroManager.gridLocator : astroWindow.fallbackGrid
                        font.pixelSize: 14
                        font.bold: true
                        font.family: "Consolas"
                        color: primaryBlue
                    }

                    Text {
                        Layout.fillWidth: true
                        text: astroWindow.hasAstroManager
                              ? "(" + astroManager.latitude.toFixed(2) + ", " + astroManager.longitude.toFixed(2) + ")"
                              : (astroWindow.hasGrid
                                 ? "(" + astroWindow.fallbackLatitude.toFixed(2) + ", " + astroWindow.fallbackLongitude.toFixed(2) + ")"
                                 : "Configure your grid locator to enable the local astro view")
                        font.pixelSize: 11
                        color: textSecondary
                        elide: Text.ElideRight
                    }

                    Text {
                        text: Qt.formatDateTime(new Date(), "yyyy-MM-dd hh:mm") + " UTC"
                        font.pixelSize: 11
                        font.family: "Consolas"
                        color: textSecondary
                    }
                }
            }

            Rectangle {
                visible: !astroWindow.hasAstroManager
                Layout.fillWidth: true
                Layout.preferredHeight: astroWindow.hasGrid ? 310 : 120
                color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.08)
                border.color: glassBorder
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: astroWindow.hasGrid
                              ? "Simplified astronomical data calculated locally from grid " + astroWindow.fallbackGrid
                              : "Astronomical backend not available. Configure your grid to enable the local fallback view."
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.bold: true
                        color: secondaryCyan
                        Layout.fillWidth: true
                    }

                    Text {
                        text: astroWindow.hasGrid
                              ? "Live Moon, Sun and EME essentials are shown below. Full rise/set ephemeris requires the dedicated astro backend."
                              : "Set the station grid and reopen this window to show the local Moon, Sun and EME panel."
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        color: textSecondary
                        Layout.fillWidth: true
                    }

                    AstroPanel {
                        visible: astroWindow.hasGrid
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 220
                    }
                }
            }

            Rectangle {
                visible: astroWindow.hasAstroManager
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                color: Qt.rgba(255 / 255, 213 / 255, 79 / 255, 0.1)
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
                            text: astroManager ? "(" + astroManager.moonPhase + ")" : ""
                            font.pixelSize: 11
                            color: textSecondary
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: astroManager ? astroManager.moonIllumination.toFixed(0) + "% illuminated" : ""
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
                            text: astroManager ? astroManager.moonAzimuth.toFixed(1) + "°" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonElevation.toFixed(1) + "°" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: astroManager && astroManager.moonElevation > 0 ? accentGreen : "#f44336"
                        }

                        Text { text: "Distance:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? (astroManager.moonDistance / 1000).toFixed(0) + " Mm" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Doppler:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonDoppler.toFixed(0) + " Hz" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Rise:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonRise + " UTC" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Set:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonSet + " UTC" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }
                    }
                }
            }

            Rectangle {
                visible: astroWindow.hasAstroManager
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                color: Qt.rgba(255 / 255, 152 / 255, 0 / 255, 0.1)
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
                            text: astroManager ? astroManager.sunAzimuth.toFixed(1) + "°" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.sunElevation.toFixed(1) + "°" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: astroManager && astroManager.sunElevation > 0 ? sunColor : textSecondary
                        }

                        Text { text: "Sunrise:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.sunrise + " UTC" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Sunset:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.sunset + " UTC" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }
                    }
                }
            }

            Rectangle {
                visible: astroWindow.hasAstroManager
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

                        Row {
                            spacing: 2

                            Repeater {
                                model: 5

                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 2
                                    color: astroManager && index < astroManager.emeCondition
                                           ? accentGreen : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                                }
                            }
                        }

                        Text {
                            text: astroManager ? "Condition: " + astroManager.emeCondition + "/5" : ""
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
                            text: astroManager && astroManager.emePossible ? "POSSIBLE" : "NOT POSSIBLE"
                            font.pixelSize: 12
                            font.bold: true
                            color: astroManager && astroManager.emePossible ? accentGreen : "#f44336"
                        }

                        Text { text: "Path Loss:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.emePathLoss.toFixed(1) + " dB" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Doppler:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager
                                  ? (astroManager.emeDoppler >= 0 ? "+" : "") + astroManager.emeDoppler.toFixed(0) + " Hz"
                                  : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Frequency:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.frequency.toFixed(3) + " MHz" : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: secondaryCyan
                        }

                        Text { text: "Window:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager
                                  ? astroManager.emeWindowStart + " - " + astroManager.emeWindowEnd + " UTC"
                                  : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                            Layout.columnSpan: 3
                        }
                    }
                }
            }

            Rectangle {
                visible: astroWindow.hasPropagationManager
                Layout.fillWidth: true
                Layout.preferredHeight: 250
                color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.09)
                border.color: secondaryCyan
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Text {
                            text: "Propagation (HamQSL)"
                            font.pixelSize: 14
                            font.bold: true
                            color: secondaryCyan
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: propagationManager
                                  ? astroWindow.displayPropagationValue(
                                        propagationManager.updated,
                                        propagationManager.updating ? "Updating..." : "Waiting for first update")
                                  : "---"
                            font.pixelSize: 10
                            font.family: "Consolas"
                            color: textSecondary
                        }
                    }

                    GridLayout {
                        columns: 4
                        columnSpacing: 20
                        rowSpacing: 6

                        Text { text: "Solar Flux:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.solarFlux) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "A-Index:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.aIndex) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "K-Index:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.kIndex) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "X-Ray:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.xRay) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Sunspots:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.sunspots) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Solar Wind:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.solarWind) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "MUF:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.muf) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Geomagnetic:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.geomagneticField) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Signal Noise:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.signalNoise) : "---"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: textPrimary
                            Layout.columnSpan: 3
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: glassBorder
                    }

                    Text {
                        Layout.fillWidth: true
                        text: propagationManager
                              ? astroWindow.displayPropagationValue(
                                    propagationManager.statusText,
                                    "Source: " + propagationManager.sourcePageUrl)
                              : "---"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 10
                        color: textSecondary
                    }
                }
            }

            Rectangle {
                visible: astroWindow.hasPropagationManager
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    150,
                    82 + ((propagationManager ? propagationManager.hfConditions.length : 0) * 28))
                color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.08)
                border.color: accentGreen
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "HF Band Conditions"
                        font.pixelSize: 14
                        font.bold: true
                        color: accentGreen
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.preferredWidth: 90
                            text: "Band"
                            font.pixelSize: 11
                            font.bold: true
                            color: textSecondary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "Day"
                            font.pixelSize: 11
                            font.bold: true
                            color: textSecondary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "Night"
                            font.pixelSize: 11
                            font.bold: true
                            color: textSecondary
                        }
                    }

                    Repeater {
                        model: propagationManager ? propagationManager.hfConditions : []

                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                Layout.preferredWidth: 90
                                text: modelData.band
                                font.family: "Consolas"
                                font.pixelSize: 12
                                color: textPrimary
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.02)
                                border.color: glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.day
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: modelData.dayColor
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.02)
                                border.color: glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.night
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: modelData.nightColor
                                }
                            }
                        }
                    }

                    Text {
                        visible: propagationManager && propagationManager.hfConditions.length === 0
                        text: "No HF condition data available yet."
                        font.pixelSize: 11
                        color: textSecondary
                    }
                }
            }

            Rectangle {
                visible: astroWindow.hasPropagationManager
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    120,
                    74 + ((propagationManager ? propagationManager.vhfConditions.length : 0) * 28))
                color: Qt.rgba(moonColor.r, moonColor.g, moonColor.b, 0.08)
                border.color: moonColor
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "VHF Conditions"
                        font.pixelSize: 14
                        font.bold: true
                        color: moonColor
                    }

                    Repeater {
                        model: propagationManager ? propagationManager.vhfConditions : []

                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                font.pixelSize: 11
                                color: textPrimary
                                elide: Text.ElideRight
                            }

                            Text {
                                text: modelData.status
                                font.pixelSize: 11
                                font.bold: true
                                color: modelData.color
                            }
                        }
                    }

                    Text {
                        visible: propagationManager && propagationManager.vhfConditions.length === 0
                        text: "No VHF condition data available yet."
                        font.pixelSize: 11
                        color: textSecondary
                    }
                }
            }

            RowLayout {
                visible: astroWindow.hasAstroManager || astroWindow.hasPropagationManager
                Layout.alignment: Qt.AlignHCenter

                Button {
                    id: refreshButton
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
                        text: refreshButton.text
                        font.pixelSize: 12
                        font.bold: true
                        color: textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }

                    onClicked: {
                        if (astroWindow.astroManager) {
                            astroWindow.astroManager.update()
                        }
                        if (astroWindow.propagationManager) {
                            astroWindow.propagationManager.refresh()
                        }
                    }
                }
            }
        }
    }

    Timer {
        interval: 60000
        running: astroWindow.visible && astroWindow.hasAstroManager
        repeat: true
        onTriggered: {
            if (astroWindow.astroManager) {
                astroWindow.astroManager.update()
            }
        }
    }

}
