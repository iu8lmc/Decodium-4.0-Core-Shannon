#include "VoiceKeyer.h"
#include "PiperTts.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace decodium::voicekeyer {

namespace {

constexpr int kLeadDefaultMs = 150;
constexpr int kTailTicks = 8;   // 320 ms di silenzio dopo l'ultimo suono

quint32 le32(const uchar* p) { return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24); }
quint16 le16(const uchar* p) { return quint16(p[0] | (p[1] << 8)); }

void put32(QByteArray& b, quint32 v) { for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xff)); }
void put16(QByteArray& b, quint16 v) { b.append(char(v & 0xff)); b.append(char((v >> 8) & 0xff)); }

}  // namespace

std::optional<QVector<short>> readWav(const QString& path, QString* error)
{
    auto fail = [&](const QString& why) -> std::optional<QVector<short>> {
        if (error) *error = why;
        return std::nullopt;
    };
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return fail(QStringLiteral("file not readable"));
    const QByteArray d = f.readAll();
    if (d.size() < 44 || d.mid(0, 4) != "RIFF" || d.mid(8, 4) != "WAVE") return fail(QStringLiteral("not a WAV file"));

    const uchar* p = reinterpret_cast<const uchar*>(d.constData());
    qsizetype pos = 12;
    int channels = 0, rate = 0, bits = 0, format = 0;
    qsizetype dataAt = -1, dataLen = 0;
    while (pos + 8 <= d.size()) {
        const QByteArray id = d.mid(pos, 4);
        const qsizetype len = qsizetype(le32(p + pos + 4));
        const qsizetype body = pos + 8;
        if (id == "fmt " && body + 16 <= d.size()) {
            format = le16(p + body);
            channels = le16(p + body + 2);
            rate = int(le32(p + body + 4));
            bits = le16(p + body + 14);
        } else if (id == "data") {
            dataAt = body;
            dataLen = std::min<qsizetype>(len, d.size() - body);
            break;
        }
        pos = body + len + (len & 1);
    }
    if (dataAt < 0 || channels < 1 || rate <= 0) return fail(QStringLiteral("missing fmt or data chunk"));
    if (format != 1 && format != 0xFFFE) return fail(QStringLiteral("not PCM"));
    if (bits != 16) return fail(QStringLiteral("only 16-bit PCM is supported"));

    const qsizetype frames = dataLen / (2 * channels);
    QVector<short> mono(static_cast<int>(frames));
    for (qsizetype i = 0; i < frames; ++i)
        mono[int(i)] = short(le16(p + dataAt + i * 2 * channels));
    return resampleLinear(mono, rate, kRadioRate);
}

bool writeWav(const QString& path, const QVector<short>& samples, int rate, QString* error)
{
    QByteArray out;
    const quint32 dataBytes = quint32(samples.size()) * 2u;
    out.append("RIFF", 4);
    put32(out, 36 + dataBytes);
    out.append("WAVE", 4);
    out.append("fmt ", 4);
    put32(out, 16);
    put16(out, 1);                       // PCM
    put16(out, 1);                       // mono
    put32(out, quint32(rate));
    put32(out, quint32(rate) * 2u);      // byte rate
    put16(out, 2);                       // block align
    put16(out, 16);                      // bits
    out.append("data", 4);
    put32(out, dataBytes);
    for (short s : samples) put16(out, quint16(s));

    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString tmp = path + QStringLiteral(".part");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("cannot write %1").arg(tmp);
        return false;
    }
    f.write(out);
    f.close();
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        if (error) *error = QStringLiteral("cannot replace %1").arg(path);
        return false;
    }
    return true;
}

QVector<short> resampleLinear(const QVector<short>& in, int fromRate, int toRate)
{
    if (in.isEmpty() || fromRate <= 0 || toRate <= 0) return {};
    if (fromRate == toRate) return in;
    const qsizetype outLen = qsizetype(std::llround(double(in.size()) * toRate / fromRate));
    QVector<short> out(static_cast<int>(outLen));
    const double step = double(fromRate) / toRate;
    for (qsizetype i = 0; i < outLen; ++i) {
        const double src = i * step;
        const qsizetype a = qsizetype(src);
        const qsizetype b = std::min<qsizetype>(a + 1, in.size() - 1);
        const double t = src - double(a);
        out[int(i)] = short(std::lround(in[int(a)] * (1.0 - t) + in[int(b)] * t));
    }
    return out;
}

VoiceKeyer::VoiceKeyer(const QString& directory, PackManager* packs, QObject* parent)
    : QObject(parent)
    , m_directory(directory)
    , m_pack(packs)
    , m_slots(kSlots)
{
    m_speed = std::clamp(QSettings().value(QStringLiteral("VoiceKeyer/speed"), 1.0).toDouble(), 0.6, 1.6);
    m_lead.setSingleShot(true);
    connect(&m_lead, &QTimer::timeout, this, [this] { m_pace.start(); });
    m_pace.setInterval(40);
    connect(&m_pace, &QTimer::timeout, this, &VoiceKeyer::tick);
    load();
    m_status = tr("Ready");
}

VoiceKeyer::~VoiceKeyer()
{
    stop();
    if (m_recording) stopRecording();
}

QString VoiceKeyer::wavPath(int index) const
{
    return QDir(m_directory).absoluteFilePath(QStringLiteral("slot%1.wav").arg(index + 1));
}

void VoiceKeyer::load()
{
    QFile f(QDir(m_directory).absoluteFilePath(QStringLiteral("keyer.json")));
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("slots")).toArray();
    for (int i = 0; i < std::min<int>(arr.size(), kSlots); ++i) {
        const QJsonObject o = arr.at(i).toObject();
        m_slots[i].label = o.value(QStringLiteral("label")).toString();
        m_slots[i].text = o.value(QStringLiteral("text")).toString();
        const QString lang = o.value(QStringLiteral("lang")).toString();
        if (lang == QLatin1String("en") || lang == QLatin1String("it")) m_slots[i].lang = lang;
    }
}

void VoiceKeyer::save()
{
    QJsonArray arr;
    for (const Slot& s : m_slots)
        arr.append(QJsonObject{{QStringLiteral("label"), s.label}, {QStringLiteral("text"), s.text},
                               {QStringLiteral("lang"), s.lang}});
    QDir().mkpath(m_directory);
    QFile f(QDir(m_directory).absoluteFilePath(QStringLiteral("keyer.json")));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("slots"), arr}}).toJson());
}

QVariantList VoiceKeyer::messages() const
{
    QVariantList out;
    for (int i = 0; i < kSlots; ++i) {
        const bool hasAudio = QFileInfo::exists(wavPath(i));
        out.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("label"), m_slots[i].label.isEmpty() ? QStringLiteral("F%1").arg(i + 1) : m_slots[i].label},
            {QStringLiteral("text"), m_slots[i].text},
            {QStringLiteral("hasAudio"), hasAudio},
            {QStringLiteral("lang"), m_slots[i].lang},
        });
    }
    return out;
}

void VoiceKeyer::setStatus(const QString& text)
{
    m_status = text;
    emit changed();
}

void VoiceKeyer::setSlotText(int index, const QString& label, const QString& text)
{
    if (index < 0 || index >= kSlots) return;
    m_slots[index].label = label.trimmed().left(12);
    m_slots[index].text = text.left(200);
    save();
    emit messagesChanged();
}

void VoiceKeyer::clearSlot(int index)
{
    if (index < 0 || index >= kSlots || m_playing || m_recording) return;
    QFile::remove(wavPath(index));
    emit messagesChanged();
}

void VoiceKeyer::setSlotLanguage(int index, const QString& lang)
{
    if (index < 0 || index >= kSlots || (lang != QLatin1String("it") && lang != QLatin1String("en"))) return;
    m_slots[index].lang = lang;
    save();
    emit messagesChanged();
}

void VoiceKeyer::sayText(const QString& text, const QString& lang)
{
    if (m_playing || m_recording) return;
    if (text.trimmed().isEmpty()) return;
    if (!m_pack->active(Pack::Voice)) {
        setStatus(tr("To read text aloud install and enable the Voice pack first"));
        return;
    }
    synthesize(text, lang, tr("reply"), true);
}

void VoiceKeyer::preview(const QString& text, const QString& lang)
{
    if (m_playing || m_recording || m_tts || text.trimmed().isEmpty()) return;
    if (!m_pack->active(Pack::Voice)) {
        setStatus(tr("To read text aloud install and enable the Voice pack first"));
        return;
    }
    synthesize(text, lang, tr("preview"), false);
}

void VoiceKeyer::setSpeed(double s)
{
    m_speed = std::clamp(s, 0.6, 1.6);
    QSettings().setValue(QStringLiteral("VoiceKeyer/speed"), m_speed);
    emit changed();
}

void VoiceKeyer::synthesize(const QString& text, const QString& lang, const QString& name, bool forAir)
{
    if (m_tts) return;
    QString error;
    const QString model = m_pack->voiceModel(lang == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("it"));
    m_tts = PiperTts::start(m_pack->piperExecutable(), m_pack->runtimeDirectory(), model, text, m_speed, &error);
    if (!m_tts) {
        setStatus(tr("Voice synthesis failed: %1").arg(error));
        return;
    }
    m_ttsName = name;
    m_ttsForAir = forAir;
    m_ttsRate = PiperTts::modelSampleRate(model);
    connect(m_tts, &QProcess::finished, this, &VoiceKeyer::onSynthesized);
    setStatus(tr("Synthesising %1…").arg(name));
}

void VoiceKeyer::onSynthesized(int code, QProcess::ExitStatus status)
{
    QProcess* p = m_tts;
    if (!p) return;
    const QByteArray raw = p->readAllStandardOutput();
    m_tts = nullptr;
    p->deleteLater();
    if (status != QProcess::NormalExit || code != 0) {
        setStatus(tr("Voice synthesis failed: the engine stopped"));
        return;
    }
    const auto pcm = PiperTts::decode(raw, m_ttsRate);
    if (!pcm) {
        setStatus(tr("Voice synthesis failed: no audio produced"));
        return;
    }
    if (m_ttsForAir) startPlayback(*pcm, m_ttsName);
    else playLocally(*pcm);
}

void VoiceKeyer::playLocally(const QVector<short>& pcm)
{
    if (m_preview) {
        m_preview->stop();
        m_preview->deleteLater();
        m_preview = nullptr;
    }
    QAudioFormat fmt;
    fmt.setSampleRate(kRadioRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    const QAudioDevice out = QMediaDevices::defaultAudioOutput();
    if (out.isNull() || !out.isFormatSupported(fmt)) {
        setStatus(tr("No speaker found for the preview"));
        return;
    }
    auto* buf = new QBuffer();
    buf->setData(reinterpret_cast<const char*>(pcm.constData()), int(pcm.size() * 2));
    buf->open(QIODevice::ReadOnly);
    m_preview = new QAudioSink(out, fmt, this);
    connect(m_preview, &QAudioSink::stateChanged, this, [this, buf](QAudio::State st) {
        if (st == QAudio::IdleState || st == QAudio::StoppedState) {
            if (m_preview) { m_preview->deleteLater(); m_preview = nullptr; }
            buf->deleteLater();
            setStatus(tr("Preview finished"));
        }
    });
    m_preview->start(buf);
    setStatus(tr("Preview on this computer (not transmitted)"));
}

void VoiceKeyer::play(int index)
{
    if (m_playing || m_recording || m_tts) return;
    if (index < 0 || index >= kSlots) return;
    const QString name = m_slots[index].label.isEmpty() ? QStringLiteral("F%1").arg(index + 1) : m_slots[index].label;

    // Prima la registrazione dell'operatore; se non c'e', il testo letto dalla voce.
    QString error;
    if (QFileInfo::exists(wavPath(index))) {
        const auto pcm = readWav(wavPath(index), &error);
        if (!pcm || pcm->isEmpty()) {
            setStatus(tr("Cannot read the recording of %1: %2").arg(name, error));
            return;
        }
        startPlayback(*pcm, name);
        return;
    }
    if (m_slots[index].text.trimmed().isEmpty()) {
        setStatus(tr("F%1 is empty: record it or write a text first").arg(index + 1));
        return;
    }
    if (!m_pack->active(Pack::Voice)) {
        setStatus(tr("To read text aloud install the voice pack first"));
        return;
    }
    const QString lang = m_slots[index].lang;
    const QString model = m_pack->voiceModel(lang);
    synthesize(m_slots[index].text, lang, name, true);
}

void VoiceKeyer::startPlayback(const QVector<short>& pcm, const QString& name)
{
    if (m_hooks.begin && !m_hooks.begin()) {
        setStatus(tr("PTT refused: check CAT, SSB mode and the TX audio output"));
        return;
    }
    m_pcm = pcm;
    m_pos = 0;
    m_tail = 0;
    m_playing = true;
    setStatus(tr("Sending %1").arg(name));
    const int lead = m_hooks.leadMs ? std::clamp(m_hooks.leadMs(), 0, 1500) : kLeadDefaultMs;
    m_lead.start(lead);
}

void VoiceKeyer::tick()
{
    if (!m_playing) return;
    if (m_hooks.valid && !m_hooks.valid()) {
        finish(tr("Radio unavailable; PTT released"));
        return;
    }
    if (m_pos < m_pcm.size()) {
        QVector<short> packet(kPacket, 0);
        const int n = std::min<int>(kPacket, m_pcm.size() - m_pos);
        std::copy(m_pcm.constBegin() + m_pos, m_pcm.constBegin() + m_pos + n, packet.begin());
        m_pos += n;
        if (m_hooks.send) m_hooks.send(packet);
        return;
    }
    if (++m_tail >= kTailTicks) finish(tr("Message sent"));
    else if (m_hooks.send) m_hooks.send(QVector<short>(kPacket, 0));
}

void VoiceKeyer::finish(const QString& message)
{
    m_lead.stop();
    m_pace.stop();
    m_playing = false;
    m_pcm.clear();
    if (m_hooks.end) m_hooks.end();
    setStatus(message);
}

void VoiceKeyer::stop()
{
    if (m_tts) {
        m_tts->kill();
        m_tts->deleteLater();
        m_tts = nullptr;
        setStatus(tr("Stopped"));
    }
    if (m_preview) {
        m_preview->stop();
        m_preview->deleteLater();
        m_preview = nullptr;
    }
    if (m_playing) finish(tr("Stopped"));
}

bool VoiceKeyer::startRecording(int index)
{
    if (m_playing || m_recording || index < 0 || index >= kSlots) return false;
    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        setStatus(tr("No microphone found"));
        return false;
    }
    QAudioFormat fmt;
    fmt.setSampleRate(kCaptureRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(fmt)) {
        setStatus(tr("Microphone must support 48 kHz 16-bit PCM"));
        return false;
    }
    m_captured.clear();
    m_source = new QAudioSource(device, fmt, this);
    m_source->setBufferSize(fmt.bytesForDuration(400000));
    m_input = m_source->start();
    if (!m_input) {
        delete m_source;
        m_source = nullptr;
        setStatus(tr("Cannot open the microphone"));
        return false;
    }
    connect(m_input, &QIODevice::readyRead, this, &VoiceKeyer::captureReady);
    m_recordIndex = index;
    m_recording = true;
    setStatus(tr("Recording F%1 (release to save)").arg(index + 1));
    return true;
}

void VoiceKeyer::captureReady()
{
    if (!m_input || !m_recording) return;
    m_captured += m_input->readAll();
    if (m_captured.size() > kCaptureRate * 2 * kMaxSeconds) stopRecording();
}

void VoiceKeyer::stopRecording()
{
    if (!m_recording) return;
    captureReady();
    m_recording = false;
    if (m_source) {
        m_source->stop();
        delete m_source;
        m_source = nullptr;
    }
    m_input = nullptr;
    finishRecording();
}

void VoiceKeyer::finishRecording()
{
    QVector<short> pcm(m_captured.size() / 2);
    std::memcpy(pcm.data(), m_captured.constData(), std::size_t(pcm.size()) * 2);
    m_captured.clear();
    const QVector<short> radio = resampleLinear(pcm, kCaptureRate, kRadioRate);
    if (radio.size() < kRadioRate / 4) {
        setStatus(tr("Recording too short: keep the key pressed while speaking"));
        emit changed();
        return;
    }
    QString error;
    if (!writeWav(wavPath(m_recordIndex), radio, kRadioRate, &error)) {
        setStatus(tr("Cannot save the recording: %1").arg(error));
        return;
    }
    setStatus(tr("F%1 recorded (%2 s)").arg(m_recordIndex + 1).arg(radio.size() / kRadioRate));
    emit messagesChanged();
}

}  // namespace decodium::voicekeyer
