import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium 1.0

Rectangle {
    id: root
    required property var engine

    color: "transparent"
    property bool detachable: false
    property bool detached: false
    signal detachRequested()

    property color bgDeep: engine ? engine.themeManager.bgDeep : "#0b1220"
    property color primaryBlue: engine ? engine.themeManager.primaryColor : "#3f7cff"
    property color secondaryCyan: engine ? engine.themeManager.secondaryColor : "#00d8ff"
    property color accentGreen: engine ? engine.themeManager.accentColor : "#2ecc71"
    property color accentAmber: engine ? engine.themeManager.warningColor : "#f6c344"
    property color textPrimary: engine ? engine.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: engine ? engine.themeManager.textSecondary : "#9db1c9"
    property color glassBorder: engine ? engine.themeManager.glassBorder : "#2a3950"
    property var worldMap: worldMapLoader.item
    property var mapLayers: engine ? engine.mapIntelligenceService : null
    property var satelliteTracking: engine ? engine.satelliteTracking : null
    property var baseMapService: mapLayers ? mapLayers.baseMapService : null
    property var externalOverlays: mapLayers ? mapLayers.externalOverlayService : null
    property var mapOperations: mapLayers ? mapLayers.operationsService : null
    // Values from older INI files can arrive as strings (for example
    // "false" on some Linux installations).  !! would turn any non-empty
    // string into true, so use the same strict conversion as other settings.
    property bool gpuLiveMapEnabled: engine
        ? root.coerceBool(engine.getSetting("LiveMapUseGpu", true), true) : true
    property bool intelligencePanelRequested: width >= 760
    property bool showRosterPreferences: false
    property bool showRosterColumns: false
    property bool showRosterRules: false
    property bool showRosterMatrix: false
    property var hoveredGridDetails: ({})
    property var hoveredHistoricalGridDetails: ({})
    property var hoveredLiveGridDetails: ({})
    property real hoveredGridX: 0
    property real hoveredGridY: 0
    property bool gridPreviewVisible: false
    property bool gridDetailsPinned: false
    property var hoveredGeographicDetails: ({})
    property real hoveredGeographicX: 0
    property real hoveredGeographicY: 0
    property bool geographicPreviewVisible: false
    property var selectedOperationalDetails: ({})
    property var selectedGeographicDetails: ({})
    property real selectedMapX: 0
    property real selectedMapY: 0
    property bool operationalDetailsVisible: false
    property bool geographicDetailsVisible: false
    property var savedViewportBeforeQsoFocus: ({})
    property bool qsoViewportFocused: false
    property bool moonLocatePending: false
    property string activitySelectedBand: ""
    property bool mapSnapshotSyncPending: false
    readonly property bool compactIntelligencePanel: width < 760

    function ensureActivityBand() {
        var rows = root.mapLayers ? (root.mapLayers.bandActivity || []) : []
        if (rows.length === 0) {
            root.activitySelectedBand = ""
            return
        }
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].band === root.activitySelectedBand)
                return
        }
        root.activitySelectedBand = String(rows[0].band || "")
    }

    component LayerToggle: Rectangle {
        required property string label
        required property color activeColor
        property bool checked: false
        property string helpText: ""
        signal toggled(bool value)

        width: Math.max(54, layerLabel.implicitWidth + 22)
        height: 24
        radius: 4
        color: checked
            ? Qt.rgba(activeColor.r, activeColor.g, activeColor.b, 0.18)
            : (layerMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
        border.width: 1
        border.color: checked ? activeColor : root.glassBorder

        Text {
            id: layerLabel
            anchors.centerIn: parent
            text: parent.label
            color: parent.checked ? parent.activeColor : root.textSecondary
            font.pixelSize: 10
            font.bold: true
        }
        MouseArea {
            id: layerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.toggled(!parent.checked)
        }
        ToolTip.visible: layerMouse.containsMouse && helpText.length > 0
        ToolTip.text: helpText
        ToolTip.delay: 450
    }

    // Do not use the platform Material Button here.  In the operational
    // marker card, disabled Material buttons can render their label below the
    // rounded background on some Qt/Windows style combinations, leaving four
    // apparently blank controls.  This explicit content keeps the action and
    // its unavailable state readable on every platform.
    component OperationalActionButton: Rectangle {
        required property string label
        property bool actionEnabled: true
        property string unavailableHint: ""
        signal invoked()

        radius: 14
        color: actionEnabled
            ? Qt.rgba(root.secondaryCyan.r, root.secondaryCyan.g, root.secondaryCyan.b, 0.16)
            : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.20)
        border.width: 1
        border.color: actionEnabled
            ? Qt.rgba(root.secondaryCyan.r, root.secondaryCyan.g, root.secondaryCyan.b, 0.72)
            : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.52)
        opacity: actionEnabled ? 1.0 : 0.82

        Text {
            anchors.centerIn: parent
            text: parent.label
            color: parent.actionEnabled ? root.textPrimary : root.textSecondary
            font.pixelSize: 10
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            id: actionMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: parent.actionEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (parent.actionEnabled)
                    parent.invoked()
            }
        }

        ToolTip.visible: actionMouse.containsMouse && !actionEnabled
                          && unavailableHint.length > 0
        ToolTip.text: unavailableHint
        ToolTip.delay: 450
    }

    function coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return !!fallback
        if (typeof value === "boolean")
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return !!fallback
    }

    function syncMapSettings() {
        if (!engine || !worldMap)
            return
        worldMap.setHomeGrid(engine.grid)
        worldMap.setBaseMapEnabled(true)
        worldMap.setGreylineEnabled(
            root.coerceBool(engine.getSetting("ShowGreyline", true), true))
        worldMap.setDistanceInMiles(root.coerceBool(engine.getSetting("Miles", false), false))
        worldMap.setBaseMapService(root.baseMapService)
        worldMap.setExternalOverlayService(root.externalOverlays)
        worldMap.setCoveragePushPins(root.mapLayers
                                     ? root.mapLayers.coveragePushPinsEnabled : false)
        worldMap.setTimeZoneOverlayEnabled(root.mapLayers
                                           ? root.mapLayers.timeZoneOverlayEnabled : false)
        if (root.mapLayers && root.mapLayers.layerModel
                && worldMap.setLayerStyles)
            worldMap.setLayerStyles(root.mapLayers.layerModel.allLayerStyles())
        root.syncOperations()
    }

    function syncOperations() {
        if (!worldMap)
            return
        if (mapOperations && engine)
            mapOperations.operatorCall = String(engine.callsign || "").toUpperCase()
        var operationalMarkers = []
        var sourceMarkers = mapOperations
                ? (mapOperations.operationalMarkers || []) : []
        for (var markerIndex = 0; markerIndex < sourceMarkers.length; ++markerIndex)
            operationalMarkers.push(sourceMarkers[markerIndex])
        if (satelliteTracking && satelliteTracking.mapMarkers) {
            var satelliteMarkers = satelliteTracking.mapMarkers || []
            for (var satelliteIndex = 0; satelliteIndex < satelliteMarkers.length; ++satelliteIndex)
                operationalMarkers.push(satelliteMarkers[satelliteIndex])
        }
        if (mapLayerEnabled("moon") && externalOverlays
                && externalOverlays.moonDataAvailable) {
            operationalMarkers.push({
                "id": "moon-subpoint",
                "type": "MOON",
                "reference": "MOON",
                "label": qsTr("MOON"),
                "longitude": Number(externalOverlays.moonSublunarLongitude),
                "latitude": Number(externalOverlays.moonSublunarLatitude),
                "azimuth": Number(externalOverlays.moonAzimuth),
                "elevation": Number(externalOverlays.moonElevation),
                "distanceKm": Number(externalOverlays.moonDistanceKm),
                "illumination": Number(externalOverlays.moonIllumination)
            })
        }
        var geographicFeatures = mapOperations
                ? (mapOperations.geographicFeatures || []) : []
        if (externalOverlays && externalOverlays.earthquakeFeatures
                && externalOverlays.earthquakeFeatures.length > 0)
            geographicFeatures = geographicFeatures.concat(externalOverlays.earthquakeFeatures)
        worldMap.setOperationalMarkers(operationalMarkers)
        worldMap.setGeographicFeatures(geographicFeatures)
        worldMap.setProjection(
            mapOperations ? mapOperations.mapProjection : "Equirectangular")
    }

    function showOperationalDetails(details, x, y) {
        selectedOperationalDetails = details || ({})
        selectedMapX = Number(x || 0)
        selectedMapY = Number(y || 0)
        geographicDetailsVisible = false
        operationalDetailsVisible = true
        var isPota = details && String(details.type || "").toLowerCase() === "pota"
        if (mapOperations && isPota && details.reference)
            mapOperations.selectPotaPark(details.reference)
        if (mapOperations && isPota && engine) {
            var action = mapOperations.preparePotaAction(
                        details, String(engine.callsign || ""))
            if (action.messageReady) {
                if (Number(action.frequencyHz || 0) > 0)
                    engine.frequency = Number(action.frequencyHz)
                if (String(action.mode || "").length > 0)
                    engine.mode = String(action.mode)
                // Selecting the target regenerates the standard WSJT-X
                // messages. TX remains governed by the existing map setting.
                engine.processMapContactClick(
                            String(action.targetCall), String(action.targetGrid || ""))
            }
        }
    }

    function showGeographicDetails(details, x, y) {
        selectedGeographicDetails = details || ({})
        selectedMapX = Number(x || 0)
        selectedMapY = Number(y || 0)
        operationalDetailsVisible = false
        geographicDetailsVisible = true
    }

    function showGeographicPreview(details, x, y) {
        if (!details || details.type !== "earthquake")
            return
        hoveredGeographicDetails = details
        hoveredGeographicX = Number(x || 0)
        hoveredGeographicY = Number(y || 0)
        geographicPreviewVisible = true
    }

    function hideGeographicPreview() {
        geographicPreviewVisible = false
        hoveredGeographicDetails = ({})
    }

    function earthquakeSummary(details) {
        var magnitude = Number(details ? details.magnitude : NaN)
        var depth = Number(details ? details.depthKm : NaN)
        var parts = []
        if (isFinite(magnitude))
            parts.push("M " + magnitude.toFixed(1))
        if (isFinite(depth) && depth >= 0)
            parts.push(depth.toFixed(depth < 10 ? 1 : 0) + " km deep")
        return parts.join("  ·  ")
    }

    function captureMapScreenshot() {
        if (!mapOperations || !worldMapLoader.item)
            return
        var path = mapOperations.reserveScreenshotPath()
        if (!path || path.length === 0)
            return
        worldMapLoader.grabToImage(function(result) {
            if (!result.saveToFile(path))
                console.warn("Unable to save Live Map screenshot", path)
        })
    }

    function exportMapConfiguration() {
        if (!root.mapLayers)
            return
        var path = root.mapLayers.reserveMapConfigurationPath()
        if (path)
            root.mapLayers.exportMapConfiguration(
                path, root.worldMap && root.worldMap.viewportState
                    ? root.worldMap.viewportState() : ({}))
    }

    function importMapConfiguration(path) {
        if (!root.mapLayers || !path)
            return
        var viewport = root.mapLayers.importMapConfiguration(path)
        if (root.worldMap && root.worldMap.setViewportState
                && viewport && Object.keys(viewport).length > 0)
            root.worldMap.setViewportState(viewport)
        root.syncMapSettings()
        root.syncCoverage()
    }

    function aimSelectedMarker() {
        if (!mapOperations || !engine || !selectedOperationalDetails)
            return
        var latitude = Number(selectedOperationalDetails.latitude)
        var longitude = Number(selectedOperationalDetails.longitude)
        var homeGrid = String(engine.grid || "")
        if (!isFinite(latitude) || !isFinite(longitude)
                || homeGrid.length < 4)
            return
        mapOperations.aimRotatorAt(
            latitude, longitude,
            Number(engine.latFromGrid(homeGrid)),
            Number(engine.lonFromGrid(homeGrid)))
    }

    function trackSelectedMarker() {
        if (!mapOperations || !engine || !selectedOperationalDetails)
            return
        var latitude = Number(selectedOperationalDetails.latitude)
        var longitude = Number(selectedOperationalDetails.longitude)
        var homeGrid = String(engine.grid || "")
        if (!isFinite(latitude) || !isFinite(longitude)
                || homeGrid.length < 4)
            return
        var type = String(selectedOperationalDetails.type || "").toUpperCase()
        var elevation = Number(selectedOperationalDetails.elevation)
        if (type === "SATELLITE" && isFinite(elevation)) {
            mapOperations.trackRotator(
                Number(selectedOperationalDetails.azimuth), elevation, true)
            return
        }
        mapOperations.trackRotatorAt(
            latitude, longitude,
            Number(engine.latFromGrid(homeGrid)),
            Number(engine.lonFromGrid(homeGrid)))
    }

    function operationalValue(key) {
        var base = selectedOperationalDetails || ({})
        if (String(base.type || "").toLowerCase() === "pota" && mapOperations
                && mapOperations.selectedPotaPark
                && mapOperations.selectedPotaPark[key] !== undefined
                && String(mapOperations.selectedPotaPark[key]).length > 0)
            return mapOperations.selectedPotaPark[key]
        return base[key] !== undefined ? base[key] : ""
    }

    function overlayUpdatedText(updatedMs) {
        if (!updatedMs || updatedMs <= 0)
            return qsTr("not updated")
        return Qt.formatDateTime(new Date(updatedMs), "HH:mm:ss")
    }

    function mapLayerEnabled(layerId) {
        return !!(mapLayers && mapLayers.layerModel
                  && mapLayers.layerModel.layerEnabled(layerId))
    }

    function layerDescription(layerId) {
        if (layerId === "confirmed")
            return qsTr("Confirmed grids contain an imported ADIF QSO with QSL_RCVD=Y, LOTW_QSL_RCVD=Y or EQSL_QSL_RCVD=Y.")
        if (layerId === "psk")
            return qsTr("PSK Reporter reception reports for your callsign during the last %1 minutes. Decodium PSK upload does not need to be enabled.")
                    .arg(engine ? engine.pskMapSpotWindowMinutes : 15)
        if (layerId === "pota")
            return qsTr("Live Parks on the Air activator spots. Disabling this layer immediately removes all POTA markers from the map.")
        if (layerId === "states")
            return qsTr("United States state boundaries from the U.S. Census TIGER service. The map focuses on the United States when enabled.")
        if (layerId === "counties")
            return qsTr("United States county boundaries from the U.S. Census TIGER service. Zoom in after it loads to inspect individual counties.")
        if (layerId === "iota")
            return qsTr("Official IOTA Directory groups. Catalog positions are shown on the map; worked and confirmed status is applied only when the imported ADIF QSO contains an IOTA field.")
        if (layerId === "wpx")
            return qsTr("WPX prefixes derived from your imported ADIF log.")
        if (layerId === "moon")
            return qsTr("Moon visibility hemisphere, sublunar point and path from your station. The marker appears as soon as the ephemeris has been calculated.")
        if (layerId === "muf")
            return qsTr("Maximum Usable Frequency forecast. The temporal legend reports acquisition time, validity and visual decay.")
        if (layerId === "fof2")
            return qsTr("F2 critical-frequency forecast used to estimate ionospheric support for HF paths.")
        if (layerId === "nvis")
            return qsTr("NVIS forecast derived from the current foF2 map; it is kept as a separate operational layer.")
        if (layerId === "es")
            return qsTr("Sporadic-E probability forecast. Cached data fades after its validity window and remains labelled stale.")
        if (layerId === "aurora")
            return qsTr("Auroral propagation forecast. The temporal legend shows source age and validity.")
        if (layerId === "earthquakes")
            return qsTr("Global earthquakes of magnitude 2.5 or greater reported by USGS during the last day.")
        if (layerId === "wildfires")
            return qsTr("Open global wildfire events published by NASA EONET.")
        if (layerId === "offline")
            return qsTr("Offline mode pauses cloud/network services, keeps ADIF, logbook, cache and radio activity available, and can use an imported user-provided world raster.")
        return ""
    }

    function rosterStatusColor(status) {
        if (status === "NEW")
            return "#ffb347"
        if (status === "UNCONFIRMED")
            return "#f6c344"
        if (status === "CONFIRMED")
            return root.accentGreen
        return root.textSecondary
    }

    function rosterStatusFill(status) {
        if (status === "NEW")
            return "#24ffb347"
        if (status === "UNCONFIRMED")
            return "#24f6c344"
        if (status === "CONFIRMED")
            return "#242ecc71"
        return "#181d2a3b"
    }

    function openCallLookup(rawCall) {
        var raw = String(rawCall || "").trim()
        var segments = raw.split("/")
        var best = ""
        for (var i = 0; i < segments.length; ++i) {
            if (segments[i].length > best.length)
                best = segments[i]
        }
        var call = (best.length ? best : raw).toUpperCase()
            .replace(/[^A-Z0-9]/g, "")
        if (call.length === 0)
            return
        var provider = mapLayers ? mapLayers.callLookupProvider : "QRZ"
        var base = provider === "HamQTH"
            ? "https://www.hamqth.com/"
            : (provider === "QRZCQ"
               ? "https://www.qrzcq.com/call/" : "https://www.qrz.com/db/")
        Qt.openUrlExternally(base + call)
    }

    function requestPskData() {
        if (!visible || !engine || !mapLayers || !mapLayers.layerModel
                || !mapLayers.layerModel.layerEnabled("psk")
                || mapLayers.layerModel.layerEnabled("offline")
                || engine.pskMapSpotFetching)
            return
        engine.fetchPskMapSpots()
    }

    function updateMoonOverlay() {
        if (!externalOverlays)
            return
        var moonEnabled = mapLayers && mapLayers.layerModel
            && mapLayers.layerModel.layerEnabled("moon")
        var stationGrid = engine ? String(engine.grid || "").trim() : ""
        if (!moonEnabled || !engine || stationGrid.length < 4) {
            externalOverlays.setMoonData(false, 0, 0, 0, 0, 0, 0)
            return
        }
        externalOverlays.updateMoonForStation(
            Number(engine.latFromGrid(stationGrid)),
            Number(engine.lonFromGrid(stationGrid)))
    }

    onMapLayersChanged: Qt.callLater(root.updateMoonOverlay)
    onExternalOverlaysChanged: Qt.callLater(root.updateMoonOverlay)

    function locateMoon() {
        if (!worldMap || !externalOverlays
                || !externalOverlays.moonDataAvailable)
            return
        worldMap.focusLocation(
            Number(externalOverlays.moonSublunarLongitude),
            Number(externalOverlays.moonSublunarLatitude),
            90, 54)
    }

    function coordinateText(value, positiveSuffix, negativeSuffix) {
        var coordinate = Number(value)
        return Math.abs(coordinate).toFixed(1) + "°"
            + (coordinate >= 0 ? positiveSuffix : negativeSuffix)
    }

    function statisticsDate(epoch) {
        var value = Number(epoch || 0)
        if (value <= 0)
            return qsTr("n/a")
        return Qt.formatDateTime(new Date(value), "yyyy-MM-dd")
    }

    function syncTxState() {
        if (!engine || !worldMap)
            return
        var txTargetCall = engine.currentTx === 6 ? "" : engine.dxCall
        var txTargetGrid = engine.currentTx === 6 ? "" : engine.dxGrid
        worldMap.setTransmitState(!!(engine.transmitting || engine.tuning),
                                  txTargetCall,
                                  txTargetGrid,
                                  engine.mode)
    }

    function syncCoverage() {
        if (!worldMap)
            return
        worldMap.setCoverageCells(mapLayers ? mapLayers.coverageCells : [])
    }

    function scheduleMapSnapshotSync() {
        mapSnapshotSyncPending = true
        // A live snapshot is visual information, not decode-critical work.
        // Keep normal systems responsive while avoiding repeated map rebuilds
        // during a short UI-pressure window.
        mapSnapshotSyncTimer.interval = root.engine && root.engine.cpuPressureNow
                && root.engine.cpuPressureNow() ? 750 : 450
        mapSnapshotSyncTimer.restart()
    }

    function syncSpotPaths() {
        if (!worldMap || !mapLayers || !mapLayers.pskLayerEnabled)
            return
        var paths = mapLayers.spotPaths || []
        // The renderer keeps a bounded contact list.  These paths are a
        // recent animated view of reporter -> station directionality.
        for (var index = 0; index < Math.min(paths.length, 80); ++index) {
            var path = paths[index]
            var fromGrid = String(path.fromGrid || "")
            var toGrid = String(path.toGrid || "")
            if (fromGrid.length < 4 || toGrid.length < 4)
                continue
            // A spot path is not a synthetic station.  Passing the old
            // PSKPATH<n> identifier as a callsign made the renderer label
            // every PSK route with that implementation detail and displaced
            // real calls from the bounded contact set.  The source endpoint
            // below is the spotted station (toGrid), so label it with toCall
            // when available.  Keep anonymous paths visible, just unlabelled.
            var spottedCall = String(path.toCall || "").trim()
            worldMap.addContact(spottedCall, toGrid, fromGrid, 0)
        }
    }

    function rosterColumnValue(row, column) {
        if (!row)
            return ""
        if (column === "Grid") return rosterGridLabel(row)
        if (column === "Grid source") {
            if (!row.grid)
                return ""
            var origin = row.gridOrigin || qsTr("Unspecified")
            var reliability = row.gridReliability || ""
            return reliability.length > 0 ? origin + " · " + reliability : origin
        }
        if (column === "Band") return row.band || ""
        if (column === "Mode") return row.mode || ""
        if (column === "SNR") return row.snr !== undefined ? String(row.snr) + " dB" : ""
        if (column === "DT") return row.dt !== undefined ? "DT " + Number(row.dt).toFixed(2) : ""
        if (column === "DXCC") return row.dxcc || ""
        if (column === "Continent") return row.continent || ""
        if (column === "CQ zone") return row.cqZone ? "CQ " + row.cqZone : ""
        if (column === "ITU zone") return row.ituZone ? "ITU " + row.ituZone : ""
        if (column === "State") return row.state || ""
        if (column === "County") return row.county || ""
        if (column === "POTA") return row.pota || ""
        if (column === "IOTA") return row.iota || ""
        if (column === "WPX") return row.wpx || ""
        if (column === "LoTW age") return row.lotwAgeDays >= 0 ? "LoTW " + row.lotwAgeDays + "d" : ""
        if (column === "eQSL age") return row.eqslAgeDays >= 0 ? "eQSL " + row.eqslAgeDays + "d" : ""
        if (column === "OQRS") return row.oqrs ? "OQRS" : ""
        if (column === "Age") return row.ageMinutes >= 0 ? row.ageMinutes + "m" : ""
        if (column === "Source") {
            var sourceText = row.sourceSummary || row.source || ""
            return Number(row.sourceCount || 1) > 1
                ? (row.corroborationLevel || qsTr("Corroborated")) + ": " + sourceText
                : sourceText
        }
        return ""
    }

    function rosterGridLabel(row) {
        if (!row || !row.grid)
            return ""
        return String(row.grid) + (row.gridMarker ? " " + row.gridMarker : "")
    }

    function rosterColumnSummary(row) {
        if (!row || !mapLayers)
            return ""
        var values = []
        var columns = mapLayers.rosterVisibleColumns || []
        for (var index = 0; index < columns.length; ++index) {
            var value = rosterColumnValue(row, columns[index])
            if (value.length > 0)
                values.push(value)
        }
        if (Number(row.sourceCount || 1) > 1
                && columns.indexOf("Source") < 0) {
            values.push("✓ " + (row.sourceSummary || row.source || ""))
        }
        return values.join("  ·  ")
    }

    function showGridPreview(details, x, y) {
        if (!details || !details.grid || gridDetailsPinned)
            return
        hoveredGridDetails = details
        hoveredGridX = Number(x)
        hoveredGridY = Number(y)
        gridPreviewVisible = true
    }

    function hideGridPreview() {
        gridPreviewVisible = false
        hoveredHistoricalGridDetails = ({})
        hoveredLiveGridDetails = ({})
    }

    function showGridSegmentPreview(details, x, y, segment) {
        showGridPreview(details, x, y)
        if (String(segment || "Combined") === "Historical")
            hoveredHistoricalGridDetails = details
        else if (String(segment || "Combined") === "Live")
            hoveredLiveGridDetails = details
        else {
            hoveredHistoricalGridDetails = details
            hoveredLiveGridDetails = details
        }
    }

    function pinGridDetails(details) {
        if (!details || !details.grid || !mapLayers)
            return
        hoveredGridDetails = details
        gridPreviewVisible = false
        gridDetailsPinned = true
        mapLayers.selectGrid(details.grid)
    }

    function closeGridDetails() {
        gridDetailsPinned = false
        if (mapLayers)
            mapLayers.clearGridSelection()
    }

    function focusSelectedGrid() {
        if (!worldMap || !engine || !mapLayers || !mapLayers.selectedGrid)
            return
        worldMap.focusLocation(
            Number(engine.lonFromGrid(mapLayers.selectedGrid)),
            Number(engine.latFromGrid(mapLayers.selectedGrid)),
            28, 18)
    }

    function focusActiveQso() {
        if (!worldMap || !engine)
            return
        var grid = String(engine.dxGrid || "").trim().toUpperCase()
        if (grid.length < 4 || !engine.lonFromGrid || !engine.latFromGrid)
            return
        if (!root.qsoViewportFocused && worldMap.viewportState)
            root.savedViewportBeforeQsoFocus = worldMap.viewportState()
        worldMap.focusLocation(Number(engine.lonFromGrid(grid)),
                                Number(engine.latFromGrid(grid)), 28, 18)
        root.qsoViewportFocused = true
    }

    function restoreViewportAfterQsoFocus() {
        if (!worldMap || !root.qsoViewportFocused)
            return
        if (worldMap.setViewportState
                && Object.keys(root.savedViewportBeforeQsoFocus).length > 0)
            worldMap.setViewportState(root.savedViewportBeforeQsoFocus)
        else
            worldMap.resetView()
        root.savedViewportBeforeQsoFocus = ({})
        root.qsoViewportFocused = false
    }

    function scheduleRebuild() {
        if (!engine || !worldMap)
            return
        rebuildTimer.restart()
    }

    function decoderFeedAllowed() {
        if (!mapLayers)
            return true
        var source = String(mapLayers.sourceFilter || "All").trim().toLowerCase()
        return source === "all" || source === "decoder"
    }

    function updateConsumerReady() {
        if (!engine || !engine.setWorldMapConsumerReady)
            return
        engine.setWorldMapConsumerReady(root, !!root.visible && !!worldMap)
    }

    function initializeMap() {
        if (!worldMap)
            return
        worldMap.setActive(visible)
        root.syncMapSettings()
        root.syncCoverage()
        root.syncTxState()
        root.updateConsumerReady()
        if (visible)
            root.scheduleRebuild()
    }

    Timer {
        id: rebuildTimer
        // 1.0.209 — Debounce 1s. Era interval:0 (fires next tick) che con
        // 13 signal del bridge che invocavano scheduleRebuild() (decodeList,
        // rxDecodeList, transmitting, tuning, dxCall, dxGrid, currentTx,
        // txEnabled, qsoProgress, autoCqRepeat, mode, settingValue, grid)
        // significava clearContacts + replayWorldMapFeed (re-itera 500 entries
        // + addContact ognuno + paint) 2 volte/sec. Mappa mai stabile, "non
        // si vedeva" perche' sempre in rebuild. Ora rebuild totale solo
        // quando l'utente apre il pannello o cambia home grid; i nuovi
        // contact arrivano incrementali via onWorldMapContactAdded.
        interval: 1000
        repeat: false
        onTriggered: {
            if (!root.engine || !root.worldMap)
                return
            worldMap.clearContacts()
            root.syncMapSettings()
            root.syncCoverage()
            if ((!root.mapLayers || root.mapLayers.liveLayerEnabled)
                    && root.decoderFeedAllowed())
                root.engine.replayWorldMapFeed()
            root.syncSpotPaths()
            root.syncTxState()
        }
    }

    Timer {
        interval: 60000
        repeat: true
        triggeredOnStart: true
        running: root.visible && root.mapLayers && root.mapLayers.layerModel
                 && root.mapLayers.layerModel.layerEnabled("moon")
        onTriggered: root.updateMoonOverlay()
    }

    Timer {
        id: pskInitialFetchTimer
        interval: 1200
        repeat: false
        onTriggered: root.requestPskData()
    }

    Timer {
        id: pskMapRefreshTimer
        interval: 65000
        repeat: true
        running: root.visible && root.mapLayerEnabled("psk")
                 && !root.mapLayerEnabled("offline")
        onTriggered: root.requestPskData()
    }

    Timer {
        id: mapSnapshotSyncTimer
        interval: 450
        repeat: false
        onTriggered: {
            root.mapSnapshotSyncPending = false
            if (!root.visible || !root.worldMap)
                return
            root.ensureActivityBand()
            root.syncCoverage()
            if (activityChart.visible && activityChart.width > 1 && activityChart.height > 1)
                activityChart.requestPaint()
        }
    }

    Connections {
        target: root.mapLayers ? root.mapLayers.layerModel : null
        function onLayerToggled(layerId, enabled) {
        if (layerId === "moon") {
                root.moonLocatePending = enabled
                root.updateMoonOverlay()
            } else if (layerId === "psk" && enabled) {
                pskInitialFetchTimer.restart()
            }
        }
        function onLayerStyleChanged(layerId) {
            root.syncMapSettings()
        }
    }

    Component.onCompleted: {
        // 1.0.213 — pausa l'animation timer del widget legacy quando il
        // pannello non e' visibile (riduce sprechi CPU ~50% in idle dietro
        // ad altri tab/pop-out chiusi).
        root.initializeMap()
        root.updateMoonOverlay()
        pskInitialFetchTimer.restart()
    }
    Component.onDestruction: {
        if (engine && engine.setWorldMapConsumerReady)
            engine.setWorldMapConsumerReady(root, false)
    }
    onVisibleChanged: {
        if (worldMap)
            worldMap.setActive(visible)
        root.updateConsumerReady()
        if (visible) {
            scheduleRebuild()
            Qt.callLater(root.updateMoonOverlay)
            pskInitialFetchTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.16)
            radius: 4
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.detachable && !root.detached ? 30 : 6
                anchors.rightMargin: 6
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 8

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: secondaryCyan
                    opacity: root.visible ? 1.0 : 0.5
                }

                Text {
                    text: qsTr("Live Map")
                    font.pixelSize: 14
                    font.bold: true
                    color: secondaryCyan
                }

                Item { Layout.fillWidth: true }

                // 1.0.223 — Toolbar zoom + greyline. Rifatti come Rectangle
                // inline (no Loader+Component) per evitare il bug 1.0.221 in
                // cui Layout.preferredWidth/Height era sul template Component
                // ma non veniva propagato al Loader -> bottoni 0x0 = invisibili
                // al click. Ora ogni bottone e' un Rectangle diretto figlio
                // del RowLayout, le Layout attached funzionano.
                Rectangle {
                    id: zoomOutBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: zoomOutMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: zoomOutMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "−"
                        font.pixelSize: 14
                        font.bold: true
                        color: zoomOutMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: zoomOutMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.zoomOut(1.4)
                    }
                    ToolTip.visible: zoomOutMa.containsMouse
                    ToolTip.text: qsTr("Zoom out")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: zoomInBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: zoomInMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: zoomInMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        font.pixelSize: 14
                        font.bold: true
                        color: zoomInMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: zoomInMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.zoomIn(1.4)
                    }
                    ToolTip.visible: zoomInMa.containsMouse
                    ToolTip.text: qsTr("Zoom in")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: resetBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: resetMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: resetMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "⌂"
                        font.pixelSize: 12
                        font.bold: true
                        color: resetMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.qsoViewportFocused
                            ? root.restoreViewportAfterQsoFocus()
                            : (root.worldMap ? root.worldMap.resetView() : undefined)
                    }
                    ToolTip.visible: resetMa.containsMouse
                    ToolTip.text: root.qsoViewportFocused
                        ? qsTr("Restore previous view") : qsTr("Reset view (auto-fit)")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: greylineBtn
                    property bool greylineOn: engine
                        ? root.coerceBool(engine.getSetting("ShowGreyline", true), true) : true
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    // 1.0.227 — Reference esplicita greylineBtn.greylineOn per chiudere
                    // scope chain ambiguity. Pre-1.0.227 alcune builds Qt6.11 lamentavano
                    // ReferenceError "greylineOn is not defined" su hover/repaint cycle
                    // (deja-vu 1.0.205 TypeError flood -> logger sync stalls main thread).
                    color: greylineMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : (greylineBtn.greylineOn ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18) : "transparent")
                    border.color: (greylineMa.containsMouse || greylineBtn.greylineOn) ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "☼"
                        font.pixelSize: 12
                        font.bold: true
                        color: (greylineMa.containsMouse || greylineBtn.greylineOn) ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: greylineMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            greylineBtn.greylineOn = !greylineBtn.greylineOn
                            if (root.engine)
                                root.engine.setSetting("ShowGreyline", greylineBtn.greylineOn)
                            if (root.worldMap)
                                root.worldMap.setGreylineEnabled(greylineBtn.greylineOn)
                        }
                    }
                    ToolTip.visible: greylineMa.containsMouse
                    ToolTip.text: qsTr("Toggle day/night greyline overlay")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: qsoFocusBtn
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 18
                    radius: 4
                    color: qsoFocusMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : (root.qsoViewportFocused
                           ? Qt.rgba(246/255, 195/255, 68/255, 0.18)
                           : "transparent")
                    border.color: qsoFocusMa.containsMouse || root.qsoViewportFocused
                        ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: root.qsoViewportFocused ? "↶" : "DX"
                        font.pixelSize: 10
                        font.bold: true
                        color: qsoFocusMa.containsMouse || root.qsoViewportFocused
                            ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: qsoFocusMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.qsoViewportFocused
                            ? root.restoreViewportAfterQsoFocus()
                            : root.focusActiveQso()
                    }
                    ToolTip.visible: qsoFocusMa.containsMouse
                    ToolTip.text: root.qsoViewportFocused
                        ? qsTr("Restore previous map view")
                        : qsTr("Auto-fit active QSO")
                    ToolTip.delay: 500
                }

                Rectangle {
                    visible: root.detachable
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 18
                    radius: 4
                    color: liveMapDetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25) : "transparent"
                    border.color: liveMapDetachMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.detached ? qsTr("Dock") : qsTr("POP")
                        font.pixelSize: 10
                        font.bold: true
                        color: liveMapDetachMA.containsMouse ? secondaryCyan : textSecondary
                    }

                    MouseArea {
                        id: liveMapDetachMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.detachRequested()
                    }

                    ToolTip.visible: liveMapDetachMA.containsMouse
                    ToolTip.text: root.detached ? qsTr("Dock Live Map") : qsTr("Detach Live Map")
                    ToolTip.delay: 500
                }

                Text {
                    text: engine && engine.grid ? engine.grid : ""
                    font.pixelSize: 10
                    color: textSecondary
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.78)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.22)
            border.width: 1
            radius: 4
            clip: true

            Flickable {
                id: layerFlickable
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                contentWidth: layerRow.implicitWidth
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentWidth > width

                function showFirstControl() {
                    contentX = 0
                }

                Component.onCompleted: initialToolbarPosition.restart()
                onVisibleChanged: {
                    if (visible)
                        initialToolbarPosition.restart()
                }

                Timer {
                    id: initialToolbarPosition
                    interval: 350
                    repeat: false
                    onTriggered: layerFlickable.showFirstControl()
                }

                ScrollBar.horizontal: ScrollBar {
                    policy: layerFlickable.contentWidth > layerFlickable.width
                        ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                Row {
                    id: layerRow
                    height: parent.height
                    spacing: 6

                    Repeater {
                        model: root.mapLayers ? root.mapLayers.layerModel : null

                        delegate: LayerToggle {
                            required property string layerId
                            required property string layerColor
                            required property bool layerEnabled
                            required property int layerCount

                            anchors.verticalCenter: parent.verticalCenter
                            activeColor: layerColor
                            checked: layerEnabled
                            helpText: root.layerDescription(layerId)
                            onToggled: function(value) {
                                if (root.mapLayers)
                                    root.mapLayers.layerModel.setLayerEnabled(layerId, value)
                            }
                        }
                    }

                    BusyIndicator {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 18
                        running: (root.mapLayers && root.mapLayers.loading)
                                 || (root.externalOverlays
                                     && root.externalOverlays.loading)
                        visible: running
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.mapLayers
                            ? qsTr("%1 worked / %2 confirmed")
                                  .arg(root.mapLayers.workedGridCount)
                                  .arg(root.mapLayers.confirmedGridCount)
                            : ""
                        color: root.textSecondary
                        font.pixelSize: 10
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: root.mapOperations
                            ? root.mapOperations.dataViewMode : qsTr("Live")
                        activeColor: root.secondaryCyan
                        checked: true
                        helpText: qsTr("Cycle Live, Logbook and combined map views")
                        onToggled: function(value) {
                            if (root.mapOperations)
                                root.mapOperations.cycleDataView()
                        }
                    }

                    ComboBox {
                        id: projectionCombo
                        anchors.verticalCenter: parent.verticalCenter
                        width: 168
                        height: 24
                        model: root.mapOperations
                            ? root.mapOperations.availableProjections : []
                        currentIndex: root.mapOperations
                            ? Math.max(0, model.indexOf(
                                           root.mapOperations.mapProjection)) : 0
                        font.pixelSize: 9
                        onActivated: {
                            if (root.mapOperations)
                                root.mapOperations.mapProjection = currentText
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Map projection")
                    }

                    ComboBox {
                        id: presetCombo
                        anchors.verticalCenter: parent.verticalCenter
                        width: 112
                        height: 24
                        model: root.mapOperations
                            ? root.mapOperations.mapPresets : []
                        currentIndex: root.mapOperations
                            ? Math.max(0, model.indexOf(
                                           root.mapOperations.activeMapPreset)) : 0
                        font.pixelSize: 9
                        onActivated: {
                            if (root.mapOperations)
                                root.mapOperations.applyMapPreset(currentText)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Map preset")
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("SHOT")
                        activeColor: root.secondaryCyan
                        helpText: qsTr("Save a screenshot of the map")
                        onToggled: function(value) {
                            root.captureMapScreenshot()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("ROSTER")
                        activeColor: root.primaryBlue
                        helpText: qsTr("Open the independent call roster")
                        onToggled: function(value) {
                            mapOperationsWindows.openRoster()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("STATS")
                        activeColor: root.primaryBlue
                        helpText: qsTr("Open detailed statistics")
                        onToggled: function(value) {
                            mapOperationsWindows.openStatistics()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("COND")
                        activeColor: root.primaryBlue
                        helpText: qsTr("Open radio and propagation conditions")
                        onToggled: function(value) {
                            mapOperationsWindows.openConditions()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: root.intelligencePanelRequested ? qsTr("HIDE DETAILS")
                                                               : qsTr("DETAILS")
                        activeColor: root.primaryBlue
                        checked: root.intelligencePanelRequested
                        onToggled: function(value) {
                            root.intelligencePanelRequested = value
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
            border.width: 1
            radius: 4
            clip: true

            Loader {
                id: worldMapLoader
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 2
                anchors.bottomMargin: 2
                anchors.leftMargin: 2
                anchors.rightMargin: intelligencePanel.visible && !root.compactIntelligencePanel
                    ? intelligencePanel.width + 8 : 2
                active: root.visible
                sourceComponent: root.gpuLiveMapEnabled ? gpuWorldMapComponent : painterWorldMapComponent
                onLoaded: {
                    // A custom build may not contain the QSB greyline shader.
                    // Fall back to the painter implementation rather than
                    // silently presenting a map with no day/night overlay.
                    if (root.gpuLiveMapEnabled && worldMapLoader.item
                            && worldMapLoader.item.greylineShaderAvailable === false) {
                        console.warn("Live Map greyline shader unavailable; falling back to CPU WorldMapItem")
                        root.gpuLiveMapEnabled = false
                        return
                    }
                    root.initializeMap()
                    Qt.callLater(root.updateMoonOverlay)
                }
                onStatusChanged: {
                    root.updateConsumerReady()
                    if (status === Loader.Error && root.gpuLiveMapEnabled) {
                        console.warn("Live Map GPU component failed to load; falling back to CPU WorldMapItem")
                        root.gpuLiveMapEnabled = false
                    }
                }

                Component {
                    id: painterWorldMapComponent
                    WorldMapItem {
                        onContactClicked: function(call, grid) {
                            if (root.engine)
                                root.engine.processMapContactClick(call, grid)
                        }
                        onCoverageCellHovered: function(details, x, y) {
                            root.showGridPreview(details, x, y)
                        }
                        onCoverageCellSegmentHovered: function(details, x, y, segment) {
                            root.showGridSegmentPreview(details, x, y, segment)
                        }
                        onCoverageCellHoverEnded: root.hideGridPreview()
                        onCoverageCellClicked: function(details, x, y) {
                            root.pinGridDetails(details)
                        }
                        onOperationalMarkerClicked: function(details, x, y) {
                            root.showOperationalDetails(details, x, y)
                        }
                        onGeographicFeatureClicked: function(details, x, y) {
                            root.showGeographicDetails(details, x, y)
                        }
                        onGeographicFeatureHovered: function(details, x, y) {
                            root.showGeographicPreview(details, x, y)
                        }
                        onGeographicFeatureHoverEnded: root.hideGeographicPreview()
                    }
                }

                Component {
                    id: gpuWorldMapComponent
                    WorldMapGpuItem {
                        onContactClicked: function(call, grid) {
                            if (root.engine)
                                root.engine.processMapContactClick(call, grid)
                        }
                        onCoverageCellHovered: function(details, x, y) {
                            root.showGridPreview(details, x, y)
                        }
                        onCoverageCellSegmentHovered: function(details, x, y, segment) {
                            root.showGridSegmentPreview(details, x, y, segment)
                        }
                        onCoverageCellHoverEnded: root.hideGridPreview()
                        onCoverageCellClicked: function(details, x, y) {
                            root.pinGridDetails(details)
                        }
                        onOperationalMarkerClicked: function(details, x, y) {
                            root.showOperationalDetails(details, x, y)
                        }
                        onGeographicFeatureClicked: function(details, x, y) {
                            root.showGeographicDetails(details, x, y)
                        }
                        onGeographicFeatureHovered: function(details, x, y) {
                            root.showGeographicPreview(details, x, y)
                        }
                        onGeographicFeatureHoverEnded: root.hideGeographicPreview()
                    }
                }
            }

            Rectangle {
                id: operationalDetailsCard
                visible: root.operationalDetailsVisible
                z: 10
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.selectedMapX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.selectedMapY + 14))
                width: Math.min(330, parent.width - 16)
                height: 214
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.operationalValue("color")
                    || root.secondaryCyan

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: (root.operationalValue("reference")
                                   || root.operationalValue("type")
                                   || qsTr("Map item")).toString().toUpperCase()
                            color: root.secondaryCyan
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            text: "×"
                            onClicked: {
                                root.operationalDetailsVisible = false
                                if (root.mapOperations)
                                    root.mapOperations.clearSelectedPotaPark()
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.operationalValue("name")
                              || root.operationalValue("label")
                              || root.operationalValue("call")
                              || qsTr("Operational marker")
                        color: root.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [
                            root.operationalValue("call"),
                            root.operationalValue("grid"),
                            root.operationalValue("frequency"),
                            root.operationalValue("mode")
                        ].filter(function(value) {
                            return String(value || "").length > 0
                        }).join("  ·  ")
                        color: root.textSecondary
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: String(root.operationalValue("type") || "").toUpperCase() === "POTA"
                        Layout.fillWidth: true
                        text: [
                            root.operationalValue("role"),
                            root.operationalValue("expired") ? qsTr("EXPIRED")
                                                                : qsTr("VALID"),
                            root.operationalValue("worked") ? qsTr("WORKED") : qsTr("NEW"),
                            root.operationalValue("confirmed") ? qsTr("CONFIRMED") : qsTr("UNCONFIRMED")
                        ].filter(function(value) {
                            return String(value || "").length > 0
                        }).join("  ·  ")
                        color: root.operationalValue("expired")
                            ? root.accentAmber : root.textSecondary
                        font.pixelSize: 9
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: String(root.operationalValue("type") || "").toUpperCase() === "SATELLITE"
                        Layout.fillWidth: true
                        text: [
                            root.operationalValue("elevation") !== undefined
                                ? qsTr("EL %1°").arg(Number(root.operationalValue("elevation")).toFixed(1)) : "",
                            root.operationalValue("azimuth") !== undefined
                                ? qsTr("AZ %1°").arg(Number(root.operationalValue("azimuth")).toFixed(1)) : "",
                            root.operationalValue("rangeKm") !== undefined
                                ? qsTr("%1 km").arg(Number(root.operationalValue("rangeKm")).toFixed(0)) : "",
                            root.operationalValue("visible") ? qsTr("VISIBLE") : qsTr("BELOW HORIZON")
                        ].filter(function(value) {
                            return String(value || "").length > 0
                        }).join("  ·  ")
                        color: root.operationalValue("visible") ? root.accentGreen : root.textSecondary
                        font.pixelSize: 9
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: String(root.operationalValue("type") || "").toUpperCase() === "POTA"
                        Layout.fillWidth: true
                        text: {
                            var remaining = Number(root.operationalValue("remainingSeconds") || 0)
                            if (root.operationalValue("expired"))
                                return qsTr("Spot scaduto")
                            if (remaining > 0)
                                return qsTr("Spot valido ancora %1 s").arg(remaining)
                            return qsTr("Validità non dichiarata dal provider")
                        }
                        color: root.textSecondary
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: root.operationalValue("comments")
                              || root.operationalValue("locationDesc")
                              || root.operationalValue("description")
                              || qsTr("Click CALL to start a QSO or ROTATE to aim the antenna.")
                        color: root.textSecondary
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        OperationalActionButton {
                            label: qsTr("CALL")
                            Layout.minimumWidth: 50
                            Layout.preferredWidth: 54
                            Layout.maximumWidth: 58
                            Layout.minimumHeight: 28
                            Layout.preferredHeight: 28
                            Layout.maximumHeight: 28
                            actionEnabled: String(root.operationalValue("call")).length > 0
                            unavailableHint: qsTr("This map marker has no callsign.")
                            onInvoked: root.engine.processMapRosterCall(
                                root.operationalValue("call"),
                                root.operationalValue("grid"))
                        }
                        OperationalActionButton {
                            label: "QRZ"
                            Layout.minimumWidth: 48
                            Layout.preferredWidth: 52
                            Layout.maximumWidth: 56
                            Layout.minimumHeight: 28
                            Layout.preferredHeight: 28
                            Layout.maximumHeight: 28
                            actionEnabled: String(root.operationalValue("call")).length > 0
                            unavailableHint: qsTr("This map marker has no callsign.")
                            onInvoked: root.openCallLookup(
                                root.operationalValue("call"))
                        }
                        OperationalActionButton {
                            label: qsTr("ROTATE")
                            Layout.minimumWidth: 66
                            Layout.preferredWidth: 70
                            Layout.maximumWidth: 74
                            Layout.minimumHeight: 28
                            Layout.preferredHeight: 28
                            Layout.maximumHeight: 28
                            actionEnabled: root.mapOperations
                                           && root.mapOperations.rotatorEnabled
                            unavailableHint: qsTr("Enable a rotator to aim the antenna.")
                            onInvoked: root.aimSelectedMarker()
                        }
                        OperationalActionButton {
                            label: qsTr("TRACK")
                            Layout.minimumWidth: 60
                            Layout.preferredWidth: 64
                            Layout.maximumWidth: 68
                            Layout.minimumHeight: 28
                            Layout.preferredHeight: 28
                            Layout.maximumHeight: 28
                            actionEnabled: root.mapOperations
                                           && root.mapOperations.rotatorEnabled
                            unavailableHint: qsTr("Enable a rotator to track this satellite.")
                            onInvoked: root.trackSelectedMarker()
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                id: geographicDetailsCard
                visible: root.geographicDetailsVisible
                z: 9
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.selectedMapX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.selectedMapY + 14))
                width: Math.min(280, parent.width - 16)
                height: root.selectedGeographicDetails.type === "earthquake" ? 156 : 104
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.selectedGeographicDetails.color
                    || root.secondaryCyan

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.selectedGeographicDetails.label
                                  || root.selectedGeographicDetails.county
                                  || root.selectedGeographicDetails.state
                                  || qsTr("Geographic area")
                            color: root.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            text: "×"
                            onClicked: root.geographicDetailsVisible = false
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [
                            root.selectedGeographicDetails.type,
                            root.selectedGeographicDetails.state,
                            root.selectedGeographicDetails.county
                        ].filter(function(value) {
                            return String(value || "").length > 0
                        }).join("  ·  ")
                        color: root.textSecondary
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
                    }
                    Text {
                        visible: root.selectedGeographicDetails.type === "earthquake"
                        Layout.fillWidth: true
                        text: root.earthquakeSummary(root.selectedGeographicDetails)
                        color: root.selectedGeographicDetails.color || root.secondaryCyan
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Text {
                        visible: root.selectedGeographicDetails.type === "earthquake"
                        Layout.fillWidth: true
                        text: root.selectedGeographicDetails.timeUtc || ""
                        color: root.textSecondary
                        font.pixelSize: 9
                    }
                    RowLayout {
                        visible: root.selectedGeographicDetails.type === "earthquake"
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.selectedGeographicDetails.tsunami
                                  ? qsTr("Tsunami flag reported") : ""
                            color: "#ffb56a"
                            font.pixelSize: 9
                        }
                        ToolButton {
                            visible: String(root.selectedGeographicDetails.url || "").length > 0
                            text: qsTr("USGS")
                            onClicked: Qt.openUrlExternally(root.selectedGeographicDetails.url)
                        }
                    }
                }
            }

            Rectangle {
                id: geographicHoverCard
                visible: root.geographicPreviewVisible
                         && root.hoveredGeographicDetails
                         && root.hoveredGeographicDetails.type === "earthquake"
                z: 8
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.hoveredGeographicX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.hoveredGeographicY + 14))
                width: Math.min(272, parent.width - 16)
                height: 94
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.hoveredGeographicDetails.color || "#ff9b4b"

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3
                    Text {
                        width: parent.width
                        text: root.hoveredGeographicDetails.place
                              || root.hoveredGeographicDetails.label || qsTr("Earthquake")
                        color: root.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: root.earthquakeSummary(root.hoveredGeographicDetails)
                        color: root.hoveredGeographicDetails.color || "#ffb15f"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: root.hoveredGeographicDetails.timeUtc || ""
                        color: root.textSecondary
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                    Text {
                        text: qsTr("Click for event details")
                        color: root.textSecondary
                        font.pixelSize: 8
                    }
                }
            }

            Rectangle {
                id: gridHoverCard
                visible: root.gridPreviewVisible
                         && root.hoveredGridDetails
                         && !!root.hoveredGridDetails.grid
                z: 8
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.hoveredGridX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.hoveredGridY + 14))
                width: 232
                height: root.hoveredGridDetails.split ? 108 : 94
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.96)
                border.width: 1
                border.color: root.secondaryCyan

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3
                    Text {
                        text: root.hoveredGridDetails.grid || ""
                        color: root.secondaryCyan
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Text {
                        text: qsTr("%1 worked  %2 confirmed")
                            .arg(Number(root.hoveredGridDetails.workedCount || 0))
                            .arg(Number(root.hoveredGridDetails.confirmedCount || 0))
                        color: root.textPrimary
                        font.pixelSize: 10
                    }
                    Text {
                        text: qsTr("%1 live  %2 PSK  %3")
                            .arg(Number(root.hoveredGridDetails.activeCount || 0))
                            .arg(Number(root.hoveredGridDetails.pskCount || 0))
                            .arg(root.hoveredGridDetails.liveStatus || "")
                        color: root.textSecondary
                        font.pixelSize: 9
                    }
                    Text {
                        text: (root.hoveredGridDetails.historicalStatus || "")
                              + (root.hoveredGridDetails.ageSeconds !== undefined
                                 ? qsTr("  last %1 s ago")
                                       .arg(Number(root.hoveredGridDetails.ageSeconds))
                                 : "")
                        color: root.textSecondary
                        font.pixelSize: 8
                    }
                    Text {
                        visible: !!root.hoveredGridDetails.split
                        text: qsTr("Hovered half: %1")
                            .arg(root.hoveredGridDetails.splitSegment || qsTr("Combined"))
                        color: "#f6c344"
                        font.pixelSize: 8
                    }
                    Text {
                        visible: !!root.hoveredGridDetails.split
                        text: qsTr("Historical hover: %1 · Live hover: %2")
                            .arg(root.hoveredHistoricalGridDetails.grid
                                     ? qsTr("ready") : qsTr("—"))
                            .arg(root.hoveredLiveGridDetails.grid
                                     ? qsTr("ready") : qsTr("—"))
                        color: root.textSecondary
                        font.pixelSize: 8
                    }
                }
            }

            Rectangle {
                id: gridDetailsCard
                visible: root.gridDetailsPinned && root.mapLayers
                         && !!root.mapLayers.selectedGrid
                z: 7
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                width: Math.max(260, Math.min(root.detached ? 520 : 460,
                                               parent.width - 24))
                height: Math.max(190, Math.min(520, parent.height - 24))
                radius: 4
                clip: true
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.secondaryCyan
                Material.theme: Material.Dark
                Material.accent: root.primaryBlue
                Material.foreground: root.textPrimary
                Material.background: root.bgDeep

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: root.mapLayers
                                    ? qsTr("GRID %1").arg(root.mapLayers.selectedGrid)
                                    : ""
                                color: root.secondaryCyan
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: {
                                    var summary = root.mapLayers
                                        ? root.mapLayers.selectedGridSummary : ({})
                                    return qsTr("%1 QSO  %2 confirmed  %3 live  %4 PSK")
                                        .arg(Number(summary.workedCount || 0))
                                        .arg(Number(summary.confirmedCount || 0))
                                        .arg(Number(summary.activeCount || 0))
                                        .arg(Number(summary.pskCount || 0))
                                }
                                color: root.textSecondary
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                        }
                        BusyIndicator {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            running: root.mapLayers
                                     && root.mapLayers.gridDetailsLoading
                            visible: running
                        }
                        ToolButton {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 26
                            text: "◎"
                            onClicked: root.focusSelectedGrid()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Center this grid")
                        }
                        ToolButton {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 26
                            text: "×"
                            onClicked: root.closeGridDetails()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Close grid details")
                        }
                    }

                    TabBar {
                        id: gridDetailsTabs
                        Layout.fillWidth: true
                        TabButton {
                            text: root.mapLayers
                                ? qsTr("LIVE %1").arg(root.mapLayers.selectedGridLive.length)
                                : qsTr("LIVE")
                            font.pixelSize: 9
                        }
                        TabButton {
                            text: root.mapLayers
                                ? qsTr("HISTORY %1").arg(root.mapLayers.selectedGridQsos.length)
                                : qsTr("HISTORY")
                            font.pixelSize: 9
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: gridDetailsTabs.currentIndex

                        Item {
                            ListView {
                                id: gridLiveList
                                anchors.fill: parent
                                clip: true
                                spacing: 3
                                model: root.mapLayers
                                    ? root.mapLayers.selectedGridLive : []
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: gridLiveList.width
                                    height: 62
                                    radius: 3
                                    color: index % 2 ? "#101a28" : "#0d2430"
                                    border.width: modelData.isCq ? 1 : 0
                                    border.color: modelData.isCq
                                        ? root.accentGreen : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 5
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.call || qsTr("Unknown"))
                                                      + (modelData.grid
                                                         ? "  " + modelData.grid : "")
                                                color: modelData.isCq
                                                    ? root.accentGreen
                                                    : root.textPrimary
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("%1  %2  %3 dB  %4  |  %5")
                                                    .arg(modelData.band || "-")
                                                    .arg(modelData.mode || "-")
                                                    .arg(modelData.snr)
                                                    .arg(Number(modelData.sourceCount || 1) > 1
                                                         ? (modelData.corroborationLevel
                                                            || qsTr("Corroborated")) + ": "
                                                           + (modelData.sourceSummary
                                                              || modelData.source || "")
                                                         : (modelData.sourceSummary
                                                            || modelData.source || ""))
                                                    .arg(modelData.gridEvidence || qsTr("Station locator"))
                                                color: root.textSecondary
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.message
                                                      || modelData.observedUtc || ""
                                                color: root.secondaryCyan
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                        }
                                        ToolButton {
                                            text: "☆"
                                            enabled: !!modelData.call
                                            onClicked: root.mapLayers.setRosterCallWatched(
                                                modelData.call, true)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Watch this station")
                                        }
                                        ToolButton {
                                            text: "QRZ"
                                            font.pixelSize: 8
                                            enabled: !!modelData.call
                                            onClicked: root.openCallLookup(modelData.call)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Open callsign lookup")
                                        }
                                        Button {
                                            text: qsTr("CALL")
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.engine.processMapRosterCall(
                                                modelData.call, modelData.grid || "")
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Start QSO with this station")
                                        }
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !root.mapLayers
                                         || (!root.mapLayers.gridDetailsLoading
                                             && gridLiveList.count === 0)
                                text: qsTr("No recent traffic in this grid")
                                color: root.textSecondary
                                font.pixelSize: 10
                            }
                        }

                        Item {
                            ListView {
                                id: gridHistoryList
                                anchors.fill: parent
                                clip: true
                                spacing: 3
                                model: root.mapLayers
                                    ? root.mapLayers.selectedGridQsos : []
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: gridHistoryList.width
                                    height: modelData.vuccGrids
                                            && modelData.vuccGrids.length > 0 ? 70 : 56
                                    radius: 3
                                    color: modelData.confirmed
                                        ? "#142a22"
                                        : (index % 2 ? "#101a28" : "#0d2430")
                                    border.width: modelData.confirmed ? 1 : 0
                                    border.color: modelData.confirmed
                                        ? root.accentGreen : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 6
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.call || qsTr("Unknown"))
                                                      + (modelData.grid
                                                         ? "  " + modelData.grid : "")
                                                color: modelData.confirmed
                                                    ? root.accentGreen
                                                    : root.textPrimary
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("%1 %2  %3  %4  %5")
                                                    .arg(modelData.qsoDate || "")
                                                    .arg(modelData.timeOn || "")
                                                    .arg(modelData.band || "-")
                                                    .arg(modelData.mode || "-")
                                                    .arg(modelData.source || "")
                                                color: root.textSecondary
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                visible: modelData.vuccGrids
                                                         && modelData.vuccGrids.length > 0
                                                text: modelData.matchedGridIsPrimary
                                                    ? qsTr("VUCC grids: %1")
                                                          .arg(modelData.vuccGrids.join(", "))
                                                    : qsTr("Matched VUCC grid: %1 · Primary: %2")
                                                          .arg(modelData.matchedGrid || "-")
                                                          .arg(modelData.grid || "-")
                                                color: root.secondaryCyan
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                        }
                                        Text {
                                            text: modelData.confirmed
                                                ? qsTr("CONFIRMED") : qsTr("WORKED")
                                            color: modelData.confirmed
                                                ? root.accentGreen
                                                : root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        Button {
                                            text: qsTr("CALL")
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.engine.processMapRosterCall(
                                                modelData.call, modelData.grid || "")
                                        }
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !root.mapLayers
                                         || (!root.mapLayers.gridDetailsLoading
                                             && gridHistoryList.count === 0)
                                text: qsTr("No QSO history in this grid")
                                color: root.textSecondary
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: intelligencePanel
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 6
                width: Math.min(root.detached ? 480 : 380, parent.width - 12)
                visible: root.intelligencePanelRequested
                z: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b,
                               root.compactIntelligencePanel ? 0.97 : 0.92)
                border.color: Qt.rgba(root.secondaryCyan.r, root.secondaryCyan.g,
                                      root.secondaryCyan.b, 0.48)
                border.width: 1
                radius: 4
                clip: true
                Material.theme: Material.Dark
                Material.accent: root.primaryBlue
                Material.foreground: root.textPrimary
                Material.background: root.bgDeep

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: qsTr("Map Intelligence")
                            color: root.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        BusyIndicator {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            running: (root.mapLayers && root.mapLayers.loading)
                                     || (root.externalOverlays
                                         && root.externalOverlays.loading)
                            visible: running
                        }
                        ToolButton {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 22
                            text: "×"
                            onClicked: root.intelligencePanelRequested = false
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Close details")
                        }
                    }

                    TabBar {
                        id: intelligenceTabs
                        Layout.fillWidth: true
                        TabButton {
                            text: qsTr("MAP")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                        TabButton {
                            text: qsTr("ROSTER")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                        TabButton {
                            text: qsTr("LOGBOOK")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                        TabButton {
                            text: qsTr("STATS")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                        TabButton {
                            text: qsTr("ACTIVITY")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                        TabButton {
                            text: qsTr("AWARDS")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                        TabButton {
                            text: root.mapLayers && root.mapLayers.unreadAlertCount > 0
                                ? qsTr("ALERTS %1").arg(root.mapLayers.unreadAlertCount)
                                : qsTr("ALERTS")
                            font.pixelSize: 8
                            leftPadding: 2
                            rightPadding: 2
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: intelligenceTabs.currentIndex

                        ScrollView {
                            id: mapControlsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: mapControlsScroll.availableWidth
                                spacing: 5

                                Text {
                                    text: qsTr("LAYERS")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 4
                                    rowSpacing: 0
                                    Repeater {
                                        model: root.mapLayers ? root.mapLayers.layerModel : null
                                        delegate: CheckBox {
                                            required property string layerId
                                            required property string label
                                            required property string layerColor
                                            required property bool layerEnabled
                                            required property int layerCount
                                            Layout.fillWidth: true
                                            checked: layerEnabled
                                            text: qsTr("%1  %2").arg(label).arg(layerCount)
                                            font.pixelSize: 9
                                            palette.text: layerColor
                                            onToggled: {
                                                if (root.mapLayers)
                                                    root.mapLayers.layerModel
                                                        .setLayerEnabled(layerId, checked)
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("LAYER STYLE")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                ComboBox {
                                    id: layerStyleSelector
                                    Layout.fillWidth: true
                                    model: root.mapLayers ? root.mapLayers.layerModel : null
                                    textRole: "label"
                                    valueRole: "layerId"
                                    font.pixelSize: 9
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Select the layer whose appearance is being edited")
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        text: qsTr("Color")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                    }
                                    TextField {
                                        id: layerColorField
                                        Layout.fillWidth: true
                                        text: root.mapLayers && root.mapLayers.layerModel
                                            ? root.mapLayers.layerModel.layerColor(
                                                  layerStyleSelector.currentValue) : "#ffffff"
                                        placeholderText: qsTr("#RRGGBB")
                                        font.pixelSize: 9
                                        onEditingFinished: {
                                            if (root.mapLayers && root.mapLayers.layerModel)
                                                root.mapLayers.layerModel.setLayerColor(
                                                    layerStyleSelector.currentValue, text)
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        text: qsTr("Opacity")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        Layout.preferredWidth: 44
                                    }
                                    Slider {
                                        id: layerOpacitySlider
                                        Layout.fillWidth: true
                                        from: 0.05
                                        to: 1.0
                                        stepSize: 0.05
                                        value: root.mapLayers && root.mapLayers.layerModel
                                            ? root.mapLayers.layerModel.layerOpacity(
                                                  layerStyleSelector.currentValue) : 1.0
                                        onMoved: if (root.mapLayers && root.mapLayers.layerModel)
                                            root.mapLayers.layerModel.setLayerOpacity(
                                                layerStyleSelector.currentValue, value)
                                    }
                                    Text {
                                        text: Math.round(layerOpacitySlider.value * 100) + "%"
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        Layout.preferredWidth: 28
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        text: qsTr("Width")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        Layout.preferredWidth: 44
                                    }
                                    Slider {
                                        id: layerThicknessSlider
                                        Layout.fillWidth: true
                                        from: 0.5
                                        to: 8.0
                                        stepSize: 0.5
                                        value: root.mapLayers && root.mapLayers.layerModel
                                            ? root.mapLayers.layerModel.layerThickness(
                                                  layerStyleSelector.currentValue) : 1.0
                                        onMoved: if (root.mapLayers && root.mapLayers.layerModel)
                                            root.mapLayers.layerModel.setLayerThickness(
                                                layerStyleSelector.currentValue, value)
                                    }
                                    Text {
                                        text: layerThicknessSlider.value.toFixed(1)
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        Layout.preferredWidth: 28
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        text: qsTr("Labels")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        Layout.preferredWidth: 44
                                    }
                                    Slider {
                                        id: layerLabelDensitySlider
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        stepSize: 10
                                        value: root.mapLayers && root.mapLayers.layerModel
                                            ? root.mapLayers.layerModel.labelDensity(
                                                  layerStyleSelector.currentValue) : 100
                                        onMoved: if (root.mapLayers && root.mapLayers.layerModel)
                                            root.mapLayers.layerModel.setLabelDensity(
                                                layerStyleSelector.currentValue, Math.round(value))
                                    }
                                    Text {
                                        text: Math.round(layerLabelDensitySlider.value) + "%"
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        Layout.preferredWidth: 28
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Button {
                                        Layout.fillWidth: true
                                        text: qsTr("Export map config")
                                        font.pixelSize: 8
                                        onClicked: root.exportMapConfiguration()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Export presets, roster preferences/rules, layer styles, temporal decay and map viewport")
                                    }
                                    Button {
                                        Layout.fillWidth: true
                                        text: qsTr("Import map config")
                                        font.pixelSize: 8
                                        onClicked: mapConfigurationImportDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Import the complete map configuration bundle")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("BASE MAP")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.baseMapService
                                            ? root.baseMapService.availableProviders : []
                                        currentIndex: root.baseMapService
                                            ? Math.max(0, model.indexOf(
                                                           root.baseMapService.provider)) : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.baseMapService)
                                                root.baseMapService.provider = currentText
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Atlas locale, OpenStreetMap/OpenTopoMap, GEBCO bathymetry, NASA GIBS e MapTiler. Se un provider fallisce viene provato il fallback configurato.")
                                    }
                                    CheckBox {
                                        Layout.preferredWidth: implicitWidth
                                        text: qsTr("Offline")
                                        checked: root.mapLayerEnabled("offline")
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapLayers)
                                                root.mapLayers.layerModel.setLayerEnabled(
                                                    "offline", checked)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Pause cloud/network services, keep local ADIF, logbook, cache and radio activity available, and use the local atlas or imported raster pack.")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        text: qsTr("STYLE")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                    }
                                    ComboBox {
                                        Layout.preferredWidth: 110
                                        model: root.baseMapService
                                            ? root.baseMapService.availableStyles : []
                                        currentIndex: root.baseMapService
                                            ? Math.max(0, model.indexOf(root.baseMapService.style)) : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.baseMapService)
                                                root.baseMapService.style = currentText
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.baseMapService && root.baseMapService.fallbackActive
                                            ? qsTr("Fallback attivo: %1").arg(root.baseMapService.activeProvider)
                                            : (root.baseMapService && root.baseMapService.staleCache
                                               ? qsTr("Cache obsoleta · aggiornamento in corso") : "")
                                        color: root.baseMapService && root.baseMapService.fallbackActive
                                            ? root.accentAmber : root.textSecondary
                                        font.pixelSize: 8
                                        elide: Text.ElideRight
                                    }
                                    Button {
                                        text: qsTr("Clear cache")
                                        font.pixelSize: 8
                                        enabled: root.baseMapService && !root.baseMapService.loading
                                        onClicked: root.baseMapService.invalidateCache()
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Button {
                                        Layout.fillWidth: true
                                        text: qsTr("Import offline raster")
                                        font.pixelSize: 8
                                        onClicked: offlineRasterImportDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Import a legally obtained equirectangular world raster, such as a Natural Earth export. Decodium stores a private local copy and does not download tiles.")
                                    }
                                    Button {
                                        text: qsTr("Clear")
                                        font.pixelSize: 8
                                        enabled: root.baseMapService
                                            && root.baseMapService.offlinePackAvailable
                                        onClicked: root.baseMapService.clearOfflinePack()
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.baseMapService
                                        && root.baseMapService.offlinePackStatus.length > 0
                                    text: root.baseMapService
                                        ? root.baseMapService.offlinePackStatus : ""
                                    color: root.baseMapService
                                        && root.baseMapService.offlinePackAvailable
                                        ? root.accentGreen : root.textSecondary
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    visible: root.baseMapService
                                        && root.baseMapService.provider === "MapTiler satellite"
                                    text: root.baseMapService
                                        ? root.baseMapService.mapTilerApiKey : ""
                                    placeholderText: qsTr("MapTiler API key")
                                    echoMode: TextInput.Password
                                    font.pixelSize: 9
                                    onEditingFinished: {
                                        if (root.baseMapService)
                                            root.baseMapService.mapTilerApiKey = text
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.baseMapService
                                            ? ((root.baseMapService.activeProvider !== root.baseMapService.provider
                                                ? qsTr("[%1] ").arg(root.baseMapService.activeProvider) : "")
                                               + root.baseMapService.status) : ""
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        elide: Text.ElideRight
                                    }
                                    Button {
                                        text: root.baseMapService
                                            && root.baseMapService.loading
                                            ? qsTr("Loading...") : qsTr("Refresh")
                                        font.pixelSize: 8
                                        enabled: root.baseMapService
                                            && !root.baseMapService.loading
                                            && !root.mapLayerEnabled("offline")
                                        onClicked: root.baseMapService.refresh()
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.baseMapService
                                        && root.baseMapService.attribution.length > 0
                                    text: root.baseMapService && root.baseMapService.attributionUrl.length > 0
                                        ? qsTr("Base: <a href=\"%1\">%2</a>")
                                              .arg(root.baseMapService.attributionUrl)
                                              .arg(root.baseMapService.attribution)
                                        : qsTr("Base: %1").arg(root.baseMapService.attribution)
                                    textFormat: Text.RichText
                                    onLinkActivated: Qt.openUrlExternally(link)
                                    color: root.textSecondary
                                    linkColor: root.secondaryCyan
                                    font.pixelSize: 8
                                    wrapMode: Text.Wrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableBands : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableBands
                                                       .indexOf(root.mapLayers.bandFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.bandFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Band")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableModes : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableModes
                                                       .indexOf(root.mapLayers.modeFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.modeFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Mode")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availablePeriods : ["All time"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availablePeriods
                                                       .indexOf(root.mapLayers.periodFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.periodFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Period")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableSources : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableSources
                                                       .indexOf(root.mapLayers.sourceFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.sourceFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Source")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availablePropagationTypes : []
                                        textRole: "label"
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availablePropagationModes
                                                       .indexOf(root.mapLayers.propagationFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: function(index) {
                                            if (root.mapLayers && root.mapLayers.availablePropagationTypes[index])
                                                root.mapLayers.propagationFilter =
                                                    root.mapLayers.availablePropagationTypes[index].code
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Propagation")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableContinents : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableContinents
                                                       .indexOf(root.mapLayers.continentFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.continentFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Continent")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableDxcc : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableDxcc
                                                       .indexOf(root.mapLayers.dxccFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.dxccFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("DXCC")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    CheckBox {
                                        text: qsTr("CQ only")
                                        checked: root.mapLayers ? root.mapLayers.cqOnly : false
                                        font.pixelSize: 9
                                        onToggled: root.mapLayers.cqOnly = checked
                                    }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        text: root.engine && root.engine.pskMapSpotFetching
                                            ? qsTr("Loading PSK...")
                                            : qsTr("Refresh PSK")
                                        font.pixelSize: 9
                                        enabled: root.engine && !root.engine.pskMapSpotFetching
                                            && root.mapLayerEnabled("psk")
                                            && !root.mapLayerEnabled("offline")
                                        onClicked: root.engine.refreshPskMapSpots()
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.mapLayerEnabled("offline")
                                            ? qsTr("Unavailable while Offline mode is enabled")
                                            : !root.mapLayerEnabled("psk")
                                                ? qsTr("Enable the PSK layer to retrieve PSK Reporter spots")
                                                : qsTr("Refresh PSK Reporter spots for the Live Map. PSK upload is independent.")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    spacing: 5
                                    Text {
                                        text: qsTr("PSK spots")
                                        color: "#ba7cff"
                                        font.pixelSize: 9
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: [
                                            { label: qsTr("5 min"), value: 5 },
                                            { label: qsTr("10 min"), value: 10 },
                                            { label: qsTr("15 min"), value: 15 },
                                            { label: qsTr("20 min"), value: 20 },
                                            { label: qsTr("25 min"), value: 25 },
                                            { label: qsTr("30 min"), value: 30 },
                                            { label: qsTr("35 min"), value: 35 },
                                            { label: qsTr("40 min"), value: 40 },
                                            { label: qsTr("45 min"), value: 45 },
                                            { label: qsTr("50 min"), value: 50 },
                                            { label: qsTr("55 min"), value: 55 },
                                            { label: qsTr("60 min"), value: 60 }
                                        ]
                                        textRole: "label"
                                        valueRole: "value"
                                        currentIndex: root.engine
                                            ? Math.max(0, Math.min(11,
                                                Math.round(root.engine.pskMapSpotWindowMinutes / 5) - 1))
                                            : 2
                                        font.pixelSize: 9
                                        enabled: root.engine && !root.engine.pskMapSpotFetching
                                            && !root.mapLayerEnabled("offline")
                                        onActivated: {
                                            if (!root.engine)
                                                return
                                            root.engine.pskMapSpotWindowMinutes = Number(currentValue)
                                            root.engine.refreshPskMapSpots()
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Look-back and expiry period for PSK Reporter spots displayed on the Live Map")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    Text {
                                        text: qsTr("PSK")
                                        color: "#ba7cff"
                                        font.pixelSize: 8
                                        font.bold: true
                                    }
                                    ComboBox {
                                        Layout.preferredWidth: 96
                                        model: root.mapLayers
                                            ? root.mapLayers.availablePskDisplayModes
                                            : ["Overlay", "Replace"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, model.indexOf(
                                                           root.mapLayers.pskDisplayMode))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: root.mapLayers.pskDisplayMode =
                                            currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Overlay PSK receivers with local activity or replace local activity")
                                    }
                                    Slider {
                                        Layout.fillWidth: true
                                        from: 20
                                        to: 100
                                        stepSize: 5
                                        value: root.mapLayers
                                            ? root.mapLayers.pskOpacityPercent : 65
                                        onMoved: {
                                            if (root.mapLayers)
                                                root.mapLayers.pskOpacityPercent =
                                                    Math.round(value)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("PSK grid opacity")
                                    }
                                    Text {
                                        text: root.mapLayers
                                            ? root.mapLayers.pskOpacityPercent + "%" : "65%"
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    spacing: 5
                                    CheckBox {
                                        id: mqttEnabled
                                        text: qsTr("Live MQTT")
                                        checked: root.mapLayers
                                            && root.mapLayers.pskFeedService
                                            && root.mapLayers.pskFeedService.enabled
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (!root.mapLayers
                                                    || !root.mapLayers.pskFeedService)
                                                return
                                            if (checked)
                                                root.mapLayers.configurePskFeed(
                                                    root.engine.callsign,
                                                    root.engine.grid)
                                            else
                                                root.mapLayers.pskFeedService.enabled = false
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.mapLayerEnabled("offline")
                                            ? qsTr("The subscription is retained but paused while Offline mode is enabled")
                                            : qsTr("Subscribe to the PSK Reporter MQTT stream for this station")
                                    }
                                    Button {
                                        text: root.mapLayers && root.mapLayers.pskFeedService
                                              && root.mapLayers.pskFeedService.connected
                                            ? qsTr("Connected") : qsTr("Connect")
                                        font.pixelSize: 8
                                        enabled: root.mapLayers
                                            && root.mapLayers.pskFeedService
                                            && !root.mapLayerEnabled("offline")
                                        onClicked: root.mapLayers.configurePskFeed(
                                            root.engine.callsign, root.engine.grid)
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.mapLayerEnabled("offline")
                                            ? qsTr("Unavailable while Offline mode is enabled")
                                            : qsTr("Connect the PSK Reporter MQTT feed")
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: root.mapLayers && root.mapLayers.pskFeedService
                                            ? root.mapLayers.pskFeedService.receivedCount
                                              + qsTr(" spots") : ""
                                        color: "#ba7cff"
                                        font.pixelSize: 8
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    spacing: 5
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableSpotAgeFilters : []
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, model.indexOf(root.mapLayers.spotAgeFilter)) : 0
                                        font.pixelSize: 9
                                        onActivated: root.mapLayers.spotAgeFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("PSK/spot history age")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableCorrelationFilters : []
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, model.indexOf(root.mapLayers.spotCorrelationFilter)) : 0
                                        font.pixelSize: 9
                                        onActivated: root.mapLayers.spotCorrelationFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Source correlation filter")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    text: root.mapLayers
                                        ? qsTr("Heat %1  Timeline %2  Paths %3")
                                              .arg(root.mapLayers.spotHeatmap.length)
                                              .arg(root.mapLayers.spotTimeline.length)
                                              .arg(root.mapLayers.spotPaths.length)
                                        : ""
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: [qsTr("4-char grids"), qsTr("6-char grids")]
                                        currentIndex: root.mapLayers
                                            && root.mapLayers.gridPrecision === 6 ? 1 : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapLayers)
                                                root.mapLayers.gridPrecision =
                                                    currentIndex === 1 ? 6 : 4
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Coverage grid precision")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: [
                                            { label: qsTr("Live 5 min"), value: 5 },
                                            { label: qsTr("Live 15 min"), value: 15 },
                                            { label: qsTr("Live 30 min"), value: 30 },
                                            { label: qsTr("Live 60 min"), value: 60 }
                                        ]
                                        textRole: "label"
                                        valueRole: "value"
                                        currentIndex: {
                                            if (!root.mapLayers)
                                                return 1
                                            var values = [5, 15, 30, 60]
                                            var found = values.indexOf(
                                                root.mapLayers.liveDecayMinutes)
                                            return found >= 0 ? found : 1
                                        }
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapLayers)
                                                root.mapLayers.liveDecayMinutes =
                                                    Number(currentValue)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Time before live grid activity fades out")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("TEMPORAL LEGEND / SOURCE DECAY")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Repeater {
                                    model: {
                                        if (!root.mapLayers)
                                            return []
                                        return root.mapLayers.temporalLegend.filter(function(entry) {
                                            return entry.source !== "psk"
                                        })
                                    }
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Rectangle {
                                            Layout.preferredWidth: 8
                                            Layout.preferredHeight: 8
                                            radius: 4
                                            color: modelData.color
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("%1 · %2 / %3 / %4")
                                                .arg(modelData.label)
                                                .arg(modelData.freshLabel)
                                                .arg(modelData.fadingLabel)
                                                .arg(modelData.staleLabel)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                            elide: Text.ElideRight
                                        }
                                        ComboBox {
                                            Layout.preferredWidth: 68
                                            model: [5, 15, 30, 60, 120]
                                            currentIndex: {
                                                var values = [5, 15, 30, 60, 120]
                                                var current = Number(modelData.decayMinutes)
                                                var found = values.indexOf(current)
                                                return found >= 0 ? found : 1
                                            }
                                            font.pixelSize: 8
                                            onActivated: {
                                                if (!root.mapLayers)
                                                    return
                                                var next = {}
                                                var sourceValues = root.mapLayers.sourceDecayMinutes || {}
                                                for (var key in sourceValues)
                                                    next[key] = sourceValues[key]
                                                next[modelData.source] = Number(currentText)
                                                root.mapLayers.sourceDecayMinutes = next
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Decay lifetime for this source")
                                        }
                                    }
                                }
                                CheckBox {
                                    text: qsTr("Split historical and live grid status")
                                    checked: root.mapLayers
                                        ? root.mapLayers.splitGridEnabled : true
                                    font.pixelSize: 9
                                    onToggled: {
                                        if (root.mapLayers)
                                            root.mapLayers.splitGridEnabled = checked
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Show QSO/QSL history and current activity in separate halves of the same grid")
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Label { text: qsTr("AZ"); font.pixelSize: 8; color: root.textSecondary }
                                    SpinBox {
                                        Layout.preferredWidth: 78
                                        from: 0
                                        to: 360
                                        editable: true
                                        value: root.mapOperations
                                            ? Math.round(root.mapOperations.rotatorMinAzimuth) : 0
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorMinAzimuth = value
                                        }
                                    }
                                    SpinBox {
                                        Layout.preferredWidth: 78
                                        from: 0
                                        to: 360
                                        editable: true
                                        value: root.mapOperations
                                            ? Math.round(root.mapOperations.rotatorMaxAzimuth) : 360
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorMaxAzimuth = value
                                        }
                                    }
                                    Label { text: qsTr("range"); font.pixelSize: 8; color: root.textSecondary }
                                    Label { text: qsTr("refresh"); font.pixelSize: 8; color: root.textSecondary }
                                    SpinBox {
                                        Layout.preferredWidth: 82
                                        from: 250
                                        to: 10000
                                        stepSize: 250
                                        editable: true
                                        value: root.mapOperations
                                            ? root.mapOperations.rotatorTrackingIntervalMs : 1000
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorTrackingIntervalMs = value
                                        }
                                    }
                                    Label { text: qsTr("ms"); font.pixelSize: 8; color: root.textSecondary }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    CheckBox {
                                        Layout.fillWidth: true
                                        text: qsTr("Push-pin live grids")
                                        checked: root.mapLayers
                                            ? root.mapLayers.coveragePushPinsEnabled : false
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapLayers)
                                                root.mapLayers.coveragePushPinsEnabled = checked
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Draw active and wanted grids as compact map pins")
                                    }
                                    CheckBox {
                                        Layout.fillWidth: true
                                        text: qsTr("UTC time zones")
                                        checked: root.mapLayers
                                            ? root.mapLayers.timeZoneOverlayEnabled : false
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapLayers)
                                                root.mapLayers.timeZoneOverlayEnabled = checked
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Show UTC-offset meridians on the map")
                                    }
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 4
                                    columnSpacing: 5
                                    rowSpacing: 3
                                    Repeater {
                                        model: [
                                            { label: "QSO", color: "#22c7e8" },
                                            { label: "QSL", color: "#51e58a" },
                                            { label: "CQ", color: "#f6c344" },
                                            { label: "CQDX", color: "#ff8c42" },
                                            { label: "QRZ", color: "#ff8c42" },
                                            { label: "QSX", color: "#ba7cff" },
                                            { label: "WSPR", color: "#ba7cff" },
                                            { label: "PSK", color: "#ba7cff" }
                                        ]
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Rectangle {
                                                Layout.preferredWidth: 9
                                                Layout.preferredHeight: 9
                                                radius: 1
                                                color: modelData.color
                                            }
                                            Text {
                                                text: modelData.label
                                                color: root.textSecondary
                                                font.pixelSize: 8
                                            }
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.engine && root.engine.pskMapSpotFetching
                                        ? qsTr("PSK Reporter: refreshing Live Map spots…")
                                        : qsTr("PSK Reporter Live Map window: %1 minutes")
                                              .arg(root.engine ? root.engine.pskMapSpotWindowMinutes : 15)
                                    color: root.engine && root.engine.pskMapSpotFetching
                                        ? "#ba7cff" : root.textSecondary
                                    font.pixelSize: 9
                                    wrapMode: Text.Wrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Confirmed = paper QSL, LoTW or eQSL received (ADIF status Y)")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    wrapMode: Text.Wrap
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    Text {
                                        text: qsTr("QSO  %1").arg(root.mapLayers
                                                                  ? root.mapLayers.qsoCount : 0)
                                        color: root.textSecondary; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Live  %1").arg(root.mapLayers
                                                                   ? root.mapLayers.liveSpotCount : 0)
                                        color: root.textSecondary; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Worked  %1").arg(root.mapLayers
                                                                     ? root.mapLayers.workedGridCount : 0)
                                        color: root.secondaryCyan; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Confirmed  %1").arg(root.mapLayers
                                                                        ? root.mapLayers.confirmedGridCount : 0)
                                        color: root.accentGreen; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Active  %1").arg(root.mapLayers
                                                                    ? root.mapLayers.activeGridCount : 0)
                                        color: "#f6c344"; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Missing  %1").arg(root.mapLayers
                                                                     ? root.mapLayers.missingGridCount : 0)
                                        color: "#ff8c42"; font.pixelSize: 9
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.layerModel
                                           && root.mapLayers.layerModel.layerEnabled("moon")
                                    spacing: 5

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.externalOverlays
                                              && root.externalOverlays.moonDataAvailable
                                            ? qsTr("Moon  Az %1°  El %2°  ·  Subpoint %3 %4")
                                                  .arg(root.externalOverlays.moonAzimuth.toFixed(1))
                                                  .arg(root.externalOverlays.moonElevation.toFixed(1))
                                                  .arg(root.coordinateText(
                                                           root.externalOverlays.moonSublunarLatitude,
                                                           "N", "S"))
                                                  .arg(root.coordinateText(
                                                           root.externalOverlays.moonSublunarLongitude,
                                                           "E", "W"))
                                            : qsTr("Moon data unavailable")
                                        color: root.externalOverlays
                                               && root.externalOverlays.moonDataAvailable
                                            ? "#dbe7ff" : root.textSecondary
                                        font.pixelSize: 9
                                        wrapMode: Text.Wrap
                                    }
                                    Button {
                                        visible: root.externalOverlays
                                            && root.externalOverlays.moonDataAvailable
                                        text: qsTr("LOCATE")
                                        font.pixelSize: 8
                                        Layout.preferredHeight: 24
                                        onClicked: root.locateMoon()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Center map on the Moon subpoint")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.layerModel
                                           && root.mapLayers.layerModel.layerEnabled("propagation")
                                    text: root.engine && root.engine.propagationManager
                                        ? qsTr("Propagation  SFI %1  K %2  MUF %3")
                                              .arg(root.engine.propagationManager.solarFlux)
                                              .arg(root.engine.propagationManager.kIndex)
                                              .arg(root.engine.propagationManager.muf)
                                        : qsTr("Propagation data unavailable")
                                    color: "#ffcf66"
                                    font.pixelSize: 9
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.layerModel
                                           && root.mapLayers.layerModel
                                                  .layerEnabled("propagation")
                                    text: qsTr("Propagation controls MUF, foF2, Es and Aurora")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    wrapMode: Text.Wrap
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayerEnabled("muf")
                                        || root.mapLayerEnabled("fof2")
                                        || root.mapLayerEnabled("es")
                                    spacing: 3

                                    Text {
                                        text: qsTr("PROPAGATION SCALE")
                                        color: root.secondaryCyan
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("MUF and foF2 use MHz. Es is a probability index, not a frequency.")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        wrapMode: Text.Wrap
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: root.mapLayerEnabled("muf")
                                        spacing: 1
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Text {
                                                Layout.preferredWidth: 34
                                                text: "MUF"
                                                color: "#f6c344"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#b89fbe"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#aadbd1"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#fdf6ab"
                                                }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Item { Layout.preferredWidth: 39 }
                                            Text { text: qsTr("<5 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("14 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr(">28 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: root.mapLayerEnabled("fof2")
                                        spacing: 1
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Text {
                                                Layout.preferredWidth: 34
                                                text: qsTr("foF2")
                                                color: "#66d9ff"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#b89fbe"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#aadbd1"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#fdf6ab"
                                                }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Item { Layout.preferredWidth: 39 }
                                            Text { text: qsTr("<1 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("5 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr(">14 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: root.mapLayerEnabled("es")
                                        spacing: 1
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Text {
                                                Layout.preferredWidth: 34
                                                text: qsTr("Es")
                                                color: "#ff9f43"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Repeater {
                                                    model: ["#728890", "#39ddce", "#4d9817",
                                                            "#f7f500", "#fbb800", "#db2d01",
                                                            "#7030a0", "#f2f2f2", "#5c5c61"]
                                                    delegate: Rectangle {
                                                        required property string modelData
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 8
                                                        color: modelData
                                                    }
                                                }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Item { Layout.preferredWidth: 39 }
                                            Text { text: qsTr("Poor"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("Good"); color: root.textSecondary; font.pixelSize: 8 }
                                        }
                                    }
                                }
                                Text {
                                    text: qsTr("MAP OPERATIONS")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapOperations
                                            ? root.mapOperations.availableDataViews : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.dataViewMode))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.dataViewMode =
                                                    currentText
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Live and logbook visibility")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapOperations
                                            ? root.mapOperations.availableProjections : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.mapProjection))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.mapProjection =
                                                    currentText
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Projection")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapOperations
                                            ? root.mapOperations.mapPresets : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.activeMapPreset))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.applyMapPreset(
                                                    currentText)
                                        }
                                    }
                                    Button {
                                        text: qsTr("SCREENSHOT")
                                        font.pixelSize: 8
                                        onClicked: root.captureMapScreenshot()
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    ComboBox {
                                        Layout.preferredWidth: 120
                                        model: root.mapOperations
                                            ? root.mapOperations.rotatorProtocols : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.rotatorProtocol))
                                            : 0
                                        font.pixelSize: 8
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorProtocol = currentText
                                        }
                                    }
                                    CheckBox {
                                        text: qsTr("Rotator")
                                        checked: root.mapOperations
                                            ? root.mapOperations.rotatorEnabled : false
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorEnabled =
                                                    checked
                                        }
                                    }
                                    TextField {
                                        Layout.fillWidth: true
                                        text: root.mapOperations
                                            ? root.mapOperations.rotatorHost : ""
                                        placeholderText: "127.0.0.1"
                                        font.pixelSize: 9
                                        onEditingFinished: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorHost = text
                                        }
                                    }
                                    Label {
                                        text: root.mapOperations
                                              ? root.mapOperations.rotatorTransport : "UDP"
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                    }
                                    SpinBox {
                                        Layout.preferredWidth: 88
                                        from: 1
                                        to: 65535
                                        editable: true
                                        value: root.mapOperations
                                            ? root.mapOperations.rotatorPort : 12000
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorPort = value
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    CheckBox {
                                        text: qsTr("Safety")
                                        checked: root.mapOperations
                                            ? root.mapOperations.rotatorSafetyEnabled : true
                                        font.pixelSize: 8
                                        onToggled: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorSafetyEnabled = checked
                                        }
                                    }
                                    SpinBox {
                                        Layout.preferredWidth: 82
                                        from: -10
                                        to: 180
                                        editable: true
                                        value: root.mapOperations
                                            ? Math.round(root.mapOperations.rotatorMinElevation) : 0
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorMinElevation = value
                                        }
                                    }
                                    Label { text: qsTr("min EL"); font.pixelSize: 8; color: root.textSecondary }
                                    SpinBox {
                                        Layout.preferredWidth: 82
                                        from: -10
                                        to: 180
                                        editable: true
                                        value: root.mapOperations
                                            ? Math.round(root.mapOperations.rotatorMaxElevation) : 180
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorMaxElevation = value
                                        }
                                    }
                                    Label { text: qsTr("max EL"); font.pixelSize: 8; color: root.textSecondary }
                                    CheckBox {
                                        text: qsTr("Park on stop")
                                        checked: root.mapOperations
                                            ? root.mapOperations.rotatorParkOnStop : false
                                        font.pixelSize: 8
                                        onToggled: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorParkOnStop = checked
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Button {
                                        text: qsTr("STOP")
                                        font.pixelSize: 8
                                        onClicked: if (root.mapOperations)
                                            root.mapOperations.stopRotator()
                                    }
                                    Button {
                                        text: qsTr("PARK")
                                        font.pixelSize: 8
                                        onClicked: if (root.mapOperations)
                                            root.mapOperations.parkRotator()
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            if (!root.mapOperations) return ""
                                            var feedback = root.mapOperations.rotatorFeedbackAvailable
                                                ? qsTr("FB AZ %1° / EL %2°")
                                                      .arg(root.mapOperations.rotatorCurrentAzimuth.toFixed(1))
                                                      .arg(root.mapOperations.rotatorCurrentElevation.toFixed(1))
                                                : qsTr("Feedback non disponibile")
                                            return feedback + (root.mapOperations.rotatorTracking
                                                ? qsTr(" · tracking") : "")
                                        }
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        elide: Text.ElideRight
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.mapOperations
                                        ? root.mapOperations.rotatorStatus : ""
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: Qt.rgba(root.secondaryCyan.r,
                                                   root.secondaryCyan.g,
                                                   root.secondaryCyan.b, 0.22)
                                    visible: root.externalOverlays
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.externalOverlays
                                    Text {
                                        text: qsTr("EXTERNAL DATA")
                                        color: root.secondaryCyan
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    ToolButton {
                                        Layout.preferredHeight: 24
                                        text: qsTr("Refresh")
                                        font.pixelSize: 9
                                        enabled: root.externalOverlays
                                                 && !root.externalOverlays.loading
                                        onClicked: root.externalOverlays.refreshAll()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Refresh enabled external layers")
                                    }
                                }
                                Repeater {
                                    model: root.externalOverlays
                                        ? root.externalOverlays.providerStatus : []
                                    delegate: ColumnLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 1
                                        visible: modelData.enabled
                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.loading ? "↻ " : "")
                                                  + modelData.label + "  "
                                                  + (modelData.available
                                                     ? root.overlayUpdatedText(
                                                           modelData.updatedMs)
                                                     : qsTr("unavailable"))
                                            color: modelData.error
                                                ? "#ff8c42" : root.textPrimary
                                            font.pixelSize: 9
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.error
                                                ? modelData.error
                                                : (modelData.attributionUrl
                                                   ? qsTr("Source: <a href=\"%1\">%2</a>")
                                                         .arg(modelData.attributionUrl)
                                                         .arg(modelData.attribution)
                                                   : qsTr("Source: %1")
                                                         .arg(modelData.attribution))
                                            textFormat: modelData.attributionUrl
                                                ? Text.RichText : Text.PlainText
                                            color: root.textSecondary
                                            linkColor: root.secondaryCyan
                                            font.pixelSize: 8
                                            elide: Text.ElideRight
                                            onLinkActivated: function(link) {
                                                Qt.openUrlExternally(link)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ScrollView {
                            id: rosterScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: rosterContent
                                width: rosterScroll.availableWidth
                                spacing: 4
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 4
                                rowSpacing: 0
                                CheckBox {
                                    text: qsTr("New grid")
                                    checked: root.mapLayers
                                        ? root.mapLayers.alertNewGridEnabled : true
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.alertNewGridEnabled =
                                        checked
                                }
                                CheckBox {
                                    text: qsTr("New DXCC")
                                    checked: root.mapLayers
                                        ? root.mapLayers.alertNewDxccEnabled : true
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.alertNewDxccEnabled =
                                        checked
                                }
                                CheckBox {
                                    text: qsTr("CQ activity")
                                    checked: root.mapLayers
                                        ? root.mapLayers.alertCqEnabled : true
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.alertCqEnabled =
                                        checked
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Call pattern, e.g. 9H*")
                                    text: root.mapLayers
                                        ? root.mapLayers.alertCallPattern : ""
                                    font.pixelSize: 9
                                    selectByMouse: true
                                    onEditingFinished: root.mapLayers
                                        .alertCallPattern = text
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Wildcard pattern matched against callsigns and messages")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                ComboBox {
                                    Layout.preferredWidth: 88
                                    model: [
                                        qsTr("No filter"), qsTr("Only"),
                                        qsTr("Exclude"), qsTr("Regex")
                                    ]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.rosterTextMode)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterTextMode = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Call roster text-filter mode")
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Call, grid, DXCC or message")
                                    text: root.mapLayers
                                        ? root.mapLayers.rosterTextFilter : ""
                                    font.pixelSize: 9
                                    selectByMouse: true
                                    onEditingFinished: {
                                        if (root.mapLayers)
                                            root.mapLayers.rosterTextFilter = text
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Filtering runs in the map database worker")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("LOOKUP")
                                    color: root.secondaryCyan
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableCallLookupProviders
                                        : ["QRZ", "HamQTH", "QRZCQ"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.callLookupProvider))
                                        : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.callLookupProvider =
                                        currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Provider used by lookup actions in grid popups and the roster")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    text: qsTr("AWARD")
                                    color: root.secondaryCyan
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardPrograms : ["None"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.activeAwardProgram)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.activeAwardProgram = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Select the award whose missing entities become wanted stations")
                                }
                                ComboBox {
                                    Layout.preferredWidth: 92
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardGoals : ["Confirmed"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.awardGoal)) : 0
                                    enabled: root.mapLayers
                                        && root.mapLayers.activeAwardProgram !== "None"
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.awardGoal = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Hunt entities not yet worked or not yet confirmed")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("SCOPE")
                                    color: root.secondaryCyan
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableRosterScopes : ["All bands"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(root.mapLayers.rosterScope)) : 0
                                    font.pixelSize: 8
                                    onActivated: root.mapLayers.rosterScope = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Current band/mode use the selected map band and mode; Digital modes includes FT, JT, Q and other digital modes")
                                }
                                ComboBox {
                                    Layout.preferredWidth: 98
                                    model: root.mapLayers
                                        ? root.mapLayers.availableRosterDxccScopes : ["All"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(root.mapLayers.rosterDxccScope)) : 0
                                    font.pixelSize: 8
                                    onActivated: root.mapLayers.rosterDxccScope = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Limit the roster to your DXCC or to other DXCCs")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                CheckBox {
                                    text: qsTr("LoTW")
                                    checked: root.mapLayers ? root.mapLayers.rosterUsesLoTW : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterUsesLoTW = checked
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Keep stations with a LoTW confirmation within the configured age")
                                }
                                CheckBox {
                                    text: qsTr("eQSL")
                                    checked: root.mapLayers ? root.mapLayers.rosterUsesEQSL : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterUsesEQSL = checked
                                }
                                CheckBox {
                                    text: qsTr("OQRS")
                                    checked: root.mapLayers ? root.mapLayers.rosterUsesOQRS : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterUsesOQRS = checked
                                }
                                CheckBox {
                                    text: qsTr("Spotted me")
                                    checked: root.mapLayers ? root.mapLayers.rosterSpottedMeOnly : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterSpottedMeOnly = checked
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Use only PSK Reporter spots heard by your callsign")
                                }
                                CheckBox {
                                    text: qsTr("RR73=CQ")
                                    checked: root.mapLayers ? root.mapLayers.rosterTreatRr73AsCq : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterTreatRr73AsCq = checked
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Treat messages containing RR73 as CQ when CQ-only is enabled")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                CheckBox {
                                    text: qsTr("Min SNR")
                                    checked: root.mapLayers ? root.mapLayers.rosterMinSnrEnabled : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterMinSnrEnabled = checked
                                }
                                SpinBox {
                                    Layout.preferredWidth: 72
                                    from: -60
                                    to: 30
                                    editable: true
                                    value: root.mapLayers ? root.mapLayers.rosterMinSnr : -25
                                    enabled: root.mapLayers && root.mapLayers.rosterMinSnrEnabled
                                    font.pixelSize: 8
                                    textFromValue: function(value, locale) { return value + " dB" }
                                    valueFromText: function(text, locale) {
                                        var parsed = parseInt(text)
                                        return isNaN(parsed) ? -25 : parsed
                                    }
                                    onValueModified: {
                                        if (root.mapLayers) root.mapLayers.rosterMinSnr = value
                                    }
                                }
                                CheckBox {
                                    text: qsTr("Max DT")
                                    checked: root.mapLayers ? root.mapLayers.rosterMaxDtEnabled : false
                                    font.pixelSize: 8
                                    onToggled: root.mapLayers.rosterMaxDtEnabled = checked
                                }
                                TextField {
                                    Layout.preferredWidth: 64
                                    text: root.mapLayers ? Number(root.mapLayers.rosterMaxDt).toFixed(2) : "0.50"
                                    enabled: root.mapLayers && root.mapLayers.rosterMaxDtEnabled
                                    font.pixelSize: 8
                                    selectByMouse: true
                                    validator: DoubleValidator { bottom: 0.01; top: 10.0; decimals: 2 }
                                    onEditingFinished: {
                                        if (root.mapLayers) {
                                            var parsed = parseFloat(text)
                                            if (!isNaN(parsed)) root.mapLayers.rosterMaxDt = parsed
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("provider filters are OR")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: root.mapLayers && root.mapLayers.rosterUsesLoTW
                                Text {
                                    text: qsTr("LoTW age")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                }
                                SpinBox {
                                    Layout.preferredWidth: 76
                                    from: 1
                                    to: 3650
                                    editable: true
                                    value: root.mapLayers ? root.mapLayers.rosterMaxLoTWDays : 810
                                    font.pixelSize: 8
                                    textFromValue: function(value, locale) { return value + " d" }
                                    valueFromText: function(text, locale) {
                                        var parsed = parseInt(text)
                                        return isNaN(parsed) ? 810 : parsed
                                    }
                                    onValueModified: {
                                        if (root.mapLayers) root.mapLayers.rosterMaxLoTWDays = value
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableRosterStatuses : ["All"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.rosterStatusFilter)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterStatusFilter = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Show all, new, unconfirmed, wanted or watched stations")
                                }
                                CheckBox {
                                    text: qsTr("CQ")
                                    checked: root.mapLayers ? root.mapLayers.rosterCqOnly : false
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.rosterCqOnly = checked
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Show only active CQ calls in the roster")
                                }
                                ToolButton {
                                    text: qsTr("Clear")
                                    enabled: root.mapLayers && root.mapLayers.liveSpotCount > 0
                                    onClicked: root.mapLayers.clearLiveSpots()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Clear live station history")
                                }
                                ToolButton {
                                    text: root.mapLayers
                                        ? qsTr("Lists %1").arg(
                                              root.mapLayers.rosterPreferenceCount)
                                        : qsTr("Lists")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterPreferences =
                                        !root.showRosterPreferences
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Review watched calls and ignored calls or DXCC entities")
                                }
                                ToolButton {
                                    text: qsTr("Columns")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterColumns = !root.showRosterColumns
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Choose the information shown on each roster row")
                                }
                                ToolButton {
                                    text: root.mapLayers
                                        ? qsTr("Rules %1").arg(root.mapLayers.rosterRules.length)
                                        : qsTr("Rules")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterRules = !root.showRosterRules
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Create wanted, ignored or watched rules with optional band and mode scopes")
                                }
                                ToolButton {
                                    text: qsTr("Matrix")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterMatrix = !root.showRosterMatrix
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Select NEW categories and inspect wanted or exception matrices")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableRosterHuntScopes : ["All time"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.rosterHuntScope)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterHuntScope = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Compare worked and confirmed status globally, by band or by band and mode")
                                }
                                SpinBox {
                                    id: rosterRetention
                                    from: 1
                                    to: 60
                                    editable: true
                                    value: root.mapLayers
                                        ? root.mapLayers.rosterRetentionMinutes : 5
                                    Layout.preferredWidth: 82
                                    font.pixelSize: 9
                                    textFromValue: function(value, locale) {
                                        return value + " min"
                                    }
                                    valueFromText: function(text, locale) {
                                        var parsed = parseInt(text)
                                        return isNaN(parsed) ? 5 : parsed
                                    }
                                    onValueModified: {
                                        if (root.mapLayers)
                                            root.mapLayers.rosterRetentionMinutes = value
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Keep each station active in the roster for this many minutes")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["Time", "Call", "Grid", "SNR", "Distance", "DXCC"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(root.mapLayers.rosterSort)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterSort = currentText
                                }
                                ToolButton {
                                    text: root.mapLayers
                                        && root.mapLayers.rosterSortDescending ? "↓" : "↑"
                                    onClicked: root.mapLayers.rosterSortDescending =
                                        !root.mapLayers.rosterSortDescending
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Reverse sort")
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.mapLayers
                                    ? qsTr("%1 stations · %2 wanted · %3 new · %4 unconfirmed")
                                          .arg(root.mapLayers.rosterCount)
                                          .arg(root.mapLayers.rosterWantedCount)
                                          .arg(root.mapLayers.rosterNewCount)
                                          .arg(root.mapLayers.rosterUnconfirmedCount)
                                    : ""
                                color: root.textSecondary
                                font.pixelSize: 8
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 96 : 0
                                visible: root.showRosterColumns && !!root.mapLayers
                                clip: true
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 3
                                    Text {
                                        text: qsTr("VISIBLE ROSTER COLUMNS")
                                        color: root.secondaryCyan
                                        font.pixelSize: 8
                                        font.bold: true
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Repeater {
                                            model: root.mapLayers
                                                ? root.mapLayers.availableRosterColumns : []
                                            delegate: CheckBox {
                                                required property string modelData
                                                text: modelData
                                                font.pixelSize: 8
                                                checked: root.mapLayers
                                                    && root.mapLayers.rosterVisibleColumns.indexOf(modelData) >= 0
                                                onToggled: {
                                                    if (!root.mapLayers)
                                                        return
                                                    var columns = root.mapLayers.rosterVisibleColumns.slice()
                                                    var columnIndex = columns.indexOf(modelData)
                                                    if (checked && columnIndex < 0)
                                                        columns.push(modelData)
                                                    else if (!checked && columnIndex >= 0)
                                                        columns.splice(columnIndex, 1)
                                                    root.mapLayers.rosterVisibleColumns = columns
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 176 : 0
                                visible: root.showRosterMatrix && !!root.mapLayers
                                clip: true
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 3
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("NEW / WANTED MATRIX")
                                            color: root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        Text {
                                            text: qsTr("wanted %1 · exceptions %2")
                                                .arg(root.mapLayers.rosterWantedMatrix.length)
                                                .arg(root.mapLayers.rosterExceptionMatrix.length)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                        }
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Repeater {
                                            model: root.mapLayers.availableRosterWantedTypes
                                            delegate: CheckBox {
                                                required property string modelData
                                                text: modelData
                                                font.pixelSize: 8
                                                checked: root.mapLayers.rosterWantedTypes.indexOf(modelData) >= 0
                                                onToggled: {
                                                    var types = root.mapLayers.rosterWantedTypes.slice()
                                                    var index = types.indexOf(modelData)
                                                    if (checked && index < 0)
                                                        types.push(modelData)
                                                    else if (!checked && index >= 0)
                                                        types.splice(index, 1)
                                                    root.mapLayers.rosterWantedTypes = types
                                                }
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("Use this entity in NEW and UNCONFIRMED calculations")
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                text: qsTr("WANTED")
                                                color: root.accentGreen
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            ListView {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                clip: true
                                                model: root.mapLayers.rosterWantedMatrix
                                                delegate: Text {
                                                    required property var modelData
                                                    width: parent ? parent.width : 0
                                                    text: modelData.type + "=" + modelData.value
                                                        + (modelData.band ? "  " + modelData.band : "")
                                                        + (modelData.mode ? "  " + modelData.mode : "")
                                                    color: root.textPrimary
                                                    font.pixelSize: 8
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                text: qsTr("EXCEPTIONS")
                                                color: "#ff8c8c"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            ListView {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                clip: true
                                                model: root.mapLayers.rosterExceptionMatrix
                                                delegate: Text {
                                                    required property var modelData
                                                    width: parent ? parent.width : 0
                                                    text: modelData.type + "=" + modelData.value
                                                        + (modelData.band ? "  " + modelData.band : "")
                                                        + (modelData.mode ? "  " + modelData.mode : "")
                                                    color: root.textPrimary
                                                    font.pixelSize: 8
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 156 : 0
                                visible: root.showRosterRules && !!root.mapLayers
                                clip: true
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 3
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("OPERATIONAL RULES")
                                            color: root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        ToolButton {
                                            text: qsTr("Hide")
                                            font.pixelSize: 8
                                            onClicked: root.showRosterRules = false
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: rosterRuleType
                                            Layout.preferredWidth: 86
                                            model: root.mapLayers
                                                ? root.mapLayers.availableRosterRuleTypes
                                                : ["CALL", "GRID", "DXCC", "WPX", "POTA", "CQ", "ITU",
                                                   "STATE", "COUNTY", "CONTINENT", "IOTA", "OQRS", "BAND", "MODE"]
                                            font.pixelSize: 8
                                        }
                                        TextField {
                                            id: rosterRuleValue
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Value")
                                            font.pixelSize: 8
                                            selectByMouse: true
                                        }
                                        ComboBox {
                                            id: rosterRuleAction
                                            Layout.preferredWidth: 74
                                            model: ["WANTED", "WATCH", "IGNORE"]
                                            font.pixelSize: 8
                                        }
                                        Button {
                                            text: qsTr("Add")
                                            font.pixelSize: 8
                                            enabled: root.mapLayers && rosterRuleValue.text.trim().length > 0
                                            onClicked: {
                                                root.mapLayers.setRosterRule(
                                                    rosterRuleType.currentText,
                                                    rosterRuleValue.text,
                                                    rosterRuleAction.currentText,
                                                    rosterRuleBand.text,
                                                    rosterRuleMode.text)
                                                rosterRuleValue.clear()
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            id: rosterRuleBand
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Band scope, optional")
                                            font.pixelSize: 8
                                            selectByMouse: true
                                        }
                                        TextField {
                                            id: rosterRuleMode
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Mode scope, optional")
                                            font.pixelSize: 8
                                            selectByMouse: true
                                        }
                                    }
                                    ListView {
                                        id: rosterRuleList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        spacing: 1
                                        model: root.mapLayers ? root.mapLayers.rosterRules : []
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        delegate: RowLayout {
                                            required property var modelData
                                            width: rosterRuleList.width
                                            height: 22
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.action + "  " + modelData.type + "="
                                                    + modelData.value
                                                    + (modelData.band ? "  " + modelData.band : "")
                                                    + (modelData.mode ? "  " + modelData.mode : "")
                                                color: modelData.action === "IGNORE"
                                                    ? "#ff7b7b" : (modelData.action === "WATCH"
                                                        ? root.secondaryCyan : root.accentGreen)
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                            ToolButton {
                                                Layout.preferredWidth: 24
                                                text: "×"
                                                font.pixelSize: 9
                                                onClicked: root.mapLayers.removeRosterRule(
                                                    modelData.type, modelData.value,
                                                    modelData.band || "", modelData.mode || "")
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("Remove rule")
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(
                                    156, 34 + (root.mapLayers
                                              ? root.mapLayers.rosterPreferenceCount
                                                * 29 : 0))
                                visible: root.showRosterPreferences
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    spacing: 2
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("WATCHED / IGNORED LISTS")
                                            color: root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        ToolButton {
                                            text: qsTr("Clear all")
                                            font.pixelSize: 8
                                            enabled: root.mapLayers
                                                && root.mapLayers.rosterPreferenceCount > 0
                                            onClicked: root.mapLayers
                                                .clearRosterPreferences()
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Clear all watched calls and all call or DXCC exclusions")
                                        }
                                    }
                                    ListView {
                                        id: rosterPreferenceList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        spacing: 1
                                        model: root.mapLayers
                                            ? root.mapLayers.rosterPreferences : []
                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                        }
                                        delegate: RowLayout {
                                            required property var modelData
                                            width: rosterPreferenceList.width
                                            height: 27
                                            spacing: 4
                                            Rectangle {
                                                Layout.preferredWidth: 48
                                                Layout.preferredHeight: 20
                                                radius: 3
                                                color: modelData.type === "WATCH"
                                                    ? "#182538" : "#2d2513"
                                                border.width: 1
                                                border.color: modelData.type === "WATCH"
                                                    ? root.secondaryCyan : "#f6c344"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.type
                                                    color: parent.border.color
                                                    font.pixelSize: 8
                                                    font.bold: true
                                                }
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.value
                                                color: root.textPrimary
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                            ToolButton {
                                                Layout.preferredWidth: 24
                                                text: "×"
                                                onClicked: root.mapLayers
                                                    .removeRosterPreference(
                                                        modelData.type,
                                                        modelData.value)
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("Remove this entry")
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: root.mapLayers
                                            && root.mapLayers.rosterPreferenceCount === 0
                                        text: qsTr("No watched or ignored entries")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                            ListView {
                                id: rosterList
                                Layout.fillWidth: true
                                // The roster controls can be taller than a docked map.
                                // Keep the station list independently scrollable while the
                                // enclosing ScrollView makes every control reachable.
                                Layout.preferredHeight: Math.max(
                                    180, Math.min(360, intelligencePanel.height * 0.45))
                                clip: true
                                spacing: 3
                                // The roster contains rich delegates. Do not make every
                                // live-map snapshot rebuild it while the user is on the
                                // MAP/LOGBOOK/STATS tabs; bind the current snapshot when
                                // ROSTER is actually selected.
                                model: intelligenceTabs.currentIndex === 1 && root.mapLayers
                                       ? root.mapLayers.roster : []
                                reuseItems: true
                                cacheBuffer: 260
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: rosterList.width
                                    height: 86
                                    radius: 3
                                    color: modelData.watched
                                        ? "#182538"
                                        : modelData.wanted
                                        ? root.rosterStatusFill(modelData.status)
                                        : (index % 2 ? "#101a28" : "#0d2430")
                                    border.width: modelData.wanted || modelData.watched ? 1 : 0
                                    border.color: modelData.watched
                                        ? root.secondaryCyan
                                        : modelData.wanted
                                        ? root.rosterStatusColor(modelData.status)
                                        : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.call || qsTr("Unknown"))
                                                      + (modelData.grid
                                                         ? "  " + root.rosterGridLabel(modelData)
                                                               + (modelData.gridOrigin
                                                                  ? " · " + modelData.gridOrigin : "")
                                                         : "")
                                                color: root.rosterStatusColor(modelData.status)
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                                text: root.rosterColumnSummary(modelData)
                                                color: root.textSecondary
                                                font.pixelSize: 9
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                visible: !!modelData.huntReason
                                                text: modelData.huntReason || ""
                                                color: root.rosterStatusColor(modelData.status)
                                                font.pixelSize: 8
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 3
                                            }
                                        }
                                        Rectangle {
                                            Layout.preferredWidth: rosterStatusLabel.implicitWidth + 12
                                            Layout.preferredHeight: 22
                                            radius: 3
                                            color: root.rosterStatusFill(modelData.status)
                                            border.width: 1
                                            border.color: root.rosterStatusColor(modelData.status)
                                            Text {
                                                id: rosterStatusLabel
                                                anchors.centerIn: parent
                                                text: modelData.status || "LIVE"
                                                color: root.rosterStatusColor(modelData.status)
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                        }
                                        ToolButton {
                                            text: modelData.watched ? "★" : "☆"
                                            font.pixelSize: 13
                                            enabled: !!modelData.call
                                            onClicked: root.mapLayers.setRosterCallWatched(
                                                modelData.call, !modelData.watched)
                                            ToolTip.visible: hovered
                                            ToolTip.text: modelData.watched
                                                ? qsTr("Stop watching this station")
                                                : qsTr("Keep this station at the top of the roster")
                                        }
                                        ToolButton {
                                            text: "X"
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.mapLayers.setRosterCallIgnored(
                                                modelData.call, true)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Ignore this station until roster preferences are reset")
                                        }
                                        ToolButton {
                                            text: qsTr("DX")
                                            font.pixelSize: 8
                                            enabled: !!modelData.dxcc
                                            visible: !!modelData.dxcc
                                            onClicked: root.mapLayers.setRosterDxccIgnored(
                                                modelData.dxcc, true)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Ignore every station from %1")
                                                .arg(modelData.dxcc || "")
                                        }
                                        ToolButton {
                                            text: "QRZ"
                                            font.pixelSize: 8
                                            enabled: !!modelData.call
                                            onClicked: root.openCallLookup(modelData.call)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Open callsign lookup")
                                        }
                                        ToolButton {
                                            text: qsTr("CALL")
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.engine.processMapRosterCall(
                                                modelData.call, modelData.grid || "")
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Start QSO with this station")
                                        }
                                    }
                                }
                            }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }

                        MapLogbookPanel {
                            operations: root.mapOperations
                            borderColor: root.glassBorder
                            primaryColor: root.primaryBlue
                            accentColor: root.accentGreen
                            textColor: root.textPrimary
                            mutedColor: root.textSecondary
                            onCallRequested: function(call, grid) {
                                if (root.engine)
                                    root.engine.processMapRosterCall(call, grid)
                            }
                        }

                        ScrollView {
                            id: statisticsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: statisticsContent
                                width: statisticsScroll.availableWidth
                                spacing: 6
                                property var stats: root.mapLayers
                                    ? root.mapLayers.statistics : ({})

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var period = statisticsContent.stats.period || "All time"
                                            var filtered = Number(statisticsContent.stats.qso || 0)
                                            var total = Number(statisticsContent.stats.totalQso || 0)
                                            return period === "All time"
                                                ? qsTr("LOGBOOK · ALL TIME · %1 QSO").arg(total)
                                                : qsTr("FILTERED · %1 · %2 of %3 QSO")
                                                      .arg(period).arg(filtered).arg(total)
                                        }
                                        color: (statisticsContent.stats.period || "All time") === "All time"
                                            ? root.secondaryCyan : root.accentAmber
                                        font.pixelSize: 9
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    ToolButton {
                                        visible: root.mapLayers
                                                 && root.mapLayers.periodFilter !== "All time"
                                        text: qsTr("ALL TIME")
                                        font.pixelSize: 8
                                        onClicked: root.mapLayers.periodFilter = "All time"
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Remove the temporary map time filter")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.mapLayers && root.mapLayers.sourcePath
                                        ? qsTr("ADIF  %1").arg(root.mapLayers.sourcePath)
                                        : qsTr("No active ADIF logbook")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    elide: Text.ElideMiddle
                                    ToolTip.visible: sourcePathMouse.containsMouse
                                    ToolTip.text: text
                                    MouseArea {
                                        id: sourcePathMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 8
                                    rowSpacing: 5
                                    Repeater {
                                        model: [
                                            { label: qsTr("QSO"), value: statisticsContent.stats.qso || 0 },
                                            { label: qsTr("Confirmed"), value: statisticsContent.stats.confirmed || 0 },
                                            { label: qsTr("Calls"), value: statisticsContent.stats.calls || 0 },
                                            { label: qsTr("DXCC"), value: statisticsContent.stats.dxcc || 0 },
                                            { label: qsTr("Grids"), value: statisticsContent.stats.grids || 0 },
                                            { label: qsTr("Live spots"), value: statisticsContent.stats.live || 0 }
                                        ]
                                        delegate: Rectangle {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 40
                                            radius: 3
                                            color: "#101a28"
                                            border.width: 1
                                            border.color: root.glassBorder
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 1
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.value
                                                    color: root.secondaryCyan
                                                    font.pixelSize: 13
                                                    font.bold: true
                                                }
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.label
                                                    color: root.textSecondary
                                                    font.pixelSize: 8
                                                }
                                            }
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("ADIF span  %1 — %2")
                                        .arg(root.statisticsDate(
                                            statisticsContent.stats.totalFirstEpoch))
                                        .arg(root.statisticsDate(
                                            statisticsContent.stats.totalLastEpoch))
                                    color: root.textSecondary
                                    font.pixelSize: 9
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: Number(statisticsContent.stats.qso || 0) === 0
                                             && Number(statisticsContent.stats.totalQso || 0) > 0
                                    text: qsTr("The ADIF logbook is loaded, but no QSO matches the current map filters.")
                                    color: root.accentAmber
                                    font.pixelSize: 8
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    text: qsTr("TOP BANDS")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Repeater {
                                    model: statisticsContent.stats.bands || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: root.textPrimary
                                            font.pixelSize: 9
                                        }
                                        Text {
                                            text: qsTr("%1 QSO · %2 QSL")
                                                .arg(modelData.qso)
                                                .arg(modelData.confirmed)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: root.glassBorder
                                }
                                Text {
                                    text: qsTr("TOP MODES")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Repeater {
                                    model: statisticsContent.stats.modes || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: root.textPrimary
                                            font.pixelSize: 9
                                        }
                                        Text {
                                            text: qsTr("%1 QSO · %2 QSL")
                                                .arg(modelData.qso)
                                                .arg(modelData.confirmed)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: root.glassBorder
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("PROPAGATION TYPES")
                                        color: root.secondaryCyan
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Text {
                                        text: {
                                            var summary = statisticsContent.stats.propagationSummary || {}
                                            return qsTr("%1 classified · %2 unknown")
                                                .arg(summary.classified || 0).arg(summary.unknown || 0)
                                        }
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                    }
                                }
                                Repeater {
                                    model: statisticsContent.stats.propagation || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: modelData.qso > 0
                                                ? root.textPrimary : root.textSecondary
                                            font.pixelSize: 8
                                        }
                                        Text {
                                            text: qsTr("%1 QSO · %2 QSL · %3%")
                                                .arg(modelData.qso)
                                                .arg(modelData.confirmed)
                                                .arg(Number(modelData.percent || 0).toFixed(1))
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                            }
                        }

                        ScrollView {
                            id: bandActivityScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: bandActivityContent
                                width: bandActivityScroll.availableWidth
                                spacing: 6
                                property var summary: root.mapLayers
                                    ? root.mapLayers.bandActivitySummary : ({})

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("BAND ACTIVITY")
                                        color: root.secondaryCyan
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Repeater {
                                        model: [1, 6, 12, 24]
                                        delegate: Button {
                                            required property int modelData
                                            Layout.preferredWidth: 38
                                            Layout.minimumWidth: 38
                                            Layout.preferredHeight: 24
                                            Layout.minimumHeight: 24
                                            text: modelData + "h"
                                            checkable: true
                                            checked: root.mapLayers
                                                && root.mapLayers.bandActivityWindowHours
                                                   === modelData
                                            padding: 0

                                            contentItem: Text {
                                                text: parent.text
                                                color: parent.checked
                                                    ? "#07131b" : root.textPrimary
                                                font.pixelSize: 9
                                                font.bold: parent.checked
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                radius: 4
                                                color: parent.checked
                                                    ? root.secondaryCyan
                                                    : (parent.hovered
                                                       ? "#223348" : "#172333")
                                                border.width: 1
                                                border.color: parent.checked
                                                    ? root.secondaryCyan
                                                    : (parent.hovered
                                                       ? root.textSecondary : "#35475d")
                                            }

                                            onClicked: {
                                                if (root.mapLayers)
                                                    root.mapLayers.bandActivityWindowHours =
                                                        modelData
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Analyse the last %1 hours")
                                                .arg(modelData)
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 58
                                    radius: 4
                                    color: "#101a28"
                                    border.width: 1
                                    border.color: root.accentGreen

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 7
                                        spacing: 8
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                text: qsTr("BEST BAND")
                                                color: root.textSecondary
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            Text {
                                                text: bandActivityContent.summary.bestBand
                                                      || qsTr("No activity")
                                                color: bandActivityContent.summary.bestBand
                                                    ? root.accentGreen
                                                    : root.textSecondary
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }
                                        Column {
                                            spacing: 1
                                            Text {
                                                anchors.right: parent.right
                                                text: bandActivityContent.summary.bestBand
                                                    ? qsTr("%1 / 100")
                                                          .arg(bandActivityContent.summary.bestScore || 0)
                                                    : "--"
                                                color: root.textPrimary
                                                font.pixelSize: 13
                                                font.bold: true
                                            }
                                            Text {
                                                anchors.right: parent.right
                                                text: qsTr("%1 bands / %2h")
                                                    .arg(bandActivityContent.summary.bandCount || 0)
                                                    .arg(bandActivityContent.summary.windowHours || 6)
                                                color: root.textSecondary
                                                font.pixelSize: 8
                                            }
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 4
                                    columnSpacing: 4
                                    rowSpacing: 4
                                    Repeater {
                                        model: [
                                            {
                                                label: qsTr("LOCAL RX"),
                                                value: bandActivityContent.summary.localRx || 0,
                                                tone: root.secondaryCyan
                                            },
                                            {
                                                label: qsTr("LOCAL TX"),
                                                value: bandActivityContent.summary.localTx || 0,
                                                tone: root.accentGreen
                                            },
                                            {
                                                label: qsTr("PSK RX"),
                                                value: bandActivityContent.summary.pskRx || 0,
                                                tone: root.accentAmber
                                            },
                                            {
                                                label: qsTr("PSK TX"),
                                                value: bandActivityContent.summary.pskTx || 0,
                                                tone: "#d16cff"
                                            }
                                        ]
                                        delegate: Rectangle {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 42
                                            radius: 3
                                            color: "#101a28"
                                            border.width: 1
                                            border.color: root.glassBorder
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 1
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.value
                                                    color: modelData.tone
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.label
                                                    color: root.textSecondary
                                                    font.pixelSize: 7
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 196
                                    radius: 3
                                    color: "#0a111d"
                                    border.width: 1
                                    border.color: root.glassBorder

                                    Canvas {
                                        id: activityChart
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        antialiasing: true

                                        function drawSeries(ctx, values, color, left,
                                                            top, plotWidth,
                                                            plotHeight, maxValue) {
                                            if (values.length === 0)
                                                return
                                            ctx.beginPath()
                                            ctx.strokeStyle = color
                                            ctx.lineWidth = 1.6
                                            for (var i = 0; i < values.length; ++i) {
                                                var x = left + (values.length === 1
                                                    ? plotWidth
                                                    : plotWidth * i / (values.length - 1))
                                                var y = top + plotHeight
                                                    - plotHeight * values[i] / maxValue
                                                if (i === 0)
                                                    ctx.moveTo(x, y)
                                                else
                                                    ctx.lineTo(x, y)
                                            }
                                            ctx.stroke()
                                        }

                                        onPaint: {
                                            var ctx = getContext("2d")
                                            ctx.reset()
                                            var w = width
                                            var h = height
                                            var left = 28
                                            var right = 7
                                            var top = 10
                                            var bottom = 22
                                            var plotWidth = Math.max(1, w - left - right)
                                            var plotHeight = Math.max(1, h - top - bottom)
                                            var hours = root.mapLayers
                                                ? root.mapLayers.bandActivityWindowHours : 6
                                            var bucketCount = Math.max(4, hours * 4)
                                            var endMs = Math.floor(Date.now() / 900000) * 900000
                                                + 900000
                                            var startMs = endMs - bucketCount * 900000
                                            var localRx = []
                                            var localTx = []
                                            var pskRx = []
                                            var pskTx = []
                                            var buckets = ({})
                                            var rows = root.mapLayers
                                                ? (root.mapLayers.bandActivityTimeline || []) : []
                                            for (var r = 0; r < rows.length; ++r) {
                                                if (String(rows[r].band || "")
                                                        !== root.activitySelectedBand)
                                                    continue
                                                buckets[String(Number(rows[r].bucketMs))] = rows[r]
                                            }
                                            var maxValue = 1
                                            for (var i = 0; i < bucketCount; ++i) {
                                                var bucket = buckets[String(startMs + i * 900000)]
                                                    || ({})
                                                var values = [
                                                    Number(bucket.localRx || 0),
                                                    Number(bucket.localTx || 0),
                                                    Number(bucket.pskRx || 0),
                                                    Number(bucket.pskTx || 0)
                                                ]
                                                localRx.push(values[0])
                                                localTx.push(values[1])
                                                pskRx.push(values[2])
                                                pskTx.push(values[3])
                                                for (var v = 0; v < values.length; ++v)
                                                    maxValue = Math.max(maxValue, values[v])
                                            }

                                            ctx.strokeStyle = root.glassBorder
                                            ctx.lineWidth = 1
                                            ctx.fillStyle = root.textSecondary
                                            ctx.font = "8px monospace"
                                            for (var line = 0; line <= 4; ++line) {
                                                var y = top + plotHeight * line / 4
                                                ctx.beginPath()
                                                ctx.moveTo(left, y)
                                                ctx.lineTo(left + plotWidth, y)
                                                ctx.stroke()
                                            }
                                            ctx.fillText(String(maxValue), 1, top + 4)
                                            ctx.fillText("0", 15, top + plotHeight + 3)
                                            ctx.fillText("-" + hours + "h", left,
                                                         h - 3)
                                            var nowLabel = qsTr("now")
                                            ctx.fillText(nowLabel,
                                                left + plotWidth
                                                - ctx.measureText(nowLabel).width,
                                                h - 3)

                                            drawSeries(ctx, localRx, root.secondaryCyan,
                                                       left, top, plotWidth,
                                                       plotHeight, maxValue)
                                            drawSeries(ctx, localTx, root.accentGreen,
                                                       left, top, plotWidth,
                                                       plotHeight, maxValue)
                                            drawSeries(ctx, pskRx, root.accentAmber,
                                                       left, top, plotWidth,
                                                       plotHeight, maxValue)
                                            drawSeries(ctx, pskTx, "#d16cff",
                                                       left, top, plotWidth,
                                                       plotHeight, maxValue)
                                        }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: !root.activitySelectedBand
                                        text: qsTr("No band activity in this window")
                                        color: root.textSecondary
                                        font.pixelSize: 9
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    Repeater {
                                        model: [
                                            { label: qsTr("Local RX"), tone: root.secondaryCyan },
                                            { label: qsTr("Local TX"), tone: root.accentGreen },
                                            { label: qsTr("PSK RX"), tone: root.accentAmber },
                                            { label: qsTr("PSK TX"), tone: "#d16cff" }
                                        ]
                                        delegate: Row {
                                            required property var modelData
                                            spacing: 3
                                            Rectangle {
                                                width: 8
                                                height: 3
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: modelData.tone
                                            }
                                            Text {
                                                text: modelData.label
                                                color: root.textSecondary
                                                font.pixelSize: 7
                                            }
                                        }
                                    }
                                }

                                Text {
                                    text: qsTr("BAND RANKING")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }

                                Repeater {
                                    model: root.mapLayers
                                        ? root.mapLayers.bandActivity : []
                                    delegate: Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 52
                                        radius: 3
                                        color: root.activitySelectedBand === modelData.band
                                            ? "#153040" : "#101a28"
                                        border.width: 1
                                        border.color: modelData.best
                                            ? root.accentGreen : root.glassBorder

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            spacing: 2
                                            RowLayout {
                                                Layout.fillWidth: true
                                                Text {
                                                    text: "#" + modelData.rank
                                                    color: root.textSecondary
                                                    font.pixelSize: 8
                                                }
                                                Text {
                                                    text: modelData.band
                                                    color: modelData.best
                                                        ? root.accentGreen
                                                        : root.textPrimary
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                                Text {
                                                    visible: !!modelData.best
                                                    text: qsTr("BEST")
                                                    color: root.accentGreen
                                                    font.pixelSize: 7
                                                    font.bold: true
                                                }
                                                Item { Layout.fillWidth: true }
                                                Text {
                                                    text: qsTr("%1 / 100")
                                                        .arg(modelData.score)
                                                    color: root.textPrimary
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("Local %1 RX / %2 TX   PSK %3 RX / %4 TX   %5 calls   %6 dB")
                                                    .arg(modelData.localRx)
                                                    .arg(modelData.localTx)
                                                    .arg(modelData.pskRx)
                                                    .arg(modelData.pskTx)
                                                    .arg(modelData.uniqueCalls)
                                                    .arg(Number(modelData.averageSnr).toFixed(1))
                                                color: root.textSecondary
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                root.activitySelectedBand =
                                                    String(modelData.band || "")
                                                activityChart.requestPaint()
                                            }
                                        }
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }

                        ScrollView {
                            id: awardsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: awardsContent
                                width: awardsScroll.availableWidth
                                spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardPrograms : ["None"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.activeAwardProgram)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.activeAwardProgram = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Use this award to identify wanted stations in the roster")
                                }
                                ComboBox {
                                    Layout.preferredWidth: 96
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardGoals : ["Confirmed"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.awardGoal)) : 0
                                    enabled: root.mapLayers
                                        && root.mapLayers.activeAwardProgram !== "None"
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.awardGoal = currentText
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardEndorsements : ["Mixed"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.awardEndorsement || "Mixed")) : 0
                                    enabled: root.mapLayers
                                        && root.mapLayers.activeAwardProgram !== "None"
                                    font.pixelSize: 8
                                    onActivated: root.mapLayers.awardEndorsement = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Award endorsement band")
                                }
                                ComboBox {
                                    Layout.preferredWidth: 92
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardConfirmations : ["Any"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.awardConfirmation)) : 0
                                    enabled: root.mapLayers
                                        && root.mapLayers.activeAwardProgram !== "None"
                                    font.pixelSize: 8
                                    onActivated: root.mapLayers.awardConfirmation = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Confirmation source used for the confirmed score")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Operator callsign")
                                    text: root.mapLayers ? root.mapLayers.awardCallsign : ""
                                    font.pixelSize: 8
                                    selectByMouse: true
                                    onEditingFinished: root.mapLayers.awardCallsign = text
                                }
                                TextField {
                                    Layout.preferredWidth: 72
                                    placeholderText: qsTr("From YYYY-MM-DD")
                                    text: root.mapLayers ? root.mapLayers.awardFromDate : ""
                                    font.pixelSize: 8
                                    selectByMouse: true
                                    onEditingFinished: root.mapLayers.awardFromDate = text
                                }
                                TextField {
                                    Layout.preferredWidth: 72
                                    placeholderText: qsTr("To YYYY-MM-DD")
                                    text: root.mapLayers ? root.mapLayers.awardToDate : ""
                                    font.pixelSize: 8
                                    selectByMouse: true
                                    onEditingFinished: root.mapLayers.awardToDate = text
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.mapLayers
                                    && root.mapLayers.activeAwardProgram !== "None"
                                    ? qsTr("%1 / %2 drives the Award and Wanted roster filters")
                                          .arg(root.mapLayers.activeAwardProgram)
                                          .arg(root.mapLayers.awardGoal)
                                    : qsTr("Select an award to make its missing entities operational targets")
                                color: root.textSecondary
                                font.pixelSize: 8
                                wrapMode: Text.Wrap
                            }
                            ListView {
                                id: awardsList
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(
                                    180, Math.min(420, intelligencePanel.height * 0.62))
                                clip: true
                                spacing: 5
                                model: root.mapLayers ? root.mapLayers.awards : []
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                                delegate: Rectangle {
                                    required property var modelData
                                    width: awardsList.width
                                    height: 74
                                    radius: 3
                                    color: modelData.selected
                                        ? "#182538" : "#101a28"
                                    border.width: modelData.selected ? 1 : 0
                                    border.color: root.secondaryCyan
                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 2
                                        Text {
                                            text: qsTr("%1  %2 / %3")
                                                .arg(modelData.label)
                                                .arg(modelData.achieved)
                                                .arg(modelData.target)
                                            color: modelData.complete
                                                ? root.accentGreen
                                                : root.textPrimary
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                        ProgressBar {
                                            width: parent.width
                                            from: 0
                                            to: 1
                                            value: modelData.progress
                                        }
                                        Text {
                                            width: parent.width
                                            text: qsTr("Worked %1 · Confirmed %2 · Remaining %3")
                                                .arg(modelData.worked)
                                                .arg(modelData.confirmed)
                                                .arg(modelData.remaining)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                            elide: Text.ElideRight
                                        }
                                    }
                                    MouseArea {
                                        id: awardMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.mapLayers.activeAwardProgram =
                                            modelData.label
                                    }
                                    ToolTip.visible: awardMouse.containsMouse
                                    ToolTip.text: (modelData.rule || "")
                                        + "\n"
                                        + qsTr("Select %1 as the active roster award")
                                              .arg(modelData.label)
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.mapLayers && root.mapLayers.activeAwardProgram !== "None"
                                    ? qsTr("MISSING LIVE ENTITIES (%1)")
                                          .arg(root.mapLayers.awardMissing.length) : ""
                                color: root.secondaryCyan
                                font.pixelSize: 8
                                font.bold: true
                                visible: root.mapLayers && root.mapLayers.awardMissing.length > 0
                            }
                            ListView {
                                id: awardMissingList
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.mapLayers
                                    && root.mapLayers.awardMissing.length > 0
                                    ? Math.min(180, root.mapLayers.awardMissing.length * 38) : 0
                                visible: root.mapLayers && root.mapLayers.awardMissing.length > 0
                                clip: true
                                spacing: 3
                                model: root.mapLayers ? root.mapLayers.awardMissing : []
                                delegate: Rectangle {
                                    required property var modelData
                                    width: awardMissingList.width
                                    height: 34
                                    radius: 3
                                    color: "#1b1824"
                                    border.color: root.accentAmber
                                    border.width: 1
                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        spacing: 1
                                        Text {
                                            text: qsTr("%1 · %2 · %3")
                                                .arg(modelData.entity)
                                                .arg(modelData.call)
                                                .arg(modelData.grid || "no grid")
                                            color: root.textPrimary
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        Text {
                                            text: qsTr("%1 / %2 — open roster and map")
                                                .arg(modelData.band || "All bands")
                                                .arg(modelData.mode || "All modes")
                                            color: root.textSecondary
                                            font.pixelSize: 7
                                        }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!root.mapLayers) return
                                            root.mapLayers.rosterTextMode = "Only"
                                            root.mapLayers.rosterTextFilter = modelData.call
                                            root.mapLayers.selectGrid(modelData.grid)
                                            intelligenceTabs.currentIndex = 1
                                        }
                                    }
                                }
                            }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }

                        ScrollView {
                            id: alertsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: alertsContent
                                width: alertsScroll.availableWidth
                                spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: qsTr("Mark read")
                                    font.pixelSize: 9
                                    enabled: root.mapLayers
                                        && root.mapLayers.unreadAlertCount > 0
                                    onClicked: root.mapLayers.markAlertsRead()
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: qsTr("Clear")
                                    font.pixelSize: 9
                                    onClicked: root.mapLayers.clearAlerts()
                                }
                            }
                            ListView {
                                id: alertsList
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(
                                    180, Math.min(420, intelligencePanel.height * 0.68))
                                clip: true
                                spacing: 3
                                model: root.mapLayers ? root.mapLayers.alerts : []
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: alertsList.width
                                    height: 48
                                    radius: 3
                                    color: modelData.read ? "#101a28" : "#2d2513"
                                    Text {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        wrapMode: Text.Wrap
                                        text: modelData.message
                                        color: modelData.read
                                            ? root.textSecondary : "#f6c344"
                                        font.pixelSize: 9
                                    }
                                }
                            }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    MapOperationsWindows {
        id: mapOperationsWindows
        engine: root.engine
        mapLayers: root.mapLayers
        operations: root.mapOperations
        externalOverlays: root.externalOverlays
        backgroundColor: root.bgDeep
        borderColor: root.glassBorder
        primaryColor: root.primaryBlue
        accentColor: root.accentGreen
        textColor: root.textPrimary
        mutedColor: root.textSecondary
    }

    FileDialog {
        id: mapConfigurationImportDialog
        title: qsTr("Import Decodium map configuration")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Decodium map configuration (*.json)"), qsTr("JSON files (*.json)")]
        onAccepted: root.importMapConfiguration(selectedFile)
    }

    FileDialog {
        id: offlineRasterImportDialog
        title: qsTr("Import offline world raster")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("World raster (*.png *.jpg *.jpeg *.webp *.tif *.tiff *.bmp)"),
            qsTr("Image files (*.png *.jpg *.jpeg *.webp *.tif *.tiff *.bmp)")
        ]
        onAccepted: {
            if (root.baseMapService)
                root.baseMapService.importOfflinePack(selectedFile.toString())
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        enabled: root.visible && root.mapOperations
        onActivated: root.mapOperations.cycleDataView()
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: root.visible && root.mapOperations
        onActivated: root.captureMapScreenshot()
    }
    Shortcut {
        sequence: "Ctrl+Shift+R"
        enabled: root.visible
        onActivated: mapOperationsWindows.openRoster()
    }
    Shortcut {
        sequence: "Ctrl+Shift+T"
        enabled: root.visible
        onActivated: mapOperationsWindows.openStatistics()
    }
    Shortcut {
        sequence: "Ctrl+Shift+C"
        enabled: root.visible
        onActivated: mapOperationsWindows.openConditions()
    }

    Connections {
        target: root.mapLayers
        ignoreUnknownSignals: true

        function onBandActivityChanged() {
            root.scheduleMapSnapshotSync()
        }

        function onBandActivityWindowHoursChanged() {
            activityChart.requestPaint()
        }
    }

    Connections {
        target: engine
        ignoreUnknownSignals: true

        function onGridChanged() {
            root.syncMapSettings()
            root.updateMoonOverlay()
            if (root.visible)
                root.scheduleRebuild()
        }
        // 1.0.209 — RIMOSSO: onDecodeListChanged + onRxDecodeListChanged
        // triggeravano scheduleRebuild() = clearContacts + replayWorldMapFeed
        // a ogni decode (2/sec). I nuovi contact arrivano gia' incrementali
        // via onWorldMapContactAdded signal del bridge, no replay full
        // necessario.
        //
        // I signal TX/QSO sotto chiamano solo syncTxState() (cambio target
        // line lampeggiante, no rebuild): leggera e safe a ogni cambio.
        function onTransmittingChanged() {
            root.syncTxState()
        }
        function onTuningChanged() {
            root.syncTxState()
        }
        function onDxCallChanged() {
            root.syncTxState()
            if (root.qsoViewportFocused)
                root.focusActiveQso()
        }
        function onDxGridChanged() {
            root.syncTxState()
            if (root.qsoViewportFocused)
                root.focusActiveQso()
        }
        function onCurrentTxChanged() {
            root.syncTxState()
        }
        function onTxEnabledChanged() {
            root.syncTxState()
        }
        function onQsoProgressChanged() {
            root.syncTxState()
        }
        function onAutoCqRepeatChanged() {
            root.syncTxState()
        }
        function onModeChanged() {
            root.syncTxState()
        }
        function onSettingValueChanged(key, value) {
            if (key === "LiveMapUseGpu") {
                root.gpuLiveMapEnabled = root.coerceBool(value, true)
                return
            }
            if (!worldMap)
                return
            if (key === "ShowGreyline" || key === "MapShowGreyline") {
                var greylineEnabled = root.coerceBool(value, true)
                worldMap.setGreylineEnabled(greylineEnabled)
                greylineBtn.greylineOn = greylineEnabled
            } else if (key === "Miles") {
                worldMap.setDistanceInMiles(root.coerceBool(value, false))
            } else if (key === "WorldMapDisplayed" && root.visible) {
                root.scheduleRebuild()
            }
        }
        function onWorldMapResetRequested() {
            if (!root.visible || !worldMap)
                return
            worldMap.clearContacts()
        }
        function onWorldMapContactAdded(call, sourceGrid, destinationGrid, role) {
            if (!root.visible || !worldMap
                    || (root.mapLayers && !root.mapLayers.liveLayerEnabled)
                    || !root.decoderFeedAllowed())
                return
            worldMap.addContact(call, sourceGrid, destinationGrid, role)
        }
        function onWorldMapContactAddedByLonLat(call, sourceLon, sourceLat, destinationGrid, role) {
            if (!root.visible || !worldMap
                    || (root.mapLayers && !root.mapLayers.liveLayerEnabled)
                    || !root.decoderFeedAllowed())
                return
            worldMap.addContactByLonLat(call, sourceLon, sourceLat, destinationGrid, role)
        }
        function onWorldMapContactDowngraded(call) {
            if (!root.visible || !worldMap || !root.decoderFeedAllowed())
                return
            worldMap.downgradeContactToBand(call)
        }
    }

    Connections {
        target: root.mapLayers
        ignoreUnknownSignals: true

        function onCoverageChanged() {
            root.scheduleMapSnapshotSync()
        }
        // Snapshot refreshes also emit filtersChanged. Coverage and roster
        // have dedicated incremental signals, so a full contact replay here
        // would block the UI after every decode cycle.
        function onLiveLayerEnabledChanged() {
            if (!root.worldMap)
                return
            root.worldMap.clearContacts()
            if (root.visible && root.mapLayers.liveLayerEnabled
                    && root.decoderFeedAllowed())
                root.engine.replayWorldMapFeed()
            root.syncSpotPaths()
        }
        function onSpotAnalyticsChanged() {
            if (root.visible && root.mapLayers && root.mapLayers.pskLayerEnabled)
                spotPathRefreshTimer.restart()
        }
        function onCoveragePushPinsEnabledChanged() {
            root.syncMapSettings()
        }
        function onTimeZoneOverlayEnabledChanged() {
            root.syncMapSettings()
        }
    }

    Timer {
        id: spotPathRefreshTimer
        interval: 2000
        repeat: false
        onTriggered: {
            if (!root.visible || !root.worldMap)
                return
            // Spot analytics only changes PSK paths. Decoder contacts already
            // arrive incrementally through worldMapContactAdded; clearing and
            // replaying them here caused two full-map rebuilds per FT slot.
            root.syncSpotPaths()
            root.syncTxState()
        }
    }

    Connections {
        target: root.mapOperations
        ignoreUnknownSignals: true

        function onOperationalMarkersChanged() {
            root.syncOperations()
        }
        function onGeographicFeaturesChanged() {
            root.syncOperations()
        }
        function onMapProjectionChanged() {
            root.syncOperations()
        }
        function onSelectedPotaParkChanged() {
            if (root.operationalDetailsVisible
                    && root.mapOperations.selectedPotaPark
                    && Object.keys(root.mapOperations.selectedPotaPark).length > 0)
                root.selectedOperationalDetails =
                    root.mapOperations.selectedPotaPark
        }
    }

    Connections {
        target: root.satelliteTracking
        ignoreUnknownSignals: true

        function onStateChanged() {
            root.syncOperations()
        }
        function onSatellitesChanged() {
            root.syncOperations()
        }
    }

    Connections {
        target: root.externalOverlays
        ignoreUnknownSignals: true

        function onEarthquakeFeaturesChanged() {
            root.syncOperations()
        }
        function onMoonDataChanged() {
            root.syncOperations()
            if (root.moonLocatePending && root.externalOverlays
                    && root.externalOverlays.moonDataAvailable) {
                root.moonLocatePending = false
                Qt.callLater(root.locateMoon)
            }
        }
    }

    Connections {
        target: root.mapLayers ? root.mapLayers.layerModel : null
        ignoreUnknownSignals: true

        function onLayerToggled(layerId, enabled) {
            if (enabled && (layerId === "states" || layerId === "counties")
                    && root.worldMap) {
                // These boundary sources are U.S.-only.  Center them when the
                // layer is enabled so a valid 56/3k item count is immediately
                // visible instead of looking like an empty world layer.
                root.worldMap.focusLocation(-98.5, 39.0, 72, 44)
            }
            if (layerId === "pota" || layerId === "states"
                    || layerId === "counties" || layerId === "iota"
                    || layerId === "wpx" || layerId === "earthquakes")
                root.syncOperations()
        }
    }
}
