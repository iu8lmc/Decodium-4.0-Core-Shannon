// Native SSTV workspace. All DSP, storage and network work remains in C++;
// QML consumes bounded properties/models and issues explicit user commands.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win

    property var engine: null
    readonly property var theme: win.engine ? win.engine.themeManager : null
    readonly property color bg: theme ? theme.bgDeep : "#091118"
    readonly property color panel: theme ? theme.panelColor : "#111c25"
    readonly property color border: theme ? theme.borderColor : "#273946"
    readonly property color textPrimary: theme ? theme.textPrimary : "#e5edf3"
    readonly property color textSecondary: theme ? theme.textSecondary : "#91a0ab"
    readonly property color accent: theme ? theme.accentColor : "#24c9ee"
    readonly property color success: theme ? theme.successColor : "#43d17b"
    readonly property color warning: theme ? theme.warningColor : "#ffb454"
    readonly property color rxStateColor: win.engine && win.engine.sstvRxActive
                                           ? win.success : win.textSecondary

    // This is a separate top-level Window, so it does not inherit Main.qml's
    // Material palette.  Keep the window controls on the same readable dark
    // palette as the SSTV pages instead of falling back to host light controls.
    Material.theme: win.theme && win.theme.isLightTheme
                    ? Material.Light : Material.Dark
    Material.accent: win.accent
    Material.primary: win.accent
    Material.foreground: win.textPrimary
    Material.background: win.bg

    property int pageIndex: 0
    readonly property var pageLabels: [
        qsTr("Receive"),
        qsTr("Transmit Studio"),
        qsTr("Gallery"),
        qsTr("Remote Sharing"),
        qsTr("Digital HAMDRM"),
        qsTr("Settings"),
        qsTr("Diagnostics")
    ]

    title: qsTr("SSTV - Decodium")
    width: 1120
    height: 740
    minimumWidth: 1040
    minimumHeight: 700
    color: bg

    onClosing: function(close) {
        // A requested session may be in Error and therefore not "active".
        // Closing the workspace still owns an explicit stop for that session.
        if (win.engine && win.engine.sstvRxRequested)
            win.engine.stopSstvRx()
        // Never hide the dedicated SSTV TX cancellation control while the
        // shared audio/PTT coordinator still owns an analog transmission.
        if (win.engine && win.engine.sstvTxActive)
            win.engine.cancelSstvTx()
        if (win.engine && win.engine.sstvDigital)
            win.engine.sstvDigital.cancelAll()
        if (win.engine
                && typeof win.engine.leaveSstvWorkspace === "function")
            win.engine.leaveSstvWorkspace()
        close.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: win.bg

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                color: win.panel
                border.color: win.border
                border.width: 1
                radius: 10

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 12
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            text: qsTr("NATIVE SSTV")
                            color: win.accent
                            font.pixelSize: 18
                            font.bold: true
                            font.letterSpacing: 1.5
                        }
                        Label {
                            text: qsTr("Native analog SSTV and separate HAMDRM image transfer inside Decodium")
                            color: win.textSecondary
                            font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        implicitWidth: stateText.implicitWidth + 22
                        implicitHeight: 28
                        radius: 14
                        color: Qt.rgba(win.rxStateColor.r, win.rxStateColor.g,
                                       win.rxStateColor.b, 0.14)
                        border.color: win.engine && win.engine.sstvRxActive ? win.success : win.border
                        Label {
                            id: stateText
                            anchors.centerIn: parent
                            text: win.engine && win.engine.sstvRxActive ? qsTr("RX ACTIVE") : qsTr("RX IDLE")
                            color: win.engine && win.engine.sstvRxActive ? win.success : win.textSecondary
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Button {
                        text: qsTr("Close")
                        focusPolicy: Qt.StrongFocus
                        onClicked: win.close()
                        Accessible.name: qsTr("Close SSTV workspace")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 178
                    Layout.fillHeight: true
                    color: win.panel
                    border.color: win.border
                    border.width: 1
                    radius: 10

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 5

                        Repeater {
                            model: win.pageLabels
                            delegate: Button {
                                id: navButton
                                required property int index
                                required property string modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                text: modelData
                                checkable: true
                                checked: win.pageIndex === index
                                focusPolicy: Qt.StrongFocus
                                Accessible.name: modelData
                                onClicked: win.pageIndex = index

                                contentItem: Label {
                                    text: navButton.text
                                    color: navButton.checked ? win.accent : win.textPrimary
                                    font.pixelSize: 12
                                    font.bold: navButton.checked
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 10
                                }
                                background: Rectangle {
                                    radius: 7
                                    color: navButton.checked
                                           ? Qt.rgba(win.accent.r, win.accent.g, win.accent.b, 0.14)
                                           : (navButton.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
                                    border.color: navButton.checked ? win.accent : "transparent"
                                    border.width: 1
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Label {
                            Layout.fillWidth: true
                            text: win.engine && win.engine.sstvAvailable
                                  ? qsTr("Uses Decodium's selected RX audio source")
                                  : qsTr("Native SSTV is unavailable in this build")
                            color: win.engine && win.engine.sstvAvailable ? win.textSecondary : win.warning
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: win.panel
                    border.color: win.border
                    border.width: 1
                    radius: 10
                    clip: true

                    StackLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        currentIndex: win.pageIndex

                        SstvReceivePage {
                            engine: win.engine
                            panelColor: win.panel
                            borderColor: win.border
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            successColor: win.success
                            warningColor: win.warning
                        }
                        SstvTransmitPage {
                            engine: win.engine
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            borderColor: win.border
                        }
                        SstvGalleryPage {
                            engine: win.engine
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            borderColor: win.border
                            onOpenStudioRequested: win.pageIndex = 1
                            onOpenReceiveRequested: win.pageIndex = 0
                            onOpenShareRequested: function(source) {
                                sharePage.selectedUpload = source
                                win.pageIndex = 3
                            }
                        }
                        SstvSharePage {
                            id: sharePage
                            engine: win.engine
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            borderColor: win.border
                        }
                        SstvDigitalPage {
                            controller: win.engine ? win.engine.sstvDigital : null
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            borderColor: win.border
                            panelColor: win.bg
                        }
                        SstvSettingsPage {
                            engine: win.engine
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            borderColor: win.border
                        }
                        SstvDiagnosticsPage {
                            engine: win.engine
                            primaryTextColor: win.textPrimary
                            secondaryTextColor: win.textSecondary
                            accentColor: win.accent
                            borderColor: win.border
                        }
                    }
                }
            }
        }
    }
}
