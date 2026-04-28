/* Decodium 4.0 — Impostazioni Generali
 * Sostituisce il dialogo impostazioni legacy WSJT-X.
 * Tutte le modifiche sono LIVE (bind diretto alle proprieta bridge).
 * Layout orizzontale: sidebar + StackLayout con GridLayout 4 colonne.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: settingsDialog
    title: qsTr("Settings")
    modal: true
    width: Math.min(Math.round(((parent && parent.width > 0) ? parent.width : 1440) * 0.94), 1520)
    height: Math.min(Math.round(((parent && parent.height > 0) ? parent.height : 960) * 0.94), 980)
    closePolicy: Popup.CloseOnEscape
    property bool positionInitialized: false
    property int currentTab: 0
    readonly property int labelWidth: 140
    readonly property int fieldMinWidth: 300
    readonly property int wideFieldMinWidth: 420
    readonly property int portFieldMinWidth: 190
    property string dataDownloadStatus: ""
    property bool dataDownloadIsError: false
    property string uiFontLabel: bridge.fontSettingLabel("Font", "", 0)
    property string decodedFontLabel: bridge.fontSettingLabel("DecodedTextFont", "Courier", 10)
    property string fontPickerKey: ""
    property string fontPickerFallbackFamily: ""
    property int fontPickerFallbackPointSize: 0
    property string fontPickerFamily: ""
    property int fontPickerPointSize: 10
    property bool fontPickerBold: false
    property bool fontPickerItalic: false
    property bool fontPickerFixedOnly: false
    property string fontPickerSearch: ""
    property var fontPickerFamilies: []
    property bool loggingChecksUpdating: false

    onOpened: {
        Qt.callLater(refreshCatPorts)
    }

    function refreshFontLabels() {
        uiFontLabel = bridge.fontSettingLabel("Font", "", 0)
        decodedFontLabel = bridge.fontSettingLabel("DecodedTextFont", "Courier", 10)
    }

    function boolSetting(key, fallback) {
        var value = bridge.getSetting(key, fallback)
        if (value === true || value === false)
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off" || text.length === 0)
            return false
        return !!fallback
    }

    function setBoolSettingIfChanged(key, value, fallback) {
        if (boolSetting(key, fallback) !== value)
            bridge.setSetting(key, value)
    }

    function setLoggingMode(promptMode) {
        if (loggingChecksUpdating)
            return

        loggingChecksUpdating = true
        promptToLogCheck.checked = promptMode
        autoLogCheck.checked = !promptMode
        bridge.setSetting("PromptToLog", promptMode)
        bridge.setSetting("AutoLog", !promptMode)
        loggingChecksUpdating = false
    }

    function normalizeLoggingModeChecks() {
        loggingChecksUpdating = true
        var promptMode = boolSetting("PromptToLog", false)
        var autoMode = boolSetting("AutoLog", true)
        if (promptMode === autoMode) {
            promptMode = false
            autoMode = true
            bridge.setSetting("PromptToLog", false)
            bridge.setSetting("AutoLog", true)
        }
        promptToLogCheck.checked = promptMode
        autoLogCheck.checked = autoMode
        loggingChecksUpdating = false
    }

    component SettingsComboPopup: Popup {
        id: comboPopup
        property var combo: null
        property int minPopupWidth: 220
        property int maxPopupHeight: 360
        readonly property var comboOrigin: combo && parent ? combo.mapToItem(parent, 0, 0) : Qt.point(0, 0)
        readonly property real wantedHeight: Math.min(maxPopupHeight, comboPopupList.contentHeight + padding * 2)
        readonly property real spaceBelow: parent && combo ? parent.height - comboOrigin.y - combo.height - 8 : maxPopupHeight
        readonly property real spaceAbove: parent && combo ? comboOrigin.y - 8 : 0
        readonly property bool openAbove: wantedHeight > spaceBelow && spaceAbove > spaceBelow

        parent: Overlay.overlay
        modal: false
        focus: true
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: parent ? Math.min(Math.max(combo ? combo.width : 0, minPopupWidth), Math.max(80, parent.width - 16))
                      : Math.max(combo ? combo.width : 0, minPopupWidth)
        height: Math.max(44, Math.min(wantedHeight, Math.max(44, openAbove ? spaceAbove : spaceBelow)))
        x: parent ? Math.max(8, Math.min(comboOrigin.x, parent.width - width - 8)) : 0
        y: parent
           ? (openAbove
              ? Math.max(8, comboOrigin.y - height - 2)
              : Math.min(comboOrigin.y + (combo ? combo.height : 0) + 2, parent.height - height - 8))
           : 0
        onOpened: comboPopupList.forceActiveFocus()

        background: Rectangle {
            color: bgDeep
            border.color: glassBorder
            radius: 4
        }

        contentItem: ListView {
            id: comboPopupList
            anchors.fill: parent
            clip: true
            model: comboPopup.visible && combo ? combo.delegateModel : null
            currentIndex: combo ? combo.highlightedIndex : -1
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            interactive: true
            focus: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }
    }

    function setAlertEnabled(value) {
        bridge.alertSoundsEnabled = value
        bridge.setSetting("alertSoundsEnabled", value)
        bridge.setSetting("alert_Enabled", value)
    }

    function setAlertCq(value) {
        bridge.alertOnCq = value
        bridge.setSetting("alertOnCq", value)
        bridge.setSetting("alert_CQ", value)
    }

    function setAlertMyCall(value) {
        bridge.alertOnMyCall = value
        bridge.setSetting("alertOnMyCall", value)
        bridge.setSetting("alert_MyCall", value)
    }

    function localDirectoryToUrl(path) {
        var text = String(path || "").trim()
        if (text.length === 0)
            return ""
        if (text.indexOf("file:") === 0)
            return text
        text = text.replace(/\\/g, "/")
        if (Qt.platform.os === "windows" && /^[A-Za-z]:\//.test(text))
            return "file:///" + encodeURI(text)
        if (text.charAt(0) === "/")
            return "file://" + encodeURI(text)
        return "file:///" + encodeURI(text)
    }

    function folderUrlToLocalDirectory(url) {
        var text = String(url || "")
        if (text.indexOf("file:///") === 0) {
            var absolutePath = decodeURIComponent(text.substring(8))
            return Qt.platform.os === "windows" ? absolutePath : "/" + absolutePath
        }
        if (text.indexOf("file://") === 0)
            return decodeURIComponent(text.substring(7))
        return decodeURIComponent(text)
    }

    function openDirectoryPicker(settingKey, currentPath) {
        directoryPickerDialog.settingKey = settingKey
        var folderUrl = localDirectoryToUrl(currentPath)
        if (folderUrl.length > 0)
            directoryPickerDialog.currentFolder = folderUrl
        directoryPickerDialog.open()
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            if (key === "Font" || key === "DecodedTextFont")
                settingsDialog.refreshFontLabels()
        }
        function onStatusMessage(msg) {
            var text = String(msg || "")
            var lower = text.toLowerCase()
            if (lower.indexOf("cty.dat") >= 0 || lower.indexOf("call3.txt") >= 0) {
                dataDownloadStatus = text
                dataDownloadIsError = false
            }
        }
        function onErrorMessage(msg) {
            var text = String(msg || "")
            var lower = text.toLowerCase()
            if (lower.indexOf("cty.dat") >= 0 || lower.indexOf("call3.txt") >= 0) {
                dataDownloadStatus = text
                dataDownloadIsError = true
            }
        }
    }

    function activeCatController() {
        return bridge.catManager ? bridge.catManager : null
    }

    function activeCatPortType() {
        var controller = activeCatController()
        if (!controller || controller.portType === undefined || controller.portType === null)
            return "none"
        return String(controller.portType)
    }

    function activeRigName() {
        var controller = activeCatController()
        if (!controller || controller.rigName === undefined || controller.rigName === null)
            return ""
        return String(controller.rigName)
    }

    function activeBaudRateText() {
        var controller = activeCatController()
        if (!controller || controller.baudRate === undefined || controller.baudRate === null)
            return ""
        var text = String(controller.baudRate).trim()
        return text === "0" ? "" : text
    }

    function activeStopBitsText() {
        var controller = activeCatController()
        if (!controller || controller.stopBits === undefined || controller.stopBits === null)
            return "1"
        var text = String(controller.stopBits).trim().toLowerCase()
        if (text === "2" || text === "2.0" || text.indexOf("two") >= 0)
            return "2"
        return "1"
    }

    function stringListIndexOf(list, value) {
        if (!list || value === undefined || value === null)
            return -1
        var wanted = String(value)
        var wantedNorm = wanted.trim().toLowerCase()
        for (var i = 0; i < list.length; ++i) {
            var candidate = String(list[i])
            var candidateNorm = candidate.trim().toLowerCase()
            if (candidate === wanted
                    || candidateNorm === wantedNorm
                    || candidateNorm.indexOf(wantedNorm) !== -1
                    || wantedNorm.indexOf(candidateNorm) !== -1)
                return i
        }
        return -1
    }

    function selectTciRigIfNeeded() {
        var controller = activeCatController()
        if (!controller || controller.rigName === undefined || controller.rigName === null)
            return
        var currentRig = String(controller.rigName || "")
        if (controller.pttMethod !== undefined)
            controller.pttMethod = "CAT"
        if (currentRig.indexOf("TCI Client") === 0)
            return
        var rigs = controller.rigList || []
        var rx1Index = stringListIndexOf(rigs, "TCI Client RX1")
        controller.rigName = rx1Index >= 0 ? String(rigs[rx1Index]) : "TCI Client RX1"
    }

    function normalizedRigName(value) {
        return String(value || "").toUpperCase().replace(/[\s_]+/g, "")
    }

    function rigIsIcom() {
        var rig = normalizedRigName(activeRigName())
        return rig.indexOf("ICOM") !== -1 || rig.indexOf("IC-") !== -1 || rig.indexOf("IC7") !== -1
                || rig.indexOf("IC9") !== -1 || rig.indexOf("IC705") !== -1
    }

    function usesSerialControls() {
        var portType = activeCatPortType()
        return portType === "serial" || portType === "usb"
    }

    function usesNetworkControls() {
        return activeCatPortType() === "network"
    }

    function usesTciControls() {
        return bridge.catBackend === "tci" || activeCatPortType() === "tci"
    }

    function activePttMethod() {
        var controller = activeCatController()
        if (!controller || controller.pttMethod === undefined || controller.pttMethod === null)
            return "CAT"
        var method = String(controller.pttMethod).trim().toUpperCase()
        return method === "" ? "CAT" : method
    }

    function usesSeparatePttPort() {
        var method = activePttMethod()
        return method === "DTR" || method === "RTS"
    }

    function pttPortOptions() {
        var controller = activeCatController()
        var options = []
        if (!controller || !usesSeparatePttPort())
            return options

        if (activeCatPortType() === "serial")
            options.push("CAT")

        var ports = controller.portList || []
        for (var i = 0; i < ports.length; ++i) {
            var port = String(ports[i]).trim()
            if (port !== "" && settingsDialog.stringListIndexOf(options, port) < 0)
                options.push(port)
        }

        var saved = controller.pttPort !== undefined && controller.pttPort !== null
                ? String(controller.pttPort).trim() : ""
        if (saved !== "" && saved.toUpperCase() !== "CAT"
                && settingsDialog.stringListIndexOf(options, saved) < 0)
            options.push(saved)
        return options
    }

    function normalizedPortName(value) {
        var text = String(value || "").trim()
        if (text === "" || text.toUpperCase() === "CAT")
            return "CAT"
        if (text.indexOf("/dev/") === 0)
            text = text.substring(5)
        return text.toLowerCase()
    }

    function pttSharesCatPort() {
        var controller = activeCatController()
        if (!controller)
            return false
        var pttPort = normalizedPortName(controller.pttPort)
        if (pttPort === "CAT")
            return true
        return pttPort === normalizedPortName(controller.serialPort)
    }

    function forceDtrControlEnabled() {
        return activeCatPortType() === "serial"
                && !(activePttMethod() === "DTR" && pttSharesCatPort())
    }

    function forceRtsControlEnabled() {
        var controller = activeCatController()
        var handshake = controller && controller.handshake !== undefined && controller.handshake !== null
                ? String(controller.handshake).trim().toLowerCase() : "none"
        return activeCatPortType() === "serial"
                && handshake !== "hardware"
                && !(activePttMethod() === "RTS" && pttSharesCatPort())
    }

    function enforceForceLineAvailability() {
        var controller = activeCatController()
        if (!controller)
            return
        var changed = false
        if (!forceDtrControlEnabled() && (controller.forceDtr || controller.dtrHigh)) {
            controller.forceDtr = false
            controller.dtrHigh = false
            changed = true
        }
        if (!forceRtsControlEnabled() && (controller.forceRts || controller.rtsHigh)) {
            controller.forceRts = false
            controller.rtsHigh = false
            changed = true
        }
        if (changed)
            scheduleCatPersist()
    }

    function supportsSwrTelemetry() {
        var rig = normalizedRigName(activeRigName())
        if (bridge.catBackend === "omnirig")
            return false
        if (rig.indexOf("OMNIRIG") === 0 || rig.indexOf("DXLAB") === 0 || rig.indexOf("HAMRADIO") === 0)
            return false
        if (rig.indexOf("KENWOODTS-480") === 0 || rig.indexOf("KENWOODTS-850") === 0 || rig.indexOf("KENWOODTS-870") === 0)
            return false
        return true
    }

    function splitModeLabel(value) {
        if (value === "rig")
            return qsTr("Rig")
        if (value === "emulate")
            return qsTr("Fake It")
        return qsTr("None")
    }

    function splitModeOptions() {
        var controller = activeCatController()
        var source = controller && controller.splitModeList ? controller.splitModeList : ["none", "rig", "emulate"]
        var options = []
        for (var i = 0; i < source.length; ++i) {
            var value = String(source[i])
            options.push({ value: value, label: splitModeLabel(value) })
        }
        return options
    }

    function setupChoiceLabel(value) {
        var text = String(value)
        if (text === "None")
            return qsTr("None")
        if (text === "Default")
            return qsTr("Default")
        if (text === "On")
            return qsTr("On")
        if (text === "Off")
            return qsTr("Off")
        if (text === "Mono")
            return qsTr("Mono")
        if (text === "Left")
            return qsTr("Left")
        if (text === "Right")
            return qsTr("Right")
        if (text === "Both")
            return qsTr("Both")
        if (text === "Rear/Data")
            return qsTr("Rear/Data")
        if (text === "Front/Mic")
            return qsTr("Front/Mic")
        return text
    }

    function settingChoiceIndex(key, choices, fallbackIndex) {
        var raw = bridge.getSetting(key, fallbackIndex)
        var numeric = Number(raw)
        if (!isNaN(numeric) && numeric >= 0 && numeric < choices.length)
            return numeric

        var text = String(raw).trim().toLowerCase()
        for (var i = 0; i < choices.length; ++i) {
            if (String(choices[i]).trim().toLowerCase() === text)
                return i
        }
        return fallbackIndex
    }

    function forceLineMode(forceEnabled, highLevel) {
        if (!forceEnabled)
            return "Default"
        return highLevel ? "On" : "Off"
    }

    function applyForceLineValue(lineName, value) {
        var controller = activeCatController()
        if (!controller)
            return
        if (lineName === "dtr" && !forceDtrControlEnabled())
            value = "Default"
        if (lineName === "rts" && !forceRtsControlEnabled())
            value = "Default"
        var forceEnabled = value !== "Default"
        var highLevel = value === "On"
        if (lineName === "dtr") {
            controller.forceDtr = forceEnabled
            controller.dtrHigh = highLevel
        } else {
            controller.forceRts = forceEnabled
            controller.rtsHigh = highLevel
        }
        scheduleCatPersist()
    }

    function resetForcedSerialLines() {
        var controller = activeCatController()
        if (!controller)
            return
        controller.forceDtr = false
        controller.dtrHigh = false
        controller.forceRts = false
        controller.rtsHigh = false
        forceDtrCombo.currentIndex = 0
        forceRtsCombo.currentIndex = 0
    }

    function openTab(index) {
        currentTab = Math.max(0, index)
        open()
    }

    function toggleCatConnection() {
        var controller = activeCatController()
        if (!controller) return
        if (bridge.catConnected) controller.disconnectRig()
        else controller.connectRig()
    }

    function refreshCatPorts() {
        var controller = activeCatController()
        if (controller && controller.refreshPorts) controller.refreshPorts()
    }

    function refreshAudioDevices() {
        if (bridge && bridge.refreshAudioDevices)
            bridge.refreshAudioDevices()
    }

    function filteredFontFamilies() {
        var filter = String(fontPickerSearch || "").trim().toLowerCase()
        if (filter === "")
            return fontPickerFamilies
        var result = []
        for (var i = 0; i < fontPickerFamilies.length; ++i) {
            var family = String(fontPickerFamilies[i])
            if (family.toLowerCase().indexOf(filter) !== -1)
                result.push(family)
        }
        return result
    }

    function openFontPicker(key, fallbackFamily, fallbackPointSize, fixedOnly) {
        fontPickerKey = key
        fontPickerFallbackFamily = fallbackFamily
        fontPickerFallbackPointSize = fallbackPointSize
        fontPickerFixedOnly = fixedOnly
        fontPickerFamilies = bridge.availableFontFamilies(fixedOnly)
        fontPickerFamily = bridge.fontSettingFamily(key, fallbackFamily, fallbackPointSize)
        fontPickerPointSize = bridge.fontSettingPointSize(key, fallbackFamily, fallbackPointSize)
        fontPickerBold = bridge.fontSettingBold(key, fallbackFamily, fallbackPointSize)
        fontPickerItalic = bridge.fontSettingItalic(key, fallbackFamily, fallbackPointSize)
        fontPickerSearch = ""
        fontPicker.open()
    }

    function applyFontPicker() {
        bridge.setFontSetting(fontPickerKey,
                              fontPickerFamily,
                              fontPickerPointSize,
                              fontPickerBold,
                              fontPickerItalic,
                              fontPickerFallbackFamily,
                              fontPickerFallbackPointSize)
        refreshFontLabels()
        fontPicker.close()
    }

    function scheduleCatPersist() {
        var controller = activeCatController()
        if (controller && controller.saveSettings)
            controller.saveSettings()
        catPersistTimer.restart()
    }

    function clampToParent() {
        if (!parent) return
        var parentWidth = parent.width > 0 ? parent.width : width
        var parentHeight = parent.height > 0 ? parent.height : height
        x = Math.max(0, Math.min(x, parentWidth - width))
        y = Math.max(0, Math.min(y, parentHeight - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        var parentWidth = parent.width > 0 ? parent.width : width
        var parentHeight = parent.height > 0 ? parent.height : height
        x = Math.max(0, Math.round((parentWidth - width) / 2))
        y = Math.max(0, Math.round((parentHeight - height) / 2))
        positionInitialized = true
    }

    onAboutToShow: ensureInitialPosition()

    Timer {
        id: catPersistTimer
        interval: 300
        repeat: false
        onTriggered: bridge.saveSettings()
    }

    FolderDialog {
        id: directoryPickerDialog
        property string settingKey: ""
        title: settingKey === "AzElDirectory" ? "Seleziona directory AzEl" : "Seleziona directory salvataggio"
        onAccepted: {
            var path = settingsDialog.folderUrlToLocalDirectory(selectedFolder)
            if (settingKey === "" || path === "")
                return
            bridge.setSetting(settingKey, path)
            if (settingKey === "SaveDirectory")
                saveDirectoryField.text = path
            else if (settingKey === "AzElDirectory")
                azElDirectoryField.text = path
        }
    }

    // ── Theme colors ─────────────────────────────────────────────────────
    property color bgDeep:        bridge.themeManager.bgDeep
    property color bgMedium:      bridge.themeManager.bgMedium
    property color bgLight:       bridge.themeManager.bgLight
    property color primaryBlue:   bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder:   bridge.themeManager.glassBorder
    readonly property int controlHeight: 32
    readonly property int controlFontSize: 12

    // ── Preset colors for color pickers ──────────────────────────────────
    readonly property var presetColors: [
        "#ff0000","#ff6600","#ffcc00","#33cc33","#00ccff","#0066ff",
        "#9933ff","#ff33cc","#ffffff","#cccccc","#666666","#000000"
    ]

    Popup {
        id: fontPicker
        modal: true
        focus: true
        width: Math.min(settingsDialog.width - 80, 760)
        height: Math.min(settingsDialog.height - 80, 680)
        x: Math.round((settingsDialog.width - width) / 2)
        y: Math.round((settingsDialog.height - height) / 2)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: fontSearchField.forceActiveFocus()

        background: Rectangle {
            color: bgDeep
            border.color: secondaryCyan
            border.width: 1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: fontPickerKey === "DecodedTextFont" ? qsTr("Choose Decoded Font") : qsTr("Choose Font")
                color: secondaryCyan
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Search:")
                color: textSecondary
                font.pixelSize: 11
            }

            TextField {
                id: fontSearchField
                Layout.fillWidth: true
                implicitHeight: controlHeight
                text: settingsDialog.fontPickerSearch
                placeholderText: qsTr("filter by name")
                color: textPrimary
                font.pixelSize: controlFontSize
                selectByMouse: true
                onTextChanged: settingsDialog.fontPickerSearch = text
                background: Rectangle {
                    color: bgMedium
                    border.color: parent.activeFocus ? secondaryCyan : glassBorder
                    radius: 4
                }
            }

            Text {
                text: settingsDialog.fontPickerFixedOnly ? qsTr("Monospaced fonts:") : qsTr("Fonts:")
                color: textSecondary
                font.pixelSize: 11
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 190
                color: bgMedium
                border.color: glassBorder
                radius: 4

                ListView {
                    id: fontFamilyList
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: settingsDialog.filteredFontFamilies()
                    currentIndex: -1
                    delegate: ItemDelegate {
                        width: fontFamilyList.width
                        height: 32
                        highlighted: modelData === settingsDialog.fontPickerFamily
                        onClicked: settingsDialog.fontPickerFamily = String(modelData)
                        background: Rectangle {
                            color: parent.highlighted
                                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.32)
                                   : (parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                        }
                        contentItem: Text {
                            text: modelData
                            color: textPrimary
                            font.family: modelData
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 8

                Text { text: qsTr("Selected:"); color: textSecondary; font.pixelSize: 11 }
                Text {
                    text: settingsDialog.fontPickerFamily
                    color: textPrimary
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Size:"); color: textSecondary; font.pixelSize: 11 }
                SpinBox {
                    id: fontPointSpin
                    from: 6
                    to: 48
                    value: settingsDialog.fontPickerPointSize
                    editable: true
                    Layout.preferredWidth: 140
                    onValueChanged: settingsDialog.fontPickerPointSize = value
                    contentItem: TextInput {
                        text: fontPointSpin.textFromValue(fontPointSpin.value, fontPointSpin.locale)
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        readOnly: !fontPointSpin.editable
                        validator: fontPointSpin.validator
                    }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                }

                CheckBox {
                    text: qsTr("Bold")
                    checked: settingsDialog.fontPickerBold
                    onCheckedChanged: settingsDialog.fontPickerBold = checked
                    contentItem: Text { text: parent.text; leftPadding: 26; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                }

                CheckBox {
                    text: qsTr("Italic")
                    checked: settingsDialog.fontPickerItalic
                    onCheckedChanged: settingsDialog.fontPickerItalic = checked
                    contentItem: Text { text: parent.text; leftPadding: 26; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                }

                Item { Layout.fillWidth: true }
                Item { Layout.preferredWidth: 140 }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: glassBorder
                radius: 4
                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "173045  -21  0.1  1045  CQ LB9ZG JP20"
                    color: textPrimary
                    font.family: settingsDialog.fontPickerFamily
                    font.pointSize: settingsDialog.fontPickerPointSize
                    font.bold: settingsDialog.fontPickerBold
                    font.italic: settingsDialog.fontPickerItalic
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: fontPicker.close()
                }
                Button {
                    text: qsTr("Apply")
                    Layout.fillWidth: true
                    enabled: settingsDialog.fontPickerFamily !== ""
                    onClicked: settingsDialog.applyFontPicker()
                }
            }
        }
    }

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan; border.width: 2; radius: 12
    }

    // ── Draggable header ─────────────────────────────────────────────────
    header: Rectangle {
        height: 56
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                settingsDialog.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                settingsDialog.x += mouse.x - clickPos.x
                settingsDialog.y += mouse.y - clickPos.y
                settingsDialog.clampToParent()
            }
        }

        RowLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 10

            Text {
                text: qsTr("Settings")
                font.pixelSize: 16; font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: 34; height: 34; radius: 6
                color: closeMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.3) : Qt.rgba(1,1,1,0.1)
                border.color: closeMA.containsMouse ? "#f44336" : glassBorder
                Text { anchors.centerIn: parent; text: "\u2715"; color: closeMA.containsMouse ? "#f44336" : textPrimary; font.pixelSize: 14 }
                MouseArea { id: closeMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.close() }
            }
        }
    }

    footer: Rectangle {
        height: 64
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)
        border.color: glassBorder
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: qsTr("Changes are applied immediately where supported.")
                color: textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Rectangle {
                width: 110
                height: 36
                radius: 6
                color: closeFooterMA.containsMouse ? Qt.rgba(1,1,1,0.08) : bgMedium
                border.color: glassBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Close")
                    color: textPrimary
                    font.pixelSize: 12
                }

                MouseArea {
                    id: closeFooterMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.close()
                }
            }

            Rectangle {
                width: 110
                height: 36
                radius: 6
                color: okFooterMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.22) : bgMedium
                border.color: accentGreen

                Text {
                    anchors.centerIn: parent
                    text: qsTr("OK")
                    color: accentGreen
                    font.pixelSize: 12
                    font.bold: true
                }

                MouseArea {
                    id: okFooterMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        bridge.saveSettings()
                        settingsDialog.close()
                    }
                }
            }
        }
    }

    // ── Content ──────────────────────────────────────────────────────────
    contentItem: Item {
        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ── Sidebar ──────────────────────────────────────────────
            Rectangle {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)

                Column {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 2

                    Repeater {
                        model: [qsTr("Station"), qsTr("Radio"), qsTr("Audio"), qsTr("TX"), qsTr("Display"), qsTr("Decode"), qsTr("Reporting"), qsTr("Colors"), qsTr("Advanced"), qsTr("Alerts"), qsTr("Filters")]
                        delegate: Rectangle {
                            width: parent.width; height: 36; radius: 6
                            color: tabStack.currentIndex === index ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (tabMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent")
                            border.color: tabStack.currentIndex === index ? primaryBlue : "transparent"
                            Text { anchors.centerIn: parent; text: modelData; color: tabStack.currentIndex === index ? primaryBlue : textSecondary; font.pixelSize: 12 }
                            MouseArea { id: tabMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.currentTab = index }
                        }
                    }
                }
            }

            // Vertical separator
            Rectangle { Layout.fillHeight: true; width: 1; color: glassBorder }

            // ── StackLayout ──────────────────────────────────────────
            StackLayout {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsDialog.currentTab

                // ═══════════ TAB 0 — STAZIONE ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Dettagli Stazione ──
                        Text { text: qsTr("STATION DETAILS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("My Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.callsign; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.callsign = text
                        }
                        Text { text: qsTr("My Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            text: bridge.grid; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.grid = text
                        }

                        Text { text: qsTr("Auto Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AutoGrid", false)
                            onCheckedChanged: bridge.setSetting("AutoGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("IARU Region:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            model: ["1","2","3"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("Region", 0))
                            onActivated: bridge.setSetting("Region", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                        }

                        Text { text: qsTr("Type 2 Msg Gen:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            model: [qsTr("Full"),qsTr("Type 1 prefix"),qsTr("Type 2 prefix")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("Type2MsgGen", 0))
                            onActivated: bridge.setSetting("Type2MsgGen", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                        }
                        Text { text: qsTr("Op Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OpCall", ""); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OpCall", text)
                        }

                        // ── Info Stazione ──
                        Text { text: qsTr("STATION INFO"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Station Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationName; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationName = text
                        }
                        Text { text: qsTr("QTH:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationQth; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationQth = text
                        }

                        Text { text: qsTr("Rig Info:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationRigInfo; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationRigInfo = text
                        }
                        Text { text: qsTr("Antenna:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.stationAntenna; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.stationAntenna = text
                        }

                        Text { text: qsTr("Power (W):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: stPowerSpin
                            from: 0; to: 9999; value: bridge.stationPowerWatts; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth
                            onValueChanged: bridge.stationPowerWatts = value
                            contentItem: TextInput { text: stPowerSpin.textFromValue(stPowerSpin.value, stPowerSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !stPowerSpin.editable; validator: stPowerSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 1 — RADIO ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Backend CAT ──
                        Text { text: qsTr("BACKEND CAT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Backend:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Row {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 6
                            Repeater {
                                model: [["native",qsTr("Native (15 radios)")],["hamlib",qsTr("Hamlib (300+ radios)")],["tci","TCI"],["omnirig","OmniRig"]]
                                delegate: Rectangle {
                                    property string bk: modelData[0]
                                    property bool active: bridge.catBackend === bk
                                    width: 170; height: 30; radius: 6
                                    color: active ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (bkMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent")
                                    border.color: active ? primaryBlue : glassBorder
                                    Text { anchors.centerIn: parent; text: modelData[1]; color: active ? primaryBlue : textSecondary; font.pixelSize: 11 }
                                    MouseArea { id: bkMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            bridge.catBackend = bk
                                            if (bk === "tci")
                                                settingsDialog.selectTciRigIfNeeded()
                                            settingsDialog.scheduleCatPersist()
                                        }
                                    }
                                }
                            }
                        }

                        // Banner: porta seriale occupata da altro software
                        Item {
                            Layout.columnSpan: 4
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
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 8
                            Rectangle { width: 12; height: 12; radius: 6; color: bridge.catConnected ? accentGreen : "#f44336"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: bridge.catConnected ? qsTr("Connected") + " — " + bridge.catRigName + " — " + bridge.catMode : qsTr("Disconnected"); color: bridge.catConnected ? accentGreen : "#f44336"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Item { width: 20; height: 1 }
                            Rectangle {
                                width: 100; height: 28; radius: 6
                                color: connMA.containsMouse ? (bridge.catConnected ? Qt.rgba(0.95,0.26,0.21,0.2) : Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.2)) : "transparent"
                                border.color: bridge.catConnected ? "#f44336" : accentGreen
                                Text { anchors.centerIn: parent; text: bridge.catConnected ? qsTr("Disconnect") : qsTr("Connect"); color: bridge.catConnected ? "#f44336" : accentGreen; font.pixelSize: 11 }
                                MouseArea { id: connMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsDialog.toggleCatConnection()
                                }
                            }
                            Rectangle {
                                width: 28; height: 28; radius: 6
                                color: refreshMA.containsMouse ? bgMedium : "transparent"
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: "↻"; color: secondaryCyan; font.pixelSize: 16 }
                                MouseArea { id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsDialog.refreshCatPorts()
                                }
                            }
                        }

                        // ── Controllo CAT ──
                        Text { text: qsTr("CAT CONTROL"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Rig:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: rigCombo
                            model: bridge.catBackend === "tci" ? ["TCI Client RX1", "TCI Client RX2"] : (bridge.catManager ? bridge.catManager.rigList : []); Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
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
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
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
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text {
                                text: rigCombo.currentIndex >= 0 ? rigCombo.displayText : settingsDialog.activeRigName()
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
                                                          Math.min(settingsDialog.height - 160,
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
                                                          Math.min(settingsDialog.height - 160,
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

                                    TextField {
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
                                            highlighted: modelData === settingsDialog.activeRigName()
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
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("Serial Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        ComboBox {
                            id: serialPortCombo
                            visible: settingsDialog.usesSerialControls()
                            model: bridge.catManager ? bridge.catManager.portList : []; Layout.fillWidth: true; implicitHeight: controlHeight
                            Layout.minimumWidth: wideFieldMinWidth
                            currentIndex: {
                                if (!bridge.catManager)
                                    return -1
                                return find(bridge.catManager.serialPort)
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.serialPort = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
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
                        Text {
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("Baud Rate:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        ComboBox {
                            id: baudCombo
                            visible: settingsDialog.usesSerialControls()
                            model: bridge.catManager && bridge.catManager.baudList ? bridge.catManager.baudList : ["4800","9600","19200","38400","57600","115200"]
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                var baud = settingsDialog.activeBaudRateText()
                                return baud === "" ? -1 : settingsDialog.stringListIndexOf(model, baud)
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.baudRate = parseInt(currentText)
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text {
                                text: baudCombo.currentIndex >= 0 ? baudCombo.displayText : settingsDialog.activeBaudRateText()
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
                            visible: settingsDialog.usesSerialControls() && settingsDialog.rigIsIcom()
                            text: qsTr("CI-V Addr:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        TextField {
                            id: civAddrField
                            visible: settingsDialog.usesSerialControls() && settingsDialog.rigIsIcom()
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: "0x94 (IC-7300)"
                            selectByMouse: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            text: {
                                if (!bridge.catManager || bridge.catManager.civAddress === undefined)
                                    return ""
                                var v = parseInt(bridge.catManager.civAddress)
                                if (!v) return ""
                                return "0x" + v.toString(16).toUpperCase().padStart(2, "0")
                            }
                            onEditingFinished: {
                                if (!bridge.catManager) return
                                var t = text.trim().replace(/^0x/i, "")
                                var v = parseInt(t, 16)
                                if (!isNaN(v) && v >= 0 && v <= 0xFF) {
                                    bridge.catManager.civAddress = v
                                    settingsDialog.scheduleCatPersist()
                                }
                            }
                        }

                        Text {
                            visible: settingsDialog.usesNetworkControls()
                            text: qsTr("Host:Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        TextField {
                            visible: settingsDialog.usesNetworkControls()
                            text: bridge.catManager ? bridge.catManager.networkPort : ""
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: "localhost:4532"
                            selectByMouse: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                if (bridge.catManager) bridge.catManager.networkPort = text
                                settingsDialog.scheduleCatPersist()
                            }
                        }

                        Text {
                            visible: settingsDialog.usesTciControls()
                            text: qsTr("TCI Host:Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        TextField {
                            visible: settingsDialog.usesTciControls()
                            text: bridge.catManager ? bridge.catManager.tciPort : ""
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: "localhost:50001"
                            selectByMouse: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                if (bridge.catManager) bridge.catManager.tciPort = text
                                settingsDialog.scheduleCatPersist()
                            }
                        }

                        Text {
                            visible: settingsDialog.usesTciControls()
                            text: qsTr("TCI Audio:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        CheckBox {
                            visible: settingsDialog.usesTciControls()
                            checked: bridge.catManager ? bridge.catManager.tciAudioEnabled : true
                            text: qsTr("RX/TX via TCI")
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            onCheckedChanged: {
                                if (bridge.catManager) bridge.catManager.tciAudioEnabled = checked
                                settingsDialog.scheduleCatPersist()
                            }
                            contentItem: Text {
                                text: parent.text
                                color: textPrimary
                                font.pixelSize: 12
                                leftPadding: 26
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Text { text: qsTr("PTT Method:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: pttCombo
                            enabled: !settingsDialog.usesTciControls()
                            model: settingsDialog.usesTciControls()
                                   ? ["CAT"]
                                   : (bridge.catManager && bridge.catManager.pttMethodList ? bridge.catManager.pttMethodList : ["CAT","DTR","RTS","VOX"])
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (settingsDialog.usesTciControls())
                                    return 0
                                var methods = (bridge.catManager && bridge.catManager.pttMethodList)
                                              ? bridge.catManager.pttMethodList
                                              : ["CAT","DTR","RTS","VOX"]
                                var savedMethod = bridge.catManager ? bridge.catManager.pttMethod : "CAT"
                                var idx = settingsDialog.stringListIndexOf(methods, savedMethod)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.pttMethod = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
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
                            visible: settingsDialog.usesSeparatePttPort()
                            text: qsTr("PTT Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: labelWidth
                        }
                        ComboBox {
                            id: pttPortCombo
                            visible: settingsDialog.usesSeparatePttPort()
                            model: settingsDialog.pttPortOptions()
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
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: pttPortCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: pttPortCombo }
                        }
                        Item { visible: settingsDialog.usesSeparatePttPort(); Layout.fillWidth: true; Layout.columnSpan: 2 }
                        Text { text: qsTr("Poll Interval (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: pollSpin
                            from: 1; to: 99; value: bridge.catManager ? bridge.catManager.pollInterval : 3; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: {
                                if (bridge.catManager) bridge.catManager.pollInterval = value
                                settingsDialog.scheduleCatPersist()
                            }
                            contentItem: TextInput { text: pollSpin.textFromValue(pollSpin.value, pollSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !pollSpin.editable; validator: pollSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Parametri Seriali ──
                        Text {
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("SERIAL PARAMETERS")
                            color: secondaryCyan
                            font.pixelSize: 12
                            font.bold: true
                            Layout.columnSpan: 4
                            Layout.topMargin: 10
                        }
                        Rectangle {
                            visible: settingsDialog.usesSerialControls()
                            Layout.fillWidth: true
                            Layout.columnSpan: 4
                            height: 1
                            color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3)
                        }

                        Text { visible: settingsDialog.usesSerialControls(); text: qsTr("Data Bits:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: dataBitsCombo
                            visible: settingsDialog.usesSerialControls()
                            model: ["8","7"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (!bridge.catManager)
                                    return 0
                                return Math.max(0, find(String(bridge.catManager.dataBits)))
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.dataBits = currentText
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: dataBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: dataBitsCombo }
                        }
                        Text { visible: settingsDialog.usesSerialControls(); text: qsTr("Stop Bits:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: stopBitsCombo
                            visible: settingsDialog.usesSerialControls()
                            model: ["1","2"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                return settingsDialog.activeStopBitsText() === "2" ? 1 : 0
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.stopBits = currentText
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: stopBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: stopBitsCombo }
                        }

                        Text { visible: settingsDialog.usesSerialControls(); text: qsTr("Handshake:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: handshakeCombo
                            visible: settingsDialog.usesSerialControls()
                            model: ["none","xonxoff","hardware"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (!bridge.catManager)
                                    return 0
                                return Math.max(0, find(String(bridge.catManager.handshake)))
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.handshake = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: handshakeCombo.displayText === "none" ? qsTr("None") : (handshakeCombo.displayText === "xonxoff" ? "XON/XOFF" : qsTr("Hardware")); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData === "none" ? qsTr("None") : (modelData === "xonxoff" ? "XON/XOFF" : qsTr("Hardware")); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: handshakeCombo }
                        }
                        Item { visible: settingsDialog.usesSerialControls(); Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { visible: settingsDialog.usesSerialControls(); enabled: settingsDialog.forceDtrControlEnabled(); text: qsTr("Force DTR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: forceDtrCombo
                            visible: settingsDialog.usesSerialControls()
                            enabled: settingsDialog.forceDtrControlEnabled()
                            model: ["Default","On","Off"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: enabled ? find(settingsDialog.forceLineMode(bridge.catManager ? bridge.catManager.forceDtr : false,
                                                                                      bridge.catManager ? bridge.catManager.dtrHigh : false)) : 0
                            onActivated: settingsDialog.applyForceLineValue("dtr", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(forceDtrCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: forceDtrCombo }
                        }
                        Text { visible: settingsDialog.usesSerialControls(); enabled: settingsDialog.forceRtsControlEnabled(); text: qsTr("Force RTS:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: forceRtsCombo
                            visible: settingsDialog.usesSerialControls()
                            enabled: settingsDialog.forceRtsControlEnabled()
                            model: ["Default","On","Off"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: enabled ? find(settingsDialog.forceLineMode(bridge.catManager ? bridge.catManager.forceRts : false,
                                                                                      bridge.catManager ? bridge.catManager.rtsHigh : false)) : 0
                            onActivated: settingsDialog.applyForceLineValue("rts", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(forceRtsCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: forceRtsCombo }
                        }

                        // ── Operazione Split ──
                        Text { text: qsTr("SPLIT OPERATION"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Split:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: splitCombo
                            model: settingsDialog.splitModeOptions(); Layout.fillWidth: true; implicitHeight: controlHeight
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
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: splitCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData.label; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: splitCombo }
                        }
                        Text { text: qsTr("Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: modeCombo
                            model: ["USB","Data/Pkt","None"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: settingsDialog.settingChoiceIndex("CATMode", model, 0)
                            onActivated: bridge.setSetting("CATMode", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(modeCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: modeCombo }
                        }

                        Text { visible: !settingsDialog.usesTciControls(); text: qsTr("TX Audio Src:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: txAudioSrcCombo
                            visible: !settingsDialog.usesTciControls()
                            model: ["Rear/Data","Front/Mic"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: settingsDialog.settingChoiceIndex("TXAudioSource", model, 0)
                            onActivated: bridge.setSetting("TXAudioSource", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(txAudioSrcCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: txAudioSrcCombo }
                        }
                        Text { visible: settingsDialog.usesTciControls(); text: qsTr("TX Audio:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            visible: settingsDialog.usesTciControls()
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

                        // ── Diagnostica ──
                        Text { text: qsTr("DIAGNOSTICS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Check SWR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.supportsSwrTelemetry() ? bridge.getSetting("CheckSWR", false) : false
                            enabled: settingsDialog.supportsSwrTelemetry()
                            onCheckedChanged: if (enabled) {
                                bridge.setSetting("CheckSWR", checked)
                                if (checked && !bridge.getSetting("PWRandSWR", false))
                                    bridge.setSetting("PWRandSWR", true)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("PWR and SWR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.supportsSwrTelemetry() ? bridge.getSetting("PWRandSWR", false) : false
                            enabled: settingsDialog.supportsSwrTelemetry()
                            onCheckedChanged: if (enabled) bridge.setSetting("PWRandSWR", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 10
                            Rectangle {
                                width: 100; height: controlHeight; radius: 4
                                color: catConnMA.containsMouse ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.3) : bgMedium
                                border.color: accentGreen
                                Text { anchors.centerIn: parent; text: qsTr("Connect"); color: accentGreen; font.pixelSize: 12 }
                                MouseArea { id: catConnMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { var controller = settingsDialog.activeCatController(); if (controller) controller.connectRig() } }
                            }
                            Rectangle {
                                width: 100; height: controlHeight; radius: 4
                                color: catDiscMA.containsMouse ? Qt.rgba(1,0.3,0.3,0.3) : bgMedium
                                border.color: "#f44336"
                                Text { anchors.centerIn: parent; text: qsTr("Disconnect"); color: "#f44336"; font.pixelSize: 12 }
                                MouseArea { id: catDiscMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { var controller = settingsDialog.activeCatController(); if (controller) controller.disconnectRig() } }
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
                            Layout.columnSpan: 3
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
                    }
                }

                // ═══════════ TAB 2 — AUDIO ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Dispositivi Audio ──
                        Text { text: qsTr("AUDIO DEVICES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 4 }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            Layout.preferredWidth: 110
                            Layout.preferredHeight: 28
                            Layout.alignment: Qt.AlignRight
                            radius: 6
                            color: audioRefreshMA.containsMouse ? bgMedium : "transparent"
                            border.color: glassBorder
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("↻  Refresh")
                                color: secondaryCyan
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: audioRefreshMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsDialog.refreshAudioDevices()
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Input Device:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: audioInDevCombo
                            model: bridge.audioInputDevices
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            currentIndex: settingsDialog.stringListIndexOf(bridge.audioInputDevices, bridge.audioInputDevice)
                            onActivated: bridge.audioInputDevice = currentText
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioInDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.width: Math.max(audioInDevCombo.width, 560)
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Input Channel:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: audioInChCombo
                            model: [qsTr("Mono"),qsTr("Left"),qsTr("Right"),qsTr("Both")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            Layout.minimumWidth: fieldMinWidth
                            currentIndex: bridge.audioInputChannel
                            onActivated: bridge.audioInputChannel = currentIndex
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioInChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.columnSpan: 2 }

                        Text { text: qsTr("Output Device:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: audioOutDevCombo
                            model: bridge.audioOutputDevices
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            currentIndex: settingsDialog.stringListIndexOf(bridge.audioOutputDevices, bridge.audioOutputDevice)
                            onActivated: bridge.audioOutputDevice = currentText
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioOutDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.width: Math.max(audioOutDevCombo.width, 560)
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Output Channel:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: audioOutChCombo
                            model: [qsTr("Mono"),qsTr("Left"),qsTr("Right"),qsTr("Both")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            Layout.minimumWidth: fieldMinWidth
                            currentIndex: bridge.audioOutputChannel
                            onActivated: bridge.audioOutputChannel = currentIndex
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioOutChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        // ── Livelli ──
                        Text { text: qsTr("LEVELS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("RX Input Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            id: setupRxInputLevelSlider
                            from: 0; to: 100; live: true; stepSize: 1
                            Layout.fillWidth: true; Layout.columnSpan: 3
                            Binding on value { value: bridge.rxInputLevel; when: !setupRxInputLevelSlider.pressed }
                            onMoved: bridge.rxInputLevel = value
                            onPressedChanged: {
                                if (!pressed && Math.abs(bridge.rxInputLevel - value) >= 0.5)
                                    bridge.rxInputLevel = value
                            }
                        }

                        Text { text: qsTr("TX Output Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            id: setupTxOutputLevelSlider
                            from: 450; to: 0; live: true; stepSize: 1
                            Layout.fillWidth: true; Layout.columnSpan: 3
                            Binding on value { value: bridge.txOutputLevel; when: !setupTxOutputLevelSlider.pressed }
                            onMoved: bridge.txOutputLevel = value
                            onPressedChanged: {
                                if (!pressed && Math.abs(bridge.txOutputLevel - value) >= 0.5)
                                    bridge.txOutputLevel = value
                            }
                        }

                        // ── Directory ──
                        Text { text: qsTr("DIRECTORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Save Directory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            id: saveDirectoryField
                            text: bridge.getSetting("SaveDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            readOnly: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("SaveDirectory", text)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsDialog.openDirectoryPicker("SaveDirectory", saveDirectoryField.text)
                            }
                        }

                        Text { text: qsTr("AzEl Directory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            id: azElDirectoryField
                            text: bridge.getSetting("AzElDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            readOnly: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AzElDirectory", text)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsDialog.openDirectoryPicker("AzElDirectory", azElDirectoryField.text)
                            }
                        }

                        // ── Power Memory ──
                        Text { text: qsTr("POWER MEMORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Band TX Memory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PowerBandTXMemory", false)
                            onCheckedChanged: bridge.setSetting("PowerBandTXMemory", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Band Tune Mem:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PowerBandTuneMemory", false)
                            onCheckedChanged: bridge.setSetting("PowerBandTuneMemory", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 3 — TX ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Frequenza e Timing ──
                        Text { text: qsTr("FREQUENCY AND TIMING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("TX Frequency:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: txFreqSpin
                            from: 0; to: 5000; value: bridge.txFrequency; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: {
                                if (bridge.txFrequency !== value)
                                    bridge.txFrequency = value
                                bridge.setSetting("txFrequency", value)
                            }
                            contentItem: TextInput { text: txFreqSpin.textFromValue(txFreqSpin.value, txFreqSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !txFreqSpin.editable; validator: txFreqSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("TX Slot:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: txSlotCombo
                            model: [qsTr("Second (:15/:45)"), qsTr("First (:00/:30)")]
                            currentIndex: bridge.txPeriod === 1 ? 1 : 0
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            onActivated: {
                                bridge.txPeriod = currentIndex === 1 ? 1 : 0
                                bridge.setSetting("txPeriod", bridge.txPeriod)
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: txSlotCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("TX Delay (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: txDelaySpin
                            from: 0; to: 5; stepSize: 1; value: Math.round(Number(bridge.getSetting("TxDelay", 0.2)) * 10); editable: true
                            textFromValue: function(value, locale) { return Number(value / 10).toLocaleString(locale, "f", 1) }
                            valueFromText: function(text, locale) {
                                var parsed = Number.fromLocaleString(locale, text)
                                return isNaN(parsed) ? 0 : Math.round(parsed * 10)
                            }
                            validator: DoubleValidator { bottom: 0.0; top: 0.5; decimals: 1; notation: DoubleValidator.StandardNotation }
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("TxDelay", value / 10)
                            contentItem: TextInput { text: txDelaySpin.textFromValue(txDelaySpin.value, txDelaySpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !txDelaySpin.editable; validator: txDelaySpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Allow TX QSY:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TxQSYAllowed", false)
                            onCheckedChanged: bridge.setSetting("TxQSYAllowed", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Sequenza Automatica ──
                        Text { text: qsTr("AUTO SEQUENCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Auto Sequence:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.autoSeq
                            onCheckedChanged: {
                                bridge.autoSeq = checked
                                bridge.setSetting("autoSeq", checked)
                                bridge.setSetting("AutoSeq", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Send RR73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.sendRR73
                            onCheckedChanged: {
                                bridge.sendRR73 = checked
                                bridge.setSetting("sendRR73", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Quick QSO:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.quickQsoEnabled
                            onCheckedChanged: {
                                bridge.quickQsoEnabled = checked
                                bridge.setSetting("quickQsoEnabled", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Disable TX after 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("73TxDisable", true)
                            onCheckedChanged: bridge.setSetting("73TxDisable", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("MSK/Q65 TX until 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("RepeatTx", false)
                            onCheckedChanged: bridge.setSetting("RepeatTx", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Watchdog ──
                        Text { text: qsTr("WATCHDOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("TX Watchdog (min):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: txWdSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("TxWatchdog", bridge.txWatchdogMode === 1 ? bridge.txWatchdogTime : 6)); editable: true
                            property bool completed: false
                            function applyWatchdog() {
                                bridge.txWatchdogTime = value
                                bridge.txWatchdogMode = value > 0 ? 1 : 0
                                bridge.setSetting("TxWatchdog", value)
                                bridge.setSetting("txWatchdogMode", bridge.txWatchdogMode)
                                bridge.setSetting("txWatchdogTime", bridge.txWatchdogTime)
                            }
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: if (completed) applyWatchdog()
                            Component.onCompleted: { completed = true; applyWatchdog() }
                            contentItem: TextInput { text: txWdSpin.textFromValue(txWdSpin.value, txWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !txWdSpin.editable; validator: txWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Tune Watchdog (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            CheckBox {
                                id: tuneWdCheck
                                checked: bridge.getSetting("TuneWatchdog", true)
                                onCheckedChanged: bridge.setSetting("TuneWatchdog", checked)
                                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                contentItem: Text { text: ""; leftPadding: 24 }
                            }
                            SpinBox {
                                id: tuneWdSpin
                                from: 0; to: 300; value: Number(bridge.getSetting("TuneWatchdogTime", 90)); editable: true; enabled: tuneWdCheck.checked
                                implicitHeight: controlHeight; Layout.fillWidth: true
                                onValueChanged: bridge.setSetting("TuneWatchdogTime", value)
                                contentItem: TextInput { text: tuneWdSpin.textFromValue(tuneWdSpin.value, tuneWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !tuneWdSpin.editable; validator: tuneWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            }
                        }

                        // ── CW ID ──
                        Text { text: qsTr("CW ID"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("CW ID after 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("After73", false)
                            onCheckedChanged: bridge.setSetting("After73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("CW ID Interval (min):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: cwIdIntSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("IDint", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("IDint", value)
                            contentItem: TextInput { text: cwIdIntSpin.textFromValue(cwIdIntSpin.value, cwIdIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !cwIdIntSpin.editable; validator: cwIdIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Tone Spacing ──
                        Text { text: qsTr("TONE SPACING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("2x Tone Spacing:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: x2ToneSpacingCheck
                            checked: bridge.getSetting("x2ToneSpacing", false)
                            onCheckedChanged: {
                                if (checked) {
                                    x4ToneSpacingCheck.checked = false
                                    bridge.setSetting("x4ToneSpacing", false)
                                }
                                bridge.setSetting("x2ToneSpacing", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("4x Tone Spacing:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: x4ToneSpacingCheck
                            checked: bridge.getSetting("x4ToneSpacing", false)
                            onCheckedChanged: {
                                if (checked) {
                                    x2ToneSpacingCheck.checked = false
                                    bridge.setSetting("x2ToneSpacing", false)
                                }
                                bridge.setSetting("x4ToneSpacing", checked)
                            }
                            Component.onCompleted: {
                                if (checked && x2ToneSpacingCheck.checked) {
                                    x2ToneSpacingCheck.checked = false
                                    bridge.setSetting("x2ToneSpacing", false)
                                }
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Alt F1-F6 Bind:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlternateBindings", false)
                            onCheckedChanged: bridge.setSetting("AlternateBindings", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 4 — DISPLAY ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Font ──
                        Text { text: qsTr("FONT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Font:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlHeight
                                radius: 4
                                color: bgMedium
                                border.color: glassBorder
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: settingsDialog.uiFontLabel
                                    color: textPrimary
                                    font.pixelSize: controlFontSize
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                            Rectangle {
                                width: 78; height: controlHeight; radius: 4
                                color: fontChooseMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: qsTr("Choose"); color: primaryBlue; font.pixelSize: 11 }
                                MouseArea { id: fontChooseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.openFontPicker("Font", "", 0, false) }
                            }
                            Rectangle {
                                width: 64; height: controlHeight; radius: 4
                                color: fontResetMA.containsMouse ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.18) : bgMedium
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: textSecondary; font.pixelSize: 11 }
                                MouseArea { id: fontResetMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.resetFontSetting("Font", "", 0) }
                            }
                        }
                        Text { text: qsTr("Decoded Font:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlHeight
                                radius: 4
                                color: bgMedium
                                border.color: glassBorder
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: settingsDialog.decodedFontLabel
                                    color: textPrimary
                                    font.pixelSize: controlFontSize
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                            Rectangle {
                                width: 78; height: controlHeight; radius: 4
                                color: decodedFontChooseMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: qsTr("Choose"); color: primaryBlue; font.pixelSize: 11 }
                                MouseArea { id: decodedFontChooseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.openFontPicker("DecodedTextFont", "Courier", 10, true) }
                            }
                            Rectangle {
                                width: 64; height: controlHeight; radius: 4
                                color: decodedFontResetMA.containsMouse ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.18) : bgMedium
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: textSecondary; font.pixelSize: 11 }
                                MouseArea { id: decodedFontResetMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.resetFontSetting("DecodedTextFont", "Courier", 10) }
                            }
                        }

                        // ── Decodifiche ──
                        Text { text: qsTr("DECODES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Show DXCC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowDXCC", true)
                            onCheckedChanged: bridge.setSetting("ShowDXCC", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("TX Msg to RX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TXMessagesToRX", true)
                            onCheckedChanged: bridge.setSetting("TXMessagesToRX", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Mappa e Distanza ──
                        Text { text: qsTr("MAP AND DISTANCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Miles:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Miles", false)
                            onCheckedChanged: bridge.setSetting("Miles", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Greyline:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowGreyline", false)
                            onCheckedChanged: bridge.setSetting("ShowGreyline", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Map All Msgs:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapAllMessages", false)
                            onCheckedChanged: bridge.setSetting("MapAllMessages", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Click TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapSingleClickTX", false)
                            onCheckedChanged: bridge.setSetting("MapSingleClickTX", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Allineamento ──
                        Text { text: qsTr("ALIGNMENT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Align:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Align", false)
                            onCheckedChanged: bridge.setSetting("Align", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Align Steps:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: alignStepsSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AlignSteps", value)
                            contentItem: TextInput { text: alignStepsSpin.textFromValue(alignStepsSpin.value, alignStepsSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !alignStepsSpin.editable; validator: alignStepsSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Align Steps 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: alignSteps2Spin
                            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps2", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AlignSteps2", value)
                            contentItem: TextInput { text: alignSteps2Spin.textFromValue(alignSteps2Spin.value, alignSteps2Spin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !alignSteps2Spin.editable; validator: alignSteps2Spin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 5 — DECODIFICA ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Parametri Decodifica ──
                        Text { text: qsTr("DECODE PARAMETERS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Decode Depth:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: decodeDepthCombo
                            model: [qsTr("Fast"),qsTr("Normal"),qsTr("Deep")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: bridge.ndepth - 1
                            onActivated: bridge.ndepth = currentIndex + 1
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: decodeDepthCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("Low Freq (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: nfaSpin
                            from: 0; to: 5000; value: bridge.nfa; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.nfa = value
                            contentItem: TextInput { text: nfaSpin.textFromValue(nfaSpin.value, nfaSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !nfaSpin.editable; validator: nfaSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("High Freq (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: nfbSpin
                            from: 0; to: 5000; value: bridge.nfb; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.nfb = value
                            contentItem: TextInput { text: nfbSpin.textFromValue(nfbSpin.value, nfbSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !nfbSpin.editable; validator: nfbSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("RX Bandwidth:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: rxBwSpin
                            from: 100; to: 5000; value: Number(bridge.getSetting("RXBandwidth", 2500)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("RXBandwidth", value)
                            contentItem: TextInput { text: rxBwSpin.textFromValue(rxBwSpin.value, rxBwSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !rxBwSpin.editable; validator: rxBwSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Decode at 52s:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DecodeAt52s", false)
                            onCheckedChanged: bridge.setSetting("DecodeAt52s", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Single Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("SingleDecode", false)
                            onCheckedChanged: bridge.setSetting("SingleDecode", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── JT65 VHF/UHF ──
                        Text { text: qsTr("JT65 VHF/UHF"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Erasure Patterns:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: erasurePatSpin
                            from: 0; to: 99999; value: Number(bridge.getSetting("RandomErasurePatterns", 7)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("RandomErasurePatterns", value)
                            contentItem: TextInput { text: erasurePatSpin.textFromValue(erasurePatSpin.value, erasurePatSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !erasurePatSpin.editable; validator: erasurePatSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Aggressive:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: aggressiveSpin
                            from: 0; to: 10; value: Number(bridge.getSetting("AggressiveLevel", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AggressiveLevel", value)
                            contentItem: TextInput { text: aggressiveSpin.textFromValue(aggressiveSpin.value, aggressiveSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !aggressiveSpin.editable; validator: aggressiveSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Two-Pass:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TwoPassDecoding", false)
                            onCheckedChanged: bridge.setSetting("TwoPassDecoding", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Sidelobe Control ──
                        Text { text: qsTr("SIDELOBE CONTROL"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Sidelobe Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: sidelobeCombo
                            model: [qsTr("Low Sidelobes"),qsTr("Max Sensitivity")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("SidelobeMode", 0))
                            onActivated: bridge.setSetting("SidelobeMode", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: sidelobeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Degrade S/N:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: degradeSnSpin
                            from: 0; to: 100; value: Number(bridge.getSetting("DegradeSN", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("DegradeSN", value)
                            contentItem: TextInput { text: degradeSnSpin.textFromValue(degradeSnSpin.value, degradeSnSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !degradeSnSpin.editable; validator: degradeSnSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Filtri Decodifica ──
                        Text { text: qsTr("DECODE FILTERS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("CQ Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.filterCqOnly
                            onCheckedChanged: bridge.filterCqOnly = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("My Call Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.filterMyCallOnly
                            onCheckedChanged: bridge.filterMyCallOnly = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Zap:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.zapEnabled
                            onCheckedChanged: bridge.zapEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Deep Search:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.deepSearchEnabled
                            onCheckedChanged: bridge.deepSearchEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("AP Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.ft8ApEnabled
                            onCheckedChanged: bridge.ft8ApEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Avg Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.avgDecodeEnabled
                            onCheckedChanged: bridge.avgDecodeEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 6 — REPORTING ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Servizi di Rete ──
                        Text { text: qsTr("NETWORK SERVICES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("PSK Reporter:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.pskReporterEnabled
                            onCheckedChanged: bridge.pskReporterEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("TCP/IP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PSKRtcpip", false)
                            onCheckedChanged: bridge.setSetting("PSKRtcpip", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── DX Cluster ──
                        Text { text: qsTr("DX CLUSTER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            id: dxClusterHostField
                            text: bridge.dxCluster ? bridge.dxCluster.host : ""
                            Layout.fillWidth: true
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: "dx.iz7auh.net"
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: if (bridge.dxCluster) bridge.dxCluster.host = text.trim()
                        }
                        Text { text: qsTr("Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: dxClusterPortSpin
                            from: 1; to: 65535
                            value: bridge.dxCluster ? bridge.dxCluster.port : 8000
                            editable: true
                            implicitHeight: controlHeight
                            Layout.fillWidth: true
                            Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: if (bridge.dxCluster) bridge.dxCluster.port = value
                            contentItem: TextInput { text: dxClusterPortSpin.textFromValue(dxClusterPortSpin.value, dxClusterPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !dxClusterPortSpin.editable; validator: dxClusterPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            spacing: 10

                            Text {
                                text: bridge.dxCluster && bridge.dxCluster.connected ? qsTr("Connected") : qsTr("Disconnected")
                                color: bridge.dxCluster && bridge.dxCluster.connected ? accentGreen : textSecondary
                                font.pixelSize: 12
                            }

                            Rectangle {
                                width: 96; height: controlHeight; radius: 4
                                color: dxClusterConnMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : bgMedium
                                border.color: accentGreen
                                Text { anchors.centerIn: parent; text: qsTr("Connect"); color: accentGreen; font.pixelSize: 12 }
                                MouseArea {
                                    id: dxClusterConnMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (!bridge.dxCluster) return
                                        bridge.dxCluster.host = dxClusterHostField.text.trim()
                                        bridge.dxCluster.port = dxClusterPortSpin.value
                                        bridge.dxCluster.callsign = bridge.callsign
                                        bridge.connectDxCluster()
                                    }
                                }
                            }

                            Rectangle {
                                width: 110; height: controlHeight; radius: 4
                                color: dxClusterDiscMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.2) : bgMedium
                                border.color: "#f44336"
                                Text { anchors.centerIn: parent; text: qsTr("Disconnect"); color: "#f44336"; font.pixelSize: 12 }
                                MouseArea {
                                    id: dxClusterDiscMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.disconnectDxCluster()
                                }
                            }
                        }

                        Text { text: qsTr("Detail:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        Text {
                            text: bridge.dxCluster && bridge.dxCluster.lastStatus ? bridge.dxCluster.lastStatus : qsTr("No message")
                            color: textSecondary
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                        }

                        // ── Cloudlog ──
                        Text { text: qsTr("CLOUDLOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.cloudlogEnabled
                            onCheckedChanged: bridge.cloudlogEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("API URL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.cloudlogUrl; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.cloudlogUrl = text
                        }

                        Text { text: qsTr("API Key:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.cloudlogApiKey; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.cloudlogApiKey = text
                        }

                        Text { text: qsTr("Station ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: cloudlogStIdSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("CloudlogStationID", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("CloudlogStationID", value)
                            contentItem: TextInput { text: cloudlogStIdSpin.textFromValue(cloudlogStIdSpin.value, cloudlogStIdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !cloudlogStIdSpin.editable; validator: cloudlogStIdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── LotW ──
                        Text { text: qsTr("LOTW"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("LotW Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.lotwEnabled
                            onCheckedChanged: bridge.lotwEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Password:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("LoTWPassword", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("LoTWPassword", text)
                        }

                        Text { text: qsTr("Non-QSL'd:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LoTWNonQSL", false)
                            onCheckedChanged: bridge.setSetting("LoTWNonQSL", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Days Upload:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: lotwDaysSpin
                            from: 0; to: 9999; value: Number(bridge.getSetting("LoTWDaysSinceUpload", 365)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("LoTWDaysSinceUpload", value)
                            contentItem: TextInput { text: lotwDaysSpin.textFromValue(lotwDaysSpin.value, lotwDaysSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !lotwDaysSpin.editable; validator: lotwDaysSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Logging ──
                        Text { text: qsTr("LOGGING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Prompt to Log:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: promptToLogCheck
                            checked: boolSetting("PromptToLog", false)
                            onToggled: {
                                if (!settingsDialog.loggingChecksUpdating)
                                    settingsDialog.setLoggingMode(checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Auto Log:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: autoLogCheck
                            checked: boolSetting("AutoLog", true)
                            onToggled: {
                                if (!settingsDialog.loggingChecksUpdating)
                                    settingsDialog.setLoggingMode(!checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            Component.onCompleted: Qt.callLater(function() { settingsDialog.normalizeLoggingModeChecks() })
                        }

                        Text { text: qsTr("Log as RTTY:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LogAsRTTY", false)
                            onCheckedChanged: bridge.setSetting("LogAsRTTY", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("4-digit Grids:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Log4DigitGrids", false)
                            onCheckedChanged: bridge.setSetting("Log4DigitGrids", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Contest Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            enabled: !promptToLogCheck.checked
                            opacity: enabled ? 1.0 : 0.45
                            checked: bridge.getSetting("ContestingOnly", true)
                            onCheckedChanged: bridge.setSetting("ContestingOnly", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Spec Op Cmts:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("SpecOpInComments", false)
                            onCheckedChanged: bridge.setSetting("SpecOpInComments", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("dB in Cmts:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("dBReportsToComments", false)
                            onCheckedChanged: bridge.setSetting("dBReportsToComments", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("ZZ00:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ZZ00", false)
                            onCheckedChanged: bridge.setSetting("ZZ00", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Registrazione ──
                        Text { text: qsTr("RECORDING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Record RX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.recordRxEnabled
                            onCheckedChanged: bridge.recordRxEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Record TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.recordTxEnabled
                            onCheckedChanged: bridge.recordTxEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("WSPR Upload:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.wsprUploadEnabled
                            onCheckedChanged: bridge.wsprUploadEnabled = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Remote Web Dashboard ──
                        Text { text: qsTr("REMOTE WEB DASHBOARD (LAN)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("RemoteWebEnabled", false)
                            onCheckedChanged: bridge.setSetting("RemoteWebEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("HTTP port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: remoteHttpPortSpin
                            from: 1025; to: 65535; value: Number(bridge.getSetting("RemoteHttpPort", 19091)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("RemoteHttpPort", value)
                            contentItem: TextInput { text: remoteHttpPortSpin.textFromValue(remoteHttpPortSpin.value, remoteHttpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !remoteHttpPortSpin.editable; validator: remoteHttpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("WS socket port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            readOnly: true
                            text: String(bridge.remoteWebSocketPort())
                            Layout.fillWidth: true
                            Layout.preferredWidth: portFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("WS bind:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            text: bridge.getSetting("RemoteWsBind", "0.0.0.0"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RemoteWsBind", text)
                        }
                        Text { text: qsTr("Username:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            text: bridge.getSetting("RemoteUser", "admin"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RemoteUser", text)
                        }

                        Text { text: qsTr("Access token:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("RemoteToken", ""); Layout.fillWidth: true; Layout.columnSpan: 3; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            placeholderText: qsTr("Required for LAN/WAN")
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RemoteToken", text)
                        }

                        Text {
                            text: qsTr("App restart required. For LAN/WAN, use a token of at least 12 characters.")
                            color: textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Layout.columnSpan: 4
                        }

                        // ── UDP Server ──
                        Text { text: qsTr("UDP SERVER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Client ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            id: udpClientIdField
                            text: bridge.getSetting("UDPClientId", "WSJTX")
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            maximumLength: 64
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            inputMethodHints: Qt.ImhNoPredictiveText
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: {
                                var cleaned = String(text).trim()
                                if (!cleaned.length)
                                    cleaned = "WSJTX"
                                if (cleaned !== text)
                                    text = cleaned
                                bridge.setSetting("UDPClientId", cleaned)
                            }
                        }
                        Text { text: qsTr("Preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: udpClientIdPreset
                            model: ["WSJTX", "Decodium"]
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: currentIndex = Math.max(0, find(String(bridge.getSetting("UDPClientId", "WSJTX"))))
                            onActivated: {
                                udpClientIdField.text = currentText
                                bridge.setSetting("UDPClientId", currentText)
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpClientIdPreset.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Server Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            text: bridge.getSetting("UDPServer", "127.0.0.1"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("UDPServer", text)
                        }
                        Text { text: qsTr("Server Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPServerPort", 2237)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPServerPort", value)
                            contentItem: TextInput { text: udpPortSpin.textFromValue(udpPortSpin.value, udpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpPortSpin.editable; validator: udpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Listen Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpListenSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPListenPort", 2238)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPListenPort", value)
                            contentItem: TextInput { text: udpListenSpin.textFromValue(udpListenSpin.value, udpListenSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpListenSpin.editable; validator: udpListenSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Multicast TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpTtlSpin
                            from: 0; to: 255; value: Number(bridge.getSetting("UDPTTL", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPTTL", value)
                            contentItem: TextInput { text: udpTtlSpin.textFromValue(udpTtlSpin.value, udpTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpTtlSpin.editable; validator: udpTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Interface Used:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: udpInterfaceCombo
                            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: {
                                var saved = bridge.udpInterfaceName()
                                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
                            }
                            onActivated: bridge.setUdpInterfaceName(currentIndex <= 0 ? "" : currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Send ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: udpPrimaryAdifCheck
                            checked: boolSetting("UDPPrimaryLoggedAdifEnabled", true)
                            onToggled: setBoolSettingIfChanged("UDPPrimaryLoggedAdifEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Secondary UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: udpSecondaryCheck
                            checked: boolSetting("UDPSecondaryEnabled", true)
                            onToggled: setBoolSettingIfChanged("UDPSecondaryEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Secondary Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            text: bridge.getSetting("UDPSecondaryServer", bridge.getSetting("UDPServer", "127.0.0.1")); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("UDPSecondaryServer", text)
                        }

                        Text { text: qsTr("Secondary Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpSecondaryPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPSecondaryServerPort", 2239)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPSecondaryServerPort", value)
                            contentItem: TextInput { text: udpSecondaryPortSpin.textFromValue(udpSecondaryPortSpin.value, udpSecondaryPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpSecondaryPortSpin.editable; validator: udpSecondaryPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Secondary TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpSecondaryTtlSpin
                            from: 0; to: 255; value: Number(bridge.getSetting("UDPSecondaryTTL", bridge.getSetting("UDPTTL", 1))); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPSecondaryTTL", value)
                            contentItem: TextInput { text: udpSecondaryTtlSpin.textFromValue(udpSecondaryTtlSpin.value, udpSecondaryTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !udpSecondaryTtlSpin.editable; validator: udpSecondaryTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Secondary Interface:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        ComboBox {
                            id: udpSecondaryInterfaceCombo
                            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: {
                                var saved = String(bridge.getSetting("UDPSecondaryInterface", ""))
                                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
                            }
                            onActivated: bridge.setSetting("UDPSecondaryInterface", currentIndex <= 0 ? "" : currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpSecondaryInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Secondary ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: udpSecondaryAdifCheck
                            checked: boolSetting("UDPSecondaryLoggedAdifEnabled", true)
                            onToggled: setBoolSettingIfChanged("UDPSecondaryLoggedAdifEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Accept UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            // Default allineato con Configuration.cpp (true) per evitare
                            // che il primo onCheckedChanged scriva `false` nel legacy INI
                            // prima che Configuration abbia fatto write_settings.
                            checked: bridge.getSetting("AcceptUDPRequests", true)
                            onCheckedChanged: bridge.setSetting("AcceptUDPRequests", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Notify Request:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("NotifyOnRequest", false)
                            onCheckedChanged: bridge.setSetting("NotifyOnRequest", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Restore Win:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("udpWindowRestore", false)
                            onCheckedChanged: bridge.setSetting("udpWindowRestore", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── ADIF TCP ──
                        Text { text: qsTr("ADIF TCP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enable TCP ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: adifTcpCheck
                            checked: boolSetting("ADIFTcpEnabled", false)
                            onToggled: setBoolSettingIfChanged("ADIFTcpEnabled", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("TCP Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: adifTcpPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("ADIFTcpPort", 52001)); editable: true
                            enabled: adifTcpCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("ADIFTcpPort", value)
                            contentItem: TextInput { text: adifTcpPortSpin.textFromValue(adifTcpPortSpin.value, adifTcpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !adifTcpPortSpin.editable; validator: adifTcpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: adifTcpPortSpin.enabled }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("TCP Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        TextField {
                            text: bridge.getSetting("ADIFTcpServer", "127.0.0.1"); Layout.fillWidth: true; Layout.columnSpan: 3; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            enabled: adifTcpCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("ADIFTcpServer", text)
                        }

                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            Layout.preferredHeight: 96
                        }
                    }
                }

                // ═══════════ TAB 7 — COLORI ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Colori Decodifica ──
                        Text { text: qsTr("DECODE COLORS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        // Color CQ
                        Text { text: qsTr("Color CQ:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorCqRect
                            width: 60; height: 24; radius: 4; color: bridge.colorCQ; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorCqPop.open() }
                            Popup {
                                id: colorCqPop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorCQ = modelData; bridge.setSetting("colorCQ", modelData); colorCqPop.close() } }
                                    } }
                                }
                            }
                        }
                        // Color My Call
                        Text { text: qsTr("Color My Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorMyCallRect
                            width: 60; height: 24; radius: 4; color: bridge.colorMyCall; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorMyCallPop.open() }
                            Popup {
                                id: colorMyCallPop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorMyCall = modelData; bridge.setSetting("colorMyCall", modelData); colorMyCallPop.close() } }
                                    } }
                                }
                            }
                        }

                        // Color DX Entity
                        Text { text: qsTr("Color DX Entity:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorDxEntRect
                            width: 60; height: 24; radius: 4; color: bridge.colorDXEntity; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorDxEntPop.open() }
                            Popup {
                                id: colorDxEntPop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorDXEntity = modelData; bridge.setSetting("colorDXEntity", modelData); colorDxEntPop.close() } }
                                    } }
                                }
                            }
                        }
                        // Color 73
                        Text { text: qsTr("Color 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: color73Rect
                            width: 60; height: 24; radius: 4; color: bridge.color73; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: color73Pop.open() }
                            Popup {
                                id: color73Pop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.color73 = modelData; bridge.setSetting("color73", modelData); color73Pop.close() } }
                                    } }
                                }
                            }
                        }

                        // Color B4
                        Text { text: qsTr("Color B4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            id: colorB4Rect
                            width: 60; height: 24; radius: 4; color: bridge.colorB4; border.color: glassBorder
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: colorB4Pop.open() }
                            Popup {
                                id: colorB4Pop; width: 200; height: 80; background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                Flow { anchors.fill: parent; anchors.margins: 8; spacing: 4
                                    Repeater { model: presetColors; delegate: Rectangle { width: 20; height: 20; radius: 3; color: modelData; border.color: glassBorder
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bridge.colorB4 = modelData; bridge.setSetting("colorB4", modelData); colorB4Pop.close() } }
                                    } }
                                }
                            }
                        }
                        Text { text: qsTr("B4 Strikethrough:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.b4Strikethrough
                            onCheckedChanged: {
                                bridge.b4Strikethrough = checked
                                bridge.setSetting("b4Strikethrough", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Highlighting ──
                        Text { text: qsTr("HIGHLIGHTING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Highlight 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Highlight73", true)
                            onCheckedChanged: bridge.setSetting("Highlight73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("HL Orange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightOrange", false)
                            onCheckedChanged: bridge.setSetting("HighlightOrange", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Orange Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("HighlightOrangeCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("HighlightOrangeCallsigns", text.toUpperCase())
                        }

                        Text { text: qsTr("HL Blue:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightBlue", false)
                            onCheckedChanged: bridge.setSetting("HighlightBlue", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Blue Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("HighlightBlueCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("HighlightBlueCallsigns", text.toUpperCase())
                        }

                        // ── Spettro ──
                        Text { text: qsTr("SPECTRUM"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Palette:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: paletteCombo
                            model: ["SDR Classic","Raptor Green","Grayscale","SmartSDR","Hot (SDR#)","deskHPSDR","Aether Default","Aether BlueGreen","Aether Fire","Aether Plasma","FlexRadio"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            currentIndex: Math.max(0, bridge.uiPaletteIndex)
                            onActivated: {
                                bridge.uiPaletteIndex = currentIndex
                                bridge.setSetting("uiPaletteIndex", currentIndex)
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: paletteCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Black Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 0; to: 100; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallBlackLevel", 15)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("uiWaterfallBlackLevel", value)
                        }

                        Text { text: qsTr("Color Gain:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 0; to: 100; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallColorGain", 50)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("uiWaterfallColorGain", value)
                        }

                        Text { text: qsTr("Contrast:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 10; to: 150; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallContrast", 80)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("uiWaterfallContrast", value)
                        }

                        // ── Download Dati ──
                        Text { text: qsTr("DATA DOWNLOAD"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 10
                            Rectangle {
                                width: 170; height: controlHeight; radius: 4
                                opacity: bridge.ctyDatUpdating ? 0.65 : 1.0
                                color: dlCtyMA.containsMouse && !bridge.ctyDatUpdating ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.ctyDatUpdating ? "Download CTY.dat..." : "Download CTY.dat"
                                    color: primaryBlue
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    id: dlCtyMA
                                    anchors.fill: parent
                                    enabled: !bridge.ctyDatUpdating
                                    hoverEnabled: true
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        dataDownloadStatus = "Verifica cty.dat..."
                                        dataDownloadIsError = false
                                        bridge.checkCtyDatUpdate()
                                    }
                                }
                            }
                            Rectangle {
                                width: 190; height: controlHeight; radius: 4
                                color: dlCall3MA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: qsTr("Download CALL3.TXT"); color: primaryBlue; font.pixelSize: 12 }
                                MouseArea { id: dlCall3MA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.downloadCall3Txt() }
                            }
                        }
                        Text {
                            text: dataDownloadStatus.length > 0 ? dataDownloadStatus : "Dopo il click compare qui un messaggio con esito o errore."
                            color: dataDownloadIsError ? "#ff5555" : (dataDownloadStatus.length > 0 ? secondaryCyan : textSecondary)
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Layout.columnSpan: 4
                        }
                    }
                }

                // ═══════════ TAB 8 — AVANZATE ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Avvio ──
                        Text { text: qsTr("STARTUP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Monitor OFF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("MonitorOFF", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("MonitorOFF", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Monitor Last:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("MonitorLastUsed", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("MonitorLastUsed", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Auto Astro:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AutoAstroWindow", false)
                            onCheckedChanged: bridge.setSetting("AutoAstroWindow", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("kHz no k:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("kHzWithoutK", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("kHzWithoutK", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Progress Red:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("ProgressBarRed", true)
                            onToggled: settingsDialog.setBoolSettingIfChanged("ProgressBarRed", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("High DPI:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("HighDPI", true)
                            onToggled: settingsDialog.setBoolSettingIfChanged("HighDPI", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Larger Tab:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LargerTabWidget", false)
                            onCheckedChanged: bridge.setSetting("LargerTabWidget", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Comportamento ──
                        Text { text: qsTr("BEHAVIOR"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Quick Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("QuickCall", true)
                            onToggled: settingsDialog.setBoolSettingIfChanged("QuickCall", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Force Call 1st:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("ForceCallFirst", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("ForceCallFirst", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("VHF/UHF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.vhfUhfFeatures
                            onToggled: {
                                bridge.vhfUhfFeatures = checked
                                bridge.setSetting("VHFUHF", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Wait Features:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("WaitFeaturesEnabled", true)
                            onToggled: settingsDialog.setBoolSettingIfChanged("WaitFeaturesEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Erase Band Act:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("erase_BandActivity", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("erase_BandActivity", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Clear DX Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("clear_DXgrid", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("clear_DXgrid", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Clear DX Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("clear_DXcall", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("clear_DXcall", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("RX>TX after QSO:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("set_RXtoTX", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("set_RXtoTX", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Alt Erase Btn:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("AlternateEraseButtonBehavior", true)
                            onToggled: settingsDialog.setBoolSettingIfChanged("AlternateEraseButtonBehavior", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("No Btn Color:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("TxWarningDisabled", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("TxWarningDisabled", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Modo Operativo ──
                        Text { text: qsTr("OPERATING MODE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Fox Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.foxMode
                            onToggled: bridge.foxMode = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Hound Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.houndMode
                            onToggled: bridge.houndMode = checked
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("SuperFox:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("SuperFox", true)
                            onToggled: settingsDialog.setBoolSettingIfChanged("SuperFox", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Show OTP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("ShowOTP", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("ShowOTP", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Contest ──
                        Text { text: qsTr("CONTEST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Activity:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ComboBox {
                            id: contestCombo
                            model: [qsTr("None"),"NA VHF","EU VHF",qsTr("Field Day"),"RTTY Roundup","WW DIGI",qsTr("Fox"),qsTr("Hound"),"ARRL Digi","Q65 Pileup"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            currentIndex: Math.max(0, Math.min(model.length - 1, bridge.specialOperationActivity))
                            onActivated: {
                                bridge.specialOperationActivity = currentIndex
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: contestCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("FD Exchange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("Field_Day_Exchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Field_Day_Exchange", text.toUpperCase())
                        }
                        Text { text: qsTr("RTTY Exchange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("RTTY_Exchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RTTY_Exchange", text.toUpperCase())
                        }

                        Text { text: qsTr("Contest Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("Contest_Name", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Contest_Name", text.toUpperCase())
                        }
                        Text { text: qsTr("Indiv Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("Individual_Contest_Name", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("Individual_Contest_Name", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("NCCC Sprint:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("NCCC_Sprint", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("NCCC_Sprint", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── NTP Time Sync ──
                        Text { text: qsTr("NTP TIME SYNC"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enable NTP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.ntpEnabled
                            onClicked: bridge.setSetting("NTPEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text {
                            text: bridge.ntpEnabled
                                  ? (bridge.ntpSynced ? "Synced" : "Syncing / waiting reply")
                                  : "Disabled"
                            color: bridge.ntpEnabled ? (bridge.ntpSynced ? accentGreen : "#FF9800") : textSecondary
                            font.pixelSize: 12
                            Layout.columnSpan: 2
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text { text: qsTr("Custom Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            id: ntpServerField
                            text: bridge.getSetting("NTPCustomServer", "")
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: qsTr("Empty = automatic public servers")
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: bridge.setSetting("NTPCustomServer", text.trim())
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            radius: 4
                            color: ntpSyncNowMouse.containsMouse && bridge.ntpEnabled
                                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25)
                                   : bgMedium
                            border.color: bridge.ntpEnabled ? secondaryCyan : glassBorder
                            opacity: bridge.ntpEnabled ? 1.0 : 0.55
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("Sync Now")
                                color: bridge.ntpEnabled ? textPrimary : textSecondary
                                font.pixelSize: 12
                                font.bold: bridge.ntpEnabled
                            }
                            MouseArea {
                                id: ntpSyncNowMouse
                                anchors.fill: parent
                                enabled: bridge.ntpEnabled
                                hoverEnabled: true
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: bridge.syncNtpNow()
                            }
                        }

                        Text {
                            text: qsTr("Leave the server empty to automatically use pool.ntp.org, Apple, Cloudflare, and Google.")
                            color: textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.columnSpan: 4
                        }

                        // ── OTP ──
                        Text { text: qsTr("OTP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("OTP Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("OTPEnabled", false)
                            onCheckedChanged: bridge.setSetting("OTPEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("OTP Seed:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OTPSeed", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OTPSeed", text)
                        }

                        Text { text: qsTr("OTP Interval:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: otpIntSpin
                            from: 1; to: 3600; value: Number(bridge.getSetting("OTPinterval", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("OTPinterval", value)
                            contentItem: TextInput { text: otpIntSpin.textFromValue(otpIntSpin.value, otpIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; readOnly: !otpIntSpin.editable; validator: otpIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("OTP URL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField {
                            text: bridge.getSetting("OTPUrl", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OTPUrl", text)
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 4; Layout.preferredHeight: 80 }
                    }
                }

                // ═══════════ TAB 9 — ALERTS ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Audio Alerts ──
                        Text { text: qsTr("AUDIO ALERTS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Alerts Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertSoundsEnabled
                            onToggled: settingsDialog.setAlertEnabled(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // Alert grid
                        Text { text: qsTr("CQ in Msg:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertOnCq
                            onToggled: settingsDialog.setAlertCq(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("My Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertOnMyCall
                            onToggled: settingsDialog.setAlertMyCall(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New DXCC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_DXCC", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_DXCC", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("New DXCC Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_DXCCOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_DXCCOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_Grid", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_Grid", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("New Grid Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_GridOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_GridOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New Continent:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_Continent", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_Continent", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("New Cont Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_ContinentOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_ContinentOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New CQ Zone:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_CQZ", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_CQZ", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("CQ Zone Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_CQZOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_CQZOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New ITU Zone:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_ITUZ", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_ITUZ", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("ITU Zone Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_ITUZOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_ITUZOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("DX Call/Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_DXcall", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_DXcall", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("QSY Message:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_QSYmessage", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_QSYmessage", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 10 — FILTRI ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: parent.width - 20
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

                        // ── Blacklist ──
                        Text { text: qsTr("BLACKLIST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("Blacklisted", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("Blacklisted", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // Blacklist 1-12 (2 per row)
                        Text { text: qsTr("Blacklist 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist1", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist2", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist3", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist4", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist5", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist6", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist7", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist8", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist9", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist10", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist11", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Blacklist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist12", text.toUpperCase()) }

                        // ── Whitelist ──
                        Text { text: qsTr("WHITELIST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("Whitelisted", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("Whitelisted", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("Whitelist 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist1", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist2", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist3", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist4", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist5", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist6", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist7", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist8", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist9", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist10", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist11", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Whitelist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist12", text.toUpperCase()) }

                        // ── Always Pass ──
                        Text { text: qsTr("ALWAYS PASS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("AlwaysPass", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("AlwaysPass", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("Always Pass 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass1", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass2", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass3", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass4", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass5", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass6", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass7", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass8", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass9", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass10", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass11", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Pass12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass12", text.toUpperCase()) }

                        // ── Territory ──
                        Text { text: qsTr("TERRITORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Territory 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory1", text) }
                        Text { text: qsTr("Territory 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory2", text) }

                        Text { text: qsTr("Territory 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory3", text) }
                        Text { text: qsTr("Territory 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        TextField { text: bridge.getSetting("Territory4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Territory4", text) }

                        // ── Opzioni Filtro ──
                        Text { text: qsTr("FILTER OPTIONS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Wait & Pounce:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("FiltersForWaitAndPounceOnly", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("FiltersForWaitAndPounceOnly", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Calling Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("FiltersForWord2", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("FiltersForWord2", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

            } // StackLayout
        } // RowLayout
    } // contentItem
}
