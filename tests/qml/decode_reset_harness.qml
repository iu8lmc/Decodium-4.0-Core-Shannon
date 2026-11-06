import QtQuick
import "../../qml/decodium/components"

Rectangle {
    width: 480
    height: 280
    color: "black"
    property bool attached: true
    property bool newestFirst: false
    ListView {
        id: list
        objectName: "decodeList"
        anchors.fill: parent
        clip: true
        reuseItems: true
        model: parent.attached ? decodeTestModel : null
        verticalLayoutDirection: parent.newestFirst ? ListView.BottomToTop : ListView.TopToBottom
        property int resetEpoch: 0
        property bool followTail: false
        property bool tailFollowPending: true
        property bool tailFollowQueued: true
        property int pendingNewDecodes: 7
        property real tailLastY: 120
        property real tailLastHeight: 1000
        property real tailLastOriginY: 0
        function showRows() {
            forceLayout()
            positionViewAtBeginning()
        }
        delegate: Rectangle {
            width: ListView.view.width
            height: 28
            color: "#00ee66"
            Text { text: modelData ? modelData.message : "" }
        }
        add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 100 } }
        removeDisplaced: Transition { NumberAnimation { property: "y"; duration: 100 } }
        NumberAnimation {
            id: scroll
            objectName: "scroll"
            target: list
            property: "contentY"
            to: 500
            duration: 1000
        }
        Timer { id: settle; interval: 32 }
        DecodeListResetGuard {
            view: list
            sourceModel: decodeTestModel
            scrollAnimation: scroll
            settleTimer: settle
        }
    }
}
