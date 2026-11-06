// DecoRTTY — soft-decision Baudot decoder.
//
// A hard-decision RTTY decoder throws away everything the demodulator knew
// about how confident each bit was, then prints whatever five bits came out.
// This stage keeps the confidences and runs a Viterbi search over a trellis
// whose state is (shift case, previous character), scoring each candidate
// against both the received soft bits and a character-bigram model of real RTTY
// traffic. A bit that landed near the slicer threshold is then decided by
// context — "CQ" wins over "CX", "599" over "5I9" — and a lost LTRS/FIGS is
// recovered from the run of characters that follows it.
#pragma once

#include "dsp/RttyTypes.h"

#include <array>
#include <functional>
#include <vector>

namespace decortty::dsp {

class ViterbiBaudot {
public:
    struct Config {
        // Traceback depth in characters. 0 disables the search entirely and
        // prints hard decisions with zero latency; 4 costs about 0.9 s at 45.45
        // baud and recovers most single-bit errors; 10 is for weak-signal work
        // where the operator will accept 2 s of lag.
        int   depth{4};
        // How much the language model is allowed to override the received bits.
        float priorWeight{0.55f};
        // Trim on the demodulator's log-likelihood ratios. They already arrive
        // in nats; this only lets the operator trade the received bits against
        // the language model when conditions are unusual.
        float llrScale{1.0f};
        bool  unshiftOnSpace{true};
        FiguresSet figuresSet{FiguresSet::US};
    };

    ViterbiBaudot();

    void setConfig(const Config& config);
    const Config& config() const { return m_config; }

    void reset();

    // Feed one demodulated frame. Characters are emitted through the callback
    // once they fall out of the traceback window.
    void push(const SoftFrame& frame, const std::function<void(const DecodedChar&)>& out);

    // Emit everything still inside the window — call at end of transmission.
    void flush(const std::function<void(const DecodedChar&)>& out);

private:
    static constexpr int kCodes     = 32;
    static constexpr int kShifts    = 2;
    // Alphabet index 0 is "nothing received yet"; 1..N are the printable
    // characters the bigram model knows about.
    static constexpr int kAlphabet  = 64;
    static constexpr int kStates    = kShifts * kAlphabet;

    struct Step {
        std::array<int, kStates>     back{};      // previous state index
        std::array<char, kStates>    emitted{};   // character produced, 0 = none
        std::array<uint8_t, kStates> code{};      // the 5-bit code chosen
        std::array<float, kStates>   quality{};
        // Probabilita' che il carattere sia esatto, portata a valle per chi
        // deve scegliere fra due copie della stessa parola.
        std::array<float, kStates>   certainty{};
        std::array<uint8_t, kStates> corrected{};
        int64_t                      sampleIndex{0};
    };

    void  buildLanguageModel();
    int   alphaIndex(char c) const;
    float bigramCost(int prevAlpha, int nextAlpha) const;
    void  emitOldest(const std::function<void(const DecodedChar&)>& out);
    static int stateIndex(int shift, int alpha) { return shift * kAlphabet + alpha; }

    Config m_config;

    std::array<float, kStates> m_metric{};
    std::vector<Step>          m_history;
    bool                       m_primed{false};

    // char → alphabet index, and the bigram log-probability table.
    std::array<int, 128>                              m_alphaOf{};
    std::array<char, kAlphabet>                       m_charOf{};
    int                                               m_alphaCount{1};
    std::vector<float>                                m_bigram;   // kAlphabet^2
    std::vector<float>                                m_unigram;  // kAlphabet
};

} // namespace decortty::dsp
