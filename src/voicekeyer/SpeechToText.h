// Riconoscimento vocale locale: l'operatore tiene premuto il pulsante, parla,
// rilascia, e la trascrizione compare nella finestra. Il motore e' whisper.cpp
// (processo separato, licenza MIT); il modello sta nel pacchetto vocale.
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class QAudioSource;
namespace decodium::voicekeyer { class PackManager; }
class QIODevice;

namespace decodium::voicekeyer {

// Toglie gli spazi in eccesso e i marcatori tra parentesi quadre di whisper
// ([BLANK_AUDIO], [MUSIC], ...).
QString cleanTranscript(const QString& raw);

class SpeechToText : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString transcript READ transcript NOTIFY changed)
    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(bool working READ working NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    explicit SpeechToText(PackManager* packs, QObject* parent = nullptr);
    ~SpeechToText() override;

    QString transcript() const { return m_transcript; }
    bool listening() const { return m_source != nullptr; }
    bool working() const { return m_working; }
    QString status() const { return m_status; }

    Q_INVOKABLE void startListening(const QString& lang);
    Q_INVOKABLE void stopListening();
    Q_INVOKABLE void clearTranscript();

signals:
    void changed();

private:
    void captureReady();
    void transcribe();
    void setStatus(const QString& text);

    PackManager* m_packs;
    QString m_lang {QStringLiteral("it")};
    QString m_transcript;
    QString m_status;
    bool m_working {false};
    QAudioSource* m_source {nullptr};
    QIODevice* m_input {nullptr};
    QByteArray m_captured;
    QProcess* m_process {nullptr};
};

}  // namespace decodium::voicekeyer
