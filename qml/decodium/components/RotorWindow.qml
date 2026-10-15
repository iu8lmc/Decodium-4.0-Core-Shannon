/* RotorWindow - il rotore d'antenna PRO.SIS.TEL dentro Decodium.
 *
 * Viene da DecoRotor (iu8lmc/decorotor): quadrante, mappa satellitare con le
 * stazioni sentite da Decodium, memorie e diagnostica. Si accende dalle
 * impostazioni della finestra: finche' e' spento non apre nessuna porta.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import "../rotor"

ApplicationWindow {
    id: window

    // Lo shack puo' avere un ultrawide scalato o un portatile: la finestra
    // si adatta allo spazio realmente disponibile invece di eccederlo.
    width: Math.min(1400, Screen.desktopAvailableWidth - 60)
    height: Math.min(900, Screen.desktopAvailableHeight - 40)
    minimumWidth: 940
    minimumHeight: 600
    title: qsTr("Rotator - PRO.SIS.TEL control")
    color: RotorTheme.bgDeep

    header: TopBar {}
    footer: StatusStrip {}

    // Il quadrante chiaro o notturno e' una scelta che resta nella
    // configurazione: alla riapertura lo shack ritrova la luce che aveva.
    Binding {
        target: RotorTheme
        property: "dark"
        value: rotor.darkTheme
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: RotorTheme.spacing
        spacing: RotorTheme.spacing

        TabBar {
            id: tabs

            Layout.fillWidth: true
            background: Item {}

            Repeater {
                model: [qsTr("CONTROL"), qsTr("DIAGNOSTICS"), qsTr("SETTINGS")]

                TabButton {
                    id: tab

                    required property string modelData

                    text: modelData
                    implicitHeight: 34
                    width: implicitWidth

                    background: Rectangle {
                        radius: 8
                        color: tab.checked ? Qt.rgba(0.22, 0.74, 0.97, 0.16) : "transparent"
                        border.color: tab.checked ? RotorTheme.primary : "transparent"
                        border.width: 1
                    }

                    contentItem: Text {
                        text: tab.text
                        color: tab.checked ? RotorTheme.primary : RotorTheme.textSecondary
                        font.pixelSize: RotorTheme.fontSmall
                        font.bold: true
                        font.letterSpacing: 1.2
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            ControlPage {}

            DiagnosticsPage {}

            SettingsPage {}
        }
    }

    // Avvisi del controllo (angolo fuori corsa, rotore spento, memoria piena...).
    Rectangle {
        id: toast

        property bool isError: false

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 56
        width: Math.min(parent.width - 40, toastText.implicitWidth + 32)
        height: toastText.implicitHeight + 18
        radius: 10
        color: RotorTheme.bgElevated
        border.color: isError ? RotorTheme.danger : RotorTheme.primary
        border.width: 1
        opacity: 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        Text {
            id: toastText

            anchors.centerIn: parent
            width: parent.width - 32
            color: RotorTheme.textPrimary
            font.pixelSize: RotorTheme.fontBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Timer {
            id: toastTimer

            interval: 4000
            onTriggered: toast.opacity = 0
        }
    }

    Connections {
        target: rotor

        function onNotified(message, isError) {
            toastText.text = message
            toast.isError = isError
            toast.opacity = 1
            toastTimer.restart()
        }
    }
}
