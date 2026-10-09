// 1.0.268 (Phase 5.3 fork-only iu8lmc) — Decode History Dialog
// Esplora i decode persistiti nel DB SQLite. Filtri: callsign LIKE, banda
// exact, modo exact, date range. Export ADIF dal filtro corrente.
//
// Backend API (DecodiumBridge):
//   bridge.queryDecodeHistory(filters) -> QVariantList rows
//   bridge.queryDecodeBands() -> QStringList
//   bridge.queryDecodeModes() -> QStringList
//   bridge.queryDecodeStats() -> QVariantMap {total, first_utc, last_utc, session_count}
//   bridge.exportAdifFromHistory(filters, path) -> bool
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: historyDialog
    title: qsTr("Decode History (DB)")
    width: 1100
    height: 700
    minimumWidth: 800
    minimumHeight: 500
    // 1.0.270 (fork-only) — Qt.Window invece di Qt.Dialog:
    // su Windows il flag Qt.Dialog limitava drag/resize/maximize.
    // Adesso e' una finestra normale con title bar nativa + tutti i pulsanti.
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
         | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint
         | Qt.WindowCloseButtonHint
    color: "#1a1a2e"

    readonly property color cAccent:  "#3a9dff"
    readonly property color cGreen:   "#3acb6c"
    readonly property color cOrange:  "#ff9b3a"
    readonly property color cRed:     "#e84545"
    readonly property color cText:    "#e8edf2"
    readonly property color cMuted:   "#a5afc4"
    readonly property color cBorder:  "#4a5372"
    readonly property color cFieldBg: "#30354f"
    readonly property color cFieldBgFocus: "#384463"
    readonly property color cRowAlt:  "#1f2236"

    property var results: []
    property var stats: ({})
    property var bands: []
    property var modes: []

    function refreshStats() {
        if (!bridge) return
        stats = bridge.queryDecodeStats()
        bands = bridge.queryDecodeBands()
        modes = bridge.queryDecodeModes()
        bandCombo.model = ["All"].concat(bands)
        modeCombo.model = ["All"].concat(modes)
    }

    function currentFilters(includeSort) {
        var f = {}
        if (callsignField.text.trim().length > 0) f.callsign = callsignField.text.trim()
        if (bandCombo.currentIndex > 0)           f.band     = bandCombo.currentText
        if (modeCombo.currentIndex > 0)           f.mode     = modeCombo.currentText
        if (fromDateField.text.trim().length > 0) {
            var fd = Date.parse(fromDateField.text.trim() + "T00:00:00Z")
            if (!isNaN(fd)) f.fromUtc = fd
        }
        if (toDateField.text.trim().length > 0) {
            var td = Date.parse(toDateField.text.trim() + "T23:59:59Z")
            if (!isNaN(td)) f.toUtc = td
        }
        f.limit = limitSpin.value
        if (includeSort)
            f.newestFirst = true
        return f
    }

    function runQuery() {
        if (!bridge) return
        var f = currentFilters(true)
        results = bridge.queryDecodeHistory(f)
    }

    function openExportAdifDialog() {
        if (!bridge) return
        var path = bridge.saveFileDialog(qsTr("Export ADIF"), "", [qsTr("ADIF (*.adi)")])
        if (path.length === 0)
            return
        var lower = path.toLowerCase()
        if (lower.slice(-4) !== ".adi" && lower.slice(-5) !== ".adif")
            path += ".adi"
        bridge.exportAdifFromHistory(currentFilters(false), path)
    }

    function formatUtc(ms) {
        if (!ms || ms <= 0) return ""
        var d = new Date(ms)
        return d.toISOString().substring(0, 19).replace("T", " ")
    }

    Component.onCompleted: refreshStats()
    onVisibleChanged: if (visible) { refreshStats(); runQuery() }

    Rectangle {
        anchors.fill: parent
        color: historyDialog.color

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            // ===== HEADER =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: "📚  " + qsTr("Decode History")
                    color: historyDialog.cText
                    font.pixelSize: 18
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: stats && stats.total !== undefined
                          ? qsTr("%1 total decodes  •  %2 sessions").arg(stats.total).arg(stats.session_count || 0)
                          : qsTr("Empty DB")
                    color: historyDialog.cMuted
                    font.pixelSize: 12
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: historyDialog.cBorder }

            // ===== FILTERS =====
            GridLayout {
                Layout.fillWidth: true
                columns: 6
                columnSpacing: 8
                rowSpacing: 6

                Text { text: qsTr("Callsign"); color: historyDialog.cMuted; font.pixelSize: 11 }
                Text { text: qsTr("Band");     color: historyDialog.cMuted; font.pixelSize: 11 }
                Text { text: qsTr("Mode");     color: historyDialog.cMuted; font.pixelSize: 11 }
                Text { text: qsTr("From (UTC)"); color: historyDialog.cMuted; font.pixelSize: 11 }
                Text { text: qsTr("To (UTC)"); color: historyDialog.cMuted; font.pixelSize: 11 }
                Text { text: qsTr("Limit");    color: historyDialog.cMuted; font.pixelSize: 11 }

                DecoTextField {
                    id: callsignField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("e.g. F4CQS")
                    placeholderTextColor: "#7d89a5"
                    color: historyDialog.cText
                    font.family: decodiumMonoFontFamily; font.pixelSize: 13
                    selectByMouse: true
                    background: Rectangle {
                        color: callsignField.activeFocus ? historyDialog.cFieldBgFocus : historyDialog.cFieldBg
                        border.color: callsignField.activeFocus ? historyDialog.cAccent : historyDialog.cBorder
                        border.width: callsignField.activeFocus ? 2 : 1
                        radius: 5
                    }
                    onAccepted: historyDialog.runQuery()
                }
                DecoComboBox {
                    id: bandCombo
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    model: ["All"]
                    onActivated: historyDialog.runQuery()
                    contentItem: Text {
                        text: bandCombo.displayText
                        color: historyDialog.cText
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                        rightPadding: 28
                        elide: Text.ElideRight
                    }
                    indicator: Text {
                        x: bandCombo.width - width - 10
                        y: (bandCombo.height - height) / 2
                        text: "▾"
                        color: historyDialog.cMuted
                        font.pixelSize: 14
                    }
                    background: Rectangle {
                        color: bandCombo.activeFocus || bandCombo.hovered ? historyDialog.cFieldBgFocus : historyDialog.cFieldBg
                        border.color: bandCombo.activeFocus ? historyDialog.cAccent : historyDialog.cBorder
                        border.width: bandCombo.activeFocus ? 2 : 1
                        radius: 5
                    }
                    delegate: ItemDelegate {
                        width: bandCombo.width
                        contentItem: Text {
                            text: modelData
                            color: historyDialog.cText
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        background: Rectangle {
                            color: highlighted ? Qt.alpha(historyDialog.cAccent, 0.28) : historyDialog.cFieldBg
                        }
                    }
                    popup: Popup {
                        y: bandCombo.height + 2
                        width: bandCombo.width
                        implicitHeight: contentItem.implicitHeight
                        padding: 2
                        background: Rectangle { color: historyDialog.cFieldBg; border.color: historyDialog.cBorder; radius: 5 }
                        contentItem: ListView {
                            clip: true
                            implicitHeight: Math.min(contentHeight, 260)
                            model: bandCombo.popup.visible ? bandCombo.delegateModel : null
                            currentIndex: bandCombo.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                    }
                }
                DecoComboBox {
                    id: modeCombo
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    model: ["All"]
                    onActivated: historyDialog.runQuery()
                    contentItem: Text {
                        text: modeCombo.displayText
                        color: historyDialog.cText
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                        rightPadding: 28
                        elide: Text.ElideRight
                    }
                    indicator: Text {
                        x: modeCombo.width - width - 10
                        y: (modeCombo.height - height) / 2
                        text: "▾"
                        color: historyDialog.cMuted
                        font.pixelSize: 14
                    }
                    background: Rectangle {
                        color: modeCombo.activeFocus || modeCombo.hovered ? historyDialog.cFieldBgFocus : historyDialog.cFieldBg
                        border.color: modeCombo.activeFocus ? historyDialog.cAccent : historyDialog.cBorder
                        border.width: modeCombo.activeFocus ? 2 : 1
                        radius: 5
                    }
                    delegate: ItemDelegate {
                        width: modeCombo.width
                        contentItem: Text {
                            text: modelData
                            color: historyDialog.cText
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        background: Rectangle {
                            color: highlighted ? Qt.alpha(historyDialog.cAccent, 0.28) : historyDialog.cFieldBg
                        }
                    }
                    popup: Popup {
                        y: modeCombo.height + 2
                        width: modeCombo.width
                        implicitHeight: contentItem.implicitHeight
                        padding: 2
                        background: Rectangle { color: historyDialog.cFieldBg; border.color: historyDialog.cBorder; radius: 5 }
                        contentItem: ListView {
                            clip: true
                            implicitHeight: Math.min(contentHeight, 260)
                            model: modeCombo.popup.visible ? modeCombo.delegateModel : null
                            currentIndex: modeCombo.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                    }
                }
                DecoTextField {
                    id: fromDateField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: "yyyy-mm-dd"
                    placeholderTextColor: "#7d89a5"
                    color: historyDialog.cText
                    font.family: decodiumMonoFontFamily; font.pixelSize: 13
                    background: Rectangle {
                        color: fromDateField.activeFocus ? historyDialog.cFieldBgFocus : historyDialog.cFieldBg
                        border.color: fromDateField.activeFocus ? historyDialog.cAccent : historyDialog.cBorder
                        border.width: fromDateField.activeFocus ? 2 : 1
                        radius: 5
                    }
                    onAccepted: historyDialog.runQuery()
                }
                DecoTextField {
                    id: toDateField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: "yyyy-mm-dd"
                    placeholderTextColor: "#7d89a5"
                    color: historyDialog.cText
                    font.family: decodiumMonoFontFamily; font.pixelSize: 13
                    background: Rectangle {
                        color: toDateField.activeFocus ? historyDialog.cFieldBgFocus : historyDialog.cFieldBg
                        border.color: toDateField.activeFocus ? historyDialog.cAccent : historyDialog.cBorder
                        border.width: toDateField.activeFocus ? 2 : 1
                        radius: 5
                    }
                    onAccepted: historyDialog.runQuery()
                }
                SpinBox {
                    id: limitSpin
                    from: 50; to: 5000; stepSize: 50
                    value: 500
                    editable: true
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 34
                    contentItem: TextInput {
                        selectByMouse: true
                        onActiveFocusChanged: if (activeFocus) selectAll()
                        z: 2
                        text: limitSpin.textFromValue(limitSpin.value, limitSpin.locale)
                        color: historyDialog.cText
                        selectionColor: historyDialog.cAccent
                        selectedTextColor: historyDialog.cText
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        readOnly: !limitSpin.editable
                        validator: limitSpin.validator
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        font.family: decodiumMonoFontFamily
                        font.pixelSize: 13
                    }
                    up.indicator: Rectangle {
                        x: limitSpin.width - width
                        width: 30
                        height: limitSpin.height
                        color: limitSpin.up.pressed ? Qt.alpha(historyDialog.cAccent, 0.35) : Qt.alpha(historyDialog.cAccent, 0.18)
                        border.color: historyDialog.cBorder
                        radius: 5
                        Text { anchors.centerIn: parent; text: "+"; color: historyDialog.cText; font.pixelSize: 18; font.bold: true }
                    }
                    down.indicator: Rectangle {
                        x: 0
                        width: 30
                        height: limitSpin.height
                        color: limitSpin.down.pressed ? Qt.alpha(historyDialog.cAccent, 0.35) : Qt.alpha(historyDialog.cAccent, 0.18)
                        border.color: historyDialog.cBorder
                        radius: 5
                        Text { anchors.centerIn: parent; text: "-"; color: historyDialog.cText; font.pixelSize: 18; font.bold: true }
                    }
                    background: Rectangle {
                        color: limitSpin.activeFocus ? historyDialog.cFieldBgFocus : historyDialog.cFieldBg
                        border.color: limitSpin.activeFocus ? historyDialog.cAccent : historyDialog.cBorder
                        border.width: limitSpin.activeFocus ? 2 : 1
                        radius: 5
                    }
                }
            }

            // ===== ACTIONS =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Button {
                    text: "🔍  " + qsTr("Search")
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 34
                    onClicked: historyDialog.runQuery()
                    background: Rectangle {
                        color: parent.down ? Qt.alpha(historyDialog.cAccent, 0.38)
                                           : (parent.hovered ? Qt.alpha(historyDialog.cAccent, 0.32)
                                                             : Qt.alpha(historyDialog.cAccent, 0.24))
                        border.color: historyDialog.cAccent; border.width: 2
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text; color: "#78bdff"
                        font.bold: true; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: qsTr("Clear filters")
                    Layout.preferredWidth: 118
                    Layout.preferredHeight: 34
                    onClicked: {
                        callsignField.text = ""
                        bandCombo.currentIndex = 0
                        modeCombo.currentIndex = 0
                        fromDateField.text = ""
                        toDateField.text = ""
                        historyDialog.runQuery()
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.alpha(historyDialog.cText, 0.20)
                                           : (parent.hovered ? Qt.alpha(historyDialog.cText, 0.15)
                                                             : Qt.alpha(historyDialog.cText, 0.10))
                        border.color: historyDialog.cBorder
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text; color: historyDialog.cText
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: historyDialog.results.length > 0
                          ? qsTr("%1 results").arg(historyDialog.results.length)
                          : qsTr("No results")
                    color: historyDialog.cMuted
                    font.pixelSize: 12
                }
                Button {
                    text: "💾  " + qsTr("Export ADIF…")
                    enabled: historyDialog.results.length > 0
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 34
                    onClicked: historyDialog.openExportAdifDialog()
                    background: Rectangle {
                        color: parent.enabled
                               ? (parent.down ? Qt.alpha(historyDialog.cGreen, 0.36)
                                              : (parent.hovered ? Qt.alpha(historyDialog.cGreen, 0.30)
                                                                : Qt.alpha(historyDialog.cGreen, 0.22)))
                               : Qt.alpha(historyDialog.cText, 0.06)
                        border.color: parent.enabled ? historyDialog.cGreen : historyDialog.cBorder
                        border.width: parent.enabled ? 2 : 1
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#58e089" : historyDialog.cMuted
                        font.bold: parent.enabled; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: historyDialog.cBorder }

            // ===== TABLE HEADER =====
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                color: Qt.alpha(historyDialog.cAccent, 0.12)
                radius: 4
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 4
                    Text { text: "UTC";    color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 150 }
                    Text { text: "Band";   color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 50 }
                    Text { text: "Mode";   color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 50 }
                    Text { text: "DX";     color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 90 }
                    Text { text: "DE";     color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 90 }
                    Text { text: qsTr("Grid");   color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 55 }
                    Text { text: "SNR";    color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
                    Text { text: qsTr("Freq");   color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                    Text { text: qsTr("Message"); color: historyDialog.cAccent; font.bold: true; font.pixelSize: 11; Layout.fillWidth: true }
                }
            }

            // ===== TABLE BODY =====
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0, 0, 0, 0.25)
                border.color: historyDialog.cBorder
                radius: 4

                ListView {
                    id: resultsList
                    anchors.fill: parent
                    anchors.margins: 2
                    clip: true
                    spacing: 0
                    cacheBuffer: 800
                    reuseItems: true
                    model: historyDialog.results
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: ListView.view ? ListView.view.width : 0
                        height: 22
                        color: index % 2 === 0 ? "transparent" : historyDialog.cRowAlt

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 4

                            Text {
                                text: parent.parent.modelData ? historyDialog.formatUtc(parent.parent.modelData.ts_utc) : ""
                                color: historyDialog.cMuted
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                Layout.preferredWidth: 150
                            }
                            Text {
                                text: parent.parent.modelData ? parent.parent.modelData.band || "" : ""
                                color: historyDialog.cText
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                Layout.preferredWidth: 50
                            }
                            Text {
                                text: parent.parent.modelData ? parent.parent.modelData.mode || "" : ""
                                color: historyDialog.cText
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                Layout.preferredWidth: 50
                            }
                            Text {
                                text: parent.parent.modelData ? parent.parent.modelData.callsign_dx || "" : ""
                                color: historyDialog.cAccent
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11; font.bold: true
                                Layout.preferredWidth: 90
                                elide: Text.ElideRight
                            }
                            Text {
                                text: parent.parent.modelData ? parent.parent.modelData.callsign_de || "" : ""
                                color: historyDialog.cText
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                Layout.preferredWidth: 90
                                elide: Text.ElideRight
                            }
                            Text {
                                text: parent.parent.modelData ? parent.parent.modelData.grid || "" : ""
                                color: historyDialog.cMuted
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                Layout.preferredWidth: 55
                            }
                            Text {
                                text: parent.parent.modelData && parent.parent.modelData.snr_db !== undefined
                                      ? String(parent.parent.modelData.snr_db) : ""
                                color: historyDialog.cText
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 36
                            }
                            Text {
                                text: parent.parent.modelData && parent.parent.modelData.freq_hz
                                      ? (parent.parent.modelData.freq_hz / 1000).toFixed(2) : ""
                                color: historyDialog.cMuted
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 70
                            }
                            Text {
                                text: parent.parent.modelData ? parent.parent.modelData.message || "" : ""
                                color: historyDialog.cText
                                font.family: decodiumMonoFontFamily; font.pixelSize: 11
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // ===== FOOTER =====
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Close")
                    Layout.preferredWidth: 90
                    onClicked: historyDialog.close()
                    background: Rectangle {
                        color: Qt.alpha(historyDialog.cText, 0.08)
                        border.color: historyDialog.cBorder
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text; color: historyDialog.cText
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

}
