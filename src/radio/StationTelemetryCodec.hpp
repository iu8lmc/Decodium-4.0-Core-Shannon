// Codifica compatta "stazione + meteo" nel payload FT8/FT4 tipo 0.5
// (i3=0, n3=5, telemetria: 71 bit grezzi, nessun campo nominativo).
//
// Non tocca il protocollo: la stringa esadecimale a 18 caratteri prodotta
// da encodeStationTelemetryHex() e' pensata per passare cosi' com'e' a
// decodium::txmsg::encodeFt8()/encodeFt2() (il dispatcher pack_message77_cpp
// instrada gia' oggi qualunque testo tutto-esadecimale <=18 caratteri verso
// il tipo telemetria). In ricezione, unpack77_cpp restituisce la stessa
// stringa esadecimale: decodeStationTelemetryHex() la reinterpreta.
//
// Il byte di firma (0xC3, bit piu' alto = 1) garantisce che il primo
// carattere esadecimale non sia mai '0', cosi' il taglio degli zeri
// iniziali che unpack77_cpp applica al testo prima di restituirlo non
// tronca mai il payload.
//
// By IU8LMC

#ifndef STATIONTELEMETRYCODEC_H
#define STATIONTELEMETRYCODEC_H

#include <QString>
#include <QStringList>

namespace decodium
{
namespace telemetry
{

// Enumerazione condizioni cielo (3 bit, 0-7).
enum class SkyCondition : int
{
    Clear = 0,
    PartlyCloudy = 1,
    Cloudy = 2,
    Rain = 3,
    Snow = 4,
    Thunderstorm = 5,
    Fog = 6,
    Unknown = 7
};

// Nessun dato meteo disponibile (API disattivata/non ancora risposta):
// sentinella per il campo temperatura (fuori range reale -50..+77).
constexpr int kTempUnknown = -128;

struct StationTelemetryFields
{
    QString grid4;          // locatore Maidenhead a 4 caratteri, es. "JN63"
    int tempC {kTempUnknown};
    int windSpeedKmh {0};
    int windDirDeg16 {0};   // 0-15, rosa dei venti a 16 punti (22.5 gradi/passo)
    SkyCondition sky {SkyCondition::Unknown};
    int powerWatts {0};
    int radioModelId {0};   // indice in stationRadioModelNames()
    int antennaTypeId {0};  // indice in stationAntennaTypeNames()
};

// Elenchi fissi condivisi fra C++ (codifica/decodifica) e QML (ComboBox
// impostazioni) — indice 0 e' sempre "Non specificato" in entrambi.
QStringList const& stationRadioModelNames ();
QStringList const& stationAntennaTypeNames ();
QStringList const& stationSkyConditionNames ();

// Converte una direzione vento in gradi (0-359) nell'indice a 16 punti piu'
// vicino; converte l'indice a 16 punti in un'etichetta testuale ("NW", ecc.).
int windDirDegToIndex16 (int degrees);
QString windDirIndex16ToLabel (int index16);

// hex esadecimale MAIUSCOLA a 18 caratteri, pronta per
// decodium::txmsg::encodeFt8()/encodeFt2() come testo del messaggio.
// I campi fuori range vengono troncati (clamp), non generano errore.
QString encodeStationTelemetryHex (StationTelemetryFields const& fields);

// Ritorna false se hex non e' una stringa esadecimale valida di 18
// caratteri o se il byte di firma non corrisponde (0xC3): in quel caso
// out non viene modificato ed e' un payload telemetria generico, non
// nostro.
bool decodeStationTelemetryHex (QString const& hex, StationTelemetryFields& out);

}
}

#endif // STATIONTELEMETRYCODEC_H
