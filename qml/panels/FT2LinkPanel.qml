pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    signal closeRequested()
    signal popDockRequested()

    property var dragTarget: null
    property bool toolTabsExternal: false
    property bool poppedOut: false
    readonly property var ft2LinkAdapter: ft2Link
    property var stations: []
    property var sessions: []
    property var selectedMessages: []
    property var broadcasts: []
    property var alerts: []
    property var alertTagList: []
    property var mailbox: []
    property var mailboxCenterState: ({})
    property var relayQueue: []
    property var formTemplates: []
    property var forms: []
    property var fileTransfers: []
    property var receivedFiles: []
    property int receivedFileUnreadCount: 0
    property var bulletins: []
    property int bulletinUnreadCount: 0
    property var qsoLog: []
    property var logbookOutbox: []
    property var contactHistory: []
    property var selectedContactTimeline: []
    property var pingLog: []
    property var pathReports: []
    property var digipeaterState: ({})
    property var digipeaterEvents: []
    property var bbsFileServerState: ({})
    property var bbsSharedFiles: []
    property var beaconHistory: []
    property var clusterLastHeard: []
    property var pathAnalysis: ({})
    property var statistics: ({})
    property var storeAudit: ({})
    property var clusterConfigState: ({})
    property var cannedMessages: []
    property var customCannedMessages: []
    property var qsySlots: []
    property var qsyPlan: ({})
    property var pendingOutgoingQsyPlan: ({})
    property int pendingOutgoingQsySessionId: 0
    property var delayedQsyPlan: ({})
    property string delayedQsyReason: ""
    property var appliedQsyKeys: ({})
    property var frequencyPresetList: []
    property var allowedQsyRangeList: []
    property var frequencyScheduleList: []
    property bool frequencyScheduleAutoApply: settingBool("uiFt2LinkFrequencyScheduleAutoApply", false)
    property string lastFrequencyScheduleApplyKey: ""
    property var presenceState: ({})
    property var qsoAutomationState: ({})
    property var privacyPanelState: ({})
    property var inquiryPreviewState: ({})
    property string inquiryPreviewCall: ""
    property var blockedCalls: []
    property int selectedSessionId: ft2Link ? ft2Link.activeSessionId : 0
    property string selectedRemoteCall: ""
    property string selectedSessionStateName: ""
    property string pendingConnectCall: ""
    property var lastHelloBytes: null
    property bool cqOnly: false
    property int formTemplateIndex: 0
    property bool preferW2300: settingBool("uiFt2LinkPreferW2300", true)
    property bool robustMode: settingBool("uiFt2LinkRobustMode", false)
    readonly property bool deepRateEnabled: !!(bridge && bridge.deepSearchEnabled && !bridge.lowCpuModeEnabled)
    property int beaconIntervalSeconds: settingInt("uiFt2LinkBeaconIntervalSeconds", 180, 180, 600)
    property int toolPageIndex: 0
    property int qsySlotIndex: settingInt("uiFt2LinkQsySlotIndex", 0, 0, 9)
    property int cqSlotIndex: settingInt("uiFt2LinkCqSlotIndex", 0, 0, 9)
    property int cqSlotWaitSeconds: settingInt("uiFt2LinkCqSlotWaitSeconds", 120, 60, 3600)
    property int slotSnifferSeconds: settingInt("uiFt2LinkSlotSnifferSeconds", 8, 3, 30)
    property double slotSnifferUntilMs: 0
    property double slotSnifferDeadlineMs: 0
    property string slotSnifferAction: ""
    property string slotSnifferLabel: ""
    property bool slotSnifferRequireSlotClear: false
    property string slotSnifferStatus: ""
    property var slotSnifferPayload: ({})
    property int cqTypeIndex: settingInt("uiFt2LinkCqTypeIndex", 0, 0, 5)
    property string cqLocator: settingString("uiFt2LinkCqLocator", "")
    property int qsyCallingFrequencyHz: settingInt("uiFt2LinkCallingFrequencyHz", 0, 0, 999999999)
    property int stationPaneWidth: settingInt("uiFt2LinkStationPaneWidth", 220, 160, 420)
    property int sessionPaneWidth: settingInt("uiFt2LinkSessionPaneWidth", 170, 130, 340)
    property string selectedContactCall: ""
    property string pathFilterCall: ""
    property string pathFilterGrid: ""
    property bool digipeaterEnabledSetting: settingBool("uiFt2LinkDigipeaterEnabled", false)
    property int digipeaterMaxHopsSetting: settingInt("uiFt2LinkDigipeaterMaxHops", 2, 0, 9)
    property bool bbsFileServerEnabledSetting: settingBool("uiFt2LinkBbsFileServerEnabled", false)
    property string bbsServerStatus: ""
    property string bbsServerContentBase64: ""
    property bool bbsServerBinary: false
    property int bbsServerBytes: 0
    property string logExportText: ""
    property string databaseActionText: ""
    property string typingSummaryText: ""
    property string fileTransferStatus: ""
    property string receivedFileStatus: ""
    property bool receivedFileAutoSave: settingBool("uiFt2LinkReceivedFileAutoSave", false)
    property string receivedFileDirectory: settingString("uiFt2LinkReceivedFileDirectory", "")
    property var receivedFileSaveRequests: ({})
    property var receivedFilePendingTransfers: ({})
    property int receivedFileIoPendingCount: 0
    property string bulletinStatus: ""
    property bool alertTagsDirty: false
    property string selectedFilePath: ""
    property string selectedFileName: ""
    property string selectedFileContent: ""
    property string selectedFileBase64: ""
    property bool selectedFileBinary: false
    property int selectedFileBytes: 0
    readonly property int filePayloadLimitBytes: 16384
    property bool chatScrollPinned: true
    property bool chatUnreadBelow: false
    property bool chatUnreadPulse: false
    property bool stationHistoryMode: false
    property bool skipCqSlot: settingBool("uiFt2LinkSkipCqSlot", false)
    property double cqSlotWaitUntilMs: 0
    readonly property string bbsGroupDefaultCsv: "ALL,NEWS,NET,EMCOMM,CLUB,TEST"
    property string bbsGroupCsv: settingString("uiFt2LinkBbsGroups", bbsGroupDefaultCsv)
    readonly property var bbsGroupList: parseBbsGroups(bbsGroupCsv)
    property string bbsDefaultGroup: normalizeBbsGroup(settingString("uiFt2LinkBbsDefaultGroup", "ALL"))
    property string bbsGroupFilter: normalizeBbsGroupOrEmpty(settingString("uiFt2LinkBbsGroupFilter", ""))
    property string profileName: settingString("uiFt2LinkProfileName", "")
    property string profileQth: settingString("uiFt2LinkProfileQth", "")
    property string profileEmail: settingString("uiFt2LinkProfileEmail", "")
    property string profileRig: settingString("uiFt2LinkProfileRig", "")
    property string profileAntenna: settingString("uiFt2LinkProfileAntenna", "")
    property string profilePower: settingString("uiFt2LinkProfilePower", "")
    property string profileIce: settingString("uiFt2LinkProfileIce", "")
    property string profileGps: settingString("uiFt2LinkProfileGps", "")
    property string clusterSharePath: settingString("uiFt2LinkClusterSharePath", "")
    property bool clusterAutoSync: settingBool("uiFt2LinkClusterAutoSync", false)
    property int clusterAutoSyncSeconds: settingInt("uiFt2LinkClusterAutoSyncSeconds", 60, 30, 900)
    property double clusterLastAutoSyncMs: 0
    property string clusterSyncStatus: ""
    property bool emailGatewayEnabled: settingBool("uiFt2LinkEmailGatewayEnabled", false)
    property string emailGatewayHost: settingString("uiFt2LinkEmailGatewayHost", "")
    property int emailGatewayPort: settingInt("uiFt2LinkEmailGatewayPort", 587, 1, 65535)
    property int emailGatewaySecurityIndex: settingInt("uiFt2LinkEmailGatewaySecurityIndex", 0, 0, 2)
    property string emailGatewayUsername: settingString("uiFt2LinkEmailGatewayUsername", "")
    property string emailGatewayFrom: settingString("uiFt2LinkEmailGatewayFrom", "")
    property string emailGatewayStatus: ""
    property var emailGatewayRequestStates: ({})
    property string checkInCity: settingString("uiFt2LinkCheckInCity", "")
    property string checkInRegion: settingString("uiFt2LinkCheckInRegion", "")
    property string checkInChannel: settingString("uiFt2LinkCheckInChannel", "HF")
    // These are profile-scoped settings.  Editing them never retunes the rig;
    // the dedicated C++ state machine reads them only for an FT2-Link RF TX.
    property bool satelliteHalfDuplexEnabled: settingBool("Ft2LinkSatelliteHalfDuplexEnabled", false)
    property double satelliteRxDialHz: settingNumber("Ft2LinkSatelliteRxDialHz", 0, 0, 30000000000)
    property double satelliteTxDialHz: settingNumber("Ft2LinkSatelliteTxDialHz", 0, 0, 30000000000)
    property int satelliteCatSettleMs: settingInt("Ft2LinkSatelliteCatSettleMs", 900, 250, 5000)
    property string satelliteRigImportStatus: ""
    property bool satelliteRigAutoImported: false
    readonly property var satelliteHalfDuplexStatus: bridge
                                                    ? bridge.ft2LinkSatelliteHalfDuplexStatus : ({})
    property double uiNowMs: Date.now()
    readonly property bool selectedSessionConnected: selectedSessionStateName === "Connected"
    readonly property var cqTypeOptions: ["CQ", "CHAT", "NET", "EMCOMM", "TEST", "QSY"]
    readonly property var emailGatewaySecurityOptions: ["STARTTLS", "TLS", "NONE"]

    readonly property color panelBg: Qt.rgba(0.025, 0.035, 0.045, 0.94)
    readonly property color railBg: Qt.rgba(0.055, 0.070, 0.085, 0.96)
    readonly property color rowHover: Qt.rgba(0.18, 0.62, 0.72, 0.16)
    readonly property color rowSelect: Qt.rgba(0.10, 0.42, 0.50, 0.36)
    readonly property color borderSoft: Qt.rgba(0.42, 0.58, 0.64, 0.30)
    readonly property color cyan: "#49d7e8"
    readonly property color green: "#78d77c"
    readonly property color amber: "#e7b85c"
    readonly property color red: "#ef6f6c"
    readonly property color textPrimary: "#e8eef2"
    readonly property color textSecondary: "#93a7b0"
    readonly property string mono: decodiumMonoFontFamily

    color: panelBg
    border.color: Qt.rgba(0.30, 0.80, 0.90, 0.42)
    border.width: 1
    radius: 6
    clip: true
    implicitWidth: 720
    implicitHeight: 430

    onPreferW2300Changed: {
        persistSetting("uiFt2LinkPreferW2300", preferW2300)
        applyCapabilities()
    }
    onRobustModeChanged: {
        persistSetting("uiFt2LinkRobustMode", robustMode)
        applyCapabilities()
    }
    onBeaconIntervalSecondsChanged: {
        persistSetting("uiFt2LinkBeaconIntervalSeconds", beaconIntervalSeconds)
    }
    onQsySlotIndexChanged: {
        persistSetting("uiFt2LinkQsySlotIndex", qsySlotIndex)
    }
    onQsyCallingFrequencyHzChanged: persistSetting("uiFt2LinkCallingFrequencyHz", qsyCallingFrequencyHz)
    onCqSlotIndexChanged: persistSetting("uiFt2LinkCqSlotIndex", cqSlotIndex)
    onCqSlotWaitSecondsChanged: persistSetting("uiFt2LinkCqSlotWaitSeconds", cqSlotWaitSeconds)
    onSlotSnifferSecondsChanged: persistSetting("uiFt2LinkSlotSnifferSeconds", slotSnifferSeconds)
    onCqTypeIndexChanged: persistSetting("uiFt2LinkCqTypeIndex", cqTypeIndex)
    onCqLocatorChanged: persistSetting("uiFt2LinkCqLocator", cqLocator)
    onStationPaneWidthChanged: persistSetting("uiFt2LinkStationPaneWidth", stationPaneWidth)
    onSessionPaneWidthChanged: persistSetting("uiFt2LinkSessionPaneWidth", sessionPaneWidth)
    onDigipeaterEnabledSettingChanged: persistSetting("uiFt2LinkDigipeaterEnabled", digipeaterEnabledSetting)
    onDigipeaterMaxHopsSettingChanged: persistSetting("uiFt2LinkDigipeaterMaxHops", digipeaterMaxHopsSetting)
    onBbsFileServerEnabledSettingChanged: persistSetting("uiFt2LinkBbsFileServerEnabled", bbsFileServerEnabledSetting)
    onSkipCqSlotChanged: persistSetting("uiFt2LinkSkipCqSlot", skipCqSlot)
    onProfileNameChanged: { persistSetting("uiFt2LinkProfileName", profileName); syncLocalStation() }
    onProfileQthChanged: { persistSetting("uiFt2LinkProfileQth", profileQth); syncLocalStation() }
    onProfileEmailChanged: { persistSetting("uiFt2LinkProfileEmail", profileEmail); syncLocalStation() }
    onProfileRigChanged: { persistSetting("uiFt2LinkProfileRig", profileRig); syncLocalStation() }
    onProfileAntennaChanged: { persistSetting("uiFt2LinkProfileAntenna", profileAntenna); syncLocalStation() }
    onProfilePowerChanged: { persistSetting("uiFt2LinkProfilePower", profilePower); syncLocalStation() }
    onProfileIceChanged: { persistSetting("uiFt2LinkProfileIce", profileIce); syncLocalStation() }
    onProfileGpsChanged: { persistSetting("uiFt2LinkProfileGps", profileGps); syncLocalStation() }
    onClusterSharePathChanged: persistSetting("uiFt2LinkClusterSharePath", clusterSharePath)
    onFrequencyScheduleAutoApplyChanged: persistSetting("uiFt2LinkFrequencyScheduleAutoApply", frequencyScheduleAutoApply)
    onClusterAutoSyncChanged: {
        persistSetting("uiFt2LinkClusterAutoSync", clusterAutoSync)
        clusterLastAutoSyncMs = 0
        clusterSyncStatus = clusterAutoSync ? "AUTO waiting" : "AUTO off"
    }
    onClusterAutoSyncSecondsChanged: persistSetting("uiFt2LinkClusterAutoSyncSeconds", clusterAutoSyncSeconds)
    onEmailGatewayEnabledChanged: persistSetting("uiFt2LinkEmailGatewayEnabled", emailGatewayEnabled)
    onEmailGatewayHostChanged: persistSetting("uiFt2LinkEmailGatewayHost", emailGatewayHost)
    onEmailGatewayPortChanged: persistSetting("uiFt2LinkEmailGatewayPort", emailGatewayPort)
    onEmailGatewaySecurityIndexChanged: persistSetting("uiFt2LinkEmailGatewaySecurityIndex", emailGatewaySecurityIndex)
    onEmailGatewayUsernameChanged: persistSetting("uiFt2LinkEmailGatewayUsername", emailGatewayUsername)
    onEmailGatewayFromChanged: persistSetting("uiFt2LinkEmailGatewayFrom", emailGatewayFrom)
    onCheckInCityChanged: persistSetting("uiFt2LinkCheckInCity", checkInCity)
    onCheckInRegionChanged: persistSetting("uiFt2LinkCheckInRegion", checkInRegion)
    onCheckInChannelChanged: persistSetting("uiFt2LinkCheckInChannel", checkInChannel)
    onSatelliteHalfDuplexEnabledChanged: {
        persistSetting("Ft2LinkSatelliteHalfDuplexEnabled", satelliteHalfDuplexEnabled)
        if (satelliteHalfDuplexEnabled)
            Qt.callLater(root.tryAutoImportSatelliteRigPair)
    }
    onSatelliteRxDialHzChanged: persistSetting("Ft2LinkSatelliteRxDialHz", Math.round(satelliteRxDialHz))
    onSatelliteTxDialHzChanged: persistSetting("Ft2LinkSatelliteTxDialHz", Math.round(satelliteTxDialHz))
    onSatelliteCatSettleMsChanged: persistSetting("Ft2LinkSatelliteCatSettleMs", satelliteCatSettleMs)
    onBbsGroupCsvChanged: persistSetting("uiFt2LinkBbsGroups", bbsGroupCsv)
    onBbsDefaultGroupChanged: persistSetting("uiFt2LinkBbsDefaultGroup", bbsDefaultGroup)
    onBbsGroupFilterChanged: {
        persistSetting("uiFt2LinkBbsGroupFilter", bbsGroupFilter)
        refreshBulletins()
    }
    onReceivedFileAutoSaveChanged: {
        persistSetting("uiFt2LinkReceivedFileAutoSave", receivedFileAutoSave)
        if (receivedFileAutoSave)
            scheduleReceivedFileAutoSave()
    }
    onReceivedFileDirectoryChanged: {
        persistSetting("uiFt2LinkReceivedFileDirectory", receivedFileDirectory)
        if (receivedFileAutoSave)
            scheduleReceivedFileAutoSave()
    }

    function coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return fallback
        if (typeof value === "boolean")
            return value
        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return fallback
    }

    function settingBool(key, fallback) {
        if (bridge && typeof bridge.getSetting === "function")
            return coerceBool(bridge.getSetting(key, fallback), fallback)
        return fallback
    }

    function settingInt(key, fallback, minValue, maxValue) {
        var value = fallback
        if (bridge && typeof bridge.getSetting === "function")
            value = Number(bridge.getSetting(key, fallback))
        if (!isFinite(value))
            value = fallback
        value = Math.round(value)
        if (minValue !== undefined)
            value = Math.max(minValue, value)
        if (maxValue !== undefined)
            value = Math.min(maxValue, value)
        return value
    }

    function settingNumber(key, fallback, minValue, maxValue) {
        var value = fallback
        if (bridge && typeof bridge.getSetting === "function")
            value = Number(bridge.getSetting(key, fallback))
        if (!isFinite(value))
            value = fallback
        if (minValue !== undefined)
            value = Math.max(minValue, value)
        if (maxValue !== undefined)
            value = Math.min(maxValue, value)
        return value
    }

    function settingString(key, fallback) {
        if (bridge && typeof bridge.getSetting === "function")
            return String(bridge.getSetting(key, fallback))
        return fallback
    }

    function persistSetting(key, value) {
        if (bridge && typeof bridge.setSetting === "function")
            bridge.setSetting(key, value)
    }

    function satelliteRigPair() {
        var result = { valid: false, detail: "", rxHz: 0, txHz: 0 }
        if (!bridge) {
            result.detail = "Decodium bridge is not available."
            return result
        }
        if (String(bridge.catBackend || "").toLowerCase() !== "hamlib") {
            result.detail = "Reading the satellite VFO pair is available with Hamlib CAT only."
            return result
        }
        var rig = bridge.hamlibCat
        if (!rig || !rig.connected) {
            result.detail = "Connect the Hamlib CAT rig first."
            return result
        }
        if (String(rig.splitMode || "").toLowerCase() !== "rig") {
            result.detail = "Set Hamlib Split mode to 'rig' before reading the VFO pair."
            return result
        }
        if (rig.split !== true) {
            result.detail = "The rig is not currently reporting split as active."
            return result
        }

        var rxHz = Math.round(Number(rig.frequency || 0))
        var txHz = Math.round(Number(rig.txFrequency || 0))
        if (!isFinite(rxHz) || rxHz < 1000000) {
            result.detail = "The rig did not report a valid RX dial frequency."
            return result
        }
        if (!isFinite(txHz) || txHz < 1000000) {
            result.detail = "The rig did not report a valid TX dial frequency."
            return result
        }
        if (rxHz === txHz) {
            result.detail = "The rig reported identical RX and TX dial frequencies."
            return result
        }

        result.valid = true
        result.rxHz = rxHz
        result.txHz = txHz
        result.detail = "Rig reports RX " + (rxHz / 1000000).toFixed(6)
                        + " MHz / TX " + (txHz / 1000000).toFixed(6) + " MHz."
        return result
    }

    function importSatelliteRigPair(overwrite, showFailure) {
        var pair = satelliteRigPair()
        if (!pair.valid) {
            if (showFailure)
                satelliteRigImportStatus = pair.detail
            return false
        }
        if (!overwrite && (satelliteRxDialHz > 0 || satelliteTxDialHz > 0))
            return false

        satelliteRxDialHz = pair.rxHz
        satelliteTxDialHz = pair.txHz
        satelliteRigAutoImported = true
        satelliteRigImportStatus = (overwrite ? "Read from rig: " : "Imported from rig: ")
                                  + pair.detail + " No CAT command was sent."
        return true
    }

    function tryAutoImportSatelliteRigPair() {
        if (!satelliteHalfDuplexEnabled || satelliteRigAutoImported
                || satelliteRxDialHz > 0 || satelliteTxDialHz > 0)
            return false
        return importSatelliteRigPair(false, false)
    }

    function normalizeBbsGroup(text) {
        var value = String(text || "").trim().toUpperCase()
        value = value.replace(/[^A-Z0-9_-]/g, "")
        value = value.substring(0, 12)
        return value.length > 0 ? value : "ALL"
    }

    function normalizeBbsGroupOrEmpty(text) {
        var value = String(text || "").trim().toUpperCase()
        value = value.replace(/[^A-Z0-9_-]/g, "")
        return value.substring(0, 12)
    }

    function parseBbsGroups(csv) {
        var defaults = bbsGroupDefaultCsv.split(",")
        var input = String(csv || "").length > 0 ? String(csv).split(",") : defaults
        var out = []
        var seen = ({})
        function append(raw) {
            var group = normalizeBbsGroup(raw)
            if (seen[group])
                return
            seen[group] = true
            out.push(group)
        }
        append("ALL")
        for (var i = 0; i < input.length; ++i)
            append(input[i])
        for (var j = 0; j < defaults.length; ++j)
            append(defaults[j])
        return out
    }

    function ensureBbsGroup(rawGroup) {
        var group = normalizeBbsGroup(rawGroup)
        var list = parseBbsGroups(bbsGroupCsv)
        if (list.indexOf(group) < 0) {
            list.push(group)
            bbsGroupCsv = list.join(",")
        }
        return group
    }

    function setBbsComposerGroup(group) {
        var clean = ensureBbsGroup(group)
        if (typeof bulletinGroupText !== "undefined" && bulletinGroupText)
            bulletinGroupText.text = clean
        bbsDefaultGroup = clean
        bulletinStatus = "BBS group " + clean + " selected"
    }

    function saveBbsComposerGroup() {
        var source = typeof bulletinGroupText !== "undefined" && bulletinGroupText
                     ? bulletinGroupText.text
                     : bbsDefaultGroup
        var clean = ensureBbsGroup(source)
        if (typeof bulletinGroupText !== "undefined" && bulletinGroupText)
            bulletinGroupText.text = clean
        bbsDefaultGroup = clean
        bulletinStatus = "Saved BBS group " + clean
    }

    function cycleBbsGroupFilter() {
        var options = [""].concat(bbsGroupList)
        var idx = options.indexOf(bbsGroupFilter)
        bbsGroupFilter = options[(idx + 1) % options.length]
    }

    function bbsFilterLabel() {
        return bbsGroupFilter.length > 0 ? ("FILTER " + bbsGroupFilter) : "SHOW ALL"
    }

    function bulletinMatchesFilter(item) {
        if (bbsGroupFilter.length === 0)
            return true
        return normalizeBbsGroup(item && item.group ? item.group : "ALL") === bbsGroupFilter
    }

    function filteredBulletins() {
        var out = []
        for (var i = 0; i < bulletins.length; ++i) {
            if (bulletinMatchesFilter(bulletins[i]))
                out.push(bulletins[i])
        }
        return out
    }

    function nowMs() {
        return root.uiNowMs
    }

    function syncLocalStation() {
        if (!ft2Link)
            return
        var call = bridge && bridge.callsign ? String(bridge.callsign) : ""
        var grid = bridge && bridge.grid ? String(bridge.grid) : ""
        ft2Link.setLocalStation(call, grid, profileName.trim())
        ft2Link.setLocalOperatorProfile(profileQth.trim(),
                                        profileEmail.trim(),
                                        profileIce.trim(),
                                        profileRig.trim(),
                                        profileAntenna.trim(),
                                        profilePower.trim(),
                                        profileGps.trim())
    }

    function applyCapabilities() {
        if (!ft2Link)
            return
        if (typeof ft2Link.setDeepRateEnabled === "function")
            ft2Link.setDeepRateEnabled(deepRateEnabled)
        ft2Link.setLocalCapabilities(true, true, true, true,
                                     preferW2300 ? 2 : 1,
                                     robustMode ? 1 : 0)
    }

    function refreshStations() {
        if (!ft2Link) {
            stations = []
            return
        }
        stations = ft2Link.activeStations(nowMs(), 300000, cqOnly)
    }

    function refreshSessions() {
        if (!ft2Link) {
            sessions = []
            selectedMessages = []
            pendingConnectCall = ""
            return
        }
        sessions = ft2Link.sessions()
        updatePendingConnectCall()
        var backendActiveSessionId = Number(ft2Link.activeSessionId || 0)
        if (backendActiveSessionId > 0 && selectedSessionId !== backendActiveSessionId)
            selectSession(backendActiveSessionId)
        else if (selectedSessionId === 0 && sessions.length > 0)
            selectSession(Number(sessions[sessions.length - 1].sessionId))
        else {
            updateSelectedSessionFromSessions()
            refreshMessages()
        }
    }

    function refreshMessages() {
        var previousCount = selectedMessages.length
        var wasAtEnd = messageListAtEnd()
        if (ft2Link && selectedSessionId > 0) {
            selectedMessages = typeof ft2Link.chatLog === "function"
                               ? ft2Link.chatLog(selectedSessionId)
                               : ft2Link.messages(selectedSessionId)
        } else {
            selectedMessages = []
        }
        refreshQsyPlan()
        processQsyReplies()
        if (selectedMessages.length === 0) {
            chatUnreadBelow = false
            chatScrollPinned = true
            return
        }
        if (wasAtEnd || selectedMessages.length <= previousCount) {
            chatUnreadBelow = false
            chatScrollPinned = true
            Qt.callLater(scrollChatToEnd)
        } else if (selectedMessages.length > previousCount) {
            chatUnreadBelow = true
            chatScrollPinned = false
        }
    }

    function refreshBroadcasts() {
        var previousCount = broadcasts.length
        var wasAtEnd = messageListAtEnd()
        broadcasts = ft2Link ? ft2Link.broadcasts() : []
        if (toolPageIndex !== 4 || broadcasts.length === 0)
            return
        if (wasAtEnd || broadcasts.length <= previousCount) {
            chatUnreadBelow = false
            chatScrollPinned = true
            Qt.callLater(scrollChatToEnd)
        } else if (broadcasts.length > previousCount) {
            chatUnreadBelow = true
            chatScrollPinned = false
        }
    }

    function refreshAlerts() {
        alerts = ft2Link ? ft2Link.alertEvents() : []
    }

    function refreshAlertTags(forceEditorSync) {
        alertTagList = ft2Link && typeof ft2Link.customAlertTags === "function"
                       ? ft2Link.customAlertTags()
                       : []
        if (typeof alertTagsText !== "undefined" && alertTagsText
                && (forceEditorSync === true
                    || (!alertTagsText.activeFocus && !alertTagsDirty)))
            alertTagsText.text = alertTagList.join(", ")
    }

    function refreshMailbox() {
        mailbox = ft2Link ? ft2Link.mailbox() : []
        relayQueue = ft2Link && typeof ft2Link.relayQueue === "function"
                     ? ft2Link.relayQueue(nowMs())
                     : []
        mailboxCenterState = ft2Link && typeof ft2Link.mailboxCenter === "function"
                             ? ft2Link.mailboxCenter(nowMs())
                             : ({ rows: mailbox })
    }

    function refreshFormTemplates() {
        formTemplates = ft2Link ? ft2Link.formTemplates() : []
        if (formTemplateIndex >= formTemplates.length)
            formTemplateIndex = 0
    }

    function refreshForms() {
        forms = ft2Link ? ft2Link.forms() : []
    }

    function refreshFileTransfers() {
        fileTransfers = ft2Link ? ft2Link.fileTransfers() : []
    }

    function countUnreadReceivedFiles(items) {
        var count = 0
        var list = items || []
        for (var i = 0; i < list.length; ++i) {
            if (list[i] && list[i].unread)
                ++count
        }
        return count
    }

    function refreshReceivedFiles() {
        receivedFiles = ft2Link ? ft2Link.receivedFiles() : []
        receivedFileUnreadCount = countUnreadReceivedFiles(receivedFiles)
        if (receivedFileAutoSave)
            scheduleReceivedFileAutoSave()
    }

    function refreshBbsFileServer() {
        bbsFileServerState = ft2Link && typeof ft2Link.bbsFileServerState === "function"
                             ? ft2Link.bbsFileServerState(nowMs())
                             : ({})
        bbsSharedFiles = ft2Link && typeof ft2Link.bbsSharedFiles === "function"
                         ? ft2Link.bbsSharedFiles()
                         : []
    }

    function countUnreadBulletins(items) {
        var count = 0
        var list = items || []
        for (var i = 0; i < list.length; ++i) {
            if (list[i] && list[i].unread)
                ++count
        }
        return count
    }

    function refreshBulletins() {
        bulletins = ft2Link ? ft2Link.bulletins() : []
        bulletinUnreadCount = countUnreadBulletins(bulletins)
    }

    function refreshQsoLog() {
        qsoLog = ft2Link ? ft2Link.qsoLog() : []
    }

    function refreshLogbookOutbox() {
        logbookOutbox = ft2Link && typeof ft2Link.logbookOutbox === "function"
                        ? ft2Link.logbookOutbox()
                        : []
    }

    function refreshContactHistory() {
        contactHistory = ft2Link ? ft2Link.contactHistory() : []
    }

    function refreshContactTimeline() {
        selectedContactTimeline = ft2Link && selectedContactCall.length > 0
                                  ? ft2Link.contactTimeline(selectedContactCall)
                                  : []
    }

    function refreshPingLog() {
        pingLog = ft2Link ? ft2Link.pingLog() : []
    }

    function refreshPathReports() {
        pathReports = ft2Link ? ft2Link.pathReports() : []
    }

    function refreshDigipeater() {
        digipeaterState = ft2Link && typeof ft2Link.digipeaterState === "function"
                           ? ft2Link.digipeaterState(nowMs())
                           : ({})
        digipeaterEvents = ft2Link && typeof ft2Link.digipeaterEvents === "function"
                           ? ft2Link.digipeaterEvents()
                           : []
    }

    function refreshBeaconHistory() {
        beaconHistory = ft2Link && typeof ft2Link.beaconHistory === "function"
                        ? ft2Link.beaconHistory()
                        : []
    }

    function refreshClusterLastHeard() {
        clusterConfigState = ft2Link && typeof ft2Link.clusterConfig === "function"
                             ? ft2Link.clusterConfig()
                             : ({})
        clusterLastHeard = ft2Link && typeof ft2Link.clusterLastHeard === "function"
                           ? ft2Link.clusterLastHeard()
                           : []
    }

    function updateClusterFromRig() {
        if (!ft2Link || typeof ft2Link.configureCluster !== "function")
            return
        ft2Link.configureCluster(true, "", "", currentDialFrequencyHz())
        refreshClusterLastHeard()
    }

    function refreshPathAnalysis() {
        pathAnalysis = ft2Link ? ft2Link.pathAnalysis(pathFilterCall, pathFilterGrid) : ({})
    }

    function refreshStatistics() {
        statistics = ft2Link ? ft2Link.statistics() : ({})
    }

    function refreshStoreAudit() {
        storeAudit = ft2Link ? ft2Link.localStoreAudit() : ({})
    }

    function refreshCannedMessages() {
        cannedMessages = ft2Link ? ft2Link.cannedMessages() : []
        customCannedMessages = ft2Link && typeof ft2Link.customCannedMessages === "function"
                               ? ft2Link.customCannedMessages()
                               : []
    }

    function refreshQsySlots() {
        qsySlots = ft2Link ? ft2Link.qsySlots(750, 5) : []
        if (qsySlotIndex >= qsySlots.length)
            qsySlotIndex = 0
        if (cqSlotIndex >= qsySlots.length)
            cqSlotIndex = 0
    }

    function refreshFrequencyPlan() {
        frequencyPresetList = ft2Link && typeof ft2Link.frequencyPresets === "function"
                              ? ft2Link.frequencyPresets()
                              : []
        allowedQsyRangeList = ft2Link && typeof ft2Link.allowedQsyRanges === "function"
                              ? ft2Link.allowedQsyRanges()
                              : []
        frequencyScheduleList = ft2Link && typeof ft2Link.frequencySchedule === "function"
                                ? ft2Link.frequencySchedule()
                                : []
    }

    function refreshPresence() {
        presenceState = ft2Link && typeof ft2Link.presence === "function"
                        ? ft2Link.presence()
                        : ({})
    }

    function refreshQsoAutomation() {
        qsoAutomationState = ft2Link && typeof ft2Link.qsoAutomation === "function"
                             ? ft2Link.qsoAutomation()
                             : ({})
    }

    function refreshPrivacyPanel() {
        privacyPanelState = ft2Link && typeof ft2Link.privacyPanel === "function"
                            ? ft2Link.privacyPanel(nowMs())
                            : ({})
    }

    function refreshInquiryPreview() {
        var call = inquiryPreviewCall.trim()
        if (call.length === 0)
            call = selectedRemoteCall
        inquiryPreviewState = ft2Link && typeof ft2Link.inquiryPreview === "function"
                              ? ft2Link.inquiryPreview(call, nowMs())
                              : ({})
    }

    function refreshBlockedCalls() {
        blockedCalls = ft2Link && typeof ft2Link.blockedCalls === "function"
                       ? ft2Link.blockedCalls()
                       : []
    }

    function refreshTypingIndicators() {
        typingSummaryText = ft2Link && typeof ft2Link.typingSummary === "function"
                            ? String(ft2Link.typingSummary(nowMs()))
                            : ""
    }

    function refreshLivePollState() {
        syncLocalStation()
        refreshStations()
        refreshSessions()
        refreshStatistics()
        refreshTypingIndicators()
    }

    function refreshSlowPollState() {
        refreshBroadcasts()
        refreshAlerts()
        refreshAlertTags()
        refreshMailbox()
        refreshForms()
        refreshFileTransfers()
        refreshReceivedFiles()
        refreshBbsFileServer()
        refreshBulletins()
        refreshQsoLog()
        refreshLogbookOutbox()
        refreshContactHistory()
        refreshPingLog()
        refreshPathReports()
        refreshDigipeater()
        refreshBeaconHistory()
        updateClusterFromRig()
        refreshPathAnalysis()
        refreshContactTimeline()
        refreshPrivacyPanel()
        refreshInquiryPreview()
    }

    function logbookStateCount(stateName) {
        var wanted = String(stateName || "")
        var count = 0
        for (var i = 0; i < logbookOutbox.length; ++i) {
            if (String(logbookOutbox[i].state || "") === wanted)
                ++count
        }
        return count
    }

    function queueStatusLine() {
        if (!ft2Link)
            return "QUEUE --"
        if (slotSnifferActive())
            return "QUEUE " + slotSnifferLine()
        if (slotSnifferStatus.length > 0)
            return "QUEUE " + slotSnifferStatus
        var parts = []
        var queued = logbookStateCount("Queued")
        var submitted = logbookStateCount("Submitted")
        var failed = logbookStateCount("Failed")
        if (ft2Link.relayQueueCount > 0)
            parts.push("RLY " + ft2Link.relayQueueCount)
        if (ft2Link.mailboxUnreadCount > 0)
            parts.push("MAIL UNREAD " + ft2Link.mailboxUnreadCount)
        if (receivedFileUnreadCount > 0)
            parts.push("RXF " + receivedFileUnreadCount)
        if (bulletinUnreadCount > 0)
            parts.push("BBS " + bulletinUnreadCount)
        if (queued > 0 || submitted > 0 || failed > 0)
            parts.push("LBQ " + queued + "/" + submitted + "/" + failed)
        if (ft2Link.alertCount > 0)
            parts.push("ALERT " + ft2Link.alertCount)
        if (parts.length === 0)
            parts.push("clear")
        return "QUEUE " + parts.join("  ")
    }

    function queueStatusColor() {
        if (slotSnifferActive())
            return root.amber
        if (slotSnifferStatus.indexOf("abort") >= 0
                || slotSnifferStatus.indexOf("BUSY") >= 0)
            return root.red
        if (ft2Link && (ft2Link.alertCount > 0 || logbookStateCount("Failed") > 0))
            return root.red
        if (ft2Link && (ft2Link.relayQueueCount > 0
                        || ft2Link.mailboxUnreadCount > 0
                        || receivedFileUnreadCount > 0
                        || bulletinUnreadCount > 0
                        || logbookStateCount("Submitted") > 0
                        || logbookStateCount("Queued") > 0))
            return root.amber
        return root.textSecondary
    }

    function queueStatusActive() {
        return slotSnifferActive()
               || slotSnifferStatus.length > 0
               || (!!ft2Link && (ft2Link.alertCount > 0
                             || ft2Link.relayQueueCount > 0
                             || ft2Link.mailboxUnreadCount > 0
                             || receivedFileUnreadCount > 0
                             || bulletinUnreadCount > 0
                             || logbookStateCount("Submitted") > 0
                             || logbookStateCount("Queued") > 0
                             || logbookStateCount("Failed") > 0))
    }

    function queueStatusClickable() {
        return queueStatusActive()
    }

    function queueStatusTip() {
        if (!ft2Link)
            return "Queue unavailable"
        if (ft2Link.alertCount > 0)
            return "Open alerts and broadcasts"
        if (logbookStateCount("Failed") > 0)
            return "Open failed logbook uploads"
        if (ft2Link.mailboxUnreadCount > 0)
            return "Open unread mail"
        if (receivedFileUnreadCount > 0)
            return "Open unread received files"
        if (bulletinUnreadCount > 0)
            return "Open unread BBS bulletins"
        if (ft2Link.relayQueueCount > 0)
            return "Open relay mailbox queue"
        if (logbookStateCount("Queued") > 0 || logbookStateCount("Submitted") > 0)
            return "Open logbook upload queue"
        return "Queue is clear"
    }

    function openQueueStatus() {
        if (!ft2Link)
            return
        refreshSlowPollState()
        if (ft2Link.alertCount > 0) {
            openAlertQueue()
            return
        }
        if (logbookStateCount("Failed") > 0) {
            openLogbookQueue()
            return
        }
        if (ft2Link.mailboxUnreadCount > 0) {
            openMailboxQueue()
            return
        }
        if (receivedFileUnreadCount > 0) {
            openReceivedFilesQueue()
            return
        }
        if (bulletinUnreadCount > 0) {
            openBulletinQueue()
            return
        }
        if (ft2Link.relayQueueCount > 0) {
            openRelayQueue()
            return
        }
        if (logbookStateCount("Queued") > 0 || logbookStateCount("Submitted") > 0) {
            openLogbookQueue()
            return
        }
    }

    function openMailboxQueue() {
        refreshMailbox()
        toolPageIndex = 5
        Qt.callLater(function() {
            if (typeof mailboxList === "undefined" || !mailboxList)
                return
            var unreadIndex = -1
            for (var i = 0; i < mailbox.length; ++i) {
                if (mailbox[i] && mailbox[i].unread) {
                    unreadIndex = i
                    break
                }
            }
            if (unreadIndex >= 0) {
                mailboxList.currentIndex = unreadIndex
                mailboxList.positionViewAtIndex(unreadIndex, ListView.Beginning)
            }
        })
    }

    function openBulletinQueue() {
        refreshBulletins()
        toolPageIndex = 3
        Qt.callLater(function() {
            if (typeof bulletinList === "undefined" || !bulletinList)
                return
            var unreadIndex = -1
            for (var i = 0; i < bulletins.length; ++i) {
                if (bulletins[i] && bulletins[i].unread) {
                    unreadIndex = i
                    break
                }
            }
            if (unreadIndex < 0 && bulletins.length > 0)
                unreadIndex = 0
            if (unreadIndex >= 0) {
                bulletinList.currentIndex = unreadIndex
                bulletinList.positionViewAtIndex(unreadIndex, ListView.Beginning)
            }
        })
    }

    function openAlertQueue() {
        refreshBroadcasts()
        refreshAlerts()
        toolPageIndex = 4
    }

    function openRelayQueue() {
        refreshMailbox()
        toolPageIndex = 5
        Qt.callLater(function() {
            if (typeof mailboxList === "undefined" || !mailboxList)
                return
            var relayIndex = -1
            for (var i = 0; i < mailbox.length; ++i) {
                if (mailbox[i] && (String(mailbox[i].direction || "") === "Relay"
                        || mailbox[i].relayEnvelope
                        || String(mailbox[i].state || "").indexOf("Parked") >= 0)) {
                    relayIndex = i
                    break
                }
            }
            if (relayIndex >= 0) {
                mailboxList.currentIndex = relayIndex
                mailboxList.positionViewAtIndex(relayIndex, ListView.Beginning)
            }
        })
    }

    function openLogbookQueue() {
        refreshLogbookOutbox()
        toolPageIndex = 12
        if (logExportText.length === 0 || logbookStateCount("Failed") > 0)
            exportLog("OUTBOX")
    }

    function openReceivedFilesQueue() {
        refreshFileTransfers()
        refreshReceivedFiles()
        toolPageIndex = 11
        Qt.callLater(function() {
            if (typeof receivedFileListPanel !== "undefined"
                    && receivedFileListPanel) {
                if (receivedFiles.length > 0) {
                    receivedFileListPanel.currentIndex = 0
                    receivedFileListPanel.positionViewAtIndex(0, ListView.Beginning)
                }
                return
            }
            if (typeof receivedFileList === "undefined" || !receivedFileList)
                return
            if (receivedFiles.length > 0) {
                receivedFileList.currentIndex = 0
                receivedFileList.positionViewAtIndex(0, ListView.Beginning)
            }
        })
    }

    function rfStatusLine() {
        if (!ft2Link)
            return "RF --"
        if (slotSnifferActive())
            return "RF " + slotSnifferLine()
        var plan = ft2Link.lastRadioTxPlan || ({})
        var kind = String(plan.kind || plan.frameKind || "")
        var profile = String(plan.profileName || plan.profile || "")
        var state = String(ft2Link.transportState || "Idle")
        var detail = kind.length > 0 ? kind : state
        if (profile.length > 0)
            detail += " " + profile
        var bursts = Number(plan.bursts || plan.burstCount || 0)
        if (isFinite(bursts) && bursts > 0)
            detail += " " + bursts + "b"
        return "RF " + detail
    }

    function globalErrorLine() {
        if (!ft2Link || String(ft2Link.lastError || "").length === 0)
            return ""
        return "ERR " + String(ft2Link.lastError)
    }

    function toolStackPreferredHeight() {
        switch (toolPageIndex) {
        case 0:
            return selectedSessionId > 0 ? 112 : 44
        case 1:
            return 68
        case 2:
            return 56
        case 3:
            return Math.max(188, Math.min(260, root.height * 0.38))
        case 4:
            return Math.max(216, Math.min(286, root.height * 0.40))
        case 6: {
            var infoContentHeight = (typeof infoColumn !== "undefined"
                                     && infoColumn.implicitHeight > 0)
                                    ? Math.ceil(infoColumn.implicitHeight + 8)
                                    : 260
            return Math.max(260, Math.min(Math.max(300, root.height * 0.52),
                                          Math.max(300, infoContentHeight)))
        }
        default:
            return Math.max(132, Math.min(190, root.height * 0.36))
        }
    }

    function toolStackIndexForPage(page) {
        switch (Number(page)) {
        case 0: return 0   // CHAT
        case 1: return 1   // FORM
        case 2: return 2   // FILE
        case 3: return 3   // BBS
        case 4: return 4   // BCAST
        case 5: return 5   // MAIL
        case 6: return 6   // INFO
        case 7: return 7   // CALL
        case 8: return 7   // CLST shares the CALL/contact stack slot
        case 9: return 8   // PATH
        case 10: return 11 // STAT
        case 11: return 12 // RXF fallback; normal RXF uses receivedFilesPanel
        case 12: return 9  // LOG
        case 13: return 10 // DB
        case 14: return 13 // PRE
        case 15: return 14 // FREQ
        case 16: return 15 // BLK
        case 17: return 16 // SAT
        default: return 0
        }
    }

    function openToolPage(page) {
        var target = Number(page)
        switch (target) {
        case 3:
            refreshBulletins()
            toolPageIndex = 3
            break
        case 4:
            toolPageIndex = 4
            refreshBroadcasts()
            Qt.callLater(scrollChatToEnd)
            break
        case 6:
            toolPageIndex = 6
            loadPresenceEditor()
            break
        case 8:
            toolPageIndex = 8
            updateClusterFromRig()
            break
        case 11:
            openReceivedFilesQueue()
            break
        case 12:
            toolPageIndex = 12
            if (logExportText.length === 0)
                exportLog("OPS")
            break
        case 13:
            toolPageIndex = 13
            auditStore()
            break
        case 15:
            toolPageIndex = 15
            if (ft2Link) {
                frequencyPresetText.text = ft2Link.frequencyPresetsText()
                allowedQsyRangeText.text = ft2Link.allowedQsyRangesText()
                frequencyScheduleText.text = typeof ft2Link.frequencyScheduleText === "function"
                                             ? ft2Link.frequencyScheduleText()
                                             : ""
            }
            break
        case 16:
            toolPageIndex = 16
            refreshBlockedCalls()
            if (ft2Link && typeof ft2Link.blockedCallsText === "function")
                blockedCallsText.text = ft2Link.blockedCallsText()
            break
        case 17:
            toolPageIndex = 17
            break
        default:
            toolPageIndex = target
            break
        }
    }

    function loadPresenceEditor() {
        refreshPresence()
        awayCheck.checked = !!presenceState.awayEnabled
        awayQsyCheck.checked = !!presenceState.awayAcceptsQsy
        awayMessageText.text = String(presenceState.awayMessage || "QRX DE <MYCALL>")
        welcomeCheck.checked = !!presenceState.welcomeEnabled
        welcomeMessageText.text = String(presenceState.welcomeMessage || "HELLO <CALL> DE <MYCALL>")
        autoReplyCheck.checked = !!presenceState.autoReplyEnabled
        autoAwayCheck.checked = !!presenceState.autoAwayEnabled
        autoAwayMinutesText.text = String(presenceState.autoAwayMinutes || 10)
        refreshQsoAutomation()
        refreshPrivacyPanel()
        if (inquiryPreviewCall.length === 0 && selectedRemoteCall.length > 0)
            inquiryPreviewCall = selectedRemoteCall
        refreshInquiryPreview()
        callIdIntervalText.text = String(qsoAutomationState.callIdIntervalMinutes || 0)
        autoDisconnectText.text = String(qsoAutomationState.autoDisconnectMinutes || 0)
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        verboseSnrAcceptCheck.checked = !!qsoAutomationState.verboseSnrAutoAcceptEnabled
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
    }

    function savePresence() {
        if (!ft2Link || typeof ft2Link.configurePresence !== "function")
            return
        var result = ft2Link.configurePresence(awayCheck.checked,
                                               awayQsyCheck.checked,
                                               awayMessageText.text,
                                               welcomeCheck.checked,
                                               welcomeMessageText.text)
        if (typeof ft2Link.configureAutoReply === "function")
            result = ft2Link.configureAutoReply(autoReplyCheck.checked)
        if (typeof ft2Link.configureAutoAway === "function")
            result = ft2Link.configureAutoAway(autoAwayCheck.checked,
                                               Number(autoAwayMinutesText.text || 10),
                                               nowMs())
        databaseActionText = prettyJson(result)
        refreshPresence()
        refreshStatistics()
        refreshStoreAudit()
        awayMessageText.text = String(presenceState.awayMessage || "QRX DE <MYCALL>")
        welcomeMessageText.text = String(presenceState.welcomeMessage || "HELLO <CALL> DE <MYCALL>")
        autoReplyCheck.checked = !!presenceState.autoReplyEnabled
        autoAwayCheck.checked = !!presenceState.autoAwayEnabled
        autoAwayMinutesText.text = String(presenceState.autoAwayMinutes || 10)
        root.saveQsoAutomation()
        refreshPrivacyPanel()
        refreshInquiryPreview()
    }

    function saveQsoAutomation() {
        if (!ft2Link || typeof ft2Link.configureQsoAutomation !== "function")
            return
        var result = ft2Link.configureQsoAutomation(Number(callIdIntervalText.text || 0),
                                                    Number(autoDisconnectText.text || 0))
        if (typeof ft2Link.configureIncomingPings === "function")
            result = ft2Link.configureIncomingPings(incomingPingCheck.checked)
        if (typeof ft2Link.configureLastHeardPeeking === "function")
            result = ft2Link.configureLastHeardPeeking(lastHeardPeekingCheck.checked)
        if (typeof ft2Link.configureLastConnectionsPeeking === "function")
            result = ft2Link.configureLastConnectionsPeeking(lastConnectionsPeekingCheck.checked)
        if (typeof ft2Link.configureParkedVmailPeeking === "function")
            result = ft2Link.configureParkedVmailPeeking(parkedVmailPeekingCheck.checked)
        if (typeof ft2Link.configureVmailParking === "function")
            result = ft2Link.configureVmailParking(vmailParkingCheck.checked)
        if (typeof ft2Link.configureSnrReportSending === "function")
            result = ft2Link.configureSnrReportSending(snrReportCheck.checked)
        if (typeof ft2Link.configureVerboseSnrAutoAccept === "function")
            result = ft2Link.configureVerboseSnrAutoAccept(verboseSnrAcceptCheck.checked)
        if (typeof ft2Link.configureInfoInquire === "function")
            result = ft2Link.configureInfoInquire(infoInquireCheck.checked)
        databaseActionText = prettyJson(result)
        refreshQsoAutomation()
        refreshPrivacyPanel()
        refreshInquiryPreview()
        refreshStatistics()
        refreshStoreAudit()
        callIdIntervalText.text = String(qsoAutomationState.callIdIntervalMinutes || 0)
        autoDisconnectText.text = String(qsoAutomationState.autoDisconnectMinutes || 0)
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        verboseSnrAcceptCheck.checked = !!qsoAutomationState.verboseSnrAutoAcceptEnabled
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
    }

    function applyPrivacyPreset(preset) {
        if (!ft2Link || typeof ft2Link.applyPrivacyPreset !== "function")
            return
        var result = ft2Link.applyPrivacyPreset(String(preset || "CONTROL"))
        databaseActionText = prettyJson(result)
        refreshQsoAutomation()
        refreshPresence()
        refreshPrivacyPanel()
        refreshInquiryPreview()
        refreshStatistics()
        refreshStoreAudit()
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        verboseSnrAcceptCheck.checked = !!qsoAutomationState.verboseSnrAutoAcceptEnabled
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
        autoReplyCheck.checked = !!presenceState.autoReplyEnabled
        welcomeCheck.checked = !!presenceState.welcomeEnabled
    }

    function saveInquiryPrivacy() {
        if (!ft2Link || typeof ft2Link.configureInquiryPrivacy !== "function")
            return
        var result = ft2Link.configureInquiryPrivacy(incomingPingCheck.checked,
                                                     lastHeardPeekingCheck.checked,
                                                     lastConnectionsPeekingCheck.checked,
                                                     parkedVmailPeekingCheck.checked,
                                                     vmailParkingCheck.checked,
                                                     snrReportCheck.checked,
                                                     verboseSnrAcceptCheck.checked,
                                                     infoInquireCheck.checked,
                                                     autoReplyCheck.checked,
                                                     welcomeCheck.checked,
                                                     nowMs())
        databaseActionText = prettyJson(result)
        refreshPresence()
        refreshQsoAutomation()
        refreshPrivacyPanel()
        refreshInquiryPreview()
        refreshStatistics()
        refreshStoreAudit()
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        verboseSnrAcceptCheck.checked = !!qsoAutomationState.verboseSnrAutoAcceptEnabled
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
        autoReplyCheck.checked = !!presenceState.autoReplyEnabled
        welcomeCheck.checked = !!presenceState.welcomeEnabled
    }

    function privacyPresetName() {
        return String(qsoAutomationState.privacyPreset || "CUSTOM")
    }

    function privacySummaryText() {
        return String(qsoAutomationState.privacySummary || "Privacy custom")
    }

    function saveBlockedCalls() {
        if (!ft2Link || typeof ft2Link.setBlockedCalls !== "function")
            return
        var result = ft2Link.setBlockedCalls(blockedCallsText.text)
        databaseActionText = prettyJson(result)
        if (result && result.text !== undefined)
            blockedCallsText.text = String(result.text)
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function addBlockedCallFromEditor() {
        if (!ft2Link || typeof ft2Link.addBlockedCall !== "function")
            return
        var call = blockedCallText.text.trim().toUpperCase()
        if (call.length === 0)
            return
        var result = ft2Link.addBlockedCall(call)
        databaseActionText = prettyJson(result)
        blockedCallText.text = ""
        if (result && result.text !== undefined)
            blockedCallsText.text = String(result.text)
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function deleteBlockedCall(call) {
        if (!ft2Link || typeof ft2Link.deleteBlockedCall !== "function")
            return
        var result = ft2Link.deleteBlockedCall(String(call || ""))
        databaseActionText = prettyJson(result)
        if (result && result.text !== undefined)
            blockedCallsText.text = String(result.text)
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function clearBlockedCalls() {
        if (!ft2Link || typeof ft2Link.clearBlockedCalls !== "function")
            return
        var result = ft2Link.clearBlockedCalls()
        databaseActionText = prettyJson(result)
        blockedCallsText.text = ""
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function latestBroadcastLine() {
        if (broadcasts.length === 0)
            return ""
        var item = broadcasts[broadcasts.length - 1]
        var prefix = item.qsy ? "QSY " : (item.alert ? "ALERT " : "BCAST ")
        return prefix + String(item.fromCall || "--") + ": " + String(item.text || "")
    }

    function latestMailboxLine() {
        if (mailbox.length === 0)
            return ""
        var item = mailbox[mailbox.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.toCall || "--")
        return "MAIL " + String(item.state || "--") + " " + peer + ": "
               + String(item.subject || "")
    }

    function latestFormLine() {
        if (forms.length === 0)
            return ""
        var item = forms[forms.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.toCall || "--")
        return "FORM " + String(item.state || "--") + " "
               + String(item.formType || "--") + " " + peer
    }

    function latestFileLine() {
        if (fileTransfers.length === 0)
            return ""
        var item = fileTransfers[fileTransfers.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.toCall || "--")
        return "FILE " + String(item.state || "--") + " " + peer + ": "
               + String(item.fileName || "")
    }

    function latestBulletinLine() {
        if (bulletins.length === 0)
            return ""
        var item = bulletins[bulletins.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.group || "ALL")
        return "BBS " + String(item.state || "--") + " " + peer + ": "
               + String(item.title || "")
    }

    function latestQsoLine() {
        if (qsoLog.length === 0)
            return ""
        var item = qsoLog[0]
        return "QSO " + String(item.remoteCall || "--") + " "
               + String(item.state || "--") + " " + String(item.profileName || "--")
    }

    function latestContactLine() {
        if (contactHistory.length === 0)
            return ""
        var item = contactHistory[0]
        var tag = String(item.tag || "")
        return "CALL " + String(item.call || "--")
               + (tag.length > 0 ? " [" + tag + "]" : "") + " "
               + String(item.lastEvent || "--") + " qso "
               + String(item.qsoCount || 0)
    }

    function latestPingLine() {
        if (pingLog.length === 0)
            return ""
        var item = pingLog[0]
        var rtt = Number(item.rttMs || 0)
        return "PING " + String(item.remoteCall || "--") + " "
               + String(item.state || "--")
               + (rtt > 0 ? " " + String(rtt) + " ms" : "")
    }

    function statValue(key, fallback) {
        if (!statistics)
            return fallback
        var value = statistics[key]
        return value === undefined || value === null ? fallback : value
    }

    function statCount(key) {
        var value = Number(statValue(key, 0))
        return isFinite(value) ? String(Math.round(value)) : "0"
    }

    function pathValue(key, fallback) {
        if (!pathAnalysis)
            return fallback
        var value = pathAnalysis[key]
        return value === undefined || value === null ? fallback : value
    }

    function pathCount(key) {
        var value = Number(pathValue(key, 0))
        return isFinite(value) ? String(Math.round(value)) : "0"
    }

    function pathAverage(key) {
        var value = Number(pathValue(key, NaN))
        return isFinite(value) ? value.toFixed(1) : "--"
    }

    function twoDigit(value) {
        var number = Math.max(0, Math.min(99, Math.round(Number(value || 0))))
        return number < 10 ? "0" + String(number) : String(number)
    }

    function applyPathFilter(call, grid) {
        pathFilterCall = String(call || "").trim().toUpperCase()
        pathFilterGrid = String(grid || "").trim().toUpperCase()
        refreshPathAnalysis()
    }

    function clearPathFilter() {
        pathFilterCall = ""
        pathFilterGrid = ""
        refreshPathAnalysis()
    }

    function receivedFileDate(item) {
        return String(item.receivedUtc || "--")
    }

    function receivedFilesCountText() {
        var text = qsTr("%n item", "", receivedFiles.length)
        if (receivedFileUnreadCount > 0)
            text += qsTr(" / %n unread", "", receivedFileUnreadCount)
        return text
    }

    function receivedFilePendingText() {
        return qsTr("%n save pending", "", receivedFileIoPendingCount)
    }

    function markReceivedFileRead(item, read, showStatus) {
        if (!ft2Link || !item)
            return false
        var id = Number(item.id || 0)
        if (id <= 0)
            return false
        if (!ft2Link.markReceivedFileRead(id, read, nowMs()))
            return false
        refreshFileTransfers()
        refreshReceivedFiles()
        if (showStatus !== false) {
            var fileName = String(item.fileName || qsTr("received file"))
            receivedFileStatus = read
                                 ? qsTr("Marked read %1").arg(fileName)
                                 : qsTr("Marked unread %1").arg(fileName)
        }
        return true
    }

    function markAllReceivedFilesRead() {
        if (!ft2Link)
            return
        var changed = 0
        for (var i = 0; i < receivedFiles.length; ++i) {
            var item = receivedFiles[i]
            if (item && item.unread
                    && ft2Link.markReceivedFileRead(Number(item.id || 0),
                                                    true,
                                                    nowMs()))
                ++changed
        }
        refreshFileTransfers()
        refreshReceivedFiles()
        receivedFileStatus = changed > 0
                             ? qsTr("Marked read %n received file", "", changed)
                             : qsTr("No unread received files")
    }

    function deleteReceivedFile(item) {
        if (!ft2Link || !item || typeof ft2Link.deleteReceivedFile !== "function")
            return
        var name = String(item.fileName || qsTr("received file"))
        if (ft2Link.deleteReceivedFile(Number(item.id || 0))) {
            refreshFileTransfers()
            refreshReceivedFiles()
            receivedFileStatus = qsTr("Deleted %1").arg(name)
        } else {
            receivedFileStatus = qsTr("Delete failed: %1")
                                 .arg(String(ft2Link.lastError || ""))
        }
    }

    function clearReadReceivedFiles() {
        if (!ft2Link || typeof ft2Link.clearReceivedFiles !== "function")
            return
        var removed = Number(ft2Link.clearReceivedFiles(true) || 0)
        refreshFileTransfers()
        refreshReceivedFiles()
        receivedFileStatus = removed > 0
                             ? qsTr("Deleted %n read received file", "", removed)
                             : qsTr("No read received files")
    }

    function receivedFileHasContent(item) {
        return !!item && (String(item.content || "").length > 0
                          || String(item.contentBase64 || "").length > 0)
    }

    function effectiveReceivedFileDirectory() {
        var configured = String(receivedFileDirectory || "").trim()
        if (configured.length > 0)
            return configured
        if (bridge
                && typeof bridge.defaultFt2LinkReceivedFilesDirectory === "function")
            return String(bridge.defaultFt2LinkReceivedFilesDirectory() || "")
        return ""
    }

    function receivedFileLeafName(path) {
        var normalized = String(path || "").replace(/\\/g, "/")
        var slash = normalized.lastIndexOf("/")
        return slash >= 0 ? normalized.substring(slash + 1) : normalized
    }

    function receivedFileParentDirectory(path) {
        var normalized = String(path || "").replace(/\\/g, "/")
        var slash = normalized.lastIndexOf("/")
        if (slash < 0)
            return ""
        if (slash === 2 && normalized.charAt(1) === ":")
            return normalized.substring(0, 3)
        return slash === 0 ? "/" : normalized.substring(0, slash)
    }

    function receivedFilePathHint(directory, fileName) {
        var base = String(directory || "").replace(/[\\/]+$/, "")
        var leaf = String(fileName || "ft2link_received.bin")
        return base.length > 0 ? (base + "/" + leaf) : leaf
    }

    function cloneReceivedFileMap(source) {
        var clone = ({})
        for (var key in source)
            clone[key] = source[key]
        return clone
    }

    function queueReceivedFileSave(item, directory, preserveExisting, automatic) {
        if (!item || !receivedFileHasContent(item)) {
            receivedFileStatus = qsTr("Received file has no content")
            return false
        }
        if (!bridge
                || typeof bridge.saveFt2LinkReceivedFileAsync !== "function") {
            receivedFileStatus = qsTr("Asynchronous file saving is unavailable")
            return false
        }
        var transferId = Number(item.id || 0)
        var transferKey = String(transferId)
        if (transferId <= 0 || receivedFilePendingTransfers[transferKey]) {
            if (transferId > 0)
                receivedFileStatus = qsTr("Save already in progress")
            return false
        }

        var fileName = String(item.fileName || "ft2link_received.bin").trim()
        if (fileName.length === 0)
            fileName = "ft2link_received.bin"
        var requestId = Number(bridge.saveFt2LinkReceivedFileAsync(
                                   String(directory || ""),
                                   fileName,
                                   String(item.content || ""),
                                   String(item.contentBase64 || ""),
                                   !!item.binary,
                                   preserveExisting !== false))
        if (requestId <= 0) {
            receivedFileStatus = qsTr("Unable to queue received file save")
            return false
        }

        var requests = cloneReceivedFileMap(receivedFileSaveRequests)
        requests[String(requestId)] = {
            transferId: transferId,
            fileName: fileName,
            automatic: automatic === true
        }
        receivedFileSaveRequests = requests
        var pending = cloneReceivedFileMap(receivedFilePendingTransfers)
        pending[transferKey] = requestId
        receivedFilePendingTransfers = pending
        receivedFileIoPendingCount += 1
        receivedFileStatus = automatic
                             ? qsTr("Auto-saving %1...").arg(fileName)
                             : qsTr("Saving %1...").arg(fileName)
        return true
    }

    function saveReceivedFile(item) {
        return queueReceivedFileSave(item,
                                     effectiveReceivedFileDirectory(),
                                     true,
                                     false)
    }

    function saveReceivedFileAs(item) {
        if (!item || !receivedFileHasContent(item)) {
            receivedFileStatus = qsTr("Received file has no content")
            return false
        }
        if (!bridge || typeof bridge.saveFileDialog !== "function") {
            receivedFileStatus = qsTr("Save As is unavailable")
            return false
        }
        var fileName = String(item.fileName || "ft2link_received.bin").trim()
        if (fileName.length === 0)
            fileName = "ft2link_received.bin"
        var path = bridge.saveFileDialog(
                    qsTr("Save FT2-Link received file as"),
                    receivedFilePathHint(effectiveReceivedFileDirectory(), fileName),
                    [qsTr("All files (*)"),
                     qsTr("Text files (*.txt *.md *.log *.csv *.json)"),
                     qsTr("Images (*.png *.jpg *.jpeg *.gif *.bmp)")])
        if (String(path || "").length === 0) {
            receivedFileStatus = qsTr("Save As cancelled")
            return false
        }
        var directory = receivedFileParentDirectory(path)
        var selectedName = receivedFileLeafName(path)
        if (directory.length > 0)
            receivedFileDirectory = directory
        var saveItem = {
            id: item.id,
            fileName: selectedName,
            content: item.content,
            contentBase64: item.contentBase64,
            binary: item.binary
        }
        return queueReceivedFileSave(saveItem, directory, false, false)
    }

    function chooseReceivedFileDirectory() {
        if (!bridge || typeof bridge.openDirectoryDialog !== "function") {
            receivedFileStatus = qsTr("Folder selection is unavailable")
            return
        }
        var path = bridge.openDirectoryDialog(
                    qsTr("Select FT2-Link received files folder"),
                    effectiveReceivedFileDirectory())
        if (String(path || "").length === 0)
            return
        receivedFileDirectory = String(path)
        receivedFileStatus = qsTr("Receive folder: %1").arg(receivedFileDirectory)
    }

    function openReceivedFileDirectory() {
        if (!bridge
                || typeof bridge.openFt2LinkReceivedFilesDirectoryAsync !== "function") {
            receivedFileStatus = qsTr("Open folder is unavailable")
            return
        }
        bridge.openFt2LinkReceivedFilesDirectoryAsync(
                    effectiveReceivedFileDirectory())
        receivedFileStatus = qsTr("Opening received files folder...")
    }

    function scheduleReceivedFileAutoSave() {
        Qt.callLater(function() {
            if (root && typeof root.autoSavePendingReceivedFiles === "function")
                root.autoSavePendingReceivedFiles()
        })
    }

    function autoSavePendingReceivedFiles() {
        if (!receivedFileAutoSave)
            return
        var directory = effectiveReceivedFileDirectory()
        for (var i = 0; i < receivedFiles.length; ++i) {
            var item = receivedFiles[i]
            if (!item || !item.unread || !receivedFileHasContent(item))
                continue
            var transferKey = String(Number(item.id || 0))
            if (receivedFilePendingTransfers[transferKey])
                continue
            queueReceivedFileSave(item, directory, true, true)
        }
    }

    function finishReceivedFileSave(requestId, result) {
        var requestKey = String(Number(requestId || 0))
        var metadata = receivedFileSaveRequests[requestKey]
        if (!metadata)
            return

        var requests = cloneReceivedFileMap(receivedFileSaveRequests)
        delete requests[requestKey]
        receivedFileSaveRequests = requests
        var pending = cloneReceivedFileMap(receivedFilePendingTransfers)
        delete pending[String(Number(metadata.transferId || 0))]
        receivedFilePendingTransfers = pending
        receivedFileIoPendingCount = Math.max(0, receivedFileIoPendingCount - 1)

        if (!(result && result.ok)) {
            var errorText = String(result && result.error
                                   ? result.error : qsTr("unknown error"))
            receivedFileStatus = metadata.automatic
                                 ? qsTr("Auto-save failed: %1").arg(errorText)
                                 : qsTr("Save failed: %1").arg(errorText)
            return
        }

        var savedItem = null
        for (var i = 0; i < receivedFiles.length; ++i) {
            if (Number(receivedFiles[i].id || 0) === Number(metadata.transferId || 0)) {
                savedItem = receivedFiles[i]
                break
            }
        }
        if (savedItem)
            markReceivedFileRead(savedItem, true, false)
        receivedFileStatus = metadata.automatic
                             ? qsTr("Auto-saved %1").arg(
                                   String(result.path || metadata.fileName))
                             : qsTr("Saved %1").arg(
                                   String(result.path || metadata.fileName))
    }

    function finishReceivedFileDirectoryOpen(result) {
        if (result && result.ok)
            receivedFileStatus = qsTr("Opened %1").arg(
                        String(result.path || qsTr("receive folder")))
        else
            receivedFileStatus = qsTr("Open folder failed: %1").arg(
                        String(result && result.error
                               ? result.error : qsTr("unknown error")))
    }

    function bulletinDate(item) {
        if (!item)
            return "--"
        var ms = Number(item.updatedAtMs || item.atMs || 0)
        if (!isFinite(ms) || ms <= 0)
            return "--"
        var d = new Date(ms)
        function pad(n) { return n < 10 ? "0" + n : String(n) }
        return d.getUTCFullYear() + "-" + pad(d.getUTCMonth() + 1)
               + "-" + pad(d.getUTCDate()) + " "
               + pad(d.getUTCHours()) + ":" + pad(d.getUTCMinutes())
    }

    function bulletinDirectionLabel(item) {
        return String(item && item.direction || "") === "Incoming" ? "RX" : "TX"
    }

    function bulletinPeer(item) {
        if (!item)
            return "--"
        return String(item.direction || "") === "Incoming"
               ? String(item.fromCall || "--")
               : String(item.group || "ALL")
    }

    function bulletinText(item) {
        if (!item)
            return ""
        return "BBS " + bulletinDirectionLabel(item)
               + " " + String(item.group || "ALL")
               + " " + bulletinPeer(item)
               + ": " + String(item.title || "Bulletin")
               + "\n\n" + String(item.body || "")
    }

    function copyBulletin(item) {
        if (!item)
            return
        copyPlainText(bulletinText(item))
        if (item.unread)
            markBulletinRead(item, true, false)
        bulletinStatus = "Copied BBS " + String(item.title || "Bulletin")
    }

    function markBulletinRead(item, read, showStatus) {
        if (!ft2Link || !item)
            return false
        var id = Number(item.id || 0)
        if (id <= 0)
            return false
        if (!ft2Link.markBulletinRead(id, read, nowMs()))
            return false
        refreshBulletins()
        if (showStatus !== false) {
            var title = String(item.title || "Bulletin")
            bulletinStatus = read ? ("Marked read " + title)
                                  : ("Marked unread " + title)
        }
        return true
    }

    function markAllBulletinsRead() {
        if (!ft2Link)
            return
        var changed = 0
        for (var i = 0; i < bulletins.length; ++i) {
            var item = bulletins[i]
            if (item && item.unread
                    && ft2Link.markBulletinRead(Number(item.id || 0),
                                                true,
                                                nowMs()))
                ++changed
        }
        refreshBulletins()
        bulletinStatus = changed > 0
                         ? ("Marked read " + changed + " BBS bulletin"
                            + (changed === 1 ? "" : "s"))
                         : "No unread BBS bulletins"
    }

    function clearBulletinList() {
        if (!ft2Link || typeof ft2Link.clearBulletins !== "function")
            return
        ft2Link.clearBulletins()
        refreshBulletins()
        bulletinStatus = "BBS list cleared"
    }

    function prettyJson(value) {
        try {
            return JSON.stringify(value || {}, null, 2)
        } catch (error) {
            return String(value || "")
        }
    }

    function copyPlainText(text) {
        var value = String(text || "")
        if (value.length === 0)
            return
        if (bridge && typeof bridge.copyToClipboard === "function")
            bridge.copyToClipboard(value)
        else
            composeText.text = value
    }

    function copyStatisticsText() {
        if (!ft2Link)
            return
        var text = ft2Link.statisticsText()
        copyPlainText(text)
    }

    function exportLog(kind) {
        if (!ft2Link)
            return
        var mode = String(kind || "OPS").toUpperCase()
        if (mode === "ADIF")
            logExportText = ft2Link.adifLog()
        else if (mode === "OUTBOX" && typeof ft2Link.logbookOutboxText === "function")
            logExportText = ft2Link.logbookOutboxText()
        else if (mode === "CHAT")
            logExportText = ft2Link.chatHistoryLog()
        else if (mode === "CLUSTER" && typeof ft2Link.clusterExportJson === "function")
            logExportText = ft2Link.clusterExportJson()
        else if (mode === "STORE")
            logExportText = ft2Link.localStoreJson()
        else if (mode === "BUNDLE")
            logExportText = ft2Link.logsBundleText()
        else
            logExportText = ft2Link.operationalLog()
    }

    function exportCluster() {
        if (!ft2Link || typeof ft2Link.clusterExportJson !== "function")
            return
        var text = ft2Link.clusterExportJson()
        logExportText = text
        clusterJsonArea.text = text
        databaseActionText = "Cluster export " + String(root.clusterLastHeard.length) + " records"
    }

    function importCluster() {
        if (!ft2Link || typeof ft2Link.importClusterLastHeard !== "function")
            return
        var text = clusterJsonArea.text.trim()
        if (text.length === 0)
            text = logExportText.trim()
        if (text.length === 0)
            return
        var result = ft2Link.importClusterLastHeard(text, nowMs())
        databaseActionText = prettyJson(result)
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function clusterAutoSyncIntervalText() {
        if (clusterAutoSyncSeconds < 60)
            return String(clusterAutoSyncSeconds) + "s"
        if (clusterAutoSyncSeconds % 60 === 0)
            return String(clusterAutoSyncSeconds / 60) + "m"
        return String(clusterAutoSyncSeconds) + "s"
    }

    function cycleClusterAutoSyncInterval() {
        var options = [30, 60, 120, 300, 600, 900]
        var index = 0
        for (var i = 0; i < options.length; ++i) {
            if (clusterAutoSyncSeconds <= options[i]) {
                index = i
                break
            }
        }
        clusterAutoSyncSeconds = options[(index + 1) % options.length]
    }

    function clusterShareSummary(prefix, result) {
        if (!result)
            return prefix + " no result"
        if (!result.ok)
            return prefix + " error: " + String(result.error || "unknown")
        var records = Number(result.records || result.total || root.clusterLastHeard.length || 0)
        var imported = Number(result.imported || 0)
        var merged = Number(result.merged || 0)
        var action = result.action ? String(result.action).toUpperCase() : prefix
        if (imported > 0 || merged > 0)
            return action + " +" + imported + " / ~" + merged + " / " + records + " rec"
        return action + " " + records + " rec"
    }

    function pushClusterShare() {
        if (!ft2Link || typeof ft2Link.writeClusterShareFile !== "function")
            return
        var result = ft2Link.writeClusterShareFile(clusterSharePath.trim())
        databaseActionText = prettyJson(result)
        clusterSyncStatus = clusterShareSummary("PUSH", result)
        if (result && result.ok) {
            clusterSharePath = String(result.path || clusterSharePath)
            clusterJsonArea.text = ft2Link.clusterExportJson()
        }
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function pullClusterShare() {
        if (!ft2Link || typeof ft2Link.mergeClusterShareFile !== "function")
            return
        var result = ft2Link.mergeClusterShareFile(clusterSharePath.trim(), nowMs())
        databaseActionText = prettyJson(result)
        clusterSyncStatus = clusterShareSummary("PULL", result)
        if (result && result.ok)
            clusterSharePath = String(result.path || clusterSharePath)
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function syncClusterShare(showDetail) {
        if (!ft2Link)
            return false
        var detail = showDetail === undefined ? true : !!showDetail
        var result = null
        if (typeof ft2Link.syncClusterShareFile === "function")
            result = ft2Link.syncClusterShareFile(clusterSharePath.trim(), nowMs())
        else if (typeof ft2Link.mergeClusterShareFile === "function"
                 && typeof ft2Link.writeClusterShareFile === "function") {
            var pull = ft2Link.mergeClusterShareFile(clusterSharePath.trim(), nowMs())
            if (pull && pull.ok)
                result = ft2Link.writeClusterShareFile(String(pull.path || clusterSharePath).trim())
            else if (pull && String(pull.error || "").indexOf("does not exist") >= 0)
                result = ft2Link.writeClusterShareFile(clusterSharePath.trim())
            else
                result = pull
        }
        if (!result)
            return false
        if (result.path)
            clusterSharePath = String(result.path)
        clusterSyncStatus = clusterShareSummary("SYNC", result)
        if (detail)
            databaseActionText = prettyJson(result)
        if (result.ok && typeof ft2Link.clusterExportJson === "function")
            clusterJsonArea.text = ft2Link.clusterExportJson()
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
        return !!result.ok
    }

    function clearCluster() {
        if (!ft2Link || typeof ft2Link.clearClusterLastHeard !== "function")
            return
        ft2Link.clearClusterLastHeard()
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function writeAdifFile() {
        if (!ft2Link || typeof ft2Link.writeAdifLogFile !== "function")
            return
        var result = ft2Link.writeAdifLogFile("")
        logExportText = prettyJson(result)
        refreshStoreAudit()
    }

    function queueSelectedLogbookUpload() {
        if (!ft2Link || typeof ft2Link.queueLogbookUpload !== "function"
            || selectedSessionId === 0)
            return
        var result = ft2Link.queueLogbookUpload(selectedSessionId, "ALL", nowMs())
        logExportText = prettyJson(result)
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
    }

    function queueAllLogbookUploads() {
        if (!ft2Link || typeof ft2Link.queueAllLogbookUploads !== "function")
            return
        var result = ft2Link.queueAllLogbookUploads("ALL", nowMs())
        logExportText = prettyJson(result)
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
    }

    function sendQueuedLogbookUploads() {
        var hasOutboxUpload = bridge && typeof bridge.uploadExternalAdifForOutbox === "function"
        var hasLegacyUpload = bridge && typeof bridge.uploadExternalAdif === "function"
        if (!ft2Link || !bridge
            || typeof ft2Link.logbookUploadPayload !== "function"
            || typeof ft2Link.markLogbookUpload !== "function"
            || (!hasOutboxUpload && !hasLegacyUpload))
            return
        var sent = 0
        var failed = 0
        var skipped = 0
        var details = []
        for (var i = 0; i < logbookOutbox.length; ++i) {
            var row = logbookOutbox[i]
            var state = String(row.state || "")
            if (state !== "Queued" && state !== "Failed")
                continue
            var id = Number(row.id || 0)
            var payload = ft2Link.logbookUploadPayload(id)
            if (!payload || !payload.ok) {
                ++failed
                continue
            }
            var result = hasOutboxUpload
                         ? bridge.uploadExternalAdifForOutbox(id,
                                                              String(payload.remoteCall || ""),
                                                              String(payload.adif || ""),
                                                              String(payload.target || "ALL"))
                         : bridge.uploadExternalAdif(String(payload.remoteCall || ""),
                                                     String(payload.adif || ""),
                                                     String(payload.target || "ALL"))
            var ok = !!(result && result.ok)
            var nextState = ok ? String(result.state || "Submitted") : "Failed"
            var detail = result && result.detail ? String(result.detail)
                         : (result && result.error ? String(result.error) : "")
            ft2Link.markLogbookUpload(id, nextState, detail, nowMs())
            if (ok)
                ++sent
            else
                ++failed
            if (result && result.skipped)
                skipped += result.skipped.length
            details.push(result)
        }
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
        logExportText = prettyJson({
            ok: failed === 0 && sent > 0,
            submitted: sent,
            failed: failed,
            skipped: skipped,
            details: details
        })
    }

    function clearLogbookOutbox() {
        if (!ft2Link || typeof ft2Link.clearLogbookOutbox !== "function")
            return
        ft2Link.clearLogbookOutbox()
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
        logExportText = "Logbook outbox cleared"
    }

    function copyAdifPath() {
        if (!ft2Link || typeof ft2Link.adifLogPath !== "function")
            return
        copyPlainText(ft2Link.adifLogPath())
    }

    function backupStore() {
        if (!ft2Link)
            return
        var result = ft2Link.backupLocalStore("")
        databaseActionText = prettyJson(result)
        refreshStoreAudit()
    }

    function fixStore(makeBackup) {
        if (!ft2Link)
            return
        var result = ft2Link.fixLocalStore(!!makeBackup)
        databaseActionText = prettyJson(result)
        refreshStoreAudit()
    }

    function saveFrequencyPresets() {
        if (!ft2Link || typeof ft2Link.setFrequencyPresets !== "function")
            return
        var result = ft2Link.setFrequencyPresets(frequencyPresetText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok && result.text)
            frequencyPresetText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
        refreshQsyPlan()
    }

    function resetFrequencyPresets() {
        if (!ft2Link || typeof ft2Link.resetFrequencyPresets !== "function")
            return
        var result = ft2Link.resetFrequencyPresets()
        databaseActionText = prettyJson(result)
        if (result && result.text)
            frequencyPresetText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
    }

    function saveAllowedQsyRanges() {
        if (!ft2Link || typeof ft2Link.setAllowedQsyRanges !== "function")
            return
        var result = ft2Link.setAllowedQsyRanges(allowedQsyRangeText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok && result.text)
            allowedQsyRangeText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
        refreshQsyPlan()
    }

    function resetAllowedQsyRanges() {
        if (!ft2Link || typeof ft2Link.resetAllowedQsyRanges !== "function")
            return
        var result = ft2Link.resetAllowedQsyRanges()
        databaseActionText = prettyJson(result)
        if (result && result.text)
            allowedQsyRangeText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
        refreshQsyPlan()
    }

    function saveFrequencySchedule() {
        if (!ft2Link || typeof ft2Link.setFrequencySchedule !== "function")
            return
        var result = ft2Link.setFrequencySchedule(frequencyScheduleText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok)
            frequencyScheduleText.text = String(result.text || "")
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
    }

    function resetFrequencySchedule() {
        if (!ft2Link || typeof ft2Link.resetFrequencySchedule !== "function")
            return
        var result = ft2Link.resetFrequencySchedule()
        databaseActionText = prettyJson(result)
        frequencyScheduleText.text = ""
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
    }

    function activeFrequencyScheduleItem() {
        if (!ft2Link || typeof ft2Link.activeFrequencySchedule !== "function")
            return null
        var item = ft2Link.activeFrequencySchedule(nowMs())
        if (!item || !item.active || Number(item.dialFrequencyHz || 0) <= 0)
            return null
        return item
    }

    function activeFrequencyScheduleLine() {
        var item = activeFrequencyScheduleItem()
        if (!item)
            return "No active schedule slot"
        return String(item.label || item.action || "SCHEDULE")
               + "  " + frequencyHzText(Number(item.dialFrequencyHz || 0))
               + "  " + String(item.cqType || "CQ")
    }

    function frequencyScheduleApplyKey(item) {
        if (!item)
            return ""
        return String(item.startMinute || 0) + "-"
               + String(item.endMinute || 0) + "|"
               + String(item.action || "") + "|"
               + String(Math.round(Number(item.dialFrequencyHz || 0)))
    }

    function applyActiveFrequencySchedule(autoTriggered) {
        var item = activeFrequencyScheduleItem()
        if (!item) {
            if (!autoTriggered)
                databaseActionText = "No active frequency schedule slot"
            return false
        }
        if (!bridge || typeof bridge.qsyTo !== "function") {
            databaseActionText = "Schedule target ready "
                                 + frequencyHzText(Number(item.dialFrequencyHz || 0))
                                 + " but bridge.qsyTo is unavailable"
            return false
        }
        var key = frequencyScheduleApplyKey(item)
        if (autoTriggered && key.length > 0 && lastFrequencyScheduleApplyKey === key)
            return true
        var hz = Math.round(Number(item.dialFrequencyHz || 0))
        if (hz <= 0)
            return false
        bridge.qsyTo(hz, "FT2-Link schedule")
        lastFrequencyScheduleApplyKey = key
        databaseActionText = (autoTriggered ? "Schedule auto-applied " : "Schedule applied ")
                             + frequencyHzText(hz) + " "
                             + String(item.label || item.action || "")
        return true
    }

    function prepareActiveScheduleBroadcast() {
        var item = activeFrequencyScheduleItem()
        if (!item) {
            databaseActionText = "No active frequency schedule slot"
            return
        }
        if (!ft2Link || typeof ft2Link.qsyBroadcastText !== "function")
            return
        var hz = Math.round(Number(item.dialFrequencyHz || 0))
        var text = ft2Link.qsyBroadcastText(hz,
                                            String(item.label || item.cqType || "schedule"),
                                            String(item.action || "SCHEDULE"))
        if (String(text || "").length > 0) {
            broadcastText.text = text
            toolPageIndex = 4
            databaseActionText = "Schedule QSY broadcast prepared "
                                 + frequencyHzText(hz)
        }
    }

    function auditStore() {
        refreshStoreAudit()
        databaseActionText = prettyJson(storeAudit)
    }

    function currentFormTemplate() {
        if (formTemplates.length === 0)
            return null
        return formTemplates[Math.max(0, Math.min(formTemplateIndex, formTemplates.length - 1))]
    }

    function currentCqType() {
        if (cqTypeOptions.length === 0)
            return "CQ"
        return String(cqTypeOptions[Math.max(0, Math.min(cqTypeIndex, cqTypeOptions.length - 1))])
    }

    function cycleCqType() {
        if (cqTypeOptions.length === 0)
            return
        cqTypeIndex = (cqTypeIndex + 1) % cqTypeOptions.length
    }

    function currentCqLocator() {
        var locator = cqLocator.trim().toUpperCase()
        if (locator.length === 0 && bridge && bridge.grid)
            locator = String(bridge.grid).trim().toUpperCase()
        locator = locator.replace(/[^A-Z0-9]/g, "")
        return locator.slice(0, 8)
    }

    function currentFormType() {
        var template = currentFormTemplate()
        return template ? String(template.id || "ICS213") : "ICS213"
    }

    function currentFormLabel() {
        var template = currentFormTemplate()
        return template ? String(template.label || template.id || "FORM") : "FORM"
    }

    function cycleFormTemplate() {
        if (formTemplates.length === 0)
            return
        formTemplateIndex = (formTemplateIndex + 1) % formTemplates.length
        applyCurrentFormTemplate()
    }

    function applyCurrentFormTemplate() {
        var template = currentFormTemplate()
        if (!template)
            return
        formFieldsText.text = String(template.fields || "").replace(/\n/g, "; ")
    }

    function parseFormFields(text) {
        var map = {}
        var parts = String(text || "").split(/[;\n]/)
        for (var i = 0; i < parts.length; ++i) {
            var line = parts[i].trim()
            if (line.length === 0)
                continue
            var at = line.indexOf("=")
            if (at <= 0) {
                map["message"] = line
                continue
            }
            var key = line.slice(0, at).trim()
            var value = line.slice(at + 1).trim()
            if (key.length > 0)
                map[key] = value
        }
        return map
    }

    function updateSelectedSessionFromSessions() {
        selectedRemoteCall = ""
        selectedSessionStateName = ""
        for (var i = 0; i < sessions.length; ++i) {
            if (Number(sessions[i].sessionId) === selectedSessionId) {
                selectedRemoteCall = String(sessions[i].remoteCall || "")
                selectedSessionStateName = String(sessions[i].stateName || "")
                break
            }
        }
    }

    function normalizeStationCall(call) {
        return String(call || "").trim().toUpperCase()
    }

    function stationSessionForCall(call) {
        var wanted = normalizeStationCall(call)
        if (wanted.length === 0)
            return null
        for (var i = 0; i < sessions.length; ++i) {
            var session = sessions[i]
            if (normalizeStationCall(session.remoteCall) === wanted)
                return session
        }
        return null
    }

    function isStationConnecting(call) {
        var wanted = normalizeStationCall(call)
        var session = stationSessionForCall(wanted)
        if (!session)
            return wanted.length > 0 && pendingConnectCall === wanted
        var state = String(session.stateName || "")
        return state.length > 0 && state !== "Connected"
               && state !== "Closed" && state !== "Rejected"
    }

    function stationConnectText(call) {
        var wanted = normalizeStationCall(call)
        var session = stationSessionForCall(wanted)
        if (!session)
            return wanted.length > 0 && pendingConnectCall === wanted ? "Connecting..." : "Connect"
        var state = String(session.stateName || "")
        if (state === "Connected")
            return "Connected"
        if (state === "Closed" || state === "Rejected" || state.length === 0)
            return "Connect"
        return "Connecting..."
    }

    function stationConnectEnabled(call) {
        var label = stationConnectText(call)
        return !!ft2Link && label === "Connect"
    }

    function stationConnectTip(call) {
        var label = stationConnectText(call)
        if (label === "Connected")
            return "Station already connected"
        if (label === "Connecting...")
            return "Connection handshake in progress"
        return "Send HELLO and connect this station"
    }

    function updatePendingConnectCall() {
        if (pendingConnectCall.length === 0)
            return
        var session = stationSessionForCall(pendingConnectCall)
        if (!session)
            return
        var state = String(session.stateName || "")
        if (state === "Connected" || state === "Closed" || state === "Rejected")
            pendingConnectCall = ""
    }

    function selectSession(sessionId) {
        selectedSessionId = Number(sessionId)
        updateSelectedSessionFromSessions()
        chatUnreadBelow = false
        chatScrollPinned = true
        refreshMessages()
        Qt.callLater(scrollChatToEnd)
    }

    function closeSelectedSession() {
        if (!ft2Link || selectedSessionId === 0)
            return false
        if (ft2Link.closeSession(selectedSessionId, nowMs())) {
            refreshSessions()
            return true
        }
        return false
    }

    function disconnectSelectedSession() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!selectedSessionConnected) {
            closeSelectedSession()
            return
        }
        var farewell = ft2Link.expandCannedMessage("73 <CALL> DE <MYCALL> <DISC>",
                                                   selectedSessionId,
                                                   nowMs())
        composeText.text = farewell
        if (sendChatText())
            closeSelectedSession()
    }

    function startSession(call) {
        if (!ft2Link || !call)
            return
        if (ft2Link.radioTxArmed) {
            startRadioSession(call)
            return
        }
        syncLocalStation()
        applyCapabilities()
        lastHelloBytes = ft2Link.startSessionHelloBytes(String(call), nowMs())
        selectedRemoteCall = String(call)
        pendingConnectCall = normalizeStationCall(call)
        selectedSessionId = ft2Link.activeSessionId
        refreshSessions()
    }

    function startRadioSession(call) {
        if (!ft2Link || !call)
            return
        syncLocalStation()
        applyCapabilities()
        lastHelloBytes = null
        pendingConnectCall = normalizeStationCall(call)
        if (ft2Link.startSessionRadioHandshake(String(call), nowMs())) {
            selectedRemoteCall = String(call)
            selectedSessionId = ft2Link.activeSessionId
            refreshSessions()
        } else {
            pendingConnectCall = ""
        }
    }

    // 1.0.462 iu8lmc: CONNETTI a prova di errore. Connettersi a una stazione
    // manda un HELLO via radio; se ARM non e' attivo lo attiva, poi fa
    // l'handshake radio (prima, senza ARM, il click finiva sul percorso locale
    // e non trasmetteva nulla, confondendo l'utente).
    function connectStationRadio(call) {
        if (!ft2Link || !call)
            return false
        var wanted = normalizeStationCall(call)
        if (wanted.length === 0 || !stationConnectEnabled(wanted))
            return false
        var accepted = requestSlotSniffedTx("CONNECT",
                                            "HELLO " + wanted,
                                            false,
                                            ({ call: String(call) }))
        if (!accepted)
            return false
        pendingConnectCall = wanted
        if (!ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        databaseActionText = "CONNECT queued: " + wanted
        return true
    }

    function transmitBeacon(cq) {
        requestSlotSniffedTx(cq ? "CQ" : "BEACON",
                             cq ? "CQ" : "BEACON",
                             cq && !skipCqSlot,
                             ({ cq: cq }))
    }

    function transmitBeaconNow(cq) {
        if (!ft2Link)
            return
        syncLocalStation()
        applyCapabilities()
        var type = currentCqType()
        var locator = currentCqLocator()
        if (cq && !skipCqSlot) {
            var slot = currentCqSlot()
            if (slot) {
                if (typeof ft2Link.transmitSpecialCqRadio === "function"
                    ? ft2Link.transmitSpecialCqRadio(type, locator, Number(slot.slotId || 0), 750, nowMs())
                    : ft2Link.transmitCqSlotRadio(Number(slot.slotId || 0), 750, nowMs()))
                    cqSlotWaitUntilMs = nowMs() + cqSlotWaitSeconds * 1000
                return
            }
        }
        if (cq && typeof ft2Link.transmitSpecialCqRadio === "function")
            ft2Link.transmitSpecialCqRadio(type, locator, 0, 750, nowMs())
        else
            ft2Link.transmitBeaconRadio(cq, nowMs())
    }

    function configureAutoBeacon(enabled) {
        if (!ft2Link)
            return
        syncLocalStation()
        applyCapabilities()
        if (!ft2Link.configureAutoBeacon(enabled, beaconIntervalSeconds, true, nowMs()))
            autoBeaconCheck.checked = false
    }

    function toggleAutoBeacon(enabled) {
        if (!ft2Link)
            return
        if (enabled && !ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        configureAutoBeacon(enabled)
    }

    function cycleBeaconInterval() {
        if (beaconIntervalSeconds === 180)
            beaconIntervalSeconds = 300
        else if (beaconIntervalSeconds === 300)
            beaconIntervalSeconds = 600
        else
            beaconIntervalSeconds = 180
        if (ft2Link && ft2Link.autoBeaconEnabled)
            ft2Link.configureAutoBeacon(true, beaconIntervalSeconds, true, nowMs())
    }

    function beaconIntervalText() {
        return Math.round(beaconIntervalSeconds / 60) + " min"
    }

    function beaconCooldownSeconds() {
        if (!ft2Link || typeof ft2Link.beaconCooldownSeconds !== "function")
            return 0
        return Math.max(0, Number(ft2Link.beaconCooldownSeconds(nowMs())))
    }

    function clampValue(value, minValue, maxValue) {
        var number = Math.round(Number(value))
        if (!isFinite(number))
            number = minValue
        return Math.max(minValue, Math.min(maxValue, number))
    }

    function resizeStationPane(width) {
        stationPaneWidth = clampValue(width, 160, 420)
    }

    function resizeSessionPane(width) {
        sessionPaneWidth = clampValue(width, 130, 340)
    }

    function messageListAtEnd() {
        if (typeof messageList === "undefined" || !messageList)
            return true
        if (messageList.contentHeight <= messageList.height + 4)
            return true
        return messageList.atYEnd
               || messageList.contentY >= messageList.contentHeight - messageList.height - 10
    }

    function scrollChatToEnd() {
        if (typeof messageList === "undefined" || !messageList)
            return
        messageList.positionViewAtEnd()
        chatUnreadBelow = false
        chatScrollPinned = true
    }

    function cqTxButtonText() {
        if (slotSnifferActive() && slotSnifferAction === "CQ")
            return "SNIFF " + slotSnifferRemainingSeconds() + "s"
        if (cqSlotWaitRemainingSeconds() > 0)
            return "WAIT " + cqSlotWaitRemainingSeconds() + "s"
        return "CQ TX"
    }

    function armOrTransmitBeacon(cq) {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        transmitBeacon(cq)
    }

    function armOrTransmitBroadcast() {
        if (!ft2Link)
            return false
        if (broadcastTxPending()) {
            databaseActionText = slotSnifferLine()
            return false
        }
        if (!ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        return sendBroadcastText()
    }

    function broadcastTxPending() {
        return slotSnifferActive() && slotSnifferAction === "BCAST"
    }

    function broadcastTxButtonText() {
        if (broadcastTxPending())
            return "BCAST queued..."
        return "BCAST TX"
    }

    function armOrTransmitPathFinderRequest() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendPathFinderRequest()
    }

    function armOrTransmitPathFinderResponse() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendPathFinderResponse()
    }

    function armOrTransmitMailbox() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendMailboxText()
    }

    function armOrTransmitRelayMailbox() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendRelayMailbox()
    }

    function armOrTransmitForm() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendFormText()
    }

    function armOrTransmitFile() {
        if (!ft2Link)
            return false
        if (fileTxPending()) {
            fileTransferStatus = slotSnifferActive()
                                 ? slotSnifferLine()
                                 : "FILE transfer already in progress"
            return false
        }
        if (!ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        return sendFileText()
    }

    function fileTxInFlight() {
        if (ft2Link && selectedSessionId > 0
                && typeof ft2Link.applicationRadioTxReady === "function"
                && ft2Link.applicationRadioTxReady(selectedSessionId))
            return false
        var target = String(selectedRemoteCall || "").trim().toUpperCase()
        for (var i = fileTransfers.length - 1; i >= 0; --i) {
            var transfer = fileTransfers[i]
            if (String(transfer.direction || "") !== "Outgoing")
                continue
            if (target.length > 0
                    && String(transfer.toCall || "").trim().toUpperCase() !== target)
                continue
            var state = String(transfer.state || "").trim().toUpperCase()
            if (state === "PENDING" || state === "QUEUED"
                    || state === "SENDING" || state === "RETRY")
                return true
        }
        return false
    }

    function fileTxPending() {
        return (slotSnifferActive() && slotSnifferAction === "FILE")
               || fileTxInFlight()
    }

    function fileTxButtonText() {
        if (slotSnifferActive() && slotSnifferAction === "FILE")
            return "FILE queued..."
        if (fileTxInFlight())
            return "FILE sending..."
        return "SEND FILE"
    }

    function armOrTransmitBulletin() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendBulletinText()
    }

    function armOrTransmitPing(call) {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendPing(call)
    }

    function completeLoopbackAck() {
        if (!ft2Link || !lastHelloBytes || selectedRemoteCall.length === 0)
            return
        var ack = ft2Link.answerHelloBytes(selectedRemoteCall, lastHelloBytes, nowMs())
        if (ack && ack.length > 0)
            ft2Link.receiveHelloAckBytes(ack, nowMs())
        selectedSessionId = ft2Link.activeSessionId
        refreshSessions()
    }

    function addManualStation() {
        if (!ft2Link)
            return
        var call = manualCall.text.trim().toUpperCase()
        if (call.length === 0)
            return
        applyCapabilities()
        ft2Link.observeStation(call, manualGrid.text.trim().toUpperCase(), manualName.text.trim(),
                               manualCq.checked, true, true, true, true,
                               preferW2300 ? 2 : 1,
                               robustMode ? 1 : 0,
                               nowMs())
        if (manualTag.text.trim().length > 0)
            ft2Link.setContactTag(call, manualTag.text.trim(), nowMs())
        manualCall.text = ""
        manualTag.text = ""
        refreshStations()
        refreshContactHistory()
    }

    function sendChatText() {
        if (!ft2Link || selectedSessionId === 0)
            return false
        var text = composeText.text.trim()
        if (text.length === 0)
            return false
        if (!guardWideTx("CHAT TX"))
            return false
        rememberOutgoingQsyInvite(text)
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
        }
        if (ft2Link.transmitPreparedRadioTxAudio(selectedSessionId, text, nowMs())) {
            composeText.text = ""
            refreshSessions()
            return true
        }
        refreshSessions()
        return false
    }

    function sendBroadcastText() {
        if (!ft2Link)
            return false
        var text = broadcastText.text.trim()
        if (text.length === 0)
            return false
        var accepted = requestSlotSniffedTx("BCAST", "BCAST", false,
                                            ({ text: text }))
        if (accepted)
            databaseActionText = "BCAST queued"
        return accepted
    }

    function sendBroadcastTextNow(queuedText) {
        if (!ft2Link)
            return false
        var text = String(queuedText || "").trim()
        if (text.length === 0)
            text = broadcastText.text.trim()
        if (text.length === 0)
            return false
        if (!ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        if (ft2Link.transmitBroadcastRadio(text, nowMs())) {
            if (broadcastText.text.trim() === text)
                broadcastText.text = ""
            refreshBroadcasts()
            refreshAlerts()
            databaseActionText = "BCAST accepted for TX"
            return true
        }
        databaseActionText = "BCAST TX failed: " + String(ft2Link.lastError || "unknown")
        return false
    }

    function prepareQsyBroadcast() {
        if (!ft2Link || typeof ft2Link.qsyBroadcastText !== "function")
            return
        var hz = 0
        var label = ""
        var reason = "MANUAL"
        if (typeof ft2Link.activeFrequencySchedule === "function") {
            var active = ft2Link.activeFrequencySchedule(nowMs())
            if (active && active.active && Number(active.dialFrequencyHz || 0) > 0) {
                hz = Number(active.dialFrequencyHz)
                label = String(active.label || active.cqType || active.action || "schedule")
                reason = String(active.action || "SCHEDULE")
            }
        }
        if (hz <= 0) {
            hz = currentDialFrequencyHz()
            label = "current"
        }
        var text = ft2Link.qsyBroadcastText(Math.round(hz), label, reason)
        if (String(text || "").length === 0) {
            databaseActionText = "QSY broadcast requires a dial frequency"
            return
        }
        broadcastText.text = text
        databaseActionText = "QSY broadcast prepared " + frequencyHzText(hz)
    }

    function saveAlertTags() {
        if (!ft2Link || typeof ft2Link.setCustomAlertTags !== "function")
            return
        var result = ft2Link.setCustomAlertTags(alertTagsText.text)
        databaseActionText = prettyJson(result)
        alertTagsDirty = false
        refreshAlertTags(true)
        refreshStatistics()
        refreshStoreAudit()
    }

    function clearAlertTags() {
        if (!ft2Link || typeof ft2Link.clearCustomAlertTags !== "function")
            return
        var result = ft2Link.clearCustomAlertTags()
        databaseActionText = prettyJson(result)
        alertTagsText.text = ""
        alertTagsDirty = false
        refreshAlertTags(true)
        refreshStatistics()
        refreshStoreAudit()
    }

    function visibleAlerts(showArchived) {
        var result = []
        for (var i = 0; i < alerts.length; ++i) {
            var item = alerts[i]
            if (!item)
                continue
            if (!!item.archived === !!showArchived)
                result.push(item)
        }
        return result
    }

    function activeAlerts() {
        return visibleAlerts(false)
    }

    function archivedAlertCount() {
        var count = 0
        for (var i = 0; i < alerts.length; ++i) {
            if (alerts[i] && alerts[i].archived)
                ++count
        }
        return count
    }

    function alertDate(item) {
        if (!item)
            return "--"
        var ms = Number(item.updatedAtMs || item.atMs || 0)
        if (!isFinite(ms) || ms <= 0)
            return "--"
        var d = new Date(ms)
        function pad(n) { return n < 10 ? "0" + n : String(n) }
        return d.getUTCFullYear() + "-" + pad(d.getUTCMonth() + 1)
               + "-" + pad(d.getUTCDate()) + " "
               + pad(d.getUTCHours()) + ":" + pad(d.getUTCMinutes())
    }

    function markAlertItemRead(item, read) {
        if (!ft2Link || !item || typeof ft2Link.markAlertRead !== "function")
            return
        if (ft2Link.markAlertRead(Number(item.id || 0), read, nowMs())) {
            refreshAlerts()
            refreshStatistics()
            refreshStoreAudit()
        }
    }

    function archiveAlertItem(item, archived) {
        if (!ft2Link || !item || typeof ft2Link.archiveAlert !== "function")
            return
        if (ft2Link.archiveAlert(Number(item.id || 0), archived, nowMs())) {
            refreshAlerts()
            refreshStatistics()
            refreshStoreAudit()
        }
    }

    function clearArchivedAlerts() {
        if (!ft2Link || typeof ft2Link.clearArchivedAlertEvents !== "function")
            return
        ft2Link.clearArchivedAlertEvents()
        refreshAlerts()
        refreshStatistics()
        refreshStoreAudit()
    }

    function copyAlertItem(item) {
        if (!item)
            return
        var text = "ALERT " + String(item.tag || "--") + " "
                   + String(item.fromCall || "--") + ": "
                   + String(item.text || "")
        copyPlainText(text)
        markAlertItemRead(item, true)
    }

    function pathFinderTarget() {
        return pathTargetText.text.trim().toUpperCase()
    }

    function pathFinderCandidate() {
        if (!ft2Link)
            return null
        var target = pathFinderTarget()
        if (target.length === 0)
            return null
        var item = ft2Link.pathFinderCandidate(target, nowMs())
        if (!item || !item.canRespond)
            return null
        return item
    }

    function pathRelayHintForTarget(target) {
        if (!ft2Link || typeof ft2Link.pathRelayCandidate !== "function")
            return null
        var call = String(target || "").trim().toUpperCase()
        if (call.length === 0)
            return null
        var item = ft2Link.pathRelayCandidate(call, nowMs())
        if (!item || !item.canRelay)
            return null
        return item
    }

    function pathRelayHint() {
        var target = ""
        if (typeof mailToText !== "undefined" && mailToText)
            target = mailToText.text.trim().toUpperCase()
        if (target.length === 0)
            target = pathFinderTarget()
        return pathRelayHintForTarget(target)
    }

    function pathRelayLine() {
        var item = pathRelayHint()
        if (!item)
            return "No path relay hint"
        if (String(item.line || "").length > 0)
            return String(item.line)
        var target = String(item.targetCall || "--")
        var relay = String(item.relayCall || "--")
        var parked = Number(item.parkedMailboxCount || 0)
        var heardAge = Number(item.heardAgeMinutes)
        var heard = isFinite(heardAge) && heardAge >= 0 ? " heard " + heardAge + "m" : ""
        return "Relay " + relay + " -> " + target + heard
               + " / mail " + parked
               + (parked > 0 ? " / ready" : " / park mail first")
    }

    function usePathRelayForMail() {
        var item = pathRelayHint()
        if (!item)
            return
        mailToText.text = String(item.targetCall || "")
        if (String(item.mailboxSubject || "").length > 0
            && mailSubjectText.text.trim().length === 0)
            mailSubjectText.text = String(item.mailboxSubject)
        toolPageIndex = 5
    }

    function callPathRelay() {
        var item = pathRelayHint()
        if (!item)
            return
        startSession(String(item.relayCall || ""))
    }

    function forwardPathRelay() {
        var item = pathRelayHint()
        if (!item || !item.readyToForward)
            return
        usePathRelayForMail()
        startSession(String(item.relayCall || ""))
    }

    function stationRelayWorkflow(station) {
        if (!station)
            return null
        var workflow = station.relayWorkflow
        if (workflow && workflow.canRelay)
            return workflow
        if (!ft2Link || typeof ft2Link.relayWorkflowForStation !== "function")
            return null
        var call = String(station.call || "").trim().toUpperCase()
        if (call.length === 0)
            return null
        workflow = ft2Link.relayWorkflowForStation(call, nowMs())
        return workflow && workflow.canRelay ? workflow : null
    }

    function stationSubtitle(station) {
        var workflow = stationRelayWorkflow(station)
        if (workflow && String(workflow.line || "").length > 0)
            return String(workflow.line)
        return (station.cqLocator || station.locator || "--")
               + "  " + (station.name || "")
               + (root.stationHistoryMode && String(station.profileName || "").length > 0
                  ? "  " + String(station.profileName || "")
                  : "")
               + (!root.stationHistoryMode && Number(station.parkedMailboxCount || 0) > 0
                  ? "  MAIL " + Number(station.parkedMailboxCount || 0)
                  : "")
               + (!root.stationHistoryMode && String(station.pathRelayTarget || "").length > 0
                  ? "  RLY>" + String(station.pathRelayTarget || "")
                  : "")
    }

    function stationSubtitleColor(station) {
        var workflow = stationRelayWorkflow(station)
        if (workflow) {
            var priority = String(workflow.priority || "NORMAL")
            if (priority === "EMCOMM")
                return root.red
            if (priority === "URGENT")
                return root.amber
            return root.green
        }
        return String(station.pathRelayTarget || "").length > 0
               ? root.amber
               : (Number(station.parkedMailboxCount || 0) > 0
                  ? root.green
                  : root.textSecondary)
    }

    function useStationRelayWorkflow(station) {
        var workflow = stationRelayWorkflow(station)
        if (!workflow)
            return
        mailToText.text = String(workflow.targetCall || "")
        if (String(workflow.mailboxSubject || "").length > 0)
            mailSubjectText.text = String(workflow.mailboxSubject || "")
        pathTargetText.text = String(workflow.targetCall || "")
        toolPageIndex = 5
        startSession(String(workflow.relayCall || station.call || ""))
    }

    function sendPathFinderRequest() {
        if (!ft2Link)
            return
        var target = pathFinderTarget()
        if (target.length === 0)
            return
        if (ft2Link.transmitPathFinderRadio(target, nowMs())) {
            refreshBroadcasts()
            refreshAlerts()
        }
    }

    function sendPathFinderResponse() {
        if (!ft2Link)
            return
        var target = pathFinderTarget()
        if (target.length === 0)
            return
        if (ft2Link.transmitPathFinderResponseRadio(target, nowMs())) {
            refreshBroadcasts()
            refreshAlerts()
        }
    }

    function configureDigipeater() {
        if (!ft2Link || typeof ft2Link.configureDigipeater !== "function")
            return
        var result = ft2Link.configureDigipeater(digipeaterEnabledSetting,
                                                 digipeaterMaxHopsSetting)
        digipeaterState = result || ({})
        refreshDigipeater()
    }

    function configureBbsFileServer() {
        if (!ft2Link || typeof ft2Link.configureBbsFileServer !== "function")
            return
        var result = ft2Link.configureBbsFileServer(bbsFileServerEnabledSetting)
        bbsFileServerState = result || ({})
        refreshBbsFileServer()
    }

    function clearBbsServerBinarySelection() {
        bbsServerContentBase64 = ""
        bbsServerBinary = false
        bbsServerBytes = 0
    }

    function publishBbsServerFile() {
        if (!ft2Link || typeof ft2Link.publishBbsSharedFileText !== "function")
            return
        var name = bbsServerFileNameText.text.trim()
        var body = bbsServerBodyText.text
        if (name.length === 0
                || (!bbsServerBinary && body.length === 0)
                || (bbsServerBinary && bbsServerContentBase64.length === 0)) {
            bbsServerStatus = "Choose a file name and content"
            return
        }
        var result = null
        if (bbsServerBinary) {
            if (typeof ft2Link.publishBbsSharedFileBytes !== "function") {
                bbsServerStatus = "Binary BBS publish unavailable"
                return
            }
            result = ft2Link.publishBbsSharedFileBytes(name,
                                                       bbsServerContentBase64,
                                                       nowMs())
        } else {
            result = ft2Link.publishBbsSharedFileText(name, body, nowMs())
        }
        if (result && result.ok) {
            bbsServerStatus = "Published " + String(result.fileName || name)
                              + (bbsServerBinary ? " binary" : "")
            bbsFileServerEnabledSetting = true
            configureBbsFileServer()
            refreshBbsFileServer()
        } else {
            bbsServerStatus = "Publish failed: "
                              + String(result && result.error ? result.error
                                       : (ft2Link.lastError || "unknown"))
        }
    }

    function loadBbsServerTextFile() {
        if (!bridge || typeof bridge.openFileDialog !== "function"
                || typeof bridge.readTextFile !== "function") {
            bbsServerStatus = "File picker unavailable"
            return
        }
        var path = bridge.openFileDialog("Publish FT2-Link BBS text file",
                                         "",
                                         ["Text files (*.txt *.md *.log *.csv *.json)",
                                          "All files (*)"])
        if (!path || path.length === 0)
            return
        var result = bridge.readTextFile(path, root.filePayloadLimitBytes)
        if (!result || !result.ok) {
            bbsServerStatus = "Load failed: "
                              + String(result && result.error ? result.error : "unknown")
            return
        }
        bbsServerFileNameText.text = root.baseFileName(path)
        bbsServerBodyText.text = String(result.text || "")
        clearBbsServerBinarySelection()
        bbsServerStatus = "Loaded " + bbsServerFileNameText.text
    }

    function loadBbsServerBinaryFile() {
        if (!bridge || typeof bridge.openFileDialog !== "function"
                || typeof bridge.readFileBytes !== "function") {
            bbsServerStatus = "Binary file picker unavailable"
            return
        }
        var path = bridge.openFileDialog("Publish FT2-Link BBS binary file",
                                         "",
                                         ["All files (*)",
                                          "Images (*.png *.jpg *.jpeg *.gif *.bmp)",
                                          "Documents (*.txt *.md *.pdf *.json)"])
        if (!path || path.length === 0)
            return
        var result = bridge.readFileBytes(path, root.filePayloadLimitBytes)
        if (!result || !result.ok) {
            bbsServerStatus = "Load failed: "
                              + String(result && result.error ? result.error : "unknown")
            return
        }
        var bytes = Number(result.bytes || 0)
        var fullSize = Number(result.fileSize || bytes)
        if (result.truncated || fullSize > root.filePayloadLimitBytes) {
            clearBbsServerBinarySelection()
            bbsServerStatus = "File too large: " + fullSize + " B / max "
                              + root.filePayloadLimitBytes + " B"
            return
        }
        bbsServerFileNameText.text = root.baseFileName(path)
        bbsServerBodyText.text = "Binary payload " + bytes + " B"
        bbsServerContentBase64 = String(result.base64 || "")
        bbsServerBinary = true
        bbsServerBytes = bytes
        bbsServerStatus = "Loaded binary " + bbsServerFileNameText.text
                          + " " + bytes + " B"
    }

    function requestBbsFileList() {
        if (!ft2Link || typeof ft2Link.requestBbsFileListRadio !== "function")
            return
        if (!root.selectedSessionConnected) {
            bbsServerStatus = "BBS list requires a connected session"
            return
        }
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            bbsServerStatus = "BBS list armed"
            return
        }
        if (ft2Link.requestBbsFileListRadio(root.selectedSessionId, nowMs())) {
            bbsServerStatus = "BBS list request sent"
            refreshSessions()
        } else {
            bbsServerStatus = "BBS list request failed: " + String(ft2Link.lastError || "")
        }
    }

    function requestBbsFileByName(name) {
        if (!ft2Link || typeof ft2Link.requestBbsFileRadio !== "function")
            return
        var fileName = String(name || bbsRequestFileText.text || "").trim()
        if (fileName.length === 0) {
            bbsServerStatus = "Enter a BBS file name"
            return
        }
        if (!root.selectedSessionConnected) {
            bbsServerStatus = "BBS download requires a connected session"
            return
        }
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            bbsServerStatus = "BBS download armed"
            return
        }
        if (ft2Link.requestBbsFileRadio(root.selectedSessionId, fileName, nowMs())) {
            bbsServerStatus = "BBS file request sent " + fileName
            refreshSessions()
        } else {
            bbsServerStatus = "BBS file request failed: " + String(ft2Link.lastError || "")
        }
    }

    function removeBbsSharedFile(item) {
        if (!ft2Link || !item || typeof ft2Link.removeBbsSharedFile !== "function")
            return
        if (ft2Link.removeBbsSharedFile(Number(item.id || 0))) {
            bbsServerStatus = "Removed " + String(item.fileName || "file")
            refreshBbsFileServer()
        } else {
            bbsServerStatus = "Remove failed: " + String(ft2Link.lastError || "")
        }
    }

    function sendDigipeaterText() {
        if (!ft2Link)
            return
        var target = digipeaterTargetText.text.trim().toUpperCase()
        var body = digipeaterMessageText.text.trim()
        if (target.length === 0)
            target = "ALL"
        if (body.length === 0)
            return
        requestSlotSniffedTx("DIGI",
                             "DIGI " + target,
                             false,
                             ({ target: target, body: body,
                                hops: digipeaterMaxHopsSetting }))
    }

    function sendDigipeaterTextNow(target, body, hops) {
        if (!ft2Link || typeof ft2Link.transmitDigipeaterRadio !== "function")
            return
        if (ft2Link.transmitDigipeaterRadio(String(target || "ALL"),
                                           String(body || ""),
                                           Number(hops || 0),
                                           nowMs())) {
            digipeaterMessageText.text = ""
            refreshBroadcasts()
            refreshDigipeater()
        }
    }

    function clearDigipeaterLog() {
        if (!ft2Link || typeof ft2Link.clearDigipeaterEvents !== "function")
            return
        ft2Link.clearDigipeaterEvents()
        refreshDigipeater()
    }

    function sendMailboxText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("MAIL"))
            return
        var body = mailBodyText.text.trim()
        if (body.length === 0)
            return
        var toCall = mailToText.text.trim().toUpperCase()
        if (toCall.length === 0)
            toCall = selectedRemoteCall
        var ok = typeof ft2Link.transmitMailboxRadioTyped === "function"
                 ? ft2Link.transmitMailboxRadioTyped(selectedSessionId,
                                                     toCall,
                                                     mailSubjectText.text.trim(),
                                                     body,
                                                     mailUrgentCheck.checked,
                                                     mailEmcommCheck.checked,
                                                     nowMs())
                 : ft2Link.transmitMailboxRadio(selectedSessionId,
                                                toCall,
                                                mailSubjectText.text.trim(),
                                                body,
                                                nowMs())
        if (ok) {
            mailBodyText.text = ""
            refreshMailbox()
            refreshSessions()
        }
    }

    function parkMailboxText() {
        if (!ft2Link)
            return
        var body = mailBodyText.text.trim()
        var toCall = mailToText.text.trim().toUpperCase()
        if (body.length === 0 || toCall.length === 0)
            return
        var ok = typeof ft2Link.parkMailboxTyped === "function"
                 ? ft2Link.parkMailboxTyped(toCall,
                                            mailSubjectText.text.trim(),
                                            body,
                                            mailUrgentCheck.checked,
                                            mailEmcommCheck.checked,
                                            nowMs())
                 : ft2Link.parkMailbox(toCall,
                                       mailSubjectText.text.trim(),
                                       body,
                                       nowMs())
        if (ok) {
            mailBodyText.text = ""
            refreshMailbox()
            refreshAlerts()
        }
    }

    function copyMailboxText() {
        if (!ft2Link || typeof ft2Link.mailboxText !== "function")
            return
        copyPlainText(ft2Link.mailboxText())
    }

    function copyRelayQueueText() {
        if (!ft2Link || typeof ft2Link.relayQueueText !== "function")
            return
        copyPlainText(ft2Link.relayQueueText(nowMs()))
    }

    function relayMailboxCandidate() {
        if (!ft2Link || selectedSessionId === 0)
            return null
        var item = ft2Link.relayMailboxForSession(selectedSessionId)
        if (!item || Number(item.id || 0) === 0)
            return null
        return item
    }

    function relayGuideLine() {
        var item = relayMailboxCandidate()
        if (item) {
            var toCall = String(item.toCall || "--")
            var subject = String(item.subject || "")
            var hop = Number(item.relayHopCount || 0)
            return "Ready to relay -> " + toCall
                   + " / hop " + String(hop + 1)
                   + (subject.length > 0 ? " / " + subject : "")
        }
        var hint = pathRelayHint()
        if (hint)
            return pathRelayLine()
        if (ft2Link && ft2Link.relayQueueCount > 0)
            return "Relay queue has " + String(ft2Link.relayQueueCount)
                   + " item(s); connect a matching relay station"
        return "No relay action ready"
    }

    function relayGuideColor() {
        var item = relayMailboxCandidate()
        if (item) {
            if (item.emcomm)
                return root.red
            if (item.urgent)
                return root.amber
            return root.green
        }
        return pathRelayHint() ? root.amber : root.textSecondary
    }

    function relayMailboxButtonText() {
        var item = relayMailboxCandidate()
        if (!item)
            return "RELAY"
        return ft2Link && ft2Link.radioTxArmed ? "RELAY TX" : "ARM R"
    }

    function sendRelayMailbox() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("RELAY"))
            return
        var item = relayMailboxCandidate()
        if (!item)
            return
        if (ft2Link.transmitRelayMailboxRadio(selectedSessionId,
                                              Number(item.id),
                                              nowMs())) {
            refreshMailbox()
            refreshSessions()
        }
    }

    function markMailboxItemRead(item, read) {
        if (!ft2Link || !item)
            return
        if (ft2Link.markMailboxRead(Number(item.id || 0), read, nowMs()))
            refreshMailbox()
    }

    function mailboxRelayCallForItem(item) {
        if (!item)
            return ""
        var selected = String(selectedRemoteCall || "").trim().toUpperCase()
        if (selected.length > 0)
            return selected
        var suggested = String(item.suggestedRelayCall || "").trim().toUpperCase()
        if (suggested.length > 0)
            return suggested
        return String(item.toCall || "").trim().toUpperCase()
    }

    function markMailboxItemRelayReady(item) {
        if (!ft2Link || !item || typeof ft2Link.markMailboxRelayReady !== "function")
            return
        if (ft2Link.markMailboxRelayReady(Number(item.id || 0), nowMs()))
            refreshMailbox()
    }

    function markMailboxItemPendingRelay(item) {
        if (!ft2Link || !item || typeof ft2Link.markMailboxPendingRelay !== "function")
            return
        var relay = mailboxRelayCallForItem(item)
        if (relay.length === 0)
            return
        if (ft2Link.markMailboxPendingRelay(Number(item.id || 0), relay, nowMs()))
            refreshMailbox()
    }

    function cancelMailboxItemRelay(item) {
        if (!ft2Link || !item || typeof ft2Link.cancelMailboxRelay !== "function")
            return
        if (ft2Link.cancelMailboxRelay(Number(item.id || 0), nowMs()))
            refreshMailbox()
    }

    function transmitMailboxCenterRelay(item) {
        if (!ft2Link || !item || selectedSessionId === 0)
            return
        if (!guardWideTx("RELAY"))
            return
        if (ft2Link.transmitRelayMailboxRadio(selectedSessionId,
                                              Number(item.id || 0),
                                              nowMs())) {
            refreshMailbox()
            refreshSessions()
        }
    }

    function markAllMailboxRead() {
        if (!ft2Link)
            return
        var changed = 0
        for (var i = 0; i < mailbox.length; ++i) {
            var item = mailbox[i]
            if (item && item.unread
                    && ft2Link.markMailboxRead(Number(item.id || 0),
                                               true,
                                               nowMs()))
                ++changed
        }
        refreshMailbox()
        emailGatewayStatus = changed > 0
                             ? ("Marked read " + changed + " mail item"
                                + (changed === 1 ? "" : "s"))
                             : "No unread mail"
    }

    function deleteMailboxItem(item) {
        if (!ft2Link || !item)
            return
        if (ft2Link.deleteMailboxMessage(Number(item.id || 0)))
            refreshMailbox()
    }

    function clearMailboxList() {
        if (!ft2Link || typeof ft2Link.clearMailbox !== "function")
            return
        ft2Link.clearMailbox()
        refreshMailbox()
        emailGatewayStatus = "Mailbox cleared"
    }

    function mailboxEmailDraft(item) {
        if (!ft2Link || !item || typeof ft2Link.mailboxEmailGateway !== "function")
            return null
        return ft2Link.mailboxEmailGateway(Number(item.id || 0), "")
    }

    function openMailboxEmail(item) {
        var draft = mailboxEmailDraft(item)
        if (!draft || !draft.ok)
            return
        logExportText = draft.eml || prettyJson(draft)
        if (draft.mailtoReady && bridge && typeof bridge.openExternalUrl === "function") {
            if (bridge.openExternalUrl(String(draft.mailtoUrl || "")))
                return
        }
        copyPlainText(String(draft.eml || ""))
    }

    function saveMailboxEml(item) {
        var draft = mailboxEmailDraft(item)
        if (!draft || !draft.ok)
            return
        var fileName = String(draft.emlFileName || "FT2-Link_VMail.eml")
        var path = ""
        if (bridge && typeof bridge.saveFileDialog === "function")
            path = bridge.saveFileDialog("Save FT2-Link VMail email", fileName,
                                         ["Email message (*.eml)", "All files (*)"])
        if (path.length === 0) {
            copyPlainText(String(draft.eml || ""))
            logExportText = String(draft.eml || "")
            return
        }
        if (bridge && typeof bridge.writeTextFile === "function") {
            var result = bridge.writeTextFile(path, String(draft.eml || ""))
            logExportText = prettyJson(result)
        } else {
            copyPlainText(String(draft.eml || ""))
            logExportText = String(draft.eml || "")
        }
    }

    function emailGatewaySecurity() {
        var index = Math.max(0, Math.min(emailGatewaySecurityIndex,
                                         emailGatewaySecurityOptions.length - 1))
        return String(emailGatewaySecurityOptions[index] || "STARTTLS")
    }

    function emailGatewayConfig(draft) {
        return {
            host: emailGatewayHost.trim(),
            port: emailGatewayPort,
            security: emailGatewaySecurity(),
            username: emailGatewayUsername.trim(),
            fromEmail: emailGatewayFrom.trim().length > 0
                       ? emailGatewayFrom.trim()
                       : profileEmail.trim(),
            toEmail: draft ? String(draft.toEmail || "") : "",
            auth: emailGatewayUsername.trim().length > 0
        }
    }

    function cycleEmailGatewaySecurity() {
        emailGatewaySecurityIndex = (emailGatewaySecurityIndex + 1)
                                    % emailGatewaySecurityOptions.length
    }

    function setEmailGatewayItemState(mailboxId, state, detail) {
        var key = String(Number(mailboxId || 0))
        var next = {}
        for (var existing in emailGatewayRequestStates)
            next[existing] = emailGatewayRequestStates[existing]
        next[key] = {
            state: String(state || ""),
            detail: String(detail || "")
        }
        emailGatewayRequestStates = next
    }

    function emailGatewayItemState(item) {
        if (!item)
            return ""
        var entry = emailGatewayRequestStates[String(Number(item.id || 0))]
        if (!entry)
            return String(item.emailGatewayState || "")
        return String(entry.state || "")
    }

    function saveEmailGatewayPassword() {
        if (!bridge || typeof bridge.setFt2LinkEmailGatewayPassword !== "function"
                || typeof emailGatewayPasswordText === "undefined")
            return
        var password = emailGatewayPasswordText.text
        if (password.length === 0) {
            emailGatewayStatus = "SMTP password empty"
            return
        }
        var result = bridge.setFt2LinkEmailGatewayPassword(emailGatewayConfig(null), password)
        emailGatewayPasswordText.text = ""
        emailGatewayStatus = result && result.ok ? "SMTP password saved securely"
                                                 : "SMTP password error: " + String(result ? result.error || "unknown" : "unknown")
        logExportText = prettyJson(result)
    }

    function clearEmailGatewayPassword() {
        if (!bridge || typeof bridge.clearFt2LinkEmailGatewayPassword !== "function")
            return
        var result = bridge.clearFt2LinkEmailGatewayPassword(emailGatewayConfig(null))
        emailGatewayStatus = result && result.ok ? "SMTP password cleared"
                                                 : "SMTP password error: " + String(result ? result.error || "unknown" : "unknown")
        logExportText = prettyJson(result)
    }

    function testEmailGateway() {
        if (!bridge || typeof bridge.testFt2LinkEmailGateway !== "function") {
            emailGatewayStatus = "SMTP test unavailable"
            return
        }
        var result = bridge.testFt2LinkEmailGateway(emailGatewayConfig(null))
        logExportText = prettyJson(result)
        if (result && result.ok)
            emailGatewayStatus = String(result.detail || "SMTP test queued")
        else
            emailGatewayStatus = "SMTP test error: " + String(result ? result.error || "unknown" : "unknown")
    }

    function sendMailboxGatewayEmail(item) {
        var draft = mailboxEmailDraft(item)
        if (!draft || !draft.ok) {
            emailGatewayStatus = "SMTP draft unavailable"
            return
        }
        if (!emailGatewayEnabled) {
            emailGatewayStatus = "SMTP gateway disabled"
            return
        }
        if (!draft.mailtoReady) {
            emailGatewayStatus = "SMTP recipient email missing"
            logExportText = draft.eml || prettyJson(draft)
            return
        }
        if (!bridge || typeof bridge.sendFt2LinkGatewayEmail !== "function") {
            emailGatewayStatus = "SMTP bridge unavailable"
            return
        }
        var config = emailGatewayConfig(draft)
        var result = bridge.sendFt2LinkGatewayEmail(Number(item.id || 0),
                                                    config,
                                                    String(draft.eml || ""))
        logExportText = prettyJson(result)
        if (result && result.ok) {
            emailGatewayStatus = String(result.detail || "SMTP queued")
            setEmailGatewayItemState(item.id, "Queued", emailGatewayStatus)
        } else {
            emailGatewayStatus = "SMTP error: " + String(result ? result.error || "unknown" : "unknown")
            setEmailGatewayItemState(item.id, "Failed", emailGatewayStatus)
        }
    }

    function loadContactDetails(item) {
        if (!item)
            return
        selectedContactCall = String(item.call || "").trim().toUpperCase()
        contactCallText.text = selectedContactCall
        contactGridText.text = String(item.locator || "")
        contactNameText.text = String(item.name || "")
        contactTagText.text = String(item.tag || "")
        contactCommentText.text = String(item.comment || "")
        refreshContactTimeline()
    }

    function saveContactDetails() {
        if (!ft2Link)
            return
        var call = contactCallText.text.trim().toUpperCase()
        if (call.length === 0)
            return
        if (ft2Link.setContactDetails(call,
                                      contactGridText.text.trim(),
                                      contactNameText.text.trim(),
                                      contactTagText.text.trim(),
                                      contactCommentText.text.trim(),
                                      nowMs())) {
            selectedContactCall = call
            refreshContactHistory()
            refreshContactTimeline()
            refreshStations()
        }
    }

    function clearContactDetailsEditor() {
        selectedContactCall = ""
        contactCallText.text = ""
        contactGridText.text = ""
        contactNameText.text = ""
        contactTagText.text = ""
        contactCommentText.text = ""
        selectedContactTimeline = []
    }

    function sendFormText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("FORM"))
            return
        var fields = parseFormFields(formFieldsText.text)
        if (Object.keys(fields).length === 0)
            return
        var toCall = selectedRemoteCall.trim().toUpperCase()
        if (ft2Link.transmitFormRadio(selectedSessionId,
                                      toCall,
                                      currentFormType(),
                                      fields,
                                      nowMs())) {
            refreshForms()
            refreshSessions()
        }
    }

    function sendFileText() {
        if (!ft2Link || selectedSessionId === 0)
            return false
        if (!guardWideTx("FILE"))
            return false
        var content = String(selectedFileContent || "")
        var contentBase64 = String(selectedFileBase64 || "")
        if (selectedFileName.length === 0 || selectedFileBytes <= 0
                || (selectedFileBinary && contentBase64.length === 0)
                || (!selectedFileBinary && content.length === 0)) {
            fileTransferStatus = "Select a file first"
            return false
        }
        var bytes = selectedFileBytes > 0 ? selectedFileBytes : utf8ByteCount(content)
        if (bytes > filePayloadLimitBytes) {
            fileTransferStatus = "File too large: " + bytes + " B / max "
                                 + filePayloadLimitBytes + " B"
            return false
        }
        var toCall = String(selectedRemoteCall || "").trim().toUpperCase()
        if (toCall.length === 0) {
            fileTransferStatus = "No active QSO target"
            return false
        }
        var fileName = selectedFileName.length > 0 ? selectedFileName : baseFileName(selectedFilePath)
        if (fileName.length === 0)
            fileName = "file.txt"
        var accepted = requestSlotSniffedTx(
                    "FILE", "FILE " + fileName, false,
                    ({ sessionId: selectedSessionId,
                       toCall: toCall,
                       fileName: fileName,
                       content: content,
                       contentBase64: contentBase64,
                       binary: selectedFileBinary,
                       bytes: bytes }))
        if (accepted)
            fileTransferStatus = "FILE queued " + fileName + " " + bytes
                                 + " B; waiting for TX"
        return accepted
    }

    function sendFileTextNow(payload) {
        if (!ft2Link || !payload)
            return false
        var sessionId = Number(payload.sessionId || 0)
        var toCall = String(payload.toCall || "").trim().toUpperCase()
        var fileName = String(payload.fileName || "file.txt")
        var bytes = Number(payload.bytes || 0)
        if (sessionId <= 0 || toCall.length === 0 || fileName.length === 0) {
            fileTransferStatus = "FILE TX failed: invalid queued request"
            return false
        }
        if (!ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        var queued = !!payload.binary
                     ? (typeof ft2Link.transmitFileRadioBytes === "function"
                        && ft2Link.transmitFileRadioBytes(sessionId,
                                                          toCall,
                                                          fileName,
                                                          String(payload.contentBase64 || ""),
                                                          nowMs()))
                     : ft2Link.transmitFileRadio(sessionId,
                                                 toCall,
                                                 fileName,
                                                 String(payload.content || ""),
                                                 nowMs())
        if (queued) {
            fileTransferStatus = "FILE accepted for TX " + fileName + " "
                                 + bytes + " B"
            refreshFileTransfers()
            refreshSessions()
            return true
        }
        fileTransferStatus = "FILE TX failed: "
                             + String(ft2Link.lastError || "unknown")
        return false
    }

    function utf8ByteCount(text) {
        try {
            return unescape(encodeURIComponent(String(text || ""))).length
        } catch (e) {
            return String(text || "").length
        }
    }

    function baseFileName(path) {
        var clean = String(path || "").replace(/\\/g, "/")
        var index = clean.lastIndexOf("/")
        return index >= 0 ? clean.substring(index + 1) : clean
    }

    function loadFileText() {
        if (!bridge || typeof bridge.openFileDialog !== "function"
                || typeof bridge.readTextFile !== "function") {
            fileTransferStatus = "File picker unavailable"
            return
        }
        fileTransferStatus = "Opening file selector..."
        var path = bridge.openFileDialog("Load FT2-Link text file",
                                         "",
                                         ["Text files (*.txt *.log *.csv *.json *.md)",
                                          "All files (*)"])
        if (!path || path.length === 0) {
            fileTransferStatus = "File selection cancelled"
            return
        }
        var result = bridge.readTextFile(path, filePayloadLimitBytes)
        if (!result || !result.ok) {
            fileTransferStatus = String(result && result.error
                                        ? result.error
                                        : "Cannot load file")
            return
        }
        var bytes = Number(result.bytes || 0)
        var fullSize = Number(result.fileSize || bytes)
        if (result.truncated || fullSize > filePayloadLimitBytes) {
            clearSelectedFile()
            fileTransferStatus = "File too large: " + fullSize + " B / max "
                                 + filePayloadLimitBytes + " B"
            return
        }
        selectedFilePath = String(result.path || path)
        selectedFileName = baseFileName(selectedFilePath)
        selectedFileContent = String(result.text || "")
        selectedFileBase64 = ""
        selectedFileBinary = false
        selectedFileBytes = bytes
        fileTransferStatus = "Ready " + selectedFileName + " " + bytes + " B"
    }

    function loadFileBytes() {
        if (!bridge || typeof bridge.openFileDialog !== "function"
                || typeof bridge.readFileBytes !== "function") {
            fileTransferStatus = "Binary file picker unavailable"
            return
        }
        fileTransferStatus = "Opening file selector..."
        var path = bridge.openFileDialog("Load FT2-Link binary file",
                                         "",
                                         ["All files (*)",
                                          "Images (*.png *.jpg *.jpeg *.gif *.bmp)",
                                          "Documents (*.txt *.md *.pdf *.json)"])
        if (!path || path.length === 0) {
            fileTransferStatus = "File selection cancelled"
            return
        }
        var result = bridge.readFileBytes(path, filePayloadLimitBytes)
        if (!result || !result.ok) {
            fileTransferStatus = String(result && result.error
                                        ? result.error
                                        : "Cannot load file")
            return
        }
        var bytes = Number(result.bytes || 0)
        var fullSize = Number(result.fileSize || bytes)
        if (result.truncated || fullSize > filePayloadLimitBytes) {
            clearSelectedFile()
            fileTransferStatus = "File too large: " + fullSize + " B / max "
                                 + filePayloadLimitBytes + " B"
            return
        }
        selectedFilePath = String(result.path || path)
        selectedFileName = baseFileName(selectedFilePath)
        selectedFileContent = ""
        selectedFileBase64 = String(result.base64 || "")
        selectedFileBinary = true
        selectedFileBytes = bytes
        fileTransferStatus = "Ready binary " + selectedFileName + " " + bytes + " B"
    }

    function clearSelectedFile() {
        selectedFilePath = ""
        selectedFileName = ""
        selectedFileContent = ""
        selectedFileBase64 = ""
        selectedFileBinary = false
        selectedFileBytes = 0
        fileTransferStatus = ""
    }

    function sendBulletinText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("BBS"))
            return
        var groupText = bulletinGroupText.text.trim().length > 0
                        ? bulletinGroupText.text
                        : bbsDefaultGroup
        var group = ensureBbsGroup(groupText)
        bulletinGroupText.text = group
        var body = bulletinBodyText.text.trim()
        if (body.length === 0)
            return
        if (ft2Link.transmitBulletinRadio(selectedSessionId,
                                          group,
                                          bulletinTitleText.text.trim(),
                                          body,
                                          nowMs())) {
            bulletinBodyText.text = ""
            refreshBulletins()
            refreshSessions()
        }
    }

    function insertQslCard() {
        if (!ft2Link || selectedSessionId === 0)
            return
        var text = ft2Link.qslCard(selectedSessionId)
        if (String(text || "").length > 0)
            composeText.text = text
    }

    function copyAdifRecord() {
        if (!ft2Link || selectedSessionId === 0)
            return
        var text = ft2Link.adifRecord(selectedSessionId)
        if (String(text || "").length === 0)
            return
        if (bridge && typeof bridge.copyToClipboard === "function")
            bridge.copyToClipboard(text)
        else
            composeText.text = text
    }

    function sendPing(call) {
        if (!ft2Link)
            return
        var target = String(call || selectedRemoteCall || "").trim().toUpperCase()
        if (target.length === 0)
            return
        if (ft2Link.transmitPingRadio(target, nowMs()))
            refreshPingLog()
    }

    function insertCannedMessage(templateText) {
        if (!ft2Link)
            return
        var text = ft2Link.expandCannedMessage(String(templateText || ""),
                                               selectedSessionId,
                                               nowMs())
        if (text.length === 0)
            return
        composeText.text = text
        composeText.forceActiveFocus()
        composeText.cursorPosition = composeText.text.length
    }

    function loadPreset(item) {
        if (!item)
            return
        presetLabelText.text = String(item.label || "")
        presetTemplateText.text = String(item.templateText || "")
        presetTipText.text = String(item.tip || "")
    }

    function savePreset() {
        if (!ft2Link || typeof ft2Link.addOrUpdateCannedMessage !== "function")
            return
        var result = ft2Link.addOrUpdateCannedMessage(presetLabelText.text,
                                                      presetTemplateText.text,
                                                      presetTipText.text)
        databaseActionText = prettyJson(result)
        refreshCannedMessages()
        refreshStoreAudit()
    }

    function deletePreset() {
        if (!ft2Link || typeof ft2Link.deleteCannedMessage !== "function")
            return
        var result = ft2Link.deleteCannedMessage(presetLabelText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok) {
            presetLabelText.text = ""
            presetTemplateText.text = ""
            presetTipText.text = ""
        }
        refreshCannedMessages()
        refreshStoreAudit()
    }

    function resetPresets() {
        if (!ft2Link || typeof ft2Link.resetCannedMessages !== "function")
            return
        var result = ft2Link.resetCannedMessages()
        databaseActionText = prettyJson(result)
        presetLabelText.text = ""
        presetTemplateText.text = ""
        presetTipText.text = ""
        refreshCannedMessages()
        refreshStoreAudit()
    }

    function checkInBody() {
        if (!ft2Link || typeof ft2Link.checkInMessage !== "function")
            return ""
        return ft2Link.checkInMessage(checkInCityText.text,
                                      checkInRegionText.text,
                                      checkInChannelText.text,
                                      checkInWeatherText.text,
                                      nowMs())
    }

    function prepareCheckInMail() {
        var body = checkInBody()
        if (body.length === 0)
            return
        checkInCity = checkInCityText.text.trim()
        checkInRegion = checkInRegionText.text.trim()
        checkInChannel = checkInChannelText.text.trim()
        mailToText.text = "varacwednesday@gmail.com"
        mailSubjectText.text = "VarAC Wednesday Check-In"
        mailBodyText.text = body
        toolPageIndex = 5
    }

    function prepareCheckInChat() {
        var body = checkInBody()
        if (body.length === 0)
            return
        checkInCity = checkInCityText.text.trim()
        checkInRegion = checkInRegionText.text.trim()
        checkInChannel = checkInChannelText.text.trim()
        composeText.text = body
        composeText.forceActiveFocus()
        composeText.cursorPosition = composeText.text.length
        toolPageIndex = 0
    }

    function currentQsySlot() {
        if (qsySlots.length === 0)
            return null
        return qsySlots[Math.max(0, Math.min(qsySlotIndex, qsySlots.length - 1))]
    }

    function currentCqSlot() {
        if (qsySlots.length === 0)
            return null
        return qsySlots[Math.max(0, Math.min(cqSlotIndex, qsySlots.length - 1))]
    }

    function slotIdLabel(slotId) {
        var id = Number(slotId || 0)
        if (id > 0)
            return "S+" + id
        if (id < 0)
            return "S" + id
        return "S0"
    }

    function currentQsySlotLabel() {
        var slot = currentQsySlot()
        return slot ? String(slot.label || "QSY") : "QSY"
    }

    function currentCqSlotLabel() {
        var slot = currentCqSlot()
        return slot ? slotIdLabel(slot.slotId) : "SLOT"
    }

    function currentCqSlotOffsetLabel() {
        var slot = currentCqSlot()
        if (!slot)
            return "--"
        var offset = Number(slot.offsetHz || 0)
        return (offset > 0 ? "+" : "") + offset + " Hz"
    }

    function currentQsySlotTip() {
        var slot = currentQsySlot()
        return slot ? String(slot.tip || "Insert QSY invitation") : "Insert QSY invitation"
    }

    function currentCqSlotTip() {
        var slot = currentCqSlot()
        if (!slot)
            return "Select CQ slot"
        return "CQ will advertise " + slotIdLabel(slot.slotId) + " (" + currentCqSlotOffsetLabel() + ")"
    }

    function cycleQsySlot() {
        if (qsySlots.length === 0)
            return
        qsySlotIndex = (qsySlotIndex + 1) % qsySlots.length
    }

    function cycleCqSlot() {
        if (qsySlots.length === 0)
            return
        cqSlotIndex = (cqSlotIndex + 1) % qsySlots.length
    }

    function cycleCqSlotWait() {
        if (cqSlotWaitSeconds < 120)
            cqSlotWaitSeconds = 120
        else if (cqSlotWaitSeconds < 300)
            cqSlotWaitSeconds = 300
        else if (cqSlotWaitSeconds < 600)
            cqSlotWaitSeconds = 600
        else if (cqSlotWaitSeconds < 900)
            cqSlotWaitSeconds = 900
        else if (cqSlotWaitSeconds < 1800)
            cqSlotWaitSeconds = 1800
        else if (cqSlotWaitSeconds < 3600)
            cqSlotWaitSeconds = 3600
        else
            cqSlotWaitSeconds = 60
    }

    function cycleSlotSnifferSeconds() {
        if (slotSnifferSeconds < 5)
            slotSnifferSeconds = 5
        else if (slotSnifferSeconds < 8)
            slotSnifferSeconds = 8
        else if (slotSnifferSeconds < 12)
            slotSnifferSeconds = 12
        else if (slotSnifferSeconds < 20)
            slotSnifferSeconds = 20
        else
            slotSnifferSeconds = 3
    }

    function slotSnifferText() {
        return slotSnifferSeconds + "s"
    }

    function cqSlotWaitText() {
        if (cqSlotWaitSeconds < 60)
            return cqSlotWaitSeconds + "s"
        if (cqSlotWaitSeconds % 60 === 0)
            return Math.round(cqSlotWaitSeconds / 60) + "m"
        return cqSlotWaitSeconds + "s"
    }

    function cqSlotWaitRemainingSeconds() {
        if (cqSlotWaitUntilMs <= nowMs())
            return 0
        return Math.ceil((cqSlotWaitUntilMs - nowMs()) / 1000)
    }

    function cqSlotBusy() {
        var slot = currentCqSlot()
        if (!slot)
            return false
        var slotId = Number(slot.slotId || 0)
        for (var i = 0; i < stations.length; ++i) {
            if (Number(stations[i].cqSlotId || 0) === slotId)
                return true
        }
        return false
    }

    function cqSlotStatusText() {
        if (slotSnifferActive() && slotSnifferAction === "CQ")
            return "SNIFF"
        var remaining = cqSlotWaitRemainingSeconds()
        if (remaining > 0)
            return "WAIT " + remaining + "s"
        return cqSlotBusy() ? "BUSY" : "FREE"
    }

    function slotSnifferActive() {
        return slotSnifferAction.length > 0
    }

    function slotSnifferRemainingSeconds() {
        if (!slotSnifferActive())
            return 0
        return Math.max(0, Math.ceil((slotSnifferUntilMs - nowMs()) / 1000))
    }

    function slotSnifferBusyReason(requireSlotClear, action, payload) {
        if (ft2Link && ft2Link.transportBusy)
            return "TX busy"
        if (String(action || "") === "FILE" && ft2Link) {
            var fileSessionId = Number(payload && payload.sessionId || 0)
            var fileSessionConnected = false
            for (var i = 0; i < sessions.length; ++i) {
                if (Number(sessions[i].sessionId || 0) === fileSessionId
                        && String(sessions[i].stateName || "") === "Connected") {
                    fileSessionConnected = true
                    break
                }
            }
            if (!fileSessionConnected)
                return "session unavailable"
            if (typeof ft2Link.applicationRadioTxReady === "function"
                    && !ft2Link.applicationRadioTxReady(fileSessionId))
                return "session busy"
        }
        if (ft2Link && ft2Link.liveChannelLbtBusy)
            return "channel busy"
        if (requireSlotClear && cqSlotBusy())
            return "slot busy"
        return ""
    }

    function slotSnifferLine() {
        if (!slotSnifferActive())
            return slotSnifferStatus
        var reason = slotSnifferBusyReason(slotSnifferRequireSlotClear,
                                           slotSnifferAction,
                                           slotSnifferPayload)
        if (reason.length > 0)
            return "SNIFF " + slotSnifferLabel + " " + reason
        return "SNIFF " + slotSnifferLabel + " " + slotSnifferRemainingSeconds() + "s"
    }

    function clearSlotSniffer(status) {
        slotSnifferAction = ""
        slotSnifferLabel = ""
        slotSnifferRequireSlotClear = false
        slotSnifferUntilMs = 0
        slotSnifferDeadlineMs = 0
        slotSnifferPayload = ({})
        slotSnifferStatus = String(status || "")
    }

    function requestSlotSniffedTx(action, label, requireSlotClear, payload) {
        if (!ft2Link)
            return false
        if (slotSnifferActive()) {
            databaseActionText = slotSnifferLine()
            return false
        }
        slotSnifferAction = String(action || "")
        slotSnifferLabel = String(label || slotSnifferAction)
        slotSnifferRequireSlotClear = !!requireSlotClear
        slotSnifferPayload = payload || ({})
        var now = nowMs()
        slotSnifferUntilMs = now + slotSnifferSeconds * 1000
        slotSnifferDeadlineMs = now + Math.max(slotSnifferSeconds * 3, 24) * 1000
        slotSnifferStatus = "SNIFF " + slotSnifferLabel
        continueSlotSniffer()
        return true
    }

    function continueSlotSniffer() {
        if (!slotSnifferActive())
            return
        var now = nowMs()
        var reason = slotSnifferBusyReason(slotSnifferRequireSlotClear,
                                           slotSnifferAction,
                                           slotSnifferPayload)
        if (reason.length > 0) {
            var persistentWait = slotSnifferAction === "CONNECT"
                                 || slotSnifferAction === "BCAST"
                                 || (slotSnifferAction === "FILE"
                                     && reason !== "session unavailable")
            if (now >= slotSnifferDeadlineMs && !persistentWait) {
                clearSlotSniffer("SNIFF abort: " + reason)
                databaseActionText = slotSnifferStatus
                return
            }
            slotSnifferUntilMs = now + slotSnifferSeconds * 1000
            slotSnifferStatus = "SNIFF " + slotSnifferLabel + " " + reason
            return
        }
        if (now < slotSnifferUntilMs) {
            slotSnifferStatus = "SNIFF " + slotSnifferLabel
            return
        }
        var action = slotSnifferAction
        var payload = slotSnifferPayload
        clearSlotSniffer("SNIFF clear: " + String(action))
        dispatchSlotSnifferAction(action, payload)
    }

    function dispatchSlotSnifferAction(action, payload) {
        var key = String(action || "")
        if (key === "CQ" || key === "BEACON") {
            armStrictNextTx()
            transmitBeaconNow(!!(payload && payload.cq))
            return
        }
        if (key === "CONNECT") {
            if (payload && payload.call) {
                startRadioSession(String(payload.call))
            }
            return
        }
        if (key === "BCAST") {
            sendBroadcastTextNow(String(payload && payload.text || ""))
            return
        }
        if (key === "FILE") {
            if (!sendFileTextNow(payload)
                    && String(ft2Link && ft2Link.lastError || "")
                       .indexOf("already pending") >= 0)
                requestSlotSniffedTx("FILE",
                                     "FILE " + String(payload && payload.fileName || ""),
                                     false,
                                     payload)
            return
        }
        if (key === "DIGI") {
            if (payload && payload.body) {
                armStrictNextTx()
                sendDigipeaterTextNow(String(payload.target || "ALL"),
                                      String(payload.body || ""),
                                      Number(payload.hops || 0))
            }
            return
        }
        if (key === "QSY_INVITE") {
            if (payload && payload.text) {
                armStrictNextTx()
                sendQsyInviteNow(String(payload.text))
            }
            return
        }
    }

    function armStrictNextTx() {
        if (!ft2Link || typeof ft2Link.armStrictListenBeforeTransmit !== "function")
            return
        ft2Link.armStrictListenBeforeTransmit(Math.max(24000, slotSnifferSeconds * 3000))
    }

    function insertQsySlotTag() {
        var slot = currentQsySlot()
        if (!slot)
            return
        root.insertCannedMessage(currentQsySlotTag(slot))
    }

    function currentQsySlotTag(slot) {
        if (!slot)
            return ""
        var dial = currentDialFrequencyHz()
        var offset = Number(slot.offsetHz || 0)
        if (ft2Link && typeof ft2Link.qsyFrequencyTag === "function"
                && dial > 0 && offset !== 0) {
            var absoluteTag = ft2Link.qsyFrequencyTag(Math.round(dial + offset))
            if (String(absoluteTag || "").length > 0)
                return String(absoluteTag)
        }
        return String(slot.tag || "")
    }

    function sendQsySlotInvite() {
        var slot = currentQsySlot()
        if (!slot)
            return
        var tag = currentQsySlotTag(slot)
        if (tag.length === 0)
            return
        if (!selectedSessionConnected) {
            insertCannedMessage(tag)
            databaseActionText = "QSY tag inserted: select a connected session to send"
            return
        }
        requestSlotSniffedTx("QSY_INVITE",
                             "QSY " + currentQsySlotLabel(),
                             false,
                             ({ text: tag }))
    }

    function sendQsyInviteNow(text) {
        composeText.text = String(text || "")
        sendChatText()
    }

    function currentDialFrequencyHz() {
        var hz = 0
        if (typeof bridge !== "undefined" && bridge) {
            if (bridge.dialFrequency !== undefined)
                hz = Number(bridge.dialFrequency)
            if ((!hz || hz <= 0) && bridge.frequency !== undefined)
                hz = Number(bridge.frequency)
        }
        if (!isFinite(hz) || hz <= 0)
            return 0
        return Math.round(hz)
    }

    function frequencyHzText(hz) {
        var value = Math.round(Number(hz || 0))
        if (!isFinite(value) || value <= 0)
            return "--"
        return (value / 1000000).toFixed(6) + " MHz"
    }

    function refreshQsyPlan() {
        qsyPlan = ({})
        if (!ft2Link || selectedMessages.length === 0
                || typeof ft2Link.qsyPlanForText !== "function")
            return
        var dial = currentDialFrequencyHz()
        for (var i = selectedMessages.length - 1; i >= 0; --i) {
            var message = selectedMessages[i]
            if (String(message.directionName || "") === "Outgoing")
                continue
            var plan = ft2Link.qsyPlanForText(String(message.text || ""), dial)
            if (plan && plan.valid) {
                qsyPlan = plan
                return
            }
        }
    }

    function qsyPlanCanApply(plan) {
        return !!plan && plan.valid === true
               && plan.hasTargetFrequency === true
               && Number(plan.dialFrequencyHz || 0) > 0
               && plan.allowed === true
    }

    function qsyPlanApplyKey(plan, reason) {
        return selectedSessionId + "|"
               + Math.round(Number(plan && plan.dialFrequencyHz || 0))
               + "|" + String(reason || "")
    }

    function applyQsyPlan(plan, reason) {
        if (!qsyPlanCanApply(plan)) {
            databaseActionText = "QSY not applied: "
                                 + String(plan && plan.rangeStatus
                                          ? plan.rangeStatus
                                          : "invalid target")
            return false
        }
        if (!bridge || typeof bridge.qsyTo !== "function") {
            databaseActionText = "QSY target ready "
                                 + frequencyHzText(Number(plan.dialFrequencyHz || 0))
                                 + " but bridge.qsyTo is unavailable"
            return false
        }
        var key = qsyPlanApplyKey(plan, reason)
        if (appliedQsyKeys[key])
            return true
        appliedQsyKeys[key] = true
        var hz = Math.round(Number(plan.dialFrequencyHz || 0))
        bridge.qsyTo(hz, "FT2-Link")
        databaseActionText = "QSY applied "
                             + frequencyHzText(hz)
                             + " (" + String(reason || "session") + ")"
        return true
    }

    function scheduleQsyPlanApply(plan, reason) {
        if (!qsyPlanCanApply(plan))
            return false
        delayedQsyPlan = plan
        delayedQsyReason = String(reason || "accepted")
        var seconds = 0
        if (ft2Link && ft2Link.lastRadioTxPlan)
            seconds = Number(ft2Link.lastRadioTxPlan.audioSeconds || 0)
        qsyApplyTimer.interval = Math.max(1200, Math.round(seconds * 1000) + 900)
        qsyApplyTimer.restart()
        databaseActionText = "QSY queued "
                             + frequencyHzText(Number(plan.dialFrequencyHz || 0))
                             + " after ACK TX"
        return true
    }

    function rememberOutgoingQsyInvite(text) {
        if (!ft2Link || typeof ft2Link.qsyPlanForText !== "function")
            return
        var plan = ft2Link.qsyPlanForText(String(text || ""),
                                          currentDialFrequencyHz())
        if (qsyPlanCanApply(plan)) {
            pendingOutgoingQsyPlan = plan
            pendingOutgoingQsySessionId = selectedSessionId
            databaseActionText = "QSY invite pending "
                                 + frequencyHzText(Number(plan.dialFrequencyHz || 0))
        } else if (plan && plan.valid) {
            pendingOutgoingQsyPlan = ({})
            pendingOutgoingQsySessionId = 0
            databaseActionText = "QSY invite not stored: "
                                 + String(plan.rangeStatus || "invalid target")
        }
    }

    function processQsyReplies() {
        if (!selectedMessages || selectedMessages.length === 0)
            return
        for (var i = selectedMessages.length - 1; i >= 0; --i) {
            var message = selectedMessages[i]
            if (String(message.directionName || "") !== "Incoming")
                continue
            var text = String(message.text || "").toUpperCase()
            if (text.indexOf("<QSYJ>") >= 0 || text.indexOf("<QJO>") >= 0) {
                if (pendingOutgoingQsySessionId === selectedSessionId) {
                    pendingOutgoingQsyPlan = ({})
                    pendingOutgoingQsySessionId = 0
                    databaseActionText = "QSY rejected"
                }
                return
            }
            if (text.indexOf("<QSYR>") >= 0) {
                if (pendingOutgoingQsySessionId === selectedSessionId
                        && qsyPlanCanApply(pendingOutgoingQsyPlan)) {
                    applyQsyPlan(pendingOutgoingQsyPlan, "accepted")
                    pendingOutgoingQsyPlan = ({})
                    pendingOutgoingQsySessionId = 0
                }
                return
            }
        }
    }

    function qsyPlanValid() {
        return qsyPlan && qsyPlan.valid === true
    }

    function qsyPlanText() {
        if (!qsyPlanValid())
            return "No QSY invite"
        var text = String(qsyPlan.summary || "QSY invite")
        if (qsyPlan.hasTargetFrequency)
            text += "  " + frequencyHzText(qsyPlan.dialFrequencyHz)
        if (qsyPlan.rangeChecked)
            text += "  " + String(qsyPlan.rangeStatus || "")
        return text
    }

    function qsyCallingFrequencyText() {
        return qsyCallingFrequencyHz > 0 ? frequencyHzText(qsyCallingFrequencyHz) : "CF --"
    }

    function setCallingFrequencyFromRig() {
        var dial = currentDialFrequencyHz()
        if (dial > 0)
            qsyCallingFrequencyHz = dial
    }

    function callingFrequencyGuard(action) {
        if (!ft2Link || typeof ft2Link.callingFrequencyGuard !== "function")
            return ({ allowed: true, blocked: false })
        return ft2Link.callingFrequencyGuard(String(action || ""),
                                             currentDialFrequencyHz(),
                                             qsyCallingFrequencyHz)
    }

    function guardWideTx(action) {
        var result = callingFrequencyGuard(action)
        if (result && result.blocked) {
            databaseActionText = String(result.message || "Calling frequency guard")
            return false
        }
        return true
    }

    function insertCallingFrequencyQsyTag() {
        if (!ft2Link || qsyCallingFrequencyHz <= 0)
            return
        var tag = ft2Link.qsyFrequencyTag(qsyCallingFrequencyHz)
        if (String(tag || "").length > 0)
            insertCannedMessage(tag)
    }

    function sendQsyControlTag(tag) {
        if (!tag || !selectedSessionConnected)
            return false
        composeText.text = String(tag)
        return sendChatText()
    }

    function acceptQsyInvite() {
        if (!qsyPlanCanApply(qsyPlan)) {
            sendQsyControlTag(String(qsyPlan.outOfRangeTag || "<QJO>"))
            return
        }
        var acceptedPlan = qsyPlan
        if (sendQsyControlTag(String(acceptedPlan.acceptTag || "<QSYR>")))
            scheduleQsyPlanApply(acceptedPlan, "accepted-local")
    }

    function prepareRadioTx() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("RF TX"))
            return
        var text = composeText.text.trim()
        if (text.length === 0)
            return
        ft2Link.prepareRadioTxAudio(selectedSessionId, text, nowMs())
    }

    function transmitRadioTx() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("RF TX"))
            return
        var text = composeText.text.trim()
        if (text.length === 0)
            return
        if (ft2Link.transmitPreparedRadioTxAudio(selectedSessionId, text, nowMs())) {
            composeText.text = ""
            refreshSessions()
        }
    }

    function metric(key, fallback) {
        if (!ft2Link || !ft2Link.lastTransportMetrics)
            return fallback
        var value = ft2Link.lastTransportMetrics[key]
        return value === undefined || value === null ? fallback : value
    }

    function fixedMetric(key, digits, fallback) {
        var value = Number(metric(key, NaN))
        return isFinite(value) ? value.toFixed(digits) : fallback
    }

    function radioMetric(key, fallback) {
        if (!ft2Link || !ft2Link.lastRadioTxPlan)
            return fallback
        var value = ft2Link.lastRadioTxPlan[key]
        return value === undefined || value === null ? fallback : value
    }

    function fixedRadioMetric(key, digits, fallback) {
        var value = Number(radioMetric(key, NaN))
        return isFinite(value) ? value.toFixed(digits) : fallback
    }

    function transportLine() {
        if (metric("liveRx", false))
            return "RX " + String(metric("profileName", "--"))
                   + "  " + String(metric("w2300RateModeName", "--"))
                   + "  q " + fixedMetric("quality", 2, "--")
                   + "  off " + fixedMetric("estimatedFrequencyOffsetHz", 1, "--") + " Hz"
                   + "  next " + String(metric("nextW2300RateModeName", "--"))
        return "RF " + String(metric("profileName", "--"))
               + "  " + fixedMetric("effectivePayloadBps", 1, "0.0") + " B/s"
               + "  " + metric("burstCount", 0) + " burst"
               + "  retry " + metric("retryBurstCount", 0)
               + "  drop " + (Number(metric("droppedBurstCount", 0)) + Number(metric("droppedAckBurstCount", 0)))
    }

    function radioLine() {
        var lbt = ft2Link && ft2Link.liveChannelLbtBusy ? "  LBT BUSY" : ""
        var guard = callingFrequencyGuard("RF TX")
        var cf = guard && guard.blocked ? "  CF GUARD" : ""
        return "RADIO " + String(radioMetric("profileName", "--"))
               + "  " + String(radioMetric("w2300RateModeName", "--"))
               + "  " + String(radioMetric("w2300RateSource", "--"))
               + "  " + fixedRadioMetric("sampleRate", 0, "0") + " Hz"
               + "  " + fixedRadioMetric("audioSeconds", 2, "0.00") + " s"
               + "  " + radioMetric("burstCount", 0) + " burst"
               + lbt + cf
    }

    Component.onCompleted: {
        syncLocalStation()
        applyCapabilities()
        refreshFormTemplates()
        refreshStations()
        refreshSessions()
        refreshBroadcasts()
        refreshAlerts()
        refreshAlertTags()
        refreshMailbox()
        refreshForms()
        refreshFileTransfers()
        refreshReceivedFiles()
        configureBbsFileServer()
        refreshBbsFileServer()
        refreshBulletins()
        refreshQsoLog()
        refreshLogbookOutbox()
        refreshContactHistory()
        refreshPingLog()
        refreshPathReports()
        configureDigipeater()
        refreshBeaconHistory()
        updateClusterFromRig()
        refreshPathAnalysis()
        refreshStatistics()
        refreshStoreAudit()
        refreshCannedMessages()
        refreshQsySlots()
        refreshFrequencyPlan()
        refreshPresence()
        refreshQsoAutomation()
        refreshBlockedCalls()
        refreshContactTimeline()
        refreshTypingIndicators()
        applyCurrentFormTemplate()
        // iu8lmc fix (verso 1.0.448): il pannello e' caricato da un Loader asynchronous (Main.qml)
        // -> puo' essere DISTRUTTO durante l'incubazione prima che questa callLater differita scatti
        // ('Object or context destroyed during incubation' + TypeError ... is not a function su
        // loadPresenceEditor/refreshPresence). Guard: esegui solo se il componente e' ancora vivo e
        // il metodo e' callable; altrimenti no-op (il pannello sta morendo, popolarlo e' inutile).
        Qt.callLater(function() {
            if (root && typeof root.loadPresenceEditor === "function")
                root.loadPresenceEditor()
        })
        Qt.callLater(root.tryAutoImportSatelliteRigPair)
    }

    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        onTriggered: {
            root.uiNowMs = Date.now()
        }
    }

    Timer {
        id: slotSnifferTimer
        interval: 250
        running: root.slotSnifferActive()
        repeat: true
        onTriggered: root.continueSlotSniffer()
        onRunningChanged: {
            if (!running && root.slotSnifferActive())
                root.clearSlotSniffer("SNIFF cancelled")
        }
    }

    Timer {
        interval: 5000
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            root.uiNowMs = Date.now()
            root.refreshLivePollState()
        }
    }

    Timer {
        interval: 15000
        running: root.visible
        repeat: true
        onTriggered: {
            root.refreshSlowPollState()
        }
    }

    Timer {
        interval: 30000
        running: root.visible
        repeat: true
        onTriggered: {
            root.refreshStoreAudit()
        }
    }

    Timer {
        interval: 30000
        running: root.visible
        repeat: true
        onTriggered: {
            if (!ft2Link)
                return
            var changed = false
            if (typeof ft2Link.evaluateAutoAway === "function") {
                var awayResult = ft2Link.evaluateAutoAway(root.nowMs())
                changed = changed || !!(awayResult && awayResult.changed)
            }
            if (typeof ft2Link.evaluateQsoAutomation === "function") {
                var qsoResult = ft2Link.evaluateQsoAutomation(root.nowMs())
                changed = changed || !!(qsoResult && qsoResult.changed)
            }
            if (changed) {
                root.refreshPresence()
                root.refreshQsoAutomation()
                root.refreshSessions()
                root.refreshMessages()
                root.refreshStatistics()
                root.refreshStoreAudit()
                if (root.toolPageIndex === 6)
                    root.loadPresenceEditor()
            }
        }
    }

    Timer {
        interval: 30000
        running: root.visible && root.frequencyScheduleAutoApply
        repeat: true
        triggeredOnStart: false
        onTriggered: {
            if (!ft2Link)
                return
            if (String(ft2Link.transportState || "").indexOf("TX") >= 0)
                return
            root.applyActiveFrequencySchedule(true)
        }
    }

    Timer {
        interval: 5000
        running: root.visible && root.clusterAutoSync
        repeat: true
        onTriggered: {
            var now = Date.now()
            if (root.clusterLastAutoSyncMs > 0
                    && now - root.clusterLastAutoSyncMs < root.clusterAutoSyncSeconds * 1000)
                return
            root.clusterLastAutoSyncMs = now
            root.syncClusterShare(false)
        }
    }

    Timer {
        interval: 450
        running: root.chatUnreadBelow
        repeat: true
        onTriggered: root.chatUnreadPulse = !root.chatUnreadPulse
        onRunningChanged: {
            if (!running)
                root.chatUnreadPulse = false
        }
    }

    Timer {
        id: qsyApplyTimer
        interval: 1800
        repeat: false
        onTriggered: {
            root.applyQsyPlan(root.delayedQsyPlan,
                              root.delayedQsyReason.length > 0
                              ? root.delayedQsyReason
                              : "accepted-local")
            root.delayedQsyPlan = ({})
            root.delayedQsyReason = ""
        }
    }

    Connections {
        target: ft2Link
        ignoreUnknownSignals: true
        function onStationCountChanged() { root.refreshStations() }
        function onSessionCountChanged() { root.refreshSessions() }
        function onActiveSessionChanged() {
            root.selectedSessionId = ft2Link.activeSessionId
            root.refreshSessions()
            root.refreshStatistics()
            root.refreshPathAnalysis()
        }
        function onSessionsChanged() { root.refreshSessions(); root.refreshStatistics() }
        function onMessagesChanged(sessionId) {
            var changedSessionId = Number(sessionId)
            if (ft2Link && changedSessionId > 0
                    && Number(ft2Link.activeSessionId || 0) === changedSessionId
                    && root.selectedSessionId !== changedSessionId)
                root.selectedSessionId = changedSessionId
            if (changedSessionId === root.selectedSessionId)
                root.refreshMessages()
            root.refreshContactTimeline()
            root.refreshStatistics()
        }
        function onTransportStateChanged() { root.refreshStatistics(); root.refreshPathAnalysis() }
        function onBroadcastsChanged() { root.refreshBroadcasts(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onAlertsChanged() { root.refreshAlerts(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onAlertTagsChanged() { root.refreshAlertTags(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onMailboxChanged() { root.refreshMailbox(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onFormsChanged() { root.refreshForms(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onFileTransfersChanged() { root.refreshFileTransfers(); root.refreshReceivedFiles(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onBbsFileServerChanged() { root.refreshBbsFileServer(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onBulletinsChanged() { root.refreshBulletins(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onQsoLogChanged() { root.refreshQsoLog(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onLogbookOutboxChanged() { root.refreshLogbookOutbox(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onContactHistoryChanged() { root.refreshContactHistory(); root.refreshContactTimeline(); root.refreshStatistics(); root.refreshPathAnalysis() }
        function onPingLogChanged() { root.refreshPingLog(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onPathReportsChanged() { root.refreshPathReports(); root.refreshPathAnalysis(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onDigipeaterChanged() { root.refreshDigipeater(); root.refreshPathReports(); root.refreshStatistics() }
        function onBeaconHistoryChanged() { root.refreshBeaconHistory(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onClusterLastHeardChanged() { root.refreshClusterLastHeard(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onLocalStoreChanged() { root.refreshStoreAudit(); root.refreshPresence(); root.refreshQsoAutomation(); root.refreshBlockedCalls(); root.refreshClusterLastHeard(); root.refreshLogbookOutbox() }
        function onCannedMessagesChanged() { root.refreshCannedMessages(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onFrequencyPlanChanged() { root.refreshFrequencyPlan(); root.refreshStatistics(); root.refreshStoreAudit(); root.refreshQsyPlan() }
        function onBlockListChanged() { root.refreshBlockedCalls(); root.refreshStations(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onQsoAutomationChanged() { root.refreshQsoAutomation(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onTypingIndicatorsChanged() { root.refreshTypingIndicators() }
        function onPresenceChanged() {
            root.refreshPresence()
            root.refreshStatistics()
            root.refreshStoreAudit()
            if (root.toolPageIndex === 6)
                root.loadPresenceEditor()
        }
    }

    Connections {
        target: bridge
        ignoreUnknownSignals: true
        function onDeepSearchEnabledChanged() { root.applyCapabilities() }
        function onLowCpuModeEnabledChanged() { root.applyCapabilities() }
        function onCatBackendChanged() { root.tryAutoImportSatelliteRigPair() }
        function onCatConnectedChanged() { root.tryAutoImportSatelliteRigPair() }
        function onFt2LinkReceivedFileSaveFinished(requestId, result) {
            root.finishReceivedFileSave(requestId, result)
        }
        function onFt2LinkReceivedFilesDirectoryOpenFinished(requestId, result) {
            root.finishReceivedFileDirectoryOpen(result)
        }
        function onExternalAdifUploadStatus(uploadId, state, detail) {
            if (!ft2Link || typeof ft2Link.markLogbookUpload !== "function")
                return
            var cleanState = String(state || "Submitted")
            var cleanDetail = String(detail || "")
            ft2Link.markLogbookUpload(Number(uploadId || 0), cleanState, cleanDetail, root.nowMs())
            root.refreshLogbookOutbox()
            root.refreshStatistics()
            root.refreshStoreAudit()
            root.logExportText = root.prettyJson({
                ok: cleanState === "Sent",
                uploadId: Number(uploadId || 0),
                state: cleanState,
                detail: cleanDetail
            })
        }
        function onFt2LinkEmailGatewayStatus(requestId, mailboxId, state, detail) {
            var cleanState = String(state || "")
            var cleanDetail = String(detail || "")
            if (ft2Link && typeof ft2Link.markMailboxEmailGateway === "function"
                    && Number(mailboxId || 0) > 0) {
                ft2Link.markMailboxEmailGateway(Number(mailboxId || 0),
                                                cleanState,
                                                cleanDetail,
                                                root.nowMs())
                root.refreshMailbox()
                root.refreshStatistics()
                root.refreshStoreAudit()
            }
            root.emailGatewayStatus = "SMTP #" + Number(requestId || 0)
                                      + " " + cleanState
                                      + (cleanDetail.length > 0 ? " - " + cleanDetail : "")
            root.setEmailGatewayItemState(mailboxId, cleanState, cleanDetail)
            root.logExportText = root.prettyJson({
                ok: cleanState === "Sent",
                requestId: Number(requestId || 0),
                mailboxId: Number(mailboxId || 0),
                state: cleanState,
                detail: cleanDetail
            })
        }
    }

    Connections {
        // Reading these properties never changes the VFOs or PTT.  The signal
        // handlers only auto-populate an entirely empty satellite profile.
        target: bridge && bridge.hamlibCat ? bridge.hamlibCat : null
        ignoreUnknownSignals: true
        function onConnectedChanged() { root.tryAutoImportSatelliteRigPair() }
        function onFrequencyChanged() { root.tryAutoImportSatelliteRigPair() }
        function onTxFrequencyChanged() { root.tryAutoImportSatelliteRigPair() }
        function onSplitChanged() { root.tryAutoImportSatelliteRigPair() }
        function onSplitModeChanged() { root.tryAutoImportSatelliteRigPair() }
    }

    component SmallButton: Rectangle {
        id: smallButton
        signal clicked(var mouse)

        property string text: ""
        property string tip: ""
        property color accent: root.cyan
        property bool danger: false
        property bool hovered: false
        property bool pressed: false
        property bool checked: false
        property bool interactive: true
        property int labelSize: 10
        implicitHeight: 24
        implicitWidth: 64
        radius: 4
        color: !enabled ? Qt.rgba(1, 1, 1, 0.020)
                        : (checked ? Qt.rgba(accent.r, accent.g, accent.b, 0.22)
                        : (pressed ? Qt.rgba(accent.r, accent.g, accent.b, 0.24)
                                   : (hovered ? Qt.rgba(accent.r, accent.g, accent.b, 0.14)
                                              : Qt.rgba(1, 1, 1, 0.035))))
        border.color: checked ? accent
                              : (enabled ? Qt.rgba(accent.r, accent.g, accent.b, 0.58)
                              : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.18)
                                )
        border.width: 1
        opacity: enabled ? 1.0 : 0.72

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            text: smallButton.text
            color: smallButton.enabled ? smallButton.accent
                                       : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.52)
            font.family: root.mono
            font.pixelSize: smallButton.labelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            enabled: smallButton.enabled && smallButton.interactive
            hoverEnabled: true
            cursorShape: smallButton.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
            onEntered: smallButton.hovered = true
            onExited: {
                smallButton.hovered = false
                smallButton.pressed = false
            }
            onPressed: smallButton.pressed = true
            onReleased: smallButton.pressed = false
            onCanceled: smallButton.pressed = false
            onClicked: function(mouse) { smallButton.clicked(mouse) }
        }

        ToolTip.visible: hovered && tip.length > 0
        ToolTip.text: tip
        ToolTip.delay: 450
    }

    component CompactCheck: Item {
        id: compactCheck
        signal toggled(bool checked)

        property bool checked: false
        property string text: ""
        property string tip: ""
        property color accent: root.green
        property bool hovered: false
        implicitHeight: 24
        implicitWidth: checkBox.width + label.implicitWidth + 8

        Rectangle {
            id: checkBox
            width: 16
            height: 16
            radius: 2
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: compactCheck.checked ? compactCheck.accent
                                        : Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: compactCheck.checked
                          ? compactCheck.accent
                          : Qt.rgba(root.textSecondary.r,
                                    root.textSecondary.g,
                                    root.textSecondary.b,
                                    0.70)

            Text {
                anchors.centerIn: parent
                visible: compactCheck.checked
                text: "✓"
                color: root.panelBg
                font.family: root.mono
                font.pixelSize: 13
                font.bold: true
            }
        }

        Text {
            id: label
            anchors.left: checkBox.right
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: compactCheck.text
            color: compactCheck.checked ? root.textPrimary : root.textSecondary
            font.family: root.mono
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: compactCheck.hovered = true
            onExited: compactCheck.hovered = false
            onClicked: compactCheck.toggled(!compactCheck.checked)
        }

        ToolTip.visible: hovered && tip.length > 0
        ToolTip.text: tip
        ToolTip.delay: 450
    }

    component PaneResizeHandle: Rectangle {
        id: resizeHandle

        property string targetPane: "station"
        property bool hovered: false
        property bool pressed: false

        Layout.preferredWidth: 12
        Layout.fillHeight: true
        radius: 4
        color: pressed ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.16)
                       : (hovered ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.10)
                                  : "transparent")
        border.color: hovered || pressed ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.32)
                                          : root.borderSoft
        border.width: hovered || pressed ? 1 : 0

        Column {
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: 3
                Rectangle {
                    width: 3
                    height: 3
                    radius: 2
                    color: resizeHandle.hovered || resizeHandle.pressed
                           ? root.cyan : root.textSecondary
                    opacity: resizeHandle.hovered || resizeHandle.pressed ? 0.9 : 0.45
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            property real pressX: 0
            property int pressWidth: 0

            onEntered: resizeHandle.hovered = true
            onExited: {
                if (!resizeHandle.pressed)
                    resizeHandle.hovered = false
            }
            onPressed: function(mouse) {
                resizeHandle.pressed = true
                pressX = mouse.x
                pressWidth = resizeHandle.targetPane === "session"
                             ? root.sessionPaneWidth
                             : root.stationPaneWidth
            }
            onPositionChanged: function(mouse) {
                if (!resizeHandle.pressed)
                    return
                var width = pressWidth + (mouse.x - pressX)
                if (resizeHandle.targetPane === "session")
                    root.resizeSessionPane(width)
                else
                    root.resizeStationPane(width)
            }
            onReleased: {
                resizeHandle.pressed = false
                resizeHandle.hovered = containsMouse
            }
            onCanceled: {
                resizeHandle.pressed = false
                resizeHandle.hovered = containsMouse
            }
            onDoubleClicked: {
                if (resizeHandle.targetPane === "session")
                    root.resizeSessionPane(170)
                else
                    root.resizeStationPane(220)
            }
        }

        ToolTip.visible: hovered
        ToolTip.text: targetPane === "session" ? "Resize sessions" : "Resize stations"
        ToolTip.delay: 450
    }

    // 1.0.457 iu8lmc fix ("finestra troppo alta, comandi in basso tagliati"):
    // il root ha clip:true, quindi su layout bassi il contenuto oltre l'altezza
    // disponibile veniva TAGLIATO (composer/strumenti irraggiungibili). Wrapper
    // Flickable verticale: se lo spazio basta (>= minContentHeight) il layout
    // riempie come prima e il flick e' disattivato (zero cambi); se manca, il
    // contenuto tiene l'altezza minima usabile e il pannello SCORRE.
    Flickable {
        id: rootFlick
        // 1.0.467 iu8lmc: minContentHeight = altezza REALE del contenuto
        // (contentCol.implicitHeight) invece di un valore fisso. Cosi il
        // Flickable scorre ESATTAMENTE quanto serve: se la finestra e' alta
        // abbastanza il contenuto riempie senza scroll; se e' bassa/larga
        // (area centrale schiacciata) si scorre e NIENTE viene tagliato —
        // tutte le opzioni restano raggiungibili. Il fisso 414 forzava scroll
        // sui monitor normali; Math.min(414,h) invece uccideva lo scroll e
        // NASCONDEVA le opzioni in eccesso: entrambi sbagliati.
        readonly property int minContentHeight: contentCol.implicitHeight
        anchors.fill: parent
        anchors.margins: 8
        contentWidth: width
        contentHeight: Math.max(height, minContentHeight)
        interactive: contentHeight > height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar {
            policy: rootFlick.contentHeight > rootFlick.height
                    ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }

    ColumnLayout {
        id: contentCol
        width: rootFlick.contentWidth
        height: rootFlick.contentHeight
        spacing: 8

        RowLayout {
            id: headerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spacing: 8

            MouseArea {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                cursorShape: root.dragTarget ? Qt.SizeAllCursor : Qt.ArrowCursor
                drag.target: root.dragTarget
                drag.axis: Drag.XAndYAxis
                drag.minimumX: 0
                drag.minimumY: 0
                drag.maximumX: root.dragTarget && root.dragTarget.parent
                               ? Math.max(0, root.dragTarget.parent.width - root.dragTarget.width)
                               : 0
                drag.maximumY: root.dragTarget && root.dragTarget.parent
                               ? Math.max(0, root.dragTarget.parent.height - root.dragTarget.height)
                               : 0
                onReleased: {
                    if (root.dragTarget && root.dragTarget.savePosition)
                        root.dragTarget.savePosition()
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        text: qsTr("FT2-LINK")
                        font.family: root.mono
                        font.pixelSize: 14
                        font.bold: true
                        color: root.cyan
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Rectangle {
                        Layout.preferredWidth: 62
                        Layout.preferredHeight: 20
                        radius: 4
                        color: Qt.rgba(root.green.r, root.green.g, root.green.b, 0.12)
                        border.color: Qt.rgba(root.green.r, root.green.g, root.green.b, 0.46)

                        Text {
                            anchors.centerIn: parent
                            text: root.preferW2300 ? "2300" : "500"
                            font.family: root.mono
                            font.pixelSize: 10
                            font.bold: true
                            color: root.green
                        }
                    }

                    Text {
                        text: ft2Link ? (ft2Link.stationCount + " stn / "
                                         + ft2Link.sessionCount + " sess / "
                                         + ft2Link.broadcastCount + " bc / "
                                         + ft2Link.alertCount + " alert / "
                                         + ft2Link.mailboxCount + " mail"
                                         + (ft2Link.mailboxUnreadCount > 0 ? "+" + ft2Link.mailboxUnreadCount : "")
                                         + " / "
                                         + ft2Link.relayQueueCount + " rly / "
                                         + ft2Link.formCount + " form / "
                                         + ft2Link.fileTransferCount + " file / "
                                         + ft2Link.bulletinCount + " bbs / "
                                         + ft2Link.qsoLogCount + " qso / "
                                         + ft2Link.logbookOutboxCount + " lbq / "
                                         + ft2Link.contactCount + " call / "
                                         + ft2Link.pingCount + " ping / "
                                         + ft2Link.transportState) : "offline"
                        font.family: root.mono
                        font.pixelSize: 10
                        color: ft2Link && ft2Link.alertCount > 0 ? root.red
                              : (ft2Link && ft2Link.transportState === "Failed" ? root.red
                              : (ft2Link && ft2Link.transportBusy ? root.amber : root.textSecondary)
                                )
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            SmallButton {
                text: root.preferW2300 ? "W2300" : "W500"
                implicitWidth: 76
                accent: root.green
                tip: "Switch 500/2300 Hz profile"
                onClicked: {
                    root.preferW2300 = !root.preferW2300
                    root.applyCapabilities()
                }
            }

            SmallButton {
                text: root.robustMode ? "ROB" : "FAST"
                implicitWidth: 58
                accent: root.amber
                tip: root.deepRateEnabled
                     ? "Switch fast/robust rate. DEEP and ULTRA retries are enabled by Deep Search."
                     : "Switch fast/robust rate. Enable Deep Search and disable Low CPU to allow DEEP/ULTRA retry."
                onClicked: {
                    root.robustMode = !root.robustMode
                    root.applyCapabilities()
                }
            }

            SmallButton {
                text: root.poppedOut ? "DOCK" : "POP"
                implicitWidth: root.poppedOut ? 56 : 44
                accent: root.cyan
                tip: root.poppedOut ? "Dock FT2-Link panel" : "Pop out FT2-Link panel"
                onClicked: root.popDockRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.borderSoft
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height < 360 ? 20 : 26
            radius: 4
            color: Qt.rgba(1, 1, 1, 0.025)
            border.color: Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.16)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12

                Text {
                    Layout.preferredWidth: 190
                    text: root.rfStatusLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    font.bold: ft2Link && ft2Link.transportBusy
                    color: ft2Link && ft2Link.transportBusy ? root.amber : root.textSecondary
                }

                Text {
                    id: queueStatusText
                    Layout.preferredWidth: 230
                    text: root.queueStatusLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    font.bold: root.queueStatusActive()
                    color: root.queueStatusColor()

                    MouseArea {
                        id: queueStatusMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: root.queueStatusClickable()
                                     ? Qt.PointingHandCursor
                                     : Qt.ArrowCursor
                        onClicked: root.openQueueStatus()
                    }

                    ToolTip.visible: queueStatusMouse.containsMouse
                                     && root.queueStatusClickable()
                    ToolTip.text: root.queueStatusTip()
                    ToolTip.delay: 450
                }

                Text {
                    Layout.fillWidth: true
                    text: root.globalErrorLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.red
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            ColumnLayout {
                Layout.preferredWidth: root.stationPaneWidth
                Layout.minimumWidth: 160
                Layout.maximumWidth: 420
                Layout.fillHeight: true
                spacing: 6

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: root.stationHistoryMode ? "CQ/BEACON" : "STATIONS"
                            font.family: root.mono
                            font.pixelSize: 11
                            font.bold: true
                            color: root.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        SmallButton {
                            text: "HIST"
                            implicitWidth: 46
                            checked: root.stationHistoryMode
                            accent: root.amber
                            tip: "Toggle CQ/beacon history"
                            onClicked: root.stationHistoryMode = !root.stationHistoryMode
                        }
                        CompactCheck {
                            id: cqOnlyCheck
                            visible: !root.stationHistoryMode
                            text: qsTr("CQ only")
                            checked: root.cqOnly
                            accent: root.green
                            tip: "Show CQ-capable stations only"
                            onToggled: function(nextChecked) {
                                root.cqOnly = nextChecked
                                root.refreshStations()
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        spacing: 6

                        CompactCheck {
                            id: cqArmCheck
                            text: "ARM"
                            checked: ft2Link ? ft2Link.radioTxArmed : false
                            accent: root.green
                            tip: "Arm FT2-Link RF transmit"
                            onToggled: function(nextChecked) {
                                if (ft2Link)
                                    ft2Link.setRadioTxArmed(nextChecked)
                            }
                        }

                        SmallButton {
                            text: root.cqTxButtonText()
                            implicitWidth: 64
                            accent: root.amber
                            enabled: !!ft2Link && ft2Link.radioTxArmed
                            tip: "Transmit one CQ beacon"
                            onClicked: root.transmitBeacon(true)
                        }

                        CompactCheck {
                            id: autoBeaconCheck
                            text: qsTr("AUTO CQ")
                            checked: ft2Link ? ft2Link.autoBeaconEnabled : false
                            accent: root.green
                            tip: "Enable periodic CQ"
                            onToggled: function(nextChecked) {
                                root.toggleAutoBeacon(nextChecked)
                            }
                        }

                        SmallButton {
                            text: root.beaconIntervalText()
                            implicitWidth: 54
                            accent: root.textSecondary
                            tip: "Auto CQ interval"
                            onClicked: root.cycleBeaconInterval()
                        }

                        SmallButton {
                            text: "SNIFF " + root.slotSnifferText()
                            implicitWidth: 72
                            accent: root.cyan
                            tip: "Listen-before-transmit window"
                            onClicked: root.cycleSlotSnifferSeconds()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        spacing: 4

                        SmallButton {
                            text: root.currentCqType()
                            implicitWidth: 70
                            accent: root.green
                            tip: "Special CQ type"
                            onClicked: root.cycleCqType()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.currentCqLocator().length > 0
                                  ? "LOC " + root.currentCqLocator()
                                  : "LOC --"
                            elide: Text.ElideRight
                            font.family: root.mono
                            font.pixelSize: 10
                            color: root.textSecondary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        spacing: 4

                        CompactCheck {
                            text: "SLOT"
                            checked: !root.skipCqSlot
                            accent: root.green
                            tip: "Advertise CQ slot offset"
                            onToggled: function(nextChecked) {
                                root.skipCqSlot = !nextChecked
                            }
                        }

                        SmallButton {
                            text: root.currentCqSlotLabel()
                            implicitWidth: 42
                            accent: root.amber
                            enabled: !root.skipCqSlot && root.qsySlots.length > 0
                            tip: root.currentCqSlotTip()
                            onClicked: root.cycleCqSlot()
                        }

                        SmallButton {
                            text: root.cqSlotStatusText()
                            implicitWidth: 62
                            accent: root.cqSlotWaitRemainingSeconds() > 0
                                    ? root.amber
                                    : (root.cqSlotBusy() ? root.red : root.green)
                            enabled: !root.skipCqSlot
                            interactive: false
                            tip: root.currentCqSlotOffsetLabel()
                        }

                        SmallButton {
                            text: root.cqSlotWaitText()
                            implicitWidth: 44
                            accent: root.textSecondary
                            enabled: !root.skipCqSlot
                            tip: "CQ slot wait time"
                            onClicked: root.cycleCqSlotWait()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    Layout.minimumHeight: 58
                    Layout.maximumHeight: 58
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        spacing: 4

                        TextField {
                            id: manualCall
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            placeholderText: "CALL"
                            font.family: root.mono
                            font.pixelSize: 10
                            selectByMouse: true
                            onAccepted: root.addManualStation()
                        }

                        TextField {
                            id: manualGrid
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 24
                            placeholderText: "GRID"
                            font.family: root.mono
                            font.pixelSize: 10
                            selectByMouse: true
                            onAccepted: root.addManualStation()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        spacing: 4

                        TextField {
                            id: manualName
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            placeholderText: "NAME"
                            font.family: root.mono
                            font.pixelSize: 10
                            selectByMouse: true
                            onAccepted: root.addManualStation()
                        }

                        TextField {
                            id: manualTag
                            Layout.preferredWidth: 54
                            Layout.preferredHeight: 24
                            placeholderText: "TAG"
                            font.family: root.mono
                            font.pixelSize: 10
                            maximumLength: 16
                            selectByMouse: true
                            onAccepted: root.addManualStation()
                        }

                        CompactCheck {
                            id: manualCq
                            checked: true
                            text: "CQ"
                            accent: root.green
                            tip: "Mark manual station as CQ-capable"
                            onToggled: function(nextChecked) {
                                manualCq.checked = nextChecked
                            }
                        }

                        SmallButton {
                            text: "ADD"
                            implicitWidth: 46
                            implicitHeight: 24
                            accent: root.green
                            tip: "Add manual station"
                            onClicked: root.addManualStation()
                        }
                    }
                }

                ListView {
                    id: stationList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: 6
                    // 1.0.462 iu8lmc: floor esplicito (~4 righe da 50px) cosi la
                    // lista resta visibile e cliccabile anche su finestre basse
                    // (monitor ~775px): la ColumnLayout le riserva questo spazio
                    // togliendolo alle altre sezioni fillHeight (sessioni/chat).
                    // 1.0.478: su pannelli bassi questa soglia tagliava il composer
                    // manuale; ora la lista si comprime e i campi CALL/GRID restano
                    // sempre raggiungibili.
                    Layout.minimumHeight: Math.max(64, Math.min(124, root.height * 0.16))
                    clip: true
                    model: root.stationHistoryMode ? root.beaconHistory : root.stations
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: stationCard
                        // 1.0.462 iu8lmc: modelData come REQUIRED PROPERTY.
                        // Per un modello ad array JS il 'modelData' nudo e' un
                        // context property: funziona nelle binding ma NON nei
                        // signal handler (onClicked) -> il click su una stazione
                        // non connetteva. Dichiararlo required lo rende una
                        // property vera del delegato, in scope ovunque.
                        required property var modelData
                        width: stationList.width
                        height: Math.max(62, stationCardContent.implicitHeight + 16)
                        clip: true
                        color: stationMouse.containsMouse
                               ? root.rowHover
                               : Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.06)
                        border.color: Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.40)
                        border.width: 1
                        radius: 4

                        RowLayout {
                            id: stationCardContent
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text {
                                        text: String(modelData.call || "")
                                        font.family: root.mono
                                        font.pixelSize: 20
                                        font.bold: true
                                        color: root.cyan
                                    }
	                                    Text {
	                                        text: modelData.cq ? String(modelData.cqType || "CQ") : "BCN"
	                                        font.family: root.mono
	                                        font.pixelSize: 11
	                                        font.bold: true
	                                        color: modelData.cq ? root.green : root.textSecondary
	                                    }
	                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.capabilities
                                          ? String(modelData.capabilities.beaconSummary
                                                   || modelData.capabilities.preferredProfileName
                                                   || "")
                                          : ""
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.amber
                                    visible: text.length > 0
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.stationSubtitle(modelData)
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.stationSubtitleColor(modelData)
                                }
                            }

                            SmallButton {
                                id: stationRelayButton
                                readonly property var workflow: root.stationRelayWorkflow(modelData)
                                text: "FWD"
                                implicitWidth: 42
                                implicitHeight: 26
                                labelSize: 9
                                accent: !workflow ? root.amber
                                        : (String(workflow.priority || "") === "EMCOMM" ? root.red : root.amber)
                                visible: !root.stationHistoryMode && workflow !== null
                                enabled: visible
                                tip: "Call relay and prepare parked mail forwarding"
                                onClicked: root.useStationRelayWorkflow(modelData)
                            }

                            SmallButton {
                                text: root.stationConnectText(modelData.call)
                                implicitWidth: 112
                                implicitHeight: 36
                                labelSize: 12
                                accent: root.isStationConnecting(modelData.call) ? root.amber : root.green
                                visible: !root.stationHistoryMode
                                enabled: visible
                                interactive: root.stationConnectEnabled(modelData.call)
                                tip: root.stationConnectTip(modelData.call)
                                onClicked: root.connectStationRadio(modelData.call)
                            }
                        }

                        MouseArea {
                            id: stationMouse
                            anchors.fill: parent
                            z: -1
                            hoverEnabled: true
                            cursorShape: root.stationConnectEnabled(modelData.call)
                                         ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (root.stationConnectEnabled(modelData.call))
                                    root.connectStationRadio(modelData.call)
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: stationList.count === 0
                        text: root.stationHistoryMode ? "No CQ/beacon history"
                                                      : "No FT2-Link stations"
                        font.family: root.mono
                        font.pixelSize: 11
                        color: root.textSecondary
                    }
                }

	            }

            PaneResizeHandle {
                targetPane: "station"
            }

            ColumnLayout {
                Layout.preferredWidth: root.sessionPaneWidth
                Layout.minimumWidth: 130
                Layout.maximumWidth: 340
                Layout.fillHeight: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "SESSIONS"
                        font.family: root.mono
                        font.pixelSize: 11
                        font.bold: true
                        color: root.textPrimary
                    }
                    Item { Layout.fillWidth: true }
                    SmallButton {
                        text: qsTr("LOOP ACK")
                        implicitWidth: 72
                        accent: root.amber
                        visible: root.lastHelloBytes && root.selectedRemoteCall.length > 0
                        enabled: root.lastHelloBytes && root.selectedRemoteCall.length > 0
                        tip: "Complete local loopback handshake"
                        onClicked: root.completeLoopbackAck()
                    }
                }

                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.sessions
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: sessionDelegate
                        // 1.0.463 iu8lmc: required property -> modelData in scope anche
                        // negli onClicked del delegato sessione (era blank/ReferenceError).
                        required property var modelData
                        readonly property string capsText: String(modelData.capabilitySummary
                                                                  || (modelData.capabilities
                                                                      ? modelData.capabilities.beaconSummary
                                                                      : "")
                                                                  || "")
                        width: sessionList.width
                        height: Math.max(44, sessionContent.implicitHeight + 12)
                        radius: 3
                        color: root.selectedSessionId === Number(modelData.sessionId)
                               ? root.rowSelect
                               : (sessionMouse.containsMouse ? root.rowHover : "transparent")

                        Column {
                            id: sessionContent
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 2
                            Text {
                                text: "#" + Number(modelData.sessionId).toString(16).toUpperCase() + " " + String(modelData.remoteCall || "")
                                width: parent.width
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 11
                                font.bold: true
                                color: root.textPrimary
                            }
                            Text {
                                text: String(modelData.stateName || "") + "  " + String(modelData.profileName || "")
                                width: parent.width
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 10
                                color: modelData.stateName === "Closed" ? root.red
                                      : (modelData.accepted ? root.green
                                                            : (modelData.stateName === "Calling" ? root.amber : root.textSecondary))
                            }
                            Text {
                                text: sessionMouse.containsMouse && String(modelData.serviceSummary || "").length > 0
                                      ? sessionDelegate.capsText
                                      : String(modelData.waveformSummary || sessionDelegate.capsText)
                                width: parent.width
                                visible: text.length > 0
                                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                maximumLineCount: sessionMouse.containsMouse ? 2 : 1
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 9
                                color: root.amber
                            }
                        }

                        MouseArea {
                            id: sessionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectSession(modelData.sessionId)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: sessionList.count === 0
                        text: qsTr("No sessions")
                        font.family: root.mono
                        font.pixelSize: 11
                        color: root.textSecondary
                    }
                }
            }

            PaneResizeHandle {
                targetPane: "session"
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: root.selectedRemoteCall.length > 0
                              ? root.selectedRemoteCall + (root.selectedSessionStateName.length > 0 ? "  " + root.selectedSessionStateName : "")
                              : "CHAT"
                        font.family: root.mono
                        font.pixelSize: 11
                        font.bold: true
                        color: root.selectedSessionStateName === "Closed" ? root.red : root.textPrimary
                    }
                    Text {
                        visible: root.typingSummaryText.length > 0
                        text: root.typingSummaryText
                        Layout.maximumWidth: 180
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 10
                        color: root.green
                    }
                    Item { Layout.fillWidth: true }
                    SmallButton {
                        text: ft2Link && ft2Link.radioTxArmed ? "PING TX" : "PING"
                        implicitWidth: 62
                        accent: root.cyan
                        enabled: !!ft2Link && root.selectedRemoteCall.length > 0
                        tip: ft2Link && ft2Link.radioTxArmed ? "Transmit ping" : "Arm ping"
                        onClicked: root.armOrTransmitPing(root.selectedRemoteCall)
                    }
                    SmallButton {
                        text: "DISC"
                        implicitWidth: 46
                        accent: root.amber
                        visible: root.selectedSessionId > 0
                        enabled: root.selectedSessionId > 0 && root.selectedSessionStateName !== "Closed"
                        tip: "Send 73 and disconnect"
                        onClicked: root.disconnectSelectedSession()
                    }
                    SmallButton {
                        text: "ABORT"
                        implicitWidth: 54
                        accent: root.red
                        visible: root.selectedSessionId > 0
                        enabled: root.selectedSessionId > 0 && root.selectedSessionStateName !== "Closed"
                        tip: "Abort selected session locally"
                        onClicked: root.closeSelectedSession()
                    }
                    Text {
                        text: ft2Link && ft2Link.lastError.length > 0 ? ft2Link.lastError : ""
                        Layout.maximumWidth: 220
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 9
                        color: root.red
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    spacing: 8

                    Text {
                        text: root.transportLine()
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 10
                        color: root.textSecondary
                    }

                    CompactCheck {
                        id: ackAudioCheck
                        checked: true
                        text: "ACK"
                        accent: root.green
                        onToggled: function(nextChecked) {
                            ackAudioCheck.checked = nextChecked
                        }
                    }

                    CompactCheck {
                        id: dropDataCheck
                        text: qsTr("DROP DATA")
                        accent: root.red
                        onToggled: function(nextChecked) {
                            dropDataCheck.checked = nextChecked
                        }
                    }

                    CompactCheck {
                        id: dropAckCheck
                        text: qsTr("DROP ACK")
                        accent: root.red
                        onToggled: function(nextChecked) {
                            dropAckCheck.checked = nextChecked
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    spacing: 8

                    Text {
                        text: root.radioLine()
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 10
                        color: ft2Link && ft2Link.radioTxArmed ? root.amber : root.textSecondary
                    }

                    CompactCheck {
                        id: radioArmCheck
                        checked: ft2Link ? ft2Link.radioTxArmed : false
                        text: "ARM"
                        accent: root.green
                        onToggled: function(nextChecked) {
                            if (ft2Link)
                                ft2Link.setRadioTxArmed(nextChecked)
                        }
                    }

                    SmallButton {
                        text: "PREP"
                        implicitWidth: 46
                        accent: root.amber
                        enabled: root.selectedSessionConnected && composeText.text.trim().length > 0
                        tip: "Prepare RF audio"
                        onClicked: root.prepareRadioTx()
                    }

                    SmallButton {
                        text: "RF TX"
                        implicitWidth: 52
                        accent: root.red
                        enabled: root.selectedSessionConnected && composeText.text.trim().length > 0
                                 && ft2Link && ft2Link.radioTxArmed
                        tip: "Transmit prepared RF audio"
                        onClicked: root.transmitRadioTx()
                    }

                    SmallButton {
                        text: "QSL"
                        implicitWidth: 42
                        accent: root.green
                        enabled: root.selectedSessionId > 0
                        tip: "Insert QSL card text"
                        onClicked: root.insertQslCard()
                    }

                    SmallButton {
                        text: "ADIF"
                        implicitWidth: 46
                        accent: root.amber
                        enabled: root.selectedSessionId > 0
                        tip: "Copy ADIF record"
                        onClicked: root.copyAdifRecord()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.toolPageIndex !== 4
                             && root.broadcasts.length > 0
                    text: root.latestBroadcastLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    font.bold: root.alerts.length > 0
                    color: root.alerts.length > 0 ? root.red : root.textSecondary
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.mailbox.length > 0
                    text: root.latestMailboxLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.green
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.forms.length > 0
                             && root.height > 360
                    text: root.latestFormLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.amber
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.fileTransfers.length > 0
                             && root.height > 360
                    text: root.latestFileLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.cyan
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.bulletins.length > 0
                    text: root.latestBulletinLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.qsoLog.length > 0
                    text: root.latestQsoLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.green
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.contactHistory.length > 0
                    text: root.latestContactLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.textSecondary
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedMessages.length === 0
                             && root.pingLog.length > 0
                             && root.height > 360
                    text: root.latestPingLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.cyan
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 76
                    Layout.preferredHeight: 180

                    ListView {
                        id: messageList
                        anchors.fill: parent
                        clip: true
                        spacing: 4
                        model: root.toolPageIndex === 4
                               ? root.broadcasts : root.selectedMessages
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                        onContentYChanged: {
                            if (root.messageListAtEnd()) {
                                root.chatScrollPinned = true
                                root.chatUnreadBelow = false
                            } else {
                                root.chatScrollPinned = false
                            }
                        }
                        onMovementEnded: {
                            if (root.messageListAtEnd()) {
                                root.chatScrollPinned = true
                                root.chatUnreadBelow = false
                            }
                        }
                        onCountChanged: {
                            if (root.chatScrollPinned)
                                Qt.callLater(root.scrollChatToEnd)
                        }

                        delegate: Rectangle {
                            required property var modelData
                            readonly property string directionLabel:
                                modelData.directionName === "Outgoing" ? "TX"
                                : (modelData.directionName === "Incoming" ? "RX" : "SYS")
                            readonly property string peerLabel:
                                String(modelData.remoteCall || "").length > 0
                                ? (" " + String(modelData.remoteCall || ""))
                                : ""

                            width: messageList.width
                            implicitHeight: messageText.implicitHeight + 16
                            radius: 4
                            color: modelData.directionName === "System"
                                   ? Qt.rgba(root.amber.r, root.amber.g, root.amber.b, 0.12)
                                   : (modelData.directionName === "Outgoing"
                                      ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.10)
                                      : Qt.rgba(root.green.r, root.green.g, root.green.b, 0.10))
                            border.color: Qt.rgba(1, 1, 1, 0.06)

                            Text {
                                id: messageText
                                anchors.fill: parent
                                anchors.margins: 8
                                text: parent.directionLabel + parent.peerLabel
                                      + " [" + String(modelData.deliveryName || "") + "] "
                                      + String(modelData.text || "")
                                wrapMode: Text.Wrap
                                font.family: root.mono
                                font.pixelSize: 11
                                color: root.textPrimary
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: messageList.count === 0
                            text: root.toolPageIndex === 4
                                  ? "No broadcast messages"
                                  : (root.selectedSessionId > 0
                                     ? "No messages" : "Select a session")
                            font.family: root.mono
                            font.pixelSize: 11
                            color: root.textSecondary
                        }
                    }

                    SmallButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10
                        text: "DOWN"
                        implicitWidth: 62
                        accent: root.chatUnreadBelow && root.chatUnreadPulse ? root.green : root.cyan
                        visible: messageList.count > 0 && !root.messageListAtEnd()
                        tip: root.chatUnreadBelow ? "New messages below" : "Scroll to latest"
                        onClicked: root.scrollChatToEnd()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.borderSoft
                }

                FT2LinkTabBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 32 : 0
                    Layout.minimumHeight: visible ? 32 : 0
                    Layout.maximumHeight: visible ? 32 : 0
                    visible: !root.toolTabsExternal
                    panel: root
                }

                Item {
                    id: receivedFilesPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? root.toolStackPreferredHeight() : 0
                    Layout.minimumHeight: visible ? root.toolStackPreferredHeight() : 0
                    Layout.maximumHeight: visible ? root.toolStackPreferredHeight() : 0
                    visible: root.toolPageIndex === 11
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            spacing: 6

                            Text {
                                text: qsTr("RECEIVED FILES")
                                font.family: root.mono
                                font.pixelSize: 11
                                font.bold: true
                                color: root.cyan
                            }

                            Text {
                                text: root.receivedFilesCountText()
                                font.family: root.mono
                                font.pixelSize: 10
                                color: root.receivedFileUnreadCount > 0
                                       ? root.amber : root.textSecondary
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.receivedFileIoPendingCount > 0
                                      ? root.receivedFilePendingText()
                                      : root.effectiveReceivedFileDirectory()
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 10
                                color: root.textSecondary
                            }

                            SmallButton {
                                text: root.receivedFileAutoSave
                                      ? qsTr("AUTO ON") : qsTr("AUTO OFF")
                                implicitWidth: 70
                                accent: root.receivedFileAutoSave ? root.green : root.textSecondary
                                tip: qsTr("Automatically save new files in the receive folder")
                                onClicked: root.receivedFileAutoSave = !root.receivedFileAutoSave
                            }

                            SmallButton {
                                text: qsTr("FOLDER")
                                implicitWidth: 58
                                accent: root.cyan
                                tip: qsTr("Choose the received files folder")
                                onClicked: root.chooseReceivedFileDirectory()
                            }

                            SmallButton {
                                text: qsTr("OPEN")
                                implicitWidth: 48
                                accent: root.cyan
                                tip: qsTr("Open the received files folder")
                                onClicked: root.openReceivedFileDirectory()
                            }

                            SmallButton {
                                text: qsTr("READ ALL")
                                implicitWidth: 70
                                accent: root.amber
                                enabled: root.receivedFileUnreadCount > 0
                                tip: qsTr("Clear unread marker for all received files")
                                onClicked: root.markAllReceivedFilesRead()
                            }

                            SmallButton {
                                text: qsTr("CLEAR")
                                implicitWidth: 52
                                accent: root.red
                                enabled: !!ft2Link && root.receivedFiles.length > root.receivedFileUnreadCount
                                tip: qsTr("Delete received files already marked read")
                                onClicked: root.clearReadReceivedFiles()
                            }
                        }

                        ListView {
                            id: receivedFileListPanel
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 3
                            boundsBehavior: Flickable.StopAtBounds
                            model: root.receivedFiles
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Rectangle {
                                id: rxFilePanelDelegate
                                required property var modelData
                                width: receivedFileListPanel.width
                                height: 28
                                radius: 4
                                color: rxFilePanelMouse.containsMouse ? root.rowHover : "transparent"

                                MouseArea {
                                    id: rxFilePanelMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    spacing: 6

                                    Text {
                                        Layout.preferredWidth: 36
                                        text: rxFilePanelDelegate.modelData.unread
                                              ? qsTr("NEW")
                                              : (rxFilePanelDelegate.modelData.imageLike
                                                 ? qsTr("IMG") : qsTr("FILE"))
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: rxFilePanelDelegate.modelData.unread
                                               ? root.amber
                                               : (rxFilePanelDelegate.modelData.imageLike ? root.amber : root.cyan)
                                    }

                                    Text {
                                        Layout.preferredWidth: 64
                                        text: String(rxFilePanelDelegate.modelData.senderCall || "--")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.textPrimary
                                    }

                                    Text {
                                        Layout.preferredWidth: 110
                                        text: String(rxFilePanelDelegate.modelData.fileName || "")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.green
                                    }

                                    Text {
                                        Layout.preferredWidth: 90
                                        text: root.receivedFileDate(rxFilePanelDelegate.modelData)
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    Text {
                                        Layout.preferredWidth: 52
                                        text: String(rxFilePanelDelegate.modelData.sizeBytes || 0) + " B"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: String(rxFilePanelDelegate.modelData.preview || "")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    SmallButton {
                                        text: qsTr("SAVE")
                                        implicitWidth: 48
                                        accent: root.green
                                        enabled: root.receivedFileHasContent(rxFilePanelDelegate.modelData)
                                                 && !root.receivedFilePendingTransfers[
                                                     String(Number(rxFilePanelDelegate.modelData.id || 0))]
                                        tip: qsTr("Save to the configured receive folder")
                                        onClicked: root.saveReceivedFile(rxFilePanelDelegate.modelData)
                                    }

                                    SmallButton {
                                        text: qsTr("SAVE AS")
                                        implicitWidth: 58
                                        accent: root.cyan
                                        enabled: root.receivedFileHasContent(rxFilePanelDelegate.modelData)
                                                 && !root.receivedFilePendingTransfers[
                                                     String(Number(rxFilePanelDelegate.modelData.id || 0))]
                                        tip: qsTr("Save with another name or in another folder")
                                        onClicked: root.saveReceivedFileAs(rxFilePanelDelegate.modelData)
                                    }

                                    SmallButton {
                                        text: rxFilePanelDelegate.modelData.unread
                                              ? qsTr("READ") : qsTr("UNREAD")
                                        implicitWidth: 54
                                        accent: rxFilePanelDelegate.modelData.unread ? root.amber : root.textSecondary
                                        enabled: !!ft2Link
                                        tip: rxFilePanelDelegate.modelData.unread
                                             ? qsTr("Mark received file as read")
                                             : qsTr("Mark received file as unread")
                                        onClicked: root.markReceivedFileRead(rxFilePanelDelegate.modelData,
                                                                              !!rxFilePanelDelegate.modelData.unread,
                                                                              true)
                                    }

                                    SmallButton {
                                        text: qsTr("COPY")
                                        implicitWidth: 48
                                        accent: root.cyan
                                        enabled: String(rxFilePanelDelegate.modelData.content || "").length > 0
                                        tip: qsTr("Copy received file content")
                                        onClicked: {
                                            var text = String(rxFilePanelDelegate.modelData.content || "")
                                            root.copyPlainText(text)
                                            root.markReceivedFileRead(rxFilePanelDelegate.modelData, true, false)
                                            root.receivedFileStatus = qsTr("Copied %1").arg(
                                                        String(rxFilePanelDelegate.modelData.fileName
                                                               || qsTr("received file")))
                                        }
                                    }

                                    SmallButton {
                                        text: qsTr("DEL")
                                        implicitWidth: 36
                                        accent: root.red
                                        enabled: !!ft2Link
                                        tip: qsTr("Delete this received file entry")
                                        onClicked: root.deleteReceivedFile(rxFilePanelDelegate.modelData)
                                    }
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: receivedFileListPanel.count === 0
                                text: qsTr("No received files")
                                font.family: root.mono
                                font.pixelSize: 10
                                color: root.textSecondary
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 16
                            text: root.receivedFileStatus.length > 0
                                  ? root.receivedFileStatus
                                  : (root.receivedFiles.length > 0
                                     ? qsTr("Folder: %1").arg(
                                           root.effectiveReceivedFileDirectory())
                                     : qsTr("Waiting for files in %1").arg(
                                           root.effectiveReceivedFileDirectory()))
                            elide: Text.ElideMiddle
                            font.family: root.mono
                            font.pixelSize: 10
                            color: root.textSecondary
                        }
                    }
                }

                StackLayout {
                    id: toolStack
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? root.toolStackPreferredHeight() : 0
                    Layout.minimumHeight: visible ? root.toolStackPreferredHeight() : 0
                    Layout.maximumHeight: visible ? root.toolStackPreferredHeight() : 0
                    visible: root.toolPageIndex !== 11
                    currentIndex: root.toolStackIndexForPage(root.toolPageIndex)
                    clip: true

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                visible: root.selectedSessionId > 0
                                spacing: 0

                                ListView {
                                    id: cannedList
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    orientation: ListView.Horizontal
                                    spacing: 6
                                    clip: true
                                    boundsBehavior: Flickable.StopAtBounds
                                    model: root.cannedMessages
                                    ScrollBar.horizontal: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }

                                    delegate: SmallButton {
                                        required property var modelData
                                        text: String(modelData.label || "")
                                        implicitWidth: Math.max(44, Math.min(82, text.length * 9 + 18))
                                        accent: root.cyan
                                        tip: String(modelData.tip || "")
                                        onClicked: root.insertCannedMessage(String(modelData.templateText || ""))
                                    }
                                }

                                Text {
                                    visible: cannedList.count === 0
                                    text: qsTr("No tags")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                visible: root.selectedSessionId > 0
                                spacing: 6

                                Text {
                                    text: root.qsyPlanText()
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.qsyPlanValid() ? root.amber : root.textSecondary
                                }

                                SmallButton {
                                    text: "OK"
                                    implicitWidth: 34
                                    accent: root.green
                                    enabled: root.selectedSessionConnected && root.qsyPlanValid()
                                    tip: "Send QSY accepted"
                                    onClicked: root.acceptQsyInvite()
                                }

                                SmallButton {
                                    text: "NO"
                                    implicitWidth: 34
                                    accent: root.amber
                                    enabled: root.selectedSessionConnected && root.qsyPlanValid()
                                    tip: "Send QSY rejected"
                                    onClicked: root.sendQsyControlTag(String(root.qsyPlan.rejectTag || "<QSYJ>"))
                                }

                                SmallButton {
                                    text: "QJO"
                                    implicitWidth: 40
                                    accent: root.red
                                    enabled: root.selectedSessionConnected && root.qsyPlanValid()
                                    tip: "Reject out-of-range QSY"
                                    onClicked: root.sendQsyControlTag(String(root.qsyPlan.outOfRangeTag || "<QJO>"))
                                }

                                SmallButton {
                                    text: qsTr("SET CF")
                                    implicitWidth: 58
                                    accent: root.cyan
                                    enabled: root.currentDialFrequencyHz() > 0
                                    tip: "Store current dial as calling frequency"
                                    onClicked: root.setCallingFrequencyFromRig()
                                }

                                SmallButton {
                                    text: qsTr("CF TAG")
                                    implicitWidth: 58
                                    accent: root.cyan
                                    enabled: root.selectedSessionId > 0 && root.qsyCallingFrequencyHz > 0
                                    tip: root.qsyCallingFrequencyText()
                                    onClicked: root.insertCallingFrequencyQsyTag()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                spacing: 6

                                SmallButton {
                                    text: root.currentQsySlotLabel()
                                    implicitWidth: 62
                                    accent: root.amber
                                    enabled: root.selectedSessionId > 0 && root.qsySlots.length > 0
                                    tip: "Cycle QSY slot offset"
                                    onClicked: root.cycleQsySlot()
                                }

                                SmallButton {
                                    text: "QSY"
                                    implicitWidth: 44
                                    accent: root.amber
                                    enabled: root.selectedSessionId > 0 && root.qsySlots.length > 0
                                    tip: root.selectedSessionConnected
                                         ? "Sniff, then send QSY invitation"
                                         : root.currentQsySlotTip()
                                    onClicked: root.sendQsySlotInvite()
                                }

                                TextField {
                                    id: composeText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    placeholderText: qsTr("Message")
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    selectByMouse: true
                                    onAccepted: root.sendChatText()
                                }

                                SmallButton {
                                    text: "TX"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: root.selectedSessionConnected && composeText.text.trim().length > 0
                                             && (!ft2Link || !ft2Link.transportBusy)
                                    tip: "Transmit chat over RF audio"
                                    onClicked: root.sendChatText()
                                }
		                            }
		                        }
			                    }

			                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 48
                                spacing: 6

                                ColumnLayout {
                                    Layout.preferredWidth: 82
                                    Layout.minimumWidth: 82
                                    Layout.maximumWidth: 82
                                    Layout.fillHeight: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: "TYPE"
                                        font.family: root.mono
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: root.amber
                                        elide: Text.ElideRight
                                    }

                                    SmallButton {
                                        text: root.currentFormLabel()
                                        implicitWidth: 82
                                        accent: root.amber
                                        enabled: root.formTemplates.length > 0
                                        tip: "Cycle form template"
                                        onClicked: root.cycleFormTemplate()
                                    }
                                }

                                ColumnLayout {
                                    Layout.preferredWidth: 110
                                    Layout.minimumWidth: 96
                                    Layout.maximumWidth: 130
                                    Layout.fillHeight: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: "TO"
                                        font.family: root.mono
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: root.textSecondary
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        radius: 4
                                        color: Qt.rgba(1, 1, 1, 0.025)
                                        border.width: 1
                                        border.color: root.selectedSessionConnected
                                                      ? Qt.rgba(root.green.r, root.green.g, root.green.b, 0.55)
                                                      : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.35)

                                        Text {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            verticalAlignment: Text.AlignVCenter
                                            text: root.selectedRemoteCall.length > 0 ? root.selectedRemoteCall : "--"
                                            elide: Text.ElideRight
                                            color: root.selectedSessionConnected ? root.textPrimary : root.textSecondary
                                            font.family: root.mono
                                            font.pixelSize: 11
                                            font.bold: root.selectedSessionConnected
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: "FIELDS"
                                        font.family: root.mono
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: root.cyan
                                        elide: Text.ElideRight
                                    }

                                    TextField {
                                        id: formFieldsText
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.preferredHeight: 28
                                        placeholderText: qsTr("key=value; key=value")
                                        enabled: root.selectedSessionConnected
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 512
                                        selectByMouse: true
                                        onAccepted: root.armOrTransmitForm()
                                    }
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "FORM TX" : "ARM F"
                                    implicitWidth: 78
                                    Layout.alignment: Qt.AlignBottom
                                    accent: root.amber
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && formFieldsText.text.trim().length > 0
                                    tip: ft2Link && ft2Link.radioTxArmed ? "Transmit form" : "Arm form transmit"
                                    onClicked: root.armOrTransmitForm()
                                }
		                            }
		                        }
				                    }

				                    Item {
		                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                spacing: 6

                                Text {
                                    text: "TO"
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 10
                                }

                                Rectangle {
                                    Layout.preferredWidth: 110
                                    Layout.preferredHeight: 28
                                    radius: 4
                                    color: Qt.rgba(1, 1, 1, 0.025)
                                    border.width: 1
                                    border.color: root.selectedSessionConnected
                                                  ? Qt.rgba(root.green.r, root.green.g, root.green.b, 0.55)
                                                  : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.35)

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        verticalAlignment: Text.AlignVCenter
                                        text: root.selectedRemoteCall.length > 0 ? root.selectedRemoteCall : "--"
                                        elide: Text.ElideRight
                                        color: root.selectedSessionConnected ? root.textPrimary : root.textSecondary
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        font.bold: root.selectedSessionConnected
                                    }
                                }

                                Text {
                                    text: "FILE"
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 10
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    radius: 4
                                    color: Qt.rgba(1, 1, 1, 0.025)
                                    border.width: 1
                                    border.color: root.selectedFileName.length > 0
                                                  ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.60)
                                                  : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.35)

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        verticalAlignment: Text.AlignVCenter
                                        text: root.selectedFileName.length > 0
                                              ? root.selectedFileName
                                              : "No file selected"
                                        elide: Text.ElideMiddle
                                        color: root.selectedFileName.length > 0 ? root.textPrimary : root.textSecondary
                                        font.family: root.mono
                                        font.pixelSize: 11
                                    }
                                }

                                    Text {
                                        Layout.preferredWidth: 96
                                        horizontalAlignment: Text.AlignRight
                                    text: (root.selectedFileBinary ? "BIN " : "TXT ")
                                          + String(root.selectedFileBytes) + " / "
                                          + String(root.filePayloadLimitBytes) + " B"
                                    elide: Text.ElideRight
                                    color: root.selectedFileBytes > root.filePayloadLimitBytes
                                           ? root.red
                                           : root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 10
                                }

                                SmallButton {
                                    text: "LOAD TXT"
                                    implicitWidth: 78
                                    accent: root.cyan
                                    enabled: !root.fileTxPending()
                                             && !!bridge && typeof bridge.openFileDialog === "function"
                                             && typeof bridge.readTextFile === "function"
                                    tip: "Load a local text file, max 16 KiB"
                                    onClicked: root.loadFileText()
                                }

                                SmallButton {
                                    text: "LOAD BIN"
                                    implicitWidth: 78
                                    accent: root.amber
                                    enabled: !root.fileTxPending()
                                             && !!bridge && typeof bridge.openFileDialog === "function"
                                             && typeof bridge.readFileBytes === "function"
                                    tip: "Load a local binary file, max 16 KiB"
                                    onClicked: root.loadFileBytes()
                                }

                                SmallButton {
                                    text: "CLEAR"
                                    implicitWidth: 54
                                    accent: root.red
                                    enabled: !root.fileTxPending()
                                             && root.selectedFileName.length > 0
                                    tip: "Clear selected file"
                                    onClicked: root.clearSelectedFile()
                                }

                                SmallButton {
                                    text: root.fileTxButtonText()
                                    implicitWidth: root.fileTxPending() ? 116 : 88
                                    accent: root.cyan
                                    enabled: !!ft2Link && !root.fileTxPending()
                                             && root.selectedSessionConnected
                                             && root.selectedFileName.length > 0
                                             && root.selectedFileBytes > 0
                                             && root.selectedFileBytes <= root.filePayloadLimitBytes
                                    tip: root.fileTxPending()
                                         ? "File request accepted; waiting or transmitting automatically"
                                         : "Queue file and transmit automatically when ready"
                                    onClicked: root.armOrTransmitFile()
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 18
                                text: root.fileTransferStatus.length > 0
                                      ? root.fileTransferStatus
                                      : (root.selectedFileName.length > 0 ? root.selectedFilePath : "No file selected")
                                elide: Text.ElideMiddle
                                color: root.fileTransferStatus.indexOf("too large") >= 0
                                       || root.fileTransferStatus.indexOf("unavailable") >= 0
                                       || root.fileTransferStatus.indexOf("Cannot") >= 0
                                       ? root.red
                                       : root.textSecondary
                                font.family: root.mono
                                font.pixelSize: 10
			            }
			        }
			    }

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 48
                                spacing: 6

                                ColumnLayout {
                                    Layout.preferredWidth: 72
                                    Layout.minimumWidth: 72
                                    Layout.maximumWidth: 72
                                    Layout.fillHeight: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: "GROUP"
                                        font.family: root.mono
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: root.amber
                                        elide: Text.ElideRight
                                    }

                                    TextField {
                                        id: bulletinGroupText
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.preferredHeight: 28
                                        placeholderText: "ALL"
                                        text: root.bbsDefaultGroup
                                        enabled: root.selectedSessionConnected
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 16
                                        selectByMouse: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("BBS group. Empty means ALL.")
                                        ToolTip.delay: 450
                                        background: Rectangle {
                                            radius: 4
                                            color: bulletinGroupText.enabled
                                                   ? Qt.rgba(1, 1, 1, 0.045)
                                                   : Qt.rgba(1, 1, 1, 0.020)
                                            border.width: 1
                                            border.color: bulletinGroupText.activeFocus
                                                          ? root.amber
                                                          : Qt.rgba(root.amber.r, root.amber.g, root.amber.b, 0.45)
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.preferredWidth: 150
                                    Layout.minimumWidth: 118
                                    Layout.maximumWidth: 170
                                    Layout.fillHeight: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: "TITLE"
                                        font.family: root.mono
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: root.cyan
                                        elide: Text.ElideRight
                                    }

                                    TextField {
                                        id: bulletinTitleText
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.preferredHeight: 28
                                        placeholderText: qsTr("Bulletin title")
                                        enabled: root.selectedSessionConnected
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 64
                                        selectByMouse: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Short BBS subject/title.")
                                        ToolTip.delay: 450
                                        background: Rectangle {
                                            radius: 4
                                            color: bulletinTitleText.enabled
                                                   ? Qt.rgba(1, 1, 1, 0.045)
                                                   : Qt.rgba(1, 1, 1, 0.020)
                                            border.width: 1
                                            border.color: bulletinTitleText.activeFocus
                                                          ? root.cyan
                                                          : Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.45)
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: "MESSAGE"
                                        font.family: root.mono
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: root.green
                                        elide: Text.ElideRight
                                    }

                                    TextField {
                                        id: bulletinBodyText
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.preferredHeight: 28
                                        placeholderText: qsTr("BBS message body")
                                        enabled: root.selectedSessionConnected
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 1024
                                        selectByMouse: true
                                        onAccepted: root.armOrTransmitBulletin()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Text to store and send as the BBS bulletin body.")
                                        ToolTip.delay: 450
                                        background: Rectangle {
                                            radius: 4
                                            color: bulletinBodyText.enabled
                                                   ? Qt.rgba(1, 1, 1, 0.045)
                                                   : Qt.rgba(1, 1, 1, 0.020)
                                            border.width: 1
                                            border.color: bulletinBodyText.activeFocus
                                                          ? root.green
                                                          : Qt.rgba(root.green.r, root.green.g, root.green.b, 0.45)
                                        }
                                    }
                                }

                                SmallButton {
                                    Layout.alignment: Qt.AlignBottom
                                    text: ft2Link && ft2Link.radioTxArmed ? "BBS TX" : "ARM BBS"
                                    implicitWidth: 82
                                    implicitHeight: 28
                                    accent: root.amber
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && bulletinBodyText.text.trim().length > 0
                                    tip: ft2Link && ft2Link.radioTxArmed ? "Transmit BBS bulletin" : "Arm BBS transmit"
                                    onClicked: root.armOrTransmitBulletin()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                CompactCheck {
                                    text: "SERVER"
                                    checked: root.bbsFileServerEnabledSetting
                                    accent: root.green
                                    tip: "Enable BBS file server replies for BLR/BG requests"
                                    onToggled: function(nextChecked) {
                                        root.bbsFileServerEnabledSetting = nextChecked
                                        root.configureBbsFileServer()
                                    }
                                }

                                TextField {
                                    id: bbsServerFileNameText
                                    Layout.preferredWidth: 150
                                    Layout.minimumWidth: 90
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("server-file.txt")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    background: Rectangle {
                                        radius: 4
                                        color: Qt.rgba(1, 1, 1, 0.045)
                                        border.width: 1
                                        border.color: bbsServerFileNameText.activeFocus
                                                      ? root.cyan
                                                      : Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.42)
                                    }
                                }

                                TextField {
                                    id: bbsServerBodyText
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 120
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("BBS server file body")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    maximumLength: root.filePayloadLimitBytes
                                    onTextEdited: root.clearBbsServerBinarySelection()
                                    background: Rectangle {
                                        radius: 4
                                        color: Qt.rgba(1, 1, 1, 0.045)
                                        border.width: 1
                                        border.color: bbsServerBodyText.activeFocus
                                                      ? root.green
                                                      : Qt.rgba(root.green.r, root.green.g, root.green.b, 0.42)
                                    }
                                }

                                SmallButton {
                                    text: "LOAD TXT"
                                    implicitWidth: 68
                                    implicitHeight: 24
                                    accent: root.cyan
                                    tip: "Load a text file into the BBS file server composer"
                                    onClicked: root.loadBbsServerTextFile()
                                }

                                SmallButton {
                                    text: "LOAD BIN"
                                    implicitWidth: 68
                                    implicitHeight: 24
                                    accent: root.amber
                                    enabled: !!bridge && typeof bridge.openFileDialog === "function"
                                             && typeof bridge.readFileBytes === "function"
                                    tip: "Load a binary file into the BBS file server"
                                    onClicked: root.loadBbsServerBinaryFile()
                                }

                                SmallButton {
                                    text: "PUBLISH"
                                    implicitWidth: 72
                                    implicitHeight: 24
                                    accent: root.green
                                    enabled: !!ft2Link && bbsServerFileNameText.text.trim().length > 0
                                             && ((root.bbsServerBinary
                                                  && root.bbsServerContentBase64.length > 0)
                                                 || (!root.bbsServerBinary
                                                     && bbsServerBodyText.text.length > 0))
                                    tip: "Publish this file in the local BBS file server"
                                    onClicked: root.publishBbsServerFile()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                Text {
                                    text: "REQ"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: root.amber
                                }

                                TextField {
                                    id: bbsRequestFileText
                                    Layout.preferredWidth: 180
                                    Layout.minimumWidth: 100
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("remote-file.txt")
                                    enabled: root.selectedSessionConnected
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onAccepted: root.requestBbsFileByName("")
                                    background: Rectangle {
                                        radius: 4
                                        color: bbsRequestFileText.enabled
                                               ? Qt.rgba(1, 1, 1, 0.045)
                                               : Qt.rgba(1, 1, 1, 0.020)
                                        border.width: 1
                                        border.color: bbsRequestFileText.activeFocus
                                                      ? root.amber
                                                      : Qt.rgba(root.amber.r, root.amber.g, root.amber.b, 0.42)
                                    }
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "REQ LIST" : "ARM L"
                                    implicitWidth: 72
                                    implicitHeight: 24
                                    accent: root.amber
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                    tip: "Request remote BBS file list"
                                    onClicked: root.requestBbsFileList()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "REQ FILE" : "ARM F"
                                    implicitWidth: 72
                                    implicitHeight: 24
                                    accent: root.cyan
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && bbsRequestFileText.text.trim().length > 0
                                    tip: "Request a remote BBS file by name"
                                    onClicked: root.requestBbsFileByName("")
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.bbsServerStatus.length > 0
                                          ? root.bbsServerStatus
                                          : String(root.bbsFileServerState.line || "BBS file server OFF")
                                    elide: Text.ElideMiddle
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.bbsFileServerEnabledSetting ? root.green : root.textSecondary
                                }
                            }

                            ListView {
                                id: bbsSharedFileList
                                Layout.fillWidth: true
                                Layout.preferredHeight: 58
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.bbsSharedFiles
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    required property var modelData
                                    width: bbsSharedFileList.width
                                    height: 24
                                    radius: 4
                                    color: Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.035)
                                    border.width: 1
                                    border.color: Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.16)

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 36
                                            text: modelData.binary ? "BIN" : "TXT"
                                            font.family: root.mono
                                            font.pixelSize: 9
                                            font.bold: true
                                            color: modelData.binary ? root.amber : root.cyan
                                        }

                                        Text {
                                            Layout.preferredWidth: 160
                                            text: String(modelData.fileName || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.green
                                        }

                                        Text {
                                            Layout.preferredWidth: 70
                                            text: String(modelData.sizeBytes || 0) + " B"
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 9
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(modelData.preview || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 9
                                            color: root.textPrimary
                                        }

                                        SmallButton {
                                            text: "REQ"
                                            implicitWidth: 42
                                            implicitHeight: 20
                                            labelSize: 8
                                            accent: root.amber
                                            enabled: root.selectedSessionConnected
                                            tip: "Request this file name from the connected peer"
                                            onClicked: root.requestBbsFileByName(String(modelData.fileName || ""))
                                        }

                                        SmallButton {
                                            text: "DEL"
                                            implicitWidth: 40
                                            implicitHeight: 20
                                            labelSize: 8
                                            accent: root.red
                                            enabled: !!ft2Link
                                            tip: "Remove from local BBS file server"
                                            onClicked: root.removeBbsSharedFile(modelData)
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: bbsSharedFileList.count === 0
                                    text: qsTr("No BBS server files")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 6

                                Text {
                                    text: "GROUPS"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: root.textSecondary
                                }

                                ListView {
                                    id: bbsGroupListView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    orientation: ListView.Horizontal
                                    spacing: 5
                                    clip: true
                                    boundsBehavior: Flickable.StopAtBounds
                                    model: root.bbsGroupList
                                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                                    delegate: SmallButton {
                                        required property var modelData
                                        text: String(modelData || "ALL")
                                        implicitWidth: Math.max(44, Math.min(82, text.length * 9 + 18))
                                        implicitHeight: 22
                                        labelSize: 9
                                        checked: root.normalizeBbsGroup(bulletinGroupText.text) === text
                                        accent: checked ? root.green : root.cyan
                                        tip: "Use BBS group " + text
                                        onClicked: root.setBbsComposerGroup(text)
                                    }
                                }

                                SmallButton {
                                    text: qsTr("SAVE GROUP")
                                    implicitWidth: 86
                                    implicitHeight: 22
                                    labelSize: 9
                                    accent: root.green
                                    enabled: root.normalizeBbsGroupOrEmpty(bulletinGroupText.text).length > 0
                                    tip: "Save typed GROUP as a preset and make it default"
                                    onClicked: root.saveBbsComposerGroup()
                                }

                                SmallButton {
                                    text: root.bbsFilterLabel()
                                    implicitWidth: root.bbsGroupFilter.length > 0 ? 92 : 72
                                    implicitHeight: 22
                                    labelSize: 9
                                    checked: root.bbsGroupFilter.length > 0
                                    accent: root.bbsGroupFilter.length > 0 ? root.amber : root.textSecondary
                                    tip: "Cycle BBS list filter by group"
                                    onClicked: root.cycleBbsGroupFilter()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 20
                                spacing: 6

                                Text {
                                    text: qsTr("BBS BULLETINS")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.cyan
                                }

                                Text {
                                    text: String(root.filteredBulletins().length)
                                          + (root.bbsGroupFilter.length > 0
                                             ? ("/" + String(root.bulletins.length))
                                             : "")
                                          + " item"
                                          + (root.filteredBulletins().length === 1 ? "" : "s")
                                          + (root.bulletinUnreadCount > 0
                                             ? (" / " + root.bulletinUnreadCount + " unread")
                                             : "")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.bulletinUnreadCount > 0
                                           ? root.amber : root.textSecondary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.bbsGroupFilter.length > 0
                                          ? ("Showing group " + root.bbsGroupFilter)
                                          : (root.bulletins.length > 0
                                             ? "Incoming and outgoing BBS bulletins"
                                             : "No BBS bulletins")
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }

                                SmallButton {
                                    text: qsTr("MARK ALL READ")
                                    implicitWidth: 104
                                    accent: root.amber
                                    enabled: root.bulletinUnreadCount > 0
                                    tip: "Clear unread marker for all BBS bulletins"
                                    onClicked: root.markAllBulletinsRead()
                                }

                                SmallButton {
                                    text: "CLEAR"
                                    implicitWidth: 54
                                    accent: root.red
                                    enabled: !!ft2Link && root.bulletins.length > 0
                                    tip: "Clear BBS bulletin list"
                                    onClicked: root.clearBulletinList()
                                }
                            }

	                            ListView {
	                                id: bulletinList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
	                                model: root.filteredBulletins()
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: bulletinDelegate
                                    required property var modelData
                                    width: bulletinList.width
                                    height: 42
                                    radius: 4
                                    color: bulletinMouse.containsMouse ? root.rowHover
                                                                       : (bulletinDelegate.modelData.unread
                                                                          ? Qt.rgba(root.amber.r, root.amber.g, root.amber.b, 0.075)
                                                                          : Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.035))
                                    border.width: 1
                                    border.color: Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.16)

                                    MouseArea {
                                        id: bulletinMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 7

                                        Text {
                                            Layout.preferredWidth: 38
                                            text: bulletinDelegate.modelData.unread
                                                  ? "NEW"
                                                  : root.bulletinDirectionLabel(bulletinDelegate.modelData)
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: bulletinDelegate.modelData.unread
                                                   ? root.amber
                                                   : (root.bulletinDirectionLabel(bulletinDelegate.modelData) === "RX"
                                                      ? root.green : root.amber)
                                        }

                                        Text {
                                            Layout.preferredWidth: 64
                                            text: String(bulletinDelegate.modelData.group || "ALL")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.preferredWidth: 82
                                            text: root.bulletinPeer(bulletinDelegate.modelData)
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 128
                                            text: String(bulletinDelegate.modelData.title || "Bulletin")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.cyan
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(bulletinDelegate.modelData.body || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 104
                                            text: root.bulletinDate(bulletinDelegate.modelData)
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        SmallButton {
                                            text: bulletinDelegate.modelData.unread ? "READ" : "UNREAD"
                                            implicitWidth: 58
                                            accent: bulletinDelegate.modelData.unread ? root.amber : root.textSecondary
                                            visible: String(bulletinDelegate.modelData.direction || "") === "Incoming"
                                            enabled: visible && !!ft2Link
                                            tip: bulletinDelegate.modelData.unread
                                                 ? "Mark BBS bulletin as read"
                                                 : "Mark BBS bulletin as unread"
                                            onClicked: root.markBulletinRead(bulletinDelegate.modelData,
                                                                            !!bulletinDelegate.modelData.unread,
                                                                            true)
                                        }

                                        SmallButton {
                                            text: "COPY"
                                            implicitWidth: 52
                                            accent: root.cyan
                                            enabled: String(bulletinDelegate.modelData.body || "").length > 0
                                            tip: "Copy this BBS bulletin"
                                            onClicked: root.copyBulletin(bulletinDelegate.modelData)
                                        }
                                    }
                                }

	                                Text {
	                                    anchors.centerIn: parent
	                                    visible: bulletinList.count === 0
	                                    text: root.bbsGroupFilter.length > 0
	                                          ? ("No BBS bulletins in " + root.bbsGroupFilter)
	                                          : "No BBS bulletins"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 16
                                text: root.bulletinStatus.length > 0
                                      ? root.bulletinStatus
                                      : "GROUP = destination group, TITLE = bulletin subject, MESSAGE = body"
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 10
                                color: root.textSecondary
                            }
                        }
                    }

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                spacing: 6

                                TextField {
                                    id: broadcastText
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.preferredHeight: 32
                                    placeholderText: qsTr("Broadcast message")
                                    enabled: !root.broadcastTxPending()
                                    font.family: root.mono
                                    font.pixelSize: 12
                                    maximumLength: 256
                                    selectByMouse: true
                                    onAccepted: root.armOrTransmitBroadcast()
                                }

                                SmallButton {
                                    text: "QSY"
                                    implicitWidth: 48
                                    implicitHeight: 30
                                    accent: root.cyan
                                    enabled: !!ft2Link && root.currentDialFrequencyHz() > 0
                                    tip: "Prepare QSY broadcast from schedule or current dial"
                                    onClicked: root.prepareQsyBroadcast()
                                }

                                SmallButton {
                                    text: root.broadcastTxButtonText()
                                    implicitWidth: root.broadcastTxPending() ? 112 : 82
                                    implicitHeight: 30
                                    accent: root.alerts.length > 0 ? root.red : root.amber
                                    enabled: !!ft2Link && !root.broadcastTxPending()
                                             && broadcastText.text.trim().length > 0
                                    tip: root.broadcastTxPending()
                                         ? "Broadcast queued; waiting for a clear transmit opportunity"
                                         : "Queue broadcast and transmit automatically when ready"
                                    onClicked: root.armOrTransmitBroadcast()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                TextField {
                                    id: alertTagsText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    placeholderText: qsTr("Alert tags: WX, POTA, NET")
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 256
                                    selectByMouse: true
                                    Component.onCompleted: text = root.alertTagList.join(", ")
                                    onTextEdited: root.alertTagsDirty = true
                                    onActiveFocusChanged: {
                                        if (!activeFocus && !root.alertTagsDirty)
                                            text = root.alertTagList.join(", ")
                                    }
                                    onAccepted: root.saveAlertTags()
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Save custom alert tags"
                                    onClicked: root.saveAlertTags()
                                }

                                SmallButton {
                                    text: "CLR"
                                    implicitWidth: 44
                                    accent: root.red
                                    enabled: !!ft2Link && root.alertTagList.length > 0
                                    tip: "Clear custom alert tags"
                                    onClicked: root.clearAlertTags()
                                }

                                Text {
                                    Layout.preferredWidth: 90
                                    text: String(root.alertTagList.length) + " custom"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                spacing: 6

                                TextField {
                                    id: pathTargetText
                                    Layout.preferredWidth: 116
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("PATH CALL")
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 16
                                    selectByMouse: true
                                    onAccepted: root.armOrTransmitPathFinderRequest()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "PATH TX" : "PATH?"
                                    implicitWidth: 68
                                    implicitHeight: 24
                                    accent: root.cyan
                                    enabled: !!ft2Link && pathTargetText.text.trim().length > 0
                                    tip: "Send path finder request"
                                    onClicked: root.armOrTransmitPathFinderRequest()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "PATH TX" : "PATH!"
                                    implicitWidth: 68
                                    implicitHeight: 24
                                    accent: root.amber
                                    enabled: !!ft2Link && root.pathFinderCandidate() !== null
                                    tip: "Reply that target was heard recently"
                                    onClicked: root.armOrTransmitPathFinderResponse()
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.pathRelayLine()
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.pathRelayHint() ? root.amber : root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 22 : 0
                                Layout.minimumHeight: visible ? 22 : 0
                                Layout.maximumHeight: visible ? 22 : 0
                                visible: root.pathRelayHint() !== null
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: root.pathRelayLine()
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.pathRelayHint() ? root.amber : root.textSecondary
                                }

                                SmallButton {
                                    text: "MAIL"
                                    implicitWidth: 48
                                    accent: root.green
                                    enabled: root.pathRelayHint() !== null
                                    tip: "Use relay hint as mailbox target"
                                    onClicked: root.usePathRelayForMail()
                                }

                                SmallButton {
                                    text: "CALL"
                                    implicitWidth: 48
                                    accent: root.cyan
                                    enabled: root.pathRelayHint() !== null
                                    tip: "Start session with suggested relay"
                                    onClicked: root.callPathRelay()
                                }

                                SmallButton {
                                    readonly property var relayHint: root.pathRelayHint()
                                    text: "FWD"
                                    implicitWidth: 46
                                    accent: root.amber
                                    enabled: relayHint !== null && !!relayHint.readyToForward
                                    tip: "Prepare parked mail and call relay"
                                    onClicked: root.forwardPathRelay()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                CompactCheck {
                                    id: digipeaterEnableCheck
                                    text: "DIGI"
                                    accent: root.green
                                    checked: root.digipeaterEnabledSetting
                                    onToggled: function(nextChecked) {
                                        root.digipeaterEnabledSetting = nextChecked
                                        root.configureDigipeater()
                                    }
                                }

                                SmallButton {
                                    text: "H" + root.digipeaterMaxHopsSetting
                                    implicitWidth: 44
                                    implicitHeight: 24
                                    accent: root.amber
                                    tip: "Cycle maximum digipeater hops"
                                    onClicked: {
                                        root.digipeaterMaxHopsSetting = (root.digipeaterMaxHopsSetting + 1) % 4
                                        root.configureDigipeater()
                                    }
                                }

                                TextField {
                                    id: digipeaterTargetText
                                    Layout.preferredWidth: 100
                                    Layout.preferredHeight: 26
                                    placeholderText: "TO/ALL"
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 16
                                    selectByMouse: true
                                }

                                TextField {
                                    id: digipeaterMessageText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("Digipeater payload")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 96
                                    selectByMouse: true
                                    onAccepted: root.sendDigipeaterText()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "DIGI TX" : "ARM D"
                                    implicitWidth: 70
                                    implicitHeight: 24
                                    accent: root.cyan
                                    enabled: !!ft2Link && digipeaterMessageText.text.trim().length > 0
                                    tip: "Send an explicit digipeater frame"
                                    onClicked: {
                                        if (!ft2Link.radioTxArmed) {
                                            ft2Link.setRadioTxArmed(true)
                                            return
                                        }
                                        root.sendDigipeaterText()
                                    }
                                }

                                SmallButton {
                                    text: "CLR"
                                    implicitWidth: 42
                                    implicitHeight: 24
                                    accent: root.red
                                    enabled: root.digipeaterEvents.length > 0
                                    tip: "Clear digipeater event log"
                                    onClicked: root.clearDigipeaterLog()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 8

                                Text {
                                    text: qsTr("DIGIPEATER")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.cyan
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: String(root.digipeaterState.line || "Digi OFF")
                                          + (root.digipeaterEvents.length > 0
                                             ? (" / last " + String(root.digipeaterEvents[0].state || "--")
                                                + " " + String(root.digipeaterEvents[0].originCall || "--")
                                                + ">" + String(root.digipeaterEvents[0].targetCall || "--"))
                                             : "")
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.digipeaterEnabledSetting ? root.green : root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                Text {
                                    text: qsTr("ALERT CENTER")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.red
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: String(root.activeAlerts().length) + " active / "
                                          + String(root.archivedAlertCount()) + " archived"
                                          + (ft2Link && ft2Link.alertCount > 0
                                             ? (" / " + ft2Link.alertCount + " unread")
                                             : "")
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: ft2Link && ft2Link.alertCount > 0
                                           ? root.amber : root.textSecondary
                                }

                                CompactCheck {
                                    id: showArchivedAlertsCheck
                                    text: "ARCH"
                                    accent: root.amber
                                    onToggled: function(nextChecked) { showArchivedAlertsCheck.checked = nextChecked }
                                }

                                SmallButton {
                                    text: qsTr("CLEAR ARCH")
                                    implicitWidth: 86
                                    implicitHeight: 22
                                    labelSize: 9
                                    accent: root.red
                                    enabled: !!ft2Link && root.archivedAlertCount() > 0
                                    tip: "Remove archived alert records"
                                    onClicked: root.clearArchivedAlerts()
                                }
                            }

                            ListView {
                                id: alertCenterList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.visibleAlerts(showArchivedAlertsCheck.checked)
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: alertDelegate
                                    required property var modelData
                                    width: alertCenterList.width
                                    height: 32
                                    radius: 4
                                    color: alertMouse.containsMouse ? root.rowHover
                                           : (alertDelegate.modelData.unread
                                              ? Qt.rgba(root.red.r, root.red.g, root.red.b, 0.085)
                                              : Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.030))
                                    border.width: 1
                                    border.color: Qt.rgba(root.red.r, root.red.g, root.red.b, 0.18)

                                    MouseArea {
                                        id: alertMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 7
                                        anchors.rightMargin: 7
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 42
                                            text: alertDelegate.modelData.archived
                                                  ? "ARCH"
                                                  : (alertDelegate.modelData.unread ? "NEW" : "READ")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: alertDelegate.modelData.archived ? root.textSecondary
                                                   : (alertDelegate.modelData.unread ? root.red : root.green)
                                        }

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: String(alertDelegate.modelData.tag || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.preferredWidth: 68
                                            text: String(alertDelegate.modelData.fromCall || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(alertDelegate.modelData.text || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 94
                                            text: root.alertDate(alertDelegate.modelData)
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        SmallButton {
                                            text: alertDelegate.modelData.unread ? "READ" : "UNREAD"
                                            implicitWidth: 58
                                            accent: alertDelegate.modelData.unread ? root.green : root.textSecondary
                                            enabled: !!ft2Link && !alertDelegate.modelData.archived
                                            tip: alertDelegate.modelData.unread ? "Mark alert as read" : "Mark alert as unread"
                                            onClicked: root.markAlertItemRead(alertDelegate.modelData,
                                                                              !!alertDelegate.modelData.unread)
                                        }

                                        SmallButton {
                                            text: alertDelegate.modelData.archived ? "RESTORE" : "ARCH"
                                            implicitWidth: 66
                                            accent: alertDelegate.modelData.archived ? root.green : root.amber
                                            enabled: !!ft2Link
                                            tip: alertDelegate.modelData.archived ? "Restore alert" : "Archive alert"
                                            onClicked: root.archiveAlertItem(alertDelegate.modelData,
                                                                             !alertDelegate.modelData.archived)
                                        }

                                        SmallButton {
                                            text: "COPY"
                                            implicitWidth: 48
                                            accent: root.cyan
                                            enabled: String(alertDelegate.modelData.text || "").length > 0
                                            tip: "Copy alert text"
                                            onClicked: root.copyAlertItem(alertDelegate.modelData)
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: alertCenterList.count === 0
                                    text: showArchivedAlertsCheck.checked ? "No archived alerts" : "No active alerts"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
		                    }
	                    }

		                    Item {
		                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                spacing: 6

                                TextField {
                                    id: mailToText
                                    Layout.preferredWidth: 86
                                    placeholderText: root.selectedRemoteCall.length > 0 ? root.selectedRemoteCall : "TO"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 16
                                    selectByMouse: true
                                }

                                TextField {
                                    id: mailSubjectText
                                    Layout.preferredWidth: 150
                                    placeholderText: qsTr("Subject")
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 48
                                    selectByMouse: true
                                }

                                TextField {
                                    id: mailBodyText
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Mail body")
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 512
                                    selectByMouse: true
                                    onAccepted: root.armOrTransmitMailbox()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "MAIL TX" : "ARM M"
                                    implicitWidth: 72
                                    accent: root.green
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && mailBodyText.text.trim().length > 0
                                    tip: ft2Link && ft2Link.radioTxArmed ? "Transmit mailbox item" : "Arm mailbox transmit"
                                    onClicked: root.armOrTransmitMailbox()
                                }

                                SmallButton {
                                    text: "PARK"
                                    implicitWidth: 62
                                    accent: root.cyan
                                    enabled: !!ft2Link
                                             && mailToText.text.trim().length > 0
                                             && mailBodyText.text.trim().length > 0
                                    tip: "Park mail for relay notification"
                                    onClicked: root.parkMailboxText()
                                }

                                SmallButton {
                                    text: root.relayMailboxButtonText()
                                    implicitWidth: 78
                                    accent: root.amber
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && root.relayMailboxCandidate() !== null
                                    tip: "Relay parked mail for this session"
                                    onClicked: root.armOrTransmitRelayMailbox()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                spacing: 6

                                CompactCheck {
                                    id: mailUrgentCheck
                                    text: "URG"
                                    accent: root.amber
                                    onToggled: function(nextChecked) { mailUrgentCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: mailEmcommCheck
                                    text: "EMC"
                                    accent: root.red
                                    onToggled: function(nextChecked) { mailEmcommCheck.checked = nextChecked }
                                }

	                                Text {
	                                    Layout.fillWidth: true
	                                    text: ft2Link
	                                          ? ("MAILBOX CENTER  "
	                                             + String(root.mailboxCenterState.summary || "")
	                                             + "  FLAGS "
	                                             + (root.mailboxCenterState.parkingEnabled ? "PARK" : "NO-PARK")
	                                             + "/"
	                                             + (root.mailboxCenterState.peekingEnabled ? "PEEK" : "NO-PEEK"))
	                                          : "MAILBOX CENTER --"
	                                    elide: Text.ElideRight
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    font.bold: ft2Link && (ft2Link.mailboxUnreadCount > 0 || ft2Link.relayQueueCount > 0)
                                    color: ft2Link && ft2Link.mailboxUnreadCount > 0 ? root.green
	                                           : (ft2Link && ft2Link.relayQueueCount > 0 ? root.amber : root.textSecondary)
	                                }

	                                SmallButton {
	                                    text: ft2Link && ft2Link.vmailParkingEnabled ? "PARK ON" : "PARK OFF"
	                                    implicitWidth: 70
	                                    labelSize: 9
	                                    accent: ft2Link && ft2Link.vmailParkingEnabled ? root.green : root.red
	                                    enabled: !!ft2Link && typeof ft2Link.configureVmailParking === "function"
	                                    tip: "Toggle inbound relay mailbox parking"
	                                    onClicked: {
	                                        ft2Link.configureVmailParking(!(ft2Link && ft2Link.vmailParkingEnabled))
	                                        refreshMailbox()
	                                    }
	                                }

	                                SmallButton {
	                                    text: ft2Link && ft2Link.parkedVmailPeekingEnabled ? "PEEK ON" : "PEEK OFF"
	                                    implicitWidth: 70
	                                    labelSize: 9
	                                    accent: ft2Link && ft2Link.parkedVmailPeekingEnabled ? root.green : root.red
	                                    enabled: !!ft2Link && typeof ft2Link.configureParkedVmailPeeking === "function"
	                                    tip: "Toggle parked mailbox peek replies"
	                                    onClicked: {
	                                        ft2Link.configureParkedVmailPeeking(!(ft2Link && ft2Link.parkedVmailPeekingEnabled))
	                                        refreshMailbox()
	                                    }
	                                }

	                                SmallButton {
	                                    text: "COPY"
                                    implicitWidth: 52
                                    accent: root.cyan
                                    enabled: !!ft2Link && root.mailbox.length > 0
                                    tip: "Copy printable mailbox export"
                                    onClicked: root.copyMailboxText()
                                }

                                SmallButton {
                                    text: "RLYQ"
                                    implicitWidth: 48
                                    accent: root.amber
                                    enabled: !!ft2Link && root.relayQueue.length > 0
                                    tip: "Copy relay queue export"
                                    onClicked: root.copyRelayQueueText()
                                }

                                SmallButton {
                                    text: "READ"
                                    implicitWidth: 48
                                    accent: root.green
                                    enabled: !!ft2Link && ft2Link.mailboxUnreadCount > 0
                                    tip: "Mark all incoming mail as read"
                                    onClicked: root.markAllMailboxRead()
                                }

                                SmallButton {
                                    text: "CLEAR"
                                    implicitWidth: 54
                                    accent: root.red
                                    enabled: !!ft2Link && root.mailbox.length > 0
                                    tip: "Clear mailbox list"
                                    onClicked: root.clearMailboxList()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 5

                                Text {
                                    text: "RELAY"
                                    Layout.preferredWidth: 48
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: root.relayGuideColor()
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.relayGuideLine()
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.relayGuideColor()
                                }

                                SmallButton {
                                    text: qsTr("USE PATH")
                                    implicitWidth: 72
                                    labelSize: 9
                                    accent: root.cyan
                                    enabled: root.pathRelayHint() !== null
                                    tip: "Fill mail target from the current path relay hint"
                                    onClicked: root.usePathRelayForMail()
                                }

                                SmallButton {
                                    text: qsTr("CALL RLY")
                                    implicitWidth: 72
                                    labelSize: 9
                                    accent: root.amber
                                    enabled: root.pathRelayHint() !== null
                                    tip: "Call the relay station suggested by PATH"
                                    onClicked: root.callPathRelay()
                                }

                                SmallButton {
                                    text: root.relayMailboxButtonText()
                                    implicitWidth: 74
                                    labelSize: 9
                                    accent: root.green
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && root.relayMailboxCandidate() !== null
                                    tip: "Transmit the next parked relay item through this session"
                                    onClicked: root.armOrTransmitRelayMailbox()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 5

                                CompactCheck {
                                    text: "GW"
                                    checked: root.emailGatewayEnabled
                                    accent: root.green
                                    onToggled: function(nextChecked) { root.emailGatewayEnabled = nextChecked }
                                }

                                TextField {
                                    Layout.preferredWidth: 150
                                    Layout.preferredHeight: 22
                                    text: root.emailGatewayHost
                                    placeholderText: qsTr("smtp.host")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onEditingFinished: root.emailGatewayHost = text.trim()
                                    onAccepted: root.emailGatewayHost = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 22
                                    text: String(root.emailGatewayPort)
                                    placeholderText: "587"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    horizontalAlignment: TextInput.AlignHCenter
                                    validator: IntValidator { bottom: 1; top: 65535 }
                                    onEditingFinished: {
                                        var value = Number(text)
                                        if (isFinite(value))
                                            root.emailGatewayPort = Math.max(1, Math.min(65535, Math.round(value)))
                                        text = String(root.emailGatewayPort)
                                    }
                                    onAccepted: {
                                        var value = Number(text)
                                        if (isFinite(value))
                                            root.emailGatewayPort = Math.max(1, Math.min(65535, Math.round(value)))
                                        text = String(root.emailGatewayPort)
                                    }
                                }

                                SmallButton {
                                    text: root.emailGatewaySecurity()
                                    implicitWidth: 70
                                    accent: root.textSecondary
                                    tip: "Cycle SMTP security"
                                    onClicked: root.cycleEmailGatewaySecurity()
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 22
                                    text: root.emailGatewayUsername
                                    placeholderText: qsTr("SMTP user")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onEditingFinished: root.emailGatewayUsername = text.trim()
                                    onAccepted: root.emailGatewayUsername = text.trim()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 5

                                TextField {
                                    Layout.preferredWidth: 185
                                    Layout.preferredHeight: 22
                                    text: root.emailGatewayFrom
                                    placeholderText: qsTr("From email")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onEditingFinished: root.emailGatewayFrom = text.trim()
                                    onAccepted: root.emailGatewayFrom = text.trim()
                                }

                                TextField {
                                    id: emailGatewayPasswordText
                                    Layout.preferredWidth: 150
                                    Layout.preferredHeight: 22
                                    placeholderText: qsTr("SMTP password")
                                    echoMode: TextInput.Password
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onAccepted: root.saveEmailGatewayPassword()
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 46
                                    accent: root.green
                                    enabled: !!bridge && emailGatewayPasswordText.text.length > 0
                                    tip: "Save SMTP password in secure storage"
                                    onClicked: root.saveEmailGatewayPassword()
                                }

                                SmallButton {
                                    text: "CLR"
                                    implicitWidth: 38
                                    accent: root.red
                                    enabled: !!bridge
                                    tip: "Clear saved SMTP password"
                                    onClicked: root.clearEmailGatewayPassword()
                                }

                                SmallButton {
                                    text: "TEST"
                                    implicitWidth: 46
                                    accent: root.amber
                                    enabled: !!bridge && root.emailGatewayEnabled
                                             && root.emailGatewayHost.trim().length > 0
                                    tip: "Test SMTP connect, TLS and authentication without sending mail"
                                    onClicked: root.testEmailGateway()
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.emailGatewayStatus.length > 0
                                          ? root.emailGatewayStatus
                                          : "SMTP gateway idle"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.emailGatewayStatus.indexOf("error") >= 0
                                           || root.emailGatewayStatus.indexOf("Failed") >= 0
                                           ? root.red
                                           : root.textSecondary
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ListView {
                                id: mailboxList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
	                                model: root.mailboxCenterState.rows || root.mailbox
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Item {
                                    id: mailDelegate
                                    required property var modelData
	                                    readonly property string mailDirection: String(modelData.direction || "")
	                                    readonly property string mailState: String(modelData.state || "--")
	                                    readonly property string mailRole: String(modelData.role || modelData.direction || "MAIL")
	                                    readonly property string peer: mailDirection === "Incoming"
	                                                                   ? String(modelData.fromCall || "--")
	                                                                   : String(modelData.toCall || "--")
	                                    readonly property string priority: String(modelData.priority || "NORMAL")
	                                    width: mailboxList.width
	                                    height: 34

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 5

	                                        Text {
	                                            Layout.preferredWidth: 64
	                                            text: mailDelegate.mailRole
	                                            elide: Text.ElideRight
	                                            font.family: root.mono
	                                            font.pixelSize: 10
	                                            font.bold: true
	                                            color: mailDelegate.modelData.relayActive ? root.amber
	                                                                                      : (mailDelegate.modelData.unread ? root.green : root.textSecondary)
	                                        }

	                                        Text {
	                                            Layout.preferredWidth: 78
	                                            text: mailDelegate.mailState
	                                            elide: Text.ElideRight
	                                            font.family: root.mono
	                                            font.pixelSize: 10
	                                            color: mailDelegate.modelData.unread ? root.green
	                                                                                 : (mailDelegate.mailState === "Read" ? root.textSecondary : root.amber)
	                                        }

	                                        Text {
	                                            Layout.preferredWidth: 68
	                                            text: mailDelegate.priority === "NORMAL" ? "--" : mailDelegate.priority
	                                            elide: Text.ElideRight
	                                            font.family: root.mono
	                                            font.pixelSize: 10
	                                            font.bold: mailDelegate.priority !== "NORMAL"
	                                            color: mailDelegate.modelData.urgent ? root.red
	                                                                                 : (mailDelegate.modelData.emcomm ? root.amber : root.textSecondary)
	                                        }

	                                        Text {
	                                            Layout.preferredWidth: 94
	                                            text: mailDelegate.modelData.relayActive
	                                                  ? ("RLY "
	                                                     + (String(mailDelegate.modelData.suggestedRelayCall || "").length > 0
	                                                        ? String(mailDelegate.modelData.suggestedRelayCall)
	                                                        : String(mailDelegate.modelData.relayViaCall || "--")))
	                                                  : mailDelegate.peer
	                                            elide: Text.ElideRight
	                                            font.family: root.mono
	                                            font.pixelSize: 10
	                                            color: mailDelegate.modelData.canRelayNow ? root.green
	                                                                                     : (mailDelegate.modelData.relayActive ? root.amber : root.textPrimary)
	                                        }

	                                        Text {
	                                            Layout.fillWidth: true
	                                            text: String(mailDelegate.modelData.summaryLine || (String(mailDelegate.modelData.subject || "") + "  " + String(mailDelegate.modelData.body || "")))
	                                            elide: Text.ElideRight
	                                            font.family: root.mono
	                                            font.pixelSize: 10
	                                            color: root.textSecondary
	                                        }

	                                        SmallButton {
	                                            text: "READY"
	                                            implicitWidth: 52
	                                            labelSize: 9
	                                            accent: root.amber
	                                            enabled: !!ft2Link && mailDelegate.modelData.relayActive
	                                                     && !mailDelegate.modelData.pendingRelay
	                                                     && mailDelegate.mailState !== "Relay ready"
	                                            tip: "Mark parked mail ready for relay"
	                                            onClicked: root.markMailboxItemRelayReady(mailDelegate.modelData)
	                                        }

	                                        SmallButton {
	                                            text: "PEND"
	                                            implicitWidth: 46
	                                            labelSize: 9
	                                            accent: root.cyan
	                                            enabled: !!ft2Link && mailDelegate.modelData.relayActive
	                                                     && !mailDelegate.modelData.pendingRelay
	                                            tip: "Mark relay item pending through current or suggested station"
	                                            onClicked: root.markMailboxItemPendingRelay(mailDelegate.modelData)
	                                        }

	                                        SmallButton {
	                                            text: "RELAY"
	                                            implicitWidth: 52
	                                            labelSize: 9
	                                            accent: root.green
	                                            enabled: !!ft2Link && root.selectedSessionConnected
	                                                     && mailDelegate.modelData.relayActive
	                                                     && mailDelegate.modelData.canRelayNow
	                                            tip: "Transmit this parked relay item on the current session"
	                                            onClicked: root.transmitMailboxCenterRelay(mailDelegate.modelData)
	                                        }

	                                        SmallButton {
	                                            text: "CANCEL"
	                                            implicitWidth: 58
	                                            labelSize: 9
	                                            accent: root.red
	                                            enabled: !!ft2Link && mailDelegate.modelData.relayActive
	                                                     && (mailDelegate.modelData.pendingRelay
	                                                         || mailDelegate.mailState === "Relay ready")
	                                            tip: "Return relay item to parked state"
	                                            onClicked: root.cancelMailboxItemRelay(mailDelegate.modelData)
	                                        }

                                        SmallButton {
                                            text: mailDelegate.mailState === "Read" ? "NEW" : "READ"
                                            implicitWidth: 48
                                            accent: root.green
                                            enabled: !!ft2Link && mailDelegate.mailDirection === "Incoming"
                                            tip: mailDelegate.mailState === "Read" ? "Mark mail as new" : "Mark mail as read"
                                            onClicked: root.markMailboxItemRead(mailDelegate.modelData, mailDelegate.mailState !== "Read")
                                        }

                                        SmallButton {
                                            text: "EMAIL"
                                            implicitWidth: 54
                                            accent: root.cyan
                                            enabled: !!ft2Link
                                            tip: "Open as email draft or copy EML"
                                            onClicked: root.openMailboxEmail(mailDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: root.emailGatewayItemState(mailDelegate.modelData) === "Sent"
                                                  ? "SENT" : "SMTP"
                                            implicitWidth: 48
                                            accent: root.emailGatewayItemState(mailDelegate.modelData) === "Failed"
                                                    ? root.red
                                                    : (root.emailGatewayItemState(mailDelegate.modelData) === "Sent"
                                                       ? root.green
                                                       : root.cyan)
                                            enabled: !!bridge && root.emailGatewayEnabled
                                            tip: "Send this VMail through configured SMTP gateway"
                                            onClicked: root.sendMailboxGatewayEmail(mailDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: "EML"
                                            implicitWidth: 42
                                            accent: root.amber
                                            enabled: !!ft2Link
                                            tip: "Save mailbox item as .eml"
                                            onClicked: root.saveMailboxEml(mailDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: "DEL"
                                            implicitWidth: 42
                                            accent: root.red
                                            enabled: !!ft2Link
                                            tip: "Delete mailbox item"
                                            onClicked: root.deleteMailboxItem(mailDelegate.modelData)
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: mailboxList.count === 0
                                    text: qsTr("No mail")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        clip: true
                        Flickable {
                            id: infoFlick
                            anchors.fill: parent
                            clip: true
                            contentWidth: width
                            contentHeight: Math.max(height + 1,
                                                    infoColumn.y + Math.max(infoColumn.height,
                                                                            infoColumn.implicitHeight,
                                                                            infoColumn.childrenRect.height) + 28)
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            ColumnLayout {
                                id: infoColumn
                                width: infoFlick.width
                                spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                TextField {
                                    Layout.preferredWidth: 92
                                    Layout.preferredHeight: 26
                                    text: root.profileName
                                    placeholderText: "NAME"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 32
                                    selectByMouse: true
                                    onEditingFinished: root.profileName = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 116
                                    Layout.preferredHeight: 26
                                    text: root.profileQth
                                    placeholderText: "QTH"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 64
                                    selectByMouse: true
                                    onEditingFinished: root.profileQth = text.trim()
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    text: root.profileEmail
                                    placeholderText: "EMAIL"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
                                    onEditingFinished: root.profileEmail = text.trim()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                TextField {
                                    Layout.preferredWidth: 132
                                    Layout.preferredHeight: 26
                                    text: root.profileRig
                                    placeholderText: "RIG"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
                                    onEditingFinished: root.profileRig = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 132
                                    Layout.preferredHeight: 26
                                    text: root.profileAntenna
                                    placeholderText: "ANT"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
                                    onEditingFinished: root.profileAntenna = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 70
                                    Layout.preferredHeight: 26
                                    text: root.profilePower
                                    placeholderText: "PWR"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 32
                                    selectByMouse: true
                                    onEditingFinished: root.profilePower = text.trim()
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    text: root.profileIce
                                    placeholderText: "ICE"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 128
                                    selectByMouse: true
                                    onEditingFinished: root.profileIce = text.trim()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    text: root.profileGps
                                    placeholderText: "GPS"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
	                                    onEditingFinished: root.profileGps = text.trim()
	                            }
	                        }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                CompactCheck {
                                    id: awayCheck
                                    text: "AWAY"
                                    enabled: !!ft2Link
                                    accent: root.amber
                                    onToggled: function(nextChecked) { awayCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: awayQsyCheck
                                    text: "QSY"
                                    enabled: !!ft2Link && awayCheck.checked
                                    accent: root.cyan
                                    onToggled: function(nextChecked) { awayQsyCheck.checked = nextChecked }
                                }

                                TextField {
                                    id: awayMessageText
                                    Layout.preferredWidth: 176
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("Away message")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 240
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    onAccepted: root.savePresence()
                                }

                                CompactCheck {
                                    id: welcomeCheck
                                    text: "WELCOME"
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { welcomeCheck.checked = nextChecked }
                                }

                                TextField {
                                    id: welcomeMessageText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    placeholderText: qsTr("Welcome message")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 240
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    onAccepted: root.savePresence()
                                }

                                CompactCheck {
                                    id: autoReplyCheck
                                    text: qsTr("AUTO REPLY")
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { autoReplyCheck.checked = nextChecked }
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Save away and welcome settings"
                                    onClicked: root.savePresence()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                CompactCheck {
                                    id: autoAwayCheck
                                    text: qsTr("AUTO AWAY")
                                    enabled: !!ft2Link
                                    accent: root.amber
                                    onToggled: function(nextChecked) { autoAwayCheck.checked = nextChecked }
                                }

                                TextField {
                                    id: autoAwayMinutesText
                                    Layout.preferredWidth: 58
                                    Layout.preferredHeight: 26
                                    text: "10"
                                    placeholderText: "MIN"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 3
                                    selectByMouse: true
                                    enabled: !!ft2Link && autoAwayCheck.checked
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: IntValidator { bottom: 1; top: 240 }
                                    horizontalAlignment: TextInput.AlignHCenter
                                    onAccepted: root.savePresence()
                                }

                                Text {
                                    Layout.preferredWidth: 96
                                    text: root.presenceState.autoAwayActive ? "AUTO AWAY ACTIVE" : "AUTO AWAY IDLE"
                                    color: root.presenceState.autoAwayActive ? root.amber : root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                Text {
                                    text: qsTr("CALL ID")
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    Layout.preferredWidth: 54
                                }

                                TextField {
                                    id: callIdIntervalText
                                    Layout.preferredWidth: 58
                                    Layout.preferredHeight: 26
                                    text: "0"
                                    placeholderText: "MIN"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 3
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: IntValidator { bottom: 0; top: 240 }
                                    horizontalAlignment: TextInput.AlignHCenter
                                    onAccepted: root.saveQsoAutomation()
                                }

                                Text {
                                    text: qsTr("AUTO DISC")
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    Layout.preferredWidth: 66
                                }

                                TextField {
                                    id: autoDisconnectText
                                    Layout.preferredWidth: 58
                                    Layout.preferredHeight: 26
                                    text: "0"
                                    placeholderText: "MIN"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 3
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: IntValidator { bottom: 0; top: 240 }
                                    horizontalAlignment: TextInput.AlignHCenter
                                    onAccepted: root.saveQsoAutomation()
                                }

                                CompactCheck {
                                    id: incomingPingCheck
                                    text: qsTr("PING RX")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { incomingPingCheck.checked = nextChecked }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: String(root.qsoAutomationState.callIdText || "")
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                Text {
                                    text: "PRIV"
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    Layout.preferredWidth: 36
                                }

                                SmallButton {
                                    text: "OPEN"
                                    implicitWidth: 54
                                    checked: root.privacyPresetName() === "OPEN"
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Publish last-heard, connections, VMail hints, SNR and info replies"
                                    onClicked: root.applyPrivacyPreset("OPEN")
                                }

                                SmallButton {
                                    text: "CONTROL"
                                    implicitWidth: 76
                                    checked: root.privacyPresetName() === "CONTROL"
                                    accent: root.amber
                                    enabled: !!ft2Link
                                    tip: "Keep operations active but hide last-connections and parked VMail peek"
                                    onClicked: root.applyPrivacyPreset("CONTROL")
                                }

                                SmallButton {
                                    text: "QUIET"
                                    implicitWidth: 58
                                    checked: root.privacyPresetName() === "QUIET"
                                    accent: root.red
                                    enabled: !!ft2Link
                                    tip: "Disable automatic disclosure, inbound pings and relay parking"
                                    onClicked: root.applyPrivacyPreset("QUIET")
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.privacySummaryText()
                                    color: root.privacyPresetName() === "QUIET" ? root.red
                                           : (root.privacyPresetName() === "CONTROL" ? root.amber : root.textSecondary)
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                CompactCheck {
                                    id: lastHeardPeekingCheck
                                    text: qsTr("LH PEEK")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { lastHeardPeekingCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: snrReportCheck
                                    text: qsTr("SNR TX")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { snrReportCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: verboseSnrAcceptCheck
                                    text: qsTr("VSNR OK")
                                    checked: false
                                    enabled: !!ft2Link
                                    accent: root.cyan
                                    onToggled: function(nextChecked) { verboseSnrAcceptCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: lastConnectionsPeekingCheck
                                    text: qsTr("LC PEEK")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { lastConnectionsPeekingCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: parkedVmailPeekingCheck
                                    text: qsTr("VM PEEK")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { parkedVmailPeekingCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: vmailParkingCheck
                                    text: qsTr("VM PARK")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { vmailParkingCheck.checked = nextChecked }
                                }

                                CompactCheck {
                                    id: infoInquireCheck
                                    text: qsTr("INFO REQ")
                                    checked: true
                                    enabled: !!ft2Link
                                    accent: root.green
                                    onToggled: function(nextChecked) { infoInquireCheck.checked = nextChecked }
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Save inquiry, peeking and privacy toggles"
                                    onClicked: root.saveInquiryPrivacy()
                                }

		                                Item {
		                                    Layout.fillWidth: true
		                                }
		                            }

	                            RowLayout {
	                                Layout.fillWidth: true
	                                Layout.preferredHeight: 28
	                                spacing: 6

	                                Text {
	                                    text: "INQUIRY"
	                                    color: root.cyan
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    font.bold: true
	                                    Layout.preferredWidth: 62
	                                }

	                                TextField {
	                                    id: inquiryPreviewCallText
	                                    Layout.preferredWidth: 112
	                                    Layout.preferredHeight: 24
	                                    text: root.inquiryPreviewCall
	                                    placeholderText: root.selectedRemoteCall.length > 0
	                                                     ? root.selectedRemoteCall
	                                                     : "CALL"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 16
	                                    selectByMouse: true
	                                    enabled: !!ft2Link
	                                    onAccepted: {
	                                        root.inquiryPreviewCall = text.trim().toUpperCase()
	                                        root.refreshInquiryPreview()
	                                    }
	                                    onEditingFinished: {
	                                        root.inquiryPreviewCall = text.trim().toUpperCase()
	                                        root.refreshInquiryPreview()
	                                    }
	                                }

	                                SmallButton {
	                                    text: "PREVIEW"
	                                    implicitWidth: 72
	                                    accent: root.cyan
	                                    enabled: !!ft2Link
	                                    tip: "Preview automatic replies for this callsign"
	                                    onClicked: {
	                                        root.inquiryPreviewCall = inquiryPreviewCallText.text.trim().toUpperCase()
	                                        root.refreshInquiryPreview()
	                                    }
	                                }

	                                Text {
	                                    Layout.fillWidth: true
	                                    text: String(root.inquiryPreviewState.summary
	                                                 || root.privacyPanelState.inquirySummary
	                                                 || "")
	                                    color: root.inquiryPreviewState.blocked ? root.red : root.textSecondary
	                                    font.family: root.mono
	                                    font.pixelSize: 9
	                                    elide: Text.ElideRight
	                                }

	                                SmallButton {
	                                    text: "PANEL"
	                                    implicitWidth: 58
	                                    accent: root.amber
	                                    enabled: !!ft2Link
	                                    tip: "Refresh privacy panel"
	                                    onClicked: {
	                                        root.refreshPrivacyPanel()
	                                        root.refreshInquiryPreview()
	                                    }
	                                }
	                            }

	                            ListView {
	                                id: inquiryPreviewList
	                                Layout.fillWidth: true
	                                Layout.preferredHeight: 74
	                                clip: true
	                                spacing: 3
	                                boundsBehavior: Flickable.StopAtBounds
	                                model: root.inquiryPreviewState.rows || root.privacyPanelState.exposures || []
	                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

	                                delegate: Rectangle {
	                                    required property var modelData
	                                    width: inquiryPreviewList.width
	                                    height: 22
	                                    radius: 4
	                                    color: Qt.rgba(1, 1, 1, 0.025)
	                                    border.width: 1
	                                    border.color: modelData.status === "SHARE"
	                                                  ? Qt.rgba(root.green.r, root.green.g, root.green.b, 0.38)
	                                                  : (modelData.status === "BLOCK"
	                                                     ? Qt.rgba(root.red.r, root.red.g, root.red.b, 0.42)
	                                                     : Qt.rgba(root.amber.r, root.amber.g, root.amber.b, 0.32))

	                                    RowLayout {
	                                        anchors.fill: parent
	                                        anchors.leftMargin: 7
	                                        anchors.rightMargin: 7
	                                        spacing: 7

	                                        Text {
	                                            Layout.preferredWidth: 46
	                                            text: String(modelData.status || "--")
	                                            color: modelData.status === "SHARE" ? root.green
	                                                   : (modelData.status === "BLOCK" ? root.red : root.amber)
	                                            font.family: root.mono
	                                            font.pixelSize: 9
	                                            font.bold: true
	                                            elide: Text.ElideRight
	                                        }

	                                        Text {
	                                            Layout.preferredWidth: 94
	                                            text: String(modelData.label || modelData.key || "")
	                                            color: root.textPrimary
	                                            font.family: root.mono
	                                            font.pixelSize: 9
	                                            elide: Text.ElideRight
	                                        }

	                                        Text {
	                                            Layout.preferredWidth: 80
	                                            text: String(modelData.request || "")
	                                            color: root.cyan
	                                            font.family: root.mono
	                                            font.pixelSize: 9
	                                            elide: Text.ElideRight
	                                        }

	                                        Text {
	                                            Layout.fillWidth: true
	                                            text: String(modelData.reply || modelData.detail || "")
	                                            color: root.textSecondary
	                                            font.family: root.mono
	                                            font.pixelSize: 9
	                                            elide: Text.ElideRight
	                                        }
	                                    }
	                                }
	                            }

	                            Item {
	                                Layout.fillWidth: true
	                                Layout.preferredHeight: 20
	                            }
		                        }
			                    }
			                }

		                    Item {
	                        ColumnLayout {
	                            anchors.fill: parent
	                            spacing: 4

                            RowLayout {
                                visible: root.toolPageIndex === 7
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 28 : 0
                                Layout.minimumHeight: visible ? 28 : 0
                                Layout.maximumHeight: visible ? 28 : 0
                                spacing: 5

	                                TextField {
	                                    id: contactCallText
	                                    Layout.preferredWidth: 82
	                                    placeholderText: "CALL"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 16
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactGridText
	                                    Layout.preferredWidth: 66
	                                    placeholderText: "GRID"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 12
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactNameText
	                                    Layout.preferredWidth: 116
	                                    placeholderText: "NAME"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 48
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactTagText
	                                    Layout.preferredWidth: 58
	                                    placeholderText: "TAG"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 16
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactCommentText
	                                    Layout.fillWidth: true
	                                    placeholderText: qsTr("Comment")
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 240
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                SmallButton {
	                                    text: "SAVE"
	                                    implicitWidth: 52
	                                    accent: root.green
	                                    enabled: !!ft2Link && contactCallText.text.trim().length > 0
	                                    tip: "Save contact details"
	                                    onClicked: root.saveContactDetails()
	                                }

	                                SmallButton {
	                                    text: "CLR"
	                                    implicitWidth: 42
	                                    accent: root.textSecondary
	                                    tip: "Clear editor"
	                                    onClicked: root.clearContactDetailsEditor()
	                                }
	                            }

			                            RowLayout {
			                                visible: root.toolPageIndex === 7
			                                Layout.fillWidth: true
			                                Layout.fillHeight: visible
			                                spacing: 6

		                                ListView {
		                                    id: contactList
		                                    Layout.preferredWidth: 430
		                                    Layout.fillHeight: true
		                                    clip: true
		                                    spacing: 2
		                                    boundsBehavior: Flickable.StopAtBounds
		                                    model: root.contactHistory
		                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

		                                    delegate: Rectangle {
		                                        id: contactDelegate
		                                        required property var modelData
		                                        width: contactList.width
		                                        height: 24
		                                        radius: 4
		                                        color: root.selectedContactCall === String(modelData.call || "")
		                                               ? root.rowSelect
		                                               : (contactMouse.containsMouse ? root.rowHover : "transparent")
		                                        border.color: root.selectedContactCall === String(modelData.call || "")
		                                                      ? root.cyan
		                                                      : "transparent"
		                                        border.width: 1

		                                        RowLayout {
		                                            anchors.fill: parent
		                                            anchors.leftMargin: 6
		                                            anchors.rightMargin: 6
		                                            spacing: 6

		                                            Text {
		                                                Layout.preferredWidth: 74
		                                                text: String(contactDelegate.modelData.call || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                font.bold: true
		                                                color: root.cyan
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 46
		                                                text: String(contactDelegate.modelData.tag || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                font.bold: text.length > 0
		                                                color: text.length > 0 ? root.amber : root.textSecondary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 54
		                                                text: String(contactDelegate.modelData.locator || "--")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 92
		                                                text: String(contactDelegate.modelData.name || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textPrimary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 70
		                                                text: "q" + String(contactDelegate.modelData.qsoCount || 0)
		                                                      + " m" + String(contactDelegate.modelData.messageCount || 0)
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.green
		                                            }

		                                            Text {
		                                                Layout.fillWidth: true
		                                                text: String(contactDelegate.modelData.comment || contactDelegate.modelData.lastEvent || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }
		                                        }

		                                        MouseArea {
		                                            id: contactMouse
		                                            anchors.fill: parent
		                                            hoverEnabled: true
		                                            cursorShape: Qt.PointingHandCursor
		                                            onClicked: root.loadContactDetails(contactDelegate.modelData)
		                                        }
		                                    }

		                                    Text {
		                                        anchors.centerIn: parent
		                                        visible: contactList.count === 0
		                                        text: qsTr("No contacts")
		                                        font.family: root.mono
		                                        font.pixelSize: 10
		                                        color: root.textSecondary
		                                    }
		                                }

		                                Rectangle {
		                                    Layout.preferredWidth: 1
		                                    Layout.fillHeight: true
		                                    color: root.borderSoft
		                                }

		                                ListView {
		                                    id: contactTimelineList
		                                    Layout.fillWidth: true
		                                    Layout.fillHeight: true
		                                    clip: true
		                                    spacing: 2
		                                    boundsBehavior: Flickable.StopAtBounds
		                                    model: root.selectedContactTimeline
		                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

		                                    delegate: Rectangle {
		                                        id: timelineDelegate
		                                        required property var modelData
		                                        width: contactTimelineList.width
		                                        height: 24
		                                        radius: 4
		                                        color: timelineMouse.containsMouse ? root.rowHover : "transparent"

		                                        RowLayout {
		                                            anchors.fill: parent
		                                            anchors.leftMargin: 6
		                                            anchors.rightMargin: 6
		                                            spacing: 6

		                                            Text {
		                                                Layout.preferredWidth: 48
		                                                text: String(timelineDelegate.modelData.type || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                font.bold: true
		                                                color: String(timelineDelegate.modelData.type || "") === "ALERT" ? root.red
		                                                       : (String(timelineDelegate.modelData.type || "") === "FILE" ? root.cyan
		                                                       : (String(timelineDelegate.modelData.type || "") === "MAIL" ? root.green : root.amber))
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 68
		                                                text: String(timelineDelegate.modelData.state || timelineDelegate.modelData.label || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }

		                                            Text {
		                                                Layout.fillWidth: true
		                                                text: String(timelineDelegate.modelData.summary || timelineDelegate.modelData.details || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textPrimary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 92
		                                                text: String(timelineDelegate.modelData.details || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }
		                                        }

		                                        MouseArea {
		                                            id: timelineMouse
		                                            anchors.fill: parent
		                                            hoverEnabled: true
		                                        }
		                                    }

		                                    Text {
		                                        anchors.centerIn: parent
		                                        visible: contactTimelineList.count === 0
		                                        text: root.selectedContactCall.length > 0 ? "No contact history" : "Select contact"
		                                        font.family: root.mono
		                                        font.pixelSize: 10
		                                        color: root.textSecondary
	                            }
	                        }
	                    }

	                    Item {
	                        visible: root.toolPageIndex === 8
	                        Layout.fillWidth: true
	                        Layout.fillHeight: visible
	                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 320
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    SmallButton {
                                        text: "EXPORT"
                                        implicitWidth: 68
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Export cluster last-heard JSON"
                                        onClicked: root.exportCluster()
                                    }

                                    SmallButton {
                                        text: "IMPORT"
                                        implicitWidth: 68
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Merge pasted cluster JSON"
                                        onClicked: root.importCluster()
                                    }

                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 54
                                        accent: root.cyan
                                        enabled: clusterJsonArea.text.length > 0
                                        tip: "Copy cluster JSON"
                                        onClicked: root.copyPlainText(clusterJsonArea.text)
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 42
                                        accent: root.red
                                        enabled: !!ft2Link && root.clusterLastHeard.length > 0
                                        tip: "Clear local cluster last heard"
                                        onClicked: root.clearCluster()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    TextField {
                                        id: clusterSharePathText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 24
                                        text: root.clusterSharePath
                                        placeholderText: qsTr("Shared cluster JSON path")
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        selectByMouse: true
                                        onEditingFinished: root.clusterSharePath = text.trim()
                                        onAccepted: {
                                            root.clusterSharePath = text.trim()
                                            root.pullClusterShare()
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.clusterSyncStatus.length > 0
                                              ? root.clusterSyncStatus
                                              : "Manual or auto cluster share sync"
                                        color: root.clusterSyncStatus.indexOf("error") >= 0
                                               ? root.red
                                               : root.textSecondary
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    CompactCheck {
                                        text: "AUTO"
                                        checked: root.clusterAutoSync
                                        accent: root.green
                                        onToggled: function(nextChecked) { root.clusterAutoSync = nextChecked }
                                    }

                                    SmallButton {
                                        text: root.clusterAutoSyncIntervalText()
                                        implicitWidth: 44
                                        accent: root.textSecondary
                                        tip: "Cluster auto-sync interval"
                                        onClicked: root.cycleClusterAutoSyncInterval()
                                    }

                                    SmallButton {
                                        text: "SYNC"
                                        implicitWidth: 48
                                        accent: root.cyan
                                        enabled: !!ft2Link
                                        tip: "Pull, merge, and push cluster JSON"
                                        onClicked: {
                                            root.clusterSharePath = clusterSharePathText.text.trim()
                                            root.clusterLastAutoSyncMs = Date.now()
                                            root.syncClusterShare(true)
                                        }
                                    }

                                    SmallButton {
                                        text: "PUSH"
                                        implicitWidth: 48
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Write cluster JSON to shared file"
                                        onClicked: {
                                            root.clusterSharePath = clusterSharePathText.text.trim()
                                            root.pushClusterShare()
                                        }
                                    }

                                    SmallButton {
                                        text: "PULL"
                                        implicitWidth: 48
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Merge cluster JSON from shared file"
                                        onClicked: {
                                            root.clusterSharePath = clusterSharePathText.text.trim()
                                            root.pullClusterShare()
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    columns: 2
                                    rowSpacing: 3
                                    columnSpacing: 6

                                    Text {
                                        text: "NODE"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.cyan
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.clusterConfigState.nodeId || "--")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "BAND"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.clusterConfigState.band || "--")
                                              + "  "
                                              + root.frequencyHzText(Number(root.clusterConfigState.dialFrequencyHz || 0))
                                              + "  "
                                              + String(root.clusterLastHeard.length) + " rec"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }

                                ScrollView {
                                    id: clusterJsonScroll
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                                    TextArea {
                                        id: clusterJsonArea
                                        width: Math.max(clusterJsonScroll.availableWidth, implicitWidth)
                                        height: Math.max(clusterJsonScroll.availableHeight, implicitHeight)
                                        selectByMouse: true
                                        wrapMode: TextEdit.NoWrap
                                        placeholderText: qsTr("Paste cluster JSON here, or press EXPORT")
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                        selectedTextColor: root.panelBg
                                        selectionColor: root.cyan
                                        background: Rectangle {
                                            color: Qt.rgba(0.02, 0.025, 0.03, 0.90)
                                            border.color: root.borderSoft
                                            border.width: 1
                                            radius: 4
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: clusterLastHeardList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.clusterLastHeard
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: clusterDelegate
                                    required property var modelData
                                    width: clusterLastHeardList.width
                                    height: 26
                                    radius: 4
                                    color: clusterMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 72
                                            text: String(clusterDelegate.modelData.call || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: clusterDelegate.modelData.cq ? root.green : root.cyan
                                        }

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: String(clusterDelegate.modelData.band || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.preferredWidth: 96
                                            text: root.frequencyHzText(Number(clusterDelegate.modelData.dialFrequencyHz || 0))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 96
                                            text: String(clusterDelegate.modelData.nodeId || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: String(clusterDelegate.modelData.locator || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(clusterDelegate.modelData.event || "--")
                                                  + "  "
                                                  + String(clusterDelegate.modelData.source || "--")
                                                  + "  n"
                                                  + String(clusterDelegate.modelData.heardCount || 0)
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 96
                                            text: String(clusterDelegate.modelData.lastHeardUtc || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }
                                    }

                                    MouseArea {
                                        id: clusterMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: clusterLastHeardList.count === 0
                                    text: qsTr("No cluster last-heard records")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }
			                }
			            }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    TextField {
                                        id: pathCallText
                                        Layout.preferredWidth: 78
                                        text: root.pathFilterCall
                                        placeholderText: "CALL"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        maximumLength: 16
                                        selectByMouse: true
                                        onAccepted: root.applyPathFilter(text, pathGridText.text)
                                        onEditingFinished: root.pathFilterCall = text.trim().toUpperCase()
                                    }

                                    TextField {
                                        id: pathGridText
                                        Layout.preferredWidth: 62
                                        text: root.pathFilterGrid
                                        placeholderText: "GRID"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        maximumLength: 8
                                        selectByMouse: true
                                        onAccepted: root.applyPathFilter(pathCallText.text, text)
                                        onEditingFinished: root.pathFilterGrid = text.trim().toUpperCase()
                                    }

                                    SmallButton {
                                        text: "GO"
                                        implicitWidth: 38
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Apply path filter"
                                        onClicked: root.applyPathFilter(pathCallText.text, pathGridText.text)
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 42
                                        accent: root.textSecondary
                                        tip: "Clear path filter"
                                        onClicked: {
                                            pathCallText.text = ""
                                            pathGridText.text = ""
                                            root.clearPathFilter()
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 2
                                    rowSpacing: 4
                                    columnSpacing: 8

                                    Text {
                                        text: "SNR"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.cyan
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pathCount("snrCount") + " avg " + root.pathAverage("avgSnr")
                                              + " min " + String(root.pathValue("minSnr", 0))
                                              + " max " + String(root.pathValue("maxSnr", 0))
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "BEST"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: Number(root.pathValue("bestHourUtc", -1)) >= 0
                                              ? (root.twoDigit(root.pathValue("bestHourUtc", 0))
                                                 + "Z " + root.pathAverage("bestHourAvgSnr")
                                                 + " dB / " + root.pathCount("bestHourCount"))
                                              : "--"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "PATH"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.green
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pathCount("count") + " rec / " + root.statCount("pathQualityReports") + " q"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    Text {
                                        text: "BAND"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.textSecondary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("not tracked")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: pathReportList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.pathAnalysis && root.pathAnalysis.recentReports
                                       ? root.pathAnalysis.recentReports : []
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: pathReportDelegate
                                    required property var modelData
                                    width: pathReportList.width
                                    height: 26
                                    radius: 4
                                    color: pathReportMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 70
                                            text: String(pathReportDelegate.modelData.remoteCall || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 48
                                            text: String(pathReportDelegate.modelData.locator || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: pathReportDelegate.modelData.snrValid
                                                  ? (String(pathReportDelegate.modelData.snrDb || 0) + " dB")
                                                  : (pathReportDelegate.modelData.qualityValid
                                                     ? ("q " + Number(pathReportDelegate.modelData.quality || 0).toFixed(2))
                                                     : String(pathReportDelegate.modelData.kind || "PATH"))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: pathReportDelegate.modelData.snrValid
                                                   ? root.green
                                                   : (pathReportDelegate.modelData.qualityValid ? root.cyan : root.amber)
                                        }

                                        Text {
                                            Layout.preferredWidth: 70
                                            text: String(pathReportDelegate.modelData.direction || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 64
                                            text: String(pathReportDelegate.modelData.targetCall || "").length > 0
                                                  ? ("T>" + String(pathReportDelegate.modelData.targetCall || ""))
                                                  : String(pathReportDelegate.modelData.source || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(pathReportDelegate.modelData.detail || "").length > 0
                                                  ? (String(pathReportDelegate.modelData.detail || "")
                                                     + "  " + String(pathReportDelegate.modelData.atUtc || "--"))
                                                  : String(pathReportDelegate.modelData.atUtc || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }
                                    }

                                    MouseArea {
                                        id: pathReportMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: pathReportList.count === 0
                                    text: qsTr("No path reports")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                        }
                    }
                }
            }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 224
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "OPS"
                                        implicitWidth: 42
                                        accent: root.green
                                        tip: "Export operational log"
                                        onClicked: root.exportLog("OPS")
                                    }
                                    SmallButton {
                                        text: "ADIF"
                                        implicitWidth: 48
                                        accent: root.amber
                                        tip: "Export ADIF log"
                                        onClicked: root.exportLog("ADIF")
                                    }
                                    SmallButton {
                                        text: "OUT"
                                        implicitWidth: 46
                                        accent: root.green
                                        tip: "Export logbook outbox"
                                        onClicked: root.exportLog("OUTBOX")
                                    }
                                    SmallButton {
                                        text: "CHAT"
                                        implicitWidth: 52
                                        accent: root.cyan
                                        tip: "Export chat history"
                                        onClicked: root.exportLog("CHAT")
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "STORE"
                                        implicitWidth: 58
                                        accent: root.textSecondary
                                        tip: "Export local store JSON"
                                        onClicked: root.exportLog("STORE")
                                    }
                                    SmallButton {
                                        text: "ALL"
                                        implicitWidth: 42
                                        accent: root.red
                                        tip: "Export all logs"
                                        onClicked: root.exportLog("BUNDLE")
                                    }
                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 54
                                        accent: root.green
                                        enabled: root.logExportText.length > 0
                                        tip: "Copy current export"
                                        onClicked: root.copyPlainText(root.logExportText)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "WRITE"
                                        implicitWidth: 58
                                        accent: root.amber
                                        tip: "Write ADIF file"
                                        onClicked: root.writeAdifFile()
                                    }
                                    SmallButton {
                                        text: "PATH"
                                        implicitWidth: 48
                                        accent: root.cyan
                                        tip: "Copy ADIF file path"
                                        onClicked: root.copyAdifPath()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "QSO"
                                        implicitWidth: 46
                                        accent: root.green
                                        enabled: root.selectedSessionId !== 0
                                        tip: "Queue selected QSO for external logbook upload"
                                        onClicked: root.queueSelectedLogbookUpload()
                                    }
                                    SmallButton {
                                        text: "QALL"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: root.qsoLog.length > 0
                                        tip: "Queue all FT2-Link QSOs for external logbook upload"
                                        onClicked: root.queueAllLogbookUploads()
                                    }
                                    SmallButton {
                                        text: "SEND"
                                        implicitWidth: 58
                                        accent: root.amber
                                        enabled: root.logbookOutbox.length > 0
                                        tip: "Submit queued FT2-Link ADIF records to external loggers"
                                        onClicked: root.sendQueuedLogbookUploads()
                                    }
                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 46
                                        accent: root.red
                                        enabled: root.logbookOutbox.length > 0
                                        tip: "Clear FT2-Link logbook outbox"
                                        onClicked: root.clearLogbookOutbox()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: "Outbox " + root.logbookOutbox.length
                                          + " / queued "
                                          + root.statCount("logbookQueued")
                                          + " / failed "
                                          + root.statCount("logbookFailed")
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            ScrollView {
                                id: logExportScroll
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                                TextArea {
                                    id: logExportArea
                                    width: Math.max(logExportScroll.availableWidth, implicitWidth)
                                    height: Math.max(logExportScroll.availableHeight, implicitHeight)
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.NoWrap
                                    text: root.logExportText
                                    placeholderText: qsTr("Select an export")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textPrimary
                                    selectedTextColor: root.panelBg
                                    selectionColor: root.cyan
                                    background: Rectangle {
                                        color: Qt.rgba(0.02, 0.025, 0.03, 0.90)
                                        border.color: root.borderSoft
                                        border.width: 1
                                        radius: 4
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "AUDIT"
                                        implicitWidth: 58
                                        accent: root.cyan
                                        enabled: !!ft2Link
                                        tip: "Audit local FT2-Link store"
                                        onClicked: root.auditStore()
                                    }
                                    SmallButton {
                                        text: "BACKUP"
                                        implicitWidth: 70
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Create JSON backup"
                                        onClicked: root.backupStore()
                                    }
                                    SmallButton {
                                        text: "FIX"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Backup and rewrite store"
                                        onClicked: root.fixStore(true)
                                    }
                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.textSecondary
                                        enabled: !!ft2Link
                                        tip: "Rewrite store without backup"
                                        onClicked: root.fixStore(false)
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 2
                                    rowSpacing: 4
                                    columnSpacing: 6

                                    Text {
                                        text: "STATE"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.cyan
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.storeAudit.summary || "--")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.storeAudit.ok === false ? root.red : root.green
                                    }

                                    Text {
                                        text: "REC"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.storeAudit.recordCount || 0)
                                              + " / " + String(root.storeAudit.serializedBytes || 0) + " B"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "FILE"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.textSecondary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.storeAudit.storePath || "--")
                                        elide: Text.ElideMiddle
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }
                            }

                            ScrollView {
                                id: dbAuditScroll
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                                TextArea {
                                    id: dbAuditArea
                                    width: Math.max(dbAuditScroll.availableWidth, implicitWidth)
                                    height: Math.max(dbAuditScroll.availableHeight, implicitHeight)
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.NoWrap
                                    text: root.databaseActionText.length > 0
                                          ? root.databaseActionText
                                          : root.prettyJson(root.storeAudit)
                                    placeholderText: qsTr("Run audit")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textPrimary
                                    selectedTextColor: root.panelBg
                                    selectionColor: root.cyan
                                    background: Rectangle {
                                        color: Qt.rgba(0.02, 0.025, 0.03, 0.90)
                                        border.color: root.borderSoft
                                        border.width: 1
                                        radius: 4
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ListView {
                                id: statsList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
                                model: [
                                    { label: "QSO", value: root.statCount("qsoTotal") + " / " + root.statCount("qsoDistinctCallsigns") },
                                    { label: "LONG", value: root.statCount("longestQsoMinutes") + "m / " + root.statCount("longestQsoMessages") + " msg" },
                                    { label: "CQ", value: root.statCount("cqsSent") + " tx / " + root.statCount("cqsReceived") + " rx" },
                                    { label: "BCN", value: root.statCount("beaconsSent") + " tx / " + root.statCount("beaconsReceived") + " rx" },
                                    { label: "CHAT", value: root.statCount("chatMessagesLogged") + " log / " + root.statCount("chatMessagesSent") + " tx / " + root.statCount("chatMessagesReceived") + " rx" },
                                    { label: "PING", value: root.statCount("pingsSent") + " tx / " + root.statCount("pingsReceived") + " rx / " + root.statCount("pingReplies") + " rep" },
                                    { label: "PATH", value: root.statCount("pathReportsTotal") + " rep / " + root.statCount("pathQualityReports") + " q" },
                                    { label: "CLST", value: root.statCount("clusterLastHeardTotal") + " heard" },
                                    { label: "PRE", value: root.statCount("customCannedMessages") + " custom" },
                                    { label: "ATAG", value: root.statCount("customAlertTags") + " custom" },
                                    { label: "FREQ", value: root.statCount("frequencyPresets") + " cf / " + root.statCount("allowedQsyRanges") + " rng / " + root.statCount("frequencySchedule") + " sch" },
                                    { label: "BCAST", value: root.statCount("broadcastsSent") + " tx / " + root.statCount("broadcastsReceived") + " rx" },
                                    { label: "MAIL", value: root.statCount("mailboxIncoming") + " in / " + root.statCount("mailboxOutgoing") + " out / " + root.statCount("mailboxRelay") + " relay" },
                                    { label: "FORM", value: root.statCount("formsIncoming") + " in / " + root.statCount("formsOutgoing") + " out" },
                                    { label: "FILE", value: root.statCount("filesReceived") + " rx / " + root.statCount("filesSent") + " tx / " + root.statCount("receivedFileBytes") + " B" },
                                    { label: "BBS", value: root.statCount("bulletinsIncoming") + " in / " + root.statCount("bulletinsOutgoing") + " out" },
                                    { label: "ALERT", value: root.statCount("alertsTotal") }
                                ]
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: statDelegate
                                    required property var modelData
                                    width: statsList.width
                                    height: 22
                                    radius: 4
                                    color: statMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: String(statDelegate.modelData.label || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.cyan
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(statDelegate.modelData.value || "0")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: statMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 210
                                Layout.fillHeight: true
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 6

                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 56
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Copy statistics text"
                                        onClicked: root.copyStatisticsText()
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "REC " + root.statCount("storeRecordsTotal")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "LAST " + String(root.statValue("lastActivityUtc", "--"))
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.statValue("snrTracked", false)
                                          ? ("SNR rx " + root.statCount("snrsReceived")
                                             + " avg " + Number(root.statValue("snrReceivedAvg", 0)).toFixed(1)
                                             + " / tx " + root.statCount("snrsSent"))
                                          : "SNR --"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                Text {
                                    text: qsTr("RECEIVED FILES")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.cyan
                                }

                                Text {
                                    text: root.receivedFilesCountText()
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.receivedFileUnreadCount > 0
                                           ? root.amber : root.textSecondary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.receivedFileIoPendingCount > 0
                                          ? root.receivedFilePendingText()
                                          : root.effectiveReceivedFileDirectory()
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }

                                SmallButton {
                                    text: root.receivedFileAutoSave
                                          ? qsTr("AUTO ON") : qsTr("AUTO OFF")
                                    implicitWidth: 70
                                    accent: root.receivedFileAutoSave ? root.green : root.textSecondary
                                    tip: qsTr("Automatically save new files in the receive folder")
                                    onClicked: root.receivedFileAutoSave = !root.receivedFileAutoSave
                                }

                                SmallButton {
                                    text: qsTr("FOLDER")
                                    implicitWidth: 58
                                    accent: root.cyan
                                    tip: qsTr("Choose the received files folder")
                                    onClicked: root.chooseReceivedFileDirectory()
                                }

                                SmallButton {
                                    text: qsTr("OPEN")
                                    implicitWidth: 48
                                    accent: root.cyan
                                    tip: qsTr("Open the received files folder")
                                    onClicked: root.openReceivedFileDirectory()
                                }

                                SmallButton {
                                    text: qsTr("READ ALL")
                                    implicitWidth: 70
                                    accent: root.amber
                                    enabled: root.receivedFileUnreadCount > 0
                                    tip: qsTr("Clear unread marker for all received files")
                                    onClicked: root.markAllReceivedFilesRead()
                                }

                                SmallButton {
                                    text: qsTr("CLEAR")
                                    implicitWidth: 52
                                    accent: root.red
                                    enabled: !!ft2Link && root.receivedFiles.length > root.receivedFileUnreadCount
                                    tip: qsTr("Delete received files already marked read")
                                    onClicked: root.clearReadReceivedFiles()
                                }
                            }

                            ListView {
                                id: receivedFileList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.receivedFiles
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: rxFileDelegate
                                    required property var modelData
                                    width: receivedFileList.width
                                    height: 28
                                    radius: 4
                                    color: rxFileMouse.containsMouse ? root.rowHover : "transparent"

                                    MouseArea {
                                        id: rxFileMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 42
                                            text: rxFileDelegate.modelData.unread
                                                  ? qsTr("NEW")
                                                  : (rxFileDelegate.modelData.imageLike
                                                     ? qsTr("IMG") : qsTr("FILE"))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: rxFileDelegate.modelData.unread
                                                   ? root.amber
                                                   : (rxFileDelegate.modelData.imageLike ? root.amber : root.cyan)
                                        }

                                        Text {
                                            Layout.preferredWidth: 64
                                            text: String(rxFileDelegate.modelData.senderCall || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 110
                                            text: String(rxFileDelegate.modelData.fileName || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.green
                                        }

                                        Text {
                                            Layout.preferredWidth: 90
                                            text: root.receivedFileDate(rxFileDelegate.modelData)
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 52
                                            text: String(rxFileDelegate.modelData.sizeBytes || 0) + " B"
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(rxFileDelegate.modelData.preview || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        SmallButton {
                                            text: qsTr("SAVE")
                                            implicitWidth: 48
                                            accent: root.green
                                            enabled: root.receivedFileHasContent(rxFileDelegate.modelData)
                                                     && !root.receivedFilePendingTransfers[
                                                         String(Number(rxFileDelegate.modelData.id || 0))]
                                            tip: qsTr("Save to the configured receive folder")
                                            onClicked: root.saveReceivedFile(rxFileDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: qsTr("SAVE AS")
                                            implicitWidth: 58
                                            accent: root.cyan
                                            enabled: root.receivedFileHasContent(rxFileDelegate.modelData)
                                                     && !root.receivedFilePendingTransfers[
                                                         String(Number(rxFileDelegate.modelData.id || 0))]
                                            tip: qsTr("Save with another name or in another folder")
                                            onClicked: root.saveReceivedFileAs(rxFileDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: rxFileDelegate.modelData.unread
                                                  ? qsTr("READ") : qsTr("UNREAD")
                                            implicitWidth: 54
                                            accent: rxFileDelegate.modelData.unread ? root.amber : root.textSecondary
                                            enabled: !!ft2Link
                                            tip: rxFileDelegate.modelData.unread
                                                 ? qsTr("Mark received file as read")
                                                 : qsTr("Mark received file as unread")
                                            onClicked: root.markReceivedFileRead(rxFileDelegate.modelData,
                                                                                  !!rxFileDelegate.modelData.unread,
                                                                                  true)
                                        }

                                        SmallButton {
                                            text: qsTr("COPY")
                                            implicitWidth: 48
                                            accent: root.cyan
                                            enabled: String(rxFileDelegate.modelData.content || "").length > 0
                                            tip: qsTr("Copy received file content")
                                            onClicked: {
                                                var text = String(rxFileDelegate.modelData.content || "")
                                                root.copyPlainText(text)
                                                root.markReceivedFileRead(rxFileDelegate.modelData, true, false)
                                                root.receivedFileStatus = qsTr("Copied %1").arg(
                                                            String(rxFileDelegate.modelData.fileName
                                                                   || qsTr("received file")))
                                            }
                                        }

                                        SmallButton {
                                            text: qsTr("DEL")
                                            implicitWidth: 36
                                            accent: root.red
                                            enabled: !!ft2Link
                                            tip: qsTr("Delete this received file entry")
                                            onClicked: root.deleteReceivedFile(rxFileDelegate.modelData)
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: receivedFileList.count === 0
                                    text: qsTr("No received files")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 16
                                text: root.receivedFileStatus.length > 0
                                      ? root.receivedFileStatus
                                      : (root.receivedFiles.length > 0
                                         ? qsTr("Folder: %1").arg(
                                               root.effectiveReceivedFileDirectory())
                                         : qsTr("Waiting for files in %1").arg(
                                               root.effectiveReceivedFileDirectory()))
                                elide: Text.ElideMiddle
                                font.family: root.mono
                                font.pixelSize: 10
                                color: root.textSecondary
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 330
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: presetLabelText
                                        Layout.preferredWidth: 82
                                        Layout.preferredHeight: 26
                                        placeholderText: "LABEL"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 12
                                        selectByMouse: true
                                    }

                                    TextField {
                                        id: presetTipText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 26
                                        placeholderText: qsTr("description")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 96
                                        selectByMouse: true
                                    }
                                }

                                TextField {
                                    id: presetTemplateText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    placeholderText: qsTr("template text with <MYCALL>, <CALL>, <QTH>...")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 512
                                    selectByMouse: true
                                    onAccepted: root.savePreset()
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: presetLabelText.text.trim().length > 0
                                                 && presetTemplateText.text.trim().length > 0
                                        tip: "Save custom preset"
                                        onClicked: root.savePreset()
                                    }

                                    SmallButton {
                                        text: "DEL"
                                        implicitWidth: 44
                                        accent: root.red
                                        enabled: presetLabelText.text.trim().length > 0
                                        tip: "Delete custom preset"
                                        onClicked: root.deletePreset()
                                    }

                                    SmallButton {
                                        text: "RST"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: root.customCannedMessages.length > 0
                                        tip: "Clear all custom presets"
                                        onClicked: root.resetPresets()
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.customCannedMessages.length) + " custom"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }
                            }

                            ListView {
                                id: customPresetList
                                Layout.preferredWidth: 210
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.customCannedMessages
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: presetDelegate
                                    required property var modelData
                                    width: customPresetList.width
                                    height: 26
                                    radius: 4
                                    color: presetMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: String(presetDelegate.modelData.label || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(presetDelegate.modelData.templateText || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: presetMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: root.loadPreset(presetDelegate.modelData)
                                        onDoubleClicked: root.insertCannedMessage(String(presetDelegate.modelData.templateText || ""))
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: customPresetList.count === 0
                                    text: qsTr("No custom presets")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: checkInCityText
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 26
                                        text: root.checkInCity
                                        placeholderText: root.profileQth.length > 0 ? root.profileQth : "city"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                    }

                                    TextField {
                                        id: checkInRegionText
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 26
                                        text: root.checkInRegion
                                        placeholderText: qsTr("county/state")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                    }

                                    TextField {
                                        id: checkInChannelText
                                        Layout.preferredWidth: 54
                                        Layout.preferredHeight: 26
                                        text: root.checkInChannel
                                        placeholderText: "HF"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 8
                                        selectByMouse: true
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: checkInWeatherText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        placeholderText: qsTr("weather line optional")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                    }

                                    SmallButton {
                                        text: "MAIL"
                                        implicitWidth: 54
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Prepare VarAC Wednesday mail"
                                        onClicked: root.prepareCheckInMail()
                                    }

                                    SmallButton {
                                        text: "CHAT"
                                        implicitWidth: 54
                                        accent: root.cyan
                                        enabled: !!ft2Link
                                        tip: "Insert check-in in chat composer"
                                        onClicked: root.prepareCheckInChat()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: qsTr("Wednesday check-in: MAIL sets address, subject and body; CHAT inserts only the body.")
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: frequencyPresetText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.frequencyPresetsText === "function"
                                              ? ft2Link.frequencyPresetsText()
                                              : ""
                                        placeholderText: qsTr("14105000|20m|Main, 7105000|40m|Main")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveFrequencyPresets()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save calling frequency presets"
                                        onClicked: root.saveFrequencyPresets()
                                    }

                                    SmallButton {
                                        text: "RST"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Restore default frequency presets"
                                        onClicked: root.resetFrequencyPresets()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: frequencyScheduleText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.frequencyScheduleText === "function"
                                              ? ft2Link.frequencyScheduleText()
                                              : ""
                                        placeholderText: qsTr("0000-2359|CALLING|14105000|20m main|CQ")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveFrequencySchedule()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save UTC frequency schedule"
                                        onClicked: root.saveFrequencySchedule()
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Clear frequency schedule"
                                        onClicked: root.resetFrequencySchedule()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    spacing: 5

                                    Text {
                                        Layout.preferredWidth: 64
                                        text: "ACTIVE"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.activeFrequencyScheduleLine()
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.activeFrequencyScheduleItem() ? root.green : root.textSecondary
                                    }

                                    CompactCheck {
                                        text: "AUTO"
                                        checked: root.frequencyScheduleAutoApply
                                        accent: root.green
                                        onToggled: function(nextChecked) { root.frequencyScheduleAutoApply = nextChecked }
                                    }

                                    SmallButton {
                                        text: "APPLY"
                                        implicitWidth: 58
                                        accent: root.green
                                        enabled: !!bridge && root.activeFrequencyScheduleItem() !== null
                                        tip: "Apply active UTC schedule frequency now"
                                        onClicked: root.applyActiveFrequencySchedule(false)
                                    }

                                    SmallButton {
                                        text: "BCAST"
                                        implicitWidth: 58
                                        accent: root.amber
                                        enabled: !!ft2Link && root.activeFrequencyScheduleItem() !== null
                                        tip: "Prepare broadcast for active schedule frequency"
                                        onClicked: root.prepareActiveScheduleBroadcast()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: allowedQsyRangeText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.allowedQsyRangesText === "function"
                                              ? ft2Link.allowedQsyRangesText()
                                              : ""
                                        placeholderText: qsTr("14101250-14108750|20m, 7101250-7108750|40m")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveAllowedQsyRanges()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save allowed QSY ranges"
                                        onClicked: root.saveAllowedQsyRanges()
                                    }

                                    SmallButton {
                                        text: "RST"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Restore default QSY ranges"
                                        onClicked: root.resetAllowedQsyRanges()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: qsTr("FREQ stores calling-frequency presets, UTC schedule windows and allowed QSY ranges. Schedule actions CALLING/CQ/BEACON/EMCOMM/QUIET protect the active frequency; DATA marks a data window. CAT auto-QSY is not performed.")
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: frequencyPresetListView
                                Layout.preferredWidth: 230
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.frequencyPresetList
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: freqDelegate
                                    required property var modelData
                                    width: frequencyPresetListView.width
                                    height: 24
                                    radius: 4
                                    color: freqMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: String(freqDelegate.modelData.band || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.cyan
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.frequencyHzText(Number(freqDelegate.modelData.dialFrequencyHz || 0))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: freqMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }

                            ListView {
                                id: allowedRangeListView
                                Layout.preferredWidth: 250
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.allowedQsyRangeList
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: rangeDelegate
                                    required property var modelData
                                    width: allowedRangeListView.width
                                    height: 24
                                    radius: 4
                                    color: rangeMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 48
                                            text: String(rangeDelegate.modelData.label || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.frequencyHzText(Number(rangeDelegate.modelData.fromHz || 0))
                                                  + " - "
                                                  + root.frequencyHzText(Number(rangeDelegate.modelData.toHz || 0))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: rangeMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: blockedCallsText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.blockedCallsText === "function"
                                              ? ft2Link.blockedCallsText()
                                              : ""
                                        placeholderText: qsTr("CALL1, CALL2, Z6/TEST")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveBlockedCalls()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save blocked callsigns"
                                        onClicked: root.saveBlockedCalls()
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 42
                                        accent: root.red
                                        enabled: !!ft2Link && root.blockedCalls.length > 0
                                        tip: "Clear blocked callsigns"
                                        onClicked: root.clearBlockedCalls()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: blockedCallText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        placeholderText: qsTr("Add callsign")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 24
                                        selectByMouse: true
                                        onAccepted: root.addBlockedCallFromEditor()
                                    }

                                    SmallButton {
                                        text: "ADD"
                                        implicitWidth: 48
                                        accent: root.amber
                                        enabled: !!ft2Link && blockedCallText.text.trim().length > 0
                                        tip: "Add callsign to block list"
                                        onClicked: root.addBlockedCallFromEditor()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: qsTr("Blocked calls cannot start a session, are hidden from last-heard, and their beacon, CQ, ping and broadcast traffic is ignored locally.")
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: blockedCallListView
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.blockedCalls
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: blockedCallDelegate
                                    required property string modelData
                                    width: blockedCallListView.width
                                    height: 24
                                    radius: 4
                                    color: blockedMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.fillWidth: true
                                            text: blockedCallDelegate.modelData
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.red
                                        }

                                        SmallButton {
                                            text: "DEL"
                                            implicitWidth: 42
                                            accent: root.red
                                            enabled: !!ft2Link
                                            tip: "Remove callsign from block list"
                                            onClicked: root.deleteBlockedCall(blockedCallDelegate.modelData)
                                        }
                                    }

                                    MouseArea {
                                        id: blockedMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: blockedCallListView.count === 0
                                    text: qsTr("No blocked calls")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 7

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("SATELLITE HALF-DUPLEX")
                                    font.family: root.mono
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: root.amber
                                }

                                CompactCheck {
                                    text: qsTr("Enable independent RX/TX VFOs")
                                    checked: root.satelliteHalfDuplexEnabled
                                    accent: root.amber
                                    tip: "For FT2-Link RF TX only. Editing this setting does not retune the radio."
                                    onToggled: function(nextChecked) {
                                        root.satelliteHalfDuplexEnabled = nextChecked
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text {
                                        Layout.preferredWidth: 72
                                        text: qsTr("RX MHz")
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.cyan
                                    }

                                    TextField {
                                        id: satelliteRxDialField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: root.satelliteRxDialHz > 0
                                              ? (root.satelliteRxDialHz / 1000000).toFixed(6) : ""
                                        placeholderText: qsTr("downlink dial MHz")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        onEditingFinished: {
                                            var mhz = Number(text.replace(",", "."))
                                            if (isFinite(mhz) && mhz > 0)
                                                root.satelliteRxDialHz = Math.round(mhz * 1000000)
                                        }
                                    }

                                    SmallButton {
                                        text: "RIG"
                                        implicitWidth: 44
                                        accent: root.cyan
                                        enabled: !!bridge && Number(bridge.frequency || 0) > 0
                                        tip: "Copy the current receive dial frequency; does not retune the rig"
                                        onClicked: root.satelliteRxDialHz = Math.round(Number(bridge.frequency))
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text {
                                        Layout.preferredWidth: 72
                                        text: qsTr("TX MHz")
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.red
                                    }

                                    TextField {
                                        id: satelliteTxDialField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: root.satelliteTxDialHz > 0
                                              ? (root.satelliteTxDialHz / 1000000).toFixed(6) : ""
                                        placeholderText: qsTr("uplink dial MHz")
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        onEditingFinished: {
                                            var mhz = Number(text.replace(",", "."))
                                            if (isFinite(mhz) && mhz > 0)
                                                root.satelliteTxDialHz = Math.round(mhz * 1000000)
                                        }
                                    }

                                    Text {
                                        Layout.preferredWidth: 76
                                        text: qsTr("SETTLE ms")
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    TextField {
                                        id: satelliteSettleField
                                        Layout.preferredWidth: 62
                                        Layout.preferredHeight: 28
                                        text: String(root.satelliteCatSettleMs)
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        onEditingFinished: {
                                            var value = Math.round(Number(text))
                                            if (isFinite(value))
                                                root.satelliteCatSettleMs = Math.max(250, Math.min(5000, value))
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    SmallButton {
                                        text: qsTr("READ RX/TX FROM RIG")
                                        implicitWidth: 168
                                        accent: root.cyan
                                        enabled: !!bridge && !!bridge.hamlibCat
                                        tip: "Copy the current Hamlib RX/TX split pair into this profile. It never retunes the rig."
                                        onClicked: root.importSatelliteRigPair(true, true)
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.satelliteRigImportStatus.length > 0
                                              ? root.satelliteRigImportStatus
                                              : "Empty fields are imported automatically when Hamlib reports active split."
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                        color: root.satelliteRigImportStatus.length > 0
                                               ? root.green : root.textSecondary
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var state = root.satelliteHalfDuplexStatus || ({})
                                        var detail = String(state.detail || "")
                                        return "STATE: " + String(state.state || "Idle")
                                               + (detail.length > 0 ? " — " + detail : "")
                                    }
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                    color: root.satelliteHalfDuplexStatus.busy ? root.amber : root.textPrimary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var state = root.satelliteHalfDuplexStatus || ({})
                                        var issue = String(state.configurationError || "")
                                        if (issue.length > 0)
                                            return issue
                                        if (!state.supportedBackend)
                                            return "Supported safely with a Hamlib CAT backend only."
                                        if (!state.catConnected)
                                            return "Connect the Hamlib CAT rig before transmitting."
                                        if (String(state.rigSplitMode || "").toLowerCase() !== "rig")
                                            return "Set Hamlib Split mode to 'rig' (not emulate or none)."
                                        return "Ready: PTT will follow the CAT settle delay and RX is restored after TX."
                                    }
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                    color: String(root.satelliteHalfDuplexStatus.configurationError || "").length > 0
                                           ? root.red
                                           : (root.satelliteHalfDuplexStatus.supportedBackend
                                              && root.satelliteHalfDuplexStatus.catConnected
                                              && String(root.satelliteHalfDuplexStatus.rigSplitMode || "").toLowerCase() === "rig"
                                              ? root.green : root.amber)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Use the normal FT2-Link ARM control for every RF transmission. This mode is half-duplex: receive audio pauses on TX, PTT is released before the RX VFO is restored, and a manual stop follows the same safe return path.")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                    color: root.textSecondary
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("QO-100 / cross-band setup")
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.cyan
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Enter the actual CAT dial frequencies used by your station or transverter. No fixed QO-100 defaults are applied: radio IF plans and transverter offsets differ.")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                    color: root.textSecondary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("The normal FT audio-offset split is deliberately bypassed for this mode, so a cross-band TX cannot reuse or move the FT2-Link chat/session view.")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                    color: root.textSecondary
                                }

                                Item { Layout.fillHeight: true }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("CAT diagnostics are recorded with the [FT2SAT] tag. Start with low power and verify the rig VFOs before on-air use.")
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                    color: root.amber
                                }
                            }
                        }
                    }
		            }
		        }
        }
    }
    }  // rootFlick (1.0.457)
}
