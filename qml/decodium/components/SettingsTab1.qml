/* Decodium 4.0 - lazy Settings tab */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SettingsPageScroll {
    property var dialog
    readonly property var bridge: dialog ? dialog.appBridge : null

    // Applica in un colpo solo lo stato dei quattro comandi della CAT
    // condivisa: il server apre o chiude la porta di conseguenza.
    // Applica in un colpo solo i comandi dell'amplificatore.
    function applyAmplifier() {
        if (!bridge) return
        var b = parseInt(ampBaud.text, 10)
        if (isNaN(b) || b < 300) b = 9600
        bridge.configureAmplifier(ampEnabled.checked, ampPort.text.trim(),
                                  b, ampPassive.checked, 500)
    }

    function applyCatShare() {
        if (!bridge) return
        var p = parseInt(catSharePort.text, 10)
        if (isNaN(p) || p < 1024 || p > 65535) p = 4533
        bridge.configureCatShare(catShareEnabled.checked, p,
                                 catShareControl.checked, catSharePtt.checked)
    }

    // Spot condivisi: come la CAT, ma per il cluster. Nessun permesso da
    // concedere, perche' non c'e' niente da comandare.
    function applySpotShare() {
        if (!bridge) return
        var p = parseInt(spotSharePort.text, 10)
        if (isNaN(p) || p < 1024 || p > 65535) p = 4534
        bridge.configureSpotShare(spotShareEnabled.checked, p)
    }
    readonly property bool compactSettingsLayout: dialog ? dialog.compactSettingsLayout : false
    readonly property bool narrowSettingsLayout: dialog ? dialog.narrowSettingsLayout : false
    readonly property int pageColumns: compactSettingsLayout ? 2 : 4
    readonly property int labelWidth: dialog ? dialog.labelWidth : 120
    readonly property int fieldMinWidth: dialog ? dialog.fieldMinWidth : 180
    readonly property int wideFieldMinWidth: dialog ? dialog.wideFieldMinWidth : 260
    readonly property int portFieldMinWidth: dialog ? dialog.portFieldMinWidth : 140
    readonly property int numericFieldMinWidth: dialog ? dialog.numericFieldMinWidth : 120
    readonly property int comboFieldMinWidth: dialog ? dialog.comboFieldMinWidth : 180
    readonly property int frequencyPageMinWidth: dialog ? dialog.frequencyPageMinWidth : 760
    readonly property int scrollLeftMargin: dialog ? dialog.scrollLeftMargin : 10
    readonly property int scrollTopMargin: dialog ? dialog.scrollTopMargin : 10
    readonly property int scrollRightMargin: dialog ? dialog.scrollRightMargin : 12
    readonly property int scrollBottomMargin: dialog ? dialog.scrollBottomMargin : 96
    pageLeftMargin: scrollLeftMargin
    pageTopMargin: scrollTopMargin
    pageRightMargin: scrollRightMargin
    pageBottomMargin: scrollBottomMargin
    minimumContentWidth: dialog ? dialog.settingsPageMinimumContentWidth(pageColumns) : 0
    readonly property color bgDeep: dialog ? dialog.bgDeep : "#080b12"
    readonly property color bgMedium: dialog ? dialog.bgMedium : "#101722"
    readonly property color bgLight: dialog ? dialog.bgLight : "#1a2433"
    readonly property color bgDark: dialog ? dialog.bgDark : "#080b12"
    readonly property color primaryBlue: dialog ? dialog.primaryBlue : "#3ba7ff"
    readonly property color secondaryCyan: dialog ? dialog.secondaryCyan : "#00d9ff"
    readonly property color accentGreen: dialog ? dialog.accentGreen : "#00f08b"
    readonly property color textPrimary: dialog ? dialog.textPrimary : "#f2f5f7"
    readonly property color textSecondary: dialog ? dialog.textSecondary : "#a7b2c0"
    readonly property color textDim: dialog ? dialog.textDim : "#667180"
    readonly property color glassBorder: dialog ? dialog.glassBorder : "#334455"
    readonly property int controlHeight: dialog ? dialog.controlHeight : 32
    readonly property int controlFontSize: dialog ? dialog.controlFontSize : 12
    readonly property int controlVerticalPadding: dialog ? dialog.controlVerticalPadding : 0
    readonly property int spinTextSidePadding: dialog ? dialog.spinTextSidePadding : 52

    function boolSetting(key, fallback) {
        return dialog ? dialog.boolSetting(key, fallback) : !!fallback
    }

    function setBoolSettingIfChanged(key, value, fallback) {
        if (dialog)
            dialog.setBoolSettingIfChanged(key, value, fallback)
    }

    function selectedCatProfileName() {
        if (catProfileCombo.currentIndex >= 0)
            return String(catProfileCombo.currentText || "").trim()
        return String(bridge.activeCatProfile || "").trim()
    }

    function nextCatProfileName() {
        var base = String(bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : "Radio Profile").trim()
        if (base.length === 0)
            base = "Radio Profile"
        var profiles = bridge.catProfileList || []
        var exists = function(name) {
            for (var i = 0; i < profiles.length; ++i) {
                if (String(profiles[i]).trim().toLowerCase() === String(name).trim().toLowerCase())
                    return true
            }
            return false
        }
        if (!exists(base))
            return base
        for (var n = 2; n < 100; ++n) {
            var candidate = base + " " + n
            if (!exists(candidate))
                return candidate
        }
        return base + " " + Date.now()
    }

    function refreshCatProfileDraft() {
        var active = String(bridge.activeCatProfile || "").trim()
        if (active.length > 0)
            catProfileNameField.text = active
        else if (String(catProfileNameField.text || "").trim().length === 0)
            catProfileNameField.text = bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : ""
    }

    function saveCatProfileFromField() {
        var name = String(catProfileNameField.text || "").trim()
        if (name.length === 0)
            name = nextCatProfileName()
        if (bridge.saveCatProfile(name))
            catProfileNameField.text = String(bridge.activeCatProfile || name)
    }

    function saveNewCatProfileFromCurrent() {
        catProfileNameField.text = nextCatProfileName()
        saveCatProfileFromField()
    }

    function loadSelectedCatProfile() {
        var name = selectedCatProfileName()
        if (name.length === 0)
            return
        if (bridge.loadCatProfile(name)) {
            catProfileNameField.text = String(bridge.activeCatProfile || name)
            dialog.refreshCatPorts()
        }
    }

    function deleteSelectedCatProfile() {
        var name = selectedCatProfileName()
        if (name.length === 0)
            return
        if (bridge.deleteCatProfile(name))
            catProfileNameField.text = bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : ""
    }
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    GridLayout {
        width: Math.max(0, parent.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        columns: pageColumns; columnSpacing: 10; rowSpacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.rightMargin: dialog.scrollRightMargin
        anchors.topMargin: dialog.scrollTopMargin

        // ── Backend CAT ──
        Text { text: qsTr("BACKEND CAT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Backend:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        Row {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 6
            Repeater {
                model: [["native",qsTr("Native (15 radios)")],["hamlib",qsTr("Hamlib (300+ radios)")],["tci","TCI"],["omnirig","OmniRig"],["cat4om","Cat4OM"]]
                delegate: Rectangle {
                    property string bk: modelData[0]
                    property bool active: bridge.catBackend === bk
                    property bool catBusy: dialog.catConnectionInProgress()
                    width: 170; height: 30; radius: 6
                    color: active ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (catBusy ? Qt.rgba(1,1,1,0.025) : (bkMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent"))
                    border.color: active ? primaryBlue : glassBorder
                    Text { anchors.centerIn: parent; text: modelData[1]; color: active ? primaryBlue : (catBusy ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.55) : textSecondary); font.pixelSize: 11 }
                    MouseArea { id: bkMA; anchors.fill: parent; hoverEnabled: true; enabled: !parent.catBusy; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            bridge.catBackend = bk
                            if (bk === "tci")
                                dialog.selectTciRigIfNeeded()
                            dialog.scheduleCatPersist()
                        }
                    }
                }
            }
        }

        Text { text: qsTr("Profile:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        RowLayout {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 6

            DecoComboBox {
                id: catProfileCombo
                model: bridge.catProfileList || []
                Layout.preferredWidth: compactSettingsLayout ? 180 : 240
                Layout.minimumWidth: compactSettingsLayout ? 150 : 200
                implicitHeight: controlHeight
                currentIndex: {
                    var active = String(bridge.activeCatProfile || "")
                    if (active.length === 0)
                        return -1
                    return find(active)
                }
                onActivated: {
                    catProfileNameField.text = currentText
                    loadSelectedCatProfile()
                }
                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text {
                    text: catProfileCombo.currentIndex >= 0 ? catProfileCombo.displayText : qsTr("No profile")
                    color: catProfileCombo.currentIndex >= 0 ? textPrimary : textSecondary
                    font.pixelSize: controlFontSize
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                delegate: ItemDelegate {
                    contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                    background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium }
                }
                popup: SettingsComboPopup { combo: catProfileCombo }
            }

            DecoTextField {
                id: catProfileNameField
                Layout.fillWidth: true
                Layout.minimumWidth: compactSettingsLayout ? 180 : 260
                implicitHeight: controlHeight
                text: bridge.activeCatProfile || (bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : "")
                placeholderText: qsTr("Profile name")
                color: textPrimary
                font.pixelSize: controlFontSize
                leftPadding: 8
                onAccepted: saveCatProfileFromField()
                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            }

            Button {
                id: catProfileLoadButton
                text: qsTr("Load")
                enabled: catProfileCombo.currentIndex >= 0 && !dialog.catConnectionInProgress()
                Layout.preferredWidth: 68
                implicitHeight: controlHeight
                onClicked: loadSelectedCatProfile()
                background: Rectangle { color: catProfileLoadButton.enabled && catProfileLoadButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.22) : bgMedium; border.color: catProfileLoadButton.enabled ? primaryBlue : glassBorder; radius: 4 }
                contentItem: Text { text: catProfileLoadButton.text; color: catProfileLoadButton.enabled ? primaryBlue : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }

            Button {
                id: catProfileSaveButton
                text: qsTr("Save")
                enabled: String(catProfileNameField.text || "").trim().length > 0
                Layout.preferredWidth: 68
                implicitHeight: controlHeight
                onClicked: saveCatProfileFromField()
                background: Rectangle { color: catProfileSaveButton.enabled && catProfileSaveButton.hovered ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.22) : bgMedium; border.color: catProfileSaveButton.enabled ? accentGreen : glassBorder; radius: 4 }
                contentItem: Text { text: catProfileSaveButton.text; color: catProfileSaveButton.enabled ? accentGreen : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }

            Button {
                id: catProfileNewButton
                text: qsTr("New")
                Layout.preferredWidth: 62
                implicitHeight: controlHeight
                onClicked: saveNewCatProfileFromCurrent()
                background: Rectangle { color: catProfileNewButton.hovered ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.18) : bgMedium; border.color: secondaryCyan; radius: 4 }
                contentItem: Text { text: catProfileNewButton.text; color: secondaryCyan; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }

            Button {
                id: catProfileDeleteButton
                text: qsTr("Delete")
                enabled: catProfileCombo.currentIndex >= 0
                Layout.preferredWidth: 76
                implicitHeight: controlHeight
                onClicked: deleteSelectedCatProfile()
                background: Rectangle { color: catProfileDeleteButton.enabled && catProfileDeleteButton.hovered ? Qt.rgba(1,0.25,0.25,0.18) : bgMedium; border.color: catProfileDeleteButton.enabled ? "#ff5b5b" : glassBorder; radius: 4 }
                contentItem: Text { text: catProfileDeleteButton.text; color: catProfileDeleteButton.enabled ? "#ff7777" : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // Banner: porta seriale occupata da altro software
        Item {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            visible: bridge.lastCatError.indexOf("occupata") !== -1
            implicitHeight: visible ? (settingsBannerText.implicitHeight + 16) : 0
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(1.0, 0.65, 0.0, 0.15)
                border.color: Qt.rgba(1.0, 0.65, 0.0, 0.6)
                border.width: 1
                radius: 6
                Text {
                    id: settingsBannerText
                    anchors.fill: parent
                    anchors.margins: 8
                    wrapMode: Text.WordWrap
                    color: textPrimary
                    font.pixelSize: 11
                    text: bridge.lastCatError + "\n" + qsTr("Tip: close OmniRig from the Windows tray icon, then press Connect again.")
                }
            }
        }

        // ── Stato connessione ──
        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        Row {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 8
            Rectangle { width: 12; height: 12; radius: 6; color: bridge.catConnected ? accentGreen : "#f44336"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: bridge.catConnected ? qsTr("Connected") + " — " + bridge.catRigName + " — " + bridge.catMode : qsTr("Disconnected"); color: bridge.catConnected ? accentGreen : "#f44336"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
            Item { width: 20; height: 1 }
            Rectangle {
                width: 100; height: 28; radius: 6
                color: connMA.containsMouse ? (bridge.catConnected ? Qt.rgba(0.95,0.26,0.21,0.2) : Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.2)) : "transparent"
                border.color: bridge.catConnected ? "#f44336" : accentGreen
                Text { anchors.centerIn: parent; text: bridge.catConnected ? qsTr("Disconnect") : qsTr("Connect"); color: bridge.catConnected ? "#f44336" : accentGreen; font.pixelSize: 11 }
                MouseArea { id: connMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.toggleCatConnection()
                }
            }
            Rectangle {
                width: 28; height: 28; radius: 6
                color: refreshMA.containsMouse ? bgMedium : "transparent"
                border.color: glassBorder
                Text { anchors.centerIn: parent; text: "↻"; color: secondaryCyan; font.pixelSize: 16 }
                MouseArea { id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.refreshCatPorts()
                }
            }
        }

        // ── Controllo CAT ──
        Text { text: qsTr("CAT CONTROL"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        // Rilevamento automatico: legge solo cio' che il sistema
        // gia' sa, non apre porte e non invia comandi.
        Text {
            text: qsTr("Auto-detect:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        Button {
            id: detectRigButton
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            implicitHeight: controlHeight
            text: "🔍  " + qsTr("Detect my radio")
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Reads what the system already knows: it opens no port and sends no command")
            onClicked: detectRigResults.detectAndOpen("")
            background: Rectangle {
                color: detectRigButton.hovered
                       ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.18)
                       : Qt.rgba(1, 1, 1, 0.07)
                border.color: accentGreen
                radius: 4
            }
            contentItem: Text {
                text: detectRigButton.text
                color: accentGreen
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            visible: !dialog.usesCat4OmControls()
            text: qsTr("Rig:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        DecoComboBox {
            id: rigCombo
            visible: !dialog.usesCat4OmControls()
            model: bridge.catBackend === "tci" ? ["TCI Client RX1", "TCI Client RX2"] : (bridge.catManager ? bridge.catManager.rigList : []); Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            property string filterText: ""
            property var filteredRigList: {
                var src = bridge.catBackend === "tci" ? ["TCI Client RX1", "TCI Client RX2"] : (bridge.catManager ? bridge.catManager.rigList : [])
                var q = filterText.trim().toLowerCase()
                if (q.length === 0)
                    return src

                var terms = q.split(/\s+/)
                var out = []
                for (var i = 0; i < src.length; ++i) {
                    var name = String(src[i])
                    var haystack = name.toLowerCase()
                    var match = true
                    for (var t = 0; t < terms.length; ++t) {
                        if (terms[t].length > 0 && haystack.indexOf(terms[t]) < 0) {
                            match = false
                            break
                        }
                    }
                    if (match)
                        out.push(name)
                }
                return out
            }
            function chooseRig(name) {
                var idx = model.indexOf(name)
                if (idx >= 0)
                    currentIndex = idx
                if (bridge.catManager) {
                    bridge.catManager.rigName = name
                    dialog.enforceForceLineAvailability()
                }
                dialog.scheduleCatPersist()
                rigComboPopup.close()
            }
            currentIndex: {
                if (!bridge.catManager)
                    return -1
                return find(bridge.catManager.rigName)
            }
            onActivated: {
                if (bridge.catManager) {
                    bridge.catManager.rigName = currentText
                    dialog.enforceForceLineAvailability()
                }
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text {
                text: rigCombo.currentIndex >= 0 ? rigCombo.displayText : dialog.activeRigName()
                color: textPrimary
                font.pixelSize: controlFontSize
                leftPadding: 8
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            popup: Popup {
                id: rigComboPopup
                parent: Overlay.overlay
                readonly property var comboOrigin: rigCombo && parent ? rigCombo.mapToItem(parent, 0, 0) : Qt.point(0, 0)
                readonly property real wantedHeight: Math.min(420,
                                 Math.max(180,
                                          Math.min(dialog.height - 160,
                                                   54 + Math.max(34, rigComboPopupList.contentHeight))))
                readonly property real spaceBelow: parent ? parent.height - comboOrigin.y - rigCombo.height - 8 : wantedHeight
                readonly property real spaceAbove: parent ? comboOrigin.y - 8 : 0
                readonly property bool openAbove: wantedHeight > spaceBelow && spaceAbove > spaceBelow
                x: parent ? Math.max(8, Math.min(comboOrigin.x, parent.width - width - 8)) : 0
                y: parent
                   ? (openAbove
                      ? Math.max(8, comboOrigin.y - height - 2)
                      : Math.min(comboOrigin.y + rigCombo.height + 2, parent.height - height - 8))
                   : 0
                width: parent ? Math.min(Math.max(rigCombo.width, 560), Math.max(80, parent.width - 16))
                              : Math.max(rigCombo.width, 560)
                height: Math.min(420,
                                 Math.max(180,
                                          Math.min(dialog.height - 160,
                                                   54 + Math.max(34, rigComboPopupList.contentHeight))))
                focus: true
                onOpened: {
                    rigCombo.filterText = ""
                    rigSearchField.forceActiveFocus()
                }
                background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                contentItem: Column {
                    width: rigComboPopup.width
                    spacing: 6

                    DecoTextField {
                        id: rigSearchField
                        x: 8
                        width: parent.width - 16
                        height: 36
                        placeholderText: qsTr("Search radio, model or brand...")
                        text: rigCombo.filterText
                        selectByMouse: true
                        color: textPrimary
                        placeholderTextColor: textSecondary
                        font.pixelSize: controlFontSize
                        leftPadding: 10
                        rightPadding: 10
                        onTextChanged: rigCombo.filterText = text
                        background: Rectangle {
                            color: bgMedium
                            border.color: activeFocus ? secondaryCyan : glassBorder
                            radius: 4
                        }
                    }

                    ListView {
                        id: rigComboPopupList
                        x: 8
                        width: parent.width - 16
                        height: rigComboPopup.height - rigSearchField.height - 22
                        clip: true
                        model: rigCombo.filteredRigList
                        currentIndex: -1
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: true
                        focus: true
                        reuseItems: true
                        delegate: ItemDelegate {
                            width: rigComboPopupList.width
                            height: 34
                            highlighted: modelData === dialog.activeRigName()
                            contentItem: Text {
                                text: modelData
                                color: parent.highlighted ? secondaryCyan : textPrimary
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                color: hovered || parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                            }
                            onClicked: rigCombo.chooseRig(modelData)
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                    }
                }
            }
        }

        Text {
            visible: dialog.usesCat4OmControls()
            text: qsTr("Management:")
            color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        DecoTextField {
            visible: dialog.usesCat4OmControls()
            text: dialog.usesCat4OmControls() && bridge.catManager ? bridge.catManager.managementEndpoint : ""
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth; implicitHeight: controlHeight
            placeholderText: "127.0.0.1:5000"
            color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
            selectByMouse: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                if (bridge.catManager) bridge.catManager.managementEndpoint = text.trim()
                dialog.scheduleCatPersist()
            }
        }

        Text {
            visible: dialog.usesCat4OmControls()
            text: qsTr("Control:")
            color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        DecoTextField {
            visible: dialog.usesCat4OmControls()
            text: dialog.usesCat4OmControls() && bridge.catManager ? bridge.catManager.controlEndpoint : ""
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth; implicitHeight: controlHeight
            placeholderText: "127.0.0.1:5001"
            color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8
            selectByMouse: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                if (bridge.catManager) bridge.catManager.controlEndpoint = text.trim()
                dialog.scheduleCatPersist()
            }
        }

        Text {
            visible: dialog.usesCat4OmControls()
            text: qsTr("Radio group:")
            color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        DecoComboBox {
            id: cat4OmGroupCombo
            visible: dialog.usesCat4OmControls()
            model: dialog.usesCat4OmControls() && bridge.catManager ? bridge.catManager.groupList : []
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); implicitHeight: controlHeight
            currentIndex: dialog.usesCat4OmControls() && bridge.catManager ? find(bridge.catManager.groupId) : -1
            onActivated: {
                if (bridge.catManager) bridge.catManager.groupId = currentText
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text {
                text: cat4OmGroupCombo.currentIndex >= 0 ? cat4OmGroupCombo.displayText : qsTr("Automatic discovery")
                color: cat4OmGroupCombo.currentIndex >= 0 ? textPrimary : textSecondary
                font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: cat4OmGroupCombo }
        }

        Text {
            visible: dialog.usesCat4OmControls()
            text: qsTr("Radio:")
            color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        DecoComboBox {
            id: cat4OmRadioCombo
            visible: dialog.usesCat4OmControls()
            model: dialog.usesCat4OmControls() && bridge.catManager ? bridge.catManager.radioList : []
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); implicitHeight: controlHeight
            currentIndex: dialog.usesCat4OmControls() && bridge.catManager ? find(bridge.catManager.radioId) : -1
            onActivated: {
                if (bridge.catManager) bridge.catManager.radioId = currentText
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text {
                text: cat4OmRadioCombo.currentIndex >= 0 ? cat4OmRadioCombo.displayText : qsTr("First available radio")
                color: cat4OmRadioCombo.currentIndex >= 0 ? textPrimary : textSecondary
                font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: cat4OmRadioCombo }
        }

        Text {
            visible: dialog.usesCat4OmControls()
            text: qsTr("Ownership:")
            color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100
        }
        RowLayout {
            visible: dialog.usesCat4OmControls()
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 12
            CheckBox {
                checked: dialog.usesCat4OmControls() && bridge.catManager ? bridge.catManager.autoRequestOwnership : true
                text: qsTr("Request control automatically")
                onToggled: {
                    if (bridge.catManager) bridge.catManager.autoRequestOwnership = checked
                    dialog.scheduleCatPersist()
                }
                contentItem: Text { text: parent.text; color: textPrimary; font.pixelSize: 12; leftPadding: 26; verticalAlignment: Text.AlignVCenter }
            }
            Text {
                Layout.fillWidth: true
                text: dialog.usesCat4OmControls() && bridge.catManager ? bridge.catManager.connectionDetail : ""
                color: bridge.catConnected ? accentGreen : textSecondary
                font.pixelSize: 11; elide: Text.ElideRight
            }
        }

        Text {
            visible: dialog.usesSerialControls()
            text: qsTr("Serial Port:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        RowLayout {
            visible: dialog.usesSerialControls()
            Layout.fillWidth: true
            Layout.minimumWidth: wideFieldMinWidth
            spacing: 8

            DecoComboBox {
                id: serialPortCombo
                visible: dialog.usesSerialControls()
                model: bridge.catManager ? bridge.catManager.portList : []
                Layout.fillWidth: true
                implicitHeight: controlHeight
                currentIndex: {
                    if (!bridge.catManager)
                        return -1
                    return find(bridge.catManager.serialPort)
                }
                onActivated: {
                    if (bridge.catManager) {
                        bridge.catManager.serialPort = currentText
                        dialog.enforceForceLineAvailability()
                    }
                    dialog.scheduleCatPersist()
                }
                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                contentItem: Text {
                    text: serialPortCombo.currentIndex >= 0 ? serialPortCombo.displayText : (bridge.catManager ? bridge.catManager.serialPort : "")
                    color: textPrimary
                    font.pixelSize: controlFontSize
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                    background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                popup: SettingsComboPopup { combo: serialPortCombo }
            }

            Rectangle {
                id: serialPortRefreshButton
                Layout.preferredWidth: controlHeight
                Layout.preferredHeight: controlHeight
                radius: 4
                color: serialPortRefreshMA.containsMouse ? bgMedium : "transparent"
                border.color: secondaryCyan
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "↻"
                    color: secondaryCyan
                    font.pixelSize: 17
                    font.bold: true
                }

                MouseArea {
                    id: serialPortRefreshMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.refreshCatPorts()
                }

                ToolTip.visible: serialPortRefreshMA.containsMouse
                ToolTip.text: qsTr("Refresh serial ports")
            }
        }
        Text {
            visible: dialog.usesSerialControls()
            text: qsTr("Baud Rate:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        DecoComboBox {
            id: baudCombo
            visible: dialog.usesSerialControls()
            model: bridge.catManager && bridge.catManager.baudList ? bridge.catManager.baudList : ["4800","9600","19200","38400","57600","115200"]
            Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                var baud = dialog.activeBaudRateText()
                return baud === "" ? -1 : dialog.stringListIndexOf(model, baud)
            }
            onActivated: {
                if (bridge.catManager) bridge.catManager.baudRate = parseInt(currentText)
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text {
                text: baudCombo.currentIndex >= 0 ? baudCombo.displayText : dialog.activeBaudRateText()
                color: textPrimary
                font.pixelSize: controlFontSize
                leftPadding: 8
                verticalAlignment: Text.AlignVCenter
            }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: baudCombo }
        }

        // ── CI-V Address (solo rig ICOM) ──
        Text {
            visible: dialog.usesSerialControls() && dialog.rigIsIcom()
            text: qsTr("CI-V Addr:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        DecoTextField {
            id: civAddrField
            visible: dialog.usesSerialControls() && dialog.rigIsIcom()
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            property bool invalidAddress: false
            text: dialog.civAddressText()
            placeholderText: dialog.civAddressPlaceholderText()
            selectByMouse: true
            inputMethodHints: Qt.ImhNoPredictiveText
            color: textPrimary
            font.pixelSize: controlFontSize
            leftPadding: 8
            ToolTip.visible: activeFocus
            ToolTip.delay: 500
            ToolTip.text: qsTr("Enter the radio CI-V address in hexadecimal, from 0x00 to 0xFF. The address is saved with the CAT settings.")
            background: Rectangle {
                color: bgMedium
                border.color: civAddrField.invalidAddress ? "#f44336"
                             : (civAddrField.activeFocus ? secondaryCyan : glassBorder)
                radius: 4
            }
            onActiveFocusChanged: {
                if (activeFocus)
                    invalidAddress = false
            }
            onEditingFinished: {
                var address = dialog.civAddressFromText(text)
                if (address < 0) {
                    invalidAddress = true
                    text = dialog.civAddressText()
                    return
                }

                invalidAddress = false
                if (bridge.catManager)
                    bridge.catManager.civAddress = address
                text = dialog.civAddressText()
                dialog.scheduleCatPersist()
            }
        }
        Connections {
            target: bridge ? bridge.catManager : null
            function onCivAddressChanged() {
                if (!civAddrField.activeFocus)
                    civAddrField.text = dialog.civAddressText()
            }
        }

        Text {
            visible: dialog.usesNetworkControls()
            text: qsTr("Host:Port:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        DecoTextField {
            visible: dialog.usesNetworkControls()
            text: bridge.catManager ? bridge.catManager.networkPort : ""
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            placeholderText: bridge.catManager && bridge.catManager.rigName === "Ham Radio Deluxe" ? "127.0.0.1:7809" : "host:port"
            selectByMouse: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: {
                if (bridge.catManager) bridge.catManager.networkPort = text.trim()
                dialog.scheduleCatPersist()
            }
        }

        Text {
            visible: bridge.catManager && bridge.catManager.rigName === "Ham Radio Deluxe"
            text: qsTr("HRD Radio:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        CheckBox {
            id: hrdStrictRadioMatchCheck
            visible: bridge.catManager && bridge.catManager.rigName === "Ham Radio Deluxe"
            checked: bridge.catManager ? bridge.catManager.hrdStrictRadioMatch : true
            text: qsTr("Strict match (abort if configured radio is not current in HRD)")
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            onCheckedChanged: {
                if (bridge.catManager && bridge.catManager.hrdStrictRadioMatch !== checked) {
                    bridge.catManager.hrdStrictRadioMatch = checked
                    dialog.scheduleCatPersist()
                }
            }
            contentItem: Text {
                text: parent.text
                color: textPrimary
                font.pixelSize: 12
                leftPadding: 26
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            visible: dialog.usesTciControls()
            text: qsTr("TCI Host:Port:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        DecoTextField {
            visible: dialog.usesTciControls()
            text: bridge.catManager ? bridge.catManager.tciPort : ""
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            Layout.minimumWidth: wideFieldMinWidth
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            placeholderText: qsTr("localhost:50001")
            selectByMouse: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onTextChanged: {
                if (bridge.catManager) bridge.catManager.tciPort = text
                dialog.scheduleCatPersist()
            }
        }

        Text {
            visible: dialog.usesTciControls()
            text: qsTr("TCI Audio:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        CheckBox {
            visible: dialog.usesTciControls()
            checked: bridge.catManager ? bridge.catManager.tciAudioEnabled : true
            text: qsTr("RX/TX via TCI")
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            onCheckedChanged: {
                if (bridge.catManager) bridge.catManager.tciAudioEnabled = checked
                dialog.scheduleCatPersist()
            }
            contentItem: Text {
                text: parent.text
                color: textPrimary
                font.pixelSize: 12
                leftPadding: 26
                verticalAlignment: Text.AlignVCenter
            }
        }

        // I server TCI consegnano l'audio a fondo scala: senza attenuazione il
        // flusso arriva saturo, la cascata diventa una macchia uniforme e solo
        // i segnali forti si decodificano. Il decodificatore lavora bene
        // intorno a -27 dBFS.
        Text {
            visible: dialog.usesTciControls()
            text: qsTr("TCI RX gain:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        RowLayout {
            visible: dialog.usesTciControls()
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 8
            Slider {
                id: tciRxGainSlider
                Layout.fillWidth: true
                from: -40
                to: 6
                stepSize: 1
                value: bridge.catManager ? bridge.catManager.tciRxGainDb : -20
                onMoved: {
                    if (bridge.catManager) bridge.catManager.setTciRxGainDb(value)
                    dialog.scheduleCatPersist()
                }
            }
            Text {
                text: Math.round(tciRxGainSlider.value) + " dB"
                color: textPrimary
                font.pixelSize: 12
                font.family: decodiumMonoFontFamily
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
        }

        Text { text: qsTr("PTT Method:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: pttCombo
            enabled: !dialog.usesProtocolCatOnly()
            model: dialog.usesProtocolCatOnly()
                   ? ["CAT"]
                   : (bridge.catManager && bridge.catManager.pttMethodList ? bridge.catManager.pttMethodList : ["CAT","DTR","RTS","VOX"])
            Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                if (dialog.usesProtocolCatOnly())
                    return 0
                var methods = (bridge.catManager && bridge.catManager.pttMethodList)
                              ? bridge.catManager.pttMethodList
                              : ["CAT","DTR","RTS","VOX"]
                var savedMethod = bridge.catManager ? bridge.catManager.pttMethod : "CAT"
                var idx = dialog.stringListIndexOf(methods, savedMethod)
                return idx >= 0 ? idx : 0
            }
            onActivated: {
                if (bridge.catManager) {
                    bridge.catManager.pttMethod = currentText
                    dialog.enforceForceLineAvailability()
                }
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text {
                text: {
                    if (pttCombo.currentIndex >= 0 && pttCombo.displayText !== "")
                        return pttCombo.displayText
                    if (bridge.catManager && bridge.catManager.pttMethod !== undefined && bridge.catManager.pttMethod !== null) {
                        var fallback = String(bridge.catManager.pttMethod).trim().toUpperCase()
                        return fallback !== "" ? fallback : "CAT"
                    }
                    return "CAT"
                }
                color: pttCombo.enabled ? textPrimary : textSecondary
                font.pixelSize: controlFontSize
                leftPadding: 8
                verticalAlignment: Text.AlignVCenter
            }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: pttCombo }
        }
        Text {
            visible: dialog.usesSeparatePttPort()
            text: qsTr("PTT Port:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: labelWidth
        }
        DecoComboBox {
            id: pttPortCombo
            visible: dialog.usesSeparatePttPort()
            model: dialog.pttPortOptions()
            Layout.fillWidth: true
            implicitHeight: controlHeight
            currentIndex: {
                if (!bridge.catManager)
                    return -1
                var idx = find(bridge.catManager.pttPort)
                return idx >= 0 ? idx : (count > 0 ? 0 : -1)
            }
            onActivated: {
                if (bridge.catManager) {
                    bridge.catManager.pttPort = currentText
                    dialog.enforceForceLineAvailability()
                }
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: pttPortCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: pttPortCombo }
        }
        Item { visible: dialog.usesSeparatePttPort(); Layout.fillWidth: true; Layout.columnSpan: 2 }
        // Let GridLayout place this row sequentially.  Setting Layout.column
        // without Layout.row pins the controls to row 0 and collides with the
        // first CAT controls when their visibility changes.
        Text { visible: !dialog.usesCat4OmControls(); text: qsTr("Poll Interval (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: pollSpin
            visible: !dialog.usesCat4OmControls()
            from: 1; to: 99; value: bridge.catManager ? bridge.catManager.pollInterval : 3; editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1)
            onValueChanged: {
                if (bridge.catManager) bridge.catManager.pollInterval = value
                dialog.scheduleCatPersist()
            }
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: pollSpin.textFromValue(pollSpin.value, pollSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !pollSpin.editable; validator: pollSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text {
            visible: dialog.usesSerialControls()
            text: qsTr("CAT keep-alive:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
        }
        CheckBox {
            visible: dialog.usesSerialControls()
            checked: bridge.catManager ? bridge.catManager.catKeepAlive : false
            text: qsTr("Light polling for interface activity LEDs")
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            onCheckedChanged: {
                if (bridge.catManager && bridge.catManager.catKeepAlive !== checked)
                    bridge.catManager.catKeepAlive = checked
                dialog.scheduleCatPersist()
            }
            contentItem: Text {
                text: parent.text
                color: textPrimary
                font.pixelSize: 12
                leftPadding: 26
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        // ── Parametri Seriali ──
        Text {
            visible: dialog.usesSerialControls()
            text: qsTr("SERIAL PARAMETERS")
            color: secondaryCyan
            font.pixelSize: 12
            font.bold: true
            Layout.columnSpan: pageColumns
            Layout.topMargin: 10
        }
        Rectangle {
            visible: dialog.usesSerialControls()
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
            height: 1
            color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3)
        }

        Text { visible: dialog.usesSerialControls(); text: qsTr("Data Bits:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: dataBitsCombo
            visible: dialog.usesSerialControls()
            model: ["Default","8","7"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                if (!bridge.catManager)
                    return 0
                return dialog.catSerialChoiceIndex(model, bridge.catManager.dataBits, 0)
            }
            onActivated: {
                if (bridge.catManager) bridge.catManager.dataBits = currentText
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: dataBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: dataBitsCombo }
        }
        Text { visible: dialog.usesSerialControls(); text: qsTr("Stop Bits:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: stopBitsCombo
            visible: dialog.usesSerialControls()
            model: ["Default","1","2"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                return dialog.catSerialChoiceIndex(model, dialog.activeStopBitsText(), 0)
            }
            onActivated: {
                if (bridge.catManager) bridge.catManager.stopBits = currentText
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: stopBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: stopBitsCombo }
        }

        Text { visible: dialog.usesSerialControls(); text: qsTr("Handshake:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: handshakeCombo
            visible: dialog.usesSerialControls()
            model: ["Default","none","xonxoff","hardware"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                if (!bridge.catManager)
                    return 0
                return dialog.catSerialChoiceIndex(model, bridge.catManager.handshake, 0)
            }
            onActivated: {
                if (bridge.catManager) {
                    bridge.catManager.handshake = currentText
                    dialog.enforceForceLineAvailability()
                }
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: dialog.handshakeChoiceLabel(handshakeCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: dialog.handshakeChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: handshakeCombo }
        }
        Item { visible: dialog.usesSerialControls(); Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { visible: dialog.usesSerialControls(); enabled: dialog.forceDtrControlEnabled(); text: qsTr("Force DTR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: forceDtrCombo
            visible: dialog.usesSerialControls()
            enabled: dialog.forceDtrControlEnabled()
            model: ["Default","On","Off"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                if (!enabled || !bridge.catManager)
                    return 0
                var v = dialog.forceLineMode(bridge.catManager.forceDtr, bridge.catManager.dtrHigh)
                var idx = find(v)
                return idx >= 0 ? idx : 0
            }
            onActivated: dialog.applyForceLineValue("dtr", currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            // Lookup diretto su model[currentIndex] — displayText non si propaga
            // affidabilmente al primo render con model JS array (Qt 6 quirk).
            contentItem: Text { text: dialog.setupChoiceLabel(forceDtrCombo.model[Math.max(0, forceDtrCombo.currentIndex)]); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: dialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: forceDtrCombo }
        }
        Text { visible: dialog.usesSerialControls(); enabled: dialog.forceRtsControlEnabled(); text: qsTr("Force RTS:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: forceRtsCombo
            visible: dialog.usesSerialControls()
            enabled: dialog.forceRtsControlEnabled()
            model: ["Default","On","Off"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: {
                if (!enabled || !bridge.catManager)
                    return 0
                var v = dialog.forceLineMode(bridge.catManager.forceRts, bridge.catManager.rtsHigh)
                var idx = find(v)
                return idx >= 0 ? idx : 0
            }
            onActivated: dialog.applyForceLineValue("rts", currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: dialog.setupChoiceLabel(forceRtsCombo.model[Math.max(0, forceRtsCombo.currentIndex)]); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: dialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: forceRtsCombo }
        }

        // ── Operazione Split ──
        Text { text: qsTr("SPLIT OPERATION"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Split:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: splitCombo
            model: dialog.splitModeOptions(); Layout.fillWidth: true; implicitHeight: controlHeight
            textRole: "label"
            currentIndex: {
                if (!bridge.catManager)
                    return 0
                for (var i = 0; i < splitCombo.model.length; ++i) {
                    if (splitCombo.model[i].value === String(bridge.catManager.splitMode))
                        return i
                }
                return 0
            }
            onActivated: {
                if (bridge.catManager) bridge.catManager.splitMode = splitCombo.model[currentIndex].value
                dialog.scheduleCatPersist()
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: splitCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: modelData.label; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: splitCombo }
        }
        Text { text: qsTr("Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: modeCombo
            model: ["USB","Data/Pkt","None"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: dialog.settingChoiceIndex("CATMode", model, 0)
            onActivated: bridge.setSetting("CATMode", currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: dialog.setupChoiceLabel(modeCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: dialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: modeCombo }
        }

        Text { visible: !dialog.usesTciControls(); text: qsTr("TX Audio Src:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            id: txAudioSrcCombo
            visible: !dialog.usesTciControls()
            model: ["Rear/Data","Front/Mic"]; Layout.fillWidth: true; implicitHeight: controlHeight
            currentIndex: dialog.settingChoiceIndex("TXAudioSource", model, 0)
            onActivated: bridge.setSetting("TXAudioSource", currentText)
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: dialog.setupChoiceLabel(txAudioSrcCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate { contentItem: Text { text: dialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
            popup: SettingsComboPopup { combo: txAudioSrcCombo }
        }
        Text { visible: dialog.usesTciControls(); text: qsTr("TX Audio:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            visible: dialog.usesTciControls()
            text: qsTr("TCI Audio")
            readOnly: true
            enabled: false
            Layout.fillWidth: true
            implicitHeight: controlHeight
            leftPadding: 8
            color: textSecondary
            font.pixelSize: controlFontSize
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── CAT condivisa ──
        // Decodium tiene la seriale e la rivende in rete con il protocollo
        // rigctld: gli altri programmi si collegano come "Hamlib NET rigctl".
        Text { text: qsTr("SHARED CAT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Share CAT:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: catShareEnabled
            checked: (bridge && bridge.catShare) ? bridge.catShare.enabled : false
            onToggled: applyCatShare()
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Shared port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: catSharePort
            text: (bridge && bridge.catShare) ? String(bridge.catShare.port) : "4533"
            inputMethodHints: Qt.ImhDigitsOnly
            Layout.fillWidth: true
            implicitHeight: controlHeight
            font.pixelSize: controlFontSize
            onEditingFinished: applyCatShare()
        }

        Text { text: qsTr("Allow control:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: catShareControl
            checked: (bridge && bridge.catShare) ? bridge.catShare.allowControl : false
            enabled: catShareEnabled.checked
            onToggled: applyCatShare()
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        // La trasmissione ha un interruttore proprio: cambiare frequenza a una
        // radio altrui e' un fastidio, mandarla in aria e' un'altra cosa.
        Text { text: qsTr("Allow transmit:"); color: "#ff6b6b"; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: catSharePtt
            checked: (bridge && bridge.catShare) ? bridge.catShare.allowPtt : false
            enabled: catShareEnabled.checked && catShareControl.checked
            onToggled: applyCatShare()
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? "#ff6b6b" : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: (bridge && bridge.catShare && bridge.catShare.listening) ? accentGreen
                   : ((bridge && bridge.catShare && bridge.catShare.enabled) ? "#ff6b6b" : textSecondary)
            text: {
                if (!bridge || !bridge.catShare)
                    return ""
                if (!bridge.catShare.listening) {
                    // Un guasto va detto: "non condivisa" senza il motivo manda
                    // a cercare il problema nel posto sbagliato. Il caso tipico
                    // e' la porta gia' occupata da un altro programma.
                    if (bridge.catShare.enabled)
                        return qsTr("Sharing not started: %1").arg(bridge.catShare.status)
                    return qsTr("Not shared: other programs cannot use the radio while Decodium holds the serial port.")
                }
                return qsTr("Listening on 127.0.0.1:%1 \u00b7 connected programs: %2")
                       .arg(bridge.catShare.port).arg(bridge.catShare.clientCount)
                       + "  \u00b7  " + qsTr("in other programs choose \"Hamlib NET rigctl\"")
            }
        }

        // ── Spot condivisi ──
        // Decodium tiene la linea col nodo DX Cluster e ne rivende gli spot
        // gia' interpretati a chi non puo' aprirne una propria: il telefono,
        // prima di tutto. Una riga JSON per spot, nessun comando accettato.
        Text { text: qsTr("SHARED SPOTS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Share spots:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: spotShareEnabled
            checked: (bridge && bridge.spotShare) ? bridge.spotShare.enabled : false
            onToggled: applySpotShare()
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Spot port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: spotSharePort
            text: (bridge && bridge.spotShare) ? String(bridge.spotShare.port) : "4534"
            inputMethodHints: Qt.ImhDigitsOnly
            Layout.fillWidth: true
            implicitHeight: controlHeight
            font.pixelSize: controlFontSize
            onEditingFinished: applySpotShare()
        }

        Text {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: (bridge && bridge.spotShare && bridge.spotShare.listening) ? accentGreen
                   : ((bridge && bridge.spotShare && bridge.spotShare.enabled) ? "#ff6b6b" : textSecondary)
            text: {
                if (!bridge || !bridge.spotShare)
                    return ""
                if (!bridge.spotShare.listening) {
                    // Come per la CAT: se e' acceso e non ascolta, il motivo
                    // va detto, altrimenti si cerca il guasto dove non e'.
                    if (bridge.spotShare.enabled)
                        return qsTr("Spot sharing not started: %1").arg(bridge.spotShare.status)
                    return qsTr("Spots stay on this computer. Share them to read the cluster from a phone on the same WiFi.")
                }
                return qsTr("Listening on port %1 · connected clients: %2")
                       .arg(bridge.spotShare.port).arg(bridge.spotShare.clientCount)
                       + "  ·  " + qsTr("needs the DX Cluster connected")
            }
        }

        // ── L'altro capo: usare la CAT condivisa da qualcun altro ──
        // Il motore lo sapeva gia' fare (Hamlib "NET rigctl" e' un rig di rete
        // come Ham Radio Deluxe), ma bisognava saperlo scegliere fra trecento
        // radio e digitare l'indirizzo a mano. Qui e' un bottone: serve a chi
        // apre una seconda istanza, che la seriale non puo' averla, perche' ce
        // l'ha gia' la prima.
        Text { text: qsTr("Use a shared CAT:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: catClientAddress
            text: bridge.getSetting("CatShareClientAddress", "127.0.0.1:4533")
            Layout.fillWidth: true
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            placeholderText: "127.0.0.1:4533"
            selectByMouse: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: bridge.setSetting("CatShareClientAddress", text.trim())
        }
        Item { Layout.preferredWidth: 100 }
        Rectangle {
            id: catClientConnect
            Layout.fillWidth: true
            implicitHeight: controlHeight
            radius: 6
            color: catClientMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : bgMedium
            border.color: accentGreen
            Text {
                anchors.centerIn: parent
                text: qsTr("Connect to it")
                color: accentGreen
                font.pixelSize: controlFontSize
            }
            MouseArea {
                id: catClientMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    var address = catClientAddress.text.trim()
                    if (address.length === 0)
                        address = "127.0.0.1:4533"
                    bridge.setSetting("CatShareClientAddress", address)
                    // Il cambio di backend rifa' il gestore CAT: rig e indirizzo
                    // si scrivono dopo, quando c'e' quello nuovo ad ascoltare.
                    bridge.catBackend = "hamlib"
                    Qt.callLater(function() {
                        if (!bridge.catManager)
                            return
                        bridge.catManager.rigName = "Hamlib NET rigctl"
                        bridge.catManager.networkPort = address
                        dialog.scheduleCatPersist()
                    })
                }
            }
        }

        Text {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: textSecondary
            text: qsTr("For a second instance of Decodium on the same radio: start it with --rig-name and connect it here. The serial port stays with the first one, which shares it.")
        }

        // ── Aprire la seconda istanza ──
        // Farlo a mano vuol dire una riga di comando e un profilo da
        // sistemare. Qui si da' un nome e si preme: il profilo nasce gia'
        // senza la seriale di questa istanza (che e' occupata) e, se
        // richiesto, gia' puntato sulla CAT condivisa.
        Text { text: qsTr("Second instance:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: secondInstanceName
            text: bridge.getSetting("SecondInstanceName", "RX2")
            Layout.fillWidth: true
            implicitHeight: controlHeight
            leftPadding: 8
            color: textPrimary
            font.pixelSize: controlFontSize
            placeholderText: qsTr("name, e.g. RX2")
            selectByMouse: true
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            onEditingFinished: bridge.setSetting("SecondInstanceName", text.trim())
        }
        Text { text: qsTr("On shared CAT:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: secondInstanceShared
            checked: bridge.getSetting("SecondInstanceSharedCat", true)
            onToggled: bridge.setSetting("SecondInstanceSharedCat", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Item { Layout.preferredWidth: 100 }
        Rectangle {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            implicitHeight: controlHeight
            radius: 6
            color: secondInstanceMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : bgMedium
            border.color: accentGreen
            Text {
                anchors.centerIn: parent
                text: qsTr("Open a second Decodium")
                color: accentGreen
                font.pixelSize: controlFontSize
            }
            MouseArea {
                id: secondInstanceMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    var name = secondInstanceName.text.trim()
                    bridge.setSetting("SecondInstanceName", name)
                    bridge.setSetting("SecondInstanceSharedCat", secondInstanceShared.checked)
                    var problem = bridge.launchSecondInstance(name, secondInstanceShared.checked)
                    secondInstanceResult.problem = problem
                    secondInstanceResult.opened = (problem.length === 0)
                }
            }
        }

        Text {
            id: secondInstanceResult
            property string problem: ""
            property bool opened: false
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: problem.length > 0 ? "#ff6b6b" : (opened ? accentGreen : textSecondary)
            text: {
                if (problem.length > 0)
                    return problem
                if (opened)
                    return qsTr("Second instance started. Choose its own audio device and, if it has a receiver of its own, its own radio: that is what lets you listen on two bands at once.")
                return qsTr("A second Decodium with its own settings profile. It does not take the serial port - this one keeps it and shares it.")
            }
        }

        // ── Amplificatore ──
        // Sorgente di misura indipendente dalla radio: il DECOMETER puo
        // mostrare i watt del PA invece di quelli dell'eccitatrice.
        Text { text: qsTr("AMPLIFIER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Read amplifier:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: ampEnabled
            checked: (bridge && bridge.amplifier) ? bridge.amplifier.enabled : false
            onToggled: applyAmplifier()
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Amplifier port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: ampPort
            text: (bridge && bridge.amplifier) ? bridge.amplifier.port : ""
            placeholderText: "COM7"
            Layout.fillWidth: true
            implicitHeight: controlHeight
            font.pixelSize: controlFontSize
            onEditingFinished: applyAmplifier()
        }
        // Cerca l'amplificatore invece di far ricopiare a mano il nome della
        // porta: si chiede lo stato a ognuna delle porte libere e si guarda
        // chi risponde nel protocollo SPE. Le porte del CAT restano fuori
        // dalla ricerca, perche' aprirle strapperebbe la radio a chi la usa.
        Button {
            id: ampCerca
            text: qsTr("Search")
            Layout.preferredWidth: 96
            implicitHeight: controlHeight
            enabled: !!bridge && !ampCerca.inCorso
            background: Rectangle {
                color: ampCerca.enabled && ampCerca.hovered
                       ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.22) : bgMedium
                border.color: ampCerca.enabled ? primaryBlue : glassBorder
                radius: 4
            }
            contentItem: Text {
                text: ampCerca.text
                color: ampCerca.enabled ? primaryBlue : textSecondary
                font.pixelSize: 11; font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            property bool inCorso: false
            property bool trovato: false
            property string esito: ""
            onClicked: {
                if (!bridge) return
                ampCerca.inCorso = true
                ampCerca.esito = qsTr("searching...")
                // Un giro di eventi prima di partire, cosi' la scritta cambia
                // davvero: la ricerca apre le porte e blocca per qualche
                // decimo di secondo, e senza questo l'operatore non vedrebbe
                // nulla fino alla fine.
                Qt.callLater(function() {
                    var r = bridge.cercaAmplificatore()
                    ampCerca.inCorso = false
                    if (r && r.trovato) {
                        ampPort.text = r.porta
                        ampBaud.text = String(r.baud)
                        ampPassive.checked = false
                        ampEnabled.checked = true
                        ampCerca.trovato = true
                        ampCerca.esito = qsTr("found %1 on %2").arg(r.modello).arg(r.porta)
                    } else {
                        ampCerca.trovato = false
                        ampCerca.esito = qsTr("no amplifier answered")
                    }
                })
            }
        }
        Text {
            text: ampCerca.esito
            visible: ampCerca.esito.length > 0
            color: ampCerca.trovato ? accentGreen : textSecondary
            font.pixelSize: 11
            Layout.columnSpan: 2
        }

        // In ascolto la porta si apre in sola lettura: e' la sola via se il
        // software del costruttore deve restare aperto.
        Text { text: qsTr("Listen only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            id: ampPassive
            checked: (bridge && bridge.amplifier) ? bridge.amplifier.passive : true
            enabled: ampEnabled.checked
            onToggled: applyAmplifier()
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Speed:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoTextField {
            id: ampBaud
            text: "9600"
            inputMethodHints: Qt.ImhDigitsOnly
            Layout.fillWidth: true
            implicitHeight: controlHeight
            font.pixelSize: controlFontSize
            onEditingFinished: applyAmplifier()
        }

        Text {
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: (bridge && bridge.amplifier && bridge.amplifier.responding)
                   ? accentGreen
                   : ((bridge && bridge.amplifier && bridge.amplifier.enabled) ? "#ff6b6b" : textSecondary)
            text: {
                if (!bridge || !bridge.amplifier || !bridge.amplifier.enabled)
                    return qsTr("The DECOMETER shows the exciter's power. Reading the amplifier requires its serial port, or a mirrored virtual port if its own software must stay open.")
                if (bridge.amplifier.responding)
                    return qsTr("Reading: %1 W, SWR %2")
                           .arg(bridge.amplifier.watts.toFixed(0))
                           .arg(bridge.amplifier.swr.toFixed(2))
                if (!bridge.amplifier.connected) {
                    // "Porta non aperta: errore" mandava a cercare un guasto
                    // dove non c'era. I due casi che capitano davvero hanno
                    // una risposta, e va data qui.
                    var st = bridge.amplifier.status
                    if (st === "noport")
                        return qsTr("No port set: type the amplifier's serial port, or the mirrored virtual port if its own software has to stay open.")
                    if (st === "busy")
                        return qsTr("Port %1 is held by another program - usually the amplifier's own software. Windows gives a serial port to one program at a time, reading included: close that program, or mirror the port and point Decodium at the copy.").arg(bridge.amplifier.port)
                    return qsTr("Port not open: %1").arg(st)
                }
                return qsTr("Port open, but no valid frame yet.")
            }
        }
        // ── Diagnostica ──
        Text { text: qsTr("DIAGNOSTICS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Check SWR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.supportsSwrTelemetry() ? bridge.getSetting("CheckSWR", false) : false
            enabled: dialog.supportsSwrTelemetry()
            // Only a user gesture may change CAT telemetry settings.  A
            // checked binding is evaluated while this lazy page is created;
            // onCheckedChanged used to treat that initial value as an edit
            // and unnecessarily reconnect Hamlib whenever Setup was opened.
            onToggled: if (enabled) {
                bridge.setSetting("CheckSWR", checked)
                if (checked && !bridge.getSetting("PWRandSWR", false))
                    bridge.setSetting("PWRandSWR", true)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("PWR and SWR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.supportsSwrTelemetry() ? bridge.getSetting("PWRandSWR", false) : false
            enabled: dialog.supportsSwrTelemetry()
            onToggled: if (enabled) bridge.setSetting("PWRandSWR", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        // Soglia SWR oltre la quale il TX viene bloccato/interrotto (protezione PA).
        // Configurabile: utile per il CW e per antenne con SWR moderato. Default 2.5.
        Text { text: qsTr("SWR max:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        DecoComboBox {
            enabled: dialog.supportsSwrTelemetry()
            model: ["2.0","2.5","3.0","3.5","4.0"]
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); implicitHeight: controlHeight
            currentIndex: Math.max(0, model.indexOf(Number(bridge.getSetting("SWRStopThreshold", 2.5)).toFixed(1)))
            onActivated: bridge.setSetting("SWRStopThreshold", Number(currentText))
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
        }

        Text { text: ""; Layout.preferredWidth: 100 }
        RowLayout {
            Layout.fillWidth: true; Layout.columnSpan: Math.max(1, pageColumns - 1); spacing: 10
            Rectangle {
                property bool catBusy: dialog.catConnectionInProgress()
                width: 100; height: controlHeight; radius: 4
                color: catBusy ? bgMedium : (catConnMA.containsMouse ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.3) : bgMedium)
                border.color: catBusy ? glassBorder : accentGreen
                Text { anchors.centerIn: parent; text: parent.catBusy ? qsTr("Connecting...") : qsTr("Connect"); color: parent.catBusy ? textSecondary : accentGreen; font.pixelSize: 12 }
                MouseArea { id: catConnMA; anchors.fill: parent; hoverEnabled: true; enabled: !parent.catBusy; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: { var controller = dialog.activeCatController(); if (controller) controller.connectRig() } }
            }
            Rectangle {
                property bool catBusy: dialog.catConnectionInProgress()
                width: 100; height: controlHeight; radius: 4
                color: catBusy ? bgMedium : (catDiscMA.containsMouse ? Qt.rgba(1,0.3,0.3,0.3) : bgMedium)
                border.color: catBusy ? glassBorder : "#f44336"
                Text { anchors.centerIn: parent; text: qsTr("Disconnect"); color: parent.catBusy ? textSecondary : "#f44336"; font.pixelSize: 12 }
                MouseArea { id: catDiscMA; anchors.fill: parent; hoverEnabled: true; enabled: !parent.catBusy; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: { var controller = dialog.activeCatController(); if (controller) controller.disconnectRig() } }
            }
        }
        Text {
            visible: bridge.catBackend === "hamlib"
            text: qsTr("Hamlib:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: labelWidth
        }
        RowLayout {
            visible: bridge.catBackend === "hamlib"
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 10
            Rectangle {
                width: 180; height: controlHeight; radius: 4
                color: hamlibUpdateMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                border.color: primaryBlue
                Text { anchors.centerIn: parent; text: qsTr("Open Hamlib update"); color: primaryBlue; font.pixelSize: 12 }
                MouseArea {
                    id: hamlibUpdateMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bridge.openHamlibUpdatePage()
                }
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Windows: DLL updated from the Hamlib site. macOS/Linux: official documentation and releases.")
                wrapMode: Text.Wrap
                color: textSecondary
                font.pixelSize: 11
            }
        }

        // ── ALC AUTO CALIBRATION (1.0.324) ──
        Text {
            text: qsTr("ALC AUTO CALIBRATION")
            color: secondaryCyan
            font.pixelSize: 12
            font.bold: true
            Layout.columnSpan: pageColumns
            Layout.topMargin: 10
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
            height: 1
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
        }

        Text {
            text: qsTr("ALC target:")
            color: textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 100
            ToolTip.visible: alcTargetHover.hovered
            ToolTip.delay: 600
            ToolTip.text: qsTr("ALC scale 0-100. FT8/data: typically 15-25. Values >60 risk overdriving the PA.")
            HoverHandler { id: alcTargetHover }
        }
        SpinBox {
            id: alcTargetSpinBox
            from: 5
            to: 60
            value: bridge.alcTarget
            Layout.fillWidth: true
            implicitHeight: controlHeight
            onValueModified: bridge.setAlcTarget(value)
            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
            contentItem: Text {
                text: alcTargetSpinBox.value
                color: textPrimary
                font.pixelSize: controlFontSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            up.indicator: Rectangle {
                x: alcTargetSpinBox.mirrored ? 0 : parent.width - width
                width: 28; height: parent.height
                color: "transparent"
                Text { anchors.centerIn: parent; text: "+"; color: textPrimary; font.pixelSize: 14 }
            }
            down.indicator: Rectangle {
                x: alcTargetSpinBox.mirrored ? parent.width - width : 0
                width: 28; height: parent.height
                color: "transparent"
                Text { anchors.centerIn: parent; text: "-"; color: textPrimary; font.pixelSize: 14 }
            }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        Text { text: ""; Layout.preferredWidth: 100 }
        RowLayout {
            Layout.fillWidth: true
            Layout.columnSpan: Math.max(1, pageColumns - 1)
            spacing: 10

            Rectangle {
                id: alcCalBtn
                property bool calibrating: bridge.alcCalibrating
                width: 220; height: controlHeight; radius: 4
                color: calibrating
                       ? (alcCalMA.containsMouse ? Qt.rgba(1,0.5,0,0.3) : bgMedium)
                       : (alcCalMA.containsMouse ? Qt.rgba(1,0.6,0,0.3) : bgMedium)
                border.color: calibrating ? "#ff9800" : "#ff9800"
                ToolTip.visible: alcCalMA.containsMouse
                ToolTip.delay: 600
                ToolTip.text: qsTr("Transmits a tune carrier and auto-adjusts the TX audio level until the radio's ALC reaches the target. One-shot. Requires Hamlib CAT connected.")
                Text {
                    anchors.centerIn: parent
                    text: alcCalBtn.calibrating
                          ? qsTr("Cancel calibration")
                          : qsTr("Calibrate ALC (transmits a carrier)")
                    color: "#ff9800"
                    font.pixelSize: 12
                    font.bold: alcCalBtn.calibrating
                }
                MouseArea {
                    id: alcCalMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (bridge.alcCalibrating)
                            bridge.cancelAlcCalibration()
                        else
                            bridge.startAlcCalibration()
                    }
                }
            }
        }

        // 1.0.325 — status label ALC: riga dedicata a tutta larghezza
        RowLayout {
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
            Layout.minimumHeight: bridge.alcCalibrationStatus !== "" ? controlHeight : 0
            visible: bridge.alcCalibrationStatus !== ""
            spacing: 0
            Text {
                Layout.fillWidth: true
                text: bridge.alcCalibrationStatus
                color: bridge.alcCalibrating
                       ? "#ff9800"
                       : (bridge.alcCalibrationStatus.indexOf("Calibration done") >= 0
                          ? accentGreen
                          : "#f44336")
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.columnSpan: pageColumns
            Layout.preferredHeight: dialog.scrollBottomMargin
        }
    }

    // Esito del rilevamento automatico della radio. Sta QUI, accanto al
    // pulsante che lo apre: gli id non attraversano i confini fra componenti,
    // quindi lasciarlo in SettingsDialog.qml dopo la suddivisione in schede
    // avrebbe reso il pulsante muto. E' un Popup, non un Item: non entra nel
    // layout della scheda e si ancora alla finestra.
    RigDetectResults { id: detectRigResults }
}
