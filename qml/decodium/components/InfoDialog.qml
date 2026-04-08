/* Decodium Info Dialog
 * Credits, History, Contacts, Guide, Links
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: infoDialog
    width: 680
    height: 580
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: parent

    property int currentTab: 0

    // Color palette
    property color bgDeep: bridge.themeManager.bgDeep
    property color bgMedium: bridge.themeManager.bgMedium
    property color bgLight: bridge.themeManager.bgLight
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color accentOrange: bridge.themeManager.warningColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color glassBorder: bridge.themeManager.glassBorder

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan
        border.width: 2
        radius: 16
    }

    header: Rectangle {
        width: parent.width
        height: 50
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16

            Text {
                text: "DECODIUM INFO"
                font.pixelSize: 18
                font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            // Close button
            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: closeMA.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.2) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "X"
                    font.pixelSize: 14
                    font.bold: true
                    color: textPrimary
                }

                MouseArea {
                    id: closeMA
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: infoDialog.close()
                }
            }
        }

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: glassBorder
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Tab Bar
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            currentIndex: infoDialog.currentTab
            background: Rectangle { color: "transparent" }

            Repeater {
                model: ["About", "Storia", "Contatti", "Guida", "Link"]

                TabButton {
                    text: modelData
                    width: implicitWidth
                    font.pixelSize: 12

                    background: Rectangle {
                        color: tabBar.currentIndex === index ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) :
                               (hovered ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,0.1) : "transparent")
                        border.color: tabBar.currentIndex === index ? secondaryCyan : "transparent"
                        border.width: 1
                        radius: 6

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: tabBar.currentIndex === index ? secondaryCyan : textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: glassBorder
        }

        // Tab Content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // ===== TAB 0: ABOUT =====
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 20

                    Item { height: 20 }

                    // Logo
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 120
                        height: 120
                        radius: 60
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#1a73e8" }
                            GradientStop { position: 1.0; color: "#00bcd4" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "D"
                            font.pixelSize: 72
                            font.bold: true
                            color: "white"
                        }
                    }

                    // Title
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "DECODIUM"
                        font.pixelSize: 32
                        font.bold: true
                        color: textPrimary
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "4.0 Core Shannon"
                        font.pixelSize: 16
                        color: secondaryCyan
                    }

                    // Description
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 500
                        text: "Advanced Digital Mode Decoder for Amateur Radio\nFT8 / FT4 / FT2 / Q65 / JT65 / MSK144 / MSK40 / JTMS / FSK441"
                        font.pixelSize: 13
                        color: textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Item { height: 10 }

                    // Credits Box
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 450
                        implicitHeight: creditsCol.height + 30
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
                        border.color: glassBorder
                        radius: 12

                        ColumnLayout {
                            id: creditsCol
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "CREDITS"
                                font.pixelSize: 14
                                font.bold: true
                                color: secondaryCyan
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Decodium by IU8LMC"
                                font.pixelSize: 12
                                color: textPrimary
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Qt6 Modern UI Port by IU8LMC"
                                font.pixelSize: 12
                                color: textPrimary
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "WSJT-X Algorithms by K1JT (Joe Taylor)"
                                font.pixelSize: 12
                                color: textPrimary
                            }

                            Item { height: 5 }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Copyright 2024-2026 - Open Source GPL"
                                font.pixelSize: 11
                                color: textSecondary
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }

            // ===== TAB 1: STORIA =====
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                    Item { height: 10 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "LA STORIA DI DECODIUM"
                        font.pixelSize: 16
                        font.bold: true
                        color: secondaryCyan
                    }

                    // Timeline
                    Repeater {
                        model: [
                            { year: "2018", title: "WSJT-X / Predecessori", desc: "LZ2HV (Christo) sviluppa un decoder multi-mode per FT8, JT65 e altri modi digitali basato su WSJT-X di K1JT. Nasce il supporto per decodifica multi-stream." },
                            { year: "2020", title: "Decoder Multi-Mode", desc: "Evoluzione con supporto per nuovi modi come FT4 e Q65. La community cresce e contribuisce all'ecosistema WSJT." },
                            { year: "2024", title: "Fork Qt6", desc: "IU8LMC inizia il porting a Qt6 con interfaccia moderna QML. Nasce il progetto Decodium con UI glass-morphism." },
                            { year: "2025", title: "Decodium 1.0", desc: "Prima release pubblica con Neural Sync, Coherent Averaging, interfaccia moderna, CAT control integrato e PSK Reporter." },
                            { year: "2025", title: "Decodium 1.5", desc: "Release con miglioramenti decoder, nuova UI, gestione macro, finestra astronomica." },
                            { year: "2026", title: "Decodium 2.0", desc: "Versione ottimizzata per computer datati: bitshift, memcpy, magnitude squared. Fix CAT Yaesu con DTR/RTS." },
                            { year: "2026", title: "Decodium 4.0 Core Shannon", desc: "Refactoring architetturale completo. Nuova UI modulare, integrazione PSK Reporter, Cloudlog, DX Cluster live, ADIF, LotW. Named after Claude Shannon, padre della teoria dell'informazione." }
                        ]

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 580
                            implicitHeight: timelineCol.height + 20
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                            border.color: glassBorder
                            radius: 10

                            RowLayout {
                                id: timelineCol
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 16

                                // Year badge
                                Rectangle {
                                    Layout.preferredWidth: 60
                                    Layout.preferredHeight: 28
                                    color: primaryBlue
                                    radius: 6

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.year
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: "white"
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: modelData.title
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: accentGreen
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.desc
                                        font.pixelSize: 11
                                        color: textSecondary
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }

            // ===== TAB 2: CONTATTI =====
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 20

                    Item { height: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "CONTATTI & FEEDBACK"
                        font.pixelSize: 16
                        font.bold: true
                        color: secondaryCyan
                    }

                    // Contact Info
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 400
                        implicitHeight: contactCol.height + 30
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
                        border.color: glassBorder
                        radius: 12

                        ColumnLayout {
                            id: contactCol
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Sviluppatore: IU8LMC"
                                font.pixelSize: 14
                                color: textPrimary
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 250
                                height: 36
                                color: emailMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3) : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
                                border.color: secondaryCyan
                                radius: 8

                                Text {
                                    anchors.centerIn: parent
                                    text: "iu8lmc@gmail.com"
                                    font.pixelSize: 14
                                    color: secondaryCyan
                                }

                                MouseArea {
                                    id: emailMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally("mailto:iu8lmc@gmail.com")
                                }

                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                        }
                    }

                    // Feedback Form
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 500
                        implicitHeight: feedbackCol.height + 30
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
                        border.color: glassBorder
                        radius: 12

                        ColumnLayout {
                            id: feedbackCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 15
                            width: parent.width - 40
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "INVIA FEEDBACK"
                                font.pixelSize: 14
                                font.bold: true
                                color: accentOrange
                            }

                            TextField {
                                id: feedbackSubject
                                Layout.fillWidth: true
                                placeholderText: "Oggetto..."
                                font.pixelSize: 12
                                color: textPrimary
                                placeholderTextColor: textSecondary
                                background: Rectangle {
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                                    border.color: feedbackSubject.activeFocus ? secondaryCyan : glassBorder
                                    radius: 6
                                }
                            }

                            TextArea {
                                id: feedbackMessage
                                Layout.fillWidth: true
                                Layout.preferredHeight: 100
                                placeholderText: "Scrivi il tuo messaggio, suggerimento o segnalazione bug..."
                                font.pixelSize: 12
                                color: textPrimary
                                placeholderTextColor: textSecondary
                                wrapMode: TextArea.Wrap
                                background: Rectangle {
                                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                                    border.color: feedbackMessage.activeFocus ? secondaryCyan : glassBorder
                                    radius: 6
                                }
                            }

                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Invia Feedback"
                                font.pixelSize: 12

                                background: Rectangle {
                                    implicitWidth: 150
                                    implicitHeight: 36
                                    color: parent.hovered ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.4) : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2)
                                    border.color: accentGreen
                                    radius: 8

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font: parent.font
                                    color: accentGreen
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    var subject = encodeURIComponent(feedbackSubject.text || "Decodium Feedback")
                                    var body = encodeURIComponent(feedbackMessage.text + "\n\n--\nDecodium 4.0 Core Shannon\nCallsign: " + (bridge ? bridge.callsign : "N/A"))
                                    Qt.openUrlExternally("mailto:iu8lmc@gmail.com?subject=" + subject + "&body=" + body)
                                }
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }

            // ===== TAB 3: GUIDA =====
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                    Item { height: 10 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "GUIDA RAPIDA"
                        font.pixelSize: 16
                        font.bold: true
                        color: secondaryCyan
                    }

                    // Quick Start
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 580
                        implicitHeight: quickStartCol.height + 30
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 12

                        ColumnLayout {
                            id: quickStartCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 15
                            width: parent.width - 30
                            spacing: 10

                            Text {
                                text: "QUICK START"
                                font.pixelSize: 13
                                font.bold: true
                                color: accentGreen
                            }

                            Repeater {
                                model: [
                                    "1. Configura il tuo callsign e grid in Settings > Station",
                                    "2. Seleziona dispositivi audio in Settings > Audio",
                                    "3. Configura CAT control per la tua radio (opzionale)",
                                    "4. Seleziona modo (FT8, FT4, etc.) e banda",
                                    "5. Clicca Monitor per iniziare a decodificare",
                                    "6. Doppio click su un decode per rispondere"
                                ]

                                Text {
                                    text: modelData
                                    font.pixelSize: 11
                                    color: textSecondary
                                }
                            }
                        }
                    }

                    // Keyboard Shortcuts
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 580
                        implicitHeight: shortcutsCol.height + 30
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 12

                        ColumnLayout {
                            id: shortcutsCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 15
                            width: parent.width - 30
                            spacing: 8

                            Text {
                                text: "SHORTCUTS TASTIERA"
                                font.pixelSize: 13
                                font.bold: true
                                color: accentOrange
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: 40
                                rowSpacing: 6

                                Repeater {
                                    model: [
                                        { key: "F1", action: "Monitor On/Off" },
                                        { key: "F2", action: "Enable TX" },
                                        { key: "F3", action: "Auto Sequence" },
                                        { key: "F4", action: "Log QSO" },
                                        { key: "ESC", action: "Halt TX" },
                                        { key: "Ctrl+S", action: "Apri Settings" },
                                        { key: "Ctrl+L", action: "Apri Log" },
                                        { key: "Ctrl+M", action: "Apri Macro" }
                                    ]

                                    RowLayout {
                                        spacing: 10

                                        Rectangle {
                                            width: 60
                                            height: 24
                                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2)
                                            border.color: secondaryCyan
                                            radius: 4

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.key
                                                font.pixelSize: 11
                                                font.bold: true
                                                color: secondaryCyan
                                            }
                                        }

                                        Text {
                                            text: modelData.action
                                            font.pixelSize: 11
                                            color: textSecondary
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Tips
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 580
                        implicitHeight: tipsCol.height + 30
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                        border.color: glassBorder
                        radius: 12

                        ColumnLayout {
                            id: tipsCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 15
                            width: parent.width - 30
                            spacing: 10

                            Text {
                                text: "TIPS OPERATIVI"
                                font.pixelSize: 13
                                font.bold: true
                                color: primaryBlue
                            }

                            Repeater {
                                model: [
                                    "Usa Deep Search per segnali deboli (sotto -20dB)",
                                    "Attiva AP Decode per migliorare la decodifica",
                                    "Neural Sync aiuta con segnali al limite",
                                    "MAM (Multi-Answer) per contest e pileup",
                                    "Mantieni TX power sotto 60% per evitare ALC"
                                ]

                                Text {
                                    text: "- " + modelData
                                    font.pixelSize: 11
                                    color: textSecondary
                                }
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }

            // ===== TAB 4: LINK =====
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                    Item { height: 20 }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "LINK UTILI"
                        font.pixelSize: 16
                        font.bold: true
                        color: secondaryCyan
                    }

                    // Links Grid
                    GridLayout {
                        Layout.alignment: Qt.AlignHCenter
                        columns: 2
                        columnSpacing: 20
                        rowSpacing: 12

                        Repeater {
                            model: [
                                { name: "PSK Reporter", url: "https://pskreporter.info", desc: "Mappa propagazione real-time" },
                                { name: "QRZ.com", url: "https://www.qrz.com", desc: "Database callsign mondiale" },
                                { name: "LOTW", url: "https://lotw.arrl.org", desc: "Logbook of The World ARRL" },
                                { name: "Club Log", url: "https://clublog.org", desc: "Log online e statistiche" },
                                { name: "WSJT-X", url: "https://wsjt.sourceforge.io", desc: "Software originale K1JT" },
                                { name: "Hamlib", url: "https://hamlib.github.io", desc: "Libreria CAT control" },
                                { name: "DX Summit", url: "https://dxsummit.fi", desc: "DX Cluster spots" },
                                { name: "VOACAP", url: "https://www.voacap.com", desc: "Previsioni propagazione HF" }
                            ]

                            Rectangle {
                                width: 260
                                height: 70
                                color: linkMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15) : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
                                border.color: linkMA.containsMouse ? secondaryCyan : glassBorder
                                radius: 10

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 4

                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: modelData.name
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: secondaryCyan
                                    }

                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: modelData.desc
                                        font.pixelSize: 10
                                        color: textSecondary
                                    }
                                }

                                MouseArea {
                                    id: linkMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally(modelData.url)
                                }

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }
        }
    }

    onCurrentTabChanged: {
        tabBar.currentIndex = currentTab
    }
}
