#ifndef DECODIUMCOMMUNITYREPORT_HPP
#define DECODIUMCOMMUNITYREPORT_HPP

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// IU8LMC — Segnalazione problemi verso il forum community.ft2.it/groups.
//
// Sostituisce l'invio a GitHub del vecchio bug-report: quando c'e' un problema,
// l'app apre un topic sul forum della community, NELLA LINGUA DELL'UTENTE (il
// testo arriva gia' tradotto dal QML) e nella categoria giusta.
//
// Due ingressi (scelta utente "A+B"):
//   A — l'autodiagnosi non trova una causa nota  -> problema vero -> si offre di
//       aprire il topic col contesto diagnostico.
//   B — l'utente descrive un problema e conferma -> il topic viene creato in
//       automatico (niente copia-incolla verso GitHub).
//
// Il forum e' "aperto a tutti anche senza registrazione": nessun token, ma CSRF
// session-based. Flusso: GET /groups/new (prende cookie ft2sid + _csrf) ->
// POST /groups (il cookie viaggia da solo con lo stesso QNetworkAccessManager).
// Campo honeypot "website" lasciato vuoto (anti-bot lato server).
//
// Anti-spam lato client: dedup per firma (categoria+titolo normalizzato) con
// finestra temporale, cosi' la stessa macchina non riapre lo stesso identico
// problema in loop.
//
// Classe separata dal bridge: DecodiumBridge.cpp e' il file piu' conteso negli
// assorbimenti da elisir80.

class DecodiumCommunityReport : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool    busy       READ busy       NOTIFY busyChanged)
    Q_PROPERTY(QString lastUrl    READ lastUrl    NOTIFY finished)
    Q_PROPERTY(QString lastError  READ lastError  NOTIFY finished)

public:
    explicit DecodiumCommunityReport(QObject* parent = nullptr);

    bool    busy()      const { return m_busy; }
    QString lastUrl()   const { return m_lastUrl; }
    QString lastError() const { return m_lastError; }

    // category: cat/rx/tx/ui/lang/audio/install/other (valori del form del forum).
    // author (nominativo) e' facoltativo. Ritorna subito; l'esito arriva via
    // segnali posted()/failed().
    Q_INVOKABLE void postProblem(const QString& category, const QString& title,
                                 const QString& author, const QString& body);

    // La stessa segnalazione (categoria+titolo) e' gia' stata inviata di recente
    // da questa macchina? Serve al QML per evitare doppioni (soprattutto nel
    // caso A automatico).
    Q_INVOKABLE bool alreadyReported(const QString& category, const QString& title) const;

    // Coda dell'ultimo log diagnostico (il file cresce fino a 5 MB; il forum non
    // accetta un corpo cosi' grande, quindi ne prendiamo solo la parte recente).
    // Vuoto se il log non e' leggibile. Usato dal pulsante "carica ultimo log".
    Q_INVOKABLE QString diagnosticLogTail(int maxBytes = 50000) const;

signals:
    void busyChanged();
    void finished();
    void posted(const QString& url);
    void failed(const QString& message);

private:
    void setBusy(bool b);
    QString signatureOf(const QString& category, const QString& title) const;
    void rememberSignature(const QString& category, const QString& title);
    void doPost(const QString& csrf, const QString& category, const QString& title,
                const QString& author, const QString& body);

    QNetworkAccessManager* m_nam {nullptr};
    bool    m_busy {false};
    QString m_lastUrl;
    QString m_lastError;
};

#endif  // DECODIUMCOMMUNITYREPORT_HPP
