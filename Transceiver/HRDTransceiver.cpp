#include "HRDTransceiver.hpp"

// HRDMessage usa placement new su buffer raw (vedi struct riga ~33): GCC 14+
// non riesce a tracciare l'inizializzazione attraverso il placement new e
// emette -Wmaybe-uninitialized in send_command. La build CI MSYS2 ha
// -Werror attivo, quindi disabilitiamo localmente il warning.
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <algorithm>
#include <cstddef>
#include <QHostAddress>
#include <QByteArray>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThread>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>

#include "Network/NetworkServerLookup.hpp"
#include "DecodiumLogging.hpp"
#include "qt_helpers.hpp"

namespace
{
  char const * const HRD_transceiver_name = "Ham Radio Deluxe";

  // some commands require a settling time, particularly "RX A" and
  // "RX B" on the Yaesu FTdx3000.
  int constexpr yaesu_delay {350};
  int constexpr hrd_connect_timeout_ms {5000};
  int constexpr hrd_write_timeout_ms {1500};
  int constexpr hrd_probe_reply_timeout_ms {1000};
  unsigned constexpr hrd_probe_reply_retries {60};
  int constexpr hrd_startup_protocol_silent_timeout_ms {15000};
  int constexpr hrd_startup_command_reply_timeout_ms {2000};
  unsigned constexpr hrd_startup_command_reply_retries {8};
  int constexpr hrd_command_reply_timeout_ms {1000};
  unsigned constexpr hrd_command_reply_retries {5};
  int constexpr hrd_shutdown_write_timeout_ms {500};
  int constexpr hrd_shutdown_command_reply_timeout_ms {500};
  unsigned constexpr hrd_shutdown_command_reply_retries {2};
  qsizetype constexpr hrd_max_reply_bytes {16 * 1024 * 1024};

  bool is_startup_probe (QString const& cmd)
  {
    return 0 == cmd.compare (QStringLiteral ("get context"), Qt::CaseInsensitive)
      || 0 == cmd.compare (QStringLiteral ("get id"), Qt::CaseInsensitive);
  }

  QString hrd_protocol_name (int protocol)
  {
    switch (protocol)
      {
      case 1: return QStringLiteral ("v4");
      case 2: return QStringLiteral ("v5");
      default: return QStringLiteral ("none");
      }
  }

  QString hrd_socket_state_name (QTcpSocket const * socket)
  {
    if (!socket) return QStringLiteral ("null");
    switch (socket->state ())
      {
      case QAbstractSocket::UnconnectedState: return QStringLiteral ("unconnected");
      case QAbstractSocket::HostLookupState: return QStringLiteral ("lookup");
      case QAbstractSocket::ConnectingState: return QStringLiteral ("connecting");
      case QAbstractSocket::ConnectedState: return QStringLiteral ("connected");
      case QAbstractSocket::BoundState: return QStringLiteral ("bound");
      case QAbstractSocket::ClosingState: return QStringLiteral ("closing");
      case QAbstractSocket::ListeningState: return QStringLiteral ("listening");
      default: return QStringLiteral ("unknown");
      }
  }

  QString hrd_preview (QString text, int limit = 180)
  {
    text.replace (QLatin1Char ('\r'), QStringLiteral ("\\r"));
    text.replace (QLatin1Char ('\n'), QStringLiteral ("\\n"));
    text.replace (QLatin1Char ('\t'), QLatin1Char (' '));
    while (text.contains (QStringLiteral ("  ")))
      {
        text.replace (QStringLiteral ("  "), QLatin1String (" "));
      }
    if (text.size () > limit)
      {
        text = text.left (limit) + QStringLiteral ("...");
      }
    return text;
  }

  QString hrd_hex_preview (QByteArray const& data, int limit = 32)
  {
    QString text = QString::fromLatin1 (data.left (limit).toHex (' '));
    if (data.size () > limit)
      {
        text += QStringLiteral (" ...");
      }
    return text;
  }

  QString hrd_normalized_radio_name (QString text)
  {
    return text.trimmed ().simplified ().toCaseFolded ();
  }

  void hrd_diag (QString const& message)
  {
    DIAG_CAT (QStringLiteral ("[HRD] ") + message);
  }

  class ScopedStartupDiagnostics
  {
  public:
    explicit ScopedStartupDiagnostics (bool& active)
      : active_ {active}
    {
      active_ = true;
    }

    ~ScopedStartupDiagnostics ()
    {
      active_ = false;
    }

  private:
    bool& active_;
  };
}

#include "moc_HRDTransceiver.cpp"

void HRDTransceiver::register_transceivers (logger_type *,
                                            TransceiverFactory::Transceivers * registry,
                                            unsigned id)
{
  (*registry)[HRD_transceiver_name] = TransceiverFactory::Capabilities (id, TransceiverFactory::Capabilities::network, true, true /* maybe */);
}

struct HRDMessage
{
  // Placement style new overload for outgoing messages that does the
  // construction too.
  static void * operator new (size_t size, QString const& payload)
  {
    size += sizeof (QChar) * (payload.size () + 1); // space for terminator too
    HRDMessage * storage (reinterpret_cast<HRDMessage *> (new char[size]));
    storage->size_ = size ;
    ushort const * pl (payload.utf16 ());
    std::copy (pl, pl + payload.size () + 1, storage->payload_); // copy terminator too
    storage->magic_1_ = magic_1_value_;
    storage->magic_2_ = magic_2_value_;
    storage->checksum_ = 0;
    return storage;
  }

  // Placement style new overload for incoming messages that does the
  // construction too.
  //
  // No memory allocation here.
  static void * operator new (size_t /* size */, QByteArray const& message)
  {
    // Nasty const_cast here to avoid copying the message buffer.
    return const_cast<HRDMessage *> (reinterpret_cast<HRDMessage const *> (message.data ()));
  }

  void operator delete (void * p, size_t)
  {
    delete [] reinterpret_cast<char *> (p); // Mirror allocation in operator new above.
  }

  quint32 size_;
  qint32 magic_1_;
  qint32 magic_2_;
  qint32 checksum_;            // Apparently not used.
  QChar payload_[0];           // UTF-16 (which is wchar_t on Windows)

  static qint32 constexpr magic_1_value_ = 0x1234ABCD;
  static qint32 constexpr magic_2_value_ = 0xABCD1234;
};

HRDTransceiver::HRDTransceiver (logger_type * logger
                                , std::unique_ptr<TransceiverBase> wrapped
                                , QString const& server
                                , bool use_for_ptt
                                , TransceiverFactory::TXAudioSource audio_source
                                , int poll_interval
                                , QObject * parent)
  : PollingTransceiver {logger, poll_interval, parent}
  , wrapped_ {std::move (wrapped)}
  , use_for_ptt_ {use_for_ptt}
  , audio_source_ {audio_source}
  , server_ {server}
  , hrd_ {0}
  , protocol_ {none}
  , current_radio_ {0}
  , vfo_count_ {0}
  , vfo_A_button_ {-1}
  , vfo_B_button_ {-1}
  , vfo_toggle_button_ {-1}
  , mode_A_dropdown_ {-1}
  , mode_B_dropdown_ {-1}
  , data_mode_toggle_button_ {-1}
  , data_mode_on_button_ {-1}
  , data_mode_off_button_ {-1}
  , data_mode_dropdown_ {-1}
  , split_mode_button_ {-1}
  , split_mode_dropdown_ {-1}
  , split_mode_dropdown_write_only_ {false}
  , split_off_button_ {-1}
  , tx_A_button_ {-1}
  , tx_B_button_ {-1}
  , rx_A_button_ {-1}
  , rx_B_button_ {-1}
  , receiver_dropdown_ {-1}
  , ptt_button_ {-1}
  , alt_ptt_button_ {-1}
  , reversed_ {false}
  , startup_diagnostics_active_ {false}
  , shutdown_in_progress_ {false}
  , hrd_command_sequence_ {0}
{
}

int HRDTransceiver::do_start ()
{
  CAT_TRACE ("starting");
  QElapsedTimer startup_timer;
  startup_timer.start ();
  ScopedStartupDiagnostics diagnostics_guard {startup_diagnostics_active_};
  shutdown_in_progress_ = false;
  hrd_command_sequence_ = 0;
  hrd_diag (QStringLiteral ("startup begin server='%1' usePtt=%2 audioSource=%3")
            .arg (server_)
            .arg (use_for_ptt_ ? 1 : 0)
            .arg (static_cast<int> (audio_source_)));

  if (wrapped_) wrapped_->start (0);

  auto server_details = network_server_lookup (server_, 7809u);
  if (!hrd_)
    {
      hrd_ = new QTcpSocket {this}; // QObject takes ownership
    }
  auto host = std::get<0> (server_details);
  auto const port = std::get<1> (server_details);
  auto const normalized_server = server_.trimmed ();
  bool const localhost_requested =
    normalized_server.compare (QStringLiteral ("localhost"), Qt::CaseInsensitive) == 0
    || normalized_server.startsWith (QStringLiteral ("localhost:"), Qt::CaseInsensitive);
  if (localhost_requested && host == QHostAddress::LocalHostIPv6)
    {
      hrd_diag (QStringLiteral ("localhost resolved to IPv6 loopback; using IPv4 loopback for HRD compatibility"));
      host = QHostAddress::LocalHost;
    }
  auto const host_text = host.toString ();
  CAT_INFO ("HRD TCP connecting to" << host_text << port);
  hrd_diag (QStringLiteral ("tcp connect host=%1 port=%2 state=%3")
            .arg (host_text)
            .arg (port)
            .arg (hrd_socket_state_name (hrd_)));
  hrd_->connectToHost (host, port);
  if (!hrd_->waitForConnected (hrd_connect_timeout_ms))
    {
      CAT_ERROR ("failed to connect:" <<  hrd_->errorString ());
      hrd_diag (QStringLiteral ("tcp connect failed host=%1 port=%2 state=%3 error=%4")
                .arg (host_text)
                .arg (port)
                .arg (hrd_socket_state_name (hrd_))
                .arg (hrd_->errorString ()));
      throw error {tr ("Failed to connect to Ham Radio Deluxe\n") + hrd_->errorString ()};
    }
  CAT_INFO ("HRD TCP connected to" << host_text << port);
  hrd_diag (QStringLiteral ("tcp connected host=%1 port=%2 local=%3:%4")
            .arg (host_text)
            .arg (port)
            .arg (hrd_->localAddress ().toString ())
            .arg (hrd_->localPort ()));

  if (none == protocol_)
    {
      QString last_error;

      auto reconnect_probe_socket = [&] (QString const& protocol_name, QString const& probe_command) -> bool
      {
        if (hrd_)
          {
            hrd_->abort ();
          }

        CAT_INFO ("HRD TCP reconnecting for protocol probe" << protocol_name << probe_command << "to" << host_text << port);
        hrd_diag (QStringLiteral ("tcp reconnect for protocol %1 command='%2' host=%3 port=%4")
                  .arg (protocol_name)
                  .arg (probe_command)
                  .arg (host_text)
                  .arg (port));
        hrd_->connectToHost (host, port);
        if (!hrd_->waitForConnected (hrd_connect_timeout_ms))
          {
            last_error = tr ("Failed to connect to Ham Radio Deluxe\n") + hrd_->errorString ();
            CAT_ERROR ("failed to connect:" << hrd_->errorString ());
            hrd_diag (QStringLiteral ("tcp reconnect %1 failed host=%2 port=%3 state=%4 error=%5")
                      .arg (protocol_name)
                      .arg (host_text)
                      .arg (port)
                      .arg (hrd_socket_state_name (hrd_))
                      .arg (hrd_->errorString ()));
            return false;
          }

        hrd_diag (QStringLiteral ("tcp connected for probe %1 command='%2' host=%3 port=%4 local=%5:%6")
                  .arg (protocol_name)
                  .arg (probe_command)
                  .arg (host_text)
                  .arg (port)
                  .arg (hrd_->localAddress ().toString ())
                  .arg (hrd_->localPort ()));
        return true;
      };

      auto try_probe = [&] (auto protocol, QString const& protocol_name, QString const& probe_command, bool reuse_existing_socket) -> bool
      {
        if (!reuse_existing_socket && !reconnect_probe_socket (protocol_name, probe_command))
          {
            protocol_ = none;
            return false;
          }

        protocol_ = protocol;
        CAT_INFO ("HRD protocol probe" << protocol_name << "starting command" << probe_command);
        hrd_diag (QStringLiteral ("protocol probe %1 start command='%2'")
                  .arg (protocol_name)
                  .arg (probe_command));

        try
          {
            auto reply = send_command (probe_command, false, false);
            CAT_INFO ("HRD protocol probe" << protocol_name << "accepted:" << reply);
            hrd_diag (QStringLiteral ("protocol probe %1 accepted command='%2' reply='%3'")
                      .arg (protocol_name)
                      .arg (probe_command)
                      .arg (hrd_preview (reply)));
            return true;
          }
        catch (error const& e)
          {
            last_error = QString::fromUtf8 (e.what ());
            CAT_ERROR ("HRD protocol probe" << protocol_name << "failed:" << e.what ());
            hrd_diag (QStringLiteral ("protocol probe %1 failed command='%2': %3")
                      .arg (protocol_name)
                      .arg (probe_command)
                      .arg (last_error));
            if (hrd_)
              {
                hrd_->abort ();
              }
            protocol_ = none;
            return false;
          }
      };

      bool const protocol_detected =
        try_probe (v5, QStringLiteral ("v5"), QStringLiteral ("get context"), true)
        || try_probe (v5, QStringLiteral ("v5"), QStringLiteral ("get id"), false)
        || try_probe (v4, QStringLiteral ("v4"), QStringLiteral ("get context"), false)
        || try_probe (v4, QStringLiteral ("v4"), QStringLiteral ("get id"), false);

      if (!protocol_detected)
        {
          throw error {last_error.isEmpty ()
                         ? tr ("Ham Radio Deluxe failed protocol probe using get context or get id")
                         : last_error};
        }
    }

  QFile HRD_info_file {QDir {QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)}.absoluteFilePath ("HRD Interface Information.txt")};
  if (!HRD_info_file.open (QFile::WriteOnly | QFile::Text | QFile::Truncate))
    {
      throw error {tr ("Failed to open file \"%1\": %2.").arg (HRD_info_file.fileName ()).arg (HRD_info_file.errorString ())};
    }
  QTextStream HRD_info {&HRD_info_file};

  auto id = send_command ("get id", false, false);
  auto version = send_command ("get version", false, false);

  CAT_INFO ("Id: " << id << "Version: " << version);
  HRD_info << "Id: " << id << "\n";
  HRD_info << "Version: " << version << "\n";

  auto radios = send_command ("get radios", false, false).trimmed ().split (',', SkipEmptyParts);
  if (radios.isEmpty ())
    {
      CAT_ERROR ("no rig found");
      throw error {tr ("Ham Radio Deluxe: no rig found")};
    }

  HRD_info << "Radios:\n";
  Q_FOREACH (auto const& radio, radios)
    {
      HRD_info << "\t" << radio << "\n";
      auto entries = radio.trimmed ().split (':', SkipEmptyParts);
      if (entries.size () < 2)
        {
          CAT_ERROR ("malformed HRD radio entry:" << radio);
          hrd_diag (QStringLiteral ("malformed radio entry raw='%1'").arg (hrd_preview (radio)));
          continue;
        }
      radios_.push_back (std::forward_as_tuple (entries[0].toUInt (), entries[1]));
    }
  if (radios_.empty ())
    {
      CAT_ERROR ("no parseable rig found");
      hrd_diag (QStringLiteral ("no parseable radio entries"));
      throw error {tr ("Ham Radio Deluxe: no rig found")};
    }

  CAT_TRACE ("radios:-");
  Q_FOREACH (auto const& radio, radios_)
    {
      CAT_TRACE ("\t[" << std::get<0> (radio) << "] " << std::get<1> (radio));
    }

  auto current_radio_name = send_command ("get radio", false, false);
  HRD_info << "Current radio: " << current_radio_name << "\n";
  hrd_diag (QStringLiteral ("radio inventory count=%1 current='%2'")
            .arg (static_cast<int> (radios_.size ()))
            .arg (hrd_preview (current_radio_name)));
  if (current_radio_name.isEmpty ())
    {
      CAT_ERROR ("no rig found");
      throw error {tr ("Ham Radio Deluxe: no rig found")};
    }
  auto current_radio_iter = std::find_if (radios_.begin (), radios_.end (), [&current_radio_name] (RadioMap::value_type const& radio)
                                          {
                                            return std::get<1> (radio) == current_radio_name;
                                          });
  if (current_radio_iter == radios_.end ())
    {
      auto const normalized_current = hrd_normalized_radio_name (current_radio_name);
      current_radio_iter = std::find_if (radios_.begin (), radios_.end (), [&normalized_current] (RadioMap::value_type const& radio)
                                         {
                                           return hrd_normalized_radio_name (std::get<1> (radio)) == normalized_current;
                                         });
    }
  if (current_radio_iter == radios_.end ())
    {
      CAT_ERROR ("current HRD radio missing from inventory:" << current_radio_name);
      // Strict match (default true, restored in 1.0.184): refuse the silent fallback to
      // the first inventory radio introduced in 1.0.176 upstream (b1b2ec9). When the
      // configured radio is no longer current in HRD, fail loudly instead of driving a
      // different rig without telling the user.
      QSettings strictSettings (QStringLiteral ("Decodium"), QStringLiteral ("Decodium3"));
      strictSettings.beginGroup (QStringLiteral ("Transceiver"));
      bool const strict_radio_match = strictSettings.value (QStringLiteral ("hrdStrictRadioMatch"), true).toBool ();
      strictSettings.endGroup ();
      if (strict_radio_match)
        {
          hrd_diag (QStringLiteral ("current radio missing from inventory current='%1'; strict match enabled, aborting")
                    .arg (hrd_preview (current_radio_name)));
          throw error {tr ("Ham Radio Deluxe: rig has disappeared or changed")};
        }
      current_radio_iter = radios_.begin ();
      hrd_diag (QStringLiteral ("current radio missing from inventory current='%1'; strict match disabled, using first inventory context id=%2 name='%3'")
                .arg (hrd_preview (current_radio_name))
                .arg (std::get<0> (*current_radio_iter))
                .arg (hrd_preview (std::get<1> (*current_radio_iter))));
    }
  current_radio_ = std::get<0> (*current_radio_iter);
  hrd_diag (QStringLiteral ("selected radio context id=%1 name='%2'")
            .arg (current_radio_)
            .arg (hrd_preview (current_radio_name)));

  vfo_count_ = send_command ("get vfo-count").toUInt ();
  HRD_info << "VFO count: " << vfo_count_ << "\n";
  CAT_TRACE ("vfo count:" << vfo_count_);

  buttons_ = send_command ("get buttons").trimmed ().split (',', SkipEmptyParts).replaceInStrings (" ", "~");
  CAT_TRACE ("HRD Buttons: " << buttons_.join (", "));
  HRD_info << "Buttons: {" << buttons_.join (", ") << "}\n";

  dropdown_names_ = send_command ("get dropdowns").trimmed ().split (',', SkipEmptyParts);
  CAT_TRACE ("Dropdowns:");
  HRD_info << "Dropdowns:\n";
  Q_FOREACH (auto const& dd, dropdown_names_)
    {
      auto selections = send_command ("get dropdown-list {" + dd + "}").trimmed ().split (',');
      CAT_TRACE ("\t" << dd << ": {" << selections.join (", ") << "}");
      HRD_info << "\t" << dd << ": {" << selections.join (", ") << "}\n";
      dropdowns_[dd] = selections;
    }

  slider_names_ = send_command ("get sliders").trimmed ().split (',', SkipEmptyParts).replaceInStrings (" ", "~");
  CAT_TRACE ("Sliders:-");
  HRD_info << "Sliders:\n";
  Q_FOREACH (auto const& s, slider_names_)
    {
      auto range = send_command ("get slider-range " + current_radio_name + " " + s).trimmed ().split (',', SkipEmptyParts);
      CAT_TRACE ("\t" << s << ": {" << range.join (", ") << "}");
      HRD_info << "\t" << s << ": {" << range.join (", ") << "}\n";
      sliders_[s] = range;
    }

  // set RX VFO
  rx_A_button_ = find_button (QRegularExpression ("^(RX~A)$"));
  rx_B_button_ = find_button (QRegularExpression ("^(RX~B)$"));

  // select VFO (sometime set as well)
  vfo_A_button_ = find_button (QRegularExpression ("^(VFO~A|Main)$"));
  vfo_B_button_ = find_button (QRegularExpression ("^(VFO~B|Sub)$"));

  vfo_toggle_button_ = find_button (QRegularExpression ("^(A~/~B|VFO~A/B)$"));

  split_mode_button_ = find_button (QRegularExpression ("^(Spl~On|Spl_On|Split|Split~On)$"));
  split_off_button_ = find_button (QRegularExpression ("^(Spl~Off|Spl_Off|Split~Off)$"));

  if ((split_mode_dropdown_ = find_dropdown (QRegularExpression ("^(Split)$"))) >= 0)
    {
      split_mode_dropdown_selection_on_ = find_dropdown_selection (split_mode_dropdown_, QRegularExpression ("^(On)$"));
      split_mode_dropdown_selection_off_ = find_dropdown_selection (split_mode_dropdown_, QRegularExpression ("^(Off)$"));
    }
  else if ((receiver_dropdown_ = find_dropdown (QRegularExpression ("^Receiver$"))) >= 0)
    {
      rx_A_selection_ = find_dropdown_selection (receiver_dropdown_, QRegularExpression ("^(RX / Off)$"));
      rx_B_selection_ = find_dropdown_selection (receiver_dropdown_, QRegularExpression ("^(Mute / RX)$"));
    }

  tx_A_button_ = find_button (QRegularExpression ("^(TX~main|TX~-~A|TX~A)$"));
  tx_B_button_ = find_button (QRegularExpression ("^(TX~sub|TX~-~B|TX~B)$"));

  if ((mode_A_dropdown_ = find_dropdown (QRegularExpression ("^(Main Mode|Mode|Mode A)$"))) >= 0)
    {
      map_modes (mode_A_dropdown_, &mode_A_map_);
    }
  else
    {
      Q_ASSERT (mode_A_dropdown_ <= 0);
    }

  if ((mode_B_dropdown_ = find_dropdown (QRegularExpression ("^(Sub Mode|Mode B)$"))) >= 0)
    {
      map_modes (mode_B_dropdown_, &mode_B_map_);
    }

  // Can't do this with ^Data$ as the button name because some Kenwood
  // rigs have a "Data" button which is for turning the DSP on and off
  // so we must filter by rig name as well
  if (current_radio_name.startsWith ("IC")) // works with Icom transceivers
    {
      data_mode_toggle_button_ = find_button (QRegularExpression ("^(Data)$"));
    }

  data_mode_on_button_ = find_button (QRegularExpression ("^(DATA-ON\\(mid\\))$"));
  data_mode_off_button_ = find_button (QRegularExpression ("^(DATA-OFF)$"));

  // Some newer Icoms have a Data drop down with (Off,On,D1,D2,D3)
  // Some newer Icoms have a Data drop down with (Off,D1,D2,D3)
  // Some newer Icoms (IC-7300) have a Data drop down with
  // (Off,,D1-FIL1,D1-FIL2,D1-FIL3) the missing value counts as an
  // index value - I think it is a drop down separator line - this
  // appears to be an HRD defect and we cannot work around it
  if ((data_mode_dropdown_ = find_dropdown (QRegularExpression ("^(Data)$"))) >= 0)
    {
      data_mode_dropdown_selection_on_ = find_dropdown_selection (data_mode_dropdown_, QRegularExpression ("^(On|Data1|D1|D1-FIL1)$"));
      data_mode_dropdown_selection_off_ = find_dropdown_selection (data_mode_dropdown_, QRegularExpression ("^(Off)$"));
    }

  ptt_button_ = find_button (QRegularExpression ("^(TX)$"));
  alt_ptt_button_ = find_button (QRegularExpression ("^(TX~Alt|TX~Data)$"));

  if (vfo_count_ == 1 && ((vfo_B_button_ >= 0 && vfo_A_button_ >= 0) || vfo_toggle_button_ >= 0))
    {
      // put the rig into a known state for tricky cases like Icoms

      auto f = send_command ("get frequency").toUInt ();
      auto m = get_data_mode (lookup_mode (get_dropdown (mode_A_dropdown_), mode_A_map_));
      set_button (vfo_B_button_ >= 0 ? vfo_B_button_ : vfo_toggle_button_);
      auto fo = send_command ("get frequency").toUInt ();
      update_other_frequency (fo);
      auto mo = get_data_mode (lookup_mode (get_dropdown (mode_A_dropdown_), mode_A_map_));
      set_button (vfo_A_button_ >= 0 ? vfo_A_button_ : vfo_toggle_button_);
      if (f != fo || m != mo)
        {
          // we must have started with A/MAIN
          update_rx_frequency (f);
          update_mode (m);
        }
      else
        {
          update_rx_frequency (send_command ("get frequency").toUInt ());
          update_mode (get_data_mode (lookup_mode (get_dropdown (mode_A_dropdown_), mode_A_map_)));
        }
    }

  int resolution {0};
  auto f = send_command ("get frequency").toUInt ();
  if (f && !(f % 10))
    {
      auto test_frequency = f - f % 100 + 55;
      send_simple_command ("set frequency-hz " + QString::number (test_frequency));
      auto new_frequency = send_command ("get frequency").toUInt ();
      switch (static_cast<Radio::FrequencyDelta> (new_frequency - test_frequency))
        {
        case -5: resolution = -1; break;  // 10Hz truncated
        case 5: resolution = 1; break;    // 10Hz rounded
        case -15: resolution = -2; break; // 20Hz truncated
        case -55: resolution = -2; break; // 100Hz truncated
        case 45: resolution = 2; break;   // 100Hz rounded
        }
      if (1 == resolution)      // may be 20Hz rounded
        {
          test_frequency = f - f % 100 + 51;
          send_simple_command ("set frequency-hz " + QString::number (test_frequency));
          new_frequency = send_command ("get frequency").toUInt ();
          if (9 == static_cast<Radio::FrequencyDelta> (new_frequency - test_frequency))
            {
              resolution = 2;   // 20Hz rounded
            }
        }
      send_simple_command ("set frequency-hz " + QString::number (f));
    }
  hrd_diag (QStringLiteral ("startup complete ms=%1 protocol=%2 vfoCount=%3 buttons=%4 dropdowns=%5 sliders=%6 resolution=%7")
            .arg (startup_timer.elapsed ())
            .arg (hrd_protocol_name (protocol_))
            .arg (vfo_count_)
            .arg (buttons_.size ())
            .arg (dropdown_names_.size ())
            .arg (slider_names_.size ())
            .arg (resolution));
  return resolution;
}

void HRDTransceiver::do_stop ()
{
  if (hrd_)
    {
      hrd_->abort ();
    }
  protocol_ = none;
  current_radio_ = 0;
  shutdown_in_progress_ = false;

  if (wrapped_) wrapped_->stop ();
  CAT_TRACE ("stopped" << state () << "reversed" << reversed_);
}

void HRDTransceiver::do_prepare_shutdown ()
{
  shutdown_in_progress_ = true;
  stop_polling ();
  hrd_diag (QStringLiteral ("shutdown preparation: polling stopped, fast HRD timeouts active state=%1 protocol=%2")
            .arg (hrd_socket_state_name (hrd_))
            .arg (hrd_protocol_name (protocol_)));
}

int HRDTransceiver::find_button (QRegularExpression const& re) const
{
  return buttons_.indexOf (re);
}

int HRDTransceiver::find_dropdown (QRegularExpression const& re) const
{
  return dropdown_names_.indexOf (re);
}

std::vector<int> HRDTransceiver::find_dropdown_selection (int dropdown, QRegularExpression const& re) const
{
  std::vector<int> indices;     // this will always contain at least a
                                // -1
  auto list = dropdowns_.value (dropdown_names_.value (dropdown));
  int index {0};
  while (-1 != (index = list.lastIndexOf (re, index - 1)))
    {
      // search backwards because more specialized modes tend to be
      // later in list
      indices.push_back (index);
      if (!index)
        {
          break;
        }
    }
  return indices;
}

void HRDTransceiver::map_modes (int dropdown, ModeMap *map)
{
  // order matters here (both in the map and in the regexps)
  map->push_back (std::forward_as_tuple (CW, find_dropdown_selection (dropdown, QRegularExpression ("^(CW|CW\\(N\\)|CW-LSB|CWL)$"))));
  map->push_back (std::forward_as_tuple (CW_R, find_dropdown_selection (dropdown, QRegularExpression ("^(CW-R|CW-R\\(N\\)|CW|CW-USB|CWU)$"))));
  map->push_back (std::forward_as_tuple (LSB, find_dropdown_selection (dropdown, QRegularExpression ("^(LSB\\(N\\)|LSB)$"))));
  map->push_back (std::forward_as_tuple (USB, find_dropdown_selection (dropdown, QRegularExpression ("^(USB\\(N\\)|USB)$"))));
  map->push_back (std::forward_as_tuple (DIG_U, find_dropdown_selection (dropdown, QRegularExpression ("^(DIG|DIGU|DATA-U|PKT-U|DATA|AFSK|USER-U|USB)$"))));
  map->push_back (std::forward_as_tuple (DIG_L, find_dropdown_selection (dropdown, QRegularExpression ("^(DIG|DIGL|DATA-L|PKT-L|DATA-R|USER-L|LSB)$"))));
  map->push_back (std::forward_as_tuple (FSK, find_dropdown_selection (dropdown, QRegularExpression ("^(DIG|FSK|RTTY|RTTY-LSB)$"))));
  map->push_back (std::forward_as_tuple (FSK_R, find_dropdown_selection (dropdown, QRegularExpression ("^(DIG|FSK-R|RTTY-R|RTTY|RTTY-USB)$"))));
  map->push_back (std::forward_as_tuple (AM, find_dropdown_selection (dropdown, QRegularExpression ("^(AM|DSB|SAM|DRM)$"))));
  map->push_back (std::forward_as_tuple (FM, find_dropdown_selection (dropdown, QRegularExpression ("^(FM|FM\\(N\\)|FM-N|WFM)$"))));
  map->push_back (std::forward_as_tuple (DIG_FM, find_dropdown_selection (dropdown, QRegularExpression ("^(PKT-FM|PKT|DATA\\(FM\\)|FM)$"))));

  CAT_TRACE ("for dropdown" << dropdown_names_[dropdown]);
  std::for_each (map->begin (), map->end (), [this, dropdown] (ModeMap::value_type const& item)
                 {
                   auto const& rhs = std::get<1> (item);
                   CAT_TRACE ('\t' << std::get<0> (item) << "<->" << (rhs.size () ? dropdowns_[dropdown_names_[dropdown]][rhs.front ()] : "None"));
                 });
}

int HRDTransceiver::lookup_mode (MODE mode, ModeMap const& map) const
{
  auto it = std::find_if (map.begin (), map.end (), [mode] (ModeMap::value_type const& item) {return std::get<0> (item) == mode;});
  if (map.end () == it)
    {
      throw error {tr ("Ham Radio Deluxe: rig doesn't support mode")};
    }
  return std::get<1> (*it).front ();
}

auto HRDTransceiver::lookup_mode (int mode, ModeMap const& map) const -> MODE
{
  if (mode < 0)
    {
      return UNK;               // no mode dropdown
    }

  auto it = std::find_if (map.begin (), map.end (), [mode] (ModeMap::value_type const& item)
                          {
                            auto const& indices = std::get<1> (item);
                            return indices.cend () != std::find (indices.cbegin (), indices.cend (), mode);
                          });
  if (map.end () == it)
    {
      throw error {tr ("Ham Radio Deluxe: sent an unrecognised mode")};
    }
  return std::get<0> (*it);
}

int HRDTransceiver::get_dropdown (int dd)
{
  if (dd < 0)
    {
      return -1;                // no dropdown to interrogate
    }

  auto dd_name = dropdown_names_.value (dd);
  auto reply = send_command ("get dropdown-text {" + dd_name + "}");
  auto colon_index = reply.indexOf (':');

  if (colon_index < 0)
    {
      return -1;
    }

  Q_ASSERT (reply.left (colon_index).trimmed () == dd_name);
  return dropdowns_.value (dropdown_names_.value (dd)).indexOf (reply.mid (colon_index + 1).trimmed ());
}

void HRDTransceiver::set_dropdown (int dd, int value)
{
  auto dd_name = dropdown_names_.value (dd);
  if (value >= 0)
    {
      send_simple_command ("set dropdown " + dd_name.replace (' ', '~') + ' ' + dropdowns_.value (dd_name).value (value).replace (' ', '~') + ' ' + QString::number (value));
    }
  else
    {
      CAT_ERROR ("item" << value << "not found in" << dd_name);
      throw error {tr ("Ham Radio Deluxe: item not found in %1 dropdown list").arg (dd_name)};
    }
}

void HRDTransceiver::do_ptt (bool on)
{
  CAT_TRACE (on);
  if (use_for_ptt_)
    {
      if (alt_ptt_button_ >= 0 && TransceiverFactory::TX_audio_source_rear == audio_source_)
        {
          set_button (alt_ptt_button_, on);
        }
      else if (ptt_button_ >= 0)
        {
          set_button (ptt_button_, on);
        }
      // else
      // allow for pathological HRD rig interfaces that don't do
      // PTT by simply not even trying
    }
  else
    {
      Q_ASSERT (wrapped_);
      TransceiverState new_state {wrapped_->state ()};
      new_state.ptt (on);
      wrapped_->set (new_state, 0);
    }
  update_PTT (on);
}

void HRDTransceiver::set_button (int button_index, bool checked)
{
  if (button_index >= 0)
    {
      send_simple_command ("set button-select " + buttons_.value (button_index) + (checked ? " 1" : " 0"));
      if (button_index == rx_A_button_ || button_index == rx_B_button_)
        {
          QThread::msleep (yaesu_delay);
        }
    }
  else
    {
      CAT_ERROR ("invalid button");
      throw error {tr ("Ham Radio Deluxe: button not available")};
    }
}

void HRDTransceiver::set_data_mode (MODE m)
{
  if (data_mode_toggle_button_ >= 0)
    {
      switch (m)
        {
        case DIG_U:
        case DIG_L:
        case DIG_FM:
          set_button (data_mode_toggle_button_, true);
          break;
        default:
          set_button (data_mode_toggle_button_, false);
          break;
        }
    }
  else if (data_mode_on_button_ >= 0 && data_mode_off_button_ >= 0)
    {
      switch (m)
        {
        case DIG_U:
        case DIG_L:
        case DIG_FM:
          set_button (data_mode_on_button_, true);
          break;
        default:
          set_button (data_mode_off_button_, true);
          break;
        }
    }
  else if (data_mode_dropdown_ >= 0
      && data_mode_dropdown_selection_off_.size ()
      && data_mode_dropdown_selection_on_.size ())
    {
      switch (m)
        {
        case DIG_U:
        case DIG_L:
        case DIG_FM:
          set_dropdown (data_mode_dropdown_, data_mode_dropdown_selection_on_.front ());
          break;
        default:
          set_dropdown (data_mode_dropdown_, data_mode_dropdown_selection_off_.front ());
          break;
        }
    }
}

auto HRDTransceiver::get_data_mode (MODE m) -> MODE
{
  if (data_mode_dropdown_ >= 0
      && data_mode_dropdown_selection_off_.size ())
    {
      auto selection = get_dropdown (data_mode_dropdown_);
      // can't check for on here as there may be multiple on values so
      // we must rely on the initial parse finding valid on values
      if (selection >= 0 && selection != data_mode_dropdown_selection_off_.front ())
        {
          switch (m)
            {
            case USB: m = DIG_U; break;
            case LSB: m = DIG_L; break;
            case FM: m = DIG_FM; break;
            default: break;
            }
        }
    }
  return m;
}

void HRDTransceiver::do_frequency (Frequency f, MODE m, bool /*no_ignore*/)
{
  CAT_TRACE (f << "reversed" << reversed_);
  if (UNK != m)
    {
      do_mode (m);
    }
  auto fo_string = QString::number (f);
  if (vfo_count_ > 1 && reversed_)
    {
      auto frequencies = send_command ("get frequencies").trimmed ().split ('-', SkipEmptyParts);
      send_simple_command ("set frequencies-hz " + QString::number (frequencies[0].toUInt ()) + ' ' + fo_string);
    }
  else
    {
      send_simple_command ("set frequency-hz " + QString::number (f));
    }
  update_rx_frequency (f);
}

void HRDTransceiver::do_tx_frequency (Frequency tx, MODE mode, bool /*no_ignore*/)
{
  CAT_TRACE (tx << "reversed" << reversed_);

  // re-check if reversed VFOs
  bool rx_A {true};
  bool rx_B {false};
  if (receiver_dropdown_ >= 0 && rx_A_selection_.size ())
    {
      auto selection = get_dropdown (receiver_dropdown_);
      rx_A = selection == rx_A_selection_.front ();
      if (!rx_A && rx_B_selection_.size ())
        {
          rx_B = selection == rx_B_selection_.front ();
        }
    }
  else if (vfo_B_button_ >= 0 || rx_B_button_ >= 0)
    {
      rx_A = is_button_checked (rx_A_button_ >= 0 ? rx_A_button_ : vfo_A_button_);
      if (!rx_A)
        {
          rx_B = is_button_checked (rx_B_button_ >= 0 ? rx_B_button_ : vfo_B_button_);
        }
    }
  reversed_ = rx_B;

  bool split {tx != 0};
  if (split)
    {
      if (mode != UNK)
        {
          if (!reversed_ && mode_B_dropdown_ >= 0)
            {
              set_dropdown (mode_B_dropdown_, lookup_mode (mode, mode_B_map_));
            }
          else if (reversed_ && mode_B_dropdown_ >= 0)
            {
              set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
            }
          else
            {
              Q_ASSERT (mode_A_dropdown_ >= 0 && ((vfo_A_button_ >=0 && vfo_B_button_ >=0) || vfo_toggle_button_ >= 0));

              if (rx_B_button_ >= 0)
                {
                  set_button (reversed_ ? rx_A_button_ : rx_B_button_);
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_data_mode (mode);
                  set_button (reversed_ ? rx_B_button_ : rx_A_button_);
                }
              else if (receiver_dropdown_ >= 0
                       && rx_A_selection_.size () && rx_B_selection_.size ())
                {
                  set_dropdown (receiver_dropdown_, (reversed_ ? rx_A_selection_ : rx_B_selection_).front ());
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_data_mode (mode);
                  set_dropdown (receiver_dropdown_, (reversed_ ? rx_B_selection_ : rx_A_selection_).front ());
                }
              else if (vfo_count_ > 1 && ((vfo_A_button_ >=0 && vfo_B_button_ >=0) || vfo_toggle_button_ >= 0))
                {
                  set_button (vfo_A_button_ >= 0 ? (reversed_ ? vfo_A_button_ : vfo_B_button_) : vfo_toggle_button_);
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_data_mode (mode);
                  set_button (vfo_A_button_ >= 0 ? (reversed_ ? vfo_B_button_ : vfo_A_button_) : vfo_toggle_button_);
                }
              // else Tx VFO mode gets set with frequency below or we
              // don't have a way of setting it so we assume it is
              // always the same as the Rx VFO mode
            }
        }

      auto fo_string = QString::number (tx);
      if (reversed_)
        {
          Q_ASSERT (vfo_count_ > 1);

          auto frequencies = send_command ("get frequencies").trimmed ().split ('-', SkipEmptyParts);
          send_simple_command ("set frequencies-hz " + fo_string + ' ' + QString::number (frequencies[1].toUInt ()));
        }
      else
        {
          if (vfo_count_ > 1)
            {
              auto frequencies = send_command ("get frequencies").trimmed ().split ('-', SkipEmptyParts);
              send_simple_command ("set frequencies-hz " + QString::number (frequencies[0].toUInt ()) + ' ' + fo_string);
            }
          else if ((vfo_B_button_ >= 0 && vfo_A_button_ >= 0) || vfo_toggle_button_ >= 0)
            {
              // we rationalise the modes here as well as the frequencies
              set_button (vfo_B_button_ >= 0 ? vfo_B_button_ : vfo_toggle_button_);
              send_simple_command ("set frequency-hz " + fo_string);
              if (mode != UNK && mode_B_dropdown_ < 0)
                {
                  // do this here rather than later so we only
                  // toggle/switch VFOs once
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_data_mode (mode);
                }
              set_button (vfo_A_button_ >= 0 ? vfo_A_button_ : vfo_toggle_button_);
            }
        }
    }
  update_other_frequency (tx);

  if (split_mode_button_ >= 0)
    {
      if (split_off_button_ >= 0 && !split)
        {
          set_button (split_off_button_);
        }
      else
        {
          set_button (split_mode_button_, split);
        }
    }
  else if (split_mode_dropdown_ >= 0
           && split_mode_dropdown_selection_off_.size ()
           && split_mode_dropdown_selection_on_.size ())
    {
      set_dropdown (split_mode_dropdown_, split ? split_mode_dropdown_selection_on_.front () : split_mode_dropdown_selection_off_.front ());
    }
  else if (vfo_A_button_ >= 0 && vfo_B_button_ >= 0 && tx_A_button_ >= 0 && tx_B_button_ >= 0)
    {
      if (split)
        {
          if (reversed_ != is_button_checked (tx_A_button_))
            {
              if (rx_A_button_ >= 0 && rx_B_button_ >= 0)
                {
                  set_button (reversed_ ? rx_B_button_ : rx_A_button_);
                }
              else if (receiver_dropdown_ >= 0
                       && rx_A_selection_.size () && rx_B_selection_.size ())
                {
                  set_dropdown (receiver_dropdown_, (reversed_ ? rx_B_selection_ : rx_A_selection_).front ());
                }
              else
                {
                  set_button (reversed_ ? vfo_B_button_ : vfo_A_button_);
                }
              set_button (reversed_ ? tx_A_button_ : tx_B_button_);
            }
        }
      else
        {
          if (reversed_ != is_button_checked (tx_B_button_))
            {
              if (rx_A_button_ >= 0 && rx_B_button_ >= 0)
                {
                  set_button (reversed_ ? rx_B_button_ : rx_A_button_);
                }
              else if (receiver_dropdown_ >= 0
                       && rx_A_selection_.size () && rx_B_selection_.size ())
                {
                  set_dropdown (receiver_dropdown_, (reversed_ ? rx_B_selection_ : rx_A_selection_).front ());
                }
              else
                {
                  set_button (reversed_ ? vfo_B_button_ : vfo_A_button_);
                }
              set_button (reversed_ ? tx_B_button_ : tx_A_button_);
            }
        }
    }
  update_split (split);
}

void HRDTransceiver::do_mode (MODE mode)
{
  CAT_TRACE (mode);
  if (reversed_ && mode_B_dropdown_ >= 0)
    {
      set_dropdown (mode_B_dropdown_, lookup_mode (mode, mode_B_map_));
    }
  else
    {
      set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
    }
  if (mode != UNK && state ().split ()) // rationalise mode if split
    {
      if (reversed_)
        {
          if (mode_B_dropdown_ >= 0)
            {
              set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
            }
          else
            {
              Q_ASSERT ((vfo_B_button_ >= 0 && vfo_A_button_ >= 0) || vfo_toggle_button_ >= 0);

              if (rx_B_button_ >= 0)
                {
                  set_button (rx_A_button_);
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_button (rx_B_button_);
                }
              else if (receiver_dropdown_ >= 0
                       && rx_A_selection_.size () && rx_B_selection_.size ())
                {
                  set_dropdown (receiver_dropdown_, rx_A_selection_.front ());
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_dropdown (receiver_dropdown_, rx_B_selection_.front ());
                }
              else if (vfo_count_ > 1 && ((vfo_A_button_ >=0 && vfo_B_button_ >=0) || vfo_toggle_button_ >= 0))
                {
                  set_button (vfo_A_button_ >= 0 ? vfo_A_button_ : vfo_toggle_button_);
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_button (vfo_B_button_ >= 0 ? vfo_B_button_ : vfo_toggle_button_);
                }
              // else Tx VFO mode gets set when Tx VFO frequency is
              // set

              if ( tx_A_button_ >= 0)
                {
                  set_button (tx_A_button_);
                }
            }
        }
      else
        {
          if (mode_B_dropdown_ >= 0)
            {
              set_dropdown (mode_B_dropdown_, lookup_mode (mode, mode_B_map_));
            }
          else
            {
              Q_ASSERT ((vfo_B_button_ >= 0 && vfo_A_button_ >= 0) || vfo_toggle_button_ >= 0);

              if (rx_B_button_ >= 0)
                {
                  set_button (rx_B_button_);
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_button (rx_A_button_);
                }
              else if (receiver_dropdown_ >= 0
                       && rx_A_selection_.size () && rx_B_selection_.size ())
                {
                  set_dropdown (receiver_dropdown_, rx_B_selection_.front ());
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_dropdown (receiver_dropdown_, rx_A_selection_.front ());
                }
              else if (vfo_count_ > 1 && ((vfo_A_button_ >=0 && vfo_B_button_ >=0) || vfo_toggle_button_ >= 0))
                {
                  set_button (vfo_B_button_ >= 0 ? vfo_B_button_ : vfo_toggle_button_);
                  set_dropdown (mode_A_dropdown_, lookup_mode (mode, mode_A_map_));
                  set_button (vfo_A_button_ >= 0 ? vfo_A_button_ : vfo_toggle_button_);
                }
              // else Tx VFO mode gets set when Tx VFO frequency is
              // set

              if ( tx_B_button_ >= 0)
                {
                  set_button (tx_B_button_);
                }
            }
        }
    }
  set_data_mode (mode);
  update_mode (mode);
}

bool HRDTransceiver::is_button_checked (int button_index)
{
  if (button_index < 0)
    {
      return false;
    }

  auto reply = send_command ("get button-select " + buttons_.value (button_index));
  if ("1" != reply && "0" != reply)
    {
      CAT_ERROR ("bad response");
      throw error {tr ("Ham Radio Deluxe didn't respond as expected")};
    }
  return "1" == reply;
}

void HRDTransceiver::do_poll ()
{
  CAT_TRACE ("+++++++ poll dump +++++++");
  CAT_TRACE ("reversed:" << reversed_);
  is_button_checked (vfo_A_button_);
  is_button_checked (vfo_B_button_);
  is_button_checked (vfo_toggle_button_);
  is_button_checked (split_mode_button_);
  is_button_checked (split_off_button_);
  is_button_checked (rx_A_button_);
  is_button_checked (rx_B_button_);
  get_dropdown (receiver_dropdown_);
  is_button_checked (tx_A_button_);
  is_button_checked (tx_B_button_);
  is_button_checked (ptt_button_);
  is_button_checked (alt_ptt_button_);
  get_dropdown (mode_A_dropdown_);
  get_dropdown (mode_B_dropdown_);
  is_button_checked (data_mode_toggle_button_);
  is_button_checked (data_mode_on_button_);
  is_button_checked (data_mode_off_button_);
  if (data_mode_dropdown_ >=0
      && data_mode_dropdown_selection_off_.size ()
      && data_mode_dropdown_selection_on_.size ())
    {
      get_dropdown (data_mode_dropdown_);
    }
  if (!split_mode_dropdown_write_only_)
    {
      get_dropdown (split_mode_dropdown_);
    }
  CAT_TRACE ("------- poll dump -------");

  if (split_off_button_ >= 0)
    {
      // we are probably dealing with an Icom and have to guess SPLIT mode :(
    }
  else if (split_mode_button_ >= 0 && !(tx_A_button_ >= 0 && tx_B_button_ >= 0))
    {
      update_split (is_button_checked (split_mode_button_));
    }
  else if (split_mode_dropdown_ >= 0)
    {
      if (!split_mode_dropdown_write_only_)
        {
          auto selection = get_dropdown (split_mode_dropdown_);
          if (selection >= 0
              && split_mode_dropdown_selection_off_.size ())
            {
              update_split (selection == split_mode_dropdown_selection_on_.front ());
            }
          else
            {
              // leave split alone as we can't query it - it should be
              // correct so long as rig or HRD haven't been changed
              split_mode_dropdown_write_only_ = true;
            }
        }
    }
  else if (vfo_A_button_ >= 0 && vfo_B_button_ >= 0 && tx_A_button_ >= 0 && tx_B_button_ >= 0)
    {
      bool rx_A {true};         // no Rx taken as not reversed
      bool rx_B {false};

      auto tx_A = is_button_checked (tx_A_button_);

      // some rigs have dual Rx, we take VFO A/MAIN receiving as
      // normal and only say reversed when only VFO B/SUB is active
      // i.e. VFO A/MAIN muted VFO B/SUB active
      if (receiver_dropdown_ >= 0 && rx_A_selection_.size ())
        {
          auto selection = get_dropdown (receiver_dropdown_);
          rx_A = selection == rx_A_selection_.front ();
          if (!rx_A && rx_B_selection_.size ())
            {
              rx_B = selection == rx_B_selection_.front ();
            }
        }
      else if (vfo_B_button_ >= 0 || rx_B_button_ >= 0)
        {
          rx_A = is_button_checked (rx_A_button_ >= 0 ? rx_A_button_ : vfo_A_button_);
          if (!rx_A)
            {
              rx_B = is_button_checked (rx_B_button_ >= 0 ? rx_B_button_ : vfo_B_button_);
            }
        }

      update_split (rx_B == tx_A);
      reversed_ = rx_B;
    }

  if (vfo_count_ > 1)
    {
      auto frequencies = send_command ("get frequencies").trimmed ().split ('-', SkipEmptyParts);
      update_rx_frequency (frequencies[reversed_ ? 1 : 0].toUInt ());
      update_other_frequency (frequencies[reversed_ ? 0 : 1].toUInt ());
    }
  else
    {
      // read frequency is unreliable on single VFO addressing rigs
      // while transmitting
      if (!state ().ptt ())
        {
          update_rx_frequency (send_command ("get frequency").toUInt ());
        }
    }

  // read mode is unreliable on single VFO addressing rigs while
  // transmitting
  if (vfo_count_ > 1 || !state ().ptt ())
    {
      update_mode (get_data_mode (lookup_mode (get_dropdown (mode_A_dropdown_), mode_A_map_)));
    }
}

QString HRDTransceiver::send_command (QString const& cmd, bool prepend_context, bool recurse)
{
  if (!hrd_) return QString {};

  quint64 const sequence = ++hrd_command_sequence_;
  QElapsedTimer command_timer;
  command_timer.start ();
  QString result;

  if (current_radio_ && prepend_context && vfo_count_ < 2)
    {
      // required on some radios because commands don't get executed
      // correctly otherwise (ICOM for example)
      QThread::msleep (50);
   }

  if (!recurse && prepend_context)
    {
      auto radio_name = send_command ("get radio", current_radio_, true);
      auto radio_iter = std::find_if (radios_.begin (), radios_.end (), [&radio_name] (RadioMap::value_type const& radio)
                                      {
                                        return std::get<1> (radio) == radio_name;
                                      });
      if (radio_iter == radios_.end ())
        {
          CAT_TRACE ("rig disappeared or changed");
          throw error {tr ("Ham Radio Deluxe: rig has disappeared or changed")};
        }

      if (0u == current_radio_ || std::get<0> (*radio_iter) != current_radio_)
        {
          current_radio_ = std::get<0> (*radio_iter);
        }
    }

  auto context = '[' + QString::number (current_radio_) + "] ";
  auto const wire_command = prepend_context ? context + cmd : cmd;
  bool const log_command = startup_diagnostics_active_;

  if (QTcpSocket::ConnectedState != hrd_->state ())
    {
      CAT_ERROR (cmd << "failed" << hrd_->errorString ());
      hrd_diag (QStringLiteral ("#%1 socket not connected state=%2 error=%3 cmd='%4'")
                .arg (QString::number (sequence))
                .arg (hrd_socket_state_name (hrd_))
                .arg (hrd_->errorString ())
                .arg (hrd_preview (wire_command)));
      throw error {
        tr ("Ham Radio Deluxe send command \"%1\" failed %2\n")
          .arg (cmd)
          .arg (hrd_->errorString ())
          };
    }

  auto const stale_bytes_available = hrd_->bytesAvailable ();
  if (stale_bytes_available > 0)
    {
      auto const stale = hrd_->readAll ();
      hrd_diag (QStringLiteral ("#%1 discarded stale HRD bytes before write proto=%2 pendingBefore=%3 discarded=%4 hex=%5 cmd='%6'")
                .arg (QString::number (sequence))
                .arg (hrd_protocol_name (protocol_))
                .arg (stale_bytes_available)
                .arg (stale.size ())
                .arg (hrd_hex_preview (stale))
                .arg (hrd_preview (wire_command)));
    }

  if (log_command)
    {
      hrd_diag (QStringLiteral ("#%1 >>> proto=%2 recurse=%3 prepend=%4 pending=%5 cmd='%6'")
                .arg (QString::number (sequence))
                .arg (hrd_protocol_name (protocol_))
                .arg (recurse ? 1 : 0)
                .arg (prepend_context ? 1 : 0)
                .arg (hrd_->bytesAvailable ())
                .arg (hrd_preview (wire_command)));
    }

  if (v4 == protocol_)
    {
      auto message = (wire_command + "\r").toLocal8Bit ();
      if (!write_to_port (message.constData (), message.size ()))
        {
          CAT_ERROR ("failed to write command" << cmd << "to HRD");
          hrd_diag (QStringLiteral ("#%1 write failed proto=v4 state=%2 error=%3 cmd='%4'")
                    .arg (QString::number (sequence))
                    .arg (hrd_socket_state_name (hrd_))
                    .arg (hrd_->errorString ())
                    .arg (hrd_preview (wire_command)));
          throw error {
            tr ("Ham Radio Deluxe: failed to write command \"%1\"")
              .arg (cmd)
              };
        }
      if (log_command)
        {
          hrd_diag (QStringLiteral ("#%1 wrote proto=v4 bytes=%2")
                    .arg (QString::number (sequence))
                    .arg (message.size ()));
        }
    }
  else
    {
      QScopedPointer<HRDMessage> message {new (wire_command) HRDMessage};
      if (!write_to_port (reinterpret_cast<char const *> (message.data ()), message->size_))
        {
          CAT_ERROR ("failed to write command" << cmd << "to HRD");
          hrd_diag (QStringLiteral ("#%1 write failed proto=v5 bytes=%2 state=%3 error=%4 cmd='%5'")
                    .arg (QString::number (sequence))
                    .arg (message->size_)
                    .arg (hrd_socket_state_name (hrd_))
                    .arg (hrd_->errorString ())
                    .arg (hrd_preview (wire_command)));
          throw error {
            tr ("Ham Radio Deluxe: failed to write command \"%1\"")
              .arg (cmd)
            };
        }
      if (log_command)
        {
          hrd_diag (QStringLiteral ("#%1 wrote proto=v5 bytes=%2")
                    .arg (QString::number (sequence))
                    .arg (message->size_));
        }
    }

  auto buffer = read_reply (cmd, sequence);
  if (v4 == protocol_)
    {
      while (!buffer.contains ('\r') && !buffer.contains ('\n') && buffer.size () < hrd_max_reply_bytes)
        {
          if (log_command)
            {
              hrd_diag (QStringLiteral ("#%1 partial v4 reply bytes=%2")
                        .arg (QString::number (sequence))
                        .arg (buffer.size ()));
            }
          buffer += read_reply (cmd, sequence);
        }
      if (buffer.size () >= hrd_max_reply_bytes)
        {
          CAT_ERROR (cmd << "reply too large");
          hrd_diag (QStringLiteral ("#%1 v4 reply too large bytes=%2 cmd='%3'")
                    .arg (QString::number (sequence))
                    .arg (buffer.size ())
                    .arg (hrd_preview (wire_command)));
          throw error {
            tr ("Ham Radio Deluxe reply to command \"%1\" is too large")
              .arg (cmd)
              };
        }
      result = QString::fromLocal8Bit (buffer).trimmed ();
    }
  else
    {
      qsizetype const header_size = static_cast<qsizetype> (offsetof (HRDMessage, payload_));
      while (buffer.size () < header_size)
        {
          if (log_command)
            {
              hrd_diag (QStringLiteral ("#%1 partial v5 header bytes=%2/%3 hex=%4")
                        .arg (QString::number (sequence))
                        .arg (buffer.size ())
                        .arg (header_size)
                        .arg (hrd_hex_preview (buffer)));
            }
          buffer += read_reply (cmd, sequence);
        }

      HRDMessage const * reply {new (buffer) HRDMessage};
      if (reply->magic_1_value_ != reply->magic_1_ || reply->magic_2_value_ != reply->magic_2_)
        {
          CAT_ERROR (cmd << "invalid reply header");
          hrd_diag (QStringLiteral ("#%1 invalid v5 header bytes=%2 hex=%3 cmd='%4'")
                    .arg (QString::number (sequence))
                    .arg (buffer.size ())
                    .arg (hrd_hex_preview (buffer))
                    .arg (hrd_preview (wire_command)));
          throw error {
            tr ("Ham Radio Deluxe sent an invalid reply to our command \"%1\"")
              .arg (cmd)
              };
        }

      if (reply->size_ < static_cast<quint32> (header_size)
          || reply->size_ > static_cast<quint32> (hrd_max_reply_bytes))
        {
          CAT_ERROR (cmd << "invalid reply size" << reply->size_);
          hrd_diag (QStringLiteral ("#%1 invalid v5 size=%2 header=%3 max=%4 cmd='%5'")
                    .arg (QString::number (sequence))
                    .arg (reply->size_)
                    .arg (header_size)
                    .arg (hrd_max_reply_bytes)
                    .arg (hrd_preview (wire_command)));
          throw error {
            tr ("Ham Radio Deluxe sent an invalid reply size to our command \"%1\"")
              .arg (cmd)
              };
        }

      // keep reading until expected size arrives
      while (buffer.size () < static_cast<qsizetype> (reply->size_))
        {
          if (log_command)
            {
              hrd_diag (QStringLiteral ("#%1 partial v5 packet bytes=%2/%3")
                        .arg (QString::number (sequence))
                        .arg (buffer.size ())
                        .arg (reply->size_));
            }
          buffer += read_reply (cmd, sequence);
          reply = new (buffer) HRDMessage;
        }

      result = QString {reply->payload_}; // this is not a memory leak (honest!)
    }
  CAT_TRACE (cmd << " ->" << result);
  if (log_command || command_timer.elapsed () >= 250)
    {
      hrd_diag (QStringLiteral ("#%1 <<< ok ms=%2 proto=%3 bytes=%4 result='%5'")
                .arg (QString::number (sequence))
                .arg (command_timer.elapsed ())
                .arg (hrd_protocol_name (protocol_))
                .arg (buffer.size ())
                .arg (hrd_preview (result)));
    }
  return result;
}

bool HRDTransceiver::write_to_port (char const * data, qint64 length)
{
  qint64 total_bytes_sent {0};
  while (total_bytes_sent < length)
    {
      auto bytes_sent = hrd_->write (data + total_bytes_sent, length - total_bytes_sent);
      int const timeout_ms = shutdown_in_progress_ ? hrd_shutdown_write_timeout_ms
                                                   : hrd_write_timeout_ms;
      if (bytes_sent < 0 || !hrd_->waitForBytesWritten (timeout_ms))
        {
          return false;
        }

      total_bytes_sent += bytes_sent;
    }
  return true;
}

QByteArray HRDTransceiver::read_reply (QString const& cmd, quint64 sequence)
{
  // HRD can accept the TCP socket before the remote protocol has a reply
  // ready. JTDX/WSJT-X tolerate this on slower Windows installs; keep the
  // initial protocol probe patient enough to match that behavior without
  // making ordinary CAT polling sluggish.
  QElapsedTimer total_timer;
  total_timer.start ();
  bool const startup_probe = is_startup_probe (cmd);
  int const timeout_ms = startup_probe ? hrd_probe_reply_timeout_ms
                                       : (shutdown_in_progress_ ? hrd_shutdown_command_reply_timeout_ms
                                                               : (startup_diagnostics_active_ ? hrd_startup_command_reply_timeout_ms
                                                                                              : hrd_command_reply_timeout_ms));
  unsigned retries = startup_probe ? hrd_probe_reply_retries
                                   : (shutdown_in_progress_ ? hrd_shutdown_command_reply_retries
                                                           : (startup_diagnostics_active_ ? hrd_startup_command_reply_retries
                                                                                          : hrd_command_reply_retries));
  bool replied {false};
  unsigned attempt {0};
  while (!replied && retries--)
    {
      ++attempt;
      replied = hrd_->waitForReadyRead (timeout_ms);
      if (!replied && hrd_->error () != QAbstractSocket::SocketTimeoutError)
        {
          CAT_ERROR (cmd << "failed to reply" << hrd_->errorString ());
          hrd_diag (QStringLiteral ("#%1 read failed attempt=%2 state=%3 error=%4 cmd='%5'")
                    .arg (QString::number (sequence))
                    .arg (attempt)
                    .arg (hrd_socket_state_name (hrd_))
                    .arg (hrd_->errorString ())
                    .arg (hrd_preview (cmd)));
          throw error {
            tr ("Ham Radio Deluxe failed to reply to command \"%1\" %2\n")
              .arg (cmd)
              .arg (hrd_->errorString ())
              };
        }
      if (!replied && startup_probe)
        {
          auto const elapsed_ms = total_timer.elapsed ();
          auto const pending_bytes = hrd_ ? hrd_->bytesAvailable () : qint64 {0};
          if (0 == pending_bytes && elapsed_ms >= hrd_startup_protocol_silent_timeout_ms)
            {
              CAT_ERROR ("HRD TCP accepted, protocol silent, reconnecting");
              hrd_diag (QStringLiteral ("#%1 TCP accepted, protocol silent, reconnecting command='%2' attempt=%3 elapsedMs=%4 timeoutMs=%5 state=%6 protocol=%7")
                        .arg (QString::number (sequence))
                        .arg (hrd_preview (cmd))
                        .arg (attempt)
                        .arg (elapsed_ms)
                        .arg (timeout_ms)
                        .arg (hrd_socket_state_name (hrd_))
                        .arg (hrd_protocol_name (protocol_)));
              if (hrd_)
                {
                  hrd_->abort ();
                }
              throw error {
                tr ("Ham Radio Deluxe TCP accepted, protocol silent while probing command \"%1\"")
                  .arg (cmd)
                  };
            }
        }
      if (!replied && startup_diagnostics_active_)
        {
          auto const elapsed_ms = total_timer.elapsed ();
          auto const pending_bytes = hrd_ ? hrd_->bytesAvailable () : qint64 {0};
          hrd_diag (QStringLiteral ("#%1 startup blocked command='%2' attempt=%3 timeoutMs=%4 elapsedMs=%5 state=%6 pendingBytes=%7 startupProbe=%8")
                    .arg (QString::number (sequence))
                    .arg (hrd_preview (cmd))
                    .arg (attempt)
                    .arg (timeout_ms)
                    .arg (elapsed_ms)
                    .arg (hrd_socket_state_name (hrd_))
                    .arg (pending_bytes)
                    .arg (startup_probe ? 1 : 0));
          hrd_diag (QStringLiteral ("#%1 read timeout attempt=%2 timeoutMs=%3 elapsedMs=%4 startupProbe=%5 cmd='%6'")
                    .arg (QString::number (sequence))
                    .arg (attempt)
                    .arg (timeout_ms)
                    .arg (elapsed_ms)
                    .arg (startup_probe ? 1 : 0)
                    .arg (hrd_preview (cmd)));
        }
    }
  if (!replied)
    {
      CAT_ERROR (cmd << "retries exhausted, timeoutMs=" << timeout_ms);
      if (startup_diagnostics_active_)
        {
          hrd_diag (QStringLiteral ("#%1 startup blocked command final='%2' attempts=%3 timeoutMs=%4 totalMs=%5 state=%6 pendingBytes=%7 startupProbe=%8")
                    .arg (QString::number (sequence))
                    .arg (hrd_preview (cmd))
                    .arg (attempt)
                    .arg (timeout_ms)
                    .arg (total_timer.elapsed ())
                    .arg (hrd_socket_state_name (hrd_))
                    .arg (hrd_ ? hrd_->bytesAvailable () : qint64 {0})
                    .arg (startup_probe ? 1 : 0));
        }
      hrd_diag (QStringLiteral ("#%1 read retries exhausted attempts=%2 timeoutMs=%3 elapsedMs=%4 startupProbe=%5 state=%6 cmd='%7'")
                .arg (QString::number (sequence))
                .arg (attempt)
                .arg (timeout_ms)
                .arg (total_timer.elapsed ())
                .arg (startup_probe ? 1 : 0)
                .arg (hrd_socket_state_name (hrd_))
                .arg (hrd_preview (cmd)));
      throw error {
        tr ("Ham Radio Deluxe retries exhausted sending command \"%1\"")
          .arg (cmd)
          };
    }
  auto const chunk = hrd_->readAll ();
  if (startup_diagnostics_active_ || total_timer.elapsed () >= 250)
    {
      hrd_diag (QStringLiteral ("#%1 read chunk bytes=%2 waitMs=%3 hex=%4")
                .arg (QString::number (sequence))
                .arg (chunk.size ())
                .arg (total_timer.elapsed ())
                .arg (hrd_hex_preview (chunk)));
    }
  return chunk;
}

void HRDTransceiver::send_simple_command (QString const& command)
{
  if ("OK" != send_command (command))
    {
      CAT_ERROR (command << "unexpected response");
      throw error {
        tr ("Ham Radio Deluxe didn't respond to command \"%1\" as expected")
          .arg (command)
          };
    }
}
