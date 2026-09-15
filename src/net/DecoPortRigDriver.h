// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QMutex>

class QTimer;
class QThread;

// Il gateway parla alla radio da solo.
//
// Fino alla 1.0.578 DecoPort chiedeva frequenza, modo e PTT all'applicazione,
// che a sua volta li chiedeva al proprio CAT. Bastava che quel CAT non fosse
// connesso — o che la porta fosse in mano a un altro programma — perche' la
// radio in rete diventasse muta: si sentiva l'audio e non si comandava niente.
//
// Questo driver toglie di mezzo quella dipendenza. Apre la seriale e parla il
// protocollo della radio usando Hamlib, e il modello non lo chiede all'utente:
// lo cerca nel catalogo che Hamlib stessa espone a runtime, partendo da quello
// che l'identita' USB ha gia' detto. Nessuna tabella di modelli da mantenere a
// mano, e nessuna radio da selezionare in un menu.
//
// Sta su un thread suo perche' una lettura seriale che si impunta non deve
// fermare l'interfaccia: chi lo interroga legge una fotografia aggiornata dal
// polling, non il bus.
class DecoPortRigDriver : public QObject
{
    Q_OBJECT

public:
    // The driver deliberately has no QObject parent: it owns a dedicated
    // worker thread and must be movable to it.  The bridge keeps the pointer
    // and deletes it explicitly during shutdown.
    explicit DecoPortRigDriver();
    ~DecoPortRigDriver() override;

    // Il catalogo Hamlib, cosi' com'e' a runtime: numero, costruttore, modello,
    // stato del backend. Serve a risolvere un modello e a mostrarlo.
    static QVariantList hamlibCatalogue();

    // Cerca nel catalogo la voce che corrisponde a quello che l'identita' USB
    // ha riconosciuto. Restituisce il numero di modello Hamlib, 0 se nessuna.
    // matchedName, se fornito, riceve "Costruttore Modello" della voce scelta.
    static int resolveModel(const QString& rigLabel,
                            const QString& rigToken,
                            QString* matchedName = nullptr);

    // Accoda l'apertura della radio sul worker. Restituisce false solo se la
    // richiesta e' incompleta; l'esito reale arriva poi tramite opened/failed,
    // senza trattenere il thread grafico durante il timeout seriale.
    bool open(const QString& port, int baudRate, int civAddress, int model);
    void close();
    bool isOpen() const;

    // Fotografia dell'ultimo polling: non tocca il bus.
    double  frequencyHz() const;
    QString modeName() const;
    bool    ptt() const;
    QString rigName() const;
    QString lastError() const;

public slots:
    // Comandi verso la radio. Tornano subito: l'esecuzione avviene sul thread
    // del driver.
    void setFrequencyHz(double hz);
    void setModeName(const QString& name);
    void setPtt(bool on);

signals:
    void stateChanged();
    void opened(const QString& rigName);
    void closed();
    void failed(const QString& reason);

private slots:
    void poll();

private:
    struct Impl;
    Impl* d;

    void doOpen(const QString& port, int baudRate, int civAddress, int model);
    void doClose(bool clearOpening = true);

    mutable QMutex m_stateMutex;
    double  m_frequencyHz {0.0};
    QString m_modeName;
    bool    m_ptt {false};
    QString m_rigName;
    QString m_lastError;
    bool    m_open {false};
    bool    m_opening {false};
    QTimer* m_pollTimer {nullptr};
    QThread* m_thread {nullptr};
};
