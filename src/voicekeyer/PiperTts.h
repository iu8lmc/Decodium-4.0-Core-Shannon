// Sintesi vocale locale con Piper: il testo entra da stdin, il PCM esce da
// stdout. Il motore e' un processo separato del pacchetto Voce, quindi la sua
// licenza non si mescola col codice di Decodium.
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class QProcess;

namespace decodium::voicekeyer {

class PiperTts {
public:
    // Testo pulito per il motore: una riga, niente caratteri che Piper non legge.
    static QString cleanText(const QString& text);
    // Argomenti del motore. `speed` 1.0 = normale, >1 piu' veloce.
    static QStringList arguments(const QString& modelPath, double speed);
    // Frequenza di campionamento scritta nel file .json accanto al modello.
    static int modelSampleRate(const QString& modelPath);
    // Prepara l'audio perche' la radio non tagli l'inizio: silenzio iniziale,
    // dissolvenza, volume normalizzato a 0,8 di fondo scala. Torna a 12 kHz.
    static QVector<short> prepareForAir(const QVector<short>& pcm, int rate);
    // Processo pronto a partire, con il testo gia' scritto su stdin.
    static QProcess* start(const QString& piperExe, const QString& runtimeDir,
                           const QString& modelPath, const QString& text, double speed,
                           QString* error = nullptr);
    // Il PCM grezzo prodotto dal motore, da preparare con prepareForAir.
    static std::optional<QVector<short>> decode(const QByteArray& raw, int rate);
    // Sincrona, per i test.
    static std::optional<QVector<short>> synthesize(const QString& piperExe, const QString& runtimeDir,
                                                    const QString& modelPath, const QString& text,
                                                    QString* error = nullptr);
};

}  // namespace decodium::voicekeyer
