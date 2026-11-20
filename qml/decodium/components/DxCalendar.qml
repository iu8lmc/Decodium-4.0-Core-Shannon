import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    objectName: "dxCalendar"
    property var bridge
    property var entries: []
    property string statusText: ""
    property bool loading: false
    property var pendingRequest: null
    property var requestFactory: null
    property bool persistenceEnabled: true
    property var cacheBackend: typeof dxCalendarCache === "undefined" ? null : dxCalendarCache
    property var visibleEntries: []
    property int parseSerial: 0
    property int filterSerial: 0
    property var workerJobs: []
    readonly property string feedUrl: "https://www.danplanet.com/dxcal.ics"
    readonly property string sourceUrl: "https://www.ng3k.com/Misc/adxo.html"
    property string todayUtc: new Date().toISOString().slice(0, 10)
    property color panelColor: "#101722"
    property color textColor: "#e7edf7"
    property color mutedColor: "#a7b2c0"
    property color accentColor: "#00d9ff"
    component CalendarButton: Button {
        id: styledButton
        implicitWidth: Math.max(88, buttonText.implicitWidth + 24)
        implicitHeight: 34
        padding: 0
        topInset: 0
        bottomInset: 0
        contentItem: Text {
            id: buttonText
            text: styledButton.text
            color: styledButton.enabled ? root.accentColor : root.mutedColor
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 4
            color: styledButton.down ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.22) : root.panelColor
            border.color: styledButton.enabled ? root.accentColor : root.mutedColor
        }
    }
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, Math.max(320, parent ? parent.width - 24 : 760))
    height: Math.min(650, Math.max(300, parent ? parent.height - 24 : 650))
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 12
    background: Rectangle {
        color: root.panelColor
        border.color: root.accentColor
        radius: 8
    }

    function queueWorker(message) {
        workerJobs.push(message)
        workerDispatch.restart()
    }
    Timer {
        id: workerDispatch
        interval: 1
        onTriggered: {
            var jobs = root.workerJobs
            root.workerJobs = []
            for (var i = 0; i < jobs.length; ++i) calendarWorker.sendMessage(jobs[i])
        }
    }
    WorkerScript {
        id: calendarWorker
        source: "DxCalendarWorker.js"
        onMessage: function(message) {
            if (message.kind === "filter") {
                if (message.serial === root.filterSerial) root.visibleEntries = message.rows
                return
            }
            if (message.kind !== "parse" || message.serial !== root.parseSerial) return
            root.loading = false
            if (!message.entries.length) {
                root.statusText = message.fromCache
                    ? qsTr("No saved calendar. Select Update to download it.")
                    : qsTr("The source returned no valid entries. The saved calendar was kept.")
                return
            }
            root.entries = message.entries
            if (message.fromCache) root.statusText = qsTr("Saved calendar: %1").arg(message.updated)
            else {
                root.statusText = qsTr("Updated: %1").arg(message.updated)
                if (root.persistenceEnabled && root.cacheBackend)
                    root.cacheBackend.writeCache(message.text, message.updated)
            }
        }
    }
    Connections {
        target: root.cacheBackend
        ignoreUnknownSignals: true
        function onCacheRead(data, updated, error) {
            if (root.loading || root.entries.length) return
            if (error || !data) { root.statusText = qsTr("No saved calendar. Select Update to download it."); return }
            root.queueWorker({kind:"parse", serial: ++root.parseSerial, text:data,
                                        updated:updated, fromCache:true})
        }
        function onCacheWritten(error) {
            if (error) root.statusText = qsTr("Calendar updated, but the offline copy could not be saved.")
        }
    }
    function requestFilter() {
        if (!query || !filter) return
        root.queueWorker({kind:"filter", serial: ++filterSerial, entries:entries,
                                    query:query.text, selection:filter.currentValue || "current", today:todayUtc})
    }
    onEntriesChanged: requestFilter()
    onTodayUtcChanged: requestFilter()
    Timer {
        id: timeout
        interval: 15000
        onTriggered: {
            if (root.pendingRequest)
                root.pendingRequest.abort();
            root.pendingRequest = null;
            root.loading = false;
            root.statusText = qsTr("Update timed out. Showing the last saved calendar.");
        }
    }
    Timer {
        interval: 60000
        repeat: true
        running: root.opened
        onTriggered: root.todayUtc = new Date().toISOString().slice(0, 10)
    }
    function restore() {
        statusText = qsTr("No saved calendar. Select Update to download it.")
        if (persistenceEnabled && cacheBackend) cacheBackend.readCache()
    }
    function refresh() {
        if (loading)
            return;
        if (bridge && bridge.offlineMode) {
            statusText = qsTr("Online services are paused. The saved calendar remains available.");
            return;
        }
        loading = true;
        ++parseSerial;
        statusText = qsTr("Updating calendar…");
        var request = requestFactory ? requestFactory() : new XMLHttpRequest();
        pendingRequest = request;
        request.onreadystatechange = function () {
            if (request.readyState !== XMLHttpRequest.DONE || root.pendingRequest !== request)
                return;
            timeout.stop();
            root.pendingRequest = null;
            if (request.status !== 200 || request.responseText.length > 2000000) {
                root.loading = false
                root.statusText = qsTr("Update failed. Showing the last saved calendar.")
                return
            }
            var updated = new Date().toISOString().replace("T", " ").slice(0, 16) + " UTC"
            root.queueWorker({kind:"parse", serial: ++root.parseSerial,
                                        text:request.responseText, updated:updated, fromCache:false})
        };
        try {
            request.open("GET", feedUrl);
            timeout.restart();
            request.send();
        } catch (error) {
            timeout.stop();
            pendingRequest = null;
            loading = false;
            statusText = qsTr("Update failed. Showing the last saved calendar.");
        }
    }
    Component.onCompleted: restore()
    onClosed: {
        if (pendingRequest) {
            var request = pendingRequest;
            pendingRequest = null;
            timeout.stop();
            loading = false;
            request.abort();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("DXpedition calendar")
                color: root.accentColor
                font.bold: true
                font.pixelSize: 16
                Layout.fillWidth: true
            }
            CalendarButton {
                objectName: "dxCalendarRefresh"
                text: root.loading ? qsTr("Updating…") : qsTr("Update")
                enabled: !root.loading
                onClicked: root.refresh()
            }
            CalendarButton {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
        Label {
            text: root.statusText
            color: root.mutedColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: query
            onTextChanged: root.requestFilter()
                objectName: "dxCalendarSearch"
                Layout.fillWidth: true
                placeholderText: qsTr("Callsign, entity or details")
                color: root.textColor
                placeholderTextColor: root.mutedColor
                selectionColor: root.accentColor
                selectedTextColor: root.panelColor
                implicitHeight: 34
                background: Rectangle {
                    radius: 4
                    color: root.panelColor
                    border.color: root.accentColor
                }
                selectByMouse: true
            }
            DecoComboBox {
                id: filter
                Layout.preferredWidth: 190
                decoTextColor: root.textColor
                decoMutedTextColor: root.mutedColor
                decoBackgroundColor: root.panelColor
                decoPopupColor: root.panelColor
                decoBorderColor: root.accentColor
                decoAccentColor: root.accentColor
            onCurrentValueChanged: root.requestFilter()
                objectName: "dxCalendarFilter"
                textRole: "label"
                valueRole: "key"
                model: [
                    {
                        label: qsTr("Active and upcoming"),
                        key: "current"
                    },
                    {
                        label: qsTr("Active"),
                        key: "active"
                    },
                    {
                        label: qsTr("Upcoming"),
                        key: "upcoming"
                    },
                    {
                        label: qsTr("All"),
                        key: "all"
                    }
                ]
            }
        }
        ListView {
            id: list
            objectName: "dxCalendarList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: root.visibleEntries
            delegate: Rectangle {
                required property var modelData
                width: list.width
                height: details.implicitHeight + 44
                radius: 4
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: Qt.rgba(1, 1, 1, 0.12)
                Column {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 8
                    }
                    spacing: 4
                    Text {
                        width: parent.width
                        text: (modelData.call ? modelData.call + " · " : "") + modelData.entity + "   " + modelData.start + " → " + modelData.end
                        textFormat: Text.PlainText
                        color: root.accentColor
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        id: details
                        width: parent.width
                        text: modelData.description || qsTr("No details provided")
                        textFormat: Text.PlainText
                        color: root.textColor
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No matching operations")
                color: root.mutedColor
            }
        }
        Text {
            Layout.fillWidth: true
            color: root.mutedColor
            font.pixelSize: 11
            wrapMode: Text.Wrap
            text: qsTr("Announcements: NG3K ADXO · calendar feed: DXCAL / KK7DS. Dates may change; verify with the operation.")
        }
        RowLayout {
            CalendarButton {
                text: qsTr("Open NG3K source")
                onClicked: Qt.openUrlExternally(root.sourceUrl)
            }
            CalendarButton {
                text: qsTr("Open DXCAL feed")
                onClicked: Qt.openUrlExternally(root.feedUrl)
            }
        }
    }
}
