import QtQuick
import QtQuick.Layouts

/*!
    Puntamento per locatore in una riga sola, in fondo alla mappa: si scrive
    il riquadro, la meta compare sulla mappa e sul quadrante, e i due tasti
    mandano l'antenna per rotta breve o per rotta lunga.
*/
Rectangle {
    id: bar

    implicitHeight: 44
    color: RotorTheme.glass

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        FieldInput {
            id: locatorField

            Layout.preferredWidth: 176
            implicitHeight: 32
            placeholderText: qsTr("Locator, e.g. FN31pr")
            font.capitalization: Font.AllUppercase
            onTextChanged: rotor.computeBearing(text)
            onAccepted: rotor.gotoLocator(text, false)
        }

        CommandButton {
            Layout.preferredWidth: 76
            Layout.preferredHeight: 32
            text: qsTr("SHORT")
            kind: CommandButton.Kind.Primary
            enabled: rotor.bearingValid
            onClicked: rotor.gotoLocator(locatorField.text, false)
        }

        CommandButton {
            Layout.preferredWidth: 76
            Layout.preferredHeight: 32
            text: qsTr("LONG")
            enabled: rotor.bearingValid
            onClicked: rotor.gotoLocator(locatorField.text, true)
        }

        Text {
            Layout.fillWidth: true
            text: rotor.bearingValid
                  ? qsTr("short %1° · long %2° · %3 km")
                        .arg(rotor.shortPath.toFixed(1))
                        .arg(rotor.longPath.toFixed(1))
                        .arg(rotor.distanceKm.toFixed(0))
                  : qsTr("Reference QTH: %1").arg(rotor.locator)
            color: rotor.bearingValid ? RotorTheme.accent : RotorTheme.textDim
            font.pixelSize: RotorTheme.fontSmall
            font.family: RotorTheme.monoFamily
            elide: Text.ElideRight
        }
    }
}
