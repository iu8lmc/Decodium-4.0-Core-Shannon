// JTTY dentro Decodium: il ricevitore su un thread suo, la coda di
// trasmissione, le macro F1-F8 e i due elenchi di testo che la finestra mostra.
//
// Non conosce DecodiumBridge: riceve dei ganci, come DecoRTTY e DecoPort. La
// frequenza RX/TX e' quella dell'applicazione (i marcatori del waterfall), il
// PTT e l'uscita audio sono quelli condivisi dei modi da tastiera.

#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include <functional>

class QSettings;

namespace decodium
{
namespace jtty
{

class Receiver;

// Una riga di testo JTTY: un messaggio che cresce frame dopo frame, o un
// messaggio trasmesso.
class JttyLineModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY (int count READ count NOTIFY countChanged)

public:
  enum Roles
  {
    MessageIdRole = Qt::UserRole + 1,
    UtcRole,
    FrequencyRole,
    TextRole,
    CompleteRole,
    TxRole
  };

  struct Line
  {
    qint64 messageId {0};
    QDateTime startUtc;
    int frequency {0};
    QString text;
    bool complete {false};
    bool tx {false};
  };

  explicit JttyLineModel (QObject* parent = nullptr);

  int rowCount (QModelIndex const& parent = {}) const override;
  QVariant data (QModelIndex const& index, int role) const override;
  QHash<int, QByteArray> roleNames () const override;

  int count () const { return m_lines.size (); }
  bool contains (qint64 messageId) const;
  // Inserisce o aggiorna; ritorna true se la riga e' nuova.
  bool upsert (Line const& line);
  void appendTx (QDateTime const& when, int frequency, QString const& text);
  void clear ();
  Line const* lineAt (int row) const;

signals:
  void countChanged ();

private:
  int indexOf (qint64 messageId) const;
  void trim ();

  QVector<Line> m_lines;
  qint64 m_nextTxId {-1};
};

// Il ricevitore vive su un thread suo: una finestra di 2,36 s ogni 0,47 s,
// con fino a qualche decina di tentativi di decodifica ciascuna.
class JttyRxWorker : public QObject
{
  Q_OBJECT

public:
  JttyRxWorker ();
  ~JttyRxWorker () override;

public slots:
  void reset (qint64 generation);
  void process (qint64 generation, QVector<short> samples, float f0, float ftol, int nfa, int nfb);

signals:
  // Ogni voce: id, freq, start (s dal primo campione), text, complete.
  void updates (qint64 generation, QVariantList list);

private:
  Receiver* m_rx;
  qint64 m_generation {0};
};

class JttyController : public QObject
{
  Q_OBJECT
  Q_PROPERTY (bool active READ active NOTIFY activeChanged)
  Q_PROPERTY (int ftol READ ftol WRITE setFtol NOTIFY ftolChanged)
  Q_PROPERTY (int serialNumber READ serialNumber WRITE setSerialNumber NOTIFY serialNumberChanged)
  // 0 = nessun contest, 1 = Field Day, 2 = RTTY Roundup (i profili di WSJT-X).
  Q_PROPERTY (int exchangeProfile READ exchangeProfile WRITE setExchangeProfile NOTIFY exchangeProfileChanged)
  // Lo scambio configurato per Field Day ("1D EMA") o RTTY Roundup ("MA", "DX", "#").
  Q_PROPERTY (QString exchange READ exchange WRITE setExchange NOTIFY exchangeChanged)
  Q_PROPERTY (QStringList macros READ macros NOTIFY macrosChanged)
  Q_PROPERTY (bool transmitting READ transmitting NOTIFY transmittingChanged)
  Q_PROPERTY (QString sendingText READ sendingText NOTIFY sendingTextChanged)
  Q_PROPERTY (QString lastError READ lastError NOTIFY lastErrorChanged)
  Q_PROPERTY (bool lowerCase READ lowerCase WRITE setLowerCase NOTIFY lowerCaseChanged)
  Q_PROPERTY (bool autoLogOnTu READ autoLogOnTu WRITE setAutoLogOnTu NOTIFY autoLogOnTuChanged)
  Q_PROPERTY (QObject* qsoLines READ qsoLines CONSTANT)
  Q_PROPERTY (QObject* allLines READ allLines CONSTANT)

public:
  struct Hooks
  {
    std::function<int ()> rxFrequency;
    std::function<int ()> txFrequency;
    std::function<void (int)> setRxFrequency;
    std::function<QString ()> myCall;
    std::function<QString ()> myGrid;
    std::function<QString ()> hisCall;
    std::function<void (QString const&)> setHisCall;
    std::function<bool ()> monitoring;
    std::function<bool ()> canTransmit;
    std::function<void (bool)> keyPtt;
    std::function<bool ()> txActive;
    std::function<void (QVector<short> const&)> sendAudio;
    std::function<double ()> txAmplitude;
    std::function<void (QString const&, double, QDateTime const&)> decodedLine;
    std::function<void (bool, int, QString const&, QDateTime const&)> allTxt;
    std::function<void (QString const&, QString const&, QString const&)> logQso;
    std::function<void (QString const&)> log;
  };

  explicit JttyController (QObject* parent = nullptr);
  ~JttyController () override;

  void setHooks (Hooks hooks);
  void load (QSettings& settings);

  bool active () const { return m_active; }
  void setActive (bool on);
  // L'audio ricevuto, a 12 kHz.
  void feedAudio (QVector<short> const& samples);
  // Lasciando JTTY: niente piu' audio in uscita, PTT giu'.
  void leave ();
  // Il monitor si e' riacceso: l'audio riprende dopo un buco di durata ignota.
  void restartReceiver ();

  int ftol () const { return m_ftol; }
  void setFtol (int v);
  int serialNumber () const { return m_serial; }
  void setSerialNumber (int v);
  int exchangeProfile () const { return m_profile; }
  void setExchangeProfile (int v);
  QString exchange () const { return m_exchange; }
  void setExchange (QString const& v);
  QStringList macros () const { return m_macros; }
  bool transmitting () const { return m_transmitting; }
  QString sendingText () const { return m_sendingText; }
  QString lastError () const { return m_lastError; }
  bool lowerCase () const { return m_lowerCase; }
  void setLowerCase (bool v);
  bool autoLogOnTu () const { return m_autoLogOnTu; }
  void setAutoLogOnTu (bool v);
  QObject* qsoLines () { return &m_qso; }
  QObject* allLines () { return &m_all; }

  Q_INVOKABLE bool send (QString const& text);
  Q_INVOKABLE bool sendFunctionKey (int key);
  Q_INVOKABLE QString previewFunctionKey (int key) const;
  Q_INVOKABLE void setMacro (int key, QString const& text);
  Q_INVOKABLE void resetMacros ();
  Q_INVOKABLE void abort ();
  Q_INVOKABLE void clearHistory ();
  // Un clic su una riga: RX su quella frequenza e, se c'e', il suo nominativo.
  Q_INVOKABLE void pickLine (bool qsoList, int row);
  Q_INVOKABLE void logQso ();
  // Stima dei frame e della durata di un testo, per la barra di stato.
  Q_INVOKABLE QString describe (QString const& text) const;

signals:
  void activeChanged ();
  void ftolChanged ();
  void serialNumberChanged ();
  void exchangeProfileChanged ();
  void exchangeChanged ();
  void macrosChanged ();
  void transmittingChanged ();
  void sendingTextChanged ();
  void lastErrorChanged ();
  void lowerCaseChanged ();
  void autoLogOnTuChanged ();

  void workerReset (qint64 generation);
  void workerProcess (qint64 generation, QVector<short> samples, float f0, float ftol, int nfa, int nfb);

private:
  void onUpdates (qint64 generation, QVariantList list);
  bool enqueue (QString const& logicalText, QVector<int> const& tones);
  void pump ();
  void finishTransmit (QString const& reason);
  void setError (QString const& e);
  void resetReceiver ();
  void save ();
  void log (QString const& s) const;

  Hooks m_hooks;
  QSettings* m_settings {nullptr};
  QThread m_thread;
  QPointer<JttyRxWorker> m_worker;
  JttyLineModel m_qso;
  JttyLineModel m_all;

  bool m_active {false};
  int m_ftol {50};
  int m_serial {1};
  int m_profile {0};
  QString m_exchange;
  QStringList m_macros;
  bool m_lowerCase {false};
  bool m_autoLogOnTu {true};

  // Ricezione: l'istante UTC del primo campione e quanti ne sono arrivati.
  qint64 m_generation {0};
  QDateTime m_streamT0;
  qint64 m_samplesFed {0};
  // Inizio della nostra ultima trasmissione, finche' l'audio non riprende.
  QDateTime m_rxPausedSince;

  // Trasmissione
  bool m_transmitting {false};
  QVector<short> m_pcm;
  int m_pcmPos {0};
  QElapsedTimer m_txClock;
  qint64 m_txSamplesSent {0};
  QTimer m_txTimer;
  QTimer m_txTail;
  QTimer m_txWatchdog;
  QString m_sendingText;
  QString m_lastError;
};

}
}
