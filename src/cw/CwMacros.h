// Le macro del CW: testo con i buchi, e i buchi riempiti da quello che sta
// succedendo nel QSO.
//
// Sono le stesse del manipolatore di DecoDXLog: dodici tasti, F1-F12, nell'ordine
// in cui li tiene ogni log da contest. Chi le ha scritte nell'uno le ritrova
// nell'altro. Qui c'e' solo la parte pura (niente radio, niente finestre), cosi'
// si prova da sola.
#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

namespace decodium::cw {

struct Macro {
    QString label;   // la scritta del tasto, "F1 CQ" compreso
    QString text;    // il testo con i buchi: {MYCALL} {CALL} {RST} {NR} {EXCH} {NAME}
};

// Con chi si sta parlando e con che numeri.
struct Context {
    QString myCall;
    QString call;
    QString rst {QStringLiteral("599")};
    QString nr;
    QString exch;
    QString name;
};

// Riempie i buchi. Quello che resta senza risposta se ne va: in aria non si
// manda una parentesi graffa. In CW le minuscole non esistono, e alcuni
// manipolatori (le Yaesu via CAT) rifiutano il messaggio intero per una sola.
QString expand(const QString& text, const Context& context);

// Le dodici di sempre.
QList<Macro> defaultMacros();

// Il massimo di tasti che si possono avere.
constexpr int kMaxMacros = 24;

// Serializzazione per le impostazioni: un array JSON di {label, text}. Una
// stringa vuota o illeggibile torna le macro di sempre; non si perdono mai i
// tasti di chi li ha scritti, e non si restituisce mai una lista vuota.
QString macrosToJson(const QList<Macro>& macros);
QList<Macro> macrosFromJson(const QString& json);

}  // namespace decodium::cw
