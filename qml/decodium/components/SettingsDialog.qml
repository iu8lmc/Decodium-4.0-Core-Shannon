/* Decodium 4.0 — Impostazioni Generali
 * Sostituisce il dialogo impostazioni legacy WSJT-X.
 * Tutte le modifiche sono LIVE (bind diretto alle proprieta bridge).
 * Layout orizzontale: sidebar + StackLayout con GridLayout 4 colonne.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: settingsDialog
    // 1.0.412 — richiesta di schermo intero gestita da Main.qml (mainWindow non è in scope qui).
    signal fullScreenRequested()
    property var nativeHostWindow: null
    readonly property int popupViewportMargin: 16
    readonly property int popupParentWidth: (parent && parent.width > 0) ? Math.round(parent.width) : 1440
    readonly property int popupParentHeight: (parent && parent.height > 0) ? Math.round(parent.height) : 960
    readonly property var popupScreenGeometry: availableScreenGeometry()
    readonly property int popupScreenWidth: Number(popupScreenGeometry.width) > 0
                                             ? Math.round(Number(popupScreenGeometry.width))
                                             : popupParentWidth
    readonly property int popupScreenHeight: Number(popupScreenGeometry.height) > 0
                                              ? Math.round(Number(popupScreenGeometry.height))
                                              : popupParentHeight
    readonly property int popupBaseWidth: Math.min(popupParentWidth, popupScreenWidth)
    readonly property int popupBaseHeight: Math.min(popupParentHeight, popupScreenHeight)
    readonly property int popupMaxWidth: Math.max(1, popupBaseWidth - popupViewportMargin)
    readonly property int popupMaxHeight: Math.max(1, popupBaseHeight - popupViewportMargin)
    // Accessibility zoom for this Setup instance only.  It deliberately does
    // not use bridge.fontScale, which also changes the main operating window.
    property real settingsFontScale: 1.0
    readonly property int settingsFontZoomPercent: Math.round(settingsFontScale * 100)
    readonly property real effectiveSettingsContentWidth: width / settingsFontScale
    // At 1280 px the dialog still has about 1,050 px for the page after the
    // sidebar.  The old 1180 px threshold therefore selected the wide grid
    // and pushed its fourth column beyond the screen.
    readonly property bool compactSettingsLayout: effectiveSettingsContentWidth < 1420
    readonly property bool narrowSettingsLayout: effectiveSettingsContentWidth < 1060
    title: qsTr("Settings")
    modal: !warmupInProgress
    opacity: warmupInProgress ? 0 : 1
    width: nativeHostWindow && parent
           ? Math.max(360, parent.width)
           : Math.min(Math.max(360, Math.round(popupBaseWidth * 0.998)), popupMaxWidth, 2200)
    height: nativeHostWindow && parent
            ? Math.max(320, parent.height)
            : Math.min(Math.max(320, Math.round(popupBaseHeight * 0.98)), popupMaxHeight, 1080)
    closePolicy: Popup.CloseOnEscape
    property bool positionInitialized: false
    property bool warmupInProgress: false
    // Keep the dialog shell cheap.  The selected settings page is created only
    // after the popup is visible, so a heavy page cannot extend the Setup click.
    property bool tabsReady: false
    // Do not persist CAT/settings while the dialog tree emits initial signals.
    property bool initializationInProgress: true
    readonly property var appBridge: bridge
    readonly property var clearCallsignCacheDialogRef: clearCallsignCacheDialog
    property int currentTab: {
        var savedTab = Number(bridge.getSetting("uiSettingsCurrentTab", 0))
        return isFinite(savedTab) ? Math.max(0, Math.min(13, Math.floor(savedTab))) : 0
    }
    property bool closeAlreadyPersisted: false

    function increaseSetupFont() {
        settingsFontScale = Math.min(1.3,
                                     Math.round((settingsFontScale + 0.1) * 10) / 10)
    }

    function decreaseSetupFont() {
        settingsFontScale = Math.max(1.0,
                                     Math.round((settingsFontScale - 0.1) * 10) / 10)
    }

    function resetSetupFont() {
        settingsFontScale = 1.0
    }

    readonly property int labelWidth: narrowSettingsLayout ? 112 : (compactSettingsLayout ? 132 : 172)
    readonly property int fieldMinWidth: narrowSettingsLayout ? 180 : (compactSettingsLayout ? 240 : 380)
    readonly property int wideFieldMinWidth: narrowSettingsLayout ? 260 : (compactSettingsLayout ? 340 : 620)
    readonly property int portFieldMinWidth: narrowSettingsLayout ? 140 : (compactSettingsLayout ? 180 : 270)
    readonly property int numericFieldMinWidth: narrowSettingsLayout ? 120 : (compactSettingsLayout ? 160 : 220)
    readonly property int comboFieldMinWidth: narrowSettingsLayout ? 180 : (compactSettingsLayout ? 240 : 320)
    readonly property int frequencyPageMinWidth: narrowSettingsLayout ? 760 : (compactSettingsLayout ? 900 : 1120)
    readonly property int scrollLeftMargin: 10
    readonly property int scrollTopMargin: 10
    readonly property int scrollRightMargin: 12
    readonly property int scrollBottomMargin: 96

    function settingsPageMinimumContentWidth(columnCount) {
        var margins = scrollLeftMargin + scrollRightMargin
        var spacing = 10
        if (Number(columnCount) <= 2) {
            return margins + labelWidth
                    + Math.max(fieldMinWidth, wideFieldMinWidth, comboFieldMinWidth)
                    + spacing
        }
        return margins + 2 * labelWidth + 2 * fieldMinWidth + 3 * spacing
    }
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
    property var workingFrequencyRows: []
    property var stationFrequencyRows: []
    property var frequencyRegionOptions: bridge.frequencyRegionOptions()
    property var frequencyModeOptions: bridge.frequencyModeOptions()
    property var frequencyBandOptions: bridge.frequencyBandOptions()
    property int selectedWorkingFrequencyIndex: -1
    property int selectedStationFrequencyIndex: -1
    property string stationFrequencyEditorStatus: ""
    property bool stationFrequencyEditorError: false
    property string qrzLogbookTestStatus: ""
    property bool qrzLogbookTestIsError: false
    property bool qrzLogbookTestBusy: false
    readonly property var callsignService: bridge ? bridge.callsignIntelligence : null

    function callsignDatabaseEntries() {
        var entries = []
        if (settingsDialog.callsignService) {
            var providerEntries = settingsDialog.callsignService.databases
            for (var i = 0; i < providerEntries.length; ++i)
                entries.push(providerEntries[i])
        }
        if (bridge) {
            entries.push(bridge.ctyDatState)
            entries.push(bridge.call3TxtState)
        }
        return entries
    }

    function callsignDatabaseLabel(entry) {
        var id = entry && entry.id ? String(entry.id) : ""
        switch (id) {
        case "fcc_uls":
            return qsTr("FCC ULS")
        case "lotw":
            return qsTr("LoTW - User activity")
        case "lotw_confirmed":
            return qsTr("LoTW - Confirmations received")
        case "eqsl":
            return qsTr("eQSL AG")
        case "eqsl_inbox":
            return qsTr("eQSL InBox - Confirmations received")
        case "qrz_confirmed":
            return qsTr("QRZ.com - Confirmations received")
        case "clublog_oqrs":
            return qsTr("Club Log OQRS")
        case "dxcc":
        case "cty_dat":
            return qsTr("DXCC cty.dat")
        case "call3_txt":
            return qsTr("CALL3.TXT")
        default:
            return entry && entry.label ? String(entry.label) : ""
        }
    }

    function callsignDatabaseUpdating(provider) {
        if (provider === "cty_dat")
            return bridge && bridge.ctyDatUpdating
        if (provider === "call3_txt")
            return bridge && bridge.call3TxtUpdating
        return settingsDialog.callsignService && settingsDialog.callsignService.databaseUpdatePending
    }

    function refreshCallsignDatabase(provider) {
        if (provider === "cty_dat") {
            bridge.checkCtyDatUpdate(true)
        } else if (provider === "call3_txt") {
            bridge.downloadCall3Txt()
        } else if (settingsDialog.callsignService) {
            settingsDialog.callsignService.refreshDatabase(provider)
        }
    }

    Dialog {
        id: clearCallsignCacheDialog
        modal: true
        title: qsTr("Clear global lookup cache")
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(520, Math.max(360, settingsDialog.width - 48))
        anchors.centerIn: parent

        contentItem: Text {
            text: qsTr("All locally stored callsign lookup results for every provider will be deleted. The FCC, LoTW, eQSL and Club Log databases will not be deleted. Continue?")
            color: textPrimary
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            padding: 16
        }

        onAccepted: {
            if (settingsDialog.callsignService)
                settingsDialog.callsignService.clearCache()
        }
    }

    function availableScreenGeometry() {
        var geometry = null
        try {
            if (nativeHostWindow && nativeHostWindow.screen
                    && nativeHostWindow.screen.availableGeometry)
                geometry = nativeHostWindow.screen.availableGeometry
            if (bridge && typeof bridge.primaryScreenAvailableGeometry === "function")
                geometry = geometry || bridge.primaryScreenAvailableGeometry()
        } catch (error) {
            console.log("SettingsDialog: screen geometry unavailable: " + error)
        }
        return geometry || {}
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

    function territorySettingMatches(key, code, aliases) {
        var raw = String(bridge.getSetting(key, "") || "")
        if (raw.trim().length === 0)
            return false

        var text = raw.toUpperCase().replace(/[^A-Z]+/g, " ").trim()
        var compact = text.replace(/\s+/g, "")
        var accepted = [code].concat(aliases || [])
        for (var i = 0; i < accepted.length; ++i) {
            var alias = String(accepted[i] || "").toUpperCase().replace(/[^A-Z]+/g, " ").trim()
            if (alias.length === 0)
                continue
            var aliasCompact = alias.replace(/\s+/g, "")
            if (text === alias || compact === aliasCompact || text.indexOf(alias) >= 0)
                return true
        }
        return false
    }

    function normalizeTerritorySetting(key, code, aliases) {
        var raw = String(bridge.getSetting(key, "") || "")
        if (raw.trim().length === 0)
            return
        bridge.setSetting(key, territorySettingMatches(key, code, aliases) ? code : "")
    }

    function setTerritoryExcluded(key, code, excluded) {
        var value = excluded ? code : ""
        if (String(bridge.getSetting(key, "") || "") !== value)
            bridge.setSetting(key, value)
    }

    // 1.0.306 (#4) — config "bande operative": lista completa (lambda + etichetta) e helper
    // per il setting "uiDisabledBands" (CSV di lambda nascosti dal selettore). Vuoto = tutte.
    readonly property var allBandsForConfig: [
        { l: "160M", n: "1.8" }, { l: "80M", n: "3.5" }, { l: "60M", n: "5" }, { l: "40M", n: "7" },
        { l: "30M", n: "10" }, { l: "20M", n: "14" }, { l: "17M", n: "18" }, { l: "15M", n: "21" },
        { l: "12M", n: "24" }, { l: "10M", n: "28" }, { l: "8M", n: "40" }, { l: "6M", n: "50" },
        { l: "4M", n: "70" }, { l: "2M", n: "144" }, { l: "1.25M", n: "222" }, { l: "70CM", n: "432" },
        { l: "33CM", n: "902" }, { l: "23CM", n: "1296" }, { l: "13CM", n: "2304" }, { l: "9CM", n: "3400" },
        { l: "6CM", n: "5760" }, { l: "3CM", n: "10G" }, { l: "1.25CM", n: "24G" }
    ]
    property string disabledBandsCsv: bridge ? String(bridge.getSetting("uiDisabledBands", "") || "") : ""
    function bandEnabledCfg(lambda) {
        return ("," + disabledBandsCsv + ",").indexOf("," + lambda + ",") < 0
    }
    function toggleBandCfg(lambda, enable) {
        var set = disabledBandsCsv.length ? disabledBandsCsv.split(",").filter(function(x){ return x.length > 0 }) : []
        var idx = set.indexOf(lambda)
        if (enable) { if (idx >= 0) set.splice(idx, 1) }
        else        { if (idx <  0) set.push(lambda) }
        var csv = set.join(",")
        if (bridge) bridge.setSetting("uiDisabledBands", csv)
        disabledBandsCsv = csv
    }

    function setLoggingMode(promptMode) {
        if (loggingChecksUpdating)
            return

        loggingChecksUpdating = true
        if (typeof promptToLogCheck !== "undefined")
            promptToLogCheck.checked = promptMode
        if (typeof autoLogCheck !== "undefined")
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
        if (typeof promptToLogCheck !== "undefined")
            promptToLogCheck.checked = promptMode
        if (typeof autoLogCheck !== "undefined")
            autoLogCheck.checked = autoMode
        loggingChecksUpdating = false
    }

    function updateQrzLogbookTestStatus(msg, isError) {
        var text = String(msg || "")
        var lower = text.toLowerCase()
        if (lower.indexOf("qrz logbook:") !== 0)
            return false

        qrzLogbookTestStatus = text.replace(/^QRZ Logbook:\s*/i, "")
        qrzLogbookTestIsError = isError
        qrzLogbookTestBusy = !isError
                && (lower.indexOf("test in corso") >= 0
                    || lower.indexOf("testing") >= 0)
        return true
    }

    function refreshFrequencySettings() {
        workingFrequencyRows = bridge.workingFrequencyRows()
        stationFrequencyRows = bridge.stationFrequencyRows()
        frequencyRegionOptions = bridge.frequencyRegionOptions()
        frequencyModeOptions = bridge.frequencyModeOptions()
        frequencyBandOptions = bridge.frequencyBandOptions()
        if (selectedWorkingFrequencyIndex >= workingFrequencyRows.length)
            selectedWorkingFrequencyIndex = -1
        if (selectedStationFrequencyIndex >= stationFrequencyRows.length)
            selectedStationFrequencyIndex = -1
        var page = frequencySettingsPage()
        if (page) {
            page.calibrationSlopeFieldControl.text = Number(bridge.frequencyCalibrationSlopePpm()).toFixed(5)
            page.calibrationInterceptFieldControl.text = Number(bridge.frequencyCalibrationInterceptHz()).toFixed(2)
        }
    }

    function frequencySettingsPage() {
        return settingsTab7Loader && settingsTab7Loader.item ? settingsTab7Loader.item : null
    }

    function commitFrequencySlope(text) {
        var value = Number(String(text).replace(",", "."))
        if (!isFinite(value))
            value = bridge.frequencyCalibrationSlopePpm()
        bridge.setFrequencyCalibrationSlopePpm(value)
        return Number(bridge.frequencyCalibrationSlopePpm()).toFixed(5)
    }

    function commitFrequencyIntercept(text) {
        var value = Number(String(text).replace(",", "."))
        if (!isFinite(value))
            value = bridge.frequencyCalibrationInterceptHz()
        bridge.setFrequencyCalibrationInterceptHz(value)
        return Number(bridge.frequencyCalibrationInterceptHz()).toFixed(2)
    }

    function setComboText(combo, value) {
        if (!combo)
            return
        var text = String(value || "")
        for (var i = 0; i < combo.count; ++i) {
            if (combo.textAt(i) === text) {
                combo.currentIndex = i
                return
            }
        }
        combo.currentIndex = combo.count > 0 ? 0 : -1
    }

    function selectWorkingFrequencyRow(row) {
        if (!row)
            return
        selectedWorkingFrequencyIndex = Number(row.index)
        var page = frequencySettingsPage()
        if (!page)
            return
        setComboText(page.workingFrequencyRegionControl, row.region || "All")
        setComboText(page.workingFrequencyModeControl, row.mode || "FT8")
        page.workingFrequencyMHzControl.text = row.frequencyMHz || ""
        page.workingFrequencyPreferredControl.checked = !!row.preferred
        page.workingFrequencyDescriptionControl.text = row.description || ""
        page.workingFrequencyStartControl.text = row.startTime || ""
        page.workingFrequencyEndControl.text = row.endTime || ""
    }

    function clearWorkingFrequencyEditor() {
        selectedWorkingFrequencyIndex = -1
        var page = frequencySettingsPage()
        if (!page)
            return
        setComboText(page.workingFrequencyRegionControl, "All")
        setComboText(page.workingFrequencyModeControl, bridge.mode || "FT8")
        page.workingFrequencyMHzControl.text = ""
        page.workingFrequencyPreferredControl.checked = false
        page.workingFrequencyDescriptionControl.text = ""
        page.workingFrequencyStartControl.text = ""
        page.workingFrequencyEndControl.text = ""
    }

    function workingFrequencyEditorFrequencyText() {
        var page = frequencySettingsPage()
        if (!page)
            return ""
        var text = String(page.workingFrequencyMHzControl.text || "").trim()
        var lower = text.toLowerCase()
        var explicitMHz = lower.indexOf("mhz") >= 0
        var explicitHz = lower.indexOf("hz") >= 0 && !explicitMHz
        text = text.replace(/,/g, ".")
        text = text.replace(/mhz/ig, "")
        text = text.replace(/hz/ig, "")
        text = text.replace(/\s+/g, "")
        if (text.length === 0)
            return ""
        return text + (explicitMHz ? " MHz" : (explicitHz ? " Hz" : ""))
    }

    function workingFrequencyEditorHasValidFrequency() {
        var text = workingFrequencyEditorFrequencyText()
        if (text.length === 0)
            return false
        var numeric = Number(text.replace(/mhz|hz/ig, "").trim())
        return isFinite(numeric) && numeric > 0
    }

    function newWorkingFrequencyEditor() {
        selectedWorkingFrequencyIndex = -1
        var page = frequencySettingsPage()
        if (!page)
            return
        setComboText(page.workingFrequencyRegionControl, "All")
        setComboText(page.workingFrequencyModeControl, bridge.mode || "FT8")
        var currentHz = Number(bridge.frequency) || 0
        page.workingFrequencyMHzControl.text = currentHz > 0 ? (currentHz / 1000000.0).toFixed(6) : ""
        page.workingFrequencyPreferredControl.checked = true
        page.workingFrequencyDescriptionControl.text = ""
        page.workingFrequencyStartControl.text = ""
        page.workingFrequencyEndControl.text = ""
        Qt.callLater(function() {
            page.workingFrequencyMHzControl.forceActiveFocus()
            page.workingFrequencyMHzControl.selectAll()
        })
    }

    function addWorkingFrequencyFromEditor() {
        if (!workingFrequencyEditorHasValidFrequency())
            return
        var page = frequencySettingsPage()
        if (!page)
            return
        if (bridge.addWorkingFrequencyRow(page.workingFrequencyRegionControl.currentText,
                                          page.workingFrequencyModeControl.currentText,
                                          workingFrequencyEditorFrequencyText(),
                                          page.workingFrequencyDescriptionControl.text,
                                          page.workingFrequencyStartControl.text,
                                          page.workingFrequencyEndControl.text,
                                          page.workingFrequencyPreferredControl.checked)) {
            refreshFrequencySettings()
        }
    }

    function updateWorkingFrequencyFromEditor() {
        if (selectedWorkingFrequencyIndex < 0)
            return
        if (!workingFrequencyEditorHasValidFrequency())
            return
        var page = frequencySettingsPage()
        if (!page)
            return
        if (bridge.updateWorkingFrequencyRow(selectedWorkingFrequencyIndex,
                                             page.workingFrequencyRegionControl.currentText,
                                             page.workingFrequencyModeControl.currentText,
                                             workingFrequencyEditorFrequencyText(),
                                             page.workingFrequencyDescriptionControl.text,
                                             page.workingFrequencyStartControl.text,
                                             page.workingFrequencyEndControl.text,
                                             page.workingFrequencyPreferredControl.checked)) {
            refreshFrequencySettings()
        }
    }

    function deleteSelectedWorkingFrequency() {
        if (selectedWorkingFrequencyIndex < 0)
            return
        if (bridge.deleteWorkingFrequencyRow(selectedWorkingFrequencyIndex)) {
            clearWorkingFrequencyEditor()
            refreshFrequencySettings()
        }
    }

    function selectStationFrequencyRow(row) {
        if (!row)
            return
        selectedStationFrequencyIndex = Number(row.index)
        var page = frequencySettingsPage()
        if (!page)
            return
        setComboText(page.stationFrequencyBandControl, row.band || "20m")
        page.stationFrequencyOffsetControl.text = row.offsetMHz || String(row.offset || "").replace(" MHz", "")
        page.stationFrequencyAntennaControl.text = row.antenna || ""
        stationFrequencyEditorStatus = ""
        stationFrequencyEditorError = false
    }

    function clearStationFrequencyEditor() {
        selectedStationFrequencyIndex = -1
        var page = frequencySettingsPage()
        if (!page)
            return
        setComboText(page.stationFrequencyBandControl, "20m")
        page.stationFrequencyOffsetControl.text = "0.000000"
        page.stationFrequencyAntennaControl.text = ""
        stationFrequencyEditorStatus = ""
        stationFrequencyEditorError = false
    }

    function addStationFrequencyFromEditor() {
        var page = frequencySettingsPage()
        if (!page)
            return
        var offsetText = String(page.stationFrequencyOffsetControl.text || "").trim()
        if (offsetText.length === 0) {
            stationFrequencyEditorError = true
            stationFrequencyEditorStatus = qsTr("Enter a valid offset, for example -2556 MHz")
            return
        }
        var saved = bridge.addStationFrequencyRow(page.stationFrequencyBandControl.currentText,
                                                  offsetText,
                                                  page.stationFrequencyAntennaControl.text)
        stationFrequencyEditorError = !saved
        stationFrequencyEditorStatus = saved
                ? qsTr("Offset saved and verified")
                : qsTr("Offset was not saved; check the value and settings-file permissions")
        if (saved) {
            refreshFrequencySettings()
        }
    }

    function updateStationFrequencyFromEditor() {
        if (selectedStationFrequencyIndex < 0)
            return
        var page = frequencySettingsPage()
        if (!page)
            return
        var offsetText = String(page.stationFrequencyOffsetControl.text || "").trim()
        if (offsetText.length === 0) {
            stationFrequencyEditorError = true
            stationFrequencyEditorStatus = qsTr("Enter a valid offset, for example -2556 MHz")
            return
        }
        var saved = bridge.updateStationFrequencyRow(selectedStationFrequencyIndex,
                                                      page.stationFrequencyBandControl.currentText,
                                                      offsetText,
                                                      page.stationFrequencyAntennaControl.text)
        stationFrequencyEditorError = !saved
        stationFrequencyEditorStatus = saved
                ? qsTr("Offset saved and verified")
                : qsTr("Offset was not saved; check the value and settings-file permissions")
        if (saved) {
            refreshFrequencySettings()
        }
    }

    function deleteSelectedStationFrequency() {
        if (selectedStationFrequencyIndex < 0)
            return
        if (bridge.deleteStationFrequencyRow(selectedStationFrequencyIndex)) {
            clearStationFrequencyEditor()
            refreshFrequencySettings()
        }
    }

    function normalizedHexColor(value) {
        var text = String(value || "").trim()
        if (text.length === 6 && text.charAt(0) !== "#")
            text = "#" + text
        return text.toUpperCase()
    }

    function validHexColor(value) {
        return /^#[0-9A-Fa-f]{6}$/.test(String(value || "").trim())
    }

    function setDecodeHighlightColor(prop, value) {
        var normalized = normalizedHexColor(value)
        if (!validHexColor(normalized))
            return false

        bridge[prop] = normalized
        bridge.setSetting(prop, normalized)
        bridge.saveSettingsAsync()
        return true
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

    function fileUrlToLocalPath(url) {
        return folderUrlToLocalDirectory(url)
    }

    function openDirectoryPicker(settingKey, currentPath) {
        var title = settingKey === "AzElDirectory" ? qsTr("Select AzEl directory") : qsTr("Select save directory")
        var path = bridge.openDirectoryDialog(title, currentPath)
        if (settingKey === "" || path === "")
            return
        bridge.setSetting(settingKey, path)
        if (settingKey === "SaveDirectory")
            saveDirectoryField.text = path
        else if (settingKey === "AzElDirectory")
            azElDirectoryField.text = path
    }

    function openWorkingFrequenciesLoadDialog(mergeMode) {
        var path = bridge.openFileDialog(mergeMode ? qsTr("Merge Working Frequencies") : qsTr("Load Working Frequencies"),
                                         "",
                                         [qsTr("Frequency files (*.qrg *.qrg.json)"), qsTr("All files (*)")])
        if (path.length > 0 && bridge.loadWorkingFrequenciesFile(path, mergeMode)) {
            settingsDialog.clearWorkingFrequencyEditor()
            settingsDialog.refreshFrequencySettings()
        }
    }

    function openWorkingFrequenciesSaveDialog() {
        var path = bridge.saveFileDialog(qsTr("Save Working Frequencies"),
                                         "",
                                         [qsTr("Frequency files (*.qrg *.qrg.json)"), qsTr("All files (*)")])
        if (path.length > 0)
            bridge.saveWorkingFrequenciesFile(path)
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            settingsDialog.scheduleSettingsPersist()
            if (key === "Font" || key === "DecodedTextFont")
                settingsDialog.refreshFontLabels()
        }
        function onStatusMessage(msg) {
            var text = String(msg || "")
            settingsDialog.updateQrzLogbookTestStatus(text, false)
        }
        function onErrorMessage(msg) {
            var text = String(msg || "")
            settingsDialog.updateQrzLogbookTestStatus(text, true)
        }
        function onActiveCatProfileChanged() {
            settingsDialog.refreshCatProfileDraft()
        }
        function onCatProfilesChanged() {
            settingsDialog.refreshCatProfileDraft()
        }
    }

    function activeCatController() {
        return bridge.catManager ? bridge.catManager : null
    }

    function catConnectionInProgress() {
        var controller = activeCatController()
        return !!(controller && controller.connecting)
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
            return "Default"
        var text = String(controller.stopBits).trim().toLowerCase()
        if (text === "" || text === "default" || text === "predefinito" || text === "auto")
            return "Default"
        if (text === "2" || text === "2.0" || text.indexOf("two") >= 0)
            return "2"
        if (text === "1" || text === "1.0" || text.indexOf("one") >= 0)
            return "1"
        return "Default"
    }

    function normalizedCatSerialChoice(value) {
        var text = String(value === undefined || value === null ? "" : value).trim()
        var lower = text.toLowerCase()
        if (lower === "" || lower === "default" || lower === "predefinito" || lower === "auto")
            return "Default"
        if (lower === "none" || lower === "no" || lower === "off")
            return "none"
        if (lower === "xonxoff" || lower === "xon/xoff" || lower.indexOf("software") >= 0)
            return "xonxoff"
        if (lower === "hardware" || lower === "hw")
            return "hardware"
        if (lower === "7" || lower.indexOf("seven") >= 0)
            return "7"
        if (lower === "8" || lower.indexOf("eight") >= 0)
            return "8"
        if (lower === "2" || lower === "2.0" || lower.indexOf("two") >= 0)
            return "2"
        if (lower === "1" || lower === "1.0" || lower.indexOf("one") >= 0)
            return "1"
        return "Default"
    }

    function catSerialChoiceIndex(model, value, fallbackIndex) {
        var wanted = normalizedCatSerialChoice(value).toLowerCase()
        for (var i = 0; i < model.length; ++i) {
            if (String(model[i]).trim().toLowerCase() === wanted)
                return i
        }
        return fallbackIndex
    }

    function handshakeChoiceLabel(value) {
        var normalized = normalizedCatSerialChoice(value)
        if (normalized === "Default")
            return qsTr("Default")
        if (normalized === "none")
            return qsTr("None")
        if (normalized === "xonxoff")
            return "XON/XOFF"
        if (normalized === "hardware")
            return qsTr("Hardware")
        return String(value)
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

    // Port names must never use the permissive lookup above: COM1 is not the
    // same device as COM10, COM15, or COM16. The generic helper intentionally
    // supports partial labels elsewhere in Settings, but that behavior drops
    // valid virtual serial ports from the PTT selector on Windows.
    function exactStringListIndexOf(list, value) {
        if (!list || value === undefined || value === null)
            return -1
        var wanted = String(value).trim().toLowerCase()
        for (var i = 0; i < list.length; ++i) {
            if (String(list[i]).trim().toLowerCase() === wanted)
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

    function civAddressText() {
        var controller = activeCatController()
        if (!controller || controller.civAddress === undefined || controller.civAddress === null)
            return ""
        var v = Number(controller.civAddress)
        if (!isFinite(v) || v <= 0)
            return ""
        v = Math.max(0, Math.min(255, Math.round(v)))
        return "0x" + v.toString(16).toUpperCase().padStart(2, "0")
    }

    function civAddressPlaceholderText() {
        var rig = normalizedRigName(activeRigName()).replace(/-/g, "")
        if (rig.indexOf("IC7300MK2") !== -1)
            return "0xB6 (IC-7300MK2)"
        if (rig.indexOf("IC7300") !== -1)
            return "0x94 (IC-7300)"
        return qsTr("Auto")
    }

    // CI-V addresses are conventionally written in hexadecimal.  Bare values
    // such as "94" therefore mean 0x94, not decimal 94.  Keep this parser in
    // the dialog so every CAT backend exposed by catManager uses the same UI
    // validation before its integer property is updated.
    function civAddressFromText(value) {
        var raw = String(value || "").trim()
        if (!/^(?:0[xX])?[0-9a-fA-F]{1,2}$/.test(raw))
            return -1
        if (raw.length >= 2 && raw.slice(0, 2).toLowerCase() === "0x")
            raw = raw.slice(2)
        var parsed = parseInt(raw, 16)
        return isFinite(parsed) && parsed >= 0 && parsed <= 255 ? parsed : -1
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

    function usesCat4OmControls() {
        return bridge.catBackend === "cat4om" || activeCatPortType() === "cat4om"
    }

    function usesProtocolCatOnly() {
        return usesTciControls() || usesCat4OmControls()
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
            if (port !== "" && settingsDialog.exactStringListIndexOf(options, port) < 0)
                options.push(port)
        }

        var saved = controller.pttPort !== undefined && controller.pttPort !== null
                ? String(controller.pttPort).trim() : ""
        if (saved !== "" && saved.toUpperCase() !== "CAT"
                && settingsDialog.exactStringListIndexOf(options, saved) < 0)
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
        return activeCatPortType() === "serial"
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
        var tab = Number(index)
        currentTab = isFinite(tab) ? Math.max(0, Math.min(13, Math.floor(tab))) : 0
        open()
    }

    function warmUpPopup() {
        if (visible || warmupInProgress)
            return
        warmupInProgress = true
        positionInitialized = true
        x = -width - 10000
        y = -height - 10000
        open()
        warmupCloseTimer.restart()
    }

    function toggleCatConnection() {
        var controller = activeCatController()
        if (!controller) return
        if (controller.connecting) return
        if (bridge.catConnected) controller.disconnectRig()
        else controller.connectRig()
    }

    function refreshCatPorts() {
        var controller = activeCatController()
        if (controller && controller.refreshPorts) controller.refreshPorts()
    }

    function refreshCatProfileDraft() {
        var page = settingsTab1Loader.item
        if (page && page.refreshCatProfileDraft)
            page.refreshCatProfileDraft()
    }

    function finishInitialSettingsLoad() {
        if (!initializationInProgress)
            return
        Qt.callLater(function() {
            if (settingsDialog.initializationInProgress) {
                settingsDialog.initializationInProgress = false
                console.log("SETUP page initialization complete")
            }
        })
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
        if (initializationInProgress)
            return
        catPersistTimer.restart()
    }

    function persistSettingsNow() {
        if (initializationInProgress)
            return
        var controller = activeCatController()
        if (controller && controller.saveSettings)
            controller.saveSettings()
        bridge.saveSettingsAsync()
    }

    function closeAfterPersist() {
        closeAlreadyPersisted = true
        persistSettingsNow()
        close()
    }

    function scheduleSettingsPersist() {
        if (!settingsDialog.visible || settingsDialog.warmupInProgress)
            return
        settingsPersistTimer.restart()
    }

    function clampToParent() {
        if (nativeHostWindow || !parent) return
        var parentWidth = parent.width > 0 ? parent.width : width
        var parentHeight = parent.height > 0 ? parent.height : height
        x = Math.max(0, Math.min(x, parentWidth - width))
        y = Math.max(0, Math.min(y, parentHeight - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        if (nativeHostWindow) {
            x = 0
            y = 0
            positionInitialized = true
            return
        }
        var parentWidth = parent.width > 0 ? parent.width : width
        var parentHeight = parent.height > 0 ? parent.height : height
        x = Math.max(0, Math.round((parentWidth - width) / 2))
        y = Math.max(0, Math.round((parentHeight - height) / 2))
        positionInitialized = true
    }

    function startNativeHostMove() {
        if (!nativeHostWindow)
            return false
        if (typeof nativeHostWindow.beginDesktopMove === "function")
            nativeHostWindow.beginDesktopMove()
        if (typeof nativeHostWindow.startSystemMove !== "function")
            return false
        try {
            return nativeHostWindow.startSystemMove()
        } catch (error) {
            console.log("Settings startSystemMove failed: " + error)
        }
        return false
    }

    function finishNativeHostMove() {
        if (nativeHostWindow && typeof nativeHostWindow.finishDesktopMove === "function")
            nativeHostWindow.finishDesktopMove()
    }

    function requestWindowClose() {
        if (nativeHostWindow && typeof nativeHostWindow.hideHostedWindow === "function") {
            nativeHostWindow.hideHostedWindow()
            return
        }
        settingsDialog.close()
    }

    onAboutToShow: {
        if (!warmupInProgress) {
            ensureInitialPosition()
            clampToParent()
        }
    }

    onWidthChanged: {
        if (visible && !warmupInProgress)
            clampToParent()
    }

    onHeightChanged: {
        if (visible && !warmupInProgress)
            clampToParent()
    }

    onCurrentTabChanged: {
        if (!warmupInProgress)
            bridge.setSetting("uiSettingsCurrentTab", currentTab)
        if (currentTab === 7)
            refreshFrequencySettings()
    }

    onOpened: {
        if (!warmupInProgress) {
            closeAlreadyPersisted = false
            refreshCatProfileDraft()
            // Let the popup enter the scene graph before constructing the
            // selected page.  This keeps the mouse event and first paint short.
            Qt.callLater(function() { settingsDialog.tabsReady = true })
        }
    }

    onClosed: {
        settingsPersistTimer.stop()
        catPersistTimer.stop()
        if (!warmupInProgress && !closeAlreadyPersisted)
            persistSettingsNow()
        closeAlreadyPersisted = false
    }

    Component.onCompleted: {
        // Keep persistence muted until the first visible page has finished
        // constructing its controls and initial bindings.
    }

    Timer {
        id: warmupCloseTimer
        interval: 1
        repeat: false
        onTriggered: {
            settingsDialog.closeAlreadyPersisted = true
            settingsDialog.close()
            settingsDialog.warmupInProgress = false
            settingsDialog.positionInitialized = false
        }
    }

    Timer {
        id: catPersistTimer
        interval: 300
        repeat: false
        onTriggered: if (!settingsDialog.initializationInProgress)
                         settingsDialog.persistSettingsNow()
    }

    Timer {
        id: settingsPersistTimer
        interval: 500
        repeat: false
        onTriggered: settingsDialog.persistSettingsNow()
    }

    // ── Theme colors ─────────────────────────────────────────────────────
    property color bgDeep:        bridge.themeManager.bgDeep
    property color bgMedium:      bridge.themeManager.bgMedium
    property color bgLight:       bridge.themeManager.bgLight
    property color bgDark:        bridge.themeManager.bgDeep
    property color primaryBlue:   bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color textDim:       Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.55)
    property color glassBorder:   bridge.themeManager.glassBorder
    readonly property int controlHeight: Qt.platform.os === "linux" ? 36 : 32
    readonly property int controlFontSize: 12
    readonly property int controlVerticalPadding: Qt.platform.os === "linux" ? 1 : 0
    readonly property int spinTextSidePadding: 52

    // ── Preset colors for color pickers ──────────────────────────────────
    readonly property var presetColors: [
        "#ff0000","#ff6600","#ffcc00","#33cc33","#00ccff","#0066ff",
        "#9933ff","#ff33cc","#ffffff","#cccccc","#666666","#000000"
    ]
    readonly property var decodeColorModel: [
        { label: qsTr("Transmitted Message"),    prop: "colorTxMessage",        defaultColor: "#FFFF00" },
        { label: qsTr("My Callsign"),            prop: "colorMyCall",           defaultColor: "#FF5555" },
        { label: qsTr("New DXCC on Band"),       prop: "colorNewDxccBand",      defaultColor: "#F8AAD0" },
        { label: qsTr("New DXCC"),               prop: "colorNewDxcc",          defaultColor: "#FF00FF" },
        { label: qsTr("New Continent on Band"),  prop: "colorNewContinentBand", defaultColor: "#F5B7C7" },
        { label: qsTr("New Continent"),          prop: "colorNewContinent",     defaultColor: "#E91E63" },
        { label: qsTr("New CQ Zone on Band"),    prop: "colorNewCqZoneBand",    defaultColor: "#F5DDA0" },
        { label: qsTr("New CQ Zone"),            prop: "colorNewCqZone",        defaultColor: "#F0A030" },
        { label: qsTr("New ITU Zone on Band"),   prop: "colorNewItuZoneBand",   defaultColor: "#D4E89F" },
        { label: qsTr("New ITU Zone"),           prop: "colorNewItuZone",       defaultColor: "#9ACD32" },
        { label: qsTr("New Grid on Band"),       prop: "colorNewGridBand",      defaultColor: "#FFCAA0" },
        { label: qsTr("New Grid"),               prop: "colorNewGrid",          defaultColor: "#FF8C00" },
        { label: qsTr("New Callsign on Band"),   prop: "colorNewCallBand",      defaultColor: "#B5E8E8" },
        { label: qsTr("New Callsign"),           prop: "colorNewCall",          defaultColor: "#00E0E0" },
        { label: qsTr("LoTW marker"),            prop: "colorLotwUser",         defaultColor: "#FFFFFF" },
        { label: qsTr("CQ in Message"),          prop: "colorCQ",               defaultColor: "#33FF33" },
        { label: qsTr("DX Entity"),              prop: "colorDXEntity",         defaultColor: "#FFAA33" },
        { label: qsTr("73 / RR73"),              prop: "color73",               defaultColor: "#5599FF" },
        { label: qsTr("B4 (Worked)"),            prop: "colorB4",               defaultColor: "#888888" },
        { label: qsTr("Normal decodes"),         prop: "colorDecodeText",       defaultColor: "#AFC4D8" }
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

            DecoTextField {
                id: fontSearchField
                Layout.fillWidth: true
                implicitHeight: controlHeight
                text: settingsDialog.fontPickerSearch
                placeholderText: qsTr("filter by name")
                color: textPrimary
                font.pixelSize: controlFontSize
                topPadding: controlVerticalPadding
                bottomPadding: controlVerticalPadding
                verticalAlignment: TextInput.AlignVCenter
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
                        selectByMouse: true
                        onActiveFocusChanged: if (activeFocus) selectAll()
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
                    text: qsTr("173045  -21  0.1  1045  CQ LB9ZG JP20")
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

        // Keep this dialog compatible with the Linux Qt 6.4 AppImage runtime.
        // QtQuick.Effects/MultiEffect is only available from Qt 6.5.
        layer.enabled: false
    }

    // 1.0.180 — Apertura/chiusura su render thread con OpacityAnimator.
    enter: Transition {
        OpacityAnimator { from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutQuad }
    }
    exit: Transition {
        OpacityAnimator { from: 1.0; to: 0.0; duration: 120; easing.type: Easing.InQuad }
    }

    // ── Draggable header ─────────────────────────────────────────────────
    header: Rectangle {
        height: 56
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            property point pressGlobalPos: Qt.point(0, 0)
            property point pressWindowPos: Qt.point(0, 0)
            property bool nativeMoveActive: false
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                settingsDialog.positionInitialized = true
                if (settingsDialog.nativeHostWindow) {
                    pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    pressWindowPos = Qt.point(settingsDialog.nativeHostWindow.x,
                                              settingsDialog.nativeHostWindow.y)
                    nativeMoveActive = settingsDialog.startNativeHostMove()
                }
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                if (settingsDialog.nativeHostWindow) {
                    if (nativeMoveActive)
                        return
                    var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    settingsDialog.nativeHostWindow.x = Math.round(
                                pressWindowPos.x + currentGlobalPos.x - pressGlobalPos.x)
                    settingsDialog.nativeHostWindow.y = Math.round(
                                pressWindowPos.y + currentGlobalPos.y - pressGlobalPos.y)
                    return
                }
                settingsDialog.x += mouse.x - clickPos.x
                settingsDialog.y += mouse.y - clickPos.y
                settingsDialog.clampToParent()
            }
            onReleased: {
                nativeMoveActive = false
                settingsDialog.finishNativeHostMove()
            }
            onCanceled: {
                nativeMoveActive = false
                settingsDialog.finishNativeHostMove()
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
                readonly property bool available: settingsDialog.settingsFontScale > 1.0
                color: !available ? Qt.rgba(1, 1, 1, 0.04)
                       : setupFontDecreaseMA.containsMouse
                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.20)
                       : Qt.rgba(1, 1, 1, 0.1)
                border.color: !available ? glassBorder
                              : setupFontDecreaseMA.containsMouse ? secondaryCyan : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("A−")
                    color: !parent.available ? textDim
                           : setupFontDecreaseMA.containsMouse ? secondaryCyan : textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }

                MouseArea {
                    id: setupFontDecreaseMA
                    anchors.fill: parent
                    enabled: parent.available
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.decreaseSetupFont()
                }

                ToolTip.visible: setupFontDecreaseMA.containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("Setup text size: %1%. Decrease. This affects Setup only.")
                              .arg(settingsDialog.settingsFontZoomPercent)
            }

            Rectangle {
                width: 34; height: 34; radius: 6
                readonly property bool available: settingsDialog.settingsFontScale < 1.3
                color: !available ? Qt.rgba(1, 1, 1, 0.04)
                       : setupFontIncreaseMA.containsMouse
                         ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.20)
                         : Qt.rgba(1, 1, 1, 0.1)
                border.color: !available ? glassBorder
                              : setupFontIncreaseMA.containsMouse ? secondaryCyan : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("A+")
                    color: !parent.available ? textDim
                           : setupFontIncreaseMA.containsMouse ? secondaryCyan : textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }

                MouseArea {
                    id: setupFontIncreaseMA
                    anchors.fill: parent
                    enabled: parent.available
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.increaseSetupFont()
                }

                ToolTip.visible: setupFontIncreaseMA.containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("Setup text size: %1%. Increase. This affects Setup only.")
                              .arg(settingsDialog.settingsFontZoomPercent)
            }

            Rectangle {
                width: 48; height: 34; radius: 6
                readonly property bool available: settingsDialog.settingsFontScale !== 1.0
                color: !available ? Qt.rgba(1, 1, 1, 0.04)
                       : setupFontResetMA.containsMouse
                         ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.20)
                         : Qt.rgba(1, 1, 1, 0.1)
                border.color: !available ? glassBorder
                              : setupFontResetMA.containsMouse ? secondaryCyan : glassBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Reset")
                    color: !parent.available ? textDim
                           : setupFontResetMA.containsMouse ? secondaryCyan : textPrimary
                    font.pixelSize: 9
                    font.bold: true
                }

                MouseArea {
                    id: setupFontResetMA
                    anchors.fill: parent
                    enabled: parent.available
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.resetSetupFont()
                }

                ToolTip.visible: setupFontResetMA.containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("Setup text size: %1%. Reset to 100%. This affects Setup only.")
                              .arg(settingsDialog.settingsFontZoomPercent)
            }

            Rectangle {
                width: 34; height: 34; radius: 6
                color: closeMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.3) : Qt.rgba(1,1,1,0.1)
                border.color: closeMA.containsMouse ? "#f44336" : glassBorder
                Text { anchors.centerIn: parent; text: qsTr("\u2715"); color: closeMA.containsMouse ? "#f44336" : textPrimary; font.pixelSize: 14 }
                MouseArea { id: closeMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.requestWindowClose() }
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
            anchors.margins: settingsDialog.width < 520 ? 8 : 12
            spacing: settingsDialog.width < 520 ? 8 : 10

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("Changes are applied immediately where supported.")
                color: textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.preferredWidth: settingsDialog.width < 520 ? 84 : 110
                Layout.minimumWidth: 72
                Layout.preferredHeight: 36
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
                    onClicked: settingsDialog.requestWindowClose()
                }
            }

            Rectangle {
                Layout.preferredWidth: settingsDialog.width < 520 ? 84 : 110
                Layout.minimumWidth: 72
                Layout.preferredHeight: 36
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
                    onClicked: settingsDialog.closeAfterPersist()
                }
            }
        }
    }

    // ── Content ──────────────────────────────────────────────────────────
    contentItem: Item {
        clip: true

        Item {
            id: settingsContentScaler
            width: parent.width / settingsDialog.settingsFontScale
            height: parent.height / settingsDialog.settingsFontScale
            scale: settingsDialog.settingsFontScale
            transformOrigin: Item.TopLeft

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // ── Sidebar ──────────────────────────────────────────────
                Rectangle {
                    Layout.preferredWidth: settingsDialog.narrowSettingsLayout ? 156 : (settingsDialog.compactSettingsLayout ? 180 : 210)
                    Layout.minimumWidth: settingsDialog.narrowSettingsLayout ? 148 : (settingsDialog.compactSettingsLayout ? 160 : 210)
                    Layout.fillHeight: true
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)

                Flickable {
                    id: settingsTabScroll
                    anchors.fill: parent
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick
                    contentWidth: width
                    contentHeight: Math.max(height, settingsTabColumn.implicitHeight + 16)
                    ScrollBar.horizontal: ScrollBar {
                        policy: ScrollBar.AlwaysOff
                    }
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        interactive: true
                        active: hovered || pressed
                    }

                    Column {
                        id: settingsTabColumn
                        x: 6
                        y: 8
                        width: Math.max(0, parent.width - 12)
                        spacing: 2

                        Repeater {
                            model: [qsTr("Station"), qsTr("Radio"), qsTr("Audio"), qsTr("TX"), qsTr("Display"), qsTr("Decode"), qsTr("Reporting"), qsTr("Frequencies"), qsTr("Colors"), qsTr("Advanced"), qsTr("Alerts"), qsTr("Filters"), qsTr("UI Buttons"), qsTr("Callsign")]
                            delegate: Rectangle {
                                width: settingsTabColumn.width; height: 36; radius: 6
                                color: tabStack.currentIndex === index ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (tabMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent")
                                border.color: tabStack.currentIndex === index ? primaryBlue : "transparent"
                                Text { anchors.centerIn: parent; text: modelData; color: tabStack.currentIndex === index ? primaryBlue : textSecondary; font.pixelSize: 12 }
                                MouseArea { id: tabMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.currentTab = index }
                            }
                        }
                    }
                }
            }

            // Vertical separator
            Rectangle { Layout.fillHeight: true; width: 1; color: glassBorder }

            // ── StackLayout ──────────────────────────────────────────
            Item {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                property int currentIndex: settingsDialog.currentTab

                // ═══════════ TAB 0 — STAZIONE ═══════════
                // Lazy Settings tab 0
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 0
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab0.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=0")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 1
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 1
                    id: settingsTab1Loader
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab1.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=1")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 2
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 2
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab2.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=2")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 3
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 3
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab3.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=3")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 4
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 4
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab4.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=4")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 5
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 5
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab5.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=5")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 6
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 6
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab6.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=6")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 7
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 7
                    id: settingsTab7Loader
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab7.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=7")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 8
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 8
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab8.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=8")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 9
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 9
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab9.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=9")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 10
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 10
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab10.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=10")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 11
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 11
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab11.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=11")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 12
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 12
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab12.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=12")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

                // Lazy Settings tab 13
                Loader {
                    anchors.fill: parent
                    asynchronous: true
                    active: settingsDialog.tabsReady && tabStack.currentIndex === 13
                    function ensureLoaded() {
                        if (!settingsDialog.tabsReady)
                            return
                        if (active && !item)
                            setSource("SettingsTab13.qml", { dialog: settingsDialog })
                    }
                    onLoaded: {
                        console.log("SETUP tab loaded index=13")
                        if (active) settingsDialog.finishInitialSettingsLoad()
                    }
                    onActiveChanged: ensureLoaded()
                    Component.onCompleted: ensureLoaded()
                }

            } // tab host
        } // RowLayout
    } // settingsContentScaler
} // contentItem

} // SettingsDialog
