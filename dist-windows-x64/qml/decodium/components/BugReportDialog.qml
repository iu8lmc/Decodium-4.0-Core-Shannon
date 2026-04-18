import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: bugDialog
    title: "Decodium — Bug Report"
    modal: true
    width: 700
    height: 580
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton

    Material.theme: Material.Dark
    Material.accent: Material.Orange

    property color bgDeep: "#1a1a2e"
    property color accentOrange: "#ff7814"

    Component.onCompleted: {
        if (bridge.diagnostics) bridge.diagnostics.analyzeErrors()
    }

    background: Rectangle {
        color: bgDeep
        radius: 12
        border.color: accentOrange
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Header
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Report Bug"
                font.pixelSize: 20
                font.bold: true
                color: accentOrange
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: bridge.diagnostics && bridge.diagnostics.errorCount > 0
                text: bridge.diagnostics ? bridge.diagnostics.errorCount + " errors detected" : ""
                color: "#ff4444"
                font.pixelSize: 13
            }
        }

        // Auto-analysis box
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: analysisText.implicitHeight + 16
            Layout.maximumHeight: 100
            visible: bridge.diagnostics && bridge.diagnostics.knownBugMatch !== ""
            color: "#2a2a3e"
            radius: 6

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                Label {
                    id: analysisText
                    text: bridge.diagnostics ? bridge.diagnostics.knownBugMatch : ""
                    color: "#aaddaa"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    width: parent.width
                }
            }
        }

        // User description
        Label {
            text: "Describe the problem:"
            color: "white"
            font.pixelSize: 14
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            TextArea {
                id: descriptionField
                placeholderText: "What happened? What did you expect? Steps to reproduce..."
                color: "white"
                font.pixelSize: 13
                wrapMode: TextArea.Wrap
                background: Rectangle { color: "#2a2a3e"; radius: 4 }
            }
        }

        // Diagnostic preview
        Label {
            text: "Diagnostic info (auto-collected):"
            color: "#888"
            font.pixelSize: 12
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: diagnosticPreview
                text: bridge.diagnostics ? bridge.diagnostics.systemInfo
                      + "\n\n--- Audio ---\n" + bridge.diagnostics.audioState
                      + "\n\n--- Rig ---\n" + bridge.diagnostics.rigState
                      + (bridge.diagnostics.errorCount > 0
                         ? "\n\n--- QML Errors ---\n" + bridge.diagnostics.qmlErrors
                         : "")
                      : "Diagnostics not available"
                readOnly: true
                color: "#999"
                font.pixelSize: 11
                font.family: "Consolas"
                wrapMode: TextArea.Wrap
                background: Rectangle { color: "#1e1e2e"; radius: 4 }
            }
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: "Copy Report"
                Material.background: "#333"
                onClicked: {
                    var report = bridge.diagnostics.buildReport(descriptionField.text)
                    bridge.copyToClipboard(report)
                    copyLabel.visible = true
                    copyTimer.restart()
                }
            }
            Label {
                id: copyLabel
                text: "Copied!"
                color: accentOrange
                visible: false
                Timer { id: copyTimer; interval: 2000; onTriggered: copyLabel.visible = false }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: bridge.diagnostics && bridge.diagnostics.submitting ? "Sending..." : "Send to GitHub"
                enabled: descriptionField.text.length > 10
                         && !(bridge.diagnostics && bridge.diagnostics.submitting)
                Material.background: accentOrange
                onClicked: {
                    var title = "[User Report] " + descriptionField.text.substring(0, 60)
                    var body = bridge.diagnostics.buildReport(descriptionField.text)
                    bridge.diagnostics.submitToGitHub(title, body)
                }
            }

            Button {
                text: "Close"
                Material.background: "#444"
                onClicked: bugDialog.close()
            }
        }

        // Submit result
        Label {
            visible: bridge.diagnostics && bridge.diagnostics.lastSubmitResult !== ""
            text: bridge.diagnostics ? bridge.diagnostics.lastSubmitResult : ""
            color: text.indexOf("Issue created") >= 0 ? "#44ff44" : "#ff8844"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Connections {
        target: bridge.diagnostics
        function onSubmitFinished(success, url) {
            if (success) {
                descriptionField.text = ""
            }
        }
    }
}
