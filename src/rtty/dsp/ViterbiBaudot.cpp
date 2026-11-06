#include "dsp/ViterbiBaudot.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string_view>

namespace decortty::dsp {

namespace {

// The target is compiled with fast-math, where IEEE infinity is not a valid
// sentinel on Clang.  A finite maximum keeps the unreachable-state marker
// portable to libc++, libstdc++ and MSVC, and reachable metrics are normalised
// after every step so they never approach this value.
constexpr float kUnreachableMetric = std::numeric_limits<float>::max();

// What it costs the search to emit a shift character. LTRS and FIGS carry no
// text, so without a price the decoder would happily explain every ambiguous
// frame as a shift. The value is roughly the surprise of an uncommon letter.
constexpr float kShiftCost = 3.0f;
// Codes that print nothing in the current case (and are not LTRS/FIGS) are
// legal on the wire but almost never intended.
constexpr float kNullCost = 6.0f;

// Training text for the character-bigram model. It is deliberately made of the
// traffic this decoder will actually see — CQ calls, contest exchanges, signal
// reports, rig and antenna descriptions — rather than generic prose, because
// the model's whole job is to know that "DE" follows "CQ" and that "599" is a
// far more likely trigram than "5I9".
constexpr std::string_view kCorpus =
    "CQ CQ CQ DE IU8LMC IU8LMC IU8LMC PSE K "
    "CQ TEST DE W1AW W1AW K "
    "DE DL1ABC DL1ABC KN "
    "RYRYRYRYRYRYRYRYRY THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890 "
    "GM GA GE DR OM TNX FER CALL AND NICE QSO "
    "UR RST 599 599 IN ITALY ITALY "
    "UR RST 579 579 QTH IS ROMA ROMA "
    "NAME IS MARIO MARIO HW CPY "
    "SO HW COPY DR OM BK "
    "TNX FER NICE QSO DR OM 73 73 ES GL "
    "RIG HR IS FLEX 6600 RUNNING 100 WATTS "
    "ANT IS A 3 ELE YAGI UP 15 MTRS "
    "WX HERE IS SUNNY TEMP 22 C "
    "QSL VIA BURO ES LOTW TNX "
    "PSE QSL DIRECT OK "
    "73 ES BEST DX DE IU8LMC SK "
    "TU 599 001 001 "
    "TU 599 042 042 "
    "599 IT 599 IT "
    "TEST DE IU8LMC IU8LMC TEST "
    "NR 123 123 K "
    "AGN AGN PSE RPT UR CALL "
    "SRI QRM QSB HR PSE AGN "
    "COPY IS 100 PERCENT SOLID "
    "THANKS FOR THE CONTACT AND HOPE TO MEET YOU AGAIN "
    "PLEASE SEND YOUR REPORT AGAIN "
    "THE STATION IS RUNNING BARE FOOT INTO A DIPOLE "
    "GOOD LUCK IN THE CONTEST AND SEE YOU DOWN THE LOG "
    "OPERATOR HERE IS USING A HOME BREW TERMINAL UNIT "
    "SIGNAL REPORT 589 WITH SOME FADING ON THE BAND "
    "MANY THANKS FOR CALLING ME ON RADIO TELETYPE "
    "IT IS ALWAYS A PLEASURE TO WORK ANOTHER RTTY STATION "
    "BAND CONDITIONS ARE FAIR TODAY WITH LONG PATH OPENING "
    "MY LOCATOR IS JN61 AND THE POWER IS 50 WATTS "
    "WILL QSL VIA THE BUREAU AND ALSO ON LOGBOOK OF THE WORLD "
    "SORRY FOR THE DELAY THERE WAS A LOT OF NOISE ON THE FREQUENCY "
    "BEST 73 AND GOOD DX TO YOU AND YOUR FAMILY ";

} // namespace

ViterbiBaudot::ViterbiBaudot()
{
    buildLanguageModel();
    reset();
}

void ViterbiBaudot::setConfig(const Config& config)
{
    const bool alphabetAffected = config.figuresSet != m_config.figuresSet;
    m_config = config;
    m_config.depth = std::clamp(m_config.depth, 0, 32);
    if (alphabetAffected)
        buildLanguageModel();
    reset();
}

void ViterbiBaudot::buildLanguageModel()
{
    m_alphaOf.fill(0);
    m_charOf.fill(0);
    m_alphaCount = 1;   // index 0 = "start of stream"

    auto intern = [this](char c) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc >= 128 || m_alphaOf[uc] != 0 || m_alphaCount >= kAlphabet)
            return;
        m_alphaOf[uc]              = m_alphaCount;
        m_charOf[static_cast<size_t>(m_alphaCount)] = c;
        ++m_alphaCount;
    };

    // Intern every character both cases can produce, so the index of a symbol
    // never depends on what the corpus happened to contain.
    for (uint8_t code = 0; code < 32; ++code) {
        if (const char l = baudotToChar(code, Shift::Letters, m_config.figuresSet))
            intern(l);
        if (const char f = baudotToChar(code, Shift::Figures, m_config.figuresSet))
            intern(f);
    }

    m_unigram.assign(static_cast<size_t>(kAlphabet), 0.0f);
    m_bigram.assign(static_cast<size_t>(kAlphabet) * kAlphabet, 0.0f);

    std::vector<float> counts(static_cast<size_t>(kAlphabet) * kAlphabet, 0.0f);
    std::vector<float> unigramCounts(static_cast<size_t>(kAlphabet), 0.0f);

    int prev = 0;
    for (const char raw : kCorpus) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const int  a = alphaIndex(c);
        if (a <= 0) {
            prev = 0;
            continue;
        }
        counts[static_cast<size_t>(prev) * kAlphabet + static_cast<size_t>(a)] += 1.0f;
        unigramCounts[static_cast<size_t>(a)] += 1.0f;
        prev = a;
    }

    // Add-k smoothing. k is generous on purpose: the corpus is small, and a
    // model that assigns zero probability to an unseen pair would refuse to
    // decode any callsign it has not met.
    constexpr float kSmoothing = 0.35f;
    float unigramTotal = 0.0f;
    for (int a = 0; a < m_alphaCount; ++a)
        unigramTotal += unigramCounts[static_cast<size_t>(a)] + kSmoothing;

    for (int a = 0; a < m_alphaCount; ++a) {
        const float p = (unigramCounts[static_cast<size_t>(a)] + kSmoothing) / unigramTotal;
        m_unigram[static_cast<size_t>(a)] = -std::log(p);
    }

    for (int p = 0; p < m_alphaCount; ++p) {
        float rowTotal = 0.0f;
        for (int n = 0; n < m_alphaCount; ++n)
            rowTotal += counts[static_cast<size_t>(p) * kAlphabet + static_cast<size_t>(n)] + kSmoothing;
        for (int n = 0; n < m_alphaCount; ++n) {
            const float prob = (counts[static_cast<size_t>(p) * kAlphabet + static_cast<size_t>(n)] + kSmoothing)
                             / rowTotal;
            m_bigram[static_cast<size_t>(p) * kAlphabet + static_cast<size_t>(n)] = -std::log(prob);
        }
    }
}

int ViterbiBaudot::alphaIndex(char c) const
{
    const auto uc = static_cast<unsigned char>(c);
    return uc < 128 ? m_alphaOf[uc] : 0;
}

float ViterbiBaudot::bigramCost(int prevAlpha, int nextAlpha) const
{
    if (nextAlpha <= 0)
        return kNullCost;
    if (prevAlpha <= 0)
        return m_unigram[static_cast<size_t>(nextAlpha)];
    return m_bigram[static_cast<size_t>(prevAlpha) * kAlphabet + static_cast<size_t>(nextAlpha)];
}

void ViterbiBaudot::reset()
{
    m_history.clear();
    m_metric.fill(kUnreachableMetric);
    m_metric[static_cast<size_t>(stateIndex(0, 0))] = 0.0f;   // letters case, nothing seen
    m_primed = true;
}

void ViterbiBaudot::push(const SoftFrame& frame,
                         const std::function<void(const DecodedChar&)>& out)
{
    // Per-bit log-likelihoods. The demodulator hands us a value in [-1, 1];
    // llrScale turns that into the log-odds the trellis wants.
    float logOne[5];
    float logZero[5];
    for (int b = 0; b < 5; ++b) {
        const float x = frame.bitLlr[b] * m_config.llrScale;
        // log sigmoid, written to stay finite at both extremes.
        logOne[b]  = x > 0.0f ? -std::log1p(std::exp(-x)) : x - std::log1p(std::exp(x));
        logZero[b] = x > 0.0f ? -x - std::log1p(std::exp(-x)) : -std::log1p(std::exp(x));
    }

    // Cost of explaining this frame with each of the 32 codes.
    std::array<float, kCodes> codeCost{};
    for (int c = 0; c < kCodes; ++c) {
        float acc = 0.0f;
        for (int b = 0; b < 5; ++b)
            acc += (c & (1 << b)) ? logOne[b] : logZero[b];
        codeCost[static_cast<size_t>(c)] = -acc;
    }

    Step step;
    step.sampleIndex = frame.sampleIndex;
    step.back.fill(-1);
    step.emitted.fill(0);
    step.code.fill(0);
    step.quality.fill(0.0f);
    step.certainty.fill(0.0f);
    step.corrected.fill(0);

    std::array<float, kStates> next{};
    next.fill(kUnreachableMetric);

    const float q = frame.quality();
    const float c = frame.certainty();

    for (int s = 0; s < kStates; ++s) {
        const float base = m_metric[static_cast<size_t>(s)];
        if (base == kUnreachableMetric)
            continue;

        const int shift     = s / kAlphabet;
        const int prevAlpha = s % kAlphabet;

        // L'indice ha un nome proprio: chiamandolo 'c' faceva ombra alla
        // certezza del frame dichiarata sopra, e la riga che riempie
        // step.certainty finiva per scriverci dentro il numero del codice
        // Baudot invece della certezza. Sullo schermo la sfumatura
        // dipendeva da quale lettera fosse, non da quanto fosse sicura.
        for (int codeIndex = 0; codeIndex < kCodes; ++codeIndex) {
            const auto code = static_cast<uint8_t>(codeIndex);

            int   newShift = shift;
            char  emitted  = 0;
            float prior    = 0.0f;

            if (code == kBaudotLtrs) {
                newShift = 0;
                prior    = kShiftCost;
            } else if (code == kBaudotFigs) {
                newShift = 1;
                prior    = kShiftCost;
            } else {
                if (code == kBaudotSpace && m_config.unshiftOnSpace)
                    newShift = 0;
                emitted = baudotToChar(code,
                                       newShift == 0 ? Shift::Letters : Shift::Figures,
                                       m_config.figuresSet);
                prior = emitted ? bigramCost(prevAlpha, alphaIndex(emitted)) : kNullCost;
            }

            const float cost = base
                             + codeCost[static_cast<size_t>(codeIndex)]
                             + m_config.priorWeight * prior;

            const int newAlpha = emitted ? alphaIndex(emitted) : prevAlpha;
            const int ns       = stateIndex(newShift, newAlpha);

            if (cost < next[static_cast<size_t>(ns)]) {
                next[static_cast<size_t>(ns)]         = cost;
                step.back[static_cast<size_t>(ns)]    = s;
                step.emitted[static_cast<size_t>(ns)] = emitted;
                step.code[static_cast<size_t>(ns)]    = code;
                step.quality[static_cast<size_t>(ns)] = q;
                step.certainty[static_cast<size_t>(ns)] = c;
                step.corrected[static_cast<size_t>(ns)] =
                    static_cast<uint8_t>(code != frame.hardCode ? 1 : 0);
            }
        }
    }

    // Keep the metrics from drifting off to infinity over a long transmission.
    float best = kUnreachableMetric;
    for (const float m : next)
        best = std::min(best, m);
    if (best != kUnreachableMetric) {
        for (float& m : next)
            if (m != kUnreachableMetric)
                m -= best;
    }

    m_metric = next;
    m_history.push_back(step);

    const size_t window = static_cast<size_t>(std::max(1, m_config.depth));
    while (m_history.size() > window)
        emitOldest(out);
}

void ViterbiBaudot::emitOldest(const std::function<void(const DecodedChar&)>& out)
{
    if (m_history.empty())
        return;

    // Trace the current best path back to the oldest step still in the window,
    // and commit whatever that step decided.
    int   bestState = -1;
    float bestCost  = kUnreachableMetric;
    for (int s = 0; s < kStates; ++s) {
        if (m_metric[static_cast<size_t>(s)] < bestCost) {
            bestCost  = m_metric[static_cast<size_t>(s)];
            bestState = s;
        }
    }
    if (bestState < 0) {
        m_history.erase(m_history.begin());
        return;
    }

    int s = bestState;
    for (size_t i = m_history.size(); i-- > 1;) {
        s = m_history[i].back[static_cast<size_t>(s)];
        if (s < 0) {
            m_history.erase(m_history.begin());
            return;
        }
    }

    const Step& oldest = m_history.front();
    const char  ch     = oldest.emitted[static_cast<size_t>(s)];
    if (ch != 0 && out) {
        DecodedChar dc;
        dc.text        = ch;
        dc.code        = oldest.code[static_cast<size_t>(s)];
        dc.quality     = oldest.quality[static_cast<size_t>(s)];
        dc.certainty   = oldest.certainty[static_cast<size_t>(s)];
        dc.corrected   = oldest.corrected[static_cast<size_t>(s)] != 0;
        dc.sampleIndex = oldest.sampleIndex;
        out(dc);
    }

    m_history.erase(m_history.begin());
}

void ViterbiBaudot::flush(const std::function<void(const DecodedChar&)>& out)
{
    while (!m_history.empty())
        emitOldest(out);
    reset();
}

} // namespace decortty::dsp
