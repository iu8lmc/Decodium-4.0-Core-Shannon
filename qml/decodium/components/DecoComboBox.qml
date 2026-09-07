import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    property color decoTextColor: typeof bridge !== "undefined" && bridge && bridge.themeManager
                                  ? bridge.themeManager.textPrimary : "#e8f4fd"
    property color decoMutedTextColor: typeof bridge !== "undefined" && bridge && bridge.themeManager
                                       ? bridge.themeManager.textSecondary : "#89b4d0"
    property color decoBackgroundColor: typeof bridge !== "undefined" && bridge && bridge.themeManager
                                        ? bridge.themeManager.bgMedium : "#111827"
    property color decoPopupColor: typeof bridge !== "undefined" && bridge && bridge.themeManager
                                   ? bridge.themeManager.bgDeep : "#0b1220"
    property color decoBorderColor: typeof bridge !== "undefined" && bridge && bridge.themeManager
                                    ? bridge.themeManager.glassBorder : "#244768"
    property color decoAccentColor: typeof bridge !== "undefined" && bridge && bridge.themeManager
                                    ? bridge.themeManager.secondaryColor : "#00bcd4"

    readonly property string _stableFontFamily: {
        var family = String(control.font.family || "").trim()
        if (family.length > 0)
            return family
        return Qt.platform.os === "osx" ? "Helvetica Neue" : ""
    }
    readonly property int _stablePixelSize: control.font.pixelSize > 0 ? control.font.pixelSize : 12

    function optionText(optionModel) {
        if (optionModel === undefined || optionModel === null)
            return ""
        // 1.0.352 fix: model di stringhe/numeri semplici (es. model: ["A","B"]) —
        // senza questo fallback nessun ramo matcha e il popup mostra item vuoti.
        if (typeof optionModel !== "object")
            return String(optionModel)
        if (control.textRole && optionModel[control.textRole] !== undefined)
            return String(optionModel[control.textRole])
        if (optionModel.text !== undefined)
            return String(optionModel.text)
        if (optionModel.label !== undefined)
            return String(optionModel.label)
        if (optionModel.name !== undefined)
            return String(optionModel.name)
        if (optionModel.modelData !== undefined) {
            var data = optionModel.modelData
            if (data === undefined || data === null)
                return ""
            if (typeof data === "object") {
                if (control.textRole && data[control.textRole] !== undefined)
                    return String(data[control.textRole])
                if (data.text !== undefined)
                    return String(data.text)
                if (data.label !== undefined)
                    return String(data.label)
                if (data.name !== undefined)
                    return String(data.name)
                return ""
            }
            return String(data)
        }
        return ""
    }

    implicitWidth: Math.max(120, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(34, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: 10
    rightPadding: 34
    topPadding: 4
    bottomPadding: 4

    contentItem: Item {
        implicitWidth: Math.max(displayTextItem.implicitWidth, editTextItem.implicitWidth) + control.leftPadding + control.rightPadding
        implicitHeight: Math.max(displayTextItem.implicitHeight, editTextItem.implicitHeight)

        Text {
            id: displayTextItem
            visible: !control.editable
            anchors.fill: parent
            anchors.leftMargin: control.leftPadding
            anchors.rightMargin: control.rightPadding
            text: control.displayText
            color: control.enabled ? control.decoTextColor : control.decoMutedTextColor
            font.family: control._stableFontFamily
            font.pixelSize: control._stablePixelSize
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            renderType: Text.QtRendering
        }

        DecoTextField {
            id: editTextItem
            visible: control.editable
            anchors.fill: parent
            text: control.editText
            enabled: control.enabled
            readOnly: control.down
            color: control.enabled ? control.decoTextColor : control.decoMutedTextColor
            selectionColor: control.decoAccentColor
            selectedTextColor: control.decoPopupColor
            font.family: control._stableFontFamily
            font.pixelSize: control._stablePixelSize
            verticalAlignment: TextInput.AlignVCenter
            leftPadding: control.leftPadding
            rightPadding: control.rightPadding
            inputMethodHints: control.inputMethodHints
            validator: control.validator
            selectByMouse: control.selectTextByMouse
            onTextEdited: control.editText = text
        }
    }

    indicator: Text {
        x: control.width - width - 12
        y: Math.round((control.height - height) / 2)
        text: qsTr("\u25BE")
        color: control.enabled ? control.decoTextColor : control.decoMutedTextColor
        font.family: control._stableFontFamily
        font.pixelSize: Math.max(12, control._stablePixelSize)
        renderType: Text.QtRendering
    }

    background: Rectangle {
        color: control.decoBackgroundColor
        border.color: control.activeFocus || control.popup.visible ? control.decoAccentColor : control.decoBorderColor
        border.width: control.activeFocus || control.popup.visible ? 2 : 1
        radius: 4
    }

    delegate: ItemDelegate {
        id: delegateRoot
        required property int index
        required property var model

        width: ListView.view ? ListView.view.width : control.width
        height: Math.max(32, control._stablePixelSize + 18)
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: control.optionText(delegateRoot.model)
            color: delegateRoot.highlighted || control.currentIndex === delegateRoot.index ? "#ffffff" : control.decoTextColor
            font.family: control._stableFontFamily
            font.pixelSize: control._stablePixelSize
            font.bold: delegateRoot.highlighted || control.currentIndex === delegateRoot.index
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 10
            renderType: Text.QtRendering
        }

        background: Rectangle {
            color: delegateRoot.highlighted || control.currentIndex === delegateRoot.index
                   ? Qt.rgba(control.decoAccentColor.r, control.decoAccentColor.g, control.decoAccentColor.b, 0.34)
                   : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        height: Math.min(contentItem.implicitHeight + 8, 360)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar { }
        }

        background: Rectangle {
            color: control.decoPopupColor
            border.color: control.decoBorderColor
            radius: 4
        }
    }
}
