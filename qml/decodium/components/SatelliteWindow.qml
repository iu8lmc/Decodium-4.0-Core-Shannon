import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

pragma ComponentBehavior: Bound

Dialog {
    id: satelliteWindow
    title: qsTr("Satellite tracking")
    modal: false
    width: nativeHostWindow && parent
           ? Math.max(600, parent.width - 28)
           : Math.min(820, Math.max(600, Screen.desktopAvailableWidth - 96))
    // Keep the popup inside the usable desktop area on compact and Retina
    // displays.  The content below remains reachable through the scroll view.
    height: nativeHostWindow && parent
            ? Math.max(520, parent.height - 28)
            : Math.min(660, Math.max(520, Screen.desktopAvailableHeight - 160))
    padding: 14
    closePolicy: nativeHostWindow ? Popup.NoAutoClose : Popup.CloseOnEscape

    property var nativeHostWindow: null
    property var bridge: (typeof appEngine !== "undefined") ? appEngine : null
    property var tracker: bridge ? bridge.satelliteTracking : null
    property var rotator: tracker ? tracker.rotator : null
    property bool positionInitialized: false
    readonly property int positionNotSaved: -1000000000
    property color bgDeep: bridge ? bridge.themeManager.bgDeep : "#0b1220"
    property color bgMedium: bridge ? bridge.themeManager.bgMedium : "#121c2d"
    property color textPrimary: bridge ? bridge.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: bridge ? bridge.themeManager.textSecondary : "#9db1c9"
    property color accent: bridge ? bridge.themeManager.secondaryColor : "#00d8ff"
    property color green: bridge ? bridge.themeManager.accentColor : "#2ecc71"
    property color amber: bridge ? bridge.themeManager.warningColor : "#f6c344"

    // Native tool windows are separate QQuickWindows, therefore Controls
    // need an explicit attached Material palette instead of relying on the
    // main ApplicationWindow's inheritance chain.
    Material.theme: bridge && bridge.themeManager.isLightTheme
                    ? Material.Light : Material.Dark
    Material.accent: bridge ? bridge.themeManager.primaryColor : satelliteWindow.accent
    Material.primary: bridge ? bridge.themeManager.secondaryColor : satelliteWindow.accent

    function positionBoundsItem() {
        var bounds = parent
        // The lazy Loader adopts exactly this Dialog's size. It is therefore
        // not a useful movement boundary: parent.width - width would always
        // be zero and would snap the popup back after every mouse event.
        while (bounds && bounds.parent
               && (Number(bounds.width) <= width + 1
                   || Number(bounds.height) <= height + 1)) {
            bounds = bounds.parent
        }
        return bounds
    }

    function positionParentOrigin(bounds) {
        if (!parent || !bounds || parent === bounds) return Qt.point(0, 0)
        return parent.mapToItem(bounds, 0, 0)
    }

    function clampToParent() {
        if (nativeHostWindow) return
        if (!parent) return
        var bounds = positionBoundsItem()
        if (!bounds) return
        var origin = positionParentOrigin(bounds)
        var minX = -origin.x
        var minY = -origin.y
        var maxX = Number(bounds.width) - width - origin.x
        var maxY = Number(bounds.height) - height - origin.y
        x = maxX >= minX ? Math.max(minX, Math.min(x, maxX)) : minX
        y = maxY >= minY ? Math.max(minY, Math.min(y, maxY)) : minY
    }

    function savedPosition(key, fallback) {
        if (!bridge || !bridge.getSetting) return fallback
        var value = Number(bridge.getSetting(key, fallback))
        return isFinite(value) ? value : fallback
    }

    function saveWindowPosition() {
        if (nativeHostWindow) return
        if (!bridge || !bridge.setSetting || !parent) return
        clampToParent()
        bridge.setSetting("uiSatelliteWindowX", Math.round(x))
        bridge.setSetting("uiSatelliteWindowY", Math.round(y))
    }

    function scheduleWindowPositionSave() {
        if (nativeHostWindow) return
        if (bridge && bridge.setSetting) positionSaveTimer.restart()
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        var savedX = savedPosition("uiSatelliteWindowX", positionNotSaved)
        var savedY = savedPosition("uiSatelliteWindowY", positionNotSaved)
        if (savedX !== positionNotSaved && savedY !== positionNotSaved) {
            x = savedX
            y = savedY
        } else {
            var bounds = positionBoundsItem()
            var origin = positionParentOrigin(bounds)
            x = Math.round((Number(bounds.width) - width) / 2 - origin.x)
            y = Math.round((Number(bounds.height) - height) / 2 - origin.y)
        }
        clampToParent()
        positionInitialized = true
    }

    function startNativeHostMove() {
        if (!nativeHostWindow || typeof nativeHostWindow.startSystemMove !== "function")
            return false
        try {
            return nativeHostWindow.startSystemMove()
        } catch (error) {
            console.log("Satellite native move failed: " + error)
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
        satelliteWindow.close()
    }

    function updateObserver() {
        if (!tracker || !bridge) return
        tracker.setObserverGrid(String(bridge.grid || ""))
        tracker.nominalFrequencyHz = Number(bridge.frequency || 0)
    }

    function manualRotatorStep(azimuthDelta, elevationDelta) {
        if (!rotator || !rotator.enabled) return

        var baseAzimuth = rotator.feedbackAvailable
                ? Number(rotator.currentAzimuth) : Number(rotator.targetAzimuth)
        var baseElevation = rotator.feedbackAvailable
                ? Number(rotator.currentElevation) : Number(rotator.targetElevation)
        if (!isFinite(baseAzimuth)) baseAzimuth = 0
        if (!isFinite(baseElevation)) baseElevation = 0

        var nextAzimuth = (baseAzimuth + azimuthDelta) % 360
        if (nextAzimuth < 0) nextAzimuth += 360
        var nextElevation = Math.max(-10, Math.min(180, baseElevation + elevationDelta))
        rotator.setTarget(nextAzimuth, nextElevation, true)
    }

    function selectFirstSatellite() {
        if (!tracker || tracker.selectedSatellite || !tracker.satelliteNames
                || tracker.satelliteNames.length === 0)
            return
        tracker.selectSatellite(tracker.satelliteNames[0])
    }

    Timer {
        id: positionSaveTimer
        interval: 250
        repeat: false
        onTriggered: satelliteWindow.saveWindowPosition()
    }

    onAboutToShow: {
        ensureInitialPosition()
        updateObserver()
        selectFirstSatellite()
        if (tracker && tracker.upcomingPasses.length === 0)
            tracker.predictPassesAsync(24, 0)
    }

    onAboutToHide: {
        positionSaveTimer.stop()
        saveWindowPosition()
    }

    Connections {
        target: Qt.application
        ignoreUnknownSignals: true
        function onAboutToQuit() { satelliteWindow.saveWindowPosition() }
    }

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: accent
        border.width: 2
        radius: 12
    }

    header: Rectangle {
        implicitHeight: 54
        // Keep the Dialog border visible behind the header.  An opaque header
        // used to cover the electric-blue top edge and its upper side pixels.
        color: "transparent"

        MouseArea {
            id: headerDragArea
            // The title RowLayout is visual-only, but it sits above children
            // in the stacking order. Keep the drag surface above it while
            // leaving the close button area excluded by rightMargin.
            z: 4
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.rightMargin: 62
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            cursorShape: Qt.SizeAllCursor
            property real pressX: 0
            property real pressY: 0
            property point pressGlobalPos: Qt.point(0, 0)
            property point pressWindowPos: Qt.point(0, 0)
            property bool nativeMoveActive: false

            onPressed: function(mouse) {
                pressX = mouse.x
                pressY = mouse.y
                pressGlobalPos = mapToGlobal(mouse.x, mouse.y)
                if (satelliteWindow.nativeHostWindow) {
                    pressWindowPos = Qt.point(satelliteWindow.nativeHostWindow.x,
                                              satelliteWindow.nativeHostWindow.y)
                    nativeMoveActive = satelliteWindow.startNativeHostMove()
                } else {
                    nativeMoveActive = false
                }
                satelliteWindow.positionInitialized = true
                mouse.accepted = true
            }

            onPositionChanged: function(mouse) {
                if (!pressed) return
                if (nativeMoveActive) return
                if (satelliteWindow.nativeHostWindow) {
                    var currentGlobalPos = mapToGlobal(mouse.x, mouse.y)
                    satelliteWindow.nativeHostWindow.x = pressWindowPos.x
                            + currentGlobalPos.x - pressGlobalPos.x
                    satelliteWindow.nativeHostWindow.y = pressWindowPos.y
                            + currentGlobalPos.y - pressGlobalPos.y
                    mouse.accepted = true
                    return
                }
                // Incremental local deltas are stable for QML Popups: each
                // event moves the dialog only by the distance travelled since
                // the previous rendered position, without overlay transforms.
                satelliteWindow.x += mouse.x - pressX
                satelliteWindow.y += mouse.y - pressY
                satelliteWindow.clampToParent()
                satelliteWindow.scheduleWindowPositionSave()
                mouse.accepted = true
            }

            onReleased: {
                nativeMoveActive = false
                if (satelliteWindow.nativeHostWindow)
                    satelliteWindow.finishNativeHostMove()
                else
                    satelliteWindow.scheduleWindowPositionSave()
            }
            onCanceled: {
                nativeMoveActive = false
                satelliteWindow.finishNativeHostMove()
            }
        }

        RowLayout {
            z: 2
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 10

            Text {
                text: qsTr("🛰 Satellite tracking")
                color: accent
                font.pixelSize: 20
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            ToolButton {
                id: closeButton
                z: 3
                text: "✕"
                implicitWidth: 34
                implicitHeight: 34
                font.pixelSize: 14
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close")
                background: Rectangle {
                    radius: 6
                    color: closeButton.hovered
                           ? Qt.rgba(244 / 255, 67 / 255, 54 / 255, 0.24)
                           : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.10)
                    border.color: closeButton.hovered ? "#f44336" : Qt.rgba(accent.r, accent.g, accent.b, 0.35)
                }
                onClicked: satelliteWindow.requestWindowClose()
            }
        }
    }

    contentItem: Item {
        implicitWidth: satelliteContentLayout.implicitWidth
        implicitHeight: satelliteContentLayout.implicitHeight

        ColumnLayout {
            id: satelliteContentLayout
            anchors.fill: parent
            spacing: 0

            ScrollView {
                id: satelliteScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                rightPadding: 10
                bottomPadding: 8
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    id: satelliteForm
                    width: Math.max(0, satelliteScroll.availableWidth - 2)
                    spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ComboBox {
                id: satelliteCombo
                Layout.fillWidth: true
                model: tracker ? tracker.satelliteNames : []
                currentIndex: {
                    if (!tracker || !tracker.selectedSatellite) return -1
                    return model.indexOf(tracker.selectedSatellite)
                }
                onActivated: {
                    if (tracker) tracker.selectSatellite(currentText)
                }
            }
            Button {
                Layout.preferredWidth: 140
                text: tracker && tracker.updating ? qsTr("Updating…") : qsTr("Refresh TLE")
                enabled: !!tracker && !tracker.updating
                onClicked: tracker.refreshTle()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Button {
                    Layout.preferredWidth: 110
                    text: tracker && tracker.tracking ? qsTr("Stop") : qsTr("Track")
                    enabled: !!tracker && !!tracker.selectedSatellite
                    onClicked: tracker.tracking ? tracker.stopTracking() : tracker.startTracking()
                }
                CheckBox {
                    text: qsTr("Auto rotator")
                    enabled: !!tracker
                    checked: tracker ? tracker.autoRotator : false
                    onToggled: if (tracker) tracker.autoRotator = checked
                }
                CheckBox {
                    text: qsTr("Rotator enabled")
                    enabled: !!tracker
                    checked: tracker ? tracker.rotatorEnabled : false
                    onToggled: if (tracker) tracker.rotatorEnabled = checked
                }
                CheckBox {
                    text: qsTr("Auto Doppler")
                    enabled: !!tracker
                    checked: tracker ? tracker.dopplerTracking : false
                    onToggled: if (tracker) tracker.dopplerTracking = checked
                }
                Item { Layout.fillWidth: true }
            }

            Text {
                text: tracker ? tracker.statusMessage : qsTr("Satellite service unavailable")
                color: textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                visible: !!tracker && tracker.tracking && tracker.autoRotator
                text: tracker && !tracker.rotatorEnabled
                      ? qsTr("Satellite tracking active • enable ‘Rotator enabled’ to send commands")
                      : tracker && !tracker.visible
                        ? qsTr("Satellite tracking active • waiting for the satellite above the horizon")
                        : qsTr("Satellite tracking active • rotator commands are sent asynchronously")
                color: tracker && tracker.rotatorEnabled && tracker.visible ? green : amber
                font.pixelSize: 9
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: qsTr("Protocol"); color: textSecondary; font.pixelSize: 10 }
                    ComboBox {
                        Layout.preferredWidth: 175
                        Layout.minimumWidth: 175
                        enabled: !!rotator
                        model: rotator ? rotator.protocols : []
                        currentIndex: rotator ? Math.max(0, model.indexOf(rotator.protocol)) : 0
                        onActivated: if (tracker) tracker.rotatorProtocol = currentText
                    }
                    Label { text: qsTr("Host"); color: textSecondary; font.pixelSize: 10 }
                    TextField {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 180
                        enabled: !!rotator
                        text: tracker ? tracker.rotatorHost : "127.0.0.1"
                        placeholderText: qsTr("IP or hostname")
                        onEditingFinished: if (tracker) tracker.rotatorHost = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: rotator ? qsTr("%1 command port").arg(rotator.transport) : qsTr("Command port"); color: textSecondary; font.pixelSize: 10 }
                    SpinBox {
                        id: rotatorPortSpinBox
                        Layout.preferredWidth: 145
                        Layout.minimumWidth: 145
                        enabled: !!rotator
                        from: 1
                        to: 65535
                        editable: true
                        value: tracker ? tracker.rotatorPort : 12000
                        onValueModified: if (tracker) tracker.rotatorPort = value
                        contentItem: TextInput {
                            selectByMouse: true
                            onActiveFocusChanged: if (activeFocus) selectAll()
                            z: 2
                            text: rotatorPortSpinBox.textFromValue(
                                      rotatorPortSpinBox.value,
                                      rotatorPortSpinBox.locale)
                            color: rotatorPortSpinBox.enabled ? textPrimary : textSecondary
                            selectionColor: accent
                            selectedTextColor: textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            readOnly: !rotatorPortSpinBox.editable
                            validator: rotatorPortSpinBox.validator
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            font.pixelSize: 14
                        }
                        background: Rectangle {
                            color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.72)
                            border.color: rotatorPortSpinBox.activeFocus ? accent : textSecondary
                            border.width: rotatorPortSpinBox.activeFocus ? 2 : 1
                            radius: 6
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            Text {
                Layout.fillWidth: true
                text: rotator
                      ? (rotator.protocol === "PSTRotator"
                         ? qsTr("UDP command port: %1 • feedback port: %2")
                               .arg(rotator.port)
                               .arg(rotator.port < 65535 ? rotator.port + 1 : qsTr("unavailable"))
                         : rotator.protocol === "CatRotator"
                           ? qsTr("UDP command port: %1 • feedback unavailable")
                                 .arg(rotator.port)
                           : qsTr("TCP rotctld: %1:%2 • commands + feedback on same port")
                                 .arg(rotator.host).arg(rotator.port))
                      : qsTr("Rotator service unavailable")
                color: textSecondary
                font.pixelSize: 9
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Label {
                    text: qsTr("Manual movement")
                    color: textSecondary
                    font.pixelSize: 10
                }
                Button {
                    id: azimuthLeftButton
                    text: "← AZ"
                    enabled: !!rotator && rotator.enabled
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 36
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move azimuth −10°")
                    contentItem: Text {
                        text: azimuthLeftButton.text
                        color: azimuthLeftButton.enabled ? textPrimary : textSecondary
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: satelliteWindow.manualRotatorStep(-10, 0)
                }
                Button {
                    id: azimuthRightButton
                    text: "→ AZ"
                    enabled: !!rotator && rotator.enabled
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 36
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move azimuth +10°")
                    contentItem: Text {
                        text: azimuthRightButton.text
                        color: azimuthRightButton.enabled ? textPrimary : textSecondary
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: satelliteWindow.manualRotatorStep(10, 0)
                }
                Button {
                    id: elevationUpButton
                    text: "↑ EL"
                    enabled: !!rotator && rotator.enabled
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 36
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move elevation +5°")
                    contentItem: Text {
                        text: elevationUpButton.text
                        color: elevationUpButton.enabled ? textPrimary : textSecondary
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: satelliteWindow.manualRotatorStep(0, 5)
                }
                Button {
                    id: elevationDownButton
                    text: "↓ EL"
                    enabled: !!rotator && rotator.enabled
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 58
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move elevation −5°")
                    contentItem: Text {
                        text: elevationDownButton.text
                        color: elevationDownButton.enabled ? textPrimary : textSecondary
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: satelliteWindow.manualRotatorStep(0, -5)
                }
                Text {
                    text: rotator && rotator.enabled
                          ? qsTr("AZ ±10° • EL ±5°")
                          : qsTr("Enable ‘Rotator enabled’ to send commands")
                    color: rotator && rotator.enabled ? textSecondary : amber
                    font.pixelSize: 9
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.72)
                radius: 8
                border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.35)

            GridLayout {
                anchors.fill: parent
                anchors.margins: 12
                columns: 4
                rowSpacing: 8
                columnSpacing: 16
                Text { text: qsTr("Azimuth"); color: textSecondary }
                Text { text: tracker ? Number(tracker.azimuth).toFixed(1) + "°" : "—"; color: textPrimary; font.bold: true }
                Text { text: qsTr("Elevation"); color: textSecondary }
                Text { text: tracker ? Number(tracker.elevation).toFixed(1) + "°" : "—"; color: tracker && tracker.visible ? green : amber; font.bold: true }
                Text { text: qsTr("Range"); color: textSecondary }
                Text { text: tracker ? Number(tracker.rangeKm).toFixed(0) + " km" : "—"; color: textPrimary }
                Text { text: qsTr("Visibility"); color: textSecondary }
                Text { text: tracker ? (tracker.visible ? qsTr("VISIBLE") : qsTr("BELOW HORIZON")) : "—"; color: tracker && tracker.visible ? green : amber }
                Text { text: qsTr("Doppler"); color: textSecondary }
                Text { text: tracker ? Number(tracker.dopplerHz).toFixed(0) + " Hz" : "—"; color: textPrimary }
                Text { text: qsTr("Tracked frequency"); color: textSecondary }
                Text { text: tracker && tracker.dopplerFrequencyHz > 0 ? (tracker.dopplerFrequencyHz / 1000000).toFixed(6) + " MHz" : "—"; color: textPrimary }
                Text { text: qsTr("Observer"); color: textSecondary }
                Text { text: tracker ? (tracker.observerGrid || (Number(tracker.observerLatitude).toFixed(2) + ", " + Number(tracker.observerLongitude).toFixed(2))) : "—"; color: textPrimary }
                Text { text: qsTr("TLE age"); color: textSecondary }
                Text { text: tracker && tracker.tleUpdatedMs > 0 ? Qt.formatDateTime(new Date(tracker.tleUpdatedMs), "yyyy-MM-dd HH:mm") : "—"; color: textPrimary }
                Text { text: qsTr("Rotor feedback"); color: textSecondary }
                Text {
                    text: rotator && rotator.feedbackAvailable
                        ? qsTr("AZ %1° / EL %2°")
                              .arg(Number(rotator.currentAzimuth).toFixed(1))
                              .arg(Number(rotator.currentElevation).toFixed(1))
                        : qsTr("Unavailable")
                    color: rotator && rotator.feedbackAvailable ? green : amber
                }
            }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Rotor STOP")
                    enabled: !!rotator
                    onClicked: if (rotator) rotator.stop()
                }
                Button {
                    text: qsTr("Rotor PARK")
                    enabled: !!rotator
                    onClicked: if (rotator) rotator.park()
                }
                Text {
                    Layout.fillWidth: true
                    text: rotator ? rotator.status : ""
                    color: textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: tracker && tracker.predictingPasses ? qsTr("Calculating passes…") : qsTr("Upcoming passes")
                    color: accent
                    font.bold: true
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Predict 24 h")
                    enabled: !!tracker && !tracker.predictingPasses && !!tracker.selectedSatellite
                    onClicked: tracker.predictPassesAsync(24, 0)
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 230
                Layout.minimumHeight: 160
                clip: true
                model: tracker ? tracker.upcomingPasses : []
                spacing: 4
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 48
                    radius: 5
                    color: index % 2
                           ? Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.55)
                           : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.72)
                    border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.16)
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        Text { text: String(modelData && modelData.aos || "—").replace("T", " "); color: textPrimary; Layout.preferredWidth: 150; elide: Text.ElideRight }
                        Text { text: qsTr("max %1°").arg(Number(modelData && modelData.maxElevation || 0).toFixed(1)); color: green; Layout.preferredWidth: 80 }
                        Text { text: qsTr("LOS %1").arg(String(modelData && modelData.los || "—").replace("T", " ")); color: textSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: qsTr("AZ %1°→%2°").arg(Number(modelData && modelData.aosAzimuth || 0).toFixed(0)).arg(Number(modelData && modelData.losAzimuth || 0).toFixed(0)); color: textSecondary; font.pixelSize: 10 }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: parent.count === 0
                    text: tracker && tracker.predictingPasses ? qsTr("Calculation running in background…") : qsTr("No pass in the selected interval")
                    color: textSecondary
                }
            }
        }
    }
    }
    }

    Connections {
        target: tracker
        ignoreUnknownSignals: true
        function onSatellitesChanged() { selectFirstSatellite() }
        function onSelectedSatelliteChanged() {
            if (tracker && tracker.selectedSatellite)
                tracker.predictPassesAsync(24, 0)
        }
    }
    Connections {
        target: bridge
        ignoreUnknownSignals: true
        function onGridChanged() { updateObserver() }
        function onFrequencyChanged() {
            if (tracker && !tracker.dopplerTracking)
                tracker.nominalFrequencyHz = Number(bridge.frequency || 0)
        }
    }
}
