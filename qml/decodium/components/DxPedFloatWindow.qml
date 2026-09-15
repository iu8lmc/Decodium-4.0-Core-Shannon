/* DxPedFloatWindow - finestra flottante per un pannello staccato del workspace
 * DX-Pedition (1.0.569). Non contiene logica: e' solo un contenitore. Il
 * pannello vero viene RE-PARENTATO qui dentro da DxPeditionWorkspace, cosi'
 * staccarlo e riagganciarlo non ricrea il componente (il Waterfall mantiene il
 * feed PCM, il TxPanel mantiene il popup di conferma log).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Window

Window {
    id: win

    property string panelKey: ""
    property string panelTitle: ""
    property color bgColor: "#0d1310"
    property color borderColor: "#1f2a22"
    property color accentColor: "#19ff88"
    property color textDim: "#6c7872"
    property alias body: bodyItem
    // Il pannello della finestra staccata e' una ISTANZA PROPRIA, caricata da
    // qui: spostare quello agganciato dentro un'altra finestra significherebbe
    // portarsi dietro nodi di scenegraph e popup legati alla finestra di
    // origine, ed e' cosi' che si arriva a un access violation.
    property url contentSource: ""

    signal dockRequested()
    // La X della finestra CHIUDE il pannello (sparisce anche dallo slot, che
    // collassa). Per rimetterlo nel workspace c'e' il pulsante AGGANCIA.
    signal closeRequested()
    // Emesso a movimento/ridimensionamento finito (mezzo secondo di quiete):
    // il workspace lo usa per memorizzare dove sta la finestra.
    signal geometrySettled()

    onXChanged: geometryTimer.restart()
    onYChanged: geometryTimer.restart()
    onWidthChanged: geometryTimer.restart()
    onHeightChanged: geometryTimer.restart()

    Timer {
        id: geometryTimer
        interval: 500
        repeat: false
        onTriggered: if (win.visible) win.geometrySettled()
    }

    width: 560
    height: 420
    minimumWidth: 280
    minimumHeight: 180
    color: bgColor
    title: panelTitle.length > 0 ? panelTitle : qsTr("DX-Pedition panel")

    // Qt manda un `closing` spurio subito dopo aver mostrato una finestra
    // transiente la cui madre e' appena stata agganciata alla scena. Senza
    // questa guardia quel colpo verrebbe letto come "chiudi", riagganciando da
    // solo un pannello che l'utente aveva staccato.
    property bool settled: false
    onVisibleChanged: {
        win.settled = false
        if (win.visible)
            settleTimer.restart()
    }
    Timer {
        id: settleTimer
        interval: 500
        repeat: false
        onTriggered: win.settled = true
    }

    // La X della finestra non la chiude: riaggancia il pannello nel workspace,
    // altrimenti il contenuto verrebbe distrutto insieme alla finestra.
    onClosing: function(close) {
        close.accepted = false
        if (win.settled)
            win.closeRequested()
    }

    Rectangle {
        anchors.fill: parent
        color: win.bgColor
        border.color: win.borderColor
        border.width: 1

        Rectangle {
            id: hdr
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 1
            height: 26
            color: "transparent"
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                text: win.panelTitle.toUpperCase()
                color: win.accentColor
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }
            Rectangle {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 8 }
                implicitWidth: dockLbl.implicitWidth + 16
                implicitHeight: 18
                radius: 3
                color: dockMA.containsMouse ? Qt.rgba(win.accentColor.r, win.accentColor.g, win.accentColor.b, 0.18)
                                            : "transparent"
                border.color: win.borderColor
                border.width: 1
                Text {
                    id: dockLbl
                    anchors.centerIn: parent
                    text: qsTr("⇲ DOCK")
                    color: dockMA.containsMouse ? win.accentColor : win.textDim
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                MouseArea {
                    id: dockMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: win.dockRequested()
                }
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: win.borderColor
            }
        }

        Item {
            id: bodyItem
            anchors { left: parent.left; right: parent.right; top: hdr.bottom; bottom: parent.bottom }
            anchors.margins: 2

            Loader {
                anchors.fill: parent
                asynchronous: false
                active: win.visible && String(win.contentSource).length > 0
                source: win.contentSource
            }
        }
    }
}
