// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/integration/SstvWavPcmReader.h"
#include "src/sstv/integration/SstvQsoLog.h"
#include "src/sstv/sharing/SstvShareManifest.h"
#include "src/sstv/sharing/SstvShareSecurity.h"
#include "src/sstv/sharing/SstvShareTransfer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

using namespace decodium::sstv;
using namespace decodium::sstv::sharing;

namespace {

bool check(bool condition, const char* detail)
{
    if (!condition) {
        std::cerr << "SSTV fuzz smoke failure: " << detail << '\n';
    }
    return condition;
}

void appendLe16(QByteArray& output, quint16 value)
{
    output.append(static_cast<char>(value & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
}

void appendLe32(QByteArray& output, quint32 value)
{
    output.append(static_cast<char>(value & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
    output.append(static_cast<char>((value >> 16U) & 0xffU));
    output.append(static_cast<char>((value >> 24U) & 0xffU));
}

QByteArray validPcm16Wave()
{
    QByteArray body("WAVE", 4);
    body.append("fmt ", 4);
    appendLe32(body, 16U);
    appendLe16(body, 1U);
    appendLe16(body, 1U);
    appendLe32(body, 12'000U);
    appendLe32(body, 24'000U);
    appendLe16(body, 2U);
    appendLe16(body, 16U);
    body.append("data", 4);
    appendLe32(body, 4U);
    appendLe16(body, 0x8000U);
    appendLe16(body, 0x7fffU);

    QByteArray wave("RIFF", 4);
    appendLe32(wave, static_cast<quint32>(body.size()));
    wave.append(body);
    return wave;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size() && file.flush();
}

bool readerAcceptsSeed(QTemporaryDir& directory,
                       const QByteArray& bytes,
                       const QString& name)
{
    const QString path = directory.filePath(name);
    if (!check(writeBytes(path, bytes), "cannot write WAV seed")) {
        return false;
    }
    SstvWavPcmReader reader;
    QString error;
    if (!check(reader.open(path, &error), "valid WAV seed was rejected")) {
        return false;
    }
    QVector<short> samples;
    if (!check(reader.readNext(&samples, &error) == SstvWavReadStatus::Chunk,
               "valid WAV seed did not yield PCM")) {
        return false;
    }
    if (!check(samples == QVector<short>({-32'768, 32'767}),
               "valid WAV seed decoded incorrectly")) {
        return false;
    }
    return check(reader.readNext(&samples, &error) == SstvWavReadStatus::End,
                 "valid WAV seed did not terminate");
}

bool readerRejectsSeed(QTemporaryDir& directory,
                       const QByteArray& bytes,
                       int index)
{
    const QString path = directory.filePath(
        QStringLiteral("malformed-%1.wav").arg(index));
    if (!check(writeBytes(path, bytes), "cannot write malformed WAV seed")) {
        return false;
    }
    SstvWavPcmReader reader;
    QString error;
    return check(!reader.open(path, &error) && !error.isEmpty(),
                 "malformed WAV seed was accepted");
}

QDateTime fixedUtc(const QString& text)
{
    return QDateTime::fromString(text, Qt::ISODateWithMs).toUTC();
}

SstvShareManifestV1 validManifest()
{
    const QByteArray payload("\0\x80\xff\x7f", 4);
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid::fromString(
        QStringLiteral("2e6492b8-f383-4aa4-b953-173ba74dfdea"));
    manifest.providerId = QStringLiteral("fuzz");
    manifest.senderId = QStringLiteral("station:test");
    manifest.recipientId = QStringLiteral("recipient:test");
    manifest.createdUtc = fixedUtc(QStringLiteral("2026-08-24T10:00:00.000Z"));
    manifest.expiresUtc = fixedUtc(QStringLiteral("2026-08-25T10:00:00.000Z"));
    manifest.mediaUtc = fixedUtc(QStringLiteral("2026-08-24T09:59:00.000Z"));
    manifest.originalFilename = QStringLiteral("seed.png");
    manifest.safeDisplayFilename = QStringLiteral("seed.png");
    manifest.mimeType = QStringLiteral("image/png");
    manifest.byteSize = static_cast<quint64>(payload.size());
    manifest.sha256 = QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    manifest.width = 1U;
    manifest.height = 1U;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

QByteArray readCorpus(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SSTV_FUZZ_CORPUS_DIR)
               + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool smokeWavParser()
{
    QTemporaryDir directory;
    if (!check(directory.isValid(), "cannot create WAV scratch directory")) {
        return false;
    }
    const QByteArray wave = validPcm16Wave();
    if (!readerAcceptsSeed(directory, wave, QStringLiteral("generated.wav"))) {
        return false;
    }

    QByteArray hexSeed = readCorpus(QStringLiteral("wav/valid_pcm16.hex"));
    if (!check(hexSeed.startsWith("hex:"), "WAV corpus seed has no hex marker")) {
        return false;
    }
    hexSeed = QByteArray::fromHex(hexSeed.mid(4).trimmed());
    if (!check(hexSeed == wave, "WAV corpus seed differs from smoke fixture")
        || !readerAcceptsSeed(directory, hexSeed, QStringLiteral("corpus.wav"))) {
        return false;
    }

    QList<QByteArray> malformed;
    QByteArray badSignature = wave;
    badSignature[0] = 'X';
    malformed.append(badSignature);
    malformed.append(wave.left(44));
    QByteArray escapingFormat = wave;
    escapingFormat[16] = static_cast<char>(0xff);
    escapingFormat[17] = static_cast<char>(0xff);
    escapingFormat[18] = static_cast<char>(0xff);
    escapingFormat[19] = static_cast<char>(0xff);
    malformed.append(escapingFormat);
    malformed.append(QByteArray(44, 'x'));
    for (int index = 0; index < malformed.size(); ++index) {
        if (!readerRejectsSeed(directory, malformed.at(index), index)) {
            return false;
        }
    }
    return true;
}

bool smokeSharingParsers()
{
    const SstvShareManifestV1 manifest = validManifest();
    SstvShareValidationError error;
    const QByteArray canonical = manifest.toCanonicalJson(&error);
    if (!check(error.ok() && !canonical.isEmpty(),
               "cannot encode valid sharing manifest")) {
        return false;
    }
    if (!check(parseBoundedJsonObject(canonical).ok(),
               "bounded JSON rejected valid manifest")
        || !check(parseSstvShareManifestV1(canonical).ok(),
                  "manifest parser rejected canonical manifest")) {
        return false;
    }

    const QByteArray corpus = readCorpus(QStringLiteral("share/valid_manifest.json"))
                                  .trimmed();
    if (!check(!corpus.isEmpty(), "sharing corpus seed is missing")
        || !check(parseSstvShareManifestV1(corpus).ok(),
                  "sharing corpus seed is invalid")) {
        return false;
    }

    SstvShareTransfer transfer(manifest);
    const QByteArray persistence = transfer.toPersistenceJson(&error);
    if (!check(error.ok() && !persistence.isEmpty(),
               "cannot encode transfer persistence seed")) {
        return false;
    }
    const QByteArray persistenceCorpus =
        readCorpus(QStringLiteral("share/valid_transfer.json")).trimmed();
    if (!check(persistenceCorpus == persistence,
               "transfer persistence corpus differs from smoke fixture")
        || !check(restoreSstvShareTransfer(
                      persistenceCorpus,
                      fixedUtc(QStringLiteral("2026-08-24T10:00:01.000Z")),
                      false)
                      .ok(),
                  "transfer persistence parser rejected valid state")) {
        return false;
    }

    const QList<QByteArray> malformed {
        QByteArray {}, QByteArray("{"), QByteArray("[]"), QByteArray("{}"),
        QByteArray(kMaximumPersistenceJsonBytes + 1, 'x'),
        QByteArray("{\"protocolVersion\":1,\"unexpected\":true}"),
    };
    for (const QByteArray& input : malformed) {
        (void) parseBoundedJsonObject(
            input, kMaximumPersistenceJsonBytes,
            kMaximumJsonDepth, kMaximumJsonNodes);
        if (!check(!parseSstvShareManifestV1(input).ok(),
                   "malformed manifest seed was accepted")
            || !check(!restoreSstvShareTransfer(
                           input,
                           fixedUtc(QStringLiteral("2026-08-24T10:00:01.000Z")),
                           false)
                           .ok(),
                      "malformed persistence seed was accepted")) {
            return false;
        }
    }
    return true;
}

bool smokeQsoAdifParser()
{
    const QByteArray valid = readCorpus(QStringLiteral("qso/valid_sstv.adi"));
    if (!check(!valid.isEmpty(), "SSTV ADIF corpus seed is missing")) {
        return false;
    }
    const auto parsed = SstvQsoLog::validateGeneratedAdif(
        QString::fromUtf8(valid));
    if (!check(parsed.ok, "valid SSTV ADIF corpus seed was rejected")
        || !check(parsed.fields.value(QStringLiteral("MODE"))
                          == QStringLiteral("SSTV"),
                  "valid SSTV ADIF mode changed")
        || !check(!parsed.associationId.isEmpty(),
                  "valid SSTV ADIF has no local QSO identity")) {
        return false;
    }

    const QList<QByteArray> malformed {
        QByteArray {},
        QByteArray("<MODE:4>SSTV"),
        valid + QByteArray("<SUBMODE:9>MARTIN_M1 "),
        QByteArray(valid).replace("<MODE:4>SSTV", "<MODE:3>FAX"),
        QByteArray(valid).replace("<BAND:3>20M", "<BAND:3>OOB"),
        QByteArray(valid).replace("<TIME_ON:6>121314", "<TIME_ON:6>256199"),
        valid + QByteArray("<APP_IMAGE_PATH:14>/tmp/image.png "),
    };
    for (const QByteArray& input : malformed) {
        if (!check(!SstvQsoLog::validateGeneratedAdif(
                        QString::fromUtf8(input)).ok,
                   "malformed SSTV ADIF seed was accepted")) {
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    if (!smokeWavParser() || !smokeSharingParsers()
        || !smokeQsoAdifParser()) {
        return 1;
    }
    std::cout << "SSTV fuzz smoke passed\n";
    return 0;
}
