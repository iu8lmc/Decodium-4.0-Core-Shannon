import QtQuick
import QtQuick.Shapes

/*!
    Quadrante del rotore: corona graduata, mappa azimutale del proprio QTH,
    lobo d'antenna, settore vietato dai finecorsa e ago della posizione.
    Un tocco dentro il disco manda il rotore in quella direzione.
*/
Item {
    id: dial

    property real azimuth: 0
    property real target: -1          // negativo = nessun bersaglio
    property real beamwidth: 60
    property bool hasPosition: false
    property bool moving: false
    property real limitMin: 0
    property real limitMax: 360

    property real latitude: 41.5      // QTH al centro della mappa
    property real longitude: 12.5
    property real pinLatitude: 0      // corrispondente del locatore digitato
    property real pinLongitude: 0
    property bool pinValid: false

    signal bearingRequested(real degrees)

    readonly property real outerRadius: Math.min(width, height) / 2 - 2
    // Corona sottile, solo tacche: i numeri stanno dentro il bordo della mappa,
    // dove c'e' solo l'oceano degli antipodi, cosi' la mappa prende quasi tutto il disco.
    readonly property real ringThickness: Math.max(12, outerRadius * 0.055)
    readonly property real digitSize: Math.max(10, outerRadius * 0.055)
    readonly property real mapRadius: outerRadius - ringThickness
    readonly property real centreX: width / 2
    readonly property real centreY: height / 2

    function _polar(degrees, reach) {
        const angle = (degrees - 90) * Math.PI / 180;
        return Qt.point(centreX + Math.cos(angle) * reach,
                        centreY + Math.sin(angle) * reach);
    }

    Rectangle {
        anchors.centerIn: parent
        width: dial.outerRadius * 2
        height: width
        radius: width / 2
        color: RotorTheme.dialRing
        border.color: RotorTheme.border
        border.width: 1
    }

    // Corona dei gradi: tacche ogni 5°, piu' lunghe ogni 10° e sui punti cardinali.
    Canvas {
        id: scale

        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        Connections {
            target: RotorTheme

            function onDarkChanged() {
                scale.requestPaint();
            }
        }

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.translate(dial.centreX, dial.centreY);

            const outer = dial.outerRadius - 2;

            for (let deg = 0; deg < 360; deg += 5) {
                const decade = deg % 10 === 0;
                const cardinal = deg % 90 === 0;
                const length = cardinal ? dial.ringThickness * 0.85
                             : decade ? dial.ringThickness * 0.60
                             : dial.ringThickness * 0.35;

                ctx.save();
                ctx.rotate(deg * Math.PI / 180);
                ctx.beginPath();
                ctx.lineWidth = decade ? 2 : 1;
                ctx.strokeStyle = cardinal ? RotorTheme.primary
                                : decade ? RotorTheme.textSecondary
                                : RotorTheme.textDim;
                ctx.moveTo(0, -outer);
                ctx.lineTo(0, -outer + length);
                ctx.stroke();
                ctx.restore();
            }
        }
    }

    AzimuthMap {
        id: world

        anchors.centerIn: parent
        width: dial.mapRadius * 2
        height: width
        latitude: dial.latitude
        longitude: dial.longitude
    }

    // Numeri dei gradi ogni 30°, dritti e dentro il bordo della mappa, con un
    // alone del colore del fondo perche' restino leggibili sopra la mappa.
    Canvas {
        id: degrees

        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        Connections {
            target: RotorTheme

            function onDarkChanged() {
                degrees.requestPaint();
            }
        }

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const size = dial.digitSize;
            ctx.font = "bold " + size + "px " + RotorTheme.monoFamily;
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.lineJoin = "round";
            const reach = dial.mapRadius - size * 1.25;
            for (let deg = 0; deg < 360; deg += 30) {
                const p = dial._polar(deg, reach);
                const text = String(deg);
                ctx.lineWidth = Math.max(3, size * 0.3);
                ctx.strokeStyle = RotorTheme.dialFace;
                ctx.strokeText(text, p.x, p.y);
                ctx.fillStyle = deg % 90 === 0 ? RotorTheme.primary : RotorTheme.textSecondary;
                ctx.fillText(text, p.x, p.y);
            }
        }
    }

    // Settore vietato dai finecorsa software: si vede subito dove non si va.
    Shape {
        anchors.fill: parent
        visible: dial.limitMax - dial.limitMin < 359.5
        opacity: 0.85

        ShapePath {
            id: forbidden

            fillColor: Qt.rgba(0.85, 0.16, 0.16, 0.16)
            strokeColor: Qt.rgba(0.85, 0.16, 0.16, 0.40)
            strokeWidth: 1

            readonly property real from: dial.limitMax - 90
            readonly property real span: 360 - (dial.limitMax - dial.limitMin)
            readonly property real reach: dial.mapRadius

            startX: dial.centreX
            startY: dial.centreY

            PathLine {
                x: dial.centreX + Math.cos(forbidden.from * Math.PI / 180) * forbidden.reach
                y: dial.centreY + Math.sin(forbidden.from * Math.PI / 180) * forbidden.reach
            }

            PathAngleArc {
                centerX: dial.centreX
                centerY: dial.centreY
                radiusX: forbidden.reach
                radiusY: forbidden.reach
                startAngle: forbidden.from
                sweepAngle: forbidden.span
            }

            PathLine {
                x: dial.centreX
                y: dial.centreY
            }
        }
    }

    // Settore illuminato: dove "vede" l'antenna, con l'apertura del lobo.
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        visible: dial.hasPosition

        ShapePath {
            id: beam

            fillColor: Qt.rgba(0.13, 0.65, 0.45, 0.22)
            strokeColor: Qt.rgba(0.13, 0.65, 0.45, 0.55)
            strokeWidth: 1

            readonly property real from: dial.azimuth - dial.beamwidth / 2 - 90
            readonly property real span: dial.beamwidth
            readonly property real reach: dial.mapRadius

            startX: dial.centreX
            startY: dial.centreY

            PathLine {
                x: dial.centreX + Math.cos(beam.from * Math.PI / 180) * beam.reach
                y: dial.centreY + Math.sin(beam.from * Math.PI / 180) * beam.reach
            }

            PathAngleArc {
                centerX: dial.centreX
                centerY: dial.centreY
                radiusX: beam.reach
                radiusY: beam.reach
                startAngle: beam.from
                sweepAngle: beam.span
            }

            PathLine {
                x: dial.centreX
                y: dial.centreY
            }
        }
    }

    // Bersaglio: dove il rotore e' diretto ma non ancora arrivato.
    Item {
        anchors.centerIn: parent
        width: 2
        height: dial.mapRadius * 2
        visible: dial.target >= 0
        rotation: dial.target

        Repeater {
            model: 10

            Rectangle {
                required property int index

                anchors.horizontalCenter: parent.horizontalCenter
                y: 6 + index * (dial.mapRadius / 10)
                width: 2
                height: dial.mapRadius / 22
                color: RotorTheme.primary
            }
        }
    }

    // Il locatore digitato, se valido, appare dove si trova davvero.
    Rectangle {
        id: pin

        readonly property point position: world.project(dial.pinLatitude, dial.pinLongitude)

        visible: dial.pinValid
        x: world.x + position.x - width / 2
        y: world.y + position.y - height / 2
        width: 10
        height: 10
        radius: 5
        color: RotorTheme.accent
        border.color: RotorTheme.dialFace
        border.width: 2
    }

    // Ago: la punta indica la direzione dell'antenna.
    Item {
        id: needle

        anchors.centerIn: parent
        width: 8
        height: dial.mapRadius * 2
        visible: dial.hasPosition
        rotation: dial.azimuth

        Behavior on rotation {
            RotationAnimation {
                duration: 220
                direction: RotationAnimation.Shortest
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 2
            width: 4
            height: parent.height / 2 - 2
            radius: 2
            color: dial.moving ? RotorTheme.needleMoving : RotorTheme.needle
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.verticalCenter
            width: 3
            height: parent.height * 0.10
            radius: 1.5
            color: RotorTheme.textDim
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 14
        height: 14
        radius: 7
        color: RotorTheme.bgElevated
        border.color: RotorTheme.textSecondary
        border.width: 2
    }

    TapHandler {
        onTapped: (eventPoint) => {
            const dx = eventPoint.position.x - dial.centreX;
            const dy = eventPoint.position.y - dial.centreY;
            if (Math.sqrt(dx * dx + dy * dy) > dial.outerRadius)
                return;
            const degrees = (Math.atan2(dx, -dy) * 180 / Math.PI + 360) % 360;
            dial.bearingRequested(Math.round(degrees * 10) / 10);
        }
    }
}
