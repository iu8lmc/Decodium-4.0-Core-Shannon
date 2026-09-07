import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: lookupDialog

    property var service: bridge ? bridge.callsignIntelligence : null
    property string requestedCall: ""
    property string fieldValue: ""

    title: service && service.currentCall ? qsTr("Callsign intelligence — %1").arg(service.currentCall) : qsTr("Callsign intelligence")
    modal: false
    width: 560
    height: 610
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openForCall(call) {
        var nextCall = String(call || (service ? service.currentCall : "")).trim()
        if (nextCall.length > 0)
            requestedCall = nextCall
        if (service && requestedCall.length > 0)
            service.lookup(requestedCall)
        open()
        raise()
    }

    function value(key) {
        return service && service.result ? String(service.result[key] || "") : ""
    }

    function openExternalProvider(provider) {
        var call = callField.text.trim()
        if (service && call.length > 0)
            service.openProviderLookup(provider, call)
    }

    background: Rectangle {
        color: bgDeep
        border.color: secondaryCyan
        border.width: 1
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: callField
                Layout.fillWidth: true
                text: lookupDialog.requestedCall || (lookupDialog.service ? lookupDialog.service.currentCall : "")
                placeholderText: qsTr("Callsign")
                selectByMouse: true
                onAccepted: lookupButton.clicked()
            }

            Button {
                id: lookupButton
                text: lookupDialog.service && lookupDialog.service.lookupPending ? qsTr("Lookup…") : qsTr("Lookup")
                enabled: callField.text.trim().length > 0 && !(lookupDialog.service && lookupDialog.service.lookupPending)
                onClicked: {
                    lookupDialog.requestedCall = callField.text.trim()
                    if (lookupDialog.service)
                        lookupDialog.service.lookup(lookupDialog.requestedCall)
                }
            }

            Button {
                text: qsTr("Refresh")
                enabled: callField.text.trim().length > 0 && lookupDialog.service && !lookupDialog.service.lookupPending
                onClicked: {
                    lookupDialog.requestedCall = callField.text.trim()
                    if (lookupDialog.service)
                        lookupDialog.service.lookup(lookupDialog.requestedCall, true)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: glassBorder
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: lookupDialog.service ? lookupDialog.service.status : qsTr("Service non disponibile")
                color: lookupDialog.service && lookupDialog.service.lookupPending ? secondaryCyan : textSecondary
                elide: Text.ElideRight
            }

            Text {
                text: lookupDialog.service && lookupDialog.service.cacheHit ? qsTr("CACHE") : (lookupDialog.service ? lookupDialog.service.activeProvider : "")
                color: lookupDialog.service && lookupDialog.service.cacheHit ? "#8fe388" : secondaryCyan
                font.bold: true
                font.pixelSize: 11
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 7

            Text { text: qsTr("Callsign"); color: textSecondary }
            Text { text: lookupDialog.value("call") || lookupDialog.requestedCall; color: textPrimary; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: qsTr("Nome"); color: textSecondary }
            Text { text: lookupDialog.value("name") || qsTr("non disponibile"); color: textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: qsTr("QTH"); color: textSecondary }
            Text { text: lookupDialog.value("qth") || qsTr("non disponibile"); color: textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: qsTr("Grid"); color: textSecondary }
            Text { text: lookupDialog.value("grid") || qsTr("non disponibile"); color: textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: qsTr("DXCC / continente"); color: textSecondary }
            Text { text: (lookupDialog.value("dxcc") || lookupDialog.value("country") || qsTr("non disponibile")) + (lookupDialog.value("continent") ? " / " + lookupDialog.value("continent") : ""); color: textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: qsTr("CQ / ITU zone"); color: textSecondary }
            Text { text: (lookupDialog.value("cqZone") || "—") + " / " + (lookupDialog.value("ituZone") || "—"); color: textPrimary; Layout.fillWidth: true }
            Text { text: qsTr("Conferme"); color: textSecondary }
            Text { text: (lookupDialog.value("lotw") === "true" ? "LoTW " : "") + (lookupDialog.value("eqsl") === "true" ? "eQSL " : "") + (lookupDialog.value("oqrs") === "true" ? "OQRS" : "") || qsTr("nessuna indicazione"); color: textPrimary; Layout.fillWidth: true }
            Text { text: qsTr("Provider"); color: textSecondary }
            Text { text: lookupDialog.value("provider") || qsTr("fallback DXCC"); color: textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: glassBorder }

        Text {
            Layout.fillWidth: true
            text: qsTr("Lookup esterni")
            color: secondaryCyan
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Button {
                text: "QRZ"
                Layout.fillWidth: true
                enabled: lookupDialog.service && callField.text.trim().length > 0
                onClicked: lookupDialog.openExternalProvider("qrz")
            }
            Button {
                text: "FCC"
                Layout.fillWidth: true
                enabled: lookupDialog.service && callField.text.trim().length > 0
                onClicked: lookupDialog.openExternalProvider("fcc_uls")
            }
            Button {
                text: "eQSL"
                Layout.fillWidth: true
                enabled: lookupDialog.service && callField.text.trim().length > 0
                onClicked: lookupDialog.openExternalProvider("eqsl")
            }
            Button {
                text: qsTr("Club Log")
                Layout.fillWidth: true
                enabled: lookupDialog.service && callField.text.trim().length > 0
                onClicked: lookupDialog.openExternalProvider("clublog")
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: lookupDialog.service && lookupDialog.service.lastLookupAt ? qsTr("Ultimo risultato: %1").arg(lookupDialog.service.lastLookupAt) : ""
                color: textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
            }
            Button { text: qsTr("Chiudi"); onClicked: lookupDialog.close() }
        }
    }

    onOpened: {
        if (requestedCall.length === 0 && service)
            requestedCall = service.currentCall
        callField.forceActiveFocus()
        callField.selectAll()
    }
}
