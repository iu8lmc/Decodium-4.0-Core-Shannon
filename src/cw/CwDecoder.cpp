#include "CwDecoder.h"

#include "ggmorse/ggmorse.h"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace decodium::cw {

namespace {

const QHash<QString, QString>& table()
{
    static const QHash<QString, QString> map{
        {".-", "A"},    {"-...", "B"},  {"-.-.", "C"},  {"-..", "D"},   {".", "E"},
        {"..-.", "F"},  {"--.", "G"},   {"....", "H"},  {"..", "I"},    {".---", "J"},
        {"-.-", "K"},   {".-..", "L"},  {"--", "M"},    {"-.", "N"},    {"---", "O"},
        {".--.", "P"},  {"--.-", "Q"},  {".-.", "R"},   {"...", "S"},   {"-", "T"},
        {"..-", "U"},   {"...-", "V"},  {".--", "W"},   {"-..-", "X"},  {"-.--", "Y"},
        {"--..", "Z"},
        {"-----", "0"}, {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"},
        {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"}, {"----.", "9"},
        {"-..-.", "/"}, {"-...-", "="}, {".-.-.", "+"}, {"-....-", "-"}, {"..--..", "?"},
        {".-.-.-", "."}, {"--..--", ","}, {"---...", ":"}, {".----.", "'"}, {".--.-.", "@"},
        {"-.--.", "("}, {"-.--.-", ")"}, {"...-.-", "<SK>"}, {"-.-.-", "<KA>"},
        {"...-.", "<SN>"}, {".-...", "<AS>"},
    };
    return map;
}

// Quanti campioni di silenzio servono in coda perche' ggmorse si convinca che
// la parola e' finita: la sua finestra di analisi e' lunga tre secondi.
constexpr double kFlushSeconds = 3.2;

// Sopra questo costo quello che esce non e' Morse: e' il rumore che a tratti
// gli somiglia. Misurato: un segnale, anche sepolto nel rumore, sta sotto 0.01;
// il solo rumore di banda sta fra 0.68 e 1.3.
constexpr float kMaxCost = 0.2f;

} // namespace

QString morseToChar(const QString& symbols)
{
    return table().value(symbols);
}

QString charToMorse(QChar c)
{
    const QString upper = QString(c).toUpper();
    for (auto it = table().cbegin(); it != table().cend(); ++it) {
        if (it.value() == upper)
            return it.key();
    }
    return {};
}

CwDecoder::CwDecoder(int sampleRate)
{
    setSampleRate(sampleRate);
}

CwDecoder::~CwDecoder() = default;

void CwDecoder::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate > 0 ? sampleRate : 8000;
    rebuild();
}

void CwDecoder::setTone(int hz)
{
    m_tone = hz > 0 ? hz : 0;
    applyDecodeParameters();
}

void CwDecoder::setSpeed(int wpm)
{
    // ggmorse cerca velocita' fino a 59 WPM; 0 mantiene il suo rilevamento
    // automatico. Non mandargli valori fuori dal suo intervallo di ricerca.
    m_speed = wpm > 0 ? std::clamp(wpm, 5, 59) : 0;
    applyDecodeParameters();
}

void CwDecoder::applyDecodeParameters()
{
    if (!m_morse)
        return;
    GGMorse::ParametersDecode decode = GGMorse::getDefaultParametersDecode();
    // Con -1 se lo cerca da solo; con un numero ascolta solo li'.
    decode.frequency_hz = m_tone > 0 ? static_cast<float>(m_tone) : -1.0f;
    // Con -1 cerca anche la velocita'; un valore positivo la blocca.
    decode.speed_wpm = m_speed > 0 ? static_cast<float>(m_speed) : -1.0f;
    m_morse->setParametersDecode(decode);
}

void CwDecoder::rebuild()
{
    GGMorse::Parameters parameters{
        static_cast<float>(m_sampleRate),
        static_cast<float>(m_sampleRate),
        GGMorse::kDefaultSamplesPerFrame,
        GGMORSE_SAMPLE_FORMAT_I16,
        GGMORSE_SAMPLE_FORMAT_I16,
    };
    m_morse = std::make_unique<GGMorse>(parameters);
    m_pending.clear();
    m_taken = 0;
    m_toneHz = 0;
    m_wpm = 0;
    m_scope = Scope();
    m_last = QChar();
    setTone(m_tone);
}

void CwDecoder::reset()
{
    rebuild();
}

QString CwDecoder::drain()
{
    QString out;
    // Un fotogramma per volta: cosi' il costo che si legge dopo e' quello di
    // quel pezzo di audio, e si sa se le lettere appena uscite valgono.
    bool served = false;
    GGMorse::CBWaveformInp feeder = [this, &served](void* data, uint32_t maxBytes) -> uint32_t {
        if (served)
            return 0;
        const qsizetype have = m_pending.size() - m_taken;
        if (have < static_cast<qsizetype>(maxBytes))
            return 0;
        std::memcpy(data, m_pending.constData() + m_taken, maxBytes);
        m_taken += maxBytes;
        served = true;
        return maxBytes;
    };

    // Il segnale lo rifa' a ogni fotogramma sull'intera finestra: basta
    // l'ultimo. La soglia invece la accoda, e se non la si prende cresce.
    GGMorse::SignalF signal;
    GGMorse::ThresholdF thresholds;
    bool fresh = false;

    for (;;) {
        served = false;
        m_morse->decode(feeder);
        if (!served)
            break;   // l'audio in mano e' finito: il resto arriva dopo
        if (m_morse->takeSignalF(signal) > 0)
            fresh = true;
        m_morse->takeThresholdF(thresholds);

        // Il costo dice quanto i tempi misurati somigliano a del Morse vero.
        // Su un segnale, anche brutto, sta sotto il centesimo; sul solo rumore
        // di banda sta intorno a uno. In mezzo c'e' tutto lo spazio per dire di
        // no: senza questo controllo il riquadro si riempie di lettere finte
        // appena la radio e' accesa e nessuno sta trasmettendo.
        const GGMorse::Statistics& stats = m_morse->getStatistics();
        const bool reading = stats.costFunction < kMaxCost && stats.estimatedPitch_Hz > 0;
        if (reading) {
            m_toneHz = stats.estimatedPitch_Hz;
            m_wpm = static_cast<int>(std::lround(stats.estimatedSpeed_wpm));
        }

        // Si svuota comunque: quello che non vale si butta, non si accumula.
        GGMorse::TxRx decoded;
        if (m_morse->takeRxData(decoded) <= 0 || !reading)
            continue;
        for (const std::uint8_t c : decoded) {
            // Quando il tono cambia di colpo ggmorse va a capo: nel pannello una
            // riga nuova ci sta, ma non due di fila, e non come prima cosa.
            const QChar ch = QLatin1Char(static_cast<char>(c));
            if (ch == QLatin1Char('\n') && (m_last.isNull() || m_last == QLatin1Char('\n')))
                continue;
            out += ch;
            m_last = ch;
        }
    }

    if (fresh)
        updateScope(signal);

    // Quello che e' stato consumato non serve piu'; il resto aspetta il pezzo
    // successivo di audio.
    if (m_taken > 0) {
        m_pending.remove(0, m_taken);
        m_taken = 0;
    }
    return out;
}

void CwDecoder::updateScope(const std::vector<float>& signal)
{
    const GGMorse::Statistics& stats = m_morse->getStatistics();
    // ggmorse decide acceso/spento con la media del segnale per la soglia: la
    // stessa riga, riportata sulla scala del disegno.
    double mean = 0;
    float peak = 0;
    for (const float v : signal) {
        mean += v;
        peak = std::max(peak, v);
    }
    mean = signal.empty() ? 0 : mean / static_cast<double>(signal.size());

    m_scope.signal.resize(static_cast<qsizetype>(signal.size()));
    for (std::size_t i = 0; i < signal.size(); ++i)
        m_scope.signal[static_cast<qsizetype>(i)] = peak > 0 ? signal[i] / peak : 0.0f;
    m_scope.level = peak > 0 ? std::min(1.0f, static_cast<float>(mean * stats.signalThreshold) / peak) : 0.0f;
    m_scope.pitch = stats.estimatedPitch_Hz;
    m_scope.speed = stats.estimatedSpeed_wpm;
    m_scope.cost = stats.costFunction;
    m_scope.reading = stats.costFunction < kMaxCost && stats.estimatedPitch_Hz > 0;
}

QString CwDecoder::feed(const qint16* samples, int count)
{
    if (!m_morse || !samples || count <= 0)
        return {};
    m_pending.append(reinterpret_cast<const char*>(samples),
                     static_cast<qsizetype>(count) * static_cast<qsizetype>(sizeof(qint16)));
    return drain();
}

QString CwDecoder::flush()
{
    if (!m_morse)
        return {};
    // Il silenzio che in aria ci sarebbe comunque: senza, l'ultima lettera
    // resterebbe in bocca al decodificatore.
    const int silence = static_cast<int>(kFlushSeconds * m_sampleRate);
    m_pending.append(static_cast<qsizetype>(silence) * static_cast<qsizetype>(sizeof(qint16)), '\0');
    QString out = drain();
    while (out.endsWith(QLatin1Char('\n')) || out.endsWith(QLatin1Char(' ')))
        out.chop(1);
    return out;
}

} // namespace decodium::cw
