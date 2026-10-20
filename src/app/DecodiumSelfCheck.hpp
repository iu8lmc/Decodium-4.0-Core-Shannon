#ifndef DECODIUMSELFCHECK_HPP
#define DECODIUMSELFCHECK_HPP

#include <QString>
#include <QVariantList>

// IU8LMC — Autodiagnosi: filtra i NON-bug PRIMA che diventino una segnalazione.
//
// PERCHE': lo storico dei report dei tester dice che ~85-90% dei problemi
// riportati ("non chiude i QSO", "fa collisione", "si blocca") NON sono bug di
// Decodium ma condizioni operative: versione vecchissima, orologio fuori sync,
// MAM multi-stream acceso in FT2, banda morta, overload, thread che affamano
// l'audio. Ogni caso costava lo stesso ping-pong: "che versione hai? che banda?
// mandami il log". Questi controlli danno la risposta all'utente subito.
//
// Ogni regola qui dentro nasce da un caso REALE diagnosticato sul campo, non da
// ipotesi: vedi i commenti sulle singole regole in DecodiumSelfCheck.cpp.
//
// La logica e' pura (nessuna dipendenza dal bridge) per restare testabile e per
// tenere minima la superficie di conflitto negli assorbimenti da elisir80.

namespace DecodiumSelfCheck {

// Ordine = gravita' crescente: l'ordinamento dei risultati ci si appoggia.
enum class Severity { Ok = 0, Warning = 1, Problem = 2 };

// Fotografia dello stato live, riempita dal bridge.
struct State {
    QString appVersion;
    QString latestVersion;
    bool    updateAvailable = false;

    QString mode;                 // "FT8", "FT4", "FT2", "FT2-LINK", ...
    QString callsign;
    bool    monitoring = false;

    bool    ntpEnabled = false;
    bool    ntpSynced  = false;
    double  ntpOffsetMs = 0.0;

    bool    mamMultiStream = false;
    int     ftThreads = 0;
    int     cpuCores = 0;

    qint64  totalDecodesReceived = 0;
    qint64  monitoringSeconds = 0;   // da quanto stiamo ricevendo
};

struct Finding {
    Severity severity = Severity::Ok;
    QString  title;    // cosa c'e' che non va
    QString  detail;   // perche' lo diciamo (il dato misurato)
    QString  action;   // cosa deve fare l'utente
    QString  category; // categoria del forum: cat/rx/tx/ui/lang/audio/install/other
};

// Esegue tutte le regole. Ritorna solo i problemi/avvisi trovati, ordinati per
// gravita' decrescente; vuoto = nessun problema noto (allora ha senso segnalare).
QList<Finding> run(const State& state);

// Comodo per QML: lista di mappe {severity, severityText, title, detail, action}.
QVariantList toVariantList(const QList<Finding>& findings);

}  // namespace DecodiumSelfCheck

#endif  // DECODIUMSELFCHECK_HPP
