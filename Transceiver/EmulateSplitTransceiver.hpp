#ifndef EMULATE_SPLIT_TRANSCEIVER_HPP__
#define EMULATE_SPLIT_TRANSCEIVER_HPP__

#include <memory>

#include "Transceiver.hpp"

//
// Emulate Split Transceiver
//
// Helper decorator class that encapsulates  the emulation of split TX
// operation.
//
// Responsibilities
//
//  Delegates  all  but setting  of  other  (split) frequency  to  the
//  wrapped Transceiver instance. Also routes failure signals from the
//  wrapped Transceiver instance to this instances failure signal.
//
//  Intercepts status  updates from  the wrapped  Transceiver instance
//  and re-signals it with the emulated status.
//
//  Generates a status update signal if the other (split) frequency is
//  changed, this is necessary  since the wrapped transceiver instance
//  never receives other frequency changes.
//
class EmulateSplitTransceiver final
  : public Transceiver
{
  Q_OBJECT

public:
  // takes ownership of wrapped Transceiver
  explicit EmulateSplitTransceiver (logger_type *,
                                    std::unique_ptr<Transceiver> wrapped,
                                    QObject * parent = nullptr);

  void set (TransceiverState const&,
            unsigned sequence_number) noexcept override;

  // forward everything else to wrapped Transceiver
  void start (unsigned sequence_number) noexcept override {wrapped_->start (sequence_number);}
  void stop () noexcept override {wrapped_->stop ();}
  bool supports_morse () const noexcept override {return wrapped_->supports_morse ();}
  int morse_text_limit () const noexcept override {return wrapped_->morse_text_limit ();}
  QString send_morse_checked (QString const& text, int wpm) override
  {
    return wrapped_->send_morse_checked (text, wpm);
  }
  void send_morse (QString const& text, int wpm) noexcept override
  {
    wrapped_->send_morse (text, wpm);
  }
  void stop_morse () noexcept override {wrapped_->stop_morse ();}
  bool supports_atu_control () const noexcept override {return wrapped_->supports_atu_control ();}
  bool request_atu_tune (QString * error) override {return wrapped_->request_atu_tune (error);}
  bool request_atu_off (QString * error) override {return wrapped_->request_atu_off (error);}
  bool read_atu_state (bool * enabled, QString * error) override
  {
    return wrapped_->read_atu_state (enabled, error);
  }
  bool supports_power_control () const noexcept override {return wrapped_->supports_power_control ();}
  bool request_power_on (QString * error) override {return wrapped_->request_power_on (error);}
  bool request_power_off (QString * error) override {return wrapped_->request_power_off (error);}
  QVariantMap voice_controls (QString const& key, double value, bool write) override
  {
    return wrapped_->voice_controls (key, value, write);
  }

private:
  void handle_update (TransceiverState const&, unsigned seqeunce_number);

  std::unique_ptr<Transceiver> wrapped_;
  Frequency rx_frequency_;        // requested Rx frequency
  Frequency tx_frequency_;        // requested Tx frequency
  bool split_; // requested split state
};

#endif
