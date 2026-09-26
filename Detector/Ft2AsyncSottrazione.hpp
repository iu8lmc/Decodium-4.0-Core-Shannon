// Sottrazione dei segnali gia' decodificati nel decode FT2 asincrono
// (FT2 asincrono, fase F4, DECODIUM_FT2_ASYNC_AVANTI=1).
//
// Il decode asincrono rilegge gli ultimi 3,75 s (45 000 campioni). Un segnale
// che la finestra contiene solo in parte non si decodifica e quindi il
// decoder non lo sottrae: un debole vicino resta coperto. Qui, prima di ogni
// decode, si tolgono dalla finestra i segnali gia' decodificati nei giri
// precedenti che vi entrano solo in parte; quelli interi li sottrae il
// decoder da se', meglio (stima il dt sulla finestra).
//
// Il retro sweep di JTTY (rifare la finestra precedente senza il forte appena
// trovato) e' stato provato e tolto: al banco genpair 149 finestre rifatte
// hanno dato 1 riga nuova, perche' la finestra precedente di rado contiene
// intero il debole. La perdita vera stava nella sottrazione stessa: vedi
// DECODIUM_FT2_SUB_NFILT in Detector/FtxSubtract.cpp.
//
// Le posizioni sono in campioni assoluti del ring audio (a 12 kHz). Per un
// segnale decodificato la "ancora" e' il campione da cui parte la
// sottrazione: inizio finestra + (DT + 0,5) * 12000 (ibest * 9 nel decoder).

#ifndef DECODIUM_FT2_ASYNC_SOTTRAZIONE_HPP
#define DECODIUM_FT2_ASYNC_SOTTRAZIONE_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

extern "C"
{
  // Toglie da dd (45 000 campioni) il segnale FT2 con quei 77 bit, a f0 Hz,
  // con il dt di sottrazione del decoder (secondi dall'inizio di dd).
  void ftx_ft2_sottrai_bits77_c (float* dd, signed char const* bits77, float f0, float dt);
}

namespace decodium
{
namespace ft2
{

// Uscita del decoder stage7 (ftx_ft2_async_decode_stage7_c).
struct AsyncDecodeOut
{
  static constexpr int kMaxLines = 100;
  static constexpr int kBits = 77;
  static constexpr int kChars = 37;
  int nout {0};
  int snrs[kMaxLines] {};
  float dts[kMaxLines] {};
  float freqs[kMaxLines] {};
  int naps[kMaxLines] {};
  float quals[kMaxLines] {};
  signed char bits77[kMaxLines * kBits] {};
  char decodeds[kMaxLines * kChars] {};
};

class AsyncSottrazione
{
public:
  static constexpr int kRate = 12000;
  static constexpr int kWindow = 45000;
  static constexpr int kNsps = 288;
  static constexpr int kSignal = 103 * kNsps;   // 29 664 campioni, 2,472 s

  long presottratti () const { return m_presottratti; }

  void reset ()
  {
    m_noti.clear ();
    m_lastEnd = -1;
  }

  // Un giro: finestra = gli ultimi 45 000 campioni, fine = posizione assoluta
  // del campione dopo l'ultimo. decode (short* iwave, AsyncDecodeOut& out)
  // chiama il decoder con i parametri del chiamante.
  template <typename Decode>
  void decodifica (short const* finestra, std::int64_t fine, AsyncDecodeOut& out, Decode&& decode)
  {
    if (fine < m_lastEnd) reset ();   // il ring e' ripartito (cambio modo)
    m_lastEnd = fine;
    std::int64_t const inizio = fine - kWindow;
    m_noti.erase (std::remove_if (m_noti.begin (), m_noti.end (),
                                  [inizio] (Noto const& n) { return n.ancora - kNsps + kSignal <= inizio; }),
                  m_noti.end ());
    std::vector<short> w (finestra, finestra + kWindow);
    m_presottratti += presottrai (w.data (), inizio);
    decode (w.data (), out);
    registra (out, inizio);
  }

private:
  struct Noto
  {
    std::array<signed char, 77> bits {};
    float f {0};
    std::int64_t ancora {0};
  };

  void registra (AsyncDecodeOut const& out, std::int64_t inizio)
  {
    for (int i = 0; i < out.nout; ++i)
      {
        signed char const* b = out.bits77 + i * AsyncDecodeOut::kBits;
        std::int64_t const ancora = inizio + std::llround ((out.dts[i] + 0.5) * kRate);
        auto it = std::find_if (m_noti.begin (), m_noti.end (), [&] (Noto const& n) {
          return std::abs (n.f - out.freqs[i]) <= 5.0f && std::llabs (n.ancora - ancora) <= 600
                 && std::equal (n.bits.begin (), n.bits.end (), b);
        });
        if (it == m_noti.end ())
          {
            m_noti.emplace_back ();
            it = m_noti.end () - 1;
            std::copy (b, b + AsyncDecodeOut::kBits, it->bits.begin ());
          }
        // l'ultima stima e' di una finestra che lo contiene intero: la si tiene
        it->f = out.freqs[i];
        it->ancora = ancora;
      }
  }

  // Toglie i noti che la finestra contiene solo in parte. Ritorna quanti.
  int presottrai (short* w, std::int64_t inizio) const
  {
    std::vector<Noto const*> tagliati;
    for (auto const& n : m_noti)
      {
        std::int64_t const s0 = n.ancora - kNsps;
        bool const dentro = s0 + kSignal > inizio && s0 < inizio + kWindow;
        bool const intero = s0 >= inizio && s0 + kSignal <= inizio + kWindow;
        if (dentro && !intero) tagliati.push_back (&n);
      }
    if (tagliati.empty ()) return 0;
    std::vector<float> dd (w, w + kWindow);
    for (Noto const* n : tagliati)
      ftx_ft2_sottrai_bits77_c (dd.data (), n->bits.data (), n->f,
                                static_cast<float> (n->ancora - inizio) / kRate);
    for (int i = 0; i < kWindow; ++i)
      w[i] = static_cast<short> (std::lround (std::max (-32767.0f, std::min (32767.0f, dd[static_cast<std::size_t> (i)]))));
    return static_cast<int> (tagliati.size ());
  }

  std::vector<Noto> m_noti;
  std::int64_t m_lastEnd {-1};
  long m_presottratti {0};
};

}
}

#endif
