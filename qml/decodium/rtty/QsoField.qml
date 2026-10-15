// DecoRTTY — a labelled entry field for the QSO details macros expand from.
import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string label: ""
    property alias  text: field.text
    property bool   highlight: false

    signal edited(string value)

    implicitWidth: 110
    implicitHeight: 42

    Text {
        id: caption
        text: root.label
        color: Theme.textSecondary
        font.pixelSize: 8
        font.letterSpacing: 1.1
        font.bold: true
    }

    Rectangle {
        anchors.top: caption.bottom
        anchors.topMargin: 2
        width: parent.width
        height: 26
        radius: 6
        color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.85)
        border.width: 1
        border.color: field.activeFocus ? Theme.secondary
                                        : (root.highlight ? Theme.primary : Theme.glassBorder)

        TextInput {
            id: field
        // Ritagliato al proprio riquadro: senza questo un testo piu' lungo
        // del campo continua a disegnarsi oltre il bordo, sopra quello che
        // gli sta accanto.
        clip: true
            anchors.fill: parent
            anchors.margins: 6
            verticalAlignment: TextInput.AlignVCenter
            color: root.highlight ? Theme.accent : Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: 13
            selectByMouse: true
            selectionColor: Theme.primary

            onTextEdited: root.edited(text.toUpperCase())
            onEditingFinished: text = text.toUpperCase()
        }
    }
}
