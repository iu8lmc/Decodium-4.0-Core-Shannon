/* Decodium Qt6 - Draggable & Resizable Panel Component
 * Panel that can be dragged, resized, and snaps back to dock with magnetic effect
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel

    // Panel properties
    property color bgDeep: bridge.themeManager.bgDeep
    property color textPrimary: bridge.themeManager.textPrimary
    property string panelTitle: "Panel"
    property color panelColor: bridge.themeManager.secondaryColor
    property rect dockZone: Qt.rect(0, 0, 400, 400)
    property int snapThreshold: 80
    property alias contentItem: contentLoader.sourceComponent

    // Size constraints
    property int minimumWidth: 250
    property int minimumHeight: 200
    property int maximumWidth: 1200
    property int maximumHeight: 900

    // State tracking
    property bool isDocked: true
    property bool isDragging: false
    property bool isResizing: false
    property point dragStartPos: Qt.point(0, 0)
    property point originalPos: Qt.point(0, 0)
    property size originalSize: Qt.size(width, height)

    // Signals
    signal dragStarted()
    signal dragEnded()
    signal nearDock()
    signal awayFromDock()
    signal docked()
    signal undocked()
    signal resizeStarted()
    signal resizeEnded()

    // Visual properties
    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.7)
    border.color: isDragging || isResizing ? panelColor : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
    border.width: isDragging || isResizing ? 2 : 1
    radius: 10

    // Smooth animations for magnetic snap
    Behavior on x {
        enabled: !isDragging && !isResizing
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutBack
            easing.overshoot: 1.2
        }
    }

    Behavior on y {
        enabled: !isDragging && !isResizing
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutBack
            easing.overshoot: 1.2
        }
    }

    Behavior on width {
        enabled: !isDragging && !isResizing
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutBack
        }
    }

    Behavior on height {
        enabled: !isDragging && !isResizing
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutBack
        }
    }

    // Scale effect when dragging
    transform: Scale {
        id: scaleTransform
        origin.x: panel.width / 2
        origin.y: panel.height / 2
        xScale: isDragging ? 0.98 : 1.0
        yScale: isDragging ? 0.98 : 1.0

        Behavior on xScale { NumberAnimation { duration: 150 } }
        Behavior on yScale { NumberAnimation { duration: 150 } }
    }

    // Shadow effect when dragging or resizing
    Rectangle {
        id: shadow
        anchors.fill: parent
        anchors.margins: -8
        radius: parent.radius + 4
        color: "transparent"
        border.color: Qt.rgba(panelColor.r, panelColor.g, panelColor.b, 0.3)
        border.width: (isDragging || isResizing) ? 8 : 0
        z: -1
        opacity: (isDragging || isResizing) ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 200 } }
        Behavior on border.width { NumberAnimation { duration: 200 } }
    }

    // ============ RESIZE HANDLES ============

    // Edge resize handle size
    readonly property int edgeSize: 6
    readonly property int cornerSize: 12

    // Left edge
    MouseArea {
        id: leftResize
        width: edgeSize
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: cornerSize
        anchors.bottomMargin: cornerSize
        cursorShape: Qt.SizeHorCursor

        property real startX
        property real startWidth

        onPressed: function(mouse) {
            startX = panel.x
            startWidth = panel.width
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var delta = mouse.x
                var newWidth = startWidth - delta
                if (newWidth >= minimumWidth && newWidth <= maximumWidth) {
                    panel.width = newWidth
                    panel.x = startX + (startWidth - newWidth)
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Right edge
    MouseArea {
        id: rightResize
        width: edgeSize
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: cornerSize
        anchors.bottomMargin: cornerSize
        cursorShape: Qt.SizeHorCursor

        property real startWidth

        onPressed: function(mouse) {
            startWidth = panel.width
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var newWidth = startWidth + mouse.x
                if (newWidth >= minimumWidth && newWidth <= maximumWidth) {
                    panel.width = newWidth
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Top edge
    MouseArea {
        id: topResize
        height: edgeSize
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: cornerSize
        anchors.rightMargin: cornerSize
        cursorShape: Qt.SizeVerCursor

        property real startY
        property real startHeight

        onPressed: function(mouse) {
            startY = panel.y
            startHeight = panel.height
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var delta = mouse.y
                var newHeight = startHeight - delta
                if (newHeight >= minimumHeight && newHeight <= maximumHeight) {
                    panel.height = newHeight
                    panel.y = startY + (startHeight - newHeight)
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Bottom edge
    MouseArea {
        id: bottomResize
        height: edgeSize
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: cornerSize
        anchors.rightMargin: cornerSize
        cursorShape: Qt.SizeVerCursor

        property real startHeight

        onPressed: function(mouse) {
            startHeight = panel.height
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var newHeight = startHeight + mouse.y
                if (newHeight >= minimumHeight && newHeight <= maximumHeight) {
                    panel.height = newHeight
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Top-left corner
    MouseArea {
        id: topLeftResize
        width: cornerSize
        height: cornerSize
        anchors.top: parent.top
        anchors.left: parent.left
        cursorShape: Qt.SizeFDiagCursor

        property real startX
        property real startY
        property real startWidth
        property real startHeight

        onPressed: function(mouse) {
            startX = panel.x
            startY = panel.y
            startWidth = panel.width
            startHeight = panel.height
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var deltaX = mouse.x
                var deltaY = mouse.y
                var newWidth = startWidth - deltaX
                var newHeight = startHeight - deltaY

                if (newWidth >= minimumWidth && newWidth <= maximumWidth) {
                    panel.width = newWidth
                    panel.x = startX + (startWidth - newWidth)
                }
                if (newHeight >= minimumHeight && newHeight <= maximumHeight) {
                    panel.height = newHeight
                    panel.y = startY + (startHeight - newHeight)
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Top-right corner
    MouseArea {
        id: topRightResize
        width: cornerSize
        height: cornerSize
        anchors.top: parent.top
        anchors.right: parent.right
        cursorShape: Qt.SizeBDiagCursor

        property real startY
        property real startWidth
        property real startHeight

        onPressed: function(mouse) {
            startY = panel.y
            startWidth = panel.width
            startHeight = panel.height
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var newWidth = startWidth + mouse.x
                var deltaY = mouse.y
                var newHeight = startHeight - deltaY

                if (newWidth >= minimumWidth && newWidth <= maximumWidth) {
                    panel.width = newWidth
                }
                if (newHeight >= minimumHeight && newHeight <= maximumHeight) {
                    panel.height = newHeight
                    panel.y = startY + (startHeight - newHeight)
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Bottom-left corner
    MouseArea {
        id: bottomLeftResize
        width: cornerSize
        height: cornerSize
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        cursorShape: Qt.SizeBDiagCursor

        property real startX
        property real startWidth
        property real startHeight

        onPressed: function(mouse) {
            startX = panel.x
            startWidth = panel.width
            startHeight = panel.height
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var deltaX = mouse.x
                var newWidth = startWidth - deltaX
                var newHeight = startHeight + mouse.y

                if (newWidth >= minimumWidth && newWidth <= maximumWidth) {
                    panel.width = newWidth
                    panel.x = startX + (startWidth - newWidth)
                }
                if (newHeight >= minimumHeight && newHeight <= maximumHeight) {
                    panel.height = newHeight
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Bottom-right corner
    MouseArea {
        id: bottomRightResize
        width: cornerSize
        height: cornerSize
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        cursorShape: Qt.SizeFDiagCursor

        property real startWidth
        property real startHeight

        onPressed: function(mouse) {
            startWidth = panel.width
            startHeight = panel.height
            isResizing = true
            resizeStarted()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                var newWidth = startWidth + mouse.x
                var newHeight = startHeight + mouse.y

                if (newWidth >= minimumWidth && newWidth <= maximumWidth) {
                    panel.width = newWidth
                }
                if (newHeight >= minimumHeight && newHeight <= maximumHeight) {
                    panel.height = newHeight
                }
            }
        }

        onReleased: {
            isResizing = false
            isDocked = false
            resizeEnded()
        }
    }

    // Visual resize corner indicator (bottom-right)
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        width: 12
        height: 12
        color: "transparent"

        // Diagonal lines for resize grip
        Canvas {
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.strokeStyle = Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)
                ctx.lineWidth = 1

                // Draw 3 diagonal lines
                for (var i = 0; i < 3; i++) {
                    var offset = i * 4
                    ctx.beginPath()
                    ctx.moveTo(width - offset, height)
                    ctx.lineTo(width, height - offset)
                    ctx.stroke()
                }
            }
        }
    }

    // ============ PANEL CONTENT ============

    // Panel layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Header with drag handle
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
            radius: 5

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                // Drag handle indicator
                Column {
                    spacing: 2
                    Layout.alignment: Qt.AlignVCenter

                    Repeater {
                        model: 3
                        Rectangle {
                            width: 16
                            height: 2
                            radius: 1
                            color: isDragging ? panelColor : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.3)

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                    }
                }

                Text {
                    text: panelTitle
                    font.pixelSize: 13
                    font.bold: true
                    color: panelColor
                    Layout.fillWidth: true
                }

                // Size indicator
                Text {
                    text: Math.round(panel.width) + "x" + Math.round(panel.height)
                    font.pixelSize: 9
                    font.family: "Monospace"
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.4)
                    visible: isResizing
                }

                // Dock status indicator
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: isDocked ? panelColor : "#ff9800"

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                // Reset to dock button
                Rectangle {
                    width: 20
                    height: 20
                    radius: 3
                    color: resetMouseArea.containsMouse ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1) : "transparent"
                    visible: !isDocked

                    Text {
                        anchors.centerIn: parent
                        text: "⌂"
                        font.pixelSize: 14
                        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
                    }

                    MouseArea {
                        id: resetMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: snapToDock()
                    }
                }
            }

            // Drag mouse area on header
            MouseArea {
                id: dragArea
                anchors.fill: parent
                anchors.rightMargin: isDocked ? 0 : 30  // Leave space for reset button
                cursorShape: Qt.SizeAllCursor
                hoverEnabled: true

                property point lastMousePos

                onPressed: function(mouse) {
                    isDragging = true
                    dragStartPos = Qt.point(mouse.x, mouse.y)
                    originalPos = Qt.point(panel.x, panel.y)
                    originalSize = Qt.size(panel.width, panel.height)
                    lastMousePos = Qt.point(mouse.x, mouse.y)
                    panel.z = 100  // Bring to front
                    dragStarted()
                }

                onPositionChanged: function(mouse) {
                    if (isDragging) {
                        var deltaX = mouse.x - lastMousePos.x
                        var deltaY = mouse.y - lastMousePos.y

                        panel.x += deltaX
                        panel.y += deltaY

                        // Check proximity to dock zone
                        checkDockProximity()
                    }
                }

                onReleased: {
                    isDragging = false
                    panel.z = 1

                    // Check if should snap to dock
                    if (isNearDock()) {
                        snapToDock()
                    } else {
                        isDocked = false
                        undocked()
                    }

                    dragEnded()
                }
            }
        }

        // Content area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                id: contentLoader
                anchors.fill: parent
            }
        }
    }

    // ============ FUNCTIONS ============

    // Check if panel is near its dock zone
    function isNearDock() {
        var centerX = panel.x + panel.width / 2
        var centerY = panel.y + panel.height / 2
        var dockCenterX = dockZone.x + dockZone.width / 2
        var dockCenterY = dockZone.y + dockZone.height / 2

        var distance = Math.sqrt(
            Math.pow(centerX - dockCenterX, 2) +
            Math.pow(centerY - dockCenterY, 2)
        )

        return distance < snapThreshold * 2
    }

    // Check dock proximity and emit signals
    function checkDockProximity() {
        if (isNearDock()) {
            nearDock()
        } else {
            awayFromDock()
        }
    }

    // Snap panel to its dock zone with magnetic effect
    function snapToDock() {
        panel.x = dockZone.x
        panel.y = dockZone.y
        panel.width = dockZone.width
        panel.height = dockZone.height
        isDocked = true
        docked()
    }

    // Undock panel (can be called programmatically)
    function undockPanel() {
        isDocked = false
        undocked()
    }

    // Reset to dock position
    function resetToDock() {
        snapToDock()
    }

    // Component initialization
    Component.onCompleted: {
        originalPos = Qt.point(x, y)
        originalSize = Qt.size(width, height)
    }

    // Keep panel within bounds when docked and parent resizes
    onDockZoneChanged: {
        if (isDocked) {
            x = dockZone.x
            y = dockZone.y
            width = dockZone.width
            height = dockZone.height
        }
    }
}
