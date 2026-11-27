import QtQuick
import QtTest
import "../../qml/decodium/components/SettingsSearchIndex.js" as Search

TestCase {
    name: "SettingsSearch"
    function test_search() {
        compare(Search.find("").length, 0)
        compare(Search.find("zzzznosuchsetting").length, 0)
        let results = Search.find("tci audio")
        verify(results.some(function(item) { return item.tab === 1 && item.label.indexOf("TCI Audio") >= 0 }))
        verify(Search.find("TCI RX GAIN").some(function(item) { return item.tab === 1 }))
        verify(Search.find("call").length > 0)
    }
}
