import QtQuick
import QtQuick.Controls

Rectangle {
    id: indicator
    required property var engine
    property color activeColor: "#00e59b"
    property color inactiveColor: "#8ca2bb"
    property color inactiveBorder: "#253b53"
    property color backgroundColor: "#0a101a"
    property int labelPixelSize: 11

    function asBool(value) {
        return value === true || value === 1
            || String(value).toLowerCase() === "true" || String(value) === "1"
    }
    property bool optionEnabled: engine ? asBool(engine.getSetting("SuperFox", false)) : false
    readonly property bool active: !!(engine && optionEnabled && engine.mode === "FT8"
                                      && (engine.houndMode || engine.foxMode))
    readonly property string statusDescription: active
        ? (engine.houndMode ? qsTr("SuperHound reception active") : qsTr("SuperFox transmission mode active"))
        : !optionEnabled ? qsTr("SuperFox is disabled in Settings")
        : !engine || engine.mode !== "FT8" ? qsTr("SuperFox requires FT8 mode")
        : qsTr("Enable Hound or Fox mode to use SuperFox")

    Connections {
        target: indicator.engine
        function onSettingValueChanged(key, value) {
            if (key === "SuperFox") indicator.optionEnabled = indicator.asBool(value)
        }
    }

    objectName: "superFoxIndicator"
    implicitWidth: content.implicitWidth + 16
    implicitHeight: 30
    radius: 4
    color: active ? Qt.alpha(activeColor, 0.14) : backgroundColor
    border.color: active ? activeColor : inactiveBorder
    Accessible.role: Accessible.Button
    Accessible.name: "SuperFox: " + statusDescription
    Accessible.onPressAction: toggleSuperFox()

    function toggleSuperFox() {
        if (!engine) return
        var next = !optionEnabled
        engine.setSetting("SuperFox", next)
        optionEnabled = next
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 6
        Rectangle {
            width: 6
            height: 6
            radius: 3
            anchors.verticalCenter: parent.verticalCenter
            color: indicator.active ? indicator.activeColor : indicator.inactiveColor
        }
        Text {
            text: "SuperFox"
            color: indicator.active ? indicator.activeColor : indicator.inactiveColor
            font.pixelSize: indicator.labelPixelSize
            font.bold: true
        }
    }
    HoverHandler { id: hover }
    TapHandler { onTapped: indicator.toggleSuperFox() }
    ToolTip.visible: hover.hovered
    ToolTip.delay: 500
    ToolTip.text: statusDescription
}
