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
    flags: Qt.SplashScreen | Qt.WindowStaysOnTopHint
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
    property var mainComponent: null
    property var mainWindowObject: null
    property int mainLoadStatus: Component.Null

    function bootElapsedMs() {
        return Math.round(Date.now() - bootStartedMs)
    }

    function updateMainComponentStatus() {
        if (!mainComponent)
            return

        mainLoadStatus = mainComponent.status
        var elapsedMs = mainLoadStartedMs > 0
                ? Math.round(Date.now() - mainLoadStartedMs)
                : -1

        if (mainLoadStatus === Component.Ready) {
            componentStatusPoller.stop()
            if (!mainWindowObject) {
                mainWindowObject = mainComponent.createObject(null)
                if (!mainWindowObject) {
                    mainLoadStatus = Component.Error
                    console.log("BootLoader: Main.qml createObject(null) failed")
                    return
                }
                if (mainWindowObject.closing) {
                    mainWindowObject.closing.connect(function() { Qt.quit() })
                }
                if (mainWindowObject.show)
                    mainWindowObject.show()
                if (mainWindowObject.raise)
                    mainWindowObject.raise()
                if (mainWindowObject.requestActivate)
                    mainWindowObject.requestActivate()
            }
            console.log("BootLoader: Main.qml created as top-level window in "
                        + elapsedMs + " ms")
            // Keep this hidden root object alive so the top-level main window
            // is not garbage-collected. The splash type has no taskbar button.
            bootWindow.visible = false
        } else if (mainLoadStatus === Component.Error) {
            componentStatusPoller.stop()
            console.log("BootLoader: Main.qml load error: "
                        + (mainComponent ? mainComponent.errorString() : "unknown"))
        }
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
        visible: bootWindow.mainLoadStatus !== Component.Ready || !bootWindow.mainWindowObject

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
            running: bootWindow.mainLoadStatus === Component.Loading && !bootWindow.startupTimedOut
            Material.accent: "#ff7814"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                if (bootWindow.startupTimedOut)
                    return "Startup is taking too long. Safe graphics will be used on the next launch."
                switch (bootWindow.mainLoadStatus) {
                case Component.Loading: return "Loading UI components..."
                case Component.Error:   return "Error loading UI."
                default:             return ""
                }
            }
            color: bootWindow.startupTimedOut || bootWindow.mainLoadStatus === Component.Error ? "#ff8844" : "#888899"
            font.pixelSize: 13
        }

        // Error details
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: bootWindow.mainLoadStatus === Component.Error || bootWindow.startupTimedOut
            text: bootWindow.startupTimedOut
                  ? "Decodium will close now. Open it again to retry with the software renderer."
                  : (bootWindow.mainComponent ? bootWindow.mainComponent.errorString() : "Try reinstalling Decodium or report this bug.")
            color: "#ff8844"
            font.pixelSize: 12
        }
    }

    Timer {
        id: componentStatusPoller
        interval: 50
        repeat: true
        onTriggered: bootWindow.updateMainComponentStatus()
    }

    Timer {
        id: mainLoadWatchdog
        interval: 10000
        repeat: true
        running: bootWindow.mainLoadStatus === Component.Loading && !bootWindow.startupTimedOut
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
                componentStatusPoller.stop()
                bootWindow.mainComponent = null
                bootWindow.mainLoadStatus = Component.Error
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
            bootWindow.mainComponent = Qt.createComponent("Main.qml", Component.Asynchronous)
            bootWindow.updateMainComponentStatus()
            if (bootWindow.mainLoadStatus === Component.Loading)
                componentStatusPoller.start()
        }
    }
}
