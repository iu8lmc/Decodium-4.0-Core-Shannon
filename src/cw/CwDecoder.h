// Decodium — il decoder CW: dall'audio alle lettere.
//
// Non serve una radio che decodifichi: basta l'audio che esce dalla radio. A
// leggerlo ci pensa ggmorse (libs/ggmorse), il decodificatore di Georgi
// Gerganov: trova il tono da solo guardando lo spettro, misura i tempi di ogni
// segno su una finestra di tre secondi e ne tira fuori le lettere. La velocita'
// non si chiede a nessuno: la impara ascoltando.
//
// Qui dentro c'e' solo quello che serve a portargli l'audio a pezzetti, come
// arriva dalla scheda audio, e a riprendersi il testo.
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

class GGMorse;

namespace decodium::cw {

class CwDecoder {
public:
    explicit CwDecoder(int sampleRate = 8000);
    ~CwDecoder();

    CwDecoder(const CwDecoder&) = delete;
    CwDecoder& operator=(const CwDecoder&) = delete;

    void setSampleRate(int sampleRate);
    int sampleRate() const { return m_sampleRate; }

    // Il tono da ascoltare, in hertz. 0: lo cerca da solo fra 200 e 1200 Hz,
    // che e' dove sta il CW di chiunque.
    void setTone(int hz);
    int tone() const { return m_tone; }
    double toneHz() const { return m_toneHz; }

    // 0 lascia che ggmorse misuri la velocita'; altrimenti usa il valore
    // indicato per evitare che un segnale intermittente venga scambiato per
    // un CW molto piu' veloce.
    void setSpeed(int wpm);
    int speed() const { return m_speed; }

    // La velocita' che ha imparato, in parole al minuto.
    int wpm() const { return m_wpm; }

    // Quello che il decodificatore sta guardando, come nella finestra di
    // ggmorse: il segnale filtrato sul tono negli ultimi tre secondi (da 0 a
    // 1, il piu' vecchio per primo), la soglia sopra la quale conta come tono
    // acceso, e le stime del momento — anche quando non sta leggendo niente,
    // che e' proprio quello che si vuole vedere.
    struct Scope {
        QVector<float> signal;
        float level{0};
        float pitch{0};
        float speed{0};
        float cost{1};
        bool reading{false};
    };
    const Scope& scope() const { return m_scope; }

    // Manda dentro l'audio (mono, 16 bit) e torna il testo nuovo, se ne e'
    // uscito. Si puo' chiamare a pezzi piccoli: lo stato resta.
    QString feed(const qint16* samples, int count);
    // Quello che resta da dire quando il segnale finisce (l'ultima lettera):
    // un po' di silenzio, che e' quello che aspetta per chiudere la parola.
    QString flush();
    void reset();

private:
    void rebuild();
    void applyDecodeParameters();
    QString drain();
    void updateScope(const std::vector<float>& signal);

    int m_sampleRate{8000};
    int m_tone{0};
    int m_speed{0};
    std::unique_ptr<GGMorse> m_morse;
    // L'audio che e' arrivato e non e' ancora stato consumato: ggmorse lo
    // chiede a blocchi interi, e dalla scheda audio arriva come capita.
    QByteArray m_pending;
    qsizetype m_taken{0};

    // Tono e velocita' si leggono solo mentre il decodificatore sta davvero
    // leggendo qualcosa: nel silenzio le sue stime scendono a fondo scala, e
    // nel pannello si vedrebbe la velocita' crollare a ogni pausa.
    double m_toneHz{0};
    int m_wpm{0};
    Scope m_scope;
    // L'ultima lettera uscita: serve a non mettere due righe vuote di fila
    // quando il tono cambia fra una chiamata e l'altra.
    QChar m_last;
};

// La tavola del Morse, per chi deve scrivere e per chi deve leggere.
QString morseToChar(const QString& symbols);
QString charToMorse(QChar c);

} // namespace decodium::cw
