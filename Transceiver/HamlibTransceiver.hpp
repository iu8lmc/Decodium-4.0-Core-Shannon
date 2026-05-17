#ifndef HAMLIB_TRANSCEIVER_HPP_
#define HAMLIB_TRANSCEIVER_HPP_

#include <QString>

#include "TransceiverFactory.hpp"
#include "PollingTransceiver.hpp"
#include "pimpl_h.hpp"

// hamlib transceiver and PTT mostly delegated directly to hamlib Rig class
class HamlibTransceiver final
  : public PollingTransceiver
{
  Q_OBJECT                      // for translation context

public:
  static void register_transceivers (logger_type *, TransceiverFactory::Transceivers *);
  static void unregister_transceivers ();

  explicit HamlibTransceiver (logger_type *, unsigned model_number, TransceiverFactory::ParameterPack const&,
                              QObject * parent = nullptr);
  explicit HamlibTransceiver (logger_type *, TransceiverFactory::PTTMethod ptt_type, QString const& ptt_port,
                              QObject * parent = nullptr);
  ~HamlibTransceiver ();

private:
  void load_user_settings ();
  int do_start () override;
  void do_stop () override;
  void do_frequency (Frequency, MODE, bool no_ignore) override;
  void do_tx_frequency (Frequency, MODE, bool no_ignore) override;
  void do_mode (MODE) override;
  void do_ptt (bool) override;
  void do_tune (bool) override;

  void do_poll () override;
  void poll_transmit_telemetry (bool force_signal = false);
  void schedule_transmit_telemetry_burst ();

  bool ptt_on_ = false;
  bool do_pwr_ = false;
  bool do_pwr2_ = false;
  bool do_swr_ = false;

  // 1.0.204 — throttle telemetry polling: SWR/PWR add ~300ms per poll on slow
  // rigs (FT-991 38400 baud). Polling at full 1Hz blocks the worker thread
  // for ~470ms which propagates as main-thread stall when sendStateSync runs
  // concurrently. Skip telemetry on N-1 ticks of every N (default 4) when
  // any telemetry channel is enabled.
  static constexpr int kTelemetrySkipRatio_ = 4;
  int telemetry_tick_ = 0;

  class impl;
  pimpl<impl> m_;
};

#endif
