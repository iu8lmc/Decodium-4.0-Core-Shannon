import QtQuick
import QtQuick.Layouts

Rectangle {
    id: strip

    implicitHeight: 30
    color: RotorTheme.bgHeader

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: RotorTheme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: RotorTheme.padding
        anchors.rightMargin: RotorTheme.padding
        spacing: 20

        Text {
            text: rotor.errorText.length > 0 ? rotor.errorText : qsTr("ready")
            color: rotor.errorText.length > 0 ? RotorTheme.danger : RotorTheme.textDim
            font.pixelSize: RotorTheme.fontSmall
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Repeater {
            model: [
                { caption: qsTr("app"), value: rotor.wsEndpoint },
                { caption: qsTr("web"), value: rotor.httpEndpoint },
                { caption: qsTr("rotctld"), value: rotor.rotctldEndpoint }
            ]

            Text {
                required property var modelData

                text: qsTr("%1 %2").arg(modelData.caption).arg(modelData.value)
                color: RotorTheme.textDim
                font.pixelSize: RotorTheme.fontSmall
                font.family: RotorTheme.monoFamily
            }
        }
    }
}
