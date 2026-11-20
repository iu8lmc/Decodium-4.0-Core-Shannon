/*
 * Cross-platform glass window shell for Decodium.
 *
 * DecoDXLog uses the same design: Liquid Glass is a layered translucent
 * surface, not a hard dependency on a desktop blur API. This is important for
 * the Linux Qt runtime, where native material support is not guaranteed.
 * Classic themes remain opaque and keep their existing visual behaviour.
 */
import QtQuick
import QtQuick.Window

Window {
    id: root

    // Components opened from a Loader may expose their engine through a
    // different property (SSTV, for example).  Let those windows use the
    // same shell without depending on a process-global bridge name.
    property var themeManagerOverride: null
    readonly property var themeManager: root.themeManagerOverride
                                         ? root.themeManagerOverride
                                         : ((typeof bridge !== "undefined" && bridge)
                                            ? bridge.themeManager : null)
    readonly property bool glassTheme: !!(root.themeManager && root.themeManager.isGlassTheme)
    // Qt can expose an alpha channel on all three desktop families, but an
    // X11 session without a compositor cannot present it correctly.  The
    // theme manager keeps the platform decision in one place and the shell
    // falls back to an opaque surface when that capability is unavailable.
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
    // Several existing pop-outs bind their Window.color directly to this
    // property.  It must therefore resolve to an *opaque* surface as soon as
    // Liquid Glass is no longer active; otherwise a window opened in Glass
    // keeps the old 30% alpha when the user switches back to Ocean (or any
    // other classic theme).
    readonly property color glassSurfaceColor: root.glassTheme
                                               && root.glassTransparencySupported
        ? (root.themeManager
           ? Qt.rgba(root.themeManager.bgDeep.r, root.themeManager.bgDeep.g,
                     root.themeManager.bgDeep.b, 0.30)
           : Qt.rgba(root.surfaceColor.r, root.surfaceColor.g,
                     root.surfaceColor.b, 0.30))
        : root.opaqueSurfaceColor

    // Keep all existing Window { Item { ... } } content in one clipped scene.
    // This also makes the shell usable by pop-out windows declared inline in
    // Main.qml, without reparenting their loaders or timers.
    default property alias windowContent: contentScene.data

    // The alpha channel is intentional: it lets the desktop/compositor show
    // through the Liquid Glass shell.  If the platform cannot present an
    // alpha window, use a fully opaque colour instead of a black/undefined
    // surface.
    color: root.transparentCorners ? "transparent"
                                   : (root.glassTheme && root.glassTransparencySupported
                                      ? root.glassSurfaceColor : root.opaqueSurfaceColor)

    Item {
        id: frame
        anchors.fill: parent
        z: -100
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
        // Keep the shell free of QtQuick.Effects so the Linux Qt 6.4 runtime
        // remains portable.  The visible Liquid Glass treatment is provided
        // by the translucent surface, sheen and border above.
    }
}
