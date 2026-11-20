// Le stazioni che Decodium sta sentendo, viste dalla parte del rotore.
//
// Ogni decode con un locatore diventa un punto sulla mappa con la sua rotta e
// la sua distanza; i decode senza locatore aggiornano solo l'ora e il rapporto
// di una stazione gia' nota. Il libro tiene l'ultimo quarto d'ora e niente di
// piu': in una serata di FT8 passano migliaia di righe, e quello che serve al
// rotore e' chi c'e' adesso.
#pragma once

#include "RotorGeo.h"

#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace decodium::rotor {

struct Spot {
    QString call;
    QString grid;
    double latitude {0.0};
    double longitude {0.0};
    double azimuth {0.0};
    double distanceKm {0.0};
    std::optional<int> snr;
    QString mode;
    qint64 frequencyHz {0};
    QString source;
    QString entity;
    QString comment;
    double firstSeen {0.0};
    double lastSeen {0.0};
    int count {0};

    QVariantMap toMap(double now) const;
};

class SpotBook {
public:
    // Quanto resta sulla mappa una stazione sentita, e quante se ne tengono.
    void configure(double ttlSeconds, int limit);
    // Il QTH da cui si contano rotta e distanza; il nominativo proprio non e'
    // un bersaglio e non entra nel libro.
    void setStation(const QString& callsign, const QString& locator);

    // Registra una stazione. Senza posizione e senza locatore valido non c'e'
    // niente da mettere sulla mappa e la riga si scarta.
    void note(const QString& call, const QString& grid, const QString& source,
              std::optional<int> snr = std::nullopt, const QString& mode = QString(),
              qint64 frequencyHz = 0, std::optional<Position> position = std::nullopt,
              const QString& entity = QString(), const QString& comment = QString());

    // Il testo di un decode: chi ha trasmesso e da quale riquadro.
    //   "CQ DX EA8ABC IL18"      -> EA8ABC, IL18
    //   "IU8LMC EA8ABC RR73"     -> EA8ABC, (nessun riquadro)
    static bool stationFromMessage(const QString& text, QString* call, QString* grid);

    QVariantList entries(int limit = -1);
    QVariantMap find(const QString& call);
    void clear();
    int version() const { return m_version; }
    int count() const { return m_spots.size(); }

    QString workingCall;
    qint64 dialFrequency {0};

private:
    void aim(Spot& spot);
    void purge(double now);
    std::optional<Position> home();

    QHash<QString, Spot> m_spots;
    QString m_callsign;
    QString m_locator;
    QString m_originLocator;
    std::optional<Position> m_origin;
    double m_ttl {900.0};
    int m_limit {120};
    int m_version {0};
};

}  // namespace decodium::rotor
