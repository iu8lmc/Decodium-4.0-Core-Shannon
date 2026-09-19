/* DxPedTxPanel - DX-Pedition TX: multi-slot MSHV-style + macro TX classiche.
 * 1.0.569: sostituisce il pannello "TX Macros" nella colonna destra del
 * DxPeditionWorkspace. Riunisce in un solo posto tutto cio' che serve per
 * trasmettere durante un pileup:
 *   - toggle multi-slot (MAM multi-stream nativo, nessun Fox/Hound),
 *   - cap slot 1..10 (bridge.mamMaxStreams),
 *   - lista degli slot QSO paralleli con stato/frequenza/step/retry,
 *   - coda dei chiamanti in attesa (bridge.callerQueue),
 *   - il TxPanel classico (macro TX1..TX6, ENABLE TX, HALT, TUNE) incorporato.
 * Il logging e' automatico per slot (mamLogSlot -> logQsoNow): stesso modello
 * di MSHV, senza prompt e senza modalita' Fox/Hound.
 * NB: niente `property var bridge` senza default -> shadowa il context global
 * quando il componente e' caricato da un Loader async (lezione 1.0.330).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: dxTx

    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)
    property var engine: (typeof appEngine !== 'undefined' ? appEngine : null)
    // Propagato al TxPanel incorporato: e' lui che possiede il popup di
    // conferma log del QSO singolo (il multi-slot logga da solo, senza prompt).
    property bool handleLogPrompt: true
    property bool macrosOpen: true

    signal mamWindowRequested()

    // ---- token tema (stessi del DxPeditionWorkspace) -----------------------
    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:  tm ? tm.accentColor   : "#19ff88"
    readonly property color cPanel:   tm ? tm.panelColor    : "#0d1310"
    readonly property color cBorder:  tm ? tm.borderColor   : "#1f2a22"
    readonly property color cText:    tm ? tm.textPrimary   : "#d6dcd8"
    readonly property color cTextDim: tm ? tm.textSecondary : "#6c7872"
    readonly property color cPile:    tm ? tm.pileColor     : "#66e6ff"
    readonly property color cTx:      tm ? tm.txColor       : "#ff7a5c"
    readonly property color cWarn:    tm ? tm.warningColor  : "#ffb84a"
    readonly property color cHot:     tm ? tm.errorColor    : "#ff5466"

    // ---- stato dal bridge --------------------------------------------------
    readonly property bool msOn:        bridge ? bridge.mamMultiStream : false
    readonly property int  maxSlots:    bridge ? bridge.mamMaxStreams : 3
    readonly property var  activeSlots: bridge ? bridge.mamActiveSlots : []
    readonly property int  queueCount:  bridge ? bridge.mamQueueCount : 0
    readonly property var  queueList:   bridge ? bridge.callerQueue : []
    readonly property bool modeOk:      bridge ? bridge.mamModeSupported : false
    readonly property bool modeExp:     bridge ? bridge.mamModeExperimental : false
    readonly property bool txOn:        bridge ? bridge.txEnabled : false
    readonly property bool mamOn:       bridge ? bridge.multiAnswerMode : false
    readonly property bool autoCqOn:    bridge ? bridge.autoCqRepeat : false
    readonly property bool cqSlots:     bridge ? bridge.mamCqSlots : false
    readonly property bool answering:   mamOn || autoCqOn
    readonly property bool running:     msOn && modeOk && txOn && answering
    // Vero quando gli slot liberi stanno davvero chiamando CQ in parallelo.
    readonly property bool cqActive:    running && cqSlots && autoCqOn
    readonly property int  freeSlots:   Math.max(0, maxSlots - activeSlots.length)

    // Righe della lista slot: gli slot attivi, riempiti fino a maxSlots con
    // segnaposto vuoti, cosi' l'operatore vede quante posizioni ha aperto.
    property var slotRows: []
    function rebuildRows() {
        var out = []
        var src = dxTx.activeSlots || []
        var n = Math.max(1, dxTx.maxSlots)
        for (var i = 0; i < n; ++i)
            out.push(i < src.length ? src[i] : null)
        dxTx.slotRows = out
    }
    onActiveSlotsChanged: rebuildRows()
    onMaxSlotsChanged: rebuildRows()
    Component.onCompleted: rebuildRows()

    function setMaxSlots(v) {
        if (!bridge)
            return
        var clamped = Math.max(1, Math.min(10, v))
        if (bridge.mamMaxStreams !== clamped)
            bridge.mamMaxStreams = clamped
    }

    // Un click solo per mettere la stazione in aria come DX-pedition: multi-slot
    // + TX abilitato + MAM (risposta automatica) + AUTO CQ, che e' cio' che fa
    // partire i CQ paralleli sugli slot ancora liberi.
    function armPileup() {
        if (!bridge)
            return
        if (!bridge.mamMultiStream)
            bridge.mamMultiStream = true
        if (!bridge.txEnabled)
            bridge.txEnabled = true
        if (!bridge.multiAnswerMode)
            bridge.multiAnswerMode = true
        if (!bridge.autoCqRepeat)
            bridge.autoCqRepeat = true
    }

    function statusLine() {
        if (!bridge)
            return ""
        if (!msOn)
            return qsTr("Multi-slot OFF - serial MAM, one caller at a time")
        if (!modeOk)
            return qsTr("Mode %1: multi-slot not available (needs FT8, FT4 or FT2)").arg(bridge.mode)
        if (!txOn)
            return qsTr("TX disabled - press ARM or ENABLE TX")
        if (!answering)
            return qsTr("MAM or AUTO CQ is required to answer callers")
        if (cqActive)
            return qsTr("On air - %1 QSO + %2 parallel CQ on %3 slots, %4 queued")
                     .arg(activeSlots.length).arg(freeSlots).arg(maxSlots).arg(queueCount)
        return qsTr("On air - %1 of %2 slots busy, %3 queued")
                 .arg(activeSlots.length).arg(maxSlots).arg(queueCount)
    }

    function slotCall(m) { return m && m.call ? String(m.call) : "" }

    // ---- pulsante tattico riusabile ---------------------------------------
    component TacBtn: Rectangle {
        id: tb
        property string label: ""
        property bool active: false
        property bool danger: false
        property bool live: true
        property color accent: dxTx.cAccent
        signal clicked()
        implicitWidth: tbTxt.implicitWidth + 14
        implicitHeight: 22
        radius: 4
        opacity: tb.live ? 1.0 : 0.35
        color: tb.active ? Qt.rgba(tb.accent.r, tb.accent.g, tb.accent.b, 0.20)
                         : (tbMa.containsMouse && tb.live ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
        border.width: 1
        border.color: tb.danger ? dxTx.cHot : (tb.active ? tb.accent : dxTx.cBorder)
        Text {
            id: tbTxt
            anchors.centerIn: parent
            text: tb.label
            color: tb.danger ? dxTx.cHot : (tb.active ? tb.accent : dxTx.cText)
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.8
        }
        MouseArea {
            id: tbMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: tb.live ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (tb.live) tb.clicked()
        }
    }

    // =======================================================================
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ---- RIGA 1: multi-slot, cap 1-10, MAM / AUTO CQ ------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TacBtn {
                label: qsTr("MULTI-SLOT")
                active: dxTx.msOn
                onClicked: if (dxTx.bridge) dxTx.bridge.mamMultiStream = !dxTx.msOn
            }

            // Stepper 1..10 (niente SpinBox: qui serve un click secco, e i
            // campi editabili con validator rifiutano la digitazione).
            Rectangle {
                Layout.preferredHeight: 22
                implicitWidth: stepRow.implicitWidth + 8
                radius: 4
                color: "transparent"
                border.width: 1
                border.color: dxTx.cBorder
                RowLayout {
                    id: stepRow
                    anchors.centerIn: parent
                    spacing: 2
                    TacBtn {
                        label: "−"
                        implicitWidth: 20
                        live: dxTx.maxSlots > 1
                        onClicked: dxTx.setMaxSlots(dxTx.maxSlots - 1)
                    }
                    Text {
                        text: String(dxTx.maxSlots)
                        color: dxTx.cAccent
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        Layout.preferredWidth: 22
                    }
                    TacBtn {
                        label: "+"
                        implicitWidth: 20
                        live: dxTx.maxSlots < 10
                        onClicked: dxTx.setMaxSlots(dxTx.maxSlots + 1)
                    }
                }
            }
            Text {
                text: qsTr("SLOT")
                color: dxTx.cTextDim
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }

            Item { Layout.fillWidth: true }

            TacBtn {
                label: qsTr("MAM")
                active: dxTx.mamOn
                onClicked: if (dxTx.bridge) dxTx.bridge.multiAnswerMode = !dxTx.mamOn
            }
            TacBtn {
                label: qsTr("AUTO CQ")
                active: dxTx.autoCqOn
                onClicked: if (dxTx.bridge) dxTx.bridge.autoCqRepeat = !dxTx.autoCqOn
            }
            TacBtn {
                label: qsTr("CQ MULTI")
                active: dxTx.cqSlots
                onClicked: if (dxTx.bridge) dxTx.bridge.mamCqSlots = !dxTx.cqSlots
            }
            TacBtn {
                label: qsTr("ARM")
                accent: dxTx.cPile
                active: dxTx.running
                onClicked: dxTx.armPileup()
            }
        }

        // ---- avviso FT2 non validato on-air -------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 22 : 0
            visible: dxTx.msOn && dxTx.modeExp
            radius: 4
            color: Qt.rgba(dxTx.cWarn.r, dxTx.cWarn.g, dxTx.cWarn.b, 0.12)
            border.width: 1
            border.color: dxTx.cWarn
            Text {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                verticalAlignment: Text.AlignVCenter
                text: qsTr("FT2: multi-slot never validated on air - if QSOs do not close, go back to 1 slot")
                color: dxTx.cWarn
                font.pixelSize: 10
                elide: Text.ElideRight
            }
        }

        // ---- riga di stato -------------------------------------------------
        Text {
            Layout.fillWidth: true
            text: dxTx.statusLine()
            color: dxTx.running ? dxTx.cAccent : dxTx.cTextDim
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        // ---- lista slot ----------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.25)
            border.width: 1
            border.color: dxTx.cBorder
            clip: true

            ListView {
                id: slotView
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                model: dxTx.slotRows
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: slotRow
                    required property var modelData
                    required property int index
                    readonly property bool used: !!slotRow.modelData
                    readonly property int prog: slotRow.used ? Number(slotRow.modelData.progress || 0) : 0
                    width: ListView.view ? ListView.view.width : 0
                    height: 24
                    color: slotRow.index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.03)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        Text {
                            text: String(slotRow.index + 1)
                            color: dxTx.cTextDim
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 14
                        }
                        Text {
                            // Slot libero: se i CQ paralleli sono attivi quella
                            // posizione non e' ferma, sta chiamando CQ.
                            text: slotRow.used ? dxTx.slotCall(slotRow.modelData)
                                               : (dxTx.cqActive ? qsTr("CQ") : qsTr("free"))
                            color: slotRow.used ? dxTx.cAccent
                                                : (dxTx.cqActive ? dxTx.cPile : dxTx.cTextDim)
                            font.pixelSize: 11
                            font.bold: slotRow.used || dxTx.cqActive
                            font.italic: !slotRow.used && !dxTx.cqActive
                            elide: Text.ElideRight
                            Layout.preferredWidth: 88
                        }
                        Text {
                            text: slotRow.used ? String(slotRow.modelData.grid || "") : ""
                            color: dxTx.cTextDim
                            font.pixelSize: 10
                            Layout.preferredWidth: 36
                        }
                        Text {
                            text: slotRow.used ? String(slotRow.modelData.freq) + " Hz" : ""
                            color: dxTx.cPile
                            font.pixelSize: 10
                            Layout.preferredWidth: 56
                        }
                        Text {
                            text: slotRow.used ? "TX" + String(slotRow.modelData.tx) : ""
                            color: dxTx.cTx
                            font.pixelSize: 10
                            font.bold: true
                            Layout.preferredWidth: 30
                        }
                        Text {
                            text: {
                                if (!slotRow.used)
                                    return ""
                                var s = Number(slotRow.modelData.snr)
                                return s === 127 ? "—" : (s > 0 ? "+" + s : String(s))
                            }
                            color: dxTx.cText
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 26
                        }
                        Text {
                            text: (slotRow.used && Number(slotRow.modelData.retry) > 1)
                                    ? "×" + String(slotRow.modelData.retry) : ""
                            color: dxTx.cWarn
                            font.pixelSize: 10
                            Layout.preferredWidth: 22
                        }

                        // Avanzamento QSO: 1=CQ 2=REPLY 3=REPORT 4=ROGER 5=73
                        Row {
                            spacing: 3
                            Layout.alignment: Qt.AlignVCenter
                            Repeater {
                                model: 5
                                delegate: Rectangle {
                                    required property int index
                                    width: 8
                                    height: 4
                                    radius: 2
                                    color: (slotRow.used && slotRow.prog > index) ? dxTx.cAccent : dxTx.cBorder
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        TacBtn {
                            label: qsTr("LOG")
                            visible: slotRow.used
                            implicitHeight: 18
                            onClicked: if (dxTx.bridge) dxTx.bridge.mamLogSlotNow(dxTx.slotCall(slotRow.modelData))
                        }
                        TacBtn {
                            label: "✕"
                            danger: true
                            visible: slotRow.used
                            implicitWidth: 20
                            implicitHeight: 18
                            onClicked: if (dxTx.bridge) dxTx.bridge.mamDropSlot(dxTx.slotCall(slotRow.modelData))
                        }
                    }
                }
            }
        }

        // ---- coda chiamanti + comandi d'emergenza -------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("QUEUE %1").arg(dxTx.queueCount)
                color: dxTx.queueCount > 0 ? dxTx.cPile : dxTx.cTextDim
                font.pixelSize: 10
                font.bold: dxTx.queueCount > 0
            }
            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                contentWidth: queueRow.implicitWidth
                clip: true
                flickableDirection: Flickable.HorizontalFlick
                Row {
                    id: queueRow
                    spacing: 4
                    Repeater {
                        model: dxTx.queueList
                        delegate: Rectangle {
                            id: qChip
                            required property var modelData
                            readonly property string qCall: String(qChip.modelData || "").split(" ")[0]
                            width: qTxt.implicitWidth + 10
                            height: 18
                            radius: 3
                            color: Qt.rgba(dxTx.cPile.r, dxTx.cPile.g, dxTx.cPile.b, 0.12)
                            border.width: 1
                            border.color: Qt.rgba(dxTx.cPile.r, dxTx.cPile.g, dxTx.cPile.b, 0.5)
                            Text {
                                id: qTxt
                                anchors.centerIn: parent
                                text: qChip.qCall
                                color: dxTx.cPile
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
            TacBtn {
                label: qsTr("CLEAR QUEUE")
                live: dxTx.queueCount > 0
                onClicked: if (dxTx.bridge) dxTx.bridge.mamClearQueue()
            }
            TacBtn {
                label: qsTr("CLEAR SLOTS")
                live: dxTx.activeSlots.length > 0
                danger: true
                onClicked: if (dxTx.bridge) dxTx.bridge.mamClearSlots()
            }
            TacBtn {
                label: qsTr("HALT")
                danger: true
                onClicked: if (dxTx.bridge) dxTx.bridge.haltWithReason("dxped-multislot-halt")
            }
        }

        // ---- separatore + toggle macro ------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: dxTx.cBorder }
            TacBtn {
                label: dxTx.macrosOpen ? qsTr("TX MACROS ▴") : qsTr("TX MACROS ▾")
                active: dxTx.macrosOpen
                onClicked: dxTx.macrosOpen = !dxTx.macrosOpen
            }
        }

        // ---- TxPanel classico incorporato ---------------------------------
        // Quando le macro sono chiuse l'altezza va a 0 ma l'item resta
        // visible:true di proposito: openLogPromptFromBridge() del TxPanel si
        // arma solo se il pannello e' visible, e con visible:false la conferma
        // di log del QSO singolo sparirebbe.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: dxTx.macrosOpen
                ? Math.max(120, Math.min(300, dxTx.height * 0.40))
                : 0
            clip: true
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 120 } }

            Flickable {
                anchors.fill: parent
                contentWidth: width
                contentHeight: txLoader.item ? txLoader.item.implicitHeight : 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Loader {
                    id: txLoader
                    width: parent.width
                    active: dxTx.engine !== null && dxTx.engine !== undefined
                    sourceComponent: txComp
                }
            }
        }
    }

    Component {
        id: txComp
        TxPanel {
            engine: dxTx.engine
            handleLogPrompt: dxTx.handleLogPrompt
            showAsyncIcon: false
            showBandBar: false
            onMamWindowRequested: dxTx.mamWindowRequested()
        }
    }
}
