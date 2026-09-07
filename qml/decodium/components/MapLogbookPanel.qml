import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var operations: null
    property var mapLayers: null
    property color backgroundColor: "#0b1220"
    property color borderColor: "#2a3950"
    property color primaryColor: "#3f7cff"
    property color accentColor: "#2ecc71"
    property color textColor: "#e5eefc"
    property color mutedColor: "#9db1c9"
    signal callRequested(string call, string grid)

    spacing: 5

    function exportRows(format) {
        if (!operations)
            return
        var path = operations.reserveLogbookExportPath(format)
        operations.exportLogbook(path, format)
    }

    Timer {
        id: searchTimer
        interval: 280
        repeat: false
        onTriggered: {
            if (root.operations)
                root.operations.logbookSearch = searchField.text
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 5

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Call, grid, DXCC, state, POTA, IOTA or WPX")
            font.pixelSize: 9
            selectByMouse: true
            onTextEdited: searchTimer.restart()
        }
        Button {
            text: qsTr("Refresh")
            font.pixelSize: 9
            enabled: root.operations && !root.operations.logbookLoading
            onClicked: root.operations.refreshLogbook()
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: width >= 430 ? 4 : 2
        columnSpacing: 5
        rowSpacing: 4

        ComboBox {
            Layout.fillWidth: true
            model: root.mapLayers ? root.mapLayers.availableBands : ["All"]
            currentIndex: root.operations
                ? Math.max(0, model.indexOf(root.operations.logbookBand)) : 0
            font.pixelSize: 9
            onActivated: root.operations.logbookBand = currentText
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Band filter")
        }
        ComboBox {
            Layout.fillWidth: true
            model: root.mapLayers ? root.mapLayers.availableModes : ["All"]
            currentIndex: root.operations
                ? Math.max(0, model.indexOf(root.operations.logbookMode)) : 0
            font.pixelSize: 9
            onActivated: root.operations.logbookMode = currentText
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Mode filter")
        }
        ComboBox {
            Layout.fillWidth: true
            model: ["All time", "24 hours", "7 days", "30 days", "1 year"]
            currentIndex: root.operations
                ? Math.max(0, model.indexOf(root.operations.logbookPeriod)) : 0
            font.pixelSize: 9
            onActivated: root.operations.logbookPeriod = currentText
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Period")
        }
        ComboBox {
            Layout.fillWidth: true
            model: ["Date", "Call", "Band", "Mode", "DXCC", "Grid",
                    "Frequency", "Status"]
            currentIndex: root.operations
                ? Math.max(0, model.indexOf(root.operations.logbookSort)) : 0
            font.pixelSize: 9
            onActivated: root.operations.logbookSort = currentText
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Sort field")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 5

        CheckBox {
            text: qsTr("Newest first")
            checked: root.operations
                ? root.operations.logbookSortDescending : true
            font.pixelSize: 9
            onToggled: root.operations.logbookSortDescending = checked
        }
        Text {
            Layout.fillWidth: true
            text: root.operations
                ? qsTr("%1 matching QSO").arg(root.operations.logbookTotal) : ""
            color: root.mutedColor
            font.pixelSize: 9
        }
        Button {
            text: qsTr("CSV")
            font.pixelSize: 9
            enabled: root.operations && !root.operations.exportInProgress
            onClicked: root.exportRows("CSV")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Export the current filtered logbook to Documents/Decodium")
        }
        Button {
            text: qsTr("ADIF")
            font.pixelSize: 9
            enabled: root.operations && !root.operations.exportInProgress
            onClicked: root.exportRows("ADIF")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Export the current filtered logbook to Documents/Decodium")
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: 4
        rowSpacing: 3

        Repeater {
            model: [
                { label: qsTr("QSO"), key: "qsos", color: root.primaryColor },
                { label: qsTr("QSL"), key: "confirmed", color: root.accentColor },
                { label: qsTr("DXCC"), key: "dxcc", color: "#f6c344" },
                { label: qsTr("Grids"), key: "grids", color: "#44d7e8" },
                { label: qsTr("Calls"), key: "calls", color: root.primaryColor },
                { label: "POTA", key: "pota", color: "#74d66a" },
                { label: "IOTA", key: "iota", color: "#44d7e8" },
                { label: "WPX", key: "wpx", color: "#f0b94d" }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 3
                color: "#101a28"
                border.width: 1
                border.color: root.borderColor
                Column {
                    anchors.centerIn: parent
                    spacing: 0
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.operations
                            ? Number(root.operations.scorecard[modelData.key] || 0) : 0
                        color: modelData.color
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: root.mutedColor
                        font.pixelSize: 7
                    }
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 22
        color: "#172337"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            Text { Layout.preferredWidth: 78; text: qsTr("DATE"); color: root.mutedColor; font.pixelSize: 8; font.bold: true }
            Text { Layout.preferredWidth: 82; text: qsTr("CALL"); color: root.mutedColor; font.pixelSize: 8; font.bold: true }
            Text { Layout.preferredWidth: 50; text: qsTr("GRID"); color: root.mutedColor; font.pixelSize: 8; font.bold: true }
            Text { Layout.preferredWidth: 42; text: qsTr("BAND"); color: root.mutedColor; font.pixelSize: 8; font.bold: true }
            Text { Layout.preferredWidth: 46; text: qsTr("MODE"); color: root.mutedColor; font.pixelSize: 8; font.bold: true }
            Text { Layout.fillWidth: true; text: qsTr("REFERENCE / DXCC"); color: root.mutedColor; font.pixelSize: 8; font.bold: true }
        }
    }

    ListView {
        id: logbookList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 2
        model: root.operations ? root.operations.logbookRows : []
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Rectangle {
            required property var modelData
            required property int index
            width: logbookList.width
            height: 34
            radius: 2
            color: index % 2 ? "#101a28" : "#0d2430"
            border.width: modelData.confirmed ? 1 : 0
            border.color: root.accentColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                spacing: 4
                Text {
                    Layout.preferredWidth: 78
                    text: String(modelData.date || "").replace(
                              /^(\d{4})(\d{2})(\d{2})$/, "$1-$2-$3")
                    color: root.mutedColor
                    font.pixelSize: 8
                }
                Text {
                    Layout.preferredWidth: 82
                    text: modelData.call || ""
                    color: modelData.confirmed ? root.accentColor : root.textColor
                    font.pixelSize: 9
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.preferredWidth: 50
                    text: (modelData.grid || "")
                          + (modelData.vuccGrids && modelData.vuccGrids.length > 0
                             ? " +" + modelData.vuccGrids.length : "")
                    color: root.mutedColor
                    font.pixelSize: 8
                    elide: Text.ElideRight
                    ToolTip.visible: gridHover.hovered
                    ToolTip.text: modelData.vuccGrids && modelData.vuccGrids.length > 0
                        ? qsTr("Primary: %1\nVUCC: %2")
                              .arg(modelData.grid || "-")
                              .arg(modelData.vuccGrids.join(", "))
                        : (modelData.grid || "")
                    HoverHandler { id: gridHover }
                }
                Text { Layout.preferredWidth: 42; text: modelData.band || ""; color: root.mutedColor; font.pixelSize: 8 }
                Text { Layout.preferredWidth: 46; text: modelData.mode || ""; color: root.mutedColor; font.pixelSize: 8 }
                Text {
                    Layout.fillWidth: true
                    text: modelData.pota || modelData.iota || modelData.wpx
                          || modelData.dxcc || ""
                    color: root.mutedColor
                    font.pixelSize: 8
                    elide: Text.ElideRight
                }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onDoubleClicked: root.callRequested(
                    modelData.call || "", modelData.grid || "")
            }
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: root.operations && root.operations.logbookLoading
            visible: running
        }
        Text {
            anchors.centerIn: parent
            visible: !root.operations
                     || (!root.operations.logbookLoading
                         && logbookList.count === 0)
            text: qsTr("No QSO matches the current filters")
            color: root.mutedColor
            font.pixelSize: 9
        }
    }

    Text {
        Layout.fillWidth: true
        visible: root.operations
                 && (root.operations.statusMessage.length > 0
                     || root.operations.lastExportPath.length > 0)
        text: root.operations.statusMessage
              + (root.operations.lastExportPath.length > 0
                 ? "\n" + root.operations.lastExportPath : "")
        color: root.mutedColor
        font.pixelSize: 8
        elide: Text.ElideMiddle
        wrapMode: Text.Wrap
    }

    BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 22
        Layout.preferredHeight: 22
        running: root.operations && root.operations.exportInProgress
        visible: running
    }
}
