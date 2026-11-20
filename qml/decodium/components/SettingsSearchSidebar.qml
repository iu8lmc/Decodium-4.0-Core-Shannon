import QtQuick
import QtQuick.Controls
import "SettingsSearchIndex.js" as SettingsSearch

Item {
    id: root
    objectName: "settingsSearchSidebar"
    required property var dialog
    required property var tabHost
    readonly property color primaryBlue: dialog.primaryBlue
    readonly property color textPrimary: dialog.textPrimary
    readonly property color textSecondary: dialog.textSecondary
    property string pendingSearchLabel: ""
    property int pendingSearchAttempts: 0
    readonly property var settingsTabNames: [qsTranslate("SettingsDialog", "Station"), qsTranslate("SettingsDialog", "Radio"), qsTranslate("SettingsDialog", "Audio"), qsTranslate("SettingsDialog", "TX"), qsTranslate("SettingsDialog", "Display"), qsTranslate("SettingsDialog", "Decode"), qsTranslate("SettingsDialog", "Reporting"), qsTranslate("SettingsDialog", "Frequencies"), qsTranslate("SettingsDialog", "Colors"), qsTranslate("SettingsDialog", "Advanced"), qsTranslate("SettingsDialog", "Alerts"), qsTranslate("SettingsDialog", "Filters"), qsTranslate("SettingsDialog", "UI Buttons"), qsTranslate("SettingsDialog", "Callsign")]

    function openSearchResult(result) {
        pendingSearchLabel = result.label;
        pendingSearchAttempts = 0;
        dialog.currentTab = result.tab;
        finishSearch.restart();
    }
    Timer {
        id: finishSearch
        interval: 0
        onTriggered: {
            settingsSearchField.text = ""
            settingsSearchScroll.restart()
        }
    }

    function findSearchLabel(item, label) {
        if (!item || !item.visible)
            return null;
        if (item.mapToItem && ((typeof item.text === "string" && item.text === label) || (typeof item.placeholderText === "string" && item.placeholderText === label)))
            return item;
        var children = item.children || [];
        for (var i = 0; i < children.length; ++i) {
            var match = findSearchLabel(children[i], label);
            if (match)
                return match;
        }
        return null;
    }
    Timer {
        id: settingsSearchScroll
        interval: 150
        repeat: true
        onTriggered: {
            var host = tabHost.children || [];
            for (var i = 0; i < host.length; ++i) {
                var page = host[i].item;
                if (!host[i].active || !page || !page.pageSurface)
                    continue;
                var found = root.findSearchLabel(page.pageSurface, root.pendingSearchLabel);
                if (found) {
                    var point = found.mapToItem(page.pageSurface, 0, 0);
                    page.contentY = Math.max(0, Math.min(page.contentHeight - page.height, point.y - 35));
                    stop();
                    return;
                }
            }
            if (++root.pendingSearchAttempts >= 20)
                stop();
        }
    }

    TextField {
        id: settingsSearchField
        objectName: "settingsSearchField"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 6
        }
        height: 36
        placeholderText: qsTranslate("SettingsDialog", "Search settings…")
        selectByMouse: true
        color: textPrimary
        font.pixelSize: 12
    }

    Flickable {
        id: settingsTabScroll
        anchors {
            top: settingsSearchField.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        contentWidth: width
        contentHeight: Math.max(height, settingsTabColumn.implicitHeight + 16)
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AlwaysOff
        }
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            interactive: true
            active: hovered || pressed
        }

        Column {
            id: settingsTabColumn
            x: 6
            y: 8
            width: Math.max(0, parent.width - 12)
            spacing: 2

            Repeater {
                model: settingsSearchField.text.trim().length ? [] : root.settingsTabNames
                delegate: Rectangle {
                    width: settingsTabColumn.width
                    height: 36
                    radius: 6
                    color: dialog.currentTab === index ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25) : (tabMA.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                    border.color: dialog.currentTab === index ? primaryBlue : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: dialog.currentTab === index ? primaryBlue : textSecondary
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: tabMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dialog.currentTab = index
                    }
                }
            }
            Text {
                visible: settingsSearchField.text.trim().length > 0 && settingsSearchResults.count === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTranslate("SettingsDialog", "No matching settings")
                color: textSecondary
                font.pixelSize: 12
            }
            Repeater {
                id: settingsSearchResults
                model: SettingsSearch.find(settingsSearchField.text)
                delegate: Rectangle {
                    required property var modelData
                    objectName: "settingsSearchResult"
                    width: settingsTabColumn.width
                    height: 52
                    radius: 6
                    color: searchResultMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                    Column {
                        anchors {
                            fill: parent
                            margins: 5
                        }
                        spacing: 2
                        Text {
                            width: parent.width
                            text: modelData.label
                            elide: Text.ElideRight
                            color: textPrimary
                            font.pixelSize: 11
                        }
                        Text {
                            width: parent.width
                            text: root.settingsTabNames[modelData.tab]
                            elide: Text.ElideRight
                            color: textSecondary
                            font.pixelSize: 10
                        }
                    }
                    MouseArea {
                        id: searchResultMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openSearchResult(modelData)
                    }
                }
            }
        }
    }
}
