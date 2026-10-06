/* DecolinkWindow - una radio lontana tramite il server Decolink.
 *
 * Il PC accanto alla radio fa girare Decolink (il gateway); da qui si entra con
 * il proprio account e si sceglie la stazione. Il ruolo lo decide il server:
 * un ascoltatore sente e basta, un operatore o il titolare sintonizza e
 * trasmette. La password resta nel deposito sicuro del sistema.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win

    readonly property var eng: (typeof appEngine !== 'undefined' ? appEngine : null)
    readonly property var lnk: eng ? eng.decolinkLink : null
    readonly property bool useRemote: !!(eng && eng.decolinkUseRemote)
    readonly property bool linked: !!(lnk && lnk.linked)
    readonly property bool loggedIn: !!(lnk && lnk.loggedIn)
    readonly property var tm: eng ? eng.themeManager : null
    readonly property color cBg:     tm ? tm.bgDeep        : "#0a0f14"
    readonly property color cPanel:  tm ? tm.panelColor    : "#111a22"
    readonly property color cBorder: tm ? tm.borderColor   : "#22303c"
    readonly property color cText:   tm ? tm.textPrimary   : "#dbe4ea"
    readonly property color cDim:    tm ? tm.textSecondary : "#7d8b96"
    readonly property color cAccent: tm ? tm.accentColor   : "#19d0ff"
    readonly property color cOk:     tm ? tm.successColor  : "#25d366"
    readonly property color cWarn:   tm ? tm.warningColor  : "#ffb84a"

    title: qsTr("Decolink - remote radio through the server")
    width: 560
    height: 640
    minimumWidth: 480
    minimumHeight: 460
    color: cBg

    property bool hasSavedPassword: false
    readonly property var stations: lnk ? lnk.stationList : []

    function loadSaved() {
        if (!eng) return
        var s = eng.decolinkSavedLogin()
        authField.text = s.authHost
        relayField.text = s.relayHost
        portField.text = String(s.relayPort)
        mailField.text = s.email
        stationField.text = s.station
        hasSavedPassword = s.hasPassword
        rememberBox.checked = s.hasPassword
        pwField.text = ""
    }
    function connectNow() {
        if (!eng) return
        eng.decolinkConnect(authField.text, relayField.text, parseInt(portField.text) || 5555,
                            mailField.text, pwField.text, stationField.text, rememberBox.checked)
        pwField.text = ""
    }
    function roleText(r) {
        if (r === "own") return qsTr("owner")
        if (r === "opr") return qsTr("operator")
        if (r === "lst") return qsTr("listener")
        return r
    }

    onVisibleChanged: if (visible) loadSaved()

    component Lbl: Text {
        color: win.cDim
        font.pixelSize: 11
    }
    component Field: TextField {
        Layout.fillWidth: true
        color: win.cText
        placeholderTextColor: win.cDim
        selectByMouse: true
        font.pixelSize: 12
        background: Rectangle {
            radius: 4
            color: "transparent"
            border.width: 1
            border.color: parent.activeFocus ? win.cAccent : win.cBorder
        }
    }
    component Btn: Rectangle {
        id: btn
        property string label: ""
        property bool danger: false
        property bool live: true
        signal clicked()
        implicitWidth: btnTxt.implicitWidth + 22
        implicitHeight: 28
        radius: 5
        opacity: btn.live ? 1.0 : 0.4
        color: btnMA.containsMouse && btn.live
               ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.16) : "transparent"
        border.width: 1
        border.color: btn.danger ? win.cWarn : win.cAccent
        Text {
            id: btnTxt
            anchors.centerIn: parent
            text: btn.label
            color: btn.danger ? win.cWarn : win.cAccent
            font.pixelSize: 11
            font.bold: true
        }
        MouseArea {
            id: btnMA
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: btn.live ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (btn.live) btn.clicked()
        }
    }
    component Panel: Rectangle {
        id: pan
        property string heading: ""
        default property alias content: body.data
        Layout.fillWidth: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        implicitHeight: col.implicitHeight + 28
        radius: 8
        color: win.cPanel
        border.color: win.cBorder
        border.width: 1
        ColumnLayout {
            id: col
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
            spacing: 8
            Text {
                text: pan.heading
                color: win.cAccent
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }
            ColumnLayout { id: body; Layout.fillWidth: true; spacing: 6 }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 12

            Item { Layout.preferredHeight: 2 }

            Panel {
                heading: qsTr("ACCOUNT")
                GridLayout {
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 6
                    Layout.fillWidth: true
                    Lbl { text: qsTr("Server") }
                    Field { id: authField; placeholderText: "decolink.ft2.it" }
                    Lbl { text: qsTr("E-mail") }
                    Field { id: mailField; inputMethodHints: Qt.ImhEmailCharactersOnly }
                    Lbl { text: qsTr("Password") }
                    Field {
                        id: pwField
                        echoMode: TextInput.Password
                        placeholderText: win.hasSavedPassword ? qsTr("(saved)") : ""
                        onAccepted: win.connectNow()
                    }
                    Lbl { text: qsTr("Station") }
                    Field {
                        id: stationField
                        placeholderText: qsTr("empty = choose from the list")
                    }
                    Lbl { text: qsTr("Relay") }
                    RowLayout {
                        Layout.fillWidth: true
                        Field { id: relayField; placeholderText: qsTr("same as server") }
                        Field { id: portField; Layout.maximumWidth: 70; placeholderText: "5555" }
                    }
                }
                CheckBox {
                    id: rememberBox
                    text: qsTr("Remember the password (system secure store)")
                    contentItem: Text {
                        text: rememberBox.text
                        color: win.cText
                        font.pixelSize: 11
                        leftPadding: rememberBox.indicator.width + 6
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                RowLayout {
                    spacing: 8
                    Btn {
                        label: win.loggedIn ? qsTr("Reconnect") : qsTr("Connect")
                        onClicked: win.connectNow()
                    }
                    Btn {
                        label: qsTr("Disconnect")
                        danger: true
                        live: win.loggedIn || win.linked
                        onClicked: {
                            if (win.useRemote) win.eng.setDecolinkUseRemote(false)
                            win.lnk.disconnectFromRelay()
                        }
                    }
                    Btn {
                        label: qsTr("Forget password")
                        danger: true
                        live: win.hasSavedPassword
                        onClicked: { win.eng.decolinkForgetPassword(); win.loadSaved() }
                    }
                }
            }

            Panel {
                heading: qsTr("STATIONS")
                visible: win.loggedIn && win.stations.length > 0
                Repeater {
                    model: win.stations
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 30
                        radius: 5
                        color: stMA.containsMouse ? Qt.rgba(win.cAccent.r, win.cAccent.g, win.cAccent.b, 0.12) : "transparent"
                        border.width: 1
                        border.color: modelData.slug === win.lnk.station ? win.cAccent : win.cBorder
                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            Text { text: modelData.name; color: win.cText; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: win.roleText(modelData.role); color: win.cDim; font.pixelSize: 10 }
                        }
                        MouseArea {
                            id: stMA
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { stationField.text = modelData.slug; win.connectNow() }
                        }
                    }
                }
            }

            Panel {
                heading: qsTr("LINK")
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: win.linked ? win.cOk : win.cWarn
                    font.pixelSize: 12
                    font.bold: true
                    text: win.lnk ? win.lnk.status : ""
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 4
                    visible: win.linked
                    Lbl { text: qsTr("Station") }
                    Text { color: win.cText; font.pixelSize: 12; text: win.lnk ? win.lnk.stationName : "" }
                    Lbl { text: qsTr("Role") }
                    Text { color: win.cText; font.pixelSize: 12; text: win.lnk ? win.roleText(win.lnk.role) : "" }
                    Lbl { text: qsTr("Frequency") }
                    Text {
                        color: win.cText; font.pixelSize: 12
                        text: win.lnk && win.lnk.frequencyHz > 0
                              ? (win.lnk.frequencyHz / 1e6).toFixed(6) + " MHz   " + win.lnk.modeName : "-"
                    }
                    Lbl { text: qsTr("S-meter") }
                    Text { color: win.cText; font.pixelSize: 12; text: win.lnk ? Math.round(win.lnk.sMeterDbm) + " dBm" : "" }
                    Lbl { text: qsTr("Round trip") }
                    Text { color: win.cText; font.pixelSize: 12; text: win.lnk ? win.lnk.latencyMs + " ms" : "" }
                    Lbl { text: qsTr("Transmit") }
                    Text {
                        font.pixelSize: 12
                        color: win.lnk && win.lnk.canTransmit ? win.cOk : win.cDim
                        text: win.lnk && win.lnk.canTransmit ? qsTr("allowed") : qsTr("listen only")
                    }
                }
            }

            Panel {
                heading: qsTr("USE AS MY RADIO")
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: win.cDim
                    font.pixelSize: 11
                    text: qsTr("Decodium decodes the audio of the remote radio and drives its frequency, mode and PTT instead of the local sound card and CAT. Turning it off brings the local ones back.")
                }
                Btn {
                    label: win.useRemote ? qsTr("Stop using the remote radio") : qsTr("Use the remote radio")
                    danger: win.useRemote
                    live: win.useRemote || win.linked
                    onClicked: win.eng.setDecolinkUseRemote(!win.useRemote)
                }
            }

            Item { Layout.preferredHeight: 10 }
        }
    }
}
