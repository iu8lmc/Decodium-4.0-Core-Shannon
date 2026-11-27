// Manual online smoke test: intentionally excluded from the offline CTest suite.
import QtQuick
import QtTest
import "../../qml/decodium/components"
TestCase {
    name: "DxCalendarLive"
    when: windowShown
    QtObject { id: mockBridge; property bool offlineMode: false }
    Component { id: calendar; DxCalendar { bridge: mockBridge; persistenceEnabled: false } }
    function test_live_feed() {
        let view = createTemporaryObject(calendar, this)
        view.refresh()
        tryVerify(function() { return !view.loading }, 20000)
        verify(view.entries.length > 0, view.statusText)
        console.log("Live DXCAL parsed entries:", view.entries.length)
    }
}
