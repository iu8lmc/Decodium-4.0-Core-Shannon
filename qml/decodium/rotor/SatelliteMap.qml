import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtLocation
import QtPositioning

/*!
    Mappa satellitare interattiva sotto il quadrante.

    I riquadri arrivano dal gateway stesso, che fa da cache: quello che si e'
    gia' guardato resta su disco e si rivede anche con la rete giu'. Sopra la
    mappa compaiono le stazioni che Decodium sta sentendo, ognuna con la sua
    rotta dal QTH; toccarne una porta l'antenna li' sopra.
*/
Item {
    id: view

    property real homeLatitude: 41.5
    property real homeLongitude: 12.5
    property string selectedCall: ""

    //: Vero dopo la prima inquadratura: da li' in poi comanda l'operatore.
    property bool framed: false

    // La larghezza definitiva arriva dopo il primo passaggio del layout: e'
    // quella che dice quanto mondo ci sta, quindi si inquadra allora.
    onWidthChanged: {
        if (!framed && width > 0) {
            framed = true;
            centreOnHome();
        }
    }

    readonly property var selectedSpot: {
        const list = rotor.spots;
        for (let i = 0; i < list.length; ++i) {
            if (list[i].call === view.selectedCall)
                return list[i];
        }
        return null;
    }

    readonly property var home: QtPositioning.coordinate(homeLatitude, homeLongitude)

    /*! Il QTH al centro e quanto mondo ci sta nella fascia disponibile. */
    function centreOnHome() {
        map.center = view.home;
        map.zoomLevel = Math.max(map.minimumZoomLevel,
                                 Math.min(5, Math.log2(Math.max(width, 320) / 256)));
    }

    /*! Inquadra tutte le stazioni sentite, per vedere dove si sta aprendo la banda. */
    function frameSpots() {
        if (rotor.spotCount === 0) {
            centreOnHome();
            return;
        }
        map.fitViewportToMapItems();
    }

    /*! Ortodromia campionata: la rotta vera, non il segmento sulla carta. */
    function greatCircle(from, to) {
        const rad = Math.PI / 180;
        const lat1 = from.latitude * rad;
        const lon1 = from.longitude * rad;
        const lat2 = to.latitude * rad;
        const lon2 = to.longitude * rad;

        const delta = 2 * Math.asin(Math.sqrt(
            Math.pow(Math.sin((lat1 - lat2) / 2), 2)
            + Math.cos(lat1) * Math.cos(lat2) * Math.pow(Math.sin((lon1 - lon2) / 2), 2)));

        const steps = 96;
        const legs = [];
        let leg = [];
        let previous = null;

        for (let i = 0; i <= steps; ++i) {
            const f = i / steps;
            let lat, lon;

            if (delta < 1e-6) {
                lat = from.latitude;
                lon = from.longitude;
            } else {
                const a = Math.sin((1 - f) * delta) / Math.sin(delta);
                const b = Math.sin(f * delta) / Math.sin(delta);
                const x = a * Math.cos(lat1) * Math.cos(lon1) + b * Math.cos(lat2) * Math.cos(lon2);
                const y = a * Math.cos(lat1) * Math.sin(lon1) + b * Math.cos(lat2) * Math.sin(lon2);
                const z = a * Math.sin(lat1) + b * Math.sin(lat2);
                lat = Math.atan2(z, Math.sqrt(x * x + y * y)) / rad;
                lon = Math.atan2(y, x) / rad;
            }

            // Sull'antimeridiano la linea va spezzata, altrimenti la carta la
            // fa tornare indietro attraversando tutto il mondo.
            if (previous !== null && Math.abs(lon - previous) > 180) {
                legs.push(leg);
                leg = [];
            }
            leg.push(QtPositioning.coordinate(lat, lon));
            previous = lon;
        }
        legs.push(leg);
        return legs;
    }

    Plugin {
        id: tiles

        name: "osm"

        PluginParameter {
            name: "osm.useragent"
            value: "DecoRotor/1.0 (rotore d'antenna amatoriale)"
        }

        // Con il gateway in piedi i riquadri arrivano da li'; se la porta HTTP
        // non ha risposto si resta sulla cartografia stradale di OpenStreetMap.
        PluginParameter {
            name: "osm.mapping.providersrepository.disabled"
            value: rotor.tileEndpoint.length > 0
        }

        PluginParameter {
            name: "osm.mapping.custom.host"
            value: rotor.tileEndpoint
        }

        PluginParameter {
            name: "osm.mapping.custom.mapcopyright"
            value: rotor.tileAttribution
        }
    }

    Map {
        id: map

        anchors.fill: parent
        plugin: tiles
        center: view.home
        minimumZoomLevel: 1.5
        maximumZoomLevel: 17
        copyrightsVisible: false
        color: RotorTheme.dialFace

        onSupportedMapTypesChanged: map.chooseMapType()

        Component.onCompleted: map.chooseMapType()

        function chooseMapType() {
            for (let i = 0; i < supportedMapTypes.length; ++i) {
                if (supportedMapTypes[i].style === MapType.CustomMap) {
                    activeMapType = supportedMapTypes[i];
                    return;
                }
            }
            if (supportedMapTypes.length > 0)
                activeMapType = supportedMapTypes[0];
        }

        DragHandler {
            target: null
            onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            rotationScale: 1 / 120
            property: "zoomLevel"
        }

        PinchHandler {
            id: pinch

            target: null

            property var origin: QtPositioning.coordinate()

            onActiveChanged: {
                if (active)
                    origin = map.toCoordinate(pinch.centroid.position, false);
            }

            onScaleChanged: (delta) => {
                map.zoomLevel += Math.log2(delta);
                map.alignCoordinateToPoint(pinch.origin, pinch.centroid.position);
            }
        }

        // Rotta dal QTH alla stazione scelta, spezzata dove passa
        // l'antimeridiano.
        MapItemView {
            model: view.selectedSpot === null
                   ? []
                   : view.greatCircle(view.home,
                                      QtPositioning.coordinate(view.selectedSpot.lat,
                                                               view.selectedSpot.lon))

            delegate: MapPolyline {
                required property var modelData

                line.width: 3
                line.color: RotorTheme.primary
                opacity: 0.95
                path: modelData
                z: 2
            }
        }

        // Il proprio QTH.
        MapQuickItem {
            coordinate: view.home
            anchorPoint.x: 9
            anchorPoint.y: 9
            z: 3

            sourceItem: Item {
                width: 18
                height: 18

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: RotorTheme.accent
                    border.width: 2
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 6
                    height: 6
                    radius: 3
                    color: RotorTheme.accent
                }
            }
        }

        // Le stazioni sentite da Decodium.
        MapItemView {
            model: rotor.spots

            delegate: MapQuickItem {
                id: pin

                required property var modelData

                readonly property bool chosen: modelData.call === view.selectedCall
                readonly property bool working: modelData.call === rotor.workingCall
                readonly property real freshness: Math.max(0, 1 - modelData.age / 900)

                coordinate: QtPositioning.coordinate(modelData.lat, modelData.lon)
                anchorPoint.x: 7
                anchorPoint.y: 7
                z: chosen ? 6 : 4

                sourceItem: Item {
                    width: 14
                    height: 14

                    Rectangle {
                        id: dot

                        anchors.centerIn: parent
                        width: pin.chosen ? 14 : 11
                        height: width
                        radius: width / 2
                        color: pin.working ? RotorTheme.warning : RotorTheme.primary
                        opacity: 0.35 + 0.65 * pin.freshness
                        border.color: pin.chosen ? RotorTheme.textPrimary : Qt.rgba(0, 0, 0, 0.55)
                        border.width: pin.chosen ? 2 : 1

                        Behavior on width {
                            NumberAnimation { duration: 120 }
                        }
                    }

                    Text {
                        anchors.left: dot.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: dot.verticalCenter
                        visible: pin.chosen || pin.working
                                 || rotor.spotCount <= 15 || map.zoomLevel > 3.2
                        text: pin.modelData.call
                        color: RotorTheme.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        style: Text.Outline
                        styleColor: RotorTheme.dark ? "#000000" : "#FFFFFF"
                    }

                    TapHandler {
                        onTapped: view.selectedCall = pin.modelData.call
                    }

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
    }

    // --- fascia superiore: cosa sta arrivando da Decodium -------------------
    Rectangle {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 34
        color: RotorTheme.glass

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 6
            spacing: 10

            StatusLed {
                label: rotor.spotsEnabled
                       ? qsTr("DECODIUM %1").arg(rotor.decodiumStation)
                       : qsTr("LISTENING OFF")
                colour: rotor.spotsEnabled ? RotorTheme.accent : RotorTheme.danger
                blinking: false
            }

            Text {
                text: qsTr("%n station(s)", "", rotor.spotCount)
                color: RotorTheme.textSecondary
                font.pixelSize: RotorTheme.fontSmall
            }

            Item { Layout.fillWidth: true }

            Repeater {
                model: [
                    { glyph: "+", action: "in" },
                    { glyph: "−", action: "out" },
                    { glyph: "⤢", action: "fit" },
                    { glyph: "⌂", action: "home" },
                    { glyph: "✕", action: "clear" }
                ]

                Rectangle {
                    id: knob

                    required property var modelData

                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 24
                    radius: 6
                    color: touch.hovered ? RotorTheme.bgElevated : "transparent"
                    border.color: RotorTheme.borderSoft
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: knob.modelData.glyph
                        color: RotorTheme.textSecondary
                        font.pixelSize: 13
                        font.bold: true
                    }

                    HoverHandler {
                        id: touch

                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: {
                            switch (knob.modelData.action) {
                            case "in": map.zoomLevel = Math.min(map.maximumZoomLevel, map.zoomLevel + 1); break;
                            case "out": map.zoomLevel = Math.max(map.minimumZoomLevel, map.zoomLevel - 1); break;
                            case "fit": view.frameSpots(); break;
                            case "home": view.centreOnHome(); break;
                            case "clear": rotor.clearSpots(); view.selectedCall = ""; break;
                            }
                        }
                    }
                }
            }
        }
    }

    // --- scheda della stazione scelta --------------------------------------
    Rectangle {
        id: card

        anchors.left: parent.left
        anchors.bottom: footer.top
        anchors.margins: 10
        width: 252
        height: 108
        radius: 10
        color: RotorTheme.glass
        border.color: RotorTheme.border
        border.width: 1
        visible: view.selectedSpot !== null

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: view.selectedSpot ? view.selectedSpot.call : ""
                    color: RotorTheme.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "✕"
                    color: close.hovered ? RotorTheme.textPrimary : RotorTheme.textDim
                    font.pixelSize: 13

                    HoverHandler {
                        id: close

                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: view.selectedCall = ""
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: view.selectedSpot
                      ? qsTr("%1 · %2° · %3 km").arg(view.selectedSpot.grid)
                            .arg(view.selectedSpot.az.toFixed(1))
                            .arg(view.selectedSpot.km)
                      : ""
                color: RotorTheme.textSecondary
                font.pixelSize: RotorTheme.fontSmall
                font.family: RotorTheme.monoFamily
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: {
                    if (view.selectedSpot === null)
                        return "";
                    const parts = [];
                    if (view.selectedSpot.mode)
                        parts.push(view.selectedSpot.mode);
                    if (view.selectedSpot.snr !== null && view.selectedSpot.snr !== undefined)
                        parts.push(view.selectedSpot.snr + " dB");
                    parts.push(Math.round(view.selectedSpot.age) + " s fa");
                    return parts.join(" · ");
                }
                color: RotorTheme.textDim
                font.pixelSize: RotorTheme.fontSmall
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                CommandButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    text: qsTr("AIM")
                    kind: CommandButton.Kind.Primary
                    onClicked: rotor.pointAtSpot(view.selectedCall)
                }

                CommandButton {
                    Layout.preferredWidth: 94
                    Layout.preferredHeight: 30
                    text: qsTr("MEMORY")
                    onClicked: rotor.savePresetFromSpot(view.selectedCall)
                }
            }
        }
    }

    // --- fascia inferiore: puntamento per locatore --------------------------
    LocatorBar {
        id: footer

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.margins: 6
        text: rotor.tileEndpoint.length > 0 ? rotor.tileAttribution
                                            : qsTr("© OpenStreetMap contributors")
        color: RotorTheme.textDim
        font.pixelSize: 9
        opacity: 0.85
    }
}
