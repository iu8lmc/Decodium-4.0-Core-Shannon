// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/sharing/SstvShareManifest.h"
#include "src/sstv/sharing/SstvShareSecurity.h"
#include "src/sstv/sharing/SstvShareTransfer.h"

#include <QByteArray>
#include <QDateTime>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace decodium::sstv::sharing;

namespace {

constexpr std::size_t kMaximumFuzzInputBytes = 1024U * 1024U;

[[noreturn]] void invariantFailure()
{
    std::abort();
}

QDateTime fixedNow()
{
    static const QDateTime value = QDateTime::fromString(
        QStringLiteral("2026-08-24T10:00:01.000Z"), Qt::ISODateWithMs)
                                       .toUTC();
    return value;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size)
{
    if ((!data && size != 0U) || size > kMaximumFuzzInputBytes) {
        return 0;
    }
    const char* const bytes = size == 0U
        ? "" : reinterpret_cast<const char*>(data);
    const QByteArray input(bytes,
                           static_cast<qsizetype>(size));

    const auto bounded = parseBoundedJsonObject(
        input, kMaximumPersistenceJsonBytes,
        kMaximumJsonDepth, kMaximumJsonNodes);
    if (bounded.ok()) {
        SstvShareValidationError error;
        const QByteArray canonical = canonicalJson(bounded.object, &error);
        if (!error.ok()
            || !parseBoundedJsonObject(
                    canonical, kMaximumPersistenceJsonBytes,
                    kMaximumJsonDepth, kMaximumJsonNodes)
                    .ok()) {
            invariantFailure();
        }
    }

    const auto manifest = parseSstvShareManifestV1(input);
    if (manifest.ok()) {
        SstvShareValidationError error;
        const QByteArray canonical = manifest.manifest->toCanonicalJson(&error);
        const auto reparsed = parseSstvShareManifestV1(canonical);
        if (!error.ok() || canonical.isEmpty() || !reparsed.ok()
            || reparsed.manifest->toCanonicalJson() != canonical) {
            invariantFailure();
        }
    }

    const auto transfer = restoreSstvShareTransfer(input, fixedNow(), false);
    if (transfer.ok()) {
        SstvShareValidationError error;
        const QByteArray persisted =
            transfer.transfer->toPersistenceJson(&error);
        if (!error.ok() || persisted.isEmpty()
            || !restoreSstvShareTransfer(persisted, fixedNow(), false).ok()) {
            invariantFailure();
        }
    }
    return 0;
}
