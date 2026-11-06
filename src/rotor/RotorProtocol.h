// Il protocollo seriale PRO.SIS.TEL dei control box dei rotori d'antenna.
//
// Viene da DecoRotor (iu8lmc/decorotor): stessi frame, stessi modelli, stesso
// autorilevamento. Riferimento: rotators/prosistel/prosistel.c di Hamlib.
//
// Ogni frame e' ASCII fra STX (0x02) e CR (0x0D):
//
//   comando   ->  STX <id> <verbo> [argomento] CR
//   risposta  <-  STX <id> , <verbo> , <valore> , <stato> CR
//
//   STX A ? CR        richiesta di posizione    risposta  STX A,?,290,R CR
//   STX A G290 CR     vai a 290 gradi
//   STX A G997 CR     stop dolce (999 = rapido)
//   STX A S CR        disabilita il CPM, da mandare all'apertura della porta
//
// Sui Combi-Track gli angoli viaggiano moltiplicati per 10 (290,5 gradi si
// scrive 2905) e i codici di stop diventano 9777 e 9999.
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace decodium::rotor {

constexpr char kStx = 0x02;
constexpr char kCr = 0x0D;

// Identificativi d'asse.
constexpr char kIdAzimuth = 'A';
constexpr char kIdElevation = 'E';
constexpr char kIdSecondUnit = 'B';   // elevazione sui Combi-Track

// Valori di "goto" che il control box legge come stop.
constexpr int kStopSoft = 997;
constexpr int kStopFast = 999;

// Come e' cablato un control box.
struct Model {
    QString key;
    QString label;
    char azId {0};     // 0 = non ha l'asse
    char elId {0};
    int multiplier {1};
    bool hasAz() const { return azId != 0; }
    bool hasEl() const { return elId != 0; }
};

// I quattro modelli di Hamlib. La chiave "auto" non e' un modello: vuol dire
// "rilevalo all'apertura".
const QList<Model>& models();
const Model* modelByKey(const QString& key);   // nullptr se "auto" o sconosciuto
inline const char* kAutoModel = "auto";

// La risposta decodificata di un control box.
struct Reply {
    QChar axisId;
    QChar verb;
    double value {0.0};
    QChar status;
    // Lo stato vale 'R' a rotore fermo; gli altri indicano rotazione in corso.
    bool isMoving() const { return status != QLatin1Char('R'); }
};

QByteArray encode(char axisId, char verb, const QString& argument = QString());
QByteArray queryPosition(char axisId);
// Un angolo negativo non si puo' rappresentare: torna un frame vuoto.
QByteArray gotoFrame(char axisId, double degrees, int multiplier = 1);
QByteArray stopFrame(char axisId, bool fast = false, int multiplier = 1);
QByteArray disableCpm(char axisId);

// Decodifica un frame di risposta (con o senza STX/CR). False se non e'
// conforme, con il motivo in *error.
bool decodeReply(const QByteArray& frame, int multiplier, Reply* reply, QString* error = nullptr);

}  // namespace decodium::rotor
