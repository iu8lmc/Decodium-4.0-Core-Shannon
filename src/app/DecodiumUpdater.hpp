#ifndef DECODIUMUPDATER_HPP
#define DECODIUMUPDATER_HPP

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// IU8LMC — Aggiornamento automatico con avviso e conferma.
//
// PERCHE': la causa numero uno delle segnalazioni e' l'uso di versioni
// vecchissime (report da 1.0.262/348/368 su bug gia' corretti da decine di
// release). Il motivo non era la pigrizia degli utenti: il controllo
// aggiornamenti esisteva in DecodiumBridge::checkForUpdates() ma era SPENTO a
// compile-time (kDecodiumUpdateCheckerEnabled=false dalla 1.0.62), partiva solo
// da una voce di menu per giunta disabilitata, non aveva interfaccia e non
// scaricava nulla. L'app non ha mai detto a nessuno che esisteva una versione
// nuova.
//
// Qui il ciclo e' completo: controlla -> avvisa -> l'utente conferma ->
// scarica -> installa/aggiorna -> esce. Su Windows avvia l'installer; su Linux
// sostituisce in modo atomico l'AppImage corrente quando la cartella e'
// scrivibile. Mai nulla di automatico alle spalle dell'utente: nessun download
// parte senza un clic esplicito.
//
// Classe separata dal bridge di proposito: DecodiumBridge.cpp e' il file che
// elisir80 riscrive di piu' e ogni riga toccata li' e' un conflitto agli
// assorbimenti.

class DecodiumUpdater : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool    available       READ available       NOTIFY stateChanged)
    Q_PROPERTY(QString currentVersion  READ currentVersion  CONSTANT)
    Q_PROPERTY(QString latestVersion   READ latestVersion   NOTIFY stateChanged)
    Q_PROPERTY(QString releaseRepository READ releaseRepository NOTIFY stateChanged)
    Q_PROPERTY(QString releaseNotes    READ releaseNotes    NOTIFY stateChanged)
    Q_PROPERTY(bool    busy            READ busy            NOTIFY busyChanged)
    Q_PROPERTY(int     progress        READ progress        NOTIFY progressChanged)  // 0-100, -1 = indeterminato
    Q_PROPERTY(QString statusText      READ statusText      NOTIFY statusTextChanged)
    Q_PROPERTY(bool    checkOnStartup  READ checkOnStartup  WRITE setCheckOnStartup NOTIFY checkOnStartupChanged)
    Q_PROPERTY(bool    offlineMode     READ offlineMode     WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(bool    appImageRuntime READ appImageRuntime CONSTANT)

public:
    explicit DecodiumUpdater(QObject* parent = nullptr);

    bool    available()      const { return m_available; }
    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion()  const { return m_latestVersion; }
    QString releaseRepository() const { return m_releaseRepository; }
    QString releaseNotes()   const { return m_releaseNotes; }
    bool    busy()           const { return m_busy; }
    int     progress()       const { return m_progress; }
    QString statusText()     const { return m_statusText; }
    bool    checkOnStartup() const { return m_checkOnStartup; }
    bool    offlineMode() const { return m_offlineMode; }
    bool    appImageRuntime() const { return m_appImageRuntime; }
    void    setCheckOnStartup(bool on);
    void    setOfflineMode(bool offline);

    // Interroga prima elisir80 e, se non trova una versione piu' nuova, usa
    // iu8lmc come fallback. silent=true (avvio) non disturba l'utente se non
    // c'e' nulla di nuovo o se la rete non risponde.
    Q_INVOKABLE void check(bool silent = false);

    // Da chiamare all'avvio: rispetta checkOnStartup e non ricontrolla piu' di
    // una volta al giorno.
    Q_INVOKABLE void checkOnStartupIfDue();

    // Scarica il pacchetto della versione trovata. Su Windows lancia
    // l'installer; su Linux aggiorna l'AppImage o la salva in Downloads se il
    // percorso corrente non e' scrivibile. Parte SOLO su conferma esplicita.
    Q_INVOKABLE void downloadAndInstall();

    // "Salta questa versione": non avvisare piu' finche' non ne esce un'altra.
    Q_INVOKABLE void skipThisVersion();

signals:
    void stateChanged();
    void busyChanged();
    void progressChanged();
    void statusTextChanged();
    void checkOnStartupChanged();
    void offlineModeChanged();

    // Emesso quando c'e' davvero una versione nuova non ancora saltata:
    // la QML apre il dialog di conferma.
    void updateFound(const QString& version);
    // Controllo manuale senza novita': l'utente ha cliccato, merita una risposta.
    void upToDate(const QString& version);
    void errorOccurred(const QString& message);

private:
    void setBusy(bool b);
    void setProgress(int p);
    void setStatus(const QString& s);
    void requestReleaseCheck(bool silent, bool secondary);
    void onCheckFinished(QNetworkReply* reply, bool silent, bool secondary);
    void finishCheckWithoutUpdate(bool silent);
    void recordCheckCompleted();
    void launchInstaller(const QString& path);

    QNetworkAccessManager* m_nam {nullptr};
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseRepository;
    QString m_releaseNotes;
    QString m_downloadUrl;
    QString m_assetName;
    QString m_releasePageUrl;
    QString m_bestCheckedVersion;
    bool    m_anyRepositoryCheckedSuccessfully {false};
    bool    m_available {false};
    bool    m_busy {false};
    int     m_progress {0};
    QString m_statusText;
    bool    m_checkOnStartup {true};
    bool    m_offlineMode {false};
    bool    m_appImageRuntime {false};
};

#endif  // DECODIUMUPDATER_HPP
