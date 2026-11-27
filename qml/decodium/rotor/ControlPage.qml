import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: page

    property date currentTime: new Date()

    // La stazione scelta sulla mappa: il quadrante ne mostra il puntino, cosi'
    // si legge subito la sua direzione sulla corona dei gradi.
    property var chosenSpot: null

    spacing: RotorTheme.spacing

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: page.currentTime = new Date()
    }

    // Quadrante sopra, mondo vero sotto: la maniglia decide quanto spazio
    // dare all'uno o all'altro secondo quello che si sta facendo.
    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 660
        Layout.minimumWidth: 380
        orientation: Qt.Vertical

        handle: Rectangle {
            implicitHeight: 10
            color: "transparent"

            Rectangle {
                anchors.centerIn: parent
                width: 54
                height: 3
                radius: 1.5
                color: SplitHandle.pressed || SplitHandle.hovered ? RotorTheme.primary : RotorTheme.borderSoft
            }
        }

        GlassPanel {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 260

            RotorDial {
                anchors.fill: parent
                azimuth: rotor.azimuth
                target: rotor.azimuthTarget
                beamwidth: rotor.beamwidth
                hasPosition: rotor.hasPosition
                moving: rotor.moving
                limitMin: rotor.azMin
                limitMax: rotor.azMax
                latitude: rotor.myLatitude
                longitude: rotor.myLongitude
                pinLatitude: page.chosenSpot ? page.chosenSpot.lat : rotor.bearingLatitude
                pinLongitude: page.chosenSpot ? page.chosenSpot.lon : rotor.bearingLongitude
                pinValid: page.chosenSpot !== null || rotor.bearingValid
                onBearingRequested: (degrees) => rotor.gotoAzimuth(degrees)
            }

            // Ora locale e QTH negli angoli liberi del riquadro, dove il
            // quadrante rotondo non arriva.
            Column {
                anchors.top: parent.top
                anchors.left: parent.left
                spacing: 0

                Text {
                    text: Qt.formatTime(page.currentTime, "HH:mm")
                    color: RotorTheme.textSecondary
                    font.pixelSize: 22
                    font.family: RotorTheme.monoFamily
                    font.bold: true
                }

                Text {
                    text: Qt.formatDate(page.currentTime, "ddd d MMM")
                    color: RotorTheme.textDim
                    font.pixelSize: RotorTheme.fontSmall
                }
            }

            Column {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                spacing: 0

                Text {
                    anchors.right: parent.right
                    text: rotor.locator
                    color: RotorTheme.textSecondary
                    font.pixelSize: RotorTheme.fontBody
                    font.family: RotorTheme.monoFamily
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    text: qsTr("azimuthal map from the QTH")
                    color: RotorTheme.textDim
                    font.pixelSize: RotorTheme.fontSmall
                }
            }
        }

        Rectangle {
            // Il quadrante ha la precedenza: la mappa del mondo parte bassa e si
            // allarga dalla maniglia quando serve.
            SplitView.preferredHeight: 220
            SplitView.minimumHeight: 160

            color: RotorTheme.bgPanel
            border.color: RotorTheme.border
            border.width: 1
            radius: RotorTheme.radius
            clip: true

            SatelliteMap {
                anchors.fill: parent
                anchors.margins: 1
                homeLatitude: rotor.myLatitude
                homeLongitude: rotor.myLongitude
                onSelectedSpotChanged: page.chosenSpot = selectedSpot
            }
        }
    }

    // Su schermi bassi la colonna scorre invece di troncare i pannelli.
    ScrollView {
        Layout.fillHeight: true
        Layout.preferredWidth: 470
        Layout.minimumWidth: 450
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: RotorTheme.spacing

            DisplayPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: rotor.hasElevation ? 220 : 176
            }

            PresetGrid {
                Layout.fillWidth: true
                onManageRequested: memories.open()
            }

            CommandBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 122
                onPresetsRequested: memories.open()
            }

            PointingPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 104
            }
        }
    }

    Popup {
        id: memories

        x: (page.width - width) / 2
        y: Math.max(20, (page.height - height) / 2)
        width: Math.min(560, page.width - 80)
        height: Math.min(480, page.height - 60)
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Item {}

        PresetPanel {
            anchors.fill: parent
        }
    }
}
