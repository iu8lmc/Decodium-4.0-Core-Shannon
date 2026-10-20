// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/integration/SstvWavPcmReader.h"

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QVector>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace decodium::sstv;

namespace {

constexpr std::size_t kMaximumFuzzInputBytes = 1024U * 1024U;

struct ScratchFile final
{
    QTemporaryDir directory;
    QString path {directory.filePath(QStringLiteral("input.wav"))};
};

QByteArray decodedInput(const std::uint8_t* data, std::size_t size)
{
    const char* const bytes = size == 0U
        ? "" : reinterpret_cast<const char*>(data);
    QByteArray input(bytes,
                     static_cast<qsizetype>(size));
    if (input.startsWith("hex:")) {
        input = QByteArray::fromHex(input.mid(4).trimmed());
    }
    return input;
}

[[noreturn]] void invariantFailure()
{
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size)
{
    if ((!data && size != 0U) || size > kMaximumFuzzInputBytes) {
        return 0;
    }
    static ScratchFile scratch;
    if (!scratch.directory.isValid()) {
        return 0;
    }
    const QByteArray input = decodedInput(data, size);
    if (input.size() > static_cast<qsizetype>(kMaximumFuzzInputBytes)) {
        return 0;
    }
    QFile file(scratch.path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(input) != input.size() || !file.flush()) {
        return 0;
    }
    file.close();

    SstvWavPcmReaderLimits limits;
    limits.maximumFileBytes = kMaximumFuzzInputBytes;
    limits.maximumDataBytes = kMaximumFuzzInputBytes - 44U;
    limits.maximumDurationMs = 60'000U;
    limits.maximumChunks = 256U;
    limits.maximumFormatChunkBytes = 4'096U;
    limits.maximumFramesPerRead = 1'024U;
    SstvWavPcmReader reader(limits);
    QString error;
    if (!reader.open(scratch.path, &error)) {
        return 0;
    }

    quint64 observedFrames = 0U;
    const quint64 iterationLimit =
        reader.format().totalFrames / limits.maximumFramesPerRead + 2U;
    for (quint64 iteration = 0U; iteration < iterationLimit; ++iteration) {
        QVector<short> samples;
        const SstvWavReadStatus status = reader.readNext(&samples, &error);
        if (status == SstvWavReadStatus::Chunk) {
            if (samples.isEmpty()
                || samples.size() > static_cast<qsizetype>(
                       limits.maximumFramesPerRead)) {
                invariantFailure();
            }
            observedFrames += static_cast<quint64>(samples.size());
            if (observedFrames != reader.framesRead()
                || observedFrames > reader.format().totalFrames) {
                invariantFailure();
            }
            continue;
        }
        if (status == SstvWavReadStatus::End) {
            if (observedFrames != reader.format().totalFrames
                || reader.framesRead() != reader.format().totalFrames) {
                invariantFailure();
            }
            return 0;
        }
        invariantFailure();
    }
    invariantFailure();
}
