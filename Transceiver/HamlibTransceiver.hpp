#ifndef HAMLIB_TRANSCEIVER_HPP_
#define HAMLIB_TRANSCEIVER_HPP_

#include <QString>
#include <QElapsedTimer>
#include <hamlib/rig.h>

#include "QmxTelemetry.hpp"
#include "TransceiverFactory.hpp"
#include "PollingTransceiver.hpp"
#include "pimpl_h.hpp"

class QTimer;

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

  void send_morse (QString const&, int) noexcept override;  // keying CW via Hamlib

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
  void poll_transmit_telemetry (bool force_signal = false,
                                bool ignore_qmx_swr_sample = false,
                                int scheduled_delay_ms = -1);
  void reset_qmx_swr_filter (bool tx_active, QString const& reason);
  void start_cat_keep_alive_timer ();
  void stop_cat_keep_alive_timer ();
  void poll_cat_keep_alive ();
  void schedule_transmit_telemetry_burst ();
  vfo_t frequency_poll_vfo () const;
  bool poll_vfo_frequency (vfo_t, freq_t *, QString const&);
  enum class FrequencyWriteResult
  {
    Applied,
    AppliedWithTransientError,
    Deferred,
    Rejected
  };
  void note_frequency_poll_success ();
  void note_frequency_poll_failure (int, QString const&);
  bool cat_write_backoff_active () const;
  bool suppress_cat_write_during_backoff (QString const& operation) const;
  FrequencyWriteResult set_frequency_or_tolerate (vfo_t, Frequency, QString const& operation);
  int ptt_off_attempt_limit (bool shutdown) const;

  bool ptt_on_ = false;
  bool ptt_off_failed_recently_ = false;
  bool rig_split_control_enabled_ = true;
  bool explicit_frequency_poll_vfo_ = false;
  bool frequency_poll_vfo_logged_ = false;
  bool poll_passive_state_ = true;
  bool poll_frequency_state_ = true;
  bool poll_ptt_state_ = true;
  bool adaptive_frequency_poll_ = false;
  bool cat_keep_alive_ = false;
  int cat_keep_alive_failures_ = 0;
  QTimer * cat_keep_alive_timer_ {nullptr};
  bool do_pwr_ = false;
  bool do_pwr2_ = false;
  bool do_swr_ = false;
  bool do_alc_ = false;  // 1.0.323 — lettura RIG_LEVEL_ALC in TX (ALC automatico, fase 1 display)
  // 1.0.581 — strumenti del finale. Si accendono SOLO se la radio li dichiara
  // nelle proprie capacita': niente sonde opportunistiche come per l'ALC,
  // perche' qui sarebbero quattro interrogazioni in piu' per ogni giro di
  // trasmissione su un bus che e' gia' il collo di bottiglia, e il prezzo lo
  // pagherebbe il PTT. Se la radio non li ha, non si chiedono mai.
  bool do_vd_ = false;
  bool do_id_ = false;
  bool do_pa_temp_ = false;
  bool do_comp_ = false;
  bool do_rfpower_ = false;   // manopola della potenza, letta solo a riposo
  int  rx_meter_tick_ = 0;    // rallenta manopola e temperatura in ricezione
  // S-meter: si legge SOLO in ricezione, dove il ciclo e' gia' rallentato, e
  // con un passo tutto suo. In trasmissione non ha significato e ruberebbe
  // tempo ai misuratori che invece contano.
  bool do_strength_ = false;
  bool alc_probe_pending_ = false;
  bool qmx_raw_power_ = false;
  bool qmx_raw_swr_ = false;
  int qmx_raw_power_failures_ = 0;
  int qmx_raw_swr_failures_ = 0;
  decodium::qmx_telemetry::SwrSafetyFilter qmx_swr_filter_;
  QElapsedTimer qmx_swr_transition_clock_;
  bool qmx_swr_filter_tx_active_ = false;
  unsigned int qmx_swr_threshold_hundredths_ = 250;
  quint64 qmx_swr_transition_serial_ = 0;

  // 1.0.204 — throttle telemetry polling: SWR/PWR add ~300ms per poll on slow
  // rigs (FT-991 38400 baud). Polling at full 1Hz blocks the worker thread
  // for ~470ms which propagates as main-thread stall when sendStateSync runs
  // concurrently. Skip telemetry on N-1 ticks of every N (default 4) when
  // any telemetry channel is enabled.
  static constexpr int kTelemetrySkipRatio_ = 4;
  static constexpr int kQmxRawTelemetryMaxFailures_ = 8;
  static constexpr int kCatKeepAliveIntervalMs_ = 300;
  static constexpr int kCatKeepAliveMaxFailures_ = 3;
  static constexpr int kFrequencyPollMaxFailures_ = 2;
  static constexpr int kFrequencyPollInitialBackoffTicks_ = 2;
  static constexpr int kFrequencyPollMaxBackoffTicks_ = 10;
  static constexpr int kFrequencyPollWriteQuietTicks_ = 2;
  // L'S-meter si legge una volta ogni quattro giri di telemetria: in
  // ricezione il ciclo ne salta gia' tre su quattro, quindi in pratica e'
  // una lettura ogni pochi secondi. Basta per un indicatore che si guarda,
  // e su una seriale lenta non toglie il posto a nient'altro - il costo di
  // una lettura di troppo su questo bus e' documentato dalla nota qui sopra.
  static constexpr int kStrengthSkipRatio_ = 4;
  static constexpr int kStrengthMaxFailures_ = 5;
  int telemetry_tick_ = 0;
  int strength_tick_ = 0;
  int strength_failures_ = 0;
  int frequency_poll_failures_ = 0;
  int frequency_poll_skip_ticks_ = 0;
  int frequency_poll_write_quiet_ticks_ = 0;
  int frequency_poll_backoff_ticks_ = kFrequencyPollInitialBackoffTicks_;

  class impl;
  pimpl<impl> m_;
};

#endif
