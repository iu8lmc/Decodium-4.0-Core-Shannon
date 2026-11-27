import QtQuick
import QtTest
import "../../qml/decodium/components"

TestCase {
    id: testCase
    name: "DxClusterCalendar"
    visible: true
    when: windowShown
    width: 800; height: 650
    property var bridge: fakeBridge
    QtObject {
        id: fakeBridge
        property bool offlineMode: true
        property string dxCall: ""
        property QtObject dxCluster: QtObject {
            property bool connected: false
            property string host: ""
            property var spotModel: null
        }
        property QtObject themeManager: QtObject {
            property color bgDeep: "#111111"
            property color secondaryColor: "cyan"
            property color accentColor: "green"
            property color textPrimary: "white"
            property color textSecondary: "lightgrey"
            property color glassBorder: "grey"
            property color warningColor: "orange"
            property bool isGlassTheme: false
            property bool glassTransparencySupported: false
            property color panelColor: "black"
        }
        function getSetting(name, value) { return value }
        function setSetting(name, value) {}
    }
    Component { id: cluster; DxClusterPanel { embedded: true; minPanelWidth: 0; minPanelHeight: 0; width: 320; height: 350 } }
    function test_open_calendar_from_narrow_cluster() {
        let panel = createTemporaryObject(cluster, testCase)
        verify(panel !== null)
        let button = findChild(panel, "openDxCalendar")
        let calendar = findChild(panel, "dxCalendar")
        verify(button !== null)
        let label = findChild(button, "dxCalendarButtonLabel")
        verify(label !== null)
        compare(label.text, "DX calendar")
        compare(label.color, fakeBridge.themeManager.secondaryColor)
        verify(calendar !== null)
        wait(50)
        verify(button.height >= 24, "Calendar button must retain its height")
        verify(label.height >= label.implicitHeight, "Calendar label must not be clipped")
        let point = button.mapToItem(panel, 0, 0)
        verify(point.x >= 0 && point.x + button.width <= panel.width,
               "Calendar button must remain inside the narrow cluster")
        mouseClick(button, button.width / 2, button.height / 2)
        tryVerify(function() { return calendar.opened })
        compare(fakeBridge.dxCall, "")
        calendar.close()
    }
}
