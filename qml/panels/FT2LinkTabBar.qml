pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var panel: null
    readonly property bool panelReady: panel !== null && panel !== undefined
    readonly property color green: panelReady ? panel.green : "#78d77c"
    readonly property color cyan: panelReady ? panel.cyan : "#49d7e8"
    readonly property color amber: panelReady ? panel.amber : "#e7b85c"
    readonly property color red: panelReady ? panel.red : "#ef6f6c"
    readonly property color textPrimary: panelReady ? panel.textPrimary : "#e8eef2"
    readonly property color textSecondary: panelReady ? panel.textSecondary : "#93a7b0"
    readonly property color panelBg: panelReady ? panel.panelBg : "#071018"
    readonly property string mono: panelReady ? panel.mono : decodiumMonoFontFamily
    readonly property int tabSpacing: 6
    readonly property var tabs: [
        { id: "CHAT", page: 0,  w: 54 },
        { id: "FORM", page: 1,  w: 54 },
        { id: "FILE", page: 2,  w: 54 },
        { id: "BBS",  page: 3,  w: 48 },
        { id: "BCAST",page: 4,  w: 62 },
        { id: "MAIL", page: 5,  w: 54 },
        { id: "INFO", page: 6,  w: 54 },
        { id: "CALL", page: 7,  w: 54 },
        { id: "STAT", page: 10, w: 54 },
        { id: "RXF",  page: 11, w: 54 },
        { id: "PATH", page: 9,  w: 54 },
        { id: "LOG",  page: 12, w: 48 },
        { id: "DB",   page: 13, w: 40 },
        { id: "PRE",  page: 14, w: 44 },
        { id: "FREQ", page: 15, w: 52 },
        { id: "SAT",  page: 17, w: 44 },
        { id: "BLK",  page: 16, w: 44 },
        { id: "CLST", page: 8,  w: 50 }
    ]
    readonly property real naturalWidth: {
        var total = 0
        for (var i = 0; i < tabs.length; ++i)
            total += Number(tabs[i].w || 54)
        return total + Math.max(0, tabs.length - 1) * tabSpacing
    }

    implicitHeight: Math.max(32, tabFlow.implicitHeight)
    implicitWidth: naturalWidth

    function tabLabel(id) {
        if (!panelReady)
            return id
        if (id === "BBS")
            return Number(panel.bulletinUnreadCount || 0) > 0 ? "BBS*" : "BBS"
        if (id === "MAIL")
            return panel.ft2LinkAdapter
                    && Number(panel.ft2LinkAdapter.mailboxUnreadCount || 0) > 0 ? "MAIL*" : "MAIL"
        if (id === "RXF")
            return Number(panel.receivedFileUnreadCount || 0) > 0 ? "RXF*" : "RXF"
        return id
    }

    function tabAccent(id) {
        if (!panelReady)
            return textSecondary
        if (id === "CHAT" || id === "MAIL" || id === "STAT" || id === "CLST")
            return green
        if (id === "FORM" || id === "BCAST" || id === "CALL" || id === "PATH" || id === "PRE" || id === "SAT")
            return amber
        if (id === "DB" || id === "BLK")
            return red
        if (id === "BBS")
            return Number(panel.bulletinUnreadCount || 0) > 0 ? amber : textPrimary
        if (id === "RXF")
            return Number(panel.receivedFileUnreadCount || 0) > 0 ? amber : cyan
        if (id === "LOG")
            return textPrimary
        return cyan
    }

    function tabTip(id) {
        if (id === "BBS" && panelReady && Number(panel.bulletinUnreadCount || 0) > 0)
            return "Unread BBS bulletins"
        if (id === "RXF" && panelReady && Number(panel.receivedFileUnreadCount || 0) > 0)
            return "Unread received files"
        return ""
    }

    function activateTab(page) {
        if (!panelReady)
            return
        if (typeof panel.openToolPage === "function")
            panel.openToolPage(Number(page))
        else
            panel.toolPageIndex = Number(page)
    }

    Flow {
        id: tabFlow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: root.tabSpacing

            Repeater {
                model: root.tabs

                Rectangle {
                    id: tabButton
                    required property var modelData
                    readonly property bool checked: root.panelReady
                                                   && Number(root.panel.toolPageIndex || 0) === Number(modelData.page)
                    readonly property color accent: root.tabAccent(String(modelData.id || ""))

                    width: Number(modelData.w || 54)
                    height: 28
                    radius: 4
                    color: !root.panelReady
                           ? Qt.rgba(1, 1, 1, 0.020)
                           : (checked ? Qt.rgba(accent.r, accent.g, accent.b, 0.22)
                                      : (tabMouse.containsMouse
                                         ? Qt.rgba(accent.r, accent.g, accent.b, 0.14)
                                         : Qt.rgba(1, 1, 1, 0.035)))
                    border.color: checked ? accent
                                           : (root.panelReady
                                              ? Qt.rgba(accent.r, accent.g, accent.b, 0.58)
                                              : Qt.rgba(root.textSecondary.r,
                                                        root.textSecondary.g,
                                                        root.textSecondary.b,
                                                        0.18))
                    border.width: 1
                    opacity: root.panelReady ? 1.0 : 0.72

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        text: root.tabLabel(String(tabButton.modelData.id || ""))
                        color: root.panelReady ? tabButton.accent
                                               : Qt.rgba(root.textSecondary.r,
                                                         root.textSecondary.g,
                                                         root.textSecondary.b,
                                                         0.52)
                        font.family: root.mono
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        enabled: root.panelReady
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activateTab(tabButton.modelData.page)
                    }

                    ToolTip.visible: tabMouse.containsMouse
                                     && root.tabTip(String(tabButton.modelData.id || "")).length > 0
                    ToolTip.text: root.tabTip(String(tabButton.modelData.id || ""))
                    ToolTip.delay: 450
            }
        }
    }
}
