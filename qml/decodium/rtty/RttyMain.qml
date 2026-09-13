// DecoRTTY — main window.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import "."

ApplicationWindow {
    id: window

    // Sized against the screen rather than assumed: on a 200% display the
    // logical desktop is half what the pixel count suggests, and a window that
    // does not fit gets squeezed until the layout collapses on itself.
    width: Math.min(1360, Screen.desktopAvailableWidth - 60)
    height: Math.min(880, Screen.desktopAvailableHeight - 60)
    // A 200% display leaves roughly 960x490 logical points to work with, so the
    // floor has to fit that rather than what a desktop-sized screen offers.
    minimumWidth: 760
    minimumHeight: 440
    visible: false
    title: qsTr("RTTY") + " — " + (radio.connected ? radio.radioName : qsTr("nessuna radio"))
    color: Theme.bgDeep

    // Il tema dei controlli va rimesso qui. Le proprieta' Material si
    // attaccano all'albero di UNA finestra e non passano a un'altra: Decodium
    // le imposta sulla sua, ma questa e' una finestra a se', e senza queste
    // quattro righe ogni ComboBox, Slider e ScrollBar qui dentro resta col
    // tema chiaro di serie — le barre bianche su fondo scuro che si vedevano.
    // Segue il tema di Decodium, cosi' chiaro e scuro cambiano insieme.
    readonly property var tm: (typeof bridge !== 'undefined' && bridge) ? bridge.themeManager : null
    Material.theme: (tm && tm.isLightTheme) ? Material.Light : Material.Dark
    Material.accent: tm ? tm.accentColor : Theme.accent
    Material.foreground: tm ? tm.textPrimary : Theme.textPrimary
    Material.background: tm ? tm.bgDeep : Theme.bgDeep

    // Il decodificatore RTTY riceve l'audio solo mentre questa finestra e'
    // aperta. Si lega alla visibilita' e non alla distruzione: chi chiude con
    // la crocetta la nasconde soltanto, e il demodulatore resterebbe a girare
    // per nessuno rubando tempo alla decodifica dei modi digitali.
    onVisibleChanged: {
        if (!bridge)
            return
        // Tutte le strade che rendono visibile questa finestra sono un cambio
        // di modo, anche un eventuale show() diretto futuro che non passi dal
        // menu principale.
        if (visible && bridge.mode !== "RTTY")
            bridge.mode = "RTTY"
        bridge.rttyInAscolto = visible
    }
    Component.onDestruction: if (bridge) bridge.rttyInAscolto = false

    // Cambiando banda il decodificatore deve dimenticare quello che ha
    // imparato: il fondo di banda e l'auto-centratura si appoggiano a cio' che
    // hanno visto, e cio' che hanno visto era un'altra banda.
    //
    // Si guarda la banda vera invece dei pulsanti di questa finestra, che non
    // ci sono piu': cosi' l'azzeramento arriva da qualunque parte venga il
    // cambio — il selettore di Decodium, il menu, o la manopola della radio,
    // che prima non lo faceva scattare affatto.
    property int ultimaBanda: -2
    Connections {
        target: radio
        function onSliceChanged() {
            if (radio.currentBand !== window.ultimaBanda) {
                window.ultimaBanda = radio.currentBand
                rtty.forgetBand()
            }
        }
    }


    // A single quiet gradient behind everything, so the glass panels have
    // something to sit on rather than floating on flat black.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgDeep }
            GradientStop { position: 0.55; color: Qt.darker(Theme.bgMedium, 1.25) }
            GradientStop { position: 1.0; color: Theme.bgDeep }
        }
    }

    header: HeaderBar {
        onSettingsRequested: setupDialog.open()
        onLogRequested: logDialog.open()
    }

    // Il log si apre anche da tastiera: durante un contest la mano non lascia
    // la tastiera per andare a cercare un pulsante.
    Shortcut { sequence: "Ctrl+L"; onActivated: logDialog.open() }

    footer: StatusBar {}

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // ── spettro e testo a sinistra, strumenti a destra ──────────────
        //
        // La colonna degli strumenti corre per tutta l'altezza invece di stare
        // sotto il waterfall. Schiacciata li' sotto le restavano poche decine di
        // pixel, e i comandi del decodificatore si leggevano a meta': un elenco
        // tagliato, senza modo evidente di arrivare al resto.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // Waterfall e testo ricevuto separati da una maniglia: quanto
            // spazio dare all'uno o all'altro lo decide l'operatore, e cambia
            // di continuo — si guarda il waterfall mentre si cerca qualcuno, si
            // guarda il testo mentre si copia. E' lo stesso meccanismo con cui
            // Decodium divide il suo waterfall dai pannelli di decodifica, con
            // la stessa maniglia che si accende quando ci passi sopra.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 320
                spacing: 6

            // I modi della radio sopra il waterfall. Le bande no: quelle si
            // scelgono da Decodium, che e' l'unico posto dove si sceglie una
            // frequenza — due strade per la stessa cosa prima o poi dicono
            // cose diverse, e il righello di sintonia di DecoRTTY sarebbe
            // stata la terza.
            BandBar {
                Layout.fillWidth: true
            }

            // Il ponte di sintonia, appena sopra il waterfall: girando la
            // manopola si guarda questo, non i numeri. Traversa dritta e
            // colonne alte vuol dire segnale a posto.
            TuningBridge {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Vertical

                handle: Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    color: SplitHandle.hovered || SplitHandle.pressed
                           ? Theme.secondary : Theme.glassBorder
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 50
                        height: 3
                        radius: 1
                        color: parent.color
                    }
                }

                // Il waterfall e' quello di Decodium, con il suo audio: RTTY
                // decodifica esattamente cio' che il waterfall mostra, perche'
                // ora e' la stessa sorgente. Non se ne tengono due — uno solo
                // da mantenere, e la stessa resa in tutti i modi. Il
                // WaterfallPanel dell'originale non serve piu'.
                Loader {
                    SplitView.preferredHeight: Math.max(150, window.height * 0.26)
                    SplitView.minimumHeight: 110
                    source: "../components/Waterfall.qml"
                    // Vivo solo a finestra aperta: e' il panadapter di Decodium,
                    // con la sua FFT, e a finestra chiusa disegnerebbe per
                    // nessuno. Chiudendo si distrugge, riaprendo si ricostruisce:
                    // costa una frazione di secondo alla riapertura e non costa
                    // nulla per tutto il tempo in cui la finestra resta chiusa.
                    active: window.visible
                }

                ReceivePanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 90
                }
            }
            }

            ColumnLayout {
                Layout.preferredWidth: 258
                Layout.minimumWidth: 220
                Layout.maximumWidth: 300
                Layout.fillHeight: true
                spacing: 10

                TuningScope {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(130, window.height * 0.18)
                    Layout.minimumHeight: 104
                }

                // Decoder e filtri condividono la colonna: si passa dall'uno
                // all'altro, perche' i filtri servono solo quando la banda e'
                // affollata e tenerli sempre a vista ruberebbe spazio al resto.
                TabBar {
                    id: sideTabs
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    background: null

                    Repeater {
                        model: [qsTr("DECODER"), qsTr("FILTERS")]
                        delegate: TabButton {
                            required property int index
                            required property string modelData
                            height: 26
                            contentItem: Text {
                                text: modelData
                                color: sideTabs.currentIndex === index ? Theme.accent
                                                                       : Theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1.1
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: sideTabs.currentIndex === index
                                       ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                       : "transparent"
                                radius: 6
                                border.color: sideTabs.currentIndex === index
                                              ? Theme.accent : "transparent"
                                border.width: 1
                            }
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: sideTabs.currentIndex

                    DecoderPanel {}
                    FilterPanel {}
                }
            }
        }

        // ── transmit along the bottom ───────────────────────────────────
        TransmitPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            Layout.minimumHeight: 104
        }
    }

    SetupDialog { id: setupDialog }
    LogDialog   { id: logDialog }

    // Errors from anywhere in the stack surface in one place rather than being
    // logged where nobody reads them.
    Rectangle {
        id: toast

        property string message: ""

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        width: Math.min(toastText.implicitWidth + 32, window.width - 80)
        height: 40
        radius: 8
        color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.92)
        opacity: 0
        visible: opacity > 0

        Text {
            id: toastText
            anchors.centerIn: parent
            text: toast.message
            color: "#FFFFFF"
            font.pixelSize: 12
            elide: Text.ElideRight
            width: Math.min(implicitWidth, window.width - 112)
        }

        SequentialAnimation {
            id: toastAnimation
            NumberAnimation { target: toast; property: "opacity"; to: 1.0; duration: 160 }
            PauseAnimation { duration: 4200 }
            NumberAnimation { target: toast; property: "opacity"; to: 0.0; duration: 400 }
        }

        function show(message) {
            toast.message = message
            toastAnimation.restart()
        }
    }

    Connections {
        target: radio
        function onErrorOccurred(message) { toast.show(message) }
    }

    // The engine has its own failures to report — a transmission that hit the
    // time limit, most importantly.
    Connections {
        target: rtty
        function onErrorOccurred(message) { toast.show(message) }
    }
}
