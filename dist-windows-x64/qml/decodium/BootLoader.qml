/* Decodium 4.0 - Boot Loader
 * Shows a splash window immediately, then loads Main.qml asynchronously.
 * This guarantees the window appears even if component compilation is slow.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    id: bootWindow
    visible: true
    visibility: Window.Windowed
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
         | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
    width: 1200
    height: 700
    minimumWidth: 800
    minimumHeight: 500
    x: Math.round((Screen.desktopAvailableWidth - width) / 2)
    y: Math.round((Screen.desktopAvailableHeight - height) / 2)
    title: "Decodium 4.0 — Loading..."
    color: "#1a1a2e"

    Component.onCompleted: {
        console.log("BootLoader: window visible, starting async load of Main.qml")
        show()
        raise()
        requestActivate()
    }

    // Splash content while loading
    Column {
        id: splashContent
        anchors.centerIn: parent
        spacing: 20
        visible: mainLoader.status !== Loader.Ready

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "DECODIUM 4.0"
            color: "#ff7814"
            font.pixelSize: 42
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Core Shannon"
            color: "#aaaacc"
            font.pixelSize: 18
        }
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: mainLoader.status === Loader.Loading
            Material.accent: "#ff7814"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                switch (mainLoader.status) {
                case Loader.Loading: return "Loading UI components..."
                case Loader.Error:   return "Error loading UI: " + mainLoader.sourceComponent
                default:             return ""
                }
            }
            color: mainLoader.status === Loader.Error ? "#ff4444" : "#888899"
            font.pixelSize: 13
        }

        // Error details
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: mainLoader.status === Loader.Error
            text: "Try reinstalling Decodium or report this bug."
            color: "#ff8844"
            font.pixelSize: 12
        }
    }

    // Async loader for the real Main.qml content
    Loader {
        id: mainLoader
        asynchronous: true
        active: true
        // Use Timer to delay load by 100ms — ensures boot window is painted first
        source: ""

        onStatusChanged: {
            console.log("BootLoader: Loader status = " + status +
                        (status === Loader.Error ? " error" :
                         status === Loader.Ready ? " ready" : " loading"))
        }

        onLoaded: {
            console.log("BootLoader: Main.qml loaded OK, transferring to main window")
            // Main.qml creates its own ApplicationWindow, so hide boot window
            bootWindow.visible = false
            bootWindow.close()
        }
    }

    Timer {
        interval: 100  // let the boot window paint first
        running: true
        repeat: false
        onTriggered: {
            console.log("BootLoader: starting Main.qml load")
            mainLoader.source = "Main.qml"
        }
    }
}
