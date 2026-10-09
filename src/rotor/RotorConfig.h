// La configurazione del rotore, salvata nelle impostazioni di Decodium (gruppo
// "Rotor"). Nominativo e locatore non stanno qui: sono quelli di Decodium.
#pragma once

#include <QList>
#include <QSettings>
#include <QString>
#include <QVariantList>

#include <optional>

namespace decodium::rotor {

// Finecorsa software applicati prima di mandare un goto.
struct Limits {
    double azMin {0.0};
    double azMax {360.0};
    double elMin {0.0};
    double elMax {90.0};
};

// Una memoria di puntamento richiamabile con un tocco.
struct Preset {
    QString name;
    double az {0.0};
    std::optional<double> el;
};

struct Config {
    // --- stato ---
    // Il rotore si accende da solo: aprire Decodium non deve aprire una porta
    // seriale che l'operatore non ha scelto.
    bool enabled {false};

    // --- seriale ---
    QString port;
    int baudrate {9600};
    double timeout {3.0};
    int retries {3};
    QString model {QStringLiteral("auto")};
    double pollInterval {0.2};   // 5 Hz di telemetria
    bool simulate {false};

    // --- rete (telefono, web, software di stazione) ---
    // Chiusi finche' non si accendono: un server in piu' sulla rete di casa e'
    // una scelta, non un effetto collaterale.
    bool networkEnabled {false};
    QString bind {QStringLiteral("0.0.0.0")};
    int wsPort {8765};
    int httpPort {8080};
    int rotctldPort {4535};      // la 4533 e' del CAT condiviso di Decodium
    QString token;

    // --- sicurezza ---
    Limits limits;
    double parkAz {0.0};
    double parkEl {0.0};
    double stallTimeout {8.0};   // nessun movimento -> stop di emergenza
    bool stopOnClientLoss {true};
    double tolerance {1.0};      // gradi entro cui il target e' raggiunto

    // --- stazione ---
    double beamwidth {60.0};
    QList<Preset> presets;

    // --- stazioni sentite ---
    bool spotsEnabled {true};
    double spotTtl {900.0};
    int spotLimit {120};

    // --- mappa satellitare ---
    QString tileUrl {QStringLiteral(
        "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}")};
    QString tileAttribution {QStringLiteral("Imagery: Esri, Maxar, Earthstar Geographics")};
    double tileCacheDays {30.0};
    double tileCacheMb {400.0};

    // --- interfaccia ---
    bool darkTheme {true};

    // --- diagnostica ---
    int historySize {600};
    int trafficSize {200};

    void load(QSettings& settings);
    void save(QSettings& settings) const;

    double clampAz(double degrees) const;
    double clampEl(double degrees) const;
};

QVariantList presetsToVariant(const QList<Preset>& presets);

}  // namespace decodium::rotor
