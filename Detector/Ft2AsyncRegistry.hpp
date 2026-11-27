// Registro delle trasmissioni FT2 viste dal decode asincrono
// (FT2 asincrono, fase F2).
//
// Il decode asincrono rilegge ogni 100 ms gli ultimi 3,75 s: la stessa
// trasmissione esce da piu' finestre. Il bridge la riconosceva con la chiave
// (slot di 3,75 s calcolato al dispatch, frequenza a gruppi di 20 Hz,
// testo): quando le finestre che la contengono cadono in due slot diversi, o
// la frequenza stimata attraversa il bordo di un gruppo, la stessa riga
// compare due volte (3 su 8 al primo banco).
//
// Qui, come fa JTTY (same_frame / is_recent_frame di jtty_mdecode.f90), una
// trasmissione si riconosce dall'istante ASSOLUTO in cui comincia e dalla sua
// frequenza: stesso testo, frequenza entro 10 Hz, inizio entro 0,6 s. Il DT
// delle righe ha un decimale e la stima del sincronismo balla di qualche
// centesimo fra una finestra e l'altra; due trasmissioni vere dello stesso
// messaggio sulla stessa frequenza distano invece almeno un frame (2,47 s),
// quindi la ripetizione legittima resta visibile.

#ifndef DECODIUM_FT2_ASYNC_REGISTRY_HPP
#define DECODIUM_FT2_ASYNC_REGISTRY_HPP

#include <cmath>
#include <deque>

namespace decodium
{
namespace ft2
{

template <typename Text>
class AsyncRegistry
{
public:
  static constexpr double kFreqToleranceHz = 10.0;
  static constexpr double kStartToleranceSec = 0.6;
  static constexpr double kRetentionSec = 15.0;

  // true se e' una trasmissione nuova (va mostrata), false se e' gia' stata
  // vista. start e now sono in secondi, sulla stessa scala assoluta.
  bool admit (Text const& message, double freq_hz, double start, double now)
  {
    while (!m_entries.empty () && now - m_entries.front ().seen > kRetentionSec) m_entries.pop_front ();
    for (auto& e : m_entries)
      if (e.message == message && std::abs (e.freq - freq_hz) <= kFreqToleranceHz
          && std::abs (e.start - start) <= kStartToleranceSec)
        {
          e.seen = now;
          return false;
        }
    m_entries.push_back ({message, freq_hz, start, now});
    return true;
  }

  void clear () { m_entries.clear (); }

private:
  struct Entry
  {
    Text message;
    double freq;
    double start;
    double seen;
  };
  std::deque<Entry> m_entries;
};

}
}

#endif
