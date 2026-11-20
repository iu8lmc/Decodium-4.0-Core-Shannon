import QtQuick
import QtQuick.Controls

Column {
    id: root
    required property var bridge
    property color textColor: "white"
    property color accentColor: "cyan"
    spacing: 6
    Button {
        id: radioAudioHelpButton
        objectName: "radioAudioHelpButton"
        text: qsTranslate("SettingsTab1", "Radio and audio troubleshooting")
        onClicked: radioAudioHelp.visible = !radioAudioHelp.visible
    }
    Rectangle {
        id: radioAudioHelp
        objectName: "radioAudioHelp"
        property bool isTci: !!bridge && !!bridge.catManager && (bridge.catBackend === "tci" || String(bridge.catManager.portType).toLowerCase() === "tci")
        visible: false
        width: root.width
        implicitHeight: radioAudioHelpText.implicitHeight + 24
        radius: 5
        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.06)
        border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.35)
        Text {
            id: radioAudioHelpText
            objectName: "radioAudioHelpText"
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 10
            }
            color: root.textColor
            font.pixelSize: 12
            wrapMode: Text.Wrap
            text: {
                if (!bridge)
                    return qsTranslate("SettingsTab1", "Diagnostics are not available.");
                var lines = [];
                lines.push(bridge.catConnected ? qsTranslate("SettingsTab1", "1. Radio connection: connected. This does not prove audio is arriving.") : qsTranslate("SettingsTab1", "1. Radio connection: not connected. Check the backend, address or port, then connect."));
                if (radioAudioHelp.isTci) {
                    lines.push(bridge.catManager.tciAudioEnabled ? qsTranslate("SettingsTab1", "2. TCI audio: enabled. Check that the TCI server and selected RX1/RX2 match the receiver in use.") : qsTranslate("SettingsTab1", "2. TCI audio: disabled. Enable RX/TX via TCI if the radio should supply audio over TCI."));
                    lines.push(qsTranslate("SettingsTab1", "3. Check TCI Host:Port and TCI RX gain below. Match the address and port to the server configuration."));
                    if (bridge.catManager.tciPort !== undefined && bridge.catManager.rigName !== undefined) {
                        lines.push(qsTranslate("RadioAudioDiagnostics", "Configured TCI: %1 · receiver: %2").arg(String(bridge.catManager.tciPort || "—")).arg(String(bridge.catManager.rigName || "—")));
                    }
                } else {
                    lines.push(qsTranslate("SettingsTab1", "2. Audio source: check the input device and operating-system audio permissions in Audio settings."));
                }
                lines.push((radioAudioHelp.isTci ? "4. " : "3. ") + (bridge.monitoring ? qsTranslate("RadioAudioDiagnostics", "MON is on. Check whether the RX level and waterfall respond to a signal; a quiet band alone does not indicate a fault.") : qsTranslate("RadioAudioDiagnostics", "MON is off. Turn on MON to receive and check the RX level and waterfall.")));
                lines.push(qsTranslate("SettingsTab1", "These checks read the current state; they do not change radio or audio settings."));
                return lines.join("\n\n");
            }
        }
    }
}
