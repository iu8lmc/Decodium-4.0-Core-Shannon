#include "PackManager.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>

namespace decodium::voicekeyer {

namespace {
const QString kPiperZip = QStringLiteral("https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip");
const QString kVoiceIt = QStringLiteral("https://huggingface.co/rhasspy/piper-voices/resolve/main/it/it_IT/paola/medium/");
const QString kVoiceEn = QStringLiteral("https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/amy/medium/");
const QString kLlamaZip = QStringLiteral("https://github.com/ggml-org/llama.cpp/releases/download/b11574/llama-b11574-bin-win-cpu-x64.zip");
const QString kQwen = QStringLiteral("https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf");
}  // namespace

const QVector<PackManager::Item>& PackManager::manifest()
{
    // Sorgenti e SHA256 fissati: un file con hash diverso viene scartato.
    static const QVector<Item> items {
        {Pack::Voice, kPiperZip,
         QStringLiteral("f3c58906402b24f3a96d92145f58acba6d86c9b5db896d207f78dc80811efcea"),
         22477236, QStringLiteral("piper_windows_amd64.zip"), QString()},
        {Pack::Voice, kVoiceIt + QStringLiteral("it_IT-paola-medium.onnx"),
         QStringLiteral("6fc918b5a0ea6137382833dddfa567bffbe6a5060c02043c87192ee59c04210c"),
         63511038, QStringLiteral("voices/it_IT-paola-medium.onnx"), QString()},
        {Pack::Voice, kVoiceIt + QStringLiteral("it_IT-paola-medium.onnx.json"),
         QStringLiteral("aea19c0a7fce29fbc359b93f10e7902854401e4c95ae2ea328ae516b15d296cf"),
         7099, QStringLiteral("voices/it_IT-paola-medium.onnx.json"), QString()},
        // Voce inglese "amy" (CC BY-SA 4.0, dataset mimic3-voices): la lessac usa dati
        // del Blizzard 2013 con licenza nominativa, non ridistribuibile.
        {Pack::Voice, kVoiceEn + QStringLiteral("en_US-amy-medium.onnx"),
         QStringLiteral("b3a6e47b57b8c7fbe6a0ce2518161a50f59a9cdd8a50835c02cb02bdd6206c18"),
         63201294, QStringLiteral("voices/en_US-amy-medium.onnx"), QString()},
        {Pack::Voice, kVoiceEn + QStringLiteral("en_US-amy-medium.onnx.json"),
         QStringLiteral("95a23eb4d42909d38df73bb9ac7f45f597dbfcde2d1bf9526fdeaf5466977d77"),
         4882, QStringLiteral("voices/en_US-amy-medium.onnx.json"), QString()},
        // Riconoscimento: il modello si puo' scaricare, il motore whisper-cli no
        // (non ha ancora un URL pubblico): per questo il pacchetto non si installa da qui.
        // Motore di riconoscimento: whisper-cli (MIT) con le sue DLL, pubblicato dal fork.
        {Pack::Speech, QStringLiteral("https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/download/packs-whisper-1/whisper-win-x64.zip"),
         QStringLiteral("ab3871c09b579024f94ed6b45bfbdceb1533f1543a78149d17b53450ed22d7af"),
         2739161, QStringLiteral("whisper-win-x64.zip"), QStringLiteral("whisper")},
        {Pack::Speech, QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"),
         QStringLiteral("1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b"),
         487601967, QStringLiteral("models/ggml-small.bin"), QString()},
        {Pack::Assistant, kLlamaZip,
         QStringLiteral("3f185fa804d8f4af359d917e21cb19916e7d579459322820bb078a4b6705736f"),
         19509340, QStringLiteral("llama-b11574-bin-win-cpu-x64.zip"), QStringLiteral("llama")},
        {Pack::Assistant, kQwen,
         QStringLiteral("6a1a2eb6d15622bf3c96857206351ba97e1af16c30d7a74ee38970e434e9407e"),
         1117320736, QStringLiteral("models/qwen2.5-1.5b-instruct-q4_k_m.gguf"), QString()},
    };
    return items;
}

PackManager::PackManager(const QString& root, QObject* parent)
    : QObject(parent)
    , m_root(root)
    , m_network(new QNetworkAccessManager(this))
{
    m_status = tr("Optional packs: install only what you want");
}

QString PackManager::runtimeDirectory() const { return QDir(m_root).absoluteFilePath(QStringLiteral("piper")); }
QString PackManager::piperExecutable() const { return QDir(runtimeDirectory()).absoluteFilePath(QStringLiteral("piper.exe")); }
QString PackManager::speechModel() const { return QDir(m_root).absoluteFilePath(QStringLiteral("models/ggml-small.bin")); }
QString PackManager::whisperExecutable() const { return QDir(m_root).absoluteFilePath(QStringLiteral("whisper/whisper-cli.exe")); }
QString PackManager::llamaDirectory() const { return QDir(m_root).absoluteFilePath(QStringLiteral("llama")); }
QString PackManager::llamaExecutable() const { return QDir(llamaDirectory()).absoluteFilePath(QStringLiteral("llama-completion.exe")); }
QString PackManager::assistantModel() const { return QDir(m_root).absoluteFilePath(QStringLiteral("models/qwen2.5-1.5b-instruct-q4_k_m.gguf")); }

QString PackManager::voiceModel(const QString& lang) const
{
    const QString name = lang == QLatin1String("en") ? QStringLiteral("en_US-amy-medium.onnx")
                                                     : QStringLiteral("it_IT-paola-medium.onnx");
    return QDir(m_root).absoluteFilePath(QStringLiteral("voices/") + name);
}

bool PackManager::active(Pack pack) const
{
    return installed(pack) && isEnabled(static_cast<int>(pack));
}

bool PackManager::isEnabled(int pack) const
{
    static const char* keys[] = {"Packs/voice", "Packs/speech", "Packs/assistant"};
    if (pack < 0 || pack > 2) return false;
    QSettings s;
    return s.value(QLatin1String(keys[pack]), true).toBool();
}

void PackManager::setEnabled(int pack, bool on)
{
    static const char* keys[] = {"Packs/voice", "Packs/speech", "Packs/assistant"};
    if (pack < 0 || pack > 2) return;
    QSettings s;
    s.setValue(QLatin1String(keys[pack]), on);
    emit changed();
}

bool PackManager::installed(Pack pack) const
{
    switch (pack) {
    case Pack::Voice:
        return QFileInfo::exists(piperExecutable())
            && QFileInfo(voiceModel(QStringLiteral("it"))).size() == 63511038
            && QFileInfo(voiceModel(QStringLiteral("en"))).size() == 63201294;
    case Pack::Speech:
        return QFileInfo::exists(whisperExecutable()) && QFileInfo(speechModel()).size() == 487601967;
    case Pack::Assistant:
        return QFileInfo::exists(llamaExecutable()) && QFileInfo(assistantModel()).size() == 1117320736;
    }
    return false;
}

void PackManager::install(int packId)
{
    if (m_busy) return;
    const Pack pack = static_cast<Pack>(packId);
    if (installed(pack)) return;
    m_queue.clear();
    m_totalBytes = 0;
    for (const Item& item : manifest()) {
        if (item.pack != pack) continue;
        m_queue.append(item);
        m_totalBytes += item.bytes;
    }
    m_current = pack;
    m_index = 0;
    m_doneBytes = 0;
    m_busy = true;
    m_progress = 0.0;
    m_status = tr("Downloading…");
    QDir().mkpath(m_root);
    emit changed();
    fetchNext();
}

void PackManager::remove(int packId)
{
    if (m_busy) return;
    const Pack pack = static_cast<Pack>(packId);
    for (const Item& item : manifest()) {
        if (item.pack != pack) continue;
        QFile::remove(QDir(m_root).absoluteFilePath(item.target));
        if (!item.extractTo.isEmpty())
            QDir(QDir(m_root).absoluteFilePath(item.extractTo)).removeRecursively();
    }
    if (pack == Pack::Voice) QDir(runtimeDirectory()).removeRecursively();
    m_status = tr("Pack removed");
    emit changed();
}

void PackManager::cancel()
{
    if (!m_busy) return;
    m_busy = false;
    m_status = tr("Download cancelled");
    emit changed();
}

void PackManager::fetchNext()
{
    if (!m_busy) return;
    if (m_index >= m_queue.size()) {
        done();
        return;
    }
    const Item item = m_queue.at(m_index);
    m_currentFile = QDir(m_root).absoluteFilePath(item.target);
    QDir().mkpath(QFileInfo(m_currentFile).absolutePath());
    const QString part = m_currentFile + QStringLiteral(".part");

    auto* file = new QFile(part, this);
    if (!file->open(QIODevice::WriteOnly)) {
        fail(tr("Cannot write %1").arg(part));
        return;
    }
    QNetworkRequest request{QUrl(item.url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium-Packs/1.0"));
    QNetworkReply* reply = m_network->get(request);

    connect(reply, &QNetworkReply::readyRead, reply, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, item](qint64 got, qint64) {
        m_progress = double(m_doneBytes + got) / double(qMax<qint64>(1, m_totalBytes));
        m_status = tr("Downloading %1…").arg(QFileInfo(item.target).fileName());
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, item, part] {
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        reply->deleteLater();
        file->close();
        file->deleteLater();
        if (!m_busy) { QFile::remove(part); return; }
        if (!ok) {
            QFile::remove(part);
            fail(tr("Download failed: %1").arg(error));
            return;
        }
        QFile check(part);
        if (!check.open(QIODevice::ReadOnly)) { fail(tr("Cannot read %1").arg(part)); return; }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&check);
        check.close();
        if (hash.result().toHex() != item.sha256.toLatin1()) {
            QFile::remove(part);
            fail(tr("Checksum mismatch for %1: the file was discarded").arg(QFileInfo(item.target).fileName()));
            return;
        }
        QFile::remove(m_currentFile);
        QFile::rename(part, m_currentFile);
        m_doneBytes += item.bytes;
        if (!item.extractTo.isEmpty() || item.target.endsWith(QLatin1String(".zip"))) {
            m_status = tr("Unpacking…");
            emit changed();
            extractArchive(m_currentFile, item.extractTo.isEmpty() ? m_root : QDir(m_root).absoluteFilePath(item.extractTo));
            if (!m_busy) return;
        }
        ++m_index;
        fetchNext();
    });
}

void PackManager::extractArchive(const QString& file, const QString& dir)
{
    QDir().mkpath(dir);
    // Windows 10 e 11 hanno tar (bsdtar) che legge gli zip.
    QProcess tar;
    tar.setWorkingDirectory(dir);
    tar.start(QStringLiteral("tar"), {QStringLiteral("-xf"), file});
    if (!tar.waitForFinished(120000) || tar.exitCode() != 0) {
        fail(tr("Cannot unpack %1").arg(QFileInfo(file).fileName()));
        return;
    }
    QFile::remove(file);
}

void PackManager::done()
{
    m_busy = false;
    m_progress = 1.0;
    m_status = installed(m_current) ? tr("Pack installed") : tr("Pack incomplete: try installing again");
    emit changed();
}

void PackManager::fail(const QString& text)
{
    m_busy = false;
    m_status = text;
    emit changed();
}

}  // namespace decodium::voicekeyer
