import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// AstroPanel — C14: calcolatrice astronomica (Luna, Doppler EME, Sole)
// Theme.* sostituiti con costanti inline; lat/lon da bridge.grid
Rectangle {
    id: astroPanel
    color: Qt.rgba(12/255, 12/255, 22/255, 0.98)
    radius: 8
    border.color: Qt.rgba(1,1,1,0.15)
    border.width: 1

    property real myLat: 0.0
    property real myLon: 0.0

    // Moon position calculation (simplified)
    // Reference: new moon Jan 11, 2024 11:57 UTC
    readonly property real synodicMonth: 29.53059

    property real moonPhase: 0.0
    property real moonAzimuth: 0.0
    property real moonElevation: 0.0
    property real moonDopplerHz: 0.0
    property real sunElevation: 0.0

    // Aggiorna lat/lon dalla griglia bridge
    function updateLatLon() {
        var g = bridge.grid
        if (g.length >= 4) {
            astroPanel.myLat = bridge.latFromGrid(g)
            astroPanel.myLon = bridge.lonFromGrid(g)
        }
    }

    Connections {
        target: bridge
        function onGridChanged() { astroPanel.updateLatLon() }
    }

    Component.onCompleted: updateLatLon()

    Timer {
        id: astroTimer
        interval: 10000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: updateAstro()
    }

    function updateAstro() {
        var now = new Date();
        var utcHours = now.getUTCHours() + now.getUTCMinutes() / 60.0 + now.getUTCSeconds() / 3600.0;

        // Julian Date
        var y = now.getUTCFullYear();
        var m = now.getUTCMonth() + 1;
        var d = now.getUTCDate();
        if (m <= 2) { y--; m += 12; }
        var A = Math.floor(y / 100);
        var B = 2 - A + Math.floor(A / 4);
        var jd = Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + d + utcHours / 24.0 + B - 1524.5;

        // Moon phase (synodic)
        var daysSinceNew = jd - 2460320.998; // JD of ref new moon
        var phase = ((daysSinceNew % synodicMonth) + synodicMonth) % synodicMonth;
        moonPhase = phase / synodicMonth;

        // Simplified moon position (low-precision formula)
        var T = (jd - 2451545.0) / 36525.0;

        // Moon ecliptic longitude (degrees)
        var L0 = 218.3164477 + 481267.88123421 * T;
        var M_moon = 134.9633964 + 477198.8675055 * T;
        var M_sun = 357.5291092 + 35999.0502909 * T;
        var D = 297.8501921 + 445267.1114034 * T;
        var F = 93.2720950 + 483202.0175233 * T;

        // Normalize to 0-360
        L0 = ((L0 % 360) + 360) % 360;
        M_moon = ((M_moon % 360) + 360) % 360 * Math.PI / 180;
        M_sun  = ((M_sun  % 360) + 360) % 360 * Math.PI / 180;
        D = ((D % 360) + 360) % 360 * Math.PI / 180;
        F = ((F % 360) + 360) % 360 * Math.PI / 180;

        // Major perturbations
        var moonLon = L0
            + 6.289 * Math.sin(M_moon)
            - 1.274 * Math.sin(2 * D - M_moon)
            + 0.658 * Math.sin(2 * D)
            + 0.214 * Math.sin(2 * M_moon)
            - 0.186 * Math.sin(M_sun);

        var moonLat = 5.128 * Math.sin(F)
            + 0.281 * Math.sin(M_moon + F)
            + 0.078 * Math.sin(2 * D - F);

        // Ecliptic to equatorial
        var eps = 23.4393 * Math.PI / 180;
        var lonRad = moonLon * Math.PI / 180;
        var latRad = moonLat * Math.PI / 180;

        var ra = Math.atan2(Math.sin(lonRad) * Math.cos(eps) - Math.tan(latRad) * Math.sin(eps),
                            Math.cos(lonRad));
        var dec = Math.asin(Math.sin(latRad) * Math.cos(eps) + Math.cos(latRad) * Math.sin(eps) * Math.sin(lonRad));

        // Hour angle
        var gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0);
        gmst = ((gmst % 360) + 360) % 360;
        var ha = (gmst + myLon - ra * 180 / Math.PI) * Math.PI / 180;

        var latRad2 = myLat * Math.PI / 180;
        var sinAlt = Math.sin(latRad2) * Math.sin(dec) + Math.cos(latRad2) * Math.cos(dec) * Math.cos(ha);
        moonElevation = Math.asin(sinAlt) * 180 / Math.PI;

        var cosAz = (Math.sin(dec) - Math.sin(latRad2) * sinAlt) / (Math.cos(latRad2) * Math.cos(Math.asin(sinAlt)));
        cosAz = Math.max(-1, Math.min(1, cosAz));
        moonAzimuth = Math.acos(cosAz) * 180 / Math.PI;
        if (Math.sin(ha) > 0) moonAzimuth = 360 - moonAzimuth;

        // EME Doppler (round-trip a 144.1 MHz)
        var vRot = 0.464 * Math.cos(latRad2) * Math.sin(ha) * Math.cos(dec);
        moonDopplerHz = -(2 * vRot / 299792.458) * 144100000;

        // Sun elevation (simplified)
        var sunLon = (280.46646 + 36000.76983 * T) * Math.PI / 180;
        var sunRa  = Math.atan2(Math.sin(sunLon) * Math.cos(eps), Math.cos(sunLon));
        var sunDec = Math.asin(Math.sin(eps) * Math.sin(sunLon));
        var sunHa  = (gmst + myLon - sunRa * 180 / Math.PI) * Math.PI / 180;
        var sinSunAlt = Math.sin(latRad2) * Math.sin(sunDec) + Math.cos(latRad2) * Math.cos(sunDec) * Math.cos(sunHa);
        sunElevation = Math.asin(sinSunAlt) * 180 / Math.PI;
    }

    function phaseText() {
        if (moonPhase < 0.03 || moonPhase > 0.97) return "New Moon";
        if (moonPhase < 0.22) return "Waxing Crescent";
        if (moonPhase < 0.28) return "First Quarter";
        if (moonPhase < 0.47) return "Waxing Gibbous";
        if (moonPhase < 0.53) return "Full Moon";
        if (moonPhase < 0.72) return "Waning Gibbous";
        if (moonPhase < 0.78) return "Last Quarter";
        return "Waning Crescent";
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Header
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "ASTRO / EME"
                font.family: "Consolas"
                font.pixelSize: 10
                font.letterSpacing: 2
                font.bold: true
                color: "#00BCD4"
            }
            Item { Layout.fillWidth: true }
            Text {
                text: phaseText()
                font.family: "Consolas"
                font.pixelSize: 10
                color: "#90A4AE"
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1,1,1,0.15) }

        // Moon info grid
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 4

            Text { text: "Az";      font.family: "Consolas"; font.pixelSize: 10; color: "#90A4AE" }
            Text { text: moonAzimuth.toFixed(1) + "\u00B0"; font.family: "Consolas"; font.pixelSize: 12; color: "#ECEFF1" }
            Text { text: "El";      font.family: "Consolas"; font.pixelSize: 10; color: "#90A4AE" }
            Text {
                text: moonElevation.toFixed(1) + "\u00B0"
                font.family: "Consolas"
                font.pixelSize: 12
                color: moonElevation > 0 ? "#69F0AE" : "#f44336"
            }

            Text { text: "Doppler"; font.family: "Consolas"; font.pixelSize: 10; color: "#90A4AE" }
            Text { text: (moonDopplerHz >= 0 ? "+" : "") + moonDopplerHz.toFixed(0) + " Hz"; font.family: "Consolas"; font.pixelSize: 12; color: "#00BCD4" }
            Text { text: "Sun El";  font.family: "Consolas"; font.pixelSize: 10; color: "#90A4AE" }
            Text {
                text: sunElevation.toFixed(1) + "\u00B0"
                font.family: "Consolas"
                font.pixelSize: 12
                color: sunElevation > 0 ? "#FFC107" : "#90A4AE"
            }

            Text { text: "Phase";   font.family: "Consolas"; font.pixelSize: 10; color: "#90A4AE" }
            Text { text: (moonPhase * 100).toFixed(0) + "%"; font.family: "Consolas"; font.pixelSize: 12; color: "#ECEFF1" }
            Text { text: "Illum";   font.family: "Consolas"; font.pixelSize: 10; color: "#90A4AE" }
            Text {
                text: {
                    var illum = (1 - Math.cos(moonPhase * 2 * Math.PI)) / 2;
                    return (illum * 100).toFixed(0) + "%";
                }
                font.family: "Consolas"
                font.pixelSize: 12
                color: "#ECEFF1"
            }
        }

        // Griglia stazione
        Text {
            visible: bridge.grid.length >= 4
            text: "Grid: " + bridge.grid + "  (" + myLat.toFixed(2) + "\u00B0, " + myLon.toFixed(2) + "\u00B0)"
            font.family: "Consolas"
            font.pixelSize: 9
            color: "#546E7A"
        }

        // EME window indicator
        Rectangle {
            Layout.fillWidth: true
            height: 24
            radius: 6
            color: moonElevation > 5 ? Qt.rgba(0, 1, 0, 0.1) : Qt.rgba(1, 0.2, 0.2, 0.1)
            border.color: moonElevation > 5 ? "#69F0AE" : "#f44336"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: moonElevation > 5 ? "EME WINDOW OPEN" : "EME WINDOW CLOSED"
                font.family: "Consolas"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1
                color: moonElevation > 5 ? "#69F0AE" : "#f44336"
            }
        }
    }
}
