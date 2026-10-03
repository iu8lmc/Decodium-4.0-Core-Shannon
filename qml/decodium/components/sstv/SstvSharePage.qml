pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

SstvPage {
    id: root

    required property var engine
    readonly property var sharing: root.engine ? root.engine.sstvShare : null
    property bool configurationExpanded: false
    property string noticeText: ""
    property bool noticeError: false
    property url selectedUpload: ""
    property string remoteRemovalTransferId: ""
    property string remoteRemovalProviderId: ""
    property string remoteRemovalFileName: ""
    property string remoteRemovalAction: "unavailable"
    property string pendingSaveTransferId: ""
    property string pendingLocalDeleteTransferId: ""
    property string pendingProviderDeleteIncomingId: ""
    property string pendingProviderDeleteFileName: ""
    property string pendingBlockIncomingId: ""
    property string pendingBlockSenderId: ""
    property string pendingBlockScope: "unavailable"

    function showNotice(message, isError) {
        root.noticeText = message
        root.noticeError = isError
        noticeTimer.restart()
    }

    function formatUtc(value) {
        if (!value)
            return qsTr("unknown")
        const date = new Date(value)
        return isNaN(date.getTime()) ? qsTr("unknown")
                                      : date.toLocaleString(Qt.locale(),
                                                            Locale.ShortFormat)
    }

    function formatBytes(value) {
        const bytes = Number(value || 0)
        if (bytes < 1024)
            return qsTr("%1 B").arg(bytes)
        if (bytes < 1024 * 1024)
            return qsTr("%1 KiB").arg((bytes / 1024).toFixed(1))
        return qsTr("%1 MiB").arg((bytes / (1024 * 1024)).toFixed(1))
    }

    function directionLabel(direction) {
        switch (direction) {
        case "Upload": return qsTr("Upload", "transfer direction")
        case "Download": return qsTr("Download", "transfer direction")
        case "Incoming": return qsTr("Incoming")
        default: return qsTr("Unknown")
        }
    }

    function transferStateLabel(state) {
        switch (state) {
        case "Queued": return qsTr("Queued")
        case "Preparing": return qsTr("Preparing")
        case "Uploading": return qsTr("Uploading")
        case "WaitingForAcknowledgement":
            return qsTr("Waiting for acknowledgement")
        case "DownloadQueued": return qsTr("Download queued")
        case "Downloading": return qsTr("Downloading")
        case "AwaitingAcceptance": return qsTr("Awaiting acceptance")
        case "Accepted": return qsTr("Accepted")
        case "Acknowledging": return qsTr("Acknowledging")
        case "Rejecting": return qsTr("Rejecting")
        case "RetryScheduled": return qsTr("Retry scheduled")
        case "Paused": return qsTr("Paused")
        case "Completed": return qsTr("Completed")
        case "RemoteDeleted": return qsTr("Remote copy deleted")
        case "RemoteRevoked": return qsTr("Remote upload revoked")
        case "Acknowledged": return qsTr("Acknowledged")
        case "Cancelled": return qsTr("Cancelled")
        case "Rejected": return qsTr("Rejected")
        case "Expired": return qsTr("Expired")
        case "Failed": return qsTr("Failed")
        case "New": return qsTr("New")
        case "BlockedLocally": return qsTr("Blocked locally")
        case "BlockedByProvider": return qsTr("Blocked by provider")
        case "ProviderDeleted": return qsTr("Deleted at provider")
        case "CancelPending": return qsTr("Cancellation pending")
        default: return qsTr("Unknown")
        }
    }

    function loadConfiguration() {
        if (!root.sharing)
            return
        const config = root.sharing.configuration || ({})
        providerType.currentIndex = config.type === "webdav" ? 1 : 0
        providerId.text = config.providerId || ""
        endpoint.text = config.endpoint || ""
        restCreate.text = config.createPath || ""
        restChunk.text = config.chunkPath || ""
        restStatus.text = config.statusPath || ""
        restComplete.text = config.completePath || ""
        restCancel.text = config.cancelPath || ""
        authRequired.checked = config.credentialsRequired === undefined
                               ? true : config.credentialsRequired
        authType.currentIndex = config.authType === "basic" ? 1 : 0
        username.text = config.username || ""
    }

    function confirmRemoteRemoval(transferId, providerId, fileName, action) {
        root.remoteRemovalTransferId = transferId
        root.remoteRemovalProviderId = providerId
        root.remoteRemovalFileName = fileName
        root.remoteRemovalAction = action
        remoteRemovalAcknowledgement.checked = false
        remoteRemovalDialog.open()
    }

    Component.onCompleted: loadConfiguration()

    Timer {
        id: noticeTimer
        interval: 7000
        onTriggered: root.noticeText = ""
    }

    Connections {
        target: root.sharing
        enabled: root.sharing !== null

        function onConfigurationChanged() {
            root.loadConfiguration()
        }

        function onOperationFinished(operation, ok, message) {
            root.showNotice(message, !ok)
        }
    }

    FileDialog {
        id: uploadPicker
        title: qsTr("Select an SSTV gallery image")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("SSTV images (*.png *.jpg *.jpeg)")]
        currentFolder: root.sharing && root.sharing.storageFolder.length > 0
                       && typeof root.sharing.localFileUrl === "function"
                       ? root.sharing.localFileUrl(root.sharing.storageFolder)
                       : ""
        onAccepted: root.selectedUpload = selectedFile
    }

    FileDialog {
        id: saveAsPicker
        objectName: "sstvShareSaveAsPicker"
        title: qsTr("Save validated SSTV image as")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            if (!root.sharing || root.pendingSaveTransferId.length === 0)
                return
            const queued = root.sharing.saveAs(root.pendingSaveTransferId,
                                               selectedFile)
            root.pendingSaveTransferId = ""
            if (!queued)
                root.showNotice(qsTr("Save As could not be queued."), true)
        }
        onRejected: root.pendingSaveTransferId = ""
    }

    Dialog {
        id: localDeleteDialog
        objectName: "sstvShareLocalDeleteDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete local sharing copy?")
        standardButtons: Dialog.Cancel | Dialog.Ok

        Label {
            width: Math.min(520, root.width - 80)
            text: qsTr("This removes only Decodium's private validated sharing copy. It does not delete an image already imported into the native Gallery and does not contact the provider.")
            wrapMode: Text.WordWrap
            color: root.primaryTextColor
        }
        onAccepted: {
            if (root.sharing && root.pendingLocalDeleteTransferId.length > 0
                    && !root.sharing.deleteLocalCopy(
                        root.pendingLocalDeleteTransferId))
                root.showNotice(qsTr("Local-copy deletion could not be queued."),
                                true)
            root.pendingLocalDeleteTransferId = ""
        }
        onRejected: root.pendingLocalDeleteTransferId = ""
    }

    Dialog {
        id: providerIncomingDeleteDialog
        objectName: "sstvShareProviderIncomingDeleteDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Request provider deletion?")
        standardButtons: Dialog.Cancel | Dialog.Ok

        Label {
            width: Math.min(520, root.width - 80)
            text: qsTr("This asks the configured provider to delete its incoming copy of %1. Local Decodium and Gallery files are not deleted.")
                  .arg(root.pendingProviderDeleteFileName)
            wrapMode: Text.WordWrap
            color: root.primaryTextColor
        }
        onAccepted: {
            if (root.sharing
                    && root.pendingProviderDeleteIncomingId.length > 0
                    && !root.sharing.requestProviderDeletion(
                        root.pendingProviderDeleteIncomingId))
                root.showNotice(qsTr("Provider deletion could not be queued."),
                                true)
            root.pendingProviderDeleteIncomingId = ""
            root.pendingProviderDeleteFileName = ""
        }
        onRejected: {
            root.pendingProviderDeleteIncomingId = ""
            root.pendingProviderDeleteFileName = ""
        }
    }

    Dialog {
        id: blockSenderDialog
        objectName: "sstvShareBlockSenderDialog"
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Block sender %1?").arg(root.pendingBlockSenderId)

        ColumnLayout {
            width: Math.min(560, root.width - 80)
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: root.pendingBlockScope === "provider-or-local"
                      ? qsTr("Provider blocking is verified for this endpoint. Choose whether to request a provider-wide block or store a local-only block in this Decodium profile.")
                      : qsTr("This provider has no verified sender-blocking capability. Only the clearly separate local Decodium block is available.")
                wrapMode: Text.WordWrap
                color: root.primaryTextColor
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    onClicked: blockSenderDialog.close()
                }
                Button {
                    objectName: "sstvShareBlockSenderLocal"
                    text: qsTr("Block locally only")
                    enabled: root.sharing
                             && root.pendingBlockIncomingId.length > 0
                    onClicked: {
                        const queued = root.sharing.blockSender(
                            root.pendingBlockIncomingId, true)
                        blockSenderDialog.close()
                        if (!queued)
                            root.showNotice(qsTr("Local sender block could not be queued."), true)
                    }
                }
                Button {
                    objectName: "sstvShareBlockSenderProvider"
                    visible: root.pendingBlockScope === "provider-or-local"
                    text: qsTr("Block at provider")
                    highlighted: true
                    enabled: root.sharing && root.sharing.enabled
                             && root.sharing.providerSupportsSenderBlocking
                             && root.pendingBlockIncomingId.length > 0
                    onClicked: {
                        const queued = root.sharing.blockSender(
                            root.pendingBlockIncomingId, false)
                        blockSenderDialog.close()
                        if (!queued)
                            root.showNotice(qsTr("Provider sender block could not be queued."), true)
                    }
                }
            }
        }
        onClosed: {
            root.pendingBlockIncomingId = ""
            root.pendingBlockSenderId = ""
            root.pendingBlockScope = "unavailable"
        }
    }

    Dialog {
        id: remoteRemovalDialog
        objectName: "sstvShareRemoteRemovalDialog"
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        title: root.remoteRemovalAction === "delete"
               ? qsTr("Delete provider copy?")
               : qsTr("Revoke provider upload?")

        ColumnLayout {
            width: Math.min(560, root.width - 80)
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: root.remoteRemovalAction === "delete"
                      ? qsTr("This asks provider %1 to permanently delete its remote copy of %2.")
                        .arg(root.remoteRemovalProviderId)
                        .arg(root.remoteRemovalFileName)
                      : qsTr("This asks provider %1 to revoke the completed upload of %2 using its documented transfer endpoint.")
                        .arg(root.remoteRemovalProviderId)
                        .arg(root.remoteRemovalFileName)
                wrapMode: Text.WordWrap
                color: root.primaryTextColor
            }
            Label {
                objectName: "sstvShareRemoteRemovalScopeWarning"
                Layout.fillWidth: true
                text: qsTr("The image in Decodium's local Gallery is not deleted. This action uses only the configured IP provider; it never keys PTT and never starts RF/TX.")
                wrapMode: Text.WordWrap
                color: "#f2c14e"
                font.bold: true
            }
            CheckBox {
                id: remoteRemovalAcknowledgement
                objectName: "sstvShareRemoteRemovalAcknowledgement"
                text: qsTr("I understand that only the provider's remote copy is affected")
                palette.text: root.primaryTextColor
                palette.buttonText: root.primaryTextColor
                palette.windowText: root.primaryTextColor
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    onClicked: remoteRemovalDialog.close()
                }
                Button {
                    objectName: "sstvShareConfirmRemoteRemoval"
                    text: root.remoteRemovalAction === "delete"
                          ? qsTr("Delete provider copy")
                          : qsTr("Revoke provider upload")
                    highlighted: true
                    enabled: remoteRemovalAcknowledgement.checked
                             && root.sharing
                             && root.sharing.enabled
                             && root.remoteRemovalTransferId.length > 0
                    onClicked: {
                        const queued = root.sharing.removeRemoteCopy(
                            root.remoteRemovalTransferId)
                        remoteRemovalDialog.close()
                        if (!queued)
                            root.showNotice(qsTr("Remote copy removal could not be queued."), true)
                    }
                }
            }
        }
    }

    component TransferDelegate: Rectangle {
        id: transferRow
        required property string transferId
        required property string incomingId
        required property string direction
        required property string transferState
        required property string providerId
        required property string peerId
        required property string fileName
        required property string mode
        required property var byteSize
        required property double progress
        required property string error
        required property var expiresUtc
        required property bool canPause
        required property bool canResume
        required property bool canCancel
        required property bool canDownload
        required property bool canAccept
        required property bool canAcknowledge
        required property bool canReject
        required property bool canSaveAs
        required property bool canDeleteLocalCopy
        required property bool canRequestProviderDeletion
        required property bool canBlockSender
        required property string blockSenderScope
        required property string message
        required property string sha256
        required property var privacySummary
        required property bool canRemoveRemoteCopy
        required property string remoteCopyAction
        required property url previewSource
        required property var validatedHandoff
        property var controller: null

        width: ListView.view ? ListView.view.width : 600
        height: details.implicitHeight + 18
        color: "#081017"
        border.color: root.borderColor
        border.width: 1
        radius: 6

        ColumnLayout {
            id: details
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 9
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: (transferRow.fileName.length > 0
                           ? transferRow.fileName : transferRow.transferId)
                          + "  ·  "
                          + root.transferStateLabel(transferRow.transferState)
                    color: root.primaryTextColor
                    font.bold: true
                    elide: Text.ElideMiddle
                }
                Label {
                    text: root.directionLabel(transferRow.direction)
                    color: root.accentColor
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Provider %1 · peer %2 · %3")
                      .arg(transferRow.providerId)
                      .arg(transferRow.peerId)
                      .arg(transferRow.mode)
                color: root.secondaryTextColor
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Size %1 · expires %2")
                      .arg(root.formatBytes(transferRow.byteSize))
                      .arg(root.formatUtc(transferRow.expiresUtc))
                color: root.secondaryTextColor
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: transferRow.sha256.length > 0
                text: qsTr("Payload SHA-256: %1").arg(transferRow.sha256)
                color: root.secondaryTextColor
                elide: Text.ElideMiddle
            }
            Label {
                Layout.fillWidth: true
                visible: transferRow.message.length > 0
                text: qsTr("Message: %1").arg(transferRow.message)
                color: root.primaryTextColor
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: transferRow.privacySummary
                         && Object.keys(transferRow.privacySummary).length > 0
                text: {
                    const privacy = transferRow.privacySummary || ({})
                    const protection = privacy.endToEndEncrypted
                                       ? qsTr("end-to-end encrypted")
                                       : (privacy.providerCanReadContent
                                          ? qsTr("TLS transport; provider can read content")
                                          : qsTr("TLS transport"))
                    return qsTr("Privacy: recipient-only · %1 · callsign %2 · grid %3 · metered %4")
                        .arg(protection)
                        .arg(privacy.callsignIncluded ? qsTr("included")
                                                      : qsTr("omitted"))
                        .arg(privacy.gridIncluded ? qsTr("included")
                                                  : qsTr("omitted"))
                        .arg(privacy.meteredNetworkAllowed ? qsTr("allowed")
                                                           : qsTr("blocked"))
                }
                color: "#f2c14e"
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                visible: transferRow.previewSource.toString().length > 0
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 120
                    color: "#03070a"
                    border.color: root.borderColor
                    border.width: 1
                    radius: 4

                    Image {
                        id: incomingPreview
                        objectName: "sstvShareIncomingPreview"
                        anchors.fill: parent
                        anchors.margins: 3
                        source: transferRow.previewSource
                        sourceSize.width: 320
                        sourceSize.height: 240
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Validated private preview · %1 × %2")
                              .arg(transferRow.validatedHandoff.width || 0)
                              .arg(transferRow.validatedHandoff.height || 0)
                        color: "#67d391"
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Source SHA-256: %1")
                              .arg(transferRow.validatedHandoff.sourceSha256 || "")
                        color: root.secondaryTextColor
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Preview is a metadata-free PNG. Accept validates and imports it into the native Gallery; it never transmits it. Provider acknowledgement remains explicit.")
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: transferRow.direction !== "Incoming"
                from: 0
                to: 1
                value: transferRow.progress
            }
            Label {
                Layout.fillWidth: true
                visible: transferRow.error.length > 0
                text: transferRow.error
                color: "#ff7474"
                wrapMode: Text.WordWrap
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                visible: transferRow.canPause || transferRow.canResume
                         || transferRow.canCancel || transferRow.canDownload
                         || transferRow.canAccept
                         || transferRow.canAcknowledge || transferRow.canReject
                         || transferRow.canSaveAs
                         || transferRow.canDeleteLocalCopy
                         || transferRow.canRequestProviderDeletion
                         || transferRow.canBlockSender
                         || transferRow.canRemoveRemoteCopy
                Button {
                    visible: transferRow.canDownload
                    text: qsTr("Download")
                    onClicked: transferRow.controller.download(
                                   transferRow.incomingId,
                                   transferRow.fileName)
                }
                Button {
                    objectName: "sstvSharePauseTransfer"
                    visible: transferRow.canPause
                    text: qsTr("Pause")
                    onClicked: transferRow.controller.pause(
                                   transferRow.transferId)
                }
                Button {
                    objectName: "sstvShareResumeTransfer"
                    visible: transferRow.canResume
                    text: qsTr("Resume")
                    onClicked: transferRow.controller.resume(
                                   transferRow.transferId)
                }
                Button {
                    visible: transferRow.canCancel
                    text: qsTr("Cancel")
                    onClicked: transferRow.controller.cancel(
                                   transferRow.transferId)
                }
                Button {
                    visible: transferRow.canAccept
                    text: transferRow.transferState === "Accepted"
                          ? qsTr("Import again")
                          : qsTr("Accept and import")
                    onClicked: transferRow.controller.accept(
                                   transferRow.transferId)
                }
                Button {
                    visible: transferRow.canAcknowledge
                    text: qsTr("Acknowledge")
                    onClicked: transferRow.controller.acknowledge(
                                   transferRow.transferId)
                }
                Button {
                    visible: transferRow.canReject
                    text: qsTr("Reject")
                    onClicked: transferRow.controller.reject(
                                   transferRow.incomingId)
                }
                Button {
                    objectName: "sstvShareSaveAs"
                    visible: transferRow.canSaveAs
                    text: qsTr("Save As…")
                    onClicked: {
                        root.pendingSaveTransferId = transferRow.transferId
                        saveAsPicker.open()
                    }
                }
                Button {
                    objectName: "sstvShareDeleteLocalCopy"
                    visible: transferRow.canDeleteLocalCopy
                    text: qsTr("Delete local copy")
                    onClicked: {
                        root.pendingLocalDeleteTransferId
                            = transferRow.transferId
                        localDeleteDialog.open()
                    }
                }
                Button {
                    objectName: "sstvShareDeleteProviderIncoming"
                    visible: transferRow.canRequestProviderDeletion
                    text: qsTr("Delete at provider")
                    onClicked: {
                        root.pendingProviderDeleteIncomingId
                            = transferRow.incomingId
                        root.pendingProviderDeleteFileName
                            = transferRow.fileName
                        providerIncomingDeleteDialog.open()
                    }
                }
                Button {
                    objectName: "sstvShareBlockSender"
                    visible: transferRow.canBlockSender
                    text: transferRow.blockSenderScope === "provider-or-local"
                          ? qsTr("Block sender…")
                          : qsTr("Block locally…")
                    onClicked: {
                        root.pendingBlockIncomingId = transferRow.incomingId
                        root.pendingBlockSenderId = transferRow.peerId
                        root.pendingBlockScope = transferRow.blockSenderScope
                        blockSenderDialog.open()
                    }
                }
                Button {
                    objectName: "sstvShareRemoveRemoteCopy"
                    visible: transferRow.canRemoveRemoteCopy
                    text: transferRow.remoteCopyAction === "delete"
                          ? qsTr("Delete provider copy")
                          : qsTr("Revoke provider upload")
                    onClicked: root.confirmRemoteRemoval(
                                   transferRow.transferId,
                                   transferRow.providerId,
                                   transferRow.fileName,
                                   transferRow.remoteCopyAction)
                }
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: Math.max(760, root.width - 8)
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("REMOTE SHARING")
                    color: root.accentColor
                    font.pixelSize: 15
                    font.bold: true
                    font.letterSpacing: 1.2
                }
                Label {
                    Layout.fillWidth: true
                    text: root.sharing ? root.sharing.statusText
                                       : qsTr("Sharing unavailable")
                    color: root.secondaryTextColor
                    elide: Text.ElideRight
                }
                BusyIndicator {
                    running: root.sharing ? root.sharing.busy : false
                    visible: running
                    implicitWidth: 26
                    implicitHeight: 26
                }
                Switch {
                    id: sharingEnabled
                    objectName: "sstvSharingOptIn"
                    text: checked ? qsTr("Enabled") : qsTr("Off")
                    checked: root.sharing ? root.sharing.enabled : false
                    enabled: root.sharing && root.sharing.ready
                    palette.text: root.primaryTextColor
                    palette.buttonText: root.primaryTextColor
                    palette.windowText: root.primaryTextColor
                    onClicked: root.sharing.setEnabled(checked)
                    Accessible.name: qsTr("Enable remote SSTV sharing")
                }
                Button {
                    text: root.configurationExpanded
                          ? qsTr("Hide provider") : qsTr("Configure provider")
                    onClicked: root.configurationExpanded
                               = !root.configurationExpanded
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Privacy default: OFF. Uploads require a confirmed recipient; downloads require explicit acceptance. No automatic or public sharing is enabled.")
                color: sharingEnabled.checked ? "#f2c14e" : root.secondaryTextColor
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: providerLayout.implicitHeight + 20
                visible: root.configurationExpanded
                color: "#081017"
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: providerLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Only user-supplied HTTPS REST or WebDAV endpoints are used. %1")
                              .arg(root.sharing
                                   ? root.sharing.preSignedUnavailableReason
                                   : qsTr("Pre-signed upload is unavailable."))
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.sharing && root.sharing.enabled
                        text: root.sharing
                              ? qsTr("Verified provider capabilities: inbox %1 · incoming deletion %2 · sender blocking %3")
                                .arg(root.sharing.providerSupportsInbox
                                     ? qsTr("yes") : qsTr("no"))
                                .arg(root.sharing.providerSupportsIncomingDelete
                                     ? qsTr("yes") : qsTr("no"))
                                .arg(root.sharing.providerSupportsSenderBlocking
                                     ? qsTr("yes") : qsTr("no"))
                              : ""
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 8
                        rowSpacing: 6

                        Label { text: qsTr("Provider"); color: root.secondaryTextColor }
                        ComboBox {
                            id: providerType
                            objectName: "sstvShareProviderType"
                            Layout.fillWidth: true
                            model: [
                                qsTr("HTTPS REST"),
                                qsTr("HTTPS WebDAV"),
                                qsTr("Pre-signed PUT (trusted broker required)"),
                                qsTr("Peer/relay (backend unavailable)")
                            ]
                        }
                        Label {
                            visible: providerType.currentIndex < 2
                            text: qsTr("Identifier")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: providerId
                            objectName: "sstvShareProviderId"
                            visible: providerType.currentIndex < 2
                            Layout.fillWidth: true
                            placeholderText: qsTr("Provider identifier")
                            maximumLength: 128
                        }
                        Label {
                            visible: providerType.currentIndex < 2
                            text: qsTr("HTTPS endpoint")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: endpoint
                            visible: providerType.currentIndex < 2
                            objectName: "sstvShareEndpoint"
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            placeholderText: providerType.currentIndex === 0
                                             ? qsTr("REST base URL supplied by your provider")
                                             : qsTr("WebDAV collection URL supplied by your provider")
                            maximumLength: 2048
                        }
                        Label {
                            visible: providerType.currentIndex === 0
                            text: qsTr("Create path")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: restCreate
                            visible: providerType.currentIndex === 0
                            Layout.fillWidth: true
                            placeholderText: qsTr("Required absolute path")
                            maximumLength: 512
                        }
                        Label {
                            visible: providerType.currentIndex === 0
                            text: qsTr("Chunk path")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: restChunk
                            visible: providerType.currentIndex === 0
                            Layout.fillWidth: true
                            placeholderText: qsTr("Required path with {uploadId}")
                            maximumLength: 512
                        }
                        Label {
                            visible: providerType.currentIndex === 0
                            text: qsTr("Status path")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: restStatus
                            visible: providerType.currentIndex === 0
                            Layout.fillWidth: true
                            placeholderText: qsTr("Required path with {uploadId}")
                            maximumLength: 512
                        }
                        Label {
                            visible: providerType.currentIndex === 0
                            text: qsTr("Complete path")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: restComplete
                            visible: providerType.currentIndex === 0
                            Layout.fillWidth: true
                            placeholderText: qsTr("Required path with {uploadId}")
                            maximumLength: 512
                        }
                        Label {
                            visible: providerType.currentIndex === 0
                            text: qsTr("Cancel path")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: restCancel
                            visible: providerType.currentIndex === 0
                            Layout.fillWidth: true
                            placeholderText: qsTr("Required path with {uploadId}")
                            maximumLength: 512
                        }
                        CheckBox {
                            id: authRequired
                            visible: providerType.currentIndex < 2
                            Layout.columnSpan: providerType.currentIndex === 0 ? 2 : 1
                            text: qsTr("Provider requires authentication")
                            checked: true
                            palette.text: root.primaryTextColor
                            palette.buttonText: root.primaryTextColor
                            palette.windowText: root.primaryTextColor
                        }
                        ComboBox {
                            id: authType
                            visible: providerType.currentIndex < 2
                                     && authRequired.checked
                            Layout.fillWidth: true
                            model: [qsTr("Bearer token"), qsTr("Basic authentication")]
                        }
                        TextField {
                            id: username
                            visible: providerType.currentIndex < 2
                                     && authRequired.checked
                                     && authType.currentIndex === 1
                            Layout.fillWidth: true
                            placeholderText: qsTr("Username")
                            maximumLength: 256
                        }
                        Label {
                            visible: providerType.currentIndex < 2
                                     && authRequired.checked
                            text: qsTr("Credential")
                            color: root.secondaryTextColor
                        }
                        TextField {
                            id: credential
                            objectName: "sstvShareCredential"
                            visible: providerType.currentIndex < 2
                                     && authRequired.checked
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            placeholderText: root.sharing
                                             && root.sharing.configuration.credentialStored
                                             ? qsTr("Stored securely; leave blank to keep it")
                                             : qsTr("Stored only in the OS secure store")
                            echoMode: TextInput.Password
                            maximumLength: 4096
                        }
                    }
                    Label {
                        objectName: "sstvShareUnavailableProviderReason"
                        Layout.fillWidth: true
                        visible: providerType.currentIndex >= 2
                        text: providerType.currentIndex === 2
                              ? (root.sharing
                                 ? root.sharing.preSignedUnavailableReason
                                 : qsTr("Pre-signed PUT requires a trusted target broker."))
                              : (root.sharing
                                 ? root.sharing.peerRelayUnavailableReason
                                 : qsTr("Peer/relay requires an authenticated backend."))
                        color: "#f2c14e"
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: providerType.currentIndex < 2
                                 && authRequired.checked && root.sharing
                                 && !root.sharing.secureStorageAvailable
                        text: qsTr("The OS secure credential store is unavailable. Decodium will not save this secret in ordinary settings.")
                        color: "#ff7474"
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: providerType.currentIndex < 2
                        Label {
                            Layout.fillWidth: true
                            text: root.sharing
                                  && root.sharing.configuration.credentialStored
                                  ? qsTr("A credential is stored in the OS secure store.")
                                  : qsTr("No credential is stored for this configuration.")
                            color: root.secondaryTextColor
                        }
                        Button {
                            text: qsTr("Remove credential")
                            enabled: root.sharing
                                     && root.sharing.configuration.credentialStored
                            onClicked: root.sharing.clearCredentials()
                        }
                        Button {
                            objectName: "sstvShareSaveProvider"
                            text: qsTr("Save provider")
                            highlighted: true
                            enabled: root.sharing && root.sharing.ready
                                     && providerType.currentIndex < 2
                            onClicked: {
                                const queued = root.sharing.configureProvider({
                                    "type": providerType.currentIndex === 0
                                            ? "rest" : "webdav",
                                    "providerId": providerId.text,
                                    "endpoint": endpoint.text,
                                    "createPath": restCreate.text,
                                    "chunkPath": restChunk.text,
                                    "statusPath": restStatus.text,
                                    "completePath": restComplete.text,
                                    "cancelPath": restCancel.text,
                                    "credentialsRequired": authRequired.checked,
                                    "authType": authType.currentIndex === 0
                                                ? "bearer" : "basic",
                                    "username": username.text,
                                    "secret": credential.text
                                })
                                credential.clear()
                                if (!queued)
                                    root.showNotice(qsTr("Provider configuration could not be queued."), true)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: uploadLayout.implicitHeight + 20
                color: "#081017"
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: uploadLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    Label {
                        text: qsTr("QUEUE PRIVATE UPLOAD")
                        color: root.primaryTextColor
                        font.bold: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            Layout.fillWidth: true
                            readOnly: true
                            text: root.selectedUpload.toString()
                            placeholderText: qsTr("Select a PNG/JPEG already inside Decodium SSTV storage")
                        }
                        Button {
                            text: qsTr("Choose image")
                            onClicked: uploadPicker.open()
                        }
                        TextField {
                            id: recipient
                            objectName: "sstvShareRecipient"
                            Layout.preferredWidth: 180
                            placeholderText: qsTr("Recipient ID")
                            maximumLength: 128
                        }
                        TextField {
                            id: uploadMode
                            Layout.preferredWidth: 140
                            text: qsTr("Martin M1")
                            placeholderText: qsTr("SSTV mode")
                            maximumLength: 64
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 8
                        rowSpacing: 6

                        Label {
                            text: qsTr("Expiration")
                            color: root.secondaryTextColor
                        }
                        ComboBox {
                            id: uploadExpiry
                            objectName: "sstvShareUploadExpiry"
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "hours"
                            currentIndex: 1
                            model: [
                                {"label": qsTr("24 hours"), "hours": 24},
                                {"label": qsTr("7 days"), "hours": 168},
                                {"label": qsTr("30 days"), "hours": 720}
                            ]
                        }
                        CheckBox {
                            id: includeCallsign
                            objectName: "sstvShareIncludeCallsign"
                            text: qsTr("Include callsign")
                            palette.text: root.primaryTextColor
                            palette.buttonText: root.primaryTextColor
                            palette.windowText: root.primaryTextColor
                        }
                        TextField {
                            id: uploadCallsign
                            objectName: "sstvShareCallsign"
                            Layout.fillWidth: true
                            enabled: includeCallsign.checked
                            placeholderText: qsTr("Station callsign")
                            maximumLength: 24
                        }
                        CheckBox {
                            id: includeGrid
                            objectName: "sstvShareIncludeGrid"
                            text: qsTr("Include Maidenhead grid/location")
                            palette.text: root.primaryTextColor
                            palette.buttonText: root.primaryTextColor
                            palette.windowText: root.primaryTextColor
                        }
                        TextField {
                            id: uploadGrid
                            objectName: "sstvShareGrid"
                            Layout.fillWidth: true
                            enabled: includeGrid.checked
                            placeholderText: qsTr("Grid, for example JM75FV")
                            maximumLength: 8
                        }
                        CheckBox {
                            id: meteredUpload
                            objectName: "sstvShareAllowMetered"
                            Layout.columnSpan: 2
                            text: qsTr("Allow this upload on a metered network")
                            checked: false
                            palette.text: root.primaryTextColor
                            palette.buttonText: root.primaryTextColor
                            palette.windowText: root.primaryTextColor
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Private recipient-only transfer. Public and automatic sharing stay off. EXIF is removed. Protection is TLS transport; this provider can read content because optional E2EE is not enabled.")
                        color: "#f2c14e"
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.sharing
                                 && !meteredUpload.checked
                                 && root.sharing.meteredNetworkStatus
                                    !== "unmetered"
                        text: root.sharing
                              && root.sharing.meteredNetworkStatus === "metered"
                              ? qsTr("The current network is metered. This upload will remain durably queued unless you explicitly allow metered transfer.")
                              : qsTr("The platform cannot determine whether the network is metered. This upload will remain durably queued unless you explicitly allow metered transfer.")
                        color: "#f2c14e"
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: uploadMessage
                            Layout.fillWidth: true
                            placeholderText: qsTr("Optional private message (URLs are not accepted)")
                            maximumLength: 1000
                        }
                        CheckBox {
                            id: recipientConfirmed
                            objectName: "sstvShareRecipientConfirmed"
                            text: qsTr("I confirmed this recipient")
                            palette.text: root.primaryTextColor
                            palette.buttonText: root.primaryTextColor
                            palette.windowText: root.primaryTextColor
                        }
                        Button {
                            objectName: "sstvShareUpload"
                            text: qsTr("Queue upload")
                            highlighted: true
                            enabled: root.sharing && root.sharing.enabled
                                     && root.selectedUpload.toString().length > 0
                                     && recipientConfirmed.checked
                                     && (!includeCallsign.checked
                                         || uploadCallsign.text.trim().length > 0)
                                     && (!includeGrid.checked
                                         || uploadGrid.text.trim().length > 0)
                            onClicked: {
                                const queued = root.sharing.uploadWithOptions(
                                    root.selectedUpload, recipient.text,
                                    uploadMode.text, uploadMessage.text,
                                    recipientConfirmed.checked, {
                                        "expiryHours": uploadExpiry.currentValue,
                                        "includeCallsign": includeCallsign.checked,
                                        "callsign": uploadCallsign.text,
                                        "includeGrid": includeGrid.checked,
                                        "grid": uploadGrid.text,
                                        "meteredNetworkAllowed": meteredUpload.checked
                                    })
                                if (!queued)
                                    root.showNotice(qsTr("Upload could not be queued."), true)
                            }
                        }
                    }
                }
            }

            Rectangle {
                objectName: "sstvShareDiagnostics"
                Layout.fillWidth: true
                Layout.preferredHeight: diagnosticsLayout.implicitHeight + 20
                color: "#081017"
                border.color: root.borderColor
                border.width: 1
                radius: 7

                ColumnLayout {
                    id: diagnosticsLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("SHARING DIAGNOSTICS")
                            color: root.primaryTextColor
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.sharing
                                  ? qsTr("Network: %1")
                                    .arg(root.sharing.meteredNetworkStatus)
                                  : qsTr("Network: unknown")
                            color: root.secondaryTextColor
                        }
                        Button {
                            objectName: "sstvShareResetDiagnostics"
                            text: qsTr("Reset counters")
                            enabled: root.sharing && root.sharing.ready
                            onClicked: root.sharing.resetDiagnostics()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        property var values: root.sharing
                                             ? root.sharing.diagnostics : ({})
                        text: qsTr("Uploaded %1 at %2/s · downloaded %3 at %4/s")
                              .arg(root.formatBytes(values.uploadedBytes))
                              .arg(root.formatBytes(values.uploadBytesPerSecond))
                              .arg(root.formatBytes(values.downloadedBytes))
                              .arg(root.formatBytes(values.downloadBytesPerSecond))
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        property var values: root.sharing
                                             ? root.sharing.diagnostics : ({})
                        text: qsTr("Queue depth %1 · uploads %2 · downloads %3 · reset %4")
                              .arg(values.activeQueueDepth || 0)
                              .arg(values.uploadQueueDepth || 0)
                              .arg(values.downloadQueueDepth || 0)
                              .arg(root.formatUtc(values.resetUtc))
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Diagnostics contain only bounded counters and UTC reset time; provider URLs, credentials, tokens and local file paths are never exposed here.")
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                TabBar {
                    id: queueTabs
                    Layout.fillWidth: true
                    TabButton { text: qsTr("Active queue") }
                    TabButton { text: qsTr("Incoming inbox") }
                    TabButton { text: qsTr("History") }
                }
                Button {
                    text: qsTr("Refresh")
                    enabled: root.sharing && root.sharing.ready
                    onClicked: root.sharing.refresh()
                }
                Button {
                    text: qsTr("Check inbox")
                    enabled: root.sharing && root.sharing.enabled
                             && root.sharing.configured
                    onClicked: root.sharing.refreshInbox()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: queueTabs.currentIndex === 1 && root.sharing
                         && !root.sharing.providerSupportsInbox
                text: qsTr("Inbox operations stay fail-closed until the configured REST endpoint returns a valid authenticated capability document. WebDAV supports bounded direct GET only; it does not invent an inbox listing contract.")
                color: root.secondaryTextColor
                wrapMode: Text.WordWrap
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 310
                currentIndex: queueTabs.currentIndex

                ListView {
                    id: activeList
                    clip: true
                    spacing: 7
                    model: root.sharing ? root.sharing.activeTransfers : null
                    delegate: TransferDelegate { controller: root.sharing }
                    ScrollBar.vertical: ScrollBar {}
                    Label {
                        anchors.centerIn: parent
                        visible: activeList.count === 0
                        text: qsTr("No active transfers")
                        color: root.secondaryTextColor
                    }
                }
                ListView {
                    id: inboxList
                    clip: true
                    spacing: 7
                    model: root.sharing ? root.sharing.inbox : null
                    delegate: TransferDelegate { controller: root.sharing }
                    ScrollBar.vertical: ScrollBar {}
                    Label {
                        anchors.centerIn: parent
                        visible: inboxList.count === 0
                        text: qsTr("No incoming items")
                        color: root.secondaryTextColor
                    }
                }
                ListView {
                    id: historyList
                    clip: true
                    spacing: 7
                    model: root.sharing ? root.sharing.transferHistory : null
                    delegate: TransferDelegate { controller: root.sharing }
                    ScrollBar.vertical: ScrollBar {}
                    Label {
                        anchors.centerIn: parent
                        visible: historyList.count === 0
                        text: qsTr("No transfer history")
                        color: root.secondaryTextColor
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.noticeText.length > 0
                text: root.noticeText
                color: root.noticeError ? "#ff7474" : "#67d391"
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: root.sharing && root.sharing.errorString.length > 0
                text: root.sharing ? root.sharing.errorString : ""
                color: "#ff7474"
                wrapMode: Text.WordWrap
            }
        }
    }
}
