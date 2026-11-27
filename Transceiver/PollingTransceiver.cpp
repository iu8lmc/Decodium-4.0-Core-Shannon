#include "PollingTransceiver.hpp"

#include <exception>

#include <QtGlobal>      // qBound, per il passo di polling in trasmissione
#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QDir>
#include <QStandardPaths>
#include "moc_PollingTransceiver.cpp"

namespace
{
  unsigned const polls_to_stabilize {3};
  unsigned const generic_poll_failures_to_tolerate {3};
  unsigned const transient_io_poll_failures_to_tolerate {8};
  int const ptt_fast_poll_first_ms {300};
  int const ptt_fast_poll_second_ms {800};

  int base_poll_interval_ms (int poll_interval)
  {
    // TransceiverFactory packs feature flags in the high bits of
    // poll_interval. The low 16 bits are the user-visible seconds.
    int seconds = poll_interval & 0xffff;
    if (seconds <= 0 && poll_interval > 0 && poll_interval < 0x10000)
      {
        seconds = poll_interval;
      }
    if (seconds <= 0)
      {
        seconds = 1;
      }
    return seconds * 1000;
  }

  // Cadenza durante la trasmissione. Un quarto dell'intervallo scelto
  // dall'utente, ma mai sotto i 250 ms: piu' fitto di cosi' non lo si
  // vedrebbe comunque sull'ago e si comincerebbe a caricare la seriale,
  // che e' esattamente il male da cui si veniva (il polling PWR/SWR
  // bloccava il thread principale su rig lenti, risolto in 1.0.204). In
  // ricezione non cambia niente: li' l'intervallo resta quello pieno.
  int tx_poll_interval_ms (int base_ms)
  {
    int const quarto = base_ms / 4;
    return qBound (250, quarto > 0 ? quarto : 250, base_ms);
  }

  bool is_transient_poll_io_failure (QString const& message)
  {
    QString const lower = message.toLower ();
    return lower.contains ("timed out")
        || lower.contains ("timeout")
        || lower.contains ("communication timed out")
        || lower.contains ("io error")
        || lower.contains ("input/output error")
        || lower.contains ("device not configured")
        || lower.contains ("device unavailable")
        || lower.contains ("resource temporarily unavailable")
        || lower.contains ("temporarily unavailable")
        || lower.contains ("read_string_generic")
        || lower.contains ("write_block")
        || lower.contains ("read_block");
  }
}

PollingTransceiver::PollingTransceiver (logger_type * logger, int poll_interval, QObject * parent)
  : TransceiverBase {logger, parent}
  , interval_ {base_poll_interval_ms (poll_interval)}
  , tx_interval_ {tx_poll_interval_ms (base_poll_interval_ms (poll_interval))}
  , poll_timer_ {nullptr}
  , retries_ {0}
  , poll_failures_ {0}
{
}

void PollingTransceiver::start_timer ()
{
  if (interval_)
    {
      if (!poll_timer_)
        {
          poll_timer_ = new QTimer {this}; // pass ownership to
                                           // QObject which handles
                                           // destruction for us

          connect (poll_timer_, &QTimer::timeout, this,
                   &PollingTransceiver::handle_timeout);
        }
      poll_timer_->start (tx_fast_active_ ? tx_interval_ : interval_);
    }
  else
    {
      stop_timer ();
    }
}

void PollingTransceiver::stop_timer ()
{
  if (poll_timer_)
    {
      poll_timer_->stop ();
    }
}

void PollingTransceiver::stop_polling ()
{
  polling_stopped_ = true;
  stop_timer ();
}

void PollingTransceiver::do_post_start ()
{
  polling_stopped_ = false;
  start_timer ();
  if (!next_state_.online ())
    {
      // remember that we are expecting to go online
      next_state_.online (true);
      retries_ = polls_to_stabilize;
    }
}

void PollingTransceiver::do_post_stop ()
{
  // not much point waiting for rig to go offline since we are ceasing
  // polls
  stop_polling ();
}

void PollingTransceiver::do_post_frequency (Frequency f, MODE m)
{
  // take care not to set the expected next mode to unknown since some
  // callers use mode == unknown to signify that they do not know the
  // mode and don't care
  if (next_state_.frequency () != f || (m != UNK && next_state_.mode () != m))
    {
      // update expected state with new frequency and set poll count
      next_state_.frequency (f);
      if (m != UNK)
        {
          next_state_.mode (m);
        }
      retries_ = polls_to_stabilize;
    }
}

void PollingTransceiver::do_post_tx_frequency (Frequency f, MODE)
{
  if (next_state_.tx_frequency () != f)
    {
      // update expected state with new TX frequency and set poll
      // count
      next_state_.tx_frequency (f);
      next_state_.split (f); // setting non-zero TX frequency means split
      retries_ = polls_to_stabilize;
    }
}

void PollingTransceiver::do_post_mode (MODE m)
{
  // we don't ever expect mode to goto to unknown
  if (m != UNK && next_state_.mode () != m)
    {
      // update expected state with new mode and set poll count
      next_state_.mode (m);
      retries_ = polls_to_stabilize;
    }
}

void PollingTransceiver::do_post_ptt (bool p)
{
  bool const changed = next_state_.ptt () != p;
  if (changed)
    {
      // update expected state with new PTT and set poll count
      next_state_.ptt (p);
      retries_ = polls_to_stabilize;
      //retries_ = 0;             // fast feedback on PTT
    }
  else
    {
      next_state_.ptt(p);         // ensure this is initialized
    }

  if (changed && !polling_stopped_ && poll_timer_ && poll_timer_->isActive ())
    {
      QTimer::singleShot (ptt_fast_poll_first_ms, this, &PollingTransceiver::handle_timeout);
      QTimer::singleShot (ptt_fast_poll_second_ms, this, &PollingTransceiver::handle_timeout);

      // Le due letture qui sopra confermano in fretta il cambio di stato,
      // ma poi si tornava a una lettura al secondo per tutto l'over: i
      // misuratori di potenza e ROS restavano fermi sullo stesso numero, e
      // da remoto sembrava che lo strumento non funzionasse. Finche' si
      // trasmette si tiene il passo svelto, e appena si torna in ascolto
      // si rimette quello pieno.
      set_fast_tx_polling (p);
    }
}

// Cambia la cadenza del timer senza fermarlo e riavviarlo: setInterval su
// un timer attivo riprogramma il prossimo scatto, quindi non si perde il
// giro in corso ne' si introduce uno scarto.
void PollingTransceiver::set_fast_tx_polling (bool on)
{
  if (tx_fast_active_ == on)
    {
      return;
    }
  tx_fast_active_ = on;
  if (poll_timer_ && poll_timer_->isActive ())
    {
      poll_timer_->setInterval (on ? tx_interval_ : interval_);
    }
}

bool PollingTransceiver::do_pre_update ()
{
  // if we are holding off a change then withhold the signal
  if (retries_ && state () != next_state_)
    {
      return false;
    }
  return true;
}

void PollingTransceiver::handle_timeout ()
{
  if (polling_stopped_ || !poll_timer_ || !poll_timer_->isActive ())
    {
      return;
    }

  QString message;
  bool force_signal {false};

  // we must catch all exceptions here since we are called by Qt and
  // inform our parent of the failure via the offline() message
  try
    {
      do_poll ();              // tell sub-classes to update our state
      poll_failures_ = 0;      // poll path recovered/healthy

      // Signal new state if it what we expected or, hasn't become
      // what we expected after polls_to_stabilize polls. Unsolicited
      // changes will be signalled immediately unless they intervene
      // in a expected sequence where they will be delayed.
      if (retries_)
        {
          --retries_;
          if (state () == next_state_ || !retries_)
            {
              // the expected state has arrived or there are no more
              // retries
              force_signal = true;
            }
        }
      else if (state () != last_signalled_state_)
        {
          // here is the normal passive polling path where state has
          // changed asynchronously
          force_signal = true;
        }

      if (force_signal)
        {
          // reset everything, record and signal the current state
          retries_ = 0;
          next_state_ = state ();
          last_signalled_state_ = state ();
          update_complete (true);
        }
    }
  catch (std::exception const& e)
    {
      CAT_WARNING ("poll failed:" << e.what ());
      message = e.what ();
    }
  catch (...)
    {
      CAT_WARNING ("poll failed: unexpected exception");
      message = tr ("Unexpected rig error");
    }
  if (!message.isEmpty ())
    {
      // CAT backends can occasionally miss one poll (USB/serial jitter,
      // temporary rig busy, log/write contention). Avoid full disconnect
      // unless failures are consecutive. Serial/USB timeout storms are common
      // on Icom CI-V while the rig is busy; keep them local to polling so a
      // single missed frequency read does not tear down and reopen CAT.
      unsigned const tolerated_failures = is_transient_poll_io_failure (message)
          ? transient_io_poll_failures_to_tolerate
          : generic_poll_failures_to_tolerate;
      if (++poll_failures_ < tolerated_failures)
        {
          CAT_WARNING ("poll failure tolerated:" << poll_failures_ << "/" << tolerated_failures << message);
          return;
        }
      poll_failures_ = 0;
      offline (message);
    }
}
