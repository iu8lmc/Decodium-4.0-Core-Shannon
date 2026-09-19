import QtQuick
import QtTest
import "../../qml/decodium/components"

TestCase {
    name: "SuperFoxToggle"
    QtObject {
        id: backend
        property string mode: "FT8"
        property bool houndMode: false
        property bool foxMode: false
        property bool transmitting: false
        property bool tuning: false
        property bool option: false
        property int saves: 0
        property var optionWritesWhileHound: []
        signal settingValueChanged(string key, var value)
        function getSetting(key, fallback) { return option }
        function setSetting(key, value) {
            optionWritesWhileHound.push(houndMode)
            option = value
            settingValueChanged(key, value)
        }
        function saveSettingsAsync() { saves++ }
    }
    SuperFoxIndicator { id: control; engine: backend }
    function init() {
        backend.mode = "FT8"
        backend.houndMode = false
        backend.foxMode = false
        backend.transmitting = false
        backend.tuning = false
        backend.setSetting("SuperFox", false)
        backend.saves = 0
        backend.optionWritesWhileHound = []
    }
    function test_repeatedToggle() {
        for (var i = 0; i < 3; ++i) {
            control.toggleSuperFox()
            verify(control.active)
            verify(backend.houndMode)
            control.toggleSuperFox()
            verify(!control.active)
            verify(!backend.houndMode)
            verify(!backend.option)
        }
        compare(backend.saves, 0)
        // Each option write occurs outside Hound, avoiding an extra FT8 reset.
        compare(backend.optionWritesWhileHound.length, 6)
        for (var j = 0; j < 6; ++j)
            compare(backend.optionWritesWhileHound[j], false)
    }
    function test_preserveFox() {
        backend.foxMode = true
        control.toggleSuperFox()
        verify(control.active)
        control.toggleSuperFox()
        verify(backend.foxMode)
        verify(!backend.houndMode)
    }
    function test_noChangeDuringTxOrTune() {
        backend.transmitting = true
        control.toggleSuperFox()
        verify(!backend.option)
        backend.transmitting = false
        backend.tuning = true
        control.toggleSuperFox()
        verify(!backend.option)
    }
    function test_noImplicitModeChange() {
        backend.mode = "FT4"
        control.toggleSuperFox()
        compare(backend.mode, "FT4")
        verify(!backend.option)
    }
}
