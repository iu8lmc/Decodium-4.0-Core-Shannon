// Shared visual foundation for every page hosted in the separate SSTV window.
// A QQuickWindow does not inherit Main.qml's Material palette, so plain
// Controls otherwise fall back to the platform (often light) palette.
import QtQuick
import QtQuick.Controls.Material

Item {
    id: root

    // These are injected by SstvWorkspace from ThemeManager.  Keep fallbacks
    // so an isolated page remains legible in QML tests and previews.
    property color panelColor: "#111c25"
    property color borderColor: "#273946"
    property color primaryTextColor: "#e5edf3"
    property color secondaryTextColor: "#91a0ab"
    property color accentColor: "#24c9ee"
    property color successColor: "#43d17b"
    property color warningColor: "#ffb454"

    // Mix the existing panel and border colours for native controls rather
    // than inheriting the host OS's white button/base colours.  On the dark
    // Decodium palette this gives approximately #20303c, while adapting when
    // the application theme supplies different colours.
    readonly property color controlSurfaceColor: Qt.rgba(
            panelColor.r * 0.30 + borderColor.r * 0.70,
            panelColor.g * 0.30 + borderColor.g * 0.70,
            panelColor.b * 0.30 + borderColor.b * 0.70,
            1.0)
    readonly property bool darkControlTheme:
        (primaryTextColor.r * 0.2126
         + primaryTextColor.g * 0.7152
         + primaryTextColor.b * 0.0722) > 0.50

    // The Material attachment reaches all child Controls.  This is necessary
    // because SstvWorkspace is its own top-level Window and cannot inherit
    // Main.qml's attached Material theme.
    Material.theme: root.darkControlTheme ? Material.Dark : Material.Light
    Material.accent: root.accentColor
    Material.primary: root.accentColor
    Material.foreground: root.primaryTextColor
    Material.background: root.panelColor

    // Material does not give every native/platform control the same palette
    // role.  Supply all relevant roles explicitly, including Disabled, so
    // controls are legible without making unavailable actions look enabled.
    palette: Palette {
        active: ColorGroup {
            base: root.controlSurfaceColor
            alternateBase: root.panelColor
            button: root.controlSurfaceColor
            buttonText: root.primaryTextColor
            text: root.primaryTextColor
            window: root.panelColor
            windowText: root.primaryTextColor
            placeholderText: root.secondaryTextColor
            highlight: root.accentColor
            highlightedText: root.panelColor
            brightText: root.primaryTextColor
            light: root.primaryTextColor
            midlight: root.borderColor
            mid: root.borderColor
            dark: root.panelColor
            shadow: root.panelColor
            link: root.accentColor
            linkVisited: root.accentColor
            toolTipBase: root.controlSurfaceColor
            toolTipText: root.primaryTextColor
            accent: root.accentColor
        }
        inactive: ColorGroup {
            base: root.controlSurfaceColor
            alternateBase: root.panelColor
            button: root.controlSurfaceColor
            buttonText: root.primaryTextColor
            text: root.primaryTextColor
            window: root.panelColor
            windowText: root.primaryTextColor
            placeholderText: root.secondaryTextColor
            highlight: root.accentColor
            highlightedText: root.panelColor
            brightText: root.primaryTextColor
            light: root.primaryTextColor
            midlight: root.borderColor
            mid: root.borderColor
            dark: root.panelColor
            shadow: root.panelColor
            link: root.accentColor
            linkVisited: root.accentColor
            toolTipBase: root.controlSurfaceColor
            toolTipText: root.primaryTextColor
            accent: root.accentColor
        }
        disabled: ColorGroup {
            base: root.controlSurfaceColor
            alternateBase: root.panelColor
            button: root.controlSurfaceColor
            buttonText: root.secondaryTextColor
            text: root.secondaryTextColor
            window: root.panelColor
            windowText: root.secondaryTextColor
            placeholderText: root.secondaryTextColor
            highlight: root.borderColor
            highlightedText: root.secondaryTextColor
            brightText: root.secondaryTextColor
            light: root.secondaryTextColor
            midlight: root.borderColor
            mid: root.borderColor
            dark: root.panelColor
            shadow: root.panelColor
            link: root.secondaryTextColor
            linkVisited: root.secondaryTextColor
            toolTipBase: root.controlSurfaceColor
            toolTipText: root.secondaryTextColor
            accent: root.borderColor
        }
    }
}
