pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

SstvPage {
    id: root

    required property var engine
    readonly property var gallery: root.engine ? root.engine.sstvGallery : null
    property bool filtersExpanded: false
    property bool deletePending: false
    property bool deleteFilesPending: false
    property bool exportPending: false
    property bool metadataUpdatePending: false
    property bool retentionPending: false
    property bool gridViewMode: true
    property string exportRecordId: ""
    property string noticeText: ""
    property bool noticeIsError: false
    property var selectedMetadata: ({})
    property string metadataEditRecordId: ""
    readonly property int visibleRecordCount: root.gridViewMode
                                               ? galleryView.count
                                               : galleryListView.count

    signal openStudioRequested()
    signal openReceiveRequested()
    signal openShareRequested(url source)

    function localFileUrl(path) {
        return root.gallery && path.length > 0
                ? root.gallery.localFileUrl(path) : ""
    }

    function formatBytes(bytes) {
        const value = Number(bytes || 0)
        if (value < 1024)
            return value + " B"
        if (value < 1024 * 1024)
            return (value / 1024).toFixed(1) + " KiB"
        if (value < 1024 * 1024 * 1024)
            return (value / (1024 * 1024)).toFixed(1) + " MiB"
        return (value / (1024 * 1024 * 1024)).toFixed(2) + " GiB"
    }

    function openInStudio(recordId, imagePath, modeName) {
        if (!root.engine || !root.engine.sstvStudio)
            return false
        const studio = root.engine.sstvStudio
        const available = studio.modes || []
        for (let index = 0; index < available.length; ++index) {
            if (available[index].name === modeName) {
                studio.modeId = available[index].id
                break
            }
        }
        if (!studio.loadSource(root.localFileUrl(imagePath))) {
            root.showNotice(qsTr("The selected Gallery image could not be queued for the Transmit Studio."), true)
            return false
        }
        root.openStudioRequested()
        root.showNotice(qsTr("Image opened in Transmit Studio. Transmission still requires an explicit TX action."), false)
        return true
    }

    function redecode(rawAudioPath) {
        if (!root.engine || rawAudioPath.length === 0
                || !root.engine.startSstvWavReplay(
                    root.localFileUrl(rawAudioPath))) {
            root.showNotice(qsTr("No retained, valid WAV is available for re-decode."), true)
            return false
        }
        root.openReceiveRequested()
        root.showNotice(qsTr("Re-decode started from the retained WAV through Decodium's native replay path."), false)
        return true
    }

    function openQsoWorkflow(record) {
        if (!root.engine || !root.engine.logSstvQso
                || !sstvQsoDialog.openForImage(record)) {
            root.showNotice(qsTr("The native SSTV QSO workflow is unavailable."), true)
            return false
        }
        return true
    }

    function frequencyHz(text) {
        const clean = text.trim()
        if (clean.length === 0)
            return -1
        const mhz = Number(clean)
        if (!Number.isFinite(mhz) || mhz < 0)
            return NaN
        return Math.round(mhz * 1000000)
    }

    function utcBoundary(text, endOfDay) {
        const clean = text.trim()
        if (clean.length === 0)
            return ""
        return clean + (endOfDay ? "T23:59:59.999Z" : "T00:00:00.000Z")
    }

    function applyFilterControls() {
        if (!root.gallery)
            return false
        const minimumHz = root.frequencyHz(minimumFrequency.text)
        const maximumHz = root.frequencyHz(maximumFrequency.text)
        if (!Number.isFinite(minimumHz) || !Number.isFinite(maximumHz)) {
            root.showNotice(qsTr("Frequency filters must be valid MHz values."), true)
            return false
        }
        const tagValues = tagsField.text.split(",").map(function(value) {
            return value.trim()
        }).filter(function(value) {
            return value.length > 0
        })
        const accepted = root.gallery.applyFilters({
            "categoryMask": categoryFilter.currentValue,
            "remoteFilter": remoteFilter.currentValue,
            "mode": modeFilter.text,
            "callsign": callsignFilter.text,
            "capturedFromUtc": root.utcBoundary(capturedFrom.text, false),
            "capturedToUtc": root.utcBoundary(capturedTo.text, true),
            "minimumFrequencyHz": minimumHz,
            "maximumFrequencyHz": maximumHz,
            "tags": tagValues,
            "requireAllTags": requireAllTags.checked,
            "partialFilter": partialFilter.currentValue,
            "uploadStateFilter": uploadFilter.currentValue,
            "search": searchField.text,
            "sortOrder": sortFilter.currentValue,
            "pageSize": 50
        })
        if (!accepted)
            root.showNotice(qsTr("One or more gallery filters are invalid."), true)
        return accepted
    }

    function resetFilters() {
        searchDelay.stop()
        searchField.clear()
        categoryFilter.currentIndex = 0
        remoteFilter.currentIndex = 0
        modeFilter.clear()
        callsignFilter.clear()
        capturedFrom.clear()
        capturedTo.clear()
        minimumFrequency.clear()
        maximumFrequency.clear()
        tagsField.clear()
        requireAllTags.checked = false
        partialFilter.currentIndex = 0
        uploadFilter.currentIndex = 0
        sortFilter.currentIndex = 0
        root.applyFilterControls()
    }

    function showNotice(message, isError) {
        root.noticeText = message
        root.noticeIsError = isError
        noticeTimer.restart()
    }

    function rememberRecord(record) {
        root.selectedMetadata = record
    }

    function editableTags(text) {
        return text.split(",").map(function(value) {
            return value.trim()
        }).filter(function(value) {
            return value.length > 0
        })
    }

    function editUserMetadata(record) {
        if (!record || !record.recordId || root.metadataUpdatePending)
            return false
        root.metadataEditRecordId = record.recordId
        metadataTagsField.text = record.tags && record.tags.length > 0
                ? record.tags.join(", ") : ""
        metadataNoteField.text = record.note || ""
        metadataEditDialog.open()
        return true
    }

    function submitUserMetadata() {
        if (!root.gallery || root.metadataEditRecordId.length === 0)
            return false
        const requestId = root.gallery.updateUserMetadata(
            root.metadataEditRecordId, metadataNoteField.text,
            root.editableTags(metadataTagsField.text))
        if (requestId === 0) {
            root.showNotice(qsTr("The Gallery metadata update could not be queued."), true)
            return false
        }
        root.metadataUpdatePending = true
        return true
    }

    function updateSelectedUserMetadata(recordId, note, tags) {
        if (!root.selectedMetadata || root.selectedMetadata.recordId !== recordId)
            return
        const updated = {}
        for (const key in root.selectedMetadata)
            updated[key] = root.selectedMetadata[key]
        updated.note = note
        updated.tags = tags
        root.selectedMetadata = updated
    }

    function qualityText(metrics, key, digits, suffix) {
        if (!metrics || metrics[key] === undefined
                || !Number.isFinite(Number(metrics[key])))
            return qsTr("Unavailable")
        return Number(metrics[key]).toFixed(digits) + suffix
    }

    function exportRecord(recordId) {
        root.exportRecordId = recordId
        exportDialog.open()
    }

    function shareRecord(imagePath) {
        root.openShareRequested(root.localFileUrl(imagePath))
        root.showNotice(qsTr("Image selected for Remote Sharing. Recipient confirmation and queueing remain explicit."), false)
    }

    Timer {
        id: searchDelay
        interval: 300
        onTriggered: {
            if (root.gallery && !root.filtersExpanded)
                root.gallery.applyFilters({"search": searchField.text.trim()})
        }
    }

    Timer {
        id: noticeTimer
        interval: 6000
        onTriggered: root.noticeText = ""
    }

    SstvLogQsoDialog {
        id: sstvQsoDialog
        objectName: "sstvGalleryLogQsoDialog"
        engine: root.engine
        primaryTextColor: root.primaryTextColor
        secondaryTextColor: root.secondaryTextColor
        accentColor: root.accentColor
        borderColor: root.borderColor
        onCompleted: function(qsoCreated, qsoId) {
            root.showNotice(
                qsoCreated
                    ? qsTr("SSTV QSO logged and Gallery image associated locally.")
                    : qsTr("Gallery image associated with the selected QSO."),
                false)
        }
    }

    component GalleryRecordActions: Menu {
        id: actionsMenu
        required property var record

        MenuItem {
            text: qsTr("Save / export PNG...")
            enabled: !root.exportPending
            onTriggered: root.exportRecord(actionsMenu.record.recordId)
        }
        MenuItem {
            objectName: "sstvGalleryEditMetadataAction"
            text: qsTr("Edit notes and tags...")
            enabled: root.gallery && !root.metadataUpdatePending
            onTriggered: root.editUserMetadata(actionsMenu.record)
        }
        MenuItem {
            text: qsTr("Prepare retransmission in Studio")
            onTriggered: root.openInStudio(
                actionsMenu.record.recordId, actionsMenu.record.imagePath,
                actionsMenu.record.mode)
        }
        MenuItem {
            text: qsTr("Re-decode retained WAV")
            enabled: actionsMenu.record.rawAudioPath.length > 0
            onTriggered: root.redecode(actionsMenu.record.rawAudioPath)
        }
        MenuItem {
            text: actionsMenu.record.relatedQsoId.length > 0
                  ? qsTr("Change QSO association...")
                  : qsTr("Log SSTV QSO...")
            onTriggered: root.openQsoWorkflow(actionsMenu.record)
        }
        MenuItem {
            text: qsTr("Prepare remote sharing...")
            enabled: Boolean(root.engine && root.engine.sstvShare)
            onTriggered: root.shareRecord(actionsMenu.record.imagePath)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Remove this record from index...")
            enabled: root.gallery && !root.deletePending
                     && !root.deleteFilesPending
            onTriggered: {
                root.gallery.clearSelection()
                root.gallery.setSelected(actionsMenu.record.recordId, true)
                deleteDialog.open()
            }
        }
        MenuItem {
            text: qsTr("Delete this record and owned files...")
            enabled: root.gallery && !root.deletePending
                     && !root.deleteFilesPending
            onTriggered: {
                root.gallery.clearSelection()
                root.gallery.setSelected(actionsMenu.record.recordId, true)
                deleteFilesDialog.open()
            }
        }
    }

    component MetadataValue: ColumnLayout {
        required property string label
        required property string value
        spacing: 1
        Label {
            Layout.fillWidth: true
            text: parent.label
            color: root.secondaryTextColor
            font.pixelSize: 9
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: parent.value
            color: root.primaryTextColor
            font.pixelSize: 11
            elide: Text.ElideRight
            Accessible.name: parent.label + ": " + parent.value
        }
    }

    Connections {
        target: root.gallery
        enabled: root.gallery !== null
        ignoreUnknownSignals: true

        function onQueryRejected(error) {
            root.showNotice(error, true)
        }

        function onDeleteFinished(requestId, ok, error) {
            root.deletePending = false
            if (ok) {
                root.showNotice(qsTr("Removed from the gallery index. PNG and JSON files were preserved."), false)
            } else {
                root.showNotice(error.length > 0 ? error : qsTr("Index removal failed."), true)
            }
        }

        function onExportFinished(requestId, ok, destination, error) {
            root.exportPending = false
            if (ok) {
                root.showNotice(qsTr("Gallery PNG exported atomically to %1").arg(destination), false)
            } else {
                root.showNotice(error.length > 0 ? error : qsTr("Gallery export failed."), true)
            }
        }

        function onDeleteFilesFinished(requestId, ok, warningOrError) {
            root.deleteFilesPending = false
            if (ok && warningOrError.length === 0) {
                root.showNotice(qsTr("Selected Gallery records and their owned local files were deleted."), false)
            } else if (ok) {
                root.showNotice(warningOrError, true)
            } else {
                root.showNotice(warningOrError.length > 0
                                ? warningOrError
                                : qsTr("Local file deletion failed; no successful deletion was reported."), true)
            }
        }

        function onFavoriteFinished(requestId, recordId, ok, error) {
            if (!ok)
                root.showNotice(error.length > 0 ? error
                                                  : qsTr("Favourite update failed."), true)
        }

        function onUserMetadataUpdateFinished(requestId, recordId, note,
                                              tags, ok, error) {
            root.metadataUpdatePending = false
            if (!ok) {
                root.showNotice(error.length > 0 ? error
                                                  : qsTr("Gallery metadata update failed."), true)
                return
            }
            root.updateSelectedUserMetadata(recordId, note, tags)
            if (root.metadataEditRecordId === recordId)
                metadataEditDialog.close()
            root.showNotice(qsTr("Gallery notes and tags saved."), false)
        }

        function onRetentionPreviewFinished(requestId, ok, error) {
            root.retentionPending = false
            if (!ok) {
                root.showNotice(error.length > 0 ? error
                                                  : qsTr("Retention preview failed."), true)
                return
            }
            retentionConfirmation.clear()
            retentionDialog.open()
        }

        function onRetentionApplyFinished(requestId, automatic, ok,
                                          warningOrError) {
            root.retentionPending = false
            if (!ok) {
                root.showNotice(warningOrError.length > 0 ? warningOrError
                                                          : qsTr("Retention apply failed."), true)
            } else if (automatic) {
                root.showNotice(warningOrError.length > 0 ? warningOrError
                                                          : qsTr("Automatic retention completed through the crash-safe deletion journal."), warningOrError.length > 0)
            } else {
                root.showNotice(warningOrError.length > 0 ? warningOrError
                                                          : qsTr("Confirmed retention plan completed through the crash-safe deletion journal."), warningOrError.length > 0)
            }
        }

        function onQuotaRefreshFinished(requestId, ok, error) {
            if (!ok)
                root.showNotice(error.length > 0 ? error
                                                  : qsTr("Storage quota calculation failed."), true)
        }
    }

    FileDialog {
        id: exportDialog
        objectName: "sstvGalleryExportDialog"
        title: qsTr("Export Gallery PNG")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            if (!root.gallery || root.exportRecordId.length === 0)
                return
            const requestId = root.gallery.requestExportRecord(
                root.exportRecordId, selectedFile, false)
            if (requestId === 0) {
                root.showNotice(qsTr("The Gallery export request could not be queued."), true)
            } else {
                root.exportPending = true
            }
        }
    }

    Popup {
        id: metadataEditDialog
        objectName: "sstvGalleryEditMetadataDialog"
        anchors.centerIn: parent
        width: Math.min(560, root.width - 40)
        height: 390
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 16
        background: Rectangle {
            color: "#111c25"
            border.color: root.accentColor
            border.width: 1
            radius: 8
        }
        contentItem: ColumnLayout {
            spacing: 9
            Label {
                Layout.fillWidth: true
                text: qsTr("Edit Gallery notes and tags")
                color: root.accentColor
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Tags are local Gallery metadata. They do not change the image, radio reception data or a remote transfer.")
                color: root.secondaryTextColor
                wrapMode: Text.WordWrap
            }
            Label {
                text: qsTr("Tags")
                color: root.secondaryTextColor
            }
            TextField {
                id: metadataTagsField
                objectName: "sstvGalleryMetadataTags"
                Layout.fillWidth: true
                enabled: !root.metadataUpdatePending
                maximumLength: 2079
                selectByMouse: true
                placeholderText: qsTr("Comma separated")
                Accessible.name: qsTr("Gallery tags")
            }
            Label {
                text: qsTr("Note")
                color: root.secondaryTextColor
            }
            TextArea {
                id: metadataNoteField
                objectName: "sstvGalleryMetadataNote"
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: !root.metadataUpdatePending
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                placeholderText: qsTr("Optional operator note")
                onTextChanged: {
                    if (text.length > 4096)
                        text = text.slice(0, 4096)
                }
                Accessible.name: qsTr("Gallery note")
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Up to 32 tags of 64 characters and a 4096-character note.")
                color: root.secondaryTextColor
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    enabled: !root.metadataUpdatePending
                    onClicked: metadataEditDialog.close()
                }
                Button {
                    objectName: "sstvGallerySaveMetadata"
                    text: qsTr("Save metadata")
                    enabled: !root.metadataUpdatePending
                             && root.metadataEditRecordId.length > 0
                    highlighted: true
                    onClicked: root.submitUserMetadata()
                }
            }
        }
    }

    Popup {
        id: deleteDialog
        anchors.centerIn: parent
        width: Math.min(480, root.width - 40)
        height: 210
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 16
        background: Rectangle {
            color: "#111c25"
            border.color: root.borderColor
            border.width: 1
            radius: 8
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: qsTr("Remove selected records?")
                color: root.primaryTextColor
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("This removes %1 selected record(s) from the SQLite gallery index only. Original PNG and JSON sidecar files will remain on disk.")
                          .arg(root.gallery ? root.gallery.selectedCount : 0)
                color: root.primaryTextColor
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); onClicked: deleteDialog.close() }
                Button {
                    text: qsTr("Remove from index")
                    highlighted: true
                    onClicked: {
                        if (!root.gallery)
                            return
                        const requestId = root.gallery.requestDeleteSelectedFromIndex()
                        deleteDialog.close()
                        if (requestId === 0) {
                            root.showNotice(qsTr("The index removal request could not be queued."), true)
                        } else {
                            root.deletePending = true
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: deleteFilesDialog
        objectName: "sstvGalleryDeleteFilesDialog"
        anchors.centerIn: parent
        width: Math.min(520, root.width - 40)
        height: 260
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 16
        background: Rectangle {
            color: "#111c25"
            border.color: "#d55b66"
            border.width: 1
            radius: 8
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: qsTr("Delete selected local SSTV files?")
                color: "#ff8b94"
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("This destructive action removes %1 selected record(s), their verified PNG and JSON sidecar, any thumbnail, and retained raw WAV only when no unselected record shares it. Files are staged privately before the SQLite transaction. Use ‘Remove from index’ if you want to preserve files.")
                          .arg(root.gallery ? root.gallery.selectedCount : 0)
                color: root.primaryTextColor
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    onClicked: deleteFilesDialog.close()
                }
                Button {
                    text: qsTr("Delete files and records")
                    highlighted: true
                    onClicked: {
                        if (!root.gallery)
                            return
                        const requestId = root.gallery.requestDeleteSelectedWithFiles()
                        deleteFilesDialog.close()
                        if (requestId === 0) {
                            root.showNotice(qsTr("The local file deletion request could not be queued."), true)
                        } else {
                            root.deleteFilesPending = true
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: retentionDialog
        objectName: "sstvGalleryRetentionDialog"
        anchors.centerIn: parent
        width: Math.min(580, root.width - 40)
        height: 390
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 16
        background: Rectangle {
            color: "#111c25"
            border.color: "#d55b66"
            border.width: 1
            radius: 8
        }
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: qsTr("Confirm Gallery retention plan")
                color: "#ff8b94"
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Preview only: %1 record(s), %2 images, %3 thumbnails and %4 raw audio. Favourites, QSO-linked records, protected shared records and unsafe paths were excluded.")
                          .arg(root.gallery ? (root.gallery.retentionPreview.recordCount || 0) : 0)
                          .arg(root.formatBytes(root.gallery ? root.gallery.retentionPreview.imageBytes : 0))
                          .arg(root.formatBytes(root.gallery ? root.gallery.retentionPreview.thumbnailBytes : 0))
                          .arg(root.formatBytes(root.gallery ? root.gallery.retentionPreview.rawAudioBytes : 0))
                color: root.primaryTextColor
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: root.gallery && root.gallery.retentionPreview.warning
                      ? root.gallery.retentionPreview.warning : ""
                visible: text.length > 0
                color: "#ffb454"
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Type exactly: %1")
                          .arg(root.gallery
                               ? (root.gallery.retentionPreview.confirmationPhrase || "") : "")
                color: root.secondaryTextColor
                wrapMode: Text.WordWrap
            }
            TextField {
                id: retentionConfirmation
                objectName: "sstvGalleryRetentionConfirmation"
                Layout.fillWidth: true
                selectByMouse: true
                Accessible.name: qsTr("Strong confirmation for Gallery retention")
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    onClicked: retentionDialog.close()
                }
                Button {
                    objectName: "sstvGalleryApplyRetention"
                    text: qsTr("Apply retention")
                    highlighted: true
                    enabled: root.gallery
                             && retentionConfirmation.text
                                === (root.gallery.retentionPreview.confirmationPhrase || "")
                             && (root.gallery.retentionPreview.recordCount || 0) > 0
                    onClicked: {
                        const requestId = root.gallery.applyRetentionPreview(
                            root.gallery.retentionPreview.token,
                            retentionConfirmation.text)
                        retentionDialog.close()
                        if (requestId === 0) {
                            root.showNotice(qsTr("The retention plan could not be queued; request a new preview."), true)
                        } else {
                            root.retentionPending = true
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("GALLERY")
                color: root.accentColor
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 1.2
            }
            ToolButton {
                objectName: "sstvGalleryGridMode"
                text: qsTr("Grid")
                checkable: true
                checked: root.gridViewMode
                onClicked: root.gridViewMode = true
                Accessible.name: qsTr("Show Gallery as a grid")
            }
            ToolButton {
                objectName: "sstvGalleryListMode"
                text: qsTr("List")
                checkable: true
                checked: !root.gridViewMode
                onClicked: root.gridViewMode = false
                Accessible.name: qsTr("Show Gallery as a list")
            }
            TextField {
                id: searchField
                objectName: "sstvGallerySearch"
                Layout.fillWidth: true
                placeholderText: qsTr("Search callsign, mode, source, note or tag")
                enabled: root.gallery !== null
                selectByMouse: true
                onTextEdited: searchDelay.restart()
                onAccepted: root.applyFilterControls()
                Accessible.name: qsTr("Search SSTV gallery")
            }
            Button {
                text: root.filtersExpanded ? qsTr("Hide filters") : qsTr("Filters")
                checkable: true
                checked: root.filtersExpanded
                onClicked: root.filtersExpanded = !root.filtersExpanded
                Accessible.name: qsTr("Toggle gallery filters")
            }
            Button {
                text: qsTr("Refresh")
                enabled: root.gallery !== null && !root.gallery.loading
                onClicked: root.gallery.reload()
                Accessible.name: qsTr("Refresh SSTV gallery")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: filterLayout.implicitHeight + 20
            visible: root.filtersExpanded
            color: "#081017"
            border.color: root.borderColor
            border.width: 1
            radius: 7

            ColumnLayout {
                id: filterLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 8
                    rowSpacing: 6

                    Label { text: qsTr("Category"); color: root.secondaryTextColor }
                    ComboBox {
                        id: categoryFilter
                        Layout.fillWidth: true
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            {"text": qsTr("All"), "value": 15},
                            {"text": qsTr("Received"), "value": 1},
                            {"text": qsTr("Transmitted"), "value": 2},
                            {"text": qsTr("Imported"), "value": 4},
                            {"text": qsTr("Draft"), "value": 8}
                        ]
                    }
                    Label { text: qsTr("Origin"); color: root.secondaryTextColor }
                    ComboBox {
                        id: remoteFilter
                        Layout.fillWidth: true
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            {"text": qsTr("Any"), "value": 0},
                            {"text": qsTr("Remote"), "value": 1},
                            {"text": qsTr("Local"), "value": 2}
                        ]
                    }

                    Label { text: qsTr("Mode"); color: root.secondaryTextColor }
                    TextField { id: modeFilter; Layout.fillWidth: true; placeholderText: qsTr("e.g. Martin M1") }
                    Label { text: qsTr("Callsign"); color: root.secondaryTextColor }
                    TextField { id: callsignFilter; Layout.fillWidth: true; placeholderText: qsTr("Local or remote") }

                    Label { text: qsTr("From UTC"); color: root.secondaryTextColor }
                    TextField { id: capturedFrom; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD"; inputMethodHints: Qt.ImhDate }
                    Label { text: qsTr("To UTC"); color: root.secondaryTextColor }
                    TextField { id: capturedTo; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD"; inputMethodHints: Qt.ImhDate }

                    Label { text: qsTr("Min MHz"); color: root.secondaryTextColor }
                    TextField { id: minimumFrequency; Layout.fillWidth: true; placeholderText: qsTr("Any"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    Label { text: qsTr("Max MHz"); color: root.secondaryTextColor }
                    TextField { id: maximumFrequency; Layout.fillWidth: true; placeholderText: qsTr("Any"); inputMethodHints: Qt.ImhFormattedNumbersOnly }

                    Label { text: qsTr("Tags"); color: root.secondaryTextColor }
                    TextField { id: tagsField; Layout.fillWidth: true; placeholderText: qsTr("Comma separated") }
                    Label { text: qsTr("Tag rule"); color: root.secondaryTextColor }
                    CheckBox { id: requireAllTags; text: qsTr("Match all tags") }

                    Label { text: qsTr("Image state"); color: root.secondaryTextColor }
                    ComboBox {
                        id: partialFilter
                        Layout.fillWidth: true
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            {"text": qsTr("Any"), "value": 0},
                            {"text": qsTr("Partial only"), "value": 1},
                            {"text": qsTr("Complete only"), "value": 2}
                        ]
                    }
                    Label { text: qsTr("Upload"); color: root.secondaryTextColor }
                    ComboBox {
                        id: uploadFilter
                        Layout.fillWidth: true
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            {"text": qsTr("Any"), "value": -1},
                            {"text": qsTr("Not requested"), "value": 0},
                            {"text": qsTr("Pending"), "value": 1},
                            {"text": qsTr("Uploading"), "value": 2},
                            {"text": qsTr("Uploaded"), "value": 3},
                            {"text": qsTr("Failed"), "value": 4}
                        ]
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Sort"); color: root.secondaryTextColor }
                    ComboBox {
                        id: sortFilter
                        Layout.preferredWidth: 230
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            {"text": qsTr("Captured newest"), "value": 0},
                            {"text": qsTr("Captured oldest"), "value": 1},
                            {"text": qsTr("Callsign A-Z"), "value": 2},
                            {"text": qsTr("Callsign Z-A"), "value": 3},
                            {"text": qsTr("Mode A-Z"), "value": 4},
                            {"text": qsTr("Mode Z-A"), "value": 5},
                            {"text": qsTr("Frequency low-high"), "value": 6},
                            {"text": qsTr("Frequency high-low"), "value": 7},
                            {"text": qsTr("Updated newest"), "value": 8},
                            {"text": qsTr("Updated oldest"), "value": 9}
                        ]
                    }
                    Item { Layout.fillWidth: true }
                    Button { text: qsTr("Reset"); onClicked: root.resetFilters() }
                    Button { text: qsTr("Apply"); highlighted: true; onClicked: root.applyFilterControls() }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: quotaLayout.implicitHeight + 16
            color: "#081017"
            border.color: root.borderColor
            border.width: 1
            radius: 6

            RowLayout {
                id: quotaLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    text: root.gallery
                          ? qsTr("Images %1   Thumbnails %2   Raw audio %3")
                              .arg(root.formatBytes(root.gallery.quotaSummary.imageBytes))
                              .arg(root.formatBytes(root.gallery.quotaSummary.thumbnailBytes))
                              .arg(root.formatBytes(root.gallery.quotaSummary.rawAudioBytes))
                          : qsTr("Storage quota unavailable")
                    color: root.gallery && root.gallery.quotaSummary.complete
                           ? root.secondaryTextColor : "#ffb454"
                    elide: Text.ElideRight
                }
                Label {
                    visible: root.gallery && !root.gallery.quotaSummary.complete
                    text: qsTr("Incomplete: unsafe or missing paths are protected")
                    color: "#ffb454"
                    font.pixelSize: 9
                }
                Button {
                    objectName: "sstvGalleryRefreshQuota"
                    text: qsTr("Recalculate")
                    enabled: root.gallery && !root.gallery.retentionBusy
                    onClicked: root.gallery.refreshQuota()
                }
                Button {
                    objectName: "sstvGalleryPreviewRetention"
                    text: root.retentionPending ? qsTr("Planning...")
                                                : qsTr("Preview retention...")
                    enabled: root.gallery && !root.gallery.retentionBusy
                             && !root.retentionPending
                    onClicked: {
                        const requestId = root.gallery.requestRetentionPreview()
                        if (requestId === 0) {
                            root.showNotice(qsTr("The retention preview could not be queued."), true)
                        } else {
                            root.retentionPending = true
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#081017"
            border.color: root.borderColor
            border.width: 1
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: root.gallery && root.gallery.errorString.length > 0
                          ? root.gallery.errorString
                          : (root.gallery
                             ? qsTr("%1 loaded - %2 selected").arg(root.visibleRecordCount).arg(root.gallery.selectedCount)
                             : qsTr("Gallery storage is unavailable"))
                    color: root.gallery && root.gallery.errorString.length > 0
                           ? "#ff8b94" : root.secondaryTextColor
                    elide: Text.ElideRight
                }
                Button {
                    text: qsTr("Clear selection")
                    font.pixelSize: 10
                    implicitHeight: 30
                    leftPadding: 8
                    rightPadding: 8
                    enabled: root.gallery && root.gallery.selectedCount > 0
                             && !root.deletePending && !root.deleteFilesPending
                    onClicked: root.gallery.clearSelection()
                }
                Button {
                    objectName: "sstvGalleryRemoveIndex"
                    text: root.deletePending ? qsTr("Removing...") : qsTr("Remove from index")
                    font.pixelSize: 10
                    implicitHeight: 30
                    leftPadding: 8
                    rightPadding: 8
                    enabled: root.gallery && root.gallery.selectedCount > 0
                             && !root.deletePending && !root.deleteFilesPending
                    onClicked: deleteDialog.open()
                    Accessible.name: qsTr("Remove selected records from gallery index")
                }
                Button {
                    objectName: "sstvGalleryDeleteFiles"
                    text: root.deleteFilesPending
                          ? qsTr("Deleting...")
                          : qsTr("Delete files...")
                    font.pixelSize: 10
                    implicitHeight: 30
                    leftPadding: 8
                    rightPadding: 8
                    enabled: root.gallery && root.gallery.selectedCount > 0
                             && !root.deletePending && !root.deleteFilesPending
                    onClicked: deleteFilesDialog.open()
                    Accessible.name: qsTr("Delete selected Gallery records and owned local files")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: noticeLabel.implicitHeight + 14
            visible: root.noticeText.length > 0
            color: root.noticeIsError ? "#3a171b" : "#123124"
            border.color: root.noticeIsError ? "#d55b66" : "#43d17b"
            radius: 5
            Label {
                id: noticeLabel
                anchors.fill: parent
                anchors.margins: 7
                text: root.noticeText
                color: root.primaryTextColor
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            objectName: "sstvGalleryMetadataPanel"
            Layout.fillWidth: true
            Layout.preferredHeight: root.width < 760 ? 238 : 142
            visible: Boolean(root.selectedMetadata.recordId)
            color: "#081017"
            border.color: root.accentColor
            border.width: 1
            radius: 7

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Selected image metadata and reception quality")
                        color: root.accentColor
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    ToolButton {
                        objectName: "sstvGalleryEditSelectedMetadata"
                        text: qsTr("Edit")
                        enabled: root.gallery && !root.metadataUpdatePending
                        onClicked: root.editUserMetadata(root.selectedMetadata)
                        Accessible.name: qsTr("Edit notes and tags")
                    }
                    Label {
                        text: root.selectedMetadata.complete
                              ? qsTr("COMPLETE") : qsTr("PARTIAL %1%").arg(
                                    root.selectedMetadata.completionPercent || 0)
                        color: root.selectedMetadata.complete
                               ? "#6be39a" : "#ffc66d"
                        font.bold: true
                    }
                    Label {
                        visible: Boolean(root.selectedMetadata.remote)
                        text: qsTr("REMOTE")
                        color: "#8fd3ff"
                        font.bold: true
                    }
                    Label {
                        visible: Boolean(root.selectedMetadata.uploadStateName)
                        text: qsTr("UPLOAD: %1").arg(
                                  root.selectedMetadata.uploadStateName || "")
                        color: "#d4adff"
                        font.bold: true
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: root.width < 760 ? 2 : 4
                    columnSpacing: 16
                    rowSpacing: 4
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Station / mode")
                        value: (root.selectedMetadata.remoteCallsign
                                || root.selectedMetadata.localCallsign
                                || qsTr("Unknown station")) + " - "
                               + (root.selectedMetadata.mode || qsTr("Unknown mode"))
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("VIS / sync confidence")
                        value: (root.selectedMetadata.visValid
                                ? String(root.selectedMetadata.visCode)
                                : qsTr("Unavailable")) + " / "
                               + root.qualityText(
                                   root.selectedMetadata.qualityMetrics,
                                   "syncConfidence", 3, "")
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Slant / frequency offset")
                        value: Number(root.selectedMetadata.slantCorrectionPpm || 0)
                                   .toFixed(2) + " ppm / "
                               + Number(root.selectedMetadata.audioFrequencyHz || 0)
                                   .toFixed(0) + " Hz"
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Coverage / SNR")
                        value: String(root.selectedMetadata.completionPercent || 0)
                               + "% / "
                               + root.qualityText(
                                   root.selectedMetadata.qualityMetrics,
                                   "snrDb", 2, " dB")
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Image / category")
                        value: String(root.selectedMetadata.imageWidth || 0)
                               + "x" + String(root.selectedMetadata.imageHeight || 0)
                               + " / " + (root.selectedMetadata.categoryName || "")
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Frequency / source")
                        value: (root.selectedMetadata.frequencyHz > 0
                                ? (root.selectedMetadata.frequencyHz / 1000000)
                                    .toFixed(3) + " MHz"
                                : qsTr("Unavailable")) + " / "
                               + (root.selectedMetadata.source || qsTr("Unavailable"))
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Line drop rate")
                        value: root.qualityText(
                                   root.selectedMetadata.qualityMetrics,
                                   "lineDropRate", 4, "")
                    }
                    MetadataValue {
                        Layout.fillWidth: true
                        label: qsTr("Captured UTC")
                        value: root.selectedMetadata.capturedAtUtc
                               ? Qt.formatDateTime(
                                     root.selectedMetadata.capturedAtUtc,
                                     "yyyy-MM-dd HH:mm:ss 'UTC'")
                               : qsTr("Unavailable")
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#05090d"
            border.color: root.borderColor
            border.width: 1
            radius: 8
            clip: true

            GridView {
                id: galleryView
                objectName: "sstvGalleryView"
                visible: root.gridViewMode
                anchors.fill: parent
                anchors.margins: 8
                model: root.gallery
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                cellWidth: Math.max(190, Math.floor(width / Math.max(1, Math.floor(width / 220))))
                cellHeight: 250
                reuseItems: true

                onAtYEndChanged: {
                    if (atYEnd && count > 0 && root.gallery
                            && root.gallery.hasMore && !root.gallery.loading)
                        root.gallery.loadMore()
                }

                delegate: Rectangle {
                    id: card
                    required property int index
                    required property string recordId
                    required property string categoryName
                    required property var capturedAtUtc
                    required property string mode
                    required property int visCode
                    required property bool visValid
                    required property string remoteCallsign
                    required property string remoteGrid
                    required property string localCallsign
                    required property string source
                    required property double frequencyHz
                    required property bool complete
                    required property bool partial
                    required property int completionPercent
                    required property bool remote
                    required property string uploadStateName
                    required property var tags
                    required property string note
                    required property bool selected
                    required property string imagePath
                    required property int imageWidth
                    required property int imageHeight
                    required property double audioFrequencyHz
                    required property var qualityMetrics
                    required property double slantCorrectionPpm
                    required property string rawAudioPath
                    required property string relatedQsoId
                    required property bool favorite

                    readonly property var recordData: ({
                        "recordId": card.recordId,
                        "categoryName": card.categoryName,
                        "capturedAtUtc": card.capturedAtUtc,
                        "mode": card.mode,
                        "visCode": card.visCode,
                        "visValid": card.visValid,
                        "remoteCallsign": card.remoteCallsign,
                        "remoteGrid": card.remoteGrid,
                        "localCallsign": card.localCallsign,
                        "source": card.source,
                        "frequencyHz": card.frequencyHz,
                        "complete": card.complete,
                        "partial": card.partial,
                        "completionPercent": card.completionPercent,
                        "remote": card.remote,
                        "uploadStateName": card.uploadStateName,
                        "tags": card.tags,
                        "note": card.note,
                        "imagePath": card.imagePath,
                        "imageWidth": card.imageWidth,
                        "imageHeight": card.imageHeight,
                        "audioFrequencyHz": card.audioFrequencyHz,
                        "qualityMetrics": card.qualityMetrics,
                        "slantCorrectionPpm": card.slantCorrectionPpm,
                        "rawAudioPath": card.rawAudioPath,
                        "relatedQsoId": card.relatedQsoId
                    })

                    width: galleryView.cellWidth - 10
                    height: galleryView.cellHeight - 10
                    activeFocusOnTab: true
                    color: selected ? "#112d38" : "#0b131a"
                    border.color: activeFocus || selected
                                  ? root.accentColor : root.borderColor
                    border.width: selected ? 2 : 1
                    radius: 7
                    Accessible.role: Accessible.ListItem
                    Accessible.name: qsTr("Gallery image %1, %2, %3")
                                     .arg(card.recordId).arg(card.mode)
                                     .arg(card.complete ? qsTr("complete")
                                                        : qsTr("partial"))
                    Accessible.description: qsTr("Press Space to select; Enter shows metadata; the Menu key opens actions.")

                    Component.onCompleted: {
                        if (!root.selectedMetadata.recordId)
                            root.rememberRecord(card.recordData)
                    }
                    TapHandler {
                        onTapped: {
                            root.rememberRecord(card.recordData)
                            card.forceActiveFocus()
                        }
                    }
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Space) {
                            if (root.gallery)
                                root.gallery.setSelected(card.recordId,
                                                         !card.selected)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return
                                   || event.key === Qt.Key_Enter) {
                            root.rememberRecord(card.recordData)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Menu
                                   || (event.key === Qt.Key_F10
                                       && (event.modifiers
                                           & Qt.ShiftModifier))) {
                            recordActions.open()
                            event.accepted = true
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 4
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 136
                            color: "#020406"
                            radius: 4
                            clip: true
                            Image {
                                id: thumbnail
                                anchors.fill: parent
                                source: "image://decodium-sstv-gallery/" + card.recordId
                                sourceSize.width: 320
                                sourceSize.height: 240
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectFit
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: thumbnail.status === Image.Loading
                                text: qsTr("Loading preview...")
                                color: root.secondaryTextColor
                                font.pixelSize: 10
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: thumbnail.status === Image.Error
                                text: qsTr("Preview unavailable")
                                color: root.secondaryTextColor
                                font.pixelSize: 10
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.margins: 5
                                implicitWidth: stateLabel.implicitWidth + 10
                                implicitHeight: stateLabel.implicitHeight + 5
                                color: card.complete ? "#153925" : "#493514"
                                radius: 4
                                Label {
                                    id: stateLabel
                                    anchors.centerIn: parent
                                    text: card.complete ? qsTr("COMPLETE") : qsTr("PARTIAL")
                                    color: card.complete ? "#6be39a" : "#ffc66d"
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                            }
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: 5
                                visible: card.remote
                                implicitWidth: remoteBadge.implicitWidth + 10
                                implicitHeight: remoteBadge.implicitHeight + 5
                                color: "#173248"
                                radius: 4
                                Label {
                                    id: remoteBadge
                                    anchors.centerIn: parent
                                    text: qsTr("REMOTE")
                                    color: "#8fd3ff"
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                            }
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 5
                                visible: card.uploadStateName.length > 0
                                implicitWidth: uploadBadge.implicitWidth + 10
                                implicitHeight: uploadBadge.implicitHeight + 5
                                color: "#332348"
                                radius: 4
                                Label {
                                    id: uploadBadge
                                    anchors.centerIn: parent
                                    text: qsTr("UPLOAD: %1").arg(card.uploadStateName)
                                    color: "#d4adff"
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                            }
                            CheckBox {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 3
                                checked: card.selected
                                onClicked: {
                                    if (root.gallery)
                                        root.gallery.setSelected(card.recordId, checked)
                                }
                                Accessible.name: qsTr("Select image %1").arg(card.recordId)
                            }
                            ToolButton {
                                objectName: "sstvGalleryFavoriteToggle"
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                text: card.favorite ? "\u2605" : "\u2606"
                                font.pixelSize: 22
                                Accessible.name: card.favorite
                                                 ? qsTr("Remove image from favourites")
                                                 : qsTr("Add image to favourites")
                                Accessible.description: qsTr("Favourite images are protected from Gallery retention")
                                onClicked: {
                                    if (root.gallery)
                                        root.gallery.setFavorite(
                                            card.recordId, !card.favorite)
                                }
                            }
                            ToolButton {
                                objectName: "sstvGalleryRecordActions"
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                text: "\u22ee"
                                Accessible.name: qsTr("Actions for Gallery image %1").arg(card.recordId)
                                onClicked: recordActions.open()

                                GalleryRecordActions {
                                    id: recordActions
                                    record: card.recordData
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: card.remoteCallsign.length > 0 ? card.remoteCallsign
                                  : (card.localCallsign.length > 0 ? card.localCallsign : qsTr("Unknown station"))
                            color: root.primaryTextColor
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: card.mode + "  -  " + card.categoryName
                            color: root.accentColor
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (card.frequencyHz > 0 ? (card.frequencyHz / 1000000).toFixed(3) + " MHz  -  " : "")
                                  + Qt.formatDateTime(card.capturedAtUtc, "yyyy-MM-dd HH:mm 'UTC'")
                            color: root.secondaryTextColor
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (card.tags && card.tags.length > 0) ? card.tags.join(", ") : card.source
                            color: root.secondaryTextColor
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                    }
                }

                footer: Item {
                    width: galleryView.width
                    height: root.gallery && (root.gallery.hasMore || root.gallery.loading) ? 54 : 0
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        BusyIndicator {
                            width: 28
                            height: 28
                            running: root.gallery && root.gallery.loading
                            visible: running
                        }
                        Button {
                            text: qsTr("Load more")
                            visible: root.gallery && root.gallery.hasMore && !root.gallery.loading
                            enabled: visible
                            onClicked: root.gallery.loadMore()
                        }
                    }
                }
            }

            ListView {
                id: galleryListView
                objectName: "sstvGalleryListView"
                visible: !root.gridViewMode
                anchors.fill: parent
                anchors.margins: 8
                model: root.gallery
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                spacing: 6
                reuseItems: true

                onAtYEndChanged: {
                    if (atYEnd && count > 0 && root.gallery
                            && root.gallery.hasMore && !root.gallery.loading)
                        root.gallery.loadMore()
                }

                delegate: Rectangle {
                    id: listCard
                    required property int index
                    required property string recordId
                    required property string categoryName
                    required property var capturedAtUtc
                    required property string mode
                    required property int visCode
                    required property bool visValid
                    required property string remoteCallsign
                    required property string remoteGrid
                    required property string localCallsign
                    required property string source
                    required property double frequencyHz
                    required property bool complete
                    required property bool partial
                    required property int completionPercent
                    required property bool remote
                    required property string uploadStateName
                    required property var tags
                    required property string note
                    required property bool selected
                    required property string imagePath
                    required property int imageWidth
                    required property int imageHeight
                    required property double audioFrequencyHz
                    required property var qualityMetrics
                    required property double slantCorrectionPpm
                    required property string rawAudioPath
                    required property string relatedQsoId
                    required property bool favorite

                    readonly property var recordData: ({
                        "recordId": listCard.recordId,
                        "categoryName": listCard.categoryName,
                        "capturedAtUtc": listCard.capturedAtUtc,
                        "mode": listCard.mode,
                        "visCode": listCard.visCode,
                        "visValid": listCard.visValid,
                        "remoteCallsign": listCard.remoteCallsign,
                        "remoteGrid": listCard.remoteGrid,
                        "localCallsign": listCard.localCallsign,
                        "source": listCard.source,
                        "frequencyHz": listCard.frequencyHz,
                        "complete": listCard.complete,
                        "partial": listCard.partial,
                        "completionPercent": listCard.completionPercent,
                        "remote": listCard.remote,
                        "uploadStateName": listCard.uploadStateName,
                        "tags": listCard.tags,
                        "note": listCard.note,
                        "imagePath": listCard.imagePath,
                        "imageWidth": listCard.imageWidth,
                        "imageHeight": listCard.imageHeight,
                        "audioFrequencyHz": listCard.audioFrequencyHz,
                        "qualityMetrics": listCard.qualityMetrics,
                        "slantCorrectionPpm": listCard.slantCorrectionPpm,
                        "rawAudioPath": listCard.rawAudioPath,
                        "relatedQsoId": listCard.relatedQsoId
                    })

                    width: ListView.view.width
                    height: 112
                    activeFocusOnTab: true
                    color: selected ? "#112d38" : "#0b131a"
                    border.color: activeFocus || selected
                                  ? root.accentColor : root.borderColor
                    border.width: selected ? 2 : 1
                    radius: 7
                    Accessible.role: Accessible.ListItem
                    Accessible.name: qsTr("Gallery image %1, %2, %3")
                                     .arg(listCard.recordId).arg(listCard.mode)
                                     .arg(listCard.complete ? qsTr("complete")
                                                            : qsTr("partial"))
                    Accessible.description: qsTr("Press Space to select; Enter shows metadata; the Menu key opens actions.")

                    TapHandler {
                        onTapped: {
                            root.rememberRecord(listCard.recordData)
                            listCard.forceActiveFocus()
                        }
                    }
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Space) {
                            if (root.gallery)
                                root.gallery.setSelected(listCard.recordId,
                                                         !listCard.selected)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return
                                   || event.key === Qt.Key_Enter) {
                            root.rememberRecord(listCard.recordData)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Menu
                                   || (event.key === Qt.Key_F10
                                       && (event.modifiers
                                           & Qt.ShiftModifier))) {
                            listActions.open()
                            event.accepted = true
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 132
                            Layout.fillHeight: true
                            color: "#020406"
                            radius: 4
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: "image://decodium-sstv-gallery/"
                                        + listCard.recordId
                                sourceSize.width: 264
                                sourceSize.height: 196
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: listCard.remoteCallsign.length > 0
                                          ? listCard.remoteCallsign
                                          : (listCard.localCallsign.length > 0
                                             ? listCard.localCallsign
                                             : qsTr("Unknown station"))
                                    color: root.primaryTextColor
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: listCard.complete
                                          ? qsTr("COMPLETE")
                                          : qsTr("PARTIAL %1%").arg(
                                                listCard.completionPercent)
                                    color: listCard.complete
                                           ? "#6be39a" : "#ffc66d"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Label {
                                    visible: listCard.remote
                                    text: qsTr("REMOTE")
                                    color: "#8fd3ff"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Label {
                                    visible: listCard.uploadStateName.length > 0
                                    text: qsTr("UPLOAD: %1").arg(
                                              listCard.uploadStateName)
                                    color: "#d4adff"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("%1 - %2 - VIS %3 - %4% coverage")
                                      .arg(listCard.mode)
                                      .arg(listCard.categoryName)
                                      .arg(listCard.visValid
                                           ? listCard.visCode
                                           : qsTr("unavailable"))
                                      .arg(listCard.completionPercent)
                                color: root.accentColor
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: (listCard.frequencyHz > 0
                                       ? (listCard.frequencyHz / 1000000)
                                           .toFixed(3) + " MHz - " : "")
                                      + Qt.formatDateTime(
                                          listCard.capturedAtUtc,
                                          "yyyy-MM-dd HH:mm 'UTC'")
                                color: root.secondaryTextColor
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: listCard.source
                                color: root.secondaryTextColor
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                        }
                        CheckBox {
                            checked: listCard.selected
                            onClicked: {
                                if (root.gallery)
                                    root.gallery.setSelected(
                                        listCard.recordId, checked)
                            }
                            Accessible.name: qsTr("Select image %1").arg(
                                                 listCard.recordId)
                        }
                        ToolButton {
                            text: listCard.favorite ? "\u2605" : "\u2606"
                            font.pixelSize: 22
                            Accessible.name: listCard.favorite
                                             ? qsTr("Remove image from favourites")
                                             : qsTr("Add image to favourites")
                            onClicked: {
                                if (root.gallery)
                                    root.gallery.setFavorite(
                                        listCard.recordId, !listCard.favorite)
                            }
                        }
                        ToolButton {
                            objectName: "sstvGalleryListRecordActions"
                            text: "\u22ee"
                            Accessible.name: qsTr("Actions for Gallery image %1")
                                             .arg(listCard.recordId)
                            onClicked: listActions.open()
                            GalleryRecordActions {
                                id: listActions
                                record: listCard.recordData
                            }
                        }
                    }
                }

                footer: Item {
                    width: galleryListView.width
                    height: root.gallery
                            && (root.gallery.hasMore || root.gallery.loading)
                            ? 54 : 0
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        BusyIndicator {
                            width: 28
                            height: 28
                            running: root.gallery && root.gallery.loading
                            visible: running
                        }
                        Button {
                            text: qsTr("Load more")
                            visible: root.gallery && root.gallery.hasMore
                                     && !root.gallery.loading
                            enabled: visible
                            onClicked: root.gallery.loadMore()
                        }
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 520)
                spacing: 8
                visible: root.visibleRecordCount === 0
                         && (!root.gallery || !root.gallery.loading)
                Label {
                    width: parent.width
                    text: root.gallery && root.gallery.errorString.length > 0
                          ? qsTr("Gallery unavailable") : qsTr("No SSTV image records match these filters")
                    color: root.primaryTextColor
                    font.pixelSize: 16
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    width: parent.width
                    text: root.gallery && root.gallery.errorString.length > 0 ? root.gallery.errorString
                          : qsTr("Received, transmitted, imported and draft images will appear here as path-only records. Full-resolution files stay on disk.")
                    color: root.gallery && root.gallery.errorString.length > 0 ? "#ff8b94" : root.secondaryTextColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
            BusyIndicator {
                anchors.centerIn: parent
                running: root.gallery && root.gallery.loading
                         && root.visibleRecordCount === 0
                visible: running
            }
        }
    }
}
