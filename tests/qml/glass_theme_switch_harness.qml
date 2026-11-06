import QtQuick
import QtQuick.Window
import "../../qml/decodium/components" as DecodiumComponents

DecodiumComponents.GlassWindow {
    width: 240
    height: 160
    visible: false
    themeManagerOverride: testTheme
}
