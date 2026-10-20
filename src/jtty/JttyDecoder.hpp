// JTTY receiver: the streaming multi-signal decoder of WSJT-X 3.2.
//
// Port di lib/jtty/jtty_mdecode.f90, rjtty_sub.f90, jtty_peakup.f90,
// subtract_jtty.f90, jtty_payload_correlators.f90, ana64a.f90, twkfreq.f90.
//
// JTTY non ha periodi: il ricevitore guarda l'audio a finestre di 1,25 frame
// che avanzano di un quarto di frame (0,472 s). In ogni finestra cerca i
// sincronismi nel canale QSO (RX +/- FTol) e in due canali larghi fissi
// (1350 e 1650 Hz +/- 150), decodifica, sottrae cio' che ha decodificato e
// ricomincia sul residuo; quando sottrae qualcosa ripassa anche le tre
// finestre precedenti, che quel segnale poteva avere sporcato. I frame
// decodificati si ricompongono in messaggi: ogni messaggio ha un
// identificativo e cresce frame dopo frame fino al bit di fine messaggio.
//
// Non e' thread-safe: un ricevitore per thread.

#ifndef DECODIUM_JTTY_DECODER_HPP
#define DECODIUM_JTTY_DECODER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace decodium
{
namespace jtty
{

constexpr int kSampleRate = 12000;
constexpr int kFrameSamples = 59 * 384;                          // 22656 = 1,888 s
constexpr int kStepSamples = kFrameSamples / 4;                  // 5664 = 0,472 s
constexpr int kChunkSamples = kFrameSamples + kFrameSamples / 4; // 28320

struct MessageUpdate
{
  std::int64_t message_id {0};
  float frequency {0.0f};
  // Inizio del messaggio in secondi dal primo campione del flusso.
  double start_seconds {0.0};
  std::string text;          // gia' pronto per l'operatore (display_message_text)
  bool complete {false};     // arrivato il frame con il bit di fine messaggio
};

class Receiver
{
public:
  Receiver ();
  ~Receiver ();
  Receiver (Receiver const&) = delete;
  Receiver& operator= (Receiver const&) = delete;

  // nfa/nfb: il campo del waterfall, che limita i due canali larghi.
  // f0/ftol: il canale QSO. smin: soglia SNR del sincronismo (4,6 in WSJT-X).
  void set_parameters (int nfa, int nfb, float f0, float ftol, float smin = 4.6f);

  // Ricomincia da zero: buffer, messaggi in corso, storia dei frame.
  void reset ();

  // Accoda audio a 12 kHz e decodifica tutte le finestre complete.
  void add_samples (std::int16_t const* samples, int count);

  // Gli aggiornamenti dei messaggi da mostrare, nell'ordine in cui sono nati.
  std::vector<MessageUpdate> take_updates ();

  // Campioni ricevuti dall'ultimo reset().
  std::int64_t samples_received () const;

  // Riga di diagnostica per ogni frame decodificato, nel formato di
  // "rjtty ... ndebug=1": serve al banco che confronta il port con il Fortran.
  void set_debug_sink (std::function<void (std::string const&)> sink);

  // Come rjtty.f90: decodifica un buffer intero, una finestra dopo l'altra,
  // buttando via gli aggiornamenti (restano solo le righe di diagnostica).
  void decode_buffer (std::int16_t const* samples, int count);

private:
  struct Impl;
  std::unique_ptr<Impl> m_;
};

}
}

#endif
