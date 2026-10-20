#include "CwTiming.h"

#include "CwKeyer.h"

#include <QtGlobal>

namespace decodium::cw {

QVector<KeyEvent> timelineFor(const QString& text, int wpm)
{
    const int dot = 1200 / qBound(5, wpm, 60);
    QVector<KeyEvent> out;
    int gap = 0;           // quanto aspettare prima del prossimo "giu'"
    bool first = true;     // niente spazio di lettera prima della prima lettera di una parola
    for (const QChar c : text.toUpper()) {
        if (c == QLatin1Char(' ')) {
            gap = dot * 7;
            first = true;
            continue;
        }
        const QString code = CwKeyer::morseOf(c);
        if (code.isEmpty())
            continue;
        if (!first)
            gap = dot * 3;
        first = false;
        for (qsizetype i = 0; i < code.size(); ++i) {
            const int length = code.at(i) == QLatin1Char('-') ? dot * 3 : dot;
            out.append({static_cast<quint16>(qMin(gap, 32767)), true});
            out.append({static_cast<quint16>(length), false});
            gap = dot;     // un punto fra gli elementi
        }
    }
    return out;
}

int totalMs(const QVector<KeyEvent>& events)
{
    int total = 0;
    for (const KeyEvent& e : events)
        total += e.deltaMs;
    return total;
}

QVector<KeyEvent> takeWindow(QVector<KeyEvent>& events, int windowMs)
{
    // Gli eventi vengono sempre a coppie (giu', su): si taglia solo fra due
    // coppie, perche' un pezzo che finisse a tasto giu' lascerebbe la nota
    // accesa se il resto non arrivasse.
    int elapsed = 0;
    int taken = 0;
    while (taken + 1 < events.size()) {
        const int pair = events.at(taken).deltaMs + events.at(taken + 1).deltaMs;
        if (taken > 0 && elapsed + pair > windowMs)
            break;
        elapsed += pair;
        taken += 2;
    }
    const QVector<KeyEvent> head = events.mid(0, taken);
    events.remove(0, taken);
    return head;
}

}  // namespace decodium::cw
