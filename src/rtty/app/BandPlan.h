// DecoRTTY — dove si fa RTTY, banda per banda.
//
// Un pulsante di banda che porta la radio sul bordo del segmento — 14.000, per
// dire — non serve a niente: la' non c'e' nessuno. Serve arrivare dove il RTTY
// vive davvero, e per quello ci vuole un piano di banda scritto, non un calcolo.
//
// Le frequenze seguono il piano IARU Regione 1: il segmento delle emissioni
// digitali a banda stretta, nel tratto dove le stazioni RTTY si chiamano fra
// loro. Non sono i bordi dell'allocazione e non sono limiti di licenza — sono
// il posto in cui vale la pena guardare per primo.
#pragma once

#include <QString>

#include <vector>

namespace decortty::app {

struct Band {
    QString name;       // "20 m", come lo dicono gli operatori
    double  rttyMhz;    // dove portare la radio
    double  lowMhz;     // l'allocazione, per riconoscere dove siamo gia'
    double  highMhz;
};

// Il piano, dall'onda piu' lunga alla piu' corta.
const std::vector<Band>& bandPlan();

// In quale banda cade questa frequenza, o -1 se in nessuna. Serve a illuminare
// il pulsante giusto quando la radio e' stata sintonizzata altrove — dalla
// manopola, o da un altro programma.
int bandAt(double mhz);

} // namespace decortty::app
