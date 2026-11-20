import QtQuick
import QtQuick.Controls
import QtQuick.Templates as Templates
import QtTest

TestCase {
    id: testCase
    name: "TooltipTemplate"
    when: windowShown
    width: 300
    height: 120

    Rectangle {
        id: original
        width: 120; height: 100
        property bool tipShown: ToolTip.visible
        property string tipText: ToolTip.text
        property bool showTip: false
        ToolTip.visible: showTip
        ToolTip.text: "Original tooltip"
        ToolTip.delay: 50
        MouseArea { id: oldMouse; anchors.fill: parent; hoverEnabled: true }
    }
    Rectangle {
        id: replacement
        x: 150; width: 120; height: 100
        property bool tipShown: Templates.ToolTip.visible
        property string tipText: Templates.ToolTip.text
        property bool showTip: false
        Templates.ToolTip.visible: showTip
        Templates.ToolTip.text: "Replacement tooltip"
        Templates.ToolTip.delay: 50
        MouseArea { id: newMouse; anchors.fill: parent; hoverEnabled: true }
    }
    function test_sharedTooltipVisibility() {
        original.showTip = true
        tryCompare(original, "tipShown", true, 3000)
        compare(original.tipText, "Original tooltip")
        original.showTip = false
        replacement.showTip = true
        tryCompare(replacement, "tipShown", true, 3000)
        tryCompare(original, "tipShown", false, 3000)
        compare(replacement.tipText, "Replacement tooltip")
    }
}
