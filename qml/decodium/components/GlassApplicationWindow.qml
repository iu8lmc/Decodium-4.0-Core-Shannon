/* ApplicationWindow counterpart of GlassWindow for the main Decodium shell. */
import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    id: root

    property var themeManagerOverride: null
    readonly property var themeManager: root.themeManagerOverride
                                         ? root.themeManagerOverride
                                         : ((typeof bridge !== "undefined" && bridge)
                                            ? bridge.themeManager : null)
    readonly property bool glassTheme: !!(root.themeManager && root.themeManager.isGlassTheme)
    readonly property bool glassTransparencySupported: root.glassTheme
        && (!root.themeManager || root.themeManager.glassTransparencySupported !== false)
    property color surfaceColor: root.themeManager ? root.themeManager.bgDeep : "#0A1020"
    property bool transparentFrame: root.glassTheme
    readonly property bool roundedFrame: root.visibility !== Window.Maximized
                                       && root.visibility !== Window.FullScreen
    readonly property bool transparentCorners: root.transparentFrame
                                               && root.roundedFrame
                                               && root.glassTransparencySupported
    readonly property real frameRadius: root.roundedFrame
                                       ? (Qt.platform.os === "osx" ? 12 : 8) : 0
    readonly property color opaqueSurfaceColor: root.surfaceColor
    // Keep the meaning of this shared colour safe for windows/components
    // which bind to it directly: classic themes must repaint an opaque root
    // after a live Liquid Glass -> Ocean/Stellar/Darkcodium switch.
    readonly property color glassSurfaceColor: root.glassTheme
                                               && root.glassTransparencySupported
        ? (root.themeManager
           ? Qt.rgba(root.themeManager.bgDeep.r, root.themeManager.bgDeep.g,
                     root.themeManager.bgDeep.b, 0.30)
           : Qt.rgba(root.surfaceColor.r, root.surfaceColor.g,
                     root.surfaceColor.b, 0.30))
        : root.opaqueSurfaceColor

    // ApplicationWindow.contentItem is read-only; expose an explicit default
    // alias so Main.qml can keep its existing direct-child structure.
    default property alias windowContent: contentScene.data

    color: root.transparentCorners ? "transparent"
                                   : (root.glassTheme && root.glassTransparencySupported
                                      ? root.glassSurfaceColor : root.opaqueSurfaceColor)

    background: Item {
        id: frame
        clip: root.frameRadius > 0

        Rectangle {
            anchors.fill: parent
            radius: root.frameRadius
            antialiasing: root.frameRadius > 0
            color: root.glassTheme && root.glassTransparencySupported
                   ? "transparent" : root.opaqueSurfaceColor
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: root.glassTheme && root.glassTransparencySupported
                           ? Qt.rgba(root.surfaceColor.r, root.surfaceColor.g,
                                     root.surfaceColor.b, 0.24)
                           : root.opaqueSurfaceColor
                }
                GradientStop {
                    position: 0.46
                    color: root.glassTheme && root.glassTransparencySupported && root.themeManager
                           ? Qt.rgba(root.themeManager.bgMedium.r, root.themeManager.bgMedium.g,
                                     root.themeManager.bgMedium.b, 0.16)
                           : root.opaqueSurfaceColor
                }
                GradientStop {
                    position: 1.0
                    color: root.glassTheme && root.glassTransparencySupported && root.themeManager
                           ? Qt.rgba(root.themeManager.bgDeep.r, root.themeManager.bgDeep.g,
                                     root.themeManager.bgDeep.b, 0.30)
                           : root.opaqueSurfaceColor
                }
            }
            border.width: root.glassTheme ? 1 : 0
            border.color: root.themeManager ? root.themeManager.glassBorder : "transparent"
        }

        Rectangle {
            visible: root.glassTheme
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: Math.max(1, parent.height * 0.34)
            radius: root.frameRadius
            opacity: 0.16
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#FFFFFF" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    Item {
        id: contentScene
        anchors.fill: parent
        // Kept free of QtQuick.Effects for the Qt 6.4 Linux runtime.
    }
}
