import QtQuick
import QtTest
import "../../qml/decodium/components"

TestCase {
    id: testCase
    name: "SettingsSearchSidebar"
    visible: true
    when: windowShown
    width: 640; height: 480
    QtObject {
        id: fakeDialog
        property color primaryBlue: "blue"
        property color textPrimary: "white"
        property color textSecondary: "grey"
        property int currentTab: 0
    }
    Item {
        id: host
        width: 300; height: 300
        Loader {
            id: pageLoader
            active: fakeDialog.currentTab === 1
            sourceComponent: Flickable {
                id: page
                width: 300; height: 300
                contentWidth: 300; contentHeight: 1200
                readonly property Item pageSurface: contentItem
                Text { x: 0; y: 900; text: "TCI Audio:" }
            }
        }
    }
    Component { id: sidebarComponent; SettingsSearchSidebar { width: 210; height: 400; dialog: fakeDialog; tabHost: host } }
    function test_find_and_navigate() {
        fakeDialog.currentTab = 0
        let sidebar = createTemporaryObject(sidebarComponent, testCase)
        let field = findChild(sidebar, "settingsSearchField")
        field.text = "TCI Audio:"
        wait(100)
        let result = findChild(sidebar, "settingsSearchResult")
        verify(result !== null)
        mouseClick(result, result.width / 2, result.height / 2)
        compare(fakeDialog.currentTab, 1)
        tryVerify(function() { return pageLoader.item && pageLoader.item.contentY > 0 }, 3000)
        compare(field.text, "")
    }
}
