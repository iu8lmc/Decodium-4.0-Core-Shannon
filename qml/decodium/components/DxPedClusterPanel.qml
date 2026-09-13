/* DxPedClusterPanel - Cluster / MAM a schede per il workspace DX-Pedition.
 * 1.0.570: estratto dal pool di DxPeditionWorkspace, cosi' la stessa cosa puo'
 * essere istanziata anche nella finestra staccata. Le finestre staccate NON
 * riusano il pannello agganciato: ne creano uno proprio, come fa da sempre il
 * resto dell'applicazione (Main.qml ha due Waterfall e due TxPanel distinti).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: clusterPanel

    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)
    property var engine: (typeof appEngine !== 'undefined' ? appEngine : null)
    property int leftTab: 0

    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:     tm ? tm.accentColor   : "#19ff88"
    readonly property color cAccentDim:  tm ? tm.accentDim     : "#0fa55a"
    readonly property color cAccentDeep: tm ? tm.accentDeep    : "#052d1a"
    readonly property color cPanelHdr:   tm ? tm.panelHeader   : "#0a0e0c"
    readonly property color cBorder:     tm ? tm.borderColor   : "#1f2a22"
    readonly property color cTextDim:    tm ? tm.textSecondary : "#6c7872"

    Row {
        id: clusterMamTabs
        anchors { top: parent.top; left: parent.left; right: parent.right; topMargin: 2 }
        height: 26
        spacing: 6
        Repeater {
            model: ["CLUSTER", "MAM"]
            delegate: Rectangle {
                id: tabItem
                required property int index
                required property string modelData
                width: (clusterMamTabs.width - 6) / 2
                height: 24
                radius: 4
                readonly property bool sel: clusterPanel.leftTab === tabItem.index
                color: tabItem.sel ? clusterPanel.cAccentDeep
                                   : (tabMA.containsMouse ? clusterPanel.cPanelHdr : "transparent")
                border.color: tabItem.sel ? clusterPanel.cAccentDim : clusterPanel.cBorder
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: tabItem.modelData
                    color: tabItem.sel ? clusterPanel.cAccent : clusterPanel.cTextDim
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.2
                }
                MouseArea {
                    id: tabMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: clusterPanel.leftTab = tabItem.index
                }
            }
        }
    }

    Loader {
        anchors { top: clusterMamTabs.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; topMargin: 4 }
        active: true
        visible: clusterPanel.leftTab === 0
        sourceComponent: clusterComp
    }
    Component {
        id: clusterComp
        DxClusterPanel {
            embedded: true   // 1.0.343 - no drag/resize interni nel workspace
            minPanelWidth: 0
            minPanelHeight: 0
            x: 0; y: 0
            width: parent ? parent.width : 320
            height: parent ? parent.height : 280
            radius: 0
            border.width: 0
        }
    }

    Loader {
        anchors { top: clusterMamTabs.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; topMargin: 4 }
        active: clusterPanel.leftTab === 1
        visible: clusterPanel.leftTab === 1
        sourceComponent: mamPanelComp
    }
    Component {
        id: mamPanelComp
        MamPanel {
            anchors.fill: parent
            engine: clusterPanel.engine
        }
    }
}
