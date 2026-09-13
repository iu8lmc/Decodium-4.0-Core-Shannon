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
    title: qsTr("Astronomical Data")
    modal: false
    width: nativeHostWindow && parent
           ? Math.max(480, parent.width - 24)
           : Math.max(480, Math.min(620, (parent ? parent.width : 620) - 48))
    height: nativeHostWindow && parent
            ? Math.max(430, parent.height - 24)
            : Math.max(430, Math.min(720, (parent ? parent.height : 720) - 48))
    padding: 16
    closePolicy: nativeHostWindow ? Popup.NoAutoClose : Popup.CloseOnEscape
    property var nativeHostWindow: null
    property bool positionInitialized: false
    property int minimumResizeWidth: 480
    property int minimumResizeHeight: 430

    function clampToParent() {
        if (nativeHostWindow) return
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

    function startNativeHostMove() {
        if (!nativeHostWindow || typeof nativeHostWindow.startSystemMove !== "function")
            return false
        try {
            return nativeHostWindow.startSystemMove()
        } catch (error) {
            console.log("Astro native move failed: " + error)
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
        astroWindow.close()
    }

    function requestWindowMinimize() {
        if (nativeHostWindow && typeof nativeHostWindow.minimizeHostedWindow === "function") {
            nativeHostWindow.minimizeHostedWindow()
            return
        }
        astroWindowMinimized = true
        astroWindow.close()
    }

    function displayPropagationValue(value, placeholder) {
        if (value === undefined || value === null) return placeholder || "---"
        const text = String(value)
        return text.length > 0 ? text : (placeholder || "---")
    }

    onAboutToShow: {
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
    property bool refreshFeedbackActive: false

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

    // A Dialog hosted by a separate QQuickWindow does not inherit the
    // ApplicationWindow's attached Material palette on every platform.
    Material.theme: bridge.themeManager.isLightTheme ? Material.Light : Material.Dark
    Material.accent: bridge.themeManager.primaryColor
    Material.primary: bridge.themeManager.secondaryColor

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
            z: 2
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            // Keep the drag handle away from the satellite, minimize and
            // close controls.  The handle used to overlap the satellite
            // button on compact dialog widths and consumed its click.
            anchors.rightMargin: 160
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            property point pressGlobalPos: Qt.point(0, 0)
            property point pressWindowPos: Qt.point(0, 0)
            property bool nativeMoveActive: false
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                pressWindowPos = astroWindow.nativeHostWindow
                        ? Qt.point(astroWindow.nativeHostWindow.x,
                                   astroWindow.nativeHostWindow.y)
                        : Qt.point(astroWindow.x, astroWindow.y)
                astroWindow.positionInitialized = true
                nativeMoveActive = astroWindow.startNativeHostMove()
                mouse.accepted = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                if (nativeMoveActive) return
                var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                if (astroWindow.nativeHostWindow) {
                    astroWindow.nativeHostWindow.x = pressWindowPos.x
                            + currentGlobalPos.x - pressGlobalPos.x
                    astroWindow.nativeHostWindow.y = pressWindowPos.y
                            + currentGlobalPos.y - pressGlobalPos.y
                } else {
                    astroWindow.x = pressWindowPos.x
                            + currentGlobalPos.x - pressGlobalPos.x
                    astroWindow.y = pressWindowPos.y
                            + currentGlobalPos.y - pressGlobalPos.y
                    astroWindow.clampToParent()
                }
                mouse.accepted = true
            }
            onReleased: {
                nativeMoveActive = false
                astroWindow.finishNativeHostMove()
            }
            onCanceled: {
                nativeMoveActive = false
                astroWindow.finishNativeHostMove()
            }
        }

        RowLayout {
            z: 3
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                text: qsTr("🌙 Astronomical Data")
                font.pixelSize: 18
                font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                id: satelliteTrackingButton
                z: 3
                text: "🛰"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Satellite tracking")
                onClicked: {
                    if (typeof appEngine !== "undefined" && appEngine
                            && appEngine.requestSatelliteTrackingWindow)
                        appEngine.requestSatelliteTrackingWindow()
                }
            }

            Rectangle {
                z: 3
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
                    onClicked: astroWindow.requestWindowMinimize()
                }

                ToolTip.visible: astroMinMA.containsMouse
                ToolTip.text: qsTr("Minimize")
                ToolTip.delay: 500
            }

            Rectangle {
                z: 3
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
                    onClicked: astroWindow.requestWindowClose()
                }

                ToolTip.visible: astroCloseMA.containsMouse
                ToolTip.text: qsTr("Close")
                ToolTip.delay: 500
            }
        }
    }

    function refreshAstroData() {
        refreshFeedbackActive = true
        refreshFeedbackTimer.restart()

        if (astroWindow.astroManager) {
            astroWindow.astroManager.update()
        }
        if (astroWindow.propagationManager) {
            astroWindow.propagationManager.refresh()
        }
    }

    contentItem: Item {
        implicitWidth: astroContentLayout.implicitWidth
        implicitHeight: astroContentLayout.implicitHeight

        ColumnLayout {
            id: astroContentLayout
            anchors.fill: parent
            spacing: 10

            ScrollView {
                id: astroScroll
                clip: true
                rightPadding: 12
                bottomPadding: 8
                contentWidth: availableWidth
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
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
                            text: qsTr("Location:")
                            font.pixelSize: 12
                            color: textSecondary
                        }

                        Text {
                            text: astroWindow.hasAstroManager ? astroManager.gridLocator : astroWindow.fallbackGrid
                            font.pixelSize: 14
                            font.bold: true
                            font.family: decodiumMonoFontFamily
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
                            font.family: decodiumMonoFontFamily
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
                            text: qsTr("Moon")
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
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonElevation.toFixed(1) + "°" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: astroManager && astroManager.moonElevation > 0 ? accentGreen : "#f44336"
                        }

                        Text { text: qsTr("Distance:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? (astroManager.moonDistance / 1000).toFixed(0) + " Mm" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Doppler:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonDoppler.toFixed(0) + " Hz" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Rise:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonRise + " UTC" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Set:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.moonSet + " UTC" : "---"
                            font.family: decodiumMonoFontFamily
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
                        text: qsTr("Sun")
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
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Elevation:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.sunElevation.toFixed(1) + "°" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: astroManager && astroManager.sunElevation > 0 ? sunColor : textSecondary
                        }

                        Text { text: qsTr("Sunrise:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.sunrise + " UTC" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Sunset:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.sunset + " UTC" : "---"
                            font.family: decodiumMonoFontFamily
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
                            text: qsTr("EME (Earth-Moon-Earth)")
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

                        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager && astroManager.emePossible ? "POSSIBLE" : "NOT POSSIBLE"
                            font.pixelSize: 12
                            font.bold: true
                            color: astroManager && astroManager.emePossible ? accentGreen : "#f44336"
                        }

                        Text { text: qsTr("Path Loss:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.emePathLoss.toFixed(1) + " dB" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "Doppler:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager
                                  ? (astroManager.emeDoppler >= 0 ? "+" : "") + astroManager.emeDoppler.toFixed(0) + " Hz"
                                  : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Frequency:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager ? astroManager.frequency.toFixed(3) + " MHz" : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: secondaryCyan
                        }

                        Text { text: qsTr("Window:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: astroManager
                                  ? astroManager.emeWindowStart + " - " + astroManager.emeWindowEnd + " UTC"
                                  : "---"
                            font.family: decodiumMonoFontFamily
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
                            text: qsTr("Propagation (HamQSL)")
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
                            font.family: decodiumMonoFontFamily
                            color: textSecondary
                        }
                    }

                    GridLayout {
                        columns: 4
                        columnSpacing: 20
                        rowSpacing: 6

                        Text { text: qsTr("Solar Flux:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.solarFlux) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("A-Index:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.aIndex) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("K-Index:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.kIndex) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("X-Ray:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.xRay) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Sunspots:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.sunspots) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Solar Wind:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.solarWind) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: "MUF:"; color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.muf) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Geomagnetic:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.geomagneticField) : "---"
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 12
                            color: textPrimary
                        }

                        Text { text: qsTr("Signal Noise:"); color: textSecondary; font.pixelSize: 11 }
                        Text {
                            text: propagationManager ? astroWindow.displayPropagationValue(propagationManager.signalNoise) : "---"
                            font.family: decodiumMonoFontFamily
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
                        text: qsTr("HF Band Conditions")
                        font.pixelSize: 14
                        font.bold: true
                        color: accentGreen
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.preferredWidth: 90
                            text: qsTr("Band")
                            font.pixelSize: 11
                            font.bold: true
                            color: textSecondary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Day")
                            font.pixelSize: 11
                            font.bold: true
                            color: textSecondary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Night")
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
                                font.family: decodiumMonoFontFamily
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
                        text: qsTr("No HF condition data available yet.")
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
                        text: qsTr("VHF Conditions")
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
                        text: qsTr("No VHF condition data available yet.")
                        font.pixelSize: 11
                        color: textSecondary
                    }
                }
            }

            }
        }

        Rectangle {
            visible: astroWindow.hasAstroManager || astroWindow.hasPropagationManager
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.84)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 5
                anchors.bottomMargin: 5

                Item { Layout.fillWidth: true }

                Button {
                    id: refreshButton
                    text: qsTr("Refresh")
                    implicitWidth: 132
                    implicitHeight: 34
                    scale: down ? 0.97 : 1.0

                    Behavior on scale {
                        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
                    }

                    background: Rectangle {
                        radius: 7
                        border.width: refreshButton.down || astroWindow.refreshFeedbackActive ? 2 : 1
                        border.color: astroWindow.refreshFeedbackActive
                                      ? accentGreen
                                      : (refreshButton.down ? textPrimary : secondaryCyan)
                        opacity: refreshButton.enabled ? 1.0 : 0.55

                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: astroWindow.refreshFeedbackActive
                                       ? Qt.lighter(accentGreen, 1.18)
                                       : (refreshButton.down ? Qt.darker(primaryBlue, 1.25) : primaryBlue)
                            }
                            GradientStop {
                                position: 1.0
                                color: astroWindow.refreshFeedbackActive
                                       ? accentGreen
                                       : (refreshButton.down ? primaryBlue : secondaryCyan)
                            }
                        }
                    }

                    contentItem: Text {
                        text: refreshButton.text
                        font.pixelSize: 12
                        font.bold: true
                        color: refreshButton.down || astroWindow.refreshFeedbackActive ? bgDeep : textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: astroWindow.refreshAstroData()
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

        Rectangle {
            id: astroResizeGrip
            z: 50
            width: 22
            height: 22
            radius: 5
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 2
            anchors.bottomMargin: 2
            color: astroResizeMA.containsMouse
                   ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.22)
                   : "transparent"
            border.color: astroResizeMA.containsMouse ? secondaryCyan : "transparent"

            Rectangle {
                width: 12
                height: 2
                radius: 1
                rotation: -45
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 5
                anchors.bottomMargin: 7
                color: astroResizeMA.containsMouse ? secondaryCyan : Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.55)
            }

            Rectangle {
                width: 7
                height: 2
                radius: 1
                rotation: -45
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 5
                anchors.bottomMargin: 12
                color: astroResizeMA.containsMouse ? secondaryCyan : Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.45)
            }

            MouseArea {
                id: astroResizeMA
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeFDiagCursor
                property point pressGlobalPos: Qt.point(0, 0)
                property real pressWidth: 0
                property real pressHeight: 0

                onPressed: function(mouse) {
                    pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    pressWidth = astroWindow.width
                    pressHeight = astroWindow.height
                    astroWindow.positionInitialized = true
                }

                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    var maxWidth = astroWindow.parent
                            ? Math.max(astroWindow.minimumResizeWidth, astroWindow.parent.width - astroWindow.x)
                            : 1200
                    var maxHeight = astroWindow.parent
                            ? Math.max(astroWindow.minimumResizeHeight, astroWindow.parent.height - astroWindow.y)
                            : 900
                    astroWindow.width = Math.max(astroWindow.minimumResizeWidth,
                                                 Math.min(maxWidth, pressWidth + currentGlobalPos.x - pressGlobalPos.x))
                    astroWindow.height = Math.max(astroWindow.minimumResizeHeight,
                                                  Math.min(maxHeight, pressHeight + currentGlobalPos.y - pressGlobalPos.y))
                }
            }
        }
    }

    Timer {
        id: refreshFeedbackTimer
        interval: 420
        repeat: false
        onTriggered: astroWindow.refreshFeedbackActive = false
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
