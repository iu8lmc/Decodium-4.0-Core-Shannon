// DecoRTTY — ITA2 (Baudot) character tables and shift state machine.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace decortty::dsp {

// The three control codes that never produce a printable character.
inline constexpr uint8_t kBaudotLtrs  = 0x1F;  // 11111 — shift to letters
inline constexpr uint8_t kBaudotFigs  = 0x1B;  // 11011 — shift to figures
inline constexpr uint8_t kBaudotSpace = 0x04;  // 00100
inline constexpr uint8_t kBaudotCr    = 0x08;  // 01000
inline constexpr uint8_t kBaudotLf    = 0x02;  // 00010
inline constexpr uint8_t kBaudotNull  = 0x00;

enum class Shift : uint8_t { Letters, Figures };

// Which figures set to use. US (the amateur-radio default) puts BELL on
// 00101 and the apostrophe on 01010; ITA2 is the international variant.
enum class FiguresSet : uint8_t { US, Ita2 };

// Decode one 5-bit code in the given shift. Returns 0 for codes that print
// nothing (LTRS, FIGS, NULL). CR/LF come back as '\r' / '\n'.
char baudotToChar(uint8_t code, Shift shift, FiguresSet set = FiguresSet::US);

// Reverse map: the 5-bit code for a character, and the shift it lives in.
// Returns false when the character has no ITA2 representation.
bool charToBaudot(char c, uint8_t& code, Shift& shift, FiguresSet set = FiguresSet::US);

// Running shift state with unshift-on-space (USOS), the convention that a
// received SPACE returns the decoder to letters. Real-world RTTY relies on it
// to recover from a corrupted LTRS/FIGS, but it corrupts legitimate figure
// groups like "599 001 002", so it stays switchable.
class ShiftState {
public:
    explicit ShiftState(bool unshiftOnSpace = true) : m_usos(unshiftOnSpace) {}

    void setUnshiftOnSpace(bool on) { m_usos = on; }
    bool unshiftOnSpace() const     { return m_usos; }

    Shift shift() const { return m_shift; }
    void  reset()       { m_shift = Shift::Letters; }

    // Feed a 5-bit code, update the shift, and return the printable character
    // (0 when the code only changed state).
    char feed(uint8_t code, FiguresSet set = FiguresSet::US);

private:
    Shift m_shift{Shift::Letters};
    bool  m_usos{true};
};

// Encode a UTF-8/ASCII string into the 5-bit code stream a transmitter needs,
// inserting LTRS/FIGS shifts as required. Characters with no ITA2 form are
// dropped. The stream always starts from `startShift` and leaves the caller's
// shift in `endShift`.
std::vector<uint8_t> encodeBaudot(const std::string& text,
                                  Shift  startShift,
                                  Shift& endShift,
                                  FiguresSet set = FiguresSet::US);

} // namespace decortty::dsp
