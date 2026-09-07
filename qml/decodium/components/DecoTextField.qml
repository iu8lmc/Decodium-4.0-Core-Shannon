import QtQuick

FocusScope {
    id: control

    property alias text: input.text
    property alias readOnly: input.readOnly
    property alias validator: input.validator
    property alias inputMask: input.inputMask
    property alias maximumLength: input.maximumLength
    property alias echoMode: input.echoMode
    property alias cursorPosition: input.cursorPosition
    property alias selectedText: input.selectedText
    readonly property alias acceptableInput: input.acceptableInput

    property string placeholderText: ""
    property color placeholderTextColor: Qt.rgba(color.r, color.g, color.b, 0.45)
    property color color: "#ffffff"
    property color selectionColor: "#448aff"
    property color selectedTextColor: "#ffffff"
    property font font: Qt.font({ pixelSize: 12 })
    property int horizontalAlignment: TextInput.AlignLeft
    property int verticalAlignment: TextInput.AlignVCenter
    property int inputMethodHints: Qt.ImhNone
    property bool selectByMouse: true
    property string passwordCharacter: "*"
    property int passwordMaskDelay: 0
    property int leftPadding: 0
    property int rightPadding: 0
    property int topPadding: 0
    property int bottomPadding: 0
    property int leftInset: 0
    property int rightInset: 0
    property int topInset: 0
    property int bottomInset: 0
    property Item background: null
    readonly property bool hovered: hoverHandler.hovered

    signal editingFinished()
    signal accepted()
    signal textEdited()

    implicitWidth: Math.max(160, input.implicitWidth + leftPadding + rightPadding + leftInset + rightInset)
    implicitHeight: Math.max(32, input.implicitHeight + topPadding + bottomPadding + topInset + bottomInset)
    clip: true
    activeFocusOnTab: true

    readonly property string _stableFontFamily: {
        var family = String(control.font.family || "").trim()
        if (family.length > 0)
            return family
        return Qt.platform.os === "osx" ? "Helvetica Neue" : ""
    }

    onBackgroundChanged: {
        if (!background)
            return
        background.parent = control
        background.z = -1
        background.anchors.fill = control
        background.anchors.leftMargin = control.leftInset
        background.anchors.rightMargin = control.rightInset
        background.anchors.topMargin = control.topInset
        background.anchors.bottomMargin = control.bottomInset
    }

    onActiveFocusChanged: {
        if (activeFocus && !input.activeFocus)
            input.forceActiveFocus()
    }

    function selectAll() { input.selectAll() }
    function deselect() { input.deselect() }
    function clear() { input.clear() }
    function copy() { input.copy() }
    function cut() { input.cut() }
    function paste() { input.paste() }

    HoverHandler {
        id: hoverHandler
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: control.leftInset + control.leftPadding
        anchors.rightMargin: control.rightInset + control.rightPadding
        anchors.topMargin: control.topInset + control.topPadding
        anchors.bottomMargin: control.bottomInset + control.bottomPadding
        color: control.enabled ? control.color : Qt.rgba(control.color.r, control.color.g, control.color.b, 0.45)
        selectionColor: control.selectionColor
        selectedTextColor: control.selectedTextColor
        font.family: control._stableFontFamily
        font.pixelSize: control.font.pixelSize > 0 ? control.font.pixelSize : 12
        font.bold: control.font.bold
        font.italic: control.font.italic
        horizontalAlignment: control.horizontalAlignment
        verticalAlignment: control.verticalAlignment
        passwordCharacter: control.passwordCharacter
        passwordMaskDelay: control.passwordMaskDelay
        inputMethodHints: control.inputMethodHints | Qt.ImhNoPredictiveText
        selectByMouse: control.selectByMouse
        clip: true
        renderType: Text.QtRendering

        onEditingFinished: control.editingFinished()
        onAccepted: control.accepted()
        onTextEdited: control.textEdited()
    }

    Text {
        anchors.fill: input
        text: control.placeholderText
        visible: input.text.length === 0 && input.preeditText.length === 0 && control.placeholderText.length > 0
        color: control.placeholderTextColor
        font.family: input.font.family
        font.pixelSize: input.font.pixelSize
        font.bold: input.font.bold
        font.italic: input.font.italic
        horizontalAlignment: control.horizontalAlignment
        verticalAlignment: control.verticalAlignment
        elide: Text.ElideRight
        renderType: Text.QtRendering
    }
}
