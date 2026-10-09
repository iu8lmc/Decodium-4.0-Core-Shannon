import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    readonly property var bridge: appEngine
    Material.theme: Material.Dark
    Material.accent: "#44d4df"
    Material.primary: "#182632"
    title: qsTr("SSB — PC microphone")
    width: 780; height: 850; minimumWidth: 640; minimumHeight: 650
    color: "#101923"
    palette.windowText: "#eaf1f8"
    palette.text: "#eaf1f8"
    palette.buttonText: "#eaf1f8"
    palette.button: "#263747"
    palette.base: "#182632"
    palette.highlight: "#17a6bb"
    property var voice: ssbModule
    property var controls: bridge.ssbControls
    property bool ready: bridge.mode === "SSB" && bridge.catConnected
    function updateRadioMode() { bridge.ssbSetRadioMode(dataInput.checked ? (side.currentIndex ? "DIGL" : "DIGU") : (side.currentIndex ? "LSB" : "USB")) }
    onClosing: voice.stop()
    onActiveChanged: if (!active) voice.stop()
    Component.onDestruction: voice.stop()
    onVisibleChanged: if (visible) bridge.ssbRefreshControls()
    Shortcut { sequence: "Escape"; onActivated: voice.stop() }
    Connections {
        target: bridge
        function onCatConnectedChanged() { if (!bridge.catConnected) voice.stop(); if(win.visible) bridge.ssbRefreshControls() }
        function onModeChanged() { if (bridge.mode !== "SSB") voice.stop() }
        function onDecoPortUseRemoteChanged() { voice.stop(); bridge.ssbRefreshControls() }
    }
    footer: Frame {
        background: Rectangle { color: "#101923" }
        padding: 14
        ColumnLayout {
            anchors.fill: parent
        Button {
            Layout.fillWidth: true; Layout.preferredHeight: 64
            text: voice.transmitting ? qsTr("TRANSMITTING — release to receive") : qsTr("HOLD TO TALK • PTT")
            font.bold: true; font.pixelSize: 20
            enabled: ready && controls.busy !== true && mic.currentIndex>=0
            onPressed: voice.start()
            onReleased: voice.stop()
            onCanceled: voice.stop()
            background: Rectangle { radius: 8; color: voice.transmitting ? "#b53238" : "#087f92"; opacity: parent.enabled ? 1 : .45 }
        }
        Label { text: voice.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
        }
    }
    ScrollView {
        anchors.fill: parent; anchors.margins: 18
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 14
            RowLayout {
                Label { text: "SSB"; font.pixelSize: 30; font.bold: true; color: "#44d4df" }
                Label { text: bridge.decoPortUseRemote ? qsTr("REMOTE RADIO") : qsTr("LOCAL RADIO"); Layout.fillWidth: true }
                Label { text: bridge.catConnected ? bridge.catMode : qsTr("CAT disconnected"); color: bridge.catConnected ? "#77e7a9" : "#ffa971" }
            }
            RowLayout {
                TextField {
                    id: dial; Layout.fillWidth: true; font.pixelSize: 24; font.family: "Consolas"
                    text: (bridge.frequency / 1000000).toFixed(6)
                    enabled: ready && controls.busy !== true && !voice.transmitting
                    onAccepted: { bridge.cwTuneFrequency(Number(text.replace(",","."))*1000000); focus=false }
                    ToolTip.visible: hovered; ToolTip.text: qsTr("Frequency in MHz. Press Enter to tune.")
                }
                Label { text: "MHz" }
                Button { text: "−100 Hz"; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.cwTuneFrequency(bridge.frequency-100) }
                Button { text: "+100 Hz"; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.cwTuneFrequency(bridge.frequency+100) }
            }
            RowLayout {
                ComboBox { id: side; model: ["USB", "LSB"]; enabled: ready && controls.busy !== true && !voice.transmitting; onActivated: win.updateRadioMode() }
                CheckBox { id: dataInput; text: qsTr("USB/data audio input"); checked: true; enabled: ready && controls.busy !== true && !voice.transmitting; onToggled: if(win.ready) win.updateRadioMode() }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Activate SSB"); visible: bridge.mode !== "SSB"; onClicked: bridge.mode="SSB" }
            }
            Label { text: qsTr("TX uses the radio audio output selected in Settings. USB/LSB requires the radio to accept PC audio; use data input when needed."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9" }
            GroupBox {
                title: qsTr("PC microphone • voice processing"); Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    ComboBox {
                        id: mic; Layout.fillWidth: true; model: voice.microphones; textRole: "name"; valueRole: "id"
                        enabled: !voice.transmitting
                        currentIndex: { for(var i=0;i<voice.microphones.length;i++)if(voice.microphones[i].id===voice.microphone)return i;return -1 }
                        displayText: currentIndex < 0 ? qsTr("Select PC microphone…") : currentText
                        onActivated: voice.microphone=currentValue
                    }
                    RowLayout {
                        Label { text: qsTr("Mic gain") }
                        Slider { Layout.fillWidth: true; from: -20; to: 30; stepSize: 1; value: voice.gainDb; onMoved: voice.gainDb=value }
                        Label { text: voice.gainDb.toFixed(0)+" dB"; Layout.preferredWidth: 50 }
                        CheckBox { text: qsTr("Automatic gain"); checked: voice.automatic; onToggled: voice.automatic=checked }
                    }
                    RowLayout {
                        Label { text: qsTr("Voice filter") }
                        SpinBox { from: 80; to: 500; stepSize: 20; value: voice.lowCut; onValueModified: voice.lowCut=value }
                        Label { text: "–" }
                        SpinBox { from: 1800; to: 3500; stepSize: 100; value: voice.highCut; onValueModified: voice.highCut=value }
                        Label { text: qsTr("Hz • limiter active"); Layout.fillWidth: true }
                    }
                    RowLayout {
                        Label { text: qsTr("Voice level") }
                        ProgressBar { from: 0; to: 1; value: voice.micPeak; Layout.fillWidth: true }
                        Label { text: voice.micPeak>.001 ? (20*Math.log(voice.micPeak)/Math.LN10).toFixed(0)+" dBFS" : "—"; Layout.preferredWidth: 65 }
                    }
                }
            }
            GroupBox {
                title: qsTr("Radio • CAT controls"); Layout.fillWidth: true
                GridLayout {
                    anchors.fill: parent; columns: 4; columnSpacing: 12
                    Label { text: qsTr("RF power %") }
                    SpinBox { from: 0; to: 100; value: controls.power === undefined ? 0 : Math.round(controls.power); enabled: ready && controls.busy !== true && !voice.transmitting && controls.powerSupported === true; onValueModified: bridge.ssbSetControl("power",value) }
                    Label { text: qsTr("RX filter Hz") }
                    SpinBox { from: 300; to: 4000; stepSize: 100; value: controls.filter === undefined ? 2400 : controls.filter; enabled: ready && controls.busy !== true && !voice.transmitting && controls.filterSupported === true; onValueModified: bridge.ssbSetControl("filter",value) }
                    Label { text: qsTr("Radio AGC") }
                    ComboBox {
                        model: controls.agcModes || []; textRole: "name"; valueRole: "value"
                        enabled: ready && controls.busy !== true && !voice.transmitting && controls.agcSupported === true
                        currentIndex: { var a=controls.agcModes || [];for(var i=0;i<a.length;i++)if(a[i].value===controls.agc)return i;return -1 }
                        onActivated: bridge.ssbSetControl("agc",currentValue)
                    }
                    Label { text: qsTr("RF gain %") }
                    SpinBox { from: 0; to: 100; value: controls.rf === undefined ? 0 : Math.round(controls.rf); enabled: ready && controls.busy !== true && !voice.transmitting && controls.rfSupported === true; onValueModified: bridge.ssbSetControl("rf",value) }
                    Label { text: qsTr("Radio mic gain %") }
                    SpinBox { from: 0; to: 100; value: controls.mic === undefined ? 0 : Math.round(controls.mic); enabled: ready && controls.busy !== true && !voice.transmitting && controls.micSupported === true; onValueModified: bridge.ssbSetControl("mic",value) }
                    Button { text: qsTr("Read radio"); Layout.columnSpan: 2; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.ssbRefreshControls() }
                    Label { text: qsTr("AGC OFF enables manual reception gain using RF gain. Unavailable controls are disabled."); Layout.columnSpan: 4; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                    Label { text: controls.error || ""; visible: text.length>0; color: "#ffa971"; Layout.columnSpan: 4; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            RowLayout {
                Label { text: "ALC: "+(bridge.rigAlcValid ? Math.round(bridge.rigAlc)+" %" : "—"); font.pixelSize: 18 }
                Label { text: qsTr("RF: ")+(bridge.rigPowerWatts > 0 ? bridge.rigPowerWatts.toFixed(1)+" W" : "—"); Layout.fillWidth: true; font.pixelSize: 18 }
                Label { text: qsTr("SWR: ")+(bridge.rigSwr >= 1 ? bridge.rigSwr.toFixed(1) : "—"); font.pixelSize: 18 }
            }
            RowLayout {
                CheckBox { text: qsTr("Listen RX"); checked: bridge.decoPortMonitor; enabled: !voice.transmitting; onToggled: { bridge.decoPortMonitor=checked; if(checked)bridge.startRx() } }
                Slider { from: 0; to: 100; stepSize: 1; value: bridge.remoteRxVolume; onMoved: bridge.remoteRxVolume=value; Layout.fillWidth: true }
                Label { text: bridge.remoteRxVolume+" %" }
            }
            ComboBox {
                Layout.fillWidth: true; model: bridge.cwMonitorOutputs(); textRole: "label"; valueRole: "id"
                enabled: !voice.transmitting
                currentIndex: { for(var i=0;i<model.length;i++)if(model[i].id===bridge.remoteRxDevice)return i;return -1 }
                onActivated: bridge.remoteRxDevice=currentValue
            }

        }
    }
}
