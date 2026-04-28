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
    x: Math.round(Math.max(0, ((Screen.desktopAvailableWidth || Screen.width || width) - width) / 2))
    y: Math.round(Math.max(0, ((Screen.desktopAvailableHeight || Screen.height || height) - height) / 2))
    title: "Decodium 4.0 — Loading..."
    color: "#1a1a2e"
    property int mainLoadElapsedSeconds: 0
    property double mainLoadStartedMs: 0
    property double bootStartedMs: Date.now()
    property bool startupTimedOut: false

    function bootElapsedMs() {
        return Math.round(Date.now() - bootStartedMs)
    }

    Component.onCompleted: {
        console.log("BootLoader: window visible at +" + bootElapsedMs()
                    + " ms, starting async load of Main.qml")
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
            running: mainLoader.status === Loader.Loading && !bootWindow.startupTimedOut
            Material.accent: "#ff7814"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                if (bootWindow.startupTimedOut)
                    return "Startup is taking too long. Safe graphics will be used on the next launch."
                switch (mainLoader.status) {
                case Loader.Loading: return "Loading UI components..."
                case Loader.Error:   return "Error loading UI: " + mainLoader.sourceComponent
                default:             return ""
                }
            }
            color: bootWindow.startupTimedOut || mainLoader.status === Loader.Error ? "#ff8844" : "#888899"
            font.pixelSize: 13
        }

        // Error details
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: mainLoader.status === Loader.Error || bootWindow.startupTimedOut
            text: bootWindow.startupTimedOut
                  ? "Decodium will close now. Open it again to retry with the software renderer."
                  : "Try reinstalling Decodium or report this bug."
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
            var elapsedMs = bootWindow.mainLoadStartedMs > 0
                    ? Math.round(Date.now() - bootWindow.mainLoadStartedMs)
                    : -1
            console.log("BootLoader: Loader status = " + status +
                        (status === Loader.Error ? " error" :
                         status === Loader.Ready ? " ready" : " loading")
                        + " elapsed=" + elapsedMs + " ms")
        }

        onLoaded: {
            var elapsedMs = bootWindow.mainLoadStartedMs > 0
                    ? Math.round(Date.now() - bootWindow.mainLoadStartedMs)
                    : -1
            console.log("BootLoader: Main.qml loaded OK in " + elapsedMs
                        + " ms, transferring to main window")
            // Main.qml creates its own ApplicationWindow, so hide boot window
            // first and close it after Windows has registered the new taskbar
            // window. Closing immediately can briefly remove the taskbar icon
            // on some Windows/GPU combinations until Alt+Tab refreshes it.
            bootWindow.visible = false
            bootWindowCloseTimer.restart()
        }
    }

    Timer {
        id: bootWindowCloseTimer
        interval: 2500
        repeat: false
        onTriggered: bootWindow.close()
    }

    Timer {
        id: mainLoadWatchdog
        interval: 10000
        repeat: true
        running: mainLoader.status === Loader.Loading && !bootWindow.startupTimedOut
        onTriggered: {
            bootWindow.mainLoadElapsedSeconds += 10
            console.log("BootLoader watchdog: Main.qml still loading after "
                        + bootWindow.mainLoadElapsedSeconds + " s")
            if (bootWindow.mainLoadElapsedSeconds >= 90) {
                bootWindow.startupTimedOut = true
                console.log("BootLoader watchdog: enabling safe graphics for next launch")
                bridge.requestSafeGraphicsNextLaunch("BootLoader Main.qml load exceeded "
                                                     + bootWindow.mainLoadElapsedSeconds
                                                     + " seconds")
                mainLoader.active = false
                delayedQuitTimer.restart()
            }
        }
    }

    Timer {
        id: delayedQuitTimer
        interval: 3500
        repeat: false
        onTriggered: Qt.quit()
    }

    Timer {
        interval: 100  // let the boot window paint first
        running: true
        repeat: false
        onTriggered: {
            console.log("BootLoader: starting Main.qml load at +" + bootWindow.bootElapsedMs() + " ms")
            bootWindow.mainLoadElapsedSeconds = 0
            bootWindow.mainLoadStartedMs = Date.now()
            mainLoader.source = "Main.qml"
        }
    }
}
