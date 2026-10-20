import QtQuick
import "../../qml/decodium/components"

// The real SSB window with no audio devices, serial ports or radio backend.
QtObject {
    property QtObject appEngine: QtObject {
        property var themeManager: null
        property var hamlibCat: null
        property string mode: "SSB"
        property string catMode: "USB"
        property bool catConnected: true
        property bool decoPortUseRemote: false
        property real frequency: 7030000
        property bool rigAlcValid: false
        property real rigAlc: 0
        property real rigPowerWatts: 0
        property real rigSwr: 0
        property bool decoPortMonitor: false
        property int remoteRxVolume: 50
        property string remoteRxDevice: "test-output"
        property int ssbRxAgcMode: 0
        property real ssbRxAgcThreshold: -60
        property real ssbRxAgcGainDb: 0
        property int writes: 0
        property string lastKey: ""
        property real lastValue: -1
        property var ssbControls: ({
            busy: false, agcSupported: true, agc: 5,
            agcModes: [{name: "OFF", value: 0}, {name: "FAST (0.3 s)", value: 5},
                       {name: "MID (2.0 s)", value: 2}, {name: "SLOW (6.0 s)", value: 3}],
            powerSupported: true, power: 50, rfSupported: true, rf: 100,
            micSupported: true, mic: 60, filterSupported: true, filter: 3000
        })
        function ssbRefreshControls() {}
        function setAgcForTest(value) {
            var updated = Object.assign({}, ssbControls)
            updated.agc = value
            ssbControls = updated
        }
        function ssbSetControl(key, value) {
            writes++
            lastKey = key
            lastValue = value
            var updated = Object.assign({}, ssbControls)
            updated[key] = value
            // Exercise the actual UI bindings during an asynchronous CAT readback.
            ssbControls = {busy: true}
            Qt.callLater(function () { ssbControls = updated })
        }
        function ssbSetRadioMode(mode) { catMode = mode }
        function cwTuneFrequency(value) { frequency = value }
        function cwMonitorOutputs() { return [{label: "Test output", id: "test-output"}] }
        function startRx() {}
    }
    property QtObject ssbModule: QtObject {
        property bool transmitting: false
        property int startCalls: 0
        property int stopCalls: 0
        property var microphones: [{name: "Test microphone", id: "test-mic"}]
        property string microphone: "test-mic"
        property real gainDb: 0
        property bool automatic: true
        property int lowCut: 200
        property int highCut: 2800
        property real micPeak: 0
        property string status: "Receiving (test)"
        function start() { startCalls++; transmitting = true }
        function stop() { stopCalls++; transmitting = false }
    }
    property QtObject voiceKeyer: QtObject {
        property var messages: []
        property real speed: 1
        property bool playing: false
        property bool recording: false
        property bool synthesizing: false
        property string status: ""
        function stop() {}
    }
    property QtObject assistant: QtObject {
        property bool busy: false
        property string transcript: ""
        property string lastReply: ""
        property string status: ""
        signal changed()
    }
    property QtObject packs: QtObject {
        property bool voiceInstalled: false
        property bool speechInstalled: false
        property bool assistantInstalled: false
        property bool busy: false
        property real progress: 0
        property string status: ""
        function isEnabled(key) { return false }
    }
    property QtObject speech: QtObject {
        property bool working: false
        property string transcript: ""
        property string status: ""
    }
    property SsbWindow window: SsbWindow {
        width: 1800
        height: 1800
    }
}
