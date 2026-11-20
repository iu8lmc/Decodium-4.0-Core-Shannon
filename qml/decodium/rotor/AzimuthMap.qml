import QtQuick

import "worldmap.js" as World

/*!
    Mappa azimutale equidistante centrata sul QTH.

    In questa proiezione ogni direzione letta sul quadrante e' la rotta vera
    verso quel punto del mondo, e la distanza dal centro cresce in modo
    proporzionale ai chilometri: e' la mappa che il rotore "vede".
    Il disegno e' su tela e viene rifatto solo quando cambia il QTH, il tema o
    la dimensione, non a ogni frame di rotazione.
*/
Canvas {
    id: map

    property real latitude: 41.5
    property real longitude: 12.5
    property bool grid: true

    //: Cerchi di distanza tracciati sulla mappa, in chilometri.
    readonly property var ranges: [5000, 10000, 15000]
    readonly property real earthRadiusKm: 6371.0
    readonly property real halfWorldKm: Math.PI * earthRadiusKm

    antialiasing: true
    renderStrategy: Canvas.Cooperative

    onLatitudeChanged: requestPaint()
    onLongitudeChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    Connections {
        target: RotorTheme

        function onDarkChanged() {
            map.requestPaint();
        }
    }

    /*! Punto geografico proiettato in coordinate di tela. */
    function project(lat, lon) {
        const rad = Math.PI / 180;
        const lat0 = map.latitude * rad;
        const dlon = (lon - map.longitude) * rad;
        const phi = lat * rad;

        const cosC = Math.sin(lat0) * Math.sin(phi)
                   + Math.cos(lat0) * Math.cos(phi) * Math.cos(dlon);
        const c = Math.acos(Math.min(1, Math.max(-1, cosC)));

        const y = Math.sin(dlon) * Math.cos(phi);
        const x = Math.cos(lat0) * Math.sin(phi) - Math.sin(lat0) * Math.cos(phi) * Math.cos(dlon);
        const azimuth = Math.atan2(y, x);

        const reach = radius() * c / Math.PI;
        return Qt.point(centre().x + reach * Math.sin(azimuth),
                        centre().y - reach * Math.cos(azimuth));
    }

    function radius() {
        return Math.min(width, height) / 2;
    }

    function centre() {
        return Qt.point(width / 2, height / 2);
    }

    function _traceRing(ctx, ring) {
        // Un anello che passa vicino all'antipodo si spalanca lungo il bordo:
        // dove il salto e' assurdo si chiude il tratto e se ne apre un altro,
        // cosi' la costa resta al suo posto invece di attraversare la mappa.
        const jump = radius() * 1.1;
        let previous = null;
        let open = false;

        for (let i = 0; i < ring.length; i += 2) {
            const point = project(ring[i + 1], ring[i]);
            const far = previous !== null
                        && Math.hypot(point.x - previous.x, point.y - previous.y) > jump;

            if (!open || far) {
                if (open) {
                    ctx.closePath();
                    ctx.fill();
                    ctx.stroke();
                }
                ctx.beginPath();
                ctx.moveTo(point.x, point.y);
                open = true;
            } else {
                ctx.lineTo(point.x, point.y);
            }
            previous = point;
        }

        if (open) {
            ctx.closePath();
            ctx.fill();
            ctx.stroke();
        }
    }

    function _drawGraticule(ctx) {
        const step = 3;
        ctx.strokeStyle = RotorTheme.mapGrid;
        ctx.lineWidth = 1;

        for (let lat = -60; lat <= 60; lat += 30) {
            ctx.beginPath();
            for (let lon = -180; lon <= 180; lon += step) {
                const point = project(lat, lon);
                if (lon === -180)
                    ctx.moveTo(point.x, point.y);
                else
                    ctx.lineTo(point.x, point.y);
            }
            ctx.stroke();
        }

        for (let lon = -180; lon < 180; lon += 30) {
            ctx.beginPath();
            for (let lat = -87; lat <= 87; lat += step) {
                const point = project(lat, lon);
                if (lat === -87)
                    ctx.moveTo(point.x, point.y);
                else
                    ctx.lineTo(point.x, point.y);
            }
            ctx.stroke();
        }
    }

    onPaint: {
        const ctx = getContext("2d");
        const middle = centre();
        const reach = radius();

        ctx.reset();
        ctx.save();

        // Il disco della mappa fa anche da maschera: nulla esce dal quadrante.
        ctx.beginPath();
        ctx.arc(middle.x, middle.y, reach, 0, 2 * Math.PI);
        ctx.clip();

        ctx.fillStyle = RotorTheme.dialFace;
        ctx.fill();

        if (map.grid)
            _drawGraticule(ctx);

        ctx.strokeStyle = RotorTheme.mapCoast;
        ctx.lineWidth = 1;
        for (let i = 0; i < World.land.length; ++i) {
            ctx.fillStyle = RotorTheme.landColour(i);
            _traceRing(ctx, World.land[i]);
        }

        // Cerchi di distanza: in questa proiezione sono cerchi veri.
        ctx.strokeStyle = RotorTheme.mapGrid;
        ctx.lineWidth = 1;
        for (let r = 0; r < map.ranges.length; ++r) {
            const km = map.ranges[r];
            if (km >= map.halfWorldKm)
                continue;
            ctx.beginPath();
            ctx.arc(middle.x, middle.y, reach * km / map.halfWorldKm, 0, 2 * Math.PI);
            ctx.stroke();
        }

        ctx.restore();
    }
}
