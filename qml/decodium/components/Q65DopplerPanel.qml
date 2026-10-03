import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    property var tracker: null
    property string stationGrid: ""
    property string dxGrid: ""
    property color textColor: "#e0edf7"
    property color accentColor: "#00c8df"
    readonly property var tracking: tracker ? tracker.snapshot : ({})
    signal dxGridEdited(string grid)
    implicitHeight: controls.implicitHeight + 24
    color: "#101c2b"
    border.color: accentColor
    radius: 8
    Material.theme: Material.Dark
    Material.accent: root.accentColor
    ColumnLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        Label {
            text: qsTr("Q65 — EME Doppler tracking")
            font.bold: true
            color: root.accentColor
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        ComboBox {
            objectName: "emeMethod"
            Layout.fillWidth: true
            model: [qsTr("Constant frequency on Moon (CFOM)"),
                    qsTr("Full Doppler to DX Grid"), qsTr("Own Echo (RX only)")]
            currentIndex: root.tracker ? root.tracker.method : 0
            enabled: root.tracker && !root.tracking.busy
            onActivated: if (root.tracker) root.tracker.method = currentIndex
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("My grid: %1").arg(root.stationGrid); color: root.textColor }
            TextField {
                objectName: "emeDxGrid"
                Layout.fillWidth: true
                visible: root.tracker && root.tracker.method === 1
                enabled: !root.tracking.busy
                placeholderText: qsTr("DX locator (6 characters)")
                text: root.dxGrid
                maximumLength: 8
                onEditingFinished: root.dxGridEdited(text.trim().toUpperCase())
            }
        }
        CheckBox {
            objectName: "emeEnabled"
            text: qsTr("Enable Doppler tracking")
            palette.windowText: root.textColor
            checked: root.tracker ? root.tracker.enabled : false
            enabled: root.tracker && (checked || (root.tracking.ready && !root.tracking.busy))
            onToggled: if (root.tracker) root.tracker.enabled = checked
        }
        Label {
            objectName: "emeStatus"
            Layout.fillWidth: true
            text: root.tracking.status || ""
            wrapMode: Text.WordWrap
            color: root.tracking.active ? "#00e89b" : root.textColor
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("RX correction: %1 Hz   TX correction: %2 Hz")
                  .arg(Number(root.tracking.rxCorrectionHz || 0).toFixed(0))
                  .arg(Number(root.tracking.txCorrectionHz || 0).toFixed(0))
            wrapMode: Text.WordWrap
            color: root.textColor
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Hamlib + Rig/Fake It split required. TX correction is held during transmission. Tracking starts OFF each session; coordinate the method with the other station.")
            wrapMode: Text.WordWrap
            color: root.textColor
            font.pixelSize: 11
        }
    }
}
