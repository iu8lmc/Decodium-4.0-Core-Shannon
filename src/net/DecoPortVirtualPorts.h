// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

// Le porte COM virtuali, per i programmi che conoscono solo una porta COM.
//
// Una porta virtuale e' una coppia: quello che si scrive da una parte esce
// dall'altra. Il programma apre la parte visibile (COM20) credendo che sia la
// radio; DecoPort tiene aperta l'altra (CNCB0) e la collega al multiplexer,
// che parla con la radio vera. Il programma resta configurato come se la radio
// fosse sua: stesso modello, stessi comandi.
//
// Le coppie le crea com0com, il "Null-modem emulator" di Vyacheslav Frolov
// (https://com0com.sourceforge.net/, GPL 2 o successiva). DecoPort lo porta con
// se', invariato, in third_party/com0com: vedi NOTICE.md per provenienza,
// impronte e licenza. Lo installa e lo comanda con setupc.exe come farebbe una
// persona, senza includerne codice. Scriverne uno nuovo non porterebbe a
// niente di installabile: da Windows 10 un driver del kernel nuovo si carica
// solo con la firma di Microsoft. Chi ha gia' un altro driver (VSPE,
// FabulaTech, Eltima) indica a mano l'estremo nascosto della coppia, e
// DecoPort lo usa allo stesso modo.
//
// Installare com0com e creare una coppia richiedono l'amministratore:
// Windows lo chiede, e la coppia poi resta, anche dopo il riavvio.
namespace DecoPortVirtualPorts {

struct Pair {
    int     index {-1};      // il numero della coppia: CNCA<n>/CNCB<n>
    QString visible;         // il nome che vedono i programmi: COM20
    QString hidden;          // l'estremo che apre DecoPort: CNCB0
    // Il codice di problema di Windows per la coppia (0 = funziona). 52 e'
    // il driver rifiutato per la firma: succede con Secure Boot attivo.
    int     problem {0};
};

// setupc.exe di com0com, vuoto se non e' installato. DECOPORT_SETUPC lo
// indica a mano.
QString setupcPath();
bool available();

// L'installatore di com0com che viaggia con DecoPort, per questa architettura;
// vuoto se manca (o se l'architettura non ha un com0com, come ARM64).
// DECOPORT_COM0COM_SETUP lo indica a mano.
QString bundledInstaller(QString* why = nullptr);

// Installa com0com dall'installatore incluso, dopo averne controllato
// l'impronta. Chiede l'elevazione a Windows e aspetta.
bool install(QString* error = nullptr);

// Le coppie presenti, lette dal gestore dispositivi: non serve
// l'amministratore (setupc lo chiederebbe anche solo per elencarle).
QVector<Pair> list(QString* error = nullptr);

// Crea una coppia nuova; la parte visibile prende il primo numero COM libero,
// su quella nascosta il DCD dice se un programma ha aperto l'altra.
// Chiede l'elevazione a Windows e aspetta che la coppia compaia.
Pair create(QString* error = nullptr);

// Il significato di un codice di problema, per chi deve capire.
QString problemText(int problem);

} // namespace DecoPortVirtualPorts
