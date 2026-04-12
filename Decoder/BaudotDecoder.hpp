#ifndef BAUDOT_DECODER_HPP
#define BAUDOT_DECODER_HPP

#include <QString>
#include <QByteArray>

namespace decodium {
namespace rtty {

/**
 * @brief The Baudot (ITA2) Encoder and Decoder.
 * Handles the state machine for LTRS/FIGS shift characters
 * and translates a 5-bit stream to ASCII and vice-versa.
 */
class BaudotHandler {
public:
    enum class ShiftState {
        Letters,
        Figures
    };

    BaudotHandler();

    /// Decodes a 5-bit ITA2 code into an ASCII character.
    /// Updates internal LTRS/FIGS state automatically.
    /// Returns '\0' if the symbol is a non-printable control character.
    QChar decodeSymbol(uint8_t bits);

    /// Resets the decoder state (defaults to Letters)
    void reset();

    /// Encodes an ASCII string into an array of 5-bit ITA2 codes.
    /// Automatically inserts LTRS/FIGS shifts as needed.
    QByteArray encodeString(const QString& text);

private:
    ShiftState current_rx_state_;
    ShiftState current_tx_state_;

    static const char LTRS_TABLE[32];
    static const char FIGS_TABLE[32];

    static const uint8_t CODE_LTRS = 0b11111;
    static const uint8_t CODE_FIGS = 0b11011;
    static const uint8_t CODE_CR   = 0b01000;
    static const uint8_t CODE_LF   = 0b00010;
    static const uint8_t CODE_SPC  = 0b00100;
};

} // namespace rtty
} // namespace decodium

#endif // BAUDOT_DECODER_HPP
