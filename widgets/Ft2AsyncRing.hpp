// Il ring buffer del decode asincrono FT2 del backend legacy (MainWindow).
//
// MainWindow::dataSink riceve da Detector::framesWritten la posizione
// CUMULATIVA di scrittura in dec_data.d2 dall'inizio del periodo, non il
// numero di campioni nuovi. Il vecchio ciclo copiava a ogni blocco tutto il
// periodo fin li' (qMin(k, 90000) campioni): il ring diventava una sequenza
// di prefissi ripetuti [0..k1][0..k2][0..k3]... e la finestra dei 45 000
// campioni che il decoder vede non era l'audio contiguo degli ultimi 3,75 s.
// Qui si copiano solo i campioni nuovi; quando k riparte si e' in un periodo
// nuovo e si ricomincia da 0. tests/ft2_async_ring_test.cpp lo verifica.

#ifndef FT2_ASYNC_RING_HPP
#define FT2_ASYNC_RING_HPP

#include <algorithm>
#include <cstdint>

namespace decodium
{
namespace ft2
{

constexpr int kAsyncRingSize = 90000;   // 7,5 s a 12 kHz

inline void async_ring_append (short* ring, std::uint64_t& pos, int& last_k, short const* d2, int k)
{
  if (k < last_k) last_k = 0;           // periodo nuovo: d2 riparte da 0
  int const first = std::max (last_k, k - kAsyncRingSize);
  for (int i = first; i < k; ++i)
    {
      ring[pos % kAsyncRingSize] = d2[i];
      ++pos;
    }
  last_k = k;
}

}
}

#endif
