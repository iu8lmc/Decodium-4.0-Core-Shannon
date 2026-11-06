// Assistente di Decodium: chat in testo con un modello linguistico locale.
//
// Gira con llama.cpp (processo separato, licenza MIT) e il modello Qwen2.5 del
// pacchetto "Assistente". Niente account, niente rete: il modello risponde sul
// computer. Non tocca la radio: le risposte si leggono o si scrivono, e una
// trasmissione parte solo se l'operatore la lancia dal voice keyer.
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QVector>

namespace decodium::voicekeyer {

class PackManager;


struct ChatTurn {
    QString role;   // "user" o "assistant"
    QString text;
};

// Il prompt nel formato del modello (ChatML): sistema, storia recente, domanda.
QString buildChatPrompt(const QVector<ChatTurn>& history, const QString& question);
// Toglie i codici colore del terminale e il marcatore di fine risposta.
QString cleanReply(const QString& raw);

class AssistantChat : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString transcript READ transcript NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString lastReply READ lastReply NOTIFY changed)

public:
    explicit AssistantChat(PackManager* packs, QObject* parent = nullptr);
    ~AssistantChat() override;

    QString transcript() const;
    bool busy() const { return m_process != nullptr; }
    bool available() const;
    QString status() const { return m_status; }
    QString lastReply() const { return m_lastReply; }

    Q_INVOKABLE void ask(const QString& question);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void setStatus(const QString& text);
    void finish(int code, QProcess::ExitStatus st);

    PackManager* m_packs;
    QVector<ChatTurn> m_history;
    QString m_status;
    QString m_lastReply;
    QString m_pendingQuestion;
    QProcess* m_process {nullptr};
};

}  // namespace decodium::voicekeyer
