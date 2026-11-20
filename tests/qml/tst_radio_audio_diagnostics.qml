import QtQuick
import QtTest
import "../../qml/decodium/components"

TestCase {
    id: testCase
    name: "RadioAudioDiagnostics"
    when: windowShown
    width: 600; height: 500
    QtObject {
        id: mockBridge
        property bool catConnected: false
        property bool monitoring: false
        property string catBackend: "tci"
        property QtObject catManager: QtObject {
            property string portType: "tci"
            property string tciPort: "127.0.0.1:50001"
            property string rigName: "TCI Client RX1"
            property bool tciAudioEnabled: false
        }
    }
    Component { id: component; RadioAudioDiagnostics { width: 580; bridge: mockBridge } }
    function test_states() {
        mockBridge.catConnected = false
        mockBridge.monitoring = false
        mockBridge.catManager.tciAudioEnabled = false
        let view = createTemporaryObject(component, testCase)
        let button = findChild(view, "radioAudioHelpButton")
        let text = findChild(view, "radioAudioHelpText")
        button.clicked()
        compare(mockBridge.catConnected, false)
        compare(mockBridge.catManager.tciAudioEnabled, false)
        compare(mockBridge.monitoring, false)
        verify(text.text.indexOf("not connected") >= 0)
        verify(text.text.indexOf("TCI audio: disabled") >= 0)
        verify(text.text.indexOf("127.0.0.1:50001") >= 0)
        verify(text.text.indexOf("TCI Client RX1") >= 0)
        verify(text.text.indexOf("MON is off") >= 0)
        mockBridge.catConnected = true
        mockBridge.catManager.tciAudioEnabled = true
        mockBridge.monitoring = true
        verify(text.text.indexOf("connection: connected") >= 0)
        verify(text.text.indexOf("TCI audio: enabled") >= 0)
        verify(text.text.indexOf("MON is on") >= 0)
        mockBridge.catBackend = "hamlib"
        verify(text.text.indexOf("TCI audio: enabled") >= 0)
        mockBridge.catManager.portType = "serial"
        verify(text.text.indexOf("Audio source") >= 0)
        verify(text.text.indexOf("TCI audio:") < 0)
        verify(text.text.indexOf("3. MON is on") >= 0)
    }
}
