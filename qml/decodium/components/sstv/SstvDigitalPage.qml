pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

SstvPage {
    id: root
    objectName: "sstvDigitalPage"

    required property var controller
    panelColor: "#081017"
    property url selectedTxImage: ""
    readonly property var capability: root.controller
                                      ? root.controller.capabilities : ({})
    readonly property var profile: root.controller
                                   ? root.controller.selectedProfile : ({})

    function syncProfileSelector() {
        if (!root.controller)
            return
        const values = root.controller.profiles || []
        for (let index = 0; index < values.length; ++index) {
            if (values[index].id === root.controller.selectedProfileId) {
                profileSelector.currentIndex = index
                return
            }
        }
        profileSelector.currentIndex = -1
    }

    Component.onCompleted: syncProfileSelector()

    Connections {
        target: root.controller
        enabled: root.controller !== null

        function onSelectedProfileChanged() {
            root.syncProfileSelector()
        }
    }

    FileDialog {
        id: txImagePicker
        title: qsTr("Select an encoded HAMDRM image object")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("HAMDRM images (*.jpg *.jpeg *.jp2 *.png *.gif *.bmp)")
        ]
        onAccepted: root.selectedTxImage = selectedFile
    }

    component SectionPanel: Rectangle {
        Layout.fillWidth: true
        color: root.panelColor
        border.color: root.borderColor
        border.width: 1
        radius: 7
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: Math.max(780, root.width - 10)
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("DIGITAL SSTV · HAMDRM")
                    color: root.accentColor
                    font.pixelSize: 16
                    font.bold: true
                    font.letterSpacing: 1.1
                }
                Rectangle {
                    implicitWidth: categoryLabel.implicitWidth + 16
                    implicitHeight: categoryLabel.implicitHeight + 8
                    color: "#152b36"
                    border.color: root.accentColor
                    radius: 4
                    Label {
                        id: categoryLabel
                        anchors.centerIn: parent
                        text: qsTr("SEPARATE FROM ANALOG SSTV")
                        color: root.accentColor
                        font.bold: true
                        font.pixelSize: 11
                    }
                }
                Item { Layout.fillWidth: true }
            }

            SectionPanel {
                implicitHeight: capabilityColumn.implicitHeight + 20

                ColumnLayout {
                    id: capabilityColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    Label {
                        text: qsTr("Capability boundary")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    Label {
                        objectName: "hamdrmCapabilityMessage"
                        Layout.fillWidth: true
                        text: root.controller
                              ? root.controller.capabilityMessage : ""
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Profiles/MOT/BSR: ready")
                            color: "#67d391"
                        }
                        Label {
                            text: qsTr("RX waveform: %1")
                                  .arg(root.capability.waveformRx
                                       ? qsTr("connected") : qsTr("not connected"))
                            color: root.capability.waveformRx
                                   ? "#67d391" : "#ffb454"
                        }
                        Label {
                            text: qsTr("TX waveform: %1")
                                  .arg(root.capability.waveformTx
                                       ? qsTr("connected") : qsTr("not connected"))
                            color: root.capability.waveformTx
                                   ? "#67d391" : "#ffb454"
                        }
                        Label {
                            text: qsTr("JPEG2000 D/E: %1/%2")
                                  .arg(root.capability.jpeg2000Decode
                                       ? qsTr("yes") : qsTr("no"))
                                  .arg(root.capability.jpeg2000Encode
                                       ? qsTr("yes") : qsTr("no"))
                            color: root.capability.jpeg2000Decode
                                   && root.capability.jpeg2000Encode
                                   ? "#67d391" : "#ffb454"
                        }
                        Label {
                            text: qsTr("Partial resume: %1")
                                  .arg(root.capability.partialResume
                                       ? qsTr("configured") : qsTr("not configured"))
                            color: root.capability.partialResume
                                   ? "#67d391" : "#ffb454"
                        }
                    }
                }
            }

            SectionPanel {
                implicitHeight: profileColumn.implicitHeight + 20

                ColumnLayout {
                    id: profileColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    Label {
                        text: qsTr("Named HAMDRM profile")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    ComboBox {
                        id: profileSelector
                        objectName: "hamdrmProfileSelector"
                        Layout.fillWidth: true
                        model: root.controller ? root.controller.profiles : []
                        textRole: "displayName"
                        valueRole: "id"
                        enabled: root.controller && !root.controller.busy
                        onActivated: {
                            if (root.controller && currentValue)
                                root.controller.selectedProfileId = currentValue
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 5
                        columnSpacing: 16
                        rowSpacing: 3

                        Label { text: qsTr("Robustness"); color: root.secondaryTextColor }
                        Label { text: qsTr("Bandwidth"); color: root.secondaryTextColor }
                        Label { text: qsTr("Protection"); color: root.secondaryTextColor }
                        Label { text: qsTr("Constellation"); color: root.secondaryTextColor }
                        Label { text: qsTr("Interleaver"); color: root.secondaryTextColor }
                        Label { text: root.profile.robustness || "—"; color: root.primaryTextColor; font.bold: true }
                        Label { text: root.profile.bandwidth || "—"; color: root.primaryTextColor; font.bold: true }
                        Label { text: root.profile.protection || "—"; color: root.primaryTextColor; font.bold: true }
                        Label { text: root.profile.constellation || "—"; color: root.primaryTextColor; font.bold: true }
                        Label { text: root.profile.interleaver || "—"; color: root.primaryTextColor; font.bold: true }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Stable ID: %1 · payload %2 bytes/400 ms · expected %3 bit/s")
                              .arg(root.profile.id || "—")
                              .arg(root.profile.payloadBytesPer400msFrame || 0)
                              .arg(root.profile.expectedPayloadBitrate || 0)
                        color: root.secondaryTextColor
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                SectionPanel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: rxColumn.implicitHeight + 20

                    ColumnLayout {
                        id: rxColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 7

                        Label {
                            text: qsTr("WAVEFORM RX")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: root.controller
                                  ? qsTr("State: %1").arg(root.controller.rxStateName)
                                  : qsTr("State: unavailable")
                            color: root.secondaryTextColor
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: root.controller ? root.controller.rxProgress : 0
                        }
                        RowLayout {
                            Button {
                                objectName: "hamdrmStartRx"
                                text: qsTr("Start RX")
                                enabled: root.controller && !root.controller.busy
                                onClicked: root.controller.startRx()
                            }
                            Button {
                                objectName: "hamdrmCancelRx"
                                text: qsTr("Cancel RX")
                                enabled: root.controller && root.controller.busy
                                onClicked: root.controller.cancelRx()
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                SectionPanel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: txColumn.implicitHeight + 20

                    ColumnLayout {
                        id: txColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 7

                        Label {
                            text: qsTr("WAVEFORM TX")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            text: root.controller
                                  ? qsTr("State: %1").arg(root.controller.txStateName)
                                  : qsTr("State: unavailable")
                            color: root.secondaryTextColor
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: root.controller ? root.controller.txProgress : 0
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.selectedTxImage.toString().length > 0
                                  ? root.selectedTxImage.toString()
                                  : qsTr("No encoded image selected")
                            color: root.secondaryTextColor
                            elide: Text.ElideMiddle
                        }
                        RowLayout {
                            Button {
                                objectName: "hamdrmChooseTxImage"
                                text: qsTr("Choose image")
                                enabled: root.controller && !root.controller.busy
                                onClicked: txImagePicker.open()
                            }
                            Button {
                                objectName: "hamdrmValidateTxImage"
                                text: qsTr("Validate")
                                enabled: root.controller
                                         && !root.controller.busy
                                         && root.selectedTxImage.toString().length > 0
                                onClicked: root.controller.validateTxImage(
                                               root.selectedTxImage)
                            }
                            Button {
                                objectName: "hamdrmStartTx"
                                text: qsTr("Start TX")
                                enabled: root.controller
                                         && !root.controller.busy
                                         && root.selectedTxImage.toString().length > 0
                                onClicked: root.controller.startTx(root.selectedTxImage)
                            }
                            Button {
                                objectName: "hamdrmCancelTx"
                                text: qsTr("Cancel TX")
                                enabled: root.controller && root.controller.busy
                                onClicked: root.controller.cancelTx()
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: root.controller
                                     && root.controller.lastImageValidation.valid === true
                            text: root.controller
                                  ? qsTr("Validated %1 · %2 × %3 · %4 bytes")
                                      .arg(root.controller.lastImageValidation.format || "")
                                      .arg(root.controller.lastImageValidation.width || 0)
                                      .arg(root.controller.lastImageValidation.height || 0)
                                      .arg(root.controller.lastImageValidation.bytes || 0)
                                  : ""
                            color: "#67d391"
                        }
                    }
                }
            }

            SectionPanel {
                implicitHeight: inboxColumn.implicitHeight + 20

                ColumnLayout {
                    id: inboxColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("MOT OBJECT / SEGMENT INBOX")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("%1 object(s)")
                                  .arg(root.controller
                                       ? root.controller.inbox.length : 0)
                            color: root.secondaryTextColor
                        }
                    }

                    ListView {
                        id: objectInbox
                        objectName: "hamdrmObjectInbox"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 190
                        clip: true
                        spacing: 5
                        model: root.controller ? root.controller.inbox : []

                        delegate: Rectangle {
                            id: objectRow
                            required property var modelData
                            width: ListView.view ? ListView.view.width : 720
                            height: objectDetails.implicitHeight + 14
                            color: root.controller
                                   && root.controller.selectedTransportId
                                      === objectRow.modelData.transportId
                                   ? "#132b36" : "#050b10"
                            border.color: root.borderColor
                            radius: 5

                            ColumnLayout {
                                id: objectDetails
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 7
                                spacing: 3

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: (objectRow.modelData.filename
                                               || objectRow.modelData.transportKey)
                                              + " · " + objectRow.modelData.state
                                        color: root.primaryTextColor
                                        font.bold: true
                                        elide: Text.ElideMiddle
                                    }
                                    Label {
                                        text: objectRow.modelData.transportKey
                                        color: root.accentColor
                                    }
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    value: objectRow.modelData.progress || 0
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Segments %1/%2 · bytes %3/%4 · missing %5 · persisted %6")
                                          .arg(objectRow.modelData.bodySegmentsReceived || 0)
                                          .arg(objectRow.modelData.totalBodySegments || 0)
                                          .arg(objectRow.modelData.bodyBytesReceived || 0)
                                          .arg(objectRow.modelData.expectedBodyBytes || 0)
                                          .arg(objectRow.modelData.missingCount || 0)
                                          .arg(objectRow.modelData.persisted ? qsTr("yes") : qsTr("no"))
                                    color: root.secondaryTextColor
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: (objectRow.modelData.error || "").length > 0
                                    text: objectRow.modelData.error || ""
                                    color: "#ff7474"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            TapHandler {
                                onTapped: root.controller.selectObject(
                                              objectRow.modelData.transportId)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Resume transport ID (hex)")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: resumeId
                            objectName: "hamdrmResumeTransportId"
                            Layout.preferredWidth: 110
                            placeholderText: qsTr("1234")
                            maximumLength: 4
                            validator: RegularExpressionValidator {
                                regularExpression: /^[0-9A-Fa-f]{1,4}$/
                            }
                        }
                        Button {
                            objectName: "hamdrmResumePartial"
                            text: qsTr("Resume partial")
                            enabled: root.controller
                                     && root.controller.partialResumeAvailable
                                     && resumeId.acceptableInput
                            onClicked: root.controller.resumePartial(
                                           parseInt(resumeId.text, 16))
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            objectName: "hamdrmBuildBsr"
                            text: qsTr("Build BSR")
                            enabled: root.controller && root.controller.canBuildBsr
                            onClicked: root.controller.buildBsr(
                                           root.controller.selectedTransportId,
                                           true)
                        }
                        Button {
                            objectName: "hamdrmDiscardObject"
                            text: qsTr("Discard object")
                            enabled: root.controller
                                     && root.controller.selectedTransportId >= 0
                            onClicked: root.controller.discardObject(
                                           root.controller.selectedTransportId)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.controller
                              && root.controller.selectedTransportId >= 0
                              ? qsTr("Missing segments: %1")
                                  .arg(root.controller.missingSegmentsText.length > 0
                                       ? root.controller.missingSegmentsText
                                       : qsTr("none or extent unknown"))
                              : qsTr("Select a partial object to inspect missing segments")
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    TextArea {
                        objectName: "hamdrmBsrText"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        readOnly: true
                        wrapMode: TextEdit.NoWrap
                        placeholderText: qsTr("Generated BSR request appears here")
                        text: root.controller ? root.controller.bsrText : ""
                    }
                }
            }

            Label {
                objectName: "hamdrmError"
                Layout.fillWidth: true
                visible: root.controller && root.controller.error.length > 0
                text: root.controller ? root.controller.error : ""
                color: "#ff7474"
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("HAMDRM is an independent digital object mode. This page never reports analog SSTV compatibility, successful RF transmission, or on-air interoperability without a connected backend and real evidence.")
                color: root.secondaryTextColor
                wrapMode: Text.WordWrap
            }
        }
    }
}
