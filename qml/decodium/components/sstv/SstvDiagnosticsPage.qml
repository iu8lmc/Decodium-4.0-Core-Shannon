pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

SstvPage {
    id: root

    required property var engine
    readonly property var controller: root.engine
                                      ? root.engine.sstvDiagnostics : null
    property string accessibleStatus: controller
                                      ? controller.statusText
                                      : qsTr("Native SSTV diagnostics are unavailable")
    property bool statusIsError: controller
                                 ? controller.errorString.length > 0 : true

    function scalar(map, key, fallback) {
        if (!map || map[key] === undefined || map[key] === null
                || map[key] === "")
            return fallback === undefined ? qsTr("Unavailable") : fallback
        return String(map[key])
    }

    component ScalarRow: Item {
        id: scalarRow
        required property string label
        required property string value
        Layout.fillWidth: true
        implicitHeight: Math.max(metricLabel.implicitHeight,
                                 metricValue.implicitHeight)
        Label {
            id: metricLabel
            anchors.left: parent.left
            anchors.top: parent.top
            width: Math.max(0, parent.width * 0.52 - 4)
            text: scalarRow.label
            color: root.secondaryTextColor
            elide: Text.ElideRight
            Accessible.ignored: true
        }
        Label {
            id: metricValue
            anchors.left: metricLabel.right
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.top: parent.top
            text: scalarRow.value
            color: root.primaryTextColor
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.Wrap
            Accessible.name: scalarRow.label + ": " + scalarRow.value
        }
    }

    component MetricSection: Rectangle {
        id: metricSection
        required property string title
        default property alias content: metricColumn.data
        Layout.fillWidth: true
        implicitHeight: metricColumn.implicitHeight + 22
        color: "#081017"
        border.color: root.borderColor
        border.width: 1
        radius: 7
        ColumnLayout {
            id: metricColumn
            anchors.fill: parent
            anchors.margins: 11
            spacing: 6
            Label {
                Layout.fillWidth: true
                text: metricSection.title
                color: root.accentColor
                font.bold: true
                font.pixelSize: 12
                Accessible.role: Accessible.Heading
            }
        }
    }

    FileDialog {
        id: exportDialog
        objectName: "sstvDiagnosticsExportDialog"
        title: qsTr("Export privacy-safe SSTV diagnostics")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("JSON report (*.json)")]
        defaultSuffix: "json"
        onAccepted: {
            if (root.controller)
                root.controller.exportReport(selectedFile, false)
        }
    }

    Connections {
        target: root.controller
        enabled: root.controller !== null

        function onExportFinished(success, message) {
            root.accessibleStatus = message
            root.statusIsError = !success
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: qsTr("DIAGNOSTICS")
                color: root.accentColor
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 1.2
                Accessible.role: Accessible.Heading
            }
            Button {
                objectName: "sstvDiagnosticsRefresh"
                text: qsTr("Refresh")
                enabled: root.controller && root.controller.ready
                         && !root.controller.exporting
                onClicked: root.controller.refresh()
                Accessible.name: qsTr("Refresh SSTV diagnostic counters")
            }
            Button {
                objectName: "sstvDiagnosticsTestTone"
                text: root.controller && root.controller.testToneResults.running
                      ? qsTr("Transmitting 1500 Hz...")
                      : qsTr("Transmit 1500 Hz (2 s)")
                enabled: root.controller && root.controller.ready
                         && root.controller.capabilities.analogTx
                         && root.controller.testToneResults.available
                         && !root.controller.testToneResults.running
                onClicked: root.controller.requestTestTone()
                Accessible.name: qsTr("Transmit a 1500 Hz SSTV calibration tone for two seconds")
                Accessible.description: qsTr("This may key the configured radio through PTT or VOX after Decodium preflight and SWR safety checks.")
            }
            Button {
                objectName: "sstvDiagnosticsExport"
                text: root.controller && root.controller.exporting
                      ? qsTr("Exporting...") : qsTr("Export JSON...")
                enabled: root.controller && root.controller.ready
                         && !root.controller.exporting
                onClicked: exportDialog.open()
                Accessible.name: qsTr("Export privacy-safe SSTV diagnostic report")
            }
        }

        Rectangle {
            objectName: "sstvDiagnosticsStatus"
            Layout.fillWidth: true
            implicitHeight: diagnosticsStatus.implicitHeight + 16
            color: root.statusIsError ? "#3a171b" : "#123124"
            border.color: root.statusIsError ? "#d55b66" : "#43d17b"
            border.width: 1
            radius: 5
            Accessible.role: Accessible.StaticText
            Accessible.name: diagnosticsStatus.text
            Label {
                id: diagnosticsStatus
                anchors.fill: parent
                anchors.margins: 8
                text: root.controller && root.controller.errorString.length > 0
                      ? root.controller.errorString : root.accessibleStatus
                color: root.primaryTextColor
                wrapMode: Text.WordWrap
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.max(0, parent.width - 14)
                spacing: 9

                MetricSection {
                    title: qsTr("Application and mode registry")
                    ScalarRow {
                        label: qsTr("Decodium / Qt")
                        value: root.scalar(root.controller
                                           ? root.controller.applicationInfo : {},
                                           "version") + " / "
                               + root.scalar(root.controller
                                             ? root.controller.applicationInfo : {},
                                             "qtVersion")
                    }
                    ScalarRow {
                        label: qsTr("Platform / ABI")
                        value: root.scalar(root.controller
                                           ? root.controller.platformInfo : {},
                                           "productType") + " / "
                               + root.scalar(root.controller
                                             ? root.controller.platformInfo : {},
                                             "buildAbi")
                    }
                    ScalarRow {
                        label: qsTr("Canonical registry")
                        value: root.controller
                               ? qsTr("v%1 - %2 modes - %3")
                                   .arg(root.scalar(
                                       root.controller.modeRegistryInfo,
                                       "schemaVersion", "0"))
                                   .arg(root.scalar(
                                       root.controller.modeRegistryInfo,
                                       "modeCount", "0"))
                                   .arg(root.scalar(
                                       root.controller.modeRegistryInfo,
                                       "sha256").slice(0, 16))
                               : qsTr("Unavailable")
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width < 760 ? 1 : 2
                    columnSpacing: 9
                    rowSpacing: 9

                    MetricSection {
                        title: qsTr("RX")
                        ScalarRow { label: qsTr("State / mode"); value: root.scalar(root.controller ? root.controller.rxMetrics : {}, "state") + " / " + root.scalar(root.controller ? root.controller.rxMetrics : {}, "modeId") }
                        ScalarRow { label: qsTr("Queued chunks / samples"); value: root.scalar(root.controller ? root.controller.rxMetrics : {}, "queuedChunks", "0") + " / " + root.scalar(root.controller ? root.controller.rxMetrics : {}, "queuedSamples", "0") }
                        ScalarRow { label: qsTr("Dropped chunks / samples"); value: root.scalar(root.controller ? root.controller.rxMetrics : {}, "droppedChunks", "0") + " / " + root.scalar(root.controller ? root.controller.rxMetrics : {}, "droppedSamples", "0") }
                        ScalarRow { label: qsTr("Failures / stale chunks"); value: root.scalar(root.controller ? root.controller.rxMetrics : {}, "processingFailures", "0") + " / " + root.scalar(root.controller ? root.controller.rxMetrics : {}, "staleChunksDiscarded", "0") }
                    }

                    MetricSection {
                        title: qsTr("TX")
                        ScalarRow { label: qsTr("State / mode"); value: root.scalar(root.controller ? root.controller.txMetrics : {}, "state") + " / " + root.scalar(root.controller ? root.controller.txMetrics : {}, "modeId") }
                        ScalarRow { label: qsTr("Progress"); value: root.scalar(root.controller ? root.controller.txMetrics : {}, "progressPermille", "0") + " ‰" }
                        ScalarRow { label: qsTr("Samples / underruns"); value: root.scalar(root.controller ? root.controller.txMetrics : {}, "samplesProduced", "0") + " / " + root.scalar(root.controller ? root.controller.txMetrics : {}, "underruns", "0") }
                        ScalarRow { label: qsTr("Failures"); value: root.scalar(root.controller ? root.controller.txMetrics : {}, "failures", "0") }
                    }

                    MetricSection {
                        title: qsTr("Storage")
                        ScalarRow { label: qsTr("Records"); value: root.scalar(root.controller ? root.controller.storageMetrics : {}, "recordCount", "0") }
                        ScalarRow { label: qsTr("Image / thumbnail bytes"); value: root.scalar(root.controller ? root.controller.storageMetrics : {}, "imageBytes", "0") + " / " + root.scalar(root.controller ? root.controller.storageMetrics : {}, "thumbnailBytes", "0") }
                        ScalarRow { label: qsTr("Raw audio / quota bytes"); value: root.scalar(root.controller ? root.controller.storageMetrics : {}, "rawAudioBytes", "0") + " / " + root.scalar(root.controller ? root.controller.storageMetrics : {}, "quotaBytes", "0") }
                        ScalarRow { label: qsTr("Operations / failures"); value: root.scalar(root.controller ? root.controller.storageMetrics : {}, "operations", "0") + " / " + root.scalar(root.controller ? root.controller.storageMetrics : {}, "failures", "0") }
                    }

                    MetricSection {
                        title: qsTr("Sharing")
                        ScalarRow { label: qsTr("Queue depth"); value: root.scalar(root.controller ? root.controller.shareMetrics : {}, "activeQueueDepth", "0") }
                        ScalarRow { label: qsTr("Uploaded / downloaded bytes"); value: root.scalar(root.controller ? root.controller.shareMetrics : {}, "uploadedBytes", "0") + " / " + root.scalar(root.controller ? root.controller.shareMetrics : {}, "downloadedBytes", "0") }
                        ScalarRow { label: qsTr("Upload / download bytes per second"); value: root.scalar(root.controller ? root.controller.shareMetrics : {}, "uploadBytesPerSecond", "0") + " / " + root.scalar(root.controller ? root.controller.shareMetrics : {}, "downloadBytesPerSecond", "0") }
                        ScalarRow { label: qsTr("Failures"); value: root.scalar(root.controller ? root.controller.shareMetrics : {}, "failures", "0") }
                    }

                    MetricSection {
                        title: qsTr("HAMDRM")
                        ScalarRow { label: qsTr("Available / state"); value: root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "available", "false") + " / " + root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "state") }
                        ScalarRow { objectName: "sstvDiagnosticsHamDrmBytes"; label: qsTr("RX / TX bytes"); value: root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "receivedBytes", qsTr("Unavailable")) + " / " + root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "transmittedBytes", qsTr("Unavailable")) }
                        ScalarRow { objectName: "sstvDiagnosticsHamDrmRepair"; label: qsTr("CRC failures / BSR requests"); value: root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "crcFailures", qsTr("Unavailable")) + " / " + root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "bsrRequests", qsTr("Unavailable")) }
                        ScalarRow { objectName: "sstvDiagnosticsHamDrmFailures"; label: qsTr("Failures"); value: root.scalar(root.controller ? root.controller.hamdrmMetrics : {}, "failures", qsTr("Unavailable")) }
                    }

                    MetricSection {
                        title: qsTr("Calibration and test tone")
                        ScalarRow { label: qsTr("Calibration result"); value: root.scalar(root.controller ? root.controller.calibrationResults : {}, "success", "false") }
                        ScalarRow { label: qsTr("Calibration frequency"); value: root.scalar(root.controller ? root.controller.calibrationResults : {}, "frequencyHz", "0") + " Hz" }
                        ScalarRow { label: qsTr("Test tone state"); value: root.scalar(root.controller ? root.controller.testToneResults : {}, "running", "false") }
                        ScalarRow { label: qsTr("Test tone frequency / duration"); value: root.scalar(root.controller ? root.controller.testToneResults : {}, "frequencyHz", "0") + " Hz / " + root.scalar(root.controller ? root.controller.testToneResults : {}, "durationMs", "0") + " ms" }
                    }
                }

                MetricSection {
                    title: qsTr("Bounded structured events")
                    ScalarRow {
                        label: qsTr("Events available for export")
                        value: root.controller
                               ? String(root.controller.recentEvents.length)
                               : "0"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Only explicitly structured, allowlisted SSTV events are retained. Images, audio, paths, person metadata and credentials are excluded.")
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Button {
                            objectName: "sstvDiagnosticsClearEvents"
                            text: qsTr("Clear events")
                            enabled: root.controller
                                     && root.controller.recentEvents.length > 0
                            onClicked: root.controller.clearDiagnosticEvents()
                            Accessible.name: qsTr("Clear the bounded SSTV diagnostic event buffer")
                        }
                    }
                }
            }
        }
    }
}
