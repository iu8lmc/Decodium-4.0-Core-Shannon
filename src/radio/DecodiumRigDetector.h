#pragma once

#include <QVariantList>

// Rilevamento automatico della radio collegata — Fase 1: SOLO PASSIVO.
//
// Non apre nessuna porta seriale e non invia alcun comando: si limita a leggere
// cio' che il sistema operativo gia' sa (identita' USB delle porte, nomi delle
// schede audio). Percio' e' sicuro chiamarlo anche con il CAT connesso e con la
// radio in trasmissione: non puo' disturbare il bus ne' azionare il PTT.
//
// L'identificazione del modello esatto richiederebbe una interrogazione attiva
// (leggere l'identita' dal rig), che e' materia della Fase 2 e va tenuta dietro
// un interruttore: in passato una sonda troppo insistente ha saturato il bus
// CI-V lasciando il PTT incollato.
namespace DecodiumRigDetector {

// Restituisce l'elenco degli apparati candidati, dal piu' attendibile al meno.
// Ogni elemento e' una mappa con queste chiavi:
//   rigLabel     QString  nome leggibile da mostrare ("Yaesu FT-991 / FT-991A / FT-DX10")
//   rigToken     QString  radice per cercare la voce giusta nella lista radio ("FT-991"),
//                         vuota se il modello non e' deducibile
//   catPort      QString  porta consigliata per il CAT ("COM5")
//   otherPorts   QStringList  altre porte dello stesso apparato (di solito PTT o dati)
//   baudRate     int      velocita' consigliata, 0 se sconosciuta
//   civAddress   int      indirizzo CI-V per gli Icom, 0 per gli altri
//   audioInput   QString  scheda audio in ingresso abbinata, vuota se non trovata
//   audioOutput  QString  scheda audio in uscita abbinata, vuota se non trovata
//   confidence   int      0-100: quanto e' sicuro il riconoscimento
//   evidence     QString  su cosa si basa, da mostrare all'utente
QVariantList detect();

} // namespace DecodiumRigDetector
