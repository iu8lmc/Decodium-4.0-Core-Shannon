// JTTY forward error correction: CRC-12 + tail-biting rate-1/2 convolutional
// code, K=10 (octal 1167/1545), and the list-WAVA decoder of WSJT-X 3.2.
//
// Port di lib/jtty/tbcc.f90, jtty_tbcc_code_profile.f90,
// jtty_tbcc_list_decoder.f90 (percorso "optimized") e jtty_tbcc_decoder.f90.

#ifndef DECODIUM_JTTY_TBCC_HPP
#define DECODIUM_JTTY_TBCC_HPP

#include <array>
#include <complex>
#include <cstdint>
#include <memory>

namespace decodium
{
namespace jtty
{

constexpr int kInformationBits = 46;   // 34 payload + 12 CRC
constexpr int kCrcBits = 12;
constexpr int kSyncSymbols = 13;
constexpr int kFrameSymbols = kSyncSymbols + kInformationBits;   // 59

extern int const kSync13[kSyncSymbols];

using SymbolCorrelations = std::array<std::array<std::complex<float>, 4>, kInformationBits>;

// Payload (34 bit) -> 46 toni 4-FSK (CRC-12 aggiunto, codifica tail-biting).
void tbcc_encode (int const payload[34], int tones[kInformationBits]);

bool tbcc_crc_valid (int const bits[kInformationBits]);

// Scala del decodificatore: blocchi coerenti di 1, 2 e 4 simboli, poi le
// energie dei mezzi simboli. Ogni ricevitore ne tiene una sua: niente stato
// condiviso fra thread.
class TbccDecoder
{
public:
  TbccDecoder ();
  ~TbccDecoder ();
  TbccDecoder (TbccDecoder const&) = delete;
  TbccDecoder& operator= (TbccDecoder const&) = delete;

  bool decode (SymbolCorrelations const& correlations,
               SymbolCorrelations const& half_correlations,
               int payload[34]);

private:
  struct Impl;
  std::unique_ptr<Impl> m_;
};

}
}

#endif
