#include "JttyController.h"

#include "JttyCodec.hpp"
#include "JttyDecoder.hpp"
#include "JttyMessages.hpp"
#include "JttyWave.hpp"

#include <QSettings>
#include <QtMath>

#include <cmath>
#include <cstring>

namespace decodium
{
namespace jtty
{

namespace
{

constexpr int kMaxLines = 600;
// Anticipo con cui si passa l'audio all'uscita: un blocco di 20 ms piu' un
// margine, perche' un timer in ritardo non lasci un buco in aria.
constexpr int kTxLeadSamples = 12000 * 60 / 1000;
// Silenzio in testa a una trasmissione nuova: il PTT e il relè della radio
// non devono mangiarsi l'inizio del primo sincronismo.
constexpr int kTxPreambleSamples = 12000 * 120 / 1000;
// Dopo l'ultimo campione: quello che e' ancora nel buffer della scheda audio.
constexpr int kTxTailMs = 450;
constexpr int kNfa = 200;
constexpr int kNfb = 2800;

static_assert (sizeof (Jtty::NativeAtomDescriptor) == sizeof (NativeAtomDescriptor),
               "atom ABI mismatch");

QString nativeEncodeError (int status)
{
  switch (status)
    {
    case ENCODE_UNKNOWN_SECTION: return JttyController::tr ("unknown ARRL/RAC section");
    default: return JttyController::tr ("invalid native atom");
    }
}

}

// ------------------------------------------------------------------ modello

JttyLineModel::JttyLineModel (QObject* parent) : QAbstractListModel {parent} {}

int JttyLineModel::rowCount (QModelIndex const& parent) const
{
  return parent.isValid () ? 0 : m_lines.size ();
}

QVariant JttyLineModel::data (QModelIndex const& index, int role) const
{
  if (!index.isValid () || index.row () < 0 || index.row () >= m_lines.size ()) return {};
  Line const& l = m_lines.at (index.row ());
  switch (role)
    {
    case MessageIdRole: return l.messageId;
    case UtcRole: return l.startUtc.isValid () ? l.startUtc.toUTC ().toString (QStringLiteral ("hhmmss")) : QString {};
    case FrequencyRole: return l.frequency;
    case Qt::DisplayRole:
    case TextRole: return l.text;
    case CompleteRole: return l.complete;
    case TxRole: return l.tx;
    }
  return {};
}

QHash<int, QByteArray> JttyLineModel::roleNames () const
{
  return {{MessageIdRole, "messageId"}, {UtcRole, "utc"}, {FrequencyRole, "frequency"},
          {TextRole, "text"}, {CompleteRole, "complete"}, {TxRole, "tx"}};
}

int JttyLineModel::indexOf (qint64 messageId) const
{
  for (int i = m_lines.size () - 1; i >= 0; --i)
    if (m_lines.at (i).messageId == messageId) return i;
  return -1;
}

bool JttyLineModel::contains (qint64 messageId) const
{
  return indexOf (messageId) >= 0;
}

JttyLineModel::Line const* JttyLineModel::lineAt (int row) const
{
  return row >= 0 && row < m_lines.size () ? &m_lines.at (row) : nullptr;
}

bool JttyLineModel::upsert (Line const& line)
{
  int const i = indexOf (line.messageId);
  if (i >= 0)
    {
      Line& l = m_lines[i];
      l.text = line.text;
      l.frequency = line.frequency;
      l.complete = l.complete || line.complete;
      emit dataChanged (index (i), index (i));
      return false;
    }
  // In ordine di inizio: un messaggio ritrovato dai ripassi retroattivi puo'
  // essere cominciato prima di uno gia' in lista.
  int pos = m_lines.size ();
  while (pos > 0 && m_lines.at (pos - 1).startUtc > line.startUtc) --pos;
  beginInsertRows ({}, pos, pos);
  m_lines.insert (pos, line);
  endInsertRows ();
  trim ();
  emit countChanged ();
  return true;
}

void JttyLineModel::appendTx (QDateTime const& when, int frequency, QString const& text)
{
  Line l;
  l.messageId = m_nextTxId--;
  l.startUtc = when;
  l.frequency = frequency;
  l.text = text;
  l.complete = true;
  l.tx = true;
  beginInsertRows ({}, m_lines.size (), m_lines.size ());
  m_lines.append (l);
  endInsertRows ();
  trim ();
  emit countChanged ();
}

void JttyLineModel::clear ()
{
  beginResetModel ();
  m_lines.clear ();
  endResetModel ();
  emit countChanged ();
}

void JttyLineModel::trim ()
{
  if (m_lines.size () <= kMaxLines) return;
  int const drop = m_lines.size () - kMaxLines;
  beginRemoveRows ({}, 0, drop - 1);
  m_lines.remove (0, drop);
  endRemoveRows ();
}

// ------------------------------------------------------------------ ricevitore

JttyRxWorker::JttyRxWorker () : m_rx {new Receiver} {}

JttyRxWorker::~JttyRxWorker ()
{
  delete m_rx;
}

void JttyRxWorker::reset (qint64 generation)
{
  m_generation = generation;
  m_rx->reset ();
}

void JttyRxWorker::process (qint64 generation, QVector<short> samples, float f0, float ftol, int nfa,
                            int nfb)
{
  if (generation != m_generation) return;   // audio di una sessione chiusa
  m_rx->set_parameters (nfa, nfb, f0, ftol);
  m_rx->add_samples (reinterpret_cast<std::int16_t const*> (samples.constData ()), samples.size ());
  std::vector<MessageUpdate> const ups = m_rx->take_updates ();
  if (ups.empty ()) return;
  QVariantList list;
  list.reserve (static_cast<int> (ups.size ()));
  for (auto const& u : ups)
    {
      QVariantMap m;
      m.insert (QStringLiteral ("id"), static_cast<qlonglong> (u.message_id));
      m.insert (QStringLiteral ("freq"), u.frequency);
      m.insert (QStringLiteral ("start"), u.start_seconds);
      m.insert (QStringLiteral ("text"), QString::fromLatin1 (u.text.c_str ()));
      m.insert (QStringLiteral ("complete"), u.complete);
      list.append (m);
    }
  emit updates (generation, list);
}

// ------------------------------------------------------------------ controllo

JttyController::JttyController (QObject* parent)
  : QObject {parent}
  , m_qso {this}
  , m_all {this}
{
  qRegisterMetaType<QVector<short>> ();
  for (int k = 1; k <= 8; ++k) m_macros << Jtty::nativeMacroTemplate (k);

  m_thread.setObjectName (QStringLiteral ("JttyRx"));
  auto* worker = new JttyRxWorker;
  worker->moveToThread (&m_thread);
  m_worker = worker;
  connect (&m_thread, &QThread::finished, worker, &QObject::deleteLater);
  connect (this, &JttyController::workerReset, worker, &JttyRxWorker::reset, Qt::QueuedConnection);
  connect (this, &JttyController::workerProcess, worker, &JttyRxWorker::process, Qt::QueuedConnection);
  connect (worker, &JttyRxWorker::updates, this, &JttyController::onUpdates, Qt::QueuedConnection);
  m_thread.start ();

  m_txTimer.setInterval (20);
  m_txTimer.setTimerType (Qt::PreciseTimer);
  connect (&m_txTimer, &QTimer::timeout, this, &JttyController::pump);
  m_txTail.setSingleShot (true);
  m_txTail.setInterval (kTxTailMs);
  connect (&m_txTail, &QTimer::timeout, this, [this] { finishTransmit (QStringLiteral ("drained")); });
  m_txWatchdog.setSingleShot (true);
  connect (&m_txWatchdog, &QTimer::timeout, this, [this] {
    setError (tr ("Transmission stopped by the watchdog."));
    finishTransmit (QStringLiteral ("watchdog"));
  });
}

JttyController::~JttyController ()
{
  if (m_transmitting && m_hooks.keyPtt) m_hooks.keyPtt (false);
  m_thread.quit ();
  m_thread.wait (3000);
}

void JttyController::setHooks (Hooks hooks)
{
  m_hooks = std::move (hooks);
}

void JttyController::log (QString const& s) const
{
  if (m_hooks.log) m_hooks.log (QStringLiteral ("JTTY: ") + s);
}

void JttyController::load (QSettings& settings)
{
  m_settings = &settings;
  settings.beginGroup (QStringLiteral ("JTTY"));
  m_ftol = qBound (10, settings.value (QStringLiteral ("Ftol"), 50).toInt (), 500);
  m_serial = qBound (0, settings.value (QStringLiteral ("SerialNumber"), 1).toInt (), 131071);
  m_profile = qBound (0, settings.value (QStringLiteral ("ExchangeProfile"), 0).toInt (), 2);
  m_exchange = settings.value (QStringLiteral ("Exchange")).toString ();
  m_lowerCase = settings.value (QStringLiteral ("LowerCase"), false).toBool ();
  m_autoLogOnTu = settings.value (QStringLiteral ("AutoLogOnTu"), true).toBool ();
  for (int k = 1; k <= 8; ++k)
    {
      QString const key = QStringLiteral ("Macro%1").arg (k);
      if (settings.contains (key))
        m_macros[k - 1] = Jtty::migratedNativeMacroTemplate (k, settings.value (key).toString ());
    }
  settings.endGroup ();
  emit ftolChanged ();
  emit serialNumberChanged ();
  emit exchangeProfileChanged ();
  emit exchangeChanged ();
  emit lowerCaseChanged ();
  emit autoLogOnTuChanged ();
  emit macrosChanged ();
}

void JttyController::save ()
{
  if (!m_settings) return;
  m_settings->beginGroup (QStringLiteral ("JTTY"));
  m_settings->setValue (QStringLiteral ("Ftol"), m_ftol);
  m_settings->setValue (QStringLiteral ("SerialNumber"), m_serial);
  m_settings->setValue (QStringLiteral ("ExchangeProfile"), m_profile);
  m_settings->setValue (QStringLiteral ("Exchange"), m_exchange);
  m_settings->setValue (QStringLiteral ("LowerCase"), m_lowerCase);
  m_settings->setValue (QStringLiteral ("AutoLogOnTu"), m_autoLogOnTu);
  for (int k = 1; k <= 8; ++k) m_settings->setValue (QStringLiteral ("Macro%1").arg (k), m_macros.at (k - 1));
  m_settings->endGroup ();
  m_settings->sync ();
}

void JttyController::setFtol (int v)
{
  v = qBound (10, v, 500);
  if (v == m_ftol) return;
  m_ftol = v;
  save ();
  emit ftolChanged ();
}

void JttyController::setSerialNumber (int v)
{
  v = qBound (0, v, 131071);
  if (v == m_serial) return;
  m_serial = v;
  save ();
  emit serialNumberChanged ();
}

void JttyController::setExchangeProfile (int v)
{
  v = qBound (0, v, 2);
  if (v == m_profile) return;
  m_profile = v;
  save ();
  emit exchangeProfileChanged ();
}

void JttyController::setExchange (QString const& v)
{
  QString const t = v.simplified ().toUpper ();
  if (t == m_exchange) return;
  m_exchange = t;
  save ();
  emit exchangeChanged ();
}

void JttyController::setLowerCase (bool v)
{
  if (v == m_lowerCase) return;
  m_lowerCase = v;
  save ();
  emit lowerCaseChanged ();
}

void JttyController::setAutoLogOnTu (bool v)
{
  if (v == m_autoLogOnTu) return;
  m_autoLogOnTu = v;
  save ();
  emit autoLogOnTuChanged ();
}

void JttyController::setMacro (int key, QString const& text)
{
  if (key < 1 || key > 8 || m_macros.at (key - 1) == text) return;
  m_macros[key - 1] = text;
  save ();
  emit macrosChanged ();
}

void JttyController::resetMacros ()
{
  for (int k = 1; k <= 8; ++k) m_macros[k - 1] = Jtty::nativeMacroTemplate (k);
  save ();
  emit macrosChanged ();
}

void JttyController::setError (QString const& e)
{
  if (!e.isEmpty ()) log (e);
  if (e == m_lastError) return;
  m_lastError = e;
  emit lastErrorChanged ();
}

void JttyController::setActive (bool on)
{
  if (on == m_active) return;
  m_active = on;
  if (on) resetReceiver ();
  else if (m_transmitting) finishTransmit (QStringLiteral ("inactive"));
  emit activeChanged ();
}

void JttyController::leave ()
{
  if (m_transmitting) finishTransmit (QStringLiteral ("leaving JTTY"));
  setActive (false);
}

void JttyController::restartReceiver ()
{
  if (m_active) resetReceiver ();
}

void JttyController::resetReceiver ()
{
  ++m_generation;
  m_streamT0 = QDateTime {};
  m_rxPausedSince = QDateTime {};
  m_samplesFed = 0;
  emit workerReset (m_generation);
}

void JttyController::feedAudio (QVector<short> const& samples)
{
  if (!m_active || samples.isEmpty ()) return;
  QDateTime const now = QDateTime::currentDateTimeUtc ();
  qint64 const n = samples.size ();
  if (!m_streamT0.isValid ())
    {
      m_streamT0 = now.addMSecs (-n * 1000 / 12000);
      m_samplesFed = 0;
    }
  else if (m_rxPausedSince.isValid ())
    {
      // Durante la nostra trasmissione il bridge non consegna audio: il buco
      // si riempie di silenzio, cosi' i tempi dei messaggi restano quelli
      // dell'orologio. Solo qui si guarda l'orologio: nel flusso normale i
      // blocchi possono arrivare in ritardo e a raffica (interfaccia
      // occupata), e un confronto con l'orologio metterebbe silenzio in mezzo
      // a un frame.
      qint64 const gap = m_rxPausedSince.msecsTo (now) * 12 - n;
      m_rxPausedSince = QDateTime {};
      if (gap > 0 && gap < 120 * 12000)
        {
          qint64 left = gap;
          while (left > 0)
            {
              int const chunk = static_cast<int> (qMin<qint64> (left, 12000));
              emit workerProcess (m_generation, QVector<short> (chunk, 0), 0.0f, 0.0f, kNfa, kNfb);
              left -= chunk;
            }
          m_samplesFed += gap;
        }
      else if (gap >= 120 * 12000)
        {
          resetReceiver ();
          m_streamT0 = now.addMSecs (-n * 1000 / 12000);
        }
    }
  int const rx = m_hooks.rxFrequency ? m_hooks.rxFrequency () : 1500;
  emit workerProcess (m_generation, samples, static_cast<float> (rx), static_cast<float> (m_ftol), kNfa, kNfb);
  m_samplesFed += n;
}

void JttyController::onUpdates (qint64 generation, QVariantList list)
{
  if (generation != m_generation) return;
  int const rx = m_hooks.rxFrequency ? m_hooks.rxFrequency () : 1500;
  for (QVariant const& v : list)
    {
      QVariantMap const m = v.toMap ();
      JttyLineModel::Line line;
      line.messageId = m.value (QStringLiteral ("id")).toLongLong ();
      line.frequency = qRound (m.value (QStringLiteral ("freq")).toDouble ());
      line.startUtc = m_streamT0.addMSecs (qRound64 (m.value (QStringLiteral ("start")).toDouble () * 1000.0));
      line.text = m.value (QStringLiteral ("text")).toString ();
      line.complete = m.value (QStringLiteral ("complete")).toBool ();

      bool wasComplete = false;
      if (auto const* known = [&] () -> JttyLineModel::Line const* {
            for (int i = m_all.count () - 1; i >= 0; --i)
              if (m_all.lineAt (i)->messageId == line.messageId) return m_all.lineAt (i);
            return nullptr;
          } ())
        wasComplete = known->complete;
      m_all.upsert (line);
      // La finestra QSO: il messaggio resta suo anche se il decodificatore
      // lo ritrova qualche hertz piu' in la'.
      if (Jtty::shouldApplyToQsoHistory (m_qso.contains (line.messageId), m.value (QStringLiteral ("freq")).toFloat (),
                                         static_cast<float> (rx), static_cast<float> (m_ftol)))
        m_qso.upsert (line);
      if (line.complete && !wasComplete && !line.text.trimmed ().isEmpty ())
        {
          if (m_hooks.allTxt) m_hooks.allTxt (false, line.frequency, line.text, line.startUtc);
          if (m_hooks.decodedLine) m_hooks.decodedLine (line.text, line.frequency, line.startUtc);
        }
    }
}

bool JttyController::send (QString const& text)
{
  auto const prepared = Jtty::prepareTransmitText (text);
  QString const message = prepared.text.trimmed ();
  if (message.isEmpty ()) return false;
  QString const frameText = Jtty::transmitFrame (message, m_transmitting);
  std::vector<Frame> frames;
  std::string canonical;
  int const n = pack_message (frameText.toLatin1 ().toStdString (), m_profile, frames, &canonical);
  if (n <= 0)
    {
      setError (n < 0 ? tr ("The message does not fit in 16 JTTY frames.") : tr ("Nothing to send."));
      return false;
    }
  QVector<int> tones;
  for (int t : tones_for_frames (frames)) tones.append (t);
  return enqueue (QString::fromLatin1 (canonical.c_str ()).trimmed (), tones);
}

bool JttyController::sendFunctionKey (int key)
{
  if (key < 1 || key > 8) return false;
  QString const macro = m_macros.at (key - 1);
  if (macro.simplified ().isEmpty ()) return false;
  Jtty::NativeMacroContext const context {
    m_hooks.myCall ? m_hooks.myCall () : QString {}, m_hooks.hisCall ? m_hooks.hisCall () : QString {},
    m_serial, static_cast<Jtty::NativeExchangeProfile> (m_profile), m_exchange,
    m_hooks.myGrid ? m_hooks.myGrid () : QString {}};
  auto const compiled = Jtty::compileNativeMacro (macro, context);
  if (compiled.status == Jtty::NativeMacroStatus::LiteralFallback) return send (compiled.text);
  if (compiled.status == Jtty::NativeMacroStatus::InvalidRuntime)
    {
      setError (QStringLiteral ("F%1: %2").arg (key).arg (compiled.error));
      return false;
    }
  std::vector<Frame> frames;
  int const status = pack_native_atoms (reinterpret_cast<NativeAtomDescriptor const*> (compiled.atoms.constData ()),
                                        compiled.atoms.size (), frames);
  if (status != ENCODE_OK)
    {
      setError (QStringLiteral ("F%1: %2").arg (key).arg (nativeEncodeError (status)));
      return false;
    }
  QVector<int> tones;
  for (int t : tones_for_frames (frames)) tones.append (t);
  return enqueue (compiled.text, tones);
}

QString JttyController::previewFunctionKey (int key) const
{
  if (key < 1 || key > 8) return {};
  Jtty::NativeMacroContext const context {
    m_hooks.myCall ? m_hooks.myCall () : QString {}, m_hooks.hisCall ? m_hooks.hisCall () : QString {},
    m_serial, static_cast<Jtty::NativeExchangeProfile> (m_profile), m_exchange,
    m_hooks.myGrid ? m_hooks.myGrid () : QString {}};
  auto const compiled = Jtty::compileNativeMacro (m_macros.at (key - 1), context);
  if (compiled.status == Jtty::NativeMacroStatus::InvalidRuntime) return QStringLiteral ("!") + compiled.error;
  return compiled.text;
}

QString JttyController::describe (QString const& text) const
{
  QString const message = Jtty::prepareTransmitText (text).text.trimmed ();
  if (message.isEmpty ()) return {};
  std::vector<Frame> frames;
  int const n = pack_message (message.toLatin1 ().toStdString (), m_profile, frames);
  if (n < 0) return tr ("too long (max 16 frames)");
  return tr ("%1 frames, %2 s").arg (n).arg (n * 1.888, 0, 'f', 1);
}

bool JttyController::enqueue (QString const& logicalText, QVector<int> const& tones)
{
  bool const chained = m_transmitting;
  if (!chained)
    {
      if (!m_hooks.canTransmit || !m_hooks.canTransmit ())
        {
          setError (tr ("Cannot transmit: JTTY must be the active mode, the TX audio output must be "
                        "available and no other transmission may be running."));
          return false;
        }
      if (m_hooks.keyPtt) m_hooks.keyPtt (true);
      if (m_hooks.txActive && !m_hooks.txActive ())
        {
          setError (tr ("The transmitter did not key (PTT refused)."));
          return false;
        }
      if (m_streamT0.isValid ()) m_rxPausedSince = QDateTime::currentDateTimeUtc ();
      m_pcm = QVector<short> (kTxPreambleSamples, 0);
      m_pcmPos = 0;
      m_txSamplesSent = 0;
      m_txClock.start ();
      m_transmitting = true;
      m_sendingText.clear ();
      m_txTail.stop ();
      m_txTimer.start ();
      emit transmittingChanged ();
    }
  setError ({});

  int const txf = m_hooks.txFrequency ? m_hooks.txFrequency () : 1500;
  std::vector<int> const t (tones.cbegin (), tones.cend ());
  std::vector<float> const wave = generate_wave (t, kNsps12k, kBt, 12000.0f, static_cast<float> (txf));
  double const amp = qBound (0.0, m_hooks.txAmplitude ? m_hooks.txAmplitude () : 0.9, 1.0) * 32767.0;
  // Si compatta la coda gia' suonata prima di aggiungere.
  if (m_pcmPos > 0)
    {
      m_pcm.remove (0, m_pcmPos);
      m_pcmPos = 0;
    }
  m_pcm.reserve (m_pcm.size () + static_cast<int> (wave.size ()));
  for (float const w : wave) m_pcm.append (static_cast<short> (qBound (-32768.0, std::round (w * amp), 32767.0)));
  m_txTail.stop ();

  QDateTime const now = QDateTime::currentDateTimeUtc ();
  m_sendingText = chained ? m_sendingText + QLatin1Char (' ') + logicalText : logicalText;
  emit sendingTextChanged ();
  m_qso.appendTx (now, txf, logicalText);
  if (m_hooks.allTxt) m_hooks.allTxt (true, txf, logicalText, now);
  log (QStringLiteral ("TX %1 frames at %2 Hz%3: %4")
           .arg (tones.size () / 59).arg (txf).arg (chained ? QStringLiteral (" (chained)") : QString {})
           .arg (logicalText));

  // Il margine del cane da guardia: tutto l'audio ancora in coda e dieci secondi.
  m_txWatchdog.start (static_cast<int> ((m_pcm.size () - m_pcmPos) / 12) + 10000);

  // Come WSJT-X: un messaggio che comincia con "TU " chiude il collegamento.
  if (logicalText.startsWith (QStringLiteral ("TU "), Qt::CaseInsensitive))
    {
      if (m_autoLogOnTu) logQso ();
      setSerialNumber (m_serial + 1);
    }
  return true;
}

void JttyController::pump ()
{
  if (!m_transmitting) return;
  if (m_hooks.txActive && !m_hooks.txActive ())
    {
      setError (tr ("Transmission interrupted: PTT dropped."));
      finishTransmit (QStringLiteral ("ptt dropped"));
      return;
    }
  qint64 const due = m_txClock.elapsed () * 12 + kTxLeadSamples;
  qint64 needed = qMin<qint64> (due - m_txSamplesSent, 3000);
  if (needed <= 0) return;
  int const available = m_pcm.size () - m_pcmPos;
  if (available <= 0)
    {
      if (!m_txTail.isActive ()) m_txTail.start ();
      return;
    }
  int const n = static_cast<int> (qMin<qint64> (needed, available));
  QVector<short> chunk (m_pcm.constData () + m_pcmPos, m_pcm.constData () + m_pcmPos + n);
  m_pcmPos += n;
  m_txSamplesSent += n;
  if (m_hooks.sendAudio) m_hooks.sendAudio (chunk);
}

void JttyController::finishTransmit (QString const& reason)
{
  if (!m_transmitting) return;
  m_txTimer.stop ();
  m_txTail.stop ();
  m_txWatchdog.stop ();
  m_pcm.clear ();
  m_pcmPos = 0;
  m_transmitting = false;
  if (m_hooks.keyPtt) m_hooks.keyPtt (false);
  log (QStringLiteral ("TX end (%1)").arg (reason));
  m_sendingText.clear ();
  emit sendingTextChanged ();
  emit transmittingChanged ();
}

void JttyController::abort ()
{
  if (m_transmitting) finishTransmit (QStringLiteral ("abort"));
}

void JttyController::clearHistory ()
{
  m_qso.clear ();
  m_all.clear ();
}

void JttyController::pickLine (bool qsoList, int row)
{
  JttyLineModel const& model = qsoList ? m_qso : m_all;
  auto const* line = model.lineAt (row);
  if (!line || line->tx) return;
  if (!qsoList && m_hooks.setRxFrequency) m_hooks.setRxFrequency (line->frequency);
  QString const mine = m_hooks.myCall ? m_hooks.myCall ().trimmed ().toUpper () : QString {};
  for (QString const& w : line->text.split (QLatin1Char (' '), Qt::SkipEmptyParts))
    {
      QString const token = w.toUpper ();
      if (token == mine) continue;
      if (standard_call (token.toLatin1 ().toStdString ()))
        {
          if (m_hooks.setHisCall) m_hooks.setHisCall (token);
          break;
        }
    }
}

void JttyController::logQso ()
{
  QString const call = m_hooks.hisCall ? m_hooks.hisCall ().trimmed ().toUpper () : QString {};
  if (call.isEmpty ())
    {
      setError (tr ("No DX call to log."));
      return;
    }
  QString sent;
  switch (m_profile)
    {
    case 1: sent = m_exchange; break;
    case 2:
      sent = (m_exchange.isEmpty () || m_exchange == QStringLiteral ("DX") || m_exchange == QStringLiteral ("#"))
          ? QStringLiteral ("599 %1").arg (Jtty::formatSerialNumber (m_serial))
          : QStringLiteral ("599 %1").arg (m_exchange);
      break;
    default: sent = QStringLiteral ("599 %1").arg (Jtty::formatSerialNumber (m_serial)); break;
    }
  if (m_hooks.logQso) m_hooks.logQso (call, sent, QStringLiteral ("599"));
}

}
}
