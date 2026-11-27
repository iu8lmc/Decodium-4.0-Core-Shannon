import QtQuick
import QtTest
import "../../qml/decodium/components"
import "../../qml/decodium/components/DxCalendarParser.js" as Calendar

TestCase {
    id: testCase
    name: "DxCalendar"
    visible: true
    when: windowShown
    width: 800; height: 700
    readonly property string validFeed: "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nSUMMARY:Example Island (ZZ0ABC)\r\nDTSTART;VALUE=DATE:20261110\r\nDTEND;VALUE=DATE:20261112\r\nDESCRIPTION:FT8\\, CW\\; QSL via TEST\r\n continuation\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nSUMMARY:Future (ZZ1XYZ)\r\nDTSTART;VALUE=DATE:20261120\r\nDTEND;VALUE=DATE:20261125\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"
    function test_parser_and_boundaries() {
        let events = Calendar.parse(validFeed)
        compare(events.length, 2)
        compare(events[0].call, "ZZ0ABC")
        compare(events[0].entity, "Example Island")
        compare(events[0].description, "FT8, CW; QSL via TESTcontinuation")
        compare(Calendar.filter(events, "zz0abc", "active", "2026-11-12").length, 1)
        compare(Calendar.filter(events, "", "active", "2026-11-13").length, 0)
        compare(Calendar.filter(events, "", "upcoming", "2026-11-12").length, 1)
        compare(Calendar.parse("<html>error</html>").length, 0)
        compare(Calendar.isoDate("20260230"), "")
    }
    QtObject { id: mockBridge; property bool offlineMode: false }
    QtObject {
        id: mockCache
        property string data: ""
        property string updated: ""
        property int syncCount: 0
        signal cacheRead(string data, string updated, string error)
        signal cacheWritten(string error)
        function readCache() { cacheRead(data, updated, "") }
        function writeCache(value, when) { data = value; updated = when; syncCount++; cacheWritten("") }
    }
    Component { id: component; DxCalendar { bridge: mockBridge; persistenceEnabled: false } }
    Timer {
        id: heartbeat
        interval: 1
        repeat: true
        property var watched: null
        property int ticksDuringLoad: 0
        onTriggered: if (watched && watched.loading) ticksDuringLoad++
    }
    function response(status, body) {
        return { readyState: 0, status: status, responseText: body,
                 open: function(method, url) {},
                 send: function() { this.readyState = 4; this.onreadystatechange() },
                 abort: function() {} }
    }
    function test_network_and_preserve() {
        mockBridge.offlineMode = false
        let view = createTemporaryObject(component, testCase)
        verify(view !== null)
        view.requestFactory = function() { return response(200, validFeed) }
        view.refresh()
        tryCompare(view, "loading", false, 5000)
        compare(view.entries.length, 2)
        view.requestFactory = function() { return response(200, "broken") }
        view.refresh()
        tryCompare(view, "loading", false, 5000)
        compare(view.entries.length, 2)
        verify(view.statusText.indexOf("saved calendar was kept") >= 0)
        view.requestFactory = function() { return response(503, "") }
        view.refresh()
        tryCompare(view, "loading", false, 5000)
        compare(view.entries.length, 2)
        verify(view.statusText.indexOf("failed") >= 0)
        mockBridge.offlineMode = true
        view.refresh()
        tryCompare(view, "loading", false, 5000)
        compare(view.entries.length, 2)
        verify(view.statusText.indexOf("paused") >= 0)
        mockBridge.offlineMode = false
    }
    function test_cache_restore_after_failure() {
        mockCache.data = ""; mockCache.updated = ""; mockCache.syncCount = 0
        let first = createTemporaryObject(component, testCase,
                                          {persistenceEnabled: true, cacheBackend: mockCache})
        first.requestFactory = function() { return response(200, validFeed) }
        first.refresh()
        tryCompare(first, "loading", false, 5000)
        compare(mockCache.syncCount, 1)
        verify(mockCache.data.length > 0)
        first.requestFactory = function() { return response(500, "") }
        first.refresh()
        tryCompare(first, "loading", false, 5000)
        compare(mockCache.syncCount, 1)
        let second = createTemporaryObject(component, testCase,
                                           {persistenceEnabled: true, cacheBackend: mockCache})
        tryCompare(second, "entries", second.entries, 100)
        tryVerify(function() { return second.entries.length === 2 }, 5000)
        verify(second.statusText.indexOf("Saved calendar") >= 0)
    }
    function test_visible_calendar() {
        let view = createTemporaryObject(component, testCase)
        view.requestFactory = function() { return response(200, validFeed) }
        view.todayUtc = "2026-11-12"
        view.open()
        tryVerify(function() { return view.opened })
        let refresh = findChild(view.contentItem, "dxCalendarRefresh")
        verify(refresh !== null)
        mouseClick(refresh, refresh.width / 2, refresh.height / 2)
        let list = findChild(view.contentItem, "dxCalendarList")
        tryCompare(list, "count", 2)
        let search = findChild(view.contentItem, "dxCalendarSearch")
        search.text = "ZZ0ABC"
        tryCompare(list, "count", 1)
        view.close()
    }
    function test_large_feed_does_not_stop_ui_timer() {
        let view = createTemporaryObject(component, testCase)
        let event = "BEGIN:VEVENT\nSUMMARY:Example (ZZ0ABC)\nDTSTART;VALUE=DATE:20261110\nDTEND;VALUE=DATE:20261112\nEND:VEVENT\n"
        let largeFeed = "BEGIN:VCALENDAR\n" + Array(12001).join(event) + "END:VCALENDAR\n"
        view.requestFactory = function() { return response(200, largeFeed) }
        heartbeat.watched = view
        heartbeat.ticksDuringLoad = 0
        heartbeat.start()
        view.refresh()
        tryCompare(view, "loading", false, 15000)
        heartbeat.stop()
        compare(view.entries.length, 12000)
        verify(heartbeat.ticksDuringLoad > 0, "UI timer stopped during parsing")
    }
    function test_baseline_synchronous_parser_cannot_deliver_timer() {
        let event = "BEGIN:VEVENT\nSUMMARY:Example (ZZ0ABC)\nDTSTART;VALUE=DATE:20261110\nDTEND;VALUE=DATE:20261112\nEND:VEVENT\n"
        let largeFeed = "BEGIN:VCALENDAR\n" + Array(12001).join(event) + "END:VCALENDAR\n"
        let fakeLoading = {loading:true}
        heartbeat.watched = fakeLoading
        heartbeat.ticksDuringLoad = 0
        heartbeat.start()
        let rows = Calendar.parse(largeFeed)
        compare(rows.length, 12000)
        compare(heartbeat.ticksDuringLoad, 0)
        heartbeat.stop()
    }
}
