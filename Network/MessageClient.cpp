#include "MessageClient.hpp"

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>

#include <QUdpSocket>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QTimer>
#include <QQueue>
#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QSet>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>

#include "NetworkMessage.hpp"
#include "qt_helpers.hpp"
#include "pimpl_impl.hpp"
#include "revision_utils.hpp"

#include "moc_MessageClient.cpp"

// some trace macros
#if WSJT_TRACE_UDP
#define TRACE_UDP(MSG) qDebug () << QString {"MessageClient::%1:"}.arg (__func__) << MSG
#else
#define TRACE_UDP(MSG)
#endif

namespace
{
char const * message_type_name (NetworkMessage::Type type)
{
  switch (type)
    {
    case NetworkMessage::Heartbeat: return "Heartbeat";
    case NetworkMessage::Status: return "Status";
    case NetworkMessage::Decode: return "Decode";
    case NetworkMessage::Clear: return "Clear";
    case NetworkMessage::Reply: return "Reply";
    case NetworkMessage::QSOLogged: return "QSOLogged";
    case NetworkMessage::Close: return "Close";
    case NetworkMessage::Replay: return "Replay";
    case NetworkMessage::HaltTx: return "HaltTx";
    case NetworkMessage::FreeText: return "FreeText";
    case NetworkMessage::WSPRDecode: return "WSPRDecode";
    case NetworkMessage::Location: return "Location";
    case NetworkMessage::LoggedADIF: return "LoggedADIF";
    case NetworkMessage::HighlightCallsign: return "HighlightCallsign";
    case NetworkMessage::SwitchConfiguration: return "SwitchConfiguration";
    case NetworkMessage::Configure: return "Configure";
    case NetworkMessage::AnnotationInfo: return "AnnotationInfo";
    case NetworkMessage::SetupTx: return "SetupTx";
    case NetworkMessage::EnqueueDecode: return "EnqueueDecode";
    default: return "Unknown";
    }
}

QByteArray const& udp_hmac_marker ()
{
  static QByteArray const marker {"WSJTHMAC256:"};
  return marker;
}

bool env_flag_enabled (char const * name, bool default_value = false)
{
  auto const value = qEnvironmentVariable (name).trimmed ().toLower ();
  if (value.isEmpty ())
    {
      return default_value;
    }
  if (value == QStringLiteral ("1")
      || value == QStringLiteral ("true")
      || value == QStringLiteral ("yes")
      || value == QStringLiteral ("on"))
    {
      return true;
    }
  if (value == QStringLiteral ("0")
      || value == QStringLiteral ("false")
      || value == QStringLiteral ("no")
      || value == QStringLiteral ("off"))
    {
      return false;
    }
  return default_value;
}

bool constant_time_equals (QByteArray const& a, QByteArray const& b)
{
  if (a.size () != b.size ())
    {
      return false;
    }
  quint8 diff = 0;
  for (int i = 0; i < a.size (); ++i)
    {
      diff |= static_cast<quint8> (a.at (i) ^ b.at (i));
    }
  return 0 == diff;
}
}

class MessageClient::impl
  : public QUdpSocket
{
  Q_OBJECT;

public:
  impl (QString const& id, QString const& version, QString const& revision,
        port_type server_port, int TTL, MessageClient * self)
    : self_ {self}
    , enabled_ {false}
    , id_ {id}
    , version_ {version}
    , revision_ {revision}
    , dns_lookup_id_ {-1}
    , server_port_ {server_port}
    , TTL_ {TTL}
    , schema_ {2}  // use 2 prior to negotiation not 1 which is broken
    , heartbeat_timer_ {new QTimer {this}}
  {
    connect (heartbeat_timer_, &QTimer::timeout, this, &impl::heartbeat);
    connect (this, &QIODevice::readyRead, this, &impl::pending_datagrams);

    auto key = qEnvironmentVariable ("WSJT_UDP_HMAC_KEY").trimmed ();
    if (!key.isEmpty ())
      {
        hmac_key_ = key.toUtf8 ();
      }
    require_hmac_ = !hmac_key_.isEmpty () && env_flag_enabled ("WSJT_UDP_HMAC_REQUIRED", true);

    heartbeat_timer_->start (NetworkMessage::pulse * 1000);
  }

  ~impl ()
  {
    closedown ();
    if (dns_lookup_id_ != -1)
      {
        QHostInfo::abortHostLookup (dns_lookup_id_);
      }
  }

  enum StreamStatus {Fail, Short, OK};

  void set_server (QString const& server_name, QStringList const& network_interface_names);
  Q_SLOT void host_info_results (QHostInfo);
  void start ();
  void parse_message (QByteArray const&);
  void parse_message (QByteArray const&, QHostAddress const&, port_type);
  void pending_datagrams ();
  void heartbeat ();
  void closedown ();
  StreamStatus check_status (QDataStream const&) const;
  void rebuild_trusted_senders ();
  bool is_trusted_sender (QHostAddress const&, port_type) const;
  bool message_target_matches (QString const&) const;
  bool control_target_matches (QString const&) const;
  bool requires_direct_target (NetworkMessage::Type) const;
  QByteArray sign_datagram (QByteArray const& payload) const;
  bool extract_hmac (QByteArray const& datagram, QByteArray * payload, QByteArray * signature) const;
  void send_message (QByteArray const&, bool queue_if_pending = true, bool allow_duplicates = false);
  void send_message (QDataStream const& out, QByteArray const& message, bool queue_if_pending = true, bool allow_duplicates = false)
  {
    if (OK == check_status (out))
      {
        send_message (message, queue_if_pending, allow_duplicates);
      }
    else
      {
        Q_EMIT self_->error ("Error creating UDP message");
      }
  }

  MessageClient * self_;
  bool enabled_;
  QString id_;
  QString version_;
  QString revision_;
  int dns_lookup_id_;
  QHostAddress server_;
  port_type server_port_;
  port_type listen_port_ {0};   // 0 = ephemeral (vecchio comportamento), >0 = porta fissa
  int TTL_;
  std::vector<QNetworkInterface> network_interfaces_;
  quint32 schema_;
  QTimer * heartbeat_timer_;
  std::vector<QHostAddress> blocked_addresses_;
  QSet<QHostAddress> trusted_senders_;
  bool warned_untrusted_sender_ {false};
  bool warned_broadcast_control_ {false};
  bool warned_missing_hmac_ {false};
  bool warned_invalid_hmac_ {false};
  bool require_hmac_ {false};
  QByteArray hmac_key_;

  // hold messages sent before host lookup completes asynchronously
  QQueue<QByteArray> pending_messages_;
  QByteArray last_message_;
};

#include "MessageClient.moc"

void MessageClient::impl::set_server (QString const& server_name, QStringList const& network_interface_names)
{
  // qDebug () << "MessageClient server:" << server_name << "port:" << server_port_ << "interfaces:" << network_interface_names;
  server_.setAddress (server_name);
  network_interfaces_.clear ();
  for (auto const& net_if_name : network_interface_names)
    {
      network_interfaces_.push_back (QNetworkInterface::interfaceFromName (net_if_name));
    }
  warned_untrusted_sender_ = false;
  warned_broadcast_control_ = false;

  if (server_.isNull () && server_name.size ()) // DNS lookup required
    {
      // queue a host address lookup
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
      dns_lookup_id_ = QHostInfo::lookupHost (server_name, this, &MessageClient::impl::host_info_results);
#else
      dns_lookup_id_ = QHostInfo::lookupHost (server_name, this, SLOT (host_info_results (QHostInfo)));
#endif
    }
  else
    {
      start ();
    }
}

void MessageClient::impl::host_info_results (QHostInfo host_info)
{
  if (host_info.lookupId () != dns_lookup_id_) return;
  dns_lookup_id_ = -1;
  if (QHostInfo::NoError != host_info.error ())
    {
      Q_EMIT self_->error ("UDP server DNS lookup failed: " + host_info.errorString ());
      return;
    }
  else
    {
      auto const& server_addresses = host_info.addresses ();
      if (server_addresses.size ())
        {
          server_ = server_addresses[0];
        }
    }
  rebuild_trusted_senders ();
  start ();
}

void MessageClient::impl::start ()
{
  rebuild_trusted_senders ();
  if (server_.isNull ())
    {
      Q_EMIT self_->close ();
      pending_messages_.clear (); // discard
      return;
    }

  if (is_broadcast_address (server_))
    {
      Q_EMIT self_->error ("IPv4 broadcast not supported, please specify the loop-back address, a server host address, or multicast group address");
      pending_messages_.clear (); // discard
      return;
    }

  if (blocked_addresses_.end () != std::find (blocked_addresses_.begin (), blocked_addresses_.end (), server_))
    {
      Q_EMIT self_->error ("UDP server blocked, please try another");
      pending_messages_.clear (); // discard
      return;
    }

  TRACE_UDP ("Trying server:" << server_.toString ());
  QHostAddress interface_addr {IPv6Protocol == server_.protocol () ? QHostAddress::AnyIPv6 : QHostAddress::AnyIPv4};

  if (localAddress () != interface_addr)
    {
      if (UnconnectedState != state () || state ())
        {
          close ();
        }
      // Bind to fixed listen_port_ if configured, otherwise ephemeral.
      // A fixed port allows external programs (JTAlert, DecoAlert) to
      // reliably send commands to Decodium without heartbeat discovery.
      if (listen_port_ > 0)
        bind (interface_addr, listen_port_, ShareAddress | ReuseAddressHint);
      else
        bind (interface_addr);
      // qDebug () << "Bound to UDP port:" << localPort () << "on:" << localAddress ();

      // set multicast TTL to limit scope when sending to multicast
      // group addresses
      setSocketOption (MulticastTtlOption, TTL_);
    }

  // send initial heartbeat which allows schema negotiation
  heartbeat ();

  // clear any backlog
  while (pending_messages_.size ())
    {
      send_message (pending_messages_.dequeue (), true, false);
    }
}

void MessageClient::impl::pending_datagrams ()
{
  while (hasPendingDatagrams ())
    {
      QByteArray datagram;
      datagram.resize (pendingDatagramSize ());
      QHostAddress sender_address;
      port_type sender_port;
      if (0 <= readDatagram (datagram.data (), datagram.size (), &sender_address, &sender_port))
        {
          TRACE_UDP ("message received from:" << sender_address << "port:" << sender_port);
          parse_message (datagram, sender_address, sender_port);
        }
    }
}

void MessageClient::impl::parse_message (QByteArray const& msg)
{
  parse_message (msg, QHostAddress {}, 0u);
}

void MessageClient::impl::parse_message (QByteArray const& msg, QHostAddress const& sender_address, port_type sender_port)
{
  try
    {
      QByteArray signed_payload = msg;
      QByteArray received_signature;
      bool const has_hmac = extract_hmac (msg, &signed_payload, &received_signature);
      if (!hmac_key_.isEmpty ())
        {
          if (!has_hmac)
            {
              if (require_hmac_)
                {
                  if (!warned_missing_hmac_)
                    {
                      warned_missing_hmac_ = true;
                      Q_EMIT self_->error (QString {"Rejected UDP packet without HMAC from %1:%2"}
                                           .arg (sender_address.toString ())
                                           .arg (sender_port));
                    }
                  return;
                }
            }
          else
            {
              auto const expected = sign_datagram (signed_payload);
              if (!constant_time_equals (expected, received_signature))
                {
                  if (!warned_invalid_hmac_)
                    {
                      warned_invalid_hmac_ = true;
                      Q_EMIT self_->error (QString {"Rejected UDP packet with invalid HMAC from %1:%2"}
                                           .arg (sender_address.toString ())
                                           .arg (sender_port));
                    }
                  return;
                }
            }
        }

      // 
      // message format is described in NetworkMessage.hpp
      // 
      NetworkMessage::Reader in {signed_payload};
      if (OK == check_status (in))
        {
          if (!sender_address.isNull () && !is_trusted_sender (sender_address, sender_port))
            {
              if (!warned_untrusted_sender_)
                {
                  warned_untrusted_sender_ = true;
                  Q_EMIT self_->error (QString {"Rejected UDP control packet from untrusted sender %1:%2"}
                                       .arg (sender_address.toString ())
                                       .arg (sender_port));
                }
              return;
            }

          if (schema_ < in.schema ()) // one time record of server's
                                      // negotiated schema
            {
              schema_ = in.schema ();
            }

          if (!enabled_)
            {
              TRACE_UDP ("message processing disabled for id:" << in.id ());
              return;
            }

          auto const incoming_type = static_cast<NetworkMessage::Type> (in.type ());
          auto const incoming_id = in.id ();

          if (requires_direct_target (incoming_type) && !control_target_matches (incoming_id))
            {
              if (!warned_broadcast_control_)
                {
                  warned_broadcast_control_ = true;
                  Q_EMIT self_->error (
                    QString {
                      "Rejected UDP control packet (%1): target id \"%2\" does not match this instance id \"%3\". "
                      "Configure your controller to use target id \"%3\" (not ALLCALL/BROADCAST/*)."
                    }
                      .arg (QString::fromLatin1 (message_type_name (incoming_type)))
                      .arg (incoming_id)
                      .arg (id_)
                  );
                }
              TRACE_UDP ("rejected control message with non-direct target id:" << incoming_id);
              return;
            }

          if (!message_target_matches (incoming_id))
            {
              TRACE_UDP ("ignored message for id:" << incoming_id);
              return;
            }

          //
          // message format is described in NetworkMessage.hpp
          //
          switch (in.type ())
            {
            case NetworkMessage::Reply:
              {
                // unpack message
                QTime time;
                qint32 snr;
                float delta_time;
                quint32 delta_frequency;
                QByteArray mode;
                QByteArray message;
                bool low_confidence {false};
                quint8 modifiers {0};
                in >> time >> snr >> delta_time >> delta_frequency >> mode >> message
                   >> low_confidence >> modifiers;
                TRACE_UDP ("Reply: time:" << time << "snr:" << snr << "dt:" << delta_time << "df:" << delta_frequency << "mode:" << mode << "message:" << message << "low confidence:" << low_confidence << "modifiers: 0x"
#if QT_VERSION >= QT_VERSION_CHECK (5, 15, 0)
                           << Qt::hex
#else
                           << hex
#endif
                           << modifiers);
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->reply (time, snr, delta_time, delta_frequency
                                         , QString::fromUtf8 (mode), QString::fromUtf8 (message)
                                         , low_confidence, modifiers);
                  }
              }
              break;

            case NetworkMessage::Clear:
              {
                quint8 window {0};
                in >> window;
                TRACE_UDP ("Clear window:" << window);
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->clear_decodes (window);
                  }
              }
              break;

            case NetworkMessage::Close:
              TRACE_UDP ("Close");
              if (check_status (in) != Fail)
                {
                  last_message_.clear ();
                  Q_EMIT self_->close ();
                }
              break;

            case NetworkMessage::Replay:
              TRACE_UDP ("Replay");
              if (check_status (in) != Fail)
                {
                  last_message_.clear ();
                  Q_EMIT self_->replay ();
                }
              break;

            case NetworkMessage::HaltTx:
              {
                bool auto_only {false};
                in >> auto_only;
                TRACE_UDP ("Halt Tx auto_only:" << auto_only);
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->halt_tx (auto_only);
                  }
              }
              break;

            case NetworkMessage::SetupTx:        //avt 11/28/20
              {
                int newTxMsgIdx {0};
                QByteArray msg;
                bool skipGrid;
                bool useRR73;
                QByteArray check;
                quint32 offset;             //avt 11/12/21
                in >> newTxMsgIdx >> msg >> skipGrid >> useRR73 >> check >> offset;
                TRACE_UDP ("Setup Tx newTxMsgIdx:" << newTxMsgIdx << "msg:" << msg << "skipGrid:" << skipGrid << "useRR73:" << useRR73 << "check:" << check << "offset" << offset); //avt 11/14/21
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->setup_tx (newTxMsgIdx, QString::fromUtf8(msg), skipGrid, useRR73, QString::fromUtf8(check), offset);
                  }
              }
              break;

            case NetworkMessage::FreeText:
              {
                QByteArray message;
                bool send {true};
                in >> message >> send;
                TRACE_UDP ("FreeText message:" << message << "send:" << send);
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->free_text (QString::fromUtf8 (message), send);
                  }
              }
              break;

            case NetworkMessage::Location:
              {
                QByteArray location;
                in >> location;
                TRACE_UDP ("Location location:" << location);
                if (check_status (in) != Fail)
                {
                    Q_EMIT self_->location (QString::fromUtf8 (location));
                }
              }
              break;

            case NetworkMessage::HighlightCallsign:
              {
                QByteArray call;
                QColor bg;      // default invalid color
                QColor fg;      // default invalid color
                bool last_only {false};
                in >> call >> bg >> fg >> last_only;
                TRACE_UDP ("HighlightCallsign call:" << call << "bg:" << bg << "fg:" << fg << "last only:" << last_only);
                if (check_status (in) != Fail && call.size ())
                  {
                    Q_EMIT self_->highlight_callsign (QString::fromUtf8 (call), bg, fg, last_only);
                  }
              }
              break;

            case NetworkMessage::SwitchConfiguration:
              {
                QByteArray configuration_name;
                in >> configuration_name;
                TRACE_UDP ("Switch Configuration name:" << configuration_name);
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->switch_configuration (QString::fromUtf8 (configuration_name));
                  }
              }
              break;

            case NetworkMessage::Configure:
              {
                QByteArray mode;
                quint32 frequency_tolerance;
                QByteArray submode;
                bool fast_mode {false};
                quint32 tr_period {std::numeric_limits<quint32>::max ()};
                quint32 rx_df {std::numeric_limits<quint32>::max ()};
                QByteArray dx_call;
                QByteArray dx_grid;
                bool generate_messages {false};
                in >> mode >> frequency_tolerance >> submode >> fast_mode >> tr_period >> rx_df
                   >> dx_call >> dx_grid >> generate_messages;
                TRACE_UDP ("Configure mode:" << mode << "frequency tolerance:" << frequency_tolerance << "submode:" << submode << "fast mode:" << fast_mode << "T/R period:" << tr_period << "rx df:" << rx_df << "dx call:" << dx_call << "dx grid:" << dx_grid << "generate messages:" << generate_messages);
                if (check_status (in) != Fail)
                  {
                    Q_EMIT self_->configure (QString::fromUtf8 (mode), frequency_tolerance
                                             , QString::fromUtf8 (submode), fast_mode, tr_period, rx_df
                                             , QString::fromUtf8 (dx_call), QString::fromUtf8 (dx_grid)
                                             , generate_messages);
                  }
              }
              break;

              case NetworkMessage::AnnotationInfo: {
                QByteArray dx_call;
                bool sort_order_provided{false};
                quint32 sort_order{std::numeric_limits<quint32>::max()};
                in >> dx_call >> sort_order_provided >> sort_order;
                TRACE_UDP ("External Callsign Info:" << dx_call << "sort_order_provided:" << sort_order_provided
                                                     << "sort_order:" << sort_order);
                if (sort_order > 50000) sort_order = 50000;
                if (check_status(in) != Fail) {
                  Q_EMIT
                  self_->annotation_info(QString::fromUtf8(dx_call), sort_order_provided, sort_order);
                }
              }
                break;

              default:
              // Ignore
              //
              // Note that although server  heartbeat messages are not
              // parsed here  they are  still partially parsed  in the
              // message reader class to  negotiate the maximum schema
              // number being used on the network.
              if (NetworkMessage::Heartbeat != in.type ())
                {
                  TRACE_UDP ("ignoring message type:" << in.type ());
                }
              break;
            }
        }
      else
        {
          TRACE_UDP ("ignored message for id:" << in.id ());
        }
    }
  catch (std::exception const& e)
    {
      Q_EMIT self_->error (QString {"MessageClient exception: %1"}.arg (e.what ()));
    }
  catch (...)
    {
      Q_EMIT self_->error ("Unexpected exception in MessageClient");
    }
}

QByteArray MessageClient::impl::sign_datagram (QByteArray const& payload) const
{
  return QMessageAuthenticationCode::hash (payload, hmac_key_, QCryptographicHash::Sha256);
}

bool MessageClient::impl::extract_hmac (QByteArray const& datagram, QByteArray * payload, QByteArray * signature) const
{
  auto const& marker = udp_hmac_marker ();
  int const trailer_size = marker.size () + 64; // SHA-256 as hex
  if (datagram.size () < trailer_size)
    {
      return false;
    }

  int const marker_pos = datagram.size () - trailer_size;
  if (datagram.mid (marker_pos, marker.size ()) != marker)
    {
      return false;
    }

  auto const sig_hex = datagram.mid (marker_pos + marker.size (), 64);
  auto const sig_bin = QByteArray::fromHex (sig_hex);
  if (sig_bin.size () != 32)
    {
      return false;
    }

  if (payload)
    {
      *payload = datagram.left (marker_pos);
    }
  if (signature)
    {
      *signature = sig_bin;
    }
  return true;
}

void MessageClient::impl::rebuild_trusted_senders ()
{
  trusted_senders_.clear ();
  if (!server_.isNull () && !server_.isMulticast ())
    {
      trusted_senders_.insert (server_);
    }
  trusted_senders_.insert (QHostAddress::LocalHost);
  trusted_senders_.insert (QHostAddress::LocalHostIPv6);

  auto const add_interface = [this] (QNetworkInterface const& net_if) {
    for (auto const& entry : net_if.addressEntries ())
      {
        auto const address = entry.ip ();
        if (!address.isNull () && !address.isMulticast ())
          {
            trusted_senders_.insert (address);
          }
      }
  };

  if (!network_interfaces_.empty ())
    {
      for (auto const& net_if : network_interfaces_)
        {
          add_interface (net_if);
        }
    }
  else
    {
      for (auto const& net_if : QNetworkInterface::allInterfaces ())
        {
          add_interface (net_if);
        }
    }
}

bool MessageClient::impl::is_trusted_sender (QHostAddress const& sender, port_type sender_port) const
{
  if (sender.isNull ())
    {
      return false;
    }
  if (server_port_ && sender_port != server_port_)
    {
      return false;
    }
  if (server_.isLoopback () && sender.isLoopback ())
    {
      return true;
    }
  return trusted_senders_.contains (sender);
}

bool MessageClient::impl::message_target_matches (QString const& incoming_id) const
{
  auto const trimmed = incoming_id.trimmed ();
  if (trimmed.isEmpty () || trimmed == id_)
    {
      return true;
    }
  return trimmed.compare ("ALLCALL", Qt::CaseInsensitive) == 0
      || trimmed.compare ("BROADCAST", Qt::CaseInsensitive) == 0
      || trimmed == "*";
}

bool MessageClient::impl::control_target_matches (QString const& incoming_id) const
{
  auto const trimmed = incoming_id.trimmed ();
  return !trimmed.isEmpty () && 0 == trimmed.compare (id_, Qt::CaseInsensitive);
}

bool MessageClient::impl::requires_direct_target (NetworkMessage::Type type) const
{
  switch (type)
    {
    case NetworkMessage::Clear:
    case NetworkMessage::Reply:
    case NetworkMessage::Close:
    case NetworkMessage::Replay:
    case NetworkMessage::HaltTx:
    case NetworkMessage::FreeText:
    case NetworkMessage::SwitchConfiguration:
    case NetworkMessage::Configure:
    case NetworkMessage::SetupTx:
      return true;

    default:
      return false;
    }
}

void MessageClient::impl::heartbeat ()
{
   if (server_port_ && !server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::Heartbeat, id_, schema_};
      out << NetworkMessage::Builder::schema_number // maximum schema number accepted
          << version_.toUtf8 () << revision_.toUtf8 ();
      TRACE_UDP ("schema:" << schema_ << "max schema:" << NetworkMessage::Builder::schema_number << "version:" << version_ << "revision:" << revision_);
      send_message (out, message, false, true);
    }
}

void MessageClient::impl::closedown ()
{
   if (server_port_ && !server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::Close, id_, schema_};
      TRACE_UDP ("");
      send_message (out, message, false);
    }
}

void MessageClient::impl::send_message (QByteArray const& message, bool queue_if_pending, bool allow_duplicates)
{
  if (server_port_)
    {
      if (!server_.isNull ())
        {
          QByteArray wire_message = message;
          if (!hmac_key_.isEmpty ())
            {
              wire_message += udp_hmac_marker ();
              wire_message += sign_datagram (message).toHex ();
            }
          if (allow_duplicates || message != last_message_) // avoid duplicates
            {
              if (is_multicast_address (server_))
                {
                  // send datagram on each selected network interface
                  std::for_each (network_interfaces_.begin (), network_interfaces_.end ()
                                 , [&] (QNetworkInterface const& net_if) {
                                     setMulticastInterface (net_if);
                                     // qDebug () << "Multicast UDP datagram sent to:" << server_ << "port:" << server_port_ << "on:" << multicastInterface ().humanReadableName ();
                                     writeDatagram (wire_message, server_, server_port_);
                                   });
                }
              else
                {
                  // qDebug () << "Unicast UDP datagram sent to:" << server_ << "port:" << server_port_;
                  writeDatagram (wire_message, server_, server_port_);
                }
              last_message_ = message;
            }
        }
      else if (queue_if_pending)
        {
          pending_messages_.enqueue (message);
        }
    }
}

auto MessageClient::impl::check_status (QDataStream const& stream) const -> StreamStatus
{
  auto stat = stream.status ();
  StreamStatus result {Fail};
  switch (stat)
    {
    case QDataStream::ReadPastEnd:
      result = Short;
      break;

    case QDataStream::ReadCorruptData:
      Q_EMIT self_->error ("Message serialization error: read corrupt data");
      break;

    case QDataStream::WriteFailed:
      Q_EMIT self_->error ("Message serialization error: write error");
      break;

    default:
      result = OK;
      break;
    }
  return result;
}

MessageClient::MessageClient (QString const& id, QString const& version, QString const& revision,
                              QString const& server_name, port_type server_port,
                              QStringList const& network_interface_names,
                              int TTL, QObject * self)
  : QObject {self}
  , m_ {id, version, revision, server_port, TTL, this}
{
  connect (&*m_
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
           , static_cast<void (impl::*) (impl::SocketError)> (&impl::error), [this] (impl::SocketError e)
#else
           , &impl::errorOccurred, [this] (impl::SocketError e)
#endif
                                   {
#if defined (Q_OS_WIN)
                                     if (e != impl::NetworkError // take this out when Qt 5.5 stops doing this spuriously
                                         && e != impl::ConnectionRefusedError) // not interested in this with UDP socket
                                       {
#else
                                       {
                                         Q_UNUSED (e);
#endif
                                         Q_EMIT error (m_->errorString ());
                                       }
                                       });
  m_->set_server (server_name, network_interface_names);
}

QHostAddress MessageClient::server_address () const
{
  return m_->server_;
}

auto MessageClient::server_port () const -> port_type
{
  return m_->server_port_;
}

void MessageClient::set_server (QString const& server_name, QStringList const& network_interface_names)
{
  m_->set_server (server_name, network_interface_names);
}

void MessageClient::set_server_port (port_type server_port)
{
  m_->server_port_ = server_port;
}

void MessageClient::set_TTL (int TTL)
{
  m_->TTL_ = TTL;
  m_->setSocketOption (QAbstractSocket::MulticastTtlOption, m_->TTL_);
}

void MessageClient::set_listen_port (port_type listen_port)
{
  if (m_->listen_port_ != listen_port)
    {
      m_->listen_port_ = listen_port;
      // Rebind immediately if already connected to a server
      if (!m_->server_.isNull ())
        {
          QHostAddress interface_addr (QAbstractSocket::IPv6Protocol == m_->server_.protocol ()
                                       ? QHostAddress::AnyIPv6 : QHostAddress::AnyIPv4);
          m_->close ();
          if (listen_port > 0)
            m_->bind (interface_addr, listen_port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
          else
            m_->bind (interface_addr);
        }
    }
}

void MessageClient::enable (bool flag)
{
  m_->enabled_ = flag;
}

void MessageClient::status_update (Frequency f, QString const& mode, QString const& dx_call
                                   , QString const& report, QString const& tx_mode
                                   , bool tx_enabled, bool transmitting, bool decoding
                                   , quint32 rx_df, quint32 tx_df, QString const& de_call
                                   , QString const& de_grid, QString const& dx_grid
                                   , bool watchdog_timeout, QString const& sub_mode
                                   , bool fast_mode, quint8 special_op_mode
                                   , quint32 frequency_tolerance, quint32 tr_period
                                   , QString const& configuration_name
                                   , QString const& lastTxMsg   //avt
                                   , quint32 qsoProgress        //avt
                                   , bool txFirst               //avt
                                   , bool cQonly                //avt
                                   , QString const& genMsg      //avt
                                   , bool txHaltClk             //avt
                                   , bool txEnableState         //avt 1/23/24
                                   , bool txEnableClk           //avt 1/28/24
                                   , QString const& myContinent //avt 5/6/24
                                   , bool metricUnits)          //avt 5/7/24
{
  if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::Status, m_->id_, m_->schema_};
      out << f << mode.toUtf8 () << dx_call.toUtf8 () << report.toUtf8 () << tx_mode.toUtf8 ()
          << tx_enabled << transmitting << decoding << rx_df << tx_df << de_call.toUtf8 ()
          << de_grid.toUtf8 () << dx_grid.toUtf8 () << watchdog_timeout << sub_mode.toUtf8 ()
          << fast_mode << special_op_mode << frequency_tolerance << tr_period << configuration_name.toUtf8 () 
          << lastTxMsg.toUtf8 () << qsoProgress << txFirst << cQonly  << genMsg.toUtf8 () << txHaltClk 
          << txEnableState << txEnableClk << myContinent.toUtf8 () << metricUnits;    //avt 5/7/24
      TRACE_UDP ("frequency:" << f << "mode:" << mode << "DX:" << dx_call << "report:" << report << "Tx mode:" << tx_mode << "tx_enabled:" << tx_enabled << "Tx:" << transmitting << "decoding:" << decoding << "Rx df:" << rx_df << "Tx df:" << tx_df << "DE:" << de_call << "DE grid:" << de_grid << "DX grid:" << dx_grid << "w/d t/o:" << watchdog_timeout << "sub_mode:" << sub_mode << "fast mode:" << fast_mode << "spec op mode:" << special_op_mode << "frequency tolerance:" << frequency_tolerance << "T/R period:" << tr_period << "configuration name:" << configuration_name "lastTxMsg:" << lastTxMsg << "qsoProgress:" << qsoProgress << "txFirst:" << txFirst << "cQonly:" << cQonly << "genMsg:" << genMsg << "txHaltClk:" << txHaltClk << "txEnableState:" << txEnableState << "txEnableClk:" << txEnableClk << "myContinent" << myContinent << "metricUnits" << metricUnits); //avt 5/7/24
      m_->send_message (out, message);
    }
}

void MessageClient::decode (bool is_new, QTime time, qint32 snr, float delta_time, quint32 delta_frequency
                            , QString const& mode, QString const& message_text, bool low_confidence
                            , bool off_air)
{
   if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::Decode, m_->id_, m_->schema_};
      out << is_new << time << snr << delta_time << delta_frequency << mode.toUtf8 ()
          << message_text.toUtf8 () << low_confidence << off_air;
      TRACE_UDP ("new" << is_new << "time:" << time << "snr:" << snr << "dt:" << delta_time << "df:" << delta_frequency << "mode:" << mode << "text:" << message_text << "low conf:" << low_confidence << "off air:" << off_air);
      m_->send_message (out, message);
    }
}

//avt 1/3/21
void MessageClient::enqueue_decode (bool autoGen, QTime time, qint32 snr, float delta_time, quint32 delta_frequency
                            , QString const& mode, QString const& message_text, bool isDx
                            , bool modifier
                            , bool isNewCallOnBand
                            , bool isNewCall            //avt 5/6/24
                            , bool isNewCountryOnBand
                            , bool isNewCountry
                            , QString const& country    //avt 5/4/24
                            , QString const& continent //avt 5/6/24
                            , int az    //avt 5/7/24
                            , int dist) //avt 5/7/24
{
   if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::EnqueueDecode, m_->id_, m_->schema_};
      out << autoGen << time << snr << delta_time << delta_frequency << mode.toUtf8 ()
          << message_text.toUtf8 () << isDx << modifier << isNewCallOnBand << isNewCall << isNewCountryOnBand << isNewCountry << country.toUtf8 () << continent.toUtf8 () << az << dist;  //avt 5/7/24
      TRACE_UDP ("autoGen" << autoGen << "time:" << time << "snr:" << snr << "dt:" << delta_time << "df:" << delta_frequency << "mode:" << mode << "text:" << message_text << "isDx:" << isDx << "modifier:" << modifier << "isNewCallOnBand" << isNewCallOnBand << "isNewCountryOnBand:" << isNewCountryOnBand << "isNewCountry"  << isNewCountry << "country" << country << "continent" << continent << "az" << az << "dist" << dist);  //avt 5/7/24
      m_->send_message (out, message);
    }
}

void MessageClient::WSPR_decode (bool is_new, QTime time, qint32 snr, float delta_time, Frequency frequency
                                 , qint32 drift, QString const& callsign, QString const& grid, qint32 power
                                 , bool off_air)
{
   if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::WSPRDecode, m_->id_, m_->schema_};
      out << is_new << time << snr << delta_time << frequency << drift << callsign.toUtf8 ()
          << grid.toUtf8 () << power << off_air;
      TRACE_UDP ("new:" << is_new << "time:" << time << "snr:" << snr << "dt:" << delta_time << "frequency:" << frequency << "drift:" << drift << "call:" << callsign << "grid:" << grid << "pwr:" << power << "off air:" << off_air);
      m_->send_message (out, message);
    }
}

void MessageClient::decodes_cleared ()
{
   if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::Clear, m_->id_, m_->schema_};
      TRACE_UDP ("");
      m_->send_message (out, message);
    }
}

void MessageClient::qso_logged (QDateTime time_off, QString const& dx_call, QString const& dx_grid
                                , Frequency dial_frequency, QString const& mode, QString const& report_sent
                                , QString const& report_received, QString const& tx_power
                                , QString const& comments, QString const& name, QDateTime time_on
                                , QString const& operator_call, QString const& my_call
                                , QString const& my_grid, QString const& exchange_sent
                                , QString const& exchange_rcvd, QString const& propmode
                                , QString const& satellite, QString const& satmode, QString const& freqRx)
{
   if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::QSOLogged, m_->id_, m_->schema_};
      out << time_off << dx_call.toUtf8 () << dx_grid.toUtf8 () << dial_frequency << mode.toUtf8 ()
          << report_sent.toUtf8 () << report_received.toUtf8 () << tx_power.toUtf8 () << comments.toUtf8 ()
          << name.toUtf8 () << time_on << operator_call.toUtf8 () << my_call.toUtf8 () << my_grid.toUtf8 ()
          << exchange_sent.toUtf8 () << exchange_rcvd.toUtf8 () << propmode.toUtf8 () << satellite.toUtf8 ()
          << satmode.toUtf8 () << freqRx.toUtf8 ();
      TRACE_UDP ("time off:" << time_off << "DX:" << dx_call << "DX grid:" << dx_grid << "dial:" << dial_frequency << "mode:" << mode << "sent:" << report_sent << "rcvd:" << report_received << "pwr:" << tx_power << "comments:" << comments << "name:" << name << "time on:" << time_on << "op:" << operator_call << "DE:" << my_call << "DE grid:" << my_grid << "exch sent:" << exchange_sent << "exch rcvd:" << exchange_rcvd  << "prop_mode:" << propmode << "sat_name:" << satellite << "sat_mode:" << satmode << "freq_rx:" << freqRx);
      m_->send_message (out, message);
    }
}

void MessageClient::logged_ADIF (QByteArray const& ADIF_record)
{
   if (m_->server_port_ && !m_->server_.isNull ())
    {
      QByteArray message;
      NetworkMessage::Builder out {&message, NetworkMessage::LoggedADIF, m_->id_, m_->schema_};
      auto const program_id = QString {"Decodium FT2 %1"}.arg (version (false));
      QByteArray ADIF {
        QString {"\n<adif_ver:5>3.1.0\n<programid:%1>%2\n<EOH>\n"}
          .arg (program_id.size ())
          .arg (program_id)
          .toLatin1 ()
        + ADIF_record + " <EOR>"};
      out << ADIF;
      TRACE_UDP ("ADIF:" << ADIF);
      m_->send_message (out, message);
    }
}
