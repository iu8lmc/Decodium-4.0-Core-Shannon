pragma Singleton

import QtQuick

QtObject {
    id: theme

    // Due letture dello stesso strumento: quadrante chiaro come il display del
    // control box in shack illuminato, fondo notturno per le gare a luce spenta.
    property bool dark: true

    readonly property color bgDeep: dark ? "#060910" : "#C9D0DB"
    readonly property color bgPanel: dark ? "#0D141F" : "#FBFCFE"
    readonly property color bgElevated: dark ? "#131C2A" : "#EAEFF6"
    readonly property color bgHeader: dark ? "#16202F" : "#DCE3EE"

    readonly property color primary: dark ? "#38BDF8" : "#0B6FC2"
    readonly property color secondary: dark ? "#22D3EE" : "#0E7490"
    readonly property color accent: dark ? "#34D399" : "#15803D"
    readonly property color warning: dark ? "#FBBF24" : "#B45309"
    readonly property color danger: dark ? "#F87171" : "#C81E1E"

    readonly property color textPrimary: dark ? "#E6EDF6" : "#0F1620"
    readonly property color textSecondary: dark ? "#94A6BC" : "#48586C"
    readonly property color textDim: dark ? "#5C6E85" : "#78879A"

    readonly property color border: dark ? Qt.rgba(0.22, 0.74, 0.97, 0.22)
                                        : Qt.rgba(0.04, 0.43, 0.76, 0.30)
    readonly property color borderSoft: dark ? Qt.rgba(0.22, 0.74, 0.97, 0.10)
                                            : Qt.rgba(0.04, 0.43, 0.76, 0.14)
    readonly property color glass: dark ? Qt.rgba(0.05, 0.08, 0.13, 0.72)
                                        : Qt.rgba(1.0, 1.0, 1.0, 0.78)

    // --- quadrante e mappa azimutale ---------------------------------------
    readonly property color dialFace: dark ? "#0A1523" : "#FFFFFF"
    readonly property color dialRing: dark ? "#0F1B2B" : "#F4F7FB"
    readonly property color mapGrid: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.13)
    readonly property color mapCoast: dark ? "#3E7A9B" : "#5D7F97"
    readonly property color needle: "#D8352A"
    readonly property color needleMoving: dark ? "#FBBF24" : "#E97C0B"

    // Tinte alternate solo per staccare una massa continentale dall'altra:
    // non hanno alcun significato geografico o politico.
    readonly property var mapLand: dark
        ? ["#1D3B31", "#3A3122", "#20344F", "#33254A", "#3C3324", "#1C3B3A"]
        : ["#C9E1C2", "#EBD8BF", "#C7D9EF", "#DED1E9", "#F1DDC7", "#C9E3DF"]

    readonly property int radius: 14
    readonly property int spacing: 12
    readonly property int padding: 16

    readonly property string monoFamily: "Consolas"
    readonly property int fontSmall: 11
    readonly property int fontBody: 13
    readonly property int fontTitle: 15
    readonly property int fontReadout: 38

    function landColour(index) {
        return theme.mapLand[index % theme.mapLand.length];
    }
}
