// DecoRTTY — the crossed-ellipse tuning scope.
//
// Mark on one axis, space on the other. A correctly tuned RTTY signal traces a
// cross; a mistuned one leans, and a signal on the wrong shift draws an ellipse.
// Operators have tuned by this display since the 1950s and it still reads faster
// than any number.
import QtQuick

import "."

GlassPanel {
    id: root

    property int historyLength: 220

    // Ring buffer of recent mark/space pairs.
    property var pointsX: []
    property var pointsY: []
    property int writeIndex: 0

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        PanelHeading { text: qsTr("TUNING") }

        Item {
            width: parent.width
            height: parent.height - 34

            Canvas {
                id: scope
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height)
                height: width

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()

                    const half = width / 2
                    const scale = half * 0.78

                    // Graticule.
                    ctx.strokeStyle = Qt.rgba(Theme.glassBorder.r, Theme.glassBorder.g,
                                              Theme.glassBorder.b, 0.5)
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.moveTo(half, 4); ctx.lineTo(half, height - 4)
                    ctx.moveTo(4, half); ctx.lineTo(width - 4, half)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.arc(half, half, scale, 0, 2 * Math.PI)
                    ctx.stroke()

                    // The trace. Older points fade, so the eye follows the
                    // current shape rather than a solid blob.
                    const n = root.pointsX.length
                    for (let i = 0; i < n; ++i) {
                        const idx = (root.writeIndex + i) % n
                        const age = i / n
                        const px = half + root.pointsX[idx] * scale
                        const py = half - root.pointsY[idx] * scale

                        ctx.fillStyle = Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                Theme.accent.b, 0.15 + 0.85 * age)
                        ctx.fillRect(px - 1, py - 1, 2.2, 2.2)
                    }
                }
            }

            // Lock lamp.
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                width: 8; height: 8; radius: 4
                color: rtty.locked ? Theme.accent : Theme.textSecondary
                opacity: rtty.locked ? 1.0 : 0.35
                Behavior on color { ColorAnimation { duration: 200 } }
            }
        }
    }

    Connections {
        target: rtty
        function onScopePointReady(mark, space) {
            if (root.pointsX.length < root.historyLength) {
                root.pointsX.push(mark)
                root.pointsY.push(space)
            } else {
                root.pointsX[root.writeIndex] = mark
                root.pointsY[root.writeIndex] = space
                root.writeIndex = (root.writeIndex + 1) % root.historyLength
            }
        }
    }

    // Repainting on every incoming point would be wasteful; 25 Hz is smooth to
    // the eye and costs a fraction of a frame.
    Timer {
        interval: 40
        running: true
        repeat: true
        onTriggered: scope.requestPaint()
    }
}
