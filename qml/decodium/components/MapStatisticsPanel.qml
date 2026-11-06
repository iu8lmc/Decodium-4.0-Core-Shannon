import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    property var operations: null
    property var mapLayers: null
    property var legacyStatistics: ({})
    property color borderColor: "#2a3950"
    property color primaryColor: "#3f7cff"
    property color accentColor: "#2ecc71"
    property color textColor: "#e5eefc"
    property color mutedColor: "#9db1c9"

    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    function exportStatistics(format) {
        if (!root.operations)
            return
        var path = root.operations.reserveStatisticsExportPath(format)
        root.operations.exportStatistics(path, format)
    }

    function deltaText(value) {
        var number = Number(value || 0)
        return number >= 0 ? "+" + number : String(number)
    }

    function activeAwardSummary() {
        if (!root.mapLayers)
            return ({})
        var selected = root.mapLayers.activeAwardProgram || "None"
        var awards = root.mapLayers.awards || []
        for (var index = 0; index < awards.length; ++index) {
            if (String(awards[index].label || "").toLowerCase()
                    === String(selected).toLowerCase())
                return awards[index]
        }
        return ({ label: selected })
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("LOGBOOK STATISTICS")
                color: root.primaryColor
                font.pixelSize: 12
                font.bold: true
            }
            Button {
                text: qsTr("JSON")
                font.pixelSize: 9
                enabled: !!root.operations
                onClicked: root.exportStatistics("JSON")
            }
            Button {
                text: qsTr("CSV")
                font.pixelSize: 9
                enabled: !!root.operations
                onClicked: root.exportStatistics("CSV")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Award focus")
                color: root.mutedColor
                font.pixelSize: 9
            }
            ComboBox {
                Layout.fillWidth: true
                enabled: !!root.mapLayers
                model: root.mapLayers ? root.mapLayers.availableAwardPrograms : []
                currentIndex: root.mapLayers
                    ? Math.max(0, model.indexOf(root.mapLayers.activeAwardProgram)) : 0
                font.pixelSize: 9
                onActivated: {
                    if (root.mapLayers)
                        root.mapLayers.activeAwardProgram = currentText
                }
            }
            Text {
                text: root.operations
                    ? qsTr("%1 QSO").arg(root.operations.logbookTotal) : ""
                color: root.mutedColor
                font.pixelSize: 9
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#101a28"
            border.width: 1
            border.color: root.borderColor
            visible: !!root.mapLayers && root.mapLayers.activeAwardProgram !== "None"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 · worked %2 · confirmed %3")
                        .arg(root.activeAwardSummary().label || "Award")
                        .arg(root.activeAwardSummary().worked || 0)
                        .arg(root.activeAwardSummary().confirmed || 0)
                    color: root.textColor
                    font.pixelSize: 8
                    elide: Text.ElideRight
                }
                Text {
                    text: root.activeAwardSummary().target
                        ? qsTr("target %1").arg(root.activeAwardSummary().target) : ""
                    color: root.accentColor
                    font.pixelSize: 8
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 520 ? 4 : 2
            columnSpacing: 5
            rowSpacing: 5
            Repeater {
                model: [
                    { label: qsTr("QSO"), key: "qsos", color: root.primaryColor },
                    { label: qsTr("Confirmed"), key: "confirmed", color: root.accentColor },
                    { label: qsTr("Calls"), key: "calls", color: "#44d7e8" },
                    { label: qsTr("DXCC"), key: "dxcc", color: "#f6c344" },
                    { label: qsTr("Grids"), key: "grids", color: "#44d7e8" },
                    { label: "POTA", key: "pota", color: "#74d66a" },
                    { label: "IOTA", key: "iota", color: "#44d7e8" },
                    { label: "WPX", key: "wpx", color: "#f0b94d" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 3
                    color: "#101a28"
                    border.width: 1
                    border.color: root.borderColor
                    Column {
                        anchors.centerIn: parent
                        spacing: 1
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.operations
                                ? Number(root.operations.scorecard[modelData.key] || 0) : 0
                            color: modelData.color
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: root.mutedColor
                            font.pixelSize: 8
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.borderColor
        }

        Text {
            text: qsTr("PERIOD COMPARISON")
            color: root.primaryColor
            font.pixelSize: 9
            font.bold: true
        }
        Repeater {
            model: root.operations ? root.operations.periodComparison : []
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 3
                color: "#101a28"
                border.width: 1
                border.color: root.borderColor
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    Text {
                        Layout.preferredWidth: 72
                        text: modelData.period
                        color: root.textColor
                        font.pixelSize: 9
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("QSO %1 → %2 (%3)\nDXCC %4 · Grid %5 · WPX %6")
                            .arg(modelData.currentQsos || 0)
                            .arg(modelData.previousQsos || 0)
                            .arg(root.deltaText(modelData.qsoDelta))
                            .arg(modelData.currentDxcc || 0)
                            .arg(modelData.currentGrids || 0)
                            .arg(modelData.currentWpx || 0)
                        color: root.mutedColor
                        font.pixelSize: 8
                    }
                    Text {
                        text: qsTr("QSL %1 (%2)")
                            .arg(modelData.currentConfirmed || 0)
                            .arg(root.deltaText(modelData.confirmedDelta))
                        color: root.accentColor
                        font.pixelSize: 8
                    }
                }
            }
        }

        Text {
            text: qsTr("AWARD PROGRESSION · CUMULATIVE")
            color: root.primaryColor
            font.pixelSize: 9
            font.bold: true
        }
        Text {
            Layout.fillWidth: true
            visible: !root.operations || root.operations.awardProgression.length === 0
            text: qsTr("No dated QSO available for historical progression")
            color: root.mutedColor
            font.pixelSize: 8
        }
        Repeater {
            model: root.operations ? root.operations.awardProgression : []
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: 3
                color: "#101a28"
                border.width: 1
                border.color: root.borderColor
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    Text {
                        Layout.preferredWidth: 72
                        text: modelData.period
                        color: root.textColor
                        font.pixelSize: 9
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%1 QSO · %2 QSL\nCalls %3/%4 · DXCC %5/%6")
                            .arg(modelData.qsos || 0)
                            .arg(modelData.confirmed || 0)
                            .arg(modelData.callsWorked || 0)
                            .arg(modelData.callsConfirmed || 0)
                            .arg(modelData.dxccWorked || 0)
                            .arg(modelData.dxccConfirmed || 0)
                        color: root.mutedColor
                        font.pixelSize: 8
                    }
                    Text {
                        Layout.preferredWidth: 126
                        text: qsTr("Grid %1/%2 · WPX %3/%4")
                            .arg(modelData.gridWorked || 0)
                            .arg(modelData.gridConfirmed || 0)
                            .arg(modelData.wpxWorked || 0)
                            .arg(modelData.wpxConfirmed || 0)
                        color: root.textColor
                        font.pixelSize: 8
                    }
                }
            }
        }

        Text {
            text: qsTr("TOP BANDS · MODES · DXCC · WPX · GRIDS")
            color: root.primaryColor
            font.pixelSize: 9
            font.bold: true
        }
        Repeater {
            model: root.operations ? root.operations.topStatistics : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 4
                Text {
                    Layout.preferredWidth: 52
                    text: modelData.group
                    color: root.mutedColor
                    font.pixelSize: 8
                }
                Button {
                    Layout.fillWidth: true
                    text: modelData.label
                    font.pixelSize: 8
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open QSO drill-down")
                    onClicked: root.operations.drillDownStatistics(
                        modelData.group, modelData.label)
                }
                Text {
                    Layout.preferredWidth: 112
                    text: qsTr("%1 QSO · %2 QSL")
                        .arg(modelData.worked || 0)
                        .arg(modelData.confirmed || 0)
                    color: root.mutedColor
                    font.pixelSize: 8
                }
            }
        }

        Text {
            text: qsTr("PROFILES / CALLSIGNS")
            color: root.primaryColor
            font.pixelSize: 9
            font.bold: true
        }
        Repeater {
            model: root.operations ? root.operations.profileStatistics : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: modelData.profile
                    color: root.textColor
                    font.pixelSize: 9
                    font.bold: true
                }
                Text {
                    text: qsTr("%1 QSO · %2 QSL · %3 DXCC")
                        .arg(modelData.qsos || 0)
                        .arg(modelData.confirmed || 0)
                        .arg(modelData.dxcc || 0)
                    color: root.mutedColor
                    font.pixelSize: 8
                }
                Button {
                    text: qsTr("QSOs")
                    font.pixelSize: 8
                    onClicked: root.operations.drillDownStatistics(
                        "Profile", modelData.profile)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: !!root.operations && root.operations.statisticsDrilldown.length > 0
            Layout.preferredHeight: visible ? 190 : 1
            color: "#0d1725"
            border.width: visible ? 1 : 0
            border.color: root.borderColor
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                Text {
                    Layout.fillWidth: true
                    text: root.operations
                        ? qsTr("DRILL-DOWN · %1").arg(root.operations.statisticsDrilldown)
                        : ""
                    color: root.primaryColor
                    font.pixelSize: 9
                    font.bold: true
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.operations ? root.operations.logbookRows : []
                    delegate: RowLayout {
                        required property var modelData
                        width: parent ? parent.width : 0
                        Text { Layout.preferredWidth: 76; text: modelData.date || ""; color: root.mutedColor; font.pixelSize: 8 }
                        Text { Layout.preferredWidth: 82; text: modelData.call || ""; color: root.textColor; font.pixelSize: 8; font.bold: true }
                        Text { Layout.preferredWidth: 50; text: modelData.grid || ""; color: root.mutedColor; font.pixelSize: 8 }
                        Text { Layout.preferredWidth: 40; text: modelData.band || ""; color: root.mutedColor; font.pixelSize: 8 }
                        Text { Layout.preferredWidth: 46; text: modelData.mode || ""; color: root.mutedColor; font.pixelSize: 8 }
                        Text { Layout.fillWidth: true; text: modelData.dxcc || modelData.wpx || ""; color: root.mutedColor; font.pixelSize: 8; elide: Text.ElideRight }
                    }
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !!root.operations && root.operations.statusMessage.length > 0
            text: root.operations ? root.operations.statusMessage : ""
            color: root.mutedColor
            font.pixelSize: 8
            wrapMode: Text.Wrap
        }
    }
}
