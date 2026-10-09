// JTTY waveform: 4-GFSK, BT=2, 31.25 baud, four tones spaced by the baud.
// Port di lib/jtty/gen_jttywave.f90, genjtty.f90 e lib/gfsk_pulse.f90.

#ifndef DECODIUM_JTTY_WAVE_HPP
#define DECODIUM_JTTY_WAVE_HPP

#include "JttyCodec.hpp"

#include <complex>
#include <vector>

namespace decodium
{
namespace jtty
{

// Campioni per simbolo a 12 kHz: 384 -> 31.25 baud, frame di 1.888 s.
constexpr int kNsps12k = 384;
constexpr float kBt = 2.0f;

// genjtty_frames: 13 simboli di sincronismo + 46 di codice per frame.
std::vector<int> tones_for_frames (std::vector<Frame> const& frames);

// gen_jttywave, forma reale (sin) — quella che va in trasmissione.
std::vector<float> generate_wave (std::vector<int> const& tones, int nsps, float bt,
                                  float fsample, float f0);

// gen_jttywave, forma complessa (tabella di 65536 fasi) — il riferimento che
// il ricevitore sottrae dal segnale decodificato.
std::vector<std::complex<float>> generate_complex_wave (int const* tones, int nsym, int nsps,
                                                        float bt, float fsample, float f0);

}
}

#endif
