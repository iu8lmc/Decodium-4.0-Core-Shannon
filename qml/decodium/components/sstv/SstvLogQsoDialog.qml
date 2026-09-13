pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var engine: null
    property color primaryTextColor: "#e5edf3"
    property color secondaryTextColor: "#91a0ab"
    property color accentColor: "#24c9ee"
    property color borderColor: "#273946"
    property color panelColor: "#0b131a"

    property string imageRecordId: ""
    property string imageMode: ""
    property string currentRelatedQsoId: ""
    property var existingChoices: []
    property string selectedExistingQsoId: ""
    property string pendingRequestToken: ""
    property bool pending: false
    property string feedback: ""
    property bool feedbackIsError: false

    readonly property bool newQso: newQsoChoice.checked
    readonly property int enteredFrequencyHz: {
        const mhz = Number(frequencyField.text.trim())
        return Number.isFinite(mhz) && mhz > 0
                ? Math.round(mhz * 1000000) : 0
    }
    readonly property bool canSubmit: !root.pending
                                      && confirmation.checked
                                      && root.imageRecordId.length > 0
                                      && root.imageMode.length > 0
                                      && (root.newQso
                                          ? callsignField.text.trim().length > 0
                                            && root.enteredFrequencyHz > 0
                                            && utcOnField.text.trim().toUpperCase().endsWith("Z")
                                          : root.selectedExistingQsoId.length > 0)

    signal completed(bool qsoCreated, string qsoId)

    title: qsTr("Log SSTV QSO")
    modal: true
    focus: true
    width: Math.min(900, parent ? parent.width - 32 : 900)
    height: Math.min(690, parent ? parent.height - 32 : 690)
    closePolicy: root.pending ? Popup.NoAutoClose
                              : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function isoUtc(value) {
        const parsed = value instanceof Date ? value : new Date(value)
        return Number.isNaN(parsed.getTime())
                ? new Date().toISOString() : parsed.toISOString()
    }

    function openForImage(record) {
        const id = String(record && record.recordId || "").trim()
        const mode = String(record && record.mode || "").trim()
        if (id.length === 0 || mode.length === 0)
            return false
        root.imageRecordId = id
        root.imageMode = mode
        root.currentRelatedQsoId = String(
                    record && record.relatedQsoId || "").trim()
        callsignField.text = String(
                    record && record.remoteCallsign || "").trim().toUpperCase()
        gridField.text = String(
                    record && record.remoteGrid || "").trim().toUpperCase()
        const frequency = Number(record && record.frequencyHz || 0)
        frequencyField.text = Number.isFinite(frequency) && frequency > 0
                ? (frequency / 1000000).toFixed(6) : ""
        utcOnField.text = root.isoUtc(
                    record && record.capturedAtUtc
                    ? record.capturedAtUtc : new Date())
        utcOffField.text = ""
        reportSentField.text = "59"
        reportReceivedField.text = "59"
        commentsField.text = ""
        existingSearch.clear()
        root.existingChoices = []
        root.selectedExistingQsoId = ""
        root.pendingRequestToken = ""
        root.pending = false
        root.feedback = ""
        root.feedbackIsError = false
        confirmation.checked = false
        newQsoChoice.checked = root.currentRelatedQsoId.length === 0
        existingQsoChoice.checked = !newQsoChoice.checked
        if (existingQsoChoice.checked)
            root.refreshExistingChoices()
        root.open()
        return true
    }

    function refreshExistingChoices() {
        if (!root.engine || !root.engine.sstvExistingQsoChoices) {
            root.existingChoices = []
            root.feedback = qsTr("The native Decodium logbook is unavailable.")
            root.feedbackIsError = true
            return
        }
        root.existingChoices = root.engine.sstvExistingQsoChoices(
                    existingSearch.text.trim(), 50) || []
        if (root.currentRelatedQsoId.length > 0) {
            for (let index = 0; index < root.existingChoices.length; ++index) {
                if (String(root.existingChoices[index].qsoId)
                        === root.currentRelatedQsoId) {
                    root.selectedExistingQsoId = root.currentRelatedQsoId
                    break
                }
            }
        }
        if (root.existingChoices.length === 0) {
            root.feedback = qsTr("No matching QSO is available in the active Decodium logbook.")
            root.feedbackIsError = false
        } else if (!root.pending) {
            root.feedback = ""
        }
    }

    function submit() {
        if (!root.canSubmit || !root.engine || !root.engine.logSstvQso)
            return false
        const request = {
            "imageRecordId": root.imageRecordId,
            "createNewQso": root.newQso,
            "existingQsoId": root.newQso ? "" : root.selectedExistingQsoId,
            "imageMode": root.imageMode
        }
        if (root.newQso) {
            request.remoteCallsign = callsignField.text.trim().toUpperCase()
            request.remoteGrid = gridField.text.trim().toUpperCase()
            request.frequencyHz = root.enteredFrequencyHz
            request.timeOnUtc = utcOnField.text.trim()
            request.timeOffUtc = utcOffField.text.trim()
            request.reportSent = reportSentField.text.trim()
            request.reportReceived = reportReceivedField.text.trim()
            request.comments = commentsField.text.trim()
        }
        const result = root.engine.logSstvQso(request) || {}
        if (!result.accepted) {
            root.feedback = String(result.error || qsTr("The SSTV QSO request was rejected."))
            root.feedbackIsError = true
            return false
        }
        root.pendingRequestToken = String(result.requestId || "")
        if (root.pendingRequestToken.length === 0) {
            root.feedback = qsTr("Decodium accepted no trackable SSTV QSO request.")
            root.feedbackIsError = true
            return false
        }
        root.pending = true
        root.feedback = qsTr("Verifying the Gallery record and committing the QSO association...")
        root.feedbackIsError = false
        return true
    }

    Connections {
        target: root.engine
        enabled: root.engine !== null
        ignoreUnknownSignals: true

        function onSstvQsoLogFinished(requestToken, imageRecordId,
                                      qsoCreated, associationStored,
                                      qsoId, error) {
            if (String(requestToken) !== root.pendingRequestToken
                    || String(imageRecordId) !== root.imageRecordId)
                return
            root.pending = false
            root.pendingRequestToken = ""
            if (associationStored) {
                root.feedback = qsoCreated
                        ? qsTr("SSTV QSO logged and image associated locally.")
                        : qsTr("SSTV image associated with the selected QSO.")
                root.feedbackIsError = false
                root.completed(qsoCreated, String(qsoId))
                root.close()
                return
            }
            root.feedback = String(error || qsTr("The SSTV QSO operation failed."))
            root.feedbackIsError = true
            if (qsoCreated && String(qsoId).length > 0) {
                // ADIF append cannot be rolled back. Move the operator to the
                // safe retry path so a second click cannot create a duplicate.
                root.currentRelatedQsoId = String(qsoId)
                existingQsoChoice.checked = true
                root.refreshExistingChoices()
            }
        }

        function onQsoLogCacheChanged() {
            if (root.visible && existingQsoChoice.checked && !root.pending)
                root.refreshExistingChoices()
        }
    }

    background: Rectangle {
        color: root.panelColor
        border.color: root.borderColor
        border.width: 1
        radius: 9
    }

    header: Rectangle {
        implicitHeight: 54
        color: "#101d26"
        border.color: root.borderColor
        border.width: 1
        Label {
            anchors.fill: parent
            anchors.leftMargin: 18
            text: root.title
            color: root.primaryTextColor
            font.pixelSize: 18
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }

    contentItem: ScrollView {
        id: contentScroll
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: contentScroll.availableWidth
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 250
                    Layout.preferredHeight: 188
                    color: "#020406"
                    border.color: root.borderColor
                    radius: 6
                    clip: true
                    Image {
                        id: qsoImagePreview
                        objectName: "sstvQsoImagePreview"
                        anchors.fill: parent
                        anchors.margins: 5
                        source: root.imageRecordId.length > 0
                                ? "image://decodium-sstv-gallery/" + root.imageRecordId
                                : ""
                        sourceSize.width: 640
                        sourceSize.height: 480
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        Accessible.name: qsTr("SSTV image being associated with the QSO")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Image mode: %1").arg(root.imageMode || qsTr("unknown"))
                        color: root.accentColor
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The image attachment remains in Decodium Gallery. ADIF receives MODE=SSTV and no local path or invented SUBMODE.")
                        color: root.secondaryTextColor
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.currentRelatedQsoId.length > 0
                        text: qsTr("This image already has a local QSO association. Choosing another existing QSO will explicitly replace it.")
                        color: "#ffb454"
                        wrapMode: Text.WordWrap
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                ButtonGroup { id: qsoKind }
                RadioButton {
                    id: newQsoChoice
                    objectName: "sstvQsoNewChoice"
                    text: qsTr("Create a new SSTV QSO")
                    checked: true
                    ButtonGroup.group: qsoKind
                    enabled: !root.pending
                }
                RadioButton {
                    id: existingQsoChoice
                    objectName: "sstvQsoExistingChoice"
                    text: qsTr("Associate with an existing QSO")
                    ButtonGroup.group: qsoKind
                    enabled: !root.pending
                    onCheckedChanged: {
                        if (checked)
                            root.refreshExistingChoices()
                    }
                }
                Item { Layout.fillWidth: true }
            }

            GridLayout {
                visible: newQsoChoice.checked
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 8
                rowSpacing: 7

                Label { text: qsTr("Callsign"); color: root.secondaryTextColor }
                TextField {
                    id: callsignField
                    objectName: "sstvQsoCallsign"
                    Layout.fillWidth: true
                    maximumLength: 64
                    enabled: !root.pending
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                    Accessible.name: qsTr("Remote callsign")
                }
                Label { text: qsTr("Grid"); color: root.secondaryTextColor }
                TextField {
                    id: gridField
                    objectName: "sstvQsoGrid"
                    Layout.fillWidth: true
                    maximumLength: 16
                    enabled: !root.pending
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                    Accessible.name: qsTr("Remote Maidenhead grid")
                }

                Label { text: qsTr("RF frequency"); color: root.secondaryTextColor }
                TextField {
                    id: frequencyField
                    objectName: "sstvQsoFrequencyMhz"
                    Layout.fillWidth: true
                    maximumLength: 20
                    enabled: !root.pending
                    placeholderText: qsTr("MHz")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Accessible.name: qsTr("RF frequency in megahertz")
                }
                Label { text: qsTr("Mode"); color: root.secondaryTextColor }
                Label {
                    Layout.fillWidth: true
                    text: root.imageMode
                    color: root.primaryTextColor
                    elide: Text.ElideRight
                }

                Label { text: qsTr("Start UTC"); color: root.secondaryTextColor }
                TextField {
                    id: utcOnField
                    objectName: "sstvQsoTimeOnUtc"
                    Layout.fillWidth: true
                    maximumLength: 32
                    enabled: !root.pending
                    placeholderText: "YYYY-MM-DDTHH:MM:SS.sssZ"
                    inputMethodHints: Qt.ImhNoPredictiveText
                    Accessible.name: qsTr("QSO start time in UTC")
                }
                Label { text: qsTr("End UTC"); color: root.secondaryTextColor }
                TextField {
                    id: utcOffField
                    objectName: "sstvQsoTimeOffUtc"
                    Layout.fillWidth: true
                    maximumLength: 32
                    enabled: !root.pending
                    placeholderText: qsTr("Optional ISO UTC")
                    inputMethodHints: Qt.ImhNoPredictiveText
                    Accessible.name: qsTr("Optional QSO end time in UTC")
                }

                Label { text: qsTr("Report sent"); color: root.secondaryTextColor }
                TextField {
                    id: reportSentField
                    objectName: "sstvQsoReportSent"
                    Layout.fillWidth: true
                    maximumLength: 32
                    enabled: !root.pending
                }
                Label { text: qsTr("Report received"); color: root.secondaryTextColor }
                TextField {
                    id: reportReceivedField
                    objectName: "sstvQsoReportReceived"
                    Layout.fillWidth: true
                    maximumLength: 32
                    enabled: !root.pending
                }
            }

            ColumnLayout {
                visible: existingQsoChoice.checked
                Layout.fillWidth: true
                spacing: 7
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: existingSearch
                        objectName: "sstvQsoExistingSearch"
                        Layout.fillWidth: true
                        maximumLength: 128
                        enabled: !root.pending
                        placeholderText: qsTr("Search callsign, grid, date, band or mode")
                        onAccepted: root.refreshExistingChoices()
                        Accessible.name: qsTr("Search active Decodium logbook")
                    }
                    Button {
                        objectName: "sstvQsoExistingSearchButton"
                        text: qsTr("Search")
                        enabled: !root.pending
                        onClicked: root.refreshExistingChoices()
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: "#05090d"
                    border.color: root.borderColor
                    radius: 5
                    ListView {
                        id: existingList
                        objectName: "sstvQsoExistingList"
                        anchors.fill: parent
                        anchors.margins: 3
                        clip: true
                        model: root.existingChoices
                        boundsBehavior: Flickable.StopAtBounds
                        delegate: ItemDelegate {
                            id: qsoRow
                            required property var modelData
                            width: existingList.width
                            highlighted: root.selectedExistingQsoId
                                         === String(qsoRow.modelData.qsoId || "")
                            onClicked: root.selectedExistingQsoId
                                       = String(qsoRow.modelData.qsoId || "")
                            contentItem: RowLayout {
                                Label {
                                    text: String(qsoRow.modelData.call || qsTr("Unknown"))
                                    color: root.primaryTextColor
                                    font.bold: true
                                    Layout.preferredWidth: 110
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: String(qsoRow.modelData.dateTime || "")
                                    color: root.secondaryTextColor
                                    Layout.preferredWidth: 155
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: String(qsoRow.modelData.mode || "")
                                          + "  " + String(qsoRow.modelData.band || "")
                                    color: root.accentColor
                                    Layout.preferredWidth: 110
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: String(qsoRow.modelData.grid || qsoRow.modelData.comment || "")
                                    color: root.secondaryTextColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: newQsoChoice.checked
                Layout.fillWidth: true
                Label {
                    text: qsTr("Comments")
                    color: root.secondaryTextColor
                }
                TextArea {
                    id: commentsField
                    objectName: "sstvQsoComments"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    enabled: !root.pending
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    placeholderText: qsTr("Optional operator comment; do not paste a local file path")
                    onTextChanged: {
                        if (text.length > 2048)
                            text = text.slice(0, 2048)
                    }
                    Accessible.name: qsTr("SSTV QSO comments")
                }
            }

            CheckBox {
                id: confirmation
                objectName: "sstvQsoExplicitConfirmation"
                Layout.fillWidth: true
                enabled: !root.pending
                text: root.newQso
                      ? qsTr("I confirm that Decodium should create this SSTV QSO and associate the Gallery image locally.")
                      : qsTr("I confirm that Decodium should associate the Gallery image with the selected existing QSO.")
                Accessible.description: qsTr("An SSTV image is never logged automatically after reception")
            }

            Label {
                Layout.fillWidth: true
                visible: root.feedback.length > 0
                text: root.feedback
                color: root.feedbackIsError ? "#ff8b94" : root.secondaryTextColor
                wrapMode: Text.WordWrap
            }
        }
    }

    footer: DialogButtonBox {
        background: Rectangle {
            color: "#101d26"
            border.color: root.borderColor
            border.width: 1
        }
        Button {
            objectName: "sstvQsoCancel"
            text: qsTr("Cancel")
            enabled: !root.pending
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
        Button {
            objectName: "sstvQsoSubmit"
            text: root.newQso ? qsTr("Log SSTV QSO") : qsTr("Associate image")
            enabled: root.canSubmit
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: root.submit()
        }
    }
}
