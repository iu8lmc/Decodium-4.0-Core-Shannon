#include "Transceiver/TCITransceiver.hpp"

#include <QRegularExpression>
#include <QLocale>
#include <QThread>
#include <QQueue>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <qmath.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#include <QRandomGenerator>
#endif

#include <QString>
#include "widgets/itoneAndicw.h"
#include "widgets/FoxWaveGuard.hpp"
#include "Network/NetworkServerLookup.hpp"
#include "moc_TCITransceiver.cpp"

#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QReadLocker>

namespace
{
  char const * const TCI_transceiver_1_name {"TCI Client RX1"};
  char const * const TCI_transceiver_2_name {"TCI Client RX2"};

  QString map_mode (Transceiver::MODE mode)
  {
    switch (mode)
      {
      case Transceiver::AM: return "am";
      case Transceiver::CW: return "cw";
//      case Transceiver::CW_R: return "CW-R";
      case Transceiver::USB: return "usb";
      case Transceiver::LSB: return "lsb";
      case Transceiver::FSK: return "fsk";
      case Transceiver::FSK_R: return "fsk-r";
      case Transceiver::DIG_L: return "digl";
      case Transceiver::DIG_U: return "digu";
      case Transceiver::FM: return "wfm";
      case Transceiver::DIG_FM:
        return "nfm";
      default: break;
      }
    return "";
  }
  static const QString SmTZ(";");
  static const QString SmDP(":");
  static const QString SmCM(",");
  static const QString SmTrue("true");
  static const QString SmFalse("false");

  QString tci_unescape (QString const& input)
  {
    QString out;
    out.reserve (input.size ());
    bool escaped = false;
    for (auto const ch : input)
      {
        if (escaped)
          {
            out.append (ch);
            escaped = false;
            continue;
          }
        if (ch == QChar {'\\'})
          {
            escaped = true;
            continue;
          }
        out.append (ch);
      }
    if (escaped)
      {
        out.append (QChar {'\\'});
      }
    return out;
  }

  QStringList tci_split_escaped (QString const& input, QChar delimiter, Qt::SplitBehavior behavior)
  {
    QStringList out;
    QString current;
    current.reserve (input.size ());
    bool escaped = false;
    for (auto const ch : input)
      {
        if (escaped)
          {
            current.append (ch);
            escaped = false;
            continue;
          }
        if (ch == QChar {'\\'})
          {
            escaped = true;
            continue;
          }
        if (ch == delimiter)
          {
            if (behavior == Qt::KeepEmptyParts || !current.isEmpty ())
              {
                out << current;
              }
            current.clear ();
            continue;
          }
        current.append (ch);
      }
    if (escaped)
      {
        current.append (QChar {'\\'});
      }
    if (behavior == Qt::KeepEmptyParts || !current.isEmpty ())
      {
        out << current;
      }
    return out;
  }

  bool tci_split_command (QString const& command, QString * name, QString * args)
  {
    bool escaped = false;
    int sep = -1;
    for (int i = 0; i < command.size (); ++i)
      {
        auto const ch = command.at (i);
        if (escaped)
          {
            escaped = false;
            continue;
          }
        if (ch == QChar {'\\'})
          {
            escaped = true;
            continue;
          }
        if (ch == QChar {':'})
          {
            sep = i;
            break;
          }
      }

    if (sep < 0)
      {
        if (name) *name = command;
        if (args) *args = QString {};
        return !command.trimmed ().isEmpty ();
      }

    if (name) *name = command.left (sep);
    if (args) *args = command.mid (sep + 1);
    return true;
  }

  // Command maps
  static const QString CmdDevice("device");
  static const QString CmdReceiveOnly("receive_only");
  static const QString CmdTrxCount("trx_count");
  static const QString CmdChannelCount("channels_count");
  static const QString CmdVfoLimits("vfo_limits");
  static const QString CmdIfLimits("if_limits");
  static const QString CmdModeList("modulations_list");
  static const QString CmdMode("modulation");
  static const QString CmdReady("ready");
  static const QString CmdStop("stop");
  static const QString CmdStart("start");
  static const QString CmdPreamp("preamp");
  static const QString CmdDds("dds");
  static const QString CmdIf("if");
  static const QString CmdTrx("trx");
  static const QString CmdRxEnable("rx_enable");
  static const QString CmdTxEnable("tx_enable");
  static const QString CmdRxChannelEnable("rx_channel_enable");
  static const QString CmdRitEnable("rit_enable");
  static const QString CmdRitOffset("rit_offset");
  static const QString CmdXitEnable("xit_enable");
  static const QString CmdXitOffset("xit_offset");
  static const QString CmdSplitEnable("split_enable");
  static const QString CmdIqSR("iq_samplerate");
  static const QString CmdIqStart("iq_start");
  static const QString CmdIqStop("iq_stop");
  static const QString CmdCWSpeed("cw_macros_speed");
  static const QString CmdCWDelay("cw_macros_delay");
  static const QString CmdSpot("spot");
  static const QString CmdSpotDelete("spot_delete");
  static const QString CmdFilterBand("rx_filter_band");
  static const QString CmdVFO("vfo");
  static const QString CmdVersion("protocol"); //protocol:esdr,1.2;
  static const QString CmdTune("tune");
  static const QString CmdRxMute("rx_mute");
  static const QString CmdSmeter("rx_smeter");
  static const QString CmdPower("tx_power");
  static const QString CmdSWR("tx_swr");
  static const QString CmdECoderRX("ecoder_switch_rx");
  static const QString CmdECoderVFO("ecoder_switch_channel");
  static const QString CmdAudioSR("audio_samplerate");
  static const QString CmdAudioStart("audio_start");
  static const QString CmdAudioStop("audio_stop");
  static const QString CmdAppFocus("app_focus");
  static const QString CmdVolume("volume");
  static const QString CmdSqlEnable("sql_enable");
  static const QString CmdSqlLevel("sql_level");
  static const QString CmdDrive("drive");
  static const QString CmdTuneDrive("tune_drive");
  static const QString CmdMute("mute");
  static const QString CmdRxSensorsEnable("rx_sensors_enable");
  static const QString CmdTxSensorsEnable("tx_sensors_enable");
  static const QString CmdRxSensors("rx_sensors");
  static const QString CmdTxSensors("tx_sensors");
  static const QString CmdAgcMode("agc_mode");
  static const QString CmdAgcGain("agc_gain");
  static const QString CmdLock("lock");

  constexpr int kMaxKin = NTMAX * RX_SAMPLE_RATE;
  constexpr quint64 kFloatPairBytes = sizeof(float) * 2ULL;

  bool checked_payload_size (quint32 length, int available_bytes, quint64 * required_bytes)
  {
    if (available_bytes < 0)
      {
        return false;
      }
    quint64 req = static_cast<quint64> (length) * kFloatPairBytes;
    if (req > static_cast<quint64> (available_bytes))
      {
        return false;
      }
    if (required_bytes)
      {
        *required_bytes = req;
      }
    return true;
  }

  bool parse_decimal_tenths (QString const& value, int * out)
  {
    if (!out)
      {
        return false;
      }
    auto const parts = value.split ('.', Qt::KeepEmptyParts);
    if (parts.size () < 2)
      {
        return false;
      }
    bool ok_whole = false;
    bool ok_fraction = false;
    auto const whole = parts.value (0).toInt (&ok_whole);
    auto const fraction = parts.value (1).toInt (&ok_fraction);
    if (!ok_whole || !ok_fraction)
      {
        return false;
      }
    *out = 10 * whole + fraction;
    return true;
  }

  QUrl make_tci_ws_url (QString const& server)
  {
    auto endpoint = server.trimmed ();
    QUrl url;
    if (endpoint.contains ("://"))
      {
        url = QUrl {endpoint};
      }
    else
      {
        url = QUrl {"ws://" + endpoint};
      }

    if (url.scheme () == "http")
      {
        url.setScheme ("ws");
      }
    else if (url.scheme () == "https")
      {
        url.setScheme ("wss");
      }
    else if (url.scheme ().isEmpty ())
      {
        url.setScheme ("ws");
      }

    if (url.host ().isEmpty ())
      {
        url.setHost ("localhost");
      }
    if (url.port () == -1)
      {
        url.setPort (url.scheme ().toLower () == "wss" ? 443 : 50001);
      }
    return url;
  }
}

class TciSocketWorker final : public QObject
{
  Q_OBJECT
public:
  explicit TciSocketWorker (QObject * parent = nullptr)
    : QObject {parent}
  {
  }

public slots:
  void openSocket (QUrl const& url)
  {
    if (!socket_)
      {
        socket_ = new QWebSocket {};
        socket_->setParent (this);
        connect (socket_, &QWebSocket::connected, this, &TciSocketWorker::connected);
        connect (socket_, &QWebSocket::disconnected, this, &TciSocketWorker::disconnected);
        connect (socket_, &QWebSocket::textMessageReceived, this, &TciSocketWorker::textFrame);
        connect (socket_, &QWebSocket::binaryMessageReceived, this, &TciSocketWorker::binaryFrame);
        connect (socket_, QOverload<QAbstractSocket::SocketError>::of (&QWebSocket::error),
                 this, [this] (QAbstractSocket::SocketError err) {
                   Q_EMIT socketError (static_cast<int> (err), socket_->errorString ());
                 });
      }
    socket_->open (url);
  }

  void closeSocket ()
  {
    if (socket_)
      {
        socket_->close (QWebSocketProtocol::CloseCodeNormal, QStringLiteral ("end"));
      }
  }

  void sendText (QString const& message)
  {
    if (socket_ && socket_->state () == QAbstractSocket::ConnectedState)
      {
        socket_->sendTextMessage (message);
      }
  }

  void sendBinary (QByteArray const& payload)
  {
    if (socket_ && socket_->state () == QAbstractSocket::ConnectedState)
      {
        socket_->sendBinaryMessage (payload);
      }
  }

signals:
  void connected ();
  void disconnected ();
  void textFrame (QString const& text);
  void binaryFrame (QByteArray const& data);
  void socketError (int err, QString const& message);

private:
  QWebSocket * socket_ {nullptr};
};

extern "C" {
  void   fil4_(qint16*, qint32*, qint16*, qint32*, short int*);
}
extern dec_data dec_data;

extern float gran();		// Noise generator (for tests only)

#define RAMP_INCREMENT 64  // MUST be an integral factor of 2^16

#if defined (WSJT_SOFT_KEYING)
# define SOFT_KEYING WSJT_SOFT_KEYING
#else
# define SOFT_KEYING 1
#endif

double constexpr TCITransceiver::m_twoPi;

void TCITransceiver::register_transceivers (logger_type *, TransceiverFactory::Transceivers * registry, unsigned id1, unsigned id2)
{
  (*registry)[TCI_transceiver_1_name] = TransceiverFactory::Capabilities {id1, TransceiverFactory::Capabilities::tci, true};
  (*registry)[TCI_transceiver_2_name] = TransceiverFactory::Capabilities {id2, TransceiverFactory::Capabilities::tci, true};
}

static constexpr quint32 AudioHeaderSize = 16u*sizeof(quint32);

TCITransceiver::TCITransceiver (logger_type * logger, std::unique_ptr<TransceiverBase> wrapped,QString const& rignr,
                               QString const& address, bool use_for_ptt,
                               int poll_interval, QObject * parent)
  : PollingTransceiver {logger, poll_interval, parent}
  , wrapped_ {std::move (wrapped)}
  , rx_ {rignr}
  , server_ {address}
  , use_for_ptt_ {use_for_ptt}
  , errortable {tr("ConnectionRefused"),
                tr("RemoteHostClosed"),
                tr("HostNotFound"),
                tr("SocketAccess"),
                tr("SocketResource"),
                tr("SocketTimeout"),
                tr("DatagramTooLarge"),
                tr("Network"),
                tr("AddressInUse"),
                tr("SocketAddressNotAvailable"),
                tr("UnsupportedSocketOperation"),
                tr("UnfinishedSocketOperation"),
                tr("ProxyAuthenticationRequired"),
                tr("SslHandshakeFailed"),
                tr("ProxyConnectionRefused"),
                tr("ProxyConnectionClosed"),
                tr("ProxyConnectionTimeout"),
                tr("ProxyNotFound"),
                tr("ProxyProtocol"),
                tr("Operation"),
                tr("SslInternal"),
                tr("SslInvalidUserData"),
                tr("Temporary"),
                tr("UnknownSocket") }
  , error_ {""}
  , do_snr_ {(poll_interval & do__snr) == do__snr}
  , do_pwr_ {(poll_interval & do__pwr) == do__pwr}
  , rig_power_ {(poll_interval & rig__power) == rig__power}
  , rig_power_off_ {(poll_interval & rig__power_off) == rig__power_off}
  , tci_audio_ {(poll_interval & tci__audio) == tci__audio}
  , tci_timer1_ {nullptr}
  , tci_timer2_ {nullptr}
  , tci_timer3_ {nullptr}
  , tci_timer4_ {nullptr}
  , tci_timer5_ {nullptr}
  , tci_timer6_ {nullptr}
  , tci_timer7_ {nullptr}
  , tci_timer8_ {nullptr}
  , wavptr_ {nullptr}
  , m_downSampleFactor {4}
  , m_buffer ((m_downSampleFactor > 1) ?
              new short [max_buffer_size * m_downSampleFactor] : nullptr)
  , m_quickClose {false}
  , m_phi {0.0}
  , m_toneSpacing {0.0}
  , m_fSpread {0.0}
  , m_state {Idle}
  , m_tuning {false}
  , m_cwLevel {false}
  , m_j0 {-1}
  , m_toneFrequency0 {1500.0}
  , wav_file_ {QDir(QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)).absoluteFilePath ("tx.wav").toStdString()}
{
  m_samplesPerFFT = 6912 / 2;
  tci_Ready = false;
  trxA = 0;
  trxB = 0;
  cntIQ = 0;
  bIQ = false;
  inConnected = false;
  audioSampleRate = 48000u;
  mapCmd_[CmdDevice]       = Cmd_Device;
  mapCmd_[CmdReceiveOnly]  = Cmd_ReceiveOnly;
  mapCmd_[CmdTrxCount]     = Cmd_TrxCount;
  mapCmd_[CmdChannelCount] = Cmd_ChannelCount;
  mapCmd_[CmdVfoLimits]    = Cmd_VfoLimits;
  mapCmd_[CmdIfLimits]     = Cmd_IfLimits;
  mapCmd_[CmdModeList]     = Cmd_ModeList;
  mapCmd_[CmdMode]         = Cmd_Mode;
  mapCmd_[CmdReady]        = Cmd_Ready;
  mapCmd_[CmdStop]         = Cmd_Stop;
  mapCmd_[CmdStart]        = Cmd_Start;
  mapCmd_[CmdPreamp]       = Cmd_Preamp;
  mapCmd_[CmdDds]          = Cmd_Dds;
  mapCmd_[CmdIf]           = Cmd_If;
  mapCmd_[CmdTrx]          = Cmd_Trx;
  mapCmd_[CmdRxEnable]     = Cmd_RxEnable;
  mapCmd_[CmdTxEnable]     = Cmd_TxEnable;
  mapCmd_[CmdRxChannelEnable] = Cmd_RxChannelEnable;
  mapCmd_[CmdRitEnable]    = Cmd_RitEnable;
  mapCmd_[CmdRitOffset]    = Cmd_RitOffset;
  mapCmd_[CmdXitEnable]    = Cmd_XitEnable;
  mapCmd_[CmdXitOffset]    = Cmd_XitOffset;
  mapCmd_[CmdSplitEnable]  = Cmd_SplitEnable;
  mapCmd_[CmdIqSR]         = Cmd_IqSR;
  mapCmd_[CmdIqStart]      = Cmd_IqStart;
  mapCmd_[CmdIqStop]       = Cmd_IqStop;
  mapCmd_[CmdCWSpeed]      = Cmd_CWSpeed;
  mapCmd_[CmdCWDelay]      = Cmd_CWDelay;
  mapCmd_[CmdFilterBand]   = Cmd_FilterBand;
  mapCmd_[CmdVFO]          = Cmd_VFO;
  mapCmd_[CmdVersion]      = Cmd_Version;
  mapCmd_[CmdTune]         = Cmd_Tune;
  mapCmd_[CmdRxMute]       = Cmd_RxMute;
  mapCmd_[CmdSmeter]       = Cmd_Smeter;
  mapCmd_[CmdPower]        = Cmd_Power;
  mapCmd_[CmdSWR]          = Cmd_SWR;
  mapCmd_[CmdECoderRX]     = Cmd_ECoderRX;
  mapCmd_[CmdECoderVFO]    = Cmd_ECoderVFO;
  mapCmd_[CmdAudioSR]      = Cmd_AudioSR;
  mapCmd_[CmdAudioStart]   = Cmd_AudioStart;
  mapCmd_[CmdAudioStop]    = Cmd_AudioStop;
  mapCmd_[CmdAppFocus]     = Cmd_AppFocus;
  mapCmd_[CmdVolume]       = Cmd_Volume;
  mapCmd_[CmdSqlEnable]    = Cmd_SqlEnable;
  mapCmd_[CmdSqlLevel]     = Cmd_SqlLevel;
  mapCmd_[CmdDrive]        = Cmd_Drive;
  mapCmd_[CmdTuneDrive]    = Cmd_TuneDrive;
  mapCmd_[CmdMute]         = Cmd_Mute;
  mapCmd_[CmdRxSensorsEnable] = Cmd_RxSensorsEnable;
  mapCmd_[CmdTxSensorsEnable] = Cmd_TxSensorsEnable;
  mapCmd_[CmdRxSensors]    = Cmd_RxSensors;
  mapCmd_[CmdTxSensors]    = Cmd_TxSensors;
  mapCmd_[CmdAgcMode]      = Cmd_AgcMode;
  mapCmd_[CmdAgcGain]      = Cmd_AgcGain;
  mapCmd_[CmdLock]         = Cmd_Lock;
}

TCITransceiver::~TCITransceiver ()
{
  shutdown_socket_worker ();
}

void TCITransceiver::ensure_socket_worker ()
{
  if (socket_thread_ && socket_worker_)
    {
      return;
    }

  if (!parse_queue_timer_)
    {
      parse_queue_timer_ = new QTimer {this};
      parse_queue_timer_->setSingleShot (true);
      connect (parse_queue_timer_, &QTimer::timeout, this, &TCITransceiver::process_pending_tci_frames);
    }

  socket_thread_ = new QThread {this};
  socket_thread_->setObjectName (QStringLiteral ("TCIWebSocketThread"));
  auto * worker = new TciSocketWorker {};
  socket_worker_ = worker;
  worker->moveToThread (socket_thread_);
  connect (socket_thread_, &QThread::finished, worker, &QObject::deleteLater);

  connect (this, &TCITransceiver::request_socket_open, worker, &TciSocketWorker::openSocket, Qt::QueuedConnection);
  connect (this, &TCITransceiver::request_socket_close, worker, &TciSocketWorker::closeSocket, Qt::QueuedConnection);
  connect (this, &TCITransceiver::request_socket_send_text, worker, &TciSocketWorker::sendText, Qt::QueuedConnection);
  connect (this, &TCITransceiver::request_socket_send_binary, worker, &TciSocketWorker::sendBinary, Qt::QueuedConnection);

  connect (worker, &TciSocketWorker::connected, this, &TCITransceiver::onConnected, Qt::QueuedConnection);
  connect (worker, &TciSocketWorker::disconnected, this, &TCITransceiver::onDisconnected, Qt::QueuedConnection);
  connect (worker, &TciSocketWorker::textFrame, this, &TCITransceiver::onSocketTextFrame, Qt::QueuedConnection);
  connect (worker, &TciSocketWorker::binaryFrame, this, &TCITransceiver::onSocketBinaryFrame, Qt::QueuedConnection);
  connect (worker, &TciSocketWorker::socketError, this, [this] (int err, QString const& message) {
    auto const casted = static_cast<QAbstractSocket::SocketError> (err);
    onError (casted);
    if (!message.isEmpty ())
      {
        error_ = tr ("TCI websocket error: %1").arg (message);
      }
  }, Qt::QueuedConnection);

  socket_thread_->start ();
}

void TCITransceiver::shutdown_socket_worker ()
{
  if (socket_thread_)
    {
      if (socket_worker_)
        {
          Q_EMIT request_socket_close ();
        }
      socket_thread_->quit ();
      socket_thread_->wait (500);
      socket_worker_ = nullptr;
      socket_thread_->deleteLater ();
      socket_thread_ = nullptr;
    }
  pending_text_frames_.clear ();
  pending_binary_frames_.clear ();
}

void TCITransceiver::onConnected()
{
  inConnected = true;
  CAT_TRACE ("TCITransceiver entered TCI onConnected and inConnected==true\n");
}

void TCITransceiver::onDisconnected()
{
  inConnected = false;
  CAT_TRACE ("TCITransceiver entered TCI onDisonnected and inConnected==false\n");
}


void TCITransceiver::onError(QAbstractSocket::SocketError err)
{
  qDebug() << "WebInThread::onError";
  CAT_TRACE ("TCITransceiver entered TCI onError and ErrorNumber is " + QString::number(err) + '\n');
  auto const index = static_cast<int> (err);
  auto const error_name = (index >= 0 && index < errortable.size ())
      ? errortable.at (index)
      : tr ("UnknownSocket");
  error_ = tr ("TCI websocket error: %1").arg (error_name);
}

void TCITransceiver::onSocketTextFrame(QString const& text)
{
  static constexpr int kMaxQueuedTextFrames = 2048;
  if (pending_text_frames_.size () >= kMaxQueuedTextFrames)
    {
      pending_text_frames_.dequeue ();
    }
  pending_text_frames_.enqueue (text);
  if (parse_queue_timer_ && !parse_queue_timer_->isActive ())
    {
      parse_queue_timer_->start (0);
    }
}

void TCITransceiver::onSocketBinaryFrame(QByteArray const& data)
{
  static constexpr int kMaxQueuedBinaryFrames = 256;
  if (pending_binary_frames_.size () >= kMaxQueuedBinaryFrames)
    {
      pending_binary_frames_.dequeue ();
    }
  pending_binary_frames_.enqueue (data);
  if (parse_queue_timer_ && !parse_queue_timer_->isActive ())
    {
      parse_queue_timer_->start (0);
    }
}

void TCITransceiver::process_pending_tci_frames()
{
  static constexpr int kTextBudgetPerTick = 64;
  static constexpr int kBinaryBudgetPerTick = 16;

  int text_budget = kTextBudgetPerTick;
  while (text_budget-- > 0 && !pending_text_frames_.isEmpty ())
    {
      onMessageReceived (pending_text_frames_.dequeue ());
    }

  int binary_budget = kBinaryBudgetPerTick;
  while (binary_budget-- > 0 && !pending_binary_frames_.isEmpty ())
    {
      onBinaryReceived (pending_binary_frames_.dequeue ());
    }

  if (( !pending_text_frames_.isEmpty () || !pending_binary_frames_.isEmpty ())
      && parse_queue_timer_)
    {
      parse_queue_timer_->start (0);
    }
}

int TCITransceiver::do_start ()
{
  if (tci_audio_) QThread::currentThread()->setPriority(QThread::HighPriority);
  CAT_TRACE ("TCITransceiver entered TCI do_start and tci_Ready is " + QString::number(tci_Ready) + '\n');
  qDebug () << "qDebug says do_start tci_Ready is: " << tci_Ready;
  if (wrapped_) wrapped_->start (0);
  url_ = make_tci_ws_url (server_);
  ensure_socket_worker ();
  if (!tci_timer1_) {
    tci_timer1_ = new QTimer {this};
    tci_timer1_ -> setSingleShot(true);
    connect(this, &TCITransceiver::tci_done1, this, [this]() {
      if (tci_timer1_ && tci_timer1_->isActive()) tci_timer1_->stop();
    });
  }
  if (!tci_timer2_) {
    tci_timer2_ = new QTimer {this};
    tci_timer2_ -> setSingleShot(true);
    connect(this, &TCITransceiver::tci_done2, this, [this]() {
      busy_other_frequency_ = false;
      if (tci_timer2_ && tci_timer2_->isActive()) tci_timer2_->stop();
    });
    connect(tci_timer2_, &QTimer::timeout, this, [this]() { busy_other_frequency_ = false; });
  }
  if (!tci_timer3_) {
    tci_timer3_ = new QTimer {this};
    tci_timer3_ -> setSingleShot(true);
    connect(this, &TCITransceiver::tci_done3, this, [this]() {
      busy_PTT_ = false;
      if (tci_timer3_ && tci_timer3_->isActive()) tci_timer3_->stop();
    });
    connect(tci_timer3_, &QTimer::timeout, this, [this]() { busy_PTT_ = false; });
  }
  if (!tci_timer4_) {
    tci_timer4_ = new QTimer {this};
    tci_timer4_ -> setSingleShot(true);
    connect(this, &TCITransceiver::tci_done4, this, [this]() {
      busy_rx2_ = false;
      if (tci_timer4_ && tci_timer4_->isActive()) tci_timer4_->stop();
    });
    connect(tci_timer4_, &QTimer::timeout, this, [this]() { busy_rx2_ = false; });
  }
  if (!tci_timer5_) {
    tci_timer5_ = new QTimer {this};
    tci_timer5_ -> setSingleShot(true);
    connect(this, &TCITransceiver::tci_done5, this, [this]() {
      busy_split_ = false;
      if (tci_timer5_ && tci_timer5_->isActive()) tci_timer5_->stop();
    });
    connect(tci_timer5_, &QTimer::timeout, this, [this]() { busy_split_ = false; });
  }
  if (!tci_timer6_) {
    tci_timer6_ = new QTimer {this};
    tci_timer6_ -> setSingleShot(true);
    connect(this, &TCITransceiver::tci_done6, this, [this]() {
      if (tci_timer6_ && tci_timer6_->isActive()) tci_timer6_->stop();
    });
  }
  if (!tci_timer7_) {
      tci_timer7_ = new QTimer {this};
      tci_timer7_ -> setSingleShot(true);
      connect(this, &TCITransceiver::tci_done7, this, [this]() {
        busy_rx_frequency_ = false;
        if (tci_timer7_ && tci_timer7_->isActive()) tci_timer7_->stop();
      });
      connect(tci_timer7_, &QTimer::timeout, this, [this]() { busy_rx_frequency_ = false; });
  }
  if (!tci_timer8_) {
      tci_timer8_ = new QTimer {this};
      tci_timer8_ -> setSingleShot(true);
      connect(this, &TCITransceiver::tci_done8, this, [this]() {
        busy_mode_ = false;
        if (tci_timer8_ && tci_timer8_->isActive()) tci_timer8_->stop();
      });
      connect(tci_timer8_, &QTimer::timeout, this, [this]() { busy_mode_ = false; });
  }

  tx_fifo = 0; tx_top_ = true;
  tci_Ready = false;
  ESDR3 = false;
  HPSDR = false;
  band_change = false;
  trxA = 0;
  trxB = 0;
  busy_rx_frequency_ = false;
  busy_mode_ = false;
  busy_other_frequency_ = false;
  busy_split_ = false;
  busy_drive_ = false;
  busy_PTT_ = false;
  busy_rx2_ = false;
  rx2_ = false;
  requested_rx2_ = false;
  started_rx2_ = false;
  split_ = false;
  requested_split_ = false;
  started_split_ = false;
  PTT_ = false;
  requested_PTT_ = false;
  mode_ = "";
  requested_mode_ = "";
  started_mode_ = "";
  requested_rx_frequency_ = "";
  rx_frequency_ = "";
  requested_other_frequency_ = "";
  other_frequency_ = "";
  requested_drive_ = "";
  drive_ = "";
  level_ = -54;
  txAtten = 45;
  power_ = 0;
  swr_ = 0;
  m_bufferPos = 0;
  m_downSampleFactor =4;
  m_ns = 999;
  audio_ = false;
  requested_stream_audio_ = false;
  stream_audio_ = false;
  _power_ = false;
  CAT_TRACE ("TCITransceiver entered TCI do_start and url " + url_.toString() + " rig_power:" + QString::number(rig_power_) + " rig_power_off:" + QString::number(rig_power_off_) + " tci_audio:" + QString::number(tci_audio_) + " do_snr:" + QString::number(do_snr_) + " do_pwr:" + QString::number(do_pwr_) + '\n');
  Q_EMIT request_socket_open (url_);
  tci_done6();
  arm_wait_timer (tci_timer6_, 2000, "startup/open");
  busy_split_ = true;
  const QString cmd = CmdSplitEnable + SmDP + "false" + SmTZ;
  sendTextMessage(cmd);
  arm_wait_timer (tci_timer5_, 500, "startup/split-off");
  if (error_.isEmpty()) {
    tci_Ready = true;
    if (!_power_) {
      if (rig_power_) {
        rig_power(true);
        arm_wait_timer (tci_timer6_, 1000, "startup/power-on");
      } else {
        tci_Ready = false;
        throw error {tr ("TCI SDR is not switched on")};
      }
    }
    if (rx_ == "1" && !rx2_) {
      rx2_enable (true);
    }
    if (tci_audio_) {
      stream_audio (true);
      arm_wait_timer (tci_timer6_, 500, "startup/audio-on");
    }
    if (ESDR3) {
      const QString cmd = CmdRxSensorsEnable + SmDP + (do_snr_ ? "true" : "false") + SmCM + "500" +  SmTZ;
      sendTextMessage(cmd);
    } else if (do_snr_) {
      const QString cmd = CmdSmeter + SmDP + rx_ + SmCM + "0" +  SmTZ;
      sendTextMessage(cmd);
    }
//    if (!requested_rx_frequency_.isEmpty()) do_frequency(string_to_frequency (requested_rx_frequency_),get_mode(true),false);
//    if (!requested_other_frequency_.isEmpty()) do_tx_frequency(string_to_frequency (requested_other_frequency_),get_mode(true),false);
//    else if (requested_split_ != split_) {rig_split();}

    do_poll ();
    if (ESDR3) {
      const QString cmd = CmdTxSensorsEnable + SmDP + (do_pwr_ ? "true" : "false") + SmCM + "500" +  SmTZ;
      sendTextMessage(cmd);
    }
    if (stream_audio_) do_audio(true);

    CAT_TRACE ("TCITransceiver started\n");
    return 0;
  } else {
    CAT_TRACE("TCITransceiver not started with error " + error_ + '\n');

    if (error_.isEmpty())
      throw error {tr ("TCI could not be opened")};
    else
      throw error {error_};
  }
}

void TCITransceiver::do_stop ()
{
  CAT_TRACE ("TCITransceiver TCI close\n");
  //printf ("TCI close\n");
  if (stream_audio_ && tci_Ready && inConnected && _power_) {
    stream_audio (false);
    arm_wait_timer (tci_timer1_, 500, "stop/audio-off");
    CAT_TRACE ("TCI audio closed\n");
    //printf ("TCI audio closed\n");
  }
  if (tci_Ready && inConnected && _power_) {
    requested_other_frequency_ = "";
    busy_split_ = true;
    const QString cmd = CmdSplitEnable + SmDP + "false" + SmTZ;
    sendTextMessage(cmd);
    arm_wait_timer (tci_timer5_, 500, "stop/split-off");
    requested_split_ = false;
    rig_split();
    if (started_mode_ != mode_) sendTextMessage(mode_to_command(started_mode_));
    if (!started_rx2_ && rx2_) {
      rx2_enable (false);
    }
  }
  if (_power_ && rig_power_off_ && tci_Ready && inConnected) {
    rig_power(false);
    arm_wait_timer (tci_timer1_, 500, "stop/power-off");
    CAT_TRACE ("TCI power down\n");
    //printf ("TCI power down\n");
  }
  tci_Ready = false;
  Q_EMIT request_socket_close ();
  shutdown_socket_worker ();
  CAT_TRACE ("closed websocket worker:");
  if (tci_timer1_)
  {
    if (tci_timer1_->isActive()) tci_timer1_->stop();
    CAT_TRACE ("timer1 ");
  }
  if (tci_timer2_)
  {
    if (tci_timer2_->isActive()) tci_timer2_->stop();
    CAT_TRACE ("timer2 ");
  }
  if (tci_timer3_)
  {
    if (tci_timer3_->isActive()) tci_timer3_->stop();
    CAT_TRACE ("timer3 ");
  }
  if (tci_timer4_)
  {
    if (tci_timer4_->isActive()) tci_timer4_->stop();
    CAT_TRACE ("timer4 ");
  }
  if (tci_timer5_)
  {
    if (tci_timer5_->isActive()) tci_timer5_->stop();
    CAT_TRACE ("timer5 ");
  }
  if (tci_timer6_)
  {
    if (tci_timer6_->isActive()) tci_timer6_->stop();
    CAT_TRACE ("timer6 ");
  }
  if (tci_timer7_)
  {
      if (tci_timer7_->isActive()) tci_timer7_->stop();
      CAT_TRACE ("timer7 ");
  }
  if (tci_timer8_)
  {
      if (tci_timer8_->isActive()) tci_timer8_->stop();
      CAT_TRACE ("timer8 ");
  }
  if (wrapped_) wrapped_->stop ();
  CAT_TRACE ("& closed TCITransceiver\n");
}

void TCITransceiver::onMessageReceived(const QString &str)
{
  qDebug() << "From WEB" << str;
  QStringList const cmd_list = tci_split_escaped (str, QChar {';'}, SkipEmptyParts);
  for (QString const& cmds : cmd_list){
    QString command_name_raw;
    QString args_raw;
    if (!tci_split_command (cmds, &command_name_raw, &args_raw))
      {
        continue;
      }

    auto const command_name = tci_unescape (command_name_raw).trimmed ();
    if (command_name.isEmpty ())
      {
        continue;
      }

    auto args = args_raw.isEmpty ()
        ? QStringList {}
        : tci_split_escaped (args_raw, QChar {','}, Qt::KeepEmptyParts);
    for (auto& item : args)
      {
        item = tci_unescape (item).trimmed ();
      }
    auto const arg = [&args] (int index) -> QString { return args.value (index).trimmed (); };
    Tci_Cmd idCmd = mapCmd_.value (command_name, Cmd_Unknown);
    if (idCmd != Cmd_Power && idCmd != Cmd_SWR && idCmd != Cmd_Smeter && idCmd != Cmd_AppFocus && idCmd != Cmd_RxSensors && idCmd != Cmd_TxSensors) { printf("%s TCI message received:|%s| ",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),str.toStdString().c_str()); printf("idCmd : %d args : %s\n",idCmd,args.join("|").toStdString().c_str());}
    qDebug() << cmds << idCmd;
    if (idCmd <=0) continue;

    switch (idCmd) {
      case Cmd_Smeter:
        if(arg (0) == rx_ && arg (1) == "0") level_ = arg (2).toInt() + 73;
        break;
      case Cmd_RxSensors:
        if(arg (0) == rx_) level_ = arg (1).split ('.', Qt::KeepEmptyParts).value (0).toInt () + 73;
        printf("Smeter=%d\n",level_);
        break;
      case Cmd_TxSensors:
        if(arg (0) == rx_) {
          int parsed = 0;
          if (parse_decimal_tenths (arg (3), &parsed))
            {
              power_ = parsed;
            }
          if (parse_decimal_tenths (arg (4), &parsed))
            {
              swr_ = parsed;
            }
          printf("Power=%d SWR=%d\n",power_,swr_);
        }
        break;
      case Cmd_SWR:
        printf("%s Cmd_SWR : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        {
          int parsed = 0;
          if (parse_decimal_tenths (arg (0), &parsed))
            {
              swr_ = parsed;
            }
        }
        break;
      case Cmd_Power:
        {
          int parsed = 0;
          if (parse_decimal_tenths (arg (0), &parsed))
            {
              power_ = parsed;
            }
        }
        printf("%s Cmd_Power : %s %d\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str(),power_);
        break;
      case Cmd_VFO:
        printf("%s Cmd_VFO : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        printf("band_change:%d busy_other_frequency_:%d timer1_remaining:%d timer2_remaining:%d",band_change,busy_other_frequency_,tci_timer7_ ? tci_timer7_->remainingTime() : -1,tci_timer2_ ? tci_timer2_->remainingTime() : -1); //was timer1 and timer2
        if(arg (0) == rx_ && arg (1) == "0") {
          if (arg (2).left(1) != "-") rx_frequency_ = arg (2);
          CAT_TRACE("Rx VFO Frequency from SDR is :");
          CAT_TRACE(rx_frequency_);
          if (!tci_Ready && requested_rx_frequency_.isEmpty()) requested_rx_frequency_ = rx_frequency_;
          if (busy_rx_frequency_ && !band_change) {
            printf (" cmdvfo0 done1");
            tci_done7(); //was tci_done1 (do_frequency)
          } else if (tci_timer2_ && !tci_timer2_->isActive() && split_) {
            printf (" cmdvfo0 timer2 start 210");
            tci_timer2_->start(210);
          }
        }
        else if (arg (0) == rx_ && arg (1) == "1") {
          if (arg (2).left(1) != "-") other_frequency_ = arg (2);
          CAT_TRACE("Tx VFO (other) Frequency from SDR is :");
          CAT_TRACE(other_frequency_);
          if (!tci_Ready && requested_other_frequency_.isEmpty()) requested_other_frequency_ = other_frequency_;
          if (band_change && tci_timer7_ && tci_timer7_->isActive()) { //was tci_timer1
            printf (" cmdvfo1 done1");
            band_change = false;
            if (tci_timer2_) tci_timer2_->start(210);
            tci_done7(); //was tci_done1 (do_frequency)
          } else if (busy_other_frequency_) {
            printf (" cmdvfo1 done2");
            tci_done2();
          } else if (tci_timer2_ && tci_timer2_->isActive()) {
            printf (" cmdvfo1 timer2 reset 210");
            tci_timer2_->start(210);
          } else if (tci_timer2_ && other_frequency_ != requested_other_frequency_ && tci_Ready && split_ && !tci_timer2_->isActive()) {
            printf (" cmdvfo1 timer2 start 210");
            tci_timer2_->start(210);
          }
        }
        break;
      case Cmd_Mode:
        printf("%s Cmd_Mode : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == rx_) {
          if (ESDR3 || HPSDR) {
            if (arg (1) == "0" ) mode_ = arg (2).toLower(); else mode_ = arg (1).toLower();
          } else mode_ = arg (1);
          if (started_mode_.isEmpty()) started_mode_ = mode_;
          if (busy_mode_ && requested_mode_.compare (mode_, Qt::CaseInsensitive) == 0) {
            tci_done8();
            update_mode (get_mode ());
          }
          else if (!busy_mode_ && !requested_mode_.isEmpty() && requested_mode_ != mode_ && !band_change) {
            sendTextMessage(mode_to_command(requested_mode_));
          }
        }
        break;
      case Cmd_SplitEnable:
        printf("%s Cmd_SplitEnable : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == rx_) {
          if (arg (1) == "false") split_ = false;
          else if (arg (1) == "true") split_ = true;
          if (!tci_Ready) {started_split_ = split_;}
          else if (busy_split_) tci_done5();  //was tci_done2
          else if (tci_timer5_ && requested_split_ != split_ && !tci_timer5_->isActive()) { //was tci_timer2
              CAT_TRACE("tci_timer5 started in onMessageReceived-Cmd_SplitEnable");
            tci_timer5_->start(210);  //was tci_timer2
            rig_split();
          }
        }
        break;
      case Cmd_Drive:
        printf("%s Cmd_Drive : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if((!ESDR3 && !HPSDR) || arg (0) == rx_) {
          if (ESDR3 || HPSDR) drive_ = arg (1); else drive_ = arg (0);
          if (requested_drive_.isEmpty()) requested_drive_ = drive_;
          busy_drive_ = false;
        }
        break;
      case Cmd_Volume:
        break;
      case Cmd_Trx:
        printf("%s Cmd_Trx : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == rx_) {
          if (arg (1) == "false") PTT_ = false;
          else if (arg (1) == "true") PTT_ = true;
          if (tci_Ready && requested_PTT_ == PTT_) tci_done3();
          else if (tci_Ready && !PTT_) {
            requested_PTT_ = PTT_;
            update_PTT(PTT_);
            power_ = 0; if (do_pwr_) update_power (0);
            swr_ = 0; if (do_pwr_) update_swr (0);
          }
        }
        break;
      case Cmd_AudioStart:
        printf("%s Cmd_AudioStart : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == rx_) {
          stream_audio_ = true;
          if (tci_Ready) {
            printf ("cmdaudiostart done1\n");
            tci_done7(); //was tci_done1 (do_frequency)
          }
        }
        break;
      case Cmd_RxEnable:
        printf("%s Cmd_RxEnable : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == "1") {
          if (arg (1) == "false") rx2_ = false;
          else if (arg (1) == "true") rx2_ = true;
          if(!tci_Ready) {requested_rx2_ = rx2_; started_rx2_ = rx2_;}
          else if (tci_Ready && busy_rx2_ && requested_rx2_ == rx2_) {
            tci_done4();
          }
        }
        break;
      case Cmd_AudioStop:
        printf("%s CmdAudioStop : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == rx_) {
          stream_audio_ = false;
          if (tci_Ready) {
            printf ("cmdaudiostop done1\n");
            tci_done7(); //was tci_done1 (do_frequency)
          }
        }
        break;
      case Cmd_Start:
        printf("%s CmdStart : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        _power_ = true;
        printf ("cmdstart done1\n");
        if (tci_Ready) {
          tci_done7(); //was tci_done1 (do_frequency)
        }
        break;
      case Cmd_Stop:
        printf("%s CmdStop : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if (tci_Ready && PTT_) {
          PTT_ = false;
          requested_PTT_ = PTT_;
          update_PTT(PTT_);
          power_ = 0; if (do_pwr_) update_power (0);
          swr_ = 0; if (do_pwr_) update_swr (0);
          m_state = Idle;
          Q_EMIT tci_mod_active(m_state != Idle);
        }
        _power_ = false;
        if (tci_timer1_ && tci_timer1_->isActive()) {  //was tci_timer1
          printf ("cmdstop done1\n");
          tci_done1();
        } else {
          tci_Ready = false;
        }
        break;
      case Cmd_Version:
        printf("%s CmdVersion : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if(arg (0) == "ExpertSDR3") ESDR3 = true;
        else if (arg (0) == "Thetis") HPSDR = true;
        break;
      case Cmd_Device:
        printf("%s CmdDevice : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        if((arg (0) == "SunSDR2DX" || arg (0) == "SunSDR2PRO") && !ESDR3) tx_top_ = false;
        printf ("tx_top_:%d\n",tx_top_);
        break;
      case Cmd_Ready:
        printf("%s CmdReady : %s\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),args.join("|").toStdString().c_str());
        tci_done7(); //was tci_done1 (do_frequency)
        break;

      default:
        break;
    }
  }
}

void TCITransceiver::sendTextMessage(const QString &message)
{
  if (inConnected && socket_worker_) {
    Q_EMIT request_socket_send_text (message);
    CAT_TRACE ("TCI Command Sent: " + message + '\n');
  }
}

void TCITransceiver::onBinaryReceived(const QByteArray &data)
{
  if (data.size () < static_cast<int> (AudioHeaderSize))
    {
      qWarning () << "TCI: dropping truncated binary frame (no header), size=" << data.size ();
      return;
    }

  auto const * pStream = reinterpret_cast<Data_Stream const *> (data.constData ());
  auto const payload_bytes = data.size () - static_cast<int> (AudioHeaderSize);
  quint64 required_bytes = 0;
  if (!checked_payload_size (pStream->length, payload_bytes, &required_bytes))
    {
      qWarning () << "TCI: dropping malformed binary frame, type=" << pStream->type
                  << "length=" << pStream->length
                  << "payload=" << payload_bytes;
      return;
    }

  if (pStream->type != last_type)
    {
      last_type = pStream->type;
    }

  if (pStream->type == Iq_Stream)
    {
      bool tx = false;
      if (pStream->receiver == 0)
        {
          tx = trxA == 0;
          trxA = 1;
        }
      if (pStream->receiver == 1)
        {
          tx = trxB == 0;
          trxB = 1;
        }
      printf ("sendIqData\n");
      emit sendIqData (pStream->receiver, pStream->length, const_cast<float *> (pStream->data), tx);
      qDebug () << "IQ" << data.size () << pStream->length;
    }
  else if (pStream->type == RxAudioStream && audio_ && pStream->receiver == rx_.toUInt ())
    {
      writeAudioData (const_cast<float *> (pStream->data), static_cast<qint32> (pStream->length));
    }
  else if (pStream->type == TxChrono && pStream->receiver == rx_.toUInt ())
    {
      quint64 const ssize64 = static_cast<quint64> (AudioHeaderSize) + required_bytes;
      if (ssize64 > static_cast<quint64> (std::numeric_limits<int>::max ()))
        {
          qWarning () << "TCI: dropping oversized TxChrono frame, bytes=" << ssize64;
          return;
        }
      std::lock_guard<std::mutex> lock (mtx_);
      tx_fifo = (tx_fifo + 1) & 7;
      int const ssize = static_cast<int> (ssize64);
      if (m_tx1[tx_fifo].size () != ssize)
        {
          m_tx1[tx_fifo].resize (ssize);
        }
      auto * pOStream1 = reinterpret_cast<Data_Stream *> (m_tx1[tx_fifo].data ());
      pOStream1->receiver = pStream->receiver;
      pOStream1->sampleRate = pStream->sampleRate;
      pOStream1->format = pStream->format;
      pOStream1->codec = 0;
      pOStream1->crc = 0;
      pOStream1->length = pStream->length;
      pOStream1->type = TxAudioStream;
      quint16 done = readAudioData (pOStream1->data, static_cast<qint32> (pOStream1->length), txAtten);
      if (done && done != pOStream1->length)
        {
          quint16 const ready = done;
          readAudioData (pOStream1->data + ready, static_cast<qint32> (pOStream1->length - ready), txAtten);
        }
      tx_fifo2 = tx_fifo;
      if (inConnected && socket_worker_)
        {
          Q_EMIT request_socket_send_binary (m_tx1[tx_fifo2]);
        }
    }
}

void TCITransceiver::txAudioData(quint32 len, float * data)
{
  QByteArray tx;
  tx.resize(AudioHeaderSize+len*sizeof(float)*2);
  Data_Stream * pStream = (Data_Stream*)(tx.data());
  pStream->receiver = 0;
  pStream->sampleRate = audioSampleRate;
  pStream->format = 3;
  pStream->codec = 0;
  pStream->crc = 0;
  pStream->length = len;
  pStream->type = TxAudioStream;
  memcpy(pStream->data,data,len*sizeof(float)*2);
  if (inConnected && socket_worker_)
    {
      Q_EMIT request_socket_send_binary (tx);
    }
}

quint32 TCITransceiver::writeAudioData (float * data, qint32 maxSize)
{
  static unsigned mstr0=999999;
  qint64 ms0 = QDateTime::currentMSecsSinceEpoch() % 86400000;
  unsigned mstr = ms0 % int(1000.0*m_period); // ms into the nominal Tx start time
  if(mstr < mstr0/2) {              //When mstr has wrapped around to 0, restart the buffer
    dec_data.params.kin = 0;
    m_bufferPos = 0;
  }
  mstr0=mstr;

  if (data && maxSize > 0)
    {
      if (dec_data.params.kin < 0 || dec_data.params.kin > kMaxKin)
        {
          qWarning () << "TCI: clamping out-of-range kin:" << dec_data.params.kin;
          dec_data.params.kin = qBound (0, dec_data.params.kin, kMaxKin);
        }
      Q_ASSERT (dec_data.params.kin >= 0);

      // no torn frames
      Q_ASSERT (!(maxSize % static_cast<qint32> (bytesPerFrame)));

      // block below adjusts receive audio attenuation
      if (rx_scaled_buffer_.size () < maxSize)
        {
          rx_scaled_buffer_.resize (maxSize);
        }
      float const gain = static_cast<float> (pow (10, 0.05 * rxAtten));
      for (qint32 i = 0; i < maxSize; ++i)
        {
          rx_scaled_buffer_[static_cast<int> (i)] = gain * data[i];
        }

      int const boundedKin0 = qBound (0, dec_data.params.kin, kMaxKin);
      size_t const framesAcceptable ((static_cast<size_t> (kMaxKin - boundedKin0)) * m_downSampleFactor);
      size_t const framesInput = static_cast<size_t> (maxSize / bytesPerFrame);
      size_t const framesAccepted = qMin (framesInput, framesAcceptable);

      if (framesAccepted < framesInput)
        {
          qDebug () << "dropped " << framesInput - framesAccepted
                    << " frames of data on the floor!"
                    << dec_data.params.kin << mstr;
        }

      for (size_t remaining = framesAccepted; remaining; )
        {
          size_t const numFramesProcessed (qMin (static_cast<size_t> (m_samplesPerFFT) *
                                                 m_downSampleFactor - m_bufferPos, remaining));

          if (m_downSampleFactor > 1)
            {
              store (&rx_scaled_buffer_[static_cast<int> ((framesAccepted - remaining) * bytesPerFrame)],
                     numFramesProcessed, &m_buffer[m_bufferPos]);
              m_bufferPos += numFramesProcessed;

              if (m_bufferPos == static_cast<unsigned> (m_samplesPerFFT * m_downSampleFactor))
                {
                  qint32 framesToProcess (m_samplesPerFFT * m_downSampleFactor);
                  qint32 framesAfterDownSample (m_samplesPerFFT);
                  int const boundedKin = qBound (0, dec_data.params.kin, kMaxKin);
                  if (boundedKin <= (kMaxKin - framesAfterDownSample))
                    {
                      fil4_ (&m_buffer[0], &framesToProcess, &dec_data.d2[boundedKin],
                             &framesAfterDownSample, &dec_data.d2[boundedKin]);
                      dec_data.params.kin = boundedKin + framesAfterDownSample;
                    }
                  else
                    {
                      qWarning () << "TCI: dropping downsample block due to kin bounds"
                                  << boundedKin << framesAfterDownSample;
                    }
                  Q_EMIT tciframeswritten (dec_data.params.kin);
                  m_bufferPos = 0;
                }
              remaining -= numFramesProcessed;
            }
          else
            {
              int const boundedKin = qBound (0, dec_data.params.kin, kMaxKin);
              size_t const writableFrames = qMin (numFramesProcessed, static_cast<size_t> (kMaxKin - boundedKin));
              if (writableFrames == 0)
                {
                  qWarning () << "TCI: no writable frames left in dec_data.d2";
                  break;
                }
              store (&rx_scaled_buffer_[static_cast<int> ((framesAccepted - remaining) * bytesPerFrame)],
                     writableFrames, &dec_data.d2[boundedKin]);
              m_bufferPos += writableFrames;
              dec_data.params.kin = boundedKin + static_cast<int> (writableFrames);
              if (writableFrames < numFramesProcessed)
                {
                  qWarning () << "TCI: truncated write to dec_data.d2 due to bounds";
                }
              if (m_bufferPos >= static_cast<unsigned> (m_samplesPerFFT))
                {
                  Q_EMIT tciframeswritten (dec_data.params.kin);
                  m_bufferPos = 0;
                }
              remaining -= writableFrames;
            }
        }
    }
  return maxSize;    // we drop any data past the end of the buffer on
  // the floor until the next period starts
}

void TCITransceiver::rx2_enable (bool on)
{
  printf("%s TCI rx2_enable:%d->%d busy:%d\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),rx2_,on,busy_rx2_);
  if (busy_rx2_) return;
  requested_rx2_ = on;
  busy_rx2_ = true;
  const QString cmd = CmdRxEnable + SmDP + "1" + SmCM + (requested_rx2_ ? "true" : "false") + SmTZ;
  sendTextMessage(cmd);
  arm_wait_timer (tci_timer4_, 1000, "rx2_enable");
}

void TCITransceiver::rig_split ()
{
  printf("%s TCI rig_split:%d->%d busy:%d\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),split_,requested_split_,busy_split_);
  if (busy_split_) return;
  if (tci_timer5_ && tci_timer5_->isActive()) return;
  busy_split_ = true;
  const QString cmd = CmdSplitEnable + SmDP + rx_ + SmCM +  "false" + SmTZ;  // changed from below so split always false
  //const QString cmd = CmdSplitEnable + SmDP + rx_ + SmCM + (requested_split_ ? "true" : "false") + SmTZ;
  sendTextMessage(cmd);
  arm_wait_timer (tci_timer5_, 500, "rig_split");
  if (requested_split_ == split_) update_split (split_);
}

void TCITransceiver::rig_power (bool on)
{
  TRACE_CAT ("TCITransceiver", on << state ());
  printf("%s TCI rig_power:%d _power_:%d\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),on,_power_);
  if (on != _power_) {
    if (on) {
      const QString cmd = CmdStart + SmTZ;
      sendTextMessage(cmd);
    } else {
      const QString cmd = CmdStop + SmTZ;
      sendTextMessage(cmd);
    }
  }
}

void TCITransceiver::stream_audio (bool on)
{
  TRACE_CAT ("TCITransceiver", on << state ());
  if (on != stream_audio_ && tci_Ready) {
    requested_stream_audio_ = on;
    if (on) {
      const QString cmd = CmdAudioStart + SmDP + rx_ + SmTZ;
      sendTextMessage(cmd);
    } else {
      const QString cmd = CmdAudioStop + SmDP + rx_ + SmTZ;
      sendTextMessage(cmd);
    }
  }
}

void TCITransceiver::do_audio (bool on)
{
  TRACE_CAT ("TCITransceiver", on << state ());
  if (on) {
    dec_data.params.kin = 0;
    m_bufferPos = 0;
  }
  audio_ = on;
}

void TCITransceiver::do_period (double period)
{
  TRACE_CAT ("TCITransceiver", period << state ());
  m_period = period;
}

void TCITransceiver::do_volume (qreal volume)
{
  rxAtten = volume;
}


void TCITransceiver::do_txvolume (qreal txvolume)
{
  txAtten = txvolume;
}

void TCITransceiver::do_blocksize (qint32 blocksize)
{
  TRACE_CAT ("TCITransceiver", blocksize << state ());
  m_samplesPerFFT = blocksize;
}

void TCITransceiver::do_ptt (bool on)
{
  TRACE_CAT ("TCITransceiver", on << state ());
  if (use_for_ptt_) {
    if (on != PTT_) {
      if (!inConnected && !error_.isEmpty()) throw error {error_};
      else if (busy_PTT_ || !tci_Ready || !_power_) return;
      else busy_PTT_ = true;
      requested_PTT_ = on;
      if (ESDR3) {
        const QString cmd = CmdTrx + SmDP + rx_ + SmCM + (on ? "true" : "false") + SmCM + "tci"+ SmTZ;
        sendTextMessage(cmd);
      } else {
        const QString cmd = CmdTrx + SmDP + rx_ + SmCM + (on ? "true" : "false") + SmTZ;
        sendTextMessage(cmd);
      }
      arm_wait_timer (tci_timer3_, 1000, "do_ptt");
    } else update_PTT(on);
  }
  else
  {
    TRACE_CAT ("TCITransceiver", "TCI should use PTT via CAT");
    throw error {tr ("TCI should use PTT via CAT")};
  }
}

void TCITransceiver::do_frequency (Frequency f, MODE m, bool no_ignore)
{
  qDebug() << no_ignore;
  TRACE_CAT ("TCITransceiver", f << state ());
  auto f_string = frequency_to_string (f);
  if (busy_rx_frequency_) {
    return;
  } else {
    requested_rx_frequency_ = f_string;
    requested_mode_ = map_mode (m);
  }
  if (tci_Ready && _power_) {
    if ((rx_frequency_ != requested_rx_frequency_)) {
      busy_rx_frequency_ = true;
      band_change = abs(rx_frequency_.toInt()-requested_rx_frequency_.toInt()) > 1000000;
      const QString cmd = CmdVFO + SmDP + rx_ + SmCM + "0" + SmCM + requested_rx_frequency_ + SmTZ;
      if(f > 100000 && f< 250000000000) sendTextMessage(cmd);
      arm_wait_timer (tci_timer7_, 2000, "do_frequency/vfo");
    } else update_rx_frequency (string_to_frequency (rx_frequency_));

    if (!requested_mode_.isEmpty() && requested_mode_ != mode_ && !busy_mode_) {
      busy_mode_ = true;
      sendTextMessage(mode_to_command(requested_mode_));
      arm_wait_timer (tci_timer8_, 1000, "do_frequency/mode");
    }
  } else {
    update_rx_frequency (f);
    update_mode (m);
  }
}

void TCITransceiver::do_tx_frequency (Frequency tx, MODE mode, bool no_ignore)
{
  TRACE_CAT ("%s TCITransceiver", tx << state ());
  auto f_string = frequency_to_string (tx);
  if (tci_Ready && busy_other_frequency_ && no_ignore) {
      CAT_TRACE("TCI do_txfrequency critical no_ignore set tx vfo will be missed\n");
    printf("%s TCI do_txfrequency critical no_ignore set tx vfo will be missed\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str());
  }
  if (busy_other_frequency_) return;
  requested_other_frequency_ = f_string;
  requested_mode_ = map_mode (mode);
  if (tx) {
    requested_split_ = true;
    if (tci_Ready && _power_) {
      if (requested_split_ != split_) rig_split();
      else update_split (split_);
      if (other_frequency_ != requested_other_frequency_) {
        busy_other_frequency_ = true;
        printf("%s TCI VFO1 command sent\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str());
        const QString cmd = CmdVFO + SmDP + rx_ + SmCM + "1" + SmCM + requested_other_frequency_ + SmTZ;
        if(tx > 100000 && tx < 250000000000) sendTextMessage(cmd);
        arm_wait_timer (tci_timer2_, 1000, "do_tx_frequency/vfo1");
      } else update_other_frequency (string_to_frequency (other_frequency_));

    } else {
      update_split (requested_split_);
      update_other_frequency (tx);
    }
  }
  else {
    requested_split_ = false;
    requested_other_frequency_ = "";
    if (tci_Ready && _power_) {
      if (requested_split_ != split_) rig_split();
      else update_split (split_);
      update_other_frequency (tx);
    } else {
      update_split (requested_split_);
      update_other_frequency (tx);
    }
  }
}

//do_mode is never called.
void TCITransceiver::do_mode (MODE m)
{
  TRACE_CAT ("TCITransceiver", m << state ());
  auto m_string = map_mode (m);
  if (requested_mode_ != m_string) requested_mode_ = m_string;
  if (!requested_mode_.isEmpty() && mode_ != requested_mode_ && !busy_mode_) {
    busy_mode_ = true;
    sendTextMessage(mode_to_command(requested_mode_));
    arm_wait_timer (tci_timer8_, 1000, "do_mode");
  } else if (requested_mode_ == mode_) {
    update_mode (m);
  }
}

void TCITransceiver::do_poll ()
{
  if (!error_.isEmpty()) {tci_Ready = false; throw error {error_};}
  if (!tci_Ready)
    {
      // Non-blocking startup: wait for async websocket/radio handshake.
      if (inConnected && _power_)
        {
          tci_Ready = true;
        }
      else
        {
          return;
        }
    }
  update_rx_frequency (string_to_frequency (rx_frequency_));
  update_split(split_);
  if (state ().split ()) {
    if (other_frequency_ == requested_other_frequency_) update_other_frequency (string_to_frequency (other_frequency_));
    else if (!busy_rx_frequency_) do_tx_frequency(string_to_frequency (requested_other_frequency_),get_mode(true),false);
  }
  update_mode (get_mode());
  if (do_pwr_ && PTT_) {update_power (power_ * 100); update_swr (swr_*10);}
  if (do_snr_ && !PTT_) {
    update_level (level_);
    if(!ESDR3) {
      const QString cmd = CmdSmeter + SmDP + rx_ + SmCM + "0" +  SmTZ;
      sendTextMessage(cmd);
    }
  }
}

auto TCITransceiver::get_mode (bool requested) -> MODE
{
  MODE m {UNK};
  if (requested) {
    if ("am" == requested_mode_)
    {
      m = AM;
    }
    else if ("cw" == requested_mode_)
    {
      m = CW;
    }
    else if ("wfm" == requested_mode_)
    {
      m = FM;
    }
    else if ("nfm" == requested_mode_)
    {
      m = DIG_FM;
    }
    else if ("lsb" == requested_mode_)
    {
      m = LSB;
    }
    else if ("usb" == requested_mode_)
    {
      m = USB;
    }
    else if ("fsk" == requested_mode_ || "rtty" == requested_mode_)
    {
      m = FSK;
    }
    else if ("fsk-r" == requested_mode_ || "fskr" == requested_mode_
             || "rtty-r" == requested_mode_ || "rttyr" == requested_mode_)
    {
      m = FSK_R;
    }
    else if ("digl" == requested_mode_)
    {
      m = DIG_L;
    }
    else if ("digu" == requested_mode_)
    {
      m = DIG_U;
    }
  } else {
    if ("am" == mode_)
    {
      m = AM;
    }
    else if ("cw" == mode_)
    {
      m = CW;
    }
    else if ("wfm" == mode_)
    {
      m = FM;
    }
    else if ("nfm" == mode_)
    {
      m = DIG_FM;
    }
    else if ("lsb" == mode_)
    {
      m = LSB;
    }
    else if ("usb" == mode_)
    {
      m = USB;
    }
    else if ("fsk" == mode_ || "rtty" == mode_)
    {
      m = FSK;
    }
    else if ("fsk-r" == mode_ || "fskr" == mode_
             || "rtty-r" == mode_ || "rttyr" == mode_)
    {
      m = FSK_R;
    }
    else if ("digl" == mode_)
    {
      m = DIG_L;
    }
    else if ("digu" == mode_)
    {
      m = DIG_U;
    }
  }
  return m;
}

QString TCITransceiver::mode_to_command (QString m_string) const
{
  //    if (ESDR3) {
  //      const QString cmd = CmdMode + SmDP + rx_ + SmCM + m_string.toUpper() + SmTZ;
  //      return cmd;
  //    } else {
  const QString cmd = CmdMode + SmDP + rx_ + SmCM + m_string + SmTZ;
  return cmd;
  //    }
}

QString TCITransceiver::frequency_to_string (Frequency f) const
{
  // number is localized and in kHz, avoid floating point translation
  // errors by adding a small number (0.1Hz)
  auto f_string = QString {}.setNum(f);
  printf ("frequency_to_string3 |%s|\n",f_string.toStdString().c_str());
  return f_string;
}

auto TCITransceiver::string_to_frequency (QString s) const -> Frequency
{
  // temporary hack because Commander is returning invalid UTF-8 bytes
  s.replace (QChar {QChar::ReplacementCharacter}, locale_.groupSeparator ());

  bool ok;

  auto f = QLocale::c ().toDouble (s, &ok); // temporary fix

  if (!ok)
  {
    printf("Frequency rejected is ***%s***\n",s.toStdString().c_str());
    // throw error {tr ("TCI sent an unrecognized frequency") + " |" + s + "|"};
  }
  return f;
}

void TCITransceiver::arm_wait_timer (QTimer * timer, int ms, char const * context)
{
  if (!timer || ms <= 0)
    {
      return;
    }
  timer->start (ms);
  CAT_TRACE (QString {"TCI async wait armed (%1 ms) for %2"}.arg (ms).arg (context));
}
// Modulator part

void TCITransceiver::do_modulator_start (QString mode, unsigned symbolsLength, double framesPerSymbol,
                                        double frequency, double toneSpacing, bool synchronize, bool fastMode, double dBSNR, double TRperiod)
{
  // Time according to this computer which becomes our base time
  qint64 ms0 = QDateTime::currentMSecsSinceEpoch() % 86400000;
  qDebug() << "ModStart" << QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.sss");
  unsigned mstr = ms0 % int(1000.0*m_period); // ms into the nominal Tx start time
  if (m_state != Idle) {
    //    stop ();
    throw error {tr ("TCI modulator not Idle")};
  }
  m_quickClose = false;
  m_symbolsLength = symbolsLength;
  m_isym0 = std::numeric_limits<unsigned>::max (); // big number
  m_frequency0 = 0.;
  m_phi = 0.;
  m_addNoise = dBSNR < 0.;
  m_nsps = framesPerSymbol;
  m_trfrequency = frequency;
  m_amp = std::numeric_limits<qint16>::max ();
  m_toneSpacing = toneSpacing;
  m_bFastMode=fastMode;
  m_TRperiod=TRperiod;
  m_waveSnapshot.clear ();
  if (!m_tuning && m_toneSpacing < 0.0)
    {
      qint64 requiredSamples = static_cast<qint64> (std::ceil (symbolsLength * 4.0 * framesPerSymbol)) + 2;
      if (m_bFastMode && m_TRperiod > 0.0)
        {
          requiredSamples = qMax (requiredSamples, static_cast<qint64> (std::ceil (m_TRperiod * 48000.0 - 24000.0)) + 2);
        }
      int const kFoxWaveSampleCount = FOXCOM_WAVE_SIZE;
      int const copySamples = static_cast<int> (qBound<qint64> (qint64(1), requiredSamples, qint64(kFoxWaveSampleCount)));
      m_waveSnapshot.resize (copySamples);
      QReadLocker wave_lock {&fox_wave_lock ()};
      std::memcpy (m_waveSnapshot.data (), foxcom_.wave, static_cast<size_t> (copySamples) * sizeof (float));
    }
  unsigned delay_ms=1000;

  if((mode=="FT8" and m_nsps==1920) or (mode=="FST4" and m_nsps==720)) delay_ms=500;  //FT8, FST4-15
  if((mode=="FT8" and m_nsps==1024)) delay_ms=400;            //SuperFox Qary Polar Code transmission
  if(mode=="Q65" and m_nsps<=3600) delay_ms=500;              //Q65-15 and Q65-30
  if(mode=="FT4") delay_ms=300;                               //FT4
  if(mode=="FT2") delay_ms=100;                               //FT2 async: minimal delay

  // noise generator parameters
  if (m_addNoise) {
    m_snr = qPow (10.0, 0.05 * (dBSNR - 6.0));
    m_fac = 3000.0;
    if (m_snr > 1.0) m_fac = 3000.0 / m_snr;
  }

  // round up to an exact portion of a second that allows for startup delays
  m_ic = 0;
  m_silentFrames = 0;
  if (synchronize && !m_tuning) {
    auto mstr2 = mstr - delay_ms;
    if (mstr <= delay_ms) {
      m_ic = 0;
      // calculate number of silent frames to send
      m_silentFrames = audioSampleRate / (1000 / delay_ms) - (mstr * (audioSampleRate / 1000));
    } else {
      m_ic = mstr2 * (audioSampleRate / 1000);
    }
  }
  m_state = (synchronize && m_silentFrames) ?
                Synchronizing : Active;
  printf("%s TCI modulator startdelay_ms=%d ASR=%d mstr=%d m_ic=%d s_Frames=%lld synchronize=%d m_tuning=%d State=%d\n",QDateTime::QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz").toStdString().c_str(),delay_ms,audioSampleRate,mstr,m_ic,m_silentFrames,synchronize,m_tuning,m_state);
  Q_EMIT tci_mod_active(m_state != Idle);
}

void TCITransceiver::do_tune (bool newState)
{
  m_tuning = newState;
  if (!m_tuning) do_modulator_stop (true);
}

void TCITransceiver::do_modulator_stop (bool quick)
{
  m_quickClose = quick;
  m_waveSnapshot.clear ();
  if(m_state != Idle) {
    m_state = Idle;
    Q_EMIT tci_mod_active(m_state != Idle);
  }
  tx_audio_ = false;
}

quint16 TCITransceiver::readAudioData (float * data, qint32 maxSize, qreal txAtten)
{
  double toneFrequency=1500.0;
  if(m_nsps==6) {
    toneFrequency=1000.0;
    m_trfrequency=1000.0;
    m_frequency0=1000.0;
  }
  if(maxSize==0) return 0;

  qreal newVolume = pow(2.22222 * (45 - txAtten) * 0.01,2);
  //printf("txAtten is %f and newVolume is %f\n",txAtten, newVolume);

  qint64 numFrames (maxSize/bytesPerFrame);
  float * samples (reinterpret_cast<float *> (data));
  float * end (samples + numFrames * bytesPerFrame);
  qint64 framesGenerated (0);

  switch (m_state)
  {
    case Synchronizing:
    {
      if (m_silentFrames)	{  // send silence up to first second
        framesGenerated = qMin (m_silentFrames, numFrames);
        for ( ; samples != end; samples = load (0, samples)) { // silence
        }
        m_silentFrames -= framesGenerated;
        return framesGenerated * bytesPerFrame;
      }
      m_state = Active;
      Q_EMIT tci_mod_active(m_state != Idle);
      m_cwLevel = false;
      m_ramp = 0;		// prepare for CW wave shaping
    }
      // fall through

    case Active:
    {
      unsigned int isym=0;
      qint16 sample=0;
      if(!m_tuning) isym=m_ic/(4.0*m_nsps);          // Actual fsample=48000
      bool slowCwId=((isym >= m_symbolsLength) && (icw[0] > 0));
      m_nspd=2560;                 // 22.5 WPM

      if(m_TRperiod > 16.0 && slowCwId) {     // Transmit CW ID?
        m_dphi = m_twoPi*m_trfrequency/audioSampleRate;
        unsigned ic0 = m_symbolsLength * 4 * m_nsps;
        unsigned j(0);

        while (samples != end) {
          j = (m_ic - ic0)/m_nspd + 1; // symbol of this sample
          bool level {bool (icw[j])};
          m_phi += m_dphi;
          if (m_phi > m_twoPi) m_phi -= m_twoPi;
          sample=0;
          float amp=32767.0;
          float x=0.0;
          if(m_ramp!=0) {
            x=qSin(float(m_phi));
            if(SOFT_KEYING) {
              amp=qAbs(qint32(m_ramp));
              if(amp>32767.0) amp=32767.0;
            }
            sample=round(newVolume * amp * x);
          }
          if (int (j) <= icw[0] && j < NUM_CW_SYMBOLS) { // stopu condition
            samples = load (postProcessSample (sample), samples);
            ++framesGenerated;
            ++m_ic;
          } else {
            m_state = Idle;
            Q_EMIT tci_mod_active(m_state != Idle);
            return framesGenerated * bytesPerFrame;
          }

          // adjust ramp
          if ((m_ramp != 0 && m_ramp != std::numeric_limits<qint16>::min ()) || level != m_cwLevel) {
            // either ramp has terminated at max/min or direction has changed
            m_ramp += RAMP_INCREMENT; // ramp
          }
          m_cwLevel = level;
        }
        return framesGenerated * bytesPerFrame;
      } //End of code for CW ID

      double const baud (12000.0 / m_nsps);
      // fade out parameters (no fade out for tuning)
      unsigned int i0,i1;
      if(m_tuning) {
        i1 = i0 =  (m_bFastMode ? 999999 : 9999) * m_nsps;
      } else {
        i0=(m_symbolsLength - 0.017) * 4.0 * m_nsps;
        i1= m_symbolsLength * 4.0 * m_nsps;
        // Precomputed wave (FT2/Fox): include ramp-up/down symbols (+2)
        if (m_toneSpacing < 0) {
          i1 = (m_symbolsLength + 2) * 4.0 * m_nsps;
          i0 = i1;  // wave has built-in ramp, disable internal fade-out
        }
      }
      if(m_bFastMode and !m_tuning) {
        i1=m_TRperiod*48000.0 - 24000.0;
        i0=i1-816;
      }

      sample=0;
      for (unsigned i = 0; i < numFrames && m_ic <= i1; ++i) {
        isym=0;
        if(!m_tuning and m_TRperiod!=3.0) isym=m_ic/(4.0*m_nsps);   //Actual fsample=48000
        if(m_bFastMode) isym=isym%m_symbolsLength;
        if (isym != m_isym0 || m_trfrequency != m_frequency0) {
          if(itone[0]>=100) {
            m_toneFrequency0=itone[0];
          } else {
            if(m_toneSpacing==0.0) {
              m_toneFrequency0=m_trfrequency + itone[isym]*baud;
            } else {
              m_toneFrequency0=m_trfrequency + itone[isym]*m_toneSpacing;
            }
          }
          m_dphi = m_twoPi * m_toneFrequency0 / audioSampleRate;
          m_isym0 = isym;
          m_frequency0 = m_trfrequency;         //???
        }

        int j=m_ic/480;
        if(m_fSpread>0.0 and j!=m_j0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
          float x1=QRandomGenerator::global ()->generateDouble ();
          float x2=QRandomGenerator::global ()->generateDouble ();
#else
          float x1=static_cast<float>(std::rand())/static_cast<float>(RAND_MAX);
          float x2=static_cast<float>(std::rand())/static_cast<float>(RAND_MAX);
#endif
          toneFrequency = m_toneFrequency0 + 0.5*m_fSpread*(x1+x2-1.0);
          m_dphi = m_twoPi * toneFrequency / audioSampleRate;
          m_j0=j;
        }

        m_phi += m_dphi;
        if (m_phi > m_twoPi) m_phi -= m_twoPi;
        //ramp for first tone
        if (m_ic==0) m_amp = m_amp * 0.008144735;
        if (m_ic > 0 and  m_ic < 191) m_amp = m_amp / 0.975;
        //ramp for last tone
        if (m_ic > i0) m_amp = 0.99 * m_amp;
        if (m_ic > i1) m_amp = 0.0;
        sample=qRound(newVolume * m_amp*qSin(m_phi));

        //transmit from a precomputed FT8 wave[] array:
        if(!m_tuning and (m_toneSpacing < 0.0)) {
          m_amp=32767.0;
          float wave_sample = 0.0f;
          if (m_ic < static_cast<unsigned> (m_waveSnapshot.size ()))
            {
              wave_sample = m_waveSnapshot[static_cast<int> (m_ic)];
            }
          sample=qRound(newVolume * m_amp * wave_sample);
        }
        samples = load (postProcessSample (sample), samples);
        ++framesGenerated;
        ++m_ic;
      }

      // Precomputed wave finished (FT2/Fox) — clean exit after ramp-down
      if (!m_tuning && m_toneSpacing < 0 && m_ic > i1) {
        m_state = Idle;
        Q_EMIT tci_mod_active(m_state != Idle);
        return framesGenerated * bytesPerFrame;
      }

      if (m_amp == 0.0) { // TODO G4WJS: compare double with zero might not be wise
        if (icw[0] == 0) {
          // no CW ID to send
          m_state = Idle;
          Q_EMIT tci_mod_active(m_state != Idle);
          return framesGenerated * bytesPerFrame;
        }
        m_phi = 0.0;
      }

      m_frequency0 = m_trfrequency;
      // done for this chunk - continue on next call
      if (samples != end && framesGenerated) {
      }
      while (samples != end) { // pad block with silence
        samples = load (0, samples);
        ++framesGenerated;
      }
      return framesGenerated * bytesPerFrame;
    }
      // fall through

    case Idle:
      while (samples != end) samples = load (0, samples);
  }

  Q_ASSERT (Idle == m_state);
  return 0;
}

qint16 TCITransceiver::postProcessSample (qint16 sample) const
{
  if (m_addNoise) {  // Test frame, we'll add noise
    qint32 s = m_fac * (gran () + sample * m_snr / 32768.0);
    if (s > std::numeric_limits<qint16>::max ()) {
      s = std::numeric_limits<qint16>::max ();
    }
    if (s < std::numeric_limits<qint16>::min ()) {
      s = std::numeric_limits<qint16>::min ();
    }
    sample = s;
  }
  return sample;
}

#include "TCITransceiver.moc"
