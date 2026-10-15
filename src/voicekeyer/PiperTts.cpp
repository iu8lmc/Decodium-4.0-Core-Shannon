#include "PiperTts.h"

#include "VoiceKeyer.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace decodium::voicekeyer {

namespace {
constexpr int kMaxChars = 300;
constexpr int kTimeoutMs = 30000;
constexpr double kSentenceSilenceSec = 0.25;   // pausa tra le frasi: piu' naturale al microfono
constexpr int kLeadSilenceMs = 300;            // silenzio prima della voce: la radio non taglia la prima sillaba
constexpr int kFadeMs = 15;
constexpr double kPeak = 0.8 * 32767.0;
}  // namespace

QString PiperTts::cleanText(const QString& text)
{
    QString clean = text;
    clean.replace(QLatin1Char('\n'), QLatin1Char(' '));
    clean.replace(QLatin1Char('\r'), QLatin1Char(' '));
    clean.replace(QLatin1Char('\t'), QLatin1Char(' '));
    clean = clean.simplified();
    return clean.left(kMaxChars);
}

QStringList PiperTts::arguments(const QString& modelPath, double speed)
{
    // length_scale e' la durata relativa: 1 = normale, <1 piu' veloce.
    const double s = std::clamp(speed, 0.6, 1.6);
    return {QStringLiteral("--model"), modelPath,
            QStringLiteral("--output_raw"),
            QStringLiteral("--length_scale"), QString::number(1.0 / s, 'f', 2),
            QStringLiteral("--sentence_silence"), QString::number(kSentenceSilenceSec, 'f', 2)};
}

int PiperTts::modelSampleRate(const QString& modelPath)
{
    QFile f(modelPath + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly)) return 22050;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const int rate = root.value(QStringLiteral("audio")).toObject().value(QStringLiteral("sample_rate")).toInt();
    return rate > 0 ? rate : 22050;
}

QVector<short> PiperTts::prepareForAir(const QVector<short>& pcm, int rate)
{
    // Riporta a 12 kHz (il trasporto della radio) e poi sistema il livello.
    QVector<short> out = resampleLinear(pcm, rate, kRadioRate);
    if (out.isEmpty()) return out;

    // Volume: picco a 0,8 di fondo scala, senza superarlo.
    int peak = 1;
    for (short s : out) peak = std::max(peak, std::abs(int(s)));
    const double gain = kPeak / peak;
    for (short& s : out) s = short(std::clamp(std::lround(s * gain), -32767L, 32767L));

    // Dissolvenza di apertura e chiusura: niente click all'inizio e alla fine.
    const int fade = kRadioRate * kFadeMs / 1000;
    for (int i = 0; i < fade && i < out.size(); ++i) {
        const double k = double(i) / fade;
        out[i] = short(out[i] * k);
        out[out.size() - 1 - i] = short(out[out.size() - 1 - i] * k);
    }

    // Silenzio iniziale.
    QVector<short> lead(kRadioRate * kLeadSilenceMs / 1000, 0);
    lead.append(out);
    return lead;
}

QProcess* PiperTts::start(const QString& piperExe, const QString& runtimeDir, const QString& modelPath,
                          const QString& text, double speed, QString* error)
{
    auto fail = [&](const QString& why) -> QProcess* {
        if (error) *error = why;
        return nullptr;
    };
    const QString clean = cleanText(text);
    if (clean.isEmpty()) return fail(QStringLiteral("empty text"));
    if (!QFileInfo::exists(piperExe)) return fail(QStringLiteral("voice engine not installed"));
    if (!QFileInfo::exists(modelPath)) return fail(QStringLiteral("voice model not installed"));

    auto* p = new QProcess();
    p->setWorkingDirectory(runtimeDir);
    p->start(piperExe, arguments(modelPath, speed));
    if (!p->waitForStarted(5000)) {
        if (error) *error = QStringLiteral("cannot start the voice engine");
        delete p;
        return nullptr;
    }
    p->write(clean.toUtf8() + "\n");
    p->closeWriteChannel();
    return p;
}

std::optional<QVector<short>> PiperTts::decode(const QByteArray& raw, int rate)
{
    if (raw.size() < 2) return std::nullopt;
    QVector<short> pcm(raw.size() / 2);
    std::memcpy(pcm.data(), raw.constData(), std::size_t(pcm.size()) * 2);
    return prepareForAir(pcm, rate);
}

std::optional<QVector<short>> PiperTts::synthesize(const QString& piperExe, const QString& runtimeDir,
                                                   const QString& modelPath, const QString& text,
                                                   QString* error)
{
    QString err;
    QProcess* p = start(piperExe, runtimeDir, modelPath, text, 1.0, &err);
    if (!p) {
        if (error) *error = err;
        return std::nullopt;
    }
    if (!p->waitForFinished(kTimeoutMs)) {
        p->kill();
        p->waitForFinished(2000);
        delete p;
        if (error) *error = QStringLiteral("voice engine timed out");
        return std::nullopt;
    }
    if (p->exitCode() != 0) {
        const int code = p->exitCode();
        delete p;
        if (error) *error = QStringLiteral("voice engine exited with code %1").arg(code);
        return std::nullopt;
    }
    const QByteArray raw = p->readAllStandardOutput();
    delete p;
    auto pcm = decode(raw, modelSampleRate(modelPath));
    if (!pcm) {
        if (error) *error = QStringLiteral("no audio produced");
    }
    return pcm;
}

}  // namespace decodium::voicekeyer
