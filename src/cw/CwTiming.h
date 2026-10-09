// Il CW come sequenza di istanti del tasto: giu' e su, con il tempo che passa
// fra uno e il precedente. E' lo stesso ritmo del manipolatore seriale (punto =
// 1200/wpm millisecondi, linea tre punti, un punto fra gli elementi, tre fra le
// lettere, sette fra le parole), ma scritto come dati invece che come impulsi su
// un piedino: serve a mandare il CW a una radio lontana senza mandare audio.
#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace decodium::cw {

// Il tasto si apre o si chiude `deltaMs` millisecondi DOPO l'evento precedente.
struct KeyEvent {
    quint16 deltaMs {0};
    bool down {false};
};

// La traccia di un testo. Comincia con il tasto giu' a tempo zero e finisce con
// il tasto su: un messaggio non lascia mai il tasto abbassato. I caratteri che
// il Morse non conosce si saltano, come fa il manipolatore.
QVector<KeyEvent> timelineFor(const QString& text, int wpm);

// Quanto dura la traccia, in millisecondi.
int totalMs(const QVector<KeyEvent>& events);

// Il pezzo di traccia che sta nei primi `windowMs` millisecondi (almeno un
// evento), e il resto. Serve a consegnare il messaggio a poco a poco: se
// l'operatore ferma, quello che e' gia' stato mandato e' poco.
QVector<KeyEvent> takeWindow(QVector<KeyEvent>& events, int windowMs);

}  // namespace decodium::cw
