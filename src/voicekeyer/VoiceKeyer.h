// Voice keyer: otto messaggi preregistrati da mandare in SSB con un tasto.
//
// Ogni slot ha un'etichetta, un testo di riferimento e, se registrato, un file
// WAV. Il messaggio esce dallo stesso percorso di trasmissione della finestra
// SSB (PTT, audio a 12 kHz, rilascio): il modulo non apre niente da solo e non
// trasmette mai senza che l'operatore abbia premuto Play.
#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "PackManager.h"

#include <functional>
#include <optional>

class QAudioSink;
class QAudioSource;
class QIODevice;

namespace decodium::voicekeyer {

constexpr int kSlots = 8;
constexpr int kRadioRate = 12000;   // il trasporto audio della radio
constexpr int kCaptureRate = 48000; // cattura del microfono, poi ricampionata
constexpr int kPacket = 480;        // 40 ms a 12 kHz
constexpr int kMaxSeconds = 60;     // durata massima di un messaggio registrato

// Lettura di un WAV PCM 16 bit (mono o stereo, canale sinistro): il risultato
// e' a kRadioRate. Nullopt con il motivo in *error se il file non va bene.
std::optional<QVector<short>> readWav(const QString& path, QString* error = nullptr);
bool writeWav(const QString& path, const QVector<short>& samples, int rate, QString* error = nullptr);
QVector<short> resampleLinear(const QVector<short>& in, int fromRate, int toRate);

class VoiceKeyer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool synthesizing READ synthesizing NOTIFY changed)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    struct Hooks {
        std::function<bool()> begin;                         // alza il PTT, false = rifiutato
        std::function<void()> end;                           // abbassa il PTT
        std::function<bool()> valid;                         // la radio e' ancora pronta a trasmettere
        std::function<void(const QVector<short>&)> send;     // un pacchetto da 40 ms a 12 kHz
        std::function<int()> leadMs;                         // attesa prima dell'audio
    };

    VoiceKeyer(const QString& directory, PackManager* packs, QObject* parent = nullptr);
    ~VoiceKeyer() override;

    void setHooks(Hooks h) { m_hooks = std::move(h); }

    QVariantList messages() const;
    bool playing() const { return m_playing; }
    bool recording() const { return m_recording; }
    QString status() const { return m_status; }

    Q_INVOKABLE void setSlotText(int index, const QString& label, const QString& text);
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool startRecording(int index);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void clearSlot(int index);
    Q_INVOKABLE void setSlotLanguage(int index, const QString& lang);
    // Legge un testo qualsiasi (per esempio una risposta dell'assistente) con la voce del pacchetto.
    Q_INVOKABLE void sayText(const QString& text, const QString& lang);
    // Ascolto in locale (altoparlanti del PC): non trasmette, serve a provare la voce.
    Q_INVOKABLE void preview(const QString& text, const QString& lang);
    bool synthesizing() const { return m_tts != nullptr; }
    double speed() const { return m_speed; }
    void setSpeed(double s);

signals:
    void changed();
    // Solo quando cambiano testi, lingue o registrazioni: le righe della QML non si ricreano
    // a ogni passo del download.
    void messagesChanged();

private:
    struct Slot {
        QString label;
        QString text;
        QString lang {QStringLiteral("it")};
    };
    void startPlayback(const QVector<short>& pcm, const QString& name);
    void synthesize(const QString& text, const QString& lang, const QString& name, bool forAir);
    void onSynthesized(int code, QProcess::ExitStatus status);
    void playLocally(const QVector<short>& pcm);

    QString wavPath(int index) const;
    void load();
    void save();
    void setStatus(const QString& text);
    void tick();
    void finish(const QString& message);
    void captureReady();
    void finishRecording();

    QString m_directory;
    PackManager* m_pack {nullptr};
    QVector<Slot> m_slots;
    Hooks m_hooks;

    QVector<short> m_pcm;
    int m_pos {0};
    int m_tail {0};
    bool m_playing {false};
    QTimer m_lead;
    QTimer m_pace;

    bool m_recording {false};
    int m_recordIndex {-1};
    QByteArray m_captured;
    QAudioSource* m_source {nullptr};
    QIODevice* m_input {nullptr};

    QString m_status;
    QProcess* m_tts {nullptr};
    QString m_ttsName;
    bool m_ttsForAir {true};
    int m_ttsRate {22050};
    double m_speed {1.0};
    QAudioSink* m_preview {nullptr};
};

}  // namespace decodium::voicekeyer
