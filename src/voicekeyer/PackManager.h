// Pacchetti opzionali di Decodium: ognuno si installa e si rimuove da solo.
//
//   Voce        motore Piper + voci italiana e inglese      (~150 MB)
//   Riconosc.   modello whisper multilingue "small"          (~490 MB)
//   Assistente  llama.cpp + modello Qwen2.5 1.5B Q4_K_M      (~1,14 GB)
//
// I download sono a sorgente fissata: ogni file viene verificato con SHA256 prima
// di essere usato. Nessun account, nessun servizio online durante l'uso.
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;

namespace decodium::voicekeyer {

enum class Pack { Voice = 0, Speech = 1, Assistant = 2 };

class PackManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool voiceInstalled READ voiceInstalled NOTIFY changed)
    Q_PROPERTY(bool speechInstalled READ speechInstalled NOTIFY changed)
    Q_PROPERTY(bool assistantInstalled READ assistantInstalled NOTIFY changed)

public:
    explicit PackManager(const QString& root, QObject* parent = nullptr);

    bool installed(Pack pack) const;
    // Attivo = installato e selezionato dall'utente. Il file resta sul disco
    // anche se il pacchetto viene disattivato.
    bool active(Pack pack) const;
    Q_INVOKABLE bool isEnabled(int pack) const;
    Q_INVOKABLE void setEnabled(int pack, bool on);
    bool voiceInstalled() const { return installed(Pack::Voice); }
    bool speechInstalled() const { return installed(Pack::Speech); }
    bool assistantInstalled() const { return installed(Pack::Assistant); }
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString status() const { return m_status; }
    QString root() const { return m_root; }

    // Percorsi dentro i pacchetti (validi solo se il pacchetto e' installato).
    QString piperExecutable() const;
    QString runtimeDirectory() const;
    QString voiceModel(const QString& lang) const;      // "it" o "en"
    QString speechModel() const;
    QString whisperExecutable() const;
    QString llamaExecutable() const;
    QString assistantModel() const;
    QString llamaDirectory() const;

    // Scarica, verifica e installa un pacchetto. Non fa nulla se e' gia' installato.
    Q_INVOKABLE void install(int pack);
    Q_INVOKABLE void remove(int pack);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool isInstalled(int pack) const { return installed(static_cast<Pack>(pack)); }
    // Il riconoscimento non ha ancora un motore pubblicabile: si puo' solo sapere.
    Q_INVOKABLE bool speechDownloadable() const { return false; }

signals:
    void changed();

private:
    struct Item {
        Pack pack;
        QString url;
        QString sha256;
        qint64 bytes;
        QString target;     // file di destinazione, relativo alla radice
        QString extractTo;  // per gli zip: cartella di estrazione (relativa alla radice)
    };
    static const QVector<Item>& manifest();
    void fetchNext();
    void extractArchive(const QString& file, const QString& dir);
    void fail(const QString& text);
    void done();

    QString m_root;
    QNetworkAccessManager* m_network;
    QVector<Item> m_queue;
    Pack m_current {Pack::Voice};
    int m_index {0};
    qint64 m_doneBytes {0};
    qint64 m_totalBytes {0};
    bool m_busy {false};
    double m_progress {0.0};
    QString m_status;
    QString m_currentFile;
};

}  // namespace decodium::voicekeyer
