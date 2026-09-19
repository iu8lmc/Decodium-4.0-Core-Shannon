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
    readonly property bool canToggle: !!(engine && !engine.transmitting && !engine.tuning
                                         && (engine.mode === "FT8" || optionEnabled))
    readonly property string statusDescription: active
        ? (engine.houndMode ? qsTr("SuperHound reception active") : qsTr("SuperFox transmission mode active"))
        : !optionEnabled ? qsTr("Click to enable SuperFox reception in FT8")
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
    Accessible.checkable: true
    Accessible.checked: active
    Accessible.onPressAction: toggleSuperFox()

    function toggleSuperFox() {
        if (!canToggle) return
        // A stored option without Hound/Fox is not an active SuperFox mode.
        // Complete reception setup in one click; never enable Fox transmission
        // implicitly or change the operator's selected radio mode.
        var next = engine.mode === "FT8" ? !active : false
        // Exit Hound before changing the decoder option; on entry set the
        // option first. Avoid configuring ordinary Hound as an intermediate.
        if (!next && engine.houndMode)
            engine.houndMode = false
        engine.setSetting("SuperFox", next)
        if (next && !engine.foxMode && !engine.houndMode)
            engine.houndMode = true
        optionEnabled = next
        // Both setters persist their own keys. A full profile save here also
        // snapshots unrelated settings and stalls the GUI during mode changes.
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
    TapHandler { enabled: indicator.canToggle; onTapped: indicator.toggleSuperFox() }
    ToolTip.visible: hover.hovered
    ToolTip.delay: 500
    ToolTip.text: statusDescription
}
