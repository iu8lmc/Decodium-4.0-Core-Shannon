// Il ring del decode asincrono FT2 deve essere una copia CONTIGUA dell'audio
// ricevuto (FT2 asincrono, fase F1).
//
// Si simula il Detector: scrive d2 a blocchi della dimensione reale e
// segnala framesWritten con la posizione cumulativa nel periodo; a fine
// periodo d2 riparte da 0. L'ingresso e' una rampa (ogni campione vale il suo
// indice globale modulo 30000), cosi' ogni discontinuita' nel ring si vede.
//
// Esce con 0 se il ring corretto e' contiguo. Stampa anche che cosa faceva il
// vecchio ciclo, per documentare il difetto.

#include "widgets/Ft2AsyncRing.hpp"

#include <cstdio>
#include <vector>

namespace
{

constexpr int kPeriod = 45000;       // FT2: 3,75 s
constexpr int kBlock = 1728;         // un blocco di Detector (m_samplesPerFFT)
constexpr int kRing = decodium::ft2::kAsyncRingSize;

short ramp (long long n) { return static_cast<short> (n % 30000); }

// Il ciclo di MainWindow::dataSink fino alla 1.0.649, copiato com'era.
void legacy_append (short* ring, std::uint64_t& pos, short const* d2, int k)
{
  int nsamples = std::min (k, 90000);
  int src_start = std::max (0, k - nsamples);
  for (int i = 0; i < nsamples; i++)
    {
      ring[pos % 90000] = d2[src_start + i];
      pos++;
    }
}

// Gli ultimi 45 000 campioni del ring sono contigui?
int discontinuities (short const* ring, std::uint64_t pos)
{
  if (pos < 45000) return 0;
  int bad = 0;
  std::uint64_t const start = pos - 45000;
  for (int i = 1; i < 45000; ++i)
    {
      short const a = ring[(start + i - 1) % kRing];
      short const b = ring[(start + i) % kRing];
      if (b != static_cast<short> ((a + 1) % 30000)) ++bad;
    }
  return bad;
}

}

int main ()
{
  std::vector<short> d2 (kPeriod + kBlock);
  std::vector<short> ring_old (kRing), ring_new (kRing);
  std::uint64_t pos_old = 0, pos_new = 0;
  int last_k = 0;
  long long global = 0;
  int worst_old = 0, worst_new = 0, windows = 0;
  long long copied_old = 0, copied_new = 0;

  for (int period = 0; period < 6; ++period)
    {
      int kin = 0;
      while (kin + kBlock <= kPeriod)
        {
          for (int i = 0; i < kBlock; ++i) d2[static_cast<std::size_t> (kin + i)] = ramp (global++);
          kin += kBlock;
          std::uint64_t const before_old = pos_old, before_new = pos_new;
          legacy_append (ring_old.data (), pos_old, d2.data (), kin);
          decodium::ft2::async_ring_append (ring_new.data (), pos_new, last_k, d2.data (), kin);
          copied_old += static_cast<long long> (pos_old - before_old);
          copied_new += static_cast<long long> (pos_new - before_new);
          ++windows;
          worst_old = std::max (worst_old, discontinuities (ring_old.data (), pos_old));
          worst_new = std::max (worst_new, discontinuities (ring_new.data (), pos_new));
        }
      // Il resto del periodo che non riempie un blocco va perso in entrambi i
      // casi (Detector lo scarta): la rampa globale lo salta.
      global += kPeriod - kin;
    }

  std::printf ("audio in ingresso: %lld campioni in %d blocchi\n", global, windows);
  std::printf ("vecchio ciclo: %lld campioni copiati nel ring, fino a %d discontinuita' nella finestra\n",
               copied_old, worst_old);
  std::printf ("ciclo corretto: %lld campioni copiati, fino a %d discontinuita' nella finestra\n",
               copied_new, worst_new);
  // Con i periodi interi e senza resti la finestra corretta ha al piu' un
  // salto: il confine di periodo, dove Detector scarta il resto non multiplo
  // del blocco (45000 mod 1728 = 72 campioni).
  bool const ok = worst_new <= 1;
  std::printf ("%s\n", ok ? "ring FT2 async: ok" : "ring FT2 async: FALLITO");
  return ok ? 0 : 1;
}
