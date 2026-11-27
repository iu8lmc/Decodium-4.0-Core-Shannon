import QtQuick

// Shared by docked and detached decode panes. Keep the source model connected
// even while a hidden/detached view temporarily has model: null.
Connections {
    id: guard
    property var view
    property var sourceModel
    property var scrollAnimation
    property var settleTimer
    target: sourceModel

    function stopOldScroll() {
        if (scrollAnimation) scrollAnimation.stop()
        if (settleTimer) settleTimer.stop()
        view.cancelFlick()
    }
    function onModelAboutToBeReset() {
        ++view.resetEpoch
        stopOldScroll()
    }
    function onModelReset() {
        stopOldScroll()
        view.tailFollowQueued = false
        view.tailFollowPending = false
        view.pendingNewDecodes = 0
        view.followTail = true
        view.forceLayout()
        view.contentY = view.verticalLayoutDirection === ListView.BottomToTop
                      ? view.originY - view.height : view.originY
        view.tailLastY = view.contentY
        view.tailLastHeight = view.contentHeight
        view.tailLastOriginY = view.originY
    }
}
