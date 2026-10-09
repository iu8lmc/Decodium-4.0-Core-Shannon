// Calcoli di puntamento: locatore Maidenhead, rilevamento e distanza.
//
// Serve a puntare l'antenna scrivendo un locatore invece di un angolo: da un
// locatore (4, 6 o 8 caratteri) si ricava il centro del suo riquadro, e da
// quello rotta breve, rotta lunga e distanza sul grande cerchio (raggio
// terrestre 6371 km). La rotta lunga e' la breve piu' 180 gradi.
#pragma once

#include <QString>
#include <QVariantMap>

namespace decodium::rotor {

constexpr double kEarthRadiusKm = 6371.0;

struct Position {
    double latitude {0.0};
    double longitude {0.0};
};

struct Bearing {
    double shortPath {0.0};
    double longPath {0.0};
    double distanceKm {0.0};
    Position target;

    // Come lo vogliono i client: gradi a un decimale, chilometri interi.
    QVariantMap toMap() const;
};

// False se il locatore non e' valido.
bool locatorToPosition(const QString& locator, Position* position);
QString positionToLocator(const Position& position, int precision = 6);
Bearing bearingBetween(const Position& origin, const Position& target);
// False se uno dei due locatori non e' valido.
bool bearingToLocator(const QString& myLocator, const QString& targetLocator, Bearing* bearing);
bool isValidLocator(const QString& locator);

}  // namespace decodium::rotor
