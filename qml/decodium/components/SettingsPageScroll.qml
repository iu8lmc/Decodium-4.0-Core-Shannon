import QtQuick
import QtQuick.Controls

// Shared viewport for every Settings page.
//
// Qt's automatic ScrollView sizing is not reliable when a GridLayout is
// anchored to the viewport but its controls have larger minimum widths.  The
// caller therefore supplies the responsive minimum width explicitly.  Do not
// derive the horizontal extent from a page's implicitWidth: a single long
// translated label or spanning control can make that value several screens
// wide and create a huge, useless horizontal scroll range.
Flickable {
    id: root

    default property alias pageData: pageCanvas.data

    property int pageLeftMargin: 10
    property int pageTopMargin: 10
    property int pageRightMargin: 12
    property int pageBottomMargin: 24
    property real minimumContentWidth: 0

    readonly property real availableWidth: width
    readonly property real availableHeight: height

    // Keep a stable content surface instead of relying on ScrollView's
    // automatic single-child sizing.  The latter creates circular bindings
    // when a GridLayout is anchored to the viewport and its implicit size is
    // also used as the scroll extent.
    readonly property Item pageSurface: Item {
        id: pageCanvas
        parent: root.contentItem
        width: root.contentWidth
        height: root.contentHeight
    }

    readonly property Item measuredItem: pageCanvas.children.length > 0
                                         ? pageCanvas.children[0]
                                         : null
    readonly property real measuredImplicitHeight: measuredItem
                                                    ? Math.max(0,
                                                               measuredItem.implicitHeight,
                                                               measuredItem.childrenRect.y
                                                               + measuredItem.childrenRect.height)
                                                    : 0

    clip: true
    interactive: true
    boundsBehavior: Flickable.StopAtBounds
    // Do not leave direction detection to the platform style.  In particular,
    // some Linux QPA/style combinations do not enable wheel scrolling when a
    // layout starts with a horizontal overflow and the vertical range is
    // calculated a frame later.
    flickableDirection: Flickable.HorizontalAndVerticalFlick
    // minimumContentWidth is calculated by SettingsDialog from the active
    // two/four-column layout.  It is the only horizontal overflow source; the
    // page still scrolls on genuinely narrow screens, but cannot grow because
    // of an anomalous child implicitWidth.
    contentWidth: Math.max(width, minimumContentWidth)
    contentHeight: Math.max(height,
                            measuredImplicitHeight + pageTopMargin + pageBottomMargin)

    // QQuick Flickable normally consumes wheel events itself.  A number of
    // Linux desktop combinations route the wheel to the focused TextField or
    // ComboBox instead, however, leaving a clipped Settings page apparently
    // frozen.  This button-less MouseArea is deliberately only a wheel bridge:
    // it does not take mouse clicks away from controls or the scroll bars.
    MouseArea {
        id: wheelBridge
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        z: 900

        onWheel: function(wheel) {
            var pixelDelta = wheel.pixelDelta ? wheel.pixelDelta : Qt.point(0, 0)
            var angleDelta = wheel.angleDelta ? wheel.angleDelta : Qt.point(0, 0)
            var pixelX = Number(pixelDelta.x || 0)
            var pixelY = Number(pixelDelta.y || 0)
            var angleX = Number(angleDelta.x || 0)
            var angleY = Number(angleDelta.y || 0)
            var deltaX = pixelX !== 0 ? pixelX : angleX / 120 * 48
            var deltaY = pixelY !== 0 ? pixelY : angleY / 120 * 48
            var shiftPressed = wheel.modifiers !== undefined
                    && (wheel.modifiers & Qt.ShiftModifier) !== 0
            var maxX = Math.max(0, root.contentWidth - root.width)
            var maxY = Math.max(0, root.contentHeight - root.height)
            var changed = false

            // Shift+wheel is the portable horizontal-wheel gesture.  Native
            // horizontal deltas remain supported for trackpads and mice that
            // expose a horizontal wheel axis.
            if ((shiftPressed || deltaX !== 0) && maxX > 0) {
                var nextX = Math.max(0, Math.min(maxX, root.contentX - (shiftPressed && deltaX === 0 ? deltaY : deltaX)))
                changed = Math.abs(nextX - root.contentX) > 0.01
                root.contentX = nextX
                if (shiftPressed)
                    deltaY = 0
            }

            if (deltaY !== 0 && maxY > 0) {
                var nextY = Math.max(0, Math.min(maxY, root.contentY - deltaY))
                changed = Math.abs(nextY - root.contentY) > 0.01 || changed
                root.contentY = nextY
            }

            // A wheel event at an edge is still consumed when this page has a
            // scroll range.  This prevents a nested control from scrolling a
            // different parent window on Linux.
            wheel.accepted = changed || maxX > 0 || maxY > 0
        }
    }

    ScrollBar.horizontal: ScrollBar {
        policy: ScrollBar.AsNeeded
        interactive: true
        active: hovered || pressed || root.contentWidth > root.availableWidth + 0.5
    }
    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
        interactive: true
        active: hovered || pressed || root.contentHeight > root.availableHeight + 0.5
    }
}
