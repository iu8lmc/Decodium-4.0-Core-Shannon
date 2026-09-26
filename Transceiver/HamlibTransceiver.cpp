#include "HamlibTransceiver.hpp"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <QByteArray>
#include <QString>
#include <QStandardPaths>
#include <QFile>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <hamlib/rig.h>
#include "QmxTelemetry.hpp"
#include "src/radio/DecodiumProfileSettings.h"
#include "pimpl_impl.hpp"
#include "moc_HamlibTransceiver.cpp"

#if HAVE_HAMLIB_OLD_CACHING
#define HAMLIB_CACHE_ALL CACHE_ALL
#endif

namespace
{
  unsigned int configured_qmx_swr_threshold_hundredths ()
  {
    double const threshold = qBound (
        1.5,
        decodium::profiledSettingsValue (
            QString {}, QStringLiteral ("SWRStopThreshold"), 2.5).toDouble (),
        5.0);
    return static_cast<unsigned int> (std::lround (threshold * 100.0));
  }

#if defined (WIN32)
  QString hamlib_windows_port_path (QString const& port)
  {
    auto const trimmed = port.trimmed ();
    if (trimmed.isEmpty ())
      {
        return trimmed;
      }
    if (trimmed.startsWith (QStringLiteral ("\\\\.\\")))
      {
        return trimmed;
      }
    return QStringLiteral ("\\\\.\\") + trimmed;
  }
#endif

  // Unfortunately bandwidth is conflated  with mode, this is probably
  // because Icom do  the same. So we have to  care about bandwidth if
  // we want  to set  mode otherwise we  will end up  setting unwanted
  // bandwidths every time we change mode.  The best we can do via the
  // Hamlib API is to request the  normal option for the mode and hope
  // that an appropriate filter is selected.  Also ensure that mode is
  // only set is absolutely necessary.  On Icoms (and probably others)
  // the filter is  selected by number without checking  the actual BW
  // so unless the  "normal" defaults are set on the  rig we won't get
  // desirable results.
  //
  // As  an ultimate  workaround make  sure  the user  always has  the
  // option to skip mode setting altogether.

  // callback function that receives transceiver capabilities from the
  // hamlib libraries
  int register_callback (rig_model_t rig_model, void * callback_data)
  {
    TransceiverFactory::Transceivers * rigs = reinterpret_cast<TransceiverFactory::Transceivers *> (callback_data);
    // We can't use this one because it is only for testing Hamlib and
    // would confuse users, possibly causing operating on the wrong
    // frequency!
#ifdef RIG_MODEL_DUMMY_NOVFO
    if (RIG_MODEL_DUMMY_NOVFO == rig_model)
      {
        return 1;
      }
#endif

    QString key;
    if (RIG_MODEL_DUMMY == rig_model)
      {
        key = TransceiverFactory::basic_transceiver_name_;
      }
    else
      {
        key = QString::fromLatin1 (rig_get_caps_cptr (rig_model, RIG_CAPS_MFG_NAME_CPTR)).trimmed ()
          + ' '+ QString::fromLatin1 (rig_get_caps_cptr (rig_model, RIG_CAPS_MODEL_NAME_CPTR)).trimmed ()
          // + ' '+ QString::fromLatin1 (rig_get_caps_cptr (rig_model, RIG_CAPS_VERSION)).trimmed ()
          // + " (" + QString::fromLatin1 (rig_get_caps_cptr (rig_model, RIG_CAPS_STATUS)).trimmed () + ')'
          ;
      }

    auto port_type = TransceiverFactory::Capabilities::none;
    switch(rig_get_caps_int (rig_model, RIG_CAPS_PORT_TYPE))
      {
      case RIG_PORT_SERIAL:
        port_type = TransceiverFactory::Capabilities::serial;
        break;

      case RIG_PORT_NETWORK:
        port_type = TransceiverFactory::Capabilities::network;
        break;

      case RIG_PORT_USB:
        port_type = TransceiverFactory::Capabilities::usb;
        break;

      default: break;
      }
	  auto ptt_type = rig_get_caps_int (rig_model, RIG_CAPS_PTT_TYPE);
    (*rigs)[key] = TransceiverFactory::Capabilities (rig_model
                                                     , port_type
                                                     , RIG_MODEL_DUMMY != rig_model
                                                     && (RIG_PTT_RIG == ptt_type
                                                         || RIG_PTT_RIG_MICDATA == ptt_type)
                                                     , RIG_PTT_RIG_MICDATA == ptt_type);

    return 1;			// keep them coming
  }

  int unregister_callback (rig_model_t rig_model, void *)
  {
    rig_unregister (rig_get_caps_int (rig_model, RIG_CAPS_RIG_MODEL));
    return 1;			// keep them coming
  }

  bool env_flag_enabled (char const * name)
  {
    QByteArray const raw = qgetenv (name).trimmed ().toLower ();
    return raw == "1" || raw == "true" || raw == "yes" || raw == "on";
  }

  bool is_icom_serial_cat (unsigned model)
  {
    if (RIG_PORT_SERIAL != rig_get_caps_int (model, RIG_CAPS_PORT_TYPE))
      {
        return false;
      }
    QString const mfg = QString::fromLatin1 (rig_get_caps_cptr (model, RIG_CAPS_MFG_NAME_CPTR)).trimmed ();
    return 0 == mfg.compare (QStringLiteral ("Icom"), Qt::CaseInsensitive);
  }

  bool is_icom_serial_cat_ptt (unsigned model, TransceiverFactory::ParameterPack const& params)
  {
    return TransceiverFactory::PTT_method_CAT == params.ptt_type
        && is_icom_serial_cat (model);
  }

  // int frequency_change_callback (RIG * /* rig */, vfo_t vfo, freq_t f, rig_ptr_t arg)
  // {
  //   (void)vfo;			// unused in release build

  //   Q_ASSERT (vfo == RIG_VFO_CURR); // G4WJS: at the time of writing only current VFO is signalled by hamlib

  //   HamlibTransceiver * transceiver (reinterpret_cast<HamlibTransceiver *> (arg));
  //   Q_EMIT transceiver->frequency_change (f, Transceiver::A);
  //   return RIG_OK;
  // }

  class hamlib_tx_vfo_fixup final
  {
  public:
    hamlib_tx_vfo_fixup (RIG * rig, vfo_t tx_vfo)
      : rig_ {rig}
    {
      original_vfo_ = rig_->state.tx_vfo;
      rig_->state.tx_vfo = tx_vfo;
    }

    ~hamlib_tx_vfo_fixup ()
    {
      rig_->state.tx_vfo = original_vfo_;
    }

  private:
    RIG * rig_;
    vfo_t original_vfo_;
  };
}

class HamlibTransceiver::impl final
{
public:
  impl (HamlibTransceiver::logger_type * logger)
    : logger_ {logger}
    , model_ {RIG_MODEL_DUMMY}
    , rig_ {rig_init (model_)}
    , ptt_only_ {true}
    , back_ptt_port_ {false}
    , one_VFO_ {false}
    , is_dummy_ {true}
    , reversed_ {false}
    , freq_query_works_ {true}
    , mode_query_works_ {true}
    , split_query_works_ {true}
    , tickle_hamlib_ {false}
    , get_vfo_works_ {true}
    , set_vfo_works_ {true}
    , do_snr_ {false}
    , do_pwr_ {false}
    , do_pwr2_ {false}
    , do_swr_ {false}
  {
  }

  impl (HamlibTransceiver::logger_type * logger, unsigned model_number
        , TransceiverFactory::ParameterPack const& params)
    :  logger_ {logger}
    , model_ {model_number}
    , rig_ {rig_init (model_)}
    , ptt_only_ {false}
    , back_ptt_port_ {TransceiverFactory::TX_audio_source_rear == params.audio_source}
    , one_VFO_ {false}
    , is_dummy_ {RIG_MODEL_DUMMY == model_}
    , reversed_ {false}
    , freq_query_works_ {rig_ && rig_get_function_ptr (model_, RIG_FUNCTION_GET_FREQ)}
    , mode_query_works_ {rig_ && rig_get_function_ptr (model_, RIG_FUNCTION_GET_MODE)}
    , split_query_works_ {rig_ && rig_get_function_ptr (model_, RIG_FUNCTION_GET_SPLIT_VFO)}
    , tickle_hamlib_ {false}
    , get_vfo_works_ {true}
    , set_vfo_works_ {true}
    , do_snr_ {false}
    , do_pwr_ {false}
    , do_pwr2_ {false}
    , do_swr_ {false}
  {
  }

  HamlibTransceiver::logger_type& logger () const
  {
    return *logger_;
  }

  void error_check (int ret_code, QString const& doing) const;
  void set_conf (char const * item, char const * value);
  QByteArray get_conf (char const * item);
  Transceiver::MODE map_mode (rmode_t) const;
  rmode_t map_mode (Transceiver::MODE mode) const;
  std::tuple<vfo_t, vfo_t> get_vfos (bool for_split) const;

  HamlibTransceiver::logger_type mutable * logger_;
  unsigned model_;
  struct RIGDeleter {static void cleanup (RIG *);};
  QScopedPointer<RIG, RIGDeleter> rig_;

  bool ptt_only_;               // we can use a dummy device for PTT
  bool back_ptt_port_;
  bool one_VFO_;
  bool is_dummy_;

  // these are saved on destruction so we can start new instances
  // where the last one left off
  static freq_t dummy_frequency_;
  static rmode_t dummy_mode_;

  bool mutable reversed_;

  bool freq_query_works_;
  bool mode_query_works_;
  bool split_query_works_;
  bool tickle_hamlib_;          // Hamlib requires a
                                // rig_set_split_vfo() call to
                                // establish the Tx VFO
  bool get_vfo_works_;          // Net rigctl promises what it can't deliver
  bool set_vfo_works_;          // More rigctl promises which it can't deliver
  bool do_snr_;
  bool do_pwr_;
  bool do_pwr2_;
  bool do_swr_;

  static int debug_callback (enum rig_debug_level_e level, rig_ptr_t arg, char const * format, va_list ap);
};

freq_t HamlibTransceiver::impl::dummy_frequency_;
rmode_t HamlibTransceiver::impl::dummy_mode_ {RIG_MODE_NONE};

  // reroute Hamlib diagnostic messages to Qt
int HamlibTransceiver::impl::debug_callback (enum rig_debug_level_e level, rig_ptr_t arg, char const * format, va_list ap)
{
  auto logger = reinterpret_cast<logger_type *> (arg);
  auto message = QString::vasprintf (format, ap);
  va_end (ap);
  auto severity = boost::log::trivial::trace;
  switch (level)
    {
    case RIG_DEBUG_BUG: severity = boost::log::trivial::fatal; break;
    case RIG_DEBUG_ERR: severity = boost::log::trivial::error; break;
    case RIG_DEBUG_WARN: severity = boost::log::trivial::warning; break;
    case RIG_DEBUG_VERBOSE: severity = boost::log::trivial::debug; break;
    case RIG_DEBUG_TRACE: severity = boost::log::trivial::trace; break;
    default: break;
    };
  if (level != RIG_DEBUG_NONE) // no idea what level NONE means so
    // ignore it
    {
      BOOST_LOG_SEV (*logger, severity) << message.trimmed ().toStdString ();
    }
  return 0;
}

void HamlibTransceiver::register_transceivers (logger_type * logger,
                                               TransceiverFactory::Transceivers * registry)
{
  rig_set_debug_callback (impl::debug_callback, logger);
  // Decodium 4: keep Hamlib CAT traffic silent in the terminal by default.
  // The bridge/UI already surfaces connection failures, so the raw poll/CI-V
  // trace is just noise during normal startup and operation.
  rig_set_debug (RIG_DEBUG_NONE);
  // Guard: rig_load_all_backends() deve essere chiamato UNA SOLA VOLTA per processo.
  // Hamlib 4.7.0 crasha con "Hash collision" se chiamato due volte (rig_check_rig_caps
  // re-invoca initrigs4_* su modelli già registrati).
  static bool backends_loaded = false;
  if (!backends_loaded) {
    BOOST_LOG_SEV (*logger, boost::log::trivial::info) << "Hamlib version: " << rig_version ();
    rig_load_all_backends ();
    backends_loaded = true;
  }
  rig_list_foreach_model (register_callback, registry);
}

void HamlibTransceiver::unregister_transceivers ()
{
  rig_list_foreach_model (unregister_callback, nullptr);
}

void HamlibTransceiver::impl::RIGDeleter::cleanup (RIG * rig)
{
  if (rig)
    {
      rig_cleanup (rig);
    }
}

void HamlibTransceiver::impl::error_check (int ret_code, QString const& doing) const
{
  if (RIG_OK != ret_code)
    {
      CAT_ERROR ("error: " << rigerror (ret_code));
      throw error {tr ("Hamlib error: %1 while %2").arg (rigerror (ret_code)).arg (doing)};
    }
}

std::tuple<vfo_t, vfo_t> HamlibTransceiver::impl::get_vfos (bool for_split) const
{
  if (get_vfo_works_ && rig_get_function_ptr (model_, RIG_FUNCTION_GET_VFO))
    {
      vfo_t v;
      error_check (rig_get_vfo (rig_.data (), &v), tr ("getting current VFO")); // has side effect of establishing current VFO inside hamlib
      CAT_TRACE ("rig_get_vfo VFO=" << rig_strvfo (v));

      reversed_ = RIG_VFO_B == v;
    }
  else if (!for_split && set_vfo_works_ && rig_get_function_ptr (model_, RIG_FUNCTION_SET_VFO) && rig_get_function_ptr (model_, RIG_FUNCTION_SET_SPLIT_VFO))
    {
      // use VFO A/MAIN for main frequency and B/SUB for Tx
      // frequency if split since these type of radios can only
      // support this way around

      CAT_TRACE ("rig_set_vfo VFO=A/MAIN");
      error_check (rig_set_vfo (rig_.data (), rig_->state.vfo_list & RIG_VFO_A ? RIG_VFO_A : RIG_VFO_MAIN), tr ("setting current VFO"));
    }
  // else only toggle available but VFOs should be substitutable 

  auto rx_vfo = rig_->state.vfo_list & RIG_VFO_A ? RIG_VFO_A : RIG_VFO_MAIN;
  auto tx_vfo = (WSJT_RIG_NONE_CAN_SPLIT || !is_dummy_) && for_split
    ? (rig_->state.vfo_list & RIG_VFO_B ? RIG_VFO_B : RIG_VFO_SUB)
    : rx_vfo;
  if (reversed_)
    {
      CAT_TRACE ("reversing VFOs");
      std::swap (rx_vfo, tx_vfo);
    }

  CAT_TRACE ("RX VFO=" << rig_strvfo (rx_vfo) << " TX VFO=" << rig_strvfo (tx_vfo));
  return std::make_tuple (rx_vfo, tx_vfo);
}

void HamlibTransceiver::impl::set_conf (char const * item, char const * value)
{
  token_t token = rig_token_lookup (rig_.data (), item);
  if (RIG_CONF_END != token)	// only set if valid for rig model
    {
      error_check (rig_set_conf (rig_.data (), token, value), tr ("setting a configuration item"));
    }
}

QByteArray HamlibTransceiver::impl::get_conf (char const * item)
{
  token_t token = rig_token_lookup (rig_.data (), item);
  QByteArray value {128, '\0'};
  if (RIG_CONF_END != token)	// only get if valid for rig model
    {
#if HAVE_HAMLIB_GET_CONF2
      error_check (rig_get_conf2 (rig_.data (), token, value.data (), value.length ()),
                   tr ("getting a configuration item"));
#else
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
      error_check (rig_get_conf (rig_.data (), token, value.data ()),
                   tr ("getting a configuration item"));
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif
    }
  return value;
}

auto HamlibTransceiver::impl::map_mode (rmode_t m) const -> MODE
{
  switch (m)
    {
    case RIG_MODE_AM:
    case RIG_MODE_SAM:
    case RIG_MODE_AMS:
    case RIG_MODE_DSB:
      return AM;

    case RIG_MODE_CW:
      return CW;

    case RIG_MODE_CWR:
      return CW_R;

    case RIG_MODE_USB:
    case RIG_MODE_ECSSUSB:
    case RIG_MODE_SAH:
    case RIG_MODE_FAX:
      return USB;

    case RIG_MODE_LSB:
    case RIG_MODE_ECSSLSB:
    case RIG_MODE_SAL:
      return LSB;

    case RIG_MODE_RTTY:
      return FSK;

    case RIG_MODE_RTTYR:
      return FSK_R;

    case RIG_MODE_PKTLSB:
      return DIG_L;

    case RIG_MODE_PKTUSB:
      return DIG_U;

    case RIG_MODE_FM:
    case RIG_MODE_WFM:
      return FM;

    case RIG_MODE_PKTFM:
      return DIG_FM;

    default:
      return UNK;
    }
}

rmode_t HamlibTransceiver::impl::map_mode (MODE mode) const
{
  switch (mode)
    {
    case AM: return RIG_MODE_AM;
    case CW: return RIG_MODE_CW;
    case CW_R: return RIG_MODE_CWR;
    case USB: return RIG_MODE_USB;
    case LSB: return RIG_MODE_LSB;
    case FSK: return RIG_MODE_RTTY;
    case FSK_R: return RIG_MODE_RTTYR;
    case DIG_L: return RIG_MODE_PKTLSB;
    case DIG_U: return RIG_MODE_PKTUSB;
    case FM: return RIG_MODE_FM;
    case DIG_FM: return RIG_MODE_PKTFM;
    default: break;
    }
  return RIG_MODE_USB;	// quieten compiler grumble
}

HamlibTransceiver::HamlibTransceiver (logger_type * logger,
                                      TransceiverFactory::PTTMethod ptt_type, QString const& ptt_port,
                                      QObject * parent)
  : PollingTransceiver {logger, 0, parent}
  , m_ {logger}
{
  if (!m_->rig_)
    {
      throw error {tr ("Hamlib initialisation error")};
    }
  switch (ptt_type)
    {
    case TransceiverFactory::PTT_method_VOX:
      m_->set_conf ("ptt_type", "None");
      break;

    case TransceiverFactory::PTT_method_CAT:
      // Use the default PTT_TYPE for the rig (defined in the Hamlib
      // rig back-end capabilities).
      break;

    case TransceiverFactory::PTT_method_DTR:
    case TransceiverFactory::PTT_method_RTS:
      if (!ptt_port.isEmpty () && ptt_port != "None")
        {
#if defined (WIN32)
          m_->set_conf ("ptt_pathname", ("\\\\.\\" + ptt_port).toLatin1 ().data ());
#else
          m_->set_conf ("ptt_pathname", ptt_port.toLatin1 ().data ());
#endif
        }

      if (TransceiverFactory::PTT_method_DTR == ptt_type)
        {
          m_->set_conf ("ptt_type", "DTR");
        }
      else
        {
          m_->set_conf ("ptt_type", "RTS");
        }
      m_->set_conf ("ptt_share", "1");
    }

  // do this late to allow any configuration option to be overriden
  load_user_settings ();
}

HamlibTransceiver::HamlibTransceiver (logger_type * logger,
                                      unsigned model_number,
                                      TransceiverFactory::ParameterPack const& params,
                                      QObject * parent)
  : PollingTransceiver {logger, params.poll_interval, parent}
  , m_ {logger, model_number, params}
{
  if (!m_->rig_)
    {
      throw error {tr ("Hamlib initialisation error")};
    }

  // Icom CI-V passive split/mode/PTT reads are fragile on some serial
  // adapters/radios. Keep those conservative by default, but still poll the
  // dial frequency: without rig_get_freq(), turning the radio VFO cannot update
  // Decodium.
  bool const icom_serial_cat = is_icom_serial_cat (m_->model_);
  rig_split_control_enabled_ = params.split_mode == TransceiverFactory::split_mode_rig;
  bool const force_passive_state = env_flag_enabled ("DECODIUM_HAMLIB_POLL_PASSIVE_STATE");
  poll_passive_state_ = !icom_serial_cat || force_passive_state;
  poll_frequency_state_ = !env_flag_enabled ("DECODIUM_HAMLIB_DISABLE_FREQ_POLL");
  adaptive_frequency_poll_ = icom_serial_cat
      && poll_frequency_state_
      && !env_flag_enabled ("DECODIUM_HAMLIB_DISABLE_FREQ_POLL_BACKOFF");
  explicit_frequency_poll_vfo_ = adaptive_frequency_poll_
      && !env_flag_enabled ("DECODIUM_HAMLIB_DISABLE_EXPLICIT_FREQ_VFO");
  poll_ptt_state_ = !is_icom_serial_cat_ptt (m_->model_, params)
      || env_flag_enabled ("DECODIUM_HAMLIB_POLL_PTT");
  cat_keep_alive_ = params.cat_keep_alive && icom_serial_cat;
  if (adaptive_frequency_poll_)
    {
      qInfo ().noquote ()
        << "[CATDBG] Hamlib Icom serial frequency polling uses adaptive backoff"
        << "initialSkipTicks=" << kFrequencyPollInitialBackoffTicks_
        << "maxSkipTicks=" << kFrequencyPollMaxBackoffTicks_
        << "explicitVfo=" << explicit_frequency_poll_vfo_;
    }

  // m_->rig_->state.obj = this;

  if (!m_->is_dummy_)
    {
      // printf("Hamlib open params: power_on=%d power_off=%d ptt_share=%d\n",(params.poll_interval & rig__power) == rig__power,(params.poll_interval & rig__power_off) == rig__power_off,(params.poll_interval & ptt__share) == ptt__share);
      if (params.poll_interval & do__pwr) { do_pwr_ = true; do_pwr2_ = true; do_swr_ = true; }
      // ALC is needed by the one-shot audio calibration even when the user has
      // not enabled the visible PWR/SWR telemetry widget. We only read it while
      // TX/PTT is active, so RX polling stays cheap.
      do_alc_ = true;

      switch (rig_get_caps_int (m_->model_, RIG_CAPS_PORT_TYPE))
        {
        case RIG_PORT_SERIAL:
          if (!params.serial_port.isEmpty ())
            {
#if defined (WIN32)
              auto const serial_path = hamlib_windows_port_path (params.serial_port);
              m_->set_conf ("rig_pathname", serial_path.toLatin1 ().data ());
#else
              m_->set_conf ("rig_pathname", params.serial_port.toLatin1 ().data ());
#endif
            }
          m_->set_conf ("serial_speed", QByteArray::number (params.baud).data ());
          if (icom_serial_cat)
            {
              // Icom CI-V can spend several seconds inside Hamlib's internal
              // band-change preflight if the serial bus is briefly late. Keep
              // failed transactions short; the adaptive poll/backoff layer will
              // absorb the miss without tearing down CAT.
              m_->set_conf ("timeout", "700");
              m_->set_conf ("retry", "1");
              m_->set_conf ("timeout_retry", "0");
            }
          if (params.data_bits != TransceiverFactory::default_data_bits)
            {
              m_->set_conf ("data_bits", TransceiverFactory::seven_data_bits == params.data_bits ? "7" : "8");
            }
          if (params.stop_bits != TransceiverFactory::default_stop_bits)
            {
              m_->set_conf ("stop_bits", TransceiverFactory::one_stop_bit == params.stop_bits ? "1" : "2");
            }

          switch (params.handshake)
            {
            case TransceiverFactory::handshake_none: m_->set_conf ("serial_handshake", "None"); break;
            case TransceiverFactory::handshake_XonXoff: m_->set_conf ("serial_handshake", "XONXOFF"); break;
            case TransceiverFactory::handshake_hardware: m_->set_conf ("serial_handshake", "Hardware"); break;
            default: break;
            }

          if (params.civ_address > 0)
            {
              m_->set_conf ("civaddr", QByteArray::number (params.civ_address).data ());
            }

          if (params.force_dtr)
            {
              m_->set_conf ("dtr_state", params.dtr_high ? "ON" : "OFF");
            }
          if (params.force_rts)
            {
              m_->set_conf ("rts_state", params.rts_high ? "ON" : "OFF");
            }
          break;

        case RIG_PORT_NETWORK:
          if (!params.network_port.isEmpty ())
            {
              m_->set_conf ("rig_pathname", params.network_port.toLatin1 ().data ());
            }
          break;

        case RIG_PORT_USB:
          if (!params.usb_port.isEmpty ())
            {
              m_->set_conf ("rig_pathname", params.usb_port.toLatin1 ().data ());
            }
          break;

        default:
          throw error {tr ("Unsupported CAT type")};
          break;
        }
    }

  switch (params.ptt_type)
    {
    case TransceiverFactory::PTT_method_VOX:
      m_->set_conf ("ptt_type", "None");
      break;

    case TransceiverFactory::PTT_method_CAT:
      // Use the default PTT_TYPE for the rig (defined in the Hamlib
      // rig back-end capabilities).
      break;

    case TransceiverFactory::PTT_method_DTR:
    case TransceiverFactory::PTT_method_RTS:
      if (params.ptt_port.size ()
          && params.ptt_port != "None"
          && (m_->is_dummy_
              || RIG_PORT_SERIAL != rig_get_caps_int (m_->model_, RIG_CAPS_PORT_TYPE)
              || params.ptt_port != params.serial_port))
        {
#if defined (WIN32)
          m_->set_conf ("ptt_pathname", ("\\\\.\\" + params.ptt_port).toLatin1 ().data ());
#else
          m_->set_conf ("ptt_pathname", params.ptt_port.toLatin1 ().data ());
#endif
        }

      if (TransceiverFactory::PTT_method_DTR == params.ptt_type)
        {
          m_->set_conf ("ptt_type", "DTR");
        }
      else
        {
          m_->set_conf ("ptt_type", "RTS");
        }
      m_->set_conf ("ptt_share", "1");
    }

  // Make Icom CAT split commands less glitchy
  m_->set_conf ("no_xchg", "1");

  // do this late to allow any configuration option to be overriden
  load_user_settings ();

  // would be nice to get events but not supported on Windows and also not on a lot of rigs
  // rig_set_freq_callback (m_->rig_.data (), &frequency_change_callback, this);
}

HamlibTransceiver::~HamlibTransceiver () = default;

void HamlibTransceiver::load_user_settings ()
{
  //
  // user defined Hamlib settings
  //
  auto settings_file_name = QStandardPaths::locate (QStandardPaths::AppConfigLocation
                                                    , "hamlib_settings.json");
  if (!settings_file_name.isEmpty ())
    {
      QFile settings_file {settings_file_name};
      qDebug () << "Using Hamlib settings file:" << settings_file_name;
      if (settings_file.open (QFile::ReadOnly))
        {
          QJsonParseError status;
          auto settings_doc = QJsonDocument::fromJson (settings_file.readAll (), &status);
          if (status.error)
            {
              throw error {tr ("Hamlib settings file error: %1 at character offset %2")
                             .arg (status.errorString ()).arg (status.offset)};
            }
          qDebug () << "Hamlib settings JSON:" << settings_doc.toJson ();
          if (!settings_doc.isObject ())
            {
              throw error {tr ("Hamlib settings file error: top level must be a JSON object")};
            }
          auto const& settings = settings_doc.object ();

          //
          // configuration settings
          //
          auto const& config = settings["config"];
          if (!config.isUndefined ())
            {
              if (!config.isObject ())
                {
                  throw error {tr ("Hamlib settings file error: config must be a JSON object")};
                }
              auto const& config_list = config.toObject ();
              for (auto item = config_list.constBegin (); item != config_list.constEnd (); ++item)
                {
                  m_->set_conf (item.key ().toLocal8Bit ().constData ()
                                , (*item).toVariant ().toString ().toLocal8Bit ().constData ());
                }
            }
        }
    }
}

int HamlibTransceiver::do_start ()
{
  CAT_TRACE ("starting: " << rig_get_caps_cptr (m_->model_, RIG_CAPS_MFG_NAME_CPTR)
             << ": " << rig_get_caps_cptr (m_->model_, RIG_CAPS_MODEL_NAME_CPTR));

  token_t token = rig_token_lookup (m_->rig_.data (), "client");
  if (RIG_CONF_END != token)	// only set if valid for rig model
    {
      rig_set_conf (m_->rig_.data (), token, "WSJTX");
    }

  m_->error_check (rig_open (m_->rig_.data ()), tr ("opening connection to rig"));

  // Hamlib 4.7.0 bug: kenwood_open (e altri) restituiscono 0 ("continuing anyway")
  // anche se il rig non risponde, lasciando caps/state in stato invalido.
  // Verifica esplicita prima di procedere per evitare access violation nel polling.
  if (!m_->rig_.data () || !m_->rig_.data ()->caps)
    {
      rig_close (m_->rig_.data ());   // release serial port before throwing
      throw error {tr ("Rig not ready — caps null after open (no response from radio?)")};
    }

  if (RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt)
    {
      CAT_TRACE ("startup rig_set_ptt PTT=false");
      int const ptt_off_rc = rig_set_ptt (m_->rig_.data (), RIG_VFO_CURR, RIG_PTT_OFF);
      if (RIG_OK == ptt_off_rc)
        {
          ptt_on_ = false;
          update_PTT (false);
        }
      else if (-RIG_ENAVAIL != ptt_off_rc && -RIG_ENIMPL != ptt_off_rc)
        {
          CAT_TRACE ("startup rig_set_ptt PTT=false failed with rc:" << ptt_off_rc << "ignoring");
        }
    }

  // reset dynamic state
  m_->one_VFO_ = false;
  m_->reversed_ = false;
  m_->freq_query_works_ = rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_FREQ);
  m_->mode_query_works_ = rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_MODE);
  m_->split_query_works_ = rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_SPLIT_VFO);
  m_->tickle_hamlib_ = false;
  m_->get_vfo_works_ = true;
  m_->set_vfo_works_ = true;
  bool const requestedTransmitTelemetry = do_pwr_ || do_pwr2_ || do_swr_ || do_alc_;
  bool const requestedPowerTelemetry = do_pwr_ || do_pwr2_;
  bool const requestedSwrTelemetry = do_swr_;
  bool const hasGetLevelFunction = !m_->is_dummy_ && rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_LEVEL);
  setting_t const getLevelCaps = !m_->is_dummy_ ? static_cast<setting_t>(rig_get_caps_int (m_->model_, RIG_CAPS_HAS_GET_LEVEL)) : 0; // 1.0.326 B1: was int (32-bit truncation; RIG_LEVEL_RFPOWER_METER_WATTS is bit 39)
  bool const hasRfPowerMeterWatts = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_RFPOWER_METER_WATTS) == RIG_LEVEL_RFPOWER_METER_WATTS;
  bool const hasRfPower = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_RFPOWER) == RIG_LEVEL_RFPOWER;
  bool const hasSwr = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_SWR) == RIG_LEVEL_SWR;
  // Some Hamlib/Linux backends can answer RIG_LEVEL_ALC even when the caps mask
  // does not advertise it. Probe once during TX, then disable only on ENAVAIL/ENIMPL.
  bool const hasAlcCap = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_ALC) == RIG_LEVEL_ALC;
  // 1.0.581 — strumenti del finale: solo se dichiarati. Vale la stessa cautela
  // scritta piu' sotto per l'ALC, ma senza l'eccezione della sonda: quattro
  // livelli in piu' chiesti "per vedere se rispondono" su un CI-V a 19200
  // sarebbero traffico speso quasi sempre per niente.
  do_vd_ = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_VD_METER) == RIG_LEVEL_VD_METER;
  do_id_ = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_ID_METER) == RIG_LEVEL_ID_METER;
  do_pa_temp_ = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_TEMP_METER) == RIG_LEVEL_TEMP_METER;
  do_comp_ = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_COMP_METER) == RIG_LEVEL_COMP_METER;
  do_rfpower_ = hasRfPower;
  // 1.0.365 — do NOT run the opportunistic ALC probe on slow serial buses
  // (Icom CI-V, Yaesu): on a rig whose caps mask does not advertise ALC, the
  // extra rig_get_level(ALC) issued every TX tick congests the serial port and
  // can make a concurrent rig_set_ptt(OFF) fail (Icom one_transaction -13 bus
  // error), leaving the radio stuck in TX — regressed in 1.0.326 when the probe
  // + unconditional do_alc_ were introduced (worked in 1.0.325). Rigs that
  // DECLARE ALC in caps (e.g. Yaesu FT-991) are unaffected; the probe stays
  // available on fast back ends (Net rigctl / non-serial).
  bool const port_is_serial =
      !m_->is_dummy_ && rig_get_caps_int (m_->model_, RIG_CAPS_PORT_TYPE) == RIG_PORT_SERIAL;
  alc_probe_pending_ = do_alc_ && hasGetLevelFunction && !hasAlcCap && !port_is_serial;
  bool const hasAlc = hasAlcCap || alc_probe_pending_;

#if HAVE_HAMLIB_SEND_RAW && defined (RIG_MODEL_QRPLABS_QMX)
  // Hamlib 4.7.x knows the QMX CAT backend, but does not expose get_level or
  // PWR/SWR capabilities for it. QMX firmware >= 1.03 nevertheless documents
  // PC; (tenths of a watt) and SW; (hundredths of SWR). Use Hamlib's own raw
  // transaction API so the commands remain serialized on this worker and on
  // the already-open rig port. Never open or contend for the serial device.
  bool const isQmx = !m_->is_dummy_ && port_is_serial && m_->model_ == RIG_MODEL_QRPLABS_QMX;
  qmx_raw_power_ = isQmx && requestedPowerTelemetry
      && !hasRfPowerMeterWatts && !hasRfPower;
  qmx_raw_swr_ = isQmx && requestedSwrTelemetry && !hasSwr;
#else
  qmx_raw_power_ = false;
  qmx_raw_swr_ = false;
#endif
  qmx_raw_power_failures_ = 0;
  qmx_raw_swr_failures_ = 0;
  qmx_swr_filter_.reset ();
  qmx_swr_filter_tx_active_ = false;
  qmx_swr_transition_clock_.invalidate ();
  qmx_swr_threshold_hundredths_ = configured_qmx_swr_threshold_hundredths ();
  qmx_swr_transition_serial_ = 0;

  // S-meter: solo se il rig lo dichiara. Niente sonde opportunistiche come
  // per l'ALC - quelle sono costate una radio rimasta in trasmissione, e un
  // indicatore di comodo non vale quel rischio.
  bool const hasStrength = hasGetLevelFunction
      && (getLevelCaps & RIG_LEVEL_STRENGTH) == RIG_LEVEL_STRENGTH;
  do_strength_ = hasStrength;
  strength_tick_ = 0;
  strength_failures_ = 0;

  do_pwr_ = requestedPowerTelemetry && (hasRfPowerMeterWatts || qmx_raw_power_);
  do_pwr2_ = requestedPowerTelemetry && !do_pwr_ && hasRfPower;
  do_swr_ = requestedSwrTelemetry && (hasSwr || qmx_raw_swr_);
  do_alc_ &= hasAlc;
  if (requestedTransmitTelemetry)
    {
      qInfo ().noquote ()
        << "[CATDBG] Hamlib TX telemetry polling support"
        << "rig=" << rig_get_caps_cptr (m_->model_, RIG_CAPS_MODEL_NAME_CPTR)
        << "model=" << m_->model_
        << "getLevel=" << hasGetLevelFunction
        << "rfpowerMeterWatts=" << hasRfPowerMeterWatts
        << "rfpower=" << hasRfPower
        << "swr=" << hasSwr
        << "alc=" << hasAlc
        << "alcCap=" << hasAlcCap
        << "alcProbe=" << alc_probe_pending_
        << "qmxRawPower=" << qmx_raw_power_
        << "qmxRawSwr=" << qmx_raw_swr_
        << "effectivePower=" << (do_pwr_ || do_pwr2_)
        << "effectiveSwr=" << do_swr_
        << "passiveStatePoll=" << poll_passive_state_
        << "frequencyPoll=" << poll_frequency_state_
        << "passivePttPoll=" << poll_ptt_state_
        << "catKeepAlive=" << cat_keep_alive_
        << "rigSplitControl=" << rig_split_control_enabled_;
    }

  // the Net rigctl back end promises all functions work but we must
  // test get_vfo as it determines our strategy for Icom rigs
  vfo_t vfo;
  int rc = rig_get_vfo (m_->rig_.data (), &vfo);
  if (-RIG_ENAVAIL == rc || -RIG_ENIMPL == rc)
    {
      m_->get_vfo_works_ = false;
      // determine if the rig uses single VFO addressing i.e. A/B and
      // no get_vfo function
      if (m_->rig_->state.vfo_list & RIG_VFO_B)
        {
          m_->one_VFO_ = true;
        }
    }
  else
    {
      m_->error_check (rc, "testing getting current VFO");
    }

  if ((WSJT_RIG_NONE_CAN_SPLIT || !m_->is_dummy_)
      && rig_get_function_ptr (m_->model_, RIG_FUNCTION_SET_SPLIT_VFO)) // if split is possible do some extra setup
    {
      freq_t f1;
      freq_t f2;
      rmode_t m {RIG_MODE_USB};
      rmode_t mb;
      pbwidth_t w {RIG_PASSBAND_NORMAL};
      pbwidth_t wb;
      if (m_->freq_query_works_ && m_->mode_query_works_
          && (!m_->get_vfo_works_ || !rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_VFO)))
        {
          // Icom have deficient CAT protocol with no way of reading which
          // VFO is selected or if SPLIT is selected so we have to simply
          // assume it is as when we started by setting at open time right
          // here. We also gather/set other initial state.
          m_->error_check (rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &f1), tr ("getting current frequency"));
          f1 = std::round (f1);
          CAT_TRACE ("current frequency=" << f1);

          m_->error_check (rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &m, &w), tr ("getting current mode"));
          CAT_TRACE ("current mode=" << rig_strrmode (m) << " bw=" << w);

          if (!rig_get_function_ptr (m_->model_, RIG_FUNCTION_SET_VFO))
            {
              CAT_TRACE ("rig_vfo_op TOGGLE");
              rc = rig_vfo_op (m_->rig_.data (), RIG_VFO_CURR, RIG_OP_TOGGLE);
            }
          else
            {
              CAT_TRACE ("rig_set_vfo to other VFO");
              rc = rig_set_vfo (m_->rig_.data (), m_->rig_->state.vfo_list & RIG_VFO_B ? RIG_VFO_B : RIG_VFO_SUB);
              if (-RIG_ENAVAIL == rc || -RIG_ENIMPL == rc)
                {
                  // if we are talking to netrigctl then toggle VFO op
                  // may still work
                  CAT_TRACE ("rig_vfo_op TOGGLE");
                  rc = rig_vfo_op (m_->rig_.data (), RIG_VFO_CURR, RIG_OP_TOGGLE);
                }
            }
          if (-RIG_ENAVAIL == rc || -RIG_ENIMPL == rc)
            {
              // we are probably dealing with rigctld so we do not
              // have completely accurate rig capabilities
              m_->set_vfo_works_ = false;
              m_->one_VFO_ = false; // we do not need single VFO addressing
            }
          else
            {
              m_->error_check (rc, tr ("exchanging VFOs"));
            }

          if (m_->set_vfo_works_)
            {
              // without the above we cannot proceed but we know we
              // are on VFO A and that will not change so there's no
              // need to execute this block
              m_->error_check (rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &f2), tr ("getting other VFO frequency"));
              f2 = std::round (f2);
              CAT_TRACE ("rig_get_freq other frequency=" << f2);

              m_->error_check (rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &mb, &wb), tr ("getting other VFO mode"));
              CAT_TRACE ("rig_get_mode other mode=" << rig_strrmode (mb) << " bw=" << wb);

              update_other_frequency (f2);

              if (!rig_get_function_ptr (m_->model_, RIG_FUNCTION_SET_VFO))
                {
                  CAT_TRACE ("rig_vfo_op TOGGLE");
                  m_->error_check (rig_vfo_op (m_->rig_.data (), RIG_VFO_CURR, RIG_OP_TOGGLE), tr ("exchanging VFOs"));
                }
              else
                {
                  CAT_TRACE ("rig_set_vfo A/MAIN");
                  m_->error_check (rig_set_vfo (m_->rig_.data (), m_->rig_->state.vfo_list & RIG_VFO_A ? RIG_VFO_A : RIG_VFO_MAIN), tr ("setting current VFO"));
                }

              if (f1 != f2 || m != mb || w != wb)	// we must have started with MAIN/A
                {
                  update_rx_frequency (f1);
                }
              else
                {
                  m_->error_check (rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &f1), tr ("getting frequency"));
                  f1 = std::round (f1);
                  CAT_TRACE ("rig_get_freq frequency=" << f1);

                  m_->error_check (rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &m, &w), tr ("getting mode"));
                  CAT_TRACE ("rig_get_mode mode=" << rig_strrmode (m) << " bw=" << w);

                  update_rx_frequency (f1);
                }
            }

          // TRACE_CAT ("rig_set_split_vfo split off");
          // m_->error_check (rig_set_split_vfo (m_->rig_.data (), RIG_VFO_CURR, RIG_SPLIT_OFF, RIG_VFO_CURR), tr ("setting split off"));
          // update_split (false);
        }
      else
        {
          vfo_t v {RIG_VFO_A};  // assume RX always on VFO A/MAIN

          if (m_->get_vfo_works_ && rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_VFO))
            {
              m_->error_check (rig_get_vfo (m_->rig_.data (), &v), tr ("getting current VFO")); // has side effect of establishing current VFO inside hamlib
              CAT_TRACE ("rig_get_vfo current VFO=" << rig_strvfo (v));
            }

          m_->reversed_ = RIG_VFO_B == v;

          if (m_->mode_query_works_ && !(rig_get_caps_int (m_->model_, RIG_CAPS_TARGETABLE_VFO) & RIG_TARGETABLE_MODE))
            {
              if (RIG_OK == rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &m, &w))
                {
                  CAT_TRACE ("rig_get_mode current mode=" << rig_strrmode (m) << " bw=" << w);
                }
              else
                {
                  m_->mode_query_works_ = false;
                  // Some rigs (HDSDR) don't have a working way of
                  // reporting MODE so we give up on mode queries -
                  // sets will still cause an error
                  CAT_TRACE ("rig_get_mode can't do on this rig");
                }
            }
        }
      update_mode (m_->map_mode (m));
    }

  m_->tickle_hamlib_ = true;

  if (m_->is_dummy_ && !m_->ptt_only_ && impl::dummy_frequency_)
    {
      // return to where last dummy instance was
      // TODO: this is going to break down if multiple dummy rigs are used
      rig_set_freq (m_->rig_.data (), RIG_VFO_CURR, impl::dummy_frequency_);
      update_rx_frequency (impl::dummy_frequency_);
      if (RIG_MODE_NONE != impl::dummy_mode_)
        {
          rig_set_mode (m_->rig_.data (), RIG_VFO_CURR, impl::dummy_mode_, RIG_PASSBAND_NOCHANGE);
          update_mode (m_->map_mode (impl::dummy_mode_));
        }
    }

#if HAVE_HAMLIB_CACHING || HAVE_HAMLIB_OLD_CACHING
  // we must disable Hamlib caching because it lies about frequency
  // for less than 1 Hz resolution rigs
  auto orig_cache_timeout = rig_get_cache_timeout_ms (m_->rig_.data (), HAMLIB_CACHE_ALL);
  rig_set_cache_timeout_ms (m_->rig_.data (), HAMLIB_CACHE_ALL, 0);
#endif

  int resolution {0};
  if (adaptive_frequency_poll_)
    {
      resolution = -1;          // best guess; avoid startup get/set/get probes on fragile CI-V links
      qInfo ().noquote ()
        << "[CATDBG] Hamlib frequency resolution probe skipped for adaptive Icom serial polling";
    }
  else if (m_->freq_query_works_)
    {
      freq_t current_frequency;
      m_->error_check (rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &current_frequency), tr ("getting current VFO frequency"));
      current_frequency = std::round (current_frequency);
      Frequency f = current_frequency;
      if (f && !(f % 10))
        {
          auto test_frequency = f - f % 100 + 55;
          m_->error_check (rig_set_freq (m_->rig_.data (), RIG_VFO_CURR, test_frequency), tr ("setting frequency"));
          freq_t new_frequency;
          m_->error_check (rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &new_frequency), tr ("getting current VFO frequency"));
          new_frequency = std::round (new_frequency);
          switch (static_cast<Radio::FrequencyDelta> (new_frequency - test_frequency))
            {
            case -5: resolution = -1; break;  // 10Hz truncated
            case 5: resolution = 1; break;    // 10Hz rounded
            case -15: resolution = -2; break; // 20Hz truncated
            case -55: resolution = -3; break; // 100Hz truncated
            case 45: resolution = 3; break;   // 100Hz rounded
            }
          if (1 == resolution)      // may be 20Hz rounded
            {
              test_frequency = f - f % 100 + 51;
              m_->error_check (rig_set_freq (m_->rig_.data (), RIG_VFO_CURR, test_frequency), tr ("setting frequency"));
              m_->error_check (rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &new_frequency), tr ("getting current VFO frequency"));
              if (9 == static_cast<Radio::FrequencyDelta> (new_frequency - test_frequency))
                {
                  resolution = 2;   // 20Hz rounded
                }
            }
          m_->error_check (rig_set_freq (m_->rig_.data (), RIG_VFO_CURR, current_frequency), tr ("setting frequency"));
        }
    }
  else
    {
      resolution = -1;          // best guess
    }

#if HAVE_HAMLIB_CACHING || HAVE_HAMLIB_OLD_CACHING
  // revert Hamlib cache timeout
  rig_set_cache_timeout_ms (m_->rig_.data (), HAMLIB_CACHE_ALL, orig_cache_timeout);
#endif

  frequency_poll_failures_ = 0;
  frequency_poll_skip_ticks_ = 0;
  frequency_poll_backoff_ticks_ = kFrequencyPollInitialBackoffTicks_;

  do_poll ();
  start_cat_keep_alive_timer ();

  CAT_TRACE ("finished start " << state () << " reversed=" << m_->reversed_ << " resolution=" << resolution);
  return resolution;
}

void HamlibTransceiver::do_stop ()
{
  stop_cat_keep_alive_timer ();
  stop_polling ();

  if (m_->is_dummy_ && !m_->ptt_only_)
    {
      auto rc = rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &impl::dummy_frequency_);
      if (RIG_OK == rc)
        impl::dummy_frequency_ = std::round (impl::dummy_frequency_);
      if (m_->mode_query_works_)
        {
          pbwidth_t width;
          rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &impl::dummy_mode_, &width);
        }
    }
  // 1.0.365 — safety net: always release PTT before closing the rig. The
  // normal TX-off path do_ptt(false) routes through error_check(), which
  // THROWS on a CI-V bus error, leaving ptt_on_ true and the radio stuck in
  // TX when the app is closed mid-transmission. rig_close() does NOT turn TX
  // off on Icom CI-V. Drop PTT here best-effort: no throw, a few retries,
  // independent of bus state.
  if (m_->rig_ && !m_->is_dummy_
      && RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt
      && (ptt_on_ || state ().ptt ()))
    {
      int const attempts = ptt_off_attempt_limit (true);
      int rc = -RIG_EIO;
      for (int attempt = 0; attempt < attempts; ++attempt)
        {
          rc = rig_set_ptt (m_->rig_.data (), RIG_VFO_CURR, RIG_PTT_OFF);
          CAT_TRACE ("do_stop PTT=false attempt=" << attempt << " rc=" << rc);
          if (RIG_OK == rc)
            {
              ptt_on_ = false;
              ptt_off_failed_recently_ = false;
              update_PTT (false);
              break;
            }
        }
      if (RIG_OK != rc)
        {
          qWarning ().noquote ()
            << "[CATDBG] Hamlib shutdown PTT-off failed rc=" << rc
            << "attempts=" << attempts
            << "— closing without more CAT retries";
        }
    }
  if (m_->rig_)
    {
      rig_close (m_->rig_.data ());
    }

  CAT_TRACE ("state: " << state () << " reversed=" << m_->reversed_);
}

int HamlibTransceiver::ptt_off_attempt_limit (bool shutdown) const
{
  // Icom CI-V bus errors/timeouts can consume seconds per rig_set_ptt(false).
  // The adaptive Icom serial path already detected a slow/failing bus, so keep
  // one safety attempt and avoid a shutdown retry train.
  if (adaptive_frequency_poll_ || ptt_off_failed_recently_)
    {
      return 1;
    }
  return shutdown ? 3 : 2;
}

void HamlibTransceiver::do_frequency (Frequency f, MODE m, bool no_ignore)
{
  CAT_TRACE ("f: " << f << " mode: " << m << " reversed: " << m_->reversed_);

  // only change when receiving or simplex or direct VFO addressing
  // unavailable or forced
  if (!state ().ptt () || !state ().split () || !m_->one_VFO_ || no_ignore)
    {
      // for the 1st time as a band change may cause a recalled mode to be
      // set
      vfo_t target_vfo = RIG_VFO_CURR;
      if (!(m_->rig_->state.vfo_list & RIG_VFO_B))
        {
          target_vfo = RIG_VFO_MAIN; // no VFO A/B so force to Rx on MAIN
        }
      auto const write_result = set_frequency_or_tolerate (target_vfo, f, tr ("setting frequency"));
      if (FrequencyWriteResult::Rejected == write_result
          || FrequencyWriteResult::Deferred == write_result)
        {
          return;
        }

      // A CI-V timeout/bus error may be returned after the request has been
      // queued, but it is not confirmation that the radio accepted the QSY.
      // Publishing f here used to clear the bridge's local-QSY guard; a late
      // poll for the old band frequency could then overwrite the requested
      // dial. Keep the last confirmed frequency until polling verifies it,
      // allowing the bounded bridge retry to reissue the QSY if necessary.
      if (FrequencyWriteResult::AppliedWithTransientError == write_result)
        {
          return;
        }
      update_rx_frequency (f);

      if (m_->mode_query_works_ && UNK != m)
        {
          rmode_t current_mode;
          pbwidth_t current_width;
          auto new_mode = m_->map_mode (m);
          m_->error_check (rig_get_mode (m_->rig_.data (), target_vfo, &current_mode, &current_width), tr ("getting current VFO mode"));
          CAT_TRACE ("rig_get_mode mode=" << rig_strrmode (current_mode) << " bw=" << current_width);

          if (new_mode != current_mode)
            {
              CAT_TRACE ("rig_set_mode mode=" << rig_strrmode (new_mode));
              m_->error_check (rig_set_mode (m_->rig_.data (), target_vfo, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting current VFO mode"));

              // for the 2nd time because a mode change may have caused a
              // frequency change
              auto const post_mode_write = set_frequency_or_tolerate (RIG_VFO_CURR, f, tr ("setting frequency"));
              if (FrequencyWriteResult::Rejected == post_mode_write
                  || FrequencyWriteResult::Deferred == post_mode_write)
                {
                  return;
                }

              // for the second time because some rigs change mode according
              // to frequency such as the TS-2000 auto mode setting
              CAT_TRACE ("rig_set_mode mode=" << rig_strrmode (new_mode));
              m_->error_check (rig_set_mode (m_->rig_.data (), target_vfo, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting current VFO mode"));
            }
          // set mode on TX VFO too if we are in split
          if (state ().split())
            {
              auto tx_vfo = m_->rig_->state.vfo_list & RIG_VFO_B ? RIG_VFO_B : RIG_VFO_SUB;
              m_->error_check (rig_set_mode (m_->rig_.data (), tx_vfo, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting TX VFO mode"));
            }
          update_mode (m);
        }
    }
}

void HamlibTransceiver::do_tx_frequency (Frequency tx, MODE mode, bool no_ignore)
{
  CAT_TRACE ("txf: " << tx << " reversed: " << m_->reversed_);

  if (!rig_split_control_enabled_)
    {
      update_other_frequency (0);
      update_split (false);
      return;
    }

  if (suppress_cat_write_during_backoff (tr ("setting TX/split frequency")))
    {
      return;
    }

  if (WSJT_RIG_NONE_CAN_SPLIT || !m_->is_dummy_) // split is meaningless if you can't see it
    {
      auto split = tx ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
      auto vfos = m_->get_vfos (tx);
      // auto rx_vfo = std::get<0> (vfos); // or use RIG_VFO_CURR
      auto tx_vfo = std::get<1> (vfos);

      if (tx)
        {
          // Doing set split for the 1st of two times, this one
          // ensures that the internal Hamlib state is correct
          // otherwise rig_set_split_freq() will target the wrong VFO
          // on some rigs

          if (m_->tickle_hamlib_)
            {
              // This potentially causes issues with the Elecraft K3
              // which will block setting split mode when it deems
              // cross mode split operation not possible. There's not
              // much we can do since the Hamlib Library needs this
              // call at least once to establish the Tx VFO. Best we
              // can do is only do this once per session.
              CAT_TRACE ("rig_set_split_vfo split=" << split);
              auto rc = rig_set_split_vfo (m_->rig_.data (), RIG_VFO_CURR, split, tx_vfo);
              if (tx || (-RIG_ENAVAIL != rc && -RIG_ENIMPL != rc))
                {
                  // On rigs that can't have split controlled only throw an
                  // exception when an error other than command not accepted
                  // is returned when trying to leave split mode. This allows
                  // fake split mode and non-split mode to work without error
                  // on such rigs without having to know anything about the
                  // specific rig.
                  m_->error_check (rc, tr ("setting/unsetting split mode"));
                }
              m_->tickle_hamlib_ = false;
              update_split (tx);
            }

          // just change current when transmitting with single VFO
          // addressing
          if (state ().ptt () && m_->one_VFO_)
            {
              CAT_TRACE ("rig_set_split_vfo split=" << split);
              m_->error_check (rig_set_split_vfo (m_->rig_.data (), RIG_VFO_CURR, split, tx_vfo), tr ("setting split mode"));

              m_->error_check (rig_set_freq (m_->rig_.data (), RIG_VFO_CURR, tx), tr ("setting frequency"));

              if (UNK != mode && m_->mode_query_works_)
                {
                  rmode_t current_mode;
                  pbwidth_t current_width;
                  auto new_mode = m_->map_mode (mode);
                  m_->error_check (rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &current_mode, &current_width), tr ("getting current VFO mode"));
                  CAT_TRACE ("rig_get_mode mode=" << rig_strrmode (current_mode) << " bw=" << current_width);

                  if (new_mode != current_mode)
                    {
                      CAT_TRACE ("rig_set_mode mode=" << rig_strrmode (new_mode));
                      m_->error_check (rig_set_mode (m_->rig_.data (), RIG_VFO_CURR, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting current VFO mode"));
                    }
                }
              update_other_frequency (tx);
            }
          else if (!m_->one_VFO_ || no_ignore)   // if not single VFO addressing and not forced
            {
              hamlib_tx_vfo_fixup fixup (m_->rig_.data (), tx_vfo);
              if (UNK != mode)
                {
                  auto new_mode = m_->map_mode (mode);
                  CAT_TRACE ("rig_set_split_freq_mode freq=" << tx
                             << " mode = " << rig_strrmode (new_mode));
                  m_->error_check (rig_set_split_freq_mode (m_->rig_.data (), RIG_VFO_CURR, tx, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting split TX frequency and mode"));
                }
              else
                {
                  CAT_TRACE ("rig_set_split_freq freq=" << tx);
                  m_->error_check (rig_set_split_freq (m_->rig_.data (), RIG_VFO_CURR, tx), tr ("setting split TX frequency"));
                }
              // Enable split last since some rigs (Kenwood for one) come out
              // of split when you switch RX VFO (to set split mode above for
              // example). Also the Elecraft K3 will refuse to go to split
              // with certain VFO A/B mode combinations.
              CAT_TRACE ("rig_set_split_vfo split=" << split);
              m_->error_check (rig_set_split_vfo (m_->rig_.data (), RIG_VFO_CURR, split, tx_vfo), tr ("setting split mode"));
              update_other_frequency (tx);
              update_split (tx);
            }
        }
      else
        {
          // Disable split
          CAT_TRACE ("rig_set_split_vfo split=" << split);
          auto rc = rig_set_split_vfo (m_->rig_.data (), RIG_VFO_CURR, split, tx_vfo);
          if (tx || (-RIG_ENAVAIL != rc && -RIG_ENIMPL != rc))
            {
              // On rigs that can't have split controlled only throw an
              // exception when an error other than command not accepted
              // is returned when trying to leave split mode. This allows
              // fake split mode and non-split mode to work without error
              // on such rigs without having to know anything about the
              // specific rig.
              m_->error_check (rc, tr ("setting/unsetting split mode"));
            }
          update_other_frequency (tx);
          update_split (tx);
        }
    }
}

void HamlibTransceiver::do_mode (MODE mode)
{
  CAT_TRACE (mode);

  if (suppress_cat_write_during_backoff (tr ("setting current VFO mode")))
    {
      return;
    }

  auto vfos = m_->get_vfos (state ().split ());
  // auto rx_vfo = std::get<0> (vfos);
  auto tx_vfo = std::get<1> (vfos);

  rmode_t current_mode;
  pbwidth_t current_width;
  auto new_mode = m_->map_mode (mode);

  vfo_t target_vfo = RIG_VFO_CURR;
  if (!(m_->rig_->state.vfo_list & RIG_VFO_B))
    {
      target_vfo = RIG_VFO_MAIN; // no VFO A/B so force to Rx on MAIN
    }

  // only change when receiving or simplex if direct VFO addressing unavailable
  if (!(state ().ptt () && state ().split () && m_->one_VFO_))
    {
      m_->error_check (rig_get_mode (m_->rig_.data (), target_vfo, &current_mode, &current_width), tr ("getting current VFO mode"));
      CAT_TRACE ("rig_get_mode mode=" << rig_strrmode (current_mode) << " bw=" << current_width);

      if (new_mode != current_mode)
        {
          CAT_TRACE ("rig_set_mode mode=" << rig_strrmode (new_mode));
          m_->error_check (rig_set_mode (m_->rig_.data (), target_vfo, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting current VFO mode"));
        }
    }

  // just change current when transmitting split with one VFO mode
  if (state ().ptt () && state ().split () && m_->one_VFO_)
    {
      m_->error_check (rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &current_mode, &current_width), tr ("getting current VFO mode"));
      CAT_TRACE ("rig_get_mode mode=" << rig_strrmode (current_mode) << " bw=" << current_width);

      if (new_mode != current_mode)
        {
          CAT_TRACE ("rig_set_mode mode=" << rig_strrmode (new_mode));
          m_->error_check (rig_set_mode (m_->rig_.data (), RIG_VFO_CURR, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting current VFO mode"));
        }
    }
  else if (state ().split () && !m_->one_VFO_)
    {
      m_->error_check (rig_get_split_mode (m_->rig_.data (), RIG_VFO_CURR, &current_mode, &current_width), tr ("getting split TX VFO mode"));
      CAT_TRACE ("rig_get_split_mode mode=" << rig_strrmode (current_mode) << " bw=" << current_width);

      if (new_mode != current_mode)
        {
          CAT_TRACE ("rig_set_split_mode mode=" << rig_strrmode (new_mode));
          hamlib_tx_vfo_fixup fixup (m_->rig_.data (), tx_vfo);
          m_->error_check (rig_set_split_mode (m_->rig_.data (), RIG_VFO_CURR, new_mode, RIG_PASSBAND_NOCHANGE), tr ("setting split TX VFO mode"));
        }
    }
  update_mode (mode);
}

bool HamlibTransceiver::poll_vfo_frequency (vfo_t vfo, freq_t * frequency, QString const& doing)
{
  int const rc = rig_get_freq (m_->rig_.data (), vfo, frequency);
  if (RIG_OK == rc)
    {
      note_frequency_poll_success ();
      return true;
    }

  if (-RIG_ENAVAIL == rc || -RIG_ENIMPL == rc)
    {
      m_->freq_query_works_ = false;
      poll_frequency_state_ = false;
      stop_cat_keep_alive_timer ();
      qInfo ().noquote ()
        << "[CATDBG] Hamlib frequency polling disabled: rig_get_freq unavailable"
        << "rc=" << rc
        << "op=" << doing;
      return false;
    }

  note_frequency_poll_failure (rc, doing);
  return false;
}

void HamlibTransceiver::note_frequency_poll_success ()
{
  bool const was_unstable = frequency_poll_failures_ || frequency_poll_skip_ticks_
      || frequency_poll_write_quiet_ticks_ > 0
      || frequency_poll_backoff_ticks_ != kFrequencyPollInitialBackoffTicks_;

  frequency_poll_write_quiet_ticks_ = 0;

  if (was_unstable)
    {
      qInfo ().noquote ()
        << "[CATDBG] Hamlib frequency polling recovered"
        << "writeQuietTicks=" << frequency_poll_write_quiet_ticks_;
    }
  frequency_poll_failures_ = 0;
  frequency_poll_skip_ticks_ = 0;
  frequency_poll_backoff_ticks_ = kFrequencyPollInitialBackoffTicks_;
}

void HamlibTransceiver::note_frequency_poll_failure (int rc, QString const& doing)
{
  ++frequency_poll_failures_;
  if (adaptive_frequency_poll_)
    {
      frequency_poll_write_quiet_ticks_ = std::max (frequency_poll_write_quiet_ticks_,
                                                    kFrequencyPollWriteQuietTicks_);
    }

  qWarning ().noquote ()
    << "[CATDBG] Hamlib frequency poll failed"
    << frequency_poll_failures_ << "/" << kFrequencyPollMaxFailures_
    << "rc=" << rc
    << "op=" << doing
    << "writeQuietTicks=" << frequency_poll_write_quiet_ticks_;

  if (adaptive_frequency_poll_ && frequency_poll_failures_ >= kFrequencyPollMaxFailures_)
    {
      frequency_poll_skip_ticks_ = frequency_poll_backoff_ticks_;
      frequency_poll_backoff_ticks_ = std::min (kFrequencyPollMaxBackoffTicks_,
                                                frequency_poll_backoff_ticks_ * 2);
      frequency_poll_failures_ = 0;
      qWarning ().noquote ()
        << "[CATDBG] Hamlib frequency polling backoff after timeout"
        << "skipTicks=" << frequency_poll_skip_ticks_
        << "nextSkipTicks=" << frequency_poll_backoff_ticks_;
    }
}

bool HamlibTransceiver::cat_write_backoff_active () const
{
  return adaptive_frequency_poll_
      && (frequency_poll_failures_ > 0
          || frequency_poll_skip_ticks_ > 0
          || frequency_poll_write_quiet_ticks_ > 0);
}

bool HamlibTransceiver::suppress_cat_write_during_backoff (QString const& operation) const
{
  if (!cat_write_backoff_active ())
    {
      return false;
    }

  qWarning ().noquote ()
    << "[CATDBG] Hamlib CAT write suppressed while Icom frequency polling is unstable"
    << "op=" << operation
    << "failures=" << frequency_poll_failures_
    << "skipTicks=" << frequency_poll_skip_ticks_
    << "writeQuietTicks=" << frequency_poll_write_quiet_ticks_
    << "nextSkipTicks=" << frequency_poll_backoff_ticks_;
  return true;
}

HamlibTransceiver::FrequencyWriteResult HamlibTransceiver::set_frequency_or_tolerate (vfo_t vfo,
                                                                                      Frequency f,
                                                                                      QString const& operation)
{
  int const rc = rig_set_freq (m_->rig_.data (), vfo, f);
  if (RIG_OK == rc)
    {
      return FrequencyWriteResult::Applied;
    }

  QString const error_text = QString::fromLocal8Bit (rigerror (rc));
  bool const rejected_after_timeout =
      adaptive_frequency_poll_
      && rc == -RIG_ERJCTED
      && (error_text.contains (QStringLiteral ("timed out"), Qt::CaseInsensitive)
          || error_text.contains (QStringLiteral ("timeout"), Qt::CaseInsensitive)
          || error_text.contains (QStringLiteral ("returning(-5)"), Qt::CaseInsensitive)
          || error_text.contains (QStringLiteral ("rig_get_freq failed"), Qt::CaseInsensitive)
          || error_text.contains (QStringLiteral ("Communication bus error"), Qt::CaseInsensitive)
          || error_text.contains (QStringLiteral ("returning(-13)"), Qt::CaseInsensitive));
  if (rejected_after_timeout)
    {
      note_frequency_poll_failure (-RIG_ETIMEOUT, operation + tr (" (write preflight timeout)"));
      qWarning ().noquote ()
        << "[CATDBG] Hamlib Icom frequency write deferred after preflight timeout"
        << "rc=" << rc
        << "vfo=" << rig_strvfo (vfo)
        << "freq=" << QString::number (static_cast<double> (f), 'f', 0)
        << "op=" << operation;
      return FrequencyWriteResult::Deferred;
    }

  // RIG_ELIMIT was added after Hamlib 4.5. Keep the stable numeric error code
  // here so source builds remain compatible with distributions shipping 4.5.
  constexpr int hamlib_limit_exceeded_error = 21;
  bool const rejected_frequency =
      rc == -RIG_ERJCTED
      || rc == -RIG_EINVAL
      || rc == -RIG_EDOM
      || rc == -hamlib_limit_exceeded_error
      || rc == -RIG_ENAVAIL
      || rc == -RIG_ENTARGET
      || rc == -RIG_EVFO;
  if (rejected_frequency)
    {
      qWarning ().noquote ()
        << "[CATDBG] Hamlib frequency write rejected by rig"
        << "rc=" << rc
        << "vfo=" << rig_strvfo (vfo)
        << "freq=" << QString::number (static_cast<double> (f), 'f', 0)
        << "op=" << operation
        << "reason=" << error_text;
      return FrequencyWriteResult::Rejected;
    }

  bool const transient_icom_serial_error =
      adaptive_frequency_poll_
      && (rc == -RIG_EIO || rc == -RIG_ETIMEOUT || rc == -RIG_BUSERROR);
  if (transient_icom_serial_error)
    {
      note_frequency_poll_failure (rc, operation + tr (" (write tolerated)"));
      qWarning ().noquote ()
        << "[CATDBG] Hamlib Icom frequency write transient error tolerated"
        << "rc=" << rc
        << "vfo=" << rig_strvfo (vfo)
        << "freq=" << QString::number (static_cast<double> (f), 'f', 0)
        << "op=" << operation;
      return FrequencyWriteResult::AppliedWithTransientError;
    }

  m_->error_check (rc, operation);
  return FrequencyWriteResult::Rejected;
}

vfo_t HamlibTransceiver::frequency_poll_vfo () const
{
  if (!explicit_frequency_poll_vfo_
      || !m_->rig_
      || m_->one_VFO_)
    {
      return RIG_VFO_CURR;
    }

  auto const vfo_list = m_->rig_->state.vfo_list;
  if (m_->reversed_)
    {
      if (vfo_list & RIG_VFO_B) return RIG_VFO_B;
      if (vfo_list & RIG_VFO_SUB) return RIG_VFO_SUB;
    }

  if (vfo_list & RIG_VFO_A) return RIG_VFO_A;
  if (vfo_list & RIG_VFO_MAIN) return RIG_VFO_MAIN;
  // Some Hamlib Icom backends under-report vfo_list/targetable capability.
  // For serial CI-V polling, try MAIN explicitly before falling back to CURR;
  // this avoids the fragile "current VFO" transaction path on IC-7300 class rigs.
  return RIG_VFO_MAIN;
}

void HamlibTransceiver::do_poll ()
{
  auto * rig = m_->rig_.data ();
  if (!rig || !rig->caps)
    {
      return;
    }

  freq_t f {0};
  rmode_t m {RIG_MODE_USB};
  pbwidth_t w {RIG_PASSBAND_NORMAL};
  split_t s {RIG_SPLIT_OFF};
  bool const tx_active = ptt_on_ || state ().ptt ();

  // While transmitting, frequency/mode/VFO reads provide no useful UI data:
  // Decodium already owns the requested TX state. On serial rigs they also
  // compete with PTT and meter transactions on the same CAT bus. Keep only
  // the PTT confirmation and explicitly enabled TX telemetry active.
  if (!tx_active
      && poll_passive_state_
      && m_->get_vfo_works_
      && rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_VFO))
    {
      vfo_t v;
      m_->error_check (rig_get_vfo (m_->rig_.data (), &v), tr ("getting current VFO")); // has side effect of establishing current VFO inside hamlib
      CAT_TRACE ("VFO=" << rig_strvfo (v));
      m_->reversed_ = RIG_VFO_B == v;
    }

  if (!tx_active
      && poll_passive_state_
      && (WSJT_RIG_NONE_CAN_SPLIT || !m_->is_dummy_)
      && rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_SPLIT_VFO) && m_->split_query_works_)
    {
      vfo_t v {RIG_VFO_NONE};		// so we can tell if it doesn't get updated :(
      auto rc = rig_get_split_vfo (m_->rig_.data (), RIG_VFO_CURR, &s, &v);
      if (-RIG_OK == rc && RIG_SPLIT_ON == s)
        {
          CAT_TRACE ("rig_get_split_vfo split=" << s << " VFO=" << rig_strvfo (v));
          update_split (true);
          // if (RIG_VFO_A == v)
          // 	{
          // 	  m_->reversed_ = true;	// not sure if this helps us here
          // 	}
        }
      else if (-RIG_OK == rc)	// not split
        {
          CAT_TRACE ("rig_get_split_vfo split=" << s << " VFO=" << rig_strvfo (v));
          update_split (false);
        }
      else
        {
          // Some rigs (Icom) don't have a way of reporting SPLIT
          // mode
          CAT_TRACE ("rig_get_split_vfo can't do on this rig");
          // just report how we see it based on prior commands
          m_->split_query_works_ = false;
        }
    }

  bool frequency_poll_due = poll_passive_state_ || poll_frequency_state_;
  if (adaptive_frequency_poll_ && frequency_poll_skip_ticks_ > 0)
    {
      --frequency_poll_skip_ticks_;
      frequency_poll_due = false;
    }

  if (!tx_active && frequency_poll_due && m_->freq_query_works_)
    {
      bool current_frequency_ok = true;
      // The outer guard limits dial reads to RX; direct VFO addressing is
      // still used where available.
      if (!state ().ptt () || !state ().split ())
        {
          vfo_t const poll_vfo = frequency_poll_vfo ();
          if (explicit_frequency_poll_vfo_ && !frequency_poll_vfo_logged_)
            {
              frequency_poll_vfo_logged_ = true;
              qInfo ().noquote ()
                << "[CATDBG] Hamlib frequency polling VFO selected"
                << rig_strvfo (poll_vfo)
                << "vfoList=" << m_->rig_->state.vfo_list
                << "targetable=" << rig_get_caps_int (m_->model_, RIG_CAPS_TARGETABLE_VFO);
            }
          current_frequency_ok = poll_vfo_frequency (
              poll_vfo,
              &f,
              poll_vfo == RIG_VFO_CURR
                  ? tr ("getting current VFO frequency")
                  : tr ("getting RX VFO frequency"));
          if (current_frequency_ok)
            {
              f = std::round (f);
              CAT_TRACE ("rig_get_freq frequency=" << Radio::frequency (f));
              update_rx_frequency (f);
            }
        }

      if (current_frequency_ok
          && (WSJT_RIG_NONE_CAN_SPLIT || !m_->is_dummy_)
          && state ().split ()
          && (rig_get_caps_int (m_->model_, RIG_CAPS_TARGETABLE_VFO) & RIG_TARGETABLE_FREQ)
          && !m_->one_VFO_)
        {
          // only read "other" VFO if in split, this allows rigs like
          // FlexRadio to work in Kenwood TS-2000 mode despite them
          // not having a FB; command

          // we can only probe current VFO unless rig supports reading
          // the other one directly because we can't glitch the Rx
          if (poll_vfo_frequency (m_->reversed_
                                  ? (m_->rig_->state.vfo_list & RIG_VFO_A ? RIG_VFO_A : RIG_VFO_MAIN)
                                  : (m_->rig_->state.vfo_list & RIG_VFO_B ? RIG_VFO_B : RIG_VFO_SUB),
                                  &f,
                                  tr ("getting other VFO frequency")))
            {
              f = std::round (f);
              CAT_TRACE ("rig_get_freq other VFO=" << f);
              update_other_frequency (f);
            }
        }
    }

  // Mode reads are useful in RX only; during TX the requested mode is known.
  if (!tx_active
      && poll_passive_state_
      && m_->mode_query_works_)
    {
      // We have to ignore errors here because Yaesu FTdx... rigs can
      // report the wrong mode when transmitting split with different
      // modes per VFO. This is unfortunate because that is exactly
      // what you need to do to get 4kHz Rx b.w and modulation into
      // the rig through the data socket or USB. I.e.  USB for Rx and
      // DATA-USB for Tx.
      auto rc = rig_get_mode (m_->rig_.data (), RIG_VFO_CURR, &m, &w);
      if (RIG_OK == rc)
        {
          CAT_TRACE ("rig_get_mode mode=" << rig_strrmode (m) << " bw=" << w);
          update_mode (m_->map_mode (m));
        }
      else
        {
          CAT_TRACE ("rig_get_mode mode failed with rc: " << rc << " ignoring");
        }
    }

  if (poll_ptt_state_
      && RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt
      && rig_get_function_ptr (m_->model_, RIG_FUNCTION_GET_PTT))
  {
    ptt_t p;
    auto rc = rig_get_ptt (m_->rig_.data (), RIG_VFO_CURR, &p);
    if (-RIG_ENAVAIL != rc && -RIG_ENIMPL != rc) // may fail if
      // Net rig ctl and target doesn't
      // support command
      {
        m_->error_check (rc, tr ("getting PTT state"));
        CAT_TRACE ("rig_get_ptt PTT=" << p);
        ptt_on_ = !(RIG_PTT_OFF == p);
        update_PTT (ptt_on_);
     }
   }
  else if (!poll_ptt_state_)
    {
      update_PTT (ptt_on_);
    }

  // 1.0.204 — throttle PWR/SWR polling: each rig_get_level RIG_LEVEL_SWR /
  // RFPOWER_METER_WATTS takes ~150ms on Yaesu FT-991 at 38400 baud. Running
  // both every tick made do_poll() ~470ms, which surfaced as main-thread
  // stall whenever sendStateSync was issued concurrently. Always run
  // telemetry when actually transmitting (so meters stay responsive), and
  // skip 3 out of 4 RX ticks otherwise — meters can also be updated by
  // schedule_transmit_telemetry_burst when PTT transitions.
  bool const telemetry_enabled = do_pwr_ || do_pwr2_ || do_swr_ || do_alc_;
  if (telemetry_enabled && !tx_active)
    {
      ++telemetry_tick_;
      if (telemetry_tick_ < kTelemetrySkipRatio_)
        {
          return;
        }
      telemetry_tick_ = 0;
    }
  else
    {
      telemetry_tick_ = 0;
    }

  poll_transmit_telemetry (false);
}

void HamlibTransceiver::start_cat_keep_alive_timer ()
{
  if (!cat_keep_alive_
      || poll_passive_state_
      || poll_frequency_state_
      || !m_->rig_
      || m_->is_dummy_
      || !m_->freq_query_works_)
    {
      return;
    }

  if (!cat_keep_alive_timer_)
    {
      cat_keep_alive_timer_ = new QTimer {this};
      cat_keep_alive_timer_->setTimerType (Qt::CoarseTimer);
      connect (cat_keep_alive_timer_, &QTimer::timeout,
               this, &HamlibTransceiver::poll_cat_keep_alive);
    }

  cat_keep_alive_timer_->start (kCatKeepAliveIntervalMs_);
  qInfo ().noquote ()
    << "[CATDBG] Hamlib CAT keep-alive timer active"
    << "interval_ms=" << kCatKeepAliveIntervalMs_;
}

void HamlibTransceiver::stop_cat_keep_alive_timer ()
{
  if (cat_keep_alive_timer_)
    {
      cat_keep_alive_timer_->stop ();
    }
}

void HamlibTransceiver::poll_cat_keep_alive ()
{
  if (!cat_keep_alive_
      || poll_passive_state_
      || poll_frequency_state_
      || !m_->freq_query_works_
      || ptt_on_
      || state ().ptt ())
    {
      return;
    }

  freq_t f {0};
  int const rc = rig_get_freq (m_->rig_.data (), RIG_VFO_CURR, &f);
  if (RIG_OK == rc)
    {
      cat_keep_alive_failures_ = 0;
      CAT_TRACE ("rig_get_freq CAT keep-alive frequency=" << Radio::frequency (std::round (f)));
      return;
    }

  if (-RIG_ENAVAIL == rc || -RIG_ENIMPL == rc)
    {
      cat_keep_alive_ = false;
      stop_cat_keep_alive_timer ();
      qInfo ().noquote ()
        << "[CATDBG] Hamlib CAT keep-alive disabled: rig_get_freq unavailable rc=" << rc;
      return;
    }

  ++cat_keep_alive_failures_;
  qWarning ().noquote ()
    << "[CATDBG] Hamlib CAT keep-alive failed"
    << cat_keep_alive_failures_ << "/" << kCatKeepAliveMaxFailures_
    << "rc=" << rc;
  if (cat_keep_alive_failures_ >= kCatKeepAliveMaxFailures_)
    {
      cat_keep_alive_ = false;
      stop_cat_keep_alive_timer ();
      qWarning ().noquote ()
        << "[CATDBG] Hamlib CAT keep-alive disabled for this connection after repeated failures";
    }
}

void HamlibTransceiver::reset_qmx_swr_filter (bool tx_active, QString const& reason)
{
  if (!qmx_raw_swr_)
    {
      return;
    }

  qmx_swr_filter_.reset ();
  qmx_swr_filter_tx_active_ = tx_active;
  ++qmx_swr_transition_serial_;
  qmx_swr_threshold_hundredths_ = configured_qmx_swr_threshold_hundredths ();
  qmx_swr_transition_clock_.restart ();
  update_swr (0);
  qInfo ().noquote ()
    << "[QMX-SWR] filter reset"
    << "state=" << (tx_active ? QStringLiteral ("TX") : QStringLiteral ("RX"))
    << "threshold=" << QString::number (qmx_swr_threshold_hundredths_ / 100.0, 'f', 2)
    << "reason=" << reason;
}

// I due parametri del filtro QMX servono solo dentro #if HAVE_HAMLIB_SEND_RAW:
// con una Hamlib priva di rig_send_raw quel blocco sparisce e restano
// inutilizzati, che con -Werror=unused-parameter ferma la build.
void HamlibTransceiver::poll_transmit_telemetry (bool force_signal,
                                                 [[maybe_unused]] bool ignore_qmx_swr_sample,
                                                 [[maybe_unused]] int scheduled_delay_ms)
{
  auto * rig = m_->rig_.data ();
  if (!rig || !rig->caps)
    {
      return;
    }

  bool const tx_active = ptt_on_ || state ().ptt ();

  // 1.0.581 — strumenti del finale. Uno schema solo, riusato da entrambi i rami
  // della funzione: quattro varianti dello stesso schema sarebbero quattro
  // posti dove sbagliare, e questa definizione sta PRIMA del bivio proprio
  // perche' la prima versione stava dopo — nel ramo di trasmissione — e a
  // riposo non veniva mai raggiunta. La manopola della potenza, che a riposo e'
  // l'unica leggibile, non arrivava mai da nessuna parte.
  auto const leggi_livello = [&] (bool attivo, setting_t livello, char const * nome,
                                  auto&& applica) {
    if (!attivo) return;
    value_t v {};
    int const rc_l = rig_get_level (rig, RIG_VFO_CURR, livello, &v);
    if (RIG_OK == rc_l && std::isfinite (v.f))
      {
        applica (static_cast<double> (v.f), true);
      }
    else
      {
        CAT_TRACE ("rig_get_level " << nome << " failed with rc:" << rc_l << "ignoring");
        applica (0.0, false);
      }
  };

  auto const posa_temp = [this] (double v, bool ok) {
    update_pa_temp (ok ? static_cast<int> (std::lround (v * 10.0)) : 0, ok);
  };

  auto const posa_vd = [this] (double v, bool ok) {
    update_vd (ok ? static_cast<unsigned int> (std::lround (v * 100.0)) : 0u, ok);
  };

  auto const posa_id = [this] (double v, bool ok) {
    update_id (ok ? static_cast<unsigned int> (std::lround (v * 100.0)) : 0u, ok);
  };

  if (qmx_raw_swr_ && tx_active != qmx_swr_filter_tx_active_)
    {
      reset_qmx_swr_filter (tx_active, QStringLiteral ("telemetry-state-transition"));
    }
  if (!tx_active)
    {
      update_power (0);
      update_swr (0);
      update_alc (0);

      // In ricezione l'unica cosa che c'e' da misurare e' il segnale che
      // arriva. Hamlib lo da' in dB rispetto a S9 (interi: -54 e' S0, 0 e'
      // S9, +20 e' S9+20), che e' la scala con cui lo legge l'operatore.
      if (do_strength_)
        {
          if (++strength_tick_ >= kStrengthSkipRatio_)
            {
              strength_tick_ = 0;
              value_t s_meter;
              int const rc_s = rig_get_level (rig, RIG_VFO_CURR, RIG_LEVEL_STRENGTH, &s_meter);
              if (RIG_OK == rc_s)
                {
                  strength_failures_ = 0;
                  update_level (s_meter.i);
                }
              else if (++strength_failures_ >= kStrengthMaxFailures_)
                {
                  // Un rig che dichiara l'S-meter e poi non risponde non va
                  // interrogato per sempre: si smette, e il resto del CAT
                  // non paga il conto.
                  do_strength_ = false;
                  qWarning ().noquote ()
                    << "[CATDBG] S-meter polling disabled after repeated failures"
                    << "rc=" << rc_s;
                }
              else
                {
                  CAT_TRACE ("rig_get_level RIG_LEVEL_STRENGTH failed with rc:" << rc_s << "ignoring");
                }
            }
        }
      // A riposo si leggono le quattro cose che a riposo significano qualcosa:
      // la manopola della potenza — che e' un'impostazione, non una misura, e in
      // trasmissione non cambia — la temperatura del finale, che si guarda
      // proprio DOPO aver trasmesso mentre scende, e tensione e corrente di
      // alimentazione, che a riposo raccontano lo stato dell'alimentatore.
      // Senza queste due il gateway continuerebbe a spedire i valori
      // dell'ultima trasmissione, e un numero vecchio su un quadrante non si
      // distingue da uno appena letto. Restano fuori solo ALC, ROS, potenza
      // diretta e compressione: a fermo non esistono.
      //
      // Ritmo rallentato: la manopola cambia quando la gira l'operatore, non
      // dodici volte al secondo, e l'alimentazione a riposo non ha fretta.
      if (++rx_meter_tick_ >= 4)
        {
          rx_meter_tick_ = 0;
          leggi_livello (do_rfpower_, RIG_LEVEL_RFPOWER, "RIG_LEVEL_RFPOWER",
                         [this] (double v, bool ok) {
                           // Hamlib la da' come frazione 0..1 del massimo del rig.
                           update_rfpower (ok ? static_cast<unsigned int> (std::lround (v * 1000.0)) : 0u, ok);
                         });
          leggi_livello (do_pa_temp_, RIG_LEVEL_TEMP_METER, "RIG_LEVEL_TEMP_METER", posa_temp);
          leggi_livello (do_vd_, RIG_LEVEL_VD_METER, "RIG_LEVEL_VD_METER", posa_vd);
          leggi_livello (do_id_, RIG_LEVEL_ID_METER, "RIG_LEVEL_ID_METER", posa_id);
        }

      if (force_signal)
        {
          update_complete (true);
        }
      return;
    }

  value_t strength {};
  int rc {RIG_OK};
  if (do_swr_)
    {
#if HAVE_HAMLIB_SEND_RAW
      if (qmx_raw_swr_)
        {
          static unsigned char const command[] {'S', 'W', ';'};
          unsigned char response[32] {};
          unsigned char terminator[] {';', '\0'};
          rc = rig_send_raw (rig, command, sizeof command, response, sizeof response, terminator);
          QByteArray const frame = rc >= 0
              ? QByteArray {reinterpret_cast<char const *> (response), rc}
              : QByteArray {};
          unsigned int swrHundredths = 0;
          if (rc >= 0 && decodium::qmx_telemetry::parse_swr_hundredths (frame, &swrHundredths))
            {
              qmx_raw_swr_failures_ = 0;
              qint64 const transition_ms = qmx_swr_transition_clock_.isValid ()
                  ? qmx_swr_transition_clock_.elapsed () : -1;
              bool const settling_sample = ignore_qmx_swr_sample
                  || (transition_ms >= 0 && transition_ms < 200);
              auto const filtered = qmx_swr_filter_.process (
                  swrHundredths,
                  qmx_swr_threshold_hundredths_,
                  settling_sample);
              update_swr (filtered.published_hundredths);
              qInfo ().noquote ()
                << "[QMX-SWR] sample"
                << "raw=" << QString::number (filtered.raw_hundredths / 100.0, 'f', 2)
                << "filtered=" << QString::number (filtered.filtered_hundredths / 100.0, 'f', 2)
                << "published=" << QString::number (filtered.published_hundredths / 100.0, 'f', 2)
                << "threshold=" << QString::number (qmx_swr_threshold_hundredths_ / 100.0, 'f', 2)
                << "samples=" << filtered.samples
                << "consecutive_high=" << filtered.consecutive_high
                << "transition_ms=" << transition_ms
                << "scheduled_ms=" << scheduled_delay_ms
                << "stop=" << filtered.stop_eligible
                << "reason=" << decodium::qmx_telemetry::swr_filter_decision_name (filtered.decision);
            }
          else
            {
              ++qmx_raw_swr_failures_;
              CAT_TRACE ("QMX raw SW telemetry failed rc=" << rc << " reply=" << frame);
              auto const filtered = qmx_swr_filter_.process (
                  0, qmx_swr_threshold_hundredths_, false);
              update_swr (filtered.published_hundredths);
              qInfo ().noquote ()
                << "[QMX-SWR] sample"
                << "raw=invalid"
                << "filtered=0.00"
                << "published=0.00"
                << "threshold=" << QString::number (qmx_swr_threshold_hundredths_ / 100.0, 'f', 2)
                << "samples=" << filtered.samples
                << "consecutive_high=" << filtered.consecutive_high
                << "scheduled_ms=" << scheduled_delay_ms
                << "stop=0"
                << "reason=" << decodium::qmx_telemetry::swr_filter_decision_name (filtered.decision);
              if (qmx_raw_swr_failures_ >= kQmxRawTelemetryMaxFailures_)
                {
                  do_swr_ = false;
                  qWarning ().noquote ()
                    << "[CATDBG] QMX raw SWR telemetry disabled after repeated failures"
                    << "rc=" << rc << "reply=" << frame;
                }
            }
        }
      else
#endif
        {
          rc = rig_get_level (rig, RIG_VFO_CURR, RIG_LEVEL_SWR, &strength);
          if (RIG_OK == rc && tx_active)
            {
              update_swr (strength.f >= 1.000 ? static_cast<unsigned int> (strength.f * 100) : 0);
            }
          else
            {
              CAT_TRACE ("rig_get_level RIG_LEVEL_SWR failed with rc:" << rc << "ignoring");
              update_swr (0);
            }
        }
    }

  // 1.0.323 — ALC: scala Hamlib 0.0..1.0 → 0..100. Some backends return an
  // already scaled meter, so accept values above 1.5 as 0..100-ish directly.
  if (do_alc_)
    {
      rc = rig_get_level (rig, RIG_VFO_CURR, RIG_LEVEL_ALC, &strength);
      if (RIG_OK == rc && tx_active)
        {
          if (alc_probe_pending_)
            {
              alc_probe_pending_ = false;
              qInfo ().noquote () << "[CATDBG] Hamlib ALC opportunistic probe succeeded";
            }
          double const rawAlc = std::isfinite (strength.f) ? strength.f : 0.0;
          unsigned int alc = 0;
          if (rawAlc > 1.5)
            {
              alc = static_cast<unsigned int> (std::round (rawAlc));
            }
          else if (rawAlc > 0.0)
            {
              alc = static_cast<unsigned int> (std::round (rawAlc * 100.0));
            }
          update_alc (alc, true);
        }
      else
        {
          CAT_TRACE ("rig_get_level RIG_LEVEL_ALC failed with rc:" << rc << "ignoring");
          if (rc == -RIG_ENAVAIL || rc == -RIG_ENIMPL)
            {
              if (do_alc_)
                {
                  qInfo ().noquote () << "[CATDBG] Hamlib ALC unavailable; disabling ALC polling rc=" << rc;
                }
              do_alc_ = false;
              alc_probe_pending_ = false;
            }
          else if (alc_probe_pending_)
            {
              // 1.0.352 - il probe ALC OPPORTUNISTICO (cap mask non dichiara ALC) ha
              // fallito con rc != ENAVAIL/ENIMPL (es. -RIG_ETIMEOUT su seriale/Net
              // congestionata). Senza questo, si ritenterebbe ad OGNI tick di TX
              // (telemetry non throttlata in TX) -> transazione seriale inutile per
              // tutta la durata del TX. Chiudi il probe best-effort. I rig che
              // DICHIARANO ALC (alc_probe_pending_=false) non sono toccati.
              qInfo ().noquote () << "[CATDBG] Hamlib ALC opportunistic probe failed rc=" << rc << "; disabling probe";
              do_alc_ = false;
              alc_probe_pending_ = false;
            }
          update_alc (0, false);
        }
    }

  // Hamlib da' VD e ID in volt e ampere, la temperatura in gradi e la
  // compressione in dB: le scale intere sono affare nostro, e stanno qui e
  // basta perche' chi legge piu' avanti non debba ricordarsele.
  // Tensione, corrente e compressione hanno senso solo mentre si trasmette, e
  // fuori dalla trasmissione sarebbero tre transazioni per niente su un bus che
  // e' gia' il collo di bottiglia. La temperatura no: quella si guarda DOPO,
  // mentre il finale si raffredda, quindi si legge sempre.
  leggi_livello (do_vd_, RIG_LEVEL_VD_METER, "RIG_LEVEL_VD_METER", posa_vd);
  leggi_livello (do_id_, RIG_LEVEL_ID_METER, "RIG_LEVEL_ID_METER", posa_id);
  leggi_livello (do_pa_temp_, RIG_LEVEL_TEMP_METER, "RIG_LEVEL_TEMP_METER", posa_temp);
  leggi_livello (do_comp_ && tx_active, RIG_LEVEL_COMP_METER, "RIG_LEVEL_COMP_METER",
                 [this] (double v, bool ok) {
                   update_comp (ok ? static_cast<unsigned int> (std::lround (v * 10.0)) : 0u, ok);
                 });

  if (do_pwr_)
    {
#if HAVE_HAMLIB_SEND_RAW
      if (qmx_raw_power_)
        {
          static unsigned char const command[] {'P', 'C', ';'};
          unsigned char response[32] {};
          unsigned char terminator[] {';', '\0'};
          rc = rig_send_raw (rig, command, sizeof command, response, sizeof response, terminator);
          QByteArray const frame = rc >= 0
              ? QByteArray {reinterpret_cast<char const *> (response), rc}
              : QByteArray {};
          unsigned int milliwatts = 0;
          if (rc >= 0 && decodium::qmx_telemetry::parse_power_milliwatts (frame, &milliwatts))
            {
              qmx_raw_power_failures_ = 0;
              update_power (milliwatts);
            }
          else
            {
              ++qmx_raw_power_failures_;
              CAT_TRACE ("QMX raw PC telemetry failed rc=" << rc << " reply=" << frame);
              update_power (0);
              if (qmx_raw_power_failures_ >= kQmxRawTelemetryMaxFailures_)
                {
                  do_pwr_ = false;
                  qWarning ().noquote ()
                    << "[CATDBG] QMX raw power telemetry disabled after repeated failures"
                    << "rc=" << rc << "reply=" << frame;
                }
            }
        }
      else
#endif
        {
          rc = rig_get_level (rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER_METER_WATTS, &strength);
          if (RIG_OK == rc)
            {
              update_power (static_cast<unsigned int> (strength.f * 1000));
            }
          else
            {
              CAT_TRACE ("rig_get_level RFPOWER_METER_WATTS failed with rc:" << rc << "ignoring");
              update_power (0);
            }
        }
    }
  else if (do_pwr2_)
    {
      rc = rig_get_level (rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &strength);
      if (RIG_OK == rc)
        {
          unsigned int mwpower {0};
          freq_t const f = state ().tx_frequency () ? state ().tx_frequency () : state ().frequency ();
          rmode_t const m = m_->map_mode (state ().mode ());
          rc = rig_power2mW (rig, &mwpower, strength.f, f, m);
          if (RIG_OK != rc)
            {
              CAT_TRACE ("rig_power2mW failed with rc:" << rc << "ignoring");
              mwpower = 0;
            }
          update_power (mwpower);
        }
      else
        {
          CAT_TRACE ("rig_get_level RFPOWER failed with rc:" << rc << "ignoring");
          update_power (0);
        }
    }
  else
    {
      update_power (0);
    }

  if (force_signal)
    {
      update_complete (true);
    }
}

void HamlibTransceiver::schedule_transmit_telemetry_burst ()
{
  if (!do_pwr_ && !do_pwr2_ && !do_swr_ && !do_alc_)
    {
      return;
    }

  quint64 const qmx_transition_serial = qmx_swr_transition_serial_;
  auto schedule_poll = [this, qmx_transition_serial] (int delay_ms)
    {
      QTimer::singleShot (delay_ms, this, [this, delay_ms, qmx_transition_serial] {
        if (qmx_raw_swr_ && qmx_transition_serial != qmx_swr_transition_serial_)
          {
            qInfo ().noquote ()
              << "[QMX-SWR] scheduled sample cancelled"
              << "scheduled_ms=" << delay_ms
              << "reason=PTT-transition-changed";
            return;
          }
        try
          {
            bool const ignore_qmx_sample = qmx_raw_swr_
                && decodium::qmx_telemetry::ignore_scheduled_swr_sample (delay_ms);
            poll_transmit_telemetry (true, ignore_qmx_sample, delay_ms);
          }
        catch (std::exception const& e)
          {
            CAT_TRACE ("early PWR/SWR poll failed:" << e.what () << "ignoring");
          }
        catch (...)
          {
            CAT_TRACE ("early PWR/SWR poll failed unexpectedly, ignoring");
          }
      });
    };

  if (qmx_raw_swr_)
    {
      for (int const delay_ms : decodium::qmx_telemetry::swr_poll_delays_ms)
        {
          schedule_poll (delay_ms);
        }
    }
  else
    {
      static constexpr int delays_ms[] {120, 350, 700, 1100};
      for (int const delay_ms : delays_ms)
        {
          schedule_poll (delay_ms);
        }
    }
}

void HamlibTransceiver::do_ptt (bool on)
{
    CAT_TRACE ("PTT: " << on << " " << state () << " reversed=" << m_->reversed_);
  if (on)
    {
       if (RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt)
        {
          CAT_TRACE ("rig_set_ptt PTT=true");
          auto ptt_type = rig_get_caps_int (m_->model_, RIG_CAPS_PTT_TYPE);
          m_->error_check (rig_set_ptt (m_->rig_.data (), RIG_VFO_CURR
                                        , RIG_PTT_RIG_MICDATA == ptt_type && m_->back_ptt_port_
                                        ? RIG_PTT_ON_DATA : RIG_PTT_ON), tr ("setting PTT on"));
          ptt_on_ = true;   // set AFTER successful rig_set_ptt
        }
    }
  else
    {
      if (RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt)
        {
          // 1.0.366 — robust PTT release on EVERY TX-off (not just shutdown).
          // The old path used error_check() which THROWS on the first hamlib
          // error (e.g. Icom CI-V bus error / timeout on a congested COM port):
          // a single failure left ptt_on_ true and the radio stuck in TX.
          // Drop PTT best-effort instead — no throw, a few retries — so a
          // transient bus glitch does not strand the rig in transmit. We do NOT
          // signal a UI error here: a failed PTT-off must not abort the
          // surrounding TX-teardown sequence. The PTT-on path above keeps
          // throwing, since a failed TX start SHOULD surface to the user.
          // 1.0.474 — use a dynamic cap: the Icom serial adaptive path gets
          // one attempt to avoid compounding long Hamlib timeouts, while other
          // rigs keep the previous bounded retry behavior.
          int rc = -RIG_EIO;
          int const attempts = ptt_off_attempt_limit (false);
          for (int attempt = 0; attempt < attempts; ++attempt)
            {
              rc = rig_set_ptt (m_->rig_.data (), RIG_VFO_CURR, RIG_PTT_OFF);
              CAT_TRACE ("rig_set_ptt PTT=false attempt=" << attempt << " rc=" << rc);
              if (RIG_OK == rc)
                {
                  break;
                }
            }
          if (RIG_OK == rc)
            {
              ptt_on_ = false;  // set AFTER successful rig_set_ptt
              ptt_off_failed_recently_ = false;
            }
          else
            {
              // Leave ptt_on_ true so do_stop()'s release retry (1.0.365) and
              // any later TX-off attempt know the rig may still be keyed.
              ptt_off_failed_recently_ = true;
              qWarning ().noquote ()
                << "[CATDBG] Hamlib PTT-off failed after retries rc=" << rc
                << "attempts=" << attempts
                << "— radio may still be transmitting (bus error/timeout)";
            }
        }
    }

  // 1.0.367 — report the ACTUAL PTT state, not the requested one. If a PTT-off
  // failed above (rig may still be keyed) ptt_on_ stays true; propagating
  // update_PTT(false) would desync the app (UI thinks RX while the rig is still
  // transmitting). Before 1.0.366 error_check() threw before reaching this
  // line, so the inconsistency could not occur; the no-throw release reopened
  // it. Use the real state when we control a PTT line; fall back to the
  // requested value only when there is no PTT port (RIG_PTT_NONE / VOX).
  bool const effective_ptt =
      (RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt) ? ptt_on_ : on;
  if (qmx_raw_swr_ && effective_ptt != qmx_swr_filter_tx_active_)
    {
      reset_qmx_swr_filter (
          effective_ptt,
          effective_ptt ? QStringLiteral ("RX-to-TX") : QStringLiteral ("TX-to-RX"));
    }
  update_PTT (effective_ptt);
  if (on)
    {
      schedule_transmit_telemetry_burst ();
    }
}

// pass in false if any post_action is needed for a rig -- don't know of any as of 2024-04-14
void HamlibTransceiver::send_morse (QString const& text, int wpm) noexcept
{
  try
    {
      if (!m_->rig_ || text.isEmpty ()) return;
      CAT_TRACE ("send_morse: '" << text << "' wpm=" << wpm);
      if (wpm > 0)
        {
          value_t v; v.i = wpm;
          rig_set_level (m_->rig_.data (), RIG_VFO_CURR, RIG_LEVEL_KEYSPD, v);
        }
      rig_send_morse (m_->rig_.data (), RIG_VFO_CURR, text.toLatin1 ().constData ());
    }
  catch (...)
    {
      // slot noexcept: non propagare eccezioni fuori dal thread del transceiver
    }
}

void HamlibTransceiver::do_tune (bool on)
{
  CAT_TRACE ("Tune: " << on << " " << state ());
  if (on)
    {
       if (RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt)
        {
          update_PTT (true); // we'll change the PTT button while we do this
          CAT_TRACE ("rig_vfo_opt RIG_VFO_OP_TUNE=" << on);
          // ptt button will stay lit if error message is displaye
          m_->error_check(rig_vfo_op (m_->rig_.data (), RIG_VFO_CURR, RIG_OP_TUNE), "turning TUNE on");
          update_PTT (false);
        }
    }
#if 0
  else // do we need to be able to turn PTT off on anybody?
    {
      if (RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt)
        {
          ptt_on_ = false;
          CAT_TRACE ("rig_set_ptt PTT=false");
          m_->error_check (rig_set_ptt (m_->rig_.data (), RIG_VFO_CURR, RIG_PTT_OFF), tr ("setting PTT off"));
        }
    }
#endif
}
